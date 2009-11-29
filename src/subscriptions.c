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

#include <assert.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "tvhead.h"
#include "transports.h"
#include "subscriptions.h"
#include "streaming.h"

struct th_subscription_list subscriptions;
static gtimer_t subscription_reschedule_timer;

/**
 *
 */
int
subscriptions_active(void)
{
  return LIST_FIRST(&subscriptions) != NULL;
}



/**
 *
 */
static int
subscription_sort(th_subscription_t *a, th_subscription_t *b)
{
  return b->ths_weight - a->ths_weight;
}


/**
 * The transport is producing output. Thus, we may link our subscription
 * to it.
 */
static void
subscription_link_transport(th_subscription_t *s, th_transport_t *t)
{
  streaming_message_t *sm;

  s->ths_transport = t;
  LIST_INSERT_HEAD(&t->tht_subscriptions, s, ths_transport_link);


  pthread_mutex_lock(&t->tht_stream_mutex);

  // Link to transport output
  streaming_target_connect(&t->tht_streaming_pad, &s->ths_input);

  if(LIST_FIRST(&t->tht_components) != NULL) {

    // Send a START message to the subscription client
    sm = streaming_msg_create_data(SMT_START,
				   transport_build_stream_start(t));

    streaming_target_deliver(s->ths_output, sm);

    // Send a TRANSPORT_STATUS message to the subscription client
    if(t->tht_feed_status != TRANSPORT_FEED_UNKNOWN) {
      sm = streaming_msg_create_code(SMT_TRANSPORT_STATUS, t->tht_feed_status);
      streaming_target_deliver(s->ths_output, sm);
    }
  }
  pthread_mutex_unlock(&t->tht_stream_mutex);
}


/**
 * Called from transport code
 */
void
subscription_unlink_transport(th_subscription_t *s)
{
  streaming_message_t *sm;
  th_transport_t *t = s->ths_transport;

  pthread_mutex_lock(&t->tht_stream_mutex);

  // Unlink from transport output
  streaming_target_disconnect(&t->tht_streaming_pad, &s->ths_input);

  if(LIST_FIRST(&t->tht_components) != NULL) {
    // Send a STOP message to the subscription client
    sm = streaming_msg_create_code(SMT_STOP, 0);
    streaming_target_deliver(s->ths_output, sm);
  }

  pthread_mutex_unlock(&t->tht_stream_mutex);

  LIST_REMOVE(s, ths_transport_link);
  s->ths_transport = NULL;
}


/**
 *
 */
static void
subscription_reschedule(void *aux)
{
  th_subscription_t *s;
  th_transport_t *t;
  streaming_message_t *sm;

  lock_assert(&global_lock);

  gtimer_arm(&subscription_reschedule_timer, subscription_reschedule, NULL, 2);

  LIST_FOREACH(s, &subscriptions, ths_global_link) {
    if(s->ths_transport != NULL)
      continue; /* Got a transport, we're happy */

    if(s->ths_channel == NULL)
      continue; /* stale entry, channel has been destroyed */

    t = transport_find(s->ths_channel, s->ths_weight);

    if(t == NULL) {
      /* No transport available */

      sm = streaming_msg_create(SMT_NOSOURCE);
      streaming_target_deliver(s->ths_output, sm);
      continue;
    }

    subscription_link_transport(s, t);
  }
}

/**
 *
 */
void 
subscription_unsubscribe(th_subscription_t *s)
{
  th_transport_t *t = s->ths_transport;

  lock_assert(&global_lock);

  LIST_REMOVE(s, ths_global_link);

  if(s->ths_channel != NULL) {
    LIST_REMOVE(s, ths_channel_link);
    tvhlog(LOG_INFO, "subscription", "\"%s\" unsubscribing from \"%s\"",
	   s->ths_title, s->ths_channel->ch_name);
  } else {
    tvhlog(LOG_INFO, "subscription", "\"%s\" unsubscribing",
	   s->ths_title);
  }

  if(t != NULL)
    transport_remove_subscriber(t, s);

  free(s->ths_title);
  free(s);

  subscription_reschedule(NULL);
}


/**
 * This callback is invoked when we receive data and status updates from
 * the currently bound transport
 */
static void
subscription_input(void *opauqe, streaming_message_t *sm)
{
  th_subscription_t *s = opauqe;
 streaming_target_deliver(s->ths_output, sm);
}



/**
 *
 */
