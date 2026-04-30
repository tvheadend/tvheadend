/*
 *  IPTV Input
 *
 *  Copyright (C) 2013 Andreas Öman
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

#include <signal.h>
#include <fcntl.h>

#include "iptv_private.h"
#include "tvhpoll.h"
#include "tcp.h"
#include "settings.h"
#include "channels.h"
#include "packet.h"
#include "config.h"

/* **************************************************************************
 * IPTV state
 * *************************************************************************/

tvh_mutex_t iptv_lock;

typedef struct iptv_thread_pool {
  TAILQ_ENTRY(iptv_thread_pool) link;
  pthread_t thread;
  iptv_input_t *input;
  tvhpoll_t *poll;
  th_pipe_t pipe;
  uint32_t streams;
} iptv_thread_pool_t;

TAILQ_HEAD(, iptv_thread_pool) iptv_tpool;
int iptv_tpool_count = 0;
iptv_thread_pool_t *iptv_tpool_last = NULL;
gtimer_t iptv_tpool_manage_timer;

static void iptv_input_thread_manage_cb(void *aux);

static inline int iptv_tpool_safe_count(void)
{
  return MINMAX(config.iptv_tpool_count, 1, 128);
}

/* **************************************************************************
 * IPTV handlers
 * *************************************************************************/

static RB_HEAD(,iptv_handler) iptv_handlers;

static int
ih_cmp ( iptv_handler_t *a, iptv_handler_t *b )
{
  return strcasecmp(a->scheme ?: "", b->scheme ?: "");
}

void
iptv_handler_register ( iptv_handler_t *ih, int num )
{
  iptv_handler_t *r;
  while (num) {
    r = RB_INSERT_SORTED(&iptv_handlers, ih, link, ih_cmp);
    if (r)
      tvhwarn(LS_IPTV, "attempt to re-register handler for %s",
              ih->scheme);
    num--;
    ih++;
  }
}

static iptv_handler_t *
iptv_handler_find ( const char *scheme )
{
  iptv_handler_t ih;
  ih.scheme = scheme;

  return RB_FIND(&iptv_handlers, &ih, link, ih_cmp);
}

/* **************************************************************************
 * IPTV input
 * *************************************************************************/

static int
iptv_input_thread_number ( iptv_input_t *mi )
{
  iptv_thread_pool_t *pool;
  int num = 1;

  TAILQ_FOREACH(pool, &iptv_tpool, link) {
    if (pool->input == mi)
      break;
    num++;
  }
  return num;
}

static void
iptv_input_class_get_title
  ( idnode_t *self, const char *lang, char *dst, size_t dstsize )
{
  int num = iptv_input_thread_number((iptv_input_t *)self);
  snprintf(dst, dstsize, "%s%d", tvh_gettext_lang(lang, N_("IPTV thread #")), num);
}

extern const idclass_t mpegts_input_class;
const idclass_t iptv_input_class = {
  .ic_super      = &mpegts_input_class,
  .ic_class      = "iptv_input",
  .ic_caption    = N_("IPTV input"),
  .ic_get_title  = iptv_input_class_get_title,
  .ic_properties = (const property_t[]){
    {}
  }
};

typedef struct {
  uint32_t active:1;
  uint32_t weight:1;
  uint32_t warm:1;
} iptv_is_free_t;

static mpegts_mux_instance_t *
iptv_input_is_free ( mpegts_input_t *mi, mpegts_mux_t *mm,
                     iptv_is_free_t *conf, int weight, int *lweight )
{
  int h = 0, l = 0, mux_running = 0, w, rw = INT_MAX;
  mpegts_mux_instance_t *mmi, *rmmi = NULL;
  iptv_input_t *mi2;
  iptv_network_t *in = (iptv_network_t *)mm->mm_network;
  iptv_thread_pool_t *pool;

  TAILQ_FOREACH(pool, &iptv_tpool, link) {
    mi2 = pool->input;
    tvh_mutex_lock(&mi2->mi_output_lock);
    LIST_FOREACH(mmi, &mi2->mi_mux_active, mmi_active_link)
      if (mmi->mmi_mux->mm_network == (mpegts_network_t *)in) {
        mux_running = mux_running || mmi->mmi_mux == mm;
        w = mpegts_mux_instance_weight(mmi);
        if (w < rw && (!conf->active || mmi->mmi_mux != mm)) {
          rmmi = mmi;
          rw = w;
        }
        if (w >= weight) h++; else l++;
      }
    tvh_mutex_unlock(&mi2->mi_output_lock);
  }

  /* If we are checking if an input is free, return null if the mux's input is already running */
  if (!conf->active && !conf->weight && !conf->warm && mux_running)
    return NULL;

  tvhtrace(LS_IPTV_SUB, "is free[%p]: h = %d, l = %d, rw = %d", mm, h, l, rw);

  if (lweight)
    *lweight = rw == INT_MAX ? 0 : rw;

  if (!rmmi)
    return NULL;

  /* Limit reached */
  w = h;
  if (conf->weight || conf->warm) w += l;
  if (in->in_max_streams && w >= in->in_max_streams)
    if (rmmi->mmi_mux != mm)
      return rmmi;
  
  /* Bandwidth reached */
  if (in->in_bw_limited && l == 0)
    if (rmmi->mmi_mux != mm)
      return rmmi;

  return NULL;
}

