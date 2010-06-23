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

#include <pthread.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "tvhead.h"
#include "transports.h"
#include "subscriptions.h"
#include "tsdemux.h"
#include "streaming.h"
#include "v4l.h"
#include "psi.h"
#include "packet.h"
#include "channels.h"
#include "cwc.h"
#include "capmt.h"
#include "notify.h"
#include "serviceprobe.h"
#include "atomic.h"
#include "dvb/dvb.h"
#include "htsp.h"

#define TRANSPORT_HASH_WIDTH 101

static struct th_transport_list transporthash[TRANSPORT_HASH_WIDTH];

static void transport_data_timeout(void *aux);

/**
 *
 */
static void
stream_init(th_stream_t *st)
{
  st->st_cc_valid = 0;

  st->st_startcond = 0xffffffff;
  st->st_curdts = AV_NOPTS_VALUE;
  st->st_curpts = AV_NOPTS_VALUE;
  st->st_prevdts = AV_NOPTS_VALUE;

  st->st_pcr_real_last = AV_NOPTS_VALUE;
  st->st_pcr_last      = AV_NOPTS_VALUE;
  st->st_pcr_drift     = 0;
  st->st_pcr_recovery_fails = 0;
}


/**
 *
 */
static void
stream_clean(th_stream_t *st)
{
  if(st->st_demuxer_fd != -1) {
    // XXX: Should be in DVB-code perhaps
    close(st->st_demuxer_fd);
    st->st_demuxer_fd = -1;
  }

  free(st->st_priv);
  st->st_priv = NULL;

  /* Clear reassembly buffers */

  st->st_startcode = 0;
    
  free(st->st_buffer);
  st->st_buffer = NULL;
  st->st_buffer_size = 0;
  st->st_buffer_ptr = 0;

  free(st->st_buffer2);
  st->st_buffer2 = NULL;
  st->st_buffer2_size = 0;
  st->st_buffer2_ptr = 0;

  free(st->st_buffer3);
  st->st_buffer3 = NULL;
  st->st_buffer3_size = 0;
  st->st_buffer3_ptr = 0;


  if(st->st_curpkt != NULL) {
    pkt_ref_dec(st->st_curpkt);
    st->st_curpkt = NULL;
  }

  free(st->st_global_data);
  st->st_global_data = NULL;
  st->st_global_data_len = 0;
}


/**
 *
 */
void
transport_stream_destroy(th_transport_t *t, th_stream_t *st)
{
  if(t->tht_status == TRANSPORT_RUNNING)
    stream_clean(st);
  TAILQ_REMOVE(&t->tht_components, st, st_link);
  free(st->st_nicename);
  free(st);
}

/**
 * Transport lock must be held
 */
static void
transport_stop(th_transport_t *t)
{
  th_descrambler_t *td;
  th_stream_t *st;
 
  gtimer_disarm(&t->tht_receive_timer);

  t->tht_stop_feed(t);

  pthread_mutex_lock(&t->tht_stream_mutex);

  while((td = LIST_FIRST(&t->tht_descramblers)) != NULL)
    td->td_stop(td);

  t->tht_tt_commercial_advice = COMMERCIAL_UNKNOWN;
 
  assert(LIST_FIRST(&t->tht_streaming_pad.sp_targets) == NULL);
  assert(LIST_FIRST(&t->tht_subscriptions) == NULL);

  /**
   * Clean up each stream
   */
  TAILQ_FOREACH(st, &t->tht_components, st_link)
    stream_clean(st);

  t->tht_status = TRANSPORT_IDLE;

  pthread_mutex_unlock(&t->tht_stream_mutex);
}


/**
 * Remove the given subscriber from the transport
 *
 * if s == NULL all subscribers will be removed
 *
 * Global lock must be held
 */
void
transport_remove_subscriber(th_transport_t *t, th_subscription_t *s,
			    int reason)
{
  lock_assert(&global_lock);

  if(s == NULL) {
    while((s = LIST_FIRST(&t->tht_subscriptions)) != NULL) {
      subscription_unlink_transport(s, reason);
    }
  } else {
    subscription_unlink_transport(s, reason);
  }

  if(LIST_FIRST(&t->tht_subscriptions) == NULL)
    transport_stop(t);
}


/**
 *
 */