static th_subscription_t *
subscription_create(int weight, const char *name, streaming_target_t *st,
		    int flags)
{
  th_subscription_t *s = calloc(1, sizeof(th_subscription_t));
  int reject = 0;

  if(flags & SUBSCRIPTION_RAW_MPEGTS)
    reject |= SMT_TO_MASK(SMT_PACKET);  // Reject parsed frames
  else
    reject |= SMT_TO_MASK(SMT_MPEGTS);  // Reject raw mpegts


  streaming_target_init(&s->ths_input, subscription_input, s, reject);

  s->ths_weight            = weight;
  s->ths_title             = strdup(name);
  s->ths_total_err         = 0;
  s->ths_output            = st;
  s->ths_flags             = flags;

  time(&s->ths_start);
  LIST_INSERT_SORTED(&subscriptions, s, ths_global_link, subscription_sort);

  return s;
}


/**
 *
 */
th_subscription_t *
subscription_create_from_channel(channel_t *ch, unsigned int weight, 
				 const char *name, streaming_target_t *st,
				 int flags)
{
  th_subscription_t *s = subscription_create(weight, name, st, flags);

  s->ths_channel = ch;
  LIST_INSERT_HEAD(&ch->ch_subscriptions, s, ths_channel_link);
  s->ths_transport = NULL;

  subscription_reschedule(NULL);

  if(s->ths_transport == NULL) {
    tvhlog(LOG_NOTICE, "subscription", 
	   "No transponder available for subscription \"%s\" "
	   "to channel \"%s\"",
	   s->ths_title, ch->ch_name);
  } else {
    source_info_t si;

    s->ths_transport->tht_setsourceinfo(s->ths_transport, &si);

    tvhlog(LOG_INFO, "subscription", 
	   "\"%s\" subscribing on \"%s\", weight: %d, adapter: \"%s\", "
	   "network: \"%s\", mux: \"%s\", provider: \"%s\", "
	   "service: \"%s\", quality: %d",
	   s->ths_title, ch->ch_name, weight,
	   si.si_adapter  ?: "<N/A>",
	   si.si_network  ?: "<N/A>",
	   si.si_mux      ?: "<N/A>",
	   si.si_provider ?: "<N/A>",
	   si.si_service  ?: "<N/A>",
	   s->ths_transport->tht_quality_index(s->ths_transport));

    transport_source_info_free(&si);
  }
  return s;
}


/**
 *
 */
th_subscription_t *
subscription_create_from_transport(th_transport_t *t, const char *name,
				   streaming_target_t *st, int flags)
{
  th_subscription_t *s = subscription_create(INT32_MAX, name, st, flags);
  source_info_t si;

  if(t->tht_status != TRANSPORT_RUNNING)
    transport_start(t, INT32_MAX, 1);

  t->tht_setsourceinfo(t, &si);

  tvhlog(LOG_INFO, "subscription", 
	 "\"%s\" direct subscription to adapter: \"%s\", "
	 "network: \"%s\", mux: \"%s\", provider: \"%s\", "
	 "service: \"%s\", quality: %d",
	 s->ths_title,
	 si.si_adapter  ?: "<N/A>",
	 si.si_network  ?: "<N/A>",
	 si.si_mux      ?: "<N/A>",
	 si.si_provider ?: "<N/A>",
	 si.si_service  ?: "<N/A>",
	 t->tht_quality_index(t));
  transport_source_info_free(&si);

  subscription_link_transport(s, t);
  return s;
}


/**
 *
 */
static void
dummy_callback(void *opauqe, streaming_message_t *sm)
{
  switch(sm->sm_type) {
  case SMT_START:
    fprintf(stderr, "dummysubscription START\n");
    break;
  case SMT_STOP:
    fprintf(stderr, "dummysubscription STOP\n");
    break;

  case SMT_TRANSPORT_STATUS:
    fprintf(stderr, "dummsubscription: %s\n", 
	    transport_feed_status_to_text(sm->sm_code));
    break;
  default:
    break;
  }

  streaming_msg_free(sm);
}

static gtimer_t dummy_sub_timer;

/**
 *
 */
static void
dummy_retry(void *opaque)
{
  subscription_dummy_join(opaque, 0);
  free(opaque);
}

/**
 *
 */
void
subscription_dummy_join(const char *id, int first)
{
  th_transport_t *t = transport_find_by_identifier(id);
  streaming_target_t *st;

  if(first) {
    gtimer_arm(&dummy_sub_timer, dummy_retry, strdup(id), 2);
    return;
  }

  if(t == NULL) {
    tvhlog(LOG_ERR, "subscription", 
	   "Unable to dummy join %s, transport not found, retrying...", id);

    gtimer_arm(&dummy_sub_timer, dummy_retry, strdup(id), 1);
    return;
  }

  st = calloc(1, sizeof(streaming_target_t));
  streaming_target_init(st, dummy_callback, NULL, 0);
  subscription_create_from_transport(t, "dummy", st, 0);

  tvhlog(LOG_NOTICE, "subscription", 
	 "Dummy join %s ok", id);
}
