/*
 *  tvheadend, muxing of packets with libavformat
 *  Copyright (C) 2012 John Törnblom
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
 *  along with this program.  If not, see <htmlui://www.gnu.org/licenses/>.
 */


#ifndef LAV_MUXER_H_
#define LAV_MUXER_H_

#include <libavformat/avformat.h>
#include <libavformat/avio.h>

#include "tvheadend.h"
#include "service.h"
#include "channels.h"
#include "epg.h"
#include "muxer.h"

typedef struct lav_muxer {
  // File descriptor for socket
  int lm_fd;

  AVFormatContext *lm_oc;
  AVIOContext *lm_pb;

  // Number of errors encountered
  int lm_errors;

  // Type of muxer
  muxer_container_type_t lm_type;
} lav_muxer_t;


lav_muxer_t* lav_muxer_create(int fd,
			      const struct streaming_start *ss,
			      const channel_t *ch,
			      muxer_container_type_t mc);

int lav_muxer_open(lav_muxer_t *lm);
int lav_muxer_write_pkt(lav_muxer_t *lm, struct th_pkt *pkt);
int lav_muxer_write_meta(lav_muxer_t *lm, epg_broadcast_t *eb);

int lav_muxer_close(lav_muxer_t *lm);
void lav_muxer_destroy(lav_muxer_t *lm);

#endif
