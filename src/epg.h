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

#include "tvhregex.h"
#include "settings.h"
#include "lang_str.h"
#include "string_list.h"
#include "access.h"
#include "ratinglabels.h"  //Needed for the ratinglabel_t struct.

/*
 * External forward decls
 */
struct channel;
struct channel_tag;
struct dvr_entry;
struct epggrab_module;

/*
 * Map/List types
 */
typedef LIST_HEAD(,epg_object)     epg_object_list_t;
typedef RB_HEAD  (,epg_object)     epg_object_tree_t;
typedef LIST_HEAD(,epg_broadcast)  epg_broadcast_list_t;
typedef RB_HEAD  (,epg_broadcast)  epg_broadcast_tree_t;
typedef RB_HEAD  (,epg_set)        epg_set_tree_t;
typedef LIST_HEAD(,epg_genre)      epg_genre_list_t;

/*
 * Typedefs (most are redundant!)
 */
typedef struct epg_genre           epg_genre_t;
typedef struct epg_object          epg_object_t;
typedef struct epg_broadcast       epg_broadcast_t;
typedef struct epg_set_item        epg_set_item_t;
typedef struct epg_set             epg_set_t;

extern int epg_in_load;
extern epg_set_tree_t epg_episodelinks;
extern epg_set_tree_t epg_serieslinks;

/*
 *
 */
typedef enum {
  EPG_RUNNING_NOTSET = 0,
  EPG_RUNNING_STOP   = 1,
  EPG_RUNNING_WARM   = 2,
  EPG_RUNNING_NOW    = 3,
  EPG_RUNNING_PAUSE  = 4,
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
  EPG_BROADCAST,
} epg_object_type_t;
#define EPG_TYPEMAX EPG_BROADCAST

/* Change type */
typedef uint64_t epg_changes_t;

/* Change flags */
#define EPG_CHANGED_CREATE         (1ULL<<0)
#define EPG_CHANGED_TITLE          (1ULL<<1)
#define EPG_CHANGED_SUBTITLE       (1ULL<<2)
#define EPG_CHANGED_SUMMARY        (1ULL<<3)
#define EPG_CHANGED_DESCRIPTION    (1ULL<<4)
#define EPG_CHANGED_IMAGE          (1ULL<<5)
#define EPG_CHANGED_CREDITS        (1ULL<<6)
#define EPG_CHANGED_CATEGORY       (1ULL<<7)
#define EPG_CHANGED_KEYWORD        (1ULL<<8)
#define EPG_CHANGED_DVB_EID        (1ULL<<9)
#define EPG_CHANGED_IS_WIDESCREEN  (1ULL<<10)
#define EPG_CHANGED_IS_HD          (1ULL<<11)
#define EPG_CHANGED_LINES          (1ULL<<12)
#define EPG_CHANGED_ASPECT         (1ULL<<13)
#define EPG_CHANGED_DEAFSIGNED     (1ULL<<14)
#define EPG_CHANGED_SUBTITLED      (1ULL<<15)
#define EPG_CHANGED_AUDIO_DESC     (1ULL<<16)
#define EPG_CHANGED_IS_NEW         (1ULL<<17)
#define EPG_CHANGED_IS_REPEAT      (1ULL<<18)
#define EPG_CHANGED_SERIESLINK     (1ULL<<19)
#define EPG_CHANGED_EPISODE        (1ULL<<20)
#define EPG_CHANGED_GENRE          (1ULL<<21)
#define EPG_CHANGED_EPNUM_NUM      (1ULL<<22)
#define EPG_CHANGED_EPNUM_CNT      (1ULL<<23)
#define EPG_CHANGED_EPPAR_NUM      (1ULL<<24)
#define EPG_CHANGED_EPPAR_CNT      (1ULL<<25)
#define EPG_CHANGED_EPSER_NUM      (1ULL<<26)
#define EPG_CHANGED_EPSER_CNT      (1ULL<<27)
#define EPG_CHANGED_EPTEXT         (1ULL<<28)
#define EPG_CHANGED_IS_BW          (1ULL<<29)
#define EPG_CHANGED_STAR_RATING    (1ULL<<30)
#define EPG_CHANGED_AGE_RATING     (1ULL<<31)
#define EPG_CHANGED_FIRST_AIRED    (1ULL<<32)
#define EPG_CHANGED_COPYRIGHT_YEAR (1ULL<<33)
#define EPG_CHANGED_RATING_LABEL   (1ULL<<34)

