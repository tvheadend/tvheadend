/*
 *  Tvheadend, RTP streamer
 *  Copyright (C) 2007, 2009 Andreas Öman
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

#ifndef RTP_H_
#define RTP_H_

typedef void (rtp_send_t)(void *opaque, void *buf, size_t len);

#define RTP_MAX_PACKET_SIZE 1472

typedef struct rtp_stream {
  uint16_t rs_seq;
  
  int rs_ptr;
  uint8_t rs_buf[RTP_MAX_PACKET_SIZE];
} rtp_stream_t;

void rtp_send_mpv(rtp_send_t *sender, void *opaque, rtp_stream_t *rs, 
		  const uint8_t *data, size_t len, int64_t pts);

void rtp_send_mpa(rtp_send_t *sender, void *opaque, rtp_stream_t *rs, 
		  const uint8_t *data, size_t len, int64_t pts);

#endif /* RTP_H_ */
