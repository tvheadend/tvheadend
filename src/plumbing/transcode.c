/**
 *  Transcoding
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

#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

#include "tvheadend.h"
#include "streaming.h"
#include "packet.h"
#include "transcode.h"

/**
 * Reference to a transcoder stream
 */
typedef struct transcoder_stream {
  AVCodecContext *sctx; // source
  AVCodecContext *tctx; // target
  int            index; // refers to the stream index

  struct SwsContext *conv; // used for scaling
  AVFrame           *frame; // decoding buffer for video stream

  short *samples; // decoding buffer for audio stream
} transcoder_stream_t;


typedef struct transcoder {
  streaming_target_t t_input;  // must be first
  streaming_target_t *t_output;
  
  // transcoder private stuff
  transcoder_stream_t ts_audio;
  transcoder_stream_t ts_video;
} transcoder_t;


/**
 * allocate the buffers used by a transcoder stream
 */
static void
transcoder_stream_create(transcoder_stream_t *ts)
{
  ts->sctx = avcodec_alloc_context();
  ts->tctx = avcodec_alloc_context();
  ts->frame = avcodec_alloc_frame();
  ts->samples = av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE*2);

  avcodec_get_frame_defaults(ts->frame);
  memset(ts->samples, 0, AVCODEC_MAX_AUDIO_FRAME_SIZE*2);
}


/**
 * free all buffers used by a transcoder stream
 */
static void
transcoder_stream_destroy(transcoder_stream_t *ts)
{
  av_free(ts->sctx);
  av_free(ts->tctx);
  av_free(ts->frame);
  av_free(ts->samples);
  sws_freeContext(ts->conv);
}


/**
 * initialize a transcoder stream
 */
static int
transcoder_stream_init(transcoder_stream_t *ts, const streaming_start_component_t *ssc)
{
  AVCodec *codec = NULL;

  if(ssc->ssc_type == SCT_H264)
    codec = avcodec_find_decoder(CODEC_ID_H264);
  else if(ssc->ssc_type == SCT_MPEG2VIDEO)
    codec = avcodec_find_decoder(CODEC_ID_MPEG2VIDEO);
  else if(ssc->ssc_type == SCT_AC3)
    codec = avcodec_find_decoder(CODEC_ID_AC3);
  else if(ssc->ssc_type == SCT_EAC3)
    codec = avcodec_find_decoder(CODEC_ID_EAC3);
  else if(ssc->ssc_type == SCT_AAC)
    codec = avcodec_find_decoder(CODEC_ID_AAC);
  else if(ssc->ssc_type == SCT_MPEG2AUDIO)
    codec = avcodec_find_decoder(CODEC_ID_MP2);

  if(codec && !avcodec_open(ts->sctx, codec)) {
    ts->index = ssc->ssc_index;
  } else {
    tvhlog(LOG_ERR, "transcoder", "Unable to find decoder");
    ts->index = 0;
    return -1;
  }

  if (SCT_ISAUDIO(ssc->ssc_type)) {
    ts->sctx->codec_type = CODEC_TYPE_AUDIO;
  } else if (SCT_ISVIDEO(ssc->ssc_type)) {
    ts->sctx->codec_type = CODEC_TYPE_VIDEO;
    ((streaming_start_component_t *)ssc)->ssc_width = ts->tctx->width = (2*ssc->ssc_width / 3);
    ((streaming_start_component_t *)ssc)->ssc_height = ts->tctx->height = (2*ssc->ssc_height / 3);
  }

  return 0;
}


/**
 * transcode an audio stream
 */
