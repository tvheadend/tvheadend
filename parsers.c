/*
 *  Packet parsing functions
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
#include <assert.h>

#include "tvhead.h"
#include "parsers.h"
#include "parser_h264.h"
#include "bitstream.h"
#include "buffer.h"
#include "dispatch.h"

static const AVRational mpeg_tc = {1, 90000};

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


static int parse_mpeg2video(th_transport_t *t, th_stream_t *st, size_t len,
			    uint32_t next_startcode, int sc_offset);

static int parse_h264(th_transport_t *t, th_stream_t *st, size_t len,
		      uint32_t next_startcode, int sc_offset);

typedef int (vparser_t)(th_transport_t *t, th_stream_t *st, size_t len,
			uint32_t next_startcode, int sc_offset);

typedef void (aparser_t)(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt);

static void parse_video(th_transport_t *t, th_stream_t *st, uint8_t *data,
			int len, vparser_t *vp);

static void parse_audio(th_transport_t *t, th_stream_t *st, uint8_t *data,
			int len, int start, aparser_t *vp);

static void parse_mpegaudio(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt);

static void parse_ac3(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt);

void parse_compute_pts(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt);

static void parser_deliver(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt);

static int parse_pes_header(th_transport_t *t, th_stream_t *st, uint8_t *buf,
			    size_t len);

void parser_compute_duration(th_transport_t *t, th_stream_t *st,
			     th_pkt_t *pkt);

/**
 * Parse raw mpeg data
 */
void
parse_raw_mpeg(th_transport_t *t, th_stream_t *st, uint8_t *data, 
	       int len, int start, int err)
{
 
  if(LIST_FIRST(&t->tht_muxers) == NULL)
    return; /* No muxers will take packet, so drop here */

  switch(st->st_type) {
  case HTSTV_MPEG2VIDEO:
    parse_video(t, st, data, len, parse_mpeg2video);
    break;

  case HTSTV_H264:
    parse_video(t, st, data, len, parse_h264);
    break;

  case HTSTV_MPEG2AUDIO:
    parse_audio(t, st, data, len, start, parse_mpegaudio);
    break;

  case HTSTV_AC3:
    parse_audio(t, st, data, len, start, parse_ac3);
    break;

  default:
    break;
  }
}


/**
 * Generic video parser
 *
 * We scan for startcodes a'la 0x000001xx and let a specific parser
 * derive further information.
 */
static void
parse_video(th_transport_t *t, th_stream_t *st, uint8_t *data, int len,
	    vparser_t *vp)
{
  uint32_t sc;
  int i, r;

  sc = st->st_startcond;

  if(st->st_buffer == NULL) {
    st->st_buffer_size = 4000;
    st->st_buffer = malloc(st->st_buffer_size);
  }

  if(st->st_buffer_ptr + len + 4 >= st->st_buffer_size) {
    st->st_buffer_size += len * 4;
    st->st_buffer = realloc(st->st_buffer, st->st_buffer_size);
  }

  for(i = 0; i < len; i++) {
    st->st_buffer[st->st_buffer_ptr++] = data[i];
    sc = sc << 8 | data[i];

    if((sc & 0xffffff00) != 0x00000100)
      continue;

    r = st->st_buffer_ptr - 4;

    if(r > 0 && st->st_startcode != 0) {
      r = vp(t, st, r, sc, st->st_startcode_offset);
    } else {
      r = 1;
    }

    if(r) {
      /* Reset packet parser upon length error or if parser
	 tells us so */
      st->st_buffer_ptr = 0;
      st->st_buffer[st->st_buffer_ptr++] = sc >> 24;
      st->st_buffer[st->st_buffer_ptr++] = sc >> 16;
      st->st_buffer[st->st_buffer_ptr++] = sc >> 8;
      st->st_buffer[st->st_buffer_ptr++] = sc >> 0;
    }
    st->st_startcode = sc;
    st->st_startcode_offset = st->st_buffer_ptr - 4;

  }
  st->st_startcond = sc;  


}



/**
 * Generic audio parser
 *
 * We trust 'start' to be set where a new frame starts, thats where we
 * can expect to find the system start code.
 *
 * We then trust ffmpeg to parse and extract packets for use
 */
