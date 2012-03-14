/*
 *  tvheadend, transport and subscription functions
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

#include "tvheadend.h"
#include "subscriptions.h"
#include "streaming.h"
#include "channels.h"
#include "service.h"

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
 * The service is producing output.
 */
static void
subscription_link_service(th_subscription_t *s, service_t *t)
{
  streaming_message_t *sm;
  s->ths_state = SUBSCRIPTION_TESTING_SERVICE;
 
  s->ths_service = t;
  LIST_INSERT_HEAD(&t->s_subscriptions, s, ths_service_link);

  pthread_mutex_lock(&t->s_stream_mutex);

  if(TAILQ_FIRST(&t->s_components) != NULL)
    s->ths_start_message =
      streaming_msg_create_data(SMT_START, service_build_stream_start(t));

  // Link to service output
  streaming_target_connect(&t->s_streaming_pad, &s->ths_input);


  if(s->ths_start_message != NULL && t->s_streaming_status & TSS_PACKETS) {

    s->ths_state = SUBSCRIPTION_GOT_SERVICE;

    // Send a START message to the subscription client
    streaming_target_deliver(s->ths_output, s->ths_start_message);
    s->ths_start_message = NULL;

    // Send status report
    sm = streaming_msg_create_code(SMT_SERVICE_STATUS, 
				   t->s_streaming_status);
    streaming_target_deliver(s->ths_output, sm);
  }

  pthread_mutex_unlock(&t->s_stream_mutex);
}


/**
 * Called from service code
 */
void
subscription_unlink_service(th_subscription_t *s, int reason)
{
  streaming_message_t *sm;
  service_t *t = s->ths_service;

  pthread_mutex_lock(&t->s_stream_mutex);

  // Unlink from service output
  streaming_target_disconnect(&t->s_streaming_pad, &s->ths_input);

  if(TAILQ_FIRST(&t->s_components) != NULL && 
     s->ths_state == SUBSCRIPTION_GOT_SERVICE) {
    // Send a STOP message to the subscription client
    sm = streaming_msg_create_code(SMT_STOP, reason);
    streaming_target_deliver(s->ths_output, sm);
  }

  pthread_mutex_unlock(&t->s_stream_mutex);

  LIST_REMOVE(s, ths_service_link);
  s->ths_service = NULL;
}


static void
subscription_reschedule_cb(void *aux)
{
  subscription_reschedule();
}

/**
 *
 */
void
subscription_reschedule(void)
{
  th_subscription_t *s;
  service_t *t, *skip;
  streaming_message_t *sm;
  char buf[128];
  int error;

  lock_assert(&global_lock);

  gtimer_arm(&subscription_reschedule_timer, 
	     subscription_reschedule_cb, NULL, 2);

  LIST_FOREACH(s, &subscriptions, ths_global_link) {
    if(s->ths_channel == NULL)
      continue; /* stale entry, channel has been destroyed */

    if(s->ths_service != NULL) {
      /* Already got a service */

      if(s->ths_state != SUBSCRIPTION_BAD_SERVICE)
	continue; /* And it seems to work ok, so we're happy */
      skip = s->ths_service;
      error = s->ths_testing_error;
      service_remove_subscriber(s->ths_service, s, s->ths_testing_error);
    } else {
      error = 0;
      skip = NULL;
    }

    snprintf(buf, sizeof(buf), "Subscription \"%s\"", s->ths_title);
    t = service_find(s->ths_channel, s->ths_weight, buf, &error, skip);

    if(t == NULL) {
      /* No service available */

      sm = streaming_msg_create_code(SMT_NOSTART, error);
      streaming_target_deliver(s->ths_output, sm);
      continue;
    }

    subscription_link_service(s, t);
  }
}

/**
 *
 */
void 
subscription_unsubscribe(th_subscription_t *s)
{
  service_t *t = s->ths_service;

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
    service_remove_subscriber(t, s, SM_CODE_OK);

  if(s->ths_start_message != NULL) 
    streaming_msg_free(s->ths_start_message);
 
  free(s->ths_title);
  free(s);

  subscription_reschedule();
}


/**
 * This callback is invoked when we receive data and status updates from
 * the currently bound service
 */