typedef struct epg_object_ops {
  void (*getref)  ( void *o );        ///< Get a reference
  int  (*putref)  ( void *o ); 	      ///< Release a reference
  void (*destroy) ( void *o );        ///< Delete the object
  void (*update)  ( void *o );        ///< Updated
} epg_object_ops_t;

/* Object */
struct epg_object
{
  RB_ENTRY(epg_object)    id_link;    ///< Global (ID) link
  LIST_ENTRY(epg_object)  un_link;    ///< Global unref'd link
  LIST_ENTRY(epg_object)  up_link;    ///< Global updated link

  epg_object_type_t       type;       ///< Specific object type
  uint32_t                id;         ///< Internal ID
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
 * Episode numbering
 * ***********************************************************************/

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

htsmsg_t *epg_episode_epnum_serialize( epg_episode_num_t *num );
void epg_episode_epnum_deserialize( htsmsg_t *m, epg_episode_num_t *num );

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
size_t epg_episode_num_format
  ( epg_episode_num_t *epnum, char *buf, size_t len,
    const char *pre,  const char *sfmt,
    const char *sep,  const char *efmt,
    const char *cfmt );
int epg_episode_number_cmp
  ( const epg_episode_num_t *a, const epg_episode_num_t *b );
int epg_episode_number_cmpfull
  ( const epg_episode_num_t *a, const epg_episode_num_t *b );

/* ************************************************************************
 * Broadcast set
 * ***********************************************************************/

struct epg_set_item {
  LIST_ENTRY(epg_set_item) item_link;
  epg_broadcast_t *broadcast;
};

struct epg_set {
  RB_ENTRY(epg_set) set_link;
  LIST_HEAD(, epg_set_item) broadcasts;
  char uri[0];
};

epg_set_t *epg_set_broadcast_insert
  (epg_set_tree_t *tree, epg_broadcast_t *b, const char *uri);
void epg_set_broadcast_remove
  (epg_set_tree_t *tree, epg_set_t *set, epg_broadcast_t *b);
epg_set_t *epg_set_broadcast_find_by_uri
  (epg_set_tree_t *tree, const char *uri);

/* ************************************************************************
 * Broadcast - specific airing (channel & time) of an episode
 * ***********************************************************************/

/* Object */
struct epg_broadcast
{
  epg_object_t;                                ///< Parent object

  struct channel            *channel;          ///< Channel being broadcast on
  RB_ENTRY(epg_broadcast)    sched_link;       ///< Schedule link
  LIST_HEAD(, dvr_entry)     dvr_entries;      ///< Associated DVR entries

  /* */
  uint16_t                   dvb_eid;          ///< DVB Event ID
  time_t                     start;            ///< Start time
  time_t                     stop;             ///< End time

  /* Some quality info */
  uint16_t                   lines;            ///< Lines in image (quality)
  uint16_t                   aspect;           ///< Aspect ratio (*100)
  uint8_t                    is_widescreen;    ///< Is widescreen
  uint8_t                    is_hd;            ///< Is HD
  uint8_t                    is_bw;            ///< Is black and white

  /* Some accessibility support */
  uint8_t                    is_deafsigned;    ///< In screen signing
  uint8_t                    is_subtitled;     ///< Teletext subtitles
  uint8_t                    is_audio_desc;    ///< Audio description

  /* Misc flags */
  uint8_t                    star_rating;      ///< Star rating
  uint8_t                    age_rating;       ///< Age certificate
  ratinglabel_t              *rating_label;    ///< Age certificate label (eg: 'PG')
  uint8_t                    is_new;           ///< New series / file premiere
  uint8_t                    is_repeat;        ///< Repeat screening
  uint8_t                    running;          ///< EPG running flag
  uint8_t                    update_running;   ///< new EPG running flag

  /* Broadcast level text */
  lang_str_t                *title;            ///< Title
  lang_str_t                *subtitle;         ///< Sub-title
  lang_str_t                *summary;          ///< Summary
  lang_str_t                *description;      ///< Description

