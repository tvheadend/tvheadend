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
#include "tcp.h"

#include <string.h>
#include <sys/epoll.h>
#include <assert.h>
#include <regex.h>
#include <unistd.h>

/*
 * Globals
 */
iptv_input_t    iptv_input;
iptv_network_t  iptv_network;
int             iptv_poll_fd;
pthread_t       iptv_thread;
pthread_mutex_t iptv_lock;

/*
 * URL processing - TODO: move to a library
 */

typedef struct url
{
  char       buf[2048];
  const char *scheme;
  const char *host;
  uint16_t   port;
  const char *path;  
} url_t;

static int
url_parse ( url_t *up, const char *urlstr )
{
  char *t1, *t2;
  strcpy(up->buf, urlstr);

  /* Scheme */
  up->scheme = t1 = up->buf;
  if (!(t2 = strstr(t1, "://"))) {
    return 1;
  }
  *t2 = 0;

  /* Host */
  up->host = t1 = t2 + 3;
  up->path = NULL;
  if ((t2 = strstr(t1, "/"))) {
    *t2 = 0;
    up->path = t2 + 1;
  }

  /* Port */
  up->port = 0;
  if (!(t2 = strstr(up->host, ":"))) {
    if (!strcmp(up->scheme, "https"))
      up->port = 443;
    else if (!strcmp(up->scheme, "http"))
      up->port = 80;
  } else {
    *t2 = 0;
    up->port = atoi(t2+1);
  }

  return 0;
}

/*
 * HTTP client
 */

static int http_connect (const char *urlstr)
{
  int fd, c, i;
  char buf[1024];
  url_t url;
  if (url_parse(&url, urlstr))
    return -1;

  /* Make connection */
  // TODO: move connection to thread
  // TODO: this is really only for testing and to allow use of TVH webserver as input
  tvhlog(LOG_DEBUG, "iptv", "connecting to http %s %d", url.host, url.port);
  fd = tcp_connect(url.host, url.port, buf, sizeof(buf), 10);
  if (fd < 0) {
    tvhlog(LOG_ERR, "iptv", "tcp_connect() failed %s", buf);
    return -1;
  }

  /* Send request (VERY basic) */
  c = snprintf(buf, sizeof(buf), "GET /%s HTTP/1.1\r\n", url.path);
  tvh_write(fd, buf, c);
  c = snprintf(buf, sizeof(buf), "Hostname: %s\r\n", url.host);
  tvh_write(fd, buf, c);
  tvh_write(fd, "\r\n", 2);

  /* Read back header */
  // TODO: do this properly
  i = 0;
  while (1) {
    if (!(c = read(fd, buf+i, 1)))
      continue;
    i++;
    if (i == 4 && !strncmp(buf, "\r\n\r\n", 4))
      break;
    memmove(buf, buf+1, 3); i = 3;
  }
  printf("DONE\n");

  return fd;
}

/*
 * Input definition
 */
extern const idclass_t mpegts_input_class;
const idclass_t iptv_input_class = {
  .ic_super      = &mpegts_input_class,
  .ic_class      = "iptv_input",
  .ic_caption    = "IPTV Input",
  .ic_properties = (const property_t[]){
  }
};

/*
 * HTTP
 */
static int
iptv_input_start_http ( iptv_mux_t *im )
{
  /* Setup connection */
  im->mm_iptv_fd = http_connect(im->mm_iptv_url);

  return im->mm_iptv_fd <= 0 ? SM_CODE_TUNING_FAILED : 0;
}

static int
iptv_input_start_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  int ret = SM_CODE_TUNING_FAILED;
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;
  assert(mmi == &im->mm_iptv_instance);

  pthread_mutex_lock(&iptv_lock);
  if (!im->mm_active) {
    
    /* HTTP */
    // TODO: this needs to happen in a thread
    if (!strncmp(im->mm_iptv_url, "http", 4))
      ret = iptv_input_start_http(im);

    /* OK */
    if (!ret) {
      struct epoll_event ev;
      memset(&ev, 0, sizeof(ev));
      ev.events  = EPOLLIN;
      ev.data.fd = im->mm_iptv_fd;
      epoll_ctl(iptv_poll_fd, EPOLL_CTL_ADD, im->mm_iptv_fd, &ev);
      im->mm_active = mmi;

      /* Install table handlers */
      mpegts_table_add(mmi->mmi_mux, DVB_PAT_BASE, DVB_PAT_MASK,
                       dvb_pat_callback, NULL, "pat",
                       MT_QUICKREQ| MT_CRC, DVB_PAT_PID);

      // TODO: need to fire mux start event
    }
  }
  pthread_mutex_unlock(&iptv_lock);

  return ret;
}

