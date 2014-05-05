/*
 *  TVHeadend - UDP common routines
 *
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

#define _GNU_SOURCE
#include "tvheadend.h"
#include "udp.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#if defined(PLATFORM_LINUX)
#include <linux/netdevice.h>
#elif defined(PLATFORM_FREEBSD)
#  include <net/if.h>
#  ifndef IPV6_ADD_MEMBERSHIP
#    define IPV6_ADD_MEMBERSHIP	IPV6_JOIN_GROUP
#    define IPV6_DROP_MEMBERSHIP	IPV6_LEAVE_GROUP
#  endif
#endif

extern int tcp_preferred_address_family;

static int
udp_resolve( udp_connection_t *uc, int receiver )
{
  struct addrinfo hints, *res, *ressave, *use = NULL;
  char port_buf[6];
  int x;

  snprintf(port_buf, 6, "%d", uc->port);

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = receiver ? AI_PASSIVE : 0;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  
  x = getaddrinfo(uc->host, port_buf, &hints, &res);
  if (x < 0) {
    tvhlog(LOG_ERR, uc->subsystem, "getaddrinfo: %s: %s",
           uc->host != NULL ? uc->host : "*",
           x == EAI_SYSTEM ? strerror(errno) : gai_strerror(x));
    return -1;
  }

  ressave = res;
  while (res) {
    if (res->ai_family == tcp_preferred_address_family) {
      use = res;
      break;
    } else if (use == NULL) {
      use = res;
    }
    res = res->ai_next;
  }
  if (use->ai_family == AF_INET6) {
    uc->ip.ss_family        = AF_INET6;
    IP_AS_V6(uc->ip, port)  = htons(uc->port);
    memcpy(&IP_AS_V6(uc->ip, addr), &((struct sockaddr_in6 *)use->ai_addr)->sin6_addr,
                                                             sizeof(struct in6_addr));
    uc->multicast           = !!IN6_IS_ADDR_MULTICAST(&IP_AS_V6(uc->ip, addr));
  } else if (use->ai_family == AF_INET) {
    uc->ip.ss_family        = AF_INET;
    IP_AS_V4(uc->ip, port)  = htons(uc->port);
    IP_AS_V4(uc->ip, addr)  = ((struct sockaddr_in *)use->ai_addr)->sin_addr;
    uc->multicast           = !!IN_MULTICAST(ntohl(IP_AS_V4(uc->ip, addr.s_addr)));
  }
  freeaddrinfo(ressave);
  if (uc->ip.ss_family != AF_INET && uc->ip.ss_family != AF_INET6) {
    tvherror(uc->subsystem, "%s - failed to process host '%s'", uc->name, uc->host);
    return -1;
  }
  return 0;
}

udp_connection_t *
udp_bind ( const char *subsystem, const char *name,
           const char *bindaddr, int port,
           const char *ifname, int rxsize )
{
  int fd, solip, reuse = 1;
  struct ifreq ifr;
  udp_connection_t *uc;
  char buf[256];
  socklen_t addrlen;

  uc = calloc(1, sizeof(udp_connection_t));
  uc->fd                   = -1;
  uc->host                 = bindaddr ? strdup(bindaddr) : NULL;
  uc->port                 = port;
  uc->ifname               = ifname ? strdup(ifname) : NULL;
  uc->subsystem            = subsystem ? strdup(subsystem) : NULL;
  uc->name                 = name ? strdup(name) : NULL;
  uc->rxtxsize             = rxsize;

  if (udp_resolve(uc, 1) < 0) {
    udp_close(uc);
    return UDP_FATAL_ERROR;
  }

  /* Open socket */
  if ((fd = tvh_socket(uc->ip.ss_family, SOCK_DGRAM, 0)) == -1) {
    tvherror(subsystem, "%s - failed to create socket [%s]",
             name, strerror(errno));
    udp_close(uc);
    return UDP_FATAL_ERROR;
  }

  /* Mark reuse address */
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  /* Bind to interface */
  memset(&ifr, 0, sizeof(ifr));
  if (ifname && *ifname) {
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    if (ioctl(fd, SIOCGIFINDEX, &ifr)) {
      tvherror(subsystem, "%s - could not find interface %s",
               name, ifname);
      goto error;
    }
  }

  /* IPv4 */
  if (uc->ip.ss_family == AF_INET) {
    struct ip_mreqn      m;
    memset(&m,   0, sizeof(m));

    /* Bind */
    if (bind(fd, (struct sockaddr *)&uc->ip, sizeof(struct sockaddr_in)) == -1) {
      inet_ntop(AF_INET, &IP_AS_V4(uc->ip, addr), buf, sizeof(buf));
      tvherror(subsystem, "%s - cannot bind %s:%hu [e=%s]",
               name, buf, ntohs(IP_AS_V4(uc->ip, port)), strerror(errno));
      goto error;
    }

    if (uc->multicast) {
      /* Join group */
      m.imr_multiaddr      = IP_AS_V4(uc->ip, addr);
      m.imr_address.s_addr = 0;
#if defined(PLATFORM_LINUX)
      m.imr_ifindex        = ifr.ifr_ifindex;
#elif defined(PLATFORM_FREEBSD)
      m.imr_ifindex        = ifr.ifr_index;
#endif
#ifdef SOL_IP
      solip = SOL_IP;
#else
      {
        struct protoent *pent;
        pent = getprotobyname("ip");
        solip = (pent != NULL) ? pent->p_proto : 0;
      }
#endif

      if (setsockopt(fd, solip, IP_ADD_MEMBERSHIP, &m, sizeof(m))) {
        inet_ntop(AF_INET, &m.imr_multiaddr, buf, sizeof(buf));
        tvhwarn("iptv", "%s - cannot join %s [%s]",
                name, buf, strerror(errno));
      }
   }

  /* Bind to IPv6 group */
  } else {
    struct ipv6_mreq m;
    memset(&m,   0, sizeof(m));

    /* Bind */
    if (bind(fd, (struct sockaddr *)&uc->ip, sizeof(struct sockaddr_in6)) == -1) {
      inet_ntop(AF_INET6, &IP_AS_V6(uc->ip, addr), buf, sizeof(buf));
      tvherror(subsystem, "%s - cannot bind %s:%hu [e=%s]",
               name, buf, ntohs(IP_AS_V6(uc->ip, port)), strerror(errno));
      goto error;
    }

    if (uc->multicast) {
      /* Join group */
      m.ipv6mr_multiaddr = IP_AS_V6(uc->ip, addr);
#if defined(PLATFORM_LINUX)
      m.ipv6mr_interface = ifr.ifr_ifindex;
#elif defined(PLATFORM_FREEBSD)
      m.ipv6mr_interface = ifr.ifr_index;
#endif
#ifdef SOL_IPV6
      if (setsockopt(fd, SOL_IPV6, IPV6_ADD_MEMBERSHIP, &m, sizeof(m))) {
        inet_ntop(AF_INET, &m.ipv6mr_multiaddr, buf, sizeof(buf));
        tvhwarn(subsystem, "%s - cannot join %s [%s]",
                name, buf, strerror(errno));
      }
#else
      tvherror("iptv", "IPv6 multicast not supported");
      goto error;
#endif
    }
  }

  addrlen = sizeof(uc->ip);
  getsockname(fd, (struct sockaddr *)&uc->ip, &addrlen);
    
  /* Increase RX buffer size */
  if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rxsize, sizeof(rxsize)) == -1)
    tvhwarn("iptv", "%s - cannot increase UDP rx buffer size [%s]",
            name, strerror(errno));

  uc->fd = fd;
  return uc;

