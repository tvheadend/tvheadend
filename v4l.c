/*
 *  TV Input - Linux analogue (v4lv2) interface
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

#include <assert.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>
#include <ffmpeg/avio.h>
#include <ffmpeg/avformat.h>

extern AVInputFormat mpegps_demuxer;

#define __user
#include <linux/videodev2.h>

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "v4l.h"
#include "channels.h"
#include "dispatch.h"
#include "transports.h"
#include "ts.h"

static struct th_v4l_adapter_list v4l_adapters;

static void v4l_fd_callback(int events, void *opaque, int fd);

static int v4l_setfreq(th_v4l_adapter_t *tva, int frequency);

static void v4l_pes_packet(th_v4l_adapter_t *tva, uint8_t *buf, int len,
			   int type);

static void v4l_ts_generate(th_v4l_adapter_t *tva, uint8_t *ptr, int len,
			    int type, int64_t pts, int64_t dts);

static void v4l_add_adapter(const char *path);




/* 
 *
 */
void
v4l_add_adapters(void)
{
  v4l_add_adapter("/dev/video0");
}



/* 
 *
 */
int
v4l_configure_transport(th_transport_t *t, const char *muxname,
			const char *channel_name)
{
  config_entry_t *ce;
  char buf[100];

  if((ce = find_mux_config("v4lmux", muxname)) == NULL)
    return -1;

  t->tht_type = TRANSPORT_V4L;

  t->tht_v4l_frequency = 
    atoi(config_get_str_sub(&ce->ce_sub, "frequency", "0"));

  ts_add_pid(t, 100, HTSTV_MPEG2VIDEO);
  ts_add_pid(t, 101, HTSTV_MPEG2AUDIO);

  snprintf(buf, sizeof(buf), "Analog: %s (%.2f MHz)", muxname, 
	   (float)t->tht_v4l_frequency / 1000000.0f);
  t->tht_name = strdup(buf);

  transport_link(t, channel_find(channel_name, 1));
  return 0;
}


/*
 *
 */
static void
v4l_add_adapter(const char *path)
{
  int fd, r;
  th_v4l_adapter_t *tva;
  struct v4l2_capability caps;

  fd = open(path, O_RDWR);
  if(fd == -1)
    return;

  r = ioctl(fd, VIDIOC_QUERYCAP, &caps);

  close(fd);
  if(r < 0)
    return;


  tva = calloc(1, sizeof(th_v4l_adapter_t));

  pthread_cond_init(&tva->tva_run_cond, NULL);

  tva->tva_path = strdup(path);
  LIST_INSERT_HEAD(&v4l_adapters, tva, tva_link);

  tva->tva_name = strdup((char *)caps.card);
}




/* 
 *
 */
static int 
v4l_setfreq(th_v4l_adapter_t *tva, int frequency)
{
  struct v4l2_frequency vf;
  struct v4l2_tuner vt;
  int result;

  vf.tuner = 0;
  vf.type = V4L2_TUNER_ANALOG_TV;
  vf.frequency = (frequency * 16) / 1000000;
  result = ioctl(tva->tva_fd, VIDIOC_S_FREQUENCY, &vf);
  if(result < 0) {
    syslog(LOG_ERR, "%s: Unable to tune to %dHz\n", tva->tva_path, frequency);
    return 1;
  }

  vt.index = 0;
  result = ioctl(tva->tva_fd, VIDIOC_G_TUNER, &vt);

  if(result < 0) {
    syslog(LOG_ERR, "%s: Unable read tuner status\n", tva->tva_path);
    return 1;
  }
	
  syslog(LOG_DEBUG, "%s: Tuned to %.3f MHz%s",
	 tva->tva_path, (float)frequency/1000000.0,
	 vt.signal ? "  (Signal Detected)" : "");

  return 0;
}


/* 
 *
 */
static void
v4l_stop(th_v4l_adapter_t *tva)
{
  if(tva->tva_dispatch_handle != NULL) {
    close(dispatch_delfd(tva->tva_dispatch_handle));
    tva->tva_dispatch_handle = NULL;
  }
}



/* 
 *
 */
void
v4l_stop_feed(th_transport_t *t)
{
  th_v4l_adapter_t *tva = t->tht_v4l_adapter;

  t->tht_v4l_adapter = NULL;
  LIST_REMOVE(t, tht_adapter_link);

  t->tht_status = TRANSPORT_IDLE;
  transport_flush_subscribers(t);

  if(LIST_FIRST(&tva->tva_transports) == NULL)
    v4l_stop(tva);
}



/* 
 *
 */
static void
v4l_adapter_clean(th_v4l_adapter_t *tva)
{
  th_transport_t *t;
  
  while((t = LIST_FIRST(&tva->tva_transports)) != NULL)
    v4l_stop_feed(t);

  v4l_stop(tva);
}





/* 
 *
 */
