#ifndef __EPGGRAB_H__
#define __EPGGRAB_H__

#include <pthread.h>
#include "cron.h"
#include "channels.h"

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
 * Grab channel
 */
typedef struct epggrab_channel
{
  RB_ENTRY(epggrab_channel)  link;   ///< Global link

  char                      *id;      ///< Grabber's ID
  char                      *name;    ///< Channel name
  struct channel            *channel; ///< Mapped channel
  // TODO: I think we might need a list of channels!
} epggrab_channel_t;

/*
 * Channel list
 */
RB_HEAD(epggrab_channel_tree, epggrab_channel);
typedef struct epggrab_channel_tree epggrab_channel_tree_t;

/* **************************************************************************
 * Grabber Modules
 * *************************************************************************/

typedef struct epggrab_module epggrab_module_t;

/*
 * Grabber base class
 */
struct epggrab_module
{
  LIST_ENTRY(epggrab_module) link;      ///< Global list link

  const char                 *id;       ///< Module identifier
  const char                 *name;     ///< Module name (for display)
  const char                 *path;     ///< Module path (for fixed config)
  const uint8_t              simple;    ///< Include in simple list
  const uint8_t              async;     ///< Asynchronous (not schedulable)
  epggrab_channel_tree_t     *channels; ///< Channel list

  /* Enable/Disable for async */
  void      (*enable) ( epggrab_module_t *m, uint8_t e );

  /* Synchronous grab&parse */
  htsmsg_t* (*grab)   ( epggrab_module_t *m,
                        const char *c, const char *o );
  int       (*parse)  ( epggrab_module_t *m,
                        htsmsg_t *d, epggrab_stats_t *s );

  /* Channel listings */
  int       (*ch_add)  ( epggrab_module_t *m, channel_t *ch );
  int       (*ch_rem)  ( epggrab_module_t *m, channel_t *ch );
  int       (*ch_mod)  ( epggrab_module_t *m, channel_t *ch );

  /* Save any settings */
  void      (*ch_save) ( epggrab_module_t *m );
};

/*
 * Default channel handlers
 */
void epggrab_module_channels_load ( epggrab_module_t *m );
void epggrab_module_channels_save ( epggrab_module_t *m );
int  epggrab_module_channel_add   ( epggrab_module_t *m, channel_t *ch );
int  epggrab_module_channel_rem   ( epggrab_module_t *m, channel_t *ch );
int  epggrab_module_channel_mod   ( epggrab_module_t *m, channel_t *ch );
epggrab_channel_t *epggrab_module_channel_create
  ( epggrab_module_t *m );
epggrab_channel_t *epggrab_module_channel_find
  ( epggrab_module_t *m, const char *id );

/*
 * Module list
 */
LIST_HEAD(epggrab_module_list, epggrab_module);
typedef struct epggrab_module_list epggrab_module_list_t;

/*
 * Access the Module list
 */
epggrab_module_t* epggrab_module_find_by_id ( const char *id );
htsmsg_t*         epggrab_module_list       ( void );

/* **************************************************************************
 * Configuration
 * *************************************************************************/

/*
 * Schedule specification (for advanced config)
 */
typedef struct epggrab_sched
{ 
  LIST_ENTRY(epggrab_sched) link;
  cron_t                    *cron;  ///< Cron definition
  epggrab_module_t          *mod;   ///< Module
  char                      *cmd;   ///< Command
  char                      *opts;  ///< Extra (advanced) options
} epggrab_sched_t;

/*
 * Schedule list
 */
LIST_HEAD(epggrab_sched_list, epggrab_sched);
typedef struct epggrab_sched_list epggrab_sched_list_t;

/*
 * Configuration
 */
extern pthread_mutex_t      epggrab_mutex;
extern uint32_t             epggrab_advanced;
extern uint32_t             epggrab_eitenabled;
extern uint32_t             epggrab_interval;
extern epggrab_module_t*    epggrab_module;
extern epggrab_sched_list_t epggrab_schedule;

htsmsg_t *epggrab_get_schedule ( void );

/*
 * Update
 */
int  epggrab_set_advanced     ( uint32_t advanced );
int  epggrab_set_eitenabled   ( uint32_t eitenabled );
int  epggrab_set_interval     ( uint32_t interval );
int  epggrab_set_module       ( epggrab_module_t *module );
int  epggrab_set_module_by_id ( const char *id );
int  epggrab_set_schedule     ( htsmsg_t *sched );
void epggrab_save             ( void );

/* **************************************************************************
 * Global Functions
 * *************************************************************************/

void epggrab_init ( void );

/*
 * Channel handling
 */
void epggrab_channel_add ( struct channel *ch );
void epggrab_channel_rem ( struct channel *ch );
void epggrab_channel_mod ( struct channel *ch );

#endif /* __EPGGRAB_H__ */