static int
iptv_input_thread_balance(iptv_input_t *mi)
{
  iptv_thread_pool_t *pool, *apool = mi->mi_tpool;

  /*
   * select input with the smallest count of active threads
   */
  TAILQ_FOREACH(pool, &iptv_tpool, link)
    if (pool->streams < apool->streams)
      return 1;
  return 0;
}

static int
iptv_input_is_enabled
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags, int weight )
{
  int r;
  mpegts_mux_instance_t *mmi;
  iptv_is_free_t conf = { .active = 0, .weight = 0, .warm = 0 };

  r = mpegts_input_is_enabled(mi, mm, flags, weight);
  if (r != MI_IS_ENABLED_OK) {
    tvhtrace(LS_IPTV_SUB, "enabled[%p]: generic %d", mm, r);
    return r;
  }
  if (iptv_input_thread_balance((iptv_input_t *)mi)) {
    tvhtrace(LS_IPTV_SUB, "enabled[%p]: balance", mm);
    return MI_IS_ENABLED_RETRY;
  }
  mmi = iptv_input_is_free(mi, mm, &conf, weight, NULL);
  tvhtrace(LS_IPTV_SUB, "enabled[%p]: free %p", mm, mmi);
  return mmi == NULL ? MI_IS_ENABLED_OK : MI_IS_ENABLED_RETRY;
}

static int
iptv_input_get_weight
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags, int weight )
{
  int w;
  mpegts_mux_instance_t *mmi;
  iptv_is_free_t conf = { .active = 1, .weight = 1, .warm = 0 };

  /* Find the "min" weight */
  mmi = iptv_input_is_free(mi, mm, &conf, weight, &w);
  tvhtrace(LS_IPTV_SUB, "get weight[%p]: %p (%d)", mm, mmi, w);
  return mmi == NULL ? 0 : w;

}

static int
iptv_input_get_grace ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  iptv_mux_t *im = (iptv_mux_t *)mm;
  iptv_network_t *in = (iptv_network_t *)im->mm_network;
  return in->in_max_timeout;
}

static int
iptv_input_get_priority ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags )
{
  iptv_mux_t *im = (iptv_mux_t *)mm;
  iptv_network_t *in = (iptv_network_t *)im->mm_network;
  if (flags & SUBSCRIPTION_STREAMING) {
    if (im->mm_iptv_streaming_priority > 0)
      return im->mm_iptv_streaming_priority;
    if (in->in_streaming_priority > 0)
      return in->in_streaming_priority;
  }
  return im->mm_iptv_priority > 0 ? im->mm_iptv_priority : in->in_priority;
}

static int
iptv_input_warm_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;
  mpegts_mux_instance_t *lmmi;
  iptv_is_free_t conf = { .active = 1, .weight = 0, .warm = 1 };

  /* Already active */
  if (im->mm_active)
    return 0;

  /* Do we need to stop something? */
  lmmi = iptv_input_is_free(mi, mmi->mmi_mux, &conf, mmi->mmi_start_weight, NULL);
  tvhtrace(LS_IPTV_SUB, "warm mux[%p]: %p (%d)", im, lmmi, mmi->mmi_start_weight);
  if (lmmi) {
    /* Stop */
    lmmi->mmi_mux->mm_stop(lmmi->mmi_mux, 1, SM_CODE_ABORTED);
  }
  return 0;
}

static const char *
iptv_sub_url_encode(iptv_mux_t *im, const char *s, char *tmp, size_t tmplen)
{
  char *p;
  if (im->mm_iptv_url &&
      (!strncmp(im->mm_iptv_url, "pipe://", 7) ||
       !strncmp(im->mm_iptv_url, "file://", 7)))
    return s;
  p = url_encode(s);
  strlcpy(tmp, p, tmplen);
  free(p);
  return tmp;
}

static const char *
iptv_sub_mux_name(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  const mpegts_mux_instance_t *mmi = aux;
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;
  return iptv_sub_url_encode(im, im->mm_iptv_muxname, tmp, tmplen);
}

static const char *
iptv_sub_service_name(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  const mpegts_mux_instance_t *mmi = aux;
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;
  return iptv_sub_url_encode(im, im->mm_iptv_svcname, tmp, tmplen);
}

static const char *
iptv_sub_weight(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  const mpegts_mux_instance_t *mmi = aux;
  snprintf(tmp, tmplen, "%d", mmi->mmi_start_weight);
  return tmp;
}

static htsstr_substitute_t iptv_input_subst[] = {
  { .id = "m",  .getval = iptv_sub_mux_name },
  { .id = "n",  .getval = iptv_sub_service_name },
  { .id = "w",  .getval = iptv_sub_weight },
  { .id = NULL, .getval = NULL }
};

