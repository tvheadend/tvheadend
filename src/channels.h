/*
 *  tvheadend, channel functions
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

#ifndef CHANNELS_H
#define CHANNELS_H

#include "epg.h"
#include "idnode.h"

struct access;
struct bouquet;

RB_HEAD(channel_tree, channel);

TAILQ_HEAD(channel_tag_queue, channel_tag);

extern struct channel_tag_queue channel_tags;
extern int                      channel_tags_count;
extern struct channel_tree      channels;
extern int                      channels_count;

#define CHANNEL_FOREACH(ch) RB_FOREACH(ch, &channels, ch_link)

/*
 * Channel definition
 */ 
typedef struct channel
{
  idnode_t ch_id;

  RB_ENTRY(channel) ch_link;

  int ch_refcount;
  int ch_load;
  int ch_dont_save;
  int ch_changed_ref;

  /* Channel info */
  int     ch_enabled;
  int     ch_autoname;
  char   *ch_name;                                 /* Note: do not access directly! */
  int64_t ch_number;
  char   *ch_icon;
  idnode_list_head_t ch_ctms;
  struct bouquet *ch_bouquet;

  /* Service/subscriptions */
  idnode_list_head_t           ch_services;
  LIST_HEAD(, th_subscription) ch_subscriptions;

  /* EPG fields */
  char                 *ch_epg_parent;
  LIST_HEAD(, channel)  ch_epg_slaves;
  LIST_ENTRY(channel)   ch_epg_slave_link;
  epg_broadcast_tree_t  ch_epg_schedule;
  epg_broadcast_t      *ch_epg_now;
  epg_broadcast_t      *ch_epg_next;
  gtimer_t              ch_epg_timer;
  gtimer_t              ch_epg_timer_head;
  gtimer_t              ch_epg_timer_current;
  uint32_t              ch_epg_limit;

  int                   ch_epgauto;
  idnode_list_head_t    ch_epggrab;                /* 1 = epggrab channel, 2 = channel */

  /* DVR */
  int                   ch_dvr_extra_time_pre;
  int                   ch_dvr_extra_time_post;
  int                   ch_epg_running;
  LIST_HEAD(, dvr_entry)         ch_dvrs;
  LIST_HEAD(, dvr_autorec_entry) ch_autorecs;
  LIST_HEAD(, dvr_timerec_entry) ch_timerecs;

} channel_t;


/**
 * Channel tag
 */
typedef struct channel_tag {

  idnode_t ct_id;

  TAILQ_ENTRY(channel_tag) ct_link;

  int ct_enabled;
  uint32_t ct_index;
  int ct_internal;
  int ct_private;
  int ct_titled_icon;
  char *ct_name;
  char *ct_comment;
  char *ct_icon;

  idnode_list_head_t ct_ctms;

  LIST_HEAD(, dvr_autorec_entry) ct_autorecs;

  idnode_list_head_t ct_accesses;

  int ct_htsp_id;

} channel_tag_t;

extern const idclass_t channel_class;
extern const idclass_t channel_tag_class;
extern const char *channel_blank_name;

void channel_init(void);
void channel_done(void);

channel_t *channel_create0
  (channel_t *ch, const idclass_t *idc, const char *uuid, htsmsg_t *conf,
   const char *name);
#define channel_create(u, c, n)\
  channel_create0(calloc(1, sizeof(channel_t)), &channel_class, u, c, n)

void channel_delete(channel_t *ch, int delconf);

void channel_remove_subscriber(channel_t *ch, int reason);

channel_t *channel_find_by_name_and_bouquet(const char *name, const struct bouquet *bq);
channel_t *channel_find_by_name(const char *name);
/// Apply fuzzy matching when finding a channel such as ignoring
/// whitespace, case, and stripping HD suffix. This means that
/// 'Channel 5+1', 'Channel 5 +1', 'Channel 5+1HD' and
/// 'Channel 5 +1HD' can all be merged together.
/// Since channel names aren't unique, this returns the
/// first match (similar to channel_find_by_name).
/// @param bouquet - Bouquet to use: can be NULL
channel_t *channel_find_by_name_bouquet_fuzzy(const char *name, const struct bouquet *bq);
#define channel_find_by_uuid(u)\
  (channel_t*)idnode_find(u, &channel_class, NULL)

channel_t *channel_find_by_id(uint32_t id);

channel_t *channel_find_by_number(const char *no);

#define channel_find channel_find_by_uuid

htsmsg_t * channel_class_get_list(void *o, const char *lang);

const void * channel_class_get_icon ( void *obj );

int channel_set_tags_by_list ( channel_t *ch, htsmsg_t *tags );

channel_tag_t *channel_tag_create(const char *uuid, htsmsg_t *conf);

channel_tag_t *channel_tag_find_by_name(const char *name, int create);

channel_tag_t *channel_tag_find_by_id(uint32_t id);

static inline channel_tag_t *channel_tag_find_by_uuid(const char *uuid)
  {  return (channel_tag_t*)idnode_find(uuid, &channel_tag_class, NULL); }

htsmsg_t * channel_tag_class_get_list(void *o, const char *lang);

const char * channel_tag_get_icon(channel_tag_t *ct);

int channel_access(channel_t *ch, struct access *a, int disabled);

void channel_event_updated(epg_broadcast_t *e);

int channel_tag_map(channel_tag_t *ct, channel_t *ch, void *origin);
void channel_tag_unmap(channel_t *ch, void *origin);

int channel_tag_access(channel_tag_t *ct, struct access *a, int disabled);

const char *channel_get_name ( const channel_t *ch, const char *blank );
int channel_set_name ( channel_t *ch, const char *name );
/// User API convenience function to rename all channels that
/// match "from". Lock must be held prior to call.
/// @return number channels that matched "from".
int channel_rename_and_save ( const char *from, const char *to );

#define CHANNEL_ENAME_NUMBERS (1<<0)
#define CHANNEL_ENAME_SOURCES (1<<1)

char *channel_get_ename ( channel_t *ch, char *dst, size_t dstlen,
                          const char *blank, uint32_t flags );

#define CHANNEL_SPLIT ((int64_t)1000000)

static inline uint32_t channel_get_major ( int64_t chnum ) { return chnum / CHANNEL_SPLIT; }
static inline uint32_t channel_get_minor ( int64_t chnum ) { return chnum % CHANNEL_SPLIT; }

int64_t channel_get_number ( const channel_t *ch );
int channel_set_number ( channel_t *ch, uint32_t major, uint32_t minor );

char *channel_get_number_as_str ( channel_t *ch, char *dst, size_t dstlen );
int64_t channel_get_number_from_str ( const char *str );

char *channel_get_source ( channel_t *ch, char *dst, size_t dstlen );

const char *channel_get_icon ( channel_t *ch );
int channel_set_icon ( channel_t *ch, const char *icon );

const char *channel_get_epgid ( channel_t *ch );

#define channel_get_uuid(ch,ub) idnode_uuid_as_str(&(ch)->ch_id, ub)

#define channel_get_id(ch)    idnode_get_short_uuid((&(ch)->ch_id))

channel_t **channel_get_sorted_list
  ( const char *sort_type, int all, int *_count ) ;
channel_t **channel_get_sorted_list_for_tag
  ( const char *sort_type, channel_tag_t *tag, int *_count );
channel_tag_t **channel_tag_get_sorted_list
  ( const char *sort_type, int *_count );
int channel_has_correct_service_filter(const channel_t *ch, int svf);

#endif /* CHANNELS_H */
