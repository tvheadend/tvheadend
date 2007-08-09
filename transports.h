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


void subscription_unsubscribe(th_subscription_t *s);
void subscription_set_weight(th_subscription_t *s, unsigned int weight);

unsigned int transport_compute_weight(struct th_transport_list *head);

void transport_flush_subscribers(th_transport_t *t);

void transport_recv_tsb(th_transport_t *t, int pid, uint8_t *tsb);

void transport_monitor_init(th_transport_t *t);

th_subscription_t *channel_subscribe(th_channel_t *ch, void *opaque,
				     void (*ths_callback)
				     (struct th_subscription *s, 
				      uint8_t *pkt, pidinfo_t *pi,
				      int streamindex),
				     unsigned int weight,
				     const char *name);

#endif /* TRANSPORTS_H */