static th_pkt_t *
transcoder_stream_audio(transcoder_stream_t *ts, th_pkt_t *pkt)
{
  uint8_t *out = NULL;
  th_pkt_t *n;
  AVPacket packet;
  int length, len;

  av_init_packet(&packet);
  packet.data = pktbuf_ptr(pkt->pkt_payload);
  packet.size = pktbuf_len(pkt->pkt_payload);
  packet.pts  = pkt->pkt_pts;
  packet.dts  = pkt->pkt_dts;
  packet.duration = pkt->pkt_duration;

  len = AVCODEC_MAX_AUDIO_FRAME_SIZE*2;
  length = avcodec_decode_audio3(ts->sctx, ts->samples, &len, &packet);

  if(length <= 0) {
    tvhlog(LOG_ERR, "transcode", "Unable to decode audio");
    goto cleanup;
  }

  ts->tctx->channel_layout = ts->sctx->channel_layout;
  ts->tctx->channels       = ts->sctx->channels;
  ts->tctx->bit_rate       = ts->sctx->channels * 64000;
  ts->tctx->sample_rate    = ts->sctx->sample_rate;
  ts->tctx->sample_fmt     = ts->sctx->sample_fmt;
  ts->tctx->frame_size     = ts->sctx->frame_size;
  ts->tctx->block_align    = ts->sctx->block_align;

  if(ts->tctx->codec_id == CODEC_ID_NONE) {
    AVCodec *codec = avcodec_find_encoder(ts->sctx->codec_id);
    if(!codec || avcodec_open(ts->tctx, codec) < 0) {
      tvhlog(LOG_ERR, "transcoder", "Unable to find audio encoder");
      goto cleanup;
    }
  }

  len = AVCODEC_MAX_AUDIO_FRAME_SIZE;
  out = av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE * 2);
  memset(out, 0, AVCODEC_MAX_AUDIO_FRAME_SIZE * 2);

  length = avcodec_encode_audio(ts->tctx, out, len, ts->samples);
  if(length <= 0) {
    tvhlog(LOG_ERR, "transcoder", "Unable to encode audio");
    goto cleanup;
  }

  n = pkt_alloc(out, length, pkt->pkt_pts, pkt->pkt_dts);
  n->pkt_duration = pkt->pkt_duration;
  n->pkt_commercial = pkt->pkt_commercial;
  n->pkt_componentindex = pkt->pkt_componentindex;
  n->pkt_frametype = pkt->pkt_frametype;
  n->pkt_field = pkt->pkt_field;
  n->pkt_channels = pkt->pkt_channels;
  n->pkt_sri = pkt->pkt_sri;
  n->pkt_aspect_num = pkt->pkt_aspect_num;
  n->pkt_aspect_den = pkt->pkt_aspect_den;

  pkt_ref_dec(pkt);
  pkt = n;

 cleanup:
  av_free_packet(&packet);
  if(out)
    av_free(out);

  return pkt;
}


/**
 * transcode a video stream
 */
static th_pkt_t *
transcoder_stream_video(transcoder_stream_t *ts, th_pkt_t *pkt)
{
  AVFrame frame;
  uint8_t *buf = NULL;
  uint8_t *out = NULL;
  th_pkt_t *n;
  AVPacket packet;
  int length, len;
  int got_picture;

  av_init_packet(&packet);
  packet.data = pktbuf_ptr(pkt->pkt_payload);
  packet.size = pktbuf_len(pkt->pkt_payload);
  packet.pts  = pkt->pkt_pts;
  packet.dts  = pkt->pkt_dts;
  packet.duration = pkt->pkt_duration;

  length = avcodec_decode_video2(ts->sctx, ts->frame, &got_picture, &packet);

  if(length <= 0) {
    tvhlog(LOG_ERR, "transcoder", "Unable to decode video");
    goto cleanup;
  }

  if(!got_picture) {
    goto cleanup;
  }

  ts->tctx->pix_fmt = ts->sctx->pix_fmt;
  ts->tctx->has_b_frames = ts->sctx->has_b_frames;
  ts->tctx->time_base.den = ts->sctx->time_base.den;
  ts->tctx->time_base.num = ts->sctx->time_base.num;
  ts->tctx->sample_aspect_ratio = ts->sctx->sample_aspect_ratio;

  ts->tctx->bit_rate = 1500*1000;
  ts->tctx->bit_rate_tolerance = 8*2*1024*1024;
  ts->tctx->gop_size = 12;
  ts->tctx->max_b_frames = 2;

  if(ts->tctx->codec_id == CODEC_ID_NONE) {
    AVCodec *codec = avcodec_find_encoder(ts->sctx->codec_id);
    if(!codec || avcodec_open(ts->tctx, codec) < 0) {
      tvhlog(LOG_ERR, "transcoder", "Unable to find video encoder");
      ts->tctx->codec_id = CODEC_ID_NONE;
      goto cleanup;
    }
  }

  len = avpicture_get_size(ts->tctx->pix_fmt, ts->tctx->width, ts->tctx->height);
  buf = av_malloc(len);
  memset(buf, 0, len);
 
  avcodec_get_frame_defaults(&frame);

  avpicture_fill((AVPicture *)&frame, 
                 buf, 
                 ts->tctx->pix_fmt,
                 ts->tctx->width, 
                 ts->tctx->height);
 
  ts->conv = sws_getCachedContext(ts->conv,
                                  ts->sctx->width,
                                  ts->sctx->height,
                                  ts->sctx->pix_fmt,
                                  ts->tctx->width,
                                  ts->tctx->height,
                                  ts->tctx->pix_fmt,
                                  1,
                                  NULL,
                                  NULL,
                                  NULL);
 
  sws_scale(ts->conv, 
            (const uint8_t * const*)ts->frame->data, 
            ts->frame->linesize,
            0, 
            ts->sctx->height, 
            frame.data, 
            frame.linesize);
 
  frame.pts = pkt->pkt_pts;

  len = avpicture_get_size(ts->tctx->pix_fmt, ts->sctx->width, ts->sctx->height);
  out = av_malloc(len);
  memset(out, 0, len);

  length = avcodec_encode_video(ts->tctx, out, len, &frame);
  if(length <= 0) {
    tvhlog(LOG_ERR, "transcoder", "Unable to encode video");
    goto cleanup;
  }
  
  n = pkt_alloc(out, length, pkt->pkt_pts, pkt->pkt_dts);
  n->pkt_duration = pkt->pkt_duration;
  n->pkt_commercial = pkt->pkt_commercial;
  n->pkt_componentindex = pkt->pkt_componentindex;
  n->pkt_frametype = pkt->pkt_frametype;
  n->pkt_field = pkt->pkt_field;
  n->pkt_channels = pkt->pkt_channels;
  n->pkt_sri = pkt->pkt_sri;
  n->pkt_aspect_num = pkt->pkt_aspect_num;
  n->pkt_aspect_den = pkt->pkt_aspect_den;

  pkt_ref_dec(pkt);
  pkt = n;
  
 cleanup:
  av_free_packet(&packet);
  if(buf)
    av_free(buf);
  if(out)
    av_free(out);

  return pkt;
}


