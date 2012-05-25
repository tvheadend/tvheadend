/*
 *  Electronic Program Guide - Common functions
 *  Copyright (C) 2007 Andreas Öman
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

/*
 * TODO LIST:
 *
 *   URI in the objects limits us to single grabber, might want something
 *   more flexible to try and merge grabbers? Is that feasible?
 */

#ifndef EPG_H
#define EPG_H

#include "channels.h"
#include "settings.h"

/*
 * Map types
 */
RB_HEAD(epg_object_tree,    epg_object);
RB_HEAD(epg_brand_tree,     epg_brand);
RB_HEAD(epg_season_tree,    epg_season);
RB_HEAD(epg_episode_tree,   epg_episode);
RB_HEAD(epg_channel_tree,   epg_channel);
RB_HEAD(epg_broadcast_tree, epg_broadcast);

/*
 * Typedefs
 */
typedef struct epg_object         epg_object_t;
typedef struct epg_brand          epg_brand_t;
typedef struct epg_season         epg_season_t;
typedef struct epg_episode        epg_episode_t;
typedef struct epg_broadcast      epg_broadcast_t;
typedef struct epg_channel        epg_channel_t;
typedef struct epg_object_tree    epg_object_tree_t;
typedef struct epg_brand_tree     epg_brand_tree_t;
typedef struct epg_season_tree    epg_season_tree_t;
typedef struct epg_episode_tree   epg_episode_tree_t;
typedef struct epg_channel_tree   epg_channel_tree_t;
typedef struct epg_broadcast_tree epg_broadcast_tree_t;

/* ************************************************************************
 * Generic Object
 * ***********************************************************************/

/* Object */
typedef struct epg_object
{
  RB_ENTRY(epg_object)    glink;     ///< Global list link
  LIST_ENTRY(epg_object)  ulink;     ///< Global unref'd link

  char                   *uri;       ///< Unique ID (from grabber)
  uint64_t                id;        ///< Internal ID
  int                     refcount;  ///< Reference counting

  void (*getref)  ( epg_object_t* ); ///< Get a reference
  void (*putref)  ( epg_object_t* ); ///< Release a reference
  void (*destroy) ( epg_object_t* ); ///< Delete the object
} epg_object_t;


/* ************************************************************************
 * Brand - Represents a specific show
 * e.g. The Simpsons, 24, Eastenders, etc...
 * ***********************************************************************/

/* Object */
typedef struct epg_brand
{
  epg_object_t               _; ///< Base object

  char                      *title;        ///< Brand name
  char                      *summary;      ///< Brand summary
  uint16_t                   season_count; ///< Total number of seasons

  epg_season_tree_t          seasons;      ///< Season list
  epg_episode_tree_t         episodes;     ///< Episode list
} epg_brand_t;

/* Lookup */
epg_brand_t *epg_brand_find_by_uri
  ( const char *uri, int create, int *save );
epg_brand_t *epg_brand_find_by_id ( uint64_t id );

/* Mutators */
int epg_brand_set_title        ( epg_brand_t *b, const char *title )
  __attribute__((warn_unused_result));
int epg_brand_set_summary      ( epg_brand_t *b, const char *summary )
  __attribute__((warn_unused_result));
int epg_brand_set_season_count ( epg_brand_t *b, uint16_t season_count )
  __attribute__((warn_unused_result));

/* Serialization */
htsmsg_t    *epg_brand_serialize   ( epg_brand_t *b );
epg_brand_t *epg_brand_deserialize ( htsmsg_t *m, int create, int *save );

/* ************************************************************************
 * Season
 * ***********************************************************************/

/* Object */
typedef struct epg_season
{
  epg_object_t               _;                ///< Parent object

  char                      *summary;       ///< Season summary
  uint16_t                   number;        ///< The season number
  uint16_t                   episode_count; ///< Total number of episodes

  RB_ENTRY(epg_season)       blink;         ///< Brand list link
  epg_brand_t               *brand;         ///< Parent brand
  epg_episode_tree_t         episodes;      ///< Episode list

} epg_season_t;

/* Lookup */
epg_season_t *epg_season_find_by_uri
  ( const char *uri, int create, int *save );
epg_season_t *epg_season_find_by_id ( uint64_t id );

/* Mutators */
int epg_season_set_summary       ( epg_season_t *s, const char *summary )
  __attribute__((warn_unused_result));
