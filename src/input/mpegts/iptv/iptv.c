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

#include "iptv_private.h"
#include "tvhpoll.h"
#include "tcp.h"
#include "settings.h"
#include "htsstr.h"
#include "channels.h"
#include "packet.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

/* **************************************************************************
 * IPTV state
 * *************************************************************************/

iptv_input_t   *iptv_input;
tvhpoll_t      *iptv_poll;
pthread_t       iptv_thread;
pthread_mutex_t iptv_lock;

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

static const char *
iptv_input_class_get_title ( idnode_t *self, const char *lang )
{
  return tvh_gettext_lang(lang, N_("IPTV"));
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
  int h = 0, l = 0, w, rw = INT_MAX;
  mpegts_mux_instance_t *mmi, *rmmi = NULL;
  iptv_network_t *in = (iptv_network_t *)mm->mm_network;
  
  pthread_mutex_lock(&mi->mi_output_lock);
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link)
    if (mmi->mmi_mux->mm_network == (mpegts_network_t *)in) {
      w = mpegts_mux_instance_weight(mmi);
      if (w < rw && (!conf->active || mmi->mmi_mux != mm)) {
        rmmi = mmi;
        rw = w;
      }
      if (w >= weight) h++; else l++;
    }
  pthread_mutex_unlock(&mi->mi_output_lock);

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
  strncpy(tmp, p, tmplen-1);
  tmp[tmplen-1] = '\0';
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
  char buf[256], rawbuf[512], *raw = im->mm_iptv_url, *s;
  const char *scheme;
  url_t url;

  /* Already active */
  if (im->mm_active)
    return 0;

  /* Substitute things */
  if (im->mm_iptv_substitute && raw) {
    htsstr_substitute(raw, rawbuf, sizeof(rawbuf), '$', iptv_input_subst, mmi, buf, sizeof(buf));
    raw = rawbuf;
  }

  /* Parse URL */
  mpegts_mux_nice_name((mpegts_mux_t*)im, buf, sizeof(buf));
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
      tvherror(LS_IPTV, "%s - invalid URL [%s]", buf, raw);
      return ret;
    }
    scheme = url.scheme;

  }

  /* Find scheme handler */
  ih = iptv_handler_find(scheme ?: "");
  if (!ih) {
    tvherror(LS_IPTV, "%s - unsupported scheme [%s]", buf, scheme ?: "none");
    return ret;
  }

  /* Start */
  pthread_mutex_lock(&iptv_lock);
  s = im->mm_iptv_url_raw;
  im->mm_iptv_url_raw = strdup(raw);
  im->mm_active = mmi; // Note: must set here else mux_started call
                       // will not realise we're ready to accept pid open calls
  ret = ih->start(im, im->mm_iptv_url_raw, &url);
  if (!ret)
    im->im_handler = ih;
  else
    im->mm_active  = NULL;
  pthread_mutex_unlock(&iptv_lock);

  urlreset(&url);
  free(s);
  return ret;
}

static void
iptv_input_stop_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;

  pthread_mutex_lock(&iptv_lock);

  mtimer_disarm(&im->im_pause_timer);

  /* Stop */
  if (im->im_handler->stop)
    im->im_handler->stop(im);

  /* Close file */
  if (im->mm_iptv_fd > 0) {
    udp_close(im->mm_iptv_connection); // removes from poll
    im->mm_iptv_connection = NULL;
    im->mm_iptv_fd = -1;
  }

  /* Close file2 */
  if (im->mm_iptv_fd2 > 0) {
    udp_close(im->mm_iptv_connection2); // removes from poll
    im->mm_iptv_connection2 = NULL;
    im->mm_iptv_fd2 = -1;
  }

  /* Free memory */
  sbuf_free(&im->mm_iptv_buffer);

  /* Clear bw limit */
  ((iptv_network_t *)im->mm_network)->in_bw_limited = 0;

  pthread_mutex_unlock(&iptv_lock);
}

static void
iptv_input_display_name ( mpegts_input_t *mi, char *buf, size_t len )
{
  snprintf(buf, len, "IPTV");
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
    tvhtrace(LS_IPTV_PCR, "pcr: updated %"PRId64", time start %"PRId64", limit %"PRId64,
             im->im_pcr, im->im_pcr_start, limit);

  /* queued more than 3 seconds? trigger the pause */
  return im->im_pcr_end - im->im_pcr_start >= limit;
}

void
iptv_input_unpause ( void *aux )
{
  iptv_mux_t *im = aux;
  int pause;
  pthread_mutex_lock(&iptv_lock);
  if (iptv_input_pause_check(im)) {
    pause = 1;
  } else {
    tvhtrace(LS_IPTV_PCR, "unpause timer callback");
    im->im_handler->pause(im, 0);
    pause = 0;
  }
  pthread_mutex_unlock(&iptv_lock);
  if (pause)
    mtimer_arm_rel(&im->im_pause_timer, iptv_input_unpause, im, sec2mono(1));
}

