/*
 *  tvheadend, TCP common functions
 *  Copyright (C) 2007 Andreas Öman
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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "dispatch.h"
#include "tcp.h"
#include "resolver.h"

static void tcp_client_reconnect_timeout(void *aux, int64_t now);

/**
 * Read max 'n' bytes of data from line parser. Used to consume binary data
 * for mixed line / binary protocols (HTTP)
 *
 * Returns bytes read
 */
int
tcp_line_drain(tcp_session_t *ses, void *buf, int n)
{
  if(n > ses->tcp_input_buf_ptr)
    n = ses->tcp_input_buf_ptr;

  memcpy(buf, ses->tcp_input_buf, n);

  memmove(ses->tcp_input_buf, ses->tcp_input_buf + n, 
	  sizeof(ses->tcp_input_buf) - n);
  ses->tcp_input_buf_ptr -= n;
  return n;
}

/**
 * Line parser for TCP based connections. Note that callback cannot
 * destroy the session on its own. It must return an error to perform
 * disconnect.
 */
void
tcp_line_read(tcp_session_t *ses, tcp_line_input_t *callback)
{
  int space = sizeof(ses->tcp_input_buf) - ses->tcp_input_buf_ptr - 1;
  int r, cr = 0, i, err;
  char buf[TCP_MAX_LINE_LEN];

  if(space < 1) {
    tcp_disconnect(ses, EBADMSG);
    return;
  }

  r = read(ses->tcp_fd, ses->tcp_input_buf + ses->tcp_input_buf_ptr, space);
  if(r < 1) {
    tcp_disconnect(ses, r == 0 ? ECONNRESET : errno);
    return;
  }

  ses->tcp_input_buf_ptr += r;
  ses->tcp_input_buf[ses->tcp_input_buf_ptr] = 0;

  while(1) {
    cr = 0;

    for(i = 0; i < ses->tcp_input_buf_ptr; i++)
      if(ses->tcp_input_buf[i] == 0xa)
	break;

    if(i == ses->tcp_input_buf_ptr)
      break;

    memcpy(buf, ses->tcp_input_buf, i);
    buf[i] = 0;
    i++;
    memmove(ses->tcp_input_buf, ses->tcp_input_buf + i, 
	    sizeof(ses->tcp_input_buf) - i);
    ses->tcp_input_buf_ptr -= i;

    i = strlen(buf);
    while(i > 0 && buf[i-1] < 32)
      buf[--i] = 0;

    if((err = callback(ses, buf)) != 0) {
      tcp_disconnect(ses, err < 0 ? 0 : err);
      break;
    }
  }
}


/**
 * Transmit data from any of the queues
 * Select hi-pri queue first if possible, but always stick on the same
 * queue until a whole message has been sent
 */
