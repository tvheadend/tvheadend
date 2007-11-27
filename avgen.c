/*
 *  A/V generator, for test purposes
 *  Copyright (C) 2007 Andreas Öman
 *  Copyright (c) 2007 Nicolas George
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
#include <math.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "avgen.h"
#include "channels.h"
#include "dispatch.h"
#include "transports.h"
#include "pes.h"
#include "buffer.h"


typedef struct avgen {

  AVCodecContext *avctx[2];

  int16_t *audioframe;

  AVFrame videoframe;

  int video_duration;
  int audio_duration;

  int64_t video_next;
  int64_t audio_next;

  int64_t refclock;

  uint8_t *video_outbuf;
  int video_outbuf_size;

  uint8_t *audio_outbuf;
  int audio_outbuf_size;

  dtimer_t timer;

} avgen_t;


static void avgen_deliver(th_transport_t *t, avgen_t *avg, int64_t clk);

static void avgen_stop_feed(th_transport_t *t);

static int avgen_start_feed(th_transport_t *t, unsigned int weight,
			    int status);

static void update_video(avgen_t *avg, int vframe, int framerate);
static void update_audio(avgen_t *avg, int vframe, int framerate);

static void init_picture(avgen_t *avg, int w, int h);

/* 
 *
 */
void
avgen_init(void)
{
  th_channel_t *ch;
  th_transport_t *t;

  if(avcodec_find_encoder(CODEC_ID_MPEG2VIDEO) == NULL)
    return;

  if(avcodec_find_encoder(CODEC_ID_MP2) == NULL)
    return;

  ch = channel_find("Test 1", 1);

  t = calloc(1, sizeof(th_transport_t));
  t->tht_prio = 100;

  t->tht_type = TRANSPORT_AVGEN;
  t->tht_start_feed = avgen_start_feed;
  t->tht_stop_feed  = avgen_stop_feed;

  t->tht_video = transport_add_stream(t, -1, HTSTV_MPEG2VIDEO);
  t->tht_audio = transport_add_stream(t, -1, HTSTV_MPEG2AUDIO);

  t->tht_name = strdup(ch->ch_name);

  transport_link(t, ch);
}





/* 
 *
 */
static void
avgen_stop_feed(th_transport_t *t)
{
  avgen_t *avg = t->tht_avgen;

  av_free(avg->video_outbuf);
  av_free(avg->audio_outbuf);

  avcodec_close(avg->avctx[0]);
  avcodec_close(avg->avctx[1]);

  av_free(avg->audioframe);

  dtimer_disarm(&avg->timer);
  av_free(avg->videoframe.data[0]);
  free(avg);

  t->tht_status = TRANSPORT_IDLE;
  transport_flush_subscribers(t);
}



/*
 *
 */

