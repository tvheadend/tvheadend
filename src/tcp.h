/*
 *  tvheadend, TCP common functions
 *  Copyright (C) 2007 Andreas �man
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

extern int tcp_preferred_address_family;

void tcp_server_init(int opt_ipv6);

int tcp_connect(const char *hostname, int port, char *errbuf,
		size_t errbufsize, int timeout);

typedef void (tcp_server_callback_t)(int fd, void *opaque,
				     struct sockaddr_storage *peer,
				     struct sockaddr_storage *self);

void *tcp_server_create(const char *bindaddr, int port, tcp_server_callback_t *start, void *opaque);

int tcp_read(int fd, void *buf, size_t len);

int tcp_read_line(int fd, char *buf, const size_t bufsize, 
		  htsbuf_queue_t *spill);

int tcp_read_data(int fd, char *buf, const size_t bufsize,
		  htsbuf_queue_t *spill);

int tcp_write_queue(int fd, htsbuf_queue_t *q);

int tcp_read_timeout(int fd, void *buf, size_t len, int timeout);

char *tcp_get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen);

#endif /* TCP_H_ */
