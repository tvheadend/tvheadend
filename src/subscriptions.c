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

#include "tvheadend.h"
#include "subscriptions.h"
#include "streaming.h"
#include "channels.h"
#include "service.h"
#include "htsmsg.h"
#include "notify.h"
#include "atomic.h"
#if ENABLE_MPEGTS
#include "input/mpegts.h"
#endif

struct th_subscription_list subscriptions;
struct th_subscription_list subscriptions_remove;
static gtimer_t             subscription_reschedule_timer;

/**
 *
 */
int
subscriptions_active(void)
{
  return LIST_FIRST(&subscriptions) != NULL;
}

/* **************************************************************************
 * Subscription linking
 * *************************************************************************/

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

  tvhtrace("subscription", "linking sub %p to svc %p", s, t);

  pthread_mutex_lock(&t->s_stream_mutex);

  if(TAILQ_FIRST(&t->s_components) != NULL) {

    if(s->ths_start_message != NULL)
      streaming_msg_free(s->ths_start_message);

    s->ths_start_message =
      streaming_msg_create_data(SMT_START, service_build_stream_start(t));
  }

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
static void
subscription_unlink_service0(th_subscription_t *s, int reason, int stop)
{
  streaming_message_t *sm;
  service_t *t = s->ths_service;

  /* Ignore - not actually linked */
  if (!s->ths_current_instance) return;

  tvhtrace("subscription", "unlinking sub %p from svc %p", s, t);

  pthread_mutex_lock(&t->s_stream_mutex);

  streaming_target_disconnect(&t->s_streaming_pad, &s->ths_input);

  if(stop &&
     TAILQ_FIRST(&t->s_components) != NULL && 
     s->ths_state == SUBSCRIPTION_GOT_SERVICE) {
    // Send a STOP message to the subscription client
    sm = streaming_msg_create_code(SMT_STOP, reason);
    streaming_target_deliver(s->ths_output, sm);
  }

  pthread_mutex_unlock(&t->s_stream_mutex);

  LIST_REMOVE(s, ths_service_link);
  s->ths_service = NULL;
}

void
subscription_unlink_service(th_subscription_t *s, int reason)
{
  subscription_unlink_service0(s, reason, 1);
}

/*
 * Called from mpegts code
 */
void
subscription_unlink_mux(th_subscription_t *s, int reason)
{
  streaming_message_t *sm;
  mpegts_mux_instance_t *mmi = s->ths_mmi;
  mpegts_mux_t   *mm = mmi->mmi_mux;
  mpegts_input_t *mi = mmi->mmi_input;

  pthread_mutex_lock(&mi->mi_delivery_mutex);

  if (!(s->ths_flags & SUBSCRIPTION_NONE))
    streaming_target_disconnect(&mmi->mmi_streaming_pad, &s->ths_input);

  sm = streaming_msg_create_code(SMT_STOP, reason);
  streaming_target_deliver(s->ths_output, sm);

  if (mi && (s->ths_flags & SUBSCRIPTION_FULLMUX))
    mi->mi_close_pid(mi, mm, MPEGTS_FULLMUX_PID, MPS_NONE, s);
  s->ths_mmi = NULL;
  LIST_REMOVE(s, ths_mmi_link);

  pthread_mutex_unlock(&mi->mi_delivery_mutex);
}

/* **************************************************************************
 * Scheduling
 * *************************************************************************/

/**
 *
 */
static int
subscription_sort(th_subscription_t *a, th_subscription_t *b)
{
  return b->ths_weight - a->ths_weight;
}


