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

LIST_HEAD(channel_tag_mapping_list, channel_tag_mapping);
TAILQ_HEAD(channel_tag_queue, channel_tag);

extern struct channel_tag_queue channel_tags;


/*
 * Channel definition
 */ 
typedef struct channel {
  
  int ch_refcount;
  int ch_zombie;

  RB_ENTRY(channel) ch_name_link;
  char *ch_name;
  char *ch_sname;

  RB_ENTRY(channel) ch_identifier_link;
  int ch_id;		    

  LIST_HEAD(, service) ch_services;
  LIST_HEAD(, th_subscription) ch_subscriptions;

  struct event_tree ch_epg_events;
  struct event *ch_epg_current;
  struct event *ch_epg_next;

  gtimer_t ch_epg_timer_head;
  gtimer_t ch_epg_timer_current;
  int ch_dvr_extra_time_pre;
  int ch_dvr_extra_time_post;
  int ch_number;  // User configurable number
  char *ch_icon;

  struct dvr_entry_list ch_dvrs;
  
  struct dvr_autorec_entry_list ch_autorecs;

  struct xmltv_channel *ch_xc;
  LIST_ENTRY(channel) ch_xc_link;

  struct channel_tag_mapping_list ch_ctms;

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



void channels_init(void);

channel_t *channel_find_by_name(const char *name, int create, int number);

channel_t *channel_find_by_identifier(int id);

void channel_set_teletext_rundown(channel_t *ch, int v);

void channel_settings_write(channel_t *ch);

int channel_rename(channel_t *ch, const char *newname);

void channel_delete(channel_t *ch);

void channel_merge(channel_t *dst, channel_t *src);

void channel_set_epg_postpre_time(channel_t *ch, int pre, int mins);

void channel_set_number(channel_t *ch, int number);

void channel_set_icon(channel_t *ch, const char *icon);

struct xmltv_channel;
void channel_set_xmltv_source(channel_t *ch, struct xmltv_channel *xc);

void channel_set_tags_from_list(channel_t *ch, const char *maplist);

channel_tag_t *channel_tag_find_by_name(const char *name, int create);

channel_tag_t *channel_tag_find_by_identifier(uint32_t id);

int channel_tag_map(channel_t *ch, channel_tag_t *ct, int check);

void channel_save(channel_t *ch);

extern struct channel_list channels_not_xmltv_mapped;

#endif /* CHANNELS_H */