int 
v4l_start_feed(th_transport_t *t, unsigned int weight)
{
  th_v4l_adapter_t *tva, *cand = NULL;
  int w, fd;
  AVCodec *c;

  LIST_FOREACH(tva, &v4l_adapters, tva_link) {
    w = transport_compute_weight(&tva->tva_transports);
    if(w < weight)
      cand = tva;

    if(tva->tva_frequency == t->tht_v4l_frequency)
      break;
  }

  if(tva == NULL) {
    if(cand == NULL)
      return 1;

    v4l_adapter_clean(cand);
    tva = cand;
  }


  if(tva->tva_streams[0].tva_ctx == NULL) {
    c = avcodec_find_decoder(CODEC_ID_MPEG2VIDEO);
    tva->tva_streams[0].tva_ctx = avcodec_alloc_context();
    avcodec_open(tva->tva_streams[0].tva_ctx, c);
    tva->tva_streams[0].tva_parser = av_parser_init(CODEC_ID_MPEG2VIDEO);
  }

  if(tva->tva_streams[1].tva_ctx == NULL) {
    c = avcodec_find_decoder(CODEC_ID_MP2);
    tva->tva_streams[1].tva_ctx = avcodec_alloc_context();
    avcodec_open(tva->tva_streams[1].tva_ctx, c);
    tva->tva_streams[1].tva_parser = av_parser_init(CODEC_ID_MP2);
  }

  if(tva->tva_dispatch_handle == NULL) {
    fd = open(tva->tva_path, O_RDWR);
    if(fd == -1)
      return 1;

    tva->tva_dispatch_handle = 
      dispatch_addfd(fd, v4l_fd_callback, tva, DISPATCH_READ);
    tva->tva_fd = fd;
  }

  tva->tva_frequency = t->tht_v4l_frequency;

  if(v4l_setfreq(tva, tva->tva_frequency))
    return 1;

  LIST_INSERT_HEAD(&tva->tva_transports, t, tht_adapter_link);
  t->tht_v4l_adapter = tva;
  t->tht_status = TRANSPORT_RUNNING;
  
  return 0;
}






static void
v4l_fd_callback(int events, void *opaque, int fd)
{
  th_v4l_adapter_t *tva = opaque;
  uint8_t buf[4000];
  uint8_t *ptr, *pkt;
  int len, s = 0, l, r;

  if(!(events & DISPATCH_READ))
    return;

  len = read(fd, buf, 4000);
  if(len < 1)
    return;

  ptr = buf;

  while(len > 0) {

    switch(tva->tva_startcode) {
    default:
      tva->tva_startcode = tva->tva_startcode << 8 | *ptr;
      tva->tva_lenlock = 0;
      ptr++; len--;
      continue;

    case 0x000001e0:
      s = 0; /* video */
      break;
    case 0x000001c0:
      s = 1; /* audio */
      break;
    }

    if(tva->tva_lenlock == 2) {
      l = tva->tva_streams[s].tva_pes_packet_len;
      pkt = realloc(tva->tva_streams[s].tva_pes_packet, l);
      tva->tva_streams[s].tva_pes_packet = pkt;
      
      r = l - tva->tva_streams[s].tva_pes_packet_pos;
      if(r > len)
	r = len;
      memcpy(pkt + tva->tva_streams[s].tva_pes_packet_pos, ptr, r);
      
      ptr += r;
      len -= r;

      tva->tva_streams[s].tva_pes_packet_pos += r;
      if(tva->tva_streams[s].tva_pes_packet_pos == l) {
	v4l_pes_packet(tva, pkt, l, s);
	tva->tva_startcode = 0;
      }
      
    } else {
      tva->tva_streams[s].tva_pes_packet_len = 
	tva->tva_streams[s].tva_pes_packet_len << 8 | *ptr;
      tva->tva_lenlock++;
      if(tva->tva_lenlock == 2) {
	tva->tva_streams[s].tva_pes_packet_pos = 0;
      }
      ptr++; len--;
    }
  }
}




#define getu32(b, l) ({						\
  uint32_t x = (b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]);	\
  b+=4;								\
  l-=4; 							\
  x;								\
})

#define getu16(b, l) ({						\
  uint16_t x = (b[0] << 8 | b[1]);	                        \
  b+=2;								\
  l-=2; 							\
  x;								\
})

#define getu8(b, l) ({						\
  uint8_t x = b[0];	                                        \
  b+=1;								\
  l-=1; 							\
  x;								\
})

#define getpts(b, l) ({					\
  int64_t _pts;						\
  _pts = (int64_t)((getu8(b, l) >> 1) & 0x07) << 30;	\
  _pts |= (int64_t)(getu16(b, l) >> 1) << 15;		\
  _pts |= (int64_t)(getu16(b, l) >> 1);			\
  _pts;							\
})


/* 
 *
 */
