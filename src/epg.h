/*
 *  Electronic Program Guide - Common functions
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

#ifndef EPG_H
#define EPG_H

#include <regex.h>
#include "settings.h"
#include "lang_str.h"
#include "access.h"

/*
 * External forward decls
 */
struct channel;
struct channel_tag;
struct epggrab_module;

/*
 * Map/List types
 */
typedef LIST_HEAD(,epg_object)     epg_object_list_t;
typedef RB_HEAD  (,epg_object)     epg_object_tree_t;
typedef LIST_HEAD(,epg_brand)      epg_brand_list_t;
typedef LIST_HEAD(,epg_season)     epg_season_list_t;
typedef LIST_HEAD(,epg_episode)    epg_episode_list_t;
typedef LIST_HEAD(,epg_broadcast)  epg_broadcast_list_t;
typedef RB_HEAD  (,epg_broadcast)  epg_broadcast_tree_t;
typedef LIST_HEAD(,epg_genre)      epg_genre_list_t;

/*
 * Typedefs (most are redundant!)
 */
typedef struct epg_genre           epg_genre_t;
typedef struct epg_object          epg_object_t;
typedef struct epg_brand           epg_brand_t;
typedef struct epg_season          epg_season_t;
typedef struct epg_episode         epg_episode_t;
typedef struct epg_broadcast       epg_broadcast_t;
typedef struct epg_serieslink      epg_serieslink_t;

extern int epg_in_load;

/*
 *
 */
typedef enum {
  EPG_SOURCE_NONE   = 0,
  EPG_SOURCE_EIT    = 1,
} epg_source_t;

typedef enum {
  EPG_RUNNING_STOP  = 0,
  EPG_RUNNING_WARM  = 1,
  EPG_RUNNING_NOW   = 2,
  EPG_RUNNING_PAUSE = 3,
} epg_running_t;

/* ************************************************************************
 * Genres
 * ***********************************************************************/

/* Genre object */
struct epg_genre
{
  LIST_ENTRY(epg_genre) link;
  uint8_t               code;
};

/* Accessors */
uint8_t epg_genre_get_eit ( const epg_genre_t *genre );
size_t  epg_genre_get_str ( const epg_genre_t *genre, int major_only,
                            int major_prefix, char *buf, size_t len,
                            const char *lang );

/* Delete */
void epg_genre_list_destroy   ( epg_genre_list_t *list );

/* Add to list */
int epg_genre_list_add        ( epg_genre_list_t *list, epg_genre_t *genre );
int epg_genre_list_add_by_eit ( epg_genre_list_t *list, uint8_t eit );
int epg_genre_list_add_by_str ( epg_genre_list_t *list, const char *str, const char *lang );

/* Search */
int epg_genre_list_contains
  ( epg_genre_list_t *list, epg_genre_t *genre, int partial );

/* List all available genres */
htsmsg_t *epg_genres_list_all ( int major_only, int major_prefix, const char *lang );

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
  EPG_BROADCAST,
  EPG_SERIESLINK
} epg_object_type_t;
#define EPG_TYPEMAX EPG_SERIESLINK

/* Change flags - shared */
#define EPG_CHANGED_CREATE        (1<<0)
#define EPG_CHANGED_TITLE         (1<<1)
#define EPG_CHANGED_SUBTITLE      (1<<2)
#define EPG_CHANGED_SUMMARY       (1<<3)
#define EPG_CHANGED_DESCRIPTION   (1<<4)
#define EPG_CHANGED_IMAGE         (1<<5)
#define EPG_CHANGED_SLAST         2

typedef struct epg_object_ops {
  void (*getref)  ( void *o );        ///< Get a reference
  int  (*putref)  ( void *o ); 	      ///< Release a reference
  void (*destroy) ( void *o );        ///< Delete the object
  void (*update)  ( void *o );        ///< Updated
} epg_object_ops_t;

