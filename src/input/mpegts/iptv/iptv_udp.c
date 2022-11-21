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
#include "iptv_rtcp.h"

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
iptv_udp_start
  ( iptv_input_t *mi, iptv_mux_t *im, const char *raw, const url_t *url )
{
  udp_connection_t *conn;
  udp_multirecv_init(&im->im_um, IPTV_PKTS, IPTV_PKT_PAYLOAD);

  /* Note: url->user is used for specifying multicast source address (SSM)
     here. The URL format is rtp://<srcaddr>@<grpaddr>:<port> */
  conn = udp_bind(LS_IPTV, im->mm_nicename, url->host, url->port, url->user,
                  im->mm_iptv_interface, IPTV_BUF_SIZE, 4*1024);
  if (conn == UDP_FATAL_ERROR)
    return SM_CODE_TUNING_FAILED;
  if (conn == NULL)
    return -1;

  im->mm_iptv_fd         = conn->fd;
  im->mm_iptv_connection = conn;

  /* Setup the RTCP Retransmission connection when configured */
  rtcp_init(&im->im_rtcp_info);
  if(im->mm_iptv_ret_url && rtcp_connect(&im->im_rtcp_info, im->mm_iptv_ret_url,
          NULL, 0, im->mm_iptv_interface, im->mm_nicename) == 0) {
      im->im_use_retransmission = 1;
      udp_multirecv_init(&im->im_rtcp_info.um, IPTV_PKTS, IPTV_PKT_PAYLOAD);
      sbuf_reset_and_alloc(&im->im_temp_buffer, IPTV_BUF_SIZE);
  }

  im->mm_iptv_rtp_seq = -1;

  iptv_input_mux_started(mi, im, 1);
  return 0;
}

static void
iptv_udp_stop
  ( iptv_input_t *mi, iptv_mux_t *im )
{
  im->im_data = NULL;
  tvh_mutex_unlock(&iptv_lock);
  udp_multirecv_free(&im->im_um);
  udp_multirecv_free(&im->im_rtcp_info.um);
  sbuf_free(&im->im_temp_buffer);
  tvh_mutex_lock(&iptv_lock);
}

static ssize_t
iptv_udp_read ( iptv_input_t *mi, iptv_mux_t *im )
{
  int i, n;
  struct iovec *iovec;
  ssize_t res = 0;

  n = udp_multirecv_read(&im->im_um, im->mm_iptv_fd, IPTV_PKTS, &iovec);
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
    tvherror(LS_IPTV, "receiving non-raw UDP data for %s!", im->mm_nicename);
    im->mm_iptv_rtp_seq = 0x10000; /* no further logs! */
  }

  return res;
}