static void
v4l_pes_packet(th_v4l_adapter_t *tva, uint8_t *buf, int len, int type)
{
  uint8_t flags, hlen, x;
  int64_t dts = AV_NOPTS_VALUE, pts = AV_NOPTS_VALUE;
  uint8_t *outbuf;
  int outlen, rlen;
  AVCodecParserContext *p;

  x = getu8(buf, len);
  flags = getu8(buf, len);
  hlen = getu8(buf, len);
  
  if(len < hlen)
    return;

  if((x & 0xc0) != 0x80)
    /* no MPEG 2 PES */
    return;

  if((flags & 0xc0) == 0xc0) {
    if(hlen < 10)
      return;

    pts = getpts(buf, len);
    dts = getpts(buf, len);

    hlen -= 10;
  } else if((flags & 0xc0) == 0x80) {
    if(hlen < 5)
      return;

    dts = pts = getpts(buf, len);
    hlen -= 5;
  }

  buf += hlen;
  len -= hlen;

  p = tva->tva_streams[type].tva_parser;

  while(len > 0) {

    rlen = av_parser_parse(p, tva->tva_streams[type].tva_ctx,
			   &outbuf, &outlen, buf, len, 
			   pts, dts);

    if(outlen)
      v4l_ts_generate(tva, outbuf, outlen, type, pts, dts);

    buf += rlen;
    len -= rlen;
  }

}





/* 
 *
 */
static void
v4l_ts_generate(th_v4l_adapter_t *tva, uint8_t *ptr, int len, int type,
		int64_t pts, int64_t dts)
{
  th_pid_t p;
  uint32_t sc;
  uint8_t tsb0[188];
  uint8_t *tsb, *src;
  int64_t ts, pcr = AV_NOPTS_VALUE;
  int hlen, tlen, slen, plen, cc, pad;
  uint16_t u16;
  th_transport_t *t;
  
  memset(&p, 0, sizeof(p));

  

  if(type) {
    sc = 0x1c0;
    p.tp_type = HTSTV_MPEG2AUDIO;
    p.tp_pid = 101;

  } else {
    sc = 0x1e0;
    p.tp_type = HTSTV_MPEG2VIDEO;
    p.tp_pid = 100;
    if(pts != AV_NOPTS_VALUE)
      pcr = dts;
  }

  tsb = tsb0;

  slen = len;
  len += 13;
  
  *tsb++ = 0x47;
  *tsb++ = p.tp_pid >> 8 | 0x40; /* payload unit start indicator */
  *tsb++ = p.tp_pid;

  cc = ++tva->tva_streams[type].tva_cc;

  *tsb++ = (cc & 0xf) | 0x10;

  *tsb++ = 0;
  *tsb++ = 0;
  *tsb++ = sc >> 8;
  *tsb++ = sc;
      
  *tsb++ = len >> 8;
  *tsb++ = len;

  *tsb++ = 0x80;  /* MPEG2 */

  if(pts == AV_NOPTS_VALUE || dts == AV_NOPTS_VALUE) {
    *tsb++ = 0x00;  /* no ts */
    *tsb++ = 0;     /* hdrlen */
    hlen = 0;
  } else {

    *tsb++ = 0xc0;  /* pts & dts */
    *tsb++ = 10;    /* pts & dts */
    hlen = 10;

    ts = pts;
    *tsb++ = (((ts >> 30) & 7) << 1) | 1;
    u16 = (((ts >> 15) & 0x7fff) << 1) | 1;
    *tsb++ = u16 >> 8;
    *tsb++ = u16;
    u16 = ((ts & 0x7fff) << 1) | 1;
    *tsb++ = u16 >> 8;
    *tsb++ = u16;

    ts = dts;
    *tsb++ = (((ts >> 30) & 7) << 1) | 1;
    u16 = (((ts >> 15) & 0x7fff) << 1) | 1;
    *tsb++ = u16 >> 8;
    *tsb++ = u16;
    u16 = ((ts & 0x7fff) << 1) | 1;
    *tsb++ = u16 >> 8;
    *tsb++ = u16;
  }

  tlen = 188 - 4 - 4 - 2 - 3 - hlen;

  src = ptr;

  plen = FFMIN(tlen, slen);

  while(1) {
    assert(plen != 0);
    assert(slen > 0);

    memcpy(tsb, src, plen);


    LIST_FOREACH(t, &tva->tva_transports, tht_adapter_link) {
      ts_recv_tsb(t, p.tp_pid, tsb0, 0, pcr);
      pcr = AV_NOPTS_VALUE;
    }

    slen -= plen;
    if(slen == 0)
      break;

    src += plen;

    tsb = tsb0;

    *tsb++ = 0x47;
    *tsb++ = p.tp_pid >> 8;
    *tsb++ = p.tp_pid;

    tlen = 188 - 4;

    cc = ++tva->tva_streams[type].tva_cc;

    plen = FFMIN(tlen, slen);
    if(plen == slen) {
      tlen -= 2;
      plen = FFMIN(tlen, slen);
      pad = tlen - plen;

      *tsb++ = (cc & 0xf) | 0x30;
      *tsb++ = pad + 1;
      *tsb++ = 0;
      tsb += pad;

    } else {
      *tsb++ = (cc & 0xf) | 0x10;
    }
  }
}
