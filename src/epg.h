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



typedef struct epg_episode {

  uint16_t ee_season;
  uint16_t ee_episode;
  uint16_t ee_part;

  char *ee_onscreen;
} epg_episode_t;


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

  epg_episode_t e_episode;

} event_t;


/**
 * Prototypes
 */
void epg_init(void);

/**
 * All the epg_event_set_ function return 1 if it actually changed
 * the EPG records. otherwise it returns 0.
 *
 * If the caller detects that something has changed, it should call
 * epg_event_updated().
 *
 * There reason to put the burden on the caller is that the caller
 * can combine multiple set()'s into one update
 *
 */
int epg_event_set_title(event_t *e, const char *title)
       __attribute__ ((warn_unused_result));

int epg_event_set_desc(event_t *e, const char *desc)
       __attribute__ ((warn_unused_result));

int epg_event_set_ext_desc(event_t *e, int ext_dn, const char *desc)
       __attribute__ ((warn_unused_result));

int epg_event_set_ext_item(event_t *e, int ext_dn, const char *item)
       __attribute__ ((warn_unused_result));

int epg_event_set_ext_text(event_t *e, int ext_dn, const char *text)
       __attribute__ ((warn_unused_result));

int epg_event_set_content_type(event_t *e, uint8_t type)
       __attribute__ ((warn_unused_result));

int epg_event_set_episode(event_t *e, epg_episode_t *ee)
       __attribute__ ((warn_unused_result));

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

#endif /* EPG_H */
