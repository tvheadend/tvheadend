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

/* **************************************************************************
 * Typedefs/Forward decls
 * *************************************************************************/

typedef struct epggrab_queued_data  epggrab_queued_data_t;
typedef struct epggrab_module       epggrab_module_t;
typedef struct epggrab_module_int   epggrab_module_int_t;
typedef struct epggrab_module_ext   epggrab_module_ext_t;
typedef struct epggrab_module_ota   epggrab_module_ota_t;
typedef struct epggrab_module_ota_scraper   epggrab_module_ota_scraper_t;
typedef struct epggrab_ota_mux      epggrab_ota_mux_t;
typedef struct epggrab_ota_mux_eit_plist    epggrab_ota_mux_eit_plist_t;
typedef struct epggrab_ota_map      epggrab_ota_map_t;
typedef struct epggrab_ota_svc_link epggrab_ota_svc_link_t;

LIST_HEAD(epggrab_module_list, epggrab_module);
typedef struct epggrab_module_list epggrab_module_list_t;

struct mpegts_mux;
struct channel;

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

TAILQ_HEAD(epggrab_channel_queue, epggrab_channel);

/*
 * Grab channel
 */
typedef struct epggrab_channel
{
  idnode_t                  idnode;
  TAILQ_ENTRY(epggrab_channel) all_link; ///< Global link
  RB_ENTRY(epggrab_channel) link;     ///< Global tree link
  epggrab_module_t          *mod;     ///< Linked module

  int                       updated;  ///< EPG channel was updated
  int                       enabled;  ///< Enabled/disabled
  char                      *id;      ///< Grabber's ID

  char                      *name;    ///< Channel name
  htsmsg_t                  *names;   ///< List of all channel names for grabber's ID
  htsmsg_t                  *newnames;///< List of all channel names for grabber's ID (scan)
  char                      *icon;    ///< Channel icon
  char                      *comment; ///< Channel comment (EPG)
  int64_t                   lcn;      ///< Channel number (split)

  time_t                    laststamp;///< Last update timestamp

  int                       only_one; ///< Map to only one channel (auto)
  idnode_list_head_t        channels; ///< Mapped channels (1 = epggrab channel, 2 = channel)

  int                       update_chicon; ///< Update channel icon
  int                       update_chnum;  ///< Update channel number
  int                       update_chname; ///< Update channel name
} epggrab_channel_t;

/*
 * Access functions
 */
htsmsg_t *epggrab_channel_list      ( int ota );

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
void epggrab_channel_link_delete ( epggrab_channel_t *ec, struct channel *ch, int delconf );
int  epggrab_channel_link        ( epggrab_channel_t *ec, struct channel *ch, void *origin );
int  epggrab_channel_map         ( idnode_t *ec, idnode_t *ch, void *origin );

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
 * Data queue
 */
struct epggrab_queued_data
{
  TAILQ_ENTRY(epggrab_queued_data) eq_link;
  uint32_t                         eq_len;
  uint8_t                          eq_data[0]; ///< Data are allocated at the end of structure
};

/*
 * Grabber base class
 */
struct epggrab_module
{
  idnode_t                     idnode;
  LIST_ENTRY(epggrab_module)   link;      ///< Global list link
  TAILQ_ENTRY(epggrab_module)  qlink;     ///< Queued data link

  enum {
    EPGGRAB_OTA,
    EPGGRAB_INT,
    EPGGRAB_EXT,
  }                            type;      ///< Grabber type
  const char                   *id;       ///< Module identifier
  int                          subsys;    ///< Module log subsystem
  const char                   *saveid;   ///< Module save identifier
  const char                   *name;     ///< Module name (for display)
  int                          enabled;   ///< Whether the module is enabled
  int                          active;    ///< Whether the module is active
  int                          priority;  ///< Priority of the module
  epggrab_channel_tree_t       channels;  ///< Channel list

  TAILQ_HEAD(, epggrab_queued_data) data_queue;

  /* Activate */
  int       (*activate)( void *m, int activate );

  /* Free */
  void      (*done)( void *m );

  /* Process queued data */
  void      (*process_data)( void *m, void *data, uint32_t len );
};

/*
 * Internal grabber
 */
struct epggrab_module_int
{
  epggrab_module_t             ;          ///< Parent object

  const char                   *path;     ///< Path for the command
  const char                   *args;     ///< Extra arguments

  int                           xmltv_chnum;
  int                           xmltv_scrape_extra; ///< Scrape actors and extra details
  int                           xmltv_scrape_onto_desc; ///< Include scraped actors
    ///< and extra details on to programme description for viewing by legacy clients.
  int                           xmltv_use_category_not_genre; ///< Use category tags and don't map to DVB genres.

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
  tvh_uuid_t                     uuid;
  uint64_t                       last_tune_count;
  RB_ENTRY(epggrab_ota_svc_link) link;
};

struct epggrab_ota_mux_eit_plist {
  LIST_ENTRY(epggrab_ota_mux_eit_plist) link;
  const char *src;
  void *priv;
};

