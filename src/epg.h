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

#include "settings.h"

/*
 * Forward decls
 */
struct channel;
struct channel_tag;

/*
 * Map types
 */
LIST_HEAD(epg_object_list,  epg_object);
RB_HEAD(epg_object_tree,    epg_object);
RB_HEAD(epg_brand_tree,     epg_brand);
RB_HEAD(epg_season_tree,    epg_season);
RB_HEAD(epg_episode_tree,   epg_episode);
RB_HEAD(epg_broadcast_tree, epg_broadcast);

/*
 * Typedefs
 */
typedef struct epg_object         epg_object_t;
typedef struct epg_brand          epg_brand_t;
typedef struct epg_season         epg_season_t;
typedef struct epg_episode        epg_episode_t;
typedef struct epg_broadcast      epg_broadcast_t;
typedef struct epg_object_list    epg_object_list_t;
typedef struct epg_object_tree    epg_object_tree_t;
typedef struct epg_brand_tree     epg_brand_tree_t;
typedef struct epg_season_tree    epg_season_tree_t;
typedef struct epg_episode_tree   epg_episode_tree_t;
typedef struct epg_broadcast_tree epg_broadcast_tree_t;

/* ************************************************************************
 * Generic Object
 * ***********************************************************************/

/* Object type */
typedef enum epg_object_type
{
  EPG_UNDEF,
  EPG_BRAND,
  EPG_SEASON,
  EPG_EPISODE,
  EPG_BROADCAST
} epg_object_type_t;

/* Object */
struct epg_object
{
  RB_ENTRY(epg_object)    glink;     ///< Global (URI) list link
  LIST_ENTRY(epg_object)  hlink;     ///< Global (ID) hash link
  LIST_ENTRY(epg_object)  ulink;     ///< Global unref'd link
  LIST_ENTRY(epg_object)  uplink;    ///< Global updated link

  epg_object_type_t       type;      ///< Specific object type
  uint64_t                id;        ///< Internal ID
  char                   *uri;       ///< Unique ID (from grabber)
  int                     refcount;  ///< Reference counting
  int                     _updated;  ///< Flag to indicate updated
  // Note: could use LIST_ENTRY field to determine this!

  void (*getref)  ( epg_object_t* ); ///< Get a reference
  void (*putref)  ( epg_object_t* ); ///< Release a reference
  void (*destroy) ( epg_object_t* ); ///< Delete the object
  void (*updated) ( epg_object_t* ); ///< Updated
};

/* ************************************************************************
 * Brand - Represents a specific show
 * e.g. The Simpsons, 24, Eastenders, etc...
 * ***********************************************************************/

/* Object */
struct epg_brand
{
  epg_object_t               _; ///< Base object

  char                      *title;        ///< Brand name
  char                      *summary;      ///< Brand summary
  uint16_t                   season_count; ///< Total number of seasons
  char                      *image;        ///< Brand image

  epg_season_tree_t          seasons;      ///< Season list
  epg_episode_tree_t         episodes;     ///< Episode list
};

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
int epg_brand_set_image        ( epg_brand_t *b, const char *i )
  __attribute__((warn_unused_result));

/* Serialization */
htsmsg_t    *epg_brand_serialize   ( epg_brand_t *b );
epg_brand_t *epg_brand_deserialize ( htsmsg_t *m, int create, int *save );

/* List all brands (serialized) */
htsmsg_t    *epg_brand_list ( void );

/* ************************************************************************
 * Season
 * ***********************************************************************/

/* Object */
struct epg_season
{
  epg_object_t               _;                ///< Parent object

  char                      *summary;       ///< Season summary
  uint16_t                   number;        ///< The season number
  uint16_t                   episode_count; ///< Total number of episodes
  char                      *image;         ///< Season image

  RB_ENTRY(epg_season)       blink;         ///< Brand list link
  epg_brand_t               *brand;         ///< Parent brand
  epg_episode_tree_t         episodes;      ///< Episode list

};

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
int epg_season_set_image         ( epg_season_t *s, const char *image )
  __attribute__((warn_unused_result));

/* Serialization */
htsmsg_t    *epg_season_serialize    ( epg_season_t *b );
epg_season_t *epg_season_deserialize ( htsmsg_t *m, int create, int *save );

/* ************************************************************************
 * Episode
 * ***********************************************************************/

