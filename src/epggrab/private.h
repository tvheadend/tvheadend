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
  ( epggrab_module_t *skel, const idclass_t *cls,
    const char *id, const char *saveid,
    const char *name, int priority );

char     *epggrab_module_grab_spawn ( void *m );
htsmsg_t *epggrab_module_trans_xml  ( void *m, char *data );

void      epggrab_module_ch_add  ( void *m, struct channel *ch );
void      epggrab_module_ch_rem  ( void *m, struct channel *ch );
void      epggrab_module_ch_mod  ( void *m, struct channel *ch );
void      epggrab_module_ch_save ( void *m, epggrab_channel_t *ec );

void      epggrab_module_parse ( void *m, htsmsg_t *data );

void      epggrab_module_channels_load ( const char *modid );

/* **************************************************************************
 * Channel processing
 * *************************************************************************/

int  epggrab_channel_match_epgid ( epggrab_channel_t *ec, struct channel *ch );
int  epggrab_channel_match_name ( epggrab_channel_t *ec, struct channel *ch );
int  epggrab_channel_match_number ( epggrab_channel_t *ec, struct channel *ch );

epggrab_channel_t *epggrab_channel_create
  ( epggrab_module_t *owner, htsmsg_t *conf, const char *uuid );

epggrab_channel_t *epggrab_channel_find
  ( epggrab_module_t *mod, const char *id, int create, int *save );

void epggrab_channel_save ( epggrab_channel_t *ec );
void epggrab_channel_destroy
  ( epggrab_channel_t *ec, int delconf, int rb_remove );
void epggrab_channel_flush
  ( epggrab_module_t *mod, int delconf );
void epggrab_channel_begin_scan
  ( epggrab_module_t *mod );
void epggrab_channel_end_scan
  ( epggrab_module_t *mod );

void epggrab_channel_init(void);
void epggrab_channel_done(void);

/* **************************************************************************
 * Internal module routines
 * *************************************************************************/

epggrab_module_int_t *epggrab_module_int_create
  ( epggrab_module_int_t *skel, const idclass_t *cls,
    const char *id, const char *saveid,
    const char *name, int priority,
    const char *path,
    char* (*grab) (void*m),
    int (*parse) (void *m, htsmsg_t *data, epggrab_stats_t *sta),
    htsmsg_t* (*trans) (void *mod, char *data) );

/* **************************************************************************
 * External module routines
 * *************************************************************************/

epggrab_module_ext_t *epggrab_module_ext_create
  ( epggrab_module_ext_t *skel,
    const char *id, const char *saveid,
    const char *name, int priority,
    const char *sockid,
    int (*parse) (void *m, htsmsg_t *data, epggrab_stats_t *sta),
    htsmsg_t* (*trans) (void *mod, char *data) );

/* **************************************************************************
 * OTA module routines
 * *************************************************************************/

typedef struct epggrab_ota_module_ops {
    int (*start)     (epggrab_ota_map_t *map, struct mpegts_mux *mm);
    int  (*activate) (void *m, int e);
    void (*done)     (void *m);
    int  (*tune)     (epggrab_ota_map_t *map, epggrab_ota_mux_t *om,
                      struct mpegts_mux *mm);
} epggrab_ota_module_ops_t;

epggrab_module_ota_t *epggrab_module_ota_create
  ( epggrab_module_ota_t *skel,
    const char *id, const char *saveid,
    const char *name, int priority,
    epggrab_ota_module_ops_t *ops );

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
  ( epggrab_module_ota_t *mod, epggrab_ota_mux_t *ota,
    struct mpegts_mux *mux );

/*
 * State change
 */
void epggrab_ota_complete 
  ( epggrab_module_ota_t *mod, epggrab_ota_mux_t *ota );

/*
 * Service list
 */
void
epggrab_ota_service_add
  ( epggrab_ota_map_t *map, epggrab_ota_mux_t *ota,
    const char *uuid, int save );
void
epggrab_ota_service_del
  ( epggrab_ota_map_t *map, epggrab_ota_mux_t *ota,
    epggrab_ota_svc_link_t *svcl, int save );

/* **************************************************************************
 * Miscellaneous
 * *************************************************************************/

/* Note: this is reused by pyepg since they share a common format */
int  xmltv_parse_accessibility
  ( epg_broadcast_t *ebc, htsmsg_t *m, uint32_t *changes );

/* Freesat huffman decoder */
size_t freesat_huffman_decode
  ( char *dst, size_t* dstlen, const uint8_t *src, size_t srclen );

/* **************************************************************************
 * Classes
 * *************************************************************************/

extern const idclass_t epggrab_mod_class;
extern const idclass_t epggrab_mod_int_class;
extern const idclass_t epggrab_mod_ext_class;
extern const idclass_t epggrab_mod_ota_class;

/* **************************************************************************
 * Module setup(s)
 * *************************************************************************/

/* EIT module */
void eit_init    ( void );
void eit_done    ( void );
void eit_load    ( void );

/* OpenTV module */
void opentv_init ( void );
void opentv_done ( void );
void opentv_load ( void );

/* PyEPG module */
void pyepg_init  ( void );
void pyepg_done  ( void );
void pyepg_load  ( void );

/* XMLTV module */
void xmltv_init  ( void );
void xmltv_done  ( void );
void xmltv_load  ( void );

/* PSIP module */
void psip_init  ( void );
void psip_done  ( void );
void psip_load  ( void );

#endif /* __EPGGRAB_PRIVATE_H__ */