error:
  udp_close(uc);
  return NULL;
}

int
udp_bind_double ( udp_connection_t **_u1, udp_connection_t **_u2,
                  const char *subsystem, const char *name1,
                  const char *name2, const char *host, int port,
                  const char *ifname, int rxsize1, int rxsize2 )
{
  udp_connection_t *u1 = NULL, *u2 = NULL;
  udp_connection_t *ucs[10] = { NULL, NULL, NULL, NULL, NULL,
                                NULL, NULL, NULL, NULL, NULL };
  int pos = 0, i, port2;

  memset(&ucs, 0, sizeof(ucs));
  while (1) {
    u1 = udp_bind(subsystem, name1, host, port, ifname, rxsize1);
    if (u1 == NULL || u1 == UDP_FATAL_ERROR)
      goto fail;
    port2 = ntohs(IP_PORT(u1->ip));
    /* RTP port should be even, RTCP port should be odd */
    if ((port2 % 2) == 0) {
      u2 = udp_bind(subsystem, name2, host, port2 + 1, ifname, rxsize2);
      if (u2 != NULL && u2 != UDP_FATAL_ERROR)
        break;
    }
    ucs[pos++] = u1;
    if (port || pos >= ARRAY_SIZE(ucs))
      goto fail;
  }
  for (i = 0; i < pos; i++)
    udp_close(ucs[i]);
  *_u1 = u1;
  *_u2 = u2;
  return 0;
fail:
  for (i = 0; i < pos; i++)
    udp_close(ucs[i]);
  return -1;
}

