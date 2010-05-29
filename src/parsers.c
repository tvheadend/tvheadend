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
#include <assert.h>

#include "tvhead.h"
#include "parsers.h"
#include "parser_h264.h"
#include "parser_latm.h"
#include "bitstream.h"
#include "packet.h"
#include "transports.h"
#include "streaming.h"

#define PTS_MASK 0x1ffffffffLL
//#define PTS_MASK 0x7ffffLL

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

static void parse_video(th_transport_t *t, th_stream_t *st, const uint8_t *data,
			int len, vparser_t *vp);

static void  parse_audio_with_lavc(th_transport_t *t, th_stream_t *st, 
				   const uint8_t *data, int len, aparser_t *ap);

static void parse_audio(th_transport_t *t, th_stream_t *st, const uint8_t *data,
			int len, int start, aparser_t *vp);

static void parse_aac(th_transport_t *t, th_stream_t *st, const uint8_t *data,
		      int len, int start);

static void parse_subtitles(th_transport_t *t, th_stream_t *st, 
			    const uint8_t *data, int len, int start);

static void parse_mpegaudio(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt);

static void parse_ac3(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt);

static void parse_compute_pts(th_transport_t *t, th_stream_t *st,
			      th_pkt_t *pkt);

static void parser_deliver(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt,
			   int checkts);

static int parse_pes_header(th_transport_t *t, th_stream_t *st, uint8_t *buf,
			    size_t len);

static void parser_compute_duration(th_transport_t *t, th_stream_t *st,
				    th_pktref_t *pr);

/**
 * Parse raw mpeg data
 */
void
parse_mpeg_ts(th_transport_t *t, th_stream_t *st, const uint8_t *data, 
	      int len, int start, int err)
{
  switch(st->st_type) {
  case SCT_MPEG2VIDEO:
    parse_video(t, st, data, len, parse_mpeg2video);
    break;

  case SCT_H264:
    parse_video(t, st, data, len, parse_h264);
    break;

  case SCT_MPEG2AUDIO:
    parse_audio(t, st, data, len, start, parse_mpegaudio);
    break;

  case SCT_AC3:
    parse_audio(t, st, data, len, start, parse_ac3);
    break;

  case SCT_DVBSUB:
    parse_subtitles(t, st, data, len, start);
    break;
    
  case SCT_AAC:
    parse_aac(t, st, data, len, start);
    break;

  default:
    break;
  }
}


/**
 * Parse program stream, as from V4L2, etc.
 *
 * Note: data does not include startcode and packet length
 */
void
parse_mpeg_ps(th_transport_t *t, th_stream_t *st, uint8_t *data, int len)
{
  int hlen;

  hlen = parse_pes_header(t, st, data, len);
#if 0  
  int i;
  for(i = 0; i < 16; i++)
    printf("%02x.", data[i]);
  printf(" %d\n", hlen);
#endif
  data += hlen;
  len  -= hlen;

  if(len < 1)
    return;

  switch(st->st_type) {
  case SCT_MPEG2AUDIO:
    parse_audio_with_lavc(t, st, data, len, parse_mpegaudio);
    break;

  case SCT_MPEG2VIDEO:
    parse_video(t, st, data, len, parse_mpeg2video);
    break;

  default:
    break;
  }
}


/**
 * Parse AAC LATM
 */
static void 
parse_aac(th_transport_t *t, th_stream_t *st, const uint8_t *data,
	  int len, int start)
{
  int l, muxlen, p;
  th_pkt_t *pkt;

  if(start) {
    /* Payload unit start */
    st->st_parser_state = 1;
    st->st_buffer_ptr = 0;
    st->st_parser_ptr = 0;
  }

  if(st->st_parser_state == 0)
    return;

  if(st->st_buffer == NULL) {
    st->st_buffer_size = 4000;
    st->st_buffer = malloc(st->st_buffer_size);
  }

  if(st->st_buffer_ptr + len >= st->st_buffer_size) {
    st->st_buffer_size += len * 4;
    st->st_buffer = realloc(st->st_buffer, st->st_buffer_size);
  }
  
  memcpy(st->st_buffer + st->st_buffer_ptr, data, len);
  st->st_buffer_ptr += len;


  if(st->st_parser_ptr == 0) {
    int hlen;

    if(st->st_buffer_ptr < 9)
      return;

    hlen = parse_pes_header(t, st, st->st_buffer + 6, st->st_buffer_ptr - 6);
    if(hlen < 0)
      return;
    st->st_parser_ptr += 6 + hlen;
  }


  p = st->st_parser_ptr;

  while((l = st->st_buffer_ptr - p) > 3) {
    if(st->st_buffer[p] == 0x56 && (st->st_buffer[p + 1] & 0xe0) == 0xe0) {
      muxlen = (st->st_buffer[p + 1] & 0x1f) << 8 |
	st->st_buffer[p + 2];

      if(l < muxlen + 3)
	break;

      pkt = parse_latm_audio_mux_element(t, st, st->st_buffer + p + 3, muxlen);

      if(pkt != NULL)
	parser_deliver(t, st, pkt, 1);

      p += muxlen + 3;
    } else {
      p++;
    }
  }
  st->st_parser_ptr = p;
}





