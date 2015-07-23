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

#include "tvheadend.h"
#include "service.h"
#include "parsers.h"
#include "parser_h264.h"
#include "parser_latm.h"
#include "parser_teletext.h"
#include "bitstream.h"
#include "packet.h"
#include "streaming.h"

#define PTS_MASK 0x1ffffffffLL
//#define PTS_MASK 0x7ffffLL

#define getu32(b, l) ({                                      \
  uint32_t x = (b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]); \
  b+=4;                                                      \
  l-=4;                                                      \
  x;                                                         \
})

#define getu16(b, l) ({            \
  uint16_t x = (b[0] << 8 | b[1]); \
  b+=2;                            \
  l-=2;                            \
  x;                               \
})

#define getu8(b, l) ({ \
  uint8_t x = b[0];    \
  b+=1;                \
  l-=1;                \
  x;                   \
})


static int64_t 
getpts(const uint8_t *p)
{
  int a =  p[0];
  int b = (p[1] << 8) | p[2];
  int c = (p[3] << 8) | p[4];

  if((a & 1) && (b & 1) && (c & 1)) {

    return 
      ((int64_t)((a >> 1) & 0x07) << 30) |
      ((int64_t)((b >> 1)       ) << 15) |
      ((int64_t)((c >> 1)       ))
      ;

  } else {
    // Marker bits not present
    return PTS_UNSET;
  }
}


static int parse_mpeg2video(service_t *t, elementary_stream_t *st, size_t len,
                            uint32_t next_startcode, int sc_offset);

static int parse_h264(service_t *t, elementary_stream_t *st, size_t len,
                      uint32_t next_startcode, int sc_offset);

typedef int (packet_parser_t)(service_t *t, elementary_stream_t *st, size_t len,
                              uint32_t next_startcode, int sc_offset);

typedef void (aparser_t)(service_t *t, elementary_stream_t *st, th_pkt_t *pkt);

static void parse_sc(service_t *t, elementary_stream_t *st, const uint8_t *data,
                     int len, packet_parser_t *vp);

static void parse_mp4a_data(service_t *t, elementary_stream_t *st, int skip_next_check);

static void parse_aac(service_t *t, elementary_stream_t *st, const uint8_t *data,
                      int len, int start);

static void parse_subtitles(service_t *t, elementary_stream_t *st, 
                            const uint8_t *data, int len, int start);

static void parse_teletext(service_t *t, elementary_stream_t *st, 
                           const uint8_t *data, int len, int start);

static int parse_mpa(service_t *t, elementary_stream_t *st, size_t len,
                     uint32_t next_startcode, int sc_offset);

static int parse_mpa2(service_t *t, elementary_stream_t *st);

static int parse_ac3(service_t *t, elementary_stream_t *st, size_t len,
                     uint32_t next_startcode, int sc_offset);

static int parse_eac3(service_t *t, elementary_stream_t *st, size_t len,
                      uint32_t next_startcode, int sc_offset);

static int parse_mp4a(service_t *t, elementary_stream_t *st, size_t ilen,
                      uint32_t next_startcode, int sc_offset);

static void parser_deliver(service_t *t, elementary_stream_t *st, th_pkt_t *pkt);

static void parser_deliver_error(service_t *t, elementary_stream_t *st);

static int parse_pes_header(service_t *t, elementary_stream_t *st,
                            const uint8_t *buf, size_t len);

/**
 * Parse raw mpeg data
 */