static void *
iptv_input_thread ( void *aux )
{
  int nfds, r;
  ssize_t n;
  iptv_mux_t *im;
  tvhpoll_event_t ev;

  while ( tvheadend_is_running() ) {
    nfds = tvhpoll_wait(iptv_poll, &ev, 1, -1);
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
    im = ev.data.ptr;
    r  = 0;

    pthread_mutex_lock(&iptv_lock);

    /* Only when active */
    if (im->mm_active) {
      /* Get data */
      if ((n = im->im_handler->read(im)) < 0) {
        tvherror(LS_IPTV, "read() error %s", strerror(errno));
        im->im_handler->stop(im);
        break;
      }
      r = iptv_input_recv_packets(im, n);
      if (r == 1)
        im->im_handler->pause(im, 1);
    }

    pthread_mutex_unlock(&iptv_lock);

    if (r == 1) {
      pthread_mutex_lock(&global_lock);
      if (im->mm_active)
        mtimer_arm_rel(&im->im_pause_timer, iptv_input_unpause, im, sec2mono(1));
      pthread_mutex_unlock(&global_lock);
    }
  }
  return NULL;
}

void
iptv_input_pause_handler ( iptv_mux_t *im, int pause )
{
  tvhpoll_event_t ev = { 0 };

  ev.fd       = im->mm_iptv_fd;
  ev.events   = TVHPOLL_IN;
  ev.data.ptr = im;
  if (pause)
    tvhpoll_rem(iptv_poll, &ev, 1);
  else
    tvhpoll_add(iptv_poll, &ev, 1);
}

void
iptv_input_recv_flush ( iptv_mux_t *im )
{
  mpegts_mux_instance_t *mmi = im->mm_active;

  if (mmi == NULL)
    return;
  mpegts_input_recv_packets((mpegts_input_t*)iptv_input, mmi,
                            &im->mm_iptv_buffer, MPEGTS_DATA_CC_RESTART,
                            NULL);
}

int
iptv_input_recv_packets ( iptv_mux_t *im, ssize_t len )
{
  static time_t t1 = 0, t2;
  iptv_network_t *in = (iptv_network_t*)im->mm_network;
  mpegts_mux_instance_t *mmi;
  mpegts_pcr_t pcr;
  int64_t s64;

  pcr.pcr_first = PTS_UNSET;
  pcr.pcr_last  = PTS_UNSET;
  pcr.pcr_pid   = im->im_pcr_pid;
  in->in_bps += len * 8;
  time(&t2);
  if (t2 != t1) {
    if (in->in_max_bandwidth &&
        in->in_bps > in->in_max_bandwidth * 1024) {
      if (!in->in_bw_limited) {
        tvhinfo(LS_IPTV, "%s bandwidth limited exceeded",
                idnode_get_title(&in->mn_id, NULL));
        in->in_bw_limited = 1;
      }
    }
    in->in_bps = 0;
    t1 = t2;
  }

  /* Pass on, but with timing */
  mmi = im->mm_active;
  if (mmi) {
    if (iptv_input_pause_check(im)) {
      tvhtrace(LS_IPTV_PCR, "pcr: paused");
      return 1;
    }
    mpegts_input_recv_packets((mpegts_input_t*)iptv_input, mmi,
                              &im->mm_iptv_buffer,
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
          tvhtrace(LS_IPTV_PCR, "pcr: first %"PRId64" last %"PRId64", time start %"PRId64", end %"PRId64,
                   pcr.pcr_first, pcr.pcr_last, im->im_pcr_start, im->im_pcr_end);
        }
      } else {
        s64 = pts_diff(im->im_pcr, pcr.pcr_last);
        if (s64 != PTS_UNSET) {
          im->im_pcr_end = im->im_pcr_start + ((s64 * 100LL) + 50LL) / 9LL;
          tvhtrace(LS_IPTV_PCR, "pcr: last %"PRId64", time end %"PRId64, pcr.pcr_last, im->im_pcr_end);
        }
      }
      if (iptv_input_pause_check(im)) {
        tvhtrace(LS_IPTV_PCR, "pcr: paused");
        return 1;
      }
    }
  }
  return 0;
}


int
iptv_input_fd_started ( iptv_mux_t *im )
{
  char buf[256];
  tvhpoll_event_t ev = { 0 };

  /* Setup poll */
  if (im->mm_iptv_fd > 0) {
    ev.fd       = im->mm_iptv_fd;
    ev.events   = TVHPOLL_IN;
    ev.data.ptr = im;

    /* Error? */
    if (tvhpoll_add(iptv_poll, &ev, 1) == -1) {
      mpegts_mux_nice_name((mpegts_mux_t*)im, buf, sizeof(buf));
      tvherror(LS_IPTV, "%s - failed to add to poll q", buf);
      close(im->mm_iptv_fd);
      im->mm_iptv_fd = -1;
      return -1;
    }
  }

  /* Setup poll2 */
  if (im->mm_iptv_fd2 > 0) {
    ev.fd       = im->mm_iptv_fd2;
    ev.events   = TVHPOLL_IN;
    ev.data.ptr = im;

    /* Error? */
    if (tvhpoll_add(iptv_poll, &ev, 1) == -1) {
      mpegts_mux_nice_name((mpegts_mux_t*)im, buf, sizeof(buf));
      tvherror(LS_IPTV, "%s - failed to add to poll q (2)", buf);
      close(im->mm_iptv_fd2);
      im->mm_iptv_fd2 = -1;
      return -1;
    }
  }
  return 0;
}

