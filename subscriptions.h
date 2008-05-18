/*
 *  tvheadend, subscription functions
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

#ifndef SUBSCRIPTIONS_H
#define SUBSCRIPTIONS_H

void subscription_unsubscribe(th_subscription_t *s);

void subscription_set_weight(th_subscription_t *s, unsigned int weight);

th_subscription_t *subscription_create(channel_t *ch, unsigned int weight,
				       const char *name, 
				       subscription_callback_t *cb,
				       void *opaque,
				       uint32_t u32);

void subscriptions_init(void);

void subscription_stop(th_subscription_t *s);

void subscription_reschedule(void);

#endif /* SUBSCRIPTIONS_H */
