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

#if ENABLE_IMAGECACHE
#define CURL_STATICLIB
#include <curl/curl.h>
#include <curl/easy.h>
#endif

// TODO: icon cache flushing?
// TODO: md5 validation?
// TODO: allow cache to be disabled by users

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

static int                        _imagecache_id;
static RB_HEAD(,imagecache_image) _imagecache_by_id;
static RB_HEAD(,imagecache_image) _imagecache_by_url;

pthread_mutex_t                   imagecache_mutex;

static void  _imagecache_save   ( imagecache_image_t *img );

#if ENABLE_IMAGECACHE
uint32_t                              imagecache_enabled;
uint32_t                              imagecache_ok_period;
uint32_t                              imagecache_fail_period;

static pthread_cond_t                 _imagecache_cond;
static TAILQ_HEAD(, imagecache_image) _imagecache_queue;
static void  _imagecache_add    ( imagecache_image_t *img );
static void* _imagecache_thread ( void *p );
static int   _imagecache_fetch  ( imagecache_image_t *img );
#endif

static int _url_cmp ( void *a, void *b )
{
  return strcmp(((imagecache_image_t*)a)->url, ((imagecache_image_t*)b)->url);
}

static int _id_cmp  ( void *a, void *b )
{
  return ((imagecache_image_t*)a)->id - ((imagecache_image_t*)b)->id;
}

/*
 * Initialise
 */
void imagecache_init ( void )
{
  htsmsg_t *m, *e;
  htsmsg_field_t *f;
  imagecache_image_t *img, *i;
  const char *url;
  uint32_t id;

  /* Init vars */
  _imagecache_id         = 0;
#if ENABLE_IMAGECACHE
  imagecache_enabled     = 0;
  imagecache_ok_period   = 24 * 7; // weekly
  imagecache_fail_period = 24;     // daily
#endif

  /* Create threads */
  pthread_mutex_init(&imagecache_mutex, NULL);
#if ENABLE_IMAGECACHE
  pthread_cond_init(&_imagecache_cond, NULL);
  TAILQ_INIT(&_imagecache_queue);
#endif

  /* Load settings */
#if ENABLE_IMAGECACHE
  if ((m = hts_settings_load("imagecache/config"))) {
    htsmsg_get_u32(m, "enabled", &imagecache_enabled);
    htsmsg_get_u32(m, "ok_period", &imagecache_ok_period);
    htsmsg_get_u32(m, "fail_period", &imagecache_fail_period);
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
      i = RB_INSERT_SORTED(&_imagecache_by_url, img, url_link, _url_cmp);
      if (i) {
        hts_settings_remove("imagecache/meta/%d", id);
        hts_settings_remove("imagecache/data/%d", id);
        free(img);
        continue;
      }
      i = RB_INSERT_SORTED(&_imagecache_by_id, img, id_link, _id_cmp);
      assert(!i);
      if (id > _imagecache_id)
        _imagecache_id = id;
#if ENABLE_IMAGECACHE
      if (!img->updated)
        _imagecache_add(img);
#endif
    }
    htsmsg_destroy(m);
  }

  /* Start threads */
#if ENABLE_IMAGECACHE
  {
    pthread_t tid;
    pthread_create(&tid, NULL, _imagecache_thread, NULL);
  }
#endif
}

/*
 * Save settings
 */
#if ENABLE_IMAGECACHE
void imagecache_save ( void )
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "enabled",     imagecache_enabled);
  htsmsg_add_u32(m, "ok_period",   imagecache_ok_period);
  htsmsg_add_u32(m, "fail_period", imagecache_fail_period);
  hts_settings_save(m, "imagecache/config");
}

/*
 * Enable/disable
 */
int imagecache_set_enabled ( uint32_t e )
{
  if (e == imagecache_enabled)
    return 0;
  imagecache_enabled = e;
  if (e)
    pthread_cond_broadcast(&_imagecache_cond);
  return 1;
}

/*
 * Set ok period
 */
int imagecache_set_ok_period ( uint32_t p )
{
  if (p == imagecache_ok_period)
    return 0;
  imagecache_ok_period = p;
  return 1;
}

/*
 * Set fail period
 */
int imagecache_set_fail_period ( uint32_t p )
{
  if (p == imagecache_fail_period)
    return 0;
  imagecache_fail_period = p;
  return 1;
}
#endif

/*
 * Fetch a URLs ID
 */
uint32_t imagecache_get_id ( const char *url )
{
  uint32_t id = 0;
  imagecache_image_t *i;
  static imagecache_image_t *skel = NULL;

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
  pthread_mutex_lock(&imagecache_mutex);
  i = RB_INSERT_SORTED(&_imagecache_by_url, skel, url_link, _url_cmp);
  if (!i) {
    i      = skel;
    i->url = strdup(url);
    i->id  = ++_imagecache_id;
    skel   = RB_INSERT_SORTED(&_imagecache_by_id, i, id_link, _id_cmp);
    assert(!skel);
#if ENABLE_IMAGECACHE
    _imagecache_add(i);
#endif
    _imagecache_save(i);
  }
#if ENABLE_IMAGECACHE
  if (!strncasecmp(url, "file://", 7) || imagecache_enabled)
    id = i->id;
#else
  if (!strncasecmp(url, "file://", 7))
    id = i->id;
#endif
  pthread_mutex_unlock(&imagecache_mutex);
  
  return id;
}

