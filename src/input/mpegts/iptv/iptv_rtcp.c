/*
 *  Multicasted IPTV Input
 *  Copyright (C) 2012 Adrien CLERC
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

#include "iptv_rtcp.h"

#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "iptv_private.h"

#ifdef UNDEF
/*
 Part of RFC 3350.
 Used to send SDES data
 */
static char *
rtp_write_sdes(char *b, u_int32_t src, int argc,
               rtcp_sdes_type_t type[], char *value[],
               int length[])
{
  rtcp_sdes_t *s = (rtcp_sdes_t *) b;
  rtcp_sdes_item_t *rsp;
  int i;
  int len;
  int pad;

  /* SSRC header */
  s->src = src;
  rsp = &s->item[0];

  /* SDES items */
  for (i = 0; i < argc; i++)
  {
    rsp->type = type[i];
    len = length[i];
    if (len > RTP_MAX_SDES)
    {
      /* invalid length, may want to take other action */
      len = RTP_MAX_SDES;
    }
    rsp->length = len;
    memcpy(rsp->data, value[i], len);
    rsp = (rtcp_sdes_item_t *) & rsp->data[len];
  }

  /* terminate with end marker and pad to next 4-octet boundary */
  len = ((char *) rsp) - b;
  pad = 4 - (len & 0x3);
  b = (char *) rsp;
  while (pad--) *b++ = RTCP_SDES_END;

  return b;
}
#endif

/*
 * From RFC3350
 * tp: the last time an RTCP packet was transmitted;

   tc: the current time;

   tn: the next scheduled transmission time of an RTCP packet;

   members: the most current estimate for the number of session
      members;

   senders: the most current estimate for the number of senders in
      the session;

   rtcp_bw: The target RTCP bandwidth, i.e., the total bandwidth
      that will be used for RTCP packets by all members of this session,
      in octets per second.  This will be a specified fraction of the
      "session bandwidth" parameter supplied to the application at
      startup.

   we_sent: Flag that is true if the application has sent data
      since the 2nd previous RTCP report was transmitted.

   avg_rtcp_size: The average compound RTCP packet size, in octets,
      over all RTCP packets sent and received by this participant.  The
      size includes lower-layer transport and network protocol headers
      (e.g., UDP and IP) as explained in Section 6.2.

   initial: Flag that is true if the application has not yet sent
      an RTCP packet.
 */
static double
rtcp_interval(int members, int senders, double rtcp_bw, int we_sent, double avg_rtcp_size, int initial)
{
  /*
   * Minimum average time between RTCP packets from this site (in
   * seconds).  This time prevents the reports from `clumping' when
   * sessions are small and the law of large numbers isn't helping
   * to smooth out the traffic.  It also keeps the report interval
   * from becoming ridiculously small during transient outages like
   * a network partition.
   */
  double const RTCP_MIN_TIME = 5.;
  
  /*
   * Fraction of the RTCP bandwidth to be shared among active
   * senders.  (This fraction was chosen so that in a typical
   * session with one or two active senders, the computed report
   * time would be roughly equal to the minimum report time so that
   * we don't unnecessarily slow down receiver reports.)  The
   * receiver fraction must be 1 - the sender fraction.
   */
  double const RTCP_SENDER_BW_FRACTION = 0.25;
  double const RTCP_RCVR_BW_FRACTION = (1 - RTCP_SENDER_BW_FRACTION);
  
  /* To compensate for "timer reconsideration" converging to a
   * value below the intended average.
   */
  double const COMPENSATION = 2.71828 - 1.5;

  double t;                   /* interval */
  double rtcp_min_time = RTCP_MIN_TIME;
  int n;                      /* no. of members for computation */

  /*
   * Very first call at application start-up uses half the min
   * delay for quicker notification while still allowing some time
   * before reporting for randomization and to learn about other
   * sources so the report interval will converge to the correct
   * interval more quickly.
   */
  if (initial)
  {
    rtcp_min_time /= 2;
  }
  /*
   * Dedicate a fraction of the RTCP bandwidth to senders unless
   * the number of senders is large enough that their share is
   * more than that fraction.
   */
  n = members;
  if (senders <= members * RTCP_SENDER_BW_FRACTION)
  {
    if (we_sent)
    {
      rtcp_bw *= RTCP_SENDER_BW_FRACTION;
      n = senders;
    } else
    {
      rtcp_bw *= RTCP_RCVR_BW_FRACTION;
      n -= senders;
    }
  }

  /*
   * The effective number of sites times the average packet size is
   * the total number of octets sent when each site sends a report.
   * Dividing this by the effective bandwidth gives the time
   * interval over which those packets must be sent in order to
   * meet the bandwidth target, with a minimum enforced.  In that
   * time interval we send one report so this time is also our
   * average time between reports.
   */
  t = avg_rtcp_size * n / rtcp_bw;
  if (t < rtcp_min_time) t = rtcp_min_time;

  /*
   * To avoid traffic bursts from unintended synchronization with
   * other sites, we then pick our actual next report interval as a
   * random number uniformly distributed between 0.5*t and 1.5*t.
   */
  t = t * (drand48() + 0.5);
  t = t / COMPENSATION;
  return t;
}

