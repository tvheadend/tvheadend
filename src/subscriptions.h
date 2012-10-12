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

extern struct th_subscription_list subscriptions;

#define SUBSCRIPTION_RAW_MPEGTS 0x1

typedef struct th_subscription {

  int ths_id;
  
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
  int ths_bytes;     // Reset every second to get aprox. bandwidth

  streaming_target_t ths_input;

  streaming_target_t *ths_output;

  int ths_flags;

  streaming_message_t *ths_start_message;

  char *ths_hostname;
  char *ths_username;
  char *ths_client;

  // Ugly ugly ugly to refer DVB code here

  LIST_ENTRY(th_subscription) ths_tdmi_link;
  struct th_dvb_mux_instance *ths_tdmi;

} th_subscription_t;


/**
 * Prototypes
 */
void subscription_init(void);

void subscription_unsubscribe(th_subscription_t *s);

void subscription_set_weight(th_subscription_t *s, unsigned int weight);

void subscription_reschedule(void);

th_subscription_t *subscription_create_from_channel(struct channel *ch,
						    unsigned int weight,
						    const char *name,
						    streaming_target_t *st,
						    int flags,
						    const char *hostname,
						    const char *username,
						    const char *client);


th_subscription_t *subscription_create_from_service(struct service *t,
						    const char *name,
						    streaming_target_t *st,
						    int flags,
						    const char *hostname,
						    const char *username,
						    const char *client);

th_subscription_t *subscription_create(int weight, const char *name,
				       streaming_target_t *st,
				       int flags, st_callback_t *cb,
				       const char *hostname,
				       const char *username,
				       const char *client);

void subscription_change_weight(th_subscription_t *s, int weight);

void subscription_set_speed
  (th_subscription_t *s, int32_t speed );

void subscription_set_skip
  (th_subscription_t *s, const streaming_skip_t *skip);

void subscription_stop(th_subscription_t *s);

void subscription_unlink_service(th_subscription_t *s, int reason);

void subscription_dummy_join(const char *id, int first);

int subscriptions_active(void);

struct htsmsg;
struct htsmsg *subscription_create_msg(th_subscription_t *s);

#endif /* SUBSCRIPTIONS_H */
