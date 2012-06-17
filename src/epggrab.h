#ifndef __EPGGRAB_H__
#define __EPGGRAB_H__

#include <pthread.h>
#include "channels.h"
#include "dvb/dvb.h"

typedef struct epggrab_module epggrab_module_t;

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
  RB_ENTRY(epggrab_channel)  link;    ///< Global link
  epggrab_module_t          *mod;     ///< Linked module

  char                      *id;      ///< Grabber's ID
  char                      *name;    ///< Channel name
  char                      **sname;  ///< Service name's
  uint16_t                  *sid;     ///< Service ID's
  int                       sid_cnt;  ///< Number of service IDs
  
  char                      *icon;    ///< Channel icon
  int                       number;   ///< Channel number

  // TODO: I think we might need a list of channels!
  struct channel            *channel; ///< Mapped channel
} epggrab_channel_t;

/*
 * Channel list structure
 */
RB_HEAD(epggrab_channel_tree, epggrab_channel);
typedef struct epggrab_channel_tree epggrab_channel_tree_t;

/*
 * Access functions
 */
htsmsg_t*         epggrab_channel_list      ( void );

/*
 * Mutators
 */
int epggrab_channel_set_name   ( epggrab_channel_t *ch, const char *name );
int epggrab_channel_set_sid    ( epggrab_channel_t *ch, const uint16_t *sid, int num );
int epggrab_channel_set_sname  ( epggrab_channel_t *ch, const char **sname );
int epggrab_channel_set_icon   ( epggrab_channel_t *ch, const char *icon );
int epggrab_channel_set_number ( epggrab_channel_t *ch, int number );

void epggrab_channel_updated ( epggrab_channel_t *ch );
void epggrab_channel_link ( epggrab_channel_t *ch );

/*
 * Match the channel
 */

/* **************************************************************************
 * Grabber Modules
 * *************************************************************************/

/*
 * Grabber flags
 */
#define EPGGRAB_MODULE_SIMPLE   0x01
#define EPGGRAB_MODULE_EXTERNAL 0x02
#define EPGGRAB_MODULE_SPECIAL  0x04

/*
 * Grabber base class
 */
struct epggrab_module
{
  LIST_ENTRY(epggrab_module) link;      ///< Global list link

  const char                 *id;       ///< Module identifier
  const char                 *name;     ///< Module name (for display)
  const char                 *path;     ///< Module path (for fixed config)
  const uint8_t              flags;     ///< Mode flag
  uint8_t                    enabled;   ///< Whether the module is enabled
  int                        sock;      ///< Socket descriptor
  epggrab_channel_tree_t     *channels; ///< Channel list

  /* Enable/Disable */
  int       (*enable) ( epggrab_module_t *m, uint8_t e );

  /* Grab/Translate/Parse */
  char*     (*grab)   ( epggrab_module_t *m );
  htsmsg_t* (*trans)  ( epggrab_module_t *m, char *d );
  int       (*parse)  ( epggrab_module_t *m,
                        htsmsg_t *d, epggrab_stats_t *s );

  /* Channel listings */
  void      (*ch_add)  ( epggrab_module_t *m, channel_t *ch );
  void      (*ch_rem)  ( epggrab_module_t *m, channel_t *ch );
  void      (*ch_mod)  ( epggrab_module_t *m, channel_t *ch );

  /* Transponder tuning */
  void      (*tune)    ( epggrab_module_t *m, th_dvb_mux_instance_t *tdmi );
};

/*
 * Default module functions
 */

int        epggrab_module_enable_socket ( epggrab_module_t *m, uint8_t e );
const char *epggrab_module_socket_path  ( const char *id );

char     *epggrab_module_grab       ( epggrab_module_t *m );
htsmsg_t *epggrab_module_trans_xml  ( epggrab_module_t *m, char *d );

void epggrab_module_channel_save    
  ( epggrab_module_t *m, epggrab_channel_t *ch );
void epggrab_module_channels_load   ( epggrab_module_t *m );
void epggrab_module_channel_add     ( epggrab_module_t *m, channel_t *ch );
void epggrab_module_channel_rem     ( epggrab_module_t *m, channel_t *ch );
void epggrab_module_channel_mod     ( epggrab_module_t *m, channel_t *ch );

epggrab_channel_t *epggrab_module_channel_find
  ( epggrab_module_t *mod, const char *id, int create, int *save );

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
 * Configuration
 */
extern pthread_mutex_t      epggrab_mutex;
extern uint32_t             epggrab_eitenabled;
extern uint32_t             epggrab_interval;
extern epggrab_module_t*    epggrab_module;

/*
 * Update
 */
int  epggrab_set_eitenabled      ( uint32_t eitenabled );
int  epggrab_set_interval        ( uint32_t interval );
int  epggrab_set_module          ( epggrab_module_t *module );
int  epggrab_set_module_by_id    ( const char *id );
int  epggrab_enable_module       ( epggrab_module_t *module, uint8_t e );
int  epggrab_enable_module_by_id ( const char *id, uint8_t e );
void epggrab_save                ( void );

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

/*
 * Transport handling
 */
void epggrab_tune ( th_dvb_mux_instance_t *tdmi );

#endif /* __EPGGRAB_H__ */
