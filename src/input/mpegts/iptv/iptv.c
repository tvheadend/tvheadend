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

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <regex.h>
#include <unistd.h>
#include <regex.h>
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
      tvhwarn("iptv", "attempt to re-register handler for %s",
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
  .ic_caption    = N_("IPTV Input"),
  .ic_get_title  = iptv_input_class_get_title,
  .ic_properties = (const property_t[]){
    {}
  }
};

static int
iptv_input_is_free ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  int c = 0;
  mpegts_mux_instance_t *mmi;
  iptv_network_t *in = (iptv_network_t *)mm->mm_network;
  
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link)
    if (mmi->mmi_mux->mm_network == (mpegts_network_t *)in)
      c++;
  
  /* Limit reached */
  if (in->in_max_streams && c >= in->in_max_streams)
    return 0;
  
  /* Bandwidth reached */
  if (in->in_bw_limited)
      return 0;

  return 1;
}

static int
iptv_input_get_weight ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags )
{
  int w = 0;
  const th_subscription_t *ths;
  const service_t *s;
  mpegts_mux_instance_t *mmi;

  /* Find the "min" weight */
  if (!iptv_input_is_free(mi, mm)) {
    w = 1000000;

    /* Service subs */
    pthread_mutex_lock(&mi->mi_output_lock);
    LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link)
      LIST_FOREACH(s, &mmi->mmi_mux->mm_transports, s_active_link)
        LIST_FOREACH(ths, &s->s_subscriptions, ths_service_link)
          w = MIN(w, ths->ths_weight);
    pthread_mutex_unlock(&mi->mi_output_lock);
  }

  return w;

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

  /* Already active */
  if (im->mm_active)
    return 0;

  /* Do we need to stop something? */
  if (!iptv_input_is_free(mi, mmi->mmi_mux)) {
    pthread_mutex_lock(&mi->mi_output_lock);
    mpegts_mux_instance_t *m, *s = NULL;
    int w = 1000000;
    LIST_FOREACH(m, &mi->mi_mux_active, mmi_active_link) {
      int t = mpegts_mux_instance_weight(m);
      if (t < w) {
        s = m;
        w = t;
      }
    }
    pthread_mutex_unlock(&mi->mi_output_lock);
  
    /* Stop */
    if (s)
      s->mmi_mux->mm_stop(s->mmi_mux, 1, SM_CODE_ABORTED);
  }
  return 0;
}

static const char *
iptv_sub_url_encode(iptv_mux_t *im, const char *s, char *tmp, size_t tmplen)
{
  char *p;
  if (im->mm_iptv_url && !strncmp(im->mm_iptv_url, "pipe://", 7))
    return s;
  p = url_encode(s);
  strncpy(tmp, p, tmplen-1);
  tmp[tmplen-1] = '\0';
  free(p);
  return tmp;
}

static const char *
iptv_sub_mux_name(const char *id, const void *aux, char *tmp, size_t tmplen)
{
  const mpegts_mux_instance_t *mmi = aux;
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;
  return iptv_sub_url_encode(im, im->mm_iptv_muxname, tmp, tmplen);
}

static const char *
iptv_sub_service_name(const char *id, const void *aux, char *tmp, size_t tmplen)
{
  const mpegts_mux_instance_t *mmi = aux;
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;
  return iptv_sub_url_encode(im, im->mm_iptv_svcname, tmp, tmplen);
}

static const char *
iptv_sub_weight(const char *id, const void *aux, char *tmp, size_t tmplen)
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
iptv_input_start_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
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
  memset(&url, 0, sizeof(url));

  if (raw && !strncmp(raw, "pipe://", 7)) {

    scheme = "pipe";

  } else {

    if (urlparse(raw ?: "", &url)) {
      tvherror("iptv", "%s - invalid URL [%s]", buf, raw);
      return ret;
    }
    scheme = url.scheme;

  }

  /* Find scheme handler */
  ih = iptv_handler_find(scheme ?: "");
  if (!ih) {
    tvherror("iptv", "%s - unsupported scheme [%s]", buf, scheme ?: "none");
    return ret;
  }

  /* Start */
  pthread_mutex_lock(&iptv_lock);
  s = im->mm_iptv_url_raw;
  im->mm_iptv_url_raw = strdup(raw);
  im->mm_active = mmi; // Note: must set here else mux_started call
                       // will not realise we're ready to accept pid open calls
  ret = ih->start(im, raw, &url);
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
  mpegts_network_link_t *mnl;

  pthread_mutex_lock(&iptv_lock);

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
  LIST_FOREACH(mnl, &mi->mi_networks, mnl_mi_link) {
    iptv_network_t *in = (iptv_network_t*)mnl->mnl_network;
    in->in_bw_limited = 0;
  }

  pthread_mutex_unlock(&iptv_lock);
}