/* Object */
struct epg_object
{
  RB_ENTRY(epg_object)    uri_link;   ///< Global URI link
  RB_ENTRY(epg_object)    id_link;    ///< Global (ID) link
  LIST_ENTRY(epg_object)  un_link;    ///< Global unref'd link
  LIST_ENTRY(epg_object)  up_link;    ///< Global updated link
 
  epg_object_type_t       type;       ///< Specific object type
  uint32_t                id;         ///< Internal ID
  char                   *uri;        ///< Unique ID (from grabber)
  time_t                  updated;    ///< Last time object was changed

  uint8_t                 _updated;   ///< Flag to indicate updated
  uint8_t                 _created;   ///< Flag to indicate creation
  int                     refcount;   ///< Reference counting
  // Note: could use LIST_ENTRY field to determine this!

  struct epggrab_module  *grabber;    ///< Originating grabber

  epg_object_ops_t       *ops;        ///< Operations on the object
};

/* Get an object by ID (special case usage) */
epg_object_t *epg_object_find_by_id  ( uint32_t id, epg_object_type_t type );
htsmsg_t     *epg_object_serialize   ( epg_object_t *eo );
epg_object_t *epg_object_deserialize ( htsmsg_t *msg, int create, int *save );

/* ************************************************************************
 * Brand - Represents a specific show
 * e.g. The Simpsons, 24, Eastenders, etc...
 * ***********************************************************************/

/* Change flags */
#define EPG_CHANGED_SEASON_COUNT (1<<(EPG_CHANGED_SLAST+1))
#define EPG_CHANGED_SEASONS      (1<<(EPG_CHANGED_SLAST+2))
#define EPG_CHANGED_EPISODES     (1<<(EPG_CHANGED_SLAST+3))

/* Object */
struct epg_brand
{
  epg_object_t;                            ///< Base object

  lang_str_t                *title;        ///< Brand name
  lang_str_t                *summary;      ///< Brand summary
  uint16_t                   season_count; ///< Total number of seasons
  char                      *image;        ///< Brand image

  epg_season_list_t          seasons;      ///< Season list
  epg_episode_list_t         episodes;     ///< Episode list
};

/* Lookup */
epg_brand_t *epg_brand_find_by_uri
  ( const char *uri, struct epggrab_module *src, int create, int *save, uint32_t *changes );
epg_brand_t *epg_brand_find_by_id ( uint32_t id );

/* Post-modify */
int epg_brand_change_finish( epg_brand_t *b, uint32_t changed, int merge )
  __attribute__((warn_unused_result));

/* Accessors */
const char *epg_brand_get_title
  ( const epg_brand_t *b, const char *lang );
const char *epg_brand_get_summary
  ( const epg_brand_t *b, const char *lang );

