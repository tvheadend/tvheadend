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

#include "tvheadend.h"
#include "subscriptions.h"
#include "streaming.h"
#include "parsers/parsers.h"
#include "channels.h"
#include "service.h"
#include "profile.h"
#include "htsmsg.h"
#include "notify.h"
#include "atomic.h"
#include "input.h"
#include "intlconv.h"
#include "dbus.h"

struct th_subscription_list subscriptions;
struct th_subscription_list subscriptions_remove;
static mtimer_t             subscription_reschedule_timer;
static int                  subscription_postpone;

/**
 *
 */
static void subscription_reschedule(void);
static void subscription_unsubscribe_cb(void *aux);

/**
 *
 */
static inline int
shortid(th_subscription_t *s)
{
  return s->ths_id & 0xffff;
}

static inline void
subsetstate(th_subscription_t *s, ths_state_t state)
{
  atomic_set(&s->ths_state, state);
}

static inline ths_state_t
subgetstate(th_subscription_t *s)
{
  return atomic_get(&s->ths_state);
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
  streaming_start_t *ss;

  subsetstate(s, SUBSCRIPTION_TESTING_SERVICE);
  s->ths_service = t;

  if ((s->ths_flags & SUBSCRIPTION_TYPE_MASK) == SUBSCRIPTION_PACKET) {
    assert(s->ths_parser == NULL);
    s->ths_output = s->ths_parser = parser_create(s->ths_output, s);
  }

  LIST_INSERT_HEAD(&t->s_subscriptions, s, ths_service_link);

  tvhtrace(LS_SUBSCRIPTION, "%04X: linking sub %p to svc %p type %i",
           shortid(s), s, t, t->s_type);

  tvh_mutex_lock(&t->s_stream_mutex);

  if(elementary_set_has_streams(&t->s_components, 1) || t->s_type != STYPE_STD) {
    streaming_msg_free(s->ths_start_message);
    ss = service_build_streaming_start(t);
    s->ths_start_message = streaming_msg_create_data(SMT_START, ss);
  }

  // Link to service output
  streaming_target_connect(&t->s_streaming_pad, &s->ths_input);

  sm = streaming_msg_create_code(SMT_GRACE, s->ths_postpone + t->s_grace_delay);
  streaming_service_deliver(t, sm);

  if(s->ths_start_message != NULL && t->s_streaming_status & TSS_PACKETS) {

    subsetstate(s, SUBSCRIPTION_GOT_SERVICE);

    // Send a START message to the subscription client
    streaming_target_deliver(s->ths_output, s->ths_start_message);
    s->ths_start_message = NULL;
    t->s_running = 1;

    // Send status report
    sm = streaming_msg_create_code(SMT_SERVICE_STATUS, 
				   t->s_streaming_status);
    streaming_target_deliver(s->ths_output, sm);
  }

  tvh_mutex_unlock(&t->s_stream_mutex);
}

/**
 * Called from service code
 */
static int
subscription_unlink_service0(th_subscription_t *s, int reason, int resched)
{
  streaming_message_t *sm;
  service_t *t = s->ths_service;

  /* Ignore - not actually linked */
  if (!s->ths_current_instance) goto stop;
  s->ths_current_instance = NULL;

  tvh_mutex_lock(&t->s_stream_mutex);

  streaming_target_disconnect(&t->s_streaming_pad, &s->ths_input);

  if(!resched && t->s_running) {
    // Send a STOP message to the subscription client
    sm = streaming_msg_create_code(SMT_STOP, reason);
    streaming_target_deliver(s->ths_output, sm);
    t->s_running = 0;
  }

  if (s->ths_parser)
    s->ths_output = parser_output(s->ths_parser);

  tvh_mutex_unlock(&t->s_stream_mutex);

  LIST_REMOVE(s, ths_service_link);

  if (s->ths_parser) {
    parser_destroy(s->ths_parser);
    s->ths_parser = NULL;
  }

  if (!resched && (s->ths_flags & SUBSCRIPTION_ONESHOT) != 0)
    mtimer_arm_rel(&s->ths_remove_timer, subscription_unsubscribe_cb, s, 0);

stop:
  if(resched || LIST_FIRST(&t->s_subscriptions) == NULL)
    service_stop(t);
  return 1;
}