static void
iptv_input_stop_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;
  assert(mmi == &im->mm_iptv_instance);
  
  pthread_mutex_lock(&iptv_lock);
  if (im->mm_active) {

    // TODO: multicast will require additional work

    close(im->mm_iptv_fd); // removes from epoll
    im->mm_iptv_fd = -1;
    
  }
  pthread_mutex_unlock(&iptv_lock);
}

static int
iptv_input_is_free ( mpegts_input_t *mi )
{
  return 1; // unlimited number of muxes
}

static int
iptv_input_current_weight ( mpegts_input_t *mi )
{
  return 0; // unlimited number of muxes
}

static void *
iptv_input_thread ( void *aux )
{
  int nfds, fd, r, pos = 0;
  uint8_t tsb[65536];
  struct epoll_event ev;
  iptv_mux_t *im;
  mpegts_mux_t *mm;

  while ( 1 ) {
    nfds = epoll_wait(iptv_poll_fd, &ev, 1, -1);
    if ( nfds < 0 ) {
      tvhlog(LOG_ERR, "iptv", "epoll() error %s, sleeping 1 second",
             strerror(errno));
      sleep(1);
      continue;
    } else if ( nfds == 0 ) {
      continue;
    }

    /* Read data */
    fd = ev.data.fd;
    r  = read(fd, tsb+pos, sizeof(tsb)-pos);

    /* Error */
    if (r < 0) {
      tvhlog(LOG_ERR, "iptv", "read() error %s", strerror(errno));
      // TODO: close and remove?
      continue;
    }

    pthread_mutex_lock(&iptv_lock);

    /* Find mux */
    LIST_FOREACH(mm, &iptv_network.mn_muxes, mm_network_link) {
      if (((iptv_mux_t*)mm)->mm_iptv_fd == fd)
        break;
    }
    if (!mm) {
      pthread_mutex_unlock(&iptv_lock);
      continue;
    }
    im = (iptv_mux_t*)mm;

    /* Raw TS */
    if (!strncmp(im->mm_iptv_url, "http", 4)) {
      pos = mpegts_input_recv_packets((mpegts_input_t*)&iptv_input,
                                      &im->mm_iptv_instance,
                                      tsb, r, NULL, NULL, "iptv");
    }

    pthread_mutex_unlock(&iptv_lock);
  }
  return NULL;
}

/*
 * Network definition
 */
extern const idclass_t mpegts_network_class;
const idclass_t iptv_network_class = {
  .ic_super      = &mpegts_network_class,
  .ic_class      = "iptv_network",
  .ic_caption    = "IPTV Network",
  .ic_properties = (const property_t[]){
  }
};

static mpegts_mux_t *
iptv_network_create_mux
  ( mpegts_mux_t *mm, uint16_t onid, uint16_t tsid, dvb_mux_conf_t *conf )
{
  return NULL;
}

static mpegts_service_t *
iptv_network_create_service
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid )
{
  return (mpegts_service_t*)
    iptv_service_create(NULL, (iptv_mux_t*)mm, sid, pmt_pid);
}

/*
 * Intialise and load config
 */
void iptv_init ( void )
{
  pthread_t tid;

  /* Init Input */
  mpegts_input_create0((mpegts_input_t*)&iptv_input,
                       &iptv_input_class, NULL, NULL);
  iptv_input.mi_start_mux      = iptv_input_start_mux;
  iptv_input.mi_stop_mux       = iptv_input_stop_mux;
  iptv_input.mi_is_free        = iptv_input_is_free;
  iptv_input.mi_current_weight = iptv_input_current_weight;
  iptv_input.mi_enabled        = 1;

  /* Init Network */
  mpegts_network_create0((mpegts_network_t*)&iptv_network,
                         &iptv_network_class, NULL, "IPTV Network", NULL);
  iptv_network.mn_create_mux     = iptv_network_create_mux;
  iptv_network.mn_create_service = iptv_network_create_service;

  /* Link */
  mpegts_input_set_network((mpegts_input_t*)&iptv_input,
                           (mpegts_network_t*)&iptv_network);
  /* Set table thread */
  pthread_create(&tid, NULL, mpegts_input_table_thread, &iptv_input);

  /* Setup TS thread */
  // TODO: could set this up only when needed
  iptv_poll_fd = epoll_create(10);
  pthread_mutex_init(&iptv_lock, NULL);
  pthread_create(&iptv_thread, NULL, iptv_input_thread, NULL);

  /* Load config */
  iptv_mux_load_all();
}


/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
