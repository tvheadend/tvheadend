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

#include <syslog.h>

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "file_input.h"
#include "channels.h"
#include "dispatch.h"
#include "transports.h"
#include "parsers.h"
#include "buffer.h"

#include <libavformat/avformat.h>

typedef struct file_input {
  AVFormatContext *fi_fctx;
  const char *fi_name;
  dtimer_t fi_timer;
  int64_t fi_refclock;
  int64_t fi_dts_offset;
  int64_t fi_last_dts;

  th_pkt_t *fi_next;

} file_input_t;


static void file_input_stop_feed(th_transport_t *t);

static int file_input_start_feed(th_transport_t *t, unsigned int weight,
				 int status, int force_start);

static void fi_deliver(void *aux, int64_t now);

static void file_input_get_pkt(th_transport_t *t, file_input_t *fi,
			       int64_t now);



static const char *file_input_source_name(th_transport_t *t)
{
  return "file input";
}


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
  channel_t *ch;

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

    ch = channel_find(s, 1);


    t->tht_name = strdup(ch->ch_name);
    t->tht_provider = strdup("HTS Tvheadend");
    t->tht_identifier = strdup(ch->ch_name);
    t->tht_file_input = fi;
    t->tht_sourcename = file_input_source_name;
    transport_map_channel(t, ch);
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
  t->tht_runstatus = TRANSPORT_IDLE;
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
file_input_start_feed(th_transport_t *t, unsigned int weight, int status,
		      int force_start)
{
  file_input_t *fi = t->tht_file_input;
  
  t->tht_runstatus = TRANSPORT_RUNNING;

  file_input_reset(t, fi);
  fi->fi_refclock = getclock_hires();
  fi->fi_dts_offset = AV_NOPTS_VALUE;

  dtimer_arm(&fi->fi_timer, fi_deliver, t, 1);
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
  int i;
  int64_t dts, pts, errcnt = 0, dt;

 again:
  i = av_read_frame(fi->fi_fctx, &ffpkt);
  if(i < 0) {
    fi->fi_dts_offset += fi->fi_last_dts;
    file_input_reset(t, fi);
    fi->fi_next = NULL;

    if(errcnt == 0) {
      errcnt++;
      goto again;
    }

    syslog(LOG_ERR, "streamedfile: Unable to read from file \"%s\"",
	   fi->fi_name);
    return;
  }

  LIST_FOREACH(st, &t->tht_streams, st_link)
    if(st->st_pid == ffpkt.stream_index)
      break;
  
  if(st == NULL)
    goto again;

  /* Update stream timebase */
  st->st_tb = fi->fi_fctx->streams[ffpkt.stream_index]->time_base;

  assert(ffpkt.dts != AV_NOPTS_VALUE); /* fixme */

  if(fi->fi_dts_offset == AV_NOPTS_VALUE)
    fi->fi_dts_offset = ffpkt.dts;

  fi->fi_last_dts = ffpkt.dts;
  dts = ffpkt.dts + fi->fi_dts_offset;

  pts = ffpkt.pts;
  if(pts != AV_NOPTS_VALUE)
    pts += fi->fi_dts_offset;

  pkt = pkt_alloc(ffpkt.data, ffpkt.size, pts, dts);
  pkt->pkt_duration = 0;
  pkt->pkt_stream = st;

  fi->fi_next = pkt;
 
  /* compute delivery time */

  dt = av_rescale_q(dts, st->st_tb, AV_TIME_BASE_Q) + fi->fi_refclock;
  dtimer_arm_hires(&fi->fi_timer, fi_deliver, t, dt);
}

static void
fi_deliver(void *aux, int64_t now)
{
  th_transport_t *t = aux;
  file_input_t *fi = t->tht_file_input;
  th_stream_t *st;

  if(fi->fi_next != NULL) {
    st = fi->fi_next->pkt_stream;
    fi->fi_next->pkt_stream = NULL; /* this must be null upon enqueue */
    parser_enqueue_packet(t, st, fi->fi_next);
    fi->fi_next = NULL;
  }

  file_input_get_pkt(t, fi, now);
}
