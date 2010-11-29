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

void ts_recv_packet1(struct service *t, const uint8_t *tsb, int64_t *pcrp);

void ts_recv_packet2(struct service *t, const uint8_t *tsb);

#endif /* TSDEMUX_H */
