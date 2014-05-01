/*
 *  Icon file server operations
 *  Copyright (C) 2012 Andy Brown
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include "settings.h"
#include "tvheadend.h"
#include "filebundle.h"
#include "imagecache.h"
#include "queue.h"
#include "redblack.h"
#include "notify.h"
#include "prop.h"

#if ENABLE_IMAGECACHE
#define CURL_STATICLIB
#include <curl/curl.h>
#include <curl/easy.h>
#endif

/*
 * Image metadata
 */
typedef struct imagecache_image
{
  int         id;       ///< Internal ID
  const char *url;      ///< Upstream URL
  int         failed;   ///< Last update failed
  time_t      updated;  ///< Last time the file was checked
  enum {
    IDLE,
    QUEUED,
    FETCHING
  }           state;    ///< fetch status

  TAILQ_ENTRY(imagecache_image) q_link;   ///< Fetch Q link
  RB_ENTRY(imagecache_image)    id_link;  ///< Index by ID
  RB_ENTRY(imagecache_image)    url_link; ///< Index by URL
} imagecache_image_t;

static int                            imagecache_id;
static RB_HEAD(,imagecache_image)     imagecache_by_id;
static RB_HEAD(,imagecache_image)     imagecache_by_url;

#if ENABLE_IMAGECACHE
struct imagecache_config imagecache_conf;
static const property_t  imagecache_props[] = {
  {
    .type   = PT_BOOL,
    .id     = "enabled",
    .name   = "Enabled",
    .off    = offsetof(struct imagecache_config, enabled),
  },
  {
    .type   = PT_BOOL,
    .id     = "ignore_sslcert",
    .name   = "Ignore invalid SSL certificate",
    .off    = offsetof(struct imagecache_config, ignore_sslcert),
  },
  {
    .type   = PT_U32,
    .id     = "ok_period",
    .name   = "Re-try period",
    .off    = offsetof(struct imagecache_config, ok_period),
  },
  {
    .type   = PT_U32,
    .id     = "fail_period",
    .name   = "Re-try period of failed images",
    .off    = offsetof(struct imagecache_config, fail_period),
  },
  {}
};

static pthread_cond_t                 imagecache_cond;
static TAILQ_HEAD(, imagecache_image) imagecache_queue;
static gtimer_t                       imagecache_timer;
#endif

static int
url_cmp ( imagecache_image_t *a, imagecache_image_t *b )
{
  return strcmp(a->url, b->url);
}

static int
id_cmp ( imagecache_image_t *a, imagecache_image_t *b )
{
  return (a->id - b->id);
}

static void
imagecache_image_save ( imagecache_image_t *img )
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "url", img->url);
  if (img->updated)
    htsmsg_add_s64(m, "updated", img->updated);
  hts_settings_save(m, "imagecache/meta/%d", img->id);
}

#if ENABLE_IMAGECACHE
static void
imagecache_image_add ( imagecache_image_t *img )
{
  if (strncasecmp("file://", img->url, 7)) {
    img->state = QUEUED;
    TAILQ_INSERT_TAIL(&imagecache_queue, img, q_link);
    pthread_cond_broadcast(&imagecache_cond);
  } else {
    time(&img->updated);
  }
}

static int
imagecache_image_fetch ( imagecache_image_t *img )
{
  int res = 1;
  CURL *curl;
  FILE *fp;
  char tmp[256], path[256], useragent[256];

  /* Open file  */
  if (hts_settings_buildpath(path, sizeof(path), "imagecache/data/%d",
                              img->id))
    goto error;
  if (hts_settings_makedirs(path))
    goto error;
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  snprintf(useragent, sizeof(useragent), "TVHeadend/%s", tvheadend_version);
  if (!(fp = fopen(tmp, "wb")))
    goto error;
  
  /* Build command */
  tvhlog(LOG_DEBUG, "imagecache", "fetch %s", img->url);
  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL,         img->url);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA,   fp);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,   useragent);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT,     120);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS,  1);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
  if (imagecache_conf.ignore_sslcert)
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

  /* Fetch (release lock, incase of delays) */
  pthread_mutex_unlock(&global_lock);
  res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  fclose(fp);
  pthread_mutex_lock(&global_lock);

  /* Process */
