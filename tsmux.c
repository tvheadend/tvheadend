/*
 *  tvheadend, MPEG Transport stream muxer
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
#include "pes.h"
#include "psi.h"
#include "buffer.h"
#include "mux.h"
#include "tsmux.h"

#define TS_LOOKAHEAD 500000

static const AVRational mpeg_tc = {1, 90000};

#define PID_PMT     1000
#define PID_ES_BASE 2000

/**
 * Send current packet
 */
static void
ts_muxer_send_packet(ts_muxer_t *ts)
{
  if(ts->ts_block == 0)
    return;

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

  l = psi_build_pmt(tm, table, sizeof(table), ts->ts_pcrpid);
  ts_muxer_build_table(ts, table, l, ts->ts_pmt_cc, PID_PMT);
  ts->ts_pmt_cc++;

  ts_muxer_send_packet(ts);
}

/**
 *
 */
static int64_t
get_delay(th_muxstream_t *tms)
{
  th_metapkt_t *f, *l;

  f = TAILQ_FIRST(&tms->tms_metaqueue);
  if(f == NULL)
    return -1;

  l = TAILQ_LAST(&tms->tms_metaqueue, th_metapkt_queue);

  return l->tm_ts_stop - f->tm_ts_start; /* Delta time */

}


/**
 *
 */
#if 0
static int
estimate_bitrate(th_muxstream_t *tms)
{
  int64_t delta, rate;

  delta = get_delay(tms);
  if(delta == -1)
    return -1;
  rate = (uint64_t)tms->tms_meta_bytes * 1000000LL / delta;
  return rate;
}
#endif

/**
 *
 */
#if 0
static int
estimate_blockrate(th_muxstream_t *tms)
{
  int64_t delta, rate;

  delta = get_delay(tms);
  if(delta == -1)
    return 0;
  rate = (uint64_t)tms->tms_meta_packets * 1000000LL / delta;
  return rate;
}
#endif

/**
 *
 */
#define PES_HEADER_SIZE 19

