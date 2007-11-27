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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include <libhts/htscfg.h>

#include <ffmpeg/avcodec.h>

#include "tvhead.h"
#include "dispatch.h"
#include "dvb_dvr.h"
#include "teletext.h"
#include "transports.h"
#include "subscriptions.h"
#include "tsdemux.h"

#include "v4l.h"
#include "dvb_dvr.h"
#include "iptv_input.h"
#include "psi.h"
#include "pes.h"
#include "buffer.h"

static dtimer_t transport_monitor_timer;



void
transport_purge(th_transport_t *t)
{
  th_stream_t *st;
  th_pkt_t *pkt;

  if(LIST_FIRST(&t->tht_subscriptions))
    return;

  t->tht_stop_feed(t);

  t->tht_tt_commercial_advice = COMMERCIAL_UNKNOWN;
 

  /*
   * Clean up each stream
   */

  LIST_FOREACH(st, &t->tht_streams, st_link) {

    if(st->st_parser != NULL)
      av_parser_close(st->st_parser);
    
    if(st->st_ctx != NULL)
      avcodec_close(st->st_ctx);

    /* Clear reassembly buffer */

    free(st->st_buffer);
    st->st_buffer = NULL;
    st->st_buffer_size = 0;
    st->st_buffer_ptr = 0;


    /* Clear DTS queue */

    while((pkt = TAILQ_FIRST(&st->st_dtsq)) != NULL) {
      TAILQ_REMOVE(&st->st_dtsq, pkt, pkt_queue_link);
      assert(pkt->pkt_refcount == 1);
      pkt_deref(pkt);
    }
    st->st_dtsq_len = 0;

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
static int
transport_start(th_transport_t *t, unsigned int weight)
{
  th_stream_t *st;
  AVCodec *c;
  enum CodecID id;

  t->tht_monitor_suspend = 10;
  t->tht_dts_start = AV_NOPTS_VALUE;

  LIST_FOREACH(st, &t->tht_streams, st_link) {
  
    st->st_dts      = AV_NOPTS_VALUE;
    st->st_last_dts = 0;
    st->st_dts_u    = 0; 
 
    /* Open ffmpeg context and parser */

    switch(st->st_type) {
    case HTSTV_MPEG2VIDEO: id = CODEC_ID_MPEG2VIDEO; break;
    case HTSTV_MPEG2AUDIO: id = CODEC_ID_MP3;        break;
    case HTSTV_H264:       id = CODEC_ID_H264;       break;
    case HTSTV_AC3:        id = CODEC_ID_AC3;        break;
    default:               id = CODEC_ID_NONE;       break;
    }
    
    st->st_ctx = NULL;
    st->st_parser = NULL;

    if(id != CODEC_ID_NONE) {
      c = avcodec_find_decoder(id);
      if(c != NULL) {
	st->st_ctx = avcodec_alloc_context();
	avcodec_open(st->st_ctx, c);
	st->st_parser = av_parser_init(id);
      }
    }
  }
  
  return t->tht_start_feed(t, weight, TRANSPORT_RUNNING);
}




th_transport_t *
transport_find(th_channel_t *ch, unsigned int weight)
{
  th_transport_t *t;

  /* First, try all transports without stealing */

  LIST_FOREACH(t, &ch->ch_transports, tht_channel_link) {
    if(t->tht_status == TRANSPORT_RUNNING) 
      return t;

    if(!transport_start(t, 0))
      return t;
  }

  /* Ok, nothing, try again, but supply our weight and thus, try to steal
     transponders */

  LIST_FOREACH(t, &ch->ch_transports, tht_channel_link) {
    if(!transport_start(t, weight))
      return t;
  }
  return NULL;
}





/*
 * 
 */

void
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

  LIST_FOREACH(t, head, tht_adapter_link) {
    LIST_FOREACH(s, &t->tht_subscriptions, ths_transport_link) {
      if(s->ths_weight > w)
	w = s->ths_weight;
    }
  }
  return w;
}





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


void
transport_monitor_init(th_transport_t *t)
{
  avgstat_init(&t->tht_cc_errors, 3600);
  avgstat_init(&t->tht_rate, 10);

  dtimer_arm(&transport_monitor_timer, transport_monitor, t, 5);
}


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

  TAILQ_INIT(&st->st_dtsq);
  TAILQ_INIT(&st->st_ptsq);
  TAILQ_INIT(&st->st_durationq);
  TAILQ_INIT(&st->st_pktq);

  avgstat_init(&st->st_rate, 10);
  avgstat_init(&st->st_cc_errors, 10);
  return st;
}
