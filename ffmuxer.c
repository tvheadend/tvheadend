/*
 *  FFmpeg based muxer output
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

#include <pthread.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#include <libavformat/avformat.h>
#include <libavutil/avstring.h>

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "channels.h"
#include "subscriptions.h"
#include "htsclient.h"
#include "ffmuxer.h"
#include "buffer.h"

static URLProtocol tffm_fifo_proto;


/**
 * Register a callback
 */
void
tffm_init(void)
{
  register_protocol(&tffm_fifo_proto);
}



/**
 *  Internal state
 */
void
tffm_set_state(th_ffmuxer_t *tffm, int status)
{
  const char *tp;

  if(tffm->tffm_state == status)
    return;

  switch(status) {
  case TFFM_STOP:
    tp = "stopped";
    break;
  case TFFM_WAIT_SUBSCRIPTION:
    tp = "waiting for subscription";
    break;
  case TFFM_WAIT_FOR_START:
    tp = "waiting for program start";
    break;
  case TFFM_WAIT_AUDIO_LOCK:
    tp = "waiting for audio lock";
    break;
  case TFFM_WAIT_VIDEO_LOCK:
    tp = "waiting for video lock";
    break;
  case TFFM_RUNNING:
    tp = "running";
    break;
  case TFFM_COMMERCIAL:
    tp = "commercial break";
    break;
  default:
    tp = "<invalid state>";
    break;
  }

  syslog(LOG_INFO, "%s - Entering state \"%s\"", tffm->tffm_printname, tp);
  tffm->tffm_state = status;
}

/**
 * Open TFFM output
 */

void
tffm_open(th_ffmuxer_t *tffm, th_transport_t *t, 
	  AVFormatContext *fctx, const char *printname)
{
  th_muxer_t *tm = &tffm->tffm_muxer;
  th_stream_t *st;
  th_muxstream_t *tms;
  AVCodecContext *ctx;
  AVCodec *codec;
  enum CodecID codec_id;
  enum CodecType codec_type;
  const char *codec_name;

  tffm->tffm_avfctx = fctx;
  tffm->tffm_printname = strdup(printname);

  av_set_parameters(fctx, NULL);

  LIST_FOREACH(st, &t->tht_streams, st_link) {
    switch(st->st_type) {
    default:
      continue;
    case HTSTV_MPEG2VIDEO:
      codec_id   = CODEC_ID_MPEG2VIDEO;
      codec_type = CODEC_TYPE_VIDEO;
      codec_name = "mpeg2 video";
      break;

    case HTSTV_MPEG2AUDIO:
      codec_id   = CODEC_ID_MP2;
      codec_type = CODEC_TYPE_AUDIO;
      codec_name = "mpeg2 audio";
      break;

    case HTSTV_AC3:
      codec_id   = CODEC_ID_AC3;
      codec_type = CODEC_TYPE_AUDIO;
      codec_name = "AC3 audio";
      break;

    case HTSTV_H264:
      codec_id   = CODEC_ID_H264;
      codec_type = CODEC_TYPE_VIDEO;
      codec_name = "h.264 video";
      break;
    }

    codec = avcodec_find_decoder(codec_id);
    if(codec == NULL) {
      syslog(LOG_ERR, 
	     "%s - Cannot find codec for %s, ignoring stream", 
	     printname, codec_name);
      continue;
    }

    tms = calloc(1, sizeof(th_muxstream_t));
    tms->tms_stream   = st;
    tms->tms_index = fctx->nb_streams;

    tms->tms_avstream = av_new_stream(fctx, fctx->nb_streams);

    ctx = tms->tms_avstream->codec;
    ctx->codec_id   = codec_id;
    ctx->codec_type = codec_type;

    if(avcodec_open(ctx, codec) < 0) {
      syslog(LOG_ERR,
	     "%s - Cannot open codec for %s, ignoring stream", 
	     printname, codec_name);
      free(ctx);
      continue;
    }

    LIST_INSERT_HEAD(&tm->tm_streams, tms, tms_muxer_link0);
    memcpy(tms->tms_avstream->language, tms->tms_stream->st_lang, 4);
  }

  /* Fire up recorder thread */

  tffm_set_state(tffm, TFFM_WAIT_FOR_START);
}

/**
 * Close ffmuxer
 */