static int
iptv_input_start_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi, int weight )
{
  int ret = SM_CODE_TUNING_FAILED;
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;
  iptv_handler_t *ih;
  iptv_thread_pool_t *pool = ((iptv_input_t *)mi)->mi_tpool;
  char buf[256], rawbuf[512], *raw = im->mm_iptv_url, *s;
  const char *scheme;
  url_t url;

  /* Active? */
  if (im->mm_active) {
    return 0;
  }
  
  /* Substitute things */
  if (im->mm_iptv_substitute && raw) {
    htsstr_substitute(raw, rawbuf, sizeof(rawbuf), '$', iptv_input_subst, mmi, buf, sizeof(buf));
    raw = rawbuf;
  }

  /* Parse URL */
  urlinit(&url);

#if ENABLE_LIBAV
  if (im->mm_iptv_libav > 0 ||
      (im->mm_iptv_libav == 0 && ((iptv_network_t *)im->mm_network)->in_libav)) {

    scheme = "libav";

  } else
#endif
  if (raw && !strncmp(raw, "pipe://", 7)) {

    scheme = "pipe";

  } else if (raw && !strncmp(raw, "file://", 7)) {

    scheme = "file";

#if ENABLE_LIBAV
  } else if (raw && !strncmp(raw, "libav:", 6)) {

    scheme = "libav";
#endif

  } else {

    if (urlparse(raw ?: "", &url)) {
      tvherror(LS_IPTV, "%s - invalid URL [%s]", im->mm_nicename, raw);
      urlreset(&url);
      return ret;
    }
    scheme = url.scheme;

  }

  /* Find scheme handler */
  ih = iptv_handler_find(scheme ?: "");
  if (!ih) {
    tvherror(LS_IPTV, "%s - unsupported scheme [%s]", im->mm_nicename, scheme ?: "none");
    urlreset(&url);
    return ret;
  }

  tvh_mutex_lock(&iptv_lock);
  /* Already active */
  if (im->mm_active) {
    tvh_mutex_unlock(&iptv_lock);
    urlreset(&url);
    return 0;
  }

  /* Start */
  s = im->mm_iptv_url_raw;
  im->mm_iptv_url_raw = raw ? strdup(raw) : NULL;
  if (im->mm_iptv_url_raw) {
    im->mm_active = mmi; // Note: must set here else mux_started call
                         // will not realise we're ready to accept pid open calls
    ret = ih->start((iptv_input_t *)mi, im, im->mm_iptv_url_raw, &url);
    if (!ret) {
      im->im_handler = ih;
      pool->streams++;
    } else {
      im->mm_active  = NULL;
    }
  }
  tvh_mutex_unlock(&iptv_lock);

  urlreset(&url);
  free(s);

  return ret;
}

void
iptv_input_close_fds ( iptv_input_t *mi, iptv_mux_t *im )
{
  iptv_thread_pool_t *pool = mi->mi_tpool;

  if (im->mm_iptv_fd > 0 || im->im_rtcp_info.connection_fd > 0)
    tvhtrace(LS_IPTV, "iptv_input_close_fds %d %d", im->mm_iptv_fd, im->im_rtcp_info.connection_fd);

  /* Close file */
  if (im->mm_iptv_fd > 0) {
    tvhpoll_rem1(pool->poll, im->mm_iptv_fd);
    if(im->mm_iptv_connection == NULL)
      close(im->mm_iptv_fd);
    else
      udp_close(im->mm_iptv_connection);
    im->mm_iptv_connection = NULL;
    im->mm_iptv_fd = -1;
  }

  /* Close file2 */
  if (im->im_rtcp_info.connection_fd > 0) {
    tvhpoll_rem1(pool->poll, im->im_rtcp_info.connection_fd);
    if(im->im_rtcp_info.connection == NULL)
      close(im->im_rtcp_info.connection_fd);
    else
      udp_close(im->im_rtcp_info.connection);
    im->im_rtcp_info.connection = NULL;
    im->im_rtcp_info.connection_fd = -1;
  }
}

static void
iptv_input_stop_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;
  iptv_thread_pool_t *pool = ((iptv_input_t *)mi)->mi_tpool;
  uint32_t u32;

  tvh_mutex_lock(&iptv_lock);

  mtimer_disarm(&im->im_pause_timer);

  /* Stop */
  if (im->im_handler->stop)
    im->im_handler->stop((iptv_input_t *)mi, im);

  iptv_input_close_fds((iptv_input_t *)mi, im);

  /* Free memory */
  sbuf_free(&im->mm_iptv_buffer);

  /* Clear bw limit */
  ((iptv_network_t *)im->mm_network)->in_bw_limited = 0;

  u32 = --pool->streams;

  tvh_mutex_unlock(&iptv_lock);

  if (u32 == 0)
    gtimer_arm_rel(&iptv_tpool_manage_timer, iptv_input_thread_manage_cb, NULL, 0);
}

static void
iptv_input_display_name ( mpegts_input_t *mi, char *buf, size_t len )
{
  snprintf(buf, len, "IPTV #%d", iptv_input_thread_number((iptv_input_t *)mi));
}

static inline int
iptv_input_pause_check ( iptv_mux_t *im )
{
  int64_t old, s64, limit;

  if ((old = im->im_pcr) == PTS_UNSET)
    return 0;
  limit = im->mm_iptv_buffer_limit;
  if (!limit)
    limit = im->im_handler->buffer_limit;
  if (limit == UINT32_MAX)
    return 0;
  limit *= 1000;
  s64 = getfastmonoclock() - im->im_pcr_start;
  im->im_pcr_start += s64;
  im->im_pcr += (((s64 / 10LL) * 9LL) + 4LL) / 10LL;
  im->im_pcr &= PTS_MASK;
  if (old != im->im_pcr)
    tvhtrace(LS_IPTV_PCR, "updated %"PRId64", time start %"PRId64", limit %"PRId64", diff %"PRId64,
             im->im_pcr, im->im_pcr_start, limit, im->im_pcr_end - im->im_pcr_start);

  /* queued more than threshold? trigger the pause */
  return im->im_pcr_end - im->im_pcr_start >= limit;
}

