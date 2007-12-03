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

TAILQ_HEAD(tcp_data_queue, tcp_data);

typedef struct tcp_data {
  TAILQ_ENTRY(tcp_data) td_link;
  const void *td_data;
  unsigned int td_datalen;
  int td_offset;
} tcp_data_t;

typedef struct tcp_queue {
  struct tcp_data_queue tq_messages;
  int tq_depth;
  int tq_maxdepth;
} tcp_queue_t;

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
  char tcp_peer_txt[100];
  tcp_callback_t *tcp_callback;
  tcp_server_t *tcp_server;  /* if this is NULL, then we are spawned
				as a client */

  /* These are only used when we spawn as a client */

  dtimer_t tcp_timer;
  char *tcp_name;
  int tcp_port;
  char *tcp_hostname;
  
  /* Output queueing */

  int tcp_blocked;
  tcp_queue_t tcp_q_hi;
  tcp_queue_t tcp_q_low;

  tcp_queue_t *tcp_q_current;

  /* Input line parser */

  int tcp_input_buf_ptr;
  char tcp_input_buf[TCP_MAX_LINE_LEN];

} tcp_session_t;

void tcp_init_queue(tcp_queue_t *tq, int maxdepth);

void tcp_flush_queue(tcp_queue_t *tq);

void tcp_disconnect(tcp_session_t *ses, int err);

void tcp_create_server(int port, size_t session_size, const char *name,
		       tcp_callback_t *cb);

void tcp_line_read(tcp_session_t *ses, tcp_line_input_t *callback);

#define tcp_logname(ses) ((ses)->tcp_peer_txt)

int tcp_send_msg(tcp_session_t *ses, tcp_queue_t *tq, const void *data,
		 size_t len);

void tcp_printf(tcp_session_t *ses, const char *fmt, ...);

void tcp_qprintf(tcp_queue_t *tq, const char *fmt, ...);

void tcp_output_queue(tcp_session_t *ses, tcp_queue_t *dst, tcp_queue_t *src);

void *tcp_create_client(const char *hostname, int port, size_t session_size,
			const char *name, tcp_callback_t *cb);

#endif /* TCP_H_ */
