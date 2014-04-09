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
  return strcasecmp(a->scheme, b->scheme);
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
iptv_input_class_get_title ( idnode_t *self )
{
  return "IPTV";
}
extern const idclass_t mpegts_input_class;
const idclass_t iptv_input_class = {
  .ic_super      = &mpegts_input_class,
  .ic_class      = "iptv_input",
  .ic_caption    = "IPTV Input",
  .ic_get_title  = iptv_input_class_get_title,
  .ic_properties = (const property_t[]){
    {}
  }
};

static int
iptv_input_is_free ( mpegts_input_t *mi )
{
  int c = 0;
  mpegts_mux_instance_t *mmi;
  mpegts_network_link_t *mnl;
  
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link)
    c++;
  
  /* Limit reached */
  LIST_FOREACH(mnl, &mi->mi_networks, mnl_mi_link) {
    iptv_network_t *in = (iptv_network_t*)mnl->mnl_network;
    if (in->in_max_streams && c >= in->in_max_streams)
      return 0;
  }
  
  /* Bandwidth reached */
  LIST_FOREACH(mnl, &mi->mi_networks, mnl_mi_link) {
    iptv_network_t *in = (iptv_network_t*)mnl->mnl_network;
    if (in->in_bw_limited)
      return 0;
  }

  return 1;
}

static int
iptv_input_get_weight ( mpegts_input_t *mi )
{
  int c = 0, w = 0;
  const th_subscription_t *ths;
  const service_t *s;
  const mpegts_mux_instance_t *mmi;
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link)
    c++;

  /* Find the "min" weight */
  if (!iptv_input_is_free(mi)) {
    w = 1000000;

    /* Direct subs */
    LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link) {
      LIST_FOREACH(ths, &mmi->mmi_subs, ths_mmi_link) {
        w = MIN(w, ths->ths_weight);
      }
    }

    /* Service subs */
    pthread_mutex_lock(&mi->mi_output_lock);
    LIST_FOREACH(s, &mi->mi_transports, s_active_link) {
      LIST_FOREACH(ths, &s->s_subscriptions, ths_service_link) {
        w = MIN(w, ths->ths_weight);
      }
    }
    pthread_mutex_unlock(&mi->mi_output_lock);
  }

  return w;

}

static int
iptv_input_start_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  int ret = SM_CODE_TUNING_FAILED;
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;
  iptv_handler_t *ih;
  char buf[256];
  url_t url;

  /* Already active */
  if (im->mm_active)
    return 0;

  /* Do we need to stop something? */
  if (!iptv_input_is_free(mi)) {
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
      s->mmi_mux->mm_stop(s->mmi_mux, 1);
  }

  /* Parse URL */
  im->mm_display_name((mpegts_mux_t*)im, buf, sizeof(buf));
  if (urlparse(im->mm_iptv_url ?: "", &url)) {
    tvherror("iptv", "%s - invalid URL [%s]", buf, im->mm_iptv_url);
    return ret;
  }

  /* Find scheme handler */
  ih = iptv_handler_find(url.scheme);
  if (!ih) {
    tvherror("iptv", "%s - unsupported scheme [%s]", buf, url.scheme);
    return ret;
  }

  /* Start */
  pthread_mutex_lock(&iptv_lock);
  im->mm_active = mmi; // Note: must set here else mux_started call
                       // will not realise we're ready to accept pid open calls
  ret            = ih->start(im, &url);
  if (!ret)
    im->im_handler = ih;
  else
    im->mm_active  = NULL;
  pthread_mutex_unlock(&iptv_lock);

  return ret;
}

