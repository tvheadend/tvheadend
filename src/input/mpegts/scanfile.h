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
  const char                  *sfn_path;
  const char                  *sfn_type;
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

typedef struct scanfile_region_list {
  LIST_HEAD(,scanfile_region)  srl_regions;
  const char                  *srl_type;
  const char                  *srl_alt_type;
} scanfile_region_list_t;

void scanfile_init ( const char *muxconf_path, int lock );
void scanfile_done ( void );

scanfile_region_list_t *scanfile_find_region_list ( const char *type );
scanfile_network_t *scanfile_find ( const char *id );
void scanfile_clean( scanfile_network_t *sfn );
  
#endif /* __DVB_SCANFILES_H__ */
