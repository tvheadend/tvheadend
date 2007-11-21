/*
 *  tvheadend, MPEG transport stream functions
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

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include <libhts/htscfg.h>

#include <ffmpeg/avcodec.h>

#include "tvhead.h"
#include "dispatch.h"
#include "teletext.h"
#include "transports.h"
#include "subscriptions.h"
#include "pes.h"
#include "psi.h"
#include "buffer.h"
#include "tsmux.h"

#define PES_HEADER_SIZE 19

static void ts_muxer_relock(th_muxer_t *tm, uint64_t pcr);


/*
 * Return the deadline for a given packet
 */
static int64_t
muxer_pkt_deadline(th_pkt_t *pkt, th_muxstream_t *tms)
{
  th_stream_t *st = pkt->pkt_stream;
  int64_t r;

  r = pkt->pkt_dts;
  if(st != NULL)
    r -= st->st_peak_presentation_delay;

  r -= tms->tms_mux_offset;
  return r;
}


/*
 *
 */
static void
tms_set_curpkt(th_muxstream_t *tms, th_pkt_t *pkt)
{
  if(tms->tms_curpkt != NULL)
    pkt_deref(tms->tms_curpkt);

  tms->tms_offset = 0;

  if(pkt != NULL) {
    tms->tms_curpkt = pkt_ref(pkt);
  } else {
    tms->tms_curpkt = NULL;
  }
}

/*
 *
 */
static void
tms_stream_set_stale(th_muxer_t *tm, th_muxstream_t *tms, int64_t pcr)
{
  tms->tms_nextblock = INT64_MAX;
  tms->tms_staletime = pcr;
  LIST_REMOVE(tms, tms_muxer_link);
  LIST_INSERT_HEAD(&tm->tm_stale_streams, tms, tms_muxer_link);
  tms_set_curpkt(tms, NULL);
}

/*
 *
 */
static void
tms_stream_add_meta(th_muxer_t *tm, th_muxstream_t *tms, th_pkt_t *pkt)
{
  LIST_REMOVE(tms, tms_muxer_link);
  LIST_INSERT_HEAD(&tm->tm_active_streams, tms, tms_muxer_link);
  tms_set_curpkt(tms, pkt);
  tms->tms_block_interval = 0;
  tms->tms_nextblock = 0;
}

/*
 *
 */
static void
tms_stream_set_active(th_muxer_t *tm, th_muxstream_t *tms, th_pkt_t *pkt,
		      int64_t pcr)
{
  int l, dt;
  int64_t dl;

  assert(pkt->pkt_duration > 0);

  tms->tms_nextblock = pcr;
  dl = muxer_pkt_deadline(pkt, tms);
  dt = dl - pcr;
  l = (pkt_len(pkt) + PES_HEADER_SIZE) / 188;

  tms->tms_dl = dl;
  tms->tms_block_interval = dt / l;

#if 0
  if(tms->tms_stream)
    printf("%-15s dts=%lld, ft=%d, bi = %5d dt = %6d dl = %lld pcr = %lld "
	   "size=%d\n",
	   htstvstreamtype2txt(tms->tms_stream->st_type),
	   pkt->pkt_dts,
	   pkt->pkt_frametype, tms->tms_block_interval, dt, dl, pcr,
	   pkt_len(pkt) + PES_HEADER_SIZE);
#endif

  if(tms->tms_block_interval < 10)
    tms->tms_block_interval = 10;

  LIST_REMOVE(tms, tms_muxer_link);
  LIST_INSERT_HEAD(&tm->tm_active_streams, tms, tms_muxer_link);
  tms_set_curpkt(tms, pkt);
  tms->tms_blockcnt = 0;
}





/*
 * Generates a 188 bytes TS packet in 'tsb'
 */
