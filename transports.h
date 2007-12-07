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

#include <ffmpeg/avcodec.h>

unsigned int transport_compute_weight(struct th_transport_list *head);

void transport_stop(th_transport_t *t, int flush_subscriptions);

void transport_monitor_init(th_transport_t *t);

int transport_set_channel(th_transport_t *th, th_channel_t *ch);

void transport_link(th_transport_t *t, th_channel_t *ch);

th_transport_t *transport_find(th_channel_t *ch, unsigned int weight);

th_stream_t *transport_add_stream(th_transport_t *t, int pid,
				  tv_streamtype_t type);

void transport_set_priority(th_transport_t *t, int prio);

void transport_move(th_transport_t *t, th_channel_t *ch);

#endif /* TRANSPORTS_H */