int
transport_start(th_transport_t *t, unsigned int weight, int force_start)
{
  th_stream_t *st;
  int r, timeout = 2;

  lock_assert(&global_lock);

  assert(t->tht_status != TRANSPORT_RUNNING);
  t->tht_streaming_status = 0;
  t->tht_pcr_drift = 0;

  if((r = t->tht_start_feed(t, weight, force_start)))
    return r;

  cwc_transport_start(t);
  capmt_transport_start(t);

  pthread_mutex_lock(&t->tht_stream_mutex);

  t->tht_status = TRANSPORT_RUNNING;

  /**
   * Initialize stream
   */
  TAILQ_FOREACH(st, &t->tht_components, st_link)
    stream_init(st);

  pthread_mutex_unlock(&t->tht_stream_mutex);

  if(t->tht_grace_period != NULL)
    timeout = t->tht_grace_period(t);

  gtimer_arm(&t->tht_receive_timer, transport_data_timeout, t, timeout);
  return 0;
}

/**
 *
 */
static int
dvb_extra_prio(th_dvb_adapter_t *tda)
{
  return tda->tda_hostconnection * 10;
}

/**
 * Return prio for the given transport
 */
static int
transport_get_prio(th_transport_t *t)
{
  switch(t->tht_type) {
  case TRANSPORT_DVB:
    return (t->tht_scrambled ? 300 : 100) + 
      dvb_extra_prio(t->tht_dvb_mux_instance->tdmi_adapter);

  case TRANSPORT_IPTV:
    return 200;

  case TRANSPORT_V4L:
    return 400;

  default:
    return 500;
  }
}

/**
 * Return quality index for given transport
 *
 * We invert the result (providers say that negative numbers are worse)
 *
 * But for sorting, we want low numbers first
 *
 * Also, we bias and trim with an offset of two to avoid counting any
 * transient errors.
 */

static int
transport_get_quality(th_transport_t *t)
{
  return t->tht_quality_index ? -MIN(t->tht_quality_index(t) + 2, 0) : 0;
}




/**
 *  a - b  -> lowest number first
 */
static int
transportcmp(const void *A, const void *B)
{
  th_transport_t *a = *(th_transport_t **)A;
  th_transport_t *b = *(th_transport_t **)B;

  int q = transport_get_quality(a) - transport_get_quality(b);

  if(q != 0)
    return q; /* Quality precedes priority */

  return transport_get_prio(a) - transport_get_prio(b);
}


/**
 *
 */
th_transport_t *
transport_find(channel_t *ch, unsigned int weight, const char *loginfo,
	       int *errorp, th_transport_t *skip)
{
  th_transport_t *t, **vec;
  int cnt = 0, i, r, off;
  int err = 0;

  lock_assert(&global_lock);

  /* First, sort all transports in order */

  LIST_FOREACH(t, &ch->ch_transports, tht_ch_link)
    cnt++;

  vec = alloca(cnt * sizeof(th_transport_t *));
  cnt = 0;
  LIST_FOREACH(t, &ch->ch_transports, tht_ch_link) {

    if(!t->tht_enabled) {
      if(loginfo != NULL) {
	tvhlog(LOG_NOTICE, "Transport", "%s: Skipping \"%s\" -- not enabled",
	       loginfo, transport_nicename(t));
	err = SM_CODE_SVC_NOT_ENABLED;
      }
      continue;
    }

    if(t->tht_quality_index(t) < 10) {
      if(loginfo != NULL) {
	tvhlog(LOG_NOTICE, "Transport", 
	       "%s: Skipping \"%s\" -- Quality below 10%",
	       loginfo, transport_nicename(t));
	err = SM_CODE_BAD_SIGNAL;
      }
      continue;
    }
    vec[cnt++] = t;
  }

  /* Sort transports, lower priority should come come earlier in the vector
     (i.e. it will be more favoured when selecting a transport */

  qsort(vec, cnt, sizeof(th_transport_t *), transportcmp);

  // Skip up to the transport that the caller didn't want
  // If the sorting above is not stable that might mess up things
  // temporary. But it should resolve itself eventually
  if(skip != NULL) {
    for(i = 0; i < cnt; i++) {
      if(skip == t)
	break;
    }
    off = i + 1;
  } else {
    off = 0;
  }

  /* First, try all transports without stealing */
  for(i = off; i < cnt; i++) {
    t = vec[i];
    if(t->tht_status == TRANSPORT_RUNNING) 
      return t;
    if((r = transport_start(t, 0, 0)) == 0)
      return t;
    if(loginfo != NULL)
      tvhlog(LOG_DEBUG, "Transport", "%s: Unable to use \"%s\" -- %s",
	     loginfo, transport_nicename(t), streaming_code2txt(r));
  }

  /* Ok, nothing, try again, but supply our weight and thus, try to steal
     transponders */

  for(i = off; i < cnt; i++) {
    t = vec[i];
    if((r = transport_start(t, weight, 0)) == 0)
      return t;
    *errorp = r;
  }
  if(err)
    *errorp = err;
  else if(*errorp == 0)
    *errorp = SM_CODE_NO_TRANSPORT;
  return NULL;
}