static void
rgb2yuv(uint8_t yuv[3], const uint8_t rgb[3])
{
  int R = rgb[0];
  int G = rgb[1];
  int B = rgb[2];

  int Y = ( ( 66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
  int U = ( ( -38 * R - 74 * G + 112 * B + 128) >> 8) + 128;
  int V = ( ( 112 * R - 94 * G - 18 * B + 128) >> 8) + 128;

  yuv[0] = Y;
  yuv[1] = U;
  yuv[2] = V;
}


/* 
 *
 */
static int 
avgen_start_feed(th_transport_t *t, unsigned int weight, int status)
{
  avgen_t *avg;
  AVCodecContext *avctx;
  AVCodec *c;

  avg = calloc(1, sizeof(avgen_t));

  /* Open video */

  c = avcodec_find_encoder(CODEC_ID_MPEG2VIDEO);
  avctx = avg->avctx[0] = avcodec_alloc_context();

  avctx->codec_type = CODEC_TYPE_VIDEO;
  avctx->bit_rate = 1000000;
  avctx->width = 768;
  avctx->height = 576;
  avctx->time_base.den = 25;
  avctx->time_base.num = 1;
  avctx->pix_fmt = PIX_FMT_YUV420P;
  avctx->gop_size = 12;
  avctx->max_b_frames = 2;

  avg->video_duration = 1000000 / 25;


  if(avcodec_open(avg->avctx[0], c)) {
    return -1;
  }

  avpicture_alloc((AVPicture *)&avg->videoframe, PIX_FMT_YUV420P,
		  avctx->width, avctx->height);

  init_picture(avg, avctx->width, avctx->height);

  /* Open audio */

  c = avcodec_find_encoder(CODEC_ID_MP2);
  avctx = avg->avctx[1] = avcodec_alloc_context();

  avctx->codec_type = CODEC_TYPE_AUDIO;
  avctx->channels = 2;
  avctx->sample_rate = 48000;
  avctx->bit_rate = 256000;

  if(avcodec_open(avg->avctx[1], c)) {
    return -1;
  }

  avg->audio_duration = 1000000 * avctx->frame_size / avctx->sample_rate;
  avg->audioframe = av_malloc(avctx->frame_size * 2 * sizeof(int16_t));


  /* All done, setup output buffers and start */

  avg->video_outbuf_size = 200000;
  avg->video_outbuf = av_malloc(avg->video_outbuf_size);

  avg->audio_outbuf_size = 20000;
  avg->audio_outbuf = av_malloc(avg->audio_outbuf_size);

  t->tht_avgen = avg;

  t->tht_status = TRANSPORT_RUNNING;

  avg->refclock = getclock_hires();
  avgen_deliver(t, avg, avg->refclock);
  return 0;
}

/**
 *
 */
static void
avgen_enqueue(th_transport_t *t, th_stream_t *st, int64_t dts, int64_t pts,
	      int duration, uint8_t *data, int datalen)
{
  th_pkt_t *pkt;
  th_muxer_t *tm;

  avgstat_add(&st->st_rate, datalen, dispatch_clock);

  pkt = pkt_alloc(data, datalen, pts, dts);
  pkt->pkt_stream = st;
  pkt->pkt_duration = duration;
  
  /* Alert all muxers tied to us that a new packet has arrived */

  LIST_FOREACH(tm, &t->tht_muxers, tm_transport_link)
    tm->tm_new_pkt(tm, st, pkt);

  /* Unref (and possibly free) the packet, muxers are supposed
     to increase refcount or copy packet if they need anything */

  pkt_deref(pkt);
}


static void
avg_timer_callback(void *aux, int64_t now)
{
  th_transport_t *t = aux;
  avgen_deliver(t, t->tht_avgen, now);
}


/**
 *
 */
static void
avgen_deliver(th_transport_t *t, avgen_t *avg, int64_t clk0)
{
  int r;
  int64_t pts, clk, nxt;
  int64_t vframe;

  vframe = avg->video_next * avg->avctx[0]->time_base.den / 1000000;

  AVCodecContext *avctx;

  clk = clk0 - avg->refclock;

  if(avg->video_next <= clk) {
    update_video(avg, vframe, avg->avctx[0]->time_base.den);
    avctx = avg->avctx[0];
    avg->videoframe.pts = AV_NOPTS_VALUE;

    r = avcodec_encode_video(avctx, avg->video_outbuf, avg->video_outbuf_size,
			     &avg->videoframe);

    if(r > 0) {
      pts = av_rescale_q(avctx->coded_frame->pts, avctx->time_base,
			 AV_TIME_BASE_Q);
      avgen_enqueue(t, t->tht_video, avg->video_next, pts, avg->video_duration,
		    avg->video_outbuf, r);

    }
    avg->video_next += avg->video_duration;
  }


  if(avg->audio_next <= clk) {
    update_audio(avg, vframe, avg->avctx[0]->time_base.den);
    avctx = avg->avctx[1];
    r = avcodec_encode_audio(avctx, avg->audio_outbuf, avg->audio_outbuf_size,
			     avg->audioframe);

    if(r > 0) {
      avgen_enqueue(t, t->tht_audio, avg->audio_next, avg->audio_next,
		    avg->audio_duration,
		    avg->audio_outbuf, r);

    }
    avg->audio_next += avg->audio_duration;
  }

  nxt = FFMIN(avg->audio_next, avg->video_next) + avg->refclock;

  if(nxt < clk0 + 1000)
    nxt = clk0 + 1000;

  dtimer_arm_hires(&avg->timer, avg_timer_callback, t, nxt);
}



/**
 *
 */
static void
draw_rectangle(unsigned val, unsigned char *p, unsigned stride,
	       unsigned sw, unsigned x, unsigned y, unsigned w, unsigned h)
{
  unsigned i;

  p += sw * (x + y * stride);
  w *= sw;
  h *= sw;
  for(i = 0; i < h; i++) {
    memset(p, val, w);
    p += stride;
  }
}

/**
 *
 */
static void
draw_digit(int digit, unsigned char *p0, unsigned stride, unsigned sw)
{
  static const unsigned char masks[10] = {
    0x7D, 0x50, 0x37, 0x57, 0x5A, 0x4F, 0x6F, 0x51, 0x7F, 0x5F
  };
  unsigned mask = masks[digit];

  draw_rectangle(0, p0, stride, sw, 0, 0, 8, 13);
  if(mask & 1)
    draw_rectangle(255, p0, stride, sw, 1, 0, 5, 1);
  if(mask & 2)
    draw_rectangle(255, p0, stride, sw, 1, 6, 5, 1);
  if(mask & 4)
    draw_rectangle(255, p0, stride, sw, 1, 12, 5, 1);
  if(mask & 8)
    draw_rectangle(255, p0, stride, sw, 0, 1, 1, 5);
  if(mask & 16)
    draw_rectangle(255, p0, stride, sw, 6, 1, 1, 5);
  if(mask & 32)
    draw_rectangle(255, p0, stride, sw, 0, 7, 1, 5);
  if(mask & 64)
    draw_rectangle(255, p0, stride, sw, 6, 7, 1, 5);
}

/**
 *
 */
static void
update_video(avgen_t *avg, int vframe, int framerate)
{
  int frame, sec, min, d;
  unsigned char *p;
  int segsize, offset;
  int as;

  frame = vframe % framerate;
  vframe /= framerate;

  sec   = vframe % 60;
  vframe /= 60;
  
  min = vframe;


  as = sec & 1 ? 255 : 0;

  p = avg->videoframe.data[0];
  offset = avg->videoframe.linesize[0] * 3;

  draw_rectangle(~as, p + offset + 30, avg->videoframe.linesize[0], 
		 4, 0, 0, 8, 13);

  draw_rectangle(as, p + offset + 700, avg->videoframe.linesize[0], 
		 4, 0, 0, 8, 13);

  offset += 500;

  segsize = 4;

  d = frame % 10;
  draw_digit(d, p + offset , avg->videoframe.linesize[0], segsize);
  offset -= 40;
  d = frame / 10;
  draw_digit(d, p + offset , avg->videoframe.linesize[0], segsize);

  offset -= 60;

  d = sec % 10;
  draw_digit(d, p + offset , avg->videoframe.linesize[0], segsize);
  offset -= 40;
  d = sec / 10;
  draw_digit(d, p + offset , avg->videoframe.linesize[0], segsize);

  offset -= 60;

  d = min % 10;
  draw_digit(d, p + offset , avg->videoframe.linesize[0], segsize);
  offset -= 40;
  d = min / 10;
  draw_digit(d, p + offset , avg->videoframe.linesize[0], segsize);

}

/**
 *
 */
static void
update_audio(avgen_t *avg, int vframe, int framerate)
{
  int frame, i, j;
  float f;

  AVCodecContext *avctx = avg->avctx[1];
  frame = vframe % framerate;
  vframe /= framerate;

  if(frame < 2) {
    j = vframe & 1;
    for(i = 0; i < avctx->frame_size; i++) {
      f = sin(M_PI * 2 * (float)i / (float)avctx->frame_size * 20);
      avg->audioframe[i * 2 + j] = f * 20000;
    }
  } else {
    memset(avg->audioframe, 0, avctx->frame_size * 2 * sizeof(int16_t));
  }
}


/**
 *
 */
static void
init_picture(avgen_t *avg, int w, int h)
{
  uint8_t rgb[3] = {0, 0, 0};
  uint8_t yuv[3];
  int i;

  for(i = 0; i < h; i++) {
    rgb[2] = 255 * ((float)i / (float)h);
    rgb2yuv(yuv, rgb);
    memset(avg->videoframe.data[0] + i * avg->videoframe.linesize[0],
	   yuv[0], w);
  }

  for(i = 0; i < h / 2; i++) {

    rgb[2] = 255 * ((float)i / (float)(h / 2));
    rgb2yuv(yuv, rgb);

    memset(avg->videoframe.data[1] + i * avg->videoframe.linesize[1],
	   yuv[1], w / 2);
    memset(avg->videoframe.data[2] + i * avg->videoframe.linesize[2],
	   yuv[2], w / 2);
  }
}
