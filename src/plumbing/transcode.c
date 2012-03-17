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
  streaming_component_type_t ttype;
  AVCodecContext *sctx; // source
  AVCodecContext *tctx; // target
  int            index; // refers to the stream index

  struct SwsContext *scaler; // used for scaling
  AVFrame           *dec_frame; // decoding buffer for video stream
  AVFrame           *enc_frame; // encoding buffer for video stream

  uint8_t           *dec_sample; // decoding buffer for audio stream
  uint32_t           dec_size;
  uint32_t           dec_offset;

  uint8_t           *enc_sample; // encoding buffer for audio stream
  uint32_t           enc_size;
  uint32_t           enc_offset;
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
transcoder_stream_create(transcoder_stream_t *ts, streaming_component_type_t type)
{
  memset(ts, 0, sizeof(transcoder_stream_t));

  ts->ttype = type;
  ts->sctx = avcodec_alloc_context();
  ts->tctx = avcodec_alloc_context();

  if(SCT_ISVIDEO(type)) {
    ts->dec_frame = avcodec_alloc_frame();
    ts->enc_frame = avcodec_alloc_frame();
    avcodec_get_frame_defaults(ts->dec_frame);
    avcodec_get_frame_defaults(ts->enc_frame);
  } else if(SCT_ISAUDIO(type)) {
    ts->dec_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*2;
    ts->dec_sample = av_malloc(ts->dec_size + FF_INPUT_BUFFER_PADDING_SIZE);
    memset(ts->dec_sample, 0, ts->dec_size + FF_INPUT_BUFFER_PADDING_SIZE);

    ts->enc_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*2;
    ts->enc_sample = av_malloc(ts->enc_size + FF_INPUT_BUFFER_PADDING_SIZE);
    memset(ts->enc_sample, 0, ts->enc_size + FF_INPUT_BUFFER_PADDING_SIZE);
  }
}


/**
 * free all buffers used by a transcoder stream
 */
static void
transcoder_stream_destroy(transcoder_stream_t *ts)
{
  av_free(ts->sctx);
  av_free(ts->tctx);

  if(ts->dec_frame)
    av_free(ts->dec_frame);
  if(ts->enc_frame)
    av_free(ts->enc_frame);
  if(ts->scaler)
    sws_freeContext(ts->scaler);

  if(ts->dec_sample)
    av_free(ts->dec_sample);
  if(ts->enc_sample)
    av_free(ts->enc_sample);
}


/**
 * transcode an audio stream
 */