/**
 *
 */
unsigned int 
transport_compute_weight(struct th_transport_list *head)
{
  th_transport_t *t;
  th_subscription_t *s;
  int w = 0;

  lock_assert(&global_lock);

  LIST_FOREACH(t, head, tht_active_link) {
    LIST_FOREACH(s, &t->tht_subscriptions, ths_transport_link) {
      if(s->ths_weight > w)
	w = s->ths_weight;
    }
  }
  return w;
}


/**
 *
 */
void
transport_unref(th_transport_t *t)
{
  if((atomic_add(&t->tht_refcount, -1)) == 1)
    free(t);
}


/**
 *
 */
void
transport_ref(th_transport_t *t)
{
  atomic_add(&t->tht_refcount, 1);
}



/**
 * Destroy a transport
 */
void
transport_destroy(th_transport_t *t)
{
  th_stream_t *st;
  th_subscription_t *s;
  
  lock_assert(&global_lock);

  serviceprobe_delete(t);

  while((s = LIST_FIRST(&t->tht_subscriptions)) != NULL) {
    subscription_unlink_transport(s, SM_CODE_SOURCE_DELETED);
  }

  if(t->tht_ch != NULL) {
    t->tht_ch = NULL;
    LIST_REMOVE(t, tht_ch_link);
  }

  LIST_REMOVE(t, tht_group_link);
  LIST_REMOVE(t, tht_hash_link);
  
  if(t->tht_status != TRANSPORT_IDLE)
    transport_stop(t);

  t->tht_status = TRANSPORT_ZOMBIE;

  free(t->tht_identifier);
  free(t->tht_svcname);
  free(t->tht_provider);

  while((st = TAILQ_FIRST(&t->tht_components)) != NULL) {
    TAILQ_REMOVE(&t->tht_components, st, st_link);
    free(st->st_nicename);
    free(st);
  }

  free(t->tht_pat_section);
  free(t->tht_pmt_section);

  transport_unref(t);
}


/**
 * Create and initialize a new transport struct
 */
th_transport_t *
transport_create(const char *identifier, int type, int source_type)
{
  unsigned int hash = tvh_strhash(identifier, TRANSPORT_HASH_WIDTH);
  th_transport_t *t = calloc(1, sizeof(th_transport_t));

  lock_assert(&global_lock);

  pthread_mutex_init(&t->tht_stream_mutex, NULL);
  pthread_cond_init(&t->tht_tss_cond, NULL);
  t->tht_identifier = strdup(identifier);
  t->tht_type = type;
  t->tht_source_type = source_type;
  t->tht_refcount = 1;
  t->tht_enabled = 1;
  t->tht_pcr_last = AV_NOPTS_VALUE;
  TAILQ_INIT(&t->tht_components);

  streaming_pad_init(&t->tht_streaming_pad);

  LIST_INSERT_HEAD(&transporthash[hash], t, tht_hash_link);
  return t;
}

/**
 * Find a transport based on the given identifier
 */
th_transport_t *
transport_find_by_identifier(const char *identifier)
{
  th_transport_t *t;
  unsigned int hash = tvh_strhash(identifier, TRANSPORT_HASH_WIDTH);

  lock_assert(&global_lock);

  LIST_FOREACH(t, &transporthash[hash], tht_hash_link)
    if(!strcmp(t->tht_identifier, identifier))
      break;
  return t;
}


/**
 *
 */
