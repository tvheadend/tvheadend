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

struct mpegts_mux;

/* **************************************************************************
 * Generic module routines
 * *************************************************************************/

epggrab_module_t *epggrab_module_create
  ( epggrab_module_t *skel,
    const char *id, const char *name, int priority,
    epggrab_channel_tree_t *channels );

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

int  epggrab_channel_match ( epggrab_channel_t *ec, struct channel *ch );
int  epggrab_channel_match_and_link
  ( epggrab_channel_t *ec, struct channel *ch );

epggrab_channel_t *epggrab_channel_find
  ( epggrab_channel_tree_t *chs, const char *id, int create, int *save,
    epggrab_module_t *owner );

/* **************************************************************************
 * Internal module routines
 * *************************************************************************/

epggrab_module_int_t *epggrab_module_int_create
  ( epggrab_module_int_t *skel,
    const char *id, const char *name, int priority,
    const char *path,
    char* (*grab) (void*m),
    int (*parse) (void *m, htsmsg_t *data, epggrab_stats_t *sta),
    htsmsg_t* (*trans) (void *mod, char *data),
    epggrab_channel_tree_t *channels );

/* **************************************************************************
 * External module routines
 * *************************************************************************/

epggrab_module_ext_t *epggrab_module_ext_create
  ( epggrab_module_ext_t *skel,
    const char *id, const char *name, int priority,
    const char *sockid,
    int (*parse) (void *m, htsmsg_t *data, epggrab_stats_t *sta),
    htsmsg_t* (*trans) (void *mod, char *data),
    epggrab_channel_tree_t *channels );

/* **************************************************************************
 * OTA module routines
 * *************************************************************************/

epggrab_module_ota_t *epggrab_module_ota_create
  ( epggrab_module_ota_t *skel,
    const char *id, const char *name, int priority,
    void (*start) (epggrab_module_ota_t*m,
                   struct mpegts_mux *mm),
    int (*enable) (void *m, uint8_t e ),
    void (*done) (epggrab_module_ota_t*m),
    epggrab_channel_tree_t *channels );

/* **************************************************************************
 * OTA mux link routines
 * *************************************************************************/

/*
 * Config handling
 */
void epggrab_ota_init ( void );

/*
 * Create/Find a link (unregistered)
 *
 * Note: this will return NULL for an already existing link that is currently
 *       blocked (i.e. has completed within interval period)
 */
epggrab_ota_mux_t *epggrab_ota_find
  ( epggrab_module_ota_t *mod, struct mpegts_mux *dm );
epggrab_ota_mux_t *epggrab_ota_create
  ( epggrab_module_ota_t *mod, struct mpegts_mux *dm );
void epggrab_ota_create_and_register_by_id
  ( epggrab_module_ota_t *mod, uint16_t onid, uint16_t tsid,
    int period, int interval, const char *name );

/*
 * Delete
 */
void epggrab_ota_destroy           ( epggrab_ota_mux_t *ota );
void epggrab_ota_destroy_by_module ( epggrab_module_ota_t *mod );
#if 0
void epggrab_ota_destroy_by_dm     ( struct dvb_mux *dm );
#endif

/*
 * In module functions
 */

epggrab_ota_mux_t *epggrab_ota_register   
  ( epggrab_module_ota_t *mod, struct mpegts_mux *mux,
    int timeout, int interval );

/*
 * State change
 */
void epggrab_ota_complete 
  ( epggrab_module_ota_t *mod, epggrab_ota_mux_t *ota );


/* **************************************************************************
 * Miscellaneous
 * *************************************************************************/

/* Note: this is reused by pyepg since they share a common format */
int  xmltv_parse_accessibility
  ( epggrab_module_t *mod, epg_broadcast_t *ebc, htsmsg_t *m );

/* Freesat huffman decoder */
size_t freesat_huffman_decode
  (char *dst, size_t* dstlen, const uint8_t *src, size_t srclen);

/* **************************************************************************
 * Module setup(s)
 * *************************************************************************/

/* EIT module */
void eit_init    ( void );
void eit_load    ( void );

/* OpenTV module */
void opentv_init ( void );
void opentv_done ( void );
void opentv_load ( void );

/* PyEPG module */
void pyepg_init  ( void );
void pyepg_load  ( void );

/* XMLTV module */
void xmltv_init  ( void );
void xmltv_load  ( void );

#endif /* __EPGGRAB_PRIVATE_H__ */
