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
#include "client.h"
#include "channels.h"
#include "teletext.h"

static struct th_v4l_adapter_list v4l_adapters;

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

  printf("%s: %s: %s %08x caps = 0x%08x\n",
	 caps.driver,
	 caps.card,
	 caps.bus_info,
	 caps.version,
	 caps.capabilities);

  tva = calloc(1, sizeof(th_v4l_adapter_t));

  pthread_cond_init(&tva->tva_run_cond, NULL);

  tva->tva_path = strdup(path);
  LIST_INSERT_HEAD(&v4l_adapters, tva, tva_link);

  tva->tva_name = strdup((char *)caps.card);
}


void
v4l_add_adapters(void)
{
  v4l_add_adapter("/dev/video0");
}



static int 
v4l_setfreq(const char *device, int frequency)
{
  struct v4l2_frequency vf;
  struct v4l2_tuner vt;
  int fd, result;

  if ((fd = open(device, O_RDWR)) < 0)
    {
      fprintf(stderr, "Failed to open %s\n", device);
      return 1;
    }

  vf.tuner = 0;
  vf.type = V4L2_TUNER_ANALOG_TV;
  vf.frequency = (frequency * 16) / 1000000;
  result = ioctl(fd, VIDIOC_S_FREQUENCY, &vf);

  vt.index = 0;
  result = ioctl(fd, VIDIOC_G_TUNER, &vt);

  close(fd);

  if (result < 0)
    {
      fprintf(stderr, "ioctl VIDIOC_S_FREQUENCY failed\n");
      return 1;
    }
	
  fprintf(stderr, "%s: %.3f MHz%s\n", device, frequency/1000000.0,
	  vt.signal ? "  (Signal Detected)" : "");

  return 0;
}




static void
v4l_receive_loop(th_v4l_adapter_t *tva)
{
  uint8_t tsb0[188];
  uint8_t *tsb, *src;
  tv_streamtype_t type;
  AVCodecContext *ctx;
  AVFormatContext *fctx;
  AVPacket pkt;
  int64_t ts;
  uint16_t u16;
  int i, slen, plen, tlen, len, pad;
  uint32_t sc;
  th_transport_t *t;
  th_subscription_t *s;
  pidinfo_t pid_a;
  pidinfo_t pid_v;
  pidinfo_t *pp;

  memset(&pid_a, 0, sizeof(pidinfo_t));
  memset(&pid_v, 0, sizeof(pidinfo_t));

  pid_a.pid = 200;
  pid_v.pid = 100;

  printf("opening %s\n", tva->tva_path);

  if(av_open_input_file(&fctx, tva->tva_path, &mpegps_demuxer, 0, NULL) != 0) {
    fprintf(stderr, "v4l: Unable to open input file\n");
    return;
  }
  

  pthread_mutex_lock(&tvh_mutex);

  tva->tva_running = 2;

  pthread_cond_signal(&tva->tva_run_cond);

  while(1) {
    pthread_mutex_unlock(&tvh_mutex);
    i = av_read_frame(fctx, &pkt);


    if(tva->tva_running == 0 || i < 0) {
      break;
    }

    pthread_mutex_lock(&tvh_mutex);

#if 0   
    printf("[%3d] %d bytes %lld %lld\t", 
	   pkt.stream_index, pkt.size, pkt.pts, pkt.dts);
    for(i = 0; i < 16; i++)
      printf("%02x ", pkt.data[i]);
    printf("\n");
#endif

    tsb = tsb0;

    ctx = fctx->streams[pkt.stream_index]->codec;

    type = 0;

    switch(ctx->codec_type) {
    case CODEC_TYPE_VIDEO:
      sc = 0x1e0;
      type = HTSTV_MPEG2VIDEO;
      pp = &pid_v;
      break;

    case CODEC_TYPE_AUDIO:
      sc = 0x1c0;
      type = HTSTV_MPEG2AUDIO;
      pp = &pid_a;
      break;

    default:
      break;
    }

    len = pkt.size + 3 + 10;

    if(type != 0) {
      
      *tsb++ = type;
      *tsb++ = pp->pid >> 8 | 0x40; /* payload unit start indicator */
      *tsb++ = pp->pid;
      pp->cc++;
      *tsb++ = (pp->cc & 0xf) | 0x10;

      *tsb++ = 0;
      *tsb++ = 0;
      *tsb++ = sc >> 8;
      *tsb++ = sc;
      
      *tsb++ = len >> 8;
      *tsb++ = len;

      *tsb++ = 0x80;  /* MPEG2 */
      *tsb++ = 0xc0;  /* pts & dts */
      *tsb++ = 10;    /* pts & dts */

      ts = pkt.pts;
      *tsb++ = (((ts >> 30) & 7) << 1) | 1;
      u16 = (((ts >> 15) & 0x7fff) << 1) | 1;
      *tsb++ = u16 >> 8;
      *tsb++ = u16;
      u16 = ((ts & 0x7fff) << 1) | 1;
      *tsb++ = u16 >> 8;
      *tsb++ = u16;

      ts = pkt.dts;
      *tsb++ = (((ts >> 30) & 7) << 1) | 1;
      u16 = (((ts >> 15) & 0x7fff) << 1) | 1;
      *tsb++ = u16 >> 8;
      *tsb++ = u16;
      u16 = ((ts & 0x7fff) << 1) | 1;
      *tsb++ = u16 >> 8;
      *tsb++ = u16;

      tlen = 188 - 4 - 4 - 2 - 3 - 10;

      src = pkt.data;
      slen = pkt.size;

      plen = FFMIN(tlen, slen);

      while(1) {
	assert(plen != 0);
	assert(slen > 0);

	memcpy(tsb, src, plen);


	LIST_FOREACH(t, &tva->tva_transports, tht_adapter_link) {
	  LIST_FOREACH(s, &t->tht_subscriptions, ths_transport_link) {
	    s->ths_callback(s, tsb0, pp, pkt.stream_index);
	  }
	}

	slen -= plen;
	if(slen == 0)
	  break;

	src += plen;

	tsb = tsb0;

	*tsb++ = type;
	*tsb++ = pp->pid >> 8;
	*tsb++ = pp->pid;

	tlen = 188 - 4;
	pp->cc++;

	plen = FFMIN(tlen, slen);
	if(plen == slen) {
	  tlen -= 2;
	  plen = FFMIN(tlen, slen);
	  pad = tlen - plen;

	  *tsb++ = (pp->cc & 0xf) | 0x30;
	  *tsb++ = pad + 1;
	  *tsb++ = 0;
	  tsb += pad;

	} else {
	  *tsb++ = (pp->cc & 0xf) | 0x10;
	}
      }
    }
    av_free_packet(&pkt);
  }
  av_close_input_file(fctx);
}

