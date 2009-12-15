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


#define TRANSPORT_HASH_WIDTH 101

static struct th_transport_list transporthash[TRANSPORT_HASH_WIDTH];

static void transport_data_timeout(void *aux);


/**
 *
 */
static void
stream_init(th_stream_t *st)
{
  AVCodec *c;
  enum CodecID id;

  st->st_startcond = 0xffffffff;
  st->st_curdts = AV_NOPTS_VALUE;
  st->st_curpts = AV_NOPTS_VALUE;
  st->st_prevdts = AV_NOPTS_VALUE;

  st->st_last_dts = AV_NOPTS_VALUE;
  st->st_dts_epoch = 0; 
 
  st->st_pcr_real_last = AV_NOPTS_VALUE;
  st->st_pcr_last      = AV_NOPTS_VALUE;
  st->st_pcr_drift     = 0;
  st->st_pcr_recovery_fails = 0;
  /* Open ffmpeg context and parser */

  switch(st->st_type) {
  case SCT_MPEG2VIDEO: id = CODEC_ID_MPEG2VIDEO; break;
  case SCT_MPEG2AUDIO: id = CODEC_ID_MP3;        break;
  case SCT_H264:       id = CODEC_ID_H264;       break;
  case SCT_AC3:        id = CODEC_ID_AC3;        break;
  default:             id = CODEC_ID_NONE;       break;
  }
    
  assert(st->st_ctx == NULL);
  assert(st->st_parser == NULL);


  if(id == CODEC_ID_NONE)
    return;
  c = avcodec_find_decoder(id);
  if(c != NULL) {
    st->st_ctx = avcodec_alloc_context();
    pthread_mutex_lock(&ffmpeg_lock);
    avcodec_open(st->st_ctx, c);
    pthread_mutex_unlock(&ffmpeg_lock);
    st->st_parser = av_parser_init(id);
  } else {
    abort();
  }
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

  if(st->st_parser != NULL)
    av_parser_close(st->st_parser);
    
  if(st->st_ctx != NULL) {
    pthread_mutex_lock(&ffmpeg_lock);
    avcodec_close(st->st_ctx);
    pthread_mutex_unlock(&ffmpeg_lock);
  }

  av_free(st->st_ctx);

  st->st_parser = NULL;
  st->st_ctx = NULL;

  free(st->st_priv);
  st->st_priv = NULL;

  /* Clear reassembly buffer */
    
  free(st->st_buffer);
  st->st_buffer = NULL;
  st->st_buffer_size = 0;
  st->st_buffer_ptr = 0;
  st->st_startcode = 0;

  free(st->st_buffer2);
  st->st_buffer2 = NULL;
  st->st_buffer2_size = 0;
  st->st_buffer2_ptr = 0;


  if(st->st_curpkt != NULL) {
    pkt_ref_dec(st->st_curpkt);
    st->st_curpkt = NULL;
  }

  /* Clear PTS queue */

  pktref_clear_queue(&st->st_ptsq);
  st->st_ptsq_len = 0;

  /* Clear durationq */

  pktref_clear_queue(&st->st_durationq);
}


/**
 *
 */