static void 
parse_audio(th_transport_t *t, th_stream_t *st, uint8_t *data,
	    int len, int start, aparser_t *ap)
{
  int hlen, rlen;
  uint8_t *outbuf;
  int outlen;
  th_pkt_t *pkt;
  int64_t dts;

  if(start) {
    /* Payload unit start */
    st->st_parser_state = 1;
    st->st_buffer_ptr = 0;
  }

  if(st->st_parser_state == 0)
    return;

  if(st->st_parser_state == 1) {

    if(st->st_buffer == NULL)
      st->st_buffer = malloc(1000); 

    if(st->st_buffer_ptr + len >= 1000)
      return;

    memcpy(st->st_buffer + st->st_buffer_ptr, data, len);
    st->st_buffer_ptr += len;

    if(st->st_buffer_ptr < 9)
      return; 

    if((hlen = parse_pes_header(t, st, st->st_buffer + 6,
				st->st_buffer_ptr - 6)) < 0)
      return;

    data = st->st_buffer     + hlen + 6;
    len  = st->st_buffer_ptr - hlen - 6;

    st->st_parser_state = 2;

    assert(len >= 0);
    if(len == 0)
      return;
  }

  while(len > 0) {

    rlen = av_parser_parse(st->st_parser, st->st_ctx,
			   &outbuf, &outlen, data, len, 
			   st->st_curpts, st->st_curdts);

    st->st_curdts = AV_NOPTS_VALUE;
    st->st_curpts = AV_NOPTS_VALUE;

    if(outlen) {
      dts = st->st_parser->dts;
      if(dts == AV_NOPTS_VALUE)
	dts = st->st_nextdts;

      pkt = pkt_alloc(outbuf, outlen, dts, dts);
      pkt->pkt_commercial = t->tht_tt_commercial_advice;

      ap(t, st, pkt);

    }
    data += rlen;
    len  -= rlen;
  }
}


/**
 * Mpeg audio parser
 */
const static int mpegaudio_freq_tab[4] = {44100, 48000, 32000, 0};

static void
parse_mpegaudio(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt)
{
  uint8_t *buf = pkt->pkt_payload;
  uint32_t header;
  int sample_rate, duration;

  header = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]);

  sample_rate = mpegaudio_freq_tab[(header >> 10) & 3];
  duration = 90000 * 1152 / sample_rate;
  pkt->pkt_duration = duration;
  st->st_nextdts = pkt->pkt_dts + duration;

  parser_deliver(t, st, pkt);
}



/**
 * AC3 audio parser
 */
const static int ac3_freq_tab[4] = {48000, 44100, 32000, 0};

const static uint16_t ac3_frame_size_tab[38][3] = {
    { 64,   69,   96   },
    { 64,   70,   96   },
    { 80,   87,   120  },
    { 80,   88,   120  },
    { 96,   104,  144  },
    { 96,   105,  144  },
    { 112,  121,  168  },
    { 112,  122,  168  },
    { 128,  139,  192  },
    { 128,  140,  192  },
    { 160,  174,  240  },
    { 160,  175,  240  },
    { 192,  208,  288  },
    { 192,  209,  288  },
    { 224,  243,  336  },
    { 224,  244,  336  },
    { 256,  278,  384  },
    { 256,  279,  384  },
    { 320,  348,  480  },
    { 320,  349,  480  },
    { 384,  417,  576  },
    { 384,  418,  576  },
    { 448,  487,  672  },
    { 448,  488,  672  },
    { 512,  557,  768  },
    { 512,  558,  768  },
    { 640,  696,  960  },
    { 640,  697,  960  },
    { 768,  835,  1152 },
    { 768,  836,  1152 },
    { 896,  975,  1344 },
    { 896,  976,  1344 },
    { 1024, 1114, 1536 },
    { 1024, 1115, 1536 },
    { 1152, 1253, 1728 },
    { 1152, 1254, 1728 },
    { 1280, 1393, 1920 },
    { 1280, 1394, 1920 },
};

static void
parse_ac3(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt)
{
  uint8_t *buf = pkt->pkt_payload;
  uint32_t src, fsc;
  int sample_rate, frame_size, duration, bsid;

  src = buf[4] >> 6;
  fsc = buf[4] & 0x3f;
  bsid = (buf[5] & 0xf) - 8;
  if(bsid < 0)
    bsid = 0;

  sample_rate = ac3_freq_tab[src] >> bsid;
  frame_size = ac3_frame_size_tab[fsc][src] * 2;

  duration = 90000 * 1536 / sample_rate;

  pkt->pkt_duration = duration;
  st->st_nextdts = pkt->pkt_dts + duration;
  parser_deliver(t, st, pkt);
}



/**
 * PES header parser
 *
 * Extract DTS and PTS and update current values in stream
 */
