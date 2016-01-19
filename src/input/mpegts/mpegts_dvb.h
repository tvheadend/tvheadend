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

typedef struct dvb_network
{
  mpegts_network_t;

  /*
   * Network type
   */
  dvb_fe_type_t ln_type;
} dvb_network_t;

typedef struct dvb_mux
{
  mpegts_mux_t;

  /*
   * Tuning information
   */
  dvb_mux_conf_t lm_tuning;
} dvb_mux_t;

/*
 * Network
 */

extern const idclass_t dvb_network_class;
extern const idclass_t dvb_network_dvbt_class;
extern const idclass_t dvb_network_dvbc_class;
extern const idclass_t dvb_network_dvbs_class;
extern const idclass_t dvb_network_atsc_t_class;
extern const idclass_t dvb_network_atsc_c_class;
extern const idclass_t dvb_network_isdb_t_class;
extern const idclass_t dvb_network_isdb_c_class;
extern const idclass_t dvb_network_isdb_s_class;
extern const idclass_t dvb_network_dab_class;

void dvb_network_init ( void );
void dvb_network_done ( void );

static inline dvb_network_t *dvb_network_find_by_uuid(const char *uuid)
  { return idnode_find(uuid, &dvb_network_class, NULL); }

const idclass_t *dvb_network_class_by_fe_type(dvb_fe_type_t type);
dvb_fe_type_t dvb_fe_type_by_network_class(const idclass_t *idc);

idnode_set_t *dvb_network_list_by_fe_type(dvb_fe_type_t type);

dvb_network_t *dvb_network_create0
  ( const char *uuid, const idclass_t *idc, htsmsg_t *conf );

dvb_mux_t *dvb_network_find_mux
  ( dvb_network_t *ln, dvb_mux_conf_t *dmc, uint16_t onid, uint16_t tsid );

const idclass_t *dvb_network_mux_class(mpegts_network_t *mn);
int dvb_network_get_orbital_pos(mpegts_network_t *mn);

void dvb_network_scanfile_set ( dvb_network_t *ln, const char *id );

htsmsg_t * dvb_network_class_scanfile_list ( void *o, const char *lang );

/*
 *
 */
extern const idclass_t dvb_mux_dvbt_class;
extern const idclass_t dvb_mux_dvbc_class;
extern const idclass_t dvb_mux_dvbs_class;
extern const idclass_t dvb_mux_atsc_t_class;
extern const idclass_t dvb_mux_atsc_c_class;
extern const idclass_t dvb_mux_isdb_t_class;
extern const idclass_t dvb_mux_isdb_c_class;
extern const idclass_t dvb_mux_isdb_s_class;
extern const idclass_t dvb_mux_dab_class;

dvb_mux_t *dvb_mux_create0
  (dvb_network_t *ln, uint16_t onid, uint16_t tsid,
   const dvb_mux_conf_t *dmc, const char *uuid, htsmsg_t *conf);

#define dvb_mux_create1(n, u, c)\
  dvb_mux_create0(n, MPEGTS_ONID_NONE, MPEGTS_TSID_NONE, NULL, u, c)

/*
 *
 */
int64_t mpegts_service_channel_number ( service_t *s );

#endif /* __TVH_MPEGTS_DVB_H__ */
