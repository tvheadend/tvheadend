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

#ifndef EPG_H
#define EPG_H

#include "channels.h"
#include "settings.h"

/*
 * Map types
 */
RB_HEAD(epg_brand_tree,     epg_brand);
RB_HEAD(epg_season_tree,    epg_season);
RB_HEAD(epg_episode_tree,   epg_episode);
RB_HEAD(epg_channel_tree,   epg_channel);
RB_HEAD(epg_broadcast_tree, epg_broadcast);

/*
 * Forward declerations
 */
typedef struct epg_brand     epg_brand_t;
typedef struct epg_season    epg_season_t;
typedef struct epg_episode   epg_episode_t;
typedef struct epg_broadcast epg_broadcast_t;
typedef struct epg_channel   epg_channel_t;

/* ************************************************************************
 * Brand - Represents a specific show
 * e.g. The Simpsons, 24, Eastenders, etc...
 * ***********************************************************************/

/* Object */
typedef struct epg_brand
{
  RB_ENTRY(epg_brand)        eb_link;         ///< Global list link

  char                      *eb_uri;          ///< Grabber URI
  char                      *eb_title;        ///< Brand name
  char                      *eb_summary;      ///< Brand summary
  uint16_t                   eb_season_count; ///< Total number of seasons

  struct epg_season_tree     eb_seasons;      ///< Season list
  struct epg_episode_tree    eb_episodes;     ///< Episode list

  int                        eb_refcount;     ///< Reference counting
} epg_brand_t;

/* Lookup */
epg_brand_t *epg_brand_find_by_uri ( const char *uri, int create );

/* Mutators */
int epg_brand_set_title        ( epg_brand_t *b, const char *title )
  __attribute__((warn_unused_result));
int epg_brand_set_summary      ( epg_brand_t *b, const char *summary )
  __attribute__((warn_unused_result));
int epg_brand_set_season_count ( epg_brand_t *b, uint16_t season_count )
  __attribute__((warn_unused_result));
int epg_brand_add_season       ( epg_brand_t *b, epg_season_t *s, int u )
  __attribute__((warn_unused_result));
int epg_brand_rem_season       ( epg_brand_t *b, epg_season_t *s, int u )
  __attribute__((warn_unused_result));
int epg_brand_add_episode      ( epg_brand_t *b, epg_episode_t *s, int u )
  __attribute__((warn_unused_result));
int epg_brand_rem_episode      ( epg_brand_t *b, epg_episode_t *s, int u )
  __attribute__((warn_unused_result));

/* ************************************************************************
 * Season
 * ***********************************************************************/

/* Object */
typedef struct epg_season
{
  RB_ENTRY(epg_season)       es_link;          ///< Global list link
  RB_ENTRY(epg_season)       es_blink;         ///< Brand list link

  char                      *es_uri;           ///< Grabber URI
  char                      *es_summary;       ///< Season summary
  uint16_t                   es_number;        ///< The season number
  uint16_t                   es_episode_count; ///< Total number of episodes

  epg_brand_t               *es_brand;         ///< Parent brand
  struct epg_episode_tree    es_episodes;      ///< Episode list

  int                        es_refcount;      ///< Reference counting

} epg_season_t;

/* Lookup */
epg_season_t *epg_season_find_by_uri ( const char *uri, int create );

/* Mutators */
int epg_season_set_summary       ( epg_season_t *s, const char *summary )
  __attribute__((warn_unused_result));
int epg_season_set_number        ( epg_season_t *s, uint16_t number )
  __attribute__((warn_unused_result));
int epg_season_set_episode_count ( epg_season_t *s, uint16_t episode_count )
  __attribute__((warn_unused_result));
int epg_season_set_brand         ( epg_season_t *s, epg_brand_t *b, int u )
  __attribute__((warn_unused_result));
int epg_season_add_episode       ( epg_season_t *s, epg_episode_t *e, int u )
  __attribute__((warn_unused_result));
int epg_season_rem_episode       ( epg_season_t *s, epg_episode_t *e, int u )
  __attribute__((warn_unused_result));

/* ************************************************************************
 * Episode
 *
 * TODO: ee_genre needs to be a list
 * ***********************************************************************/

/* Object */
typedef struct epg_episode
{
  RB_ENTRY(epg_episode)      ee_link;          ///< Global link
  RB_ENTRY(epg_episode)      ee_blink;         ///< Brand link
  RB_ENTRY(epg_episode)      ee_slink;         ///< Season link

  char                      *ee_uri;           ///< Grabber URI
  char                      *ee_title;         ///< Title
  char                      *ee_subtitle;      ///< Sub-title
  char                      *ee_summary;       ///< Summary
  char                      *ee_description;   ///< An extended description
  uint8_t                    ee_genre;         ///< Episode genre
  uint16_t                   ee_number;        ///< The episode number
  uint16_t                   ee_part_number;   ///< For multipart episodes
  uint16_t                   ee_part_count;    ///< For multipart episodes

  epg_brand_t               *ee_brand;         ///< (Grand-)Parent brand
  epg_season_t              *ee_season;        ///< Parent season
  struct epg_broadcast_tree  ee_broadcasts;    ///< Broadcast list

  int                        ee_refcount;      ///< Reference counting
} epg_episode_t;

/* Lookup */
epg_episode_t *epg_episode_find_by_uri ( const char *uri, int create );

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
int epg_episode_set_brand        ( epg_episode_t *e, epg_brand_t *b, int u )
  __attribute__((warn_unused_result));