static int
ts_make_pkt(th_muxer_t *tm, th_muxstream_t *tms, uint8_t *tsb, int64_t pcr)
{
  uint8_t *tsb0 = tsb;
  th_pkt_t *pkt = tms->tms_curpkt;
  uint8_t *src;
  int tsrem; /* Remaining bytes in tspacket */
  int frrem; /* Remaining bytes in frame */
  int64_t ts;
  int pad, cc, len;
  uint16_t u16;
  int is_section = pkt->pkt_stream == NULL;
  int header_len = 0;
  int dumppkt = 0;
  AVRational mpeg_tc = {1, 90000};

  if(pkt_payload(pkt) == NULL)
    return -1;

  frrem = pkt_len(pkt) - tms->tms_offset;
  assert(frrem > 0);

  cc = tms->tms_cc++;
  tsrem = 184;

  if(tms->tms_offset == 0) {
    /* When writing the packet header, shave of a bit of available 
       payload size */
    if(is_section) {
      header_len = 1;
    } else {
      header_len = PES_HEADER_SIZE;
    }
    tsrem -= header_len;
  }

  if(tm->tm_flags & TM_HTSCLIENTMODE) {
    /* UGLY */
    if(tms->tms_stream)
      *tsb++ = tms->tms_stream->st_type;
    else
      *tsb++ = 0xff;

  } else {
    /* TS marker */
    *tsb++ = 0x47;
  }

  /* Write PID and optionally payload unit start indicator */
  *tsb++ = tms->tms_index >> 8 | (tms->tms_offset ? 0 : 0x40);
  *tsb++ = tms->tms_index;



  /* Compute amout of padding needed */
  pad = tsrem - frrem;


  if(pcr != AV_NOPTS_VALUE) {

    /* Insert PCR */
    
    tsrem -= 8;
    pad   -= 8;
    if(pad < 0)
      pad = 0;

    *tsb++ = (cc & 0xf) | 0x30;
    *tsb++ = 7 + pad;
    *tsb++ = 0x10;   /* PCR flag */

    ts = av_rescale_q(pcr, AV_TIME_BASE_Q, mpeg_tc);
    *tsb++ =  ts >> 25;
    *tsb++ =  ts >> 17;
    *tsb++ =  ts >> 9;
    *tsb++ =  ts >> 1;
    *tsb++ = (ts & 1) << 7;
    *tsb++ = 0;

    memset(tsb, 0xff, pad);
    tsb   += pad;
    tsrem -= pad;

  } else if(pad > 0) {
    /* Must pad TS packet */

    *tsb++ = (cc & 0xf) | 0x30;
    tsrem -= pad;
    *tsb++ = --pad;

    memset(tsb, 0x00, pad);
    tsb += pad;
  } else {
    *tsb++ = (cc & 0xf) | 0x10;
  }

  if(tms->tms_offset == 0) {
    if(!is_section) {

      /* First TS packet for this frame, write PES headers */

      /* Write startcode */

      *tsb++ = 0;
      *tsb++ = 0;
      *tsb++ = tms->tms_sc >> 8;
      *tsb++ = tms->tms_sc;

      /* Write total frame length (without accounting for startcode and
	 length field itself */

      len = pkt_len(pkt) + header_len - 6;

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
      *tsb++ = 10;    /* length of header (pts & dts) */

      /* Write PTS */

      ts = av_rescale_q(pkt->pkt_pts, AV_TIME_BASE_Q, mpeg_tc);
      *tsb++ = (((ts >> 30) & 7) << 1) | 1;
      u16 = (((ts >> 15) & 0x7fff) << 1) | 1;
      *tsb++ = u16 >> 8;
      *tsb++ = u16;
      u16 = ((ts & 0x7fff) << 1) | 1;
      *tsb++ = u16 >> 8;
      *tsb++ = u16;

      /* Write DTS */

      ts = av_rescale_q(pkt->pkt_dts, AV_TIME_BASE_Q, mpeg_tc);
      *tsb++ = (((ts >> 30) & 7) << 1) | 1;
      u16 = (((ts >> 15) & 0x7fff) << 1) | 1;
      *tsb++ = u16 >> 8;
      *tsb++ = u16;
      u16 = ((ts & 0x7fff) << 1) | 1;
      *tsb++ = u16 >> 8;
      *tsb++ = u16;
    } else {
      *tsb++ = 0; /* Pointer field for tables */
    }
  }

  /* Fill rest of TS packet with payload */

  src = pkt_payload(pkt) + tms->tms_offset;
  memcpy(tsb, src, tsrem);

  tms->tms_offset += tsrem;

  if(dumppkt) {
    int i;
    for(i = 0; i < 188; i++)
      printf("%02x%c", tsb0[i], "               \n"[i & 0xf]);
    printf("\n");
  }

  assert(tsb0 + 188 == tsb + tsrem);
  return 0;
}