  char                      *image;            ///< Episode image
  epg_genre_list_t           genre;            ///< Episode genre(s)
  epg_episode_num_t          epnum;            ///< Episode numbering; NOTE: use the accessor routine!

  htsmsg_t                  *credits;          ///< Cast/Credits map of name -> role type (actor, presenter, director, etc).
  lang_str_t                *credits_cached;   ///< Comma separated cast (for regex searching in GUI/autorec). Kept in sync with cast_map
  string_list_t             *category;         ///< Extra categories (typically from xmltv) such as "Western" or "Sumo Wrestling".
                                               ///< These extra categories are often a superset of our EN 300 468 DVB genre.
                                               ///< Used with drop-down lists in the GUI.
  string_list_t             *keyword;          ///< Extra keywords (typically from xmltv) such as "Wild West" or "Unicorn".
  lang_str_t                *keyword_cached;   ///< Cached CSV version for regex searches.
  epg_set_t                 *serieslink;       ///< Series Link
  epg_set_t                 *episodelink;      ///< Episode Link

  time_t                     first_aired;      ///< Original airdate
  uint16_t                   copyright_year;   ///< xmltv DTD gives a tag "date" (separate to previously-shown/first aired).
                                               ///< This is the date programme was "finished...probably the copyright date."
                                               ///< We'll call it copyright_year since words like "complete" and "finished"
                                               ///< sound too similar to dvr recorded functionality. We'll only store the
                                               ///< year since we only get year not month and day.
  char                       *xmltv_eid;       ///< XMLTV (or other) unique event identifier
};

/* Lookup */
epg_broadcast_t *epg_broadcast_find_by_time
  ( struct channel *ch, struct epggrab_module *src,
    time_t start, time_t stop, int create, int *save, epg_changes_t *changes );
epg_broadcast_t *epg_broadcast_find_by_eid ( struct channel *ch, uint16_t eid );
epg_broadcast_t *epg_broadcast_find_by_id  ( uint32_t id );
epg_broadcast_t *epg_broadcast_find_by_xmltv_eid
  ( struct channel *ch, struct epggrab_module *src,
    time_t start, time_t stop, int create,
    int *save, epg_changes_t *changed, const char* xmltv_eid);

/* Post-modify */
int epg_broadcast_change_finish( epg_broadcast_t *b, epg_changes_t changed, int merge )
  __attribute__((warn_unused_result));

/* Special */
epg_broadcast_t *epg_broadcast_clone
  ( struct channel *channel, epg_broadcast_t *src, int *save );

/* Mutators */
int epg_broadcast_set_dvb_eid
  ( epg_broadcast_t *b, uint16_t dvb_eid, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_xmltv_eid
  ( epg_broadcast_t *b, const char *xmltv_eid, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_running
  ( epg_broadcast_t *b, epg_running_t running )
  __attribute__((warn_unused_result));
int epg_broadcast_set_is_widescreen
  ( epg_broadcast_t *b, uint8_t ws, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_is_hd
  ( epg_broadcast_t *b, uint8_t hd, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_lines
  ( epg_broadcast_t *b, uint16_t lines, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_aspect
  ( epg_broadcast_t *b, uint16_t aspect, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_is_deafsigned
  ( epg_broadcast_t *b, uint8_t ds, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_is_subtitled
  ( epg_broadcast_t *b, uint8_t st, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_is_audio_desc
  ( epg_broadcast_t *b, uint8_t ad, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_is_new
  ( epg_broadcast_t *b, uint8_t n, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_is_repeat
  ( epg_broadcast_t *b, uint8_t r, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_title
  ( epg_broadcast_t *b, const lang_str_t *str, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_subtitle
  ( epg_broadcast_t *b, const lang_str_t *str, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_summary
  ( epg_broadcast_t *b, const lang_str_t *str, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_description
  ( epg_broadcast_t *b, const lang_str_t *str, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_credits
( epg_broadcast_t *b, const htsmsg_t *msg, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_category
( epg_broadcast_t *b, const string_list_t *msg, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_keyword
( epg_broadcast_t *b, const string_list_t *msg, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_serieslink_uri
  ( epg_broadcast_t *b, const char *uri, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_episodelink_uri
  ( epg_broadcast_t *b, const char *uri, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_epnumber
  ( epg_broadcast_t *b, uint16_t number, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_eppart
  ( epg_broadcast_t *b, uint16_t number, uint16_t count, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_epnum
  ( epg_broadcast_t *b, epg_episode_num_t *num, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_genre
  ( epg_broadcast_t *b, epg_genre_list_t *g, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_image
  ( epg_broadcast_t *b, const char *i, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_is_bw
  ( epg_broadcast_t *b, uint8_t bw, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_first_aired
  ( epg_broadcast_t *b, time_t aired, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_star_rating
  ( epg_broadcast_t *b, uint8_t stars, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_copyright_year
  ( epg_broadcast_t *b, uint16_t stars, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_age_rating
  ( epg_broadcast_t *b, uint8_t age, epg_changes_t *changed )
  __attribute__((warn_unused_result));
int epg_broadcast_set_rating_label
  ( epg_broadcast_t *b, ratinglabel_t *rating_label, epg_changes_t *changed )
  __attribute__((warn_unused_result));

/* Accessors */
epg_broadcast_t *epg_broadcast_get_prev( epg_broadcast_t *b );
epg_broadcast_t *epg_broadcast_get_next( epg_broadcast_t *b );
const char *epg_broadcast_get_title
  ( const epg_broadcast_t *b, const char *lang );
const char *epg_broadcast_get_subtitle
  ( epg_broadcast_t *b, const char *lang );
const char *epg_broadcast_get_summary
  ( epg_broadcast_t *b, const char *lang );
const char *epg_broadcast_get_description
  ( epg_broadcast_t *b, const char *lang );
/* Get the cached (csv) version for regex searching */
const char *epg_broadcast_get_credits_cached
  ( epg_broadcast_t *b, const char *lang );
const char *epg_broadcast_get_keyword_cached
  ( epg_broadcast_t *b, const char *lang );
const ratinglabel_t *epg_broadcast_get_rating_label
  ( epg_broadcast_t *b );
char* epg_broadcast_get_merged_text
  ( epg_broadcast_t *b );

/* Episode number heplers */
// Note: this does NOT strdup the text field
void epg_broadcast_get_epnum
  ( const epg_broadcast_t *b, epg_episode_num_t *epnum );
size_t epg_broadcast_epnumber_format
  ( epg_broadcast_t *b, char *buf, size_t len,
    const char *pre,  const char *sfmt,
    const char *sep,  const char *efmt,
    const char *cfmt );

static inline int epg_episode_match(epg_broadcast_t *a, epg_broadcast_t *b)
{
  if (a == NULL || b == NULL) return 0;
  if (a->episodelink == NULL || b->episodelink == NULL) return 0;
  return a->episodelink == b->episodelink;
}

/* Serialization */
htsmsg_t        *epg_broadcast_serialize   ( epg_broadcast_t *b );
epg_broadcast_t *epg_broadcast_deserialize
  ( htsmsg_t *m, int create, int *save );

/* ************************************************************************
 * Channel - provides mapping from EPG channels to real channels
 * ***********************************************************************/

int epg_channel_ignore_broadcast( struct channel *ch, time_t start );
/* Unlink */
void epg_channel_unlink ( struct channel *ch );
/* Match now / next events */
epg_broadcast_t *epg_match_now_next ( struct channel *ch, epg_broadcast_t *ebc );

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
  char       *str;
  tvh_regex_t re;
  epg_comp_t  comp;
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
  epg_filter_str_t  extratext;
  epg_filter_num_t  episode;
  epg_filter_num_t  stars;
  epg_filter_num_t  age;
  epg_filter_str_t  channel_name;
  epg_filter_num_t  channel_num;
  char             *stitle;
  tvh_regex_t       stitle_re;
  int               fulltext;
  int               mergetext;
  int               new_only;
  char             *channel;
  char             *channel_tag;
  uint32_t          genre_count;
  uint8_t          *genre;
  uint8_t           genre_static[16];
  char             *cat1;
  char             *cat2;
  char             *cat3;

  enum {
    ESK_START,
    ESK_STOP,
    ESK_DURATION,
    ESK_TITLE,
    ESK_SUBTITLE,
    ESK_SUMMARY,
    ESK_DESCRIPTION,
    ESK_EXTRATEXT,
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