static void
iptv_input_display_name ( mpegts_input_t *mi, char *buf, size_t len )
{
  snprintf(buf, len, "IPTV");
}

static void *
iptv_input_thread ( void *aux )
{
  int nfds;
  ssize_t n;
  iptv_mux_t *im;
  tvhpoll_event_t ev;

  while ( tvheadend_running ) {
    nfds = tvhpoll_wait(iptv_poll, &ev, 1, -1);
    if ( nfds < 0 ) {
      if (tvheadend_running && !ERRNO_AGAIN(errno)) {
        tvhlog(LOG_ERR, "iptv", "poll() error %s, sleeping 1 second",
               strerror(errno));
        sleep(1);
      }
      continue;
    } else if ( nfds == 0 ) {
      continue;
    }
    im = ev.data.ptr;

    pthread_mutex_lock(&iptv_lock);

    /* Only when active */
    if (im->mm_active) {
      /* Get data */
      if ((n = im->im_handler->read(im)) < 0) {
        tvhlog(LOG_ERR, "iptv", "read() error %s", strerror(errno));
        im->im_handler->stop(im);
        break;
      }
      iptv_input_recv_packets(im, n);
    }

    pthread_mutex_unlock(&iptv_lock);
  }
  return NULL;
}

void
iptv_input_recv_packets ( iptv_mux_t *im, ssize_t len )
{
  static time_t t1 = 0, t2;
  iptv_network_t *in = (iptv_network_t*)im->mm_network;
  mpegts_mux_instance_t *mmi;

  in->in_bps += len * 8;
  time(&t2);
  if (t2 != t1) {
    if (in->in_max_bandwidth &&
        in->in_bps > in->in_max_bandwidth * 1024) {
      if (!in->in_bw_limited) {
        tvhinfo("iptv", "%s bandwidth limited exceeded",
                idnode_get_title(&in->mn_id, NULL));
        in->in_bw_limited = 1;
      }
    }
    in->in_bps = 0;
    t1 = t2;
  }

  /* Pass on */
  mmi = im->mm_active;
  if (mmi)
    mpegts_input_recv_packets((mpegts_input_t*)iptv_input, mmi,
                              &im->mm_iptv_buffer, NULL, NULL);
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
      tvherror("iptv", "%s - failed to add to poll q", buf);
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
      tvherror("iptv", "%s - failed to add to poll q (2)", buf);
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
  sbuf_init_fixed(&im->mm_iptv_buffer, IPTV_BUF_SIZE);
  im->mm_iptv_rtp_seq = -1;

  if (iptv_input_fd_started(im))
    return;

  /* Install table handlers */
  mpegts_mux_t *mm = (mpegts_mux_t*)im;
  if (mm->mm_active)
    psi_tables_install(mm->mm_active->mmi_input, mm,
                       im->mm_iptv_atsc ? DVB_SYS_ATSC_ALL : DVB_SYS_DVBT);
}

/* **************************************************************************
 * IPTV network
 * *************************************************************************/

static void
iptv_network_class_delete ( idnode_t *in )
{
  mpegts_network_t *mn = (mpegts_network_t*)in;

  /* Remove config */
  hts_settings_remove("input/iptv/networks/%s",
                      idnode_uuid_as_sstr(in));

  /* delete */
  mpegts_network_delete(mn, 1);
}

