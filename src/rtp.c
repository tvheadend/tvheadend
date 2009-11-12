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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "tvhead.h"
#include "rtp.h"

static const AVRational mpeg_tc = {1, 90000};

void
rtp_send_mpv(rtp_send_t *sender, void *opaque, rtp_stream_t *rs, 
	     const uint8_t *data, size_t len,
	     int64_t pts)
{
  uint32_t flags = 0;
  int s;
  int payloadsize = RTP_MAX_PACKET_SIZE - (4 + 4 + 4 + 4);
  uint8_t *buf;
    
  if(data[0] != 0x00 || data[1] != 0x00 || data[2] != 0x01)
    return; // Not a startcode, something is fishy

  pts = av_rescale_q(pts, AV_TIME_BASE_Q, mpeg_tc);

  if(data[3] == 0xb3) {
    // Sequence Start code, set Begin-Of-Sequence
    flags |= 1 << 13;
  }
  while(len > 0) {

    s = len > payloadsize ? payloadsize : len;

    buf = rs->rs_buf;
    buf[0] = 0x80;
    buf[1] = 32 | (len == payloadsize ? 0x80 : 0);
    buf[2] = rs->rs_seq >> 8;
    buf[3] = rs->rs_seq;

    buf[4] = pts >> 24;
    buf[5] = pts >> 16;
    buf[6] = pts >> 8;
    buf[7] = pts;
    
    buf[8] = 0;
    buf[9] = 0;
    buf[10] = 0;
    buf[11] = 0;

    buf[12] = flags >> 24;
    buf[13] = flags >> 16;
    buf[14] = flags >> 8;
    buf[15] = flags;
    
    memcpy(buf + 16, data, s);

    len  -= s;
    data += s;

    sender(opaque, buf, s + 16);
    rs->rs_seq++;

    flags = 0;

  }
  assert(len == 0);
}
