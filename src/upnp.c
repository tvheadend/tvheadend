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

#include "tvheadend.h"
#include "tvhpoll.h"
#include "upnp.h"

#include <signal.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#if defined(PLATFORM_FREEBSD) || ENABLE_ANDROID
#include <sys/types.h>
#include <sys/socket.h>
#endif

int              upnp_running;
static pthread_t upnp_tid;
tvh_mutex_t      upnp_lock;

TAILQ_HEAD(upnp_active_services, upnp_service);

typedef struct upnp_data {
  TAILQ_ENTRY(upnp_data) data_link;
  struct sockaddr_storage storage;
  htsbuf_queue_t queue;
  int delay_ms;
  int from_multicast;
  int fill_source;
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
  tvh_mutex_lock(&upnp_lock);
  TAILQ_INSERT_TAIL(&upnp_services, us, us_link);
  tvh_mutex_unlock(&upnp_lock);
  return us;
}

void upnp_service_destroy( upnp_service_t *us )
{
  tvh_mutex_lock(&upnp_lock);
  TAILQ_REMOVE(&upnp_services, us, us_link);
  us->us_destroy(us);
  tvh_mutex_unlock(&upnp_lock);
  free(us);
}

/*
 *
 */
void
upnp_send( htsbuf_queue_t *q, struct sockaddr_storage *storage,
           int delay_ms, int from_multicast, int fill_source )
{
  upnp_data_t *data;

  if (!atomic_get(&upnp_running))
    return;
  data = calloc(1, sizeof(upnp_data_t));
  htsbuf_queue_init(&data->queue, 0);
  htsbuf_appendq(&data->queue, q);
  if (storage == NULL)
    data->storage = upnp_ipv4_multicast;
  else
    data->storage = *storage;
  data->delay_ms = delay_ms;
  data->from_multicast = from_multicast;
  data->fill_source = fill_source;
  tvh_mutex_lock(&upnp_lock);
  TAILQ_INSERT_TAIL(&upnp_data_write, data, data_link);
  tvh_mutex_unlock(&upnp_lock);
}

/*
 *
 */
static void
upnp_dump_data( upnp_data_t *data )
{
#if 0
  char tbuf[256];
  inet_ntop(data->storage.ss_family, IP_IN_ADDR(data->storage), tbuf, sizeof(tbuf));
  printf("upnp out to %s:%d\n", tbuf, ntohs(IP_PORT(data->storage)));
  htsbuf_hexdump(&data->queue, "upnp out");
#endif
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
  int r, delay_ms;

  multicast = udp_bind(LS_UPNP, "upnp_thread_multicast",
                       "239.255.255.250", 1900, NULL,
                       NULL, 32*1024, 32*1024);
  if (multicast == NULL || multicast == UDP_FATAL_ERROR)
    goto error;
  unicast = udp_bind(LS_UPNP, "upnp_thread_unicast", bindaddr, 0, NULL,
                     NULL, 32*1024, 32*1024);
  if (unicast == NULL || unicast == UDP_FATAL_ERROR)
    goto error;

  tvhpoll_event(ev+0, multicast->fd, TVHPOLL_IN, multicast);
  tvhpoll_event(ev+1, unicast->fd, TVHPOLL_IN, unicast);
  tvhpoll_add(poll, ev, 2);

  delay_ms = 0;

  while (atomic_get(&upnp_running) && multicast->fd >= 0) {
    r = tvhpoll_wait(poll, ev, 2, delay_ms ?: 1000);
    if (r == 0) /* timeout */
      delay_ms = 0;

    while (r-- > 0) {
      if ((ev[r].events & TVHPOLL_IN) != 0) {
        conn = ev[r].ptr;
        iplen = sizeof(ip);
        size = recvfrom(conn->fd, buf, sizeof(buf), 0,
                                           (struct sockaddr *)&ip, &iplen);
        if (size > 0 && tvhtrace_enabled()) {
          char tbuf[256];
          inet_ntop(ip.ss_family, IP_IN_ADDR(ip), tbuf, sizeof(tbuf));
          tvhtrace(LS_UPNP, "%s - received data from %s:%hu [size=%zi]",
                   conn == multicast ? "multicast" : "unicast",
                   tbuf, (unsigned short) ntohs(IP_PORT(ip)), size);
          tvhlog_hexdump(LS_UPNP, buf, size);
        }
        /* TODO: a filter */
        TAILQ_FOREACH(us, &upnp_services, us_link)
          us->us_received(buf, size, conn, &ip);
      }
    }

    while (delay_ms == 0) {
      tvh_mutex_lock(&upnp_lock);
      data = TAILQ_FIRST(&upnp_data_write);
      if (data) {
        delay_ms = data->delay_ms;
        data->delay_ms = 0;
        if (!delay_ms) {
          TAILQ_REMOVE(&upnp_data_write, data, data_link);
        } else {
          data = NULL;
        }
      }
      tvh_mutex_unlock(&upnp_lock);
      if (data == NULL)
        break;
      upnp_dump_data(data);
      udp_write_queue_fill_source(data->from_multicast ? multicast : unicast,
                      &data->queue, &data->storage, data->fill_source);
      htsbuf_queue_flush(&data->queue);
      free(data);
      delay_ms = 0;
    }
  }

  /* flush the write queue (byebye messages) */
  while (1) {
    tvh_mutex_lock(&upnp_lock);
    data = TAILQ_FIRST(&upnp_data_write);
    if (data)
      TAILQ_REMOVE(&upnp_data_write, data, data_link);
    tvh_mutex_unlock(&upnp_lock);
    if (data == NULL)
      break;
    tvh_safe_usleep((long)data->delay_ms * 1000);
    upnp_dump_data(data);
    udp_write_queue_fill_source(unicast, &data->queue, &data->storage, data->fill_source);
    htsbuf_queue_flush(&data->queue);
    free(data);
  }

error:
  atomic_set(&upnp_running, 0);
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
  IP_AS_V4(&upnp_ipv4_multicast, port) = htons(1900);
  r = inet_pton(AF_INET, "239.255.255.250", &IP_AS_V4(&upnp_ipv4_multicast, addr));
  assert(r);

  tvh_mutex_init(&upnp_lock, NULL);
  TAILQ_INIT(&upnp_data_write);
  TAILQ_INIT(&upnp_services);
  atomic_set(&upnp_running, 1);
  tvh_thread_create(&upnp_tid, NULL, upnp_thread, (char *)bindaddr, "upnp");
}

void
upnp_server_done(void)
{
  upnp_data_t *data;
  upnp_service_t *us;

  atomic_set(&upnp_running, 0);
  tvh_thread_kill(upnp_tid, SIGTERM);
  pthread_join(upnp_tid, NULL);
  while ((us = TAILQ_FIRST(&upnp_services)) != NULL)
    upnp_service_destroy(us);
  while ((data = TAILQ_FIRST(&upnp_data_write)) != NULL) {
    TAILQ_REMOVE(&upnp_data_write, data, data_link);
    htsbuf_queue_flush(&data->queue);
    free(data);
  }
}
