/*
 *  tvheadend, MPEG Transport stream muxer
 *  Copyright (C) 2008 Andreas Öman
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
#define _GNU_SOURCE
#include <stdlib.h>

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
#include <string.h>

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "dispatch.h"
#include "teletext.h"
#include "transports.h"
#include "subscriptions.h"
#include "psi.h"
#include "buffer.h"
#include "mux.h"
#include "tsmux.h"

static void lookahead_dequeue(ts_muxer_t *ts, th_muxstream_t *tms);

static const AVRational mpeg_tc = {1, 90000};
static const AVRational mpeg_tc_27M = {1, 27000000};

#define PID_PMT     1000
#define PID_ES_BASE 2000

/**
 * Send current packet
 */
static void
ts_muxer_send_packet(ts_muxer_t *ts)
{
  int i;
  int64_t t, tlow, pcr;
  uint8_t *d;

  if(ts->ts_block == 0)
    return;

  d = ts->ts_packet;

  /* Update PCR */

  if(ts->ts_pcr_ref != AV_NOPTS_VALUE) {
    for(i = 0; i < ts->ts_block; i++) {
      d = ts->ts_packet + i * 188;
      if((d[3] & 0xf0) == 0x30 && d[4] >= 7 && d[5] & 0x10) {

	pcr = getclock_hires() - ts->ts_pcr_ref;
	t = av_rescale_q(pcr, AV_TIME_BASE_Q, mpeg_tc_27M);
	tlow = t % 300LL;
	t =    t / 300LL;

	d[6] =  t >> 25;
	d[7] =  t >> 17;
	d[8] =  t >> 9;
	d[9] =  t >> 1;
	d[10] = ((t & 1) << 7) | ((tlow >> 8) & 1);
	d[11] = tlow;
      }
    }
  }
  ts->ts_output(ts->ts_output_opaque, ts->ts_subscription, ts->ts_packet,
		ts->ts_block, 0);
  ts->ts_block = 0;
}

/**
 * Push a MPEG TS packet to output
 */
static void
ts_muxer_add_packet(ts_muxer_t *ts, void *data, uint16_t pid)
{
  uint8_t *tsb;

  tsb = ts->ts_packet + ts->ts_block * 188;
  ts->ts_block++;

  memcpy(tsb, data, 188);
  
  tsb[2] =                    pid;
  tsb[1] = (tsb[1] & 0xf0) | (pid >> 8);
  
  if(ts->ts_block == ts->ts_blocks_per_packet)
    ts_muxer_send_packet(ts);
}

/**
 * Raw TS input
 */
static void
ts_muxer_raw_input(struct th_subscription *s, void *data, int len,
		   th_stream_t *st, void *opaque)
{
  th_muxer_t *tm = s->ths_muxer;
  ts_muxer_t *ts = opaque;
  th_muxstream_t *tms;

  LIST_FOREACH(tms, &tm->tm_streams, tms_muxer_link0)
    if(tms->tms_stream == st)
      break;

  if(tms == NULL || tms->tms_index == 0)
    return; /* Unknown / non-mapped stream */
  ts_muxer_add_packet(ts, data, tms->tms_index);
}



/**
 * Function for encapsulating a short table into a transport stream packet
 */
static void
ts_muxer_build_table(ts_muxer_t *ts, void *table, int tlen, int cc, int pid)
{
  int pad;
  uint8_t tsb0[188], *tsb;
  tsb = tsb0;

  pad = 184 - (tlen + 1);
  
  *tsb++ = 0x47;
  *tsb++ = 0x40;
  *tsb++ = 0;
  *tsb++ = (cc & 0xf) | 0x30;
  *tsb++ = --pad;
  memset(tsb, 0, pad);
  tsb += pad;
  
  *tsb++ = 0; /* Pointer field for tables */
  memcpy(tsb, table, tlen);
  ts_muxer_add_packet(ts, tsb0, pid);
}



/**
 *
 */
