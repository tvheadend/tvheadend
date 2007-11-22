/*
 *  PES parsing functions
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ffmpeg/avcodec.h>

#include "tvhead.h"
#include "pes.h"
#include "dispatch.h"
#include "buffer.h"

static void pes_compute_dts(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt);
static void pes_compute_pts(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt);
static void pes_compute_duration(th_transport_t *t, th_stream_t *st,
				 th_pkt_t *pkt);


#define getu32(b, l) ({						\
  uint32_t x = (b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]);	\
  b+=4;								\
  l-=4; 							\
  x;								\
})

#define getu16(b, l) ({				\
  uint16_t x = (b[0] << 8 | b[1]);		\
  b+=2;						\
  l-=2;						\
  x;						\
})

#define getu8(b, l) ({				\
  uint8_t x = b[0];				\
  b+=1;						\
  l-=1;						\
  x;						\
})

#define getpts(b, l) ({					\
  int64_t _pts;						\
  _pts = (int64_t)((getu8(b, l) >> 1) & 0x07) << 30;	\
  _pts |= (int64_t)(getu16(b, l) >> 1) << 15;		\
  _pts |= (int64_t)(getu16(b, l) >> 1);			\
  _pts;							\
})




/*
 * pes_packet_input()
 *
 * return 0 if buf-memory is claimed by us, -1 if memory can be resued
 */
int
pes_packet_input(th_transport_t *t, th_stream_t *st, uint8_t *buf, size_t len)
{
  int64_t dts = AV_NOPTS_VALUE, pts = AV_NOPTS_VALUE, ts, ptsoff;
  uint8_t *outbuf;
  int hdr, flags, hlen, rlen, outlen;
  th_pkt_t *pkt;
  AVRational mpeg_tc = {1, 90000};

  avgstat_add(&st->st_rate, len, dispatch_clock);

  if(len < 3)
    return -1;

  hdr   = getu8(buf, len);
  flags = getu8(buf, len);
  hlen  = getu8(buf, len);
  
  if(len < hlen || (hdr & 0xc0) != 0x80)
    return -1;
  
  if((flags & 0xc0) == 0xc0) {
    if(hlen < 10) 
      return -1;

    pts = getpts(buf, len);
    dts = getpts(buf, len);
    hlen -= 10;

  } else if((flags & 0xc0) == 0x80) {
    if(hlen < 5)
      return -1;

    dts = pts = getpts(buf, len);
    hlen -= 5;
  }

  buf += hlen;
  len -= hlen;

  if(t->tht_dts_start == AV_NOPTS_VALUE) {
    if(dts == AV_NOPTS_VALUE)
      return -1;

    t->tht_dts_start = dts;
  }

  if(dts != AV_NOPTS_VALUE) {
    ptsoff = (int32_t)pts - (int32_t)dts;
    dts -= t->tht_dts_start;
    dts &= 0x1ffffffffULL;
    ts = dts + ((int64_t)st->st_dts_u << 33);
    if((ts < 0 || ts > 10000000) && st->st_dts == AV_NOPTS_VALUE)
      return -1;

    if(ts < st->st_dts) {
      st->st_dts_u++;
      ts = dts + ((int64_t)st->st_dts_u << 33);
    }

    st->st_dts = dts;
   
    pts = dts + ptsoff;

    dts = av_rescale_q(dts, mpeg_tc, AV_TIME_BASE_Q);
    pts = av_rescale_q(pts, mpeg_tc, AV_TIME_BASE_Q);
  }

  while(len > 0) {
    rlen = av_parser_parse(st->st_parser, st->st_ctx,
			   &outbuf, &outlen, buf, len, pts, dts);

    if(outlen) {
      pkt = pkt_alloc(outbuf, outlen, st->st_parser->pts, st->st_parser->dts);
      pkt->pkt_commercial = t->tht_tt_commercial_advice;

      pes_compute_dts(t, st, pkt);
      dts = AV_NOPTS_VALUE;
      pts = AV_NOPTS_VALUE;
    }
    buf += rlen;
    len -= rlen;
  }
  return -1;
}

/*
 * If DTS is unknown we hold packets until we see a packet with correct
 * DTS, then we do linear interpolation.
 */
