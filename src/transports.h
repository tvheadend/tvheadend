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

unsigned int transport_compute_weight(struct th_transport_list *head);

int transport_start(th_transport_t *t, unsigned int weight, int force_start);

th_transport_t *transport_create(const char *identifier, int type,
				 int source_type);

void transport_unref(th_transport_t *t);

void transport_ref(th_transport_t *t);

th_transport_t *transport_find_by_identifier(const char *identifier);

void transport_map_channel(th_transport_t *t, channel_t *ch);

void transport_unmap_channel(th_transport_t *t);

th_transport_t *transport_find(channel_t *ch, unsigned int weight);

th_stream_t *transport_add_stream(th_transport_t *t, int pid,
				  streaming_component_type_t type);

void transport_set_priority(th_transport_t *t, int prio);

void transport_settings_write(th_transport_t *t);

const char *transport_servicetype_txt(th_transport_t *t);

int transport_is_tv(th_transport_t *t);

int transport_is_available(th_transport_t *t);

void transport_destroy(th_transport_t *t);

void transport_set_feed_status(th_transport_t *t, 
			       transport_feed_status_t newstatus);

const char *transport_status_to_text(int status);

void transport_remove_subscriber(th_transport_t *t, th_subscription_t *s);

//void transport_link_muxer(th_transport_t *t, th_muxer_t *tm);

//void transport_unlink_muxer(th_muxer_t *tm);

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

htsmsg_t *transport_build_stream_start_msg(th_transport_t *t);


#endif /* TRANSPORTS_H */
