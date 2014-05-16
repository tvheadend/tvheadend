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

RB_HEAD(channel_tree, channel);

LIST_HEAD(channel_tag_mapping_list, channel_tag_mapping);
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
  int ch_zombie;

  /* Channel info */
  char *ch_name; // Note: do not access directly!
  int   ch_number;
  char *ch_usericon;
  struct channel_tag_mapping_list ch_ctms;

  /* Service/subscriptions */
  LIST_HEAD(, channel_service_mapping) ch_services;
  LIST_HEAD(, th_subscription)         ch_subscriptions;

  /* EPG fields */
  epg_broadcast_tree_t  ch_epg_schedule;
  epg_broadcast_t      *ch_epg_now;
  epg_broadcast_t      *ch_epg_next;
  gtimer_t              ch_epg_timer;
  gtimer_t              ch_epg_timer_head;
  gtimer_t              ch_epg_timer_current;

  LIST_HEAD(,epggrab_channel_link) ch_epggrab;

  /* DVR */
  int                   ch_dvr_extra_time_pre;
  int                   ch_dvr_extra_time_post;
  struct dvr_entry_list ch_dvrs;
  struct dvr_autorec_entry_list ch_autorecs;

} channel_t;


/**
 * Channel tag
 */
typedef struct channel_tag {
  TAILQ_ENTRY(channel_tag) ct_link;
  int ct_enabled;
  int ct_internal;
  int ct_titled_icon;
  int ct_identifier;
  char *ct_name;
  char *ct_comment;
  char *ct_icon;
  struct channel_tag_mapping_list ct_ctms;

  struct dvr_autorec_entry_list ct_autorecs;
} channel_tag_t;

/**
 * Channel tag mapping
 */
typedef struct channel_tag_mapping {
  LIST_ENTRY(channel_tag_mapping) ctm_channel_link;
  channel_t *ctm_channel;
  
  LIST_ENTRY(channel_tag_mapping) ctm_tag_link;
  channel_tag_t *ctm_tag;

  int ctm_mark;

} channel_tag_mapping_t;

/*
 * Service mappings
 */
typedef struct channel_service_mapping {
  LIST_ENTRY(channel_service_mapping) csm_chn_link;
  LIST_ENTRY(channel_service_mapping) csm_svc_link;
  
  struct channel *csm_chn;
  struct service *csm_svc;

  int csm_mark;
} channel_service_mapping_t;

extern const idclass_t channel_class;

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
  (channel_t*)idnode_find(u, &channel_class)

channel_t *channel_find_by_id(uint32_t id);

channel_t *channel_find_by_number(int no);

#define channel_find channel_find_by_uuid

int channel_set_tags_by_list ( channel_t *ch, htsmsg_t *tags );
int channel_set_services_by_list ( channel_t *ch, htsmsg_t *svcs );

channel_tag_t *channel_tag_find_by_name(const char *name, int create);

channel_tag_t *channel_tag_find_by_identifier(uint32_t id);

int channel_tag_map(channel_t *ch, channel_tag_t *ct);

void channel_save(channel_t *ch);

const char *channel_get_name ( channel_t *ch );
int channel_set_name ( channel_t *ch, const char *s );

int channel_get_number ( channel_t *ch );

const char *channel_get_icon ( channel_t *ch );
int channel_set_icon ( channel_t *ch, const char *icon );

#define channel_get_uuid(ch) idnode_uuid_as_str(&ch->ch_id)

#define channel_get_id(ch)   idnode_get_short_uuid((&ch->ch_id))

#endif /* CHANNELS_H */
