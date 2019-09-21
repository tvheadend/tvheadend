/*
 *  Icon file server operations
 *  Copyright (C) 2012 Andy Brown
 *            (C) 2015-2018 Jaroslav Kysela
 *
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

#include <fcntl.h>

#include "settings.h"
#include "tvheadend.h"
#include "filebundle.h"
#include "imagecache.h"
#include "sbuf.h"
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
  int         ref;      ///< Number of references
  const char *url;      ///< Upstream URL
  uint8_t     failed;   ///< Last update failed
  uint8_t     savepend; ///< Pending save
  time_t      accessed; ///< Last time the file was accessed
  time_t      updated;  ///< Last time the file was checked
  uint8_t     sha1[20]; ///< Contents hash
  enum {
    IDLE,
    SAVE,
    QUEUED,
    FETCHING
  }           state;    ///< save/fetch status

  TAILQ_ENTRY(imagecache_image) q_link;   ///< Fetch Q link
  RB_ENTRY(imagecache_image)    id_link;  ///< Index by ID
  RB_ENTRY(imagecache_image)    url_link; ///< Index by URL
} imagecache_image_t;

tvh_mutex_t imagecache_lock = TVH_THREAD_MUTEX_INITIALIZER;
static int imagecache_id;
static RB_HEAD(,imagecache_image) imagecache_by_id;
static RB_HEAD(,imagecache_image) imagecache_by_url;
SKEL_DECLARE(imagecache_skel, imagecache_image_t);

struct imagecache_config imagecache_conf = {
  .idnode.in_class = &imagecache_class,
};

static htsmsg_t *imagecache_class_save(idnode_t *self, char *filename, size_t fsize);
static void imagecache_destroy(imagecache_image_t *img, int delconf);

static inline time_t clkwrap(time_t clk)
{
  return clk / 8192; /* more than two hours */
}

CLASS_DOC(imagecache)

const idclass_t imagecache_class = {
  .ic_snode      = (idnode_t *)&imagecache_conf,
  .ic_class      = "imagecache",
  .ic_caption    = N_("Configuration - Image Cache"),
  .ic_event      = "imagecache",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_doc        = tvh_doc_imagecache_class,
  .ic_save       = imagecache_class_save,
  .ic_properties = (const property_t[]){
    {
      .type   = PT_BOOL,
      .id     = "enabled",
      .name   = N_("Enabled"),
      .desc   = N_("Select whether or not to enable caching. Note, "
                   "even with this disabled you can still specify "
                   "local (file://) icons and these will be served by "
                   "the built-in webserver."),
      .off    = offsetof(struct imagecache_config, enabled),
    },
    {
      .type   = PT_BOOL,
      .id     = "ignore_sslcert",
      .name   = N_("Ignore invalid SSL certificate"),
      .desc   = N_("Ignore invalid/unverifiable (expired, "
                   "self-certified, etc.) certificates"),
      .off    = offsetof(struct imagecache_config, ignore_sslcert),
    },
    {
      .type   = PT_U32,
      .id     = "expire",
      .name   = N_("Expire time"),
      .desc   = N_("The time in days after the cached URL will "
                   "be removed. The time starts when the URL was "
                   "lastly requested. Zero means unlimited cache "
                   "(not recommended)."),
      .off    = offsetof(struct imagecache_config, expire),
    },
    {
      .type   = PT_U32,
      .id     = "ok_period",
      .name   = N_("Re-fetch period (hours)"),
      .desc   = N_("How frequently the upstream provider is checked "
                   "for changes."),
      .off    = offsetof(struct imagecache_config, ok_period),
    },
    {
      .type   = PT_U32,
      .id     = "fail_period",
      .name   = N_("Re-try period (hours)"),
      .desc   = N_("How frequently it will re-try fetching an image "
                   "that has failed to be fetched."),
      .off    = offsetof(struct imagecache_config, fail_period),
    },
    {}
  }
};

static tvh_cond_t                     imagecache_cond;
static TAILQ_HEAD(, imagecache_image) imagecache_queue;

static void
imagecache_incref(imagecache_image_t *img)
{
  lock_assert(&imagecache_lock);
  assert(img->ref > 0);
  img->ref++;
}

static void
imagecache_decref(imagecache_image_t *img)
{
  lock_assert(&imagecache_lock);
  if (--img->ref == 0) {
    free((void *)img->url);
    free(img);
  }
}