int epg_episode_set_season       ( epg_episode_t *e, epg_season_t *s, int u )
  __attribute__((warn_unused_result));
int epg_episode_add_broadcast    ( epg_episode_t *e, epg_broadcast_t *b, int u )
  __attribute__((warn_unused_result));
int epg_episode_rem_broadcast    ( epg_episode_t *e, epg_broadcast_t *b, int u )
  __attribute__((warn_unused_result));

/* Acessors */
int epg_episode_get_number_onscreen ( epg_episode_t *e, char *b, int c );


/* ************************************************************************
 * Broadcast - specific airing (channel & time) of an episode
 * ***********************************************************************/

/* Object */
typedef struct epg_broadcast
{
  RB_ENTRY(epg_broadcast)    eb_slink;         ///< Schedule link
  RB_ENTRY(epg_broadcast)    eb_elink;         ///< Episode link

  int                        eb_id;            ///< Internal ID
  int                        eb_dvb_id;        ///< DVB identifier
  time_t                     eb_start;         ///< Start time
  time_t                     eb_stop;          ///< End time

  /* Some quality info */
  uint8_t                    eb_widescreen;    ///< Is widescreen
  uint8_t                    eb_hd;            ///< Is HD
  uint16_t                   eb_lines;         ///< Lines in image (quality)
  uint16_t                   eb_aspect;        ///< Aspect ratio (*100)

  /* Some accessibility support */
  uint8_t                    eb_deafsigned;    ///< In screen signing
  uint8_t                    eb_subtitled;     ///< Teletext subtitles
  uint8_t                    eb_audio_desc;    ///< Audio description

  /* Misc flags */
  uint8_t                    eb_new;           ///< New series / file premiere
  uint8_t                    eb_repeat;        ///< Repeat screening

  epg_episode_t             *eb_episode;       ///< Episode shown
  epg_channel_t             *eb_channel;       ///< Channel being broadcast on

  int                        eb_refcount;      ///< Reference counting
} epg_broadcast_t;

/* Lookup */
epg_broadcast_t *epg_broadcast_find_by_time ( epg_channel_t *ch, time_t start, time_t stop, int create );
epg_broadcast_t *epg_broadcast_find_by_id   ( int id );

/* Mutators */
int epg_broadcast_set_episode    ( epg_broadcast_t *b, epg_episode_t *e, int u )
  __attribute__((warn_unused_result));

/* ************************************************************************
 * Channel - provides mapping from EPG channels to real channels
 * ***********************************************************************/

/* Object */
typedef struct epg_channel
{
  RB_ENTRY(epg_channel)      ec_link;          ///< Global link

  char                      *ec_uri;           ///< Channel URI
  char                      *ec_name;          ///< Channel name
  char                     **ec_sname;         ///< DVB svc names (to map)
  int                      **ec_sid;           ///< DVB svc ids   (to map)

  channel_t                 *ec_channel;       ///< Link to real channel

  struct epg_broadcast_tree  ec_schedule;      ///< Schedule (broadcasts)
  
} epg_channel_t;

/* Lookup */
epg_channel_t *epg_channel_find_by_uri ( const char *uri, int create );

/* Mutators */
int epg_channel_set_name ( epg_channel_t *c, const char *n )
  __attribute__((warn_unused_result));

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

void epg_query0(epg_query_result_t *eqr, channel_t *ch, channel_tag_t *ct,
                uint8_t type, const char *title);
void epg_query(epg_query_result_t *eqr, const char *channel, const char *tag,
	       const char *contentgroup, const char *title);
void epg_query_free(epg_query_result_t *eqr);
void epg_query_sort(epg_query_result_t *eqr);


/* ************************************************************************
 * Function prototypes
 * ***********************************************************************/

/*
 * Load/Save
 */

void epg_init(void);
void epg_save(void);

void epg_updated           ( void );

/*
 * Simple lookup
 */


epg_broadcast_t *epg_event_find_by_id(int eventid);

/*
 * Advanced Query
 */

/*
 * Genres?
 */
uint8_t epg_content_group_find_by_name(const char *name);

const char *epg_content_group_get_name(uint8_t type);


/* ************************************************************************
 * Compatibility code
 * ***********************************************************************/

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
#if 0

void epg_event_updated(event_t *e);

event_t *epg_event_create(channel_t *ch, time_t start, time_t stop,
			  int dvb_id, int *created);

event_t *epg_event_find_by_time(channel_t *ch, time_t t);

event_t *epg_event_find_by_id(int eventid);

void epg_unlink_from_channel(channel_t *ch);


/**
 *
 */
uint8_t epg_content_group_find_by_name(const char *name);

const char *epg_content_group_get_name(uint8_t type);

/**
 *
 */
typedef struct epg_query_result {
  event_t **eqr_array;
  int eqr_entries;
  int eqr_alloced;
} epg_query_result_t;

void epg_query0(epg_query_result_t *eqr, channel_t *ch, channel_tag_t *ct,
                uint8_t type, const char *title);
void epg_query(epg_query_result_t *eqr, const char *channel, const char *tag,
	       const char *contentgroup, const char *title);
void epg_query_free(epg_query_result_t *eqr);
void epg_query_sort(epg_query_result_t *eqr);
#endif
#endif /* EPG_H */