/**
 *
 */
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
  static int reenter = 0;
  th_subscription_t *s;
  service_instance_t *si;
  streaming_message_t *sm;
  int error;
  assert(reenter == 0);
  reenter = 1;

  lock_assert(&global_lock);

  gtimer_arm(&subscription_reschedule_timer, 
	           subscription_reschedule_cb, NULL, 2);

  LIST_FOREACH(s, &subscriptions, ths_global_link) {
    if (s->ths_mmi) continue;
    if (!s->ths_service && !s->ths_channel) continue;

    if(s->ths_service != NULL && s->ths_current_instance != NULL) {
      /* Already got a service */

      if(s->ths_state != SUBSCRIPTION_BAD_SERVICE)
	      continue; /* And it not bad, so we're happy */

      subscription_unlink_service0(s, SM_CODE_BAD_SOURCE, 0);

      si = s->ths_current_instance;

      assert(si != NULL);
      si->si_error = s->ths_testing_error;
      time(&si->si_error_time);

      if (!s->ths_channel)
        s->ths_service = si->si_s;
    }

    if (s->ths_channel)
      tvhtrace("subscription", "find service for %s weight %d",
               channel_get_name(s->ths_channel), s->ths_weight);
    else 
      tvhtrace("subscription", "find instance for %s weight %d",
               s->ths_service->s_nicename, s->ths_weight);
    si = service_find_instance(s->ths_service, s->ths_channel,
                               &s->ths_instances, &error, s->ths_weight);
    s->ths_current_instance = si;

    if(si == NULL) {
      /* No service available */

      sm = streaming_msg_create_code(SMT_NOSTART, error);
      streaming_target_deliver(s->ths_output, sm);
      continue;
    }

    subscription_link_service(s, si->si_s);
  }

  while ((s = LIST_FIRST(&subscriptions_remove))) {
    LIST_REMOVE(s, ths_remove_link);
    subscription_unsubscribe(s);
  }
  
  reenter = 0;
}

/* **************************************************************************
 * Streaming handlers
 * *************************************************************************/

/**
 * NULL handlers for fake subs
 *
 * These require no data, though expect to receive the stop command
 */
static void
subscription_input_null(void *opaque, streaming_message_t *sm)
{
  th_subscription_t *s = opaque;
  if (sm->sm_type == SMT_STOP && s->ths_state != SUBSCRIPTION_ZOMBIE) {
    LIST_INSERT_HEAD(&subscriptions_remove, s, ths_remove_link);
    gtimer_arm(&subscription_reschedule_timer, 
  	       subscription_reschedule_cb, NULL, 0);
  }

  streaming_msg_free(sm);
}

/**
 *
 */
static void
subscription_input_direct(void *opauqe, streaming_message_t *sm)
{
  th_subscription_t *s = opauqe;

  /* Log data and errors */
  if(sm->sm_type == SMT_PACKET) {
    th_pkt_t *pkt = sm->sm_data;
    if(pkt->pkt_err)
      s->ths_total_err++;
    s->ths_bytes_in += pkt->pkt_payload->pb_size;
  } else if(sm->sm_type == SMT_MPEGTS) {
    pktbuf_t *pb = sm->sm_data;
    s->ths_bytes_in += pb->pb_size;
  }

  /* Pass to output */
  streaming_target_deliver(s->ths_output, sm);
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
    streaming_msg_free(sm);
    return;
  }

  /* Pass to direct handler to log traffic */
  subscription_input_direct(s, sm);
}

/* **************************************************************************
 * Destroy subscriptions
 * *************************************************************************/

/**
 * Delete
 */
void
subscription_unsubscribe(th_subscription_t *s)
{
  service_t *t = s->ths_service;

  lock_assert(&global_lock);

  s->ths_state = SUBSCRIPTION_ZOMBIE;

  service_instance_list_clear(&s->ths_instances);

  LIST_REMOVE(s, ths_global_link);

  if(s->ths_channel != NULL) {
    LIST_REMOVE(s, ths_channel_link);
    tvhlog(LOG_INFO, "subscription", "\"%s\" unsubscribing from \"%s\"",
           s->ths_title, channel_get_name(s->ths_channel));
  } else {
    tvhlog(LOG_INFO, "subscription", "\"%s\" unsubscribing",
           s->ths_title);
  }

  if(t)
    service_remove_subscriber(t, s, SM_CODE_OK);

#if ENABLE_MPEGTS
  if(s->ths_mmi)
    mpegts_mux_remove_subscriber(s->ths_mmi->mmi_mux, s, SM_CODE_OK);
#endif

  if(s->ths_start_message != NULL) 
    streaming_msg_free(s->ths_start_message);
 
  free(s->ths_title);
  free(s->ths_hostname);
  free(s->ths_username);
  free(s->ths_client);
  free(s);

  gtimer_arm(&subscription_reschedule_timer, 
            subscription_reschedule_cb, NULL, 0);
  notify_reload("subscriptions");
}