static th_pkt_t *
transcoder_stream_audio(transcoder_stream_t *ts, th_pkt_t *pkt)
{
  th_pkt_t *n = NULL;
  AVPacket packet;
  int length, len, i;
  uint32_t frame_bytes; 

  av_init_packet(&packet);
  packet.data = pktbuf_ptr(pkt->pkt_payload);
  packet.size = pktbuf_len(pkt->pkt_payload);
  packet.pts  = pkt->pkt_pts;
  packet.dts  = pkt->pkt_dts;
  packet.duration = pkt->pkt_duration;

  len = ts->dec_size - ts->dec_offset;
  if(len <= 0) {
    tvhlog(LOG_ERR, "transcode", "Decoder buffer overflow");
    goto cleanup;
  }

  length = avcodec_decode_audio3(ts->sctx, (short*)(ts->dec_sample + ts->dec_offset), &len, &packet);
  if(length <= 0) {
    tvhlog(LOG_ERR, "transcode", "Unable to decode audio (%d)", length);
    goto cleanup;
  }
  ts->dec_offset += len;

  ts->tctx->channels        = ts->sctx->channels > 1 ? 2 : 1;
  ts->tctx->channel_layout  = ts->tctx->channels > 1 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
  ts->tctx->bit_rate        = ts->tctx->channels * 64000;
  ts->tctx->sample_rate     = ts->sctx->sample_rate;
  ts->tctx->sample_fmt      = ts->sctx->sample_fmt;
  ts->tctx->time_base.den   = ts->tctx->sample_rate;
  ts->tctx->time_base.num   = 1;

  if(ts->tctx->codec_id == CODEC_ID_NONE) {

    switch(ts->ttype) {
    case SCT_MPEG2AUDIO:
      ts->tctx->codec_id = CODEC_ID_MP2;
      break;
    case SCT_AAC:
      ts->tctx->codec_id = CODEC_ID_AAC;
      ts->tctx->flags   |= CODEC_FLAG_QSCALE;
      ts->tctx->global_quality = 4*FF_QP2LAMBDA;
      break;
    default:
      ts->tctx->codec_id = CODEC_ID_NONE;
      break;
    }

    AVCodec *codec = avcodec_find_encoder(ts->tctx->codec_id);
    if(!codec || avcodec_open(ts->tctx, codec) < 0) {
      tvhlog(LOG_ERR, "transcode", "Unable to find audio encoder");
      goto cleanup;
    }
  }

  frame_bytes = av_get_bytes_per_sample(ts->tctx->sample_fmt) * 
	ts->tctx->frame_size *
	ts->tctx->channels;

  len = ts->dec_offset;
  ts->enc_offset = 0;
  
  for(i=0; i<=len-frame_bytes; i+=frame_bytes) {
    length = avcodec_encode_audio(ts->tctx, 
				  ts->enc_sample + ts->enc_offset, 
				  ts->enc_size - ts->enc_offset, 
				  (short *)(ts->dec_sample + i));
    if(length < 0) {
      tvhlog(LOG_ERR, "transcode", "Unable to encode audio (%d)", length);
      goto cleanup;
    }

    ts->enc_offset += length;
    ts->dec_offset -= frame_bytes;
  }

  if(ts->dec_offset) {
    memmove(ts->dec_sample, ts->dec_sample + len - ts->dec_offset, ts->dec_offset);
  }

  if(ts->enc_offset) {
    n = pkt_alloc(ts->enc_sample, ts->enc_offset, pkt->pkt_pts, pkt->pkt_dts);
    n->pkt_duration = pkt->pkt_duration;
    n->pkt_commercial = pkt->pkt_commercial;
    n->pkt_componentindex = pkt->pkt_componentindex;
    n->pkt_frametype = pkt->pkt_frametype;
    n->pkt_field = pkt->pkt_field;
    n->pkt_channels = ts->tctx->channels;
    n->pkt_sri = pkt->pkt_sri;
    n->pkt_aspect_num = pkt->pkt_aspect_num;
    n->pkt_aspect_den = pkt->pkt_aspect_den;

    if(ts->tctx->extradata_size) {
      n->pkt_header = pktbuf_alloc(ts->tctx->extradata, ts->tctx->extradata_size);
    }
  }

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
  packet.pts  = pkt->pkt_pts;
  packet.dts  = pkt->pkt_dts;
  packet.duration = pkt->pkt_duration;

  ts->enc_frame->pts = ts->dec_frame->pts = packet.pts;
  ts->sctx->reordered_opaque = packet.pts;
  length = avcodec_decode_video2(ts->sctx, ts->dec_frame, &got_picture, &packet);
  if(length <= 0) {
    tvhlog(LOG_ERR, "transcode", "Unable to decode video (%d)", length);
    goto cleanup;
  }

  if(!got_picture) {
    goto cleanup;
  }

  ts->tctx->sample_aspect_ratio.num = ts->sctx->sample_aspect_ratio.num;
  ts->tctx->sample_aspect_ratio.den = ts->sctx->sample_aspect_ratio.den;
  //ts->tctx->sample_aspect_ratio.num = ts->dec_frame->sample_aspect_ratio.num;
  //ts->tctx->sample_aspect_ratio.den = ts->dec_frame->sample_aspect_ratio.den;

  if(ts->tctx->codec_id == CODEC_ID_NONE) {
    ts->tctx->time_base.den = 25;
    ts->tctx->time_base.num = 1;
    ts->tctx->has_b_frames = ts->sctx->has_b_frames;

    switch(ts->ttype) {
    case SCT_MPEG2VIDEO:
      ts->tctx->codec_id              = CODEC_ID_MPEG2VIDEO;
      ts->tctx->pix_fmt               = PIX_FMT_YUV420P;
      ts->tctx->flags                |= CODEC_FLAG_QSCALE;
      ts->tctx->rc_lookahead          = 0;
      ts->tctx->max_b_frames          = 0;
      ts->tctx->qmin                  = 1;
      ts->tctx->qmax                  = FF_LAMBDA_MAX;
      ts->tctx->global_quality        = 10;
      ts->tctx->flags                |= CODEC_FLAG_GLOBAL_HEADER;
      break;
      /*
    case SCT_VP8:
      ts->tctx->codec_id              = CODEC_ID_VP8;
      ts->tctx->pix_fmt               = PIX_FMT_YUV420P;
      //ts->tctx->flags                |= CODEC_FLAG_QSCALE;
      ts->tctx->rc_lookahead          = 1;
      ts->tctx->max_b_frames          = 1;
      ts->tctx->qmin                  = 1;
      ts->tctx->qmax                  = 63;
      ts->tctx->bit_rate              = 250 * 1000;
      ts->tctx->rc_min_rate           = ts->tctx->bit_rate;
      ts->tctx->rc_max_rate           = ts->tctx->bit_rate;
      ts->enc_frame->quality          = 20;
      break;
      */
    case SCT_H264:
      ts->tctx->codec_id              = CODEC_ID_H264;
      ts->tctx->coder_type            = 0; // coder = 1
      ts->tctx->flags                |= CODEC_FLAG_LOOP_FILTER; // flags=+loop
      ts->tctx->me_cmp               |= FF_CMP_CHROMA; // cmp=+chroma
      ts->tctx->partitions           |= (X264_PART_I8X8 + X264_PART_I4X4 + X264_PART_P8X8 + X264_PART_B8X8); // partitions=+parti8x8+parti4x4+partp8x8+partb8x8
      ts->tctx->me_method             = 0;//ME_HEX; // me_method=hex
      ts->tctx->me_subpel_quality     = 7; // subq=7
      ts->tctx->me_range              = 16; // me_range=16
      ts->tctx->gop_size              = 250; // g=250
      ts->tctx->keyint_min            = 25; // keyint_min=25
      ts->tctx->scenechange_threshold = 40; // sc_threshold=40
      ts->tctx->i_quant_factor        = 0.71; // i_qfactor=0.71
      ts->tctx->b_frame_strategy      = 1; // b_strategy=1
      ts->tctx->qcompress             = 0.6; // qcomp=0.6
      ts->tctx->qmin                  = 20; // qmin=10
      ts->tctx->qmax                  = 32; // qmax=51
      ts->tctx->max_qdiff             = 4; // qdiff=4
      ts->tctx->max_b_frames          = 3; // bf=3
      ts->tctx->refs                  = 3; // refs=3
      ts->tctx->directpred            = 1; // directpred=1
      ts->tctx->trellis               = 1; // trellis=1
      ts->tctx->flags2               |= CODEC_FLAG2_FASTPSKIP; // flags2=+bpyramid+mixed_refs+wpred+dct8x8+fastpskip
      ts->tctx->weighted_p_pred       = 2; // wpredp=2

      ts->tctx->pix_fmt               = PIX_FMT_YUV420P;
      ts->tctx->dsp_mask              = (AV_CPU_FLAG_MMX | AV_CPU_FLAG_MMX2 | AV_CPU_FLAG_SSE);
      ts->tctx->rc_lookahead          = 0;
      ts->tctx->max_b_frames          = 0;
      ts->tctx->b_frame_strategy      = 1;
      ts->tctx->chromaoffset          = 0;
      ts->tctx->thread_count          = 1;
      ts->tctx->crf                   = 10;
      ts->tctx->cqp                   = -1;
      ts->tctx->bit_rate              = 500*1000;
      ts->tctx->bit_rate_tolerance    = 0;
      ts->tctx->flags                |= CODEC_FLAG_GLOBAL_HEADER;
      break;
    default:
      ts->tctx->codec_id = CODEC_ID_NONE;
      break;
    }
    
    AVCodec *codec = avcodec_find_encoder(ts->tctx->codec_id);
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
 
  len = avpicture_get_size(ts->tctx->pix_fmt, ts->sctx->width, ts->sctx->height);
  out = av_malloc(len + FF_INPUT_BUFFER_PADDING_SIZE);
  memset(out, 0, len);

  if(ts->dec_frame->reordered_opaque != AV_NOPTS_VALUE) {
    ts->enc_frame->pts = ts->dec_frame->reordered_opaque;
  }
  else if(ts->sctx->coded_frame && ts->sctx->coded_frame->pts != AV_NOPTS_VALUE) {
    ts->enc_frame->pts = ts->dec_frame->pts;
  }

  length = avcodec_encode_video(ts->tctx, out, len, ts->enc_frame);
  if(length <= 0) {
    tvhlog(LOG_ERR, "transcode", "Unable to encode video (%d)", length);
    goto cleanup;
  }
  
  n = pkt_alloc(out, length, ts->enc_frame->pts, pkt->pkt_dts);

  if(ts->enc_frame->pict_type == FF_I_TYPE)
    n->pkt_frametype = PKT_I_FRAME;
  else if(ts->enc_frame->pict_type == FF_P_TYPE)
    n->pkt_frametype = PKT_P_FRAME;
  else if(ts->enc_frame->pict_type == FF_B_TYPE)
    n->pkt_frametype = PKT_B_FRAME;

  n->pkt_duration = pkt->pkt_duration;

  n->pkt_commercial = pkt->pkt_commercial;
  n->pkt_componentindex = pkt->pkt_componentindex;
  n->pkt_field = pkt->pkt_field;

  n->pkt_channels = pkt->pkt_channels;
  n->pkt_sri = pkt->pkt_sri;

  n->pkt_aspect_num = pkt->pkt_aspect_num;
  n->pkt_aspect_den = pkt->pkt_aspect_den;
  
  if(ts->tctx->coded_frame && ts->tctx->coded_frame->pts != AV_NOPTS_VALUE) {
    n->pkt_pts = ts->tctx->coded_frame->pts;
  }
  if(ts->tctx->extradata_size) {
    n->pkt_header = pktbuf_alloc(ts->tctx->extradata, ts->tctx->extradata_size);
  }

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
      ssc->ssc_type     = t->ts_audio.ttype;
      ssc->ssc_sri      = ssc_src->ssc_sri;
      ssc->ssc_channels = MIN(2, ssc_src->ssc_channels);
      memcpy(ssc->ssc_lang, ssc_src->ssc_lang, 4);

      t->ts_audio.sctx->codec_type = AVMEDIA_TYPE_AUDIO;
      t->ts_audio.tctx->codec_type = AVMEDIA_TYPE_AUDIO;

      tvhlog(LOG_INFO, "transcode", "%s ==> %s", 
	     streaming_component_type2txt(ssc_src->ssc_type),
	     streaming_component_type2txt(ssc->ssc_type));
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
      ssc->ssc_type          = t->ts_video.ttype;
      ssc->ssc_aspect_num    = ssc_src->ssc_aspect_num;
      ssc->ssc_aspect_den    = ssc_src->ssc_aspect_den;
      ssc->ssc_height        = MIN(t->max_height, ssc_src->ssc_height);
      ssc->ssc_width         = ssc->ssc_height * ((double)ssc_src->ssc_width / ssc_src->ssc_height);
      ssc->ssc_frameduration = ssc_src->ssc_frameduration;

      t->ts_video.sctx->codec_type         = AVMEDIA_TYPE_VIDEO;

      t->ts_video.tctx->codec_type         = AVMEDIA_TYPE_VIDEO;
      t->ts_video.tctx->width              = ssc->ssc_width;
      t->ts_video.tctx->height             = ssc->ssc_height;

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
transcoder_create(streaming_target_t *output, 
		  size_t max_width, size_t max_height,
		  streaming_component_type_t v_codec, 
		  streaming_component_type_t a_codec)
{
  transcoder_t *t = calloc(1, sizeof(transcoder_t));

  memset(t, 0, sizeof(transcoder_t));
  t->t_output = output;
  t->max_width = max_width;
  t->max_height = max_height;

  transcoder_stream_create(&t->ts_video, v_codec);
  transcoder_stream_create(&t->ts_audio, a_codec);

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