void
iptv_input_unpause ( void *aux )
{
  iptv_mux_t *im = aux;
  iptv_input_t *mi;
  int pause;
  tvh_mutex_lock(&iptv_lock);
  pause = 0;
  if (im->mm_active) {
    mi = (iptv_input_t *)im->mm_active->mmi_input;
    if (iptv_input_pause_check(im)) {
      tvhtrace(LS_IPTV_PCR, "paused (in unpause)");
      pause = 1;
    } else {
      tvhtrace(LS_IPTV_PCR, "unpause timer callback");
      im->im_handler->pause(mi, im, 0);
    }
  }
  tvh_mutex_unlock(&iptv_lock);
  if (pause)
    mtimer_arm_rel(&im->im_pause_timer, iptv_input_unpause, im, sec2mono(1));
}

static void *
iptv_input_thread ( void *aux )
{
  iptv_thread_pool_t *pool = aux;
  int nfds, r;
  ssize_t n;
  iptv_mux_t *im;
  iptv_input_t *mi;
  tvhpoll_event_t ev;

  while ( tvheadend_is_running() ) {
    nfds = tvhpoll_wait(pool->poll, &ev, 1, -1);
    if ( nfds < 0 ) {
      if (tvheadend_is_running() && !ERRNO_AGAIN(errno)) {
        tvherror(LS_IPTV, "poll() error %s, sleeping 1 second",
                 strerror(errno));
        sleep(1);
      }
      continue;
    } else if ( nfds == 0 ) {
      continue;
    }

    if (ev.ptr == &pool->pipe)
      break;

    im = ev.ptr;
    r  = 0;

    tvh_mutex_lock(&iptv_lock);

    /* Only when active */
    if (im->mm_active) {
      mi = (iptv_input_t *)im->mm_active->mmi_input;
      /* Get data */
      if ((n = im->im_handler->read(mi, im)) < 0) {
        tvherror(LS_IPTV, "read() error %s", strerror(errno));
        im->im_handler->stop(mi, im);
        tvh_mutex_unlock(&iptv_lock);
        break;
      }
      r = iptv_input_recv_packets(im, n);
      if (r == 1)
        im->im_handler->pause(mi, im, 1);
    }

    tvh_mutex_unlock(&iptv_lock);

    if (r == 1) {
      tvh_mutex_lock(&global_lock);
      if (im->mm_active)
        mtimer_arm_rel(&im->im_pause_timer, iptv_input_unpause, im, sec2mono(1));
      tvh_mutex_unlock(&global_lock);
    }
  }
  return NULL;
}

void
iptv_input_pause_handler ( iptv_input_t *mi, iptv_mux_t *im, int pause )
{
  iptv_thread_pool_t *tpool = mi->mi_tpool;

  if (pause)
    tvhpoll_rem1(tpool->poll, im->mm_iptv_fd);
  else
    tvhpoll_add1(tpool->poll, im->mm_iptv_fd, TVHPOLL_IN, im);
}

void
iptv_input_recv_flush ( iptv_mux_t *im )
{
  mpegts_mux_instance_t *mmi = im->mm_active;

  if (mmi == NULL)
    return;
  mpegts_input_recv_packets(mmi, &im->mm_iptv_buffer,
                            MPEGTS_DATA_CC_RESTART, NULL);
}

int
iptv_input_recv_packets ( iptv_mux_t *im, ssize_t len )
{
  iptv_network_t *in = (iptv_network_t*)im->mm_network;
  mpegts_mux_instance_t *mmi;
  mpegts_pcr_t pcr;
  char buf[384];
  int64_t s64;

  pcr.pcr_first = PTS_UNSET;
  pcr.pcr_last  = PTS_UNSET;
  pcr.pcr_pid   = im->im_pcr_pid;
  in->in_bps += len * 8;
  s64 = mclk();
  if (mono2sec(in->in_bandwidth_clock) != mono2sec(s64)) {
    if (in->in_max_bandwidth &&
        in->in_bps > in->in_max_bandwidth * 1024) {
      if (!in->in_bw_limited) {
        tvhinfo(LS_IPTV, "%s bandwidth limited exceeded",
                idnode_get_title(&in->mn_id, NULL, buf, sizeof(buf)));
        in->in_bw_limited = 1;
      }
    }
    in->in_bps = 0;
    in->in_bandwidth_clock = s64;
  }

  /* Pass on, but with timing */
  mmi = im->mm_active;
  if (mmi) {
    if (iptv_input_pause_check(im)) {
      tvhtrace(LS_IPTV_PCR, "paused");
      return 1;
    }
    mpegts_input_recv_packets(mmi, &im->mm_iptv_buffer,
                              in->in_remove_scrambled_bits ?
                                MPEGTS_DATA_REMOVE_SCRAMBLED : 0, &pcr);
    if (pcr.pcr_first != PTS_UNSET && pcr.pcr_last != PTS_UNSET) {
      im->im_pcr_pid = pcr.pcr_pid;
      if (im->im_pcr == PTS_UNSET) {
        s64 = pts_diff(pcr.pcr_first, pcr.pcr_last);
        if (s64 != PTS_UNSET) {
          im->im_pcr = pcr.pcr_first;
          im->im_pcr_start = getfastmonoclock();
          im->im_pcr_end = im->im_pcr_start + ((s64 * 100LL) + 50LL) / 9LL;
          tvhtrace(LS_IPTV_PCR, "first %"PRId64" last %"PRId64", time start %"PRId64", end %"PRId64,
                   pcr.pcr_first, pcr.pcr_last, im->im_pcr_start, im->im_pcr_end);
        }
      } else {
        s64 = pts_diff(im->im_pcr, pcr.pcr_last);
        if (s64 != PTS_UNSET) {
          im->im_pcr_end = im->im_pcr_start + ((s64 * 100LL) + 50LL) / 9LL;
          tvhtrace(LS_IPTV_PCR, "last %"PRId64", time end %"PRId64, pcr.pcr_last, im->im_pcr_end);
        }
      }
      if (iptv_input_pause_check(im)) {
        tvhtrace(LS_IPTV_PCR, "paused");
        return 1;
      }
    }
  }
  return 0;
}