void
tffm_close(th_ffmuxer_t *tffm)
{
  AVFormatContext *fctx = tffm->tffm_avfctx;
  AVStream *avst;
  int i;

  if(fctx == NULL)
    return;

  /* Write trailer if we've written anything at all */
    
  if(tffm->tffm_header_written)
    av_write_trailer(fctx);

  /* Close streams and format */
  
  for(i = 0; i < fctx->nb_streams; i++) {
    avst = fctx->streams[i];
    avcodec_close(avst->codec);
    free(avst->codec);
    free(avst);
  }
  
  url_fclose(fctx->pb);
  free(fctx);

  free(tffm->tffm_printname);
}




/** 
 * Check if all streams of the given type has been decoded
 */
static int
is_all_decoded(th_muxer_t *tm, enum CodecType type)
{
  th_muxstream_t *tms;
  AVStream *st;

  LIST_FOREACH(tms, &tm->tm_streams, tms_muxer_link0) {
    st = tms->tms_avstream;
    if(st->codec->codec->type == type && tms->tms_decoded == 0)
      return 0;
  }
  return 1;
}



/** 
 * Write a packet to output file
 */
void
tffm_record_packet(th_ffmuxer_t *tffm, th_pkt_t *pkt)
{
  th_muxer_t *tm = &tffm->tffm_muxer;
  AVFormatContext *fctx = tffm->tffm_avfctx;
  th_muxstream_t *tms;
  AVStream *st, *stx;
  AVCodecContext *ctx;
  AVPacket avpkt;
  void *abuf;
  AVFrame pic;
  int r, data_size, i;
  void *buf;
  char txt[100];
  size_t bufsize;

  LIST_FOREACH(tms, &tm->tm_streams, tms_muxer_link0)
    if(tms->tms_stream == pkt->pkt_stream)
      break;

  if(tms == NULL)
    return;

  st = tms->tms_avstream;
  ctx = st->codec;

  /* Make sure packet is in memory */

  buf = pkt_payload(pkt);
  bufsize = pkt_len(pkt);
  
  switch(tffm->tffm_state) {
  default:
    break;

  case TFFM_WAIT_AUDIO_LOCK:
    if(ctx->codec_type != CODEC_TYPE_AUDIO || tms->tms_decoded)
      break;

    abuf = malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
    r = avcodec_decode_audio(ctx, abuf, &data_size, buf, bufsize);
    free(abuf);

    if(r != 0 && data_size) {
      syslog(LOG_DEBUG, "%s - "
	     "Stream #%d: \"%s\" decoded a complete audio frame: "
	     "%d channels in %d Hz\n",
	     tffm->tffm_printname, tms->tms_index,
	     st->codec->codec->name,
	     st->codec->channels,
	     st->codec->sample_rate);
	
      tms->tms_decoded = 1;
    }

    if(is_all_decoded(tm, CODEC_TYPE_AUDIO))
      tffm_set_state(tffm, TFFM_WAIT_VIDEO_LOCK);
    break;
    
  case TFFM_WAIT_VIDEO_LOCK:
    if(ctx->codec_type != CODEC_TYPE_VIDEO || tms->tms_decoded)
      break;

    r = avcodec_decode_video(st->codec, &pic, &data_size, buf, bufsize);
    if(r != 0 && data_size) {
      syslog(LOG_DEBUG, "%s - "
	     "Stream #%d: \"%s\" decoded a complete video frame: "
	     "%d x %d at %.2fHz\n",
	     tffm->tffm_printname, tms->tms_index,
	     ctx->codec->name,
	     ctx->width, st->codec->height,
	     (float)ctx->time_base.den / (float)ctx->time_base.num);
      
      tms->tms_decoded = 1;
    }
    
    if(!is_all_decoded(tm, CODEC_TYPE_VIDEO))
      break;

    /* All Audio & Video decoded, start recording */

    tffm_set_state(tffm, TFFM_RUNNING);

    if(!tffm->tffm_header_written) {

      if(av_write_header(fctx)) {
	syslog(LOG_ERR, 
	       "%s - Unable to write header", 
	       tffm->tffm_printname);
 	break;
      }

      tffm->tffm_header_written = 1;

      syslog(LOG_DEBUG, 
	     "%s - Header written to file, stream dump:", 
	     tffm->tffm_printname);
      
      for(i = 0; i < fctx->nb_streams; i++) {
	stx = fctx->streams[i];
	
	avcodec_string(txt, sizeof(txt), stx->codec, 1);
	
	syslog(LOG_DEBUG, "%s - Stream #%d: %s [%d/%d]",
	       tffm->tffm_printname, i, txt, 
	       stx->time_base.num, stx->time_base.den);
	
      }
    }
    /* FALLTHRU */
      
  case TFFM_RUNNING:
     
    if(tffm->tffm_header_written == 0)
      break;

    if(pkt->pkt_commercial == COMMERCIAL_YES) {
      tffm_set_state(tffm, TFFM_COMMERCIAL);
      break;
    }

    av_init_packet(&avpkt);
    avpkt.stream_index = tms->tms_index;

    avpkt.dts = av_rescale_q(pkt->pkt_dts, AV_TIME_BASE_Q, st->time_base);
    avpkt.pts = av_rescale_q(pkt->pkt_pts, AV_TIME_BASE_Q, st->time_base);
    avpkt.data = buf;
    avpkt.size = bufsize;
    avpkt.duration = pkt->pkt_duration;

    r = av_interleaved_write_frame(fctx, &avpkt);
    break;


  case TFFM_COMMERCIAL:

    if(pkt->pkt_commercial != COMMERCIAL_YES) {

      LIST_FOREACH(tms, &tm->tm_streams, tms_muxer_link0)
	tms->tms_decoded = 0;
      
      tffm_set_state(tffm, TFFM_WAIT_AUDIO_LOCK);
    }
    break;
  }
}