/* **************************************************************************
 * Create subscriptions
 * *************************************************************************/

/*
 * Generic handler for all susbcription creation
 */
th_subscription_t *
subscription_create
  (int weight, const char *name, streaming_target_t *st,
   int flags, st_callback_t *cb, const char *hostname,
   const char *username, const char *client)
{
  th_subscription_t *s = calloc(1, sizeof(th_subscription_t));
  int reject = 0;
  static int tally;
  TAILQ_INIT(&s->ths_instances);

  if(flags & SUBSCRIPTION_NONE)
    reject |= (SMT_TO_MASK(SMT_PACKET) | SMT_TO_MASK(SMT_MPEGTS));
  else if(flags & SUBSCRIPTION_RAW_MPEGTS)
    reject |= SMT_TO_MASK(SMT_PACKET);  // Reject parsed frames
  else
    reject |= SMT_TO_MASK(SMT_MPEGTS);  // Reject raw mpegts

  if (!cb) cb = subscription_input_direct;
  if (!st) {
    st = calloc(1, sizeof(streaming_target_t));
    streaming_target_init(st, subscription_input_null, s, 0);
  }

  streaming_target_init(&s->ths_input, cb, s, reject);

  s->ths_weight            = weight;
  s->ths_title             = strdup(name);
  s->ths_hostname          = hostname ? strdup(hostname) : NULL;
  s->ths_username          = username ? strdup(username) : NULL;
  s->ths_client            = client   ? strdup(client)   : NULL;
  s->ths_total_err         = 0;
  s->ths_output            = st;
  s->ths_flags             = flags;

  time(&s->ths_start);

  s->ths_id = ++tally;

  LIST_INSERT_SORTED(&subscriptions, s, ths_global_link, subscription_sort);

  gtimer_arm(&subscription_reschedule_timer, 
	           subscription_reschedule_cb, NULL, 0);
  notify_reload("subscriptions");

  return s;
}

/**
 *
 */
static th_subscription_t *
subscription_create_from_channel_or_service
  (channel_t *ch, service_t *t, unsigned int weight, 
   const char *name, streaming_target_t *st,
   int flags, const char *hostname,
   const char *username, const char *client)
{
  th_subscription_t *s;
  assert(!ch || !t);
  assert(st);

  if (ch)
    tvhtrace("subscription", "creating subscription for %s weight %d",
             channel_get_name(ch), weight);
  s = subscription_create(weight, name, st, flags, subscription_input,
                          hostname, username, client);
  s->ths_channel = ch;
  s->ths_service = t;
  if (ch)
    LIST_INSERT_HEAD(&ch->ch_subscriptions, s, ths_channel_link);

  // TODO: do we really need this here?
  subscription_reschedule();

  if(s->ths_service == NULL) {
    tvhlog(LOG_NOTICE, "subscription", 
	   "No transponder available for subscription \"%s\" "
	   "to channel \"%s\"",
	   s->ths_title, ch ? channel_get_name(ch) : "none");
  } else {
    source_info_t si;

    s->ths_service->s_setsourceinfo(s->ths_service, &si);

    tvhlog(LOG_INFO, "subscription", 
	   "\"%s\" subscribing on \"%s\", weight: %d, adapter: \"%s\", "
	   "network: \"%s\", mux: \"%s\", provider: \"%s\", "
	   "service: \"%s\"",
	   s->ths_title, ch ? channel_get_name(ch) : "none", weight,
	   si.si_adapter  ?: "<N/A>",
	   si.si_network  ?: "<N/A>",
	   si.si_mux      ?: "<N/A>",
	   si.si_provider ?: "<N/A>",
	   si.si_service  ?: "<N/A>");

    service_source_info_free(&si);
  }

  return s;
}

th_subscription_t *
subscription_create_from_channel(channel_t *ch, unsigned int weight, 
				 const char *name, streaming_target_t *st,
				 int flags, const char *hostname,
				 const char *username, const char *client)
{
  return subscription_create_from_channel_or_service
           (ch, NULL, weight, name, st, flags, hostname, username, client);
}

/**
 *
 */
