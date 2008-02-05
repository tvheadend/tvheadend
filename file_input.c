/*
 *  Circular streaming from a file
 *  Copyright (C) 2008 Andreas Öman
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
#include <errno.h>

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
#include "file_input.h"
#include "channels.h"
#include "dispatch.h"
#include "transports.h"
#include "parsers.h"
#include "buffer.h"


typedef struct file_input {
  AVFormatContext *fi_fctx;
  const char *fi_name;
  dtimer_t fi_timer;
  int64_t fi_refclock;
  int64_t fi_last_dts;
  int64_t fi_dts_offset;

} file_input_t;


static void file_input_stop_feed(th_transport_t *t);

static int file_input_start_feed(th_transport_t *t, unsigned int weight,
				 int status);

static void fi_timer_callback(void *aux, int64_t now);

static void file_input_get_pkt(th_transport_t *t, file_input_t *fi,
			       int64_t now);

/* 
 *
 */
void
file_input_init(void)
{
  const char *s;
  file_input_t *fi;
  int i;
  AVFormatContext *fctx;
  AVCodecContext *ctx;
  config_entry_t *ce;
  th_transport_t *t;
  th_stream_t *st;
  th_channel_t *ch;

  TAILQ_FOREACH(ce, &config_list, ce_link) {
    if(ce->ce_type != CFG_SUB || strcasecmp("streamedfile", ce->ce_key))
      continue;
    
    if((s = config_get_str_sub(&ce->ce_sub, "file", NULL)) == NULL)
      continue;

    if(av_open_input_file(&fctx, s, NULL, 0, NULL) != 0) {
      syslog(LOG_ERR, "streamedfile: Unable to open file \"%s\"", s);
      continue;
    }

    //    fctx->flags |= AVFMT_FLAG_GENPTS;

    if(av_find_stream_info(fctx) < 0) {
      av_close_input_file(fctx);
      syslog(LOG_ERR, "streamedfile: \"%s\": Unable to probe file format", s);
      continue;
    }

    dump_format(fctx, 0, s, 0);
    
    fi = calloc(1, sizeof(file_input_t));

    fi->fi_fctx = fctx;
    fi->fi_name = strdup(s);
    
    t = calloc(1, sizeof(th_transport_t));
    t->tht_prio = 100;

    t->tht_type = TRANSPORT_STREAMEDFILE;
    t->tht_start_feed = file_input_start_feed;
    t->tht_stop_feed  = file_input_stop_feed;

    for(i = 0; i < fctx->nb_streams; i++) {
      ctx = fctx->streams[i]->codec;
      
      switch(ctx->codec_id) {
      case CODEC_ID_H264:
	transport_add_stream(t, i, HTSTV_H264);
	break;
	
      case CODEC_ID_MPEG2VIDEO:
	st = transport_add_stream(t, i, HTSTV_MPEG2VIDEO);
	st->st_vbv_delay = -1;
	st->st_vbv_size = 229376;
 	break;

      case CODEC_ID_MP2:
      case CODEC_ID_MP3:
	if(ctx->sub_id == 2)
	  transport_add_stream(t, i, HTSTV_MPEG2AUDIO);
 	break;

      case CODEC_ID_AC3:
	transport_add_stream(t, i, HTSTV_AC3);
 	break;

      default:
	break;
      }
    }

    if((s = config_get_str_sub(&ce->ce_sub, "channel", NULL)) == NULL)
      continue;

    ch = channel_find(s, 1, channel_group_find("Streamed channels", 1));


    t->tht_name = strdup(ch->ch_name);
    t->tht_provider = strdup("HTS Tvheadend");
    t->tht_network = strdup("Internal");
    t->tht_uniquename = strdup(ch->ch_name);
    t->tht_file_input = fi;
    transport_link(t, ch, THT_OTHER);
  }
}





/* 
 *
 */