/*
 *
 */
int
ts_mux_packet(th_muxer_t *tm, int64_t pcr, uint8_t *outbuf, int maxblocks)
{
  th_muxstream_t *tms, *t;
  th_pkt_t *pkt;
  int rem, i;
  int64_t pcr1;

  LIST_FOREACH(tms, &tm->tm_active_streams, tms_muxer_link) {
    if(tms->tms_nextblock < pcr)
      tms->tms_nextblock = pcr;

    if(tms->tms_curpkt != NULL)
      pkt_load(tms->tms_curpkt);
  }

  for(i = 0; i < maxblocks; i++) {
    
    /* Find the stream with the lowest/closest time for next scheduled
       transport stream block */

    tms = NULL;
    LIST_FOREACH(t, &tm->tm_active_streams, tms_muxer_link)
      if(tms == NULL || t->tms_nextblock < tms->tms_nextblock)
	tms = t;

    if(tms == NULL)
      break; /* No active streams, cannot do anything */


    pkt = tms->tms_curpkt;
    if(pkt == NULL)
      continue;
 
    /* Do we need to send a new PCR update? */

    if(tms->tms_dopcr && tms->tms_nextpcr <= pcr) {
      pcr1 = pcr + 200000;
      tms->tms_nextpcr = pcr + 20000;
    } else {
      pcr1 = AV_NOPTS_VALUE;
    }

    /* Generate a transport stream packet */

    if(ts_make_pkt(tm, tms, outbuf, pcr1) < 0) {
      /* Packet buffer no longer exist, we need to seek again */
      return -1;
    }
    outbuf += 188;

    rem = pkt_len(pkt) - tms->tms_offset;
    if(rem == 0) {
      /* End of frame, find next */
      while(1) {
	pkt = pkt->pkt_on_stream_queue ?
	  TAILQ_NEXT(pkt, pkt_queue_link) : NULL;
	if(pkt != NULL) {
	  /* Ok, reset counters */
	  tms_stream_set_active(tm, tms, pkt, pcr);
	  pkt_load(tms->tms_curpkt);

	} else {
	  /* This stream cannot contribute, move it to stale list */
	  tms_stream_set_stale(tm, tms, pcr);
	}
	break;
      }
      continue;
    }
    tms->tms_nextblock += tms->tms_block_interval;
    tms->tms_blockcnt++;
#if 0
    if(tms->tms_stream)
      printf("%-10s %lld [seg: %d] pcr=%lld dl=%lld nxt=%lld off=%d d=%lld\n",
	     htstvstreamtype2txt(tms->tms_stream->st_type),
	     pkt->pkt_dts, tms->tms_blockcnt, pcr, tms->tms_dl,
	     tms->tms_nextblock, tms->tms_offset,
	     tms->tms_nextblock - pcr);
#endif
  }
  return i;
}

/*
 *
 */