/**
 * Generic video parser
 *
 * We scan for startcodes a'la 0x000001xx and let a specific parser
 * derive further information.
 */
static void
parse_video(th_transport_t *t, th_stream_t *st, const uint8_t *data, int len,
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
parse_audio(th_transport_t *t, th_stream_t *st, const uint8_t *data,
	    int len, int start, aparser_t *ap)
{
  int hlen;

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

    //There is no video pid, start the stream right away
    if(t->tht_dts_start == AV_NOPTS_VALUE && t->tht_servicetype == ST_RADIO)
      t->tht_dts_start = st->st_curdts;
    
    assert(len >= 0);
    if(len == 0)
      return;
  }
  parse_audio_with_lavc(t, st, data, len, ap);
}


/**
 * Use libavcodec's parsers for audio parsing
 */
static void 
parse_audio_with_lavc(th_transport_t *t, th_stream_t *st, const uint8_t *data,
		      int len, aparser_t *ap)
{
  uint8_t *outbuf;
  int outlen, rlen;
  th_pkt_t *pkt;
  int64_t dts;

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
  if(sample_rate == 0) {
    pkt_ref_dec(pkt);
    return;
  }

  duration = 90000 * 1152 / sample_rate;
  pkt->pkt_duration = duration;
  st->st_nextdts = pkt->pkt_dts + duration;

  parser_deliver(t, st, pkt, 1);
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
  if(sample_rate == 0) {
    pkt_ref_dec(pkt);
    return;
  }

  frame_size = ac3_frame_size_tab[fsc][src] * 2;

  duration = 90000 * 1536 / sample_rate;

  pkt->pkt_duration = duration;
  st->st_nextdts = pkt->pkt_dts + duration;
  parser_deliver(t, st, pkt, 1);
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
    goto err;

  if((flags & 0xc0) == 0xc0) {
    if(hlen < 10) 
      goto err;

    pts = getpts(buf, len);
    dts = getpts(buf, len);

  } else if((flags & 0xc0) == 0x80) {
    if(hlen < 5)
      goto err;

    dts = pts = getpts(buf, len);
  } else
    return hlen + 3;

  st->st_curdts = dts & PTS_MASK;
  st->st_curpts = pts & PTS_MASK;
  return hlen + 3;

 err:
    limitedlog(&st->st_loglimit_pes, "TS", transport_component_nicename(st),
	       "Corrupted PES header");
  return -1;
} 



/**
 * MPEG2VIDEO frame duration table (in 90kHz clock domain)
 */
const unsigned int mpeg2video_framedurations[16] = {
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
parse_mpeg2video_pic_start(th_transport_t *t, th_stream_t *st, int *frametype,
			   bitstream_t *bs)
{
  int v, pct;

  if(bs->len < 29)
    return 1;

  skip_bits(bs, 10); /* temporal reference */

  pct = read_bits(bs, 3);
  if(pct < PKT_I_FRAME || pct > PKT_B_FRAME)
    return 1; /* Illegal picture_coding_type */

  *frametype = pct;

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
  int frametype;

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

    if(parse_mpeg2video_pic_start(t, st, &frametype, &bs))
      return 1;

    if(st->st_curpkt != NULL)
      pkt_ref_dec(st->st_curpkt);

    st->st_curpkt = pkt_alloc(NULL, 0, st->st_curpts, st->st_curdts);
    st->st_curpkt->pkt_frametype = frametype;
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
  int l2, pkttype, l;
  bitstream_t bs;

  if(sc == 0x10c) {
    /* RBSP padding, we don't want this */
    
    l = len - sc_offset;
    memcpy(buf, buf + l, 4); /* Move down new start code */
    st->st_buffer_ptr -= l;  /* Drop buffer */
  }

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
    if(h264_decode_seq_parameter_set(st, &bs)) {
      free(bs.data);
      return 1;
    }
    break;

  case 8:
    h264_nal_deescape(&bs, buf + 3, len - 3);
    if(h264_decode_pic_parameter_set(st, &bs)) {
      free(bs.data);
      return 1;
    }
    break;


  case 5: /* IDR+SLICE */
  case 1:
    if(st->st_curpkt != NULL || st->st_frame_duration == 0 ||
       st->st_curdts == AV_NOPTS_VALUE)
      break;

    if(t->tht_dts_start == AV_NOPTS_VALUE)
      t->tht_dts_start = st->st_curdts;

    l2 = len - 3 > 64 ? 64 : len - 3;
    h264_nal_deescape(&bs, buf + 3, l2); /* we just the first stuff */
    if(h264_decode_slice_header(st, &bs, &pkttype)) {
      free(bs.data);
      return 1;
    }

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
    parser_deliver(t, st, st->st_curpkt, 1);
    
    st->st_curpkt = NULL;
    st->st_buffer = malloc(st->st_buffer_size);
    return 1;
  }
  return 0;
}