/* Mutators */
int epg_brand_set_title        
  ( epg_brand_t *b, const lang_str_t *title, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_brand_set_summary
  ( epg_brand_t *b, const lang_str_t *summary, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_brand_set_season_count
  ( epg_brand_t *b, uint16_t season_count, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_brand_set_image
  ( epg_brand_t *b, const char *i, uint32_t *changed )
  __attribute__((warn_unused_result));

/* Serialization */
htsmsg_t    *epg_brand_serialize   ( epg_brand_t *b );
epg_brand_t *epg_brand_deserialize ( htsmsg_t *m, int create, int *save );

/* List all brands (serialized) */
htsmsg_t    *epg_brand_list ( void );

/* ************************************************************************
 * Season
 * ***********************************************************************/

/* Change flags */
#define EPG_CHANGED_SEASON_NUMBER (1<<(EPG_CHANGED_SLAST+1))
#define EPG_CHANGED_EPISODE_COUNT (1<<(EPG_CHANGED_SLAST+2))

/* Object */
struct epg_season
{
  epg_object_t;                             ///< Parent object

  lang_str_t                *summary;       ///< Season summary
  uint16_t                   number;        ///< The season number
  uint16_t                   episode_count; ///< Total number of episodes
  char                      *image;         ///< Season image

  LIST_ENTRY(epg_season)     blink;         ///< Brand list link
  epg_brand_t               *brand;         ///< Parent brand
  epg_episode_list_t         episodes;      ///< Episode list

};

/* Lookup */
epg_season_t *epg_season_find_by_uri
  ( const char *uri, struct epggrab_module *src, int create, int *save, uint32_t *changes );
epg_season_t *epg_season_find_by_id ( uint32_t id );

/* Post-modify */
int epg_season_change_finish( epg_season_t *s, uint32_t changed, int merge )
  __attribute__((warn_unused_result));

/* Accessors */
const char *epg_season_get_summary
  ( const epg_season_t *s, const char *lang );

/* Mutators */
int epg_season_set_summary
  ( epg_season_t *s, const lang_str_t *summary, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_season_set_number
  ( epg_season_t *s, uint16_t number, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_season_set_episode_count
  ( epg_season_t *s, uint16_t episode_count, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_season_set_brand
  ( epg_season_t *s, epg_brand_t *b, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_season_set_image
  ( epg_season_t *s, const char *image, uint32_t *changed )
  __attribute__((warn_unused_result));

/* Serialization */
htsmsg_t    *epg_season_serialize    ( epg_season_t *b );
epg_season_t *epg_season_deserialize ( htsmsg_t *m, int create, int *save );

/* ************************************************************************
 * Episode
 * ***********************************************************************/

/* Change flags */
#define EPG_CHANGED_GENRE        (1<<(EPG_CHANGED_SLAST+1))
#define EPG_CHANGED_EPNUM_NUM    (1<<(EPG_CHANGED_SLAST+2))
#define EPG_CHANGED_EPNUM_CNT    (1<<(EPG_CHANGED_SLAST+3))
#define EPG_CHANGED_EPPAR_NUM    (1<<(EPG_CHANGED_SLAST+4))
#define EPG_CHANGED_EPPAR_CNT    (1<<(EPG_CHANGED_SLAST+5))
#define EPG_CHANGED_EPSER_NUM    (1<<(EPG_CHANGED_SLAST+6))
#define EPG_CHANGED_EPSER_CNT    (1<<(EPG_CHANGED_SLAST+7))
#define EPG_CHANGED_EPTEXT       (1<<(EPG_CHANGED_SLAST+8))
#define EPG_CHANGED_IS_BW        (1<<(EPG_CHANGED_SLAST+9))
#define EPG_CHANGED_STAR_RATING  (1<<(EPG_CHANGED_SLAST+10))
#define EPG_CHANGED_AGE_RATING   (1<<(EPG_CHANGED_SLAST+11))
#define EPG_CHANGED_FIRST_AIRED  (1<<(EPG_CHANGED_SLAST+12))
#define EPG_CHANGED_BRAND        (1<<(EPG_CHANGED_SLAST+13))
#define EPG_CHANGED_SEASON       (1<<(EPG_CHANGED_SLAST+14))

/* Episode numbering object - this is for some back-compat and also
 * to allow episode information to be "collated" into easy to use object
 */
typedef struct epg_episode_num
{
  uint16_t s_num; ///< Series number
  uint16_t s_cnt; ///< Series count
  uint16_t e_num; ///< Episode number
  uint16_t e_cnt; ///< Episode count
  uint16_t p_num; ///< Part number
  uint16_t p_cnt; ///< Part count
  char     *text; ///< Arbitary text description of episode num
} epg_episode_num_t;

/* Object */
struct epg_episode
{
  epg_object_t;                             ///< Parent object

  lang_str_t                *title;         ///< Title
  lang_str_t                *subtitle;      ///< Sub-title
  lang_str_t                *summary;       ///< Summary
  lang_str_t                *description;   ///< An extended description
  char                      *image;         ///< Episode image
  epg_genre_list_t           genre;         ///< Episode genre(s)
  epg_episode_num_t          epnum;         ///< Episode numbering
  // Note: do not use epnum directly! use the accessor routine

  uint8_t                    is_bw;          ///< Is black and white
  uint8_t                    star_rating;    ///< Star rating
  uint8_t                    age_rating;     ///< Age certificate
  time_t                     first_aired;    ///< Original airdate

  LIST_ENTRY(epg_episode)    blink;         ///< Brand link
  LIST_ENTRY(epg_episode)    slink;         ///< Season link
  epg_brand_t               *brand;         ///< (Grand-)Parent brand
  epg_season_t              *season;        ///< Parent season
  epg_broadcast_list_t       broadcasts;    ///< Broadcast list
};

/* Lookup */
epg_episode_t *epg_episode_find_by_uri
  ( const char *uri, struct epggrab_module *src, int create, int *save, uint32_t *changes );
epg_episode_t *epg_episode_find_by_id ( uint32_t id );
epg_episode_t *epg_episode_find_by_broadcast
  ( epg_broadcast_t *b, struct epggrab_module *src, int create, int *save, uint32_t *changes );

/* Post-modify */
int epg_episode_change_finish( epg_episode_t *s, uint32_t changed, int merge )
  __attribute__((warn_unused_result));

/* Accessors */
const char *epg_episode_get_title
  ( const epg_episode_t *e, const char *lang );
const char *epg_episode_get_subtitle
  ( const epg_episode_t *e, const char *lang );
const char *epg_episode_get_summary
  ( const epg_episode_t *e, const char *lang );
const char *epg_episode_get_description
  ( const epg_episode_t *e, const char *lang );

/* Mutators */
int epg_episode_set_title
  ( epg_episode_t *e, const lang_str_t *title, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_episode_set_subtitle
  ( epg_episode_t *e, const lang_str_t *subtitle, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_episode_set_summary
  ( epg_episode_t *e, const lang_str_t *summary, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_episode_set_description
  ( epg_episode_t *e, const lang_str_t *description, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_episode_set_number
  ( epg_episode_t *e, uint16_t number, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_episode_set_part
  ( epg_episode_t *e, uint16_t number, uint16_t count, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_episode_set_epnum
  ( epg_episode_t *e, epg_episode_num_t *num, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_episode_set_brand
  ( epg_episode_t *e, epg_brand_t *b, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_episode_set_season
  ( epg_episode_t *e, epg_season_t *s, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_episode_set_genre
  ( epg_episode_t *e, epg_genre_list_t *g, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_episode_set_image
  ( epg_episode_t *e, const char *i, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_episode_set_is_bw
  ( epg_episode_t *e, uint8_t bw, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_episode_set_first_aired
  ( epg_episode_t *e, time_t aired, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_episode_set_star_rating
  ( epg_episode_t *e, uint8_t stars, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_episode_set_age_rating
  ( epg_episode_t *e, uint8_t age, uint32_t *changed )
  __attribute__((warn_unused_result));

// Note: this does NOT strdup the text field
void epg_episode_get_epnum
  ( epg_episode_t *e, epg_episode_num_t *epnum );
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
int  epg_episode_number_cmp
  ( epg_episode_num_t *a, epg_episode_num_t *b );

/* Matching */
int epg_episode_fuzzy_match
  ( epg_episode_t *ee, const char *uri, const char *title,
    const char *summary, const char *description );

/* Serialization */
htsmsg_t      *epg_episode_serialize   ( epg_episode_t *b );
epg_episode_t *epg_episode_deserialize ( htsmsg_t *m, int create, int *save );

/* ************************************************************************
 * Series Link - broadcast level linkage
 * ***********************************************************************/

/* Object */
struct epg_serieslink
{
  epg_object_t;

  epg_broadcast_list_t         broadcasts;      ///< Episode list
};

/* Lookup */
epg_serieslink_t *epg_serieslink_find_by_uri
  ( const char *uri, struct epggrab_module *src, int create, int *save, uint32_t *changes );
epg_serieslink_t *epg_serieslink_find_by_id
  ( uint32_t id );

/* Post-modify */
int epg_serieslink_change_finish( epg_serieslink_t *s, uint32_t changed, int merge )
  __attribute__((warn_unused_result));

/* Serialization */
htsmsg_t         *epg_serieslink_serialize   ( epg_serieslink_t *s );
epg_serieslink_t *epg_serieslink_deserialize 
  ( htsmsg_t *m, int create, int *save );

/* ************************************************************************
 * Broadcast - specific airing (channel & time) of an episode
 * ***********************************************************************/

#define EPG_CHANGED_DVB_EID      (1<<(EPG_CHANGED_SLAST+1))
#define EPG_CHANGED_IS_WIDESCREEN (1<<(EPG_CHANGED_SLAST+2))
#define EPG_CHANGED_IS_HD        (1<<(EPG_CHANGED_SLAST+3))
#define EPG_CHANGED_LINES        (1<<(EPG_CHANGED_SLAST+4))
#define EPG_CHANGED_ASPECT       (1<<(EPG_CHANGED_SLAST+5))
#define EPG_CHANGED_DEAFSIGNED   (1<<(EPG_CHANGED_SLAST+6))
#define EPG_CHANGED_SUBTITLED    (1<<(EPG_CHANGED_SLAST+7))
#define EPG_CHANGED_AUDIO_DESC   (1<<(EPG_CHANGED_SLAST+8))
#define EPG_CHANGED_IS_NEW       (1<<(EPG_CHANGED_SLAST+9))
#define EPG_CHANGED_IS_REPEAT    (1<<(EPG_CHANGED_SLAST+10))
#define EPG_CHANGED_EPISODE      (1<<(EPG_CHANGED_SLAST+11))
#define EPG_CHANGED_SERIESLINK   (1<<(EPG_CHANGED_SLAST+12))

/* Object */
struct epg_broadcast
{
  epg_object_t;                                ///< Parent object
  
  uint16_t                   dvb_eid;          ///< DVB Event ID
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
  uint8_t                    running;          ///< EPG running flag

  /* Broadcast level text */
  lang_str_t                *summary;          ///< Summary
  lang_str_t                *description;      ///< Description

  RB_ENTRY(epg_broadcast)    sched_link;       ///< Schedule link
  LIST_ENTRY(epg_broadcast)  ep_link;          ///< Episode link
  epg_episode_t             *episode;          ///< Episode shown
  LIST_ENTRY(epg_broadcast)  sl_link;          ///< SeriesLink link
  epg_serieslink_t          *serieslink;       ///< SeriesLink;
  struct channel            *channel;          ///< Channel being broadcast on

};

/* Lookup */
epg_broadcast_t *epg_broadcast_find_by_time 
  ( struct channel *ch, struct epggrab_module *src,
    time_t start, time_t stop, int create, int *save, uint32_t *changes );
epg_broadcast_t *epg_broadcast_find_by_eid ( struct channel *ch, uint16_t eid );
epg_broadcast_t *epg_broadcast_find_by_id  ( uint32_t id );

/* Post-modify */
int epg_broadcast_change_finish( epg_broadcast_t *b, uint32_t changed, int merge )
  __attribute__((warn_unused_result));

/* Special */
epg_broadcast_t *epg_broadcast_clone
  ( struct channel *channel, epg_broadcast_t *src, int *save );
void epg_broadcast_notify_running
  ( epg_broadcast_t *b, epg_source_t esrc, epg_running_t running );

/* Mutators */
int epg_broadcast_set_dvb_eid
  ( epg_broadcast_t *b, uint16_t dvb_eid, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_episode
  ( epg_broadcast_t *b, epg_episode_t *e, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_is_widescreen
  ( epg_broadcast_t *b, uint8_t ws, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_is_hd
  ( epg_broadcast_t *b, uint8_t hd, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_lines 
  ( epg_broadcast_t *b, uint16_t lines, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_aspect
  ( epg_broadcast_t *b, uint16_t aspect, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_is_deafsigned
  ( epg_broadcast_t *b, uint8_t ds, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_is_subtitled
  ( epg_broadcast_t *b, uint8_t st, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_is_audio_desc
  ( epg_broadcast_t *b, uint8_t ad, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_is_new
  ( epg_broadcast_t *b, uint8_t n, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_is_repeat
  ( epg_broadcast_t *b, uint8_t r, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_summary
  ( epg_broadcast_t *b, const lang_str_t *str, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_description
  ( epg_broadcast_t *b, const lang_str_t *str, uint32_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_serieslink
  ( epg_broadcast_t *b, epg_serieslink_t *sl, uint32_t *changed )
  __attribute__((warn_unused_result));

/* Accessors */
epg_broadcast_t *epg_broadcast_get_next    ( epg_broadcast_t *b );
const char *epg_broadcast_get_title 
  ( epg_broadcast_t *b, const char *lang );
const char *epg_broadcast_get_subtitle
  ( epg_broadcast_t *b, const char *lang );
const char *epg_broadcast_get_summary
  ( epg_broadcast_t *b, const char *lang );
const char *epg_broadcast_get_description
  ( epg_broadcast_t *b, const char *lang );

/* Serialization */
htsmsg_t        *epg_broadcast_serialize   ( epg_broadcast_t *b );
epg_broadcast_t *epg_broadcast_deserialize 
  ( htsmsg_t *m, int create, int *save );

/* ************************************************************************
 * Channel - provides mapping from EPG channels to real channels
 * ***********************************************************************/

/* Unlink */
void epg_channel_unlink ( struct channel *ch );

/* ************************************************************************
 * Global config
 * ***********************************************************************/
htsmsg_t        *epg_config_serialize ( void );
int              epg_config_deserialize ( htsmsg_t *m );

/* ************************************************************************
 * Querying
 * ***********************************************************************/

typedef enum {
  EC_NO, ///< No filter
  EC_EQ, ///< Equals
  EC_LT, ///< LT
  EC_GT, ///< GT
  EC_RG, ///< Range
  EC_IN, ///< contains (STR only)
  EC_RE, ///< regexp (STR only)
} epg_comp_t;

typedef struct epg_filter_str {
  char      *str;
  regex_t    re;
  epg_comp_t comp;
} epg_filter_str_t;

typedef struct epg_filter_num {
  int64_t    val1;
  int64_t    val2;
  epg_comp_t comp;
} epg_filter_num_t;

typedef struct epg_query {
  /* Configuration */
  char             *lang;

  /* Filter */
  epg_filter_num_t  start;
  epg_filter_num_t  stop;
  epg_filter_num_t  duration;
  epg_filter_str_t  title;
  epg_filter_str_t  subtitle;
  epg_filter_str_t  summary;
  epg_filter_str_t  description;
  epg_filter_num_t  episode;
  epg_filter_num_t  stars;
  epg_filter_num_t  age;
  epg_filter_str_t  channel_name;
  epg_filter_num_t  channel_num;
  char             *stitle;
  regex_t           stitle_re;
  int               fulltext;
  char             *channel;
  char             *channel_tag;
  uint32_t          genre_count;
  uint8_t          *genre;
  uint8_t           genre_static[16];

  enum {
    ESK_START,
    ESK_STOP,
    ESK_DURATION,
    ESK_TITLE,
    ESK_SUBTITLE,
    ESK_SUMMARY,
    ESK_DESCRIPTION,
    ESK_CHANNEL,
    ESK_CHANNEL_NUM,
    ESK_STARS,
    ESK_AGE,
    ESK_GENRE
  } sort_key;
  enum {
    ES_ASC,
    ES_DSC
  } sort_dir;

  /* Result */
  epg_broadcast_t **result;
  uint32_t          entries;
  uint32_t          allocated;
} epg_query_t;

epg_broadcast_t  **epg_query(epg_query_t *eq, access_t *perm);
void epg_query_free(epg_query_t *eq);

/* ************************************************************************
 * Setup/Shutdown
 * ***********************************************************************/

void epg_init    (void);
void epg_done    (void);
void epg_skel_done (void);
void epg_save    (void);
void epg_save_callback (void *p);
void epg_updated (void);

#endif /* EPG_H */
