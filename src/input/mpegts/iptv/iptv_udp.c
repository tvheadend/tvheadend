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

/*
 * Connect UDP/RTP
 */
static int
iptv_udp_start ( iptv_mux_t *im, const url_t *url )
{
  char name[256];
  udp_connection_t *conn;
  udp_multirecv_t *um;

  mpegts_mux_nice_name((mpegts_mux_t*)im, name, sizeof(name));

  conn = udp_bind("iptv", name, url->host, url->port,
                  im->mm_iptv_interface, IPTV_BUF_SIZE);
  if (conn == UDP_FATAL_ERROR)
    return SM_CODE_TUNING_FAILED;
  if (conn == NULL)
    return -1;

  /* Done */
  im->mm_iptv_fd         = conn->fd;
  im->mm_iptv_connection = conn;

  um = calloc(1, sizeof(*um));
  udp_multirecv_init(um, IPTV_PKTS, IPTV_PKT_PAYLOAD);
  im->im_data = um;

  iptv_input_mux_started(im);
  return 0;
}

static void
iptv_udp_stop
  ( iptv_mux_t *im )
{
  udp_multirecv_t *um = im->im_data;

  im->im_data = NULL;
  udp_multirecv_free(um);
  free(um);
}

static ssize_t
iptv_udp_read ( iptv_mux_t *im )
{
  int i, n;
  struct iovec *iovec;
  udp_multirecv_t *um = im->im_data;
  ssize_t res = 0;

  n = udp_multirecv_read(um, im->mm_iptv_fd, IPTV_PKTS, &iovec);
  if (n < 0)
    return -1;

  for (i = 0; i < n; i++, iovec++) {
    sbuf_append(&im->mm_iptv_buffer, iovec->iov_base, iovec->iov_len);
    res += iovec->iov_len;
  }

  return res;
}

static ssize_t
iptv_rtp_read ( iptv_mux_t *im )
{
  ssize_t len, hlen;
  uint8_t *rtp;
  int i, n;
  struct iovec *iovec;
  udp_multirecv_t *um = im->im_data;
  ssize_t res = 0;

  n = udp_multirecv_read(um, im->mm_iptv_fd, IPTV_PKTS, &iovec);
  if (n < 0)
    return -1;

  for (i = 0; i < n; i++, iovec++) {

    /* Raw packet */
    rtp = iovec->iov_base;
    len = iovec->iov_len;

    /* Strip RTP header */
    if (len < 12)
      continue;

    /* Version 2 */
    if ((rtp[0] & 0xC0) != 0x80)
      continue;

    /* MPEG-TS */
    if ((rtp[1] & 0x7F) != 33)
      continue;

    /* Header length (4bytes per CSRC) */
    hlen = ((rtp[0] & 0xf) * 4) + 12;
    if (rtp[0] & 0x10) {
      if (len < hlen+4)
        continue;
      hlen += ((rtp[hlen+2] << 8) | rtp[hlen+3]) * 4;
      hlen += 4;
    }
    if (len < hlen || ((len - hlen) % 188) != 0)
      continue;

    /* Move data */
    len -= hlen;
    sbuf_append(&im->mm_iptv_buffer, rtp + hlen, len);
    res += len;
  }

  return res;
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
      .stop   = iptv_udp_stop,
      .read   = iptv_udp_read,
    },
    {
      .scheme = "rtp",
      .start  = iptv_udp_start,
      .stop   = iptv_udp_stop,
      .read   = iptv_rtp_read,
    }
  };
  iptv_handler_register(ih, 2);
}