static int
parse_pes_header(th_transport_t *t, th_stream_t *st, uint8_t *buf, size_t len)
{
  int64_t dts, pts;
  int hdr, flags, hlen;

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

  } else if((flags & 0xc0) == 0x80) {
    if(hlen < 5)
      return -1;

    dts = pts = getpts(buf, len);
  } else
    return hlen + 3;

  st->st_curdts = dts;
  st->st_curpts = pts;
  return hlen + 3;
} 



/**
 * MPEG2VIDEO frame duration table (in 90kHz clock domain)
 */
const static unsigned int mpeg2video_framedurations[16] = {
  0,
  3753,
  3750,
  3600,
  3003,
  3000,
  1800,
  1501,
  1500,
};

/**
 * Parse mpeg2video picture start
 */
static int
parse_mpeg2video_pic_start(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt,
			   bitstream_t *bs)
{
  int v, pct;

  if(bs->len < 29)
    return 1;

  skip_bits(bs, 10); /* temporal reference */

  pct = read_bits(bs, 3);
  if(pct < PKT_I_FRAME || pct > PKT_B_FRAME)
    return 1; /* Illegal picture_coding_type */

  pkt->pkt_frametype = pct;

  /* If this is the first I-frame seen, set dts_start as a reference
     offset */
  if(pct == PKT_I_FRAME && t->tht_dts_start == AV_NOPTS_VALUE)
    t->tht_dts_start = st->st_curdts;

  v = read_bits(bs, 16); /* vbv_delay */
  if(v == 0xffff)
    st->st_vbv_delay = -1;
  else
    st->st_vbv_delay = av_rescale_q(v, st->st_tb, AV_TIME_BASE_Q);
  return 0;
}

/**
 * Parse mpeg2video sequence start
 */
static int
parse_mpeg2video_seq_start(th_transport_t *t, th_stream_t *st,
			   bitstream_t *bs)
{
  int v;

  if(bs->len < 61)
    return 1;
  
  skip_bits(bs, 12);
  skip_bits(bs, 12);
  skip_bits(bs, 4);
  st->st_frame_duration = mpeg2video_framedurations[read_bits(bs, 4)];
  v = read_bits(bs, 18) * 400;
  skip_bits(bs, 1);
  
  v = read_bits(bs, 10) * 16 * 1024 / 8;
  st->st_vbv_size = v;
  return 0;
}


/**
 * MPEG2VIDEO specific reassembly
 *
 * Process all startcodes (also the system ones)
 *
 * Extract framerate (or duration to be more specific)
 *
 * 'steal' the st->st_buffer and use it as 'pkt' buffer
 *
 */
static int
parse_mpeg2video(th_transport_t *t, th_stream_t *st, size_t len, 
		 uint32_t next_startcode, int sc_offset)
{
  uint8_t *buf = st->st_buffer + sc_offset;
  bitstream_t bs;
  th_pkt_t pkt0;  /* Fake temporary packet */

  init_bits(&bs, buf + 4, (len - 4) * 8);

  switch(st->st_startcode) {
  case 0x000001e0 ... 0x000001ef:
    /* System start codes for video */
    if(len >= 9)
      parse_pes_header(t, st, buf + 6, len - 6);
    return 1;

  case 0x00000100:
    /* Picture start code */
    if(st->st_frame_duration == 0 || st->st_curdts == AV_NOPTS_VALUE)
      return 1;

    if(parse_mpeg2video_pic_start(t, st, &pkt0, &bs))
      return 1;

    if(st->st_curpkt != NULL)
      pkt_deref(st->st_curpkt);

    st->st_curpkt = pkt_alloc(NULL, 0, st->st_curpts, st->st_curdts);
    st->st_curpkt->pkt_frametype = pkt0.pkt_frametype;
    st->st_curpkt->pkt_duration = st->st_frame_duration;
    st->st_curpkt->pkt_commercial = t->tht_tt_commercial_advice;

    break;

  case 0x000001b3:
    /* Sequence start code */
    if(parse_mpeg2video_seq_start(t, st, &bs))
      return 1;

    break;

  case 0x000001b5:
    if(len < 5)
      return 1;
    switch(buf[4] >> 4) {
    case 0x1:
      /* sequence extension */
      //      printf("Sequence extension, len = %d\n", len);
      if(len < 10)
	return 1;
      //      printf("Profile = %d\n", buf[4] & 0x7);
      //      printf("  Level = %d\n", buf[5] >> 4);
      break;
    }
    break;


  case 0x00000101 ... 0x000001af:
    /* Slices */

    if(next_startcode == 0x100 || next_startcode > 0x1af) {
      /* Last picture slice (because next not a slice) */

      if(st->st_curpkt == NULL) {
	/* no packet, may've been discarded by sanity checks here */
	return 1; 
      }

      st->st_curpkt->pkt_payload = st->st_buffer;
      st->st_curpkt->pkt_payloadlen = st->st_buffer_ptr - 4;
      st->st_curpkt->pkt_duration = st->st_frame_duration;

      parse_compute_pts(t, st, st->st_curpkt);
      st->st_curpkt = NULL;

      st->st_buffer = malloc(st->st_buffer_size);
      
      /* If we know the frame duration, increase DTS accordingly */
      st->st_curdts += st->st_frame_duration;

      /* PTS cannot be extrapolated (it's not linear) */
      st->st_curpts = AV_NOPTS_VALUE; 
      return 1;
    }
    break;

  default:
    break;
  }

  return 0;
}


