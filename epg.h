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

void epg_init(void);


void epg_event_set_title(event_t *e, const char *title);

void epg_event_set_desc(event_t *e, const char *desc);

void epg_event_set_content_type(event_t *e, epg_content_type_t *ect);

event_t *epg_event_find_by_start(channel_t *ch, time_t start, int create);

event_t *epg_event_find_by_time(channel_t *ch, time_t t);

void epg_event_destroy(event_t *e);

void epg_destroy_by_channel(channel_t *ch);


/**
 *
 */
epg_content_type_t *epg_content_type_find_by_dvbcode(uint8_t dvbcode);

epg_content_group_t *epg_content_group_find_by_name(const char *name);

#endif /* EPG_H */
