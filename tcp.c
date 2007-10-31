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
      tcp_disconnect(ses, err);
      break;
    }
  }
}



/**
 * Create an output queue
 */
static void
tcp_init_queue(tcp_queue_t *tq, int maxdepth)
{
  TAILQ_INIT(&tq->tq_messages);
  tq->tq_depth = 0;
  tq->tq_maxdepth = maxdepth;
}

/**
 * Flusing all pending data from a queue
 */
static void
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
 * Disconnect handler
 */
void
tcp_disconnect(tcp_session_t *ses, int err)
{
  tcp_server_t *srv = ses->tcp_server;

  tcp_flush_queue(&ses->tcp_q_low);
  tcp_flush_queue(&ses->tcp_q_hi);

  srv->tcp_callback(TCP_DISCONNECT, ses);

  syslog(LOG_INFO, "%s: %s: disconnected -- %s",
	 srv->tcp_server_name, ses->tcp_peer_txt, strerror(err));

  close(dispatch_delfd(ses->tcp_dispatch_handle));
  free(ses);
}


/**
 * Dispatcher callback
 */
static void
tcp_socket_callback(int events, void *opaque, int fd)
{
  tcp_session_t *ses = opaque;
  tcp_server_t *srv = ses->tcp_server;

  if(events & DISPATCH_ERR) {
    tcp_disconnect(ses, ECONNRESET);
    return;
  }

  if(events & DISPATCH_READ)
    srv->tcp_callback(TCP_INPUT, ses);

  if(events & DISPATCH_WRITE)
    tcp_transmit(ses);
}



/**
 * TCP connect callback
 */
static void
tcp_connect_callback(int events, void *opaque, int fd)
{
  struct sockaddr_in from;
  socklen_t socklen = sizeof(struct sockaddr_in);
  int newfd;
  int val;
  tcp_session_t *ses;
  tcp_server_t *srv = opaque;

  if(!(events & DISPATCH_READ))
    return;

  if((newfd = accept(fd, (struct sockaddr *)&from, &socklen)) == -1)
    return;
  
  fcntl(newfd, F_SETFL, fcntl(newfd, F_GETFL) | O_NONBLOCK);

  val = 1;
  setsockopt(newfd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val));
  
  val = 30;
  setsockopt(newfd, SOL_TCP, TCP_KEEPIDLE, &val, sizeof(val));

  val = 15;
  setsockopt(newfd, SOL_TCP, TCP_KEEPINTVL, &val, sizeof(val));

  val = 5;
  setsockopt(newfd, SOL_TCP, TCP_KEEPCNT, &val, sizeof(val));

  val = 1;
  setsockopt(newfd, SOL_TCP, TCP_NODELAY, &val, sizeof(val));


  ses = calloc(1, srv->tcp_session_size);
  tcp_init_queue(&ses->tcp_q_hi,  20 * 1000 * 1000);
  tcp_init_queue(&ses->tcp_q_low, 20 * 1000 * 1000);

  ses->tcp_server = srv;

  memcpy(&ses->tcp_peer_addr, &from, socklen);
  snprintf(ses->tcp_peer_txt, sizeof(ses->tcp_peer_txt), "%s:%d",
	   inet_ntoa(from.sin_addr), ntohs(from.sin_port));

  syslog(LOG_INFO, "%s: %s: connected",
	 srv->tcp_server_name, ses->tcp_peer_txt);

  ses->tcp_fd = newfd;

  ses->tcp_dispatch_handle = dispatch_addfd(newfd, tcp_socket_callback,
					    ses, DISPATCH_READ);
  srv->tcp_callback(TCP_CONNECT, ses);
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
  dispatch_addfd(s->tcp_fd, tcp_connect_callback, s, DISPATCH_READ);

  s->tcp_server_name = strdup(name);
}