/*
 Append RTCP header data to the buffer.
 Version and padding are set to fixed values, i.e. 2 and 0;
 */
static void
rtcp_append_headers(sbuf_t *buffer, rtcp_header_t *packet)
{
  uint8_t byte = 0;
  packet->version = 2;
  packet->p = 0;

  byte |= packet->version << 6;
  byte |= packet->p << 5;
  byte |= packet->count;
  sbuf_put_byte(buffer, byte);
  byte = packet->pt;
  sbuf_put_byte(buffer, byte);
  sbuf_append(buffer, &packet->length, sizeof(packet->length));
}

/*
 Append RTCP receiver report data to the buffer.
 */
static void
rtcp_append_rr(sbuf_t *buffer, rtcp_rr_t *packet)
{
  uint8_t byte = 0;
  rtcp_rr_block_t report = packet->rr[0];
  
  sbuf_append(buffer, &packet->ssrc, sizeof(packet->ssrc));
  sbuf_append(buffer, &report.ssrc, sizeof(report.ssrc));
  byte = report.fraction;
  sbuf_put_byte(buffer, byte);
  
  // Set the lost number in 3 times
  byte = (report.lost & 0xff0000) >> 16;
  sbuf_put_byte(buffer, byte);
  byte = (report.lost & 0x00ff00) >> 8;
  sbuf_put_byte(buffer, byte);
  byte = report.lost & 0x0000ff;
  sbuf_put_byte(buffer, byte);
  
  sbuf_append(buffer, &report.last_seq, sizeof(report.last_seq));
  sbuf_append(buffer, &report.jitter, sizeof(report.jitter));
  sbuf_append(buffer, &report.lsr, sizeof(report.lsr));
  sbuf_append(buffer, &report.dlsr, sizeof(report.dlsr));
}

/*
 Append RTCP NAK data to the buffer.
 */
static void
rtcp_append_nak(sbuf_t *buffer, rtcp_gf_t *packet)
{
  sbuf_append(buffer, &packet->my_ssrc, sizeof(packet->my_ssrc));
  sbuf_append(buffer, &packet->ssrc, sizeof(packet->ssrc));
  sbuf_append(buffer, &packet->pid, sizeof(packet->pid));
  sbuf_append(buffer, &packet->blp, sizeof(packet->blp));
}

/*
 Just send the buffer to the host in the rtcp_info.
 */
static void
rtcp_send(rtcp_t *info, sbuf_t *buffer)
{
  tvhdebug(LS_IPTV, "RTCP: Sending receiver report");
  // We don't care of the result right now
  udp_write(info->connection, buffer->sb_data, buffer->sb_ptr, & info->connection->peer);
}

/*
 Send a complete receiver report (RR).
 It uses the actual informations stored in rtcp_info.
 */
static void
rtcp_send_rr(rtcp_t *info)
{
  rtcp_rr_block_t report;
  rtcp_header_t header;
  rtcp_rr_t packet;
  report.ssrc = htonl(info->source_ssrc);
  
  // Fill in the extended last sequence
  union {
    uint16_t buffer[2];
    uint32_t result;
  } join2;
  join2.buffer[0] = htons(info->sequence_cycle);
  join2.buffer[1] = htons(info->last_received_sequence);
  report.last_seq = join2.result;
  
  // We don't compute this for now
  report.fraction = 0;
  report.lost = -1;
  report.lsr = htonl(0);
  report.dlsr = htonl(0);
  
  // TODO: see how to put something meaningful
  report.jitter = htonl(12);
  
  // Build the full packet
  header.pt = RTCP_RR;
  header.count = 1;
  // TODO : set the real length
  header.length = htons(7);
  packet.ssrc = htonl(info->my_ssrc);
  packet.rr[0] = report;
  
  // Build the network packet
  sbuf_t network_buffer;
  sbuf_init(&network_buffer);
  rtcp_append_headers(&network_buffer, &header);
  rtcp_append_rr(&network_buffer, &packet);
  
  // Send it
  rtcp_send(info, &network_buffer);
  
  // TODO : send also the SDES CNAME packet
  
  // Cleanup
  sbuf_free(&network_buffer);
}

