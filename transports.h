/*
 *  tvheadend, transport and subscription functions
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

void subscription_unsubscribe(th_subscription_t *s);
void subscription_set_weight(th_subscription_t *s, unsigned int weight);

void subscription_lock(void);
void subscription_unlock(void);

unsigned int transport_compute_weight(struct th_transport_list *head);

void transport_flush_subscribers(th_transport_t *t);

void transport_recv_tsb(th_transport_t *t, int pid, uint8_t *tsb, 
			int scanpcr, int64_t pcr);

void transport_monitor_init(th_transport_t *t);

th_pid_t *transport_add_pid(th_transport_t *t, uint16_t pid,
			    tv_streamtype_t type);

int transport_set_channel(th_transport_t *th, th_channel_t *ch);
void transport_link(th_transport_t *t, th_channel_t *ch);

void transport_scheduler_init(void);

typedef void (subscription_callback_t)(struct th_subscription *s,
				       uint8_t *pkt, th_pid_t *pi,
				       int64_t pcr);

th_subscription_t *subscription_create(th_channel_t *ch, void *opaque,
				       subscription_callback_t *ths_callback,
				       unsigned int weight,
				       const char *name);


#endif /* TRANSPORTS_H */
