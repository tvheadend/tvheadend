/*
 *  Elementary stream functions
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

#ifndef PARSERS_H
#define PARSERS_H

#include "packet.h"

static inline int64_t
getpts(const uint8_t *p)
{
  int a =  p[0];
  int b = (p[1] << 8) | p[2];
  int c = (p[3] << 8) | p[4];

  if ((a & 1) && (b & 1) && (c & 1)) {

    return
      ((int64_t)((a >> 1) & 0x07) << 30) |
      ((int64_t)((b >> 1)       ) << 15) |
      ((int64_t)((c >> 1)       ))
      ;

  } else {
    // Marker bits not present
    return PTS_UNSET;
  }
}

void parse_mpeg_ts(struct service *t, struct elementary_stream *st,
		   const uint8_t *data, 
		   int len, int start, int err);

void skip_mpeg_ts(struct service *t, struct elementary_stream *st,
                  const uint8_t *data, int len);

void parser_enqueue_packet(struct service *t, struct elementary_stream *st,
			   th_pkt_t *pkt);

void parser_set_stream_vparam(struct elementary_stream *st, int width, int height,
                              int duration);

extern const unsigned int mpeg2video_framedurations[16];

#endif /* PARSERS_H */
