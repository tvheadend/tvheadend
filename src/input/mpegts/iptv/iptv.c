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

/* **************************************************************************
 * IPTV state
 * *************************************************************************/

iptv_input_t    iptv_input;
iptv_network_t  iptv_network;
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
iptv_input_start_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  int ret = SM_CODE_TUNING_FAILED;
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;
  iptv_handler_t *ih;
  assert(mmi == &im->mm_iptv_instance);
  char buf[256];
  url_t url;

  /* Already active */
  if (im->mm_active)
    return 0;

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
  im->mm_active  = mmi;
  im->im_handler = ih;
  ret            = ih->start(im, &url);
  pthread_mutex_unlock(&iptv_lock);

  return ret;
}

static void
iptv_input_stop_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;
  assert(mmi == &im->mm_iptv_instance);

  // Not active??
  if (!im->mm_active)
    return;
  
  pthread_mutex_lock(&iptv_lock);
  
  /* Stop */
  im->im_handler->stop(im);

  /* Close file */
  if (im->mm_iptv_fd > 0) {
    close(im->mm_iptv_fd); // removes from poll
    im->mm_iptv_fd = -1;
  }

  /* Free memory */
  free(im->mm_iptv_tsb);
  im->mm_iptv_tsb = NULL;

  pthread_mutex_unlock(&iptv_lock);
}

static int
iptv_input_is_free ( mpegts_input_t *mi )
{
  return 1; // unlimited number of muxes
}

static int
iptv_input_get_weight ( mpegts_input_t *mi )
{
  return 0; // unlimited number of muxes
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
  ssize_t len;
  size_t off;
  iptv_mux_t *im;
  tvhpoll_event_t ev;

  while ( 1 ) {
    nfds = tvhpoll_wait(iptv_poll, &ev, 1, -1);
    if ( nfds < 0 ) {
      tvhlog(LOG_ERR, "iptv", "poll() error %s, sleeping 1 second",
             strerror(errno));
      sleep(1);
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
    if ((len = im->im_handler->read(im, &off)) < 0) {
      tvhlog(LOG_ERR, "iptv", "read() error %s", strerror(errno));
      im->im_handler->stop(im);
      goto done;
    }

    /* Pass on */
    im->mm_iptv_pos
      = mpegts_input_recv_packets((mpegts_input_t*)&iptv_input,
                                  im->mm_active,
                                  im->mm_iptv_tsb+off, len,
                                  NULL, NULL, "iptv");

done:
    pthread_mutex_unlock(&iptv_lock);
  }
  return NULL;
}

void
iptv_input_mux_started ( iptv_mux_t *im )
{
  tvhpoll_event_t ev = { 0 };
  char buf[256];
  im->mm_display_name((mpegts_mux_t*)im, buf, sizeof(buf));

  /* Allocate input buffer */
  im->mm_iptv_pos = 0;
  im->mm_iptv_tsb = calloc(1, IPTV_PKT_SIZE);

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
  // Note: we don't install NIT as we can't do mux discovery
  // TODO: not currently installing ATSC handler
  mpegts_table_add((mpegts_mux_t*)im, DVB_SDT_BASE, DVB_SDT_MASK,
                   dvb_sdt_callback, NULL, "sdt",
                   MT_QUICKREQ | MT_CRC, DVB_SDT_PID);
  mpegts_table_add((mpegts_mux_t*)im, DVB_PAT_BASE, DVB_PAT_MASK,
                   dvb_pat_callback, NULL, "pat",
                   MT_QUICKREQ | MT_CRC, DVB_PAT_PID);
}

/* **************************************************************************
 * IPTV network
 * *************************************************************************/

extern const idclass_t mpegts_network_class;
const idclass_t iptv_network_class = {
  .ic_super      = &mpegts_network_class,
  .ic_class      = "iptv_network",
  .ic_caption    = "IPTV Network",
  .ic_properties = (const property_t[]){
    {}
  }
};

static mpegts_mux_t *
iptv_network_create_mux2
  ( mpegts_network_t *mm, htsmsg_t *conf )
{
  return (mpegts_mux_t*)iptv_mux_create(NULL, conf);
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

/* **************************************************************************
 * IPTV initialise
 * *************************************************************************/

void iptv_init ( void )
{
  pthread_t tid;

  /* Register handlers */
  iptv_http_init();

  /* Init Input */
  mpegts_input_create0((mpegts_input_t*)&iptv_input,
                       &iptv_input_class, NULL, NULL);
  iptv_input.mi_start_mux      = iptv_input_start_mux;
  iptv_input.mi_stop_mux       = iptv_input_stop_mux;
  iptv_input.mi_is_free        = iptv_input_is_free;
  iptv_input.mi_get_weight     = iptv_input_get_weight;
  iptv_input.mi_display_name   = iptv_input_display_name;
  iptv_input.mi_enabled        = 1;

  /* Init Network */
  mpegts_network_create0((mpegts_network_t*)&iptv_network,
                         &iptv_network_class, NULL, "IPTV Network", NULL);
  iptv_network.mn_create_service = iptv_network_create_service;
  iptv_network.mn_mux_class      = iptv_network_mux_class;
  iptv_network.mn_mux_create2    = iptv_network_create_mux2;

  /* Link */
  mpegts_input_set_network((mpegts_input_t*)&iptv_input,
                           (mpegts_network_t*)&iptv_network);
  /* Set table thread */
  tvhthread_create(&tid, NULL, mpegts_input_table_thread, &iptv_input, 1);

  /* Setup TS thread */
  iptv_poll = tvhpoll_create(10);
  pthread_mutex_init(&iptv_lock, NULL);
  tvhthread_create(&iptv_thread, NULL, iptv_input_thread, NULL, 1);

  /* Load config */
  iptv_mux_load_all();
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
