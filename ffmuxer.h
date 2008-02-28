/*
 *  FFmpeg based muxer output
 *  Copyright (C) 2008 Andreas Öman
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

#ifndef FFMUXER_H
#define FFMUXER_H

#include <libavformat/avformat.h>

TAILQ_HEAD(tffm_fifo_pkt_queue, tffm_fifo_pkt);


typedef struct tffm_fifo_pkt {
  TAILQ_ENTRY(tffm_fifo_pkt) tfp_link;

  size_t tfp_pktsize;
  uint8_t tfp_buf[0];
} tffm_fifo_pkt_t;

typedef void (tffm_fifo_callback_t)(void *opaque);

typedef struct tffm_fifo {
  LIST_ENTRY(tffm_fifo) tf_link;

  uint32_t tf_id;
  char *tf_filename;
  
  size_t tf_pktq_len;
  struct tffm_fifo_pkt_queue tf_pktq;

  tffm_fifo_callback_t *tf_callback;
  void *tf_opaque;

} tffm_fifo_t;

void tffm_init(void);

void tffm_set_state(th_ffmuxer_t *tffm, int status);
void tffm_record_packet(th_ffmuxer_t *tffm, th_pkt_t *pkt);
void tffm_open(th_ffmuxer_t *tffm, th_transport_t *t,
	       AVFormatContext *fctx, const char *printname);
void tffm_close(th_ffmuxer_t *tffm);
void tffm_packet_input(th_muxer_t *tm, th_stream_t *st, th_pkt_t *pkt);

tffm_fifo_t *tffm_fifo_create(tffm_fifo_callback_t *callback, void *opaque);
void tffm_fifo_destroy(tffm_fifo_t *tf);

#define tffm_filename(tffm) ((tffm)->tf_filename)

#endif /* FFMUXER_H */
