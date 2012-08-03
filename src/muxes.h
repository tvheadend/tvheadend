/*
 *  tvheadend, intial mux list
 *  Copyright (C) 2012 Adam Sutton
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

#ifndef __TVH_MUXES_H__
#define __TVH_MUXES_H__

typedef struct mux {
  LIST_ENTRY(mux) link;
 unsigned int freq;
 unsigned int symrate;
 char fec;
 char constellation;
 char bw;
 char fechp;
 char feclp;
 char tmode;
 char guard;
 char hierarchy;
 char polarisation;
} mux_t;

typedef struct network {
const char *id;
const char *name;
LIST_ENTRY(network) link;
LIST_HEAD(,mux) muxes;
} network_t;

typedef struct region {
const char *id;
const char *name;
LIST_ENTRY(region) link;
LIST_HEAD(,network) networks;
} region_t;

typedef LIST_HEAD(,region) region_list_t;
extern region_list_t regions_DVBC;
extern region_list_t regions_DVBT;
extern region_list_t regions_DVBS;
extern region_list_t regions_ATSC;

void muxes_init ( void );
  
#endif /* __TVH_MUXES_H__ */
