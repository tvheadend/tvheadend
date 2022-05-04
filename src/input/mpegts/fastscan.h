/*
 *  Tvheadend - FastScan support
 *  Copyright (C) 2014 Jaroslav Kysela
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

#ifndef __TVH_DVB_FASTSCAN_H__
#define __TVH_DVB_FASTSCAN_H__

#if ENABLE_MPEGTS_DVB

struct bouquet;

void
dvb_fastscan_each(void *aux, int position, uint32_t frequency, dvb_polarisation_t polarisation,
                  void (*job)(void *aux, struct bouquet *,
                              const char *name, int pid));

#endif

void dvb_fastscan_init ( void );
void dvb_fastscan_done ( void );

#endif /* TVH_DVB_FASTSCAN_H */