static inline int
sha1_empty( const uint8_t *sha1 )
{
  int i;
  for (i = 0; i < 20; i++)
    if (sha1[i])
      return 0;
  return 1;
}

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

static htsmsg_t *
imagecache_image_htsmsg ( imagecache_image_t *img )
{
  char hex[41];
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "url", img->url);
  if (img->accessed)
    htsmsg_add_s64(m, "accessed", img->accessed);
  if (img->updated)
    htsmsg_add_s64(m, "updated", img->updated);
  if (!sha1_empty(img->sha1)) {
    bin2hex(hex, sizeof(hex), img->sha1, 20);
    htsmsg_add_str(m, "sha1", hex);
  }
  return m;
}

static void
imagecache_image_save ( imagecache_image_t *img )
{
  img->savepend = 1;
  if (img->state != SAVE && img->state != QUEUED && img->state != FETCHING) {
    img->state = SAVE;
    TAILQ_INSERT_TAIL(&imagecache_queue, img, q_link);
    tvh_cond_signal(&imagecache_cond, 1);
  }
}

static void
imagecache_new_id ( imagecache_image_t *i )
{
  imagecache_image_t *j;

  do {
    i->id = ++imagecache_id % INT_MAX;
    if (!i->id) i->id = ++imagecache_id % INT_MAX;
    j = RB_INSERT_SORTED(&imagecache_by_id, i, id_link, id_cmp);
  } while (j);
}

static void
imagecache_image_add ( imagecache_image_t *img )
{
  int oldstate = img->state;
  if (!imagecache_conf.enabled) return;
  if (strncasecmp("file://", img->url, 7)) {
    img->state = QUEUED;
    if (oldstate != SAVE && oldstate != QUEUED)
      TAILQ_INSERT_TAIL(&imagecache_queue, img, q_link);
    tvh_cond_signal(&imagecache_cond, 1);
  } else {
    time(&img->updated);
  }
}

static int
imagecache_update_sha1 ( imagecache_image_t *img,
                         const char *path )
{
  int fd;
  uint8_t sha1[20];
  sbuf_t sb;

  sbuf_init(&sb);
  if ((fd = tvh_open(path, O_RDONLY, 0)) < 0)
    return 0;
  while (sbuf_read(&sb, fd) < 0 && ERRNO_AGAIN(errno));
  close(fd);
  sha1_calc(sha1, sb.sb_data, sb.sb_ptr, NULL, 0);
  memcpy(img->sha1, sha1, 20);
  sbuf_free(&sb);
  return 1;
}

static int
imagecache_new_contents ( imagecache_image_t *img,
                          const char *tpath, char *path,
                          const uint8_t *data, size_t dsize )
{
  int empty, r = 0;
  uint8_t sha1[20];
  FILE *fp;

  sha1_calc(sha1, data, dsize, NULL, 0);
  empty = sha1_empty(img->sha1);
  if (empty && imagecache_update_sha1(img, path))
    empty = 0;
  if (!empty && memcmp(sha1, img->sha1, 20) == 0)
    return 0; /* identical */

  if (!(fp = tvh_fopen(tpath, "wb")))
    return 1;

  fwrite(data, dsize, 1, fp);
  fclose(fp);
  unlink(path);
  tvh_mutex_lock(&imagecache_lock);
  memcpy(img->sha1, sha1, 20);
  if (!empty) {
    /* change id - contents changed */
    hts_settings_remove("imagecache/meta/%d", img->id);
    RB_REMOVE(&imagecache_by_id, img, id_link);
    imagecache_new_id(img);
    r = hts_settings_buildpath(path, PATH_MAX, "imagecache/data/%d", img->id);
  }
  if (!r) {
    if (rename(tpath, path))
      tvherror(LS_IMAGECACHE, "unable to rename file '%s' to '%s'", tpath, path);
  }
  imagecache_image_save(img);
  tvh_mutex_unlock(&imagecache_lock);
  return r;
}