void
transport_stream_destroy(th_transport_t *t, th_stream_t *st)
{
  if(t->tht_status == TRANSPORT_RUNNING)
    stream_clean(st);
  LIST_REMOVE(st, st_link);
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
  LIST_FOREACH(st, &t->tht_components, st_link)
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
transport_remove_subscriber(th_transport_t *t, th_subscription_t *s)
{
  lock_assert(&global_lock);

  if(s == NULL) {
    while((s = LIST_FIRST(&t->tht_subscriptions)) != NULL) {
      subscription_unlink_transport(s);
    }
  } else {
    subscription_unlink_transport(s);
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

  lock_assert(&global_lock);

  assert(t->tht_status != TRANSPORT_RUNNING);

  t->tht_feed_status = TRANSPORT_FEED_UNKNOWN;
  t->tht_input_status = TRANSPORT_FEED_NO_INPUT;
  t->tht_dts_start = AV_NOPTS_VALUE;
  t->tht_pcr_drift = 0;

  if(t->tht_start_feed(t, weight, force_start))
    return -1;

  pthread_mutex_lock(&t->tht_stream_mutex);

  t->tht_status = TRANSPORT_RUNNING;

  /**
   * Initialize stream
   */
  LIST_FOREACH(st, &t->tht_components, st_link)
    stream_init(st);

  pthread_mutex_unlock(&t->tht_stream_mutex);

  cwc_transport_start(t);
  capmt_transport_start(t);

  gtimer_arm(&t->tht_receive_timer, transport_data_timeout, t, 10);
  return 0;
}


/**
 * Return prio for the given transport
 */
static int
transport_get_prio(th_transport_t *t)
{
  switch(t->tht_type) {
  case TRANSPORT_DVB:
    if(t->tht_scrambled)
      return 3;
    return 1;

  case TRANSPORT_IPTV:
    return 2;

  case TRANSPORT_V4L:
    return 4;

  default:
    return 5;
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
transport_find(channel_t *ch, unsigned int weight)
{
  th_transport_t *t, **vec;
  int cnt = 0, i;
  
  lock_assert(&global_lock);

  /* First, sort all transports in order */

  LIST_FOREACH(t, &ch->ch_transports, tht_ch_link)
      if(t->tht_enabled && t->tht_quality_index(t) > 10)
      cnt++;

  vec = alloca(cnt * sizeof(th_transport_t *));
  i = 0;
  LIST_FOREACH(t, &ch->ch_transports, tht_ch_link)
    if(t->tht_enabled && t->tht_quality_index(t) > 10)
      vec[i++] = t;

  assert(i == cnt);

  /* Sort transports, lower priority should come come earlier in the vector
     (i.e. it will be more favoured when selecting a transport */

  qsort(vec, cnt, sizeof(th_transport_t *), transportcmp);

  /* First, try all transports without stealing */

  for(i = 0; i < cnt; i++) {
    t = vec[i];
    if(t->tht_status == TRANSPORT_RUNNING) 
      return t;

    if(!transport_start(t, 0, 0))
      return t;
  }

  /* Ok, nothing, try again, but supply our weight and thus, try to steal
     transponders */

  for(i = 0; i < cnt; i++) {
    t = vec[i];
    if(!transport_start(t, weight, 0))
      return t;
  }
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
    subscription_unlink_transport(s);
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

  while((st = LIST_FIRST(&t->tht_components)) != NULL) {
    LIST_REMOVE(st, st_link);
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
  t->tht_identifier = strdup(identifier);
  t->tht_type = type;
  t->tht_source_type = source_type;
  t->tht_refcount = 1;
  t->tht_enabled = 1;
  t->tht_pcr_last = AV_NOPTS_VALUE;

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
  char buf[100];
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
  char buf[100];
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

  LIST_FOREACH(st, &t->tht_components, st_link)
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

  LIST_FOREACH(st, &t->tht_components, st_link) {
    if(st->st_index > idx)
      idx = st->st_index;
    i++;
    if(pid != -1 && st->st_pid == pid)
      return st;
  }

  st = calloc(1, sizeof(th_stream_t));
  st->st_index = idx + 1;
  st->st_type = type;

  LIST_INSERT_HEAD(&t->tht_components, st, st_link);

  st->st_pid = pid;
  st->st_demuxer_fd = -1;
  st->st_tb = (AVRational){1, 90000};

  TAILQ_INIT(&st->st_ptsq);
  TAILQ_INIT(&st->st_durationq);

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

  LIST_FOREACH(st, &t->tht_components, st_link) {
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
    t->tht_ch = NULL;
    LIST_REMOVE(t, tht_ch_link);
  }


  if(ch != NULL) {

    avgstat_init(&t->tht_cc_errors, 3600);
    avgstat_init(&t->tht_rate, 10);

    t->tht_ch = ch;
    LIST_INSERT_HEAD(&ch->ch_transports, t, tht_ch_link);
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

  if(t->tht_feed_status == TRANSPORT_FEED_UNKNOWN)
    transport_set_feed_status(t, t->tht_input_status);

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
transport_set_feed_status(th_transport_t *t, transport_feed_status_t newstatus)
{
  lock_assert(&t->tht_stream_mutex);

  if(t->tht_feed_status == newstatus)
    return;

  t->tht_feed_status = newstatus;

  streaming_message_t *sm = streaming_msg_create_code(SMT_TRANSPORT_STATUS,
						      newstatus);
  streaming_pad_deliver(&t->tht_streaming_pad, sm);
  streaming_msg_free(sm);
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
    sm = streaming_msg_create_code(SMT_STOP, 0);
    streaming_pad_deliver(&t->tht_streaming_pad, sm);
    streaming_msg_free(sm);
  }

  t->tht_refresh_feed(t);

  if(LIST_FIRST(&t->tht_components) != NULL) {

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
  
  LIST_FOREACH(st, &t->tht_components, st_link)
    n++;

  ss = calloc(1, sizeof(streaming_start_t) + 
	      sizeof(streaming_start_component_t) * n);

  ss->ss_num_components = n;
  
  n = 0;
  LIST_FOREACH(st, &t->tht_components, st_link) {
    streaming_start_component_t *ssc = &ss->ss_components[n++];
    ssc->ssc_index = st->st_index;
    ssc->ssc_type  = st->st_type;
    memcpy(ssc->ssc_lang, st->st_lang, 4);
    ssc->ssc_composition_id = st->st_composition_id;
    ssc->ssc_ancillary_id = st->st_ancillary_id;
  }

  t->tht_setsourceinfo(t, &ss->ss_si);

  ss->ss_refcount = 1;
  return ss;
}




/**
 * Table for status -> text conversion
 */
static struct strtab transportstatustab[] = {
  { "Unknown",
    TRANSPORT_FEED_UNKNOWN },

  { "No data input from adapter detected",  
    TRANSPORT_FEED_NO_INPUT},

  {"No mux packets for this service",
   TRANSPORT_FEED_NO_DEMUXED_INPUT},

  {"Data received for service, but no packets could be reassembled",
   TRANSPORT_FEED_RAW_INPUT},

  {"No descrambler available for service",
   TRANSPORT_FEED_NO_DESCRAMBLER},

  {"Access denied",
   TRANSPORT_FEED_NO_ACCESS},

  {"OK",
   TRANSPORT_FEED_VALID_PACKETS},
};


const char *
transport_feed_status_to_text(transport_feed_status_t status)
{
  return val2str(status, transportstatustab) ?: "Unknown";
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
}


static pthread_mutex_t pending_save_mutex;
static pthread_cond_t pending_save_cond;
static struct th_transport_queue pending_save_queue;

/**
 *
 */
void
transport_request_save(th_transport_t *t)
{
  pthread_mutex_lock(&pending_save_mutex);

  if(!t->tht_ps_onqueue) {
    t->tht_ps_onqueue = 1;
    TAILQ_INSERT_TAIL(&pending_save_queue, t, tht_ps_link);
    transport_ref(t);
    pthread_cond_signal(&pending_save_cond);
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

  pthread_mutex_lock(&pending_save_mutex);

  while(1) {

    if((t = TAILQ_FIRST(&pending_save_queue)) == NULL) {
      pthread_cond_wait(&pending_save_cond, &pending_save_mutex);
      continue;
    }

    TAILQ_REMOVE(&pending_save_queue, t, tht_ps_link);
    t->tht_ps_onqueue = 0;

    pthread_mutex_unlock(&pending_save_mutex);
    pthread_mutex_lock(&global_lock);

    if(t->tht_status != TRANSPORT_ZOMBIE)
      t->tht_config_save(t);

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
transport_source_info_copy(source_info_t *dst, source_info_t *src)
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