extern const idclass_t mpegts_network_class;
const idclass_t iptv_network_class = {
  .ic_super      = &mpegts_network_class,
  .ic_class      = "iptv_network",
  .ic_caption    = N_("IPTV Network"),
  .ic_delete     = iptv_network_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_INT,
      .id       = "priority",
      .name     = N_("Priority"),
      .off      = offsetof(iptv_network_t, in_priority),
      .def.i    = 1,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_INT,
      .id       = "spriority",
      .name     = N_("Streaming Priority"),
      .off      = offsetof(iptv_network_t, in_streaming_priority),
      .def.i    = 1,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_U32,
      .id       = "max_streams",
      .name     = N_("Maximum Input Streams"),
      .off      = offsetof(iptv_network_t, in_max_streams),
      .def.i    = 0,
    },
    {
      .type     = PT_U32,
      .id       = "max_bandwidth",
      .name     = N_("Maximum Bandwidth (Kbps)"),
      .off      = offsetof(iptv_network_t, in_max_bandwidth),
      .def.i    = 0,
    },
    {
      .type     = PT_U32,
      .id       = "max_timeout",
      .name     = N_("Maximum timeout (seconds)"),
      .off      = offsetof(iptv_network_t, in_max_timeout),
      .def.i    = 15,
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

static void
iptv_network_config_save ( mpegts_network_t *mn )
{
  htsmsg_t *c = htsmsg_create_map();
  idnode_save(&mn->mn_id, c);
  hts_settings_save(c, "input/iptv/networks/%s/config",
                    idnode_uuid_as_sstr(&mn->mn_id));
  htsmsg_destroy(c);
}

iptv_network_t *
iptv_network_create0
  ( const char *uuid, htsmsg_t *conf )
{
  iptv_network_t *in = calloc(1, sizeof(*in));
  htsmsg_t *c;

  /* Init Network */
  in->in_priority       = 1;
  in->in_streaming_priority = 1;
  if (!mpegts_network_create0((mpegts_network_t *)in,
                              &iptv_network_class,
                              uuid, NULL, conf)) {
    free(in);
    return NULL;
  }
  in->mn_create_service = iptv_network_create_service;
  in->mn_mux_class      = iptv_network_mux_class;
  in->mn_mux_create2    = iptv_network_create_mux2;
  in->mn_config_save    = iptv_network_config_save;
 
  /* Defaults */
  if (!conf) {
    in->mn_skipinitscan = 1;
  }

  /* Link */
  mpegts_input_add_network((mpegts_input_t*)iptv_input, (mpegts_network_t*)in);

  /* Load muxes */
  if ((c = hts_settings_load_r(1, "input/iptv/networks/%s/muxes",
                                idnode_uuid_as_sstr(&in->mn_id)))) {
    htsmsg_field_t *f;
    htsmsg_t *e;
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_get_map_by_field(f)))  continue;
      if (!(e = htsmsg_get_map(e, "config"))) continue;
      iptv_mux_create0(in, f->hmf_name, e);
    }
    htsmsg_destroy(c);
  }
  
  return in;
}

static mpegts_network_t *
iptv_network_builder
  ( const idclass_t *idc, htsmsg_t *conf )
{
  return (mpegts_network_t*)iptv_network_create0(NULL, conf);
}

/* **************************************************************************
 * IPTV initialise
 * *************************************************************************/

static void
iptv_network_init ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  /* Register builder */
  mpegts_network_register_builder(&iptv_network_class,
                                  iptv_network_builder);

  /* Load settings */
  if (!(c = hts_settings_load_r(1, "input/iptv/networks")))
    return;

  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_get_map_by_field(f)))  continue;
    if (!(e = htsmsg_get_map(e, "config"))) continue;
    iptv_network_create0(f->hmf_name, e);
  }
  htsmsg_destroy(c);
}

void iptv_init ( void )
{
  /* Register handlers */
  iptv_http_init();
  iptv_udp_init();
  iptv_rtsp_init();
  iptv_pipe_init();

  iptv_input = calloc(1, sizeof(iptv_input_t));

  /* Init Input */
  mpegts_input_create0((mpegts_input_t*)iptv_input,
                       &iptv_input_class, NULL, NULL);
  iptv_input->mi_warm_mux       = iptv_input_warm_mux;
  iptv_input->mi_start_mux      = iptv_input_start_mux;
  iptv_input->mi_stop_mux       = iptv_input_stop_mux;
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
  tvhthread_create(&iptv_thread, NULL, iptv_input_thread, NULL);
}

void iptv_done ( void )
{
  pthread_kill(iptv_thread, SIGTERM);
  pthread_join(iptv_thread, NULL);
  tvhpoll_destroy(iptv_poll);
  pthread_mutex_lock(&global_lock);
  mpegts_network_unregister_builder(&iptv_network_class);
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