void
tm_gen_pat_pmt(th_muxer_t *tm, int64_t pcr)
{
  th_subscription_t *s = tm->tm_subscription;
  th_transport_t *t = s->ths_transport;
  th_pkt_t *pkt;

  pkt = pkt_alloc(NULL, 4096, pcr, pcr);
  pkt->pkt_payloadlen = psi_build_pmt(tm, pkt_payload(pkt), 4096);
  tms_stream_add_meta(tm, tm->tm_pmt, pkt);
  pkt_deref(pkt);

  pkt = pkt_alloc(NULL, 4096, pcr, pcr);
  pkt->pkt_payloadlen = psi_build_pat(t, pkt_payload(pkt), 4096);
  tms_stream_add_meta(tm, tm->tm_pat, pkt);
  pkt_deref(pkt);
}



static void ts_gen_timer_callback(void *aux, int64_t now);

/*
 *
 */
void
ts_gen_packet(th_muxer_t *tm, int64_t clk)
{
  int64_t pcr, next;
  th_muxstream_t *tms;
  int r;

  dtimer_disarm(&tm->tm_timer);

  pcr = tm->tm_start_dts + clk - tm->tm_clockref;

  do {
    if(pcr >= tm->tm_next_pat) {
      tm->tm_next_pat = pcr + 100000;
      tm_gen_pat_pmt(tm, pcr);
    }

    r = ts_mux_packet(tm, pcr, tm->tm_packet, tm->tm_blocks_per_packet);
    if(r == -1) {
      ts_muxer_relock(tm, pcr);
      return;
    }

    tm->tm_callback(tm->tm_opaque, tm->tm_subscription, tm->tm_packet, r, pcr);

    /* Figure when next packet must be sent */
    next = INT64_MAX;
    LIST_FOREACH(tms, &tm->tm_active_streams, tms_muxer_link)
      if(tms->tms_nextblock < next)
	next = tms->tms_nextblock;

    next = next - pcr;
  } while(next < 2000);

  if(next > 100000)
    next = 100000; /* We always want to send PAT/PMT at this interval */

  dtimer_arm_hires(&tm->tm_timer, ts_gen_timer_callback, tm, clk + next);
}

/*
 *
 */
static void
ts_gen_timer_callback(void *aux, int64_t now)
{
  th_muxer_t *tm = aux;
  ts_gen_packet(tm, now);
}


/*
 * start demuxing
 */
static int
ts_muxer_start(th_muxer_t *tm)
{
  th_muxstream_t *tms;
  th_pkt_t *f, *l, *pkt;
  th_stream_t *st;
  int64_t v;

  LIST_FOREACH(tms, &tm->tm_stopped_streams, tms_muxer_link) {
    st = tms->tms_stream;
    if(st == NULL)
      continue;

    f = TAILQ_FIRST(&st->st_pktq);
    l = TAILQ_LAST(&st->st_pktq, th_pkt_queue);

    if(f == NULL)
      return -1;

    v = muxer_pkt_deadline(f, tms) - f->pkt_duration;

    if(tm->tm_start_dts < v) {
      tm->tm_start_dts = v;
      tms->tms_tmppkt = f;
      continue;
    }
    
    v = muxer_pkt_deadline(l, tms);

    if(tm->tm_start_dts > v) {
      tm->tm_start_dts = v - l->pkt_duration;
      tms->tms_tmppkt = l;
      continue;
    }

    tms->tms_tmppkt = NULL;

    while(f != NULL || l != NULL) {
      
      if(f != NULL)
	f = TAILQ_NEXT(f, pkt_queue_link);

      if(f != NULL) {
	v = muxer_pkt_deadline(f, tms);
	if(tm->tm_start_dts >= v - f->pkt_duration && tm->tm_start_dts < v) {
	  tms->tms_tmppkt = f;
	  break;
	}
      }
      if(l != NULL)
	l = TAILQ_PREV(l, th_pkt_queue, pkt_queue_link);
      
      if(l != NULL) {
	v = muxer_pkt_deadline(l, tms);
	if(tm->tm_start_dts >= v - l->pkt_duration && tm->tm_start_dts < v) {
	  tms->tms_tmppkt = l;
	  break;
	}
      }
    }

    if(tms->tms_tmppkt == NULL)
      return -1;
  }

  /* All streams have a lock */

  tm->tm_status = TM_PLAY;

  while((tms = LIST_FIRST(&tm->tm_stopped_streams)) != NULL) {
    st  = tms->tms_stream;
    pkt = tms->tms_tmppkt;

    if(st == NULL) {
      LIST_REMOVE(tms, tms_muxer_link);
      LIST_INSERT_HEAD(&tm->tm_active_streams, tms, tms_muxer_link);
    } else {
      tms_stream_set_active(tm, tms, pkt, tm->tm_start_dts);
    }
  }

  tm->tm_clockref = getclock_hires();
  ts_gen_packet(tm, tm->tm_clockref);
  return 0;
}


