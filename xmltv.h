/*
 *  Electronic Program Guide - xmltv interface
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

#ifndef EPG_XMLTV_H
#define EPG_XMLTV_H

#include "channels.h"

RB_HEAD(xmltv_channel_tree, xmltv_channel);
LIST_HEAD(xmltv_channel_list, xmltv_channel);

/**
 * A channel in the XML-TV world
 */
typedef struct xmltv_channel {
  RB_ENTRY(xmltv_channel) xc_link;
  char *xc_identifier;

  LIST_ENTRY(xmltv_channel) xc_displayname_link;
  char *xc_displayname;

  char *xc_icon;

  struct channel_list xc_channels;  /* Target channel(s) */

} xmltv_channel_t;

void xmltv_init(void);

xmltv_channel_t *xmltv_channel_find(const char *id, int create);

xmltv_channel_t *xmltv_channel_find_by_displayname(const char *name);

htsmsg_t *xmltv_list_grabbers(void);

const char *xmltv_get_current_grabber(void);
void xmltv_set_current_grabber(const char *path);

extern struct xmltv_channel_list xmltv_displaylist;
extern uint32_t xmltv_grab_interval;
extern pthread_mutex_t xmltv_mutex;

#endif /* EPG_XMLTV_H__ */
