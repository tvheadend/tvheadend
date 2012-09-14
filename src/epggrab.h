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

#include <pthread.h>

/* **************************************************************************
 * Typedefs/Forward decls
 * *************************************************************************/

struct th_dvb_mux_instance;
struct th_dvb_adapter;

typedef struct epggrab_module       epggrab_module_t;
typedef struct epggrab_module_int   epggrab_module_int_t;
typedef struct epggrab_module_ext   epggrab_module_ext_t;
typedef struct epggrab_module_ota   epggrab_module_ota_t;
typedef struct epggrab_ota_mux      epggrab_ota_mux_t;

LIST_HEAD(epggrab_module_list, epggrab_module);
typedef struct epggrab_module_list epggrab_module_list_t;

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
  int                       number;   ///< Channel number
  
  LIST_HEAD(,epggrab_channel_link) channels; ///< Mapped channels
} epggrab_channel_t;

typedef struct epggrab_channel_link
{
  LIST_ENTRY(epggrab_channel_link) link;     ///< Link to grab channel
  struct channel                   *channel; ///< Real channel
} epggrab_channel_link_t;

/*
 * Access functions
 */
htsmsg_t*         epggrab_channel_list      ( void );

/*
 * Mutators
 */
int epggrab_channel_set_name     ( epggrab_channel_t *ch, const char *name );
int epggrab_channel_set_icon     ( epggrab_channel_t *ch, const char *icon );
int epggrab_channel_set_number   ( epggrab_channel_t *ch, int number );

/*
 * Updated/link
 */
void epggrab_channel_updated   ( epggrab_channel_t *ch );

/* **************************************************************************
 * Grabber Modules
 * *************************************************************************/

/*
 * Grabber base class
 */
struct epggrab_module
{
  LIST_ENTRY(epggrab_module)   link;      ///< Global list link

  enum {
    EPGGRAB_INT,
    EPGGRAB_EXT,
    EPGGRAB_OTA
  }                            type;      ///< Grabber type
  const char                   *id;       ///< Module identifier
  const char                   *name;     ///< Module name (for display)
  uint8_t                      enabled;   ///< Whether the module is enabled
  int                          priority;  ///< Priority of the module
  epggrab_channel_tree_t       *channels; ///< Channel list

  /* Enable/Disable */
  int       (*enable)  ( void *m, uint8_t e );

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
};

/*
 * OTA / mux link
 */
struct epggrab_ota_mux
{
  TAILQ_ENTRY(epggrab_ota_mux)       glob_link; ///< Grabber link
  TAILQ_ENTRY(epggrab_ota_mux)       tdmi_link; ///< Link to mux
  TAILQ_ENTRY(epggrab_ota_mux)       grab_link; ///< Link to grabber
  struct th_dvb_mux_instance        *tdmi;     ///< Mux  instance
  epggrab_module_ota_t              *grab;     ///< Grab instance

  int                               timeout;   ///< Time out if this long
  int                               interval;  ///< Re-grab this often

  int                               is_reg;    ///< Permanently registered

  void                              *status;   ///< Status information
  enum {
    EPGGRAB_OTA_MUX_IDLE,
    EPGGRAB_OTA_MUX_RUNNING,
    EPGGRAB_OTA_MUX_TIMEDOUT,
    EPGGRAB_OTA_MUX_COMPLETE
  }                                 state;     ///< Current state
  time_t                            started;   ///< Time of last start
  time_t                            completed; ///< Time of last completion

  void (*destroy) (epggrab_ota_mux_t *ota);    ///< (Custom) destroy
};

/*
 * Over the air grabber
 */
struct epggrab_module_ota
{
  epggrab_module_t               ;      ///< Parent object

  TAILQ_HEAD(, epggrab_ota_mux)  muxes; ///< List of related muxes

  /* Transponder tuning */
  void (*start) ( epggrab_module_ota_t *m, struct th_dvb_mux_instance *tdmi );
};

/*
 * Access the Module list
 */
epggrab_module_t* epggrab_module_find_by_id ( const char *id );
htsmsg_t*         epggrab_module_list       ( void );

/* **************************************************************************
 * Setup/Configuration
 * *************************************************************************/

/*
 * Configuration
 */
extern epggrab_module_list_t epggrab_modules;
extern pthread_mutex_t       epggrab_mutex;
extern uint32_t              epggrab_interval;
extern epggrab_module_int_t* epggrab_module;
extern uint32_t              epggrab_channel_rename;
extern uint32_t              epggrab_channel_renumber;
extern uint32_t              epggrab_channel_reicon;

/*
 * Set configuration
 */
int  epggrab_set_interval         ( uint32_t interval );
int  epggrab_set_module           ( epggrab_module_t *mod );
int  epggrab_set_module_by_id     ( const char *id );
int  epggrab_set_channel_rename   ( uint32_t e );
int  epggrab_set_channel_renumber ( uint32_t e );
int  epggrab_set_channel_reicon   ( uint32_t e );
int  epggrab_enable_module        ( epggrab_module_t *mod, uint8_t e );
int  epggrab_enable_module_by_id  ( const char *id, uint8_t e );

/*
 * Load/Save
 */
void epggrab_init                 ( void );
void epggrab_save                 ( void );

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
 * Transport handling
 */
void epggrab_mux_start  ( struct th_dvb_mux_instance *tdmi );
void epggrab_mux_stop   ( struct th_dvb_mux_instance *tdmi, int timeout );
void epggrab_mux_delete ( struct th_dvb_mux_instance *tdmi );
int  epggrab_mux_period ( struct th_dvb_mux_instance *tdmi );
struct th_dvb_mux_instance *epggrab_mux_next ( struct th_dvb_adapter *tda );

/*
 * Re-schedule
 */
void epggrab_resched     ( void );

#endif /* __EPGGRAB_H__ */

/* **************************************************************************
 * Editor
 *
 * vim:sts=2:ts=2:sw=2:et
 * *************************************************************************/
