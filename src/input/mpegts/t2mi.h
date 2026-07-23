/*
 *  Tvheadend - T2-MI / TS piping decapsulation input
 *
 *  Copyright (C) 2026 Tvheadend Project
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

#ifndef __T2MI_H__
#define __T2MI_H__

struct mpegts_mux;

void t2mi_init ( void );
void t2mi_done ( void );

/* a PMT on this mux produced a T2-MI carrier component - reconcile the
 * automatic networks that selected the mux as a source */
void t2mi_carrier_seen ( struct mpegts_mux *mm );

/* the mux is being deleted - unlink it from the automatic networks that
 * selected it as a source (silently when delconf is 0, i.e. shutdown) */
void t2mi_source_mux_deleting ( struct mpegts_mux *mm, int delconf );

#endif /* __T2MI_H__ */