static void
ts_muxer_generate_tables(void *aux, int64_t now)
{
  ts_muxer_t *ts = aux;
  th_muxer_t *tm = ts->ts_muxer;
  uint8_t table[180];
  int l;

  /* rearm timer */
  dtimer_arm_hires(&ts->ts_patpmt_timer, ts_muxer_generate_tables, 
		   ts, now + 100000);

  l = psi_build_pat(NULL, table, sizeof(table), PID_PMT);
  ts_muxer_build_table(ts, table, l, ts->ts_pat_cc, 0);
  ts->ts_pat_cc++;

  l = psi_build_pmt(tm, table, sizeof(table), ts->ts_pcr_stream->tms_index);
  ts_muxer_build_table(ts, table, l, ts->ts_pmt_cc, PID_PMT);
  ts->ts_pmt_cc++;

  ts_muxer_send_packet(ts);
}




/**
 *
 */
static void
tmf_enq(th_muxfifo_t *tmf, th_muxpkt_t *tm)
{
  /* record real content size */
  tmf->tmf_contentsize += tm->tm_contentsize;

  /* Enqueue packet */
  TAILQ_INSERT_TAIL(&tmf->tmf_queue, tm, tm_link);
  tmf->tmf_len++;
}

/**
 *
 */
static void
tmf_remove(th_muxfifo_t *tmf, th_muxpkt_t *tm)
{
  tmf->tmf_contentsize -= tm->tm_contentsize;
  TAILQ_REMOVE(&tmf->tmf_queue, tm, tm_link);
  tmf->tmf_len--;
}


/**
 *
 */
static th_muxpkt_t *
tmf_deq(th_muxfifo_t *tmf)
{
  th_muxpkt_t *tm;

  tm = TAILQ_FIRST(&tmf->tmf_queue);
  if(tm != NULL)
    tmf_remove(tmf, tm);
  return tm;
}




/**
 *
 */
static void
tmf_init(th_muxfifo_t *tmf)
{
  TAILQ_INIT(&tmf->tmf_queue);
}


/**
 *
 */
static void
ts_deliver(void *opaque, int64_t now)
{
  th_muxstream_t *tms = opaque;
  th_muxer_t *tm = tms->tms_muxer;
  ts_muxer_t *ts = tm->tm_opaque;
  th_muxpkt_t *f;
  th_muxfifo_t *tmf = &tms->tms_delivery_fifo;
  int64_t pcr = now - ts->ts_pcr_ref;
  int64_t dl, next, delta;

  f = tmf_deq(tmf);
  dl = f->tm_deadline;

  delta = pcr - dl;

  ts_muxer_add_packet(ts, f->tm_pkt, tms->tms_index);
  free(f);

  f = TAILQ_FIRST(&tmf->tmf_queue);  /* next packet we are going to send */
  if(f == NULL) {
    lookahead_dequeue(ts, tms);
    f = TAILQ_FIRST(&tmf->tmf_queue);
    if(f == NULL)
      return;
  }

  next = f->tm_deadline + ts->ts_pcr_ref;
  if(next < now + 100)
    next = now + 100;

  dtimer_arm_hires(&tms->tms_mux_timer, ts_deliver, tms, next - 500);
}



/**
 * Check if we need to start delivery timer for the given stream
 * 
 * Also, if it is the PCR stream and we're not yet runnig, figure out
 * PCR and start generating packets
 */
static void
ts_check_deliver(ts_muxer_t *ts, th_muxstream_t *tms)
{
  int64_t now;
  th_muxpkt_t *f;
  th_muxfifo_t *tmf = &tms->tms_delivery_fifo;
  int64_t next;

  if(dtimer_isarmed(&tms->tms_mux_timer))
    return; /* timer already running, we're fine */

  assert(tms->tms_delivery_fifo.tmf_len != 0);

  now = getclock_hires();

  if(ts->ts_pcr_ref == AV_NOPTS_VALUE) {

    if(ts->ts_pcr_start == AV_NOPTS_VALUE)
      return; /* dont know anything yet */

    ts->ts_pcr_ref = now - ts->ts_pcr_start;
  }

  f = TAILQ_FIRST(&tmf->tmf_queue);  /* next packet we are going to send */
  next = f->tm_deadline + ts->ts_pcr_ref;

  if(next < now + 100)
    next = now + 100;
 
  dtimer_arm_hires(&tms->tms_mux_timer, ts_deliver, tms, next - 500);
}