/*
 * Get data
 */
int imagecache_open ( uint32_t id )
{
  imagecache_image_t skel, *i;
  int fd = -1;

  pthread_mutex_lock(&imagecache_mutex);

  /* Find */
  skel.id = id;
  i = RB_FIND(&_imagecache_by_id, &skel, id_link, _id_cmp);

  /* Invalid */
  if (!i) {
    pthread_mutex_unlock(&imagecache_mutex);
    return -1;
  }

  /* Local file */
  if (!strncasecmp(i->url, "file://", 7))
    fd = open(i->url + 7, O_RDONLY);

  /* Remote file */
#if ENABLE_IMAGECACHE
  else if (imagecache_enabled) {
    struct timespec ts;
    int err;
    if (i->updated) {
      // use existing
    } else if (i->state == FETCHING) {
      ts.tv_nsec = 0;
      time(&ts.tv_sec);
      ts.tv_sec += 10; // TODO: sensible timeout?
      err = pthread_cond_timedwait(&_imagecache_cond, &imagecache_mutex, &ts);
      if (err == ETIMEDOUT) {
        pthread_mutex_unlock(&imagecache_mutex);
        return -1;
      }
    } else if (i->state == QUEUED) {
      i->state = FETCHING;
      TAILQ_REMOVE(&_imagecache_queue, i, q_link);
      pthread_mutex_unlock(&imagecache_mutex);
      if (_imagecache_fetch(i))
        return -1;
      pthread_mutex_lock(&imagecache_mutex);
    }
    fd = hts_settings_open_file(0, "imagecache/data/%d", i->id);
  }
#endif
  pthread_mutex_unlock(&imagecache_mutex);

  return fd;
}

static void _imagecache_save ( imagecache_image_t *img )
{
  htsmsg_t *m = htsmsg_create_map();
  
  htsmsg_add_str(m, "url", img->url);
  if (img->updated)
    htsmsg_add_s64(m, "updated", img->updated);

  hts_settings_save(m, "imagecache/meta/%d", img->id);
}

#if ENABLE_IMAGECACHE
static void _imagecache_add ( imagecache_image_t *img )
{
  if (strncasecmp("file://", img->url, 7)) {
    img->state = QUEUED;
    TAILQ_INSERT_TAIL(&_imagecache_queue, img, q_link);
    pthread_cond_broadcast(&_imagecache_cond);
  } else {
    time(&img->updated);
  }
}

static void *_imagecache_thread ( void *p )
{
  int err;
  imagecache_image_t *img;
  struct timespec ts;
  ts.tv_nsec = 0;

  while (1) {

    /* Get entry */
    pthread_mutex_lock(&imagecache_mutex);
    if (!imagecache_enabled) {
      pthread_cond_wait(&_imagecache_cond, &imagecache_mutex);
      pthread_mutex_unlock(&imagecache_mutex);
      continue;
    }
    img = TAILQ_FIRST(&_imagecache_queue);
    if (!img) {
      time(&ts.tv_sec);
      ts.tv_sec += 60;
      err = pthread_cond_timedwait(&_imagecache_cond, &imagecache_mutex, &ts);
      if (err == ETIMEDOUT) {
        uint32_t period;
        RB_FOREACH(img, &_imagecache_by_url, url_link) {
          if (img->state != IDLE) continue;
          period = img->failed ? imagecache_fail_period : imagecache_ok_period;
          period *= 3600;
          if (period && ((ts.tv_sec - img->updated) > period))
            _imagecache_add(img);
        }
      }
      pthread_mutex_unlock(&imagecache_mutex);
      continue;
    }
    img->state = FETCHING;
    TAILQ_REMOVE(&_imagecache_queue, img, q_link);
    pthread_mutex_unlock(&imagecache_mutex);

    /* Fetch */
    _imagecache_fetch(img);
  }

  return NULL;
}

static int _imagecache_fetch ( imagecache_image_t *img )
{
  int res;
  CURL *curl;
  FILE *fp;
  char tmp[256], path[256];

  /* Open file  */
  if (hts_settings_buildpath(path, sizeof(path), "imagecache/data/%d",
                              img->id))
    return 1;
  if (hts_settings_makedirs(path))
    return 1;
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  if (!(fp = fopen(tmp, "wb")))
    return 1;
  
  /* Fetch file */
  tvhlog(LOG_DEBUG, "imagecache", "fetch %s", img->url);
  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL,         img->url);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA,   fp);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,   "TVHeadend");
  curl_easy_setopt(curl, CURLOPT_TIMEOUT,     120);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS,  1);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
  res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  fclose(fp);

  /* Process */
  pthread_mutex_lock(&imagecache_mutex);
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
  _imagecache_save(img);
  pthread_cond_broadcast(&_imagecache_cond);
  pthread_mutex_unlock(&imagecache_mutex);
  
  return res;
};
#endif