error:
  img->state = IDLE;
  time(&img->updated); // even if failed (possibly request sooner?)
  if (res) {
    img->failed = 1;
    unlink(tmp);
    tvhlog(LOG_WARNING, "imagecache", "failed to download %s", img->url);
  } else {
    img->failed = 0;
    unlink(path);
    rename(tmp, path);
    tvhlog(LOG_DEBUG, "imagecache", "downloaded %s", img->url);
  }
  imagecache_image_save(img);
  pthread_cond_broadcast(&imagecache_cond);

  return res;
};

static void *
imagecache_thread ( void *p )
{
  imagecache_image_t *img;

  pthread_mutex_lock(&global_lock);
  while (tvheadend_running) {

    /* Check we're enabled */
    if (!imagecache_conf.enabled) {
      pthread_cond_wait(&imagecache_cond, &global_lock);
      continue;
    }

    /* Get entry */
    if (!(img = TAILQ_FIRST(&imagecache_queue))) {
      pthread_cond_wait(&imagecache_cond, &global_lock);
      continue;
    }

    /* Process */
    img->state = FETCHING;
    TAILQ_REMOVE(&imagecache_queue, img, q_link);

    /* Fetch */
    (void)imagecache_image_fetch(img);
  }
  pthread_mutex_unlock(&global_lock);

  return NULL;
}

static void
imagecache_timer_cb ( void *p )
{
  time_t now, when;
  imagecache_image_t *img;
  time(&now);
  RB_FOREACH(img, &imagecache_by_url, url_link) {
    if (img->state != IDLE) continue;
    when = img->failed ? imagecache_conf.fail_period
                       : imagecache_conf.ok_period;
    when = img->updated + (when * 3600);
    if (when < now)
      imagecache_image_add(img);
  }
}

#endif /* ENABLE_IMAGECACHE */

/*
 * Initialise
 */
#if ENABLE_IMAGECACHE
pthread_t imagecache_tid;
#endif

void
imagecache_init ( void )
{
  htsmsg_t *m, *e;
  htsmsg_field_t *f;
  imagecache_image_t *img, *i;
  const char *url;
  uint32_t id;

  /* Init vars */
  imagecache_id             = 0;
#if ENABLE_IMAGECACHE
  imagecache_conf.enabled        = 0;
  imagecache_conf.ok_period      = 24 * 7; // weekly
  imagecache_conf.fail_period    = 24;     // daily
  imagecache_conf.ignore_sslcert = 0;
#endif

  /* Create threads */
#if ENABLE_IMAGECACHE
  pthread_cond_init(&imagecache_cond, NULL);
  TAILQ_INIT(&imagecache_queue);
#endif

  /* Load settings */
#if ENABLE_IMAGECACHE
  if ((m = hts_settings_load("imagecache/config"))) {
    imagecache_set_config(m);
    htsmsg_destroy(m);
  }
#endif
  if ((m = hts_settings_load("imagecache/meta"))) {
    HTSMSG_FOREACH(f, m) {
      if (!(e   = htsmsg_get_map_by_field(f))) continue;
      if (!(id  = atoi(f->hmf_name))) continue;
      if (!(url = htsmsg_get_str(e, "url"))) continue;
      img          = calloc(1, sizeof(imagecache_image_t));
      img->id      = id;
      img->url     = strdup(url);
      img->updated = htsmsg_get_s64_or_default(e, "updated", 0);
      i = RB_INSERT_SORTED(&imagecache_by_url, img, url_link, url_cmp);
      if (i) {
        hts_settings_remove("imagecache/meta/%d", id);
        hts_settings_remove("imagecache/data/%d", id);
        free((void*)img->url);
        free(img);
        continue;
      }
      i = RB_INSERT_SORTED(&imagecache_by_id, img, id_link, id_cmp);
      assert(!i);
      if (id > imagecache_id)
        imagecache_id = id;
#if ENABLE_IMAGECACHE
      if (!img->updated)
        imagecache_image_add(img);
#endif
    }
    htsmsg_destroy(m);
  }

  /* Start threads */
#if ENABLE_IMAGECACHE
  tvhthread_create(&imagecache_tid, NULL, imagecache_thread, NULL, 0);

  /* Re-try timer */
  // TODO: this could be more efficient by being targetted, however
  //       the reality its not necessary and I'd prefer to avoid dumping
  //       100's of timers into the global pool
  gtimer_arm(&imagecache_timer, imagecache_timer_cb, NULL, 600);
#endif
}