static void
file_input_stop_feed(th_transport_t *t)
{
  file_input_t *fi = t->tht_file_input;

  dtimer_disarm(&fi->fi_timer);
  t->tht_status = TRANSPORT_IDLE;
}



  
static void
file_input_reset(th_transport_t *t, file_input_t *fi)
{
  av_seek_frame(fi->fi_fctx, -1, 0, AVSEEK_FLAG_BACKWARD);
  t->tht_dts_start = AV_NOPTS_VALUE;
}


/* 
 *
 */
static int 
file_input_start_feed(th_transport_t *t, unsigned int weight, int status)
{
  file_input_t *fi = t->tht_file_input;
  
  t->tht_status = TRANSPORT_RUNNING;

  file_input_reset(t, fi);
  fi->fi_refclock = getclock_hires();

  dtimer_arm(&fi->fi_timer, fi_timer_callback, t, 1);
  return 0;
}


/* 
 *
 */
static void
file_input_get_pkt(th_transport_t *t, file_input_t *fi, int64_t now)
{
  AVPacket ffpkt;
  th_pkt_t *pkt;
  th_stream_t *st;
  int i, frametype;
  int64_t dts, pts, d = 1;
  uint32_t sc;

  do {
    i = av_read_frame(fi->fi_fctx, &ffpkt);
    if(i < 0) {
      file_input_reset(t, fi);
      d = 20000;
      fi->fi_dts_offset += fi->fi_last_dts;
      printf("File wrap\n");
      break;
    }
    
    LIST_FOREACH(st, &t->tht_streams, st_link)
      if(st->st_pid == ffpkt.stream_index)
	break;
    
    if(st == NULL)
      continue;


    if(t->tht_dts_start == AV_NOPTS_VALUE)
	t->tht_dts_start = ffpkt.dts;

   /* Figure frametype */

    switch(st->st_type) {
    case HTSTV_MPEG2VIDEO:
      sc = ffpkt.data[0] << 24 | ffpkt.data[1] << 16 | ffpkt.data[2] << 8 |
	ffpkt.data[3];
      if(sc == 0x100) {
	frametype = (ffpkt.data[5] >> 3) & 7;
      } else {
	frametype = PKT_I_FRAME;
      }
      if(frametype == PKT_B_FRAME)
	ffpkt.pts = ffpkt.dts;

      break;
    default:
      frametype = 0;
      break;
    }
    assert(ffpkt.dts != AV_NOPTS_VALUE); /* fixme */

    pts = ffpkt.pts;
    if(pts != AV_NOPTS_VALUE)
      pts += fi->fi_dts_offset;

    pkt = pkt_alloc(ffpkt.data, ffpkt.size, pts,
		    ffpkt.dts + fi->fi_dts_offset);
    pkt->pkt_frametype = frametype;
    pkt->pkt_duration = 0;
    pkt->pkt_stream = st;
    st->st_tb = fi->fi_fctx->streams[ffpkt.stream_index]->time_base;
    fi->fi_last_dts = ffpkt.dts;

    avgstat_add(&st->st_rate, ffpkt.size, dispatch_clock);

    switch(st->st_type) {
    case HTSTV_MPEG2VIDEO:
      parse_compute_pts(t, st, pkt);
      break;

    default:
      parser_compute_duration(t, st, pkt);
      break;
    }

    dts = av_rescale_q(ffpkt.dts - t->tht_dts_start + fi->fi_dts_offset,
		       fi->fi_fctx->streams[ffpkt.stream_index]->time_base,
		       AV_TIME_BASE_Q);

    av_free_packet(&ffpkt);

    d = dts - 1000000 - (now - fi->fi_refclock);
  } while(d <= 0);

  dtimer_arm_hires(&fi->fi_timer, fi_timer_callback, t, now + d);
}

static void
fi_timer_callback(void *aux, int64_t now)
{
  th_transport_t *t = aux;
  file_input_get_pkt(t, t->tht_file_input, now);
}
