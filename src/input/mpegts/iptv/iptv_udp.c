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
iptv_udp_start ( iptv_mux_t *im, const char *raw, const url_t *url )
{
  char name[256];
  udp_connection_t *conn;
  udp_multirecv_t *um;

  mpegts_mux_nice_name((mpegts_mux_t*)im, name, sizeof(name));

  conn = udp_bind("iptv", name, url->host, url->port,
                  im->mm_iptv_interface, IPTV_BUF_SIZE, 4*1024);
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
  pthread_mutex_unlock(&iptv_lock);
  udp_multirecv_free(um);
  free(um);
  pthread_mutex_lock(&iptv_lock);
}

static ssize_t
iptv_udp_read ( iptv_mux_t *im )
{
  int i, n;
  struct iovec *iovec;
  udp_multirecv_t *um = im->im_data;
  ssize_t res = 0;
  char name[256];

  n = udp_multirecv_read(um, im->mm_iptv_fd, IPTV_PKTS, &iovec);
  if (n < 0)
    return -1;

  im->mm_iptv_rtp_seq &= ~0xfff;
  for (i = 0; i < n; i++, iovec++) {
    if (iovec->iov_len <= 0)
      continue;
    if (*(uint8_t *)iovec->iov_base != 0x47) {
      im->mm_iptv_rtp_seq++;
      continue;
    }
    sbuf_append(&im->mm_iptv_buffer, iovec->iov_base, iovec->iov_len);
    res += iovec->iov_len;
  }

  if (im->mm_iptv_rtp_seq < 0xffff && im->mm_iptv_rtp_seq > 0x3ff) {
    mpegts_mux_nice_name((mpegts_mux_t*)im, name, sizeof(name));
    tvherror("iptv", "receving non-raw UDP data for %s!", name);
    im->mm_iptv_rtp_seq = 0x10000; /* no further logs! */
  }

  return res;
}

ssize_t
iptv_rtp_read ( iptv_mux_t *im, udp_multirecv_t *um,
                void (*pkt_cb)(iptv_mux_t *im, uint8_t *pkt, int len) )
{
  ssize_t len, hlen;
  uint8_t *rtp;
  int i, n;
  uint32_t seq, nseq, unc = 0;
  struct iovec *iovec;
  ssize_t res = 0;

  n = udp_multirecv_read(um, im->mm_iptv_fd, IPTV_PKTS, &iovec);
  if (n < 0)
    return -1;

  seq = im->mm_iptv_rtp_seq;

  for (i = 0; i < n; i++, iovec++) {

    /* Raw packet */
    rtp = iovec->iov_base;
    len = iovec->iov_len;

    /* Strip RTP header */
    if (len < 12)
      continue;

    if (pkt_cb)
      pkt_cb(im, rtp, len);

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

    len -= hlen;

    /* Use uncorrectable value to notify RTP delivery issues */
    nseq = (rtp[2] << 8) | rtp[3];
    if (seq == -1)
      seq = nseq;
    else if (((seq + 1) & 0xffff) != nseq) {
      unc += (len / 188) * (uint32_t)((uint16_t)nseq-(uint16_t)(seq+1));
      tvhtrace("iptv", "RTP discontinuity (%i != %i)", seq + 1, nseq);
    }
    seq = nseq;

    /* Move data */
    tsdebug_write((mpegts_mux_t *)im, rtp + hlen, len);
    sbuf_append(&im->mm_iptv_buffer, rtp + hlen, len);
    res += len;
  }

  im->mm_iptv_rtp_seq = seq;
  if (im->mm_active)
    atomic_add(&im->mm_active->tii_stats.unc, unc);

  return res;
}

static ssize_t
iptv_udp_rtp_read ( iptv_mux_t *im )
{
  udp_multirecv_t *um = im->im_data;

  return iptv_rtp_read(im, um, NULL);
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
      .buffer_limit = UINT32_MAX, /* unlimited */
      .start  = iptv_udp_start,
      .stop   = iptv_udp_stop,
      .read   = iptv_udp_read,
      .pause  = iptv_input_pause_handler
    },
    {
      .scheme = "rtp",
      .buffer_limit = UINT32_MAX, /* unlimited */
      .start  = iptv_udp_start,
      .stop   = iptv_udp_stop,
      .read   = iptv_udp_rtp_read,
      .pause  = iptv_input_pause_handler
    }
  };
  iptv_handler_register(ih, 2);
}
