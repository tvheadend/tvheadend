/*
 *  tvheadend, transport functions
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

#ifndef TRANSPORTS_H
#define TRANSPORTS_H

#include "channels.h"
#include "htsmsg.h"
#include "subscriptions.h"

#define TRANSPORT_NOSTART_NO_HARDWARE     1
#define TRANSPORT_NOSTART_MUX_NOT_ENABLED 2
#define TRANSPORT_NOSTART_NOT_FREE        3
#define TRANSPORT_NOSTART_TUNING_FAILED   4
#define TRANSPORT_NOSTART_SVC_NOT_ENABLED 5
#define TRANSPORT_NOSTART_BAD_SIGNAL      6
#define TRANSPORT_NOSTART_NO_ACCESS       7
#define TRANSPORT_NOSTART_NO_DESCRAMBLER  8
#define TRANSPORT_NOSTART_NO_INPUT        9
#define TRANSPORT_NOSTART_NO_SIGNAL       10

void transport_init(void);

unsigned int transport_compute_weight(struct th_transport_list *head);

int transport_start(th_transport_t *t, unsigned int weight, int force_start,
		    int wait_for_ok);

th_transport_t *transport_create(const char *identifier, int type,
				 int source_type);

void transport_unref(th_transport_t *t);

void transport_ref(th_transport_t *t);

th_transport_t *transport_find_by_identifier(const char *identifier);

void transport_map_channel(th_transport_t *t, channel_t *ch, int save);

th_transport_t *transport_find(channel_t *ch, unsigned int weight,
			       const char *loginfo, int *errorp);

th_stream_t *transport_stream_find(th_transport_t *t, int pid);

th_stream_t *transport_stream_create(th_transport_t *t, int pid,
				     streaming_component_type_t type);

void transport_set_priority(th_transport_t *t, int prio);

void transport_settings_write(th_transport_t *t);

const char *transport_servicetype_txt(th_transport_t *t);

int transport_is_tv(th_transport_t *t);

void transport_destroy(th_transport_t *t);

void transport_remove_subscriber(th_transport_t *t, th_subscription_t *s);

void transport_set_streaming_status_flags(th_transport_t *t, int flag);

static inline th_stream_t *
transport_find_stream_by_pid(th_transport_t *t, int pid)
{
  th_stream_t *st;

  LIST_FOREACH(st, &t->tht_components, st_link) {
    if(st->st_pid == pid)
      return st;
  }
  return NULL;
}

struct streaming_start;
struct streaming_start *transport_build_stream_start(th_transport_t *t);

void transport_set_enable(th_transport_t *t, int enabled);

void transport_restart(th_transport_t *t, int had_components);

void transport_stream_destroy(th_transport_t *t, th_stream_t *st);

void transport_request_save(th_transport_t *t);

void transport_source_info_free(source_info_t *si);

void transport_source_info_copy(source_info_t *dst, source_info_t *src);

void transport_make_nicename(th_transport_t *t);

const char *transport_nicename(th_transport_t *t);

const char *transport_component_nicename(th_stream_t *st);

const char *transport_nostart2txt(int code);

const char *transport_tss2text(int flags);

static inline int transport_tss_is_error(int flags)
{
  return flags & TSS_ERRORS ? 1 : 0;
}

#endif /* TRANSPORTS_H */