/**
 * http://broadcasting.ru/pdf-standard-specifications/subtitling/dvb-sub/en300743.v1.2.1.pdf
 */
static void
parse_subtitles(th_transport_t *t, th_stream_t *st, const uint8_t *data,
		int len, int start)
{
  th_pkt_t *pkt;
  int psize, hlen;
  uint8_t *buf;

  if(start) {
    /* Payload unit start */
    st->st_parser_state = 1;
    st->st_buffer_ptr = 0;
  }

  if(st->st_parser_state == 0)
    return;

  if(st->st_buffer == NULL) {
    st->st_buffer_size = 4000;
    st->st_buffer = malloc(st->st_buffer_size);
  }

  if(st->st_buffer_ptr + len >= st->st_buffer_size) {
    st->st_buffer_size += len * 4;
    st->st_buffer = realloc(st->st_buffer, st->st_buffer_size);
  }

  memcpy(st->st_buffer + st->st_buffer_ptr, data, len);
  st->st_buffer_ptr += len;

  if(st->st_buffer_ptr < 6)
    return;

  uint32_t startcode =
    (st->st_buffer[0] << 24) |
    (st->st_buffer[1] << 16) |
    (st->st_buffer[2] << 8) |
    (st->st_buffer[3]);

  if(startcode == 0x1be) {
    st->st_parser_state = 0;
    return;
  }

  psize = st->st_buffer[4] << 8 | st->st_buffer[5];

  if(st->st_buffer_ptr != psize + 6)
    return;

  st->st_parser_state = 0;

  hlen = parse_pes_header(t, st, st->st_buffer + 6, st->st_buffer_ptr - 6);
  if(hlen < 0)
    return;

  psize -= hlen;
  buf = st->st_buffer + 6 + hlen;
  
  if(psize < 2 || buf[0] != 0x20 || buf[1] != 0x00)
    return;

  psize -= 2;
  buf += 2;

  if(psize >= 6) {

    // end_of_PES_data_field_marker
    if(buf[psize - 1] == 0xff) {
      pkt = pkt_alloc(buf, psize - 1, st->st_curpts, st->st_curdts);
      pkt->pkt_commercial = t->tht_tt_commercial_advice;
      parser_deliver(t, st, pkt, 0);
    }
  }
}


/**
 * Compute PTS (if not known)
 *
 * We do this by placing packets on a queue and wait for next I/P
 * frame to appear
 */
static void
parse_compute_pts(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt)
{
  th_pktref_t *pr;

  int validpts = pkt->pkt_pts != AV_NOPTS_VALUE && st->st_ptsq_len == 0;


  /* PTS known and no other packets in queue, deliver at once */
  if(validpts && pkt->pkt_duration)
    return parser_deliver(t, st, pkt, 1);


  /* Reference count is transfered to queue */
  pr = malloc(sizeof(th_pktref_t));
  pr->pr_pkt = pkt;


  if(validpts)
    return parser_compute_duration(t, st, pr);

  TAILQ_INSERT_TAIL(&st->st_ptsq, pr, pr_link);
  st->st_ptsq_len++;

  /* */

  while((pr = TAILQ_FIRST(&st->st_ptsq)) != NULL) {
    
    pkt = pr->pr_pkt;

    switch(pkt->pkt_frametype) {
    case PKT_B_FRAME:
      /* B-frames have same PTS as DTS, pass them on */
      pkt->pkt_pts = pkt->pkt_dts;
      break;
      
    case PKT_I_FRAME:
    case PKT_P_FRAME:
      /* Presentation occures at DTS of next I or P frame,
	 try to find it */
      pr = TAILQ_NEXT(pr, pr_link);
      while(1) {
	if(pr == NULL)
	  return; /* not arrived yet, wait */
	if(pr->pr_pkt->pkt_frametype <= PKT_P_FRAME) {
	  pkt->pkt_pts = pr->pr_pkt->pkt_dts;
	  break;
	}
	pr = TAILQ_NEXT(pr, pr_link);
      }
      break;
    }
    
    pr = TAILQ_FIRST(&st->st_ptsq);
    TAILQ_REMOVE(&st->st_ptsq, pr, pr_link);
    st->st_ptsq_len--;

    if(pkt->pkt_duration == 0) {
      parser_compute_duration(t, st, pr);
    } else {
      parser_deliver(t, st, pkt, 1);
      free(pr);
    }
  }
}

