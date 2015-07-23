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

#include "service.h"

struct profile_chain;

extern struct th_subscription_list subscriptions;

#define SUBSCRIPTION_NONE       0x000
#define SUBSCRIPTION_MPEGTS     0x001
#define SUBSCRIPTION_PACKET     0x002
#define SUBSCRIPTION_TYPE_MASK  0x00f
#define SUBSCRIPTION_STREAMING  0x010
#define SUBSCRIPTION_RESTART    0x020
#define SUBSCRIPTION_CONTACCESS 0x040
#define SUBSCRIPTION_ONESHOT    0x080
#define SUBSCRIPTION_TABLES     0x100
#define SUBSCRIPTION_MINIMAL    0x200
#define SUBSCRIPTION_INITSCAN  0x1000 ///< for mux subscriptions
#define SUBSCRIPTION_IDLESCAN  0x2000 ///< for mux subscriptions
#define SUBSCRIPTION_USERSCAN  0x4000 ///< for mux subscriptions
#define SUBSCRIPTION_EPG       0x8000 ///< for mux subscriptions
#define SUBSCRIPTION_HTSP     0x10000

/* Some internal priorities */
#define SUBSCRIPTION_PRIO_KEEP        1 ///< Keep input rolling
#define SUBSCRIPTION_PRIO_SCAN_IDLE   2 ///< Idle scanning
#define SUBSCRIPTION_PRIO_SCAN_SCHED  3 ///< Scheduled scan
#define SUBSCRIPTION_PRIO_EPG         4 ///< EPG scanner
#define SUBSCRIPTION_PRIO_SCAN_INIT   5 ///< Initial scan
#define SUBSCRIPTION_PRIO_SCAN_USER   6 ///< User defined scan
#define SUBSCRIPTION_PRIO_MAPPER      7 ///< Channel mapper
#define SUBSCRIPTION_PRIO_MIN        10 ///< User defined / Normal levels

typedef struct th_subscription {

  int ths_id;
  
  LIST_ENTRY(th_subscription) ths_global_link;
  LIST_ENTRY(th_subscription) ths_remove_link;

  struct profile_chain *ths_prch;

  int ths_weight;

  enum {
    SUBSCRIPTION_IDLE,
    SUBSCRIPTION_TESTING_SERVICE,
    SUBSCRIPTION_GOT_SERVICE,
    SUBSCRIPTION_BAD_SERVICE,
    SUBSCRIPTION_ZOMBIE
  } ths_state;

  int ths_testing_error;

  LIST_ENTRY(th_subscription) ths_channel_link;
  struct channel *ths_channel;          /* May be NULL if channel has been
					   destroyed during the
					   subscription */

  LIST_ENTRY(th_subscription) ths_service_link;
  struct service *ths_service;   /* if NULL, ths_service_link
					   is not linked */

  struct tvh_input *ths_source;  /* if NULL, all sources are allowed */

  char *ths_title; /* display title */
  time_t ths_start;  /* time when subscription started */
  int ths_total_err; /* total errors during entire subscription */
  int ths_bytes_in;   // Reset every second to get aprox. bandwidth (in)
  int ths_bytes_out; // Reset every second to get approx bandwidth (out)

  streaming_target_t ths_input;

  streaming_target_t *ths_output;

  int ths_flags;
  int ths_timeout;

  time_t ths_last_find;
  int ths_last_error;

  streaming_message_t *ths_start_message;

  char *ths_hostname;
  char *ths_username;
  char *ths_client;
  char *ths_dvrfile;

  /**
   * This is the list of service candidates we have
   */
  service_instance_list_t ths_instances;
  struct service_instance *ths_current_instance;

  /**
   * Postpone
   */
  int    ths_postpone;
  time_t ths_postpone_end;

  /*
   * MPEG-TS mux chain
   */
#if ENABLE_MPEGTS
  service_t *ths_raw_service;
  LIST_ENTRY(th_subscription) ths_mux_link;
#endif

} th_subscription_t;


/**
 * Prototypes
 */
void subscription_init(void);

void subscription_done(void);

void subscription_unsubscribe(th_subscription_t *s, int quiet);

void subscription_set_weight(th_subscription_t *s, unsigned int weight);

void subscription_reschedule(void);

th_subscription_t *
subscription_create_from_channel(struct profile_chain *prch,
                                 struct tvh_input *ti,
				 unsigned int weight,
				 const char *name,
				 int flags,
				 const char *hostname,
				 const char *username,
				 const char *client,
				 int *error);


th_subscription_t *
subscription_create_from_service(struct profile_chain *prch,
                                 struct tvh_input *ti,
                                 unsigned int weight,
				 const char *name,
				 int flags,
				 const char *hostname,
				 const char *username,
				 const char *client,
				 int *error);

#if ENABLE_MPEGTS
struct tvh_input;
th_subscription_t *
subscription_create_from_mux(struct profile_chain *prch,
                             struct tvh_input *ti,
                             unsigned int weight,
                             const char *name,
                             int flags,
                             const char *hostname,
                             const char *username,
                             const char *client,
                             int *error);
#endif

th_subscription_t *subscription_create(struct profile_chain *prch,
                                       int weight, const char *name,
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


static inline int subscriptions_active(void)
  { return LIST_FIRST(&subscriptions) != NULL; }

struct htsmsg;
struct htsmsg *subscription_create_msg(th_subscription_t *s);

#endif /* SUBSCRIPTIONS_H */