int epg_season_set_number        ( epg_season_t *s, uint16_t number )
  __attribute__((warn_unused_result));
int epg_season_set_episode_count ( epg_season_t *s, uint16_t episode_count )
  __attribute__((warn_unused_result));
int epg_season_set_brand         ( epg_season_t *s, epg_brand_t *b, int u )
  __attribute__((warn_unused_result));

/* Serialization */
htsmsg_t    *epg_season_serialize    ( epg_season_t *b );
epg_season_t *epg_season_deserialize ( htsmsg_t *m, int create, int *save );

/* ************************************************************************
 * Episode
 *
 * TODO: ee_genre needs to be a list
 * ***********************************************************************/

/* Object */
typedef struct epg_episode
{
  epg_object_t               _;                ///< Parent object

  char                      *title;         ///< Title
  char                      *subtitle;      ///< Sub-title
  char                      *summary;       ///< Summary
  char                      *description;   ///< An extended description
  uint8_t                    genre;         ///< Episode genre
  uint16_t                   number;        ///< The episode number
  uint16_t                   part_number;   ///< For multipart episodes
  uint16_t                   part_count;    ///< For multipart episodes

  RB_ENTRY(epg_episode)      blink;         ///< Brand link
  RB_ENTRY(epg_episode)      slink;         ///< Season link
  epg_brand_t               *brand;         ///< (Grand-)Parent brand
  epg_season_t              *season;        ///< Parent season
  epg_broadcast_tree_t       broadcasts;    ///< Broadcast list

} epg_episode_t;

/* Lookup */
epg_episode_t *epg_episode_find_by_uri
  ( const char *uri, int create, int *save );
epg_episode_t *epg_episode_find_by_id ( uint64_t id );

/* Mutators */
int epg_episode_set_title        ( epg_episode_t *e, const char *title )
  __attribute__((warn_unused_result));
int epg_episode_set_subtitle     ( epg_episode_t *e, const char *subtitle )
  __attribute__((warn_unused_result));
int epg_episode_set_summary      ( epg_episode_t *e, const char *summary )
  __attribute__((warn_unused_result));
int epg_episode_set_description  ( epg_episode_t *e, const char *description )
  __attribute__((warn_unused_result));
int epg_episode_set_number       ( epg_episode_t *e, uint16_t number )
  __attribute__((warn_unused_result));
int epg_episode_set_part         ( epg_episode_t *e, 
                                   uint16_t number, uint16_t count )
  __attribute__((warn_unused_result));
int epg_episode_set_brand        ( epg_episode_t *e, epg_brand_t *b )
  __attribute__((warn_unused_result));
int epg_episode_set_season       ( epg_episode_t *e, epg_season_t *s )
  __attribute__((warn_unused_result));

/* Acessors */
int epg_episode_get_number_onscreen ( epg_episode_t *e, char *b, int c );

/* Serialization */
htsmsg_t      *epg_episode_serialize   ( epg_episode_t *b );
epg_episode_t *epg_episode_deserialize ( htsmsg_t *m, int create, int *save );

/* ************************************************************************
 * Broadcast - specific airing (channel & time) of an episode
 * ***********************************************************************/

/* Object */
typedef struct epg_broadcast
{
  epg_object_t               _;                ///< Parent object
  
  uint32_t                   dvb_id;           ///< DVB identifier
  time_t                     start;            ///< Start time
  time_t                     stop;             ///< End time

  /* Some quality info */
  uint8_t                    is_widescreen;    ///< Is widescreen
  uint8_t                    is_hd;            ///< Is HD
  uint16_t                   lines;            ///< Lines in image (quality)
  uint16_t                   aspect;           ///< Aspect ratio (*100)

  /* Some accessibility support */
  uint8_t                    is_deafsigned;    ///< In screen signing
  uint8_t                    is_subtitled;     ///< Teletext subtitles
  uint8_t                    is_audio_desc;    ///< Audio description

  /* Misc flags */
  uint8_t                    is_new;           ///< New series / file premiere
  uint8_t                    is_repeat;        ///< Repeat screening

  RB_ENTRY(epg_broadcast)    elink;         ///< Episode link
  epg_episode_t             *episode;       ///< Episode shown
  epg_channel_t             *channel;       ///< Channel being broadcast on

} epg_broadcast_t;

/* Lookup */
epg_broadcast_t *epg_broadcast_find_by_time 
  ( epg_channel_t *ch, time_t start, time_t stop, int create, int *save );