static int
imagecache_image_fetch ( imagecache_image_t *img )
{
  int res = 1, r;
  url_t url;
  char tpath[PATH_MAX + 4] = "", path[PATH_MAX];
  tvhpoll_event_t ev;
  tvhpoll_t *efd = NULL;
  http_client_t *hc = NULL;

  lock_assert(&imagecache_lock);

  if (img->url == NULL || img->url[0] == '\0')
    return res;

  /* Open file  */
  if (hts_settings_buildpath(path, sizeof(path), "imagecache/data/%d",
                             img->id))
    goto error;
  if (hts_settings_makedirs(path))
    goto error;
  snprintf(tpath, sizeof(tpath), "%s.tmp", path);

  /* Fetch (release lock, incase of delays) */
  tvh_mutex_unlock(&imagecache_lock);

  urlinit(&url);

  /* Build command */
  tvhdebug(LS_IMAGECACHE, "fetch %s", img->url);
  memset(&url, 0, sizeof(url));
  if (urlparse(img->url, &url)) {
    tvherror(LS_IMAGECACHE, "Unable to parse url '%s'", img->url);
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

  while (tvheadend_is_running()) {
    r = tvhpoll_wait(efd, &ev, 1, -1);
    if (r < 0)
      break;
    if (r == 0)
      continue;
    r = http_client_run(hc);
    if (r < 0)
      break;
    if (r == HTTP_CON_DONE) {
      if (hc->hc_code == HTTP_STATUS_OK && hc->hc_data_size > 0)
        res = imagecache_new_contents(img, tpath, path,
                                      (uint8_t *)hc->hc_data, hc->hc_data_size);
      break;
    }
  }

  /* Process */
error_lock:
  if (NULL != hc) http_client_close(hc);
  tvh_mutex_lock(&imagecache_lock);
error:
  urlreset(&url);
  tvhpoll_destroy(efd);
  time(&img->updated); // even if failed (possibly request sooner?)
  if (res) {
    if (!img->failed) {
      img->failed = 1;
      imagecache_image_save(img);
    }
    tvhwarn(LS_IMAGECACHE, "failed to download %s", img->url);
  } else {
    tvhdebug(LS_IMAGECACHE, "downloaded %s", img->url);
  }
  tvh_cond_signal(&imagecache_cond, 1);

  return res;
};

static void
imagecache_timer ( void )
{
  time_t now, when;
  imagecache_image_t *img, *img_next;

  now = gclk();
  for (img = RB_FIRST(&imagecache_by_url); img; img = img_next) {
    img_next = RB_NEXT(img, url_link);
    if (imagecache_conf.expire > 0 && img->accessed > 0) {
      when = img->accessed + imagecache_conf.expire * 24 * 3600;
      if (when < now) {
        tvhdebug(LS_IMAGECACHE, "expired: %s", img->url);
        imagecache_destroy(img, 1);
        continue;
      }
    }
    if (img->state != IDLE) continue;
    when = img->failed ? imagecache_conf.fail_period
                       : imagecache_conf.ok_period;
    when = img->updated + (when * 3600);
    if (when < now)
      imagecache_image_add(img);
  }
}

static void *
imagecache_thread ( void *p )
{
  imagecache_image_t *img;
  int64_t timer_expire = mclk() + sec2mono(600);

  tvh_mutex_lock(&imagecache_lock);
  while (tvheadend_is_running()) {

    /* Timer expired */
    if (timer_expire < mclk()) {
      timer_expire = mclk() + sec2mono(600);
      imagecache_timer();
    }

    /* Get entry */
    if (!(img = TAILQ_FIRST(&imagecache_queue))) {
      tvh_cond_timedwait(&imagecache_cond, &imagecache_lock, timer_expire);
      continue;
    }

    TAILQ_REMOVE(&imagecache_queue, img, q_link);

retry:
    if (img->state == SAVE) {
      /* Do save outside global mutex */
      htsmsg_t *m = imagecache_image_htsmsg(img);
      img->state = IDLE;
      img->savepend = 0;
      tvh_mutex_unlock(&imagecache_lock);
      hts_settings_save(m, "imagecache/meta/%d", img->id);
      htsmsg_destroy(m);
      tvh_mutex_lock(&imagecache_lock);

    } else if (img->state == QUEUED) {
      /* Fetch */
      imagecache_incref(img);
      if (imagecache_conf.enabled) {
        img->state = FETCHING;
        (void)imagecache_image_fetch(img);
      }
      if (img->savepend) {
        img->state = SAVE;
        imagecache_decref(img);
        goto retry;
      }
      img->state = IDLE;
      imagecache_decref(img);

    } else {
      img->state = IDLE;
    }
  }
  tvh_mutex_unlock(&imagecache_lock);

  return NULL;
}

/*
 * Initialise
 */
pthread_t imagecache_tid;

void
imagecache_init ( void )
{
  htsmsg_t *m, *e;
  htsmsg_field_t *f;
  imagecache_image_t *img, *i;
  const char *url, *sha1;
  int id;

  /* Init vars */
  imagecache_id                  = 0;
  imagecache_conf.enabled        = 0;
  imagecache_conf.expire         = 7;      // 7 days
  imagecache_conf.ok_period      = 24 * 7; // weekly
  imagecache_conf.fail_period    = 24;     // daily
  imagecache_conf.ignore_sslcert = 0;

  idclass_register(&imagecache_class);

  /* Create threads */
  tvh_cond_init(&imagecache_cond, 1);
  TAILQ_INIT(&imagecache_queue);

  /* Load settings */
  if ((m = hts_settings_load("imagecache/config"))) {
    idnode_load(&imagecache_conf.idnode, m);
    htsmsg_destroy(m);
  }
  if ((m = hts_settings_load("imagecache/meta"))) {
    HTSMSG_FOREACH(f, m) {
      if (!(e   = htsmsg_get_map_by_field(f))) continue;
      if (!(id  = atoi(htsmsg_field_name(f)))) continue;
      if (!(url = htsmsg_get_str(e, "url"))) continue;
      img           = calloc(1, sizeof(imagecache_image_t));
      img->id       = id;
      img->ref      = 1;
      img->url      = strdup(url);
      img->accessed = htsmsg_get_s64_or_default(e, "accessed", 0);
      img->updated  = htsmsg_get_s64_or_default(e, "updated", 0);
      sha1 = htsmsg_get_str(e, "sha1");
      if (sha1 && strlen(sha1) == 40)
        hex2bin(img->sha1, 20, sha1);
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
      if (img->accessed == 0) {
        img->accessed = gclk();
        imagecache_image_save(img);
      }
      if (!img->updated)
        imagecache_image_add(img);
      if (imagecache_id <= id)
        imagecache_id = id + 1;
    }
    htsmsg_destroy(m);
  }

  /* Start threads */
  tvh_thread_create(&imagecache_tid, NULL, imagecache_thread, NULL, "imagecache");
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
  imagecache_decref(img);
}

/*
 * Shutdown
 */
void
imagecache_done ( void )
{
  imagecache_image_t *img;

  tvh_mutex_lock(&imagecache_lock);
  tvh_cond_signal(&imagecache_cond, 1);
  tvh_mutex_unlock(&imagecache_lock);
  pthread_join(imagecache_tid, NULL);
  tvh_mutex_lock(&imagecache_lock);
  while ((img = RB_FIRST(&imagecache_by_id)) != NULL) {
    if (img->state == SAVE) {
      htsmsg_t *m = imagecache_image_htsmsg(img);
      hts_settings_save(m, "imagecache/meta/%d", img->id);
      htsmsg_destroy(m);
    }
    imagecache_destroy(img, 0);
  }
  tvh_mutex_unlock(&imagecache_lock);
  SKEL_FREE(imagecache_skel);
}


/*
 * Class save
 */
static htsmsg_t *
imagecache_class_save ( idnode_t *self, char *filename, size_t fsize )
{
  htsmsg_t *c = htsmsg_create_map();
  idnode_save(&imagecache_conf.idnode, c);
  if (filename)
    snprintf(filename, fsize, "imagecache/config");
  tvh_cond_signal(&imagecache_cond, 1);
  return c;
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

  tvh_mutex_lock(&imagecache_lock);

  /* remove all cached data, except the one actually fetched */
  for (img = RB_FIRST(&imagecache_by_id); img; img = next) {
    next = RB_NEXT(img, id_link);
    if (img->state == FETCHING)
      continue;
    imagecache_destroy(img, 1);
  }

  tvhinfo(LS_IMAGECACHE, "clean request");
  /* remove unassociated data */
  if (hts_settings_buildpath(path, sizeof(path), "imagecache/data")) {
    tvherror(LS_IMAGECACHE, "clean - buildpath");
    goto done;
  }
  if((n = fb_scandir(path, &namelist)) < 0)
    goto done;
  for (i = 0; i < n; i++) {
    name = namelist[i]->name;
    if (name[0] == '.')
      continue;
    skel.id = atoi(name);
    img = RB_FIND(&imagecache_by_id, &skel, id_link, id_cmp);
    if (img)
      continue;
    tvhinfo(LS_IMAGECACHE, "clean: removing unassociated file '%s/%s'", path, name);
    hts_settings_remove("imagecache/meta/%s", name);
    hts_settings_remove("imagecache/data/%s", name);
  }
  free(namelist);

done:
  tvh_mutex_unlock(&imagecache_lock);

  imagecache_trigger();
}

/*
 * Trigger
 */
void
imagecache_trigger( void )
{
  imagecache_image_t *img;

  tvh_mutex_lock(&imagecache_lock);
  tvhinfo(LS_IMAGECACHE, "load triggered");
  RB_FOREACH(img, &imagecache_by_url, url_link) {
    if (img->state != IDLE) continue;
    imagecache_image_add(img);
  }
  tvh_mutex_unlock(&imagecache_lock);
}

/*
 * Fetch a URLs ID
 *
 * If imagecache is not enabled, just manage the id<->local filename
 * mapping database.
 */
int
imagecache_get_id ( const char *url )
{
  int id = 0;
  imagecache_image_t *i;
  int save = 0;
  time_t clk;

  /* Invalid */
  if (!url || url[0] == '\0' || !strstr(url, "://"))
    return 0;

  /* Disabled */
  if (!imagecache_conf.enabled && strncasecmp(url, "file://", 7))
    return 0;

  /* Skeleton */
  SKEL_ALLOC(imagecache_skel);
  imagecache_skel->url = url;

  tvh_mutex_lock(&imagecache_lock);

  /* Create/Find */
  i = RB_INSERT_SORTED(&imagecache_by_url, imagecache_skel, url_link, url_cmp);
  if (!i) {
    i = imagecache_skel;
    i->ref = 1;
    i->url = strdup(url);
    SKEL_USED(imagecache_skel);
    save = 1;
    imagecache_new_id(i);
    imagecache_image_add(i);
  }

  /* Do file:// to imagecache ID mapping even if imagecache is not enabled */
  if (imagecache_conf.enabled || !strncasecmp(url, "file://", 7))
    id = i->id;

  clk = gclk();
  if (clkwrap(clk) != clkwrap(i->accessed)) {
    i->accessed = clk;
    save = 1;
  }
  if (save)
    imagecache_image_save(i);

  tvh_mutex_unlock(&imagecache_lock);

  return id;
}

/*
 *
 */
const char *
imagecache_get_propstr ( const char *image, char *buf, size_t buflen )
{
  int id = imagecache_get_id(image);
  if (id == 0) return image;
  snprintf(buf, buflen, "imagecache/%d", id);
  return buf;
}

/*
 * Get data
 */
int
imagecache_filename ( int id, char *name, size_t len )
{
  imagecache_image_t skel, *i = NULL;
  char *fn;

  tvh_mutex_lock(&imagecache_lock);

  /* Find */
  skel.id = id;
  if (!(i = RB_FIND(&imagecache_by_id, &skel, id_link, id_cmp)))
    goto out_error;

  imagecache_incref(i);

  /* Local file */
  if (!strncasecmp(i->url, "file://", 7)) {
    fn = tvh_strdupa(i->url + 7);
    http_deescape(fn);
    strlcpy(name, fn, len);
  }

  /* Remote file */
  else if (imagecache_conf.enabled) {
    int e;
    int64_t mono;

    /* Use existing */
    if (i->updated) {

    /* Wait */
    } else if (i->state == FETCHING) {
      mono = mclk() + sec2mono(5);
      do {
        e = tvh_cond_timedwait(&imagecache_cond, &imagecache_lock, mono);
        if (e == ETIMEDOUT)
          goto out_error;
      } while (ERRNO_AGAIN(e));

    /* Attempt to fetch */
    } else if (i->state == QUEUED) {
      i->state = FETCHING;
      TAILQ_REMOVE(&imagecache_queue, i, q_link);
      e = imagecache_image_fetch(i);
      if (e)
        goto out_error;
    }
    if (hts_settings_buildpath(name, len, "imagecache/data/%d", i->id))
      goto out_error;
  }

  imagecache_decref(i);
  tvh_mutex_unlock(&imagecache_lock);
  return 0;

out_error:
  if (i) imagecache_decref(i);
  tvh_mutex_unlock(&imagecache_lock);
  return -1;
}