static void
pes_compute_dts(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt)
{
  int64_t dts, d_dts, v;

  if(pkt->pkt_dts == AV_NOPTS_VALUE) {
    st->st_dtsq_len++;
    pkt->pkt_pts = AV_NOPTS_VALUE;
    TAILQ_INSERT_TAIL(&st->st_dtsq, pkt, pkt_queue_link);
    return;
  }

  if(TAILQ_FIRST(&st->st_dtsq) == NULL) {
    st->st_last_dts = pkt->pkt_dts;
    pes_compute_pts(t, st, pkt);
    return;
  }
  TAILQ_INSERT_TAIL(&st->st_dtsq, pkt, pkt_queue_link);
  v = st->st_dtsq_len + 1;
  d_dts = (pkt->pkt_dts - st->st_last_dts) / v;
  dts = st->st_last_dts;

  while((pkt = TAILQ_FIRST(&st->st_dtsq)) != NULL) {
    dts += d_dts;
    pkt->pkt_dts = dts;
    pkt->pkt_pts = AV_NOPTS_VALUE;

    TAILQ_REMOVE(&st->st_dtsq, pkt, pkt_queue_link);
    pes_compute_pts(t, st, pkt);
  }
  st->st_dtsq_len = 0;
  st->st_last_dts = dts;
}


/*
 * Compute PTS of packets.
 *
 * It seems some providers send TV without PTS/DTS for all video
 * frames, so we need to reconstruct that here.
 *
 */
static void
pes_compute_pts(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt)
{
  th_pkt_t *p;
  uint8_t *buf;
  uint32_t sc;

  if(pkt->pkt_pts == AV_NOPTS_VALUE) {

    /* PTS not known, figure it out */

    switch(st->st_type) {
    case HTSTV_MPEG2VIDEO:

      /* Figure frame type */

      if(pkt_len(pkt) < 6) {
	pkt_deref(pkt);
	return;
      }

      buf = pkt_payload(pkt);
      sc = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];

      if(sc == 0x100) { /* PICTURE START CODE */
	pkt->pkt_frametype = (buf[5] >> 3) & 7;
      } else {
	pkt->pkt_frametype = PKT_I_FRAME;
      }

      if(pkt->pkt_frametype < PKT_I_FRAME ||
	 pkt->pkt_frametype > PKT_B_FRAME) {
	pkt_deref(pkt);
	return;
      }

      TAILQ_INSERT_TAIL(&st->st_ptsq, pkt, pkt_queue_link);
      st->st_ptsq_len++;

      while((pkt = TAILQ_FIRST(&st->st_ptsq)) != NULL) {
	switch(pkt->pkt_frametype) {
	case PKT_B_FRAME:
	  /* B-frames have same PTS as DTS, pass them on */
	  pkt->pkt_pts = pkt->pkt_dts;
	  break;

	case PKT_I_FRAME:
	case PKT_P_FRAME:
	  /* Presentation occures at DTS of next I or P frame,
	     try to find it */
	  p = TAILQ_NEXT(pkt, pkt_queue_link);
	  while(1) {
	    if(p == NULL)
	      return; /* not arrived yet, wait */
	    if(p->pkt_frametype <= PKT_P_FRAME) {
	      pkt->pkt_pts = p->pkt_dts;
	      break;
	    }
	    p = TAILQ_NEXT(p, pkt_queue_link);
	  }
	  break;
	}

	TAILQ_REMOVE(&st->st_ptsq, pkt, pkt_queue_link);
	st->st_ptsq_len--;
	pes_compute_duration(t, st, pkt);
      }
      return;

    case HTSTV_H264:
      /* For h264, we cannot do anything (yet) */
      pkt->pkt_pts = pkt->pkt_dts; /* this is wrong */
      break;

    default:
      /* Rest is audio, no decoder delay */
      pkt->pkt_pts = pkt->pkt_dts;
      break;
    }
  }
  pes_compute_duration(t, st, pkt);
}


/*
 * Compute duration of a packet, we do this by keeping a packet
 * until the next one arrives, then we release it
 */
static void
pes_compute_duration(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt)
{
  th_pkt_t *next;
  th_muxer_t *tm;
  int delta;

  delta = abs(pkt->pkt_dts - pkt->pkt_pts);

  if(delta > 250000) 
    delta =  250000;

  if(delta > st->st_peak_presentation_delay)
    st->st_peak_presentation_delay = delta;

  TAILQ_INSERT_TAIL(&st->st_durationq, pkt, pkt_queue_link);
  
  pkt = TAILQ_FIRST(&st->st_durationq);
  if((next = TAILQ_NEXT(pkt, pkt_queue_link)) == NULL)
    return;

  pkt->pkt_duration = next->pkt_dts - pkt->pkt_dts;

  TAILQ_REMOVE(&st->st_durationq, pkt, pkt_queue_link);

  if(pkt->pkt_duration < 1) {
    pkt_deref(pkt);
    return;
  }

  pkt->pkt_stream = st;

  /* Alert all muxers tied to us that a new packet has arrived */

  LIST_FOREACH(tm, &t->tht_muxers, tm_transport_link)
    tm->tm_new_pkt(tm, st, pkt);

  /* Unref (and possibly free) the packet, muxers are supposed
     to increase refcount or copy packet if they need anything */

  pkt_deref(pkt);
}