th_subscription_t *
subscription_create_from_service(service_t *t, unsigned int weight,
                                 const char *name,
				 streaming_target_t *st, int flags,
				 const char *hostname, const char *username, 
				 const char *client)
{
  return subscription_create_from_channel_or_service
           (NULL, t, weight, name, st, flags, hostname, username, client);
}

/**
 *
 */
/**
 *
 */
#if ENABLE_MPEGTS
// TODO: move this
static void
mpegts_mux_setsourceinfo ( mpegts_mux_t *mm, source_info_t *si )
{
  char buf[128];

  /* Validate */
  lock_assert(&global_lock);

  /* Update */
  if(mm->mm_network->mn_network_name != NULL)
    si->si_network = strdup(mm->mm_network->mn_network_name);

  mm->mm_display_name(mm, buf, sizeof(buf));
  si->si_mux = strdup(buf);

  if(mm->mm_active && mm->mm_active->mmi_input) {
    mpegts_input_t *mi = mm->mm_active->mmi_input;
    mi->mi_display_name(mi, buf, sizeof(buf));
    si->si_adapter = strdup(buf);
  }
}


th_subscription_t *
subscription_create_from_mux
  (mpegts_mux_t *mm,
   unsigned int weight,
   const char *name,
   streaming_target_t *st,
   int flags,
   const char *hostname,
   const char *username, 
   const char *client,
   int *err)
{
  th_subscription_t *s;
  streaming_message_t *sm;
  streaming_start_t *ss;
  mpegts_input_t *mi;
  int r;

  /* Tune */
  r = mm->mm_start(mm, name, weight);
  if (r) {
    if (err) *err = r;
    return NULL;
  }

  /* Create subscription */
  if (!st)
    flags |= SUBSCRIPTION_NONE;
  s = subscription_create(weight, name, st, flags, NULL,
                          hostname, username, client);
  s->ths_mmi = mm->mm_active;

  /* Install full mux handler */
  mi = s->ths_mmi->mmi_input;
  if (mi && (s->ths_flags & SUBSCRIPTION_FULLMUX)) {
    pthread_mutex_lock(&mi->mi_delivery_mutex);
    mi->mi_open_pid(mi, mm, MPEGTS_FULLMUX_PID, MPS_NONE, s);
    pthread_mutex_unlock(&mi->mi_delivery_mutex);
  }

  pthread_mutex_lock(&s->ths_mmi->mmi_input->mi_delivery_mutex);

  /* Store */
  LIST_INSERT_HEAD(&mm->mm_active->mmi_subs, s, ths_mmi_link);

  /* Connect */
  if (st)
    streaming_target_connect(&s->ths_mmi->mmi_streaming_pad, &s->ths_input);

  /* Deliver a start message */
  ss = calloc(1, sizeof(streaming_start_t));
  ss->ss_num_components = 0;
  ss->ss_refcount       = 1;
    
  mpegts_mux_setsourceinfo(mm, &ss->ss_si);
  ss->ss_si.si_service = strdup("rawmux");

  tvhinfo("subscription", 
	  "'%s' subscribing to mux, weight: %d, adapter: '%s', "
	  "network: '%s', mux: '%s'",
	  s->ths_title,
          s->ths_weight,
	  ss->ss_si.si_adapter  ?: "<N/A>",
	  ss->ss_si.si_network  ?: "<N/A>",
	  ss->ss_si.si_mux      ?: "<N/A>");

  sm = streaming_msg_create_data(SMT_START, ss);
  streaming_target_deliver(s->ths_output, sm);

  pthread_mutex_unlock(&s->ths_mmi->mmi_input->mi_delivery_mutex);

  return s;
}
#endif

/* **************************************************************************
 * Status monitoring
 * *************************************************************************/

static gtimer_t subscription_status_timer;

/*
 * Serialize info about subscription
 */