epg_broadcast_t *epg_broadcast_find_by_id ( uint64_t id, epg_channel_t *ch );

/* Mutators */
int epg_broadcast_set_episode    ( epg_broadcast_t *b, epg_episode_t *e )
  __attribute__((warn_unused_result));

/* Accessors */
epg_broadcast_t *epg_broadcast_get_next ( epg_broadcast_t *b );

/* Serialization */
htsmsg_t        *epg_broadcast_serialize   ( epg_broadcast_t *b );
epg_broadcast_t *epg_broadcast_deserialize 
  ( htsmsg_t *m, int create, int *save );

/* ************************************************************************
 * Channel - provides mapping from EPG channels to real channels
 * ***********************************************************************/

/* Object */
typedef struct epg_channel
{
  epg_object_t               _;             ///< Parent object

  char                      *name;          ///< Channel name
  char                     **sname;         ///< DVB svc names (to map)
  int                      **sid;           ///< DVB svc ids   (to map)

  epg_object_tree_t          schedule;      ///< List of broadcasts
  LIST_ENTRY(epg_channel)    umlink;        ///< Unmapped channel link
  channel_t                 *channel;       ///< Link to real channel

  epg_broadcast_t           *now;           ///< Current broadcast
  epg_broadcast_t           *next;          ///< Next broadcast

  gtimer_t                   expire;        ///< Expiration timer
} epg_channel_t;

/* Lookup */
epg_channel_t *epg_channel_find_by_uri
  ( const char *uri, int create, int *save );
epg_channel_t *epg_channel_find_by_id ( uint64_t id );

/* Mutators */
int epg_channel_set_name ( epg_channel_t *c, const char *n )
  __attribute__((warn_unused_result));
int epg_channel_set_channel ( epg_channel_t *c, channel_t *ch );

/* Accessors */
epg_broadcast_t *epg_channel_get_broadcast
  ( epg_channel_t *ch, time_t start, time_t stop, int create, int *save );

/* Serialization */
htsmsg_t      *epg_channel_serialize   ( epg_channel_t *b );
epg_channel_t *epg_channel_deserialize ( htsmsg_t *m, int create, int *save );

/* Channel mapping */
void epg_channel_map_add ( channel_t *ch );
void epg_channel_map_rem ( channel_t *ch );
void epg_channel_map_mod ( channel_t *ch );

/* ************************************************************************
 * Querying
 * ***********************************************************************/

/*
 * Query result
 */
typedef struct epg_query_result {
  epg_broadcast_t **eqr_array;
  int               eqr_entries;
  int               eqr_alloced;
} epg_query_result_t;

void epg_query_free(epg_query_result_t *eqr);

/* Sorting */
// TODO: comparator function input?
void epg_query_sort(epg_query_result_t *eqr);

/* Query routines */
void epg_query0(epg_query_result_t *eqr, channel_t *ch, channel_tag_t *ct,
                uint8_t type, const char *title);
void epg_query(epg_query_result_t *eqr, const char *channel, const char *tag,
	       const char *contentgroup, const char *title);


/* ************************************************************************
 * Setup/Shutdown
 * ***********************************************************************/

void epg_init    (void);
void epg_save    (void);
void epg_updated (void);

/* ************************************************************************
 * Compatibility code
 * ***********************************************************************/

#ifndef TODO_REMOVE_EPG_COMPAT 

typedef struct _epg_episode {

  uint16_t ee_season;
  uint16_t ee_episode;
  uint16_t ee_part;

  char *ee_onscreen;
} _epg_episode_t;


/*
 * EPG event
 */
typedef struct event {
  LIST_ENTRY(event) e_global_link;

  struct channel *e_channel;
  RB_ENTRY(event) e_channel_link;

  int e_refcount;
  uint32_t e_id;

  uint8_t e_content_type;

  time_t e_start;  /* UTC time */
  time_t e_stop;   /* UTC time */

  char *e_title;   /* UTF-8 encoded */
  char *e_desc;    /* UTF-8 encoded */
  char *e_ext_desc;/* UTF-8 encoded (from extended descriptor) */
  char *e_ext_item;/* UTF-8 encoded (from extended descriptor) */
  char *e_ext_text;/* UTF-8 encoded (from extended descriptor) */

  int e_dvb_id;

  _epg_episode_t e_episode;

} event_t;
#endif
#endif /* EPG_H */
