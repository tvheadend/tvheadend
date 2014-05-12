/*
 *  tvheadend, UPnP interface
 *  Copyright (C) 2014 Jaroslav Kysela
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

#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "tvheadend.h"
#include "tvhpoll.h"
#include "upnp.h"

int              upnp_running;
static pthread_t upnp_tid;
pthread_mutex_t  upnp_lock;

TAILQ_HEAD(upnp_active_services, upnp_service);

typedef struct upnp_data {
  TAILQ_ENTRY(upnp_data) data_link;
  struct sockaddr_storage storage;
  htsbuf_queue_t queue;
} upnp_data_t;

TAILQ_HEAD(upnp_data_queue_write, upnp_data);

static struct upnp_active_services upnp_services;
static struct upnp_data_queue_write upnp_data_write;
static struct sockaddr_storage upnp_ipv4_multicast;

/*
 *
 */
upnp_service_t *upnp_service_create0( upnp_service_t *us )
{
  pthread_mutex_lock(&upnp_lock);
  TAILQ_INSERT_TAIL(&upnp_services, us, us_link);
  pthread_mutex_unlock(&upnp_lock);
  return us;
}

void upnp_service_destroy( upnp_service_t *us )
{
  pthread_mutex_lock(&upnp_lock);
  TAILQ_REMOVE(&upnp_services, us, us_link);
  us->us_destroy(us);
  pthread_mutex_unlock(&upnp_lock);
  free(us);
}

/*
 *
 */
void
upnp_send( htsbuf_queue_t *q, struct sockaddr_storage *storage )
{
  upnp_data_t *data;

  if (!upnp_running)
    return;
  data = calloc(1, sizeof(upnp_data_t));
  htsbuf_queue_init(&data->queue, 0);
  htsbuf_appendq(&data->queue, q);
  if (storage == NULL)
    data->storage = upnp_ipv4_multicast;
  else
    data->storage = *storage;
  pthread_mutex_lock(&upnp_lock);
  TAILQ_INSERT_TAIL(&upnp_data_write, data, data_link);
  pthread_mutex_unlock(&upnp_lock);
}

/*
 *  Discovery thread
 */
static void *
upnp_thread( void *aux )
{
  char *bindaddr = aux;
  tvhpoll_t *poll = tvhpoll_create(2);
  tvhpoll_event_t ev[2];
  upnp_data_t *data;
  udp_connection_t *multicast = NULL, *unicast = NULL;
  udp_connection_t *conn;
  unsigned char buf[16384];
  upnp_service_t *us;
  struct sockaddr_storage ip;
  socklen_t iplen;
  size_t size;
  int r;

  multicast = udp_bind("upnp", "upnp_thread_multicast",
                       "239.255.255.250", 1900,
                       NULL, 32*1024);
  if (multicast == NULL || multicast == UDP_FATAL_ERROR)
    goto error;
  unicast = udp_bind("upnp", "upnp_thread_unicast", bindaddr, 0,
                     NULL, 32*1024);
  if (unicast == NULL || unicast == UDP_FATAL_ERROR)
    goto error;

  memset(&ev, 0, sizeof(ev));
  ev[0].fd       = multicast->fd;
  ev[0].events   = TVHPOLL_IN;
  ev[0].data.ptr = multicast;
  ev[1].fd       = unicast->fd;
  ev[1].events   = TVHPOLL_IN;
  ev[1].data.ptr = unicast;
  tvhpoll_add(poll, ev, 2);

  while (upnp_running && multicast->fd >= 0) {
    r = tvhpoll_wait(poll, ev, 2, 1000);

    while (r-- > 0) {
      if ((ev[r].events & TVHPOLL_IN) != 0) {
        conn = ev[r].data.ptr;
        iplen = sizeof(ip);
        size = recvfrom(conn->fd, buf, sizeof(buf), 0,
                                           (struct sockaddr *)&ip, &iplen);
#if ENABLE_TRACE
        if (size > 0) {
          char tbuf[256];
          inet_ntop(ip.ss_family, IP_IN_ADDR(ip), tbuf, sizeof(tbuf));
          tvhtrace("upnp", "%s - received data from %s:%hu [size=%zi]",
                   conn == multicast ? "multicast" : "unicast",
                   tbuf, IP_PORT(ip), size);
          tvhlog_hexdump("upnp", buf, size);
        }
#endif
        /* TODO: a filter */
        TAILQ_FOREACH(us, &upnp_services, us_link)
          us->us_received(buf, size, conn, &ip);
      }
    }

    while (1) {
      pthread_mutex_lock(&upnp_lock);
      data = TAILQ_FIRST(&upnp_data_write);
      if (data)
        TAILQ_REMOVE(&upnp_data_write, data, data_link);
      pthread_mutex_unlock(&upnp_lock);
      if (data == NULL)
        break;
      udp_write_queue(unicast, &data->queue, &data->storage);
      htsbuf_queue_flush(&data->queue);
      free(data);
    }
  }

error:
  upnp_running = 0;
  tvhpoll_destroy(poll);
  udp_close(unicast);
  udp_close(multicast);
  return NULL;
}

/*
 *  Fire up UPnP server
 */
void
upnp_server_init(const char *bindaddr)
{
  int r;

  memset(&upnp_ipv4_multicast, 0, sizeof(upnp_ipv4_multicast));
  upnp_ipv4_multicast.ss_family       = AF_INET;
  IP_AS_V4(upnp_ipv4_multicast, port) = htons(1900);
  r = inet_pton(AF_INET, "239.255.255.250", &IP_AS_V4(upnp_ipv4_multicast, addr));
  assert(r);

  pthread_mutex_init(&upnp_lock, NULL);
  TAILQ_INIT(&upnp_data_write);
  TAILQ_INIT(&upnp_services);
  upnp_running = 1;
  tvhthread_create(&upnp_tid, NULL, upnp_thread, (char *)bindaddr, 0);
}

void
upnp_server_done(void)
{
  upnp_data_t *data;
  upnp_service_t *us;

  upnp_running = 0;
  pthread_kill(upnp_tid, SIGTERM);
  pthread_join(upnp_tid, NULL);
  while ((us = TAILQ_FIRST(&upnp_services)) != NULL)
    upnp_service_destroy(us);
  while ((data = TAILQ_FIRST(&upnp_data_write)) != NULL) {
    TAILQ_REMOVE(&upnp_data_write, data, data_link);
    htsbuf_queue_flush(&data->queue);
    free(data);
  }
}
