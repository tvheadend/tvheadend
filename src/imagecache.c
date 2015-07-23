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
#include "http.h"

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
SKEL_DECLARE(imagecache_skel, imagecache_image_t);

#if ENABLE_IMAGECACHE
struct imagecache_config imagecache_conf;
static const property_t  imagecache_props[] = {
  {
    .type   = PT_BOOL,
    .id     = "enabled",
    .name   = N_("Enabled"),
    .off    = offsetof(struct imagecache_config, enabled),
  },
  {
    .type   = PT_BOOL,
    .id     = "ignore_sslcert",
    .name   = N_("Ignore invalid SSL certificate"),
    .off    = offsetof(struct imagecache_config, ignore_sslcert),
  },
  {
    .type   = PT_U32,
    .id     = "ok_period",
    .name   = N_("Re-try period"),
    .off    = offsetof(struct imagecache_config, ok_period),
  },
  {
    .type   = PT_U32,
    .id     = "fail_period",
    .name   = N_("Re-try period of failed images"),
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
  htsmsg_destroy(m);
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
  int res = 1, r;
  FILE *fp = NULL;
  url_t url;
  char tmp[256] = "", path[256];
  tvhpoll_event_t ev;
  tvhpoll_t *efd = NULL;
  http_client_t *hc;

  lock_assert(&global_lock);

  if (img->url == NULL || img->url[0] == '\0')
    return res;

  memset(&url, 0, sizeof(url));

  /* Open file  */
  if (hts_settings_buildpath(path, sizeof(path), "imagecache/data/%d",
                              img->id))
    goto error;
  if (hts_settings_makedirs(path))
    goto error;
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  if (!(fp = tvh_fopen(tmp, "wb")))
    goto error;
  
  /* Fetch (release lock, incase of delays) */
  pthread_mutex_unlock(&global_lock);

  /* Build command */
  tvhdebug("imagecache", "fetch %s", img->url);
  memset(&url, 0, sizeof(url));
  if (urlparse(img->url, &url)) {
    tvherror("imagecache", "Unable to parse url '%s'", img->url);
    goto error_lock;
  }

  hc = http_client_connect(NULL, HTTP_VERSION_1_1, url.scheme,
                           url.host, url.port, NULL);
  if (hc == NULL)
    goto error_lock;

  http_client_ssl_peer_verify(hc, imagecache_conf.ignore_sslcert ? 0 : 1);
  hc->hc_handle_location = 1;
  hc->hc_data_limit  = 256*1024;
  efd = tvhpoll_create(1);
  hc->hc_efd = efd;

  r = http_client_simple(hc, &url);
  if (r < 0)
    goto error_lock;

  while (tvheadend_running) {
    r = tvhpoll_wait(efd, &ev, 1, -1);
    if (r < 0)
      break;
    if (r == 0)
      continue;
    r = http_client_run(hc);
    if (r < 0)
      break;
    if (r == HTTP_CON_DONE) {
      if (hc->hc_code == HTTP_STATUS_OK && hc->hc_data_size > 0) {
        fwrite(hc->hc_data, hc->hc_data_size, 1, fp);
        res = 0;
      }
      break;
    }
  }
  fclose(fp);
  fp = NULL;

  /* Process */
error_lock:
  pthread_mutex_lock(&global_lock);
error:
  if (fp)
    fclose(fp);
  urlreset(&url);
  tvhpoll_destroy(efd);
  img->state = IDLE;
  time(&img->updated); // even if failed (possibly request sooner?)
  if (res) {
    img->failed = 1;
    if (tmp[0])
      unlink(tmp);
    tvhwarn("imagecache", "failed to download %s", img->url);
  } else {
    img->failed = 0;
    unlink(path);
    if (rename(tmp, path))
      tvherror("imagecache", "unable to rename file '%s' to '%s'", tmp, path);
    tvhdebug("imagecache", "downloaded %s", img->url);
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
  tvhthread_create(&imagecache_tid, NULL, imagecache_thread, NULL);

  /* Re-try timer */
  // TODO: this could be more efficient by being targetted, however
  //       the reality its not necessary and I'd prefer to avoid dumping
  //       100's of timers into the global pool
  gtimer_arm(&imagecache_timer, imagecache_timer_cb, NULL, 600);
#endif
}


/*
 * Destroy
 */
static void
imagecache_destroy ( imagecache_image_t *img, int delconf )
{
  if (delconf) {
    hts_settings_remove("imagecache/meta/%d", img->id);
    hts_settings_remove("imagecache/data/%d", img->id);
  }
  RB_REMOVE(&imagecache_by_url, img, url_link);
  RB_REMOVE(&imagecache_by_id, img, id_link);
  free((void *)img->url);
  free(img);
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
  while ((img = RB_FIRST(&imagecache_by_id)) != NULL)
    imagecache_destroy(img, 0);
  SKEL_FREE(imagecache_skel);
}


#if ENABLE_IMAGECACHE

/*
 * Get config
 */
htsmsg_t *
imagecache_get_config ( void )
{
  htsmsg_t *m = htsmsg_create_map();
  prop_read_values(&imagecache_conf, imagecache_props, m, NULL, 0, NULL);
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

/*
 * Clean
 */
void
imagecache_clean( void )
{
  imagecache_image_t *img, *next, skel;
  fb_dirent **namelist;
  char path[PATH_MAX], *name;
  int i, n;

  lock_assert(&global_lock);

#if ENABLE_IMAGECACHE
  /* remove all cached data, except the one actually fetched */
  for (img = RB_FIRST(&imagecache_by_id); img; img = next) {
    next = RB_NEXT(img, id_link);
    if (img->state == FETCHING)
      continue;
    imagecache_destroy(img, 1);
  }
#endif

  tvhinfo("imagecache", "clean request");
  /* remove unassociated data*/
  if (hts_settings_buildpath(path, sizeof(path), "imagecache/data")) {
    tvherror("imagecache", "clean - buildpath");
    return;
  }
  if((n = fb_scandir(path, &namelist)) < 0)
    return;
  for (i = 0; i < n; i++) {
    name = namelist[i]->name;
    if (name[0] == '.')
      continue;
    skel.id = atoi(name);
    img = RB_FIND(&imagecache_by_id, &skel, id_link, id_cmp);
    if (img)
      continue;
    tvhinfo("imagecache", "clean: removing unassociated file '%s/%s'", path, name);
    hts_settings_remove("imagecache/meta/%s", name);
    hts_settings_remove("imagecache/data/%s", name);
  }
  free(namelist);
}

#endif /* ENABLE_IMAGECACHE */

/*
 * Fetch a URLs ID
 */
uint32_t
imagecache_get_id ( const char *url )
{
  uint32_t id = 0;
  imagecache_image_t *i, *j;

  lock_assert(&global_lock);

  /* Invalid */
  if (!url || url[0] == '\0' || !strstr(url, "://"))
    return 0;

  /* Disabled */
#if !ENABLE_IMAGECACHE
  if (strncasecmp(url, "file://", 7))
    return 0;
#endif

  /* Skeleton */
  SKEL_ALLOC(imagecache_skel);
  imagecache_skel->url = url;

  /* Create/Find */
  i = RB_INSERT_SORTED(&imagecache_by_url, imagecache_skel, url_link, url_cmp);
  if (!i) {
    i      = imagecache_skel;
    i->url = strdup(url);
    do {
      i->id = ++imagecache_id % INT_MAX;
      if (!i->id) i->id = ++imagecache_id % INT_MAX;
      j = RB_INSERT_SORTED(&imagecache_by_id, i, id_link, id_cmp);
    } while (j);
    SKEL_USED(imagecache_skel);
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
      e = imagecache_image_fetch(i);
      if (e)
        return -1;
    }
    fd = hts_settings_open_file(0, "imagecache/data/%d", i->id);
  }
#endif

  return fd;
}