/*
 * Shutdown
 */
void
imagecache_done ( void )
{
  imagecache_image_t *img;

#if ENABLE_IMAGECACHE
  pthread_cond_broadcast(&imagecache_cond);
  pthread_join(imagecache_tid, NULL);
#endif
  while ((img = RB_FIRST(&imagecache_by_url)) != NULL) {
    RB_REMOVE(&imagecache_by_url, img, url_link);
    RB_REMOVE(&imagecache_by_id, img, id_link);
    free((void *)img->url);
    free(img);
  }
}


#if ENABLE_IMAGECACHE

/*
 * Get config
 */
htsmsg_t *
imagecache_get_config ( void )
{
  htsmsg_t *m = htsmsg_create_map();
  prop_read_values(&imagecache_conf, imagecache_props, m, 0, NULL);
  return m;
}

/*
 * Set config
 */
int
imagecache_set_config ( htsmsg_t *m )
{
  int save = prop_write_values(&imagecache_conf, imagecache_props, m, 0, NULL);
  if (save)
    pthread_cond_broadcast(&imagecache_cond);
  return save;
}

/*
 * Save
 */
void
imagecache_save ( void )
{
  htsmsg_t *m = imagecache_get_config();
  hts_settings_save(m, "imagecache/config");
  notify_reload("imagecache");
}

#endif /* ENABLE_IMAGECACHE */

/*
 * Fetch a URLs ID
 */
uint32_t
imagecache_get_id ( const char *url )
{
  uint32_t id = 0;
  imagecache_image_t *i;
  static imagecache_image_t *skel = NULL;

  lock_assert(&global_lock);

  /* Invalid */
  if (!url)
    return 0;

  /* Disabled */
#if !ENABLE_IMAGECACHE
  if (strncasecmp(url, "file://", 7))
    return 0;
#endif

  /* Skeleton */
  if (!skel)
    skel = calloc(1, sizeof(imagecache_image_t));
  skel->url = url;

  /* Create/Find */
  i = RB_INSERT_SORTED(&imagecache_by_url, skel, url_link, url_cmp);
  if (!i) {
    i      = skel;
    i->url = strdup(url);
    i->id  = ++imagecache_id;
    skel   = RB_INSERT_SORTED(&imagecache_by_id, i, id_link, id_cmp);
    assert(!skel);
#if ENABLE_IMAGECACHE
    imagecache_image_add(i);
#endif
    imagecache_image_save(i);
  }
#if ENABLE_IMAGECACHE
  if (!strncasecmp(url, "file://", 7) || imagecache_conf.enabled)
    id = i->id;
#else
  if (!strncasecmp(url, "file://", 7))
    id = i->id;
#endif
  
  return id;
}

/*
 * Get data
 */
int
imagecache_open ( uint32_t id )
{
  imagecache_image_t skel, *i;
  int fd = -1;

  lock_assert(&global_lock);

  /* Find */
  skel.id = id;
  if (!(i = RB_FIND(&imagecache_by_id, &skel, id_link, id_cmp)))
    return -1;

  /* Local file */
  if (!strncasecmp(i->url, "file://", 7))
    fd = open(i->url + 7, O_RDONLY);

  /* Remote file */
#if ENABLE_IMAGECACHE
  else if (imagecache_conf.enabled) {
    int e;
    struct timespec ts;

    /* Use existing */
    if (i->updated) {

    /* Wait */
    } else if (i->state == FETCHING) {
      time(&ts.tv_sec);
      ts.tv_nsec = 0;
      ts.tv_sec += 5;
      e = pthread_cond_timedwait(&imagecache_cond, &global_lock, &ts);
      if (e == ETIMEDOUT)
        return -1;

    /* Attempt to fetch */
    } else if (i->state == QUEUED) {
      i->state = FETCHING;
      TAILQ_REMOVE(&imagecache_queue, i, q_link);
      pthread_mutex_unlock(&global_lock);
      e = imagecache_image_fetch(i);
      pthread_mutex_lock(&global_lock);
      if (e)
        return -1;
    }
    fd = hts_settings_open_file(0, "imagecache/data/%d", i->id);
  }
#endif

  return fd;
}