int
iptv_input_fd_started ( iptv_input_t *mi, iptv_mux_t *im )
{
  iptv_thread_pool_t *tpool = mi->mi_tpool;

  /* Setup poll */
  if (im->mm_iptv_fd > 0) {
    /* Error? */
    if (tvhpoll_add1(tpool->poll, im->mm_iptv_fd, TVHPOLL_IN, im) < 0) {
      tvherror(LS_IPTV, "%s - failed to add to poll q", im->mm_nicename);
      close(im->mm_iptv_fd);
      im->mm_iptv_fd = -1;
      return -1;
    }
  }

  /* Setup poll2 */
  if (im->im_rtcp_info.connection_fd > 0) {
    /* Error? */
    if (tvhpoll_add1(tpool->poll, im->im_rtcp_info.connection_fd, TVHPOLL_IN, im) < 0) {
      tvherror(LS_IPTV, "%s - failed to add to poll q (2)", im->mm_nicename);
      close(im->im_rtcp_info.connection_fd);
      im->im_rtcp_info.connection_fd = -1;
      return -1;
    }
  }
  return 0;
}

void
iptv_input_mux_started ( iptv_input_t *mi, iptv_mux_t *im, int reset )
{
  /* Allocate input buffer */
  if (reset)
    sbuf_reset_and_alloc(&im->mm_iptv_buffer, IPTV_BUF_SIZE);

  im->im_pcr = PTS_UNSET;
  im->im_pcr_pid = MPEGTS_PID_NONE;

  if (iptv_input_fd_started(mi, im))
    return;

  /* Install table handlers */
  mpegts_mux_t *mm = (mpegts_mux_t*)im;
  if (mm->mm_active)
    psi_tables_install((mpegts_input_t *)mi, mm,
                       im->mm_iptv_atsc ? DVB_SYS_ATSC_ALL : DVB_SYS_DVBT);
}

static int
iptv_network_bouquet_source (mpegts_network_t *mn, char *source, size_t len)
{
  iptv_network_t *in = (iptv_network_t *)mn;
  char ubuf[UUID_HEX_SIZE];
  snprintf(source, len, "iptv-network://%s", idnode_uuid_as_str(&in->mn_id, ubuf));
  return 0;
}

static int
iptv_network_bouquet_comment (mpegts_network_t *mn, char *comment, size_t len)
{
  iptv_network_t *in = (iptv_network_t *)mn;
  if (in->in_url == NULL || in->in_url[0] == '\0')
    return -1;
  snprintf(comment, len, "%s", in->in_url);
  return 0;
}

static void
iptv_network_delete ( mpegts_network_t *mn, int delconf )
{
  iptv_network_t *in = (iptv_network_t*)mn;
  char *url = in->in_url;
  char *sane_url = in->in_url_sane;
  char *icon_url = in->in_icon_url;
  char *icon_url_sane = in->in_icon_url_sane;
  char ubuf[UUID_HEX_SIZE];

  idnode_save_check(&mn->mn_id, delconf);

  if (in->mn_id.in_class == &iptv_auto_network_class)
    iptv_auto_network_done(in);

  /* Remove config */
  if (delconf)
    hts_settings_remove("input/iptv/networks/%s",
                        idnode_uuid_as_str(&in->mn_id, ubuf));

  /* delete */
  free(in->in_remove_args);
  free(in->in_ignore_args);
  free(in->in_ctx_charset);
  mpegts_network_delete(mn, delconf);

  free(icon_url_sane);
  free(icon_url);
  free(sane_url);
  free(url);
}

/* **************************************************************************
 * IPTV network
 * *************************************************************************/

static void
iptv_network_class_delete ( idnode_t *in )
{
  return iptv_network_delete((mpegts_network_t *)in, 1);
}

static int
iptv_network_class_icon_url_set( void *in, const void *v )
{
  iptv_network_t *mn = in;
  return iptv_url_set(&mn->in_icon_url, &mn->in_icon_url_sane, v, 1, 0);
}

PROP_DOC(priority)
PROP_DOC(streaming_priority)