/**
 * Generate TS packet, see comments inline
 *
 * TODO: Dont insert both DTS and PTS if they're equal
 */

static void
lookahead_dequeue(ts_muxer_t *ts, th_muxstream_t *tms)
{
  //  th_stream_t *st = tms->tms_stream;
  th_pkt_t *pkt;
  th_muxpkt_t *tm;
  th_refpkt_t *o;
  uint8_t *tsb;
  uint16_t u16;
  int frrem, pad, tsrem, len, off, cc, hlen, flags;
  int64_t t, tdur, toff, tlen, dts, pcr, basedelivery;

  tlen = 0;
  TAILQ_FOREACH(o, &tms->tms_lookahead, trp_link) {
    pkt = o->trp_pkt;
    tlen += pkt->pkt_payloadlen;

    if(pkt->pkt_pts != pkt->pkt_dts) {
      tlen += 19; /* pes header with DTS and PTS */
    } else {
      tlen += 14; /* pes header with PTS only */
    }
  }

  if(tlen == 0)
    return;

  o = TAILQ_FIRST(&tms->tms_lookahead);
  pkt = o->trp_pkt;
  toff = 0;

  /* XXX: assumes duration is linear, but it's probably ok */
  tdur = pkt->pkt_duration * tms->tms_lookahead_packets;
  
  if(tms->tms_mux_offset == AV_NOPTS_VALUE) {
    if(tms->tms_stream->st_vbv_delay == -1)
      tms->tms_mux_offset = tdur - pkt->pkt_duration;
    else
      tms->tms_mux_offset = tms->tms_stream->st_vbv_delay;
  }

  if(ts->ts_pcr_start == AV_NOPTS_VALUE && ts->ts_pcr_stream == tms)
    ts->ts_pcr_start = pkt->pkt_dts - tms->tms_mux_offset;

  basedelivery = pkt->pkt_dts - tms->tms_mux_offset;

  while((o = TAILQ_FIRST(&tms->tms_lookahead)) != NULL) {

    off = 0;
    pkt = o->trp_pkt;

    pkt_load(pkt);

    if(pkt->pkt_dts == pkt->pkt_pts) {
      hlen = 8;
      flags = 0x80;
    } else {
      hlen = 13;
      flags = 0xc0;
    }

    while(off < pkt->pkt_payloadlen) {
  
      tm = malloc(sizeof(th_muxpkt_t) + 188);
      tm->tm_deadline = basedelivery + tdur * toff / tlen;

      dts = (int64_t)pkt->pkt_duration * 
	(int64_t)off / (int64_t)pkt->pkt_payloadlen;
      dts += pkt->pkt_dts;

      tm->tm_dts = dts;
      tsb = tm->tm_pkt;
      
      if(ts->ts_flags & TS_HTSCLIENT) {
	/* Temporary hack */
	*tsb++ = tms->tms_stream->st_type;
      } else {
	/* TS marker */
	*tsb++ = 0x47;
      }
      
    
      /* Write PID and optionally payload unit start indicator */
      *tsb++ = tms->tms_index >> 8 | (off ? 0 : 0x40);
      *tsb++ = tms->tms_index;

      cc = tms->tms_cc & 0xf;
      tms->tms_cc++;

      /* Remaing bytes after 4 bytes of TS header */
      tsrem = 184;

      if(off == 0) {
	/* When writing the packet header, shave of a bit of available 
	   payload size */
	tsrem -= hlen + 6;
      }
      
      /* Remaining length of frame */
      frrem = pkt->pkt_payloadlen - off;

      /* Compute amout of padding needed */
      pad = tsrem - frrem;

      pcr = tm->tm_deadline;
      tm->tm_pcr = AV_NOPTS_VALUE;

      if(ts->ts_pcr_stream == tms && ts->ts_pcr_last + 20000 < pcr) {
	 
	tm->tm_pcr = pcr;
	/* Insert PCR */
    
	tlen  += 8; /* compensate total length */
	tsrem -= 8;
	pad   -= 8;
	if(pad < 0)
	  pad = 0;
	
	*tsb++ = 0x30 | cc;
	*tsb++ = 7 + pad;
	*tsb++ = 0x10;   /* PCR flag */
	
	t = av_rescale_q(pcr, AV_TIME_BASE_Q, mpeg_tc);
	*tsb++ =  t >> 25;
	*tsb++ =  t >> 17;
	*tsb++ =  t >> 9;
	*tsb++ =  t >> 1;
	*tsb++ = (t & 1) << 7;
	*tsb++ = 0;
	
	memset(tsb, 0xff, pad);
	tsb   += pad;
	tsrem -= pad;
	
	ts->ts_pcr_last = pcr + 20000;

      } else if(pad > 0) {
	/* Must pad TS packet */
	
	*tsb++ = 0x30 | cc;
	tsrem -= pad;
	*tsb++ = --pad;

	memset(tsb, 0x00, pad);
	tsb += pad;
      } else {
	*tsb++ = 0x10 | cc;
      }


      if(off == 0) {
	/* Insert PES header */
      
	/* Write startcode */
	*tsb++ = 0;
	*tsb++ = 0;
	*tsb++ = tms->tms_sc >> 8;
	*tsb++ = tms->tms_sc;

	/* Write total frame length (without accounting for startcode and
	   length field itself */
	len = pkt_len(pkt) + hlen;

	if(tms->tms_stream->st_type == HTSTV_MPEG2VIDEO) {
	  /* It's okay to write len as 0 in transport streams,
	     but only for video frames, and i dont expect any of the
	     audio frames to exceed 64k 
	  */
	  len = 0;
	}

	*tsb++ = len >> 8;
	*tsb++ = len;

	*tsb++ = 0x80;      /* MPEG2 */
	*tsb++ = flags;
	*tsb++ = hlen - 3;  /* length of rest of header (pts & dts) */

	/* Write PTS */
	
	if(flags == 0xc0) {
	  t = av_rescale_q(pkt->pkt_pts, AV_TIME_BASE_Q, mpeg_tc);
	  *tsb++ = (((t >> 30) & 7) << 1) | 1;
	  u16 = (((t >> 15) & 0x7fff) << 1) | 1;
	  *tsb++ = u16 >> 8;
	  *tsb++ = u16;
	  u16 = ((t & 0x7fff) << 1) | 1;
	  *tsb++ = u16 >> 8;
	  *tsb++ = u16;
	}

	/* Write DTS */

	t = av_rescale_q(pkt->pkt_dts, AV_TIME_BASE_Q, mpeg_tc);
	*tsb++ = (((t >> 30) & 7) << 1) | 1;
	u16 = (((t >> 15) & 0x7fff) << 1) | 1;
	*tsb++ = u16 >> 8;
	*tsb++ = u16;
	u16 = ((t & 0x7fff) << 1) | 1;
	*tsb++ = u16 >> 8;
	*tsb++ = u16;
      }

      memcpy(tsb, pkt->pkt_payload + off, tsrem);

      if(tms->tms_corruption_interval != 0) {
	/* Only corruption payload, never the header */
	if(tms->tms_corruption_last + tms->tms_corruption_interval < pcr &&
	   off != 0 && pkt->pkt_frametype == PKT_I_FRAME) {
	  tms->tms_corruption_counter = 50;
	  tms->tms_corruption_last = pcr;
	}

	if(tms->tms_corruption_counter) {
	  tms->tms_cc++;
	  tms->tms_corruption_counter--;
	}
      }
    
      tm->tm_contentsize = tsrem;
      
      tmf_enq(&tms->tms_delivery_fifo, tm);

      toff += tsrem;
      off += tsrem;

     }

    tms->tms_lookahead_depth -= pkt->pkt_payloadlen;
    tms->tms_lookahead_packets--;
    pkt_deref(pkt);
    TAILQ_REMOVE(&tms->tms_lookahead, o, trp_link);

    free(o);
  }
  ts_check_deliver(ts, tms);
}