static void
iptv_input_stop_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;
  mpegts_network_link_t *mnl;

  // Not active??
  if (!im->mm_active)
    return;
  
  /* Stop */
  if (im->im_handler->stop)
    im->im_handler->stop(im);

  pthread_mutex_lock(&iptv_lock);

  /* Close file */
  if (im->mm_iptv_fd > 0) {
    udp_close(im->mm_iptv_connection); // removes from poll
    im->mm_iptv_connection = NULL;
    im->mm_iptv_fd = -1;
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
  size_t off;
  iptv_mux_t *im;
  tvhpoll_event_t ev;

  while ( tvheadend_running ) {
    nfds = tvhpoll_wait(iptv_poll, &ev, 1, -1);
    if ( nfds < 0 ) {
      if (tvheadend_running) {
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

    /* No longer active */
    if (!im->mm_active)
      goto done;

    /* Get data */
    off = 0;
    if ((n = im->im_handler->read(im, &off)) < 0) {
      tvhlog(LOG_ERR, "iptv", "read() error %s", strerror(errno));
      im->im_handler->stop(im);
      goto done;
    }
    iptv_input_recv_packets(im, n, off);

done:
    pthread_mutex_unlock(&iptv_lock);
  }
  return NULL;
}

void
iptv_input_recv_packets ( iptv_mux_t *im, ssize_t len, size_t off )
{
  static time_t t1 = 0, t2;
  iptv_network_t *in = (iptv_network_t*)im->mm_network;
  in->in_bps += len * 8;
  time(&t2);
  if (t2 != t1) {
    if (in->in_max_bandwidth &&
        in->in_bps > in->in_max_bandwidth * 1024) {
      if (!in->in_bw_limited) {
        tvhinfo("iptv", "%s bandwidth limited exceeded",
                idnode_get_title(&in->mn_id));
        in->in_bw_limited = 1;
      }
    }
    in->in_bps = 0;
    t1 = t2;
  }

  /* Pass on */
  mpegts_input_recv_packets((mpegts_input_t*)iptv_input, im->mm_active,
                            &im->mm_iptv_buffer, off, NULL, NULL);
}

void
iptv_input_mux_started ( iptv_mux_t *im )
{
  tvhpoll_event_t ev = { 0 };
  char buf[256];
  im->mm_display_name((mpegts_mux_t*)im, buf, sizeof(buf));

  /* Allocate input buffer */
  sbuf_init_fixed(&im->mm_iptv_buffer, IPTV_PKT_SIZE);

  /* Setup poll */
  if (im->mm_iptv_fd > 0) {
    ev.fd       = im->mm_iptv_fd;
    ev.events   = TVHPOLL_IN;
    ev.data.ptr = im;

    /* Error? */
    if (tvhpoll_add(iptv_poll, &ev, 1) == -1) {
      tvherror("iptv", "%s - failed to add to poll q", buf);
      close(im->mm_iptv_fd);
      im->mm_iptv_fd = -1;
      return;
    }
  }

  /* Install table handlers */
  mpegts_mux_t *mm = (mpegts_mux_t*)im;
  psi_tables_default(mm);
  if (im->mm_iptv_atsc) {
    psi_tables_atsc_t(mm);
    psi_tables_atsc_c(mm);
  } else
    psi_tables_dvb(mm);
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
                      idnode_uuid_as_str(in));

  /* delete */
  mpegts_network_delete(mn, 1);
}

extern const idclass_t mpegts_network_class;
const idclass_t iptv_network_class = {
  .ic_super      = &mpegts_network_class,
  .ic_class      = "iptv_network",
  .ic_caption    = "IPTV Network",
  .ic_delete     = iptv_network_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_U32,
      .id       = "max_streams",
      .name     = "Max Input Streams",
      .off      = offsetof(iptv_network_t, in_max_streams),
      .def.i    = 0,
    },
    {
      .type     = PT_U32,
      .id       = "max_bandwidth",
      .name     = "Max Bandwidth (Kbps)",
      .off      = offsetof(iptv_network_t, in_max_bandwidth),
      .def.i    = 0,
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
                    idnode_uuid_as_str(&mn->mn_id));
  htsmsg_destroy(c);
}

iptv_network_t *
iptv_network_create0
  ( const char *uuid, htsmsg_t *conf )
{
  iptv_network_t *in;
  htsmsg_t *c;

  /* Init Network */
  if (!(in = mpegts_network_create(iptv_network, uuid, NULL, conf)))
    return NULL;
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
                                idnode_uuid_as_str(&in->mn_id)))) {
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

  iptv_input = calloc(1, sizeof(iptv_input_t));

  /* Init Input */
  mpegts_input_create0((mpegts_input_t*)iptv_input,
                       &iptv_input_class, NULL, NULL);
  iptv_input->mi_start_mux      = iptv_input_start_mux;
  iptv_input->mi_stop_mux       = iptv_input_stop_mux;
  iptv_input->mi_is_free        = iptv_input_is_free;
  iptv_input->mi_get_weight     = iptv_input_get_weight;
  iptv_input->mi_display_name   = iptv_input_display_name;
  iptv_input->mi_enabled        = 1;

  /* Init Network */
  iptv_network_init();

  /* Setup TS thread */
  iptv_poll = tvhpoll_create(10);
  pthread_mutex_init(&iptv_lock, NULL);
  tvhthread_create(&iptv_thread, NULL, iptv_input_thread, NULL, 0);
}

void iptv_done ( void )
{
  pthread_kill(iptv_thread, SIGTERM);
  pthread_join(iptv_thread, NULL);
  tvhpoll_destroy(iptv_poll);
  pthread_mutex_lock(&global_lock);
  mpegts_network_unregister_builder(&iptv_network_class);
  mpegts_input_stop_all((mpegts_input_t*)iptv_input);
  mpegts_input_delete((mpegts_input_t *)iptv_input, 0);
  pthread_mutex_unlock(&global_lock);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