extern const idclass_t mpegts_network_class;
const idclass_t iptv_network_class = {
  .ic_super      = &mpegts_network_class,
  .ic_class      = "iptv_network",
  .ic_caption    = N_("IPTV Network"),
  .ic_delete     = iptv_network_class_delete,
  .ic_properties = (const property_t[]){
#if ENABLE_LIBAV
    {
      .type     = PT_BOOL,
      .id       = "use_libav",
      .name     = N_("Use A/V library"),
      .desc     = N_("The input stream is remuxed with A/V library (libav or"
                     " or ffmpeg) to the MPEG-TS format which is accepted by"
                     " Tvheadend."),
      .off      = offsetof(iptv_network_t, in_libav),
      .opts     = PO_ADVANCED
    },
#endif
    {
      .type     = PT_BOOL,
      .id       = "scan_create",
      .name     = N_("Scan after creation"),
      .desc     = N_("After creating the network scan it for services."),
      .off      = offsetof(iptv_network_t, in_scan_create),
      .def.i    = 1,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_U16,
      .id       = "service_sid",
      .name     = N_("Service ID"),
      .desc     = N_("The network's service ID"),
      .off      = offsetof(iptv_network_t, in_service_id),
      .def.i    = 0,
      .opts     = PO_EXPERT
    },
    {
      .type     = PT_INT,
      .id       = "priority",
      .name     = N_("Priority"),
      .desc     = N_("The network's priority. The network with the "
                     "highest priority value will be used out of "
                     "preference if available. See Help for details."),
      .off      = offsetof(iptv_network_t, in_priority),
      .doc      = prop_doc_priority,
      .def.i    = 1,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_INT,
      .id       = "spriority",
      .name     = N_("Streaming priority"),
      .desc     = N_("When streaming a service (via http or htsp) "
                     "Tvheadend will use the network with the highest "
                     "streaming priority set here. See Help for details."),
      .doc      = prop_doc_streaming_priority,
      .off      = offsetof(iptv_network_t, in_streaming_priority),
      .def.i    = 1,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_U32,
      .id       = "max_streams",
      .name     = N_("Maximum # input streams"),
      .desc     = N_("The maximum number of input streams allowed "
                     "on this network."),
      .off      = offsetof(iptv_network_t, in_max_streams),
      .def.i    = 0,
    },
    {
      .type     = PT_U32,
      .id       = "max_bandwidth",
      .name     = N_("Maximum bandwidth (Kbps)"),
      .desc     = N_("Maximum input bandwidth."),
      .off      = offsetof(iptv_network_t, in_max_bandwidth),
      .def.i    = 0,
    },
    {
      .type     = PT_U32,
      .id       = "max_timeout",
      .name     = N_("Maximum timeout (seconds)"),
      .desc     = N_("Maximum time to wait (in seconds) for a stream "
                     "before a timeout."),
      .off      = offsetof(iptv_network_t, in_max_timeout),
      .def.i    = 15,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .id       = "icon_url",
      .name     = N_("Icon base URL"),
      .desc     = N_("Icon base URL."),
      .off      = offsetof(iptv_network_t, in_icon_url),
      .set      = iptv_network_class_icon_url_set,
      .opts     = PO_MULTILINE | PO_ADVANCED
    },
    {
      .type     = PT_BOOL,
      .id       = "remove_scrambled",
      .name     = N_("Remove scrambled bits"),
      .desc     = N_("The scrambled bits in MPEG-TS packets are always cleared. "
                     "It is a workaround for the special streams which are "
                     "descrambled, but these bits are not touched."),
      .off      = offsetof(iptv_network_t, in_remove_scrambled_bits),
      .def.i    = 1,
      .opts     = PO_EXPERT,
    },
    {
      .id       = "autodiscovery",
      .type     = PT_NONE,
    },
    {}
  }
};

static int
iptv_auto_network_class_url_set( void *in, const void *v )
{
  iptv_network_t *mn = in;
  return iptv_url_set(&mn->in_url, &mn->in_url_sane, v, 1, 1);
}

static void
iptv_auto_network_class_notify_url( void *in, const char *lang )
{
  iptv_auto_network_trigger(in);
}

static htsmsg_t *
iptv_auto_network_class_charset_list(void *o, const char *lang)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "intlconv/charsets");
  return m;
}

PROP_DOC(ignore_path)

const idclass_t iptv_auto_network_class = {
  .ic_super      = &iptv_network_class,
  .ic_class      = "iptv_auto_network",
  .ic_caption    = N_("IPTV Automatic Network"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "url",
      .name     = N_("URL"),
      .desc     = N_("The URL to the playlist."),
      .off      = offsetof(iptv_network_t, in_url),
      .set      = iptv_auto_network_class_url_set,
      .notify   = iptv_auto_network_class_notify_url,
      .opts     = PO_MULTILINE
    },
    {
      .type     = PT_STR,
      .id       = "ctx_charset",
      .name     = N_("Content character set"),
      .desc     = N_("The playlist's character set."),
      .off      = offsetof(iptv_network_t, in_ctx_charset),
      .list     = iptv_auto_network_class_charset_list,
      .notify   = iptv_auto_network_class_notify_url,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_S64,
      .intextra = CHANNEL_SPLIT,
      .id       = "channel_number",
      .name     = N_("Channel numbers from"),
      .desc     = N_("Lowest starting channel number (when mapping). "),
      .off      = offsetof(iptv_network_t, in_channel_number),
    },
    {
      .type     = PT_U32,
      .id       = "refetch_period",
      .name     = N_("Re-fetch period (mins)"),
      .desc     = N_("Time (in minutes) to re-fetch the playlist."),
      .off      = offsetof(iptv_network_t, in_refetch_period),
      .def.i    = 60,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_BOOL,
      .id       = "ssl_peer_verify",
      .name     = N_("SSL verify peer"),
      .desc     = N_("Verify the peer's SSL."),
      .off      = offsetof(iptv_network_t, in_ssl_peer_verify),
      .opts     = PO_EXPERT
    },
    {
      .type     = PT_BOOL,
      .id       = "tsid_zero",
      .name     = N_("Accept zero value for TSID"),
      .desc     = N_("Accept transport ID if zero."),
      .off      = offsetof(iptv_network_t, in_tsid_accept_zero_value),
    },
    {
      .type     = PT_STR,
      .id       = "remove_args",
      .name     = N_("Remove HTTP arguments"),
      .desc     = N_("Argument names to remove from the query "
                     "string in the URL."),
      .off      = offsetof(iptv_network_t, in_remove_args),
      .def.s    = "ticket",
      .opts     = PO_EXPERT
    },
    {
      .type     = PT_STR,
      .id       = "ignore_args",
      .name     = N_("Ignore HTTP arguments"),
      .desc     = N_("Argument names to remove from the query "
                     "string in the URL when the identical "
                     "source is compared."),
      .off      = offsetof(iptv_network_t, in_ignore_args),
      .def.s    = "",
      .opts     = PO_EXPERT
    },
    {
      .type     = PT_INT,
      .id       = "ignore_path",
      .name     = N_("Ignore path components"),
      .desc     = N_("Ignore last components in path. The defined count "
                     "of last path components separated by / are removed "
                     "when the identical source is compared - see Help for a detailed explanation."),
      .doc      = prop_doc_ignore_path,
      .off      = offsetof(iptv_network_t, in_ignore_path),
      .def.i    = 0,
      .opts     = PO_EXPERT
    },
    {}
  }
};