/*
 *
 */
static void
ts_muxer_reinit_stream(th_muxer_t *tm, th_muxstream_t *tms)
{
  tms->tms_nextblock = INT64_MAX;
  LIST_REMOVE(tms, tms_muxer_link);
  LIST_INSERT_HEAD(&tm->tm_stopped_streams, tms, tms_muxer_link);
  tms_set_curpkt(tms, NULL);
}

/*
 *
 */
static void
ts_muxer_relock(th_muxer_t *tm, uint64_t pcr)
{
  th_muxstream_t *tms;

  while((tms = LIST_FIRST(&tm->tm_active_streams)) != NULL)
    ts_muxer_reinit_stream(tm, tms);

  while((tms = LIST_FIRST(&tm->tm_stale_streams)) != NULL)
    ts_muxer_reinit_stream(tm, tms);

  tm->tm_start_dts = pcr;
  tm->tm_status = TM_WAITING_FOR_LOCK;
  ts_muxer_start(tm);
}


/*
 * pause playback
 */
void
ts_muxer_pause(th_muxer_t *tm)
{
  tm->tm_pauseref = getclock_hires();
  
  dtimer_disarm(&tm->tm_timer);
  tm->tm_status = TM_PAUSE;
}




/*
 * playback start
 */
void
ts_muxer_play(th_muxer_t *tm, int64_t toffset)
{
  int64_t dts = 0, t;
  th_muxstream_t *tms;
  th_stream_t *st;
  int64_t clk;

  switch(tm->tm_status) {
  case TM_IDLE:
  case TM_WAITING_FOR_LOCK:

    toffset = 0;
    break;
    
  case TM_PLAY:
    if(toffset == AV_NOPTS_VALUE)
      return;

    toffset = 0;
    return; /* XXX */

  case TM_PAUSE:
    if(toffset == AV_NOPTS_VALUE) {
      /* Just continue stream, adjust clock reference with the amount
	 of time we was paused */
      clk = getclock_hires();
      t = clk - tm->tm_pauseref;
      
      tm->tm_clockref += t;
      ts_gen_packet(tm, clk);
      return;
    }
    break;
  }

  assert(LIST_FIRST(&tm->tm_active_streams) == NULL);
  assert(LIST_FIRST(&tm->tm_stale_streams)  == NULL);

  LIST_FOREACH(tms, &tm->tm_stopped_streams, tms_muxer_link) {
    if((st = tms->tms_stream) == NULL)
      continue;
    if(st->st_last_dts > dts)
      dts = st->st_last_dts;
  }

  dts -= toffset;
  if(dts < 0)
    dts = 0;

  tm->tm_start_dts = dts;
  tm->tm_status = TM_WAITING_FOR_LOCK;

  ts_muxer_start(tm);
}


/*
 * Meta streams
 */
th_muxstream_t *
tm_create_meta_stream(th_muxer_t *tm, int pid)
{
  th_muxstream_t *tms;
  
  tms = calloc(1, sizeof(th_muxstream_t));
  tms->tms_muxer = tm;
  LIST_INSERT_HEAD(&tm->tm_stopped_streams, tms, tms_muxer_link);
  tms->tms_index = pid;
  return tms;
}



/*
 *
 */
