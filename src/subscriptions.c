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
#include "profile.h"
#include "htsmsg.h"
#include "notify.h"
#include "atomic.h"
#include "input.h"
#include "dbus.h"

struct th_subscription_list subscriptions;
struct th_subscription_list subscriptions_remove;
static gtimer_t             subscription_reschedule_timer;
static int                  subscription_postpone;

/**
 *
 */
static inline int
shortid(th_subscription_t *s)
{
  return s->ths_id & 0xffff;
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

  tvhtrace("subscription", "%04X: linking sub %p to svc %p type %i",
           shortid(s), s, t, t->s_type);

  pthread_mutex_lock(&t->s_stream_mutex);

  if(TAILQ_FIRST(&t->s_filt_components) != NULL ||
     t->s_type != STYPE_STD) {

    streaming_msg_free(s->ths_start_message);

    s->ths_start_message =
      streaming_msg_create_data(SMT_START, service_build_stream_start(t));
  }

  // Link to service output
  streaming_target_connect(&t->s_streaming_pad, &s->ths_input);

  streaming_pad_deliver(&t->s_streaming_pad,
                        streaming_msg_create_code(SMT_GRACE,
                                                  s->ths_postpone +
                                                    t->s_grace_delay));

  if(s->ths_start_message != NULL && t->s_streaming_status & TSS_PACKETS) {

    s->ths_state = SUBSCRIPTION_GOT_SERVICE;

    // Send a START message to the subscription client
    streaming_target_deliver(s->ths_output, s->ths_start_message);
    s->ths_start_message = NULL;
    t->s_running = 1;

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

  tvhtrace("subscription", "%04X: unlinking sub %p from svc %p", shortid(s), s, t);

  pthread_mutex_lock(&t->s_stream_mutex);

  streaming_target_disconnect(&t->s_streaming_pad, &s->ths_input);

  if(stop && t->s_running) {
    // Send a STOP message to the subscription client
    sm = streaming_msg_create_code(SMT_STOP, reason);
    streaming_target_deliver(s->ths_output, sm);
    t->s_running = 0;
  }

  pthread_mutex_unlock(&t->s_stream_mutex);

  LIST_REMOVE(s, ths_service_link);
  s->ths_service = NULL;

  if (stop && (s->ths_flags & SUBSCRIPTION_ONESHOT) != 0)
    subscription_unsubscribe(s, 0);
}

void
subscription_unlink_service(th_subscription_t *s, int reason)
{
  subscription_unlink_service0(s, reason, 1);
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

static void
subscription_show_none(th_subscription_t *s)
{
  char buf[256], buf2[128];
  size_t l = 0;

  tvh_strlcatf(buf, sizeof(buf), l,
                "No input source available for subscription \"%s\"",
	        s->ths_title);
  if (s->ths_channel)
    tvh_strlcatf(buf, sizeof(buf), l, " to channel \"%s\"",
                  s->ths_channel ? channel_get_name(s->ths_channel) : "none");
#if ENABLE_MPEGTS
  else if (s->ths_raw_service) {
    mpegts_service_t *ms = (mpegts_service_t *)s->ths_raw_service;
    mpegts_mux_nice_name(ms->s_dvb_mux, buf2, sizeof(buf2));
    tvh_strlcatf(buf, sizeof(buf), l, " to mux \"%s\"", buf2);
  }
#endif
  else {
    tvh_strlcatf(buf, sizeof(buf), l, " to service \"%s\"",
                  s->ths_service ? s->ths_service->s_nicename : "none");
#if ENABLE_MPEGTS
    if (idnode_is_instance(&s->ths_service->s_id, &mpegts_service_class)) {
      mpegts_service_t *ms = (mpegts_service_t *)s->ths_service;
      mpegts_mux_nice_name(ms->s_dvb_mux, buf2, sizeof(buf2));
      tvh_strlcatf(buf, sizeof(buf), l, " in mux \"%s\"", buf2);
    }
#endif
  }
  tvhlog(LOG_NOTICE, "subscription", "%04X: %s", shortid(s), buf);
}

static void
subscription_show_info(th_subscription_t *s)
{
  char buf[512];
  source_info_t si;
  size_t l = 0;
  int mux = 0, service = 0;

  s->ths_service->s_setsourceinfo(s->ths_service, &si);
  tvh_strlcatf(buf, sizeof(buf), l, "\"%s\" subscribing", s->ths_title);
  if (s->ths_channel) {
    tvh_strlcatf(buf, sizeof(buf), l, " on channel \"%s\"",
                  s->ths_channel ? channel_get_name(s->ths_channel) : "none");
#if ENABLE_MPEGTS
  } else if (s->ths_raw_service && si.si_mux) {
    tvh_strlcatf(buf, sizeof(buf), l, " to mux \"%s\"", si.si_mux);
    mux = 1;
#endif
  } else {
    tvh_strlcatf(buf, sizeof(buf), l, " to service \"%s\"",
                  s->ths_service ? s->ths_service->s_nicename : "none");
    service = 1;
  }
  tvh_strlcatf(buf, sizeof(buf), l, ", weight: %d", s->ths_weight);
  if (si.si_adapter)
    tvh_strlcatf(buf, sizeof(buf), l, ", adapter: \"%s\"", si.si_adapter);
  if (si.si_network)
    tvh_strlcatf(buf, sizeof(buf), l, ", network: \"%s\"", si.si_network);
  if (!mux && si.si_mux)
    tvh_strlcatf(buf, sizeof(buf), l, ", mux: \"%s\"", si.si_mux);
  if (si.si_provider)
    tvh_strlcatf(buf, sizeof(buf), l, ", provider: \"%s\"", si.si_provider);
  if (!service && si.si_service)
    tvh_strlcatf(buf, sizeof(buf), l, ", service: \"%s\"", si.si_service);

  if (s->ths_prch && s->ths_prch->prch_pro)
    tvh_strlcatf(buf, sizeof(buf), l,
                       ", profile=\"%s\"",
                       s->ths_prch->prch_pro->pro_name ?: "");

  if (s->ths_hostname)
    tvh_strlcatf(buf, sizeof(buf), l, ", hostname=\"%s\"", s->ths_hostname);
  if (s->ths_username)
    tvh_strlcatf(buf, sizeof(buf), l, ", username=\"%s\"", s->ths_username);
  if (s->ths_client)
    tvh_strlcatf(buf, sizeof(buf), l, ", client=\"%s\"", s->ths_client);

  tvhlog(LOG_INFO, "subscription", "%04X: %s", shortid(s), buf);
  service_source_info_free(&si);
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
static service_instance_t *
subscription_start_instance
  (th_subscription_t *s, int *error)
{
  service_instance_t *si;

  if (s->ths_channel)
    tvhtrace("subscription", "%04X: find service for %s weight %d",
             shortid(s), channel_get_name(s->ths_channel), s->ths_weight);
  else
    tvhtrace("subscription", "%04X: find instance for %s weight %d",
             shortid(s), s->ths_service->s_nicename, s->ths_weight);
  si = service_find_instance(s->ths_service, s->ths_channel,
                             s->ths_source, s->ths_prch,
                             &s->ths_instances, error, s->ths_weight,
                             s->ths_flags, s->ths_timeout,
                             dispatch_clock > s->ths_postpone_end ?
                               0 : s->ths_postpone_end - dispatch_clock);
  return s->ths_current_instance = si;
}

/**
 *
 */
void
subscription_reschedule(void)
{
  static int reenter = 0;
  th_subscription_t *s;
  service_t *t;
  service_instance_t *si;
  streaming_message_t *sm;
  int error, postpone = INT_MAX;
  assert(reenter == 0);
  reenter = 1;

  lock_assert(&global_lock);

  LIST_FOREACH(s, &subscriptions, ths_global_link) {
    if (!s->ths_service && !s->ths_channel) continue;
    if (s->ths_flags & SUBSCRIPTION_ONESHOT) continue;

    /* Postpone the tuner decision */
    /* Leave some time to wakeup tuners through DBus or so */
    if (s->ths_postpone_end > dispatch_clock) {
      if (postpone > s->ths_postpone_end - dispatch_clock)
        postpone = s->ths_postpone_end - dispatch_clock;
      sm = streaming_msg_create_code(SMT_GRACE, (s->ths_postpone_end - dispatch_clock) + 5);
      streaming_target_deliver(s->ths_output, sm);
      continue;
    }

    t = s->ths_service;
    if(t != NULL && s->ths_current_instance != NULL) {
      /* Already got a service */

      if(s->ths_state != SUBSCRIPTION_BAD_SERVICE)
	continue; /* And it not bad, so we're happy */

      tvhwarn("subscription", "%04X: service instance is bad, reason: %s",
              shortid(s), streaming_code2txt(s->ths_testing_error));

      t->s_streaming_status = 0;
      t->s_status = SERVICE_IDLE;

      subscription_unlink_service0(s, SM_CODE_BAD_SOURCE, 0);

      if(t && LIST_FIRST(&t->s_subscriptions) == NULL)
        service_stop(t);

      si = s->ths_current_instance;

      assert(si != NULL);
      si->si_error = s->ths_testing_error;
      time(&si->si_error_time);

      if (!s->ths_channel)
        s->ths_service = si->si_s;

      s->ths_last_error = 0;
    }

    error = s->ths_testing_error;
    si = subscription_start_instance(s, &error);
    s->ths_current_instance = si;

    if(si == NULL) {
      if (s->ths_last_error != error || s->ths_last_find + 2 >= dispatch_clock) {
        tvhtrace("subscription", "%04X: instance not available, retrying", shortid(s));
        if (s->ths_last_error != error)
          s->ths_last_find = dispatch_clock;
        s->ths_last_error = error;
        continue;
      }
      if (s->ths_flags & SUBSCRIPTION_RESTART) {
        if (s->ths_channel)
          tvhwarn("subscription", "%04X: restarting channel %s",
                  shortid(s), channel_get_name(s->ths_channel));
        else
          tvhwarn("subscription", "%04X: restarting service %s",
                  shortid(s), s->ths_service->s_nicename);
        s->ths_testing_error = 0;
        s->ths_current_instance = NULL;
        service_instance_list_clear(&s->ths_instances);
        continue;
      }
      /* No service available */
      sm = streaming_msg_create_code(SMT_NOSTART, error);
      streaming_target_deliver(s->ths_output, sm);
      subscription_show_none(s);
      continue;
    }

    subscription_link_service(s, si->si_s);
    subscription_show_info(s);
  }

  while ((s = LIST_FIRST(&subscriptions_remove)))
    subscription_unsubscribe(s, 0);

  if (postpone <= 0 || postpone == INT_MAX)
    postpone = 2;
  gtimer_arm(&subscription_reschedule_timer,
	           subscription_reschedule_cb, NULL, postpone);

  reenter = 0;
}

/**
 *
 */
static int64_t
subscription_set_postpone(void *aux, const char *path, int64_t postpone)
{
  th_subscription_t *s;
  time_t now = time(NULL);

  if (strcmp(path, "/set"))
    return -1;
  /* some limits that make sense */
  if (postpone < 0)
    postpone = 0;
  if (postpone > 120)
    postpone = 120;
  pthread_mutex_lock(&global_lock);
  if (subscription_postpone != postpone) {
    subscription_postpone = postpone;
    tvhinfo("subscriptions", "postpone set to %d seconds", (int)postpone);
    LIST_FOREACH(s, &subscriptions, ths_global_link) {
      s->ths_postpone = postpone;
      if (s->ths_postpone_end > now && s->ths_postpone_end - now > postpone)
        s->ths_postpone_end = now + postpone;
    }
    gtimer_arm(&subscription_reschedule_timer,
  	       subscription_reschedule_cb, NULL, 0);
  }
  pthread_mutex_unlock(&global_lock);
  return postpone;
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
    s->ths_total_err += pkt->pkt_err;
    if (pkt->pkt_payload)
      s->ths_bytes_in += pkt->pkt_payload->pb_size;
  } else if(sm->sm_type == SMT_MPEGTS) {
    pktbuf_t *pb = sm->sm_data;
    s->ths_total_err += pb->pb_err;
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
  int error;
  th_subscription_t *s = opauqe;

  if(s->ths_state == SUBSCRIPTION_TESTING_SERVICE) {
    // We are just testing if this service is good

    if(sm->sm_type == SMT_GRACE) {
      streaming_target_deliver(s->ths_output, sm);
      return;
    }

    if(sm->sm_type == SMT_START) {
      streaming_msg_free(s->ths_start_message);
      s->ths_start_message = sm;
      return;
    }

    if(sm->sm_type == SMT_SERVICE_STATUS &&
       sm->sm_code & TSS_ERRORS) {
      // No, mark our subscription as bad_service
      // the scheduler will take care of things
      error = tss2errcode(sm->sm_code);
      if (error != SM_CODE_NO_ACCESS ||
          (s->ths_flags & SUBSCRIPTION_CONTACCESS) == 0) {
        if (error > s->ths_testing_error)
          s->ths_testing_error = error;
        s->ths_state = SUBSCRIPTION_BAD_SERVICE;
        streaming_msg_free(sm);
      }
      return;
    }

    if(sm->sm_type == SMT_SERVICE_STATUS &&
       sm->sm_code & TSS_PACKETS) {
      if(s->ths_start_message != NULL) {
        streaming_target_deliver(s->ths_output, s->ths_start_message);
        s->ths_start_message = NULL;
        if (s->ths_service)
          s->ths_service->s_running = 1;
      }
      s->ths_state = SUBSCRIPTION_GOT_SERVICE;
    }
  }

  if(s->ths_state != SUBSCRIPTION_GOT_SERVICE) {
    streaming_msg_free(sm);
    return;
  }

  if (sm->sm_type == SMT_SERVICE_STATUS &&
      sm->sm_code & (TSS_TUNING|TSS_TIMEOUT)) {
    error = tss2errcode(sm->sm_code);
    if (error != SM_CODE_NO_ACCESS ||
        (s->ths_flags & SUBSCRIPTION_CONTACCESS) == 0) {
      if (error > s->ths_testing_error)
        s->ths_testing_error = error;
      s->ths_state = SUBSCRIPTION_BAD_SERVICE;
    }
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
subscription_unsubscribe(th_subscription_t *s, int quiet)
{
  service_t *t = s->ths_service;
  char buf[512];
  size_t l = 0;

  if (s == NULL)
    return;

  lock_assert(&global_lock);

  s->ths_state = SUBSCRIPTION_ZOMBIE;

  service_instance_list_clear(&s->ths_instances);

  LIST_REMOVE(s, ths_global_link);
  LIST_SAFE_REMOVE(s, ths_remove_link);

#if ENABLE_MPEGTS
  if (s->ths_raw_service)
    LIST_REMOVE(s, ths_mux_link);
#endif

  if (s->ths_channel != NULL) {
    LIST_REMOVE(s, ths_channel_link);
    tvh_strlcatf(buf, sizeof(buf), l, "\"%s\" unsubscribing from \"%s\"",
             s->ths_title, channel_get_name(s->ths_channel));
  } else {
    tvh_strlcatf(buf, sizeof(buf), l, "\"%s\" unsubscribing", s->ths_title);
  }

  if (s->ths_hostname)
    tvh_strlcatf(buf, sizeof(buf), l, ", hostname=\"%s\"", s->ths_hostname);
  if (s->ths_username)
    tvh_strlcatf(buf, sizeof(buf), l, ", username=\"%s\"", s->ths_username);
  if (s->ths_client)
    tvh_strlcatf(buf, sizeof(buf), l, ", client=\"%s\"", s->ths_client);
  tvhlog(quiet ? LOG_TRACE : LOG_INFO, "subscription", "%04X: %s", shortid(s), buf);

  if (t) {
    s->ths_flags &= ~SUBSCRIPTION_ONESHOT;
    service_remove_subscriber(t, s, SM_CODE_OK);
  }

#if ENABLE_MPEGTS
  if (s->ths_raw_service)
    service_destroy(s->ths_raw_service, 0);
#endif

  streaming_msg_free(s->ths_start_message);

  if(s->ths_output->st_cb == subscription_input_null)
   free(s->ths_output);
 
  free(s->ths_title);
  free(s->ths_hostname);
  free(s->ths_username);
  free(s->ths_client);
  free(s->ths_dvrfile);
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
  (profile_chain_t *prch, int weight, const char *name,
   int flags, st_callback_t *cb, const char *hostname,
   const char *username, const char *client)
{
  th_subscription_t *s = calloc(1, sizeof(th_subscription_t));
  profile_t *pro = prch ? prch->prch_pro : NULL;
  streaming_target_t *st = prch ? prch->prch_st : NULL;
  int reject = 0;
  static int tally;

  TAILQ_INIT(&s->ths_instances);

  switch (flags & SUBSCRIPTION_TYPE_MASK) {
  case SUBSCRIPTION_NONE:
    reject |= SMT_TO_MASK(SMT_PACKET) | SMT_TO_MASK(SMT_MPEGTS);
    break;
  case SUBSCRIPTION_MPEGTS:
    reject |= SMT_TO_MASK(SMT_PACKET);  // Reject parsed frames
    break;
  case SUBSCRIPTION_PACKET:
    reject |= SMT_TO_MASK(SMT_MPEGTS);  // Reject raw mpegts
    break;
  default:
    abort();
  }

  if (!cb) cb = subscription_input_direct;
  if (!st) {
    st = calloc(1, sizeof(streaming_target_t));
    streaming_target_init(st, subscription_input_null, s, 0);
  }

  streaming_target_init(&s->ths_input, cb, s, reject);

  s->ths_prch              = prch && prch->prch_st ? prch : NULL;
  s->ths_title             = strdup(name);
  s->ths_hostname          = hostname ? strdup(hostname) : NULL;
  s->ths_username          = username ? strdup(username) : NULL;
  s->ths_client            = client   ? strdup(client)   : NULL;
  s->ths_total_err         = 0;
  s->ths_output            = st;
  s->ths_flags             = flags;
  s->ths_timeout           = pro ? pro->pro_timeout : 0;
  s->ths_postpone          = subscription_postpone;
  s->ths_postpone_end      = dispatch_clock + s->ths_postpone;

  if (s->ths_prch)
    s->ths_weight = profile_chain_weight(s->ths_prch, weight);
  else
    s->ths_weight = weight;

  if (pro) {
    if (pro->pro_restart)
      s->ths_flags |= SUBSCRIPTION_RESTART;
    if (pro->pro_contaccess)
      s->ths_flags |= SUBSCRIPTION_CONTACCESS;
  }

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
subscription_create_from_channel_or_service(profile_chain_t *prch,
                                            tvh_input_t *ti,
                                            unsigned int weight,
                                            const char *name,
                                            int flags,
                                            const char *hostname,
                                            const char *username,
                                            const char *client,
                                            int *error,
                                            service_t *service)
{
  th_subscription_t *s;
  service_instance_t *si;
  channel_t *ch = NULL;
  int _error;

  assert(prch);
  assert(prch->prch_id);

  if (error == NULL)
    error = &_error;
  *error = 0;

  if (!service)
    ch = prch->prch_id;

  s = subscription_create(prch, weight, name, flags, subscription_input,
                          hostname, username, client);
  if (tvhtrace_enabled()) {
    const char *pro_name = prch->prch_pro ? (prch->prch_pro->pro_name ?: "") : "<none>";
    if (ch)
      tvhtrace("subscription", "%04X: creating subscription for %s weight %d using profile %s",
               shortid(s), channel_get_name(ch), weight, pro_name);
    else
      tvhtrace("subscription", "%04X: creating subscription for service %s weight %d using profile %s",
               shortid(s), service->s_nicename, weight, pro_name);
  }
  s->ths_channel = ch;
  s->ths_service = service;
  s->ths_source  = ti;
  if (ch)
    LIST_INSERT_HEAD(&ch->ch_subscriptions, s, ths_channel_link);

#if ENABLE_MPEGTS
  if (service && service->s_type == STYPE_RAW) {
    mpegts_mux_t *mm = prch->prch_id;
    s->ths_raw_service = service;
    LIST_INSERT_HEAD(&mm->mm_raw_subs, s, ths_mux_link);
  }
#endif

  if (flags & SUBSCRIPTION_ONESHOT) {
    if ((si = subscription_start_instance(s, error)) == NULL) {
      subscription_unsubscribe(s, 1);
      return NULL;
    }
    subscription_link_service(s, si->si_s);
    subscription_show_info(s);
  } else {
    gtimer_arm(&subscription_reschedule_timer,
               subscription_reschedule_cb, NULL, 0);
  }
  return s;
}

th_subscription_t *
subscription_create_from_channel(profile_chain_t *prch,
                                 tvh_input_t *ti,
                                 unsigned int weight,
				 const char *name,
				 int flags,
				 const char *hostname,
				 const char *username,
				 const char *client,
				 int *error)
{
  assert(prch->prch_st);
  return subscription_create_from_channel_or_service
           (prch, ti, weight, name, flags, hostname, username, client,
            error, NULL);
}

/**
 *
 */
th_subscription_t *
subscription_create_from_service(profile_chain_t *prch,
                                 tvh_input_t *ti,
                                 unsigned int weight,
                                 const char *name,
				 int flags,
				 const char *hostname,
				 const char *username,
				 const char *client,
				 int *error)
{
  assert(prch->prch_st);
  return subscription_create_from_channel_or_service
           (prch, ti, weight, name, flags, hostname, username, client,
            error, prch->prch_id);
}

/**
 *
 */
#if ENABLE_MPEGTS
#include "input/mpegts.h"
th_subscription_t *
subscription_create_from_mux(profile_chain_t *prch,
                             tvh_input_t *ti,
                             unsigned int weight,
                             const char *name,
                             int flags,
                             const char *hostname,
                             const char *username,
                             const char *client,
                             int *error)
{
  mpegts_mux_t *mm = prch->prch_id;
  mpegts_service_t *s = mpegts_service_create_raw(mm);

  if (!s)
    return NULL;

  return subscription_create_from_channel_or_service
    (prch, ti, weight, name, flags, hostname, username, client,
     error, (service_t *)s);
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

  else if(s->ths_dvrfile != NULL)
    htsmsg_add_str(m, "service", s->ths_dvrfile ?: "");

  return m;
}

/**
 * Check status (bandwidth, errors, etc.)
 */
static void
subscription_status_callback ( void *p )
{
  th_subscription_t *s;
  int64_t count = 0;
  static int64_t old_count = -1;

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
    count++;
  }
  if (old_count != count) {
    old_count = count;
    dbus_emit_signal_s64("/status", "subscriptions", count);
  }
}

/**
 * Initialise subsystem
 */
void
subscription_init(void)
{
  subscription_status_callback(NULL);
  dbus_register_rpc_s64("postpone", NULL, subscription_set_postpone);
}

/**
 * Shutdown subsystem
 */
void
subscription_done(void)
{
  pthread_mutex_lock(&global_lock);
  /* clear remaining subscriptions */
  subscription_reschedule();
  pthread_mutex_unlock(&global_lock);
  assert(LIST_FIRST(&subscriptions) == NULL);
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
  profile_chain_t *prch;
  streaming_target_t *st;
  th_subscription_t *s;

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

  prch = calloc(1, sizeof(*prch));
  prch->prch_id = t;
  st = calloc(1, sizeof(*st));
  streaming_target_init(st, dummy_callback, NULL, 0);
  prch->prch_st = st;
  s = subscription_create_from_service(prch, NULL, 1, "dummy", 0, NULL, NULL, "dummy", NULL);

  tvhlog(LOG_NOTICE, "subscription",
         "%04X: Dummy join %s ok", shortid(s), id);
}
