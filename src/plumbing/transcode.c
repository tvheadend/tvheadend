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
#include "service.h"
#include "packet.h"
#include "transcode.h"

/**
 * Reference to a transcoder stream
 */
typedef struct transcoder_stream {
  AVCodecContext *sctx; // source
  AVCodecContext *tctx; // target
  int            index; // refers to the stream index

  struct SwsContext *scaler; // used for scaling
  AVFrame           *dec_frame; // decoding buffer for video stream
  AVFrame           *enc_frame; // encoding buffer for video stream

  short           *dec_sample; // decoding buffer for audio stream
  uint8_t         *enc_sample; // encoding buffer for audio stream
} transcoder_stream_t;


typedef struct transcoder {
  streaming_target_t t_input;  // must be first
  streaming_target_t *t_output;
  
  // transcoder private stuff
  transcoder_stream_t ts_audio;
  transcoder_stream_t ts_video;
  size_t max_width;
  size_t max_height;

  //for the PID-regulator
  int feedback_error;
  int feedback_error_sum;
  time_t feedback_clock;
} transcoder_t;


/**
 * allocate the buffers used by a transcoder stream
 */
static void
transcoder_stream_create(transcoder_stream_t *ts)
{
  ts->sctx = avcodec_alloc_context();
  ts->tctx = avcodec_alloc_context();
  ts->dec_frame = avcodec_alloc_frame();
  ts->dec_sample = av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE*2 + FF_INPUT_BUFFER_PADDING_SIZE);
  ts->enc_frame = avcodec_alloc_frame();
  ts->enc_sample = av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE*2 + FF_INPUT_BUFFER_PADDING_SIZE);

  avcodec_get_frame_defaults(ts->dec_frame);
  avcodec_get_frame_defaults(ts->enc_frame);
  memset(ts->dec_sample, 0, AVCODEC_MAX_AUDIO_FRAME_SIZE*2 + FF_INPUT_BUFFER_PADDING_SIZE);
  memset(ts->enc_sample, 0, AVCODEC_MAX_AUDIO_FRAME_SIZE*2 + FF_INPUT_BUFFER_PADDING_SIZE);
}


/**
 * free all buffers used by a transcoder stream
 */
static void
transcoder_stream_destroy(transcoder_stream_t *ts)
{
  av_free(ts->sctx);
  av_free(ts->tctx);
  av_free(ts->dec_frame);
  av_free(ts->enc_frame);
  av_free(ts->dec_sample);
  av_free(ts->enc_sample);
  sws_freeContext(ts->scaler);
}


/**
 * transcode an audio stream
 */
static th_pkt_t *
transcoder_stream_audio(transcoder_stream_t *ts, th_pkt_t *pkt)
{
  th_pkt_t *n = NULL;
  AVPacket packet;
  int length, len;

  av_init_packet(&packet);
  packet.data = pktbuf_ptr(pkt->pkt_payload);
  packet.size = pktbuf_len(pkt->pkt_payload);
  packet.pts  = pkt->pkt_pts;
  packet.dts  = pkt->pkt_dts;
  packet.duration = pkt->pkt_duration;

  len = AVCODEC_MAX_AUDIO_FRAME_SIZE*2;
  length = avcodec_decode_audio3(ts->sctx, ts->dec_sample, &len, &packet);

  if(length <= 0) {
    tvhlog(LOG_ERR, "transcode", "Unable to decode audio");
    goto cleanup;
  }

  ts->tctx->channels        = ts->sctx->channels > 1 ? 2 : 1;
  ts->tctx->channel_layout  = ts->tctx->channels > 1 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
  ts->tctx->bit_rate        = ts->tctx->channels * 64000;
  ts->tctx->sample_rate     = ts->sctx->sample_rate;
  ts->tctx->sample_fmt      = ts->sctx->sample_fmt;
  ts->tctx->frame_size      = ts->sctx->frame_size;
  ts->tctx->block_align     = ts->sctx->block_align;
  ts->tctx->time_base.den   = ts->tctx->sample_rate;
  ts->tctx->time_base.num   = 1;

  if(ts->tctx->codec_id == CODEC_ID_NONE) {
    AVCodec *codec = avcodec_find_encoder(CODEC_ID_MP3);
    if(!codec || avcodec_open(ts->tctx, codec) < 0) {
      tvhlog(LOG_ERR, "transcode", "Unable to find audio encoder");
      goto cleanup;
    }
  }

  length = avcodec_encode_audio(ts->tctx, ts->enc_sample, AVCODEC_MAX_AUDIO_FRAME_SIZE*2, ts->dec_sample);
  if(length <= 0) {
    tvhlog(LOG_ERR, "transcode", "Unable to encode audio");
    goto cleanup;
  }

  n = pkt_alloc(ts->enc_sample, length, pkt->pkt_pts, pkt->pkt_dts);
  n->pkt_duration = pkt->pkt_duration;
  n->pkt_commercial = pkt->pkt_commercial;
  n->pkt_componentindex = pkt->pkt_componentindex;
  n->pkt_frametype = pkt->pkt_frametype;
  n->pkt_field = pkt->pkt_field;
  n->pkt_channels = pkt->pkt_channels;
  n->pkt_sri = pkt->pkt_sri;
  n->pkt_aspect_num = pkt->pkt_aspect_num;
  n->pkt_aspect_den = pkt->pkt_aspect_den;

 cleanup:
  av_free_packet(&packet);
  return n;
}