static void
ts_encode_new_packet(th_muxer_t *tm, th_stream_t *st, th_pkt_t *pkt)
{
  th_muxstream_t *tms;
  int64_t pcr;

  pkt_store(pkt);  /* need to keep packet around */

  switch(tm->tm_status) {
  case TM_IDLE:
    break;
    
  case TM_WAITING_FOR_LOCK:
    ts_muxer_start(tm);
    break;
    
  case TM_PLAY:
    LIST_FOREACH(tms, &tm->tm_stale_streams, tms_muxer_link) {
      if(tms->tms_stream == st) {
	pcr = tm->tm_start_dts + getclock_hires() - tm->tm_clockref;
	tms_stream_set_active(tm, tms, pkt, pcr);
	break;
      }
    }
    break;
    
  case TM_PAUSE:
    break;
  }
}





/*
 * TS Muxer
 */
th_muxer_t *
ts_muxer_init(th_subscription_t *s, th_mux_output_t *cb, void *opaque,
	      int flags)
{
  th_transport_t *t = s->ths_transport;
  th_stream_t *st;
  th_muxer_t *tm;
  th_muxstream_t *tms;
  int pididx = 100;
  uint32_t sc;
  int dopcr = 0;
  int offset;

  tm = calloc(1, sizeof(th_muxer_t));
  LIST_INSERT_HEAD(&t->tht_muxers, tm, tm_transport_link);
  tm->tm_subscription = s;

  tm->tm_clockref = getclock_hires();
  tm->tm_blocks_per_packet = 7;
  tm->tm_callback = cb;
  tm->tm_opaque = opaque;
  tm->tm_new_pkt = ts_encode_new_packet;
  tm->tm_flags = flags;

  tm->tm_packet = malloc(188 * tm->tm_blocks_per_packet);

  pididx = 200;

  LIST_FOREACH(st, &t->tht_streams, st_link) {
    dopcr = 0;
    offset = 0;
    switch(st->st_type) {
    case HTSTV_MPEG2VIDEO:
      sc = 0x1e0;
      dopcr = 1;
      break;
    case HTSTV_MPEG2AUDIO:
      sc = 0x1c0;
      break;
    case HTSTV_AC3:
      sc = 0x1bd;
      break;
    case HTSTV_H264:
      sc = 0x1e0;
      dopcr = 1;
      break;
    default:
      continue;
    }

    tms = calloc(1, sizeof(th_muxstream_t));
    tms->tms_muxer = tm;
    tms->tms_stream = st;
    tms->tms_dopcr = dopcr;
    tms->tms_mux_offset = offset;

    LIST_INSERT_HEAD(&tm->tm_stopped_streams, tms, tms_muxer_link);
    LIST_INSERT_HEAD(&tm->tm_media_streams, tms, tms_muxer_media_link);

    tms->tms_index = pididx;
    tms->tms_sc = sc;
    pididx++;
  }

  tm->tm_pat = tm_create_meta_stream(tm, 0);
  tm->tm_pmt = tm_create_meta_stream(tm, 100);
  return tm;
}


/*
 *
 */
static void
tms_destroy(th_muxstream_t *tms)
{
  if(tms->tms_stream)
    LIST_REMOVE(tms, tms_muxer_media_link);

  LIST_REMOVE(tms, tms_muxer_link);
  tms_set_curpkt(tms, NULL);
  memset(tms, 0xff, sizeof(th_muxstream_t));
  free(tms);
}


/*
 *
 */
void
ts_muxer_deinit(th_muxer_t *tm)
{
  th_muxstream_t *tms;

  LIST_REMOVE(tm, tm_transport_link);

  dtimer_disarm(&tm->tm_timer);
    
  tms_destroy(tm->tm_pat);
  tms_destroy(tm->tm_pmt);

  while((tms = LIST_FIRST(&tm->tm_media_streams)) != NULL)
    tms_destroy(tms);

  free(tm->tm_packet);
  free(tm);
}

