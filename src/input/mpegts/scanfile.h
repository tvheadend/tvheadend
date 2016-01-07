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

#ifndef __DVB_SCANFILES_H__
#define __DVB_SCANFILES_H__

typedef struct scanfile_network {
  const char                  *sfn_id;
  const char                  *sfn_name;
  int                          sfn_satpos;
  LIST_ENTRY(scanfile_network) sfn_link;
  LIST_HEAD(,dvb_mux_conf)     sfn_muxes;
} scanfile_network_t;

typedef struct scanfile_region {
  const char                  *sfr_id;
  const char                  *sfr_name;
  LIST_ENTRY(scanfile_region)  sfr_link;
  LIST_HEAD(,scanfile_network) sfr_networks;
} scanfile_region_t;

typedef LIST_HEAD(,scanfile_region) scanfile_region_list_t;
extern scanfile_region_list_t scanfile_regions_DVBC;
extern scanfile_region_list_t scanfile_regions_DVBT;
extern scanfile_region_list_t scanfile_regions_DVBS;
extern scanfile_region_list_t scanfile_regions_ATSC_T;
extern scanfile_region_list_t scanfile_regions_ATSC_C;
extern scanfile_region_list_t scanfile_regions_ISDB_T;

void scanfile_init ( void );
void scanfile_done ( void );

scanfile_network_t *scanfile_find ( const char *id );
  
#endif /* __DVB_SCANFILES_H__ */