/**
 * transcode a video stream
 */
static th_pkt_t *
transcoder_stream_video(transcoder_stream_t *ts, th_pkt_t *pkt)
{
  uint8_t *buf = NULL;
  uint8_t *out = NULL;
  th_pkt_t *n = NULL;
  AVPacket packet;
  int length, len;
  int got_picture;

  av_init_packet(&packet);
  packet.data = pktbuf_ptr(pkt->pkt_payload);
  packet.size = pktbuf_len(pkt->pkt_payload);
  packet.pts  = ts->dec_frame->pts = pkt->pkt_pts;
  packet.dts  = pkt->pkt_dts;
  packet.duration = pkt->pkt_duration;

  length = avcodec_decode_video2(ts->sctx, ts->dec_frame, &got_picture, &packet);

  if(length <= 0) {
    tvhlog(LOG_ERR, "transcode", "Unable to decode video");
    goto cleanup;
  }

  if(!got_picture) {
    goto cleanup;
  }

  ts->tctx->time_base.den = ts->sctx->time_base.den;
  ts->tctx->time_base.num = ts->sctx->time_base.num;

  ts->tctx->sample_aspect_ratio.num = pkt->pkt_aspect_num;
  ts->tctx->sample_aspect_ratio.den = pkt->pkt_aspect_den;

  if(ts->tctx->codec_id == CODEC_ID_NONE) {
    ts->tctx->pix_fmt               = PIX_FMT_YUV420P;
    ts->tctx->flags                |= CODEC_FLAG_QSCALE;
    ts->tctx->rc_lookahead          = 0;
    ts->tctx->max_b_frames          = 0;

    AVCodec *codec = avcodec_find_encoder(CODEC_ID_MPEG2VIDEO);
    if(!codec || avcodec_open(ts->tctx, codec) < 0) {
      tvhlog(LOG_ERR, "transcode", "Unable to find video encoder");
      ts->tctx->codec_id = CODEC_ID_NONE;
      goto cleanup;
    }
  }

  len = avpicture_get_size(ts->tctx->pix_fmt, ts->tctx->width, ts->tctx->height);
  buf = av_malloc(len + FF_INPUT_BUFFER_PADDING_SIZE);
  memset(buf, 0, len);

  avpicture_fill((AVPicture *)ts->enc_frame, 
                 buf, 
                 ts->tctx->pix_fmt,
                 ts->tctx->width, 
                 ts->tctx->height);
 
  ts->scaler = sws_getCachedContext(ts->scaler,
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
 
  sws_scale(ts->scaler, 
            (const uint8_t * const*)ts->dec_frame->data, 
            ts->dec_frame->linesize,
            0, 
            ts->sctx->height, 
            ts->enc_frame->data, 
            ts->enc_frame->linesize);
 
  ts->enc_frame->pts = ts->dec_frame->pts;

  len = avpicture_get_size(ts->tctx->pix_fmt, ts->sctx->width, ts->sctx->height);
  out = av_malloc(len + FF_INPUT_BUFFER_PADDING_SIZE);
  memset(out, 0, len);

  length = avcodec_encode_video(ts->tctx, out, len, ts->enc_frame);
  if(length <= 0) {
    tvhlog(LOG_ERR, "transcode", "Unable to encode video");
    goto cleanup;
  }
  
  n = pkt_alloc(out, length, ts->enc_frame->pts, pkt->pkt_dts);

  if(n->pkt_pts == PTS_UNSET)
    n->pkt_pts = pkt->pkt_pts;

  if(n->pkt_pts == PTS_UNSET)
    n->pkt_pts = pkt->pkt_dts;

  if(ts->enc_frame->pict_type & FF_I_TYPE)
    n->pkt_frametype = PKT_I_FRAME;
  else if(ts->enc_frame->pict_type & FF_P_TYPE)
    n->pkt_frametype = PKT_P_FRAME;
  else if(ts->enc_frame->pict_type & FF_B_TYPE)
    n->pkt_frametype = PKT_B_FRAME;
  else
    n->pkt_frametype = pkt->pkt_frametype;

  n->pkt_duration = pkt->pkt_duration;

  n->pkt_commercial = pkt->pkt_commercial;
  n->pkt_componentindex = pkt->pkt_componentindex;
  n->pkt_field = pkt->pkt_field;

  n->pkt_channels = pkt->pkt_channels;
  n->pkt_sri = pkt->pkt_sri;

  n->pkt_aspect_num = pkt->pkt_aspect_num;
  n->pkt_aspect_den = pkt->pkt_aspect_den;
  
 cleanup:
  av_free_packet(&packet);
  if(buf)
    av_free(buf);
  if(out)
    av_free(out);

  return n;
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

  if(ts && ts->sctx->codec_type == AVMEDIA_TYPE_AUDIO) {
    return transcoder_stream_audio(ts, pkt);
  } else if(ts && ts->sctx->codec_type == AVMEDIA_TYPE_VIDEO) {
    return transcoder_stream_video(ts, pkt);
  }

  return NULL;
}


/**
 * initializes eatch transcoding stream
 */
static streaming_start_t *
transcoder_start(transcoder_t *t, streaming_start_t *src)
{
  int i = 0;
  streaming_start_t *ss = NULL;

  t->feedback_clock = dispatch_clock;

  ss = malloc(sizeof(streaming_start_t) + sizeof(streaming_start_component_t) * 2);
  memset(ss, 0, sizeof(streaming_start_t) + sizeof(streaming_start_component_t) * 2);

  ss->ss_num_components = 2;
  ss->ss_refcount = 1;
  ss->ss_pcr_pid = src->ss_pcr_pid;
  service_source_info_copy(&ss->ss_si, &src->ss_si);

  for(i = 0; i < src->ss_num_components; i++) {
    streaming_start_component_t *ssc_src = &src->ss_components[i];
    enum CodecID codec_id = CODEC_ID_NONE;

    switch(ssc_src->ssc_type) {
    case SCT_H264:
      codec_id = CODEC_ID_H264;
      break;
    case SCT_MPEG2VIDEO:
      codec_id = CODEC_ID_MPEG2VIDEO;
      break;
    case SCT_AC3:
      codec_id = CODEC_ID_AC3;
      break;
    case SCT_EAC3:
      codec_id = CODEC_ID_EAC3;
      break;
    case SCT_AAC:
      codec_id = CODEC_ID_AAC;
      break;
    case SCT_MPEG2AUDIO:
      codec_id = CODEC_ID_MP2;
      break;
    }

    if (!t->ts_audio.index && SCT_ISAUDIO(ssc_src->ssc_type)) {
      AVCodec *codec = avcodec_find_decoder(codec_id);
      if(!codec || avcodec_open(t->ts_audio.sctx, codec) < 0) {
	tvhlog(LOG_ERR, "transcode", "Unable to find %s decoder", streaming_component_type2txt(ssc_src->ssc_type));
	continue;
      }

      t->ts_audio.index = ssc_src->ssc_index;

      streaming_start_component_t *ssc = &ss->ss_components[0];
      ssc->ssc_index    = ssc_src->ssc_index;
      ssc->ssc_type     = SCT_MPEG2AUDIO;
      ssc->ssc_sri      = ssc_src->ssc_sri;
      ssc->ssc_channels = ssc_src->ssc_channels;
      memcpy(ssc->ssc_lang, ssc_src->ssc_lang, 4);

      t->ts_audio.sctx->codec_type = AVMEDIA_TYPE_AUDIO;
      t->ts_audio.tctx->codec_type = AVMEDIA_TYPE_AUDIO;
    }

    if (!t->ts_video.index && SCT_ISVIDEO(ssc_src->ssc_type)) {
      AVCodec *codec = avcodec_find_decoder(codec_id);
      if(!codec || avcodec_open(t->ts_video.sctx, codec) < 0) {
	tvhlog(LOG_ERR, "transcode", "Unable to find %s decoder", streaming_component_type2txt(ssc_src->ssc_type));
	continue;
      }

      t->ts_video.index = ssc_src->ssc_index;

      streaming_start_component_t *ssc = &ss->ss_components[1];

      ssc->ssc_index         = ssc_src->ssc_index;
      ssc->ssc_type          = SCT_MPEG2VIDEO;
      ssc->ssc_aspect_num    = ssc_src->ssc_aspect_num;
      ssc->ssc_aspect_den    = ssc_src->ssc_aspect_den;
      ssc->ssc_height        = MIN(t->max_height, ssc_src->ssc_height);
      ssc->ssc_width         = ssc->ssc_height * ((double)ssc_src->ssc_width / ssc_src->ssc_height);
      ssc->ssc_frameduration = ssc_src->ssc_frameduration;

      t->ts_video.sctx->codec_type         = AVMEDIA_TYPE_VIDEO;

      t->ts_video.tctx->codec_type         = AVMEDIA_TYPE_VIDEO;
      t->ts_video.tctx->width              = ssc->ssc_width;
      t->ts_video.tctx->height             = ssc->ssc_height;
      t->ts_video.tctx->qmin               = 1;
      t->ts_video.tctx->qmax               = FF_LAMBDA_MAX;

      tvhlog(LOG_INFO, "transcode", "%s %dx%d ==> %s %dx%d", 
	     streaming_component_type2txt(ssc_src->ssc_type),
	     ssc_src->ssc_width,
	     ssc_src->ssc_height,
	     streaming_component_type2txt(ssc->ssc_type),
	     ssc->ssc_width,
	     ssc->ssc_height);
    }
  }

  return ss;
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
  transcoder_t *t = opaque;

  switch(sm->sm_type) {
  case SMT_PACKET: {
    th_pkt_t *s_pkt = pkt_merge_header(sm->sm_data);
    th_pkt_t *t_pkt = transcoder_packet(t, s_pkt);

    if(t_pkt) {
      sm = streaming_msg_create_pkt(t_pkt);
      streaming_target_deliver2(t->t_output, sm);
      pkt_ref_dec(t_pkt);
    }

    pkt_ref_dec(s_pkt);
    break;
  }
  case SMT_START: {
    streaming_start_t *ss = transcoder_start(t, sm->sm_data);
    streaming_start_unref(sm->sm_data);
    sm->sm_data = ss;

    streaming_target_deliver2(t->t_output, sm);
    break;
  }
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
transcoder_create(streaming_target_t *output, size_t max_width, size_t max_height)
{
  transcoder_t *t = calloc(1, sizeof(transcoder_t));

  memset(t, 0, sizeof(transcoder_t));
  t->t_output = output;
  t->max_width = max_width;
  t->max_height = max_height;

  transcoder_stream_create(&t->ts_video);
  transcoder_stream_create(&t->ts_audio);

  streaming_target_init(&t->t_input, transcoder_input, t, 0);
  return &t->t_input;
}

#define K_P     4
#define K_D     1
#define K_I     2

/**
 * 
 */
void
transcoder_set_network_speed(streaming_target_t *st, int speed)
{
  transcoder_t *t = (transcoder_t *)st;
  transcoder_stream_t *ts = &t->ts_video;

  if(!ts->tctx)
    return;

  if(dispatch_clock - t->feedback_clock < 1)
    return;

  tvhlog(LOG_DEBUG, "transcode", "Client network speed: %d%%", speed);

  int error = 100 - speed;
  int derivative = (error - t->feedback_error) / MAX(1, dispatch_clock - t->feedback_clock);

  t->feedback_error = error;
  t->feedback_error_sum += error;
  t->feedback_clock = dispatch_clock;

  tvhlog(LOG_DEBUG, "transcode", "Error: %d, Sum: %d, Derivative: %d", 
	 t->feedback_error, t->feedback_error_sum, derivative);

  int q = 1 + (K_P*t->feedback_error + K_I*t->feedback_error_sum + K_D*derivative);

  q = MIN(q, FF_LAMBDA_MAX);
  q = MAX(q, 1);

  //if(q != ts->enc_frame->quality) {
    tvhlog(LOG_DEBUG, "transcode", "New quality: %d ==> %d", ts->enc_frame->quality, q);
    ts->enc_frame->quality = q;
    //}
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

/**
 * 
 */ 
void
transcoder_init(void)
{
  avcodec_init();
  avcodec_register_all();
}

