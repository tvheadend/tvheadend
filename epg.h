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


/**
 * EPG content group
 *
 * Based on the content types defined in EN 300 468
 */


typedef struct epg_content_group {
  const char *ecg_name;
  struct epg_content_type *ecg_types[16];
} epg_content_group_t;

typedef struct epg_content_type {
  const char *ect_name;
  struct event_list ect_events;
  epg_content_group_t *ect_group;
} epg_content_type_t;

/*
 * EPG event
 */
typedef struct event {
  struct channel *e_channel;
  RB_ENTRY(event) e_channel_link;

  int e_refcount;

  LIST_ENTRY(event) e_content_type_link;
  epg_content_type_t *e_content_type;

  time_t e_start;  /* UTC time */
  int e_duration;  /* in seconds */

  const char *e_title;   /* UTF-8 encoded */
  const char *e_desc;    /* UTF-8 encoded */

  int e_source; /* higer is better, and we never downgrade */

#define EVENT_SRC_XMLTV 1
#define EVENT_SRC_DVB   2

} event_t;


/**
 * Prototypes
 */
void epg_init(void);

void epg_event_set_title(event_t *e, const char *title);

void epg_event_set_desc(event_t *e, const char *desc);

void epg_event_set_duration(event_t *e, int duration);

void epg_event_set_content_type(event_t *e, epg_content_type_t *ect);

event_t *epg_event_find_by_start(channel_t *ch, time_t start, int create);

event_t *epg_event_find_by_time(channel_t *ch, time_t t);

void epg_unlink_from_channel(channel_t *ch);


/**
 *
 */
epg_content_type_t *epg_content_type_find_by_dvbcode(uint8_t dvbcode);

epg_content_group_t *epg_content_group_find_by_name(const char *name);

#endif /* EPG_H */