/**
 * Compute duration of a packet, we do this by keeping a packet
 * until the next one arrives, then we release it
 */
static void
parser_compute_duration(th_transport_t *t, th_stream_t *st, th_pktref_t *pr)
{
  th_pktref_t *next;
  int64_t d;

  TAILQ_INSERT_TAIL(&st->st_durationq, pr, pr_link);
  
  pr = TAILQ_FIRST(&st->st_durationq);
  if((next = TAILQ_NEXT(pr, pr_link)) == NULL)
    return;

  d = next->pr_pkt->pkt_dts - pr->pr_pkt->pkt_dts;
  TAILQ_REMOVE(&st->st_durationq, pr, pr_link);
  if(d < 10) {
    pkt_ref_dec(pr->pr_pkt);
  } else {
    pr->pr_pkt->pkt_duration = d;
    parser_deliver(t, st, pr->pr_pkt, 1);
  }
  free(pr);
}



/**
 * De-wrap and normalize PTS/DTS to 1MHz clock domain
 */
static void
parser_deliver(th_transport_t *t, th_stream_t *st, th_pkt_t *pkt,
	       int checkts)
{
  int64_t dts, pts, ptsoff, d;

  assert(pkt->pkt_dts != AV_NOPTS_VALUE);
  assert(pkt->pkt_pts != AV_NOPTS_VALUE);

  if(t->tht_dts_start == AV_NOPTS_VALUE) {
    pkt_ref_dec(pkt);
    return;
  }

  dts = pkt->pkt_dts;
  pts = pkt->pkt_pts;

  /* Compute delta between PTS and DTS (and watch out for 33 bit wrap) */
  ptsoff = (pts - dts) & PTS_MASK;

  /* Subtract the transport wide start offset */
  dts -= t->tht_dts_start;

  if(st->st_last_dts == AV_NOPTS_VALUE) {
    if(dts < 0) {
      /* Early packet with negative time stamp, drop those */
      pkt_ref_dec(pkt);
      return;
    }
  } else if(checkts) {
    d = dts + st->st_dts_epoch - st->st_last_dts;

    if(d < 0 || d > 90000) {

      if(d < -PTS_MASK || d > -PTS_MASK + 180000) {

	st->st_bad_dts++;

	if(st->st_bad_dts < 5) {
	  tvhlog(LOG_ERR, "parser", 
		 "transport %s stream %s, DTS discontinuity. "
		 "DTS = %" PRId64 ", last = %" PRId64,
		 t->tht_identifier, streaming_component_type2txt(st->st_type),
		 dts, st->st_last_dts);
	}
      } else {
	/* DTS wrapped, increase upper bits */
	st->st_dts_epoch += PTS_MASK + 1;
	st->st_bad_dts = 0;
      }
    } else {
      st->st_bad_dts = 0;
    }
  }

  st->st_bad_dts++;

  dts += st->st_dts_epoch;
  st->st_last_dts = dts;

  pts = dts + ptsoff;

  /* Rescale to tvheadned internal 1MHz clock */
  pkt->pkt_dts     =av_rescale_q(dts,               st->st_tb, AV_TIME_BASE_Q);
  pkt->pkt_pts     =av_rescale_q(pts,               st->st_tb, AV_TIME_BASE_Q);
  pkt->pkt_duration=av_rescale_q(pkt->pkt_duration, st->st_tb, AV_TIME_BASE_Q);
#if 0
  printf("%-12s %d %10"PRId64" %10"PRId64" %10d %10d\n",
	 streaming_component_type2txt(st->st_type),
	 pkt->pkt_frametype,
	 pkt->pkt_dts,
	 pkt->pkt_pts,
	 pkt->pkt_duration,
	 pkt->pkt_payloadlen);
#endif
  
  avgstat_add(&st->st_rate, pkt->pkt_payloadlen, dispatch_clock);

  /**
   * Input is ok
   */
  transport_set_streaming_status_flags(t, TSS_PACKETS);

  /* Forward packet */
  pkt->pkt_componentindex = st->st_index;

  streaming_message_t *sm = streaming_msg_create_pkt(pkt);

  streaming_pad_deliver(&t->tht_streaming_pad, sm);
  streaming_msg_free(sm);

  /* Decrease our own reference to the packet */
  pkt_ref_dec(pkt);

}
