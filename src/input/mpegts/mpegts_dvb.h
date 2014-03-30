/*
 *  Tvheadend - MPEGTS DVB support routines and defines
 *  Copyright (C) 2013 Adam Sutton
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

#ifndef __TVH_MPEGTS_DVB_H__
#define __TVH_MPEGTS_DVB_H__

/*
 * Network
 */

typedef struct dvb_network
{
  mpegts_network_t;

  /*
   * Network type
   */
  dvb_fe_type_t ln_type;
} dvb_network_t;

extern const idclass_t dvb_network_dvbt_class;
extern const idclass_t dvb_network_dvbc_class;
extern const idclass_t dvb_network_dvbs_class;
extern const idclass_t dvb_network_atsc_class;

void dvb_network_init ( void );
void dvb_network_done ( void );
dvb_network_t *dvb_network_find_by_uuid(const char *uuid);

dvb_network_t *dvb_network_create0
  ( const char *uuid, const idclass_t *idc, htsmsg_t *conf );

#endif /* __TVH_MPEGTS_DVB_H__ */