/**
 * H.264 parser
 */
static int
parse_h264(th_transport_t *t, th_stream_t *st, size_t len, 
	   uint32_t next_startcode, int sc_offset)
{
  uint8_t *buf = st->st_buffer + sc_offset;
  uint32_t sc = st->st_startcode;
  int64_t d;
  int l2, pkttype;
  bitstream_t bs;

  if(sc >= 0x000001e0 && sc <= 0x000001ef) {
    /* System start codes for video */

    if(len >= 9)
      parse_pes_header(t, st, buf + 6, len - 6);

    if(st->st_prevdts != AV_NOPTS_VALUE) {
      d = (st->st_curdts - st->st_prevdts) & 0x1ffffffffLL;

      if(d < 90000)
	st->st_frame_duration = d;
    }
    st->st_prevdts = st->st_curdts;
    return 1;
  }
  
  bs.data = NULL;

  switch(sc & 0x1f) {

  case 7:
    h264_nal_deescape(&bs, buf + 3, len - 3);
    if(h264_decode_seq_parameter_set(st, &bs))
      return 1;
    break;

  case 8:
    h264_nal_deescape(&bs, buf + 3, len - 3);
    if(h264_decode_pic_parameter_set(st, &bs))
      return 1;
    break;


  case 5: /* IDR+SLICE */
  case 1:
    if(st->st_curpkt != NULL || st->st_frame_duration == 0)
      break;

    if(t->tht_dts_start == AV_NOPTS_VALUE)
      t->tht_dts_start = st->st_curdts;

    l2 = len - 3 > 64 ? 64 : len - 3;
    h264_nal_deescape(&bs, buf + 3, len); /* we just the first stuff */
    if(h264_decode_slice_header(st, &bs, &pkttype))
      return 1;

    st->st_curpkt = pkt_alloc(NULL, 0, st->st_curpts, st->st_curdts);
    st->st_curpkt->pkt_frametype = pkttype;
    st->st_curpkt->pkt_duration = st->st_frame_duration;
    st->st_curpkt->pkt_commercial = t->tht_tt_commercial_advice;
    break;

  default:
    break;
  }

  free(bs.data);

  if(next_startcode >= 0x000001e0 && next_startcode <= 0x000001ef) {
    /* Complete frame */
    
    if(st->st_curpkt == NULL)
      return 1;

    st->st_curpkt->pkt_payload = st->st_buffer;
    st->st_curpkt->pkt_payloadlen = st->st_buffer_ptr;
    parser_deliver(t, st, st->st_curpkt);
    
    st->st_curpkt = NULL;
    st->st_buffer = malloc(st->st_buffer_size);
    return 1;
  }
  return 0;
}



/**
 * Compute PTS (if not known)
 *
 * We do this by placing packets on a queue and wait for next I/P
 * frame to appear
 */
void
parse_compute_pts(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt)
{
  th_pkt_t *p;
 
  if(pkt->pkt_pts != AV_NOPTS_VALUE && st->st_ptsq_len == 0) {
    /* PTS known and no other packets in queue, deliver at once */

    if(pkt->pkt_duration == 0)
      parser_compute_duration(t, st, pkt);
    else
      parser_deliver(t, st, pkt);
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

    if(pkt->pkt_duration == 0)
      parser_compute_duration(t, st, pkt);
    else
      parser_deliver(t, st, pkt);
  }
}

/**
 * Compute duration of a packet, we do this by keeping a packet
 * until the next one arrives, then we release it
 */