udp_connection_t *
udp_connect ( const char *subsystem, const char *name,
              const char *host, int port,
              const char *ifname, int txsize )
{
  int fd;
  struct ifreq ifr;
  udp_connection_t *uc;

  uc = calloc(1, sizeof(udp_connection_t));
  uc->fd                   = -1;
  uc->host                 = host ? strdup(host) : NULL;
  uc->port                 = port;
  uc->ifname               = ifname ? strdup(ifname) : NULL;
  uc->subsystem            = subsystem ? strdup(subsystem) : NULL;
  uc->name                 = name ? strdup(name) : NULL;
  uc->rxtxsize             = txsize;

  if (udp_resolve(uc, 1) < 0) {
    udp_close(uc);
    return UDP_FATAL_ERROR;
  }

  /* Open socket */
  if ((fd = tvh_socket(uc->ip.ss_family, SOCK_DGRAM, 0)) == -1) {
    tvherror(subsystem, "%s - failed to create socket [%s]",
             name, strerror(errno));
    udp_close(uc);
    return UDP_FATAL_ERROR;
  }

  /* Bind to interface */
  memset(&ifr, 0, sizeof(ifr));
  if (ifname && *ifname) {
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    if (ioctl(fd, SIOCGIFINDEX, &ifr)) {
      tvherror(subsystem, "%s - could not find interface %s",
               name, ifname);
      goto error;
    }
  }

  /* Increase TX buffer size */
  if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &txsize, sizeof(txsize)) == -1)
    tvhwarn("iptv", "%s - cannot increase UDP tx buffer size [%s]",
            name, strerror(errno));

  uc->fd = fd;
  return uc;

error:
  udp_close(uc);
  return NULL;
}

void
udp_close( udp_connection_t *uc )
{
  if (uc == NULL || uc == UDP_FATAL_ERROR)
    return;
  if (uc->fd >= 0)
    close(uc->fd);
  free(uc->host);
  free(uc->ifname);
  free(uc->subsystem);
  free(uc->name);
  free(uc);
}

int
udp_write( udp_connection_t *uc, const void *buf, size_t len,
           struct sockaddr_storage *storage )
{
  int r;

  if (storage == NULL)
    storage = &uc->ip;
  while (len) {
    r = sendto(uc->fd, buf, len, 0, (struct sockaddr*)storage,
               storage->ss_family == AF_INET6 ?
                 sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));
    if (r < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        usleep(100);
        continue;
      }
      break;
    }
    len -= r;
    buf += r;
  }
  return len;
}

