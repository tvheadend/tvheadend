/*
 *  tvheadend, MPEG transport stream muxer
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

#ifndef TSMUX_H
#define TSMUX_H

typedef void (ts_mux_output_t)(void *opaque, th_subscription_t *s, 
			       uint8_t *pkt, int npackets, int64_t pcr_ref);

typedef struct ts_muxer {
  th_subscription_t *ts_subscription;
  int ts_flags;
#define TS_SEEK 0x1
#define TS_HTSCLIENT 0x2

  th_muxer_t *ts_muxer;
  ts_mux_output_t *ts_output;
  void *ts_output_opaque;

  int ts_pat_cc;
  int ts_pmt_cc;
  dtimer_t ts_patpmt_timer;

  uint8_t *ts_packet;
  int ts_block;
  int ts_blocks_per_packet;

  th_muxstream_t *ts_pcr_stream;

  int64_t ts_pcr_start;
  int64_t ts_pcr_ref;  /* System clock when PCR was/is/will be 0 */
  int64_t ts_pcr_last;

} ts_muxer_t;




ts_muxer_t *ts_muxer_init(th_subscription_t *s, ts_mux_output_t *output,
			  void *opaque, int flags);

void ts_muxer_deinit(ts_muxer_t *ts, th_subscription_t *s);

void ts_muxer_play(ts_muxer_t *ts, int64_t toffset);

void ts_muxer_pause(ts_muxer_t *ts);

#endif /* TSMUX_H */
