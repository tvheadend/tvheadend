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

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "dispatch.h"
#include "dvb_dvr.h"
#include "dvb_support.h"
#include "teletext.h"
#include "transports.h"
#include "subscriptions.h"
#include "tsdemux.h"

#include "v4l.h"
#include "dvb_dvr.h"
#include "iptv_input.h"
#include "psi.h"
#include "buffer.h"
#include "channels.h"
#include "cwc.h"
#include "notify.h"

#define TRANSPORT_HASH_WIDTH 101

static struct th_transport_list transporthash[TRANSPORT_HASH_WIDTH];

static void transport_data_timeout(void *aux, int64_t now);

//static dtimer_t transport_monitor_timer;

//static const char *transport_settings_path(th_transport_t *t);
//static void transport_monitor(void *aux, int64_t now);

void
transport_stop(th_transport_t *t, int flush_subscriptions)
{
  th_subscription_t *s;
  th_descrambler_t *td;
  th_stream_t *st;
  th_pkt_t *pkt;

  if(flush_subscriptions) {
    while((s = LIST_FIRST(&t->tht_subscriptions)) != NULL)
      subscription_stop(s);
  } else {
    if(LIST_FIRST(&t->tht_subscriptions))
      return;
  }

  dtimer_disarm(&t->tht_receive_timer);

  //  dtimer_disarm(&transport_monitor_timer, transport_monitor, t, 1);

  t->tht_stop_feed(t);

  while((td = LIST_FIRST(&t->tht_descramblers)) != NULL)
      td->td_stop(td);

  t->tht_tt_commercial_advice = COMMERCIAL_UNKNOWN;
 

  /*
   * Clean up each stream
   */

  LIST_FOREACH(st, &t->tht_streams, st_link) {

    if(st->st_parser != NULL)
      av_parser_close(st->st_parser);
    
    if(st->st_ctx != NULL)
      avcodec_close(st->st_ctx);

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

    if(st->st_curpkt != NULL) {
      pkt_deref(st->st_curpkt);
      st->st_curpkt = NULL;
    }

    /* Clear PTS queue */

    while((pkt = TAILQ_FIRST(&st->st_ptsq)) != NULL) {
      TAILQ_REMOVE(&st->st_ptsq, pkt, pkt_queue_link);
      assert(pkt->pkt_refcount == 1);
      pkt_deref(pkt);
    }
    st->st_ptsq_len = 0;

    /* Clear durationq */

    while((pkt = TAILQ_FIRST(&st->st_durationq)) != NULL) {
      TAILQ_REMOVE(&st->st_durationq, pkt, pkt_queue_link);
      assert(pkt->pkt_refcount == 1);
      pkt_deref(pkt);
    }

    /* Flush framestore */

    while((pkt = TAILQ_FIRST(&st->st_pktq)) != NULL)
      pkt_unstore(pkt);

  }
}


/*
 *
 */
int
transport_start(th_transport_t *t, unsigned int weight)
{
  th_stream_t *st;
  AVCodec *c;
  enum CodecID id;

  assert(t->tht_runstatus != TRANSPORT_RUNNING);

  if(t->tht_start_feed(t, weight, TRANSPORT_RUNNING))
    return -1;

  t->tht_monitor_suspend = 10;
  t->tht_dts_start = AV_NOPTS_VALUE;
  t->tht_pcr_drift = 0;
  LIST_FOREACH(st, &t->tht_streams, st_link) {
  
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
    case HTSTV_MPEG2VIDEO: id = CODEC_ID_MPEG2VIDEO; break;
    case HTSTV_MPEG2AUDIO: id = CODEC_ID_MP3;        break;
    case HTSTV_H264:       id = CODEC_ID_H264;       break;
    case HTSTV_AC3:        id = CODEC_ID_AC3;        break;
    default:               id = CODEC_ID_NONE;       break;
    }
    
    assert(st->st_ctx == NULL);
    assert(st->st_parser == NULL);

    if(id != CODEC_ID_NONE) {
      c = avcodec_find_decoder(id);
      if(c != NULL) {
	st->st_ctx = avcodec_alloc_context();
	avcodec_open(st->st_ctx, c);
	st->st_parser = av_parser_init(id);
      }
    }
  }

  cwc_transport_start(t);
  dtimer_arm(&t->tht_receive_timer, transport_data_timeout, t, 4);
  transport_signal_status(t, TRANSPORT_STATUS_STARTING);
  return 0;
}


/**
 * Return prio for the given transport
 */
