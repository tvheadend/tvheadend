/*
 *  tvheadend, subscription functions
 *  Copyright (C) 2007 Andreas �man
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

#define SUBSCRIPTION_RAW_MPEGTS 0x1
#define SUBSCRIPTION_PMT 0x1

typedef struct th_subscription {
  LIST_ENTRY(th_subscription) ths_global_link;
  int ths_weight;

  enum {
    SUBSCRIPTION_IDLE,
    SUBSCRIPTION_TESTING_SERVICE,
    SUBSCRIPTION_GOT_SERVICE,
    SUBSCRIPTION_BAD_SERVICE,
  } ths_state;

  int ths_testing_error;

  LIST_ENTRY(th_subscription) ths_channel_link;
  struct channel *ths_channel;          /* May be NULL if channel has been
					   destroyed during the
					   subscription */

  LIST_ENTRY(th_subscription) ths_service_link;
  struct service *ths_service;   /* if NULL, ths_service_link
					   is not linked */

  char *ths_title; /* display title */
  time_t ths_start;  /* time when subscription started */
  int ths_total_err; /* total errors during entire subscription */

  streaming_target_t ths_input;

  streaming_target_t *ths_output;

  int ths_flags;

  streaming_message_t *ths_start_message;

} th_subscription_t;


/**
 * Prototypes
 */
void subscription_unsubscribe(th_subscription_t *s);

void subscription_set_weight(th_subscription_t *s, unsigned int weight);

void subscription_reschedule(void);

th_subscription_t *subscription_create_from_channel(struct channel *ch,
						    unsigned int weight,
						    const char *name,
						    streaming_target_t *st,
						    int flags);


th_subscription_t *subscription_create_from_service(struct service *t,
						    const char *name,
						    streaming_target_t *st,
						    int flags, int direct);

void subscription_change_weight(th_subscription_t *s, int weight);

void subscription_stop(th_subscription_t *s);

void subscription_unlink_service(th_subscription_t *s, int reason);

void subscription_dummy_join(const char *id, int first);

int subscriptions_active(void);

#endif /* SUBSCRIPTIONS_H */
