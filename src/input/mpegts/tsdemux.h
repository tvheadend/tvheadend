/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2007 Andreas Öman
 *
 * tvheadend, MPEG transport stream functions
 */

#ifndef TSDEMUX_H
#define TSDEMUX_H

int ts_resync ( const uint8_t *tsb, int *len, int *idx );

void ts_recv_packet0
  (struct mpegts_service *t, elementary_stream_t *st, const uint8_t *tsb, int len);

int ts_recv_packet1
  (struct mpegts_service *t, uint64_t tspos, uint16_t pid,
   const uint8_t *tsb, int len, int table);

void ts_recv_packet2(struct mpegts_service *t, const uint8_t *tsb, int len);

void ts_skip_packet2(struct mpegts_service *t, const uint8_t *tsb, int len);

void ts_recv_raw
  (struct mpegts_service *t, uint64_t tspos, const uint8_t *tsb, int len);

#endif /* TSDEMUX_H */