void
iptv_input_mux_started ( iptv_mux_t *im )
{
  /* Allocate input buffer */
  sbuf_reset_and_alloc(&im->mm_iptv_buffer, IPTV_BUF_SIZE);

  im->im_pcr = PTS_UNSET;
  im->im_pcr_pid = MPEGTS_PID_NONE;

  if (iptv_input_fd_started(im))
    return;

  /* Install table handlers */
  mpegts_mux_t *mm = (mpegts_mux_t*)im;
  if (mm->mm_active)
    psi_tables_install(mm->mm_active->mmi_input, mm,
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
                     " tvheadend."),
      .off      = offsetof(iptv_network_t, in_libav),
      .def.i    = 1,
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
      .desc     = N_("Lowest starting channel number."),
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
      .off      = offsetof(iptv_network_t, in_tsid_accept_zero_value),
    },
    {
      .type     = PT_STR,
      .id       = "remove_args",
      .name     = N_("Remove HTTP arguments"),
      .desc     = N_("Key and value pairs to remove from the query "
                     "string in the URL."),
      .off      = offsetof(iptv_network_t, in_remove_args),
      .def.s    = "ticket",
      .opts     = PO_EXPERT
    },
    {}
  }
};

static mpegts_mux_t *
iptv_network_create_mux2
  ( mpegts_network_t *mn, htsmsg_t *conf )
{
  return (mpegts_mux_t*)iptv_mux_create0((iptv_network_t*)mn, NULL, conf);
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
 
  /* Defaults */
  in->mn_autodiscovery  = 0;
  if (!conf) {
    in->mn_skipinitscan = 1;
  }

  /* Link */
  mpegts_input_add_network((mpegts_input_t*)iptv_input, (mpegts_network_t*)in);

  /* Load muxes */
  if ((c = hts_settings_load_r(1, "input/iptv/networks/%s/muxes",
                                idnode_uuid_as_str(&in->mn_id, ubuf)))) {
    htsmsg_field_t *f;
    htsmsg_t *e;
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_get_map_by_field(f)))  continue;
      if (!(e = htsmsg_get_map(e, "config"))) continue;
      iptv_mux_create0(in, f->hmf_name, e);
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
  return (mpegts_network_t*)iptv_network_create0(NULL, conf, idc);
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
      iptv_network_create0(f->hmf_name, e, &iptv_auto_network_class);
    else
      iptv_network_create0(f->hmf_name, e, &iptv_network_class);
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

void iptv_init ( void )
{
  /* Register handlers */
  iptv_http_init();
  iptv_udp_init();
  iptv_rtsp_init();
  iptv_pipe_init();
  iptv_file_init();
#if ENABLE_LIBAV
  iptv_libav_init();
#endif

  iptv_input = calloc(1, sizeof(iptv_input_t));

  /* Init Input */
  mpegts_input_create0((mpegts_input_t*)iptv_input,
                       &iptv_input_class, NULL, NULL);
  iptv_input->ti_wizard_get     = iptv_input_wizard_get;
  iptv_input->ti_wizard_set     = iptv_input_wizard_set;
  iptv_input->mi_warm_mux       = iptv_input_warm_mux;
  iptv_input->mi_start_mux      = iptv_input_start_mux;
  iptv_input->mi_stop_mux       = iptv_input_stop_mux;
  iptv_input->mi_is_enabled     = iptv_input_is_enabled;
  iptv_input->mi_get_weight     = iptv_input_get_weight;
  iptv_input->mi_get_grace      = iptv_input_get_grace;
  iptv_input->mi_get_priority   = iptv_input_get_priority;
  iptv_input->mi_display_name   = iptv_input_display_name;
  iptv_input->mi_enabled        = 1;

  /* Init Network */
  iptv_network_init();

  /* Setup TS thread */
  iptv_poll = tvhpoll_create(10);
  pthread_mutex_init(&iptv_lock, NULL);
  tvhthread_create(&iptv_thread, NULL, iptv_input_thread, NULL, "iptv");
}

void iptv_done ( void )
{
  pthread_kill(iptv_thread, SIGTERM);
  pthread_join(iptv_thread, NULL);
  tvhpoll_destroy(iptv_poll);
  pthread_mutex_lock(&global_lock);
  mpegts_network_unregister_builder(&iptv_auto_network_class);
  mpegts_network_unregister_builder(&iptv_network_class);
  mpegts_network_class_delete(&iptv_auto_network_class, 0);
  mpegts_network_class_delete(&iptv_network_class, 0);
  mpegts_input_stop_all((mpegts_input_t*)iptv_input);
  mpegts_input_delete((mpegts_input_t *)iptv_input, 0);
  pthread_mutex_unlock(&global_lock);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