static void 
transport_stream_make_nicename(th_transport_t *t, th_stream_t *st)
{
  char buf[200];
  if(st->st_pid != -1)
    snprintf(buf, sizeof(buf), "%s: %s @ #%d", 
	     transport_nicename(t),
	     streaming_component_type2txt(st->st_type), st->st_pid);
  else
    snprintf(buf, sizeof(buf), "%s: %s", 
	     transport_nicename(t),
	     streaming_component_type2txt(st->st_type));

  free(st->st_nicename);
  st->st_nicename = strdup(buf);
}


/**
 *
 */
void 
transport_make_nicename(th_transport_t *t)
{
  char buf[200];
  source_info_t si;
  th_stream_t *st;

  lock_assert(&t->tht_stream_mutex);

  t->tht_setsourceinfo(t, &si);

  snprintf(buf, sizeof(buf), 
	   "%s%s%s%s%s",
	   si.si_adapter ?: "", si.si_adapter ? "/" : "",
	   si.si_mux     ?: "", si.si_mux     ? "/" : "",
	   si.si_service ?: "");

  transport_source_info_free(&si);

  free(t->tht_nicename);
  t->tht_nicename = strdup(buf);

  TAILQ_FOREACH(st, &t->tht_components, st_link)
    transport_stream_make_nicename(t, st);
}


/**
 * Add a new stream to a transport
 */
th_stream_t *
transport_stream_create(th_transport_t *t, int pid,
			streaming_component_type_t type)
{
  th_stream_t *st;
  int i = 0;
  int idx = 0;
  lock_assert(&t->tht_stream_mutex);

  TAILQ_FOREACH(st, &t->tht_components, st_link) {
    if(st->st_index > idx)
      idx = st->st_index;
    i++;
    if(pid != -1 && st->st_pid == pid)
      return st;
  }

  st = calloc(1, sizeof(th_stream_t));
  st->st_index = idx + 1;
  st->st_type = type;

  TAILQ_INSERT_TAIL(&t->tht_components, st, st_link);
  st->st_transport = t;

  st->st_pid = pid;
  st->st_demuxer_fd = -1;

  avgstat_init(&st->st_rate, 10);
  avgstat_init(&st->st_cc_errors, 10);

  transport_stream_make_nicename(t, st);

  if(t->tht_flags & THT_DEBUG)
    tvhlog(LOG_DEBUG, "transport", "Add stream %s", st->st_nicename);

  if(t->tht_status == TRANSPORT_RUNNING)
    stream_init(st);

  return st;
}



/**
 * Add a new stream to a transport
 */
th_stream_t *
transport_stream_find(th_transport_t *t, int pid)
{
  th_stream_t *st;
 
  lock_assert(&t->tht_stream_mutex);

  TAILQ_FOREACH(st, &t->tht_components, st_link) {
    if(st->st_pid == pid)
      return st;
  }
  return NULL;
}



/**
 *
 */
void
transport_map_channel(th_transport_t *t, channel_t *ch, int save)
{
  lock_assert(&global_lock);

  if(t->tht_ch != NULL) {
    LIST_REMOVE(t, tht_ch_link);
    htsp_channel_update(t->tht_ch);
    t->tht_ch = NULL;
  }


  if(ch != NULL) {

    avgstat_init(&t->tht_cc_errors, 3600);
    avgstat_init(&t->tht_rate, 10);

    t->tht_ch = ch;
    LIST_INSERT_HEAD(&ch->ch_transports, t, tht_ch_link);
    htsp_channel_update(t->tht_ch);
  }

  if(save)
    t->tht_config_save(t);
}

/**
 *
 */
static void
transport_data_timeout(void *aux)
{
  th_transport_t *t = aux;

  pthread_mutex_lock(&t->tht_stream_mutex);

  if(!(t->tht_streaming_status & TSS_PACKETS))
    transport_set_streaming_status_flags(t, TSS_GRACEPERIOD);

  pthread_mutex_unlock(&t->tht_stream_mutex);
}

/**
 *
 */
static struct strtab stypetab[] = {
  { "SDTV",         ST_SDTV },
  { "Radio",        ST_RADIO },
  { "HDTV",         ST_HDTV },
  { "SDTV-AC",      ST_AC_SDTV },
  { "HDTV-AC",      ST_AC_HDTV },
};

