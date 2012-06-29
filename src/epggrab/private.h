/*
 *  EPG Grabber - private routines
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

#ifndef __EPGGRAB_PRIVATE_H__
#define __EPGGRAB_PRIVATE_H__

/* **************************************************************************
 * Generic module routines
 * *************************************************************************/

epggrab_module_t *epggrab_module_create
  ( epggrab_module_t *skel,
    const char *id, const char *name, epggrab_channel_tree_t *channels );

char     *epggrab_module_grab_spawn ( void *m );
htsmsg_t *epggrab_module_trans_xml  ( void *m, char *data );

void      epggrab_module_ch_add  ( void *m, struct channel *ch );
void      epggrab_module_ch_rem  ( void *m, struct channel *ch );
void      epggrab_module_ch_mod  ( void *m, struct channel *ch );
void      epggrab_module_ch_save ( void *m, epggrab_channel_t *ec );

int       epggrab_module_enable_socket ( void *m, uint8_t e );

void      epggrab_module_parse ( void *m, htsmsg_t *data );

void      epggrab_module_channels_load ( epggrab_module_t *m );

/* **************************************************************************
 * Channel processing
 * *************************************************************************/

int epggrab_channel_link ( epggrab_channel_t *ec, struct channel *ch );

epggrab_channel_t *epggrab_channel_find
  ( epggrab_channel_tree_t *chs, const char *id, int create, int *save );

/* **************************************************************************
 * Internal module routines
 * *************************************************************************/

epggrab_module_int_t *epggrab_module_int_create
  ( epggrab_module_int_t *skel,
    const char *id, const char *name, const char *path,
    char* (*grab) (void*m),
    int (*parse) (void *m, htsmsg_t *data, epggrab_stats_t *sta),
    htsmsg_t* (*trans) (void *mod, char *data),
    epggrab_channel_tree_t *channels );

/* **************************************************************************
 * External module routines
 * *************************************************************************/

epggrab_module_ext_t *epggrab_module_ext_create
  ( epggrab_module_ext_t *skel,
    const char *id, const char *name, const char *sockid,
    int (*parse) (void *m, htsmsg_t *data, epggrab_stats_t *sta),
    htsmsg_t* (*trans) (void *mod, char *data),
    epggrab_channel_tree_t *channels );

/* **************************************************************************
 * OTA module routines
 * *************************************************************************/

epggrab_module_ota_t *epggrab_module_ota_create
  ( epggrab_module_ota_t *skel,
    const char *id, const char *name,
    void (*start) (epggrab_module_ota_t*m,
                   struct th_dvb_mux_instance *tdmi),
    int (*enable) (void *m, uint8_t e ),
    epggrab_channel_tree_t *channels );

/* **************************************************************************
 * OTA mux link routines
 * *************************************************************************/

/*
 * Create/Find a link (unregistered)
 *
 * Note: this will return NULL for an already existing link that is currently
 *       blocked (i.e. has completed within interval period)
 */
epggrab_ota_mux_t *epggrab_ota_create
  ( epggrab_module_ota_t *mod, struct th_dvb_mux_instance *tdmi );
void epggrab_ota_create_and_register_by_id
  ( epggrab_module_ota_t *mod, int nid, int tsid,
    int period, int interval );

/*
 * Delete
 */
void epggrab_ota_destroy           ( epggrab_ota_mux_t *ota );
void epggrab_ota_destroy_by_module ( epggrab_module_ota_t *mod );
void epggrab_ota_destroy_by_tdmi   ( struct th_dvb_mux_instance *tdmi );

/*
 * Register interest
 */
void epggrab_ota_register   
  ( epggrab_ota_mux_t *ota, int timeout, int interval );

/*
 * State change
 */
int  epggrab_ota_begin       ( epggrab_ota_mux_t *ota );
void epggrab_ota_complete    ( epggrab_ota_mux_t *ota );
void epggrab_ota_cancel      ( epggrab_ota_mux_t *ota );
void epggrab_ota_timeout     ( epggrab_ota_mux_t *ota );

/*
 * Status
 */
int  epggrab_ota_is_complete ( epggrab_ota_mux_t *ota );
int  epggrab_ota_is_blocked  ( epggrab_ota_mux_t *ota );

/* **************************************************************************
 * Module setup(s)
 * *************************************************************************/

/* EIT module */
void eit_init    ( void );
void eit_load    ( void );

/* OpenTV module */
void opentv_init ( void );
void opentv_load ( void );

/* PyEPG module */
void pyepg_init  ( void );
void pyepg_load  ( void );

/* XMLTV module */
void xmltv_init  ( void );
void xmltv_load  ( void );

/* Note: this is reused by pyepg since they share a common format */
int  xmltv_parse_accessibility ( epg_broadcast_t *ebc, htsmsg_t *m );

#endif /* __EPGGRAB_PRIVATE_H__ */
