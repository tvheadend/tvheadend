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
#include "htsmsg.h"

#define IP_AS_V4(storage, f) ((struct sockaddr_in *)&(storage))->sin_##f
#define IP_AS_V6(storage, f) ((struct sockaddr_in6 *)&(storage))->sin6_##f
#define IP_IN_ADDR(storage) \
  ((storage).ss_family == AF_INET6 ? \
      &((struct sockaddr_in6 *)&(storage))->sin6_addr : \
      (void *)&((struct sockaddr_in  *)&(storage))->sin_addr)
#define IP_IN_ADDRLEN(storage) \
  ((storage).ss_family == AF_INET6 ? \
      sizeof(struct sockaddr_in6) : \
      sizeof(struct sockaddr_in))
#define IP_PORT(storage) \
  ((storage).ss_family == AF_INET6 ? \
      ((struct sockaddr_in6 *)&(storage))->sin6_port : \
      ((struct sockaddr_in  *)&(storage))->sin_port)

typedef struct tcp_server_ops
{
  void (*start)  (int fd, void **opaque,
                     struct sockaddr_storage *peer,
                     struct sockaddr_storage *self);
  void (*stop)   (void *opaque);
  void (*status) (void *opaque, htsmsg_t *m);
  void (*cancel) (void *opaque);
} tcp_server_ops_t;

extern int tcp_preferred_address_family;

void tcp_server_init(int opt_ipv6);
void tcp_server_done(void);

int tcp_connect(const char *hostname, int port, const char *bindaddr,
                char *errbuf, size_t errbufsize, int timeout);

typedef void (tcp_server_callback_t)(int fd, void *opaque,
				     struct sockaddr_storage *peer,
				     struct sockaddr_storage *self);

void *tcp_server_create(const char *bindaddr, int port, 
  tcp_server_ops_t *ops, void *opaque);

void tcp_server_delete(void *server);

int tcp_read(int fd, void *buf, size_t len);

char *tcp_read_line(int fd, htsbuf_queue_t *spill);

int tcp_read_data(int fd, char *buf, const size_t bufsize,
		  htsbuf_queue_t *spill);

int tcp_write_queue(int fd, htsbuf_queue_t *q);

int tcp_read_timeout(int fd, void *buf, size_t len, int timeout);

char *tcp_get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen);

htsmsg_t *tcp_server_connections ( void );

#endif /* TCP_H_ */
