/*
 *  tvheadend, UDP interface
 *  Copyright (C) 2013 Adam Sutton
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

#ifndef UDP_H_
#define UDP_H_

#include <netinet/in.h>
#include "tcp.h"

#define UDP_FATAL_ERROR ((void *)-1)

#define IP_AS_V4(storage, f) ((struct sockaddr_in *)&(storage))->sin_##f
#define IP_AS_V6(storage, f) ((struct sockaddr_in6 *)&(storage))->sin6_##f
#define IP_IN_ADDR(storage) \
  ((storage).ss_family == AF_INET6 ? \
      &((struct sockaddr_in6 *)&(storage))->sin6_addr : \
      (void *)&((struct sockaddr_in  *)&(storage))->sin_addr)
#define IP_PORT(storage) \
  ((storage).ss_family == AF_INET6 ? \
      ((struct sockaddr_in6 *)&(storage))->sin6_port : \
      ((struct sockaddr_in  *)&(storage))->sin_port)

typedef struct udp_connection {
  char *host;
  int port;
  int multicast;
  char *ifname;
  struct sockaddr_storage ip;
  int fd;
  char *subsystem;
  char *name;
  int rxtxsize;
} udp_connection_t;

udp_connection_t *
udp_bind ( const char *subsystem, const char *name,
           const char *bindaddr, int port,
           const char *ifname, int rxsize );
udp_connection_t *
udp_connect ( const char *subsystem, const char *name,
              const char *host, int port,
              const char *ifname, int txsize );
void
udp_close ( udp_connection_t *uc );
int
udp_write( udp_connection_t *uc, const void *buf, size_t len,
           struct sockaddr_storage *storage );
int
udp_write_queue( udp_connection_t *uc, htsbuf_queue_t *q,
                 struct sockaddr_storage *storage );


#endif /* UDP_H_ */
