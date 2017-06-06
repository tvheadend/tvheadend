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
extern struct channel_tree      channels;

#define CHANNEL_FOREACH(ch) RB_FOREACH(ch, &channels, ch_link)

/*
 * Channel definition
 */ 
typedef struct channel
{
  idnode_t ch_id;

  RB_ENTRY(channel)   ch_link;

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

  int                   ch_epgauto;
  idnode_list_head_t    ch_epggrab;                /* 1 = epggrab channel, 2 = channel */

  /* DVR */
  int                   ch_dvr_extra_time_pre;
  int                   ch_dvr_extra_time_post;
  int                   ch_epg_running;
  struct dvr_entry_list ch_dvrs;
  struct dvr_autorec_entry_list ch_autorecs;
  struct dvr_timerec_entry_list ch_timerecs;

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

  struct dvr_autorec_entry_list ct_autorecs;

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

channel_t *channel_find_by_name(const char *name);
#define channel_find_by_uuid(u)\
  (channel_t*)idnode_find(u, &channel_class, NULL)

channel_t *channel_find_by_id(uint32_t id);

channel_t *channel_find_by_number(const char *no);

#define channel_find channel_find_by_uuid

htsmsg_t * channel_class_get_list(void *o, const char *lang);

int channel_set_tags_by_list ( channel_t *ch, htsmsg_t *tags );

channel_tag_t *channel_tag_create(const char *uuid, htsmsg_t *conf);

channel_tag_t *channel_tag_find_by_name(const char *name, int create);

channel_tag_t *channel_tag_find_by_identifier(uint32_t id);

static inline channel_tag_t *channel_tag_find_by_uuid(const char *uuid)
  {  return (channel_tag_t*)idnode_find(uuid, &channel_tag_class, NULL); }

htsmsg_t * channel_tag_class_get_list(void *o, const char *lang);

const char * channel_tag_get_icon(channel_tag_t *ct);

int channel_access(channel_t *ch, struct access *a, int disabled);

void channel_event_updated(epg_broadcast_t *e);

int channel_tag_map(channel_tag_t *ct, channel_t *ch, void *origin);
void channel_tag_unmap(channel_t *ch, void *origin);

int channel_tag_access(channel_tag_t *ct, struct access *a, int disabled);

const char *channel_get_name ( channel_t *ch, const char *blank );
int channel_set_name ( channel_t *ch, const char *name );

#define CHANNEL_SPLIT ((int64_t)1000000)

static inline uint32_t channel_get_major ( int64_t chnum ) { return chnum / CHANNEL_SPLIT; }
static inline uint32_t channel_get_minor ( int64_t chnum ) { return chnum % CHANNEL_SPLIT; }

int64_t channel_get_number ( channel_t *ch );
int channel_set_number ( channel_t *ch, uint32_t major, uint32_t minor );

const char *channel_get_icon ( channel_t *ch );
int channel_set_icon ( channel_t *ch, const char *icon );

const char *channel_get_epgid ( channel_t *ch );

#define channel_get_uuid(ch,ub) idnode_uuid_as_str(&(ch)->ch_id, ub)

#define channel_get_id(ch)    idnode_get_short_uuid((&(ch)->ch_id))

#endif /* CHANNELS_H */