void
subscription_unlink_service(th_subscription_t *s, int reason)
{
  subscription_unlink_service0(s, reason, 0);
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
  char buf[256];
  size_t l = 0;

  tvh_strlcatf(buf, sizeof(buf), l,
                "No input source available for subscription \"%s\"",
	        s->ths_title);
  if (s->ths_channel)
    tvh_strlcatf(buf, sizeof(buf), l, " to channel \"%s\"",
                  s->ths_channel ?
                    channel_get_name(s->ths_channel, channel_blank_name) : "none");
#if ENABLE_MPEGTS
  else if (s->ths_raw_service) {
    mpegts_service_t *ms = (mpegts_service_t *)s->ths_raw_service;
    tvh_strlcatf(buf, sizeof(buf), l, " to mux \"%s\"", ms->s_dvb_mux->mm_nicename);
  }
#endif
  else {
    tvh_strlcatf(buf, sizeof(buf), l, " to service \"%s\"",
                  s->ths_service ? s->ths_service->s_nicename : "none");
#if ENABLE_MPEGTS
    if (idnode_is_instance(&s->ths_service->s_id, &mpegts_service_class)) {
      mpegts_service_t *ms = (mpegts_service_t *)s->ths_service;
      tvh_strlcatf(buf, sizeof(buf), l, " in mux \"%s\"", ms->s_dvb_mux->mm_nicename);
    }
#endif
  }
  tvhnotice(LS_SUBSCRIPTION, "%04X: %s", shortid(s), buf);
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
                  s->ths_channel ?
                    channel_get_name(s->ths_channel, channel_blank_name) : "none");
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
    tvh_strlcatf(buf, sizeof(buf), l, ", profile=\"%s\"",
                                      profile_get_name(s->ths_prch->prch_pro));

  if (s->ths_hostname)
    tvh_strlcatf(buf, sizeof(buf), l, ", hostname=\"%s\"", s->ths_hostname);
  if (s->ths_username)
    tvh_strlcatf(buf, sizeof(buf), l, ", username=\"%s\"", s->ths_username);
  if (s->ths_client)
    tvh_strlcatf(buf, sizeof(buf), l, ", client=\"%s\"", s->ths_client);

  tvhinfo(LS_SUBSCRIPTION, "%04X: %s", shortid(s), buf);
  service_source_info_free(&si);

  if (tvhtrace_enabled()) {
    htsmsg_t *list = htsmsg_create_list();
    htsmsg_field_t *f;
    const char *x;
    int i = 1;
    s->ths_input.st_ops.st_info(s->ths_input.st_opaque, list);
    HTSMSG_FOREACH(f, list)
      if ((x = htsmsg_field_get_str(f)) != NULL) {
        tvhtrace(LS_SUBSCRIPTION, "%04X:  chain %02d: %s", shortid(s), i++, x);
      }
    htsmsg_destroy(list);
  }
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
static void
subscription_ca_check_cb(void *aux)
{
  th_subscription_t *s = aux;
  service_t *t = s->ths_service;

  if (t == NULL)
    return;

  tvh_mutex_lock(&t->s_stream_mutex);

  service_set_streaming_status_flags(t, TSS_CA_CHECK);

  tvh_mutex_unlock(&t->s_stream_mutex);
}

/**
 *
 */
void
subscription_delayed_reschedule(int64_t mono)
{
  mtimer_arm_rel(&subscription_reschedule_timer,
	         subscription_reschedule_cb, NULL, mono);
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
    tvhtrace(LS_SUBSCRIPTION, "%04X: find service for %s weight %d",
             shortid(s), channel_get_name(s->ths_channel, channel_blank_name), s->ths_weight);
  else
    tvhtrace(LS_SUBSCRIPTION, "%04X: find instance for %s weight %d",
             shortid(s), s->ths_service->s_nicename, s->ths_weight);
  si = service_find_instance(s->ths_service, s->ths_channel,
                             s->ths_source, s->ths_prch,
                             &s->ths_instances, error, s->ths_weight,
                             s->ths_flags, s->ths_timeout,
                             mclk() > s->ths_postpone_end ?
                               0 : mono2sec(s->ths_postpone_end - mclk()));
  if (si && (s->ths_flags & SUBSCRIPTION_CONTACCESS) == 0)
    mtimer_arm_rel(&s->ths_ca_check_timer, subscription_ca_check_cb, s, s->ths_ca_timeout);
  return s->ths_current_instance = si;
}

