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


/*
 *  vprintf data on a TCP queue
 */
void
tcp_qvprintf(tcp_queue_t *tq, const char *fmt, va_list ap)
{
  char buf[5000];
  void *out;
  tcp_data_t *td;

  td = malloc(sizeof(tcp_data_t));
  td->td_offset = 0;

  td->td_datalen = vsnprintf(buf, sizeof(buf), fmt, ap);
  out = malloc(td->td_datalen);
  memcpy(out, buf, td->td_datalen);
  td->td_data = out;
  TAILQ_INSERT_TAIL(&tq->tq_messages, td, td_link);
  tq->tq_depth += td->td_datalen;
}


/*
 *  printf data on a TCP queue
 */
void
tcp_qprintf(tcp_queue_t *tq, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  tcp_qvprintf(tq, fmt, ap);
  va_end(ap);
}

/*
 *  Put data on a TCP queue
 */
void
tcp_qput(tcp_queue_t *tq, const uint8_t *buf, size_t len)
{
  tcp_data_t *td;
  void *out;

  td = malloc(sizeof(tcp_data_t));
  td->td_offset = 0;
  td->td_datalen = len;

  out = malloc(td->td_datalen);
  memcpy(out, buf, td->td_datalen);
  td->td_data = out;
  TAILQ_INSERT_TAIL(&tq->tq_messages, td, td_link);
  tq->tq_depth += td->td_datalen;
}

/*
 *  printfs data on a TCP connection
 */
void
tcp_printf(tcp_session_t *ses, const char *fmt, ...)
{
  va_list ap;
  char buf[5000];
  void *out;
  int l;

  va_start(ap, fmt);
  l = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  out = malloc(l);
  memcpy(out, buf, l);

  tcp_send_msg(ses, &ses->tcp_q_hi, out, l);
}

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
 * Create an output queue
 */
void
tcp_init_queue(tcp_queue_t *tq, int maxdepth)
{
  TAILQ_INIT(&tq->tq_messages);
  tq->tq_depth = 0;
  tq->tq_maxdepth = maxdepth;
}

/**
 * Flusing all pending data from a queue
 */
void
tcp_flush_queue(tcp_queue_t *tq)
{
  tcp_data_t *td;

  while((td = TAILQ_FIRST(&tq->tq_messages)) != NULL) {
    TAILQ_REMOVE(&tq->tq_messages, td, td_link);
    free((void *)td->td_data);
    free(td);
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
  tcp_queue_t *q = ses->tcp_q_current;
  tcp_data_t *hd;
  int r;

 again:
  if(q == NULL) {
    if(ses->tcp_q_hi.tq_depth)
      q = &ses->tcp_q_hi;
    if(ses->tcp_q_low.tq_depth)
      q = &ses->tcp_q_low;
  }

  while(q != NULL) {
    hd = TAILQ_FIRST(&q->tq_messages);
    
    r = write(ses->tcp_fd, hd->td_data + hd->td_offset,
	      hd->td_datalen - hd->td_offset);

    if(r == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
      r = 0;

    if(r == 0)
      break;

    if(r == -1) {
      tcp_disconnect(ses, errno);
      return;
    }
    q->tq_depth -= r;
    hd->td_offset += r;

    if(hd->td_offset == hd->td_datalen) {
      TAILQ_REMOVE(&q->tq_messages, hd, td_link);
      free((void *)hd->td_data);
      free(hd);
      q = NULL;
      goto again;
    }
  }

  if(q == NULL) {
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
  ses->tcp_q_current = q;
}

/**
 * Enqueue a message and start transmission if nothing currently is
 * being sent.
 */
int
tcp_send_msg(tcp_session_t *ses, tcp_queue_t *tq, const void *data,
	     size_t len)
{
  tcp_data_t *td;

  if(tq == NULL)
    tq = &ses->tcp_q_low;

  if(len > tq->tq_maxdepth - tq->tq_depth) {
    free((void *)data);
    return -1;
  }

  td = malloc(sizeof(tcp_data_t));
  td->td_offset = 0;
  td->td_datalen = len;
  td->td_data = data;
  TAILQ_INSERT_TAIL(&tq->tq_messages, td, td_link);
  tq->tq_depth += td->td_datalen;

  if(!ses->tcp_blocked)
    tcp_transmit(ses);

  return 0;
}


/**
 * Move a tcp queue onto a session
 *
 * Coalesce smaller chunks into bigger ones for more efficient I/O
 */
void
tcp_output_queue(tcp_session_t *ses, tcp_queue_t *dst, tcp_queue_t *src)
{
  tcp_data_t *sd;
  tcp_data_t *dd;
  int l, s;

  if(dst == NULL)
    dst = &ses->tcp_q_low;

  while((sd = TAILQ_FIRST(&src->tq_messages)) != NULL) {

    l = 4096;
    if(sd->td_datalen > l)
      l = sd->td_datalen;
    
    dd = malloc(sizeof(tcp_data_t));
    dd->td_offset = 0;
    dd->td_data = malloc(l);

    s = 0; /* accumulated size */
    while((sd = TAILQ_FIRST(&src->tq_messages)) != NULL) {

      if(sd->td_datalen + s > l)
	break;

      memcpy((char *)dd->td_data + s, sd->td_data, sd->td_datalen);
      s += sd->td_datalen;
      TAILQ_REMOVE(&src->tq_messages, sd, td_link);
      free((void *)sd->td_data);
      free(sd);
    }

    dd->td_datalen = s;
    TAILQ_INSERT_TAIL(&dst->tq_messages, dd, td_link);
    dst->tq_depth += s;
  }

  if(ses != NULL && !ses->tcp_blocked)
    tcp_transmit(ses);

  src->tq_depth = 0;
}

/**
 * Disconnect handler
 */
void
tcp_disconnect(tcp_session_t *ses, int err)
{
  tcp_flush_queue(&ses->tcp_q_low);
  tcp_flush_queue(&ses->tcp_q_hi);

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

  tcp_init_queue(&ses->tcp_q_hi,  20 * 1000 * 1000);
  tcp_init_queue(&ses->tcp_q_low, 20 * 1000 * 1000);

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