int
udp_write_queue( udp_connection_t *uc, htsbuf_queue_t *q,
                 struct sockaddr_storage *storage )
{
  htsbuf_data_t *hd;
  int l, r = 0;
  void *p;

  while ((hd = TAILQ_FIRST(&q->hq_q)) != NULL) {
    if (!r) {
      l = hd->hd_data_len - hd->hd_data_off;
      p = hd->hd_data + hd->hd_data_off;
      r = udp_write(uc, p, l, storage);
    }
    htsbuf_data_free(q, hd);
  }
  q->hq_size = 0;
  return r;
}

/*
 * UDP multi packet receive support
 */

#if !defined (CONFIG_RECVMMSG) && defined(__linux__)
/* define the syscall - works only for linux */
#include <linux/unistd.h>
#ifdef __NR_recvmmsg

struct mmsghdr {
  struct msghdr msg_hdr;
  unsigned int  msg_len;
};

int recvmmsg(int sockfd, struct mmsghdr *msgvec, unsigned int vlen,
             unsigned int flags, struct timespec *timeout);

int recvmmsg(int sockfd, struct mmsghdr *msgvec, unsigned int vlen,
             unsigned int flags, struct timespec *timeout)
{
  return syscall(__NR_recvmmsg, sockfd, msgvec, vlen, flags, timeout);
}

#define CONFIG_RECVMMSG

#endif
#endif


#ifndef CONFIG_RECVMMSG

struct mmsghdr {
  struct msghdr msg_hdr;
  unsigned int  msg_len;
};

static int
recvmmsg(int sockfd, struct mmsghdr *msgvec, unsigned int vlen,
         unsigned int flags, struct timespec *timeout)
{
  ssize_t r;
  unsigned int i;

  for (i = 0; i < vlen; i++) {
    r = recvmsg(sockfd, &msgvec->msg_hdr, flags);
    if (r < 0)
      return (i > 0) ? i : r;
    msgvec->msg_len = r;
    msgvec++;
  }
  return i;
}

#endif


void
udp_multirecv_init( udp_multirecv_t *um, int packets, int psize )
{
  int i;

  um->um_psize   = psize;
  um->um_packets = packets;
  um->um_data    = malloc(packets * psize);
  um->um_iovec   = malloc(packets * sizeof(struct iovec));
  um->um_riovec  = malloc(packets * sizeof(struct iovec));
  um->um_msg     = calloc(packets,  sizeof(struct mmsghdr));
  for (i = 0; i < packets; i++) {
    ((struct mmsghdr *)um->um_msg)[i].msg_hdr.msg_iov    = &um->um_iovec[i];
    ((struct mmsghdr *)um->um_msg)[i].msg_hdr.msg_iovlen = 1;
    um->um_iovec[i].iov_base  = /* follow thru */
    um->um_riovec[i].iov_base = um->um_data + i * psize;
    um->um_iovec[i].iov_len   = psize;
  }
}

void
udp_multirecv_free( udp_multirecv_t *um )
{
  free(um->um_msg);   um->um_msg   = NULL;
  free(um->um_iovec); um->um_iovec = NULL;
  free(um->um_data);  um->um_data  = NULL;
  um->um_psize   = 0;
  um->um_packets = 0;
}

int
udp_multirecv_read( udp_multirecv_t *um, int fd, int packets,
                    struct iovec **iovec )
{
  int n, i;
  if (packets > um->um_packets)
    packets = um->um_packets;
  n = recvmmsg(fd, (struct mmsghdr *)um->um_msg, packets, MSG_DONTWAIT, NULL);
  if (n > 0) {
    for (i = 0; i < n; i++)
      um->um_riovec[i].iov_len = ((struct mmsghdr *)um->um_msg)[i].msg_len;
    *iovec = um->um_riovec;
  }
  return n;
}
