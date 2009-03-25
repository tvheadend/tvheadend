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

#ifndef TCP_H_
#define TCP_H_

#include "htsbuf.h"

#if 0


typedef enum {
  TCP_CONNECT,
  TCP_DISCONNECT,
  TCP_INPUT,
} tcpevent_t;

typedef void (tcp_callback_t)(tcpevent_t event, void *tcpsession);
typedef int (tcp_line_input_t)(void *tcpsession, char *line);

typedef struct tcpserver {
  tcp_callback_t *tcp_callback;
  int tcp_fd;
  size_t tcp_session_size;
  const char *tcp_server_name;
} tcp_server_t;

#define TCP_MAX_LINE_LEN 4000

typedef struct tcp_session {
  void *tcp_dispatch_handle;
  int tcp_fd;
  struct sockaddr_storage tcp_peer_addr;
  struct sockaddr_storage tcp_self_addr;
  char tcp_peer_txt[100];
  tcp_callback_t *tcp_callback;
  tcp_server_t *tcp_server;  /* if this is NULL, then we are spawned
				as a client */

  /* These are only used when we spawn as a client */

  int tcp_enabled;
  //  dtimer_t tcp_timer;
  char *tcp_name;
  int tcp_port;
  char *tcp_hostname;
  void *tcp_resolver;

  /* Output queueing */

  int tcp_blocked;
  htsbuf_queue_t tcp_q[2];

  htsbuf_queue_t *tcp_q_current;

  /* Input line parser */

  int tcp_input_buf_ptr;
  char tcp_input_buf[TCP_MAX_LINE_LEN];

} tcp_session_t;

void tcp_disconnect(tcp_session_t *ses, int err);

void tcp_create_server(int port, size_t session_size, const char *name,
		       tcp_callback_t *cb);

void tcp_line_read(tcp_session_t *ses, tcp_line_input_t *callback);

int tcp_line_drain(tcp_session_t *ses, void *buf, int n);

#define tcp_logname(ses) ((ses)->tcp_peer_txt)

void tcp_printf(tcp_session_t *ses, const char *fmt, ...);

void tcp_output_queue(tcp_session_t *ses, int hiprio, htsbuf_queue_t *src);

void *tcp_create_client(const char *hostname, int port, size_t session_size,
			const char *name, tcp_callback_t *cb, int enabled);

void tcp_destroy_client(tcp_session_t *ses);

void tcp_enable_disable(tcp_session_t *ses, int enabled);

void tcp_set_hostname(tcp_session_t *ses, const char *hostname);

int tcp_send_msg(tcp_session_t *ses, int hiprio, void *data, size_t len);

#endif

void tcp_server_init(void);

int tcp_connect(const char *hostname, int port, char *errbuf,
		size_t errbufsize, int timeout);

typedef void (tcp_server_callback_t)(int fd, void *opaque,
				     struct sockaddr_in *source);

void *tcp_server_create(int port, tcp_server_callback_t *start, void *opaque);

int tcp_read(int fd, void *buf, size_t len);

int tcp_read_line(int fd, char *buf, const size_t bufsize, 
		  htsbuf_queue_t *spill);

int tcp_read_data(int fd, char *buf, const size_t bufsize,
		  htsbuf_queue_t *spill);

int tcp_write_queue(int fd, htsbuf_queue_t *q);

int tcp_read_timeout(int fd, void *buf, size_t len, int timeout);

#endif /* TCP_H_ */
