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



/*
 * Subscription
 */

typedef enum {
  TRANSPORT_AVAILABLE,
  TRANSPORT_UNAVAILABLE,
} subscription_event_t;

typedef void (subscription_callback_t)(struct th_subscription *s,
				       subscription_event_t event,
				       void *opaque);

typedef void (subscription_raw_input_t)(struct th_subscription *s,
					void *data, int len,
					th_stream_t *st,
					void *opaque);



typedef void (subscription_status_callback_t)(struct th_subscription *s,
					      int status,
					      void *opaque);

typedef struct th_subscription {
  LIST_ENTRY(th_subscription) ths_global_link;
  int ths_weight;

  LIST_ENTRY(th_subscription) ths_channel_link;
  struct channel *ths_channel;          /* May be NULL if channel has been
					   destroyed during the
					   subscription */

  LIST_ENTRY(th_subscription) ths_transport_link;
  struct th_transport *ths_transport;   /* if NULL, ths_transport_link
					   is not linked */

  LIST_ENTRY(th_subscription) ths_subscriber_link; /* Caller is responsible
						      for this link */

  char *ths_title; /* display title */
  time_t ths_start;  /* time when subscription started */
  int ths_total_err; /* total errors during entire subscription */

  subscription_callback_t *ths_callback;
  void *ths_opaque;
  uint32_t ths_u32;

  subscription_raw_input_t *ths_raw_input;

  th_muxer_t *ths_muxer;

  subscription_status_callback_t *ths_status_callback;

} th_subscription_t;


/**
 * Prototypes
 */
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