const char *
transport_servicetype_txt(th_transport_t *t)
{
  return val2str(t->tht_servicetype, stypetab) ?: "Other";
}

/**
 *
 */
int
transport_is_tv(th_transport_t *t)
{
  return 
    t->tht_servicetype == ST_SDTV    ||
    t->tht_servicetype == ST_HDTV    ||
    t->tht_servicetype == ST_AC_SDTV ||
    t->tht_servicetype == ST_AC_HDTV;
}


/**
 *
 */
void
transport_set_streaming_status_flags(th_transport_t *t, int set)
{
  int n;
  streaming_message_t *sm;
  lock_assert(&t->tht_stream_mutex);
  
  n = t->tht_streaming_status;
  
  n |= set;

  if(n == t->tht_streaming_status)
    return; // Already set

  t->tht_streaming_status = n;

  tvhlog(LOG_DEBUG, "Transport", "%s: Status changed to %s%s%s%s%s%s%s",
	 transport_nicename(t),
	 n & TSS_INPUT_HARDWARE ? "[Hardware input] " : "",
	 n & TSS_INPUT_SERVICE  ? "[Input on service] " : "",
	 n & TSS_MUX_PACKETS    ? "[Demuxed packets] " : "",
	 n & TSS_PACKETS        ? "[Reassembled packets] " : "",
	 n & TSS_NO_DESCRAMBLER ? "[No available descrambler] " : "",
	 n & TSS_NO_ACCESS      ? "[No access] " : "",
	 n & TSS_GRACEPERIOD    ? "[Graceperiod expired] " : "");

  sm = streaming_msg_create_code(SMT_TRANSPORT_STATUS,
				 t->tht_streaming_status);
  streaming_pad_deliver(&t->tht_streaming_pad, sm);
  streaming_msg_free(sm);

  pthread_cond_broadcast(&t->tht_tss_cond);
}


/**
 * Restart output on a transport.
 * Happens if the stream composition changes. 
 * (i.e. an AC3 stream disappears, etc)
 */
void
transport_restart(th_transport_t *t, int had_components)
{
  streaming_message_t *sm;
  lock_assert(&t->tht_stream_mutex);

  if(had_components) {
    sm = streaming_msg_create_code(SMT_STOP, SM_CODE_SOURCE_RECONFIGURED);
    streaming_pad_deliver(&t->tht_streaming_pad, sm);
    streaming_msg_free(sm);
  }

  if(t->tht_refresh_feed != NULL)
    t->tht_refresh_feed(t);

  if(TAILQ_FIRST(&t->tht_components) != NULL) {

    sm = streaming_msg_create_data(SMT_START, 
				   transport_build_stream_start(t));
    streaming_pad_deliver(&t->tht_streaming_pad, sm);
    streaming_msg_free(sm);
  }
}


/**
 * Generate a message containing info about all components
 */
streaming_start_t *
transport_build_stream_start(th_transport_t *t)
{
  th_stream_t *st;
  int n = 0;
  streaming_start_t *ss;

  lock_assert(&t->tht_stream_mutex);
  
  TAILQ_FOREACH(st, &t->tht_components, st_link)
    n++;

  ss = calloc(1, sizeof(streaming_start_t) + 
	      sizeof(streaming_start_component_t) * n);

  ss->ss_num_components = n;
  
  n = 0;
  TAILQ_FOREACH(st, &t->tht_components, st_link) {
    streaming_start_component_t *ssc = &ss->ss_components[n++];
    ssc->ssc_index = st->st_index;
    ssc->ssc_type  = st->st_type;
    memcpy(ssc->ssc_lang, st->st_lang, 4);
    ssc->ssc_composition_id = st->st_composition_id;
    ssc->ssc_ancillary_id = st->st_ancillary_id;
    ssc->ssc_width = st->st_width;
    ssc->ssc_height = st->st_height;
  }

  t->tht_setsourceinfo(t, &ss->ss_si);

  ss->ss_refcount = 1;
  return ss;
}


/**
 *
 */
void
transport_set_enable(th_transport_t *t, int enabled)
{
  if(t->tht_enabled == enabled)
    return;

  t->tht_enabled = enabled;
  t->tht_config_save(t);
  subscription_reschedule();
}


