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

#ifndef RTP_H_
#define RTP_H_

typedef struct th_rtp_streamer {
  int trs_fd;
  struct sockaddr_in trs_dest;
  int16_t trs_seq;

} th_rtp_streamer_t;

void rtp_streamer_init(th_rtp_streamer_t *trs, int fd,
		       struct sockaddr_in *dst);

void rtp_output_ts(void *opaque, struct th_subscription *s,
		   uint8_t *pkt, int blocks, int64_t pcr);

int rtp_sendmsg(uint8_t *pkt, int blocks, int64_t pcr,
		int fd, struct sockaddr *dst, socklen_t dstlen,
		uint16_t seq);

#endif /* RTP_H_ */