void
parser_compute_duration(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt)
{
  th_pkt_t *next;
  int64_t d;

  TAILQ_INSERT_TAIL(&st->st_durationq, pkt, pkt_queue_link);
  
  pkt = TAILQ_FIRST(&st->st_durationq);
  if((next = TAILQ_NEXT(pkt, pkt_queue_link)) == NULL)
    return;

  d = next->pkt_dts - pkt->pkt_dts;
  TAILQ_REMOVE(&st->st_durationq, pkt, pkt_queue_link);
  if(d < 10) {
    pkt_deref(pkt);
    return;
  }

  pkt->pkt_duration = d;

  parser_deliver(t, st, pkt);
}



/**
 * De-wrap and normalize PTS/DTS to 1MHz clock domain
 */
static void
parser_deliver(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt)
{
  th_muxer_t *tm;
  int64_t dts, pts, ptsoff;

  assert(pkt->pkt_dts != AV_NOPTS_VALUE);
  assert(pkt->pkt_pts != AV_NOPTS_VALUE);
  assert(pkt->pkt_duration > 10);

  if(t->tht_dts_start == AV_NOPTS_VALUE) {
    pkt_deref(pkt);
    return;
  }

  dts = pkt->pkt_dts;
  pts = pkt->pkt_pts;

  /* Compute delta between PTS and DTS (and watch out for 33 bit wrap) */
  ptsoff = (pts - dts) & 0x1ffffffffLL;

  /* Subtract the transport wide start offset */
  dts -= t->tht_dts_start;

  if(st->st_last_dts == AV_NOPTS_VALUE) {
    if(dts < 0) {
      /* Early packet with negative time stamp, drop those */
      pkt_deref(pkt);
      return;
    }
  } else if(dts + st->st_dts_epoch < st->st_last_dts - (1LL << 24)) {
    /* DTS wrapped, increase upper bits */
    st->st_dts_epoch += 1ULL << 33;
  }

  dts += st->st_dts_epoch;
  st->st_last_dts = dts;

  pts = dts + ptsoff;

  /* Rescale to tvheadned internal 1MHz clock */
  pkt->pkt_dts     =av_rescale_q(dts,               st->st_tb, AV_TIME_BASE_Q);
  pkt->pkt_pts     =av_rescale_q(pts,               st->st_tb, AV_TIME_BASE_Q);
  pkt->pkt_duration=av_rescale_q(pkt->pkt_duration, st->st_tb, AV_TIME_BASE_Q);
#if 0
  printf("%-12s %d %10lld %10lld %d\n",
	 htstvstreamtype2txt(st->st_type),
	 pkt->pkt_frametype,
	 pkt->pkt_dts,
	 pkt->pkt_pts,
	 pkt->pkt_duration);
#endif
  pkt->pkt_stream = st;
  
  avgstat_add(&st->st_rate, pkt->pkt_payloadlen, dispatch_clock);

  /* Alert all muxers tied to us that a new packet has arrived */
  
  LIST_FOREACH(tm, &t->tht_muxers, tm_transport_link)
    tm->tm_new_pkt(tm, st, pkt);

  /* Unref (and possibly free) the packet, muxers are supposed
     to increase refcount or copy packet if they need anything */

  pkt_deref(pkt);
}

/**
 * Receive whole frames
 *
 * Analyze them as much as we need to and patch up PTS and duration
 * if needed
 */
void
parser_enqueue_packet(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt)
{
  uint8_t *buf = pkt->pkt_payload;
  uint32_t sc = 0xffffffff;
  int i, err = 0, rem;
  bitstream_t bs;

  assert(pkt->pkt_dts != AV_NOPTS_VALUE); /* We require DTS to be set */
  pkt->pkt_duration = 0;

  /* Per stream type analysis */
  
  switch(st->st_type) {
  case HTSTV_MPEG2VIDEO:
    for(i = 0; i < pkt->pkt_payloadlen && err == 0; i++) {
      sc = (sc << 8) | buf[i];

      if((sc & 0xffffff00) != 0x00000100)
	continue;

      if(sc >= 0x101 && sc <= 0x1af)
	break; /* slices, dont scan further */

      rem = pkt->pkt_payloadlen - i - 1;
      init_bits(&bs, buf + i + 1, rem);

      switch(sc) {
      case 0x00000100: /* Picture start code */
	err = parse_mpeg2video_pic_start(t, st, pkt, &bs);
	break;

      case 0x000001b3: /* Sequence start code */
	if(t->tht_dts_start == AV_NOPTS_VALUE)
	  t->tht_dts_start = pkt->pkt_dts;
	err = parse_mpeg2video_seq_start(t, st, &bs);
	break;
      }
    }
    break;

  default:
    pkt->pkt_pts = pkt->pkt_dts;
    break;
  }

  if(err) {
    pkt_deref(pkt);
    return;
  }
    

  parse_compute_pts(t, st, pkt);
}