/**
 *
 */
static void
ts_mux_packet_input(void *opaque, th_muxstream_t *tms, th_pkt_t *pkt)
{
  ts_muxer_t *ts = opaque;
  th_stream_t *st = tms->tms_stream;
  th_refpkt_t *trp;

  if(tms->tms_index == 0)
    return;

  if(st->st_vbv_delay == -1) {
    if(tms->tms_lookahead_depth + pkt->pkt_payloadlen > st->st_vbv_size) 
      lookahead_dequeue(ts, tms);
  } else {
    lookahead_dequeue(ts, tms);
  }

  trp = malloc(sizeof(th_refpkt_t));
  trp->trp_pkt = pkt_ref(pkt);
  tms->tms_lookahead_depth += pkt->pkt_payloadlen;
  tms->tms_lookahead_packets++;
  TAILQ_INSERT_TAIL(&tms->tms_lookahead, trp, trp_link);

}

/**
 *
 */
ts_muxer_t *
ts_muxer_init(th_subscription_t *s, ts_mux_output_t *output,
	      void *opaque, int flags, int corruption)
{
  ts_muxer_t *ts = calloc(1, sizeof(ts_muxer_t));
  int dopcr;
  int pididx = PID_ES_BASE;
  th_muxstream_t *tms;
  th_muxer_t *tm;
  th_stream_t *st;


  ts->ts_output = output;
  ts->ts_output_opaque = opaque;
  ts->ts_flags = flags;

  ts->ts_subscription = s;
  tm = ts->ts_muxer = muxer_init(s, ts_mux_packet_input, ts);
 
  ts->ts_blocks_per_packet = 7;
  ts->ts_packet = malloc(188 * ts->ts_blocks_per_packet);
  
  ts->ts_pcr_start = AV_NOPTS_VALUE;
  ts->ts_pcr_ref   = AV_NOPTS_VALUE;
  ts->ts_pcr_last  = INT64_MIN;
  

  /* Do TS MUX specific init per stream */

  LIST_FOREACH(tms, &tm->tm_streams, tms_muxer_link0) {
    st = tms->tms_stream;

    dopcr = 0;
    switch(st->st_type) {
    case HTSTV_MPEG2VIDEO:
      tms->tms_corruption_interval = (int64_t)corruption * 1000000LL;
      tms->tms_sc = 0x1e0;
      dopcr = 1;
      break;
    case HTSTV_MPEG2AUDIO:
      tms->tms_sc = 0x1c0;
      st->st_vbv_delay = 45000;
      break;
#if 0
    case HTSTV_AC3:
      tms->tms_sc = 0x1bd;
      st->st_vbv_delay = 50000;
      break;
    case HTSTV_H264:
      tms->tms_muxoffset = 900000;
      tms->tms_sc = 0x1e0;
      dopcr = 1;
      break;
#endif
    default:
      continue;
    }

    tms->tms_mux_offset = AV_NOPTS_VALUE;
    tmf_init(&tms->tms_delivery_fifo);
    TAILQ_INIT(&tms->tms_lookahead);

    if(dopcr && ts->ts_pcr_stream == NULL)
      ts->ts_pcr_stream = tms;

    tms->tms_index = pididx++;
  }
  return ts;
}


/**
 *
 */
void 
ts_muxer_deinit(ts_muxer_t *ts, th_subscription_t *s)
{
  free(ts->ts_packet);
  muxer_deinit(ts->ts_muxer, s);
  dtimer_disarm(&ts->ts_patpmt_timer);
  free(ts);
}


/**
 *
 */
void
ts_muxer_play(ts_muxer_t *ts, int64_t toffset)
{
  th_subscription_t *s = ts->ts_muxer->tm_subscription;

  /* Start PAT / PMT generator */
  ts_muxer_generate_tables(ts, getclock_hires());

  if(!(ts->ts_flags & TS_SEEK) &&
     s->ths_transport->tht_source_type == THT_MPEG_TS) {
    /* We dont need to seek and source is MPEG TS, we can use a 
       shortcut to avoid remuxing stream */

    s->ths_raw_input = ts_muxer_raw_input;
    s->ths_opaque = ts;
  } else {
    muxer_play(ts->ts_muxer, toffset);
  }
}


/**
 *
 */
void
ts_muxer_pause(ts_muxer_t *ts)
{
  dtimer_disarm(&ts->ts_patpmt_timer);
  muxer_pause(ts->ts_muxer);
}