/**
 *
 */
static void
subscription_reschedule(void)
{
  static int reenter = 0;
  th_subscription_t *s;
  service_t *t;
  service_instance_t *si;
  streaming_message_t *sm;
  int error, postpone = INT_MAX, postpone2;
  assert(reenter == 0);
  reenter = 1;

  lock_assert(&global_lock);

  LIST_FOREACH(s, &subscriptions, ths_global_link) {
    if (!s->ths_service && !s->ths_channel) continue;
    if (s->ths_flags & SUBSCRIPTION_ONESHOT) continue;

    /* Postpone the tuner decision */
    /* Leave some time to wakeup tuners through DBus or so */
    if (s->ths_postpone_end > mclk()) {
      postpone2 = mono2sec(s->ths_postpone_end - mclk());
      if (postpone > postpone2)
        postpone = postpone2;
      sm = streaming_msg_create_code(SMT_GRACE, postpone + 5);
      streaming_target_deliver(s->ths_output, sm);
      continue;
    }

    t = s->ths_service;
    if(t != NULL && s->ths_current_instance != NULL) {
      /* Already got a service */

      if(subgetstate(s) != SUBSCRIPTION_BAD_SERVICE)
	continue; /* And it is not bad, so we're happy */

      tvhwarn(LS_SUBSCRIPTION, "%04X: service instance is bad, reason: %s",
              shortid(s), streaming_code2txt(s->ths_testing_error));

      tvh_mutex_lock(&t->s_stream_mutex);
      t->s_streaming_status = 0;
      t->s_status = SERVICE_IDLE;
      tvh_mutex_unlock(&t->s_stream_mutex);

      si = s->ths_current_instance;
      assert(si != NULL);

      subscription_unlink_service0(s, SM_CODE_BAD_SOURCE, 1);

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
      if (s->ths_last_error != error ||
          s->ths_last_find + sec2mono(2) >= mclk() ||
          error == SM_CODE_TUNING_FAILED) {
        tvhtrace(LS_SUBSCRIPTION, "%04X: instance not available, retrying", shortid(s));
        if (s->ths_last_error != error)
          s->ths_last_find = mclk();
        s->ths_last_error = error;
        continue;
      }
      if (s->ths_flags & SUBSCRIPTION_RESTART) {
        if (s->ths_channel)
          tvhwarn(LS_SUBSCRIPTION, "%04X: restarting channel %s",
                  shortid(s), channel_get_name(s->ths_channel, channel_blank_name));
        else
          tvhwarn(LS_SUBSCRIPTION, "%04X: restarting service %s",
                  shortid(s), s->ths_service->s_nicename);
        s->ths_testing_error = 0;
        s->ths_current_instance = NULL;
        service_instance_list_clear(&s->ths_instances);
        sm = streaming_msg_create_code(SMT_NOSTART_WARN, error);
        streaming_target_deliver(s->ths_output, sm);
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
  subscription_delayed_reschedule(sec2mono(postpone));
  reenter = 0;
}

/**
 *
 */
void
subscription_set_weight(th_subscription_t *s, unsigned int weight)
{
  lock_assert(&global_lock);
  s->ths_weight = weight;
}

/**
 *
 */
static int64_t
subscription_set_postpone(void *aux, const char *path, int64_t postpone)
{
  th_subscription_t *s;
  int64_t now = mclk();
  int64_t postpone2;

  if (strcmp(path, "/set"))
    return -1;
  /* some limits that make sense */
  postpone = MINMAX(postpone, 0, 120);
  postpone2 = sec2mono(postpone);
  tvh_mutex_lock(&global_lock);
  if (subscription_postpone != postpone) {
    subscription_postpone = postpone;
    tvhinfo(LS_SUBSCRIPTION, "postpone set to %"PRId64" seconds", postpone);
    LIST_FOREACH(s, &subscriptions, ths_global_link) {
      s->ths_postpone = postpone;
      if (s->ths_postpone_end > now && s->ths_postpone_end - now > postpone2)
        s->ths_postpone_end = now + postpone2;
    }
    subscription_delayed_reschedule(0);
  }
  tvh_mutex_unlock(&global_lock);
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
  if (sm->sm_type == SMT_STOP && subgetstate(s) != SUBSCRIPTION_ZOMBIE) {
    LIST_INSERT_HEAD(&subscriptions_remove, s, ths_remove_link);
    subscription_delayed_reschedule(0);
  }

  streaming_msg_free(sm);
}

static htsmsg_t *
subscription_input_null_info(void *opaque, htsmsg_t *list)
{
  htsmsg_add_str(list, NULL, "null input");
  return list;
}

static streaming_ops_t subscription_input_null_ops = {
  .st_cb   = subscription_input_null,
  .st_info = subscription_input_null_info
};

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
    atomic_add(&s->ths_total_err, pkt->pkt_err);
    if (pkt->pkt_payload)
      subscription_add_bytes_in(s, pktbuf_len(pkt->pkt_payload));
  } else if(sm->sm_type == SMT_MPEGTS) {
    pktbuf_t *pb = sm->sm_data;
    atomic_add(&s->ths_total_err, pb->pb_err);
    subscription_add_bytes_in(s, pktbuf_len(pb));
  }

  /* Pass to output */
  streaming_target_deliver(s->ths_output, sm);
}

static htsmsg_t *
subscription_input_direct_info(void *opaque, htsmsg_t *list)
{
  htsmsg_add_str(list, NULL, "direct input");
  return list;
}

static streaming_ops_t subscription_input_direct_ops = {
  .st_cb   = subscription_input_direct,
  .st_info = subscription_input_direct_info
};

/**
 * This callback is invoked when we receive data and status updates from
 * the currently bound service
 */
static void
subscription_input(void *opaque, streaming_message_t *sm)
{
  int error;
  th_subscription_t *s = opaque;

  if(subgetstate(s) == SUBSCRIPTION_TESTING_SERVICE) {
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
       (sm->sm_code & (TSS_ERRORS|TSS_CA_CHECK))) {
      // No, mark our subscription as bad_service
      // the scheduler will take care of things
      error = tss2errcode(sm->sm_code);
      if (error != SM_CODE_OK)
        if (error != SM_CODE_NO_ACCESS ||
            (s->ths_flags & SUBSCRIPTION_CONTACCESS) == 0) {
          if (error > s->ths_testing_error)
            s->ths_testing_error = error;
          subsetstate(s, SUBSCRIPTION_BAD_SERVICE);
          streaming_msg_free(sm);
          return;
        }
    }

    if(sm->sm_type == SMT_SERVICE_STATUS &&
       (sm->sm_code & TSS_PACKETS)) {
      if(s->ths_start_message != NULL) {
        streaming_target_deliver(s->ths_output, s->ths_start_message);
        s->ths_start_message = NULL;
        if (s->ths_service)
          s->ths_service->s_running = 1;
      }
      subsetstate(s, SUBSCRIPTION_GOT_SERVICE);
    }
  }

  if(subgetstate(s) != SUBSCRIPTION_GOT_SERVICE) {
    streaming_msg_free(sm);
    return;
  }

  if (sm->sm_type == SMT_SERVICE_STATUS &&
      (sm->sm_code & (TSS_TUNING|TSS_TIMEOUT|TSS_NO_DESCRAMBLER|TSS_CA_CHECK))) {
    error = tss2errcode(sm->sm_code);
    if (error != SM_CODE_OK)
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

static htsmsg_t *
subscription_input_info(void *opaque, htsmsg_t *list)
{
  th_subscription_t *s = opaque;
  streaming_target_t *st = s->ths_output;
  htsmsg_add_str(list, NULL, "input");
  return st->st_ops.st_info(st->st_opaque, list);
}

static streaming_ops_t subscription_input_ops = {
  .st_cb   = subscription_input,
  .st_info = subscription_input_info
};


/* **************************************************************************
 * Destroy subscriptions
 * *************************************************************************/

/**
 * Delete
 */
static void
subscription_unsubscribe_cb(void *aux)
{
  subscription_unsubscribe((th_subscription_t *)aux, UNSUBSCRIBE_FINAL);
}

static void
subscription_destroy(th_subscription_t *s)
{
  streaming_msg_free(s->ths_start_message);

  if(s->ths_output->st_ops.st_cb == subscription_input_null)
   free(s->ths_output);

  free(s->ths_title);
  free(s->ths_hostname);
  free(s->ths_username);
  free(s->ths_client);
  free(s->ths_dvrfile);
  free(s);

}

void
subscription_unsubscribe(th_subscription_t *s, int flags)
{
  service_t *t;
  char buf[512];
  size_t l = 0;
  service_t *raw;

  if (s == NULL)
    return;

  lock_assert(&global_lock);

  t   = s->ths_service;
  raw = s->ths_raw_service;

  if (subgetstate(s) == SUBSCRIPTION_ZOMBIE) {
    if ((flags & UNSUBSCRIBE_FINAL) != 0) {
      subscription_destroy(s);
      return;
    }
    abort();
  }
  subsetstate(s, SUBSCRIPTION_ZOMBIE);

  LIST_REMOVE(s, ths_global_link);
  LIST_SAFE_REMOVE(s, ths_remove_link);

#if ENABLE_MPEGTS
  if (raw && t == raw) {
    LIST_REMOVE(s, ths_mux_link);
    service_remove_raw(raw);
  }
#endif

  if (s->ths_channel != NULL) {
    LIST_REMOVE(s, ths_channel_link);
    tvh_strlcatf(buf, sizeof(buf), l, "\"%s\" unsubscribing from \"%s\"",
             s->ths_title, channel_get_name(s->ths_channel, channel_blank_name));
  } else {
    tvh_strlcatf(buf, sizeof(buf), l, "\"%s\" unsubscribing", s->ths_title);
  }

  if (s->ths_hostname)
    tvh_strlcatf(buf, sizeof(buf), l, ", hostname=\"%s\"", s->ths_hostname);
  if (s->ths_username)
    tvh_strlcatf(buf, sizeof(buf), l, ", username=\"%s\"", s->ths_username);
  if (s->ths_client)
    tvh_strlcatf(buf, sizeof(buf), l, ", client=\"%s\"", s->ths_client);
  tvhlog((flags & UNSUBSCRIBE_QUIET) != 0 ? LOG_TRACE : LOG_INFO,
         LS_SUBSCRIPTION, "%04X: %s", shortid(s), buf);

  if (t)
    service_remove_subscriber(t, s, SM_CODE_OK);

  service_instance_list_clear(&s->ths_instances);

  mtimer_disarm(&s->ths_remove_timer);
  mtimer_disarm(&s->ths_ca_check_timer);

  if (s->ths_parser) {
    parser_destroy(s->ths_parser);
    s->ths_parser = NULL;
  }

  if ((flags & UNSUBSCRIBE_FINAL) != 0 ||
      (s->ths_flags & SUBSCRIPTION_ONESHOT) != 0)
    subscription_destroy(s);

  subscription_delayed_reschedule(0);
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
   int flags, streaming_ops_t *ops, const char *hostname,
   const char *username, const char *client)
{
  th_subscription_t *s = calloc(1, sizeof(th_subscription_t));
  profile_t *pro = prch ? prch->prch_pro : NULL;
  streaming_target_t *st = prch ? prch->prch_st : NULL;
  static int tally;

  TAILQ_INIT(&s->ths_instances);

  if (!ops) ops = &subscription_input_direct_ops;
  if (!st) {
    st = calloc(1, sizeof(streaming_target_t));
    streaming_target_init(st, &subscription_input_null_ops, s, 0);
  }

  streaming_target_init(&s->ths_input, ops, s, 0);

  s->ths_prch              = prch && prch->prch_st ? prch : NULL;
  s->ths_title             = strdup(name);
  s->ths_hostname          = hostname ? strdup(hostname) : NULL;
  s->ths_username          = username ? strdup(username) : NULL;
  s->ths_client            = client   ? strdup(client)   : NULL;
  s->ths_output            = st;
  s->ths_flags             = flags;
  s->ths_timeout           = pro ? pro->pro_timeout : 0;
  s->ths_ca_timeout        = sec2mono(2);
  s->ths_postpone          = subscription_postpone;
  s->ths_postpone_end      = mclk() + sec2mono(s->ths_postpone);
  atomic_set(&s->ths_total_err, 0);

  if (s->ths_prch)
    s->ths_weight = profile_chain_weight(s->ths_prch, weight);
  else
    s->ths_weight = weight;

  if (pro) {
    if (pro->pro_restart)
      s->ths_flags |= SUBSCRIPTION_RESTART;
    if (pro->pro_contaccess)
      s->ths_flags |= SUBSCRIPTION_CONTACCESS;
    if (pro->pro_swservice)
      s->ths_flags |= SUBSCRIPTION_SWSERVICE;
    if (pro->pro_ca_timeout)
      s->ths_ca_timeout = ms2mono(MINMAX(pro->pro_ca_timeout, 100, 5000));
  }

  time(&s->ths_start);

  s->ths_id = ++tally;

  LIST_INSERT_SORTED(&subscriptions, s, ths_global_link, subscription_sort);

  subscription_delayed_reschedule(0);
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

  s = subscription_create(prch, weight, name, flags, &subscription_input_ops,
                          hostname, username, client);
  if (tvhtrace_enabled()) {
    const char *pro_name = prch->prch_pro ? profile_get_name(prch->prch_pro) : "<none>";
    if (ch)
      tvhtrace(LS_SUBSCRIPTION, "%04X: creating subscription for %s weight %d using profile %s",
               shortid(s), channel_get_name(ch, channel_blank_name), weight, pro_name);
    else
      tvhtrace(LS_SUBSCRIPTION, "%04X: creating subscription for service %s weight %d using profile %s",
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
      subscription_unsubscribe(s, UNSUBSCRIBE_QUIET | UNSUBSCRIBE_FINAL);
      return NULL;
    }
    subscription_link_service(s, si->si_s);
    subscription_show_info(s);
  } else {
    subscription_delayed_reschedule(0);
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
  assert(flags == SUBSCRIPTION_NONE || prch->prch_st);
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
  assert(flags == SUBSCRIPTION_NONE || prch->prch_st);
  return subscription_create_from_channel_or_service
           (prch, ti, weight, name, flags, hostname, username, client,
            error, prch->prch_id);
}

/**
 *
 */
#if ENABLE_MPEGTS
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

/**
 *
 */
th_subscription_t *
subscription_create_from_file(const char *name,
                              const char *charset,
                              const char *filename,
			      const char *hostname,
			      const char *username,
			      const char *client)
{
  th_subscription_t *ts;
  char *str, *url;

  ts = subscription_create(NULL, 1, name,
                           SUBSCRIPTION_NONE, NULL,
                           hostname, username, client);
  if (ts == NULL)
    return NULL;
  str = intlconv_to_utf8safestr(charset, filename, strlen(filename) * 3);
  if (str == NULL)
    str = intlconv_to_utf8safestr(intlconv_charset_id("ASCII", 1, 1),
                                  filename, strlen(filename) * 3);
  if (str == NULL)
    str = strdup("error");
  url = malloc(strlen(str) + 7 + 1);
  strcpy(url, "file://");
  strcat(url, str);
  ts->ths_dvrfile = url;
  free(str);
  return ts;
}

/* **************************************************************************
 * Status monitoring
 * *************************************************************************/

static mtimer_t subscription_status_timer;

/*
 * Serialize info about subscription
 */
htsmsg_t *
subscription_create_msg(th_subscription_t *s, const char *lang)
{
  htsmsg_t *m = htsmsg_create_map();
  descramble_info_t *di;
  service_t *t;
  profile_t *pro;
  char buf[284];
  const char *state;
  htsmsg_t *l;
  mpegts_apids_t *pids = NULL;

  htsmsg_add_u32(m, "id", s->ths_id);
  htsmsg_add_u32(m, "start", s->ths_start);
  htsmsg_add_u32(m, "errors", atomic_get(&s->ths_total_err));

  switch(subgetstate(s)) {
  default:
    state = N_("Idle");
    break;

  case SUBSCRIPTION_TESTING_SERVICE:
    state = N_("Testing");
    break;
    
  case SUBSCRIPTION_GOT_SERVICE:
    state = N_("Running");
    break;

  case SUBSCRIPTION_BAD_SERVICE:
    state = N_("Bad");
    break;
  }

  htsmsg_add_str(m, "state", lang ? tvh_gettext_lang(lang, state) : state);

  if (s->ths_hostname != NULL)
    htsmsg_add_str(m, "hostname", s->ths_hostname);

  if (s->ths_username != NULL)
    htsmsg_add_str(m, "username", s->ths_username);

  if (s->ths_client != NULL)
    htsmsg_add_str(m, "client", s->ths_client);

  if (s->ths_title != NULL)
    htsmsg_add_str(m, "title", s->ths_title);
  
  if (s->ths_channel != NULL)
    htsmsg_add_str(m, "channel", channel_get_name(s->ths_channel, tvh_gettext_lang(lang, channel_blank_name)));
  
  if ((t = s->ths_service) != NULL) {
    htsmsg_add_str(m, "service", service_adapter_nicename(t, buf, sizeof(buf)));

    tvh_mutex_lock(&t->s_stream_mutex);
    if ((di = t->s_descramble_info) != NULL) {
      if (di->caid == 0 && di->ecmtime == 0) {
        snprintf(buf, sizeof(buf), N_("Failed"));
      } else {
        snprintf(buf, sizeof(buf), "%04X:%06X(%ums)-%s%s%s",
                 di->caid, di->provid, di->ecmtime, di->from,
                 di->reader[0] ? "/" : "", di->reader);
      }
      htsmsg_add_str(m, "descramble", buf);
    }
    tvh_mutex_unlock(&t->s_stream_mutex);

    if (t->s_pid_list) {
      pids = t->s_pid_list(t);
      if (pids) {
        l = htsmsg_create_list();
        if (pids->all) {
          htsmsg_add_u32(l, NULL, 65535);
        } else {
          int i;
          for (i = 0; i < pids->count; i++) {
            htsmsg_add_u32(l, NULL, pids->pids[i].pid);
          }
        }
        htsmsg_add_msg(m, "pids", l);
        mpegts_pid_destroy(&pids);
      }
    }

    if (s->ths_prch != NULL) {
      pro = s->ths_prch->prch_pro;
      if (pro)
        htsmsg_add_str(m, "profile",
                       idnode_get_title(&pro->pro_id, lang, buf, sizeof(buf)));
    }

  } else if (s->ths_dvrfile != NULL) {
    htsmsg_add_str(m, "service", s->ths_dvrfile ?: "");
  }

  htsmsg_add_u32(m, "in", atomic_get(&s->ths_bytes_in_avg));
  htsmsg_add_u32(m, "out", atomic_get(&s->ths_bytes_out_avg));
  htsmsg_add_s64(m, "total_in", atomic_get_u64(&s->ths_total_bytes_in));
  htsmsg_add_s64(m, "total_out", atomic_get_u64(&s->ths_total_bytes_out));

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

  mtimer_arm_rel(&subscription_status_timer,
                 subscription_status_callback, NULL, sec2mono(1));

  LIST_FOREACH(s, &subscriptions, ths_global_link) {
    /* Store the difference between total bytes from the last round */
    uint64_t in_curr = atomic_get_u64(&s->ths_total_bytes_in);
    uint64_t in_prev = atomic_exchange_u64(&s->ths_total_bytes_in_prev, in_curr);
    uint64_t out_curr = atomic_get_u64(&s->ths_total_bytes_out);
    uint64_t out_prev = atomic_exchange_u64(&s->ths_total_bytes_out_prev, out_curr);

    atomic_set(&s->ths_bytes_in_avg, (int)(in_curr - in_prev));
    atomic_set(&s->ths_bytes_out_avg, (int)(out_curr - out_prev));

    htsmsg_t *m = subscription_create_msg(s, NULL);
    htsmsg_add_u32(m, "updateEntry", 1);
    notify_by_msg("subscriptions", m, NOTIFY_REWRITE_SUBSCRIPTIONS);
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
  tvh_mutex_lock(&global_lock);
  mtimer_disarm(&subscription_status_timer);
  /* clear remaining subscriptions */
  subscription_reschedule();
  tvh_mutex_unlock(&global_lock);
  assert(LIST_FIRST(&subscriptions) == NULL);
}

/* **************************************************************************
 * Subscription control
 * *************************************************************************/

/**
 * Update incoming byte count
 */
void subscription_add_bytes_in(th_subscription_t *s, size_t in)
{
  atomic_add_u64(&s->ths_total_bytes_in, in);
}

/**
 * Update outgoing byte count
 */
void subscription_add_bytes_out(th_subscription_t *s, size_t out)
{
  atomic_add_u64(&s->ths_total_bytes_out, out);
}

/**
 * Change weight
 */
void
subscription_change_weight(th_subscription_t *s, int weight)
{
  if(s->ths_weight == weight)
    return;

  LIST_REMOVE(s, ths_global_link);

  if (s->ths_prch)
    s->ths_weight = profile_chain_weight(s->ths_prch, weight);
  else
    s->ths_weight = weight;

  LIST_INSERT_SORTED(&subscriptions, s, ths_global_link, subscription_sort);

  subscription_delayed_reschedule(0);
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

  tvh_mutex_lock(&t->s_stream_mutex);

  sm = streaming_msg_create_code(SMT_SPEED, speed);

  streaming_target_deliver(s->ths_output, sm);

  tvh_mutex_unlock(&t->s_stream_mutex);
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

  tvh_mutex_lock(&t->s_stream_mutex);

  sm = streaming_msg_create(SMT_SKIP);
  sm->sm_data = malloc(sizeof(streaming_skip_t));
  memcpy(sm->sm_data, skip, sizeof(streaming_skip_t));

  streaming_target_deliver(s->ths_output, sm);

  tvh_mutex_unlock(&t->s_stream_mutex);
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

static htsmsg_t *
dummy_info(void *opaque, htsmsg_t *list)
{
  htsmsg_add_str(list, NULL, "null input");
  return list;
}

static streaming_ops_t dummy_ops = {
  .st_cb   = dummy_callback,
  .st_info = dummy_info
};


static mtimer_t dummy_sub_timer;
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
  service_t *t = service_find_by_uuid(id);
  profile_chain_t *prch;
  streaming_target_t *st;
  th_subscription_t *s;

  if(first) {
    mtimer_arm_rel(&dummy_sub_timer, dummy_retry, strdup(id), sec2mono(2));
    return;
  }

  if(t == NULL) {
    tvherror(LS_SUBSCRIPTION, 
	    "Unable to dummy join %s, service not found, retrying...", id);

    mtimer_arm_rel(&dummy_sub_timer, dummy_retry, strdup(id), sec2mono(1));
    return;
  }

  prch = calloc(1, sizeof(*prch));
  prch->prch_id = t;
  st = calloc(1, sizeof(*st));
  streaming_target_init(st, &dummy_ops, NULL, 0);
  prch->prch_st = st;
  s = subscription_create_from_service(prch, NULL, 1, "dummy", 0, NULL, NULL, "dummy", NULL);

  tvhnotice(LS_SUBSCRIPTION, "%04X: Dummy join %s ok", shortid(s), id);
}