static void
tcp_transmit(tcp_session_t *ses)
{
  htsbuf_queue_t *hq = ses->tcp_q_current;
  htsbuf_data_t *hd;
  int r;

 again:
  if(hq == NULL) {
    if(ses->tcp_q[1].hq_size)
      hq = &ses->tcp_q[1];
    else if(ses->tcp_q[0].hq_size)
      hq = &ses->tcp_q[0];
  }

  while(hq != NULL) {
    hd = TAILQ_FIRST(&hq->hq_q);
    
    r = write(ses->tcp_fd, hd->hd_data + hd->hd_data_off,
	      hd->hd_data_len - hd->hd_data_off);

    if(r == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
      r = 0;

    if(r == 0)
      break;

    if(r == -1) {
      tcp_disconnect(ses, errno);
      return;
    }
    hq->hq_size     -= r;
    hd->hd_data_off += r;
    
    if(hd->hd_data_off == hd->hd_data_len) {
      htsbuf_data_free(hq, hd);
      hq = NULL;
      goto again;
    }
  }

  if(hq == NULL) {
    if(ses->tcp_blocked) {
      dispatch_clr(ses->tcp_dispatch_handle, DISPATCH_WRITE);
      ses->tcp_blocked = 0;
    }
  } else {
    if(!ses->tcp_blocked) {
      dispatch_set(ses->tcp_dispatch_handle, DISPATCH_WRITE);
      ses->tcp_blocked = 1;
    }
  }
  ses->tcp_q_current = hq;
}

/**
 * Enqueue a message and start transmission if nothing currently is
 * being sent.
 */
int
tcp_send_msg(tcp_session_t *ses, int hiprio, void *data, size_t len)
{
  htsbuf_queue_t *hq = &ses->tcp_q[!!hiprio];
  htsbuf_data_t *hd;

  if(len > hq->hq_maxsize - hq->hq_size) {
    free(data);
    return -1;
  }

  hd = malloc(sizeof(htsbuf_data_t));
  hd->hd_data_off = 0;
  hd->hd_data_len = len;
  hd->hd_data = data;
  TAILQ_INSERT_TAIL(&hq->hq_q, hd, hd_link);
  hq->hq_size += len;

  if(!ses->tcp_blocked)
    tcp_transmit(ses);

  return 0;
}

/**
 *
 */
void
tcp_printf(tcp_session_t *ses, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  htsbuf_vqprintf(&ses->tcp_q[0], fmt, ap);
  va_end(ap);

  if(!ses->tcp_blocked)
    tcp_transmit(ses);
}

/**
 * Move a tcp queue onto a session
 *
 * Coalesce smaller chunks into bigger ones for more efficient I/O
 */
void
tcp_output_queue(tcp_session_t *ses, int hiprio, htsbuf_queue_t *src)
{
  htsbuf_data_t *hd;
  htsbuf_queue_t *dst = &ses->tcp_q[!!hiprio];

  while((hd = TAILQ_FIRST(&src->hq_q)) != NULL) {
    TAILQ_REMOVE(&src->hq_q, hd, hd_link);
    TAILQ_INSERT_TAIL(&dst->hq_q, hd, hd_link);

    dst->hq_size += hd->hd_data_len;
  }
  src->hq_size = 0;

  if(!ses->tcp_blocked)
    tcp_transmit(ses);
}

/**
 * Disconnect handler
 */
void
tcp_disconnect(tcp_session_t *ses, int err)
{
  htsbuf_queue_flush(&ses->tcp_q[0]);
  htsbuf_queue_flush(&ses->tcp_q[1]);

  ses->tcp_callback(TCP_DISCONNECT, ses);

  syslog(LOG_INFO, "%s: %s: disconnected -- %s",
	 ses->tcp_name, ses->tcp_peer_txt, strerror(err));

  close(dispatch_delfd(ses->tcp_dispatch_handle));
  ses->tcp_dispatch_handle = NULL;

  if(ses->tcp_server != NULL) {
    free(ses->tcp_name);
    free(ses);
  } else {
    /* Try to reconnect in 2 seconds */
    dtimer_arm(&ses->tcp_timer, tcp_client_reconnect_timeout, ses, 2);
  }
}


/**
 * Dispatcher callback
 */
static void
tcp_socket_callback(int events, void *opaque, int fd)
{
  tcp_session_t *ses = opaque;

  if(events & DISPATCH_ERR) {
    tcp_disconnect(ses, ECONNRESET);
    return;
  }

  if(events & DISPATCH_READ)
    ses->tcp_callback(TCP_INPUT, ses);

  if(events & DISPATCH_WRITE)
    tcp_transmit(ses);
}



/**
 * Setup a TCP connection, common code between server and client
 */

static void
tcp_start_session(tcp_session_t *ses)
{
  int val;
  struct sockaddr_in *si = (struct sockaddr_in *)&ses->tcp_peer_addr;

  fcntl(ses->tcp_fd, F_SETFL, fcntl(ses->tcp_fd, F_GETFL) | O_NONBLOCK);

  val = 1;
  setsockopt(ses->tcp_fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val));
  
  val = 30;
  setsockopt(ses->tcp_fd, SOL_TCP, TCP_KEEPIDLE, &val, sizeof(val));

  val = 15;
  setsockopt(ses->tcp_fd, SOL_TCP, TCP_KEEPINTVL, &val, sizeof(val));

  val = 5;
  setsockopt(ses->tcp_fd, SOL_TCP, TCP_KEEPCNT, &val, sizeof(val));

  val = 1;
  setsockopt(ses->tcp_fd, SOL_TCP, TCP_NODELAY, &val, sizeof(val));

  htsbuf_queue_init(&ses->tcp_q[0],  20 * 1000 * 1000);
  htsbuf_queue_init(&ses->tcp_q[1],  20 * 1000 * 1000);

  snprintf(ses->tcp_peer_txt, sizeof(ses->tcp_peer_txt), "%s:%d",
	   inet_ntoa(si->sin_addr), ntohs(si->sin_port));

  syslog(LOG_INFO, "%s: %s%sConnected to %s", ses->tcp_name,
	 ses->tcp_hostname ?: "", ses->tcp_hostname ? ": " : "",
	 ses->tcp_peer_txt);


  ses->tcp_dispatch_handle = dispatch_addfd(ses->tcp_fd, tcp_socket_callback,
					    ses, DISPATCH_READ);
  ses->tcp_callback(TCP_CONNECT, ses);
}



