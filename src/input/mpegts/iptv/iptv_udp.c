/*
 *  IPTV - UDP/RTP handler
 *
 *  Copyright (C) 2013 Adam Sutton
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

#include "tvheadend.h"
#include "iptv_private.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#if defined(PLATFORM_LINUX)
#include <linux/netdevice.h>
#elif defined(PLATFORM_FREEBSD)
#  include <netdb.h>
#  include <net/if.h>
#  ifndef IPV6_ADD_MEMBERSHIP
#    define IPV6_ADD_MEMBERSHIP	IPV6_JOIN_GROUP
#    define IPV6_DROP_MEMBERSHIP	IPV6_LEAVE_GROUP
#  endif
#endif


/*
 * Connect UDP/RTP
 */
static int
iptv_udp_start ( iptv_mux_t *im, const url_t *url )
{
  int fd, solip, rxsize, reuse = 1, ipv6 = 0;
  struct ifreq ifr;
  struct in_addr saddr;
  struct in6_addr s6addr;
  char name[256], buf[256];

  im->mm_display_name((mpegts_mux_t*)im, name, sizeof(name));

  /* Determine if this is IPv6 */
  if (!inet_pton(AF_INET, url->host, &saddr)) {
    ipv6 = 1;
    if (!inet_pton(AF_INET6, url->host, &s6addr)) {
      tvherror("iptv", "%s - failed to process host", name);
      return SM_CODE_TUNING_FAILED;
    }
  }

  /* Open socket */
  if ((fd = tvh_socket(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, 0)) == -1) {
    tvherror("iptv", "%s - failed to create socket [%s]",
             name, strerror(errno));
    return SM_CODE_TUNING_FAILED;
  }

  /* Mark reuse address */
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  /* Bind to interface */
  memset(&ifr, 0, sizeof(ifr));
  if (im->mm_iptv_interface && *im->mm_iptv_interface) {
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", im->mm_iptv_interface);
    if (ioctl(fd, SIOCGIFINDEX, &ifr)) {
      tvherror("iptv", "%s - could not find interface %s",
               name, im->mm_iptv_interface);
      goto error;
    }
  }

  /* IPv4 */
  if (!ipv6) {
    struct ip_mreqn      m;
    struct sockaddr_in sin;
    memset(&m,   0, sizeof(m));
    memset(&sin, 0, sizeof(sin));

    /* Bind */
    sin.sin_family = AF_INET;
    sin.sin_port   = htons(url->port);
    sin.sin_addr   = saddr;
    if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
      inet_ntop(AF_INET, &sin.sin_addr, buf, sizeof(buf));
      tvherror("iptv", "%s - cannot bind %s:%hd [e=%s]",
               name, buf, ntohs(sin.sin_port), strerror(errno));
      goto error;
    }

    /* Join group */
    m.imr_multiaddr      = sin.sin_addr;
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

  /* Bind to IPv6 group */
  } else {
    struct ipv6_mreq m;
    struct sockaddr_in6 sin;
    memset(&m,   0, sizeof(m));
    memset(&sin, 0, sizeof(sin));

    /* Bind */
    sin.sin6_family = AF_INET6;
    sin.sin6_port   = htons(url->port);
    sin.sin6_addr   = s6addr;
    if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
      inet_ntop(AF_INET6, &sin.sin6_addr, buf, sizeof(buf));
      tvherror("iptv", "%s - cannot bind %s:%hd [e=%s]",
               name, buf, ntohs(sin.sin6_port), strerror(errno));
      goto error;
    }

    /* Join group */
    m.ipv6mr_multiaddr = sin.sin6_addr;
#if defined(PLATFORM_LINUX)
    m.ipv6mr_interface = ifr.ifr_ifindex;
#elif defined(PLATFORM_FREEBSD)
    m.ipv6mr_interface = ifr.ifr_index;
#endif
#ifdef SOL_IPV6
    if (setsockopt(fd, SOL_IPV6, IPV6_ADD_MEMBERSHIP, &m, sizeof(m))) {
      inet_ntop(AF_INET, &m.ipv6mr_multiaddr, buf, sizeof(buf));
      tvhwarn("iptv", "%s - cannot join %s [%s]",
              name, buf, strerror(errno));
    }
#else
    tvherror("iptv", "IPv6 multicast not supported");
    goto error;
#endif
  }
    
  /* Increase RX buffer size */
  rxsize = IPTV_PKT_SIZE;
  if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rxsize, sizeof(rxsize)) == -1)
    tvhwarn("iptv", "%s - cannot increase UDP rx buffer size [%s]",
            name, strerror(errno));

  /* Done */
  im->mm_iptv_fd = fd;
  iptv_input_mux_started(im);
  return 0;

error:
  close(fd);
  return -1;
}

static ssize_t
iptv_udp_read ( iptv_mux_t *im, size_t *off )
{
  return sbuf_read(&im->mm_iptv_buffer, im->mm_iptv_fd);
}

static ssize_t
iptv_rtp_read ( iptv_mux_t *im, size_t *off )
{
  ssize_t len, hlen;
  int      ptr = im->mm_iptv_buffer.sb_ptr;
  uint8_t *rtp = im->mm_iptv_buffer.sb_data + ptr;

  /* Raw packet */
  len = iptv_udp_read(im, NULL);
  if (len < 0)
    return -1;

  /* Strip RTP header */
  if (len < 12)
    goto ignore;

  /* Version 2 */
  if ((rtp[0] & 0xC0) != 0x80)
    goto ignore;

  /* MPEG-TS */
  if ((rtp[1] & 0x7F) != 33)
    goto ignore;

  /* Header length (4bytes per CSRC) */
  hlen = ((rtp[0] & 0xf) * 4) + 12;
  if (rtp[0] & 0x10) {
    if (len < hlen+4)
      goto ignore;
    hlen += ((rtp[hlen+2] << 8) | rtp[hlen+3]) * 4;
    hlen += 4;
  }
  if (len < hlen || ((len - hlen) % 188) != 0)
    goto ignore;

  /* Cut header */
  memmove(rtp, rtp+hlen, len-hlen);
  im->mm_iptv_buffer.sb_ptr -= hlen;

  return len;

ignore:
  printf("ignore\n");
  im->mm_iptv_buffer.sb_ptr = ptr; // reset
  return len;
}

/*
 * Initialise UDP handler
 */

void
iptv_udp_init ( void )
{
  static iptv_handler_t ih[] = {
    {
      .scheme = "udp",
      .start  = iptv_udp_start,
      .read   = iptv_udp_read,
    },
    {
      .scheme = "rtp",
      .start  = iptv_udp_start,
      .read   = iptv_rtp_read,
    }
  };
  iptv_handler_register(ih, 2);
}
