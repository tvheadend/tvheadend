/*
 *  Tvheadend - HTTP client functions
 *
 *  Copyright (C) 2013 Adam Sutton
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
#include "redblack.h"
#include "queue.h"
#include "url.h"
#include "http.h"

#if ENABLE_CURL

#include <curl/curl.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>


/*
 * Client definition
 */
struct http_client
{
  CURL *hc_curl;
  int   hc_fd;
  url_t hc_url;
  int   hc_begin;

  /* Callbacks */
  http_client_conn_cb *hc_conn;
  http_client_data_cb *hc_data;
  http_client_fail_cb *hc_fail;
  void                *hc_opaque;

  TAILQ_ENTRY(http_client) hc_link;
};

/*
 * Global state
 */
static tvhpoll_t               *http_poll;
static TAILQ_HEAD(,http_client) http_clients;
static pthread_mutex_t          http_lock;
static CURLM                   *http_curlm;

/*
 * Disable
 */
static void
http_remove ( http_client_t *hc )
{
  tvhpoll_event_t ev;
  ev.fd = hc->hc_fd;
  
  /* Remove */
  curl_multi_remove_handle(http_curlm, hc->hc_curl);
  tvhpoll_rem(http_poll, &ev, 1);
  TAILQ_REMOVE(&http_clients, hc, hc_link);

  /* Free CURL memory */
  curl_easy_cleanup(hc->hc_curl);
  hc->hc_curl = NULL;
}

/*
 * New socket
 */
static int
http_curl_socket ( CURL *c, int fd, int a, void *u, void *s )
{
  http_client_t *hc;
  tvhpoll_event_t ev = { 0 };
  ev.fd = fd;

  /* Find client */
  TAILQ_FOREACH(hc, &http_clients, hc_link)
    if (hc->hc_curl == c)
      break;

  /* Invalid */
  if (!hc)
    goto done;

  /* Remove */
  if (a == CURL_POLL_REMOVE) {
    //http_remove(hc);
    
  /* Set */
  } else if (a & CURL_POLL_INOUT) {
    if (a & CURL_POLL_IN)
      ev.events |= TVHPOLL_IN;
    if (a & CURL_POLL_OUT)
      ev.events |= TVHPOLL_OUT;
    ev.data.fd = fd;
    hc->hc_fd  = fd;
    tvhpoll_add(http_poll, &ev, 1);
  }

  /* Done */
done:
  return 0;
}

/*
 * Data
 */
static size_t
http_curl_data ( void *buf, size_t len, size_t n, void *p )
{
  http_client_t *hc = p;
  if (!hc->hc_begin && hc->hc_conn)
    hc->hc_conn(hc->hc_opaque);
  hc->hc_begin = 1;
  len = hc->hc_data(hc->hc_opaque, buf, len * n);
  return len;
}

/*
 * Data thread
 */
static void *
http_thread ( void *p )
{
  int n, e, run = 0;
  tvhpoll_event_t ev;
  http_client_t *hc;

  while (tvheadend_running) {
    n = tvhpoll_wait(http_poll, &ev, 1, -1);
    if (n < 0) {
      tvherror("http_client", "tvhpoll_wait() error");
      break;
    } else {
      pthread_mutex_lock(&http_lock);
      TAILQ_FOREACH(hc, &http_clients, hc_link)
        if (hc->hc_fd == ev.data.fd)
          break;
      if (hc && (ev.events & (TVHPOLL_IN | TVHPOLL_OUT))) {
        e = 0;
        if (ev.events & TVHPOLL_IN)  e |= CURL_POLL_IN;
        if (ev.events & TVHPOLL_OUT) e |= CURL_POLL_OUT;
        curl_multi_socket_action(http_curlm, ev.data.fd, 0, &run);
      }
      pthread_mutex_unlock(&http_lock);
    }
  }

  return NULL;
}

/*
 * Setup a connection (async)
 */
http_client_t *
http_connect 
  ( const url_t *url,
    http_client_conn_cb conn_cb,
    http_client_data_cb data_cb, 
    http_client_fail_cb fail_cb, 
    void *p )
{
  int run;

  /* Setup structure */
  http_client_t *hc = calloc(1, sizeof(http_client_t));
  hc->hc_curl       = curl_easy_init();
  hc->hc_url        = *url;
  hc->hc_conn       = conn_cb;
  hc->hc_data       = data_cb;
  hc->hc_fail       = fail_cb;
  hc->hc_opaque     = p;

  /* Store */
  pthread_mutex_lock(&http_lock);
  TAILQ_INSERT_TAIL(&http_clients, hc, hc_link);

  /* Setup connection */
  curl_easy_setopt(hc->hc_curl, CURLOPT_URL, url->raw);
  curl_easy_setopt(hc->hc_curl, CURLOPT_FOLLOWLOCATION, 1);
  curl_easy_setopt(hc->hc_curl, CURLOPT_WRITEFUNCTION, http_curl_data);
  curl_easy_setopt(hc->hc_curl, CURLOPT_WRITEDATA,     hc);
  curl_multi_add_handle(http_curlm, hc->hc_curl);
  curl_multi_socket_action(http_curlm, CURL_SOCKET_TIMEOUT, 0, &run);
  pthread_mutex_unlock(&http_lock);

  return hc;
}

/*
 * Cancel
 */
void
http_close ( http_client_t *hc )
{
  pthread_mutex_lock(&http_lock);
  http_remove(hc);
  free(hc);
  pthread_mutex_unlock(&http_lock);
}

/*
 * Initialise subsystem
 */
pthread_t http_client_tid;

void
http_client_init ( void )
{
  /* Setup list */
  pthread_mutex_init(&http_lock, NULL);
  TAILQ_INIT(&http_clients);

  /* Initialise curl */
  curl_global_init(CURL_GLOBAL_ALL);
  http_curlm = curl_multi_init();
  curl_multi_setopt(http_curlm, CURLMOPT_SOCKETFUNCTION, http_curl_socket);

  /* Setup poll */
  http_poll = tvhpoll_create(10);

  /* Setup thread */
  tvhthread_create(&http_client_tid, NULL, http_thread, NULL, 0);
}

void
http_client_done ( void )
{
  pthread_kill(http_client_tid, SIGTERM);
  pthread_join(http_client_tid, NULL);
  tvhpoll_destroy(http_poll);
  curl_multi_cleanup(http_curlm);
  curl_global_cleanup();
}

#else /* ENABLE_CURL */

void 
http_client_init ( void )
{
}

void
http_client_done ( void )
{
}

#endif /* ENABLE_CURL */