/**
 *
 */
static void
tcp_client_connected(tcp_session_t *c)
{
  dtimer_disarm(&c->tcp_timer);
  tcp_start_session(c);
}


/**
 *
 */
static void
tcp_client_connect_fail(tcp_session_t *c, int error)
{
  struct sockaddr_in *si = (struct sockaddr_in *)&c->tcp_peer_addr;

  
  syslog(LOG_ERR, "%s: Unable to connect to \"%s\" (%s) : %d -- %s",
	 c->tcp_name, c->tcp_hostname, inet_ntoa(si->sin_addr),
	 ntohs(si->sin_port), strerror(error));

  /* Try to reconnect in 10 seconds */

  dtimer_arm(&c->tcp_timer, tcp_client_reconnect_timeout, c, 10);
}


/**
 *
 */
static void
tcp_client_connect_callback(int events, void *opaque, int fd)
{
  int err;
  socklen_t errlen = sizeof(int);
  tcp_session_t *c = opaque;

  dispatch_delfd(c->tcp_dispatch_handle);

  getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&err, &errlen);

  if(err == 0) {
    tcp_client_connected(c);
    return;
  }
  
  close(c->tcp_fd);
  tcp_client_connect_fail(c, errno);
}

/**
 * We dont want to wait for connect() to time out, so we have our
 * own timeout
 */
static void
tcp_client_connect_timeout(void *aux, int64_t now)
{
  tcp_session_t *c = aux;
  
  close(c->tcp_fd);
  tcp_client_connect_fail(c, ETIMEDOUT);
}

/**
 *
 */
static void
tcp_session_peer_resolved(void *aux, struct sockaddr *so, const char *error)
{
  tcp_session_t *c = aux;
  struct sockaddr_in *si;
  
  c->tcp_resolver = NULL;

  if(error != NULL) {
    syslog(LOG_ERR, "%s: Unable to resolve \"%s\" -- %s",
	   c->tcp_name, c->tcp_hostname, error);
    /* Try again in 30 seconds */
    dtimer_arm(&c->tcp_timer, tcp_client_reconnect_timeout, c, 30);
    return;
  }


  c->tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
  fcntl(c->tcp_fd, F_SETFL, fcntl(c->tcp_fd, F_GETFL) | O_NONBLOCK);


  si = (struct sockaddr_in *)&c->tcp_peer_addr;
  memcpy(si, so, sizeof(struct sockaddr_in));
  si->sin_port = htons(c->tcp_port);

  if(connect(c->tcp_fd, (struct sockaddr *)&c->tcp_peer_addr,
	     sizeof(struct sockaddr_in)) == 0) {
    tcp_client_connected(c);
    return;
  }
  
  if(errno == EINPROGRESS) {

    dtimer_arm(&c->tcp_timer, tcp_client_connect_timeout, c, 10);

    c->tcp_dispatch_handle = 
      dispatch_addfd(c->tcp_fd, tcp_client_connect_callback, c,
		     DISPATCH_WRITE);
    return;
  }

  close(c->tcp_fd);
  tcp_client_connect_fail(c, errno);
}

/**
 * Start by resolving hostname
 */
static void
tcp_session_try_connect(tcp_session_t *c)
{
  c->tcp_resolver = 
    async_resolve(c->tcp_hostname, tcp_session_peer_resolved, c);
}