static mpegts_mux_t *
iptv_network_create_mux2
  ( mpegts_network_t *mn, htsmsg_t *conf )
{
  iptv_mux_t *im = iptv_mux_create0((iptv_network_t*)mn, NULL, conf);
  return mpegts_mux_post_create((mpegts_mux_t *)im);
}

static void
iptv_network_auto_scan ( mpegts_network_t *mn )
{
  iptv_auto_network_trigger((iptv_network_t *)mn);
  mpegts_network_scan(mn);
}

static mpegts_service_t *
iptv_network_create_service
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid )
{
  return (mpegts_service_t*)
    iptv_service_create0((iptv_mux_t*)mm, sid, pmt_pid, NULL, NULL);
}

static const idclass_t *
iptv_network_mux_class ( mpegts_network_t *mm )
{
  extern const idclass_t iptv_mux_class;
  return &iptv_mux_class;
}

static htsmsg_t *
iptv_network_config_save ( mpegts_network_t *mn, char *filename, size_t fsize )
{
  htsmsg_t *c = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];
  idnode_save(&mn->mn_id, c);
  if (filename)
    snprintf(filename, fsize, "input/iptv/networks/%s/config",
             idnode_uuid_as_str(&mn->mn_id, ubuf));
  return c;
}

iptv_network_t *
iptv_network_create0
  ( const char *uuid, htsmsg_t *conf, const idclass_t *idc )
{
  iptv_network_t *in = calloc(1, sizeof(*in));
  iptv_mux_t *im;
  htsmsg_t *c;
  char ubuf[UUID_HEX_SIZE];

  /* Init Network */
  in->in_scan_create        = 1;
  in->in_priority           = 1;
  in->in_streaming_priority = 1;
  if (idc == &iptv_auto_network_class)
    in->in_remove_args = strdup("ticket");
  if (!mpegts_network_create0((mpegts_network_t *)in, idc,
                              uuid, NULL, conf)) {
    free(in->in_remove_args);
    free(in);
    return NULL;
  }
  in->mn_bouquet_source = iptv_network_bouquet_source;
  in->mn_bouquet_comment= iptv_network_bouquet_comment;
  in->mn_delete         = iptv_network_delete;
  in->mn_create_service = iptv_network_create_service;
  in->mn_mux_class      = iptv_network_mux_class;
  in->mn_mux_create2    = iptv_network_create_mux2;
  in->mn_config_save    = iptv_network_config_save;
  if (idc == &iptv_auto_network_class)
    in->mn_scan         = iptv_network_auto_scan;
 
  /* Defaults */
  in->mn_autodiscovery  = 0;
  if (!conf) {
    in->mn_skipinitscan = 1;
  }

  /* Load muxes */
  if ((c = hts_settings_load_r(1, "input/iptv/networks/%s/muxes",
                                idnode_uuid_as_str(&in->mn_id, ubuf)))) {
    htsmsg_field_t *f;
    htsmsg_t *e;
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_get_map_by_field(f)))  continue;
      if (!(e = htsmsg_get_map(e, "config"))) continue;
      im = iptv_mux_create0(in, htsmsg_field_name(f), e);
      mpegts_mux_post_create((mpegts_mux_t *)im);
    }
    htsmsg_destroy(c);
  }

  if (idc == &iptv_auto_network_class)
    iptv_auto_network_init(in);
  
  return in;
}

static mpegts_network_t *
iptv_network_builder
  ( const idclass_t *idc, htsmsg_t *conf )
{
  mpegts_network_t *mn;
  iptv_thread_pool_t *pool;

  mn = (mpegts_network_t *)iptv_network_create0(NULL, conf, idc);
  TAILQ_FOREACH(pool, &iptv_tpool, link)
    mpegts_input_add_network((mpegts_input_t *)pool->input, mn);
  return mn;
}

/* **************************************************************************
 * IPTV initialise
 * *************************************************************************/

static void
iptv_network_init ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  /* Register muxes */
  idclass_register(&iptv_mux_class);

  /* Register builders */
  mpegts_network_register_builder(&iptv_network_class,
                                  iptv_network_builder);
  mpegts_network_register_builder(&iptv_auto_network_class,
                                  iptv_network_builder);

  /* Load settings */
  if (!(c = hts_settings_load_r(1, "input/iptv/networks")))
    return;

  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_get_map_by_field(f)))  continue;
    if (!(e = htsmsg_get_map(e, "config"))) continue;
    if (htsmsg_get_str(e, "url"))
      iptv_network_create0(htsmsg_field_name(f), e, &iptv_auto_network_class);
    else
      iptv_network_create0(htsmsg_field_name(f), e, &iptv_network_class);
  }
  htsmsg_destroy(c);
}