/*
 * TODO: this could be embedded in the mux itself, but by using a soft-link
 *       and keeping it here I can somewhat isolate it from the mpegts code
 */
struct epggrab_ota_mux
{
  tvh_uuid_t                         om_mux_uuid;     ///< Soft-link to mux
  LIST_HEAD(,epggrab_ota_map)        om_modules;      ///< List of linked mods

  uint8_t                            om_done;         ///< The full completion mark for this round
  uint8_t                            om_complete;     ///< Has completed a scan
  uint8_t                            om_requeue;      ///< Requeue when stolen
  uint8_t                            om_save;         ///< something changed
  uint8_t                            om_detected;     ///< detected some activity
  mtimer_t                           om_timer;        ///< Per mux active timer
  mtimer_t                           om_data_timer;   ///< Any EPG data seen?
  mtimer_t                           om_handlers_timer; ///< Run handlers callback
  int64_t                            om_retry_time;   ///< Next time to retry

  char                              *om_force_modname;///< Force this module

  enum {
    EPGGRAB_OTA_MUX_IDLE,
    EPGGRAB_OTA_MUX_PENDING,
    EPGGRAB_OTA_MUX_ACTIVE
  }                                  om_q_type;

  TAILQ_ENTRY(epggrab_ota_mux)       om_q_link;
  RB_ENTRY(epggrab_ota_mux)          om_global_link;

  LIST_HEAD(, epggrab_ota_mux_eit_plist) om_eit_plist;
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
  uint64_t                            om_tune_count;
  RB_HEAD(,epggrab_ota_svc_link)      om_svcs;         ///< Services we carry data for
  void                               *om_opaque;
};

/*
 * Over the air grabber
 */
struct epggrab_module_ota
{
  epggrab_module_t               ;      ///< Parent object

  /* Transponder tuning */
  int  (*start) ( epggrab_ota_map_t *map, struct mpegts_mux *mm );
  int  (*stop)  ( epggrab_ota_map_t *map, struct mpegts_mux *mm );
  void (*handlers) (epggrab_ota_map_t *map, struct mpegts_mux *mm );
  int  (*tune)  ( epggrab_ota_map_t *map, epggrab_ota_mux_t *om,
                  struct mpegts_mux *mm );
  void  *opaque;
};

/*
 * Over the air grabber that supports configurable scraping of data.
 */
struct epggrab_module_ota_scraper
{
  epggrab_module_ota_t             ;      ///< Parent object
  char                   *scrape_config;  ///< Config to use or blank/NULL for default.
  int                     scrape_episode; ///< Scrape season/episode from EIT summary
  int                     scrape_title;   ///< Scrape title from EIT title + summary
  int                     scrape_subtitle;///< Scrape subtitle from EIT summary
  int                     scrape_summary; ///< Scrape summary from EIT summary
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
  uint32_t              epgdb_saveafterimport;
  uint32_t              epgdb_processparentallabels;
  char                 *ota_cron;
  char                 *ota_genre_translation;
  uint32_t              ota_timeout;
  uint32_t              ota_initial;
  uint32_t              int_initial;
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
extern const idclass_t epggrab_channel_class;
extern struct epggrab_channel_queue epggrab_channel_entries;

/*
 * Access the Module list
 */
epggrab_module_t* epggrab_module_find_by_id ( const char *id );
const char * epggrab_module_type(epggrab_module_t *mod);
const char * epggrab_module_get_status(epggrab_module_t *mod);

/*
 * Data queue
 */
void epggrab_queue_data(epggrab_module_t *mod,
                        const void *data1, uint32_t len1,
                        const void *data2, uint32_t len2);

/* **************************************************************************
 * Setup/Configuration
 * *************************************************************************/

/*
 * Configuration
 */
extern epggrab_module_list_t epggrab_modules;
extern tvh_mutex_t           epggrab_mutex;
extern int                   epggrab_running;
extern int                   epggrab_ota_running;

/*
 * Set configuration
 */
int epggrab_activate_module            ( epggrab_module_t *mod, int activate );
void epggrab_ota_set_cron              ( void );
void epggrab_ota_set_genre_translation ( void );
void epggrab_ota_trigger               ( int secs );
void epggrab_rerun_internal            ( void );

/*
 * Load/Save
 */
void epggrab_init                 ( void );
void epggrab_done                 ( void );
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
 * OTA kick
 */
void epggrab_ota_queue_mux( struct mpegts_mux *mm );
epggrab_ota_mux_t *epggrab_ota_find_mux ( struct mpegts_mux *mm );
htsmsg_t *epggrab_ota_module_id_list( const char *lang );
const char *epggrab_ota_check_module_id( const char *id );

/*
 * Global variable for genre translation
 */
extern unsigned char                *epggrab_ota_genre_translation;

/*
 * Get the next execution times
 */
time_t epggrab_get_next_int(void);
time_t epggrab_get_next_ota(void);
/*
 * Count active grabbers for a given type
 */
int epggrab_count_type(int grabberType);

#endif /* __EPGGRAB_H__ */

/* **************************************************************************
 * Editor
 *
 * vim:sts=2:ts=2:sw=2:et
 * *************************************************************************/