/**
 * We come here after a failed connection attempt or if we disconnected
 */
static void
tcp_client_reconnect_timeout(void *aux, int64_t now)
{
  tcp_session_t *c = aux;
  tcp_session_try_connect(c);
}


/**
 * Create a TCP based client
 */
void *
tcp_create_client(const char *hostname, int port, size_t session_size,
		  const char *name, tcp_callback_t *cb, int enabled)
{
  tcp_session_t *c = calloc(1, session_size);

  c->tcp_callback = cb;
  c->tcp_name = strdup(name);
  c->tcp_port = port;
  c->tcp_hostname = strdup(hostname);
  c->tcp_enabled = enabled;

  if(c->tcp_enabled)
    tcp_session_try_connect(c);
  return c;
}





/**
 * TCP server connect callback
 */
static void
tcp_server_callback(int events, void *opaque, int fd)
{
  struct sockaddr_in from;
  socklen_t socklen = sizeof(struct sockaddr_in);
  int newfd;
  tcp_session_t *ses;
  tcp_server_t *srv = opaque;

  if(!(events & DISPATCH_READ))
    return;

  if((newfd = accept(fd, (struct sockaddr *)&from, &socklen)) == -1)
    return;  /* XXX: Something more clever must be done here */
  
  ses = calloc(1, srv->tcp_session_size);
  ses->tcp_fd = newfd;
  ses->tcp_name = strdup(srv->tcp_server_name);
  ses->tcp_callback = srv->tcp_callback;
  ses->tcp_server = srv;
  memcpy(&ses->tcp_peer_addr, &from, socklen);

  socklen = sizeof(struct sockaddr_storage);
  if(getsockname(newfd, (struct sockaddr *)&ses->tcp_self_addr, &socklen))
    memset(&ses->tcp_self_addr, 0, sizeof(struct sockaddr_storage));

  tcp_start_session(ses);
}



/**
 * Create a TCP based server
 */
void
tcp_create_server(int port, size_t session_size, const char *name,
		  tcp_callback_t *cb)
{
  struct sockaddr_in sin;
  int one = 1;

  tcp_server_t *s = malloc(sizeof(tcp_server_t));

  s->tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
  s->tcp_session_size = session_size;
  s->tcp_callback = cb;

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);

  setsockopt(s->tcp_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

  fcntl(s->tcp_fd, F_SETFL, fcntl(s->tcp_fd, F_GETFL) | O_NONBLOCK);

  syslog(LOG_INFO, "%s: Listening for TCP connections on %s:%d",
	 name, inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

  if(bind(s->tcp_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    syslog(LOG_ERR, 
	   "%s: Unable to bind socket for incomming TCP connections, "
	   "%s:%d -- %s",
	   name,
	   inet_ntoa(sin.sin_addr), ntohs(sin.sin_port),
	   strerror(errno));
    return;
  }

  listen(s->tcp_fd, 1);
  dispatch_addfd(s->tcp_fd, tcp_server_callback, s, DISPATCH_READ);

  s->tcp_server_name = strdup(name);
}

/**
 *
 */
void
tcp_destroy_client(tcp_session_t *ses)
{
  if(ses->tcp_resolver != NULL)
    async_resolve_cancel(ses->tcp_resolver);

  if(ses->tcp_dispatch_handle != NULL)
    tcp_disconnect(ses, 0);

  dtimer_disarm(&ses->tcp_timer);
  free(ses->tcp_name);
  free(ses->tcp_hostname);
  free(ses);
}

/**
 *
 */
void
tcp_enable_disable(tcp_session_t *ses, int enabled)
{
  if(ses->tcp_enabled == enabled)
    return;

  ses->tcp_enabled = enabled;

  if(enabled) {
    tcp_session_try_connect(ses);
  } else {
    if(ses->tcp_resolver != NULL) {
      async_resolve_cancel(ses->tcp_resolver);
      ses->tcp_resolver = NULL;
    }

    if(ses->tcp_dispatch_handle != NULL)
      tcp_disconnect(ses, 0);

    dtimer_disarm(&ses->tcp_timer);
  }
}
