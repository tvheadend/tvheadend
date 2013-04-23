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

#include "htsmsg.h"

htsmsg_t *dvb_mux_preconf_get_node(int fetype, const char *node);

int dvb_mux_preconf_add_network(dvb_network_t *dn, const char *id);

#endif /* DVB_MUXCONFIG_H */
