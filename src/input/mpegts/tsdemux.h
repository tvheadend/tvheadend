/*
 *  tvheadend, MPEG transport stream functions
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

#ifndef TSDEMUX_H
#define TSDEMUX_H

int ts_resync ( const uint8_t *tsb, int *len, int *idx );

int ts_recv_packet1
  (struct mpegts_service *t, const uint8_t *tsb, int len, int table);

void ts_recv_packet2(struct mpegts_service *t, const uint8_t *tsb, int len);

void ts_skip_packet2(struct mpegts_service *t, const uint8_t *tsb, int len);

void ts_recv_raw(struct mpegts_service *t, const uint8_t *tsb, int len);

#endif /* TSDEMUX_H */
