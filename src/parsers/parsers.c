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
#include "parser_avc.h"
#include "parser_hevc.h"
#include "parser_latm.h"
#include "parser_teletext.h"
#include "bitstream.h"
#include "packet.h"
#include "streaming.h"
#include "config.h"

/* parser states */
#define PARSER_APPEND 0
#define PARSER_RESET  1
#define PARSER_DROP   2
#define PARSER_SKIP   3
#define PARSER_HEADER 4

/* backlog special mask */
#define PTS_BACKLOG (PTS_MASK + 1)

static inline int
is_ssc(uint32_t sc)
{
  return (sc & ~0x0f) == 0x1e0;
}

static inline int
pts_is_backlog(int64_t pts)
{
  return pts != PTS_UNSET && (pts & PTS_BACKLOG) != 0;
}

static inline int64_t
pts_no_backlog(int64_t pts)
{
  return pts_is_backlog(pts) ? (pts & ~PTS_BACKLOG) : pts;
}

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

static int parse_hevc(service_t *t, elementary_stream_t *st, size_t len,
                      uint32_t next_startcode, int sc_offset);

typedef int (packet_parser_t)(service_t *t, elementary_stream_t *st, size_t len,
                              uint32_t next_startcode, int sc_offset);

typedef void (aparser_t)(service_t *t, elementary_stream_t *st, th_pkt_t *pkt);

static void parse_pes(service_t *t, elementary_stream_t *st, const uint8_t *data,
                      int len, int start, packet_parser_t *vp);

static void parse_mp4a_data(service_t *t, elementary_stream_t *st, int skip_next_check);

static void parse_aac(service_t *t, elementary_stream_t *st, const uint8_t *data,
                      int len, int start);

static void parse_subtitles(service_t *t, elementary_stream_t *st, 
                            const uint8_t *data, int len, int start);

static void parse_teletext(service_t *t, elementary_stream_t *st, 
                           const uint8_t *data, int len, int start);

static int parse_mpa(service_t *t, elementary_stream_t *st, size_t len,
                     uint32_t next_startcode, int sc_offset);

static int parse_mpa123(service_t *t, elementary_stream_t *st);

static int parse_ac3(service_t *t, elementary_stream_t *st, size_t len,
                     uint32_t next_startcode, int sc_offset);

static int parse_eac3(service_t *t, elementary_stream_t *st, size_t len,
                      uint32_t next_startcode, int sc_offset);

static void parser_deliver(service_t *t, elementary_stream_t *st, th_pkt_t *pkt);

static void parser_deliver_error(service_t *t, elementary_stream_t *st);

static int parse_pes_header(service_t *t, elementary_stream_t *st,
                            const uint8_t *buf, size_t len);

static void parser_backlog(service_t *t, elementary_stream_t *st, th_pkt_t *pkt);

static void parser_do_backlog(service_t *t, elementary_stream_t *st,
                  void (*pkt_cb)(service_t *t, elementary_stream_t *st, th_pkt_t *pkt),
                  pktbuf_t *meta);

/**
 * for debugging
 */
