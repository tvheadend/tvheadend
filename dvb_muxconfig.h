/*
 *  TV headend - Code for configuring DVB muxes
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

#ifndef DVB_MUXCONFIG_H_
#define DVB_MUXCONFIG_H_

void dvb_mux_store(FILE *fp, th_dvb_mux_instance_t *tdmi);

const char *dvb_mux_create_str(th_dvb_adapter_t *tda,
			       const char *tsidstr,
			       const char *freqstr,
			       const char *symratestr,
			       const char *qamstr,
			       const char *fecstr,
			       const char *fechistr,
			       const char *feclostr,
			       const char *bwstr,
			       const char *tmodestr,
			       const char *guardstr,
			       const char *hierstr,
			       const char *polstr,
			       const char *switchportstr,
			       int save);

#endif /* DVB_MUXCONFIG_H */