htsmsg_t *
subscription_create_msg(th_subscription_t *s)
{
  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_u32(m, "id", s->ths_id);
  htsmsg_add_u32(m, "start", s->ths_start);
  htsmsg_add_u32(m, "errors", s->ths_total_err);

  const char *state;
  switch(s->ths_state) {
  default:
    state = "Idle";
    break;

  case SUBSCRIPTION_TESTING_SERVICE:
    state = "Testing";
    break;
    
  case SUBSCRIPTION_GOT_SERVICE:
    state = "Running";
    break;

  case SUBSCRIPTION_BAD_SERVICE:
    state = "Bad";
    break;
  }


  htsmsg_add_str(m, "state", state);

  if(s->ths_hostname != NULL)
    htsmsg_add_str(m, "hostname", s->ths_hostname);

  if(s->ths_username != NULL)
    htsmsg_add_str(m, "username", s->ths_username);

  if(s->ths_client != NULL)
    htsmsg_add_str(m, "title", s->ths_client);
  else if(s->ths_title != NULL)
    htsmsg_add_str(m, "title", s->ths_title);
  
  if(s->ths_channel != NULL)
    htsmsg_add_str(m, "channel", channel_get_name(s->ths_channel));
  
  if(s->ths_service != NULL)
    htsmsg_add_str(m, "service", s->ths_service->s_nicename ?: "");

  else if (s->ths_mmi != NULL && s->ths_mmi->mmi_mux != NULL) {
    char buf[512];
    mpegts_mux_t *mm = s->ths_mmi->mmi_mux;
    mm->mm_display_name(mm, buf, sizeof(buf));
    htsmsg_add_str(m, "service", buf);
  }
  
  return m;
}

/**
 * Check status (bandwidth, errors, etc.)
 */
static void
subscription_status_callback ( void *p )
{
  th_subscription_t *s;
  gtimer_arm(&subscription_status_timer,
             subscription_status_callback, NULL, 1);

  LIST_FOREACH(s, &subscriptions, ths_global_link) {
    int errors  = s->ths_total_err;
    int in      = atomic_exchange(&s->ths_bytes_in, 0);
    int out     = atomic_exchange(&s->ths_bytes_out, 0);
    htsmsg_t *m = subscription_create_msg(s);
    htsmsg_delete_field(m, "errors");
    htsmsg_add_u32(m, "errors", errors);
    htsmsg_add_u32(m, "in", in);
    htsmsg_add_u32(m, "out", out);
    htsmsg_add_u32(m, "updateEntry", 1);
    notify_by_msg("subscriptions", m);
  }
}

/**
 * Initialise subsystem
 */
void
subscription_init(void)
{
  subscription_status_callback(NULL);
}

/* **************************************************************************
 * Subscription control
 * *************************************************************************/

/**
 * Change weight
 */
void
subscription_change_weight(th_subscription_t *s, int weight)
{
  if(s->ths_weight == weight)
    return;

  LIST_REMOVE(s, ths_global_link);

  s->ths_weight = weight;
  LIST_INSERT_SORTED(&subscriptions, s, ths_global_link, subscription_sort);

  gtimer_arm(&subscription_reschedule_timer, 
	           subscription_reschedule_cb, NULL, 0);
}

/**
 * Set speed
 */
void
subscription_set_speed ( th_subscription_t *s, int speed )
{
  streaming_message_t *sm;
  service_t *t = s->ths_service;

  if (!t) return;

  pthread_mutex_lock(&t->s_stream_mutex);

  sm = streaming_msg_create_code(SMT_SPEED, speed);

  streaming_target_deliver(s->ths_output, sm);

  pthread_mutex_unlock(&t->s_stream_mutex);
}

/**
 * Set skip
 */
void
subscription_set_skip ( th_subscription_t *s, const streaming_skip_t *skip )
{
  streaming_message_t *sm;
  service_t *t = s->ths_service;

  if (!t) return;

  pthread_mutex_lock(&t->s_stream_mutex);

  sm = streaming_msg_create(SMT_SKIP);
  sm->sm_data = malloc(sizeof(streaming_skip_t));
  memcpy(sm->sm_data, skip, sizeof(streaming_skip_t));

  streaming_target_deliver(s->ths_output, sm);

  pthread_mutex_unlock(&t->s_stream_mutex);
}

/* **************************************************************************
 * Dummy subscription - testing
 * *************************************************************************/

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
  subscription_create_from_service(t, 1, "dummy", st, 0, NULL, NULL, "dummy");

  tvhlog(LOG_NOTICE, "subscription", 
	 "Dummy join %s ok", id);
}