static mpegts_network_t *
iptv_input_wizard_network ( iptv_input_t *lfe )
{
  return (mpegts_network_t *)LIST_FIRST(&lfe->mi_networks);
}

static htsmsg_t *
iptv_input_wizard_get( tvh_input_t *ti, const char *lang )
{
  iptv_input_t *mi = (iptv_input_t*)ti;
  mpegts_network_t *mn;
  const idclass_t *idc = NULL;

  mn = iptv_input_wizard_network(mi);
  if (mn == NULL || (mn && mn->mn_wizard))
    idc = &iptv_auto_network_class;
  return mpegts_network_wizard_get((mpegts_input_t *)mi, idc, mn, lang);
}

static void
iptv_input_wizard_set( tvh_input_t *ti, htsmsg_t *conf, const char *lang )
{
  iptv_input_t *mi = (iptv_input_t*)ti;
  const char *ntype = htsmsg_get_str(conf, "mpegts_network_type");
  mpegts_network_t *mn;

  mn = iptv_input_wizard_network(mi);
  if ((mn == NULL || mn->mn_wizard))
    mpegts_network_wizard_create(ntype, NULL, lang);
}

static iptv_input_t *
iptv_create_input ( void *tpool )
{
  iptv_input_t *input = calloc(1, sizeof(iptv_input_t));
  mpegts_network_t *mn;

  /* Init Input */
  mpegts_input_create0((mpegts_input_t*)input,
                       &iptv_input_class, NULL, NULL);
  input->ti_wizard_get     = iptv_input_wizard_get;
  input->ti_wizard_set     = iptv_input_wizard_set;
  input->mi_warm_mux       = iptv_input_warm_mux;
  input->mi_start_mux      = iptv_input_start_mux;
  input->mi_stop_mux       = iptv_input_stop_mux;
  input->mi_is_enabled     = iptv_input_is_enabled;
  input->mi_get_weight     = iptv_input_get_weight;
  input->mi_get_grace      = iptv_input_get_grace;
  input->mi_get_priority   = iptv_input_get_priority;
  input->mi_display_name   = iptv_input_display_name;
  input->mi_enabled        = 1;

  input->mi_tpool          = tpool;

  /* Link */
  LIST_FOREACH(mn, &mpegts_network_all, mn_global_link)
    if (idnode_is_instance(&mn->mn_id, &iptv_network_class))
      mpegts_input_add_network((mpegts_input_t *)input, mn);

  return input;
}

static void
iptv_input_thread_manage(int count, int force)
{
  iptv_thread_pool_t *pool;

  while (iptv_tpool_count < count) {
    pool = calloc(1, sizeof(*pool));
    pool->poll = tvhpoll_create(10);
    pool->input = iptv_create_input(pool);
    tvh_pipe(O_NONBLOCK, &pool->pipe);
    tvhpoll_add1(pool->poll, pool->pipe.rd, TVHPOLL_IN, &pool->pipe);
    tvh_thread_create(&pool->thread, NULL, iptv_input_thread, pool, "iptv");
    TAILQ_INSERT_TAIL(&iptv_tpool, pool, link);
    iptv_tpool_count++;
  }
  while (iptv_tpool_count > count) {
    TAILQ_FOREACH(pool, &iptv_tpool, link)
      if (pool->streams == 0 || force) {
        tvh_write(pool->pipe.wr, "q", 1);
        pthread_join(pool->thread, NULL);
        TAILQ_REMOVE(&iptv_tpool, pool, link);
        mpegts_input_stop_all((mpegts_input_t*)pool->input);
        mpegts_input_delete((mpegts_input_t *)pool->input, 0);
        tvhpoll_rem1(pool->poll, pool->pipe.rd);
        tvhpoll_destroy(pool->poll);
        free(pool);
        iptv_tpool_count--;
        break;
      }
    if (pool == NULL)
      break;
  }
}

static void
iptv_input_thread_manage_cb(void *aux)
{
  iptv_input_thread_manage(iptv_tpool_safe_count(), 0);
}

void iptv_init ( void )
{
  TAILQ_INIT(&iptv_tpool);
  tvh_mutex_init(&iptv_lock, NULL);

  /* Register handlers */
  iptv_http_init();
  iptv_udp_init();
  iptv_rtsp_init();
  iptv_pipe_init();
  iptv_file_init();
#if ENABLE_LIBAV
  iptv_libav_init();
#endif

  /* Init Network */
  iptv_network_init();

  /* Threads init */
  iptv_input_thread_manage(iptv_tpool_safe_count(), 0);
  tvhinfo(LS_IPTV, "Using %d input thread(s)", iptv_tpool_count);
}

void iptv_done ( void )
{
  tvh_mutex_lock(&global_lock);
  iptv_input_thread_manage(0, 1);
  assert(TAILQ_EMPTY(&iptv_tpool));
  mpegts_network_unregister_builder(&iptv_auto_network_class);
  mpegts_network_unregister_builder(&iptv_network_class);
  mpegts_network_class_delete(&iptv_auto_network_class, 0);
  mpegts_network_class_delete(&iptv_network_class, 0);
  tvh_mutex_unlock(&global_lock);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
