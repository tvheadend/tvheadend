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

#ifndef TS_H
#define TS_H

void ts_recv_tsb(th_transport_t *t, int pid, uint8_t *tsb, 
			int scanpcr, int64_t pcr);

th_pid_t *ts_add_pid(th_transport_t *t, uint16_t pid, tv_streamtype_t type);

#endif /* TS_H */