/**
 * Packet input
 */
void
tffm_packet_input(th_muxer_t *tm, th_stream_t *st, th_pkt_t *pkt)
{
  th_ffmuxer_t *tffm = tm->tm_opaque;

  tffm_record_packet(tffm, pkt);
}



/**
 *
 */
static int fifo_tally;
static LIST_HEAD(, tffm_fifo) tffm_fifos;

tffm_fifo_t *
tffm_fifo_create(tffm_fifo_callback_t *callback, void *opaque)
{
  tffm_fifo_t *tf = calloc(1, sizeof(tffm_fifo_t));
  char buf[100];

  tf->tf_callback = callback;
  tf->tf_opaque = opaque;

  tf->tf_id = ++fifo_tally;
  TAILQ_INIT(&tf->tf_pktq);
  LIST_INSERT_HEAD(&tffm_fifos, tf, tf_link);

  snprintf(buf, sizeof(buf), "tvheadendfifo:%d", tf->tf_id);
  tf->tf_filename = strdup(buf);
  return tf;
}


/**
 * Destroy a fifo
 */
void
tffm_fifo_destroy(tffm_fifo_t *tf)
{
  tffm_fifo_pkt_t *pkt;

  while((pkt = TAILQ_FIRST(&tf->tf_pktq)) != NULL) {
    TAILQ_REMOVE(&tf->tf_pktq, pkt, tfp_link);
    free(pkt);
  }

  LIST_REMOVE(tf, tf_link);
  free(tf->tf_filename);
  free(tf);
}


/**
 * libavformat URLProtocol open func for internal fifo
 */
static int
tffm_fifo_open(URLContext *h, const char *filename, int flags)
{
  uint32_t id;
  char *final;
  tffm_fifo_t *tf;

  av_strstart(filename, "tvheadendfifo:", &filename);  
  id = strtol(filename, &final, 10);
  if(filename == final || *final)
    return AVERROR(ENOENT);

  LIST_FOREACH(tf, &tffm_fifos, tf_link)
    if(tf->tf_id == id)
      break;
  
  if(tf == NULL)
    return AVERROR(ENOENT);

  h->priv_data = tf;
  h->is_streamed = 1;
  return 0;
}

/**
 * libavformat URLProtocol write function, copy to an internal buffer
 */
static int
tffm_fifo_write(URLContext *h, unsigned char *buf, int size)
{
  tffm_fifo_pkt_t *pkt;
  tffm_fifo_t *tf = h->priv_data;

  pkt = malloc(size + sizeof(tffm_fifo_pkt_t));
  memcpy(pkt->tfp_buf, buf, size);
  pkt->tfp_pktsize = size;
  tf->tf_pktq_len += size;
  TAILQ_INSERT_TAIL(&tf->tf_pktq, pkt, tfp_link);

  tf->tf_callback(tf->tf_opaque);
  return size;
}


static int
tffm_fifo_close(URLContext *h)
{
  h->priv_data = NULL;
  return 0;
}


static URLProtocol tffm_fifo_proto = {
    "tvheadendfifo",
    tffm_fifo_open,
    NULL, /* read */
    tffm_fifo_write,
    NULL, /* seek */
    tffm_fifo_close,
};
