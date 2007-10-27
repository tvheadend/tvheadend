/*
 *  tvheadend, RTP interface
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

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "tvhead.h"
#include "channels.h"
#include "rtp.h"
#include "dispatch.h"

#include <ffmpeg/avformat.h>
#include <ffmpeg/random.h>

//static int64_t lts;
//static int pkts;

void
rtp_output_ts(void *opaque, struct th_subscription *s,
	      uint8_t *pkt, int blocks, int64_t pcr)
{
  th_rtp_streamer_t *trs = opaque;
  struct msghdr msg;
  struct iovec vec[2];
  int r;
  
  AVRational mpeg_tc = {1, 90000};
  char hdr[12];

#if 0
  int64_t ts;
  ts = getclock_hires();
  pkts++;

  if(ts - lts > 100000) {
    printf("%d packet per sec\n", pkts * 10);
    pkts = 0;
    lts = ts;
  }
#endif

  pcr = av_rescale_q(pcr, AV_TIME_BASE_Q, mpeg_tc);

  hdr[0]  = 0x80;
  hdr[1]  = 33; /* M2TS */
  hdr[2]  = trs->trs_seq >> 8;
  hdr[3]  = trs->trs_seq;

  hdr[4]  = pcr >> 24;
  hdr[5]  = pcr >> 16;
  hdr[6]  = pcr >>  8;
  hdr[7]  = pcr;

  hdr[8]  = 0;
  hdr[9]  = 0;
  hdr[10] = 0;
  hdr[11] = 0;


  vec[0].iov_base = hdr;
  vec[0].iov_len  = 12;
  vec[1].iov_base = pkt;
  vec[1].iov_len  = blocks * 188;

  memset(&msg, 0, sizeof(msg));
  msg.msg_name    = &trs->trs_dest;
  msg.msg_namelen = sizeof(struct sockaddr_in);
  msg.msg_iov     = vec;
  msg.msg_iovlen  = 2;

  r = sendmsg(trs->trs_fd, &msg, 0);
  if(r < 0)
    perror("sendmsg");

  trs->trs_seq++;
}




void
rtp_streamer_init(th_rtp_streamer_t *trs, int fd, struct sockaddr_in *dst)
{
  trs->trs_fd = fd;
  trs->trs_dest = *dst;
  trs->trs_seq = 0;
}
