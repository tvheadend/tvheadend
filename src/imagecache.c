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
  int64_t     fetch_prio; ///< Fetch ordering key (lower = sooner; EPG start time)
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

// The pace/pause fields are driven by the throttle in automatic mode -- show them
// read-only then (they reflect the live/locked values); editable when auto is off.
static uint32_t
imagecache_class_throttle_opts(void *o, uint32_t opts)
{
  const struct imagecache_config *cfg = o;
  if (cfg && cfg->throttle)
    return opts | PO_RDONLY;
  return opts;
}

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
      .id     = "throttle",
      .name   = N_("Automatic download throttling"),
      .desc   = N_("Pace image downloads and adapt automatically so the image "
                   "provider does not rate-limit/block Tvheadend (some providers "
                   "cap images per IP per time window). Images are fetched "
                   "soonest-airing first. After a provider block it pauses "
                   "fetching and escalates the pause until one is long enough for "
                   "the provider to allow images again, then LOCKS onto that pause "
                   "(it does not keep escalating into multi-hour waits). The two "
                   "fields below then show, read-only, the delay and pause it has "
                   "settled on. Adjustments are written to the log. Uncheck to set "
                   "the delay and pause yourself; leave both at 0 for no "
                   "throttling at all."),
      .off    = offsetof(struct imagecache_config, throttle),
    },
    {
      .type     = PT_U32,
      .id       = "fetch_delay",
      .name     = N_("Minimum delay between downloads (ms)"),
      .desc     = N_("Minimum time to wait between image downloads. In automatic "
                     "mode this shows (read-only) the pace the throttle is using. "
                     "Uncheck \"Automatic download throttling\" to set it yourself; "
                     "0 means no delay."),
      .off      = offsetof(struct imagecache_config, fetch_delay),
      .get_opts = imagecache_class_throttle_opts,
    },
    {
      .type     = PT_U32,
      .id       = "backoff_min",
      .name     = N_("Back-off pause on blocking (minutes)"),
      .desc     = N_("How long to pause fetching after the provider blocks "
                     "downloads. In automatic mode this shows (read-only) the "
                     "pause the throttle has locked onto. Uncheck \"Automatic "
                     "download throttling\" to set a fixed pause yourself; 0 means "
                     "do not pause."),
      .off      = offsetof(struct imagecache_config, backoff_min),
      .get_opts = imagecache_class_throttle_opts,
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
      .name   = N_("Expire time (days)"),
      .desc   = N_("The time in days after which the cached URL is "
                   "removed. The time starts when the URL was "
                   "last requested. Zero means unlimited cache "
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
// Monotonic time (mclk) of the last NEW fetch enqueue. The fetch thread waits
// for the queue to be quiet this long before draining, so an in-progress EPG
// grab has finished enqueueing and the queue is fully sorted by air time --
// otherwise the throttled thread drains the still-growing queue and follows the
// grab's (often future-first) order instead of soonest-airing first.
static int64_t                        imagecache_queue_changed;
// While the auto-throttle is in a back-off pause, this holds the mclk() at which
// it ends; the on-demand web fetch path checks it so a click does not hit the
// provider (and re-arm the block) during a pause. 0 = not backed off.
static int64_t                        imagecache_blocked_until;
// Set by imagecache_throttle_reset() (UI "Reset auto-throttle" button); the fetch
// thread re-initialises its throttle state and re-learns the provider budget.
static int                            imagecache_throttle_reset_req;
#define IMAGECACHE_FETCH_SETTLE_MS    3000
// Auto-throttle (the "Automatic download throttling" toggle). Providers that carry
// EPG artwork limit images per IP per time window (volume), so the sustainable rate
// is "B images per T minutes" (observed on tvtv.ca: ~50 / ~60 min). The throttle
// LEARNS that budget, then enforces it with a token bucket:
//   LEARN  -- drain fast (full burst) until blocked, then escalate a recovery pause
//             up the ladder until a pause reliably refills the window (a window
//             yields >= IMAGECACHE_BATCH_MIN images on IMAGECACHE_LOCK_CONFIRM tries
//             in a row); that gives B (batch size) and T (the working pause).
//   OPERATE-- on lock, compute the sustainable pace ms = T/B and switch to a token
//             bucket: refill one token every ms, capacity ~B; spend one per image.
//             A full bucket (after idle) lets a small update burst at full speed;
//             a large queue drains at the sustainable rate and never blocks again.
// IMAGECACHE_FAIL_BACKOFF_AT = consecutive failures that mean "blocked";
// IMAGECACHE_DELAY_MIN_MS = fastest pace (full speed / floor);
// IMAGECACHE_BURST_LEARN = effectively-unlimited bucket while still learning;
// IMAGECACHE_BATCH_MIN = images after a pause that count as a real (refilled) batch.
#define IMAGECACHE_FAIL_BACKOFF_AT    5
#define IMAGECACHE_DELAY_MIN_MS       250
#define IMAGECACHE_BURST_LEARN        100000
#define IMAGECACHE_BATCH_MIN          10
// good windows in a row at the same pause before locking (a single one can be a
// fluke: a failed window does not consume quota, so the quota may have refilled
// across earlier empty windows); also the bad windows in a row that release a lock.
#define IMAGECACHE_LOCK_CONFIRM       2
// Escalating pause (minutes) per back-off level (level 1 = first block, ...).
static const int imagecache_backoff_min[] = { 5, 10, 15, 30, 60, 120, 180, 360, 720, 1440 };
#define IMAGECACHE_BACKOFF_STEPS ((int)(sizeof(imagecache_backoff_min) / sizeof(int)))
// Pace optimization (binary search after lock): a probe pace is confirmed once it
// sustains IMAGECACHE_PROBE_FACTOR x B downloads in a row without a block; the search
// converges when the good/bad pace bounds are within ~1/IMAGECACHE_PROBE_EPS of each
// other (worst case it settles back on the originally learned pace).
#define IMAGECACHE_PROBE_FACTOR  2
#define IMAGECACHE_PROBE_EPS     10

// pause (minutes) for a back-off level: 0 at level 0, then the ladder. The pause
// must stay STRICTLY BELOW the "Re-try period" (fail_period) -- the 1440 (24h) top
// step is only reachable if Re-try is set beyond a day; once the ladder would meet
// or exceed Re-try the normal failed-image retry takes over, so we never wait that
// long here. Returns the largest ladder step < cap, bounded by the level.
static int
imagecache_backoff_pause(int level)
{
  int cap = (int)((imagecache_conf.fail_period ? imagecache_conf.fail_period : 24) * 60);
  int i;
  int m = 0;
  if (level > IMAGECACHE_BACKOFF_STEPS)
    level = IMAGECACHE_BACKOFF_STEPS;
  for (i = 0; i < level && imagecache_backoff_min[i] < cap; i++)
    m = imagecache_backoff_min[i];
  return m;
}

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

/* Insert into the fetch queue keeping it ordered by fetch_prio (soonest-airing
 * first) so the visible "now" guide gets its images before far-future days. */
static void
imagecache_queue_insert ( imagecache_image_t *img )
{
  imagecache_image_t *i;
  TAILQ_FOREACH(i, &imagecache_queue, q_link)
    if (i->fetch_prio > img->fetch_prio)
      break;
  if (i)
    TAILQ_INSERT_BEFORE(i, img, q_link);
  else
    TAILQ_INSERT_TAIL(&imagecache_queue, img, q_link);
}

static void
imagecache_image_save ( imagecache_image_t *img )
{
  img->savepend = 1;
  if (img->state != SAVE && img->state != QUEUED && img->state != FETCHING) {
    img->state = SAVE;
    imagecache_queue_insert(img);
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
      imagecache_queue_insert(img);
    // Mark a new fetch enqueue so the thread waits for the grab to settle.
    imagecache_queue_changed = mclk();
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

/* Fetch an image. Returns 0 on success, 1 on failure. If 'blocking' is non-NULL it
 * is set to 1 when the failure looks like the provider rate-limiting/blocking us
 * (403/429/5xx, a timeout or a dropped connection) and 0 for a plain missing image
 * (404/410) -- so a handful of broken EPG URLs do NOT look like a provider block and
 * trip the throttle. */
/* Pump the http client until the request completes; on completion set *code to the
 * HTTP status (data left in hc->hc_data) and return 1. Return 0 if it never
 * completed (shutdown or a transport error). */
static int
imagecache_http_run ( http_client_t *hc, tvhpoll_t *efd, int *code )
{
  tvhpoll_event_t ev;
  int r;

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
      *code = hc->hc_code;
      return 1;
    }
  }
  return 0;
}

static int
imagecache_image_fetch ( imagecache_image_t *img, int *blocking )
{
  int res = 1;
  int r;
  int code = 0;
  url_t url;
  char tpath[PATH_MAX + 4] = "", path[PATH_MAX];
  tvhpoll_t *efd = NULL;
  http_client_t *hc = NULL;

  urlinit(&url);

  lock_assert(&imagecache_lock);

  if (img->url == NULL || img->url[0] == '\0') {
    if (blocking) *blocking = 0;
    return res;
  }

  /* Open file  */
  if (hts_settings_buildpath(path, sizeof(path), "imagecache/data/%d",
                             img->id))
    goto error;
  if (hts_settings_makedirs(path))
    goto error;
  snprintf(tpath, sizeof(tpath), "%s.tmp", path);

  /* Fetch (release lock, incase of delays) */
  tvh_mutex_unlock(&imagecache_lock);

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

  if (imagecache_http_run(hc, efd, &code) &&
      code == HTTP_STATUS_OK && hc->hc_data_size > 0)
    res = imagecache_new_contents(img, tpath, path,
                                  (uint8_t *)hc->hc_data, hc->hc_data_size);

  /* Process */
error_lock:
  if (NULL != hc) http_client_close(hc);
  tvh_mutex_lock(&imagecache_lock);
error:
  urlreset(&url);
  tvhpoll_destroy(efd);
  time(&img->updated); // even if failed (possibly request sooner?)
  /* a missing image (404/410) is permanent and per-image -- do NOT let it look like
   * a provider block; anything else (incl. a connection-level failure, code 0) may
   * be rate-limiting */
  if (blocking)
    *blocking = res && code != HTTP_STATUS_NOT_FOUND && code != HTTP_STATUS_GONE;
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

// number of images currently waiting in the fetch queue (for the budget note)
static int
imagecache_queue_count ( void )
{
  imagecache_image_t *i;
  int n = 0;
  TAILQ_FOREACH(i, &imagecache_queue, q_link)
    n++;
  return n;
}

// sleep (releasing imagecache_lock) until 'until', or until shutdown
static void
imagecache_wait_until ( int64_t until )
{
  while (mclk() < until && tvheadend_is_running())
    tvh_cond_timedwait(&imagecache_cond, &imagecache_lock, until);
}

/* ---- download throttle (see the block comment by the constants). Kept out of
 * the fetch thread so that loop stays simple. All run holding imagecache_lock and
 * may release it via tvh_cond_timedwait. ---- */
typedef struct imagecache_throttle {
  int     fails;         /* consecutive blocking failures (>= AT means "blocked") */
  int     level;         /* recovery-pause escalation level (0 = none yet) */
  int     locked;        /* OPERATE: the budget (B per T) has been learned */
  int     good;          /* good windows in a row at this pause (toward locking) */
  int     bad;           /* blocks in a row while locked (toward re-learning) */
  int     batch;         /* successes since the current window opened */
  int     announced;     /* logged the baseline once */
  int     pace_ms;       /* token-bucket refill interval (sustainable pace) */
  double  burst_cap;     /* bucket capacity */
  double  tokens;        /* available tokens */
  int64_t tok_last;      /* last token refill time */
  int     budget_logged; /* logged the "within budget" note for this drain */
  int     optimize;      /* OPTIMIZE: binary-searching a faster sustainable pace */
  int     target_run;    /* consecutive successes that confirm a probe pace (2*B) */
  int     ms_good;       /* slowest pace proven to sustain target_run */
  int     ms_bad;        /* fastest pace that failed (0 = none yet) */
} imagecache_throttle_t;

/* Single shared instance: the fetch thread paces against it, and the on-demand web
 * path debits it (imagecache_throttle_charge) so both consume one provider budget. */
static imagecache_throttle_t imagecache_throttle;

static void
imagecache_throttle_init ( imagecache_throttle_t *t )
{
  memset(t, 0, sizeof(*t));
  t->pace_ms   = IMAGECACHE_DELAY_MIN_MS;
  t->burst_cap = IMAGECACHE_BURST_LEARN;
  t->tokens    = IMAGECACHE_BURST_LEARN;
  t->tok_last  = mclk();
  /* Resume near the last learned operating point instead of re-learning from
   * scratch on every restart: load the saved pause and start the back-off ladder
   * at its rung, so the first window re-confirms and re-locks there rather than
   * climbing 5->10->15->... again. A block still re-escalates if the provider got
   * stingier. The learned point is kept in a standalone settings file because the
   * idnode config is not saved from this (fetch) thread (lock ordering). */
  if (imagecache_conf.throttle) {
    htsmsg_t *m = hts_settings_load("imagecache/throttle");
    if (m) {
      uint32_t bo = htsmsg_get_u32_or_default(m, "backoff_min", 0);
      int64_t pm = htsmsg_get_s64_or_default(m, "pace_ms", 0);
      int i;
      for (i = 0; i < IMAGECACHE_BACKOFF_STEPS; i++)
        if (imagecache_backoff_min[i] <= (int)bo)
          t->level = i + 1;
      if (pm >= IMAGECACHE_DELAY_MIN_MS) {
        /* resume directly at the saved (possibly optimized) pace -- a block will
         * re-learn and re-optimize, so we never re-probe needlessly on restart. */
        t->locked = 1;
        t->pace_ms = (int)pm;
        t->burst_cap = 1;
        imagecache_conf.fetch_delay = t->pace_ms;
      }
      htsmsg_destroy(m);
    }
  }
}

/* Wait (releasing the lock) for permission to issue the next fetch: a token in
 * automatic mode, the fixed delay in manual mode. */
static void
imagecache_throttle_acquire ( imagecache_throttle_t *t )
{
  int64_t now;

  if (!imagecache_conf.throttle) {
    if (imagecache_conf.fetch_delay)
      imagecache_wait_until(mclk() + ms2mono((int64_t)imagecache_conf.fetch_delay));
    return;
  }

  if (!t->announced) {
    tvhinfo(LS_IMAGECACHE, "auto-throttle: learning provider budget "
            "(pacing %d ms/image until blocked)", t->pace_ms);
    imagecache_conf.fetch_delay = t->pace_ms;
    imagecache_conf.backoff_min = imagecache_backoff_pause(t->level);
    t->announced = 1;
  }
  /* refill the bucket for the idle time since the last fetch */
  now = mclk();
  t->tokens += (double)(now - t->tok_last) / (double)((int64_t)t->pace_ms * 1000);
  /* Burst only when caught up. While a real backlog (> burst_cap) is still
   * draining, cap the bucket to ONE token so we pace evenly (T/B) instead of
   * dumping a burst that overruns the provider's rolling window and triggers a
   * block + recovery pause every cycle. The full burst is allowed only when the
   * remaining queue is small (<= burst_cap) -- a guide that has caught up taking
   * a small EPG update at full speed. */
  {
    double cap = (t->locked && imagecache_queue_count() > t->burst_cap)
                 ? 1.0 : t->burst_cap;
    if (t->tokens > cap)
      t->tokens = cap;
  }
  t->tok_last = now;
  /* once the budget is known, note when a whole drain fits inside it */
  if (t->locked && !t->budget_logged) {
    int q = imagecache_queue_count() + 1;     /* +1: this one is already out */
    if ((double)q <= t->tokens) {
      tvhinfo(LS_IMAGECACHE, "auto-throttle: %d image(s) queued, within budget "
              "(~%d/window) -- full speed", q, (int)t->burst_cap);
      t->budget_logged = 1;
    }
  }
  /* acquire one token, pacing (waiting) while the bucket is below one. We loop and
   * re-refill over real time instead of forcing tokens to 0 after the wait, so an
   * on-demand fetch that debits tokens (possibly negative) DURING the wait is repaid
   * here rather than discarded -- the combined thread+on-demand rate stays within the
   * provider window. */
  while (t->tokens < 1.0 && tvheadend_is_running()) {
    imagecache_wait_until(mclk() + ms2mono((int64_t)((1.0 - t->tokens) * t->pace_ms)));
    now = mclk();
    t->tokens += (double)(now - t->tok_last) / (double)((int64_t)t->pace_ms * 1000);
    t->tok_last = now;
  }
  t->tokens -= 1.0;
}

// step the recovery pause up one rung, while it would actually grow (not yet capped)
static void
imagecache_throttle_escalate ( imagecache_throttle_t *t )
{
  if (t->level < IMAGECACHE_BACKOFF_STEPS &&
      imagecache_backoff_pause(t->level + 1) > imagecache_backoff_pause(t->level))
    t->level++;
}

/* Persist the learned/optimized operating point (standalone file, not the idnode
 * config which cannot be saved from the fetch thread -- lock ordering). */
static void
imagecache_throttle_persist ( int pace_ms, int backoff_min )
{
  htsmsg_t *sm = htsmsg_create_map();
  htsmsg_add_s64(sm, "pace_ms", pace_ms);
  htsmsg_add_u32(sm, "backoff_min", (uint32_t)backoff_min);
  hts_settings_save(sm, "imagecache/throttle");
  htsmsg_destroy(sm);
}

/* OPTIMIZE: choose the next pace to probe -- binary search between the fastest pace
 * that failed (ms_bad, or the floor) and the slowest that sustained target_run
 * (ms_good) -- or, once converged, lock onto the slowest-known-good pace. */
static void
imagecache_throttle_probe_next ( imagecache_throttle_t *t )
{
  int lo = t->ms_bad ? t->ms_bad : IMAGECACHE_DELAY_MIN_MS;
  int next;
  if (t->ms_good - lo <= t->ms_good / IMAGECACHE_PROBE_EPS ||
      t->ms_good <= IMAGECACHE_DELAY_MIN_MS) {
    t->optimize = 0;
    t->pace_ms = t->ms_good;
    imagecache_conf.fetch_delay = t->pace_ms;
    imagecache_throttle_persist(t->pace_ms, imagecache_backoff_pause(t->level));
    tvhinfo(LS_IMAGECACHE, "auto-throttle: pace optimized -> %d ms/image "
            "(~%d images/hour sustained)", t->pace_ms, 3600000 / (t->pace_ms ? t->pace_ms : 1));
    t->batch = 0;
    return;
  }
  next = t->ms_bad ? (t->ms_bad + t->ms_good) / 2 : t->ms_good / 2;
  if (next < IMAGECACHE_DELAY_MIN_MS)
    next = IMAGECACHE_DELAY_MIN_MS;
  t->pace_ms = next;
  t->batch = 0;
  tvhinfo(LS_IMAGECACHE, "auto-throttle: probing %d ms/image (good=%d bad=%d, "
          "need %d in a row)", next, t->ms_good, t->ms_bad, t->target_run);
}

/* On a confirmed block, update the learn/lock state and return the recovery pause
 * (minutes). */
static int
imagecache_throttle_on_block ( imagecache_throttle_t *t )
{
  int good = (t->level > 0 && t->batch >= IMAGECACHE_BATCH_MIN);

  t->budget_logged = 0;
  if (t->locked && t->optimize) {
    /* OPTIMIZE: this probe pace blocked before the target run -> it is too fast. */
    t->ms_bad = t->pace_ms;
    tvhinfo(LS_IMAGECACHE, "auto-throttle: %d ms/image too fast (blocked after %d) "
            "-> pausing %d min", t->pace_ms, t->batch, imagecache_backoff_pause(t->level));
    imagecache_throttle_probe_next(t);
  } else if (t->locked) {
    /* OPERATE: a block slipped through -> the learned pace is too fast. ANY block
     * counts; after LOCK_CONFIRM in a row, drop the lock and re-learn one rung
     * slower (t->bad is cleared on a clean burst in imagecache_throttle_after). */
    if (++t->bad >= IMAGECACHE_LOCK_CONFIRM) {
      t->locked = 0; t->bad = 0; t->good = 0;
      t->pace_ms = IMAGECACHE_DELAY_MIN_MS;
      t->burst_cap = IMAGECACHE_BURST_LEARN;
      imagecache_throttle_escalate(t);
      tvhwarn(LS_IMAGECACHE, "auto-throttle: provider tightened -> re-learning, "
              "pausing %d min", imagecache_backoff_pause(t->level));
    } else {
      tvhinfo(LS_IMAGECACHE, "auto-throttle: quota reached -> pausing %d min to "
              "refill the window", imagecache_backoff_pause(t->level));
    }
  } else if (good) {
    /* a window after a pause delivered a real batch; confirm before locking (one
     * good window can be a fluke -- a failed window delivers 0 but consumes no
     * quota, so the quota may have refilled across earlier empty windows) */
    if (++t->good >= IMAGECACHE_LOCK_CONFIRM) {
      int b = t->batch;
      int tm = imagecache_backoff_pause(t->level);
      t->locked = 1; t->bad = 0;
      /* one image per (T / B); keep a 25% burst margin under the limit */
      t->pace_ms = (int)((int64_t)tm * 60 * 1000 / (b > 0 ? b : 1));
      if (t->pace_ms < IMAGECACHE_DELAY_MIN_MS)
        t->pace_ms = IMAGECACHE_DELAY_MIN_MS;
      t->burst_cap = (b * 3 / 4) >= 1 ? (b * 3 / 4) : 1;
      imagecache_conf.fetch_delay = t->pace_ms;
      tvhinfo(LS_IMAGECACHE, "auto-throttle: locked -- budget ~%d images / %d min "
              "-> pacing %d ms/image, burst %d", b, tm, t->pace_ms, (int)t->burst_cap);
      imagecache_throttle_persist(t->pace_ms, tm);
      /* Now binary-search a faster sustainable pace: the provider may allow more than
       * the learned budget if paced differently. Worst case settles back here. */
      t->target_run = b * IMAGECACHE_PROBE_FACTOR;
      t->ms_good = t->pace_ms;
      t->ms_bad = 0;
      if (t->pace_ms / 2 >= IMAGECACHE_DELAY_MIN_MS) {
        t->optimize = 1;
        imagecache_throttle_probe_next(t);
      }
    } else {
      tvhinfo(LS_IMAGECACHE, "auto-throttle: pausing %d min works (~%d images), "
              "confirming", imagecache_backoff_pause(t->level), t->batch);
    }
  } else {
    /* unproven pause too short (quota not refilled) -> wait longer next time */
    t->good = 0;
    imagecache_throttle_escalate(t);
    if (t->level == 0)
      t->level = 1;            /* first block: start the ladder */
    tvhwarn(LS_IMAGECACHE, "auto-throttle: provider blocking -> pausing %d min",
            imagecache_backoff_pause(t->level));
  }
  return imagecache_backoff_pause(t->level);
}

/* Pause the fetch thread for the given minutes, publishing the window for the
 * on-demand path. */
static void
imagecache_throttle_pause ( int pause_min )
{
  imagecache_conf.backoff_min = pause_min;
  imagecache_blocked_until = mclk() + sec2mono((int64_t)pause_min * 60);
  imagecache_wait_until(imagecache_blocked_until);
  imagecache_blocked_until = 0;
}

/* Account for a fetch result; on a confirmed block, recover (pause). */
static void
imagecache_throttle_after ( imagecache_throttle_t *t, int failed, int blocking )
{
  int block = failed && blocking;

  if (!imagecache_conf.throttle) {
    if (!block) {
      t->fails = 0;
    } else if (++t->fails >= IMAGECACHE_FAIL_BACKOFF_AT && imagecache_conf.backoff_min) {
      tvhwarn(LS_IMAGECACHE, "throttle (manual): provider blocking -> %u ms/req, "
              "pausing %u min", imagecache_conf.fetch_delay, imagecache_conf.backoff_min);
      imagecache_throttle_pause(imagecache_conf.backoff_min);
      t->fails = 0;
    }
    return;
  }

  if (block) {
    if (++t->fails >= IMAGECACHE_FAIL_BACKOFF_AT) {
      imagecache_throttle_pause(imagecache_throttle_on_block(t));
      t->fails = 0;
      t->batch = 0;               /* a fresh window opens after the pause */
      t->tokens = t->burst_cap;   /* the window refilled during the pause */
      t->tok_last = mclk();
    }
  } else {
    /* success, or a permanent per-image failure (404/410) -- not a block */
    t->fails = 0;
    t->batch++;
    if (t->locked && t->optimize) {
      if (t->batch >= t->target_run) {  /* probe sustained 2*B -> good, try faster */
        t->ms_good = t->pace_ms;
        imagecache_throttle_probe_next(t);
      }
    } else if (t->locked && t->batch >= (int)t->burst_cap) {
      t->bad = 0;                 /* a clean burst since the last block */
    }
  }
}

/* Account an out-of-band (on-demand web) fetch against the shared budget. The
 * on-demand path fetches immediately -- the active user must not wait -- but it still
 * consumed a provider slot, so we debit one token (tokens may go negative). The pacer
 * then waits proportionally longer to repay it, keeping the combined thread+on-demand
 * rate within the provider window instead of overrunning it into a recovery pause. */
static void
imagecache_throttle_charge ( imagecache_throttle_t *t )
{
  int64_t now;
  double cap;
  if (!imagecache_conf.throttle)
    return;
  now = mclk();
  t->tokens += (double)(now - t->tok_last) / (double)((int64_t)t->pace_ms * 1000);
  cap = (t->locked && imagecache_queue_count() > t->burst_cap) ? 1.0 : t->burst_cap;
  if (t->tokens > cap)
    t->tokens = cap;
  t->tok_last = now;
  t->tokens -= 1.0;
  if (t->tokens < -t->burst_cap)   /* bound the catch-up so the pacer cannot */
    t->tokens = -t->burst_cap;       /* stall for more than ~one window */
}

/* An on-demand fetch hit the provider's wall itself: nothing to do but back off. Arm
 * the same recovery pause the thread uses (while it holds, further on-demand requests
 * are served 404 and the queued image is fetched afterwards) and void the catch-up
 * debt -- moot now that everything waits for the window to refill. The learn/optimize
 * state is left untouched: the extra on-demand load caused this, not the thread. */
static void
imagecache_throttle_ondemand_blocked ( imagecache_throttle_t *t )
{
  if (!imagecache_conf.throttle) {
    if (imagecache_conf.backoff_min)
      imagecache_throttle_pause(imagecache_conf.backoff_min);
    return;
  }
  imagecache_throttle_pause(imagecache_backoff_pause(t->level));
  t->tokens = t->burst_cap;   /* the window refills during the pause */
  t->tok_last = mclk();
}

static void *
imagecache_thread ( void *p )
{
  imagecache_image_t *img;
  int pass_ok = 0;     /* fetched OK since the queue last drained */
  int pass_fail = 0;   /* failed (404/410) since the queue last drained */
  int64_t timer_expire = mclk() + sec2mono(600);

  imagecache_throttle_init(&imagecache_throttle);
  tvh_mutex_lock(&imagecache_lock);
  while (tvheadend_is_running()) {

    /* UI "Reset auto-throttle" button: drop the learned state and re-learn. */
    if (imagecache_throttle_reset_req) {
      imagecache_throttle_reset_req = 0;
      imagecache_throttle_init(&imagecache_throttle);
    }

    /* Timer expired */
    if (timer_expire < mclk()) {
      timer_expire = mclk() + sec2mono(600);
      imagecache_timer();
    }

    /* Get entry */
    if (!(img = TAILQ_FIRST(&imagecache_queue))) {
      imagecache_throttle.budget_logged = 0;  /* queue drained -> re-evaluate next time */
      if (pass_ok || pass_fail) {  /* a fetch pass just finished (initial fill or EPG update) */
        tvhinfo(LS_IMAGECACHE, "fetch queue drained -- %d image(s) fetched, %d failed",
                pass_ok, pass_fail);
        pass_ok = pass_fail = 0;
      }
      tvh_cond_timedwait(&imagecache_cond, &imagecache_lock, timer_expire);
      continue;
    }

    /* Always let the queue settle (e.g. an EPG grab finish enqueueing) before
     * draining, so it is fully sorted by air time and the soonest-airing images
     * are fetched first instead of whatever the grab happened to enqueue first.
     * This "now-first" ordering applies regardless of the throttle. Re-evaluate
     * after each new enqueue. */
    {
      int64_t settled = imagecache_queue_changed + ms2mono(IMAGECACHE_FETCH_SETTLE_MS);
      if (mclk() < settled) {
        tvh_cond_timedwait(&imagecache_cond, &imagecache_lock, settled);
        continue;
      }
    }

    TAILQ_REMOVE(&imagecache_queue, img, q_link);

retry:
    if (img->state == SAVE) {
      /* Do save outside global mutex */
      htsmsg_t *m = imagecache_image_htsmsg(img);
      int id = img->id;
      imagecache_image_t skel;
      img->state = IDLE;
      img->savepend = 0;
      tvh_mutex_unlock(&imagecache_lock);
      hts_settings_save(m, "imagecache/meta/%d", id);
      htsmsg_destroy(m);
      tvh_mutex_lock(&imagecache_lock);
      /* A concurrent imagecache_destroy() (clean/expire) may have removed the
       * image and its data file while the lock was released -- do not leave a
       * stale metadata file with no data behind. */
      skel.id = id;
      if (!RB_FIND(&imagecache_by_id, &skel, id_link, id_cmp))
        hts_settings_remove("imagecache/meta/%d", id);

    } else if (img->state == QUEUED) {
      /* Fetch: pace (throttle) before issuing it, account for the result after. */
      imagecache_incref(img);
      if (imagecache_conf.enabled) {
        int failed;
        int blocking = 0;
        img->state = FETCHING;     /* claim it before releasing the lock to pace */
        imagecache_throttle_acquire(&imagecache_throttle);
        failed = imagecache_image_fetch(img, &blocking);
        imagecache_throttle_after(&imagecache_throttle, failed, blocking);
        if (failed && blocking) {
          /* A provider block (rate-limit / 5xx / timeout), NOT a missing image:
           * do not mark it failed and defer it for the whole fail_period. Clear the
           * failed mark and re-queue so it retries after the back-off pause, keeping
           * its now-first airing priority. (404/410 keep failed -> fail_period, so we
           * never loop on genuinely-missing images.) */
          img->failed = 0;
          img->updated = 0;
          img->savepend = 0;
          img->state = QUEUED;
          imagecache_queue_insert(img);
          imagecache_decref(img);
          continue;
        }
        if (failed) pass_fail++; else pass_ok++;  /* resolved this pass */
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
  imagecache_conf.throttle       = 1;      // automatic throttling (on by default)
  imagecache_conf.fetch_delay    = IMAGECACHE_DELAY_MIN_MS;  // manual-mode default pace
  imagecache_conf.backoff_min    = 30;     // manual-mode default pause (minutes)

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
  /* If the image is still linked in the fetch queue (state QUEUED or a pending
   * SAVE), unlink it BEFORE decref: the queue holds no reference, so decref may
   * free() it and leave a dangling TAILQ entry that the fetch thread later
   * dereferences -- a use-after-free that aborts the process. This is latent in
   * stock tvheadend (a clean/purge racing the fetch thread) but the adaptive
   * back-off makes it likely: images sit QUEUED for the whole pause, so a clean
   * during a back-off almost always hits one. */
  if (img->state == QUEUED || img->state == SAVE)
    TAILQ_REMOVE(&imagecache_queue, img, q_link);
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
  tvhinfo(LS_IMAGECACHE, "config saved -- throttle %s, %u ms/image, %u min pause",
          imagecache_conf.throttle ? "automatic" : "manual",
          imagecache_conf.fetch_delay, imagecache_conf.backoff_min);
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
 * Reset the auto-throttle: drop the learned provider budget and re-learn it.
 */
void
imagecache_throttle_reset( void )
{
  tvh_mutex_lock(&imagecache_lock);
  hts_settings_remove("imagecache/throttle");      /* drop the persisted lock */
  imagecache_throttle_reset_req = 1;                 /* fetch thread re-learns */
  imagecache_conf.fetch_delay = IMAGECACHE_DELAY_MIN_MS;
  imagecache_conf.backoff_min = 0;
  tvh_cond_signal(&imagecache_cond, 1);
  tvh_mutex_unlock(&imagecache_lock);
  tvhinfo(LS_IMAGECACHE, "auto-throttle: reset -- re-learning the provider budget");
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

int
imagecache_get_id_prio ( const char *url, int64_t prio )
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
    SKEL_USED(imagecache_skel);
    save = 1;
    i->fetch_prio = prio;
    imagecache_new_id(i);
    imagecache_image_add(i);
  } else if (prio > 0 && prio < i->fetch_prio) {
    /* prio==0 is a get_propstr() serialization/icon lookup (guide load, Kodi, API)
     * and must NOT reset an EPG image's airing priority -- else every guide view
     * flattens the whole queue to FIFO and now-first dies. Only a real airing time
     * (prio > 0) may lower it.
     * Shared image (same URL) now wanted for a sooner-airing programme: raise
     * its fetch priority. If it is linked in the fetch queue (QUEUED, or a pending
     * SAVE -- both sit in imagecache_queue and we hold imagecache_lock), re-sort it
     * so the "soonest-airing first" order, and the sorted-insert invariant, stay
     * correct; otherwise just update the value. */
    if (i->state == QUEUED || i->state == SAVE) {
      TAILQ_REMOVE(&imagecache_queue, i, q_link);
      i->fetch_prio = prio;
      imagecache_queue_insert(i);
    } else {
      i->fetch_prio = prio;
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
  int id = imagecache_get_id(image);
  if (id == 0) return image;
  snprintf(buf, buflen, "imagecache/%d", id);
  return buf;
}

/*
 * Resolve the on-disk path for a remote (cached) image, fetching it on demand if
 * needed. Returns 0 on success, -1 if not (yet) available. Caller holds the lock.
 */
static int
imagecache_filename_remote ( imagecache_image_t *i, char *name, size_t len )
{
  int e;
  int64_t mono;

  /* Already cached -> fall through and return its path. Otherwise wait for an
   * in-progress fetch to finish, or fetch the image on demand. */
  if (!i->updated) {
    if (i->state == FETCHING) {
      /* loop on the predicate, not on one wakeup: imagecache_cond is signalled for
       * any image's progress, so a bare wait could return on an unrelated signal
       * and we would serve a not-yet-written file. */
      mono = mclk() + sec2mono(5);
      while (i->state == FETCHING && tvheadend_is_running()) {
        e = tvh_cond_timedwait(&imagecache_cond, &imagecache_lock, mono);
        if (e == ETIMEDOUT)
          return -1;
      }
      if (!i->updated)      /* the fetch finished but failed -> nothing to serve */
        return -1;

    } else if (i->state == QUEUED) {
      /* If the throttle is in a back-off pause (provider blocking), do NOT fetch on
       * demand: it would fail anyway and re-arm the block. Leave it queued (the
       * fetch thread gets it after the pause) and serve a 404 for now. Applies to
       * both automatic and manual pauses -- imagecache_blocked_until is non-zero
       * only while a pause is active. */
      if (imagecache_blocked_until && mclk() < imagecache_blocked_until) {
        tvhdebug(LS_IMAGECACHE, "on-demand fetch skipped, backed off: %s", i->url);
        return -1;
      }
      int blocking = 0;
      i->state = FETCHING;
      TAILQ_REMOVE(&imagecache_queue, i, q_link);
      e = imagecache_image_fetch(i, &blocking);
      if (!e) {
        /* served immediately (low latency for the active user) but it used a provider
         * slot -> debit the shared budget so the pacer waits one extra interval. */
        imagecache_throttle_charge(&imagecache_throttle);
      } else if (blocking) {
        /* the on-demand fetch itself hit the provider wall -> back off and re-queue it
         * so the thread retries after the pause; further on-demand is served 404. */
        imagecache_throttle_ondemand_blocked(&imagecache_throttle);
        i->failed = 0;
        i->state = QUEUED;
        imagecache_queue_insert(i);
      }
      /* The fetch leaves the image FETCHING and may have flagged a metadata save.
       * Persist it INLINE rather than re-queueing: the fetch queue holds no ref, so a
       * still-queued image freed by a concurrent imagecache_destroy() would dangle
       * (use-after-free). We hold a ref on i (imagecache_filename incref'd it), so it
       * stays alive across the unlock; saving outside the lock matches the thread. */
      if (i->state == FETCHING)
        i->state = IDLE;
      if (i->savepend) {
        htsmsg_t *m = imagecache_image_htsmsg(i);
        i->savepend = 0;
        tvh_mutex_unlock(&imagecache_lock);
        hts_settings_save(m, "imagecache/meta/%d", i->id);
        htsmsg_destroy(m);
        tvh_mutex_lock(&imagecache_lock);
        /* if a concurrent destroy() removed the image while unlocked, drop the
         * orphan metadata we just wrote (its data file is already gone). */
        if (!RB_FIND(&imagecache_by_id, i, id_link, id_cmp))
          hts_settings_remove("imagecache/meta/%d", i->id);
      }
      if (e)
        return -1;
    }
  }
  if (hts_settings_buildpath(name, len, "imagecache/data/%d", i->id))
    return -1;
  return 0;
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

  /* Remote file (fetch on demand if needed) */
  } else if (imagecache_conf.enabled &&
             imagecache_filename_remote(i, name, len)) {
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
