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

#include <stdint.h>

struct mpegts_mux;

void t2mi_init ( void );
void t2mi_done ( void );

/* number of T2-MI / TS-piping carrier services currently present on a mux */
int t2mi_mux_count_carriers ( struct mpegts_mux *mm );

/* a PMT on this mux produced the T2-MI carrier component of service 'sid' -
 * reconcile the automatic networks that selected the mux as a source (or
 * own it, when it is a carrier itself), and scan the carrier along with the
 * mux when the mux is being scanned */
void t2mi_carrier_seen ( struct mpegts_mux *mm, uint16_t sid );

/* order of two muxes of one T2-MI network (0 if they are not such muxes):
 * carriers of one transponder next to each other */
int t2mi_mux_compare ( struct mpegts_mux *a, struct mpegts_mux *b );

/* the mux is being deleted - unlink it from the automatic networks that
 * selected it as a source (silently when delconf is 0, i.e. shutdown) */
void t2mi_source_mux_deleting ( struct mpegts_mux *mm, int delconf );

#endif /* __T2MI_H__ */