static void *
v4l_thread(void *aux)
{
  th_v4l_adapter_t *tva = aux;

  v4l_receive_loop(tva);
  return NULL;
}

static void
v4l_stop(th_v4l_adapter_t *tva)
{
  tva->tva_running = 0;
  if(tva->tva_ptid == 0)
    return;

  pthread_mutex_unlock(&tvh_mutex);
  pthread_join(tva->tva_ptid, NULL);
  pthread_mutex_lock(&tvh_mutex);

  tva->tva_ptid = 0;
}



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



static void
v4l_adapter_clean(th_v4l_adapter_t *tva)
{
  th_transport_t *t;
  
  while((t = LIST_FIRST(&tva->tva_transports)) != NULL)
    v4l_stop_feed(t);

  v4l_stop(tva);
}


int 
v4l_start_feed(th_transport_t *t, unsigned int weight)
{
  th_v4l_adapter_t *tva, *cand = NULL;
  int w;

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

  tva->tva_frequency = t->tht_v4l_frequency;

  v4l_setfreq(tva->tva_path, tva->tva_frequency);

  LIST_INSERT_HEAD(&tva->tva_transports, t, tht_adapter_link);
  t->tht_v4l_adapter = tva;
  t->tht_status = TRANSPORT_RUNNING;
  
  assert(tva->tva_running == 0);
  assert(tva->tva_ptid == 0);

  tva->tva_running = 1;

  pthread_create(&tva->tva_ptid, NULL, v4l_thread, tva);

  while(tva->tva_running != 2)
    pthread_cond_wait(&tva->tva_run_cond, &tvh_mutex);

  return 0;
}






/* 
 *
 */


int
v4l_configure_transport(th_transport_t *t, const char *muxname)
{
  config_entry_t *ce;
  char buf[100];

  if((ce = find_mux_config("v4lmux", muxname)) == NULL)
    return -1;

  t->tht_type = TRANSPORT_V4L;

  t->tht_v4l_frequency = 
    atoi(config_get_str_sub(&ce->ce_sub, "frequency", "0"));

  t->tht_pids = calloc(1, 2 * sizeof(pidinfo_t));
  t->tht_npids = 2;

  t->tht_pids[0].pid = 100;
  t->tht_pids[0].type = HTSTV_MPEG2VIDEO;

  t->tht_pids[1].pid = 200;
  t->tht_pids[1].type = HTSTV_MPEG2AUDIO;

  snprintf(buf, sizeof(buf), "Analog: %s (%.1f MHz)", muxname, 
	   (float)t->tht_v4l_frequency / 1000000.0f);
  t->tht_name = strdup(buf);

  return 0;
}

