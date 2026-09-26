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
  uint8_t     attempts; ///< Consecutive transient fetch failures
  uint8_t     savepend; ///< Pending save
  uint8_t     deleted;  ///< Destroyed with its on-disk record
  time_t      accessed; ///< Last time the file was accessed
  time_t      updated;  ///< Last time the file was checked
  int64_t     airtime;  ///< Earliest known airing (epoch; 0 = fetch asap,
                        ///< IMAGECACHE_AIRTIME_UNSET = not known yet)
  int64_t     retry_at; ///< Monotonic clock for the next transient retry
  uint8_t     sha1[20]; ///< Contents hash
  enum {
    IDLE,
    SAVE,
    QUEUED,
    FETCHING,
    RETRY     ///< waiting (in imagecache_retry_queue) for a short retry
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
      .type   = PT_BOOL,
      .id     = "reuse_conn",
      .name   = N_("Reuse HTTP connection"),
      .desc   = N_("Keep the HTTP connection to the image server open "
                   "between downloads (one TCP/TLS handshake for many "
                   "images) instead of opening a new connection per "
                   "image. Disable only if a server misbehaves with "
                   "persistent connections."),
      .off    = offsetof(struct imagecache_config, reuse_conn),
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

/* Images whose last fetch failed transiently (network error, HTTP 5xx/429),
 * waiting for a short per-image retry -- unlike the hours-coarse fail_period
 * which handles persistent failures.  Sorted by retry_at. */
static TAILQ_HEAD(, imagecache_image) imagecache_retry_queue;
#define IMAGECACHE_TRANSIENT_RETRIES 3

/* Priority of an image nothing has asked for yet -- entries reloaded from disk
 * at start-up, before the EPG is loaded and re-references them.  It sorts after
 * every real airtime, so such entries neither jump ahead of guide artwork nor
 * block it, and the first real reference replaces it.  Kept distinct from 0,
 * which means "fetch asap" (channel icons and other non-EPG images). */
#define IMAGECACHE_AIRTIME_UNSET INT64_MAX
#define IMAGECACHE_RETRY_DELAY(attempts) sec2mono(60 << ((attempts) - 1))

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
  if (img->state != SAVE && img->state != QUEUED &&
      img->state != FETCHING && img->state != RETRY) {
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

/* Insert into the fetch queue ordered by earliest airing first (airtime 0 =
 * asap: channel icons and other non-EPG images jump the queue; entries with no
 * known priority yet sort last).  The EPG enqueues images roughly
 * chronologically, so the forward scan is cheap. */
static void
imagecache_image_enqueue ( imagecache_image_t *img )
{
  imagecache_image_t *i;

  TAILQ_FOREACH(i, &imagecache_queue, q_link)
    if (i->airtime > img->airtime)
      break;
  if (i)
    TAILQ_INSERT_BEFORE(i, img, q_link);
  else
    TAILQ_INSERT_TAIL(&imagecache_queue, img, q_link);
}

/* Schedule a short retry after a transient fetch failure (lock held). */
static void
imagecache_image_retry_later ( imagecache_image_t *img )
{
  imagecache_image_t *i;

  img->state = RETRY;
  img->retry_at = mclk() + IMAGECACHE_RETRY_DELAY(img->attempts);
  TAILQ_FOREACH(i, &imagecache_retry_queue, q_link)
    if (i->retry_at > img->retry_at)
      break;
  if (i)
    TAILQ_INSERT_BEFORE(i, img, q_link);
  else
    TAILQ_INSERT_TAIL(&imagecache_retry_queue, img, q_link);
  tvh_cond_signal(&imagecache_cond, 1);
}

static void
imagecache_image_add ( imagecache_image_t *img )
{
  int oldstate = img->state;
  if (!imagecache_conf.enabled) return;
  if (strncasecmp("file://", img->url, 7)) {
    if (oldstate == RETRY)
      TAILQ_REMOVE(&imagecache_retry_queue, img, q_link);
    img->state = QUEUED;
    if (oldstate != SAVE && oldstate != QUEUED)
      imagecache_image_enqueue(img);
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

/*
 * Persistent fetch connection ("Reuse HTTP connection" setting).
 *
 * Guide artwork typically lives on a single CDN; re-using one kept-alive
 * client turns thousands of TCP+TLS handshakes into one.  The cached client
 * is owned by whoever grabs it under imagecache_lock (the fetch thread or a
 * direct fetch from imagecache_filename); concurrent fetches simply fall
 * back to a one-shot connection.  It is dropped whenever the fetch queue
 * drains, so no idle socket is kept open.
 */
static http_client_t *imagecache_fetch_hc   = NULL;
static tvhpoll_t     *imagecache_fetch_efd  = NULL;
static int            imagecache_fetch_busy = 0;

/* Close the cached fetch connection (imagecache_lock held).  Returns 1 when
 * the lock had to be released to do it, so the caller knows its view of the
 * queues may be stale. */
static int
imagecache_fetch_conn_close ( void )
{
  http_client_t *hc = imagecache_fetch_hc;
  tvhpoll_t *efd = imagecache_fetch_efd;

  imagecache_fetch_hc  = NULL;
  imagecache_fetch_efd = NULL;
  if (hc == NULL && efd == NULL)
    return 0;
  tvh_mutex_unlock(&imagecache_lock);
  if (hc)  http_client_close(hc);
  if (efd) tvhpoll_destroy(efd);
  tvh_mutex_lock(&imagecache_lock);
  return 1;
}

/* Request headers for a reusable fetch connection.  http_client_simple() and
 * http_client_simple_reconnect() both ask the header builder for keepalive = 0,
 * which adds "Connection: close" -- and http_client_send() clears hc_keepalive
 * when it sees that header.  Left alone, no connection could ever be kept, so
 * ask for keep-alive explicitly when reuse is enabled. */
static void
imagecache_fetch_hdr_create ( http_client_t *hc, http_arg_list_t *h,
                              const url_t *url, int keepalive )
{
  http_client_basic_args(hc, h, url, 1);
}

static int
imagecache_image_fetch ( imagecache_image_t *img )
{
  int res = 1;
  int r;
  int reuse = 0;
  int conn_ok = 0;
  int keep;
  int attempted = 0;
  int http_code = 0;
  url_t url;
  char tpath[PATH_MAX + 4] = "", path[PATH_MAX];
  tvhpoll_event_t ev;
  tvhpoll_t *efd = NULL;
  http_client_t *hc = NULL;

  urlinit(&url);

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

  /* Take ownership of the cached kept-alive connection (when enabled and
   * not in use by a concurrent fetch) */
  if (imagecache_conf.reuse_conn && !imagecache_fetch_busy) {
    reuse = 1;
    imagecache_fetch_busy = 1;
    hc  = imagecache_fetch_hc;  imagecache_fetch_hc  = NULL;
    efd = imagecache_fetch_efd; imagecache_fetch_efd = NULL;
  }

  /* Fetch (release lock, incase of delays) */
  tvh_mutex_unlock(&imagecache_lock);

  /* Build command */
  tvhdebug(LS_IMAGECACHE, "fetch %s", img->url);
  memset(&url, 0, sizeof(url));
  if (urlparse(img->url, &url)) {
    tvherror(LS_IMAGECACHE, "Unable to parse url '%s'", img->url);
    goto error_lock;
  }

  /* The certificate policy is fixed when a client is created, and a reconnect
   * to another origin re-applies that stored value.  If "ignore invalid SSL
   * certificate" changed since, drop the cached client rather than carry the
   * old policy forward. */
  if (hc != NULL &&
      hc->hc_verify_peer != (imagecache_conf.ignore_sslcert ? 0 : 1)) {
    http_client_close(hc);
    hc = NULL;
    if (efd != NULL) {
      tvhpoll_destroy(efd);
      efd = NULL;
    }
  }

  attempted = 1;
  if (hc != NULL) {
    /* Re-issue on the kept-alive client: same origin reuses the open
     * connection, a different origin transparently reconnects. */
    hc->hc_handle_location = 1;
    hc->hc_data_limit  = 256*1024;
    /* unlike http_client_simple(), the reconnect entry point expects the
     * client lock to be held by the caller */
    tvh_mutex_lock(&hc->hc_mutex);
    r = http_client_simple_reconnect(hc, &url, HTTP_VERSION_1_1);
    tvh_mutex_unlock(&hc->hc_mutex);
    if (r < 0) {
      /* stale or broken cached connection: start completely afresh,
       * including the poll set */
      http_client_close(hc);
      hc = NULL;
      if (efd != NULL) {
        tvhpoll_destroy(efd);
        efd = NULL;
      }
    }
  }
  if (hc == NULL) {
    hc = http_client_connect(NULL, HTTP_VERSION_1_1, url.scheme,
                             url.host, url.port, NULL);
    if (hc == NULL)
      goto error_lock;

    http_client_ssl_peer_verify(hc, imagecache_conf.ignore_sslcert ? 0 : 1);
    if (reuse)
      hc->hc_hdr_create = imagecache_fetch_hdr_create;
    hc->hc_handle_location = 1;
    hc->hc_data_limit  = 256*1024;
    if (efd == NULL)
      efd = tvhpoll_create(1);
    hc->hc_efd = efd;

    r = http_client_simple(hc, &url);
    if (r < 0)
      goto error_lock;
  }

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
      conn_ok = 1;
      http_code = hc->hc_code;
      if (hc->hc_code == HTTP_STATUS_OK && hc->hc_data_size > 0)
        res = imagecache_new_contents(img, tpath, path,
                                      (uint8_t *)hc->hc_data, hc->hc_data_size);
      break;
    }
  }

  /* Process */
error_lock:
  /* Keep the healthy kept-alive connection for the next fetch,
   * dispose of anything else */
  keep = (hc != NULL && reuse && conn_ok && hc->hc_keepalive);
  if (!keep) {
    if (hc != NULL) http_client_close(hc);
    if (efd != NULL) tvhpoll_destroy(efd);
    hc = NULL;
    efd = NULL;
  }
  tvh_mutex_lock(&imagecache_lock);
  if (reuse) {
    imagecache_fetch_busy = 0;
    if (keep) {
      imagecache_fetch_hc  = hc;
      imagecache_fetch_efd = efd;
    }
  }
error:
  urlreset(&url);
  time(&img->updated); // even if failed (possibly request sooner?)
  if (res) {
    /* Transient failures (no HTTP response, or 5xx/429: typically a cold
     * CDN edge or a hiccup, not a property of the image) get a short
     * per-image retry before falling back to the hours-coarse
     * fail_period. */
    /* http_code stays 0 both when the server never answered and when the
     * fetch failed before any request -- a malformed URL, or the cache
     * directory unwritable.  Only the former is worth retrying soon. */
    if (((attempted && http_code == 0) || http_code == 429 || http_code >= 500) &&
        img->attempts < IMAGECACHE_TRANSIENT_RETRIES) {
      img->attempts++;
      tvhdebug(LS_IMAGECACHE, "transient failure (%d) for %s, retry %d/%d",
               http_code, img->url, img->attempts,
               IMAGECACHE_TRANSIENT_RETRIES);
      imagecache_image_retry_later(img);
      return res;
    }
    img->attempts = 0;
    if (!img->failed) {
      img->failed = 1;
      imagecache_image_save(img);
    }
    tvhwarn(LS_IMAGECACHE, "failed to download %s", img->url);
  } else {
    img->attempts = 0;
    tvhdebug(LS_IMAGECACHE, "downloaded %s", img->url);
  }
  tvh_cond_signal(&imagecache_cond, 1);

  return res;
};

/* Move due transient retries into the fetch queue and return the earlier of
 * `deadline` and the next pending retry (thread wake-up time). Lock held. */
static int64_t
imagecache_retry_promote ( int64_t deadline )
{
  imagecache_image_t *img;

  while ((img = TAILQ_FIRST(&imagecache_retry_queue)) != NULL &&
         img->retry_at <= mclk()) {
    TAILQ_REMOVE(&imagecache_retry_queue, img, q_link);
    img->state = IDLE;
    imagecache_image_add(img);
  }
  img = TAILQ_FIRST(&imagecache_retry_queue);
  if (img != NULL && img->retry_at < deadline)
    deadline = img->retry_at;
  return deadline;
}

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
    if (img->state != IDLE) continue; /* RETRY has its own schedule */
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
  int64_t wake;

  tvh_mutex_lock(&imagecache_lock);
  while (tvheadend_is_running()) {

    /* Timer expired */
    if (timer_expire < mclk()) {
      timer_expire = mclk() + sec2mono(600);
      imagecache_timer();
    }

    /* Promote due transient retries into the fetch queue */
    wake = imagecache_retry_promote(timer_expire);

    /* Get entry */
    if (!(img = TAILQ_FIRST(&imagecache_queue))) {
      /* queue drained: no need to keep an idle connection open.  Closing
       * drops the lock, and anything queued meanwhile -- a fetch or a
       * transient retry, whose due time 'wake' does not reflect -- had its
       * signal sent before we wait.  Start over rather than sleep on a stale
       * deadline; the next pass finds no connection and waits normally. */
      if (imagecache_fetch_conn_close())
        continue;
      tvh_cond_timedwait(&imagecache_cond, &imagecache_lock, wake);
      continue;
    }

    TAILQ_REMOVE(&imagecache_queue, img, q_link);

retry:
    if (img->state == SAVE) {
      /* Do save outside global mutex */
      htsmsg_t *m = imagecache_image_htsmsg(img);
      img->state = IDLE;
      img->savepend = 0;
      imagecache_incref(img);
      tvh_mutex_unlock(&imagecache_lock);
      hts_settings_save(m, "imagecache/meta/%d", img->id);
      htsmsg_destroy(m);
      tvh_mutex_lock(&imagecache_lock);
      /* "Clean image cache" can destroy the image while its record is being
       * written, leaving that record orphaned on disk.  The reference taken
       * above keeps img valid to check; ids only ever increase, so the
       * record cannot belong to another image yet. */
      if (img->deleted)
        hts_settings_remove("imagecache/meta/%d", img->id);
      imagecache_decref(img);

    } else if (img->state == QUEUED) {
      /* Fetch */
      imagecache_incref(img);
      if (imagecache_conf.enabled) {
        img->state = FETCHING;
        (void)imagecache_image_fetch(img);
      }
      if (img->state == RETRY) {
        /* transient failure: parked in the retry queue, leave it there */
        imagecache_decref(img);
        continue;
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
  imagecache_conf.reuse_conn     = 1;

  idclass_register(&imagecache_class);

  /* Create threads */
  tvh_cond_init(&imagecache_cond, 1);
  TAILQ_INIT(&imagecache_queue);
  TAILQ_INIT(&imagecache_retry_queue);

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
      /* the worker starts before the EPG is loaded: until something
       * re-references this image its priority is unknown, so keep it behind
       * the guide artwork rather than let it default to "asap" */
      img->airtime  = IMAGECACHE_AIRTIME_UNSET;
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
  /* unlink from whichever queue holds it: destroying a QUEUED/SAVE image
   * (e.g. "Clean image cache" while the fetch queue is filling) left a
   * dangling entry in the fetch queue behind */
  if (img->state == RETRY)
    TAILQ_REMOVE(&imagecache_retry_queue, img, q_link);
  else if (img->state == QUEUED || img->state == SAVE)
    TAILQ_REMOVE(&imagecache_queue, img, q_link);
  if (delconf) {
    img->deleted = 1;
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
  imagecache_fetch_conn_close();
  while ((img = RB_FIRST(&imagecache_by_id)) != NULL) {
    if (img->state == SAVE) {
      htsmsg_t *m = imagecache_image_htsmsg(img);
      hts_settings_save(m, "imagecache/meta/%d", img->id);
      htsmsg_destroy(m);
    }
    imagecache_destroy(img, 0);
  }
  SKEL_FREE(imagecache_skel);
  tvh_mutex_unlock(&imagecache_lock);
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
  return imagecache_get_id_prio(url, 0);
}

/*
 * As above, but with the earliest known airing time of the content the
 * image illustrates: the fetch queue is drained in ascending airtime order
 * so artwork for soon-airing events is downloaded first.
 */
int
imagecache_get_id_prio ( const char *url, int64_t airtime )
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

  tvh_mutex_lock(&imagecache_lock);

  /* Skeleton */
  SKEL_ALLOC(imagecache_skel);
  imagecache_skel->url = url;

  /* Create/Find */
  i = RB_INSERT_SORTED(&imagecache_by_url, imagecache_skel, url_link, url_cmp);
  if (!i) {
    i = imagecache_skel;
    i->ref = 1;
    i->url = strdup(url);
    i->airtime = airtime;
    SKEL_USED(imagecache_skel);
    save = 1;
    imagecache_new_id(i);
    imagecache_image_add(i);
  } else if (airtime < i->airtime) {
    /* Re-referenced with a higher priority: a sooner airing, or 0 (asap) for a
     * channel icon.  0 is the top priority and never gets replaced by a later
     * airing; an entry reloaded at start-up holds IMAGECACHE_AIRTIME_UNSET,
     * which any real reference replaces.  Reposition it when still waiting in
     * the fetch queue. */
    i->airtime = airtime;
    if (i->state == QUEUED) {
      TAILQ_REMOVE(&imagecache_queue, i, q_link);
      imagecache_image_enqueue(i);
    }
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
  return imagecache_get_propstr_prio(image, buf, buflen, 0);
}

/*
 * As above, for the image of an EPG event: rendering the event registers its
 * artwork at the event's airing time rather than as asap, so the order in
 * which clients walk the guide cannot reorder the fetch queue.
 */
const char *
imagecache_get_propstr_prio ( const char *image, char *buf, size_t buflen,
                              int64_t airtime )
{
  int id = imagecache_get_id_prio(image, airtime);
  if (id == 0) return image;
  snprintf(buf, buflen, "imagecache/%d", id);
  return buf;
}

/*
 * Get data
 */
/* Make a remote image available on disk for imagecache_filename(): wait for
 * an in-flight fetch, or fetch it directly when only queued. Lock held.
 * Returns 0 when the data file can be served. */
static int
imagecache_image_ready ( imagecache_image_t *i )
{
  int e;
  int64_t mono;

  /* Use existing */
  if (i->updated)
    return 0;

  /* Wait for the in-flight fetch */
  if (i->state == FETCHING) {
    mono = mclk() + sec2mono(5);
    do {
      e = tvh_cond_timedwait(&imagecache_cond, &imagecache_lock, mono);
      if (e == ETIMEDOUT)
        return -1;
    } while (ERRNO_AGAIN(e));
    return 0;
  }

  /* Attempt to fetch directly */
  if (i->state == QUEUED) {
    i->state = FETCHING;
    TAILQ_REMOVE(&imagecache_queue, i, q_link);
    e = imagecache_image_fetch(i);
    /* fetch may have parked the image for a transient retry; otherwise
     * return it to IDLE (it was left as FETCHING forever, so the
     * periodic re-check never considered it again) */
    if (i->state == FETCHING) {
      i->state = IDLE;
      /* the fetch recorded new metadata (url/updated/sha1) while FETCHING,
       * which only marks it pending; the worker persists that on its own
       * path, this one has to queue the write itself */
      if (i->savepend)
        imagecache_image_save(i);
    }
    return e ? -1 : 0;
  }

  return 0;
}

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
    if (imagecache_image_ready(i))
      goto out_error;
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