#if 0
static int data_noise(const uint8_t *data, int len)
{
  static uint64_t off = 0, win = 4096, limit = 1024*1024;
  uint32_t i;
  off += len;
  if (off >= limit && off < limit + win) {
    if ((off & 3) == 1)
      return 1;
    for (i = 0; i < len; i += 3)
      ((char *)data)[i] ^= 0xa5;
  } else if (off >= limit + win) {
    off = 0;
    limit = (uint64_t)data[15] * 4 * 1024;
    win   = (uint64_t)data[16] * 4;
  }
  return 0;
}
#else
static inline int data_noise(const uint8_t *data, int len) { return 0; }
#endif

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

  if (data_noise(data, len))
    return;

  switch(st->es_type) {
  case SCT_MPEG2VIDEO:
    parse_pes(t, st, data, len, start, parse_mpeg2video);
    break;

  case SCT_H264:
    parse_pes(t, st, data, len, start, parse_h264);
    break;

  case SCT_HEVC:
    parse_pes(t, st, data, len, start, parse_hevc);
    break;

  case SCT_MPEG2AUDIO:
    parse_pes(t, st, data, len, start, parse_mpa);
    break;

  case SCT_AC3:
    parse_pes(t, st, data, len, start, parse_ac3);
    break;

  case SCT_EAC3:
    parse_pes(t, st, data, len, start, parse_eac3);
    break;

  case SCT_DVBSUB:
    parse_subtitles(t, st, data, len, start);
    break;
    
  case SCT_AAC:
  case SCT_MP4A:
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
 * Skip raw mpeg data
 */
void
skip_mpeg_ts(service_t *t, elementary_stream_t *st, const uint8_t *data, int len)
{
  if(len >= 188)
    sbuf_err(&st->es_buf, len / 188);
  if(st->es_buf.sb_err > 1000) {
    parser_deliver_error(t, st);
    service_send_streaming_status((service_t *)t);
  }
}

/**
 * Parse AAC LATM and ADTS
 */
static void 
parse_aac(service_t *t, elementary_stream_t *st, const uint8_t *data,
          int len, int start)
{
  int l, muxlen, p, latm;
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

    if((data[0] != 0 && data[1] != 0 && data[2] != 1) ||
       (data[3] != 0xbd && (data[3] & ~0x1f) != 0xc0)) { /* startcode */
      sbuf_reset(&st->es_buf, 4000);
      return;
    }

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
  latm = -1;

  while((l = st->es_buf.sb_ptr - p) > 3) {
    const uint8_t *d = st->es_buf.sb_data + p;
    /* Startcode check */
    if(d[0] == 0 && d[1] == 0 && d[2] == 1) {
      p += 4;
    /* LATM */
    } else if(latm != 0 && d[0] == 0x56 && (d[1] & 0xe0) == 0xe0) {
      muxlen = (d[1] & 0x1f) << 8 | d[2];

      if(l < muxlen + 3)
        break;

      latm = 1;
      pkt = parse_latm_audio_mux_element(t, st, d + 3, muxlen);

      if(pkt != NULL) {
        pkt->pkt_err = st->es_buf.sb_err;
        parser_deliver(t, st, pkt);
        st->es_buf.sb_err = 0;
      }

      p += muxlen + 3;
    /* ADTS */
    } else if(latm <= 0 && d[0] == 0xff && (d[1] & 0xf0) == 0xf0) {

      if (l < 7)
        break;

      muxlen = ((uint16_t)(d[3] & 0x03) << 11) | ((uint16_t)d[4] << 3) | (d[5] >> 5);
      if (l < muxlen)
        break;

      if (muxlen < 7) {
        p++;
        continue;
      }

      latm = 0;
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
 * Generic PES parser
 *
 * We scan for startcodes a'la 0x000001xx and let a specific parser
 * derive further information.
 */
static void
parse_pes(service_t *t, elementary_stream_t *st, const uint8_t *data, int len,
          int start, packet_parser_t *vp)
{
  uint_fast32_t sc = st->es_startcond;
  uint16_t plen;
  int i, j, r, hlen, tmp, off;

  if (start) {
    st->es_parser_state = 1;
    if (data[0] != 0 && data[1] != 0 && data[2] != 1)
      if (tvhlog_limit(&st->es_pes_log, 10))
        tvhwarn("TS", "%s: Invalid start code %02x:%02x:%02x",
                service_component_nicename(st),
                data[0], data[1], data[2]);
    st->es_incomplete = 0;
    if (st->es_header_mode) {
      st->es_buf.sb_ptr = st->es_header_offset;
      st->es_header_mode = 0;
    }
  }

  if(st->es_parser_state == 0)
    return;

  for(i = 0; i < len; ) {

    if(st->es_header_mode) {
      r = len - i;
      off = st->es_header_offset;
      j = st->es_buf.sb_ptr - off;
      if (j < 5) {
        tmp = MIN(r, 5 - j);
        sbuf_append(&st->es_buf, data + i, tmp);
        j += tmp;
        r -= tmp;
        i += tmp;
      }
      if (j < 5)
        break;

      plen = st->es_buf.sb_data[off] << 8 | st->es_buf.sb_data[off+1];
      st->es_incomplete = plen >= 0xffdf;
      hlen = st->es_buf.sb_data[off+4];

      if(j < hlen + 5) {
        tmp = MIN(r, hlen);
        sbuf_append(&st->es_buf, data + i, tmp);
        j += tmp;
        r -= tmp;
        i += tmp;
      }
      if(j < hlen + 5)
        break;
      
      parse_pes_header(t, st, st->es_buf.sb_data + off + 2, hlen + 3);

      st->es_header_mode = 0;
      st->es_buf.sb_ptr = off;
      if(off > 2)
        sc = st->es_buf.sb_data[off-3] << 16 |
             st->es_buf.sb_data[off-2] << 8 |
             st->es_buf.sb_data[off-1];

      continue;
    }

    /* quick loop to find startcode */
    j = i;
    do {
      sc = (sc << 8) | data[i++];
      if((sc & 0xffffff00) == 0x00000100)
        goto found;
    } while (i < len);
    sbuf_append(&st->es_buf, data + j, len - j);
    break;

found:
    sbuf_append(&st->es_buf, data + j, i - j);

    if(sc == 0x100 && (len-i)>2) {
      if (data[0] == 0 && data[i+1] == 0x01 && data[i+2] == 0xe0)
        continue;
    }

    r = st->es_buf.sb_ptr - st->es_startcode_offset - 4;
    if(r > 0 && st->es_startcode != 0) {
      r = vp(t, st, r, sc, st->es_startcode_offset);
      
      if(r == PARSER_SKIP)
        continue;

      if(r == PARSER_HEADER) {
        st->es_buf.sb_ptr -= 4;
        st->es_header_mode = 1;
        st->es_header_offset = st->es_buf.sb_ptr;
        sc = -1;
        continue;
      }
    } else
      r = PARSER_RESET;

    if(r == PARSER_DROP) {

      assert(st->es_buf.sb_data != NULL);

      /* Drop packet */
      st->es_buf.sb_ptr = st->es_startcode_offset;
      sbuf_put_be32(&st->es_buf, sc);
      st->es_startcode = sc;

    } else /* APPEND or RESET */ {

      if(r == PARSER_RESET) {
        /* Reset packet parser upon length error or if parser
           tells us so */
        parser_deliver_error(t, st);
        sbuf_reset_and_alloc(&st->es_buf, 256);
        sbuf_put_be32(&st->es_buf, sc);
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
    return PARSER_RESET;

  plen = (buf[4] << 8) | buf[5];

  if(plen + 6 > len)
    return PARSER_SKIP;

  if(plen + 6 < len)
    return PARSER_RESET;

  buf += 6;
  len -= 6;

  hlen = parse_pes_header(t, st, buf, len);

  if(hlen < 0)
    return PARSER_RESET;

  buf += hlen;
  len -= hlen;

  st->es_buf_a.sb_err = st->es_buf.sb_err;

  sbuf_append(&st->es_buf_a, buf, len);
  return PARSER_APPEND;
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

const static int mpa_br[2][3][16] = {
{
  {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
  {0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
  {0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0},
},
{
  {0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
  {0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0},
  {0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0},
}
};

const static int mpa_sr[4]  = {44100, 48000, 32000, 0};
const static int mpa_sri[4] = {4,     3,     5,     0};

static inline int
mpa_valid_frame(uint32_t h)
{
  if ((h & 0xffe00000) != 0xffe00000) return 0; /* start bits */
  if ((h & (3<<17)) == 0) return 0;             /* unknown MPEG audio layer */
  if ((h & (15<<12)) == (15<<12)) return 0;     /* invalid bitrate */
  if ((h & (3<<10)) == (3<<10)) return 0;             /* invalid frequency */
  return 1;
}

/**
 *
 */
static int
parse_mpa123(service_t *t, elementary_stream_t *st)
{
  int i, len, layer, lsf, mpeg25, br, sr, pad, fsize, channels, duration;
  int64_t dts;
  uint32_t h;
  const uint8_t *buf;

 again:
  buf = st->es_buf_a.sb_data;
  len = st->es_buf_a.sb_ptr;

  for (i = 0; i < len - 4; i++) {
    if (!mpa_valid_frame(h = RB32(buf + i))) continue;

    layer = 4 - ((h >> 17) & 3);
    lsf = mpeg25 = 1;
    pad = (h >> 9) & 1;
    if (h & (1<<20)) {
      lsf = ((h >> 19) & 1) ^ 1;
      mpeg25 = 0;
    }
    sr = mpa_sr[(h >> 10) & 3] >> (lsf + mpeg25);
    br = (h >> 12) & 0xf;

    if (br == 0 || sr == 0) continue;

    br = mpa_br[lsf][layer - 1][br];
    if (br == 0) continue;

    switch (layer) {
    case 1:  fsize = (((br * 12000) / sr) + pad) * 4; break;
    case 2:  fsize = ((br * 144000) / sr) + pad; break;
    case 3:  fsize = ((br * 144000) / (sr << lsf)) + pad; break;
    default: fsize = 0;
    }

    duration = 90000 * 1152 / sr;
    channels = ((h >> 6) & 3) == 3 ? 1 : 2;
    dts = st->es_curdts;
    if (dts == PTS_UNSET) {
      dts = st->es_nextdts;
      if(dts == PTS_UNSET) continue;
    }

    if(len < i + fsize + 4) continue;

    h = RB32(buf + i + fsize);
    if(mpa_valid_frame(h)) {
      makeapkt(t, st, buf + i, fsize, dts, duration,
               channels, mpa_sri[(buf[i+2] >> 2) & 3],
               st->es_buf_a.sb_err);
      st->es_buf_a.sb_err = 0;
      sbuf_cut(&st->es_buf_a, i + fsize);
      goto again;
    }
  }

  return PARSER_RESET;
}

/**
 *
 */
static int 
parse_mpa(service_t *t, elementary_stream_t *st, size_t ilen,
          uint32_t next_startcode, int sc_offset)
{
  int r;
  
  if((r = depacketize(t, st, ilen, next_startcode, sc_offset)) != PARSER_APPEND)
    return r;
  return parse_mpa123(t, st);
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

  if((i = depacketize(t, st, ilen, next_startcode, sc_offset)) != PARSER_APPEND)
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
  return PARSER_RESET;
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

  if((i = depacketize(t, st, ilen, next_startcode, sc_offset)) != PARSER_APPEND)
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
  return PARSER_RESET;
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

  if (len < 3)
    return -1;

  hdr   = buf[0];
  flags = buf[1];
  hlen  = buf[2];
  buf  += 3;
  len  -= 3;

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
 *
 * MPEG2 video parser
 *
 */

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
  int pct;

  if(bs->len < 29)
    return PARSER_RESET;

  skip_bits(bs, 10); /* temporal reference */

  pct = read_bits(bs, 3);
  if(pct < PKT_I_FRAME || pct > PKT_B_FRAME)
    return PARSER_RESET; /* Illegal picture_coding_type */

  *frametype = pct;

#if 0
  int v = read_bits(bs, 16); /* vbv_delay */
  if(v == 0xffff)
    st->es_vbv_delay = -1;
  else
    st->es_vbv_delay = v;
#endif
  return PARSER_APPEND;
}

/**
 *
 */
void
parser_set_stream_vparam(elementary_stream_t *st, int width, int height,
                         int duration)
{
  int need_save = 0;

  if(st->es_width == 0 && st->es_height == 0 && st->es_frame_duration < 2) {
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
  int width, height, aspect, duration;

  if(bs->len < 61)
    return PARSER_RESET;
  
  width = read_bits(bs, 12);
  height = read_bits(bs, 12);
  aspect = read_bits(bs, 4);

  st->es_aspect_num = mpeg2_aspect[aspect][0];
  st->es_aspect_den = mpeg2_aspect[aspect][1];

  duration = mpeg2video_framedurations[read_bits(bs, 4)];

  skip_bits(bs, 18);
  skip_bits(bs, 1);
  
#if 0
  int v = read_bits(bs, 10) * 16 * 1024 / 8;
  st->es_vbv_size = v;
#endif

  parser_set_stream_vparam(st, width, height, duration);
  return PARSER_APPEND;
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
  th_pkt_t *pkt;

  if(next_startcode == 0x1e0)
    return PARSER_HEADER;

  init_rbits(&bs, buf + 4, (len - 4) * 8);

  switch(st->es_startcode & 0xff) {
  case 0xe0 ... 0xef:
    /* System start codes for video */
    if(len < 9)
      return PARSER_RESET;

    parse_pes_header(t, st, buf + 6, len - 6);
    return PARSER_RESET;

  case 0x00:
    /* Picture start code */
    if(st->es_frame_duration == 0)
      return PARSER_RESET;

    if(parse_mpeg2video_pic_start(t, st, &frametype, &bs) != PARSER_APPEND)
      return PARSER_RESET;

    if(st->es_curpkt != NULL)
      pkt_ref_dec(st->es_curpkt);

    pkt = pkt_alloc(NULL, 0, st->es_curpts, st->es_curdts);
    pkt->pkt_frametype = frametype;
    pkt->pkt_duration = st->es_frame_duration;
    pkt->pkt_commercial = t->s_tt_commercial_advice;

    st->es_curpkt = pkt;

    /* If we know the frame duration, increase DTS accordingly */
    if(st->es_curdts != PTS_UNSET)
      st->es_curdts += st->es_frame_duration;

    /* PTS cannot be extrapolated (it's not linear) */
    st->es_curpts = PTS_UNSET; 

    break;

  case 0xb3:
    /* Sequence start code */
    if(!st->es_buf.sb_err) {
      if(parse_mpeg2video_seq_start(t, st, &bs) != PARSER_APPEND)
        return PARSER_RESET;
      parser_global_data_move(st, buf, len);
      if (!st->es_priv)
        st->es_priv = malloc(1); /* starting mark */
    }
    return PARSER_DROP;

  case 0xb5:
    if(len < 5)
      return PARSER_RESET;
    switch(buf[4] >> 4) {
    case 0x1:
      // Sequence Extension
      if(!st->es_buf.sb_err)
        parser_global_data_move(st, buf, len);
      return PARSER_DROP;
    case 0x2:
      // Sequence Display Extension
      if(!st->es_buf.sb_err)
        parser_global_data_move(st, buf, len);
      return PARSER_DROP;
    }
    break;

  case 0x01 ... 0xaf:
    /* Slices */

    if(next_startcode == 0x100 || next_startcode > 0x1af) {
      /* Last picture slice (because next not a slice) */
      th_pkt_t *pkt = st->es_curpkt;
      size_t metalen = 0;
      if(pkt == NULL) {
        /* no packet, may've been discarded by sanity checks here */
        return PARSER_RESET;
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
      } else {
        pkt->pkt_payload = pktbuf_make(st->es_buf.sb_data,
                                       st->es_buf.sb_ptr - 4);
        sbuf_steal_data(&st->es_buf);
      }
      pkt->pkt_duration = st->es_frame_duration;

      if (st->es_priv) {
        if (!TAILQ_EMPTY(&st->es_backlog))
          parser_do_backlog(t, st, NULL, pkt ? pkt->pkt_meta : NULL);
        parser_deliver(t, st, pkt);
      } else if (config.parser_backlog)
        parser_backlog(t, st, pkt);
      st->es_curpkt = NULL;

      return PARSER_RESET;
    }
    break;

  case 0xb8:
    // GOP header
    if(!st->es_buf.sb_err)
      parser_global_data_move(st, buf, len);
    return PARSER_DROP;

  case 0xb2:
    // User data
    break;

  default:
    break;
  }

  return PARSER_APPEND;
}


/**
 *
 * H.264 (AVC) parser
 *
 */
static void
parse_h264_backlog(service_t *t, elementary_stream_t *st, th_pkt_t *pkt)
{
  int len, l2, pkttype = 0, isfield = 0, nal_type;
  const uint8_t *end, *nal_start, *nal_end;
  bitstream_t bs;
  void *f;

  if (pkt->pkt_payload == NULL)
    return;
  len = pktbuf_len(pkt->pkt_payload);
  if (len <= 1)
    return;
  nal_start = pktbuf_ptr(pkt->pkt_payload);
  end = nal_start + len;
  while (1) {
    while (nal_start < end && !*(nal_start++));
    if (nal_start == end)
      break;
    nal_end = avc_find_startcode(nal_start, end);
    len = nal_end - nal_start;
    if (len > 3) {
      nal_type = nal_start[0] & 0x1f;
      if (nal_type == H264_NAL_IDR_SLICE || nal_type == H264_NAL_SLICE) {
        l2 = len - 1 > 64 ? 64 : len - 1;
        f = h264_nal_deescape(&bs, nal_start + 1, l2);
        if(h264_decode_slice_header(st, &bs, &pkttype, &isfield))
          pkttype = isfield = 0;
        free(f);
        if (pkttype > 0)
          break;
      }
    }
    nal_start = nal_end;
  }

  pkt->pkt_frametype = pkttype;
  pkt->pkt_field = isfield;
  pkt->pkt_duration = st->es_frame_duration;
}

static void
parse_h264_deliver(service_t *t, elementary_stream_t *st, th_pkt_t *pkt)
{
  if (pkt->pkt_frametype > 0) {
    if (TAILQ_EMPTY(&st->es_backlog)) {
      if (pkt->pkt_frametype > 0) {
deliver:
        parser_deliver(t, st, pkt);
        return;
      }
    } else if (pkt->pkt_frametype > 0 &&
         (st->es_curdts != PTS_UNSET && (st->es_curdts & PTS_BACKLOG) == 0) &&
         (st->es_curpts != PTS_UNSET && (st->es_curpts & PTS_BACKLOG) == 0) &&
         st->es_frame_duration > 1) {
      parser_do_backlog(t, st, parse_h264_backlog, pkt ? pkt->pkt_meta : NULL);
      goto deliver;
    }
  }
  if (config.parser_backlog)
    parser_backlog(t, st, pkt);
}

static int
parse_h264(service_t *t, elementary_stream_t *st, size_t len, 
           uint32_t next_startcode, int sc_offset)
{
  const uint8_t *buf = st->es_buf.sb_data + sc_offset;
  uint32_t sc = st->es_startcode;
  int l2, pkttype, isfield;
  bitstream_t bs;
  int ret = PARSER_APPEND;
  th_pkt_t *pkt = st->es_curpkt;

  /* delimiter - finished frame */
  if ((sc & 0x1f) == H264_NAL_AUD && pkt) {
    if (pkt->pkt_payload == NULL || pkt->pkt_dts == PTS_UNSET) {
      pkt_ref_dec(st->es_curpkt);
      st->es_curpkt = NULL;
    } else {
      parse_h264_deliver(t, st, pkt);
      st->es_curpkt = NULL;
      st->es_curdts += st->es_frame_duration;
      if (st->es_frame_duration < 2)
        st->es_curdts |= PTS_BACKLOG;
      if (st->es_curpts != PTS_UNSET) {
        st->es_curpts += st->es_frame_duration;
        if (st->es_frame_duration < 2)
          st->es_curpts |= PTS_BACKLOG;
      }
      st->es_prevdts = st->es_curdts;
      return PARSER_RESET;
    }
  }

  if(is_ssc(sc)) {
    /* System start codes for video */
    if(len >= 9) {
      uint16_t plen = buf[4] << 8 | buf[5];
      pkt = st->es_curpkt;
      if (plen >= 0xffe9) st->es_incomplete = 1;
      l2 = parse_pes_header(t, st, buf + 6, len - 6);
      if (l2 < 0)
        return PARSER_DROP;

      if (pkt) {
        if (l2 < len - 6 && l2 + 7 != len) {
          /* This is the rest of this frame. */
          /* Do not include trailing zero. */
          pkt->pkt_payload = pktbuf_append(pkt->pkt_payload, buf + 6 + l2, len - 6 - l2);
        }

        if (is_ssc(next_startcode))
          return PARSER_RESET;

        if (pkt->pkt_payload == NULL || pkt->pkt_dts == PTS_UNSET) {
          pkt_ref_dec(pkt);
        } else {
          parse_h264_deliver(t, st, pkt);
        }

        st->es_curpkt = NULL;
      }
    }
    st->es_prevdts = st->es_curdts;
    return PARSER_RESET;
  }
  
  if(sc == 0x10c) {
    // Padding

    st->es_buf.sb_ptr -= len;
    ret = PARSER_DROP;

  } else {

    switch(sc & 0x1f) {

    case H264_NAL_SPS:
      if(!st->es_buf.sb_err) {
        void *f = h264_nal_deescape(&bs, buf + 4, len - 4);
        h264_decode_seq_parameter_set(st, &bs);
        free(f);
        parser_global_data_move(st, buf, len);
      }
      ret = PARSER_DROP;
      break;

    case H264_NAL_PPS:
      if(!st->es_buf.sb_err) {
        void *f = h264_nal_deescape(&bs, buf + 4, len - 4);
        h264_decode_pic_parameter_set(st, &bs);
        free(f);
        parser_global_data_move(st, buf, len);
      }
      ret = PARSER_DROP;
      break;

    case H264_NAL_IDR_SLICE:
    case H264_NAL_SLICE:
      if (st->es_curpkt != NULL)
        break;

      /* we just want the first stuff */
      l2 = len - 4 > 64 ? 64 : len - 4;
      void *f = h264_nal_deescape(&bs, buf + 4, l2);
      if(h264_decode_slice_header(st, &bs, &pkttype, &isfield))
        pkttype = isfield = 0;
      free(f);

      if (st->es_frame_duration == 0)
        st->es_frame_duration = 1;

      pkt = pkt_alloc(NULL, 0, st->es_curpts, st->es_curdts);
      pkt->pkt_frametype = pkttype;
      pkt->pkt_field = isfield;
      pkt->pkt_duration = st->es_frame_duration;
      pkt->pkt_commercial = t->s_tt_commercial_advice;
      st->es_curpkt = pkt;
      break;

    default:
      break;
    }
  }

  if(is_ssc(next_startcode) || (next_startcode & 0x1f) == H264_NAL_AUD) {
    /* Complete frame - new start code or delimiter */
    if (st->es_incomplete)
      return PARSER_HEADER;
    pkt = st->es_curpkt;
    size_t metalen = 0;

    if(pkt != NULL && pkt->pkt_payload == NULL) {
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
      } else {
        pkt->pkt_payload = pktbuf_make(st->es_buf.sb_data,
                                       st->es_buf.sb_ptr - 4);
        sbuf_steal_data(&st->es_buf);
      }
    }
    return PARSER_RESET;
  }

  return ret;
}

/**
 *
 * H.265 (HEVC) parser
 *
 */
static int
parse_hevc(service_t *t, elementary_stream_t *st, size_t len,
           uint32_t next_startcode, int sc_offset)
{
  const uint8_t *buf = st->es_buf.sb_data + sc_offset;
  uint32_t sc = st->es_startcode;
  int nal_type = (sc >> 1) & 0x3f;
  int l2, pkttype, r;
  bitstream_t bs;
  int ret = PARSER_APPEND;

  if(is_ssc(sc)) {
    /* System start codes for video */
    if(len >= 9) {
      uint16_t plen = buf[4] << 8 | buf[5];
      th_pkt_t *pkt = st->es_curpkt;
      if (plen >= 0xffe9) st->es_incomplete = 1;
      l2 = parse_pes_header(t, st, buf + 6, len - 6);
      if (l2 < 0)
        return PARSER_DROP;

      if (pkt) {
        if (l2 < len - 6 && l2 + 7 != len) {
          /* This is the rest of this frame. */
          /* Do not include trailing zero. */
          pkt->pkt_payload = pktbuf_append(pkt->pkt_payload, buf + 6 + l2, len - 6 - l2);
        }

        if (is_ssc(next_startcode))
          return PARSER_RESET;

        parser_deliver(t, st, pkt);

        st->es_curpkt = NULL;
      }
    }
    st->es_prevdts = st->es_curdts;
    return PARSER_RESET;
  }

  if (sc & 0x80)
    return PARSER_DROP;

  switch (nal_type) {
  case HEVC_NAL_TRAIL_N:
  case HEVC_NAL_TRAIL_R:
  case HEVC_NAL_TSA_N:
  case HEVC_NAL_TSA_R:
  case HEVC_NAL_STSA_N:
  case HEVC_NAL_STSA_R:
  case HEVC_NAL_RADL_N:
  case HEVC_NAL_RADL_R:
  case HEVC_NAL_RASL_N:
  case HEVC_NAL_RASL_R:
  case HEVC_NAL_BLA_W_LP:
  case HEVC_NAL_BLA_W_RADL:
  case HEVC_NAL_BLA_N_LP:
  case HEVC_NAL_IDR_W_RADL:
  case HEVC_NAL_IDR_N_LP:
  case HEVC_NAL_CRA_NUT:
    l2 = len - 3 > 64 ? 64 : len - 3;
    void *f = h264_nal_deescape(&bs, buf + 3, len - 3);
    r = hevc_decode_slice_header(st, &bs, &pkttype);
    free(f);
    if (r < 0)
      return PARSER_RESET;
    if (r > 0)
      return PARSER_APPEND;

    st->es_curpkt = pkt_alloc(NULL, 0, st->es_curpts, st->es_curdts);
    st->es_curpkt->pkt_frametype = pkttype;
    st->es_curpkt->pkt_field = 0;
    st->es_curpkt->pkt_duration = st->es_frame_duration;
    st->es_curpkt->pkt_commercial = t->s_tt_commercial_advice;
    break;

  case HEVC_NAL_VPS:
    if(!st->es_buf.sb_err) {
      void *f = h264_nal_deescape(&bs, buf + 3, len - 3);
      hevc_decode_vps(st, &bs);
      free(f);
      parser_global_data_move(st, buf, len);
    }
    ret = PARSER_DROP;
    break;

  case HEVC_NAL_SPS:
    if(!st->es_buf.sb_err) {
      void *f = h264_nal_deescape(&bs, buf + 3, len - 3);
      hevc_decode_sps(st, &bs);
      free(f);
      parser_global_data_move(st, buf, len);
    }
    ret = PARSER_DROP;
    break;

  case HEVC_NAL_PPS:
    if(!st->es_buf.sb_err) {
      void *f = h264_nal_deescape(&bs, buf + 3, len - 3);
      hevc_decode_pps(st, &bs);
      free(f);
      parser_global_data_move(st, buf, len);
    }
    ret = PARSER_DROP;
    break;

#if 0
  case HEVC_NAL_SEI_PREFIX:
  case HEVC_NAL_SEI_SUFFIX:
    if(!st->es_buf.sb_err) {
      /* FIXME: only declarative messages */
      parser_global_data_move(st, buf, len);
    }
    ret = PARSER_DROP;
    break;
#endif

  default:
    break;
  }

  if(is_ssc(next_startcode) ||
     ((next_startcode >> 1) & 0x3f) == HEVC_NAL_TRAIL_R) {
    /* Complete frame - new start code or delimiter */
    if (st->es_incomplete)
      return PARSER_HEADER;
    th_pkt_t *pkt = st->es_curpkt;
    size_t metalen = 0;

    if(pkt != NULL && pkt->pkt_payload == NULL) {
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
      } else {
        pkt->pkt_payload = pktbuf_make(st->es_buf.sb_data,
                                       st->es_buf.sb_ptr - 4);
        sbuf_steal_data(&st->es_buf);
      }
    }
    return PARSER_RESET;
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
           ts_rescale(pkt->pkt_dts, 1000000),
           pkt->pkt_dts,
           ts_rescale(pkt->pkt_pts, 1000000),
           pkt->pkt_pts,
           pkt->pkt_duration,
           pktbuf_len(pkt->pkt_payload),
           pkt->pkt_err);

  pkt->pkt_aspect_num = st->es_aspect_num;
  pkt->pkt_aspect_den = st->es_aspect_den;

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

/**
 * Do backlog (fix DTS & PTS and reparse slices - frame type, field etc.)
 */
static void
parser_backlog(service_t *t, elementary_stream_t *st, th_pkt_t *pkt)
{
  streaming_message_t *sm = streaming_msg_create_pkt(pkt);
  TAILQ_INSERT_TAIL(&st->es_backlog, sm, sm_link);
  pkt_ref_dec(pkt); /* streaming_msg_create_pkt increses ref counter */

#if ENABLE_TRACE
  int64_t dts = pts_no_backlog(pkt->pkt_dts);
  int64_t pts = pts_no_backlog(pkt->pkt_pts);
  tvhtrace("parser",
           "pkt bcklog %2d %-12s type %c"
           " dts%s%10"PRId64" (%10"PRId64") pts%s%10"PRId64" (%10"PRId64")"
           " dur %10d len %10zu err %i",
           st->es_index,
           streaming_component_type2txt(st->es_type),
           pkt_frametype_to_char(pkt->pkt_frametype),
           pts_is_backlog(pkt->pkt_dts) ? "+" : " ",
           ts_rescale(dts, 1000000),
           dts,
           pts_is_backlog(pkt->pkt_pts) ? "+" : " ",
           ts_rescale(pts, 1000000),
           pts,
           pkt->pkt_duration,
           pktbuf_len(pkt->pkt_payload),
           pkt->pkt_err);
#endif
}

static void
parser_do_backlog(service_t *t, elementary_stream_t *st,
                  void (*pkt_cb)(service_t *t, elementary_stream_t *st, th_pkt_t *pkt),
                  pktbuf_t *meta)
{
  streaming_message_t *sm;
  int64_t prevdts = PTS_UNSET, absdts = PTS_UNSET;
  int64_t prevpts = PTS_UNSET, abspts = PTS_UNSET;
  th_pkt_t *pkt, *npkt;
  size_t metalen;

  tvhtrace("parser",
           "pkt bcklog %2d %-12s - backlog flush start - (meta %ld)",
           st->es_index,
           streaming_component_type2txt(st->es_type),
           meta ? (long)pktbuf_len(meta) : -1);
  TAILQ_FOREACH(sm, &st->es_backlog, sm_link) {
    pkt = sm->sm_data;
    if (pkt->pkt_meta) {
      meta = pkt->pkt_meta;
      break;
    }
  }
  while ((sm = TAILQ_FIRST(&st->es_backlog)) != NULL) {
    pkt = sm->sm_data;
    TAILQ_REMOVE(&st->es_backlog, sm, sm_link);

    if (pkt->pkt_dts != PTS_UNSET)
      pkt->pkt_dts &= ~PTS_BACKLOG;
    if (pkt->pkt_pts != PTS_UNSET)
      pkt->pkt_pts &= ~PTS_BACKLOG;

    if (prevdts != PTS_UNSET && pkt->pkt_dts != PTS_UNSET &&
        pkt->pkt_dts == ((prevdts + 1) & PTS_MASK))
      pkt->pkt_dts = absdts;
    if (prevpts != PTS_UNSET && pkt->pkt_pts != PTS_UNSET &&
        pkt->pkt_pts == ((prevpts + 1) & PTS_MASK))
      pkt->pkt_pts = abspts;

    if (meta) {
      if (pkt->pkt_meta == NULL) {
        pktbuf_ref_inc(meta);
        pkt->pkt_meta = meta;
        /* insert the metadata into payload of the first packet, too */
        npkt = pkt_copy_shallow(pkt);
        pktbuf_ref_dec(npkt->pkt_payload);
        metalen = pktbuf_len(meta);
        npkt->pkt_payload = pktbuf_alloc(NULL, metalen + pktbuf_len(pkt->pkt_payload));
        memcpy(pktbuf_ptr(npkt->pkt_payload), pktbuf_ptr(meta), metalen);
        memcpy(pktbuf_ptr(npkt->pkt_payload) + metalen,
               pktbuf_ptr(pkt->pkt_payload), pktbuf_len(pkt->pkt_payload));
        npkt->pkt_payload->pb_err = pkt->pkt_payload->pb_err + meta->pb_err;
        pkt_ref_dec(pkt);
        pkt = npkt;
      }
      meta = NULL;
    }

    if (pkt_cb)
      pkt_cb(t, st, pkt);

    if (absdts == PTS_UNSET) {
      absdts = pkt->pkt_dts;
    } else {
      absdts += st->es_frame_duration;
      absdts &= PTS_MASK;
    }

    if (abspts == PTS_UNSET) {
      abspts = pkt->pkt_pts;
    } else {
      abspts += st->es_frame_duration;
      abspts &= PTS_MASK;
    }

    prevdts = pkt->pkt_dts;
    prevpts = pkt->pkt_pts;

    parser_deliver(t, st, pkt);
    sm->sm_data = NULL;
    streaming_msg_free(sm);
  }
  tvhtrace("parser",
           "pkt bcklog %2d %-12s - backlog flush end -",
           st->es_index,
           streaming_component_type2txt(st->es_type));
}