static pthread_mutex_t pending_save_mutex;
static pthread_cond_t pending_save_cond;
static struct th_transport_queue pending_save_queue;

/**
 *
 */
void
transport_request_save(th_transport_t *t, int restart)
{
  pthread_mutex_lock(&pending_save_mutex);

  if(!t->tht_ps_onqueue) {
    t->tht_ps_onqueue = 1 + !!restart;
    TAILQ_INSERT_TAIL(&pending_save_queue, t, tht_ps_link);
    transport_ref(t);
    pthread_cond_signal(&pending_save_cond);
  } else if(restart) {
    t->tht_ps_onqueue = 2; // upgrade to restart too
  }

  pthread_mutex_unlock(&pending_save_mutex);
}


/**
 *
 */
static void *
transport_saver(void *aux)
{
  th_transport_t *t;
  int restart;
  pthread_mutex_lock(&pending_save_mutex);

  while(1) {

    if((t = TAILQ_FIRST(&pending_save_queue)) == NULL) {
      pthread_cond_wait(&pending_save_cond, &pending_save_mutex);
      continue;
    }
    assert(t->tht_ps_onqueue != 0);
    restart = t->tht_ps_onqueue == 2;

    TAILQ_REMOVE(&pending_save_queue, t, tht_ps_link);
    t->tht_ps_onqueue = 0;

    pthread_mutex_unlock(&pending_save_mutex);
    pthread_mutex_lock(&global_lock);

    if(t->tht_status != TRANSPORT_ZOMBIE)
      t->tht_config_save(t);
    if(t->tht_status == TRANSPORT_RUNNING && restart) {
      pthread_mutex_lock(&t->tht_stream_mutex);
      transport_restart(t, 1);
      pthread_mutex_unlock(&t->tht_stream_mutex);
    }
    transport_unref(t);

    pthread_mutex_unlock(&global_lock);
    pthread_mutex_lock(&pending_save_mutex);
  }
  return NULL;
}


/**
 *
 */
void
transport_init(void)
{
  pthread_t tid;
  TAILQ_INIT(&pending_save_queue);
  pthread_mutex_init(&pending_save_mutex, NULL);
  pthread_cond_init(&pending_save_cond, NULL);
  pthread_create(&tid, NULL, transport_saver, NULL);
}


/**
 *
 */
void
transport_source_info_free(struct source_info *si)
{
  free(si->si_device);
  free(si->si_adapter);
  free(si->si_network);
  free(si->si_mux);
  free(si->si_provider);
  free(si->si_service);
}


void
transport_source_info_copy(source_info_t *dst, const source_info_t *src)
{
#define COPY(x) dst->si_##x = src->si_##x ? strdup(src->si_##x) : NULL
  COPY(device);
  COPY(adapter);
  COPY(network);
  COPY(mux);
  COPY(provider);
  COPY(service);
#undef COPY
}


/**
 *
 */
const char *
transport_nicename(th_transport_t *t)
{
  return t->tht_nicename;
}

const char *
transport_component_nicename(th_stream_t *st)
{
  return st->st_nicename;
}

const char *
transport_tss2text(int flags)
{
  if(flags & TSS_NO_ACCESS)
    return "No access";

  if(flags & TSS_NO_DESCRAMBLER)
    return "No descrambler";

  if(flags & TSS_PACKETS)
    return "Got valid packets";

  if(flags & TSS_MUX_PACKETS)
    return "Got multiplexed packets but could not decode further";

  if(flags & TSS_INPUT_SERVICE)
    return "Got packets for this service but could not decode further";

  if(flags & TSS_INPUT_HARDWARE)
    return "Sensed input from hardware but nothing for the service";

  if(flags & TSS_GRACEPERIOD)
    return "No input detected";

  return "No status";
}


/**
 *
 */
int
tss2errcode(int tss)
{
  if(tss & TSS_NO_ACCESS)
    return SM_CODE_NO_ACCESS;

  if(tss & TSS_NO_DESCRAMBLER)
    return SM_CODE_NO_DESCRAMBLER;

  if(tss & TSS_GRACEPERIOD)
    return SM_CODE_NO_INPUT;

  return SM_CODE_OK;
}


/**
 *
 */
void
transport_refresh_channel(th_transport_t *t)
{
  if(t->tht_ch != NULL)
    htsp_channel_update(t->tht_ch);
}