void
parse_mpeg_ts(service_t *t, elementary_stream_t *st, const uint8_t *data, 
              int len, int start, int err)
{
  if(err) {
    if (start)
      parser_deliver_error(t, st);
    sbuf_err(&st->es_buf, 1);
  }

  switch(st->es_type) {
  case SCT_MPEG2VIDEO:
    parse_sc(t, st, data, len, parse_mpeg2video);
    break;

  case SCT_H264:
    parse_sc(t, st, data, len, parse_h264);
    break;

  case SCT_MPEG2AUDIO:
    parse_sc(t, st, data, len, parse_mpa);
    break;

  case SCT_AC3:
    parse_sc(t, st, data, len, parse_ac3);
    break;

  case SCT_EAC3:
    parse_sc(t, st, data, len, parse_eac3);
    break;

  case SCT_MP4A:
    parse_sc(t, st, data, len, parse_mp4a);
    break;

  case SCT_DVBSUB:
    parse_subtitles(t, st, data, len, start);
    break;
    
  case SCT_AAC:
    parse_aac(t, st, data, len, start);
    break;

  case SCT_TELETEXT:
    parse_teletext(t, st, data, len, start);
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
parse_mpeg_ps(service_t *t, elementary_stream_t *st, uint8_t *data, int len)
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

  switch(st->es_type) {
  case SCT_MPEG2AUDIO:
    sbuf_append(&st->es_buf_a, data, len);
    parse_mpa2(t, st);
    break;

  case SCT_MPEG2VIDEO:
    parse_sc(t, st, data, len, parse_mpeg2video);
    break;

  default:
    break;
  }
}

/**
 * Parse AAC LATM and ADTS
 */
static void 
parse_aac(service_t *t, elementary_stream_t *st, const uint8_t *data,
          int len, int start)
{
  int l, muxlen, p;
  th_pkt_t *pkt;
  int64_t olddts = PTS_UNSET, oldpts = PTS_UNSET;

  if(st->es_parser_state == 0) {
    if (start) {
      /* Payload unit start */
      st->es_parser_state = 1;
      st->es_parser_ptr = 0;
      sbuf_reset(&st->es_buf, 4000);
    } else {
      return;
    }
  }

  if(start) {
    int hlen;

    if(len < 9)
      return;

    olddts = st->es_curdts;
    oldpts = st->es_curpts;
    hlen = parse_pes_header(t, st, data + 6, len - 6);
    if (hlen >= 0 && st->es_buf.sb_ptr) {
      st->es_curdts = olddts;
      st->es_curpts = oldpts;
    }

    if(hlen < 0)
      return;

    data += 6 + hlen;
    len  -= 6 + hlen;
  }

  sbuf_append(&st->es_buf, data, len);

  p = 0;

  while((l = st->es_buf.sb_ptr - p) > 3) {
    const uint8_t *d = st->es_buf.sb_data + p;
    /* LATM */
    if(d[0] == 0x56 && (d[1] & 0xe0) == 0xe0) {
      muxlen = (d[1] & 0x1f) << 8 | d[2];

      if(l < muxlen + 3)
        break;

      pkt = parse_latm_audio_mux_element(t, st, d + 3, muxlen);

      if(pkt != NULL) {
        pkt->pkt_err = st->es_buf.sb_err;
        parser_deliver(t, st, pkt);
        st->es_buf.sb_err = 0;
      }

      p += muxlen + 3;
    /* ADTS */
    } else if(p == 0 && d[0] == 0xff && (d[1] & 0xf0) == 0xf0) {

      if (l < 7)
        break;

      muxlen = ((uint16_t)(d[3] & 0x03) << 11) | ((uint16_t)d[4] << 3) | (d[5] >> 5);
      if (l < muxlen)
        break;

      sbuf_reset(&st->es_buf_a, 4000);
      sbuf_append(&st->es_buf_a, d, muxlen);
      parse_mp4a_data(t, st, 1);

      p += muxlen;

    /* Wrong bytestream */
    } else {
      tvhtrace("parser", "AAC skip byte %02x", d[0]);
      p++;
    }
  }

  if (p > 0)
    sbuf_cut(&st->es_buf, p);
}


/**
 * Generic video parser
 *
 * We scan for startcodes a'la 0x000001xx and let a specific parser
 * derive further information.
 */
static void
parse_sc(service_t *t, elementary_stream_t *st, const uint8_t *data, int len,
         packet_parser_t *vp)
{
  uint32_t sc = st->es_startcond;
  int i, r;
  sbuf_alloc(&st->es_buf, len);

  for(i = 0; i < len; i++) {
    if(st->es_ssc_intercept == 1) {
      if(st->es_ssc_ptr < sizeof(st->es_ssc_buf))
        st->es_ssc_buf[st->es_ssc_ptr] = data[i];

      st->es_ssc_ptr++;

      if(st->es_ssc_ptr < 5)
        continue;

      uint16_t plen = st->es_ssc_buf[0] << 8 | st->es_ssc_buf[1];
      st->es_incomplete = plen >= 0xffdf;
      int hlen = st->es_ssc_buf[4];

      if(st->es_ssc_ptr < hlen + 5)
        continue;
      
      parse_pes_header(t, st, st->es_ssc_buf + 2, hlen + 3);
      st->es_ssc_intercept = 0;

      if(st->es_buf.sb_ptr > 2)
        sc = st->es_buf.sb_data[st->es_buf.sb_ptr-3] << 16 |
             st->es_buf.sb_data[st->es_buf.sb_ptr-2] << 8 |
             st->es_buf.sb_data[st->es_buf.sb_ptr-1];

      continue;
    }

    st->es_buf.sb_data[st->es_buf.sb_ptr++] = data[i];
    sc = sc << 8 | data[i];
    if((sc & 0xffffff00) != 0x00000100)
      continue;

    if(sc == 0x100 && (len-i)>3) {
      uint32_t tempsc = data[i+1] << 16 | data[i+2] << 8 | data[i+3];

      if(tempsc == 0x1e0)
        continue;
    }

    r = st->es_buf.sb_ptr - st->es_startcode_offset - 4;
    if(r > 0 && st->es_startcode != 0) {
      r = vp(t, st, r, sc, st->es_startcode_offset);

      if(r == 3)
        continue;

      if(r == 4) {
        st->es_buf.sb_ptr -= 4;
        st->es_ssc_intercept = 1;
        st->es_ssc_ptr = 0;
        sc = -1;
        continue;
      }
    } else
      r = 1;

    if(r == 2) {
      assert(st->es_buf.sb_data != NULL);

      // Drop packet
      st->es_buf.sb_ptr = st->es_startcode_offset;

      st->es_buf.sb_data[st->es_buf.sb_ptr++] = sc >> 24;
      st->es_buf.sb_data[st->es_buf.sb_ptr++] = sc >> 16;
      st->es_buf.sb_data[st->es_buf.sb_ptr++] = sc >> 8;
      st->es_buf.sb_data[st->es_buf.sb_ptr++] = sc;
      st->es_startcode = sc;

    } else {
      if(r == 1) {
        /* Reset packet parser upon length error or if parser
           tells us so */
        parser_deliver_error(t, st);
        sbuf_reset_and_alloc(&st->es_buf, 256);
        st->es_buf.sb_data[st->es_buf.sb_ptr++] = sc >> 24;
        st->es_buf.sb_data[st->es_buf.sb_ptr++] = sc >> 16;
        st->es_buf.sb_data[st->es_buf.sb_ptr++] = sc >> 8;
        st->es_buf.sb_data[st->es_buf.sb_ptr++] = sc;
      }
      assert(st->es_buf.sb_data != NULL);
      st->es_startcode = sc;
      st->es_startcode_offset = st->es_buf.sb_ptr - 4;
    }
  }
  st->es_startcond = sc;  
}


/**
 *
 */
static int
depacketize(service_t *t, elementary_stream_t *st, size_t len, 
            uint32_t next_startcode, int sc_offset)
{
  const uint8_t *buf = st->es_buf.sb_data + sc_offset;
  uint32_t sc = st->es_startcode;
  int hlen, plen;

  if((sc != 0x1bd && (sc & ~0x1f) != 0x1c0) || len < 9)
    return 1;

  plen = (buf[4] << 8) | buf[5];

  if(plen + 6 > len)
    return 3;

  if(plen + 6 < len)
    return 1;

  buf += 6;
  len -= 6;

  hlen = parse_pes_header(t, st, buf, len);

  if(hlen < 0)
    return 1;

  buf += hlen;
  len -= hlen;

  st->es_buf_a.sb_err = st->es_buf.sb_err;

  sbuf_append(&st->es_buf_a, buf, len);
  return 0;
}


/**
 *
 */
static void
makeapkt(service_t *t, elementary_stream_t *st, const void *buf,
         int len, int64_t dts, int duration, int channels, int sri,
         int errors)
{
  th_pkt_t *pkt = pkt_alloc(buf, len, dts, dts);

  pkt->pkt_commercial = t->s_tt_commercial_advice;
  pkt->pkt_duration = duration;
  pkt->pkt_channels = channels;
  pkt->pkt_sri = sri;
  pkt->pkt_err = errors;

  parser_deliver(t, st, pkt);

  st->es_curdts = PTS_UNSET;
  st->es_nextdts = dts + duration;
}

/**
 * Parse AAC MP4A
 */

static const int aac_sample_rates[12] =
{
  96000,
  88200,
  64000,
  48000,
  44100,
  32000,
  24000,
  22050,
  16000,
  12000,
  11025,
  8000
};

/**
 * Inspect ADTS header
 */
static int
mp4a_valid_frame(const uint8_t *buf)
{
  return (buf[0] == 0xff) && ((buf[1] & 0xf6) == 0xf0);
}

static void parse_mp4a_data(service_t *t, elementary_stream_t *st,
                            int skip_next_check)
{
  int i, len;
  const uint8_t *buf;

 again:
  buf = st->es_buf_a.sb_data;
  len = st->es_buf_a.sb_ptr;

  for(i = 0; i < len - 6; i++) {
    const uint8_t *p = buf + i;

    if(mp4a_valid_frame(p)) {

      int fsize    = ((p[3] & 0x03) << 11) | (p[4] << 3) | ((p[5] & 0xe0) >> 5);
      int sr_index = (p[2] & 0x3c) >> 2;
      int sr       = aac_sample_rates[sr_index];

      if(sr && fsize) {
        int duration = 90000 * 1024 / sr;
        int64_t dts = st->es_curdts;
        int sri = rate_to_sri(sr);

        if(dts == PTS_UNSET)
          dts = st->es_nextdts;

        if(dts != PTS_UNSET && len >= i + fsize + (skip_next_check ? 0 : 6) &&
           (skip_next_check || mp4a_valid_frame(p + fsize))) {

          int channels = ((p[2] & 0x01) << 2) | ((p[3] & 0xc0) >> 6);

          makeapkt(t, st, p, fsize, dts, duration, channels, sri,
                   st->es_buf_a.sb_err);
          st->es_buf_a.sb_err = 0;
          sbuf_cut(&st->es_buf_a, i + fsize);
          goto again;
        }
      }
    }
  }
}

static int parse_mp4a(service_t *t, elementary_stream_t *st, size_t ilen,
                      uint32_t next_startcode, int sc_offset)
{
  int r;

  if((r = depacketize(t, st, ilen, next_startcode, sc_offset)) != 0)
    return r;

  parse_mp4a_data(t, st, 0);
  return 1;
}

const static int mpa_br[16] = {
    0,  32,  48,  56,
   64,  80,  96, 112, 
  128, 160, 192, 224,
  256, 320, 384, 0
};

const static int mpa_sr[4]  = {44100, 48000, 32000, 0};
const static int mpa_sri[4] = {4,     3,     5,     0};

static int
mpa_valid_frame(const uint8_t *buf)
{
  return buf[0] == 0xff && (buf[1] & 0xfe) == 0xfc;
}


/**
 *
 */
static int
parse_mpa2(service_t *t, elementary_stream_t *st)
{
  int i, len;
  const uint8_t *buf;

 again:
  buf = st->es_buf_a.sb_data;
  len = st->es_buf_a.sb_ptr;

  for(i = 0; i < len - 4; i++) {
    if(mpa_valid_frame(buf + i)) {
      int br = mpa_br[ buf[i+2] >> 4     ];
      int sr = mpa_sr[(buf[i+2] >> 2) & 3];
      int pad =       (buf[i+2] >> 1) & 1;

      if(br && sr) {
        int fsize = 144000 * br / sr + pad;
        int duration = 90000 * 1152 / sr;
        int64_t dts = st->es_curdts;
        int channels = (buf[i + 3] & 0xc0) == 0xc0 ? 1 : 2;
        if(dts == PTS_UNSET)
          dts = st->es_nextdts;

        if(dts != PTS_UNSET &&
           len >= i + fsize + 4 &&
           mpa_valid_frame(buf + i + fsize)) {

          makeapkt(t, st, buf + i, fsize, dts, duration,
                   channels, mpa_sri[(buf[i+2] >> 2) & 3],
                   st->es_buf_a.sb_err);
          st->es_buf_a.sb_err = 0;
          sbuf_cut(&st->es_buf_a, i + fsize);
          goto again;
        }
      }
    }
  }
  return 1;
}

/**
 *
 */
static int 
parse_mpa(service_t *t, elementary_stream_t *st, size_t ilen,
          uint32_t next_startcode, int sc_offset)
{
  int r;
  
  if((r = depacketize(t, st, ilen, next_startcode, sc_offset)) != 0)
    return r;
  return parse_mpa2(t, st);
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

/**
 * Inspect 6 bytes AC3 header
 */
static int
ac3_valid_frame(const uint8_t *buf)
{
  if(buf[0] != 0x0b || buf[1] != 0x77 || buf[5] >> 3 > 10)
    return 0;
  return (buf[4] & 0xc0) != 0xc0 && (buf[4] & 0x3f) < 0x26;
}

static const char acmodtab[8] = {2,1,2,3,3,4,4,5};


static int 
parse_ac3(service_t *t, elementary_stream_t *st, size_t ilen,
          uint32_t next_startcode, int sc_offset)
{
  int i, len;
  const uint8_t *buf;

  if((i = depacketize(t, st, ilen, next_startcode, sc_offset)) != 0)
    return i;

 again:
  buf = st->es_buf_a.sb_data;
  len = st->es_buf_a.sb_ptr;

  for(i = 0; i < len - 6; i++) {
    const uint8_t *p = buf + i;
    if(ac3_valid_frame(p)) {
      int bsid      = p[5] >> 3;
      int fscod     = p[4] >> 6;
      int frmsizcod = p[4] & 0x3f;
      int fsize     = ac3_frame_size_tab[frmsizcod][fscod] * 2;
      
      bsid -= 8;
      if(bsid < 0)
        bsid = 0;
      int sr = ac3_freq_tab[fscod] >> bsid;
      
      if(sr) {
        int duration = 90000 * 1536 / sr;
        int64_t dts = st->es_curdts;
        int sri = rate_to_sri(sr);

        if(dts == PTS_UNSET)
          dts = st->es_nextdts;

        if(dts != PTS_UNSET && len >= i + fsize + 6 &&
           ac3_valid_frame(p + fsize)) {

          bitstream_t bs;
          init_rbits(&bs, p + 5, (fsize - 5) * 8);

          read_bits(&bs, 5); // bsid
          read_bits(&bs, 3); // bsmod
          int acmod = read_bits(&bs, 3);

          if((acmod & 0x1) && (acmod != 0x1))
            read_bits(&bs, 2); // cmixlen
          if(acmod & 0x4)
            read_bits(&bs, 2); // surmixlev
          if(acmod == 0x2)
            read_bits(&bs, 2); // dsurmod

          int lfeon = read_bits(&bs, 1);
          int channels = acmodtab[acmod] + lfeon;

          makeapkt(t, st, p, fsize, dts, duration, channels, sri,
                   st->es_buf_a.sb_err);
          st->es_buf_a.sb_err = 0;
          sbuf_cut(&st->es_buf_a, i + fsize);
          goto again;
        }
      }
    }
  }
  return 1;
}


/**
 * EAC3 audio parser
 */


static int
eac3_valid_frame(const uint8_t *buf)
{
  if(buf[0] != 0x0b || buf[1] != 0x77 || buf[5] >> 3 <= 10)
    return 0;
  return (buf[4] & 0xc0) != 0xc0;
}

static int 
parse_eac3(service_t *t, elementary_stream_t *st, size_t ilen,
           uint32_t next_startcode, int sc_offset)
{
  int i, len;
  const uint8_t *buf;

  if((i = depacketize(t, st, ilen, next_startcode, sc_offset)) != 0)
    return i;

 again:
  buf = st->es_buf_a.sb_data;
  len = st->es_buf_a.sb_ptr;

  for(i = 0; i < len - 6; i++) {
    const uint8_t *p = buf + i;
    if(eac3_valid_frame(p)) {

      int fsize = ((((p[2] & 0x7) << 8) + p[3]) + 1) * 2;

      int sr = p[4] >> 6;
      int rate;
      if(sr == 3) {
        int sr2 = (p[4] >> 4) & 0x3;
        if(sr2 == 3)
          continue;
        rate = ac3_freq_tab[sr2] / 2;
      } else {
        rate = ac3_freq_tab[sr];
      }

      int64_t dts = st->es_curdts;
      int sri = rate_to_sri(rate);

      int acmod = (p[4] >> 1) & 0x7;
      int lfeon = p[4] & 1;

      int channels = acmodtab[acmod] + lfeon;
      int duration = 90000 * 1536 / rate;

      if(dts == PTS_UNSET)
        dts = st->es_nextdts;

      if(dts != PTS_UNSET && len >= i + fsize + 6 &&
         eac3_valid_frame(p + fsize)) {
        makeapkt(t, st, p, fsize, dts, duration, channels, sri,
                 st->es_buf_a.sb_err);
        st->es_buf_a.sb_err = 0;
        sbuf_cut(&st->es_buf_a, i + fsize);
        goto again;
      }
    }
  }
  return 1;
}


/**
 * PES header parser
 *
 * Extract DTS and PTS and update current values in stream
 */
static int
parse_pes_header(service_t *t, elementary_stream_t *st, 
                 const uint8_t *buf, size_t len)
{
  int64_t dts, pts, d;
  int hdr, flags, hlen;

  hdr   = getu8(buf, len);
  flags = getu8(buf, len);
  hlen  = getu8(buf, len);

  if(len < hlen || (hdr & 0xc0) != 0x80)
    goto err;

  if((flags & 0xc0) == 0xc0) {
    if(hlen < 10) 
      goto err;

    pts = getpts(buf);
    dts = getpts(buf + 5);

    d = (pts - dts) & PTS_MASK;
    if(d > 180000) // More than two seconds of PTS/DTS delta, probably corrupt
      pts = dts = PTS_UNSET;

  } else if((flags & 0xc0) == 0x80) {
    if(hlen < 5)
      goto err;

    dts = pts = getpts(buf);
  } else
    return hlen + 3;

  if(st->es_buf.sb_err) {
    st->es_curdts = PTS_UNSET;
    st->es_curpts = PTS_UNSET;
  } else {
    st->es_curdts = dts;
    st->es_curpts = pts;
  }
  return hlen + 3;

 err:
  st->es_curdts = PTS_UNSET;
  st->es_curpts = PTS_UNSET;
  if (tvhlog_limit(&st->es_pes_log, 10))
    tvhwarn("TS", "%s Corrupted PES header (errors %zi)",
            service_component_nicename(st), st->es_pes_log.count);
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
parse_mpeg2video_pic_start(service_t *t, elementary_stream_t *st, int *frametype,
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

  v = read_bits(bs, 16); /* vbv_delay */
  if(v == 0xffff)
    st->es_vbv_delay = -1;
  else
    st->es_vbv_delay = v;
  return 0;
}

/**
 *
 */
void
parser_set_stream_vparam(elementary_stream_t *st, int width, int height,
                         int duration)
{
  int need_save = 0;

  if(st->es_width == 0 && st->es_height == 0 && st->es_frame_duration == 0) {
    need_save = 1;
    st->es_meta_change = 0;

  } else if(st->es_width != width || st->es_height != height ||
            st->es_frame_duration != duration) {

    st->es_meta_change++;
    if(st->es_meta_change == 2)
      need_save = 1;

  } else {
    st->es_meta_change = 0;
  }

  if(need_save) {
    st->es_width = width;
    st->es_height = height;
    st->es_frame_duration = duration;
    service_request_save(st->es_service, 1);
  }
}


static const uint8_t mpeg2_aspect[16][2]={
    {0,1},
    {1,1},
    {4,3},
    {16,9},
    {221,100},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
};


/**
 * Parse mpeg2video sequence start
 */
static int
parse_mpeg2video_seq_start(service_t *t, elementary_stream_t *st,
                           bitstream_t *bs)
{
  int v, width, height, aspect;

  if(bs->len < 61)
    return 1;
  
  width = read_bits(bs, 12);
  height = read_bits(bs, 12);
  aspect = read_bits(bs, 4);

  st->es_aspect_num = mpeg2_aspect[aspect][0];
  st->es_aspect_den = mpeg2_aspect[aspect][1];

  int duration = mpeg2video_framedurations[read_bits(bs, 4)];

  v = read_bits(bs, 18) * 400;
  skip_bits(bs, 1);
  
  v = read_bits(bs, 10) * 16 * 1024 / 8;
  st->es_vbv_size = v;

  parser_set_stream_vparam(st, width, height, duration);
  return 0;
}


/**
 *
 */
static int
drop_trailing_zeroes(const uint8_t *buf, size_t len)
{
  while(len > 3 && (buf[len - 3] | buf[len - 2] | buf[len - 1]) == 0)
    len--;
  return len;
}


/**
 *
 */
static void
parser_global_data_move(elementary_stream_t *st, const uint8_t *data, size_t len)
{
  int len2 = drop_trailing_zeroes(data, len);

  st->es_global_data = realloc(st->es_global_data,
                               st->es_global_data_len + len2);
  memcpy(st->es_global_data + st->es_global_data_len, data, len2);
  st->es_global_data_len += len2;

  st->es_buf.sb_ptr -= len;
}


/**
 * MPEG2VIDEO specific reassembly
 *
 * Process all startcodes (also the system ones)
 *
 * Extract framerate (or duration to be more specific)
 *
 * 'steal' the st->es_buffer and use it as 'pkt' buffer
 *
 */
static int
parse_mpeg2video(service_t *t, elementary_stream_t *st, size_t len, 
                 uint32_t next_startcode, int sc_offset)
{
  const uint8_t *buf = st->es_buf.sb_data + sc_offset;
  bitstream_t bs;
  int frametype;

  if(next_startcode == 0x1e0)
    return 4;

  init_rbits(&bs, buf + 4, (len - 4) * 8);

  switch(st->es_startcode) {
  case 0x000001e0 ... 0x000001ef:
    /* System start codes for video */
    if(len < 9)
      return 1;

    parse_pes_header(t, st, buf + 6, len - 6);
    return 1;

  case 0x00000100:
    /* Picture start code */
    if(st->es_frame_duration == 0)
      return 1;

    if(parse_mpeg2video_pic_start(t, st, &frametype, &bs))
      return 1;

    if(st->es_curpkt != NULL)
      pkt_ref_dec(st->es_curpkt);

    st->es_curpkt = pkt_alloc(NULL, 0, st->es_curpts, st->es_curdts);
    st->es_curpkt->pkt_frametype = frametype;
    st->es_curpkt->pkt_duration = st->es_frame_duration;
    st->es_curpkt->pkt_commercial = t->s_tt_commercial_advice;

    /* If we know the frame duration, increase DTS accordingly */
    if(st->es_curdts != PTS_UNSET)
      st->es_curdts += st->es_frame_duration;

    /* PTS cannot be extrapolated (it's not linear) */
    st->es_curpts = PTS_UNSET; 

    break;

  case 0x000001b3:
    /* Sequence start code */
    if(!st->es_buf.sb_err) {
      if(parse_mpeg2video_seq_start(t, st, &bs))
        return 1;
      parser_global_data_move(st, buf, len);
    }
    return 2;

  case 0x000001b5:
    if(len < 5)
      return 1;
    switch(buf[4] >> 4) {
    case 0x1:
      // Sequence Extension
      if(!st->es_buf.sb_err)
        parser_global_data_move(st, buf, len);
      return 2;
    case 0x2:
      // Sequence Display Extension
      if(!st->es_buf.sb_err)
        parser_global_data_move(st, buf, len);
      return 2;
    }
    break;

  case 0x00000101 ... 0x000001af:
    /* Slices */

    if(next_startcode == 0x100 || next_startcode > 0x1af) {
      /* Last picture slice (because next not a slice) */
      th_pkt_t *pkt = st->es_curpkt;
      size_t metalen = 0;
      if(pkt == NULL) {
        /* no packet, may've been discarded by sanity checks here */
        return 1;
      }

      if(st->es_global_data) {
        pkt->pkt_meta = pktbuf_make(st->es_global_data,
                                    metalen = st->es_global_data_len);
        st->es_global_data = NULL;
        st->es_global_data_len = 0;
      }

      if (st->es_buf.sb_err) {
        pkt->pkt_err = 1;
        st->es_buf.sb_err = 0;
      }
      if (metalen) {
        pkt->pkt_payload = pktbuf_alloc(NULL, metalen + st->es_buf.sb_ptr - 4);
        memcpy(pktbuf_ptr(pkt->pkt_payload), pktbuf_ptr(pkt->pkt_meta), metalen);
        memcpy(pktbuf_ptr(pkt->pkt_payload) + metalen, st->es_buf.sb_data, st->es_buf.sb_ptr - 4);
        sbuf_reset(&st->es_buf, 16000);
      } else {
        pkt->pkt_payload = pktbuf_make(st->es_buf.sb_data,
                                       st->es_buf.sb_ptr - 4);
        sbuf_steal_data(&st->es_buf);
      }
      pkt->pkt_duration = st->es_frame_duration;

      parser_deliver(t, st, pkt);
      st->es_curpkt = NULL;

      return 1;
    }
    break;

  case 0x000001b8:
    // GOP header
    if(!st->es_buf.sb_err)
      parser_global_data_move(st, buf, len);
    return 2;

  case 0x000001b2:
    // User data
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
parse_h264(service_t *t, elementary_stream_t *st, size_t len, 
           uint32_t next_startcode, int sc_offset)
{
  const uint8_t *buf = st->es_buf.sb_data + sc_offset;
  uint32_t sc = st->es_startcode;
  int l2, pkttype, isfield;
  bitstream_t bs;
  int ret = 0;

  /* delimiter - finished frame */
  if ((sc & 0x1f) == 9 && st->es_curpkt && st->es_curpkt->pkt_payload) {
    if (st->es_curdts != PTS_UNSET && st->es_frame_duration) {
      parser_deliver(t, st, st->es_curpkt);
      st->es_curpkt = NULL;

      st->es_curdts += st->es_frame_duration;
      if (st->es_curpts != PTS_UNSET)
        st->es_curpts += st->es_frame_duration;
      st->es_prevdts = st->es_curdts;
    } else {
      pkt_ref_dec(st->es_curpkt);
      st->es_curpkt = NULL;
    }
    return 1;
  }

  if(sc >= 0x000001e0 && sc <= 0x000001ef) {
    /* System start codes for video */
    if(len >= 9) {
      uint16_t plen = buf[4] << 8 | buf[5];
      th_pkt_t *pkt = st->es_curpkt;
      if(plen >= 0xffe9) st->es_incomplete = 1;
      l2 = parse_pes_header(t, st, buf + 6, len - 6);

      if (pkt) {
        if (l2 + 1 <= len - 6) {
          /* This is the rest of this frame. */
          /* Do not include trailing zero. */
          pkt->pkt_payload = pktbuf_append(pkt->pkt_payload, buf + 6 + l2, len - 6 - l2 - 1);
        }

        parser_deliver(t, st, pkt);

        st->es_curpkt = NULL;
      }
    }
    st->es_prevdts = st->es_curdts;
    return 1;
  }
  
  if(sc == 0x10c) {
    // Padding

    st->es_buf.sb_ptr -= len;
    ret = 2;

  } else {

    switch(sc & 0x1f) {

    case 7:
      if(!st->es_buf.sb_err) {
        void *f = h264_nal_deescape(&bs, buf + 3, len - 3);
        h264_decode_seq_parameter_set(st, &bs);
        free(f);
        parser_global_data_move(st, buf, len);
      }
      ret = 2;
      break;

    case 8:
      if(!st->es_buf.sb_err) {
        void *f = h264_nal_deescape(&bs, buf + 3, len - 3);
        h264_decode_pic_parameter_set(st, &bs);
        free(f);
        parser_global_data_move(st, buf, len);
      }
      ret = 2;
      break;

    case 5: /* IDR+SLICE */
    case 1:
      l2 = len - 3 > 64 ? 64 : len - 3;
      void *f = h264_nal_deescape(&bs, buf + 3, l2);
      /* we just want the first stuff */

      if(h264_decode_slice_header(st, &bs, &pkttype, &isfield)) {
        free(f);
        return 1;
      }
      free(f);

      if(st->es_curpkt != NULL || st->es_frame_duration == 0)
        break;

      st->es_curpkt = pkt_alloc(NULL, 0, st->es_curpts, st->es_curdts);
      st->es_curpkt->pkt_frametype = pkttype;
      st->es_curpkt->pkt_field = isfield;
      st->es_curpkt->pkt_duration = st->es_frame_duration;
      st->es_curpkt->pkt_commercial = t->s_tt_commercial_advice;
      break;

    default:
      break;
    }
  }

  if((next_startcode >= 0x000001e0 && next_startcode <= 0x000001ef) ||
     (next_startcode & 0x1f) == 9) {
    /* Complete frame - new start code or delimiter */
    if (st->es_incomplete)
      return 4;
    th_pkt_t *pkt = st->es_curpkt;
    size_t metalen = 0;

    if(pkt != NULL) {
      if(st->es_global_data) {
        pkt->pkt_meta = pktbuf_make(st->es_global_data,
                                    metalen = st->es_global_data_len);
        st->es_global_data = NULL;
        st->es_global_data_len = 0;
      }

      if (st->es_buf.sb_err) {
        pkt->pkt_err = st->es_buf.sb_err;
        st->es_buf.sb_err = 0;
      }
      if (metalen) {
        pkt->pkt_payload = pktbuf_alloc(NULL, metalen + st->es_buf.sb_ptr - 4);
        memcpy(pktbuf_ptr(pkt->pkt_payload), pktbuf_ptr(pkt->pkt_meta), metalen);
        memcpy(pktbuf_ptr(pkt->pkt_payload) + metalen, st->es_buf.sb_data, st->es_buf.sb_ptr - 4);
        sbuf_reset(&st->es_buf, 16000);
      } else {
        pkt->pkt_payload = pktbuf_make(st->es_buf.sb_data,
                                       st->es_buf.sb_ptr - 4);
        sbuf_steal_data(&st->es_buf);
      }
    }
    return 1;
  }

  return ret;
}

/**
 * http://broadcasting.ru/pdf-standard-specifications/subtitling/dvb-sub/en300743.v1.2.1.pdf
 */
static void
parse_subtitles(service_t *t, elementary_stream_t *st, const uint8_t *data,
                int len, int start)
{
  th_pkt_t *pkt;
  int psize, hlen;
  const uint8_t *buf;
  const uint8_t *d;
  if(start) {
    /* Payload unit start */
    st->es_parser_state = 1;
    sbuf_reset(&st->es_buf, 4000);
  }

  if(st->es_parser_state == 0)
    return;

  sbuf_append(&st->es_buf, data, len);

  if(st->es_buf.sb_ptr < 6)
    return;
  d = st->es_buf.sb_data;
  uint32_t startcode = (d[0] << 24) | (d[1] << 16) | (d[2] << 8) | d[3];

  if(startcode == 0x1be) {
    st->es_parser_state = 0;
    return;
  }

  psize = d[4] << 8 | d[5];

  if(st->es_buf.sb_ptr != psize + 6)
    return;

  st->es_parser_state = 0;

  hlen = parse_pes_header(t, st, d + 6, st->es_buf.sb_ptr - 6);
  if(hlen < 0)
    return;

  psize -= hlen;
  buf = d + 6 + hlen;
  
  if(psize < 2 || buf[0] != 0x20 || buf[1] != 0x00)
    return;

  psize -= 2;
  buf += 2;

  if(psize >= 6) {

    // end_of_PES_data_field_marker
    if(buf[psize - 1] == 0xff) {
      pkt = pkt_alloc(buf, psize - 1, st->es_curpts, st->es_curdts);
      pkt->pkt_commercial = t->s_tt_commercial_advice;
      pkt->pkt_err = st->es_buf.sb_err;
      parser_deliver(t, st, pkt);
      sbuf_reset(&st->es_buf, 4000);
    }
  }
}

/**
 * Teletext parser
 */
static void
parse_teletext(service_t *t, elementary_stream_t *st, const uint8_t *data,
               int len, int start)
{
  th_pkt_t *pkt;
  int psize, hlen;
  const uint8_t *buf;
  const uint8_t *d;
  if(start) {
    st->es_parser_state = 1;
    st->es_parser_ptr = 0;
    sbuf_reset(&st->es_buf, 4000);
  }

  if(st->es_parser_state == 0)
    return;

  sbuf_append(&st->es_buf, data, len);

  if(st->es_buf.sb_ptr < 6)
    return;
  d = st->es_buf.sb_data;

  psize = d[4] << 8 | d[5];

  if(st->es_buf.sb_ptr != psize + 6)
    return;

  st->es_parser_state = 0;

  hlen = parse_pes_header(t, st, d + 6, st->es_buf.sb_ptr - 6);
  if(hlen < 0)
    return;

  psize -= hlen;
  buf = d + 6 + hlen;
  
  if(psize >= 46) {
    teletext_input((mpegts_service_t *)t, st, buf, psize);
    pkt = pkt_alloc(buf, psize, st->es_curpts, st->es_curdts);
    pkt->pkt_commercial = t->s_tt_commercial_advice;
    pkt->pkt_err = st->es_buf.sb_err;
    parser_deliver(t, st, pkt);
    sbuf_reset(&st->es_buf, 4000);
  }
}

/**
 *
 */
static void
parser_deliver(service_t *t, elementary_stream_t *st, th_pkt_t *pkt)
{
  if(SCT_ISAUDIO(st->es_type) && pkt->pkt_pts != PTS_UNSET &&
     (t->s_current_pts == PTS_UNSET ||
      pkt->pkt_pts > t->s_current_pts ||
      pkt->pkt_pts < t->s_current_pts - 180000))
    t->s_current_pts = pkt->pkt_pts;

  tvhtrace("parser",
           "pkt stream %2d %-12s type %c"
           " dts %10"PRId64" (%10"PRId64") pts %10"PRId64" (%10"PRId64")"
           " dur %10d len %10zu err %i",
           st->es_index,
           streaming_component_type2txt(st->es_type),
           pkt_frametype_to_char(pkt->pkt_frametype),
           ts_rescale(pkt->pkt_pts, 1000000),
           pkt->pkt_dts,
           ts_rescale(pkt->pkt_dts, 1000000),
           pkt->pkt_pts,
           pkt->pkt_duration,
           pktbuf_len(pkt->pkt_payload),
           pkt->pkt_err);

  pkt->pkt_aspect_num = st->es_aspect_num;
  pkt->pkt_aspect_den = st->es_aspect_den;

  //  avgstat_add(&st->es_rate, pkt->pkt_payloadlen, dispatch_clock);

  /**
   * Input is ok
   */
  if (pkt->pkt_payload) {
    service_set_streaming_status_flags(t, TSS_PACKETS);
    t->s_streaming_live |= TSS_LIVE;
  }

  /* Forward packet */
  pkt->pkt_componentindex = st->es_index;

  streaming_pad_deliver(&t->s_streaming_pad, streaming_msg_create_pkt(pkt));

  /* Decrease our own reference to the packet */
  pkt_ref_dec(pkt);

}

/**
 * Deliver errors
 */
static void
parser_deliver_error(service_t *t, elementary_stream_t *st)
{
  th_pkt_t *pkt;

  if (!st->es_buf.sb_err)
    return;
  pkt = pkt_alloc(NULL, 0, PTS_UNSET, PTS_UNSET);
  pkt->pkt_err = st->es_buf.sb_err;
  parser_deliver(t, st, pkt);
  st->es_buf.sb_err = 0;
}