/* Object */
struct epg_episode
{
  epg_object_t               _;                ///< Parent object

  char                      *title;         ///< Title
  char                      *subtitle;      ///< Sub-title
  char                      *summary;       ///< Summary
  char                      *description;   ///< An extended description
  uint8_t                   *genre;         ///< Episode genre(s)
  int                        genre_cnt;     ///< Genre count
  uint16_t                   number;        ///< The episode number
  uint16_t                   part_number;   ///< For multipart episodes
  uint16_t                   part_count;    ///< For multipart episodes
  char                      *image;         ///< Episode image

  RB_ENTRY(epg_episode)      blink;         ///< Brand link
  RB_ENTRY(epg_episode)      slink;         ///< Season link
  epg_brand_t               *brand;         ///< (Grand-)Parent brand
  epg_season_t              *season;        ///< Parent season
  epg_broadcast_tree_t       broadcasts;    ///< Broadcast list

};

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
int epg_episode_set_onscreen     ( epg_episode_t *e, const char *onscreen )
  __attribute__((warn_unused_result));
int epg_episode_set_part         ( epg_episode_t *e, 
                                   uint16_t number, uint16_t count )
  __attribute__((warn_unused_result));
int epg_episode_set_brand        ( epg_episode_t *e, epg_brand_t *b )
  __attribute__((warn_unused_result));
int epg_episode_set_season       ( epg_episode_t *e, epg_season_t *s )
  __attribute__((warn_unused_result));
int epg_episode_set_genre        ( epg_episode_t *e, const uint8_t *g, int c )
  __attribute__((warn_unused_result));
int epg_episode_set_genre_str    ( epg_episode_t *e, const char **s )
  __attribute__((warn_unused_result));
int epg_episode_set_image        ( epg_episode_t *e, const char *i )
  __attribute__((warn_unused_result));

/* EpNum format helper */
// output string will be:
// if (episode_num) 
//   ret = pre
//   if (season_num) ret += sprintf(sfmt, season_num)
//   if (season_cnt && cnt) ret += sprintf(cnt, season_cnt)
//   ret += sep
//   ret += sprintf(efmt, episode_num)
//   if (episode_cnt) ret += sprintf(cfmt, episode_cnt)
// and will return num chars written
size_t epg_episode_number_format 
  ( epg_episode_t *e, char *buf, size_t len,
    const char *pre,  const char *sfmt,
    const char *sep,  const char *efmt,
    const char *cfmt );

/* Matching */
int epg_episode_fuzzy_match
  ( epg_episode_t *ee, const char *uri, const char *title,
    const char *summary, const char *description );

/* Serialization */
htsmsg_t      *epg_episode_serialize   ( epg_episode_t *b );
epg_episode_t *epg_episode_deserialize ( htsmsg_t *m, int create, int *save );

/* ************************************************************************
 * Broadcast - specific airing (channel & time) of an episode
 * ***********************************************************************/

/* Object */
struct epg_broadcast
{
  epg_object_t               _;                ///< Parent object
  
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

  RB_ENTRY(epg_broadcast)    elink;            ///< Episode link
  epg_episode_t             *episode;          ///< Episode shown
  struct channel            *channel;          ///< Channel being broadcast on

};

/* Lookup */
epg_broadcast_t *epg_broadcast_find_by_time 
  ( struct channel *ch, time_t start, time_t stop, int create, int *save );
epg_broadcast_t *epg_broadcast_find_by_id ( uint64_t id, struct channel *ch );

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

/* Accessors */
epg_broadcast_t *epg_channel_get_broadcast
  ( struct channel *ch, time_t start, time_t stop, int create, int *save );

/* Unlink */
void epg_channel_unlink ( struct channel *ch );

/* ************************************************************************
 * Genre
 * ***********************************************************************/

uint8_t     epg_genre_find_by_name ( const char *name );
const char *epg_genre_get_name     ( uint8_t genre, int full );

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
void epg_query0(epg_query_result_t *eqr, struct channel *ch,
                struct channel_tag *ct, uint8_t type, const char *title);
void epg_query(epg_query_result_t *eqr, const char *channel, const char *tag,
	       const char *contentgroup, const char *title);


/* ************************************************************************
 * Setup/Shutdown
 * ***********************************************************************/

void epg_init    (void);
void epg_save    (void);
void epg_updated (void);

#endif /* EPG_H */