ssize_t
rtcp_send_nak(rtcp_t *rtcp_info, uint32_t ssrc, uint16_t seqn, uint16_t len)
{
  rtcp_header_t rtcp_header;
  rtcp_gf_t rtcp_data;
  sbuf_t network_buffer;
  uint32_t n;
  uint16_t blp = 0;
  udp_connection_t *uc = rtcp_info->connection;

  if (len > rtcp_info->nak_req_limit) {
    seqn += len - rtcp_info->nak_req_limit;
    len = rtcp_info->nak_req_limit;
  }
  tvhinfo(LS_IPTV,
      "RTCP: Sending NAK report for SSRC 0x%x, missing: %d, following packets: %d",
      ssrc, seqn, len - 1);

  rtcp_info->last_received_sequence = seqn;
  rtcp_info->ce_cnt = len;

  // Build the full packet
  rtcp_header.version = 2;
  rtcp_header.p = 0;
  rtcp_header.count = 1; // Generic NAK
  rtcp_header.pt = RTCP_GF;
  rtcp_header.length = htons(3);
  rtcp_data.my_ssrc = 0;
  rtcp_data.ssrc = htonl(ssrc);

  while (len > 0) {
    len--;
    rtcp_data.pid = htons(seqn);
    if (len > 16) {
      blp = 0xffff;
      len -= 16;
      seqn += 17;
    } else {
      blp = 0;
      for (n = 0; n < len; n++) {
        blp |= 1 << n;
      }
      len = 0;
    }
    rtcp_data.blp = htons(blp);

    // Build the network packet
    sbuf_init(&network_buffer);
    rtcp_append_headers(&network_buffer, &rtcp_header);
    rtcp_append_nak(&network_buffer, &rtcp_data);

    // Send it
    n = udp_write(uc, network_buffer.sb_data, network_buffer.sb_ptr, &uc->peer);
    if (n) {
      tvhwarn(LS_IPTV,
          "RTCP: Sending NAK report for SSRC 0x%x failed, no data send %d %d",
          ssrc, n, (uint32_t )sizeof(network_buffer));
    }

    // Cleanup
    sbuf_free(&network_buffer);
  }
  return 0;
}

int
rtcp_connect(rtcp_t * info, char *url, char *host, int port, char *interface, char *nicename)
{
  udp_connection_t *rtcp_conn;
  url_t rtcp_url;

  if (info->connection == NULL) {
    rtcp_conn = udp_bind(LS_IPTV, nicename, NULL, 0, NULL, interface,
    IPTV_BUF_SIZE, 1024);
    if (rtcp_conn == NULL || rtcp_conn == UDP_FATAL_ERROR) {
      tvhwarn(LS_IPTV, "%s - Unable to bind, RTCP won't be available",
          nicename);
      return -1;
    }
    info->connection_fd = rtcp_conn->fd;
    info->connection = rtcp_conn;
  }

  urlinit(&rtcp_url);
  if (host == NULL) {
    if (urlparse(url ? : "", &rtcp_url)) {
      tvhwarn(LS_IPTV, "%s - invalid RTCP URL, should be rtp://HOST:PORT [%s]",
          nicename, url);
      goto fail;
    }
    host = rtcp_url.host;
    port = rtcp_url.port;
  }

  if (udp_connect(info->connection, "rtcp", host, port)) {
    tvhwarn(LS_IPTV, "%s - Unable to connect, RTCP won't be available",
        nicename);
    goto fail;
  }
  urlreset(&rtcp_url);
  return 0;
fail:
  urlreset(&rtcp_url);
  return -1;
}

int
rtcp_init(rtcp_t * info)
{
  info->last_ts = 0;
  info->next_ts = 0;
  info->members = 2;
  info->senders = 1;
  info->last_received_sequence = 0;
  info->sequence_cycle = 1;
  info->source_ssrc = 0;
  info->average_packet_size = 52;
  info->my_ssrc = 0; // Since we are not transmitting, set this to 0
  info->nak_req_limit = 128; // This appears to be a safe limit
  
  return 0;
}

int
rtcp_destroy(rtcp_t *info)
{
  return 0;
}

/*
 * Buffer is a raw RTP buffer
 */
int
rtcp_receiver_update(rtcp_t *info, uint8_t *buffer)
{
  union {
    uint8_t bytes[2];
    uint16_t n;
  } join2;
  join2.bytes[0] = buffer[2];
  join2.bytes[1] = buffer[3];
  int new_sequence = ntohs(join2.n);
  
  if(new_sequence < info->last_received_sequence)
  {
    ++info->sequence_cycle;
  }
  info->last_received_sequence = new_sequence;

  union {
    uint8_t bytes[4];
    uint32_t n;
  } join4;
  join4.bytes[0] = buffer[8];
  join4.bytes[1] = buffer[9];
  join4.bytes[2] = buffer[10];
  join4.bytes[3] = buffer[11];
  info->source_ssrc = ntohl(join4.n);
  
  time_t current_time = time(NULL);
  if(info->next_ts < current_time)
  {
    // Re-send
    rtcp_send_rr(info);
    
    // Re-schedule
    double interval = rtcp_interval(info->members, info->senders, 12, 0, info->average_packet_size, info->last_ts == 0);
    info->next_ts = current_time + interval;
    info->last_ts = current_time;
  }
  
  return 0;
}
