/*
 *  EPG Grabber - common functions
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

#ifndef __EPGGRAB_H__
#define __EPGGRAB_H__

#include "idnode.h"

#include <pthread.h>

/* **************************************************************************
 * Typedefs/Forward decls
 * *************************************************************************/

typedef struct epggrab_module       epggrab_module_t;
typedef struct epggrab_module_int   epggrab_module_int_t;
typedef struct epggrab_module_ext   epggrab_module_ext_t;
typedef struct epggrab_module_ota   epggrab_module_ota_t;
typedef struct epggrab_ota_mux      epggrab_ota_mux_t;
typedef struct epggrab_ota_map      epggrab_ota_map_t;
typedef struct epggrab_ota_svc_link epggrab_ota_svc_link_t;

LIST_HEAD(epggrab_module_list, epggrab_module);
typedef struct epggrab_module_list epggrab_module_list_t;

struct mpegts_mux;

/* **************************************************************************
 * Grabber Stats
 * *************************************************************************/

typedef struct epggrab_stats_part
{
  int created;
  int modified;
  int total;
} epggrab_stats_part_t;

typedef struct epggrab_stats
{
  epggrab_stats_part_t channels;
  epggrab_stats_part_t brands;
  epggrab_stats_part_t seasons;
  epggrab_stats_part_t episodes;
  epggrab_stats_part_t broadcasts;
  epggrab_stats_part_t config;
} epggrab_stats_t;

/* **************************************************************************
 * Grabber Channels
 * *************************************************************************/

/*
 * Lists
 */
RB_HEAD(epggrab_channel_tree, epggrab_channel);
typedef struct epggrab_channel_tree epggrab_channel_tree_t;

/*
 * Grab channel
 */
typedef struct epggrab_channel
{
  RB_ENTRY(epggrab_channel) link;     ///< Global link
  epggrab_module_t          *mod;     ///< Linked module

  char                      *id;      ///< Grabber's ID

  char                      *name;    ///< Channel name
  char                      *icon;    ///< Channel icon
  int                       major;    ///< Channel major number
  int                       minor;    ///< Channel minor number

  LIST_HEAD(,epggrab_channel_link) channels; ///< Mapped channels
} epggrab_channel_t;

typedef struct epggrab_channel_link
{
  int                               ecl_mark;
  struct channel                    *ecl_channel;
  struct epggrab_channel            *ecl_epggrab;
  LIST_ENTRY(epggrab_channel_link)  ecl_chn_link;
  LIST_ENTRY(epggrab_channel_link)  ecl_epg_link;
} epggrab_channel_link_t;

/*
 * Access functions
 */
htsmsg_t*         epggrab_channel_list      ( int ota );

/*
 * Mutators
 */
int epggrab_channel_set_name     ( epggrab_channel_t *ch, const char *name );
int epggrab_channel_set_icon     ( epggrab_channel_t *ch, const char *icon );
int epggrab_channel_set_number   ( epggrab_channel_t *ch, int major, int minor );

/*
 * Updated/link
 */
void epggrab_channel_updated     ( epggrab_channel_t *ch );
void epggrab_channel_link_delete ( epggrab_channel_link_t *ecl, int delconf );
int  epggrab_channel_link        ( epggrab_channel_t *ec, struct channel *ch );

/* ID */
const char *epggrab_channel_get_id ( epggrab_channel_t *ch );
epggrab_channel_t *epggrab_channel_find_by_id ( const char *id );

/*
 * Check type
 */
int epggrab_channel_is_ota ( epggrab_channel_t *ec );

/* **************************************************************************
 * Grabber Modules
 * *************************************************************************/

/*
 * Grabber base class
 */
struct epggrab_module
{
  idnode_t                     idnode;
  LIST_ENTRY(epggrab_module)   link;      ///< Global list link

  enum {
    EPGGRAB_OTA,
    EPGGRAB_INT,
    EPGGRAB_EXT,
  }                            type;      ///< Grabber type
  const char                   *id;       ///< Module identifier
  const char                   *name;     ///< Module name (for display)
  int                          enabled;   ///< Whether the module is enabled
  int                          active;    ///< Whether the module is active
  int                          priority;  ///< Priority of the module
  epggrab_channel_tree_t       *channels; ///< Channel list

  /* Activate */
  int       (*activate) ( void *m, int activate );

  /* Free */
  void      (*done)    ( void *m );

  /* Channel listings */
  void      (*ch_add)  ( void *m, struct channel *ch );
  void      (*ch_rem)  ( void *m, struct channel *ch );
  void      (*ch_mod)  ( void *m, struct channel *ch );
  void      (*ch_save) ( void *m, epggrab_channel_t *ch );
};

/*
 * Internal grabber
 */
struct epggrab_module_int
{
  epggrab_module_t             ;          ///< Parent object

  const char                   *path;     ///< Path for the command

  /* Handle data */
  char*     (*grab)   ( void *mod );
  htsmsg_t* (*trans)  ( void *mod, char *data );
  int       (*parse)  ( void *mod, htsmsg_t *data, epggrab_stats_t *stat );
};