static void
ts_mux_gen_packets(ts_muxer_t *ts, th_muxstream_t *tms, th_pkt_t *pkt)
{
  th_metapkt_t *tm;
  int off = 0;
  uint8_t *tsb;
  int64_t t;
  int frrem, pad, tsrem, len;
  uint16_t u16;
  //  int pcroffset;
  int printts = 0;

  if(printts)printf("Generating TS packets, DTS = %lld +%d\n", pkt->pkt_dts,
	 pkt->pkt_duration);

  while(off < pkt->pkt_payloadlen) {


    tm = malloc(sizeof(th_metapkt_t) + 188);
    tsb = tm->tm_pkt;
    tm->tm_pcroffset = 0;

    /* Timestamp of first byte */
    tm->tm_ts_start = pkt->pkt_duration * off / 
      (pkt->pkt_payloadlen + PES_HEADER_SIZE)
      + pkt->pkt_dts - tms->tms_muxoffset;


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

    /* Remaing bytes after 4 bytes of TS header */
    tsrem = 184;

    if(off == 0) {
      /* When writing the packet header, shave of a bit of available 
	 payload size */
      tsrem -= PES_HEADER_SIZE;
    }
      
    /* Remaining length of frame */
    frrem = pkt->pkt_payloadlen - off;

    /* Compute amout of padding needed */
    pad = tsrem - frrem;

    if(pad > 0) {
      /* Must pad TS packet */
	
      *tsb++ = 0x30;
      tsrem -= pad;
      *tsb++ = --pad;

      memset(tsb, 0x00, pad);
      tsb += pad;
    } else {
      *tsb++ = 0x10;
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

      len = pkt_len(pkt) + PES_HEADER_SIZE - 6;

      if(len > 65535) {
	/* It's okay to write len as 0 in transport streams,
	   but only for video frames, and i dont expect any of the
	   audio frames to exceed 64k 
	*/
	len = 0;
      }

      *tsb++ = len >> 8;
      *tsb++ = len;

      *tsb++ = 0x80;  /* MPEG2 */
      *tsb++ = 0xc0;  /* pts & dts is present */
      *tsb++ = 10;    /* length of rest of header (pts & dts) */

      /* Write PTS */

      t = av_rescale_q(pkt->pkt_pts, AV_TIME_BASE_Q, mpeg_tc);
      *tsb++ = (((t >> 30) & 7) << 1) | 1;
      u16 = (((t >> 15) & 0x7fff) << 1) | 1;
      *tsb++ = u16 >> 8;
      *tsb++ = u16;
      u16 = ((t & 0x7fff) << 1) | 1;
      *tsb++ = u16 >> 8;
      *tsb++ = u16;

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

    /* Timestamp of last byte + 1 */
    t = pkt->pkt_duration * (off + tsrem) / (pkt->pkt_payloadlen + 
					      PES_HEADER_SIZE);

    /* Fix any rounding errors */
    if(t > pkt->pkt_duration)
      t = pkt->pkt_duration;

    tm->tm_ts_stop = pkt->pkt_dts + t - tms->tms_muxoffset;


    if(printts)printf("TS: copy %7d (%3d bytes) pad = %7d: %lld - %lld\n",
		      off, tsrem, pad, tm->tm_ts_start, tm->tm_ts_stop);
    
    TAILQ_INSERT_TAIL(&tms->tms_metaqueue, tm, tm_link);
    tms->tms_meta_packets++;

    off += tsrem;
  }
  if(printts)printf("end @ %lld\n", pkt->pkt_dts + pkt->pkt_duration);
  if(printts)exit(0);
}


/**
 *
 */
static int64_t
check_total_delay(th_muxer_t *tm)
{
  th_muxstream_t *tms;
  int64_t delta = -1, v;
  LIST_FOREACH(tms, &tm->tm_streams, tms_muxer_link0) {
    v = get_delay(tms);
    if(v > delta)
      delta = v;
  }
  return delta;
}



/**
 *
 */
static int64_t
get_start_pcr(th_muxer_t *tm)
{
  th_muxstream_t *tms;
  th_metapkt_t *f;
  int64_t r = INT64_MAX;

  LIST_FOREACH(tms, &tm->tm_streams, tms_muxer_link0) {

    f = TAILQ_FIRST(&tms->tms_metaqueue);
    if(f == NULL)
      continue;
    if(f->tm_ts_start < r)
      r = f->tm_ts_start;
  }
  return r;
}




/**
 *
 */
static void
ts_deliver(void *aux, int64_t now)
{
  ts_muxer_t *ts = aux;
  th_muxer_t *tm = ts->ts_muxer;
  th_muxstream_t *tms, *c;
  int rate;
  int64_t v, pcr;
  th_metapkt_t *x, *y;
  uint8_t *tsb;
  
  int cnt;

  pcr = now - ts->ts_pcr_ref + ts->ts_pcr_offset;

  if(ts->ts_last_pcr + 20000 < now) {
    LIST_FOREACH(tms, &tm->tm_streams, tms_muxer_link0)
      if(tms->tms_index == ts->ts_pcrpid)
	break;

    if(tms != NULL) {
      uint8_t pkt[188];
      tsb = &pkt[0];
      *tsb++ = 0x47;
      *tsb++ = 0;
      *tsb++ = 0;

      /* Insert CC */
      *tsb++ = 0x20 | (tms->tms_cc & 0xf);

      *tsb++ = 183;
      *tsb++ = 0x10;   /* PCR flag */

      v = av_rescale_q(pcr, AV_TIME_BASE_Q, mpeg_tc);
      *tsb++ =  v >> 25;
      *tsb++ =  v >> 17;
      *tsb++ =  v >> 9;
      *tsb++ =  v >> 1;
      *tsb++ = (v & 1) << 7;
      *tsb++ = 0;
 
      ts_muxer_add_packet(ts, pkt, tms->tms_index);
      ts->ts_last_pcr = now;
    }
  }
 
  cnt = ts->ts_blocks_per_packet - ts->ts_block;

  while(--cnt >= 0) {
    c = LIST_FIRST(&tm->tm_streams);
    tms = LIST_NEXT(c, tms_muxer_link0);
    for(; tms != NULL; tms = LIST_NEXT(tms, tms_muxer_link0)) {
      x = TAILQ_FIRST(&c->tms_metaqueue);
      y = TAILQ_FIRST(&tms->tms_metaqueue);
      
      if(x != NULL && y != NULL && y->tm_ts_start < x->tm_ts_start)
	c = tms;
    }

    tms = NULL;

    x = TAILQ_FIRST(&c->tms_metaqueue);
    if(x == NULL) {
      printf("underrun\n");
      break;
    }
    TAILQ_REMOVE(&c->tms_metaqueue, x, tm_link);
    c->tms_meta_packets--;

    /* Insert CC */
    x->tm_pkt[3] = (x->tm_pkt[3] & 0xf0) | (c->tms_cc & 0xf);
    c->tms_cc++;

    ts_muxer_add_packet(ts, x->tm_pkt, c->tms_index);
    free(x);
  }


  rate = 0;
  LIST_FOREACH(tms, &tm->tm_streams, tms_muxer_link0) {
    rate += (tms->tms_meta_packets * 1000000ULL) / (uint64_t)TS_LOOKAHEAD;
    //    rate += estimate_blockrate(tms);
  }

#if 0
  printf("TS blockrate = %d\n", rate);

  LIST_FOREACH(tms, &tm->tm_streams, tms_muxer_link0) {
    printf("%-10s: %-5d %-8lld | ",
	   htstvstreamtype2txt(tms->tms_stream->st_type), 
	   tms->tms_meta_packets,
	   get_delay(tms));
  }
  printf("\n");
#endif
    
  v = 1000000 / (rate / ts->ts_blocks_per_packet);
  dtimer_arm_hires(&ts->ts_stream_timer, ts_deliver, ts, now + v);
}

/**
 *
 */
static void
ts_mux_packet_input(void *opaque, th_muxstream_t *tms, th_pkt_t *pkt)
{
  ts_muxer_t *ts = opaque;
  th_muxer_t *tm = ts->ts_muxer;
  int64_t v, pcr;

  ts_mux_gen_packets(ts, tms, pkt);

  if(ts->ts_running == 0) {
    v = check_total_delay(tm);
    if(v < TS_LOOKAHEAD)
      return;
    pcr = get_start_pcr(tm);
    if(pcr == INT64_MAX)
      return;
    ts->ts_pcr_offset = pcr;
    ts->ts_pcr_ref = getclock_hires();
    ts->ts_running = 1;
    ts_deliver(ts, getclock_hires());
  }
}



/**
 *
 */
ts_muxer_t *
ts_muxer_init(th_subscription_t *s, ts_mux_output_t *output,
	      void *opaque, int flags)
{
  ts_muxer_t *ts = calloc(1, sizeof(ts_muxer_t));
  int dopcr;
  int pididx = PID_ES_BASE;
  th_muxstream_t *tms;
  th_muxer_t *tm;
  th_stream_t *st;

  ts->ts_pcr_offset = AV_NOPTS_VALUE;
  ts->ts_pcr_ref = AV_NOPTS_VALUE;

  ts->ts_output = output;
  ts->ts_output_opaque = opaque;
  ts->ts_flags = flags;

  ts->ts_subscription = s;
  tm = ts->ts_muxer = muxer_init(s, ts_mux_packet_input, ts);
 
  ts->ts_blocks_per_packet = 7;
  ts->ts_packet = malloc(188 * ts->ts_blocks_per_packet);

  /* Do TS MUX specific init per stream */

  LIST_FOREACH(tms, &tm->tm_streams, tms_muxer_link0) {
    st = tms->tms_stream;

    dopcr = 0;
    switch(st->st_type) {
    case HTSTV_MPEG2VIDEO:
      tms->tms_muxoffset = 200000;
      tms->tms_sc = 0x1ec;
      dopcr = 1;
      break;
    case HTSTV_MPEG2AUDIO:
      tms->tms_muxoffset = 75000;
      tms->tms_sc = 0x1cd;
      break;
    case HTSTV_AC3:
      tms->tms_muxoffset = 75000;
      tms->tms_sc = 0x1bd;
      break;
    case HTSTV_H264:
      tms->tms_muxoffset = 900000;
      tms->tms_sc = 0x1e0;
      dopcr = 1;
      break;
    default:
      continue;
    }

    if(dopcr && ts->ts_pcrpid == 0)
      ts->ts_pcrpid = pididx;

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
