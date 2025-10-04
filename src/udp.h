/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Adam Sutton
 * Copyright (C) 2014 Jaroslav Kysela
 *
 * tvheadend, UDP interface
 */

#ifndef UDP_H_
#define UDP_H_

#include <netinet/in.h>
#include "tcp.h"

#define UDP_FATAL_ERROR ((void *)-1)

typedef struct udp_connection {
  char *host;
  int port;
  int multicast;
  char *ifname;
  struct sockaddr_storage ip;
  char *peer_host;
  int peer_port;
  int peer_multicast;
  struct sockaddr_storage peer;
  int fd;
  int subsystem;
  char *name;
  int rxsize;
  int txsize;
} udp_connection_t;

udp_connection_t *
udp_bind ( int subsystem, const char *name,
           const char *bindaddr, int port, const char *multicast_src,
           const char *ifname, int rxsize, int txsize, int bind_fail_allowed );
int
udp_bind_double ( udp_connection_t **_u1, udp_connection_t **_u2,
                  int subsystem, const char *name1,
                  const char *name2, const char *host, int port,
                  const char *ifname, int rxsize1, int rxsize2,
                  int txsize1, int txsize2, int bind_fail_allowed );
udp_connection_t *
udp_sendinit ( int subsystem, const char *name,
               const char *ifname, int txsize );
int
udp_connect ( udp_connection_t *uc, const char *name,
              const char *host, int port );
void
udp_close ( udp_connection_t *uc );
int
udp_write( udp_connection_t *uc, const void *buf, size_t len,
           struct sockaddr_storage *storage );
int
udp_write_queue( udp_connection_t *uc, htsbuf_queue_t *q,
                 struct sockaddr_storage *storage );

typedef struct udp_multirecv {
  int             um_psize;
  int             um_packets;
  uint8_t        *um_data;
  struct iovec   *um_iovec;
  struct iovec   *um_riovec;
  struct mmsghdr *um_msg;
} udp_multirecv_t;

void
udp_multirecv_init( udp_multirecv_t *um, int packets, int psize );
void
udp_multirecv_free( udp_multirecv_t *um );
int
udp_multirecv_read( udp_multirecv_t *um, int fd, int packets,
                    struct iovec **iovec );

typedef struct udp_multisend {
  int             um_psize;
  int             um_packets;
  uint8_t        *um_data;
  struct iovec   *um_iovec;
  struct mmsghdr *um_msg;
} udp_multisend_t;

void
udp_multisend_init( udp_multisend_t *um, int packets, int psize,
                    struct iovec **iovec );
void
udp_multisend_clean( udp_multisend_t *um );
void
udp_multisend_free( udp_multisend_t *um );
int
udp_multisend_send( udp_multisend_t *um, int fd, int packets );

#endif /* UDP_H_ */