static int
transport_get_prio(th_transport_t *t)
{
  switch(t->tht_type) {
  case TRANSPORT_AVGEN:
  case TRANSPORT_STREAMEDFILE:
    return 0;
    
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
  
  /* First, sort all transports in order */

  LIST_FOREACH(t, &ch->ch_transports, tht_ch_link)
    if(!t->tht_disabled)
      cnt++;

  vec = alloca(cnt * sizeof(th_transport_t *));
  i = 0;
  LIST_FOREACH(t, &ch->ch_transports, tht_ch_link)
    if(!t->tht_disabled)
      vec[i++] = t;

  /* Sort transports, lower priority should come come earlier in the vector
     (i.e. it will be more favoured when selecting a transport */

  qsort(vec, cnt, sizeof(th_transport_t *), transportcmp);

  /* First, try all transports without stealing */

  for(i = 0; i < cnt; i++) {
    t = vec[i];
    if(t->tht_runstatus == TRANSPORT_RUNNING) 
      return t;

    if(!transport_start(t, 0))
      return t;
  }

  /* Ok, nothing, try again, but supply our weight and thus, try to steal
     transponders */

  for(i = 0; i < cnt; i++) {
    t = vec[i];
    if(!transport_start(t, weight))
      return t;
  }
  return NULL;
}





/*
 * 
 */

static void
transport_flush_subscribers(th_transport_t *t)
{
  th_subscription_t *s;
  
  while((s = LIST_FIRST(&t->tht_subscriptions)) != NULL)
    subscription_stop(s);
}

unsigned int 
transport_compute_weight(struct th_transport_list *head)
{
  th_transport_t *t;
  th_subscription_t *s;
  int w = 0;

  LIST_FOREACH(t, head, tht_active_link) {
    LIST_FOREACH(s, &t->tht_subscriptions, ths_transport_link) {
      if(s->ths_weight > w)
	w = s->ths_weight;
    }
  }
  return w;
}


#if 0


static void
transport_monitor(void *aux, int64_t now)
{
  th_transport_t *t = aux;
  int v;

  dtimer_arm(&transport_monitor_timer, transport_monitor, t, 1);

  if(t->tht_status == TRANSPORT_IDLE)
    return;

  if(t->tht_monitor_suspend > 0) {
    t->tht_monitor_suspend--;
    return;
  }

  v = avgstat_read_and_expire(&t->tht_rate, dispatch_clock) * 8 / 1000 / 10;

  if(v < 500) {
    switch(t->tht_rate_error_log_limiter) {
    case 0:
      syslog(LOG_WARNING, "\"%s\" on \"%s\", very low bitrate: %d kb/s",
	     t->tht_channel->ch_name, t->tht_name, v);
      /* FALLTHRU */
    case 1 ... 9:
      t->tht_rate_error_log_limiter++;
      break;
    }
  } else {
    switch(t->tht_rate_error_log_limiter) {
    case 0:
      break;

    case 1:
      syslog(LOG_NOTICE, "\"%s\" on \"%s\", bitrate ok (%d kb/s)",
	     t->tht_channel->ch_name, t->tht_name, v);
      /* FALLTHRU */
    default:
      t->tht_rate_error_log_limiter--;
    }
  }


  v = avgstat_read(&t->tht_cc_errors, 10, dispatch_clock);
  if(v > t->tht_cc_error_log_limiter) {
    t->tht_cc_error_log_limiter += v * 3;

    syslog(LOG_WARNING, "\"%s\" on \"%s\", "
	   "%.2f continuity errors/s (10 sec average)",
	   t->tht_channel->ch_name, t->tht_name, (float)v / 10.);

  } else if(v == 0) {
    switch(t->tht_cc_error_log_limiter) {
    case 0:
      break;
    case 1:
      syslog(LOG_NOTICE, "\"%s\" on \"%s\", no continuity errors",
	     t->tht_channel->ch_name, t->tht_name);
      /* FALLTHRU */
    default:
      t->tht_cc_error_log_limiter--;
    }
  }
}

#endif


/**
 * Timer that fires if transport is not receiving any data
 */
static void
transport_data_timeout(void *aux, int64_t now)
{
  th_transport_t *t = aux;
  transport_signal_status(t, TRANSPORT_STATUS_NO_INPUT);
}


/**
 * Destroy a transport
 */
void
transport_destroy(th_transport_t *t)
{
  th_stream_t *st;

  dtimer_disarm(&t->tht_receive_timer);

  free((void *)t->tht_name);

  if(t->tht_ch != NULL) {
    t->tht_ch = NULL;
    LIST_REMOVE(t, tht_ch_link);
  }

  LIST_REMOVE(t, tht_mux_link);
  LIST_REMOVE(t, tht_hash_link);

  transport_flush_subscribers(t);
  
  free(t->tht_identifier);
  free(t->tht_svcname);
  free(t->tht_chname);
  free(t->tht_provider);

  while((st = LIST_FIRST(&t->tht_streams)) != NULL) {
    LIST_REMOVE(st, st_link);
    free(st);
  }
  free(t);
}


/**
 * Create and initialize a new transport struct
 */
th_transport_t *
transport_create(const char *identifier, int type, int source_type)
{
  unsigned int hash = tvh_strhash(identifier, TRANSPORT_HASH_WIDTH);
  th_transport_t *t = calloc(1, sizeof(th_transport_t));
  t->tht_identifier = strdup(identifier);
  t->tht_type = type;
  t->tht_source_type = source_type;

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
  LIST_FOREACH(t, &transporthash[hash], tht_hash_link)
    if(!strcmp(t->tht_identifier, identifier))
      break;
  return t;
}



/**
 * Add a new stream to a transport
 */
th_stream_t *
transport_add_stream(th_transport_t *t, int pid, tv_streamtype_t type)
{
  th_stream_t *st;
  int i = 0;

  LIST_FOREACH(st, &t->tht_streams, st_link) {
    i++;
    if(pid != -1 && st->st_pid == pid)
      return st;
  }

  st = calloc(1, sizeof(th_stream_t));
  st->st_index = i;
  st->st_pid = pid;
  st->st_type = type;
  st->st_demuxer_fd = -1;
  LIST_INSERT_HEAD(&t->tht_streams, st, st_link);

  TAILQ_INIT(&st->st_ptsq);
  TAILQ_INIT(&st->st_durationq);
  TAILQ_INIT(&st->st_pktq);

  avgstat_init(&st->st_rate, 10);
  avgstat_init(&st->st_cc_errors, 10);
  return st;
}


/**
 *
 */
void
transport_map_channel(th_transport_t *t, channel_t *ch)
{
  assert(t->tht_ch == NULL);

  if(ch == NULL) {
    if(t->tht_chname == NULL)
      return;
    ch = channel_find(t->tht_chname, 1, NULL);
  } else {
    free(t->tht_chname);
    t->tht_chname = strdup(ch->ch_name);
  }

  avgstat_init(&t->tht_cc_errors, 3600);
  avgstat_init(&t->tht_rate, 10);

  assert(t->tht_identifier != NULL);
  t->tht_ch = ch;

  LIST_INSERT_HEAD(&ch->ch_transports, t, tht_ch_link);
}

/**
 *
 */
void
transport_unmap_channel(th_transport_t *t)
{
  t->tht_ch = NULL;
  LIST_REMOVE(t, tht_ch_link);
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
  return val2str(t->tht_servicetype, stypetab);
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
int
transport_is_available(th_transport_t *t)
{
  return transport_servicetype_txt(t) && LIST_FIRST(&t->tht_streams);
}

/**
 *
 */
void
transport_signal_status(th_transport_t *t, int newstatus)
{
  char buf[200];

  if(t->tht_last_status == newstatus)
    return;

  snprintf(buf, sizeof(buf), "\"%s\" on %s",
	   t->tht_chname ?: t->tht_svcname, t->tht_sourcename(t));

  syslog(LOG_INFO, "%s -- Changed status from \"%s\" to \"%s\"",
	 buf, transport_status_to_text(t->tht_last_status),
	 transport_status_to_text(newstatus));

  t->tht_last_status = newstatus;
  notify_transprot_status_change(t);
}


/**
 * Table for status -> text conversion
 */
static struct strtab transportstatustab[] = {
  { "Unknown",        TRANSPORT_STATUS_UNKNOWN },
  { "Starting",       TRANSPORT_STATUS_STARTING },
  { "Ok",             TRANSPORT_STATUS_OK },
  { "No input",       TRANSPORT_STATUS_NO_INPUT },
  { "No descrambler", TRANSPORT_STATUS_NO_DESCRAMBLER },
  { "No access",      TRANSPORT_STATUS_NO_ACCESS },
  { "Mux error",      TRANSPORT_STATUS_MUX_ERROR },
};


const char *
transport_status_to_text(int status)
{
  return val2str(status, transportstatustab) ?: "Invalid";
}