ssize_t
iptv_rtp_read(iptv_mux_t *im, void (*pkt_cb)(iptv_mux_t *im, uint8_t *pkt, int len))
{
  ssize_t len, hlen;
  uint8_t *rtp;
  int i, n = 0;
  uint32_t seq, nseq, oseq, ssrc, unc = 0;
  struct iovec *iovec;
  ssize_t res = 0;
  char is_ret_buffer = 0;

  if (im->im_use_retransmission) {
    n = udp_multirecv_read(&im->im_rtcp_info.um, im->im_rtcp_info.connection_fd, IPTV_PKTS, &iovec);
    if (n > 0 && !im->im_is_ce_detected) {
      tvhwarn(LS_IPTV, "RET receiving %d unexpected packets for %s", n,
          im->mm_nicename);
    }
    else if (n > 0) {
      tvhtrace(LS_IPTV, "RET receiving %d packets for %s", n, im->mm_nicename);
      is_ret_buffer = 1;
      im->im_rtcp_info.ce_cnt -= n;
      im->im_rtcp_info.last_received_sequence += n;
    } else {
      n = udp_multirecv_read(&im->im_um, im->mm_iptv_fd, IPTV_PKTS, &iovec);
    }
  } else
    n = udp_multirecv_read(&im->im_um, im->mm_iptv_fd, IPTV_PKTS, &iovec);

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

    /* MPEG-TS or DynamicRTP */
    if ((rtp[1] & 0x7F) != 33 && (rtp[1] & 0x7F) != 96)
      continue;

    /* Header length (4bytes per CSRC) */
    hlen = ((rtp[0] & 0xf) * 4) + 12;
    if (is_ret_buffer) {
      /* Skip OSN (original sequence number) field for RET packets */
      hlen += 2;
    }
    if (rtp[0] & 0x10) {
      if (len < hlen+4)
        continue;
      hlen += ((rtp[hlen+2] << 8) | rtp[hlen+3]) * 4;
      hlen += 4;
    }
    if (len < hlen || ((len - hlen) % 188) != 0)
      continue;

    len -= hlen;

    nseq = (rtp[2] << 8) | rtp[3];
    if (seq == -1 || nseq == 0)
      seq = nseq;
    /* Some sources will send the retransmission packets as part of the regular
     * stream, we can only detect them by checking for the expected seqn. */
    if (im->im_is_ce_detected && !is_ret_buffer && nseq == im->im_rtcp_info.last_received_sequence) {
      is_ret_buffer = 1;
      im->im_rtcp_info.ce_cnt --;
      im->im_rtcp_info.last_received_sequence ++;
    }

    if (!is_ret_buffer) {
      if(seq != nseq && ((seq + 1) & 0xffff) != nseq) {
        unc += (len / 188)
            * (uint32_t) ((uint16_t) nseq - (uint16_t) (seq + 1));
        ssrc = (rtp[8] << 24) | (rtp[9] << 16) | (rtp[10] << 8) | rtp[11];
      /* Use uncorrectable value to notify RTP delivery issues */
        tvhwarn(LS_IPTV, "RTP discontinuity for %s SSRC: 0x%x (%i != %i)", im->mm_nicename,
            ssrc, seq + 1, nseq);
        if (im->im_use_retransmission && !im->im_is_ce_detected) {
          im->im_is_ce_detected = 1;
          rtcp_send_nak(&im->im_rtcp_info, ssrc, seq + 1, nseq - seq - 1);
        }
      }
      seq = nseq;
    }

    if (im->im_is_ce_detected) {
      /* Move data to RET buffer */
      ssrc = (rtp[8] << 24) | (rtp[9] << 16) | (rtp[10] << 8) | rtp[11];
      if (is_ret_buffer) {
        oseq = (rtp[12] << 8) | rtp[13];
        tvhtrace(LS_IPTV, "RTP RET received SEQ %i OSN %i for SSRC: 0x%x", nseq, oseq, ssrc);
        sbuf_append(&im->mm_iptv_buffer, rtp + hlen, len);
      } else
        sbuf_append(&im->im_temp_buffer, rtp + hlen, len);
      /* If we received all RET packets dump the temporary buffer back into the iptv buffer,
       * or if it takes too long just continue as normal. RET packet rate can be a lot slower
       * then the main stream so this can take some time. */
      if(im->im_rtcp_info.ce_cnt > 0 && im->im_temp_buffer.sb_ptr > 1600 * IPTV_PKT_PAYLOAD) {
        tvhwarn(LS_IPTV, "RTP RET waiting for packets timeout for SSRC: 0x%x", ssrc);
        im->im_rtcp_info.ce_cnt = 0;
      }
      if(im->im_rtcp_info.ce_cnt <= 0) {
        im->im_rtcp_info.ce_cnt = 0;
        im->im_is_ce_detected = 0;
        sbuf_append_from_sbuf(&im->mm_iptv_buffer, &im->im_temp_buffer);
        sbuf_reset_and_alloc(&im->im_temp_buffer, IPTV_BUF_SIZE);
      }
    } else {
      /* Move data */
      sbuf_append(&im->mm_iptv_buffer, rtp + hlen, len);
    }
    res += len;
  }

  im->mm_iptv_rtp_seq = seq;
  if (im->mm_active)
    atomic_add(&im->mm_active->tii_stats.unc, unc);

  return res;
}

static ssize_t
iptv_udp_rtp_read ( iptv_input_t *mi, iptv_mux_t *im )
{
  return iptv_rtp_read(im, NULL);
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
