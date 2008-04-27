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

#ifndef XMLTV_H
#define XMLTV_H

#define XMLTV_GRAB_WORKING       0
#define XMLTV_GRAB_UNCONFIGURED  1
#define XMLTV_GRAB_DYSFUNCTIONAL 2
#define XMLTV_GRAB_OK            3



LIST_HEAD(xmltv_grabber_list,   xmltv_grabber);
TAILQ_HEAD(xmltv_grabber_queue, xmltv_grabber);

TAILQ_HEAD(xmltv_channel_queue, xmltv_channel);

typedef struct xmltv_grabber {
  LIST_ENTRY(xmltv_grabber) xg_link;
  char *xg_path;
  char *xg_title;
  char *xg_identifier;

  TAILQ_ENTRY(xmltv_grabber) xg_work_link;
  int xg_on_work_link;

  int xg_enabled;

  int xg_status;

  dtimer_t xg_grab_timer;
  dtimer_t xg_xfer_timer;

  struct xmltv_channel_queue xg_channels;

  pthread_mutex_t xg_mutex;

} xmltv_grabber_t;



/**
 * A channel in the XML-TV world
 */
typedef struct xmltv_channel {
  TAILQ_ENTRY(xmltv_channel) xc_link;
  char *xc_name;
  char *xc_displayname;

  char *xc_bestmatch; /* Best matching channel */

  char *xc_channel;

  int xc_disabled;

  char *xc_icon_url;

  struct event_queue xc_events;

  int xc_updated;

} xmltv_channel_t;


#define XMLTVSTATUS_STARTING                0
#define XMLTVSTATUS_FIND_GRABBERS_NOT_FOUND 1
#define XMLTVSTATUS_RUNNING                 2

extern int xmltv_globalstatus;
extern struct xmltv_grabber_list xmltv_grabbers;

void xmltv_init(void);

const char *xmltv_grabber_status(xmltv_grabber_t *xg);

void xmltv_grabber_enable(xmltv_grabber_t *xg);

xmltv_grabber_t *xmltv_grabber_find(const char *id);

const char *xmltv_grabber_status_long(xmltv_grabber_t *xg);

void xmltv_config_save(void);

#endif /* __XMLTV_H__ */