/*
 * External grabber
 */
struct epggrab_module_ext
{
  epggrab_module_int_t         ;          ///< Parent object
  
  int                          sock;      ///< Socket descriptor

  pthread_t                    tid;       ///< Thread ID
};

struct epggrab_ota_svc_link
{
  char                          *uuid;
  uint64_t                       last_tune_count;
  RB_ENTRY(epggrab_ota_svc_link) link;
};

/*
 * TODO: this could be embedded in the mux itself, but by using a soft-link
 *       and keeping it here I can somewhat isolate it from the mpegts code
 */
struct epggrab_ota_mux
{
  char                              *om_mux_uuid;     ///< Soft-link to mux
  LIST_HEAD(,epggrab_ota_map)        om_modules;      ///< List of linked mods
  
  uint8_t                            om_done;         ///< The full completion mark for this round
  uint8_t                            om_complete;     ///< Has completed a scan
  uint8_t                            om_requeue;      ///< Requeue when stolen
  uint8_t                            om_save;         ///< something changed
  gtimer_t                           om_timer;        ///< Per mux active timer
  gtimer_t                           om_data_timer;   ///< Any EPG data seen?

  char                              *om_force_modname;///< Force this module

  enum {
    EPGGRAB_OTA_MUX_IDLE,
    EPGGRAB_OTA_MUX_PENDING,
    EPGGRAB_OTA_MUX_ACTIVE
  }                                  om_q_type;

  TAILQ_ENTRY(epggrab_ota_mux)       om_q_link;
  RB_ENTRY(epggrab_ota_mux)          om_global_link;
};

/*
 * Link between ota_mux and ota_module
 */
struct epggrab_ota_map
{
  LIST_ENTRY(epggrab_ota_map)         om_link;
  epggrab_module_ota_t               *om_module;
  int                                 om_complete;
  uint8_t                             om_first;
  uint8_t                             om_forced;
  uint64_t                            om_tune_count;
  RB_HEAD(,epggrab_ota_svc_link)      om_svcs;         ///< Muxes we carry data for
};

/*
 * Over the air grabber
 */
struct epggrab_module_ota
{
  epggrab_module_t               ;      ///< Parent object

  //TAILQ_HEAD(, epggrab_ota_mux)  muxes; ///< List of related muxes

  /* Transponder tuning */
  int  (*start) ( epggrab_ota_map_t *map, struct mpegts_mux *mm );
  int  (*tune)  ( epggrab_ota_map_t *map, epggrab_ota_mux_t *om,
                  struct mpegts_mux *mm );
};

/*
 *
 */
typedef struct epggrab_conf {
  idnode_t              idnode;
  char                 *cron;
  uint32_t              channel_rename;
  uint32_t              channel_renumber;
  uint32_t              channel_reicon;
  uint32_t              epgdb_periodicsave;
  char                 *ota_cron;
  uint32_t              ota_timeout;
  uint32_t              ota_initial;
} epggrab_conf_t;

/*
 *
 */
extern epggrab_conf_t epggrab_conf;
extern const idclass_t epggrab_class;
extern const idclass_t epggrab_class_mod;
extern const idclass_t epggrab_class_mod_int;
extern const idclass_t epggrab_class_mod_ext;
extern const idclass_t epggrab_class_mod_ota;

/*
 * Access the Module list
 */
epggrab_module_t* epggrab_module_find_by_id ( const char *id );
const char * epggrab_module_type(epggrab_module_t *mod);
const char * epggrab_module_get_status(epggrab_module_t *mod);

/* **************************************************************************
 * Setup/Configuration
 * *************************************************************************/

/*
 * Configuration
 */
extern epggrab_module_list_t epggrab_modules;
extern pthread_mutex_t       epggrab_mutex;
extern int                   epggrab_running;
extern int                   epggrab_ota_running;

/*
 * Set configuration
 */
int epggrab_activate_module       ( epggrab_module_t *mod, int activate );
void epggrab_ota_set_cron         ( void );
void epggrab_ota_trigger          ( int secs );

/*
 * Load/Save
 */
void epggrab_init                 ( void );
void epggrab_done                 ( void );
void epggrab_save                 ( void );
void epggrab_ota_init             ( void );
void epggrab_ota_post             ( void );
void epggrab_ota_shutdown         ( void );

/* **************************************************************************
 * Global Functions
 * *************************************************************************/

/*
 * Channel handling
 */
void epggrab_channel_add ( struct channel *ch );
void epggrab_channel_rem ( struct channel *ch );
void epggrab_channel_mod ( struct channel *ch );

/*
 * Re-schedule
 */
void epggrab_resched     ( void );

/*
 * OTA kick
 */
void epggrab_ota_queue_mux( struct mpegts_mux *mm );

#endif /* __EPGGRAB_H__ */

/* **************************************************************************
 * Editor
 *
 * vim:sts=2:ts=2:sw=2:et
 * *************************************************************************/
