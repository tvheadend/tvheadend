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

#include <libhts/htsmsg.h>

void dvb_tda_save(th_dvb_adapter_t *tda);

void dvb_tda_set_displayname(th_dvb_adapter_t *tda, const char *s);

void dvb_tdmi_save(th_dvb_mux_instance_t *tdmi);

const char *dvb_mux_create_str(th_dvb_adapter_t *tda,
			       const char *tsidstr,
			       const char *network,
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

htsmsg_t *dvb_mux_preconf_get_node(int fetype, const char *node);

int dvb_mux_preconf_add_network(th_dvb_adapter_t *tda, const char *id);

void dvb_tdmi_load(th_dvb_adapter_t *tda);

#endif /* DVB_MUXCONFIG_H */