static void
subscription_input(void *opauqe, streaming_message_t *sm)
{
  th_subscription_t *s = opauqe;

  if(s->ths_state == SUBSCRIPTION_TESTING_SERVICE) {
    // We are just testing if this service is good

    if(sm->sm_type == SMT_START) {
      if(s->ths_start_message != NULL) 
	streaming_msg_free(s->ths_start_message);
      s->ths_start_message = sm;
      return;
    }

    if(sm->sm_type == SMT_SERVICE_STATUS &&
       sm->sm_code & (TSS_GRACEPERIOD | TSS_ERRORS)) {
      // No, mark our subscription as bad_service
      // the scheduler will take care of things
      s->ths_testing_error = tss2errcode(sm->sm_code);
      s->ths_state = SUBSCRIPTION_BAD_SERVICE;
      streaming_msg_free(sm);
      return;
    }

    if(sm->sm_type == SMT_SERVICE_STATUS &&
       sm->sm_code & TSS_PACKETS) {
      if(s->ths_start_message != NULL) {
	streaming_target_deliver(s->ths_output, s->ths_start_message);
	s->ths_start_message = NULL;
      }
      s->ths_state = SUBSCRIPTION_GOT_SERVICE;
    }
  }

  if(s->ths_state != SUBSCRIPTION_GOT_SERVICE) {
    if (!s->ths_input.st_clone && sm->sm_type == SMT_MPEGTS)
      return;
    streaming_msg_free(sm);
    return;
  }
  streaming_target_deliver(s->ths_output, sm);
}


/**
 *
 */
static void
subscription_input_direct(void *opauqe, streaming_message_t *sm)
{
  th_subscription_t *s = opauqe;
  streaming_target_deliver(s->ths_output, sm);
}



/**
 *
 */
static th_subscription_t *
subscription_create(int weight, const char *name, streaming_target_t *st,
		    int flags, int direct)
{
  th_subscription_t *s = calloc(1, sizeof(th_subscription_t));
  int reject = 0;

  if(flags & SUBSCRIPTION_RAW_MPEGTS)
    reject |= SMT_TO_MASK(SMT_PACKET);  // Reject parsed frames
  else
    reject |= SMT_TO_MASK(SMT_MPEGTS);  // Reject raw mpegts


  streaming_target_init(&s->ths_input, direct ? subscription_input_direct : 
			subscription_input, s, reject);
  if (flags & SUBSCRIPTION_RAW_MPEGTS)
    s->ths_input.st_clone = 0;  

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
  th_subscription_t *s = subscription_create(weight, name, st, flags, 0);

  s->ths_channel = ch;
  LIST_INSERT_HEAD(&ch->ch_subscriptions, s, ths_channel_link);
  s->ths_service = NULL;

  subscription_reschedule();

  if(s->ths_service == NULL) {
    tvhlog(LOG_NOTICE, "subscription", 
	   "No transponder available for subscription \"%s\" "
	   "to channel \"%s\"",
	   s->ths_title, ch->ch_name);
  } else {
    source_info_t si;

    s->ths_service->s_setsourceinfo(s->ths_service, &si);

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
	   s->ths_service->s_quality_index(s->ths_service));

    service_source_info_free(&si);
  }
  return s;
}


/**
 *
 */
th_subscription_t *
subscription_create_from_service(service_t *t, const char *name,
				   streaming_target_t *st, int flags,
				   int direct)
{
  th_subscription_t *s = subscription_create(INT32_MAX, name, st,
							flags, direct);
  source_info_t si;
  int r;

  if(t->s_status != SERVICE_RUNNING) {
    if((r = service_start(t, INT32_MAX, 1)) != 0) {
      subscription_unsubscribe(s);

      tvhlog(LOG_INFO, "subscription", 
	     "\"%s\" direct subscription failed -- %s", name,
	     streaming_code2txt(r));
      return NULL;
    }
  }

  t->s_setsourceinfo(t, &si);

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
	 t->s_quality_index(t));
  service_source_info_free(&si);

  subscription_link_service(s, t);
  return s;
}


/**
 *
 */
void
subscription_change_weight(th_subscription_t *s, int weight)
{
  if(s->ths_weight == weight)
    return;

  LIST_REMOVE(s, ths_global_link);

  s->ths_weight = weight;
  LIST_INSERT_SORTED(&subscriptions, s, ths_global_link, subscription_sort);

  subscription_reschedule();
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

  case SMT_SERVICE_STATUS:
    fprintf(stderr, "dummsubscription: %x\n", sm->sm_code);
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
  service_t *t = service_find_by_identifier(id);
  streaming_target_t *st;

  if(first) {
    gtimer_arm(&dummy_sub_timer, dummy_retry, strdup(id), 2);
    return;
  }

  if(t == NULL) {
    tvhlog(LOG_ERR, "subscription", 
	   "Unable to dummy join %s, service not found, retrying...", id);

    gtimer_arm(&dummy_sub_timer, dummy_retry, strdup(id), 1);
    return;
  }

  st = calloc(1, sizeof(streaming_target_t));
  streaming_target_init(st, dummy_callback, NULL, 0);
  subscription_create_from_service(t, "dummy", st, 0, 1);

  tvhlog(LOG_NOTICE, "subscription", 
	 "Dummy join %s ok", id);
}