/**
 * transcode a packet
 */
static th_pkt_t*
transcoder_packet(transcoder_t *t, th_pkt_t *pkt)
{
  transcoder_stream_t *ts = NULL;

  if(pkt->pkt_componentindex == t->ts_video.index) {
    ts = &t->ts_video;
  } else if (pkt->pkt_componentindex == t->ts_audio.index) {
    ts = &t->ts_audio;
  }

  if(ts && ts->sctx->codec_type == CODEC_TYPE_AUDIO) {
    return transcoder_stream_audio(ts, pkt);
  } else if(ts && ts->sctx->codec_type == CODEC_TYPE_VIDEO) {
    return transcoder_stream_video(ts, pkt);
  }

  return pkt;
}


/**
 * initializes eatch transcoding stream
 */
static void
transcoder_start(transcoder_t *t, const streaming_start_t *ss)
{
  int i;

  for(i = 0; i < ss->ss_num_components; i++) {
    const streaming_start_component_t *ssc = &ss->ss_components[i];

    if (SCT_ISAUDIO(ssc->ssc_type))
      if (t->ts_audio.index || transcoder_stream_init(&t->ts_audio, ssc) < 0)
	continue;
    
    if (SCT_ISVIDEO(ssc->ssc_type))
      if (t->ts_video.index || transcoder_stream_init(&t->ts_video, ssc) < 0)
	continue;
  }
}


/**
 * closes the codecs and resets the transcoding streams
 */
static void
transcoder_stop(transcoder_t *t)
{
  avcodec_close(t->ts_audio.sctx);
  avcodec_close(t->ts_audio.tctx);
  avcodec_close(t->ts_video.sctx);
  avcodec_close(t->ts_video.tctx);

  t->ts_audio.sctx->codec_id = CODEC_ID_NONE;
  t->ts_audio.tctx->codec_id = CODEC_ID_NONE;
  t->ts_video.sctx->codec_id = CODEC_ID_NONE;
  t->ts_video.tctx->codec_id = CODEC_ID_NONE;

  t->ts_audio.index = 0;
  t->ts_video.index = 0;
}


/**
 * handle a streaming message
 */
static void
transcoder_input(void *opaque, streaming_message_t *sm)
{
  th_pkt_t *pkt;
  transcoder_t *t = opaque;

  switch(sm->sm_type) {
  case SMT_PACKET:
    pkt = pkt_merge_header(sm->sm_data);
    pkt = transcoder_packet(t, pkt);

    sm = streaming_msg_create_pkt(pkt);
    streaming_target_deliver2(t->t_output, sm);
    break;

  case SMT_START:
    transcoder_start(t, sm->sm_data);
    streaming_target_deliver2(t->t_output, sm);
    break;

  case SMT_STOP:
    transcoder_stop(t);
    // Fallthrough

  case SMT_EXIT:
  case SMT_SERVICE_STATUS:
  case SMT_NOSTART:
  case SMT_MPEGTS:
    streaming_target_deliver2(t->t_output, sm);
    break;
  }
}


/**
 *
 */
streaming_target_t *
transcoder_create(streaming_target_t *output)
{
  transcoder_t *t = calloc(1, sizeof(transcoder_t));

  memset(t, 0, sizeof(transcoder_t));
  t->t_output = output;

  transcoder_stream_create(&t->ts_video);
  transcoder_stream_create(&t->ts_audio);

  streaming_target_init(&t->t_input, transcoder_input, t, 0);
  return &t->t_input;
}


/**
 * 
 */
void
transcoder_destroy(streaming_target_t *st)
{
  transcoder_t *t = (transcoder_t *)st;

  transcoder_stream_destroy(&t->ts_video);
  transcoder_stream_destroy(&t->ts_audio);
  free(t);
}

