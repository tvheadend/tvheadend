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

extern epg_content_group_t *epg_content_groups[16];

void epg_init(void);

event_t *epg_event_find_by_time0(struct event_queue *q, time_t start);

event_t *epg_event_find_by_time(th_channel_t *ch, time_t start);

event_t *epg_event_find_by_tag(uint32_t id);

event_t *epg_event_get_current(th_channel_t *ch);

event_t *epg_event_build(struct event_queue *head, time_t start, int duration);

void epg_event_free(event_t *e);

void epg_event_set_title(event_t *e, const char *title);

void epg_event_set_desc(event_t *e, const char *desc);

void epg_update_event_by_id(th_channel_t *ch, uint16_t event_id, 
			    time_t start, int duration, const char *title,
			    const char *desc, epg_content_type_t *ect);

void epg_transfer_events(th_channel_t *ch, struct event_queue *src, 
			 const char *srcname, char *icon);

void event_time_txt(time_t start, int duration, char *out, int outlen);

event_t *epg_event_find_current_or_upcoming(th_channel_t *ch);

epg_content_type_t *epg_content_type_find_by_dvbcode(uint8_t dvbcode);

epg_content_group_t *epg_content_group_find_by_name(const char *name);

int epg_search(struct event_list *h, const char *title,
	       epg_content_group_t *s_ecg, th_channel_group_t *s_tcg,
	       th_channel_t *s_ch);

#endif /* EPG_H */
