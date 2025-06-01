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

#if defined(PLATFORM_FREEBSD)
#include <sys/socket.h>
#endif

#define IP_AS_V4(storage, f) ((struct sockaddr_in *)(storage))->sin_##f
#define IP_AS_V6(storage, f) ((struct sockaddr_in6 *)(storage))->sin6_##f
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
#define IP_PORT_SET(storage, port) \
  do { \
    if ((storage).ss_family == AF_INET6) \
      ((struct sockaddr_in6 *)&(storage))->sin6_port = (port); else \
      ((struct sockaddr_in  *)&(storage))->sin_port  = (port); \
  } while (0)

typedef struct tcp_server_ops
{
  void (*start)  (int fd, void **opaque,
                     struct sockaddr_storage *peer,
                     struct sockaddr_storage *self);
  void (*stop)   (void *opaque);
  void (*cancel) (void *opaque);
} tcp_server_ops_t;

extern int tcp_preferred_address_family;

void tcp_server_preinit(int opt_ipv6);
void tcp_server_init(void);
void tcp_server_done(void);

static inline int ip_check_equal_v4
  (const struct sockaddr_storage *a, const struct sockaddr_storage *b)
    { return IP_AS_V4(a, addr).s_addr == IP_AS_V4(b, addr).s_addr; }

static inline int ip_check_equal_v6
  (const struct sockaddr_storage *a, const struct sockaddr_storage *b)
    { return memcmp(IP_AS_V6(a, addr).s6_addr, IP_AS_V6(b, addr).s6_addr, sizeof(IP_AS_V6(a, addr).s6_addr)) == 0; }

static inline int ip_check_equal
  (const struct sockaddr_storage *a, const struct sockaddr_storage *b)
    { return a->ss_family == b->ss_family &&
             a->ss_family == AF_INET ? ip_check_equal_v4(a, b) :
             (a->ss_family == AF_INET6 ? ip_check_equal_v6(a, b) : 0); }

static inline int ip_check_in_network_v4(const struct sockaddr_storage *network,
                                         const struct sockaddr_storage *mask,
                                         const struct sockaddr_storage *address)
  { return (IP_AS_V4(address, addr).s_addr & IP_AS_V4(mask, addr).s_addr) ==
           (IP_AS_V4(network, addr).s_addr & IP_AS_V4(mask, addr).s_addr); }

static inline int ip_check_in_network_v6(const struct sockaddr_storage *network,
                                         const struct sockaddr_storage *mask,
                                         const struct sockaddr_storage *address)
  {
    short i;
    for (i = 0; i < sizeof(IP_AS_V6(address, addr).s6_addr); ++i)
      if (((IP_AS_V6(address, addr).s6_addr)[i] & (IP_AS_V6(mask, addr).s6_addr)[i]) != ((IP_AS_V6(network, addr).s6_addr)[i] & (IP_AS_V6(mask, addr).s6_addr)[i]))
        return 0;
    return 1;
  }

static inline int ip_check_in_network(const struct sockaddr_storage *network,
                                      const struct sockaddr_storage *mask,
                                       const struct sockaddr_storage *address)
  { return address->ss_family == AF_INET ? ip_check_in_network_v4(network, mask, address) :
           (address->ss_family == AF_INET6 ? ip_check_in_network_v6(network, mask, address) : 0); }

static inline int ip_check_is_any_v4(const struct sockaddr_storage *address)
  { return IP_AS_V4(address, addr).s_addr == INADDR_ANY; }

static inline int ip_check_is_any_v6(const struct sockaddr_storage *address)
  { return memcmp(IP_AS_V6(address, addr).s6_addr, in6addr_any.s6_addr, sizeof(in6addr_any.s6_addr)) == 0; }

static inline int ip_check_is_any(const struct sockaddr_storage *address)
  { return address->ss_family == AF_INET ? ip_check_is_any_v4(address) :
           (address->ss_family == AF_INET6 ? ip_check_is_any_v6(address) : 0); }

int ip_check_is_local_address(const struct sockaddr_storage *peer,
                              const struct sockaddr_storage *local,
                              struct sockaddr_storage *used_local);

int socket_set_dscp(int sockfd, uint32_t dscp, char *errbuf, size_t errbufsize);

int tcp_connect(const char *hostname, int port, const char *bindaddr,
                char *errbuf, size_t errbufsize, int timeout);

typedef void (tcp_server_callback_t)(int fd, void *opaque,
				     struct sockaddr_storage *peer,
				     struct sockaddr_storage *self);

void *tcp_server_create(int subsystem, const char *name,
                        const char *bindaddr, int port,
                        tcp_server_ops_t *ops, void *opaque);

void tcp_server_register(void *server);

void tcp_server_delete(void *server);

int tcp_default_ip_addr(struct sockaddr_storage *deflt, int family);

int tcp_server_bound(void *server, struct sockaddr_storage *bound, int family);

int tcp_server_onall(void *server);

int tcp_read(int fd, void *buf, size_t len);

char *tcp_read_line(int fd, htsbuf_queue_t *spill);

int tcp_read_data(int fd, char *buf, const size_t bufsize,
		  htsbuf_queue_t *spill);

int tcp_write_queue(int fd, htsbuf_queue_t *q);

int tcp_read_timeout(int fd, void *buf, size_t len, int timeout);

char *tcp_get_str_from_ip(const struct sockaddr_storage *sa, char *dst, size_t maxlen);

struct sockaddr_storage *tcp_get_ip_from_str(const char *str, struct sockaddr_storage *sa);

int tcp_socket_dead(int fd);

struct access;

uint32_t tcp_connection_count(struct access *aa);
void *tcp_connection_launch(int fd, int streaming,
                            void (*status) (void *opaque, htsmsg_t *m),
                            struct access *aa);
void tcp_connection_land(void *tcp_id);
void tcp_connection_cancel(uint32_t id);
void tcp_connection_cancel_all(void);

htsmsg_t *tcp_server_connections ( void );
int tcp_server_connections_count ( void );

#endif /* TCP_H_ */
