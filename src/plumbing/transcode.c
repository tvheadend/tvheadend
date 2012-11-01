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

#include <unistd.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/dict.h>

#include "tvheadend.h"
#include "streaming.h"
#include "service.h"
#include "packet.h"
#include "transcode.h"

#define MAX_PASSTHROUGH_STREAMS 31

/**
 * Reference to a transcoder stream
 */
typedef struct transcoder_stream {
  streaming_component_type_t stype; // source
  streaming_component_type_t ttype; // target

  AVCodecContext *sctx; // source
  AVCodecContext *tctx; // target

  int sindex; // refers to the source stream index
  int tindex; // refers to the target stream index

  AVCodec *scodec; // source
  AVCodec *tcodec; // target

  struct SwsContext *scaler; // used for scaling
  AVFrame           *dec_frame; // decoding buffer for video stream
  AVFrame           *enc_frame; // encoding buffer for video stream

  uint8_t           *dec_sample; // decoding buffer for audio stream
  uint32_t           dec_size;
  uint32_t           dec_offset;

  uint8_t           *enc_sample; // encoding buffer for audio stream
  uint32_t           enc_size;

  uint64_t           enc_pts;
  uint64_t           dec_pts;

  uint64_t           drops;
  streaming_target_t *target;
} transcoder_stream_t;

typedef struct transcoder_passthrough {
  int sindex; // refers to the source stream index
  int tindex; // refers to the target stream index
  streaming_target_t *target;
} transcoder_passthrough_t;

typedef struct transcoder {
  streaming_target_t t_input;  // must be first
  streaming_target_t *t_output;

  // client preferences for the target stream
  streaming_component_type_t atype; // audio
  streaming_component_type_t vtype; // video
  streaming_component_type_t stype; // subtitle
  size_t max_height;

  // Audio & Video stream transcoders
  transcoder_stream_t *ts_audio;
  transcoder_stream_t *ts_video;

  // Passthrough streams
  transcoder_passthrough_t pt_streams[MAX_PASSTHROUGH_STREAMS+1];
  uint8_t pt_count;

  //for the PID-regulator
  int feedback_error;
  int feedback_error_sum;
  time_t feedback_clock;
} transcoder_t;


/**
 * free all buffers used by a transcoder stream
 */
static void
transcoder_stream_destroy(transcoder_stream_t *ts)
{
  if(ts->sctx) {
    avcodec_close(ts->sctx);
    av_free(ts->sctx);
  }

  if(ts->tctx) {
    avcodec_close(ts->tctx);
    av_free(ts->tctx);
  }

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

  free(ts);
}

/**
 * translate a component type to a libavcodec id
 */
static enum CodecID
transcoder_get_codec_id(streaming_component_type_t type)
{
  enum CodecID codec_id = CODEC_ID_NONE;

  switch(type) {
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
  case SCT_MPEG4VIDEO:
    codec_id = CODEC_ID_MPEG4;
    break;
  case SCT_VP8:
    codec_id = CODEC_ID_VP8;
    break;
  case SCT_VORBIS:
    codec_id = CODEC_ID_VORBIS;
    break;
  default:
    codec_id = CODEC_ID_NONE;
    break;
  }

  return codec_id;
}

/**
 * find the codecs and allocate buffers used by a transcoder stream
 */
static transcoder_stream_t*
transcoder_stream_create(streaming_component_type_t stype, streaming_component_type_t ttype)
{
  AVCodec *scodec = NULL;
  AVCodec *tcodec = NULL;
  transcoder_stream_t *ts = NULL;
  enum CodecID codec_id = CODEC_ID_NONE;

  codec_id = transcoder_get_codec_id(stype);
  if(codec_id == CODEC_ID_NONE) {
    tvhlog(LOG_ERR, "transcode", "Unsupported source codec %s", 
	   streaming_component_type2txt(stype));
    return NULL;
  }

  scodec = avcodec_find_decoder(codec_id);
  if(!scodec) {
    tvhlog(LOG_ERR, "transcode", "Unable to find %s decoder", 
	   streaming_component_type2txt(stype));
    return NULL;
  }

  tvhlog(LOG_DEBUG, "transcode", "Using decoder %s", scodec->name);

  codec_id = transcoder_get_codec_id(ttype);
  if(codec_id == CODEC_ID_NONE) {
    tvhlog(LOG_ERR, "transcode", "Unsupported target codec %s", 
	   streaming_component_type2txt(ttype));
    return NULL;
  }

  tcodec = avcodec_find_encoder(codec_id);
  if(!tcodec) {
    tvhlog(LOG_ERR, "transcode", "Unable to find %s encoder", 
	   streaming_component_type2txt(ttype));
    return NULL;
  }
 
  tvhlog(LOG_DEBUG, "transcode", "Using encoder %s", tcodec->name);

  ts = malloc(sizeof(transcoder_stream_t));
  memset(ts, 0, sizeof(transcoder_stream_t));

  ts->stype = stype;
  ts->ttype = ttype;

  ts->scodec = scodec;
  ts->tcodec = tcodec;

  ts->sctx = avcodec_alloc_context();
  ts->sctx->thread_count = sysconf(_SC_NPROCESSORS_ONLN);

  ts->tctx = avcodec_alloc_context();
  ts->tctx->thread_count = sysconf(_SC_NPROCESSORS_ONLN);

  if(SCT_ISVIDEO(stype)) {
    ts->dec_frame = avcodec_alloc_frame();
    ts->enc_frame = avcodec_alloc_frame();

    avcodec_get_frame_defaults(ts->dec_frame);
    avcodec_get_frame_defaults(ts->enc_frame);

    ts->sctx->codec_type = AVMEDIA_TYPE_VIDEO;
    ts->tctx->codec_type = AVMEDIA_TYPE_VIDEO;
  } else if(SCT_ISAUDIO(stype)) {
    ts->dec_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*2;
    ts->dec_sample = av_malloc(ts->dec_size + FF_INPUT_BUFFER_PADDING_SIZE);
    memset(ts->dec_sample, 0, ts->dec_size + FF_INPUT_BUFFER_PADDING_SIZE);

    ts->enc_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*2;
    ts->enc_sample = av_malloc(ts->enc_size + FF_INPUT_BUFFER_PADDING_SIZE);
    memset(ts->enc_sample, 0, ts->enc_size + FF_INPUT_BUFFER_PADDING_SIZE);

    ts->sctx->codec_type = AVMEDIA_TYPE_AUDIO;
    ts->tctx->codec_type = AVMEDIA_TYPE_AUDIO;
  }

  return ts;
}


/**
 *
 */
static void
transcoder_pkt_deliver(streaming_target_t *st, th_pkt_t *pkt)
{
  streaming_message_t *sm;

  sm = streaming_msg_create_pkt(pkt);
  streaming_target_deliver2(st, sm);
  pkt_ref_dec(pkt);
}


/**
 *
 */
static void
transcoder_stream_passthrough(transcoder_passthrough_t *pt, th_pkt_t *pkt)
{
  th_pkt_t *n;

  n = pkt_alloc(pktbuf_ptr(pkt->pkt_payload), 
		pktbuf_len(pkt->pkt_payload), 
		pkt->pkt_pts, 
		pkt->pkt_dts);

  n->pkt_duration = pkt->pkt_duration;
  n->pkt_commercial = pkt->pkt_commercial;
  n->pkt_componentindex = pt->tindex;
  n->pkt_frametype = pkt->pkt_frametype;
  n->pkt_field = pkt->pkt_field;
  n->pkt_channels = pkt->pkt_channels;
  n->pkt_sri = pkt->pkt_sri;
  n->pkt_aspect_num = pkt->pkt_aspect_num;
  n->pkt_aspect_den = pkt->pkt_aspect_den;

  transcoder_pkt_deliver(pt->target, n);
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

  // Open the decoder
  if(ts->sctx->codec_id == CODEC_ID_NONE) {
    ts->sctx->codec_id = ts->scodec->id;

    if(avcodec_open(ts->sctx, ts->scodec) < 0) {
      tvhlog(LOG_ERR, "transcode", "Unable to open %s decoder", ts->scodec->name);
      // Disable the stream
      ts->sindex = 0;
      goto cleanup;
    }

    ts->dec_pts = pkt->pkt_pts;
  }

  if(pkt->pkt_pts > ts->dec_pts + ts->drops) {
    ts->drops += (pkt->pkt_pts - ts->dec_pts);
    tvhlog(LOG_WARNING, "transcode", "Detected framedrops");
  }

  av_init_packet(&packet);
  packet.data = pktbuf_ptr(pkt->pkt_payload);
  packet.size = pktbuf_len(pkt->pkt_payload);
  packet.pts  = pkt->pkt_pts;
  packet.dts  = pkt->pkt_dts;
  packet.duration = pkt->pkt_duration;

  len = ts->dec_size - ts->dec_offset;
  if(len <= 0) {
    tvhlog(LOG_ERR, "transcode", "Decoder buffer overflow");
    ts->drops += pkt->pkt_duration;
    goto cleanup;
  }

  length = avcodec_decode_audio3(ts->sctx, (short*)(ts->dec_sample + ts->dec_offset), &len, &packet);
  if(length <= 0) {
    tvhlog(LOG_ERR, "transcode", "Unable to decode audio (%d)", length);
    ts->drops += pkt->pkt_duration;
    goto cleanup;
  }
  ts->dec_offset += len;
  ts->dec_pts += pkt->pkt_duration;

  // Set parameters that are unknown until the first packet has been decoded.
  ts->tctx->channels        = ts->sctx->channels > 1 ? 2 : 1;
  ts->tctx->channel_layout  = ts->tctx->channels > 1 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
  ts->tctx->bit_rate        = ts->tctx->channels * 64000;
  ts->tctx->sample_rate     = ts->sctx->sample_rate;
  ts->tctx->sample_fmt      = ts->sctx->sample_fmt;
  ts->tctx->time_base.den   = 90000;
  ts->tctx->time_base.num   = 1;

  // Open the encoder
  if(ts->tctx->codec_id == CODEC_ID_NONE) {
    switch(ts->ttype) {
    case SCT_MPEG2AUDIO:
      ts->tctx->codec_id = CODEC_ID_MP2;
      break;
    case SCT_AAC:
      ts->tctx->codec_id       = CODEC_ID_AAC;
      ts->tctx->flags         |= CODEC_FLAG_QSCALE;
      ts->tctx->global_quality = 4*FF_QP2LAMBDA;
      break;
    case SCT_VORBIS:
      ts->tctx->codec_id       = CODEC_ID_VORBIS;
      ts->tctx->flags         |= CODEC_FLAG_QSCALE;
      ts->tctx->flags         |= CODEC_FLAG_GLOBAL_HEADER;
      ts->tctx->channels       = 2; // Only stereo suported
      ts->tctx->global_quality = 4*FF_QP2LAMBDA;
      break;
    default:
      ts->tctx->codec_id = CODEC_ID_NONE;
      break;
    }

    if(avcodec_open(ts->tctx, ts->tcodec) < 0) {
      tvhlog(LOG_ERR, "transcode", "Unable to open %s encoder", ts->tcodec->name);
      // Disable the stream
      ts->sindex = 0;
      goto cleanup;
    }

    ts->enc_pts = pkt->pkt_pts;
  }

  frame_bytes = av_get_bytes_per_sample(ts->tctx->sample_fmt) * 
	ts->tctx->frame_size *
	ts->tctx->channels;

  len = ts->dec_offset;

  for(i=0; i<=len-frame_bytes; i+=frame_bytes) {
    length = avcodec_encode_audio(ts->tctx,
				  ts->enc_sample,
				  ts->enc_size,
				  (short *)(ts->dec_sample + i));
    if(length < 0) {
      tvhlog(LOG_ERR, "transcode", "Unable to encode audio (%d)", length);
      goto cleanup;
    } else if(length) {
      n = pkt_alloc(ts->enc_sample, length, ts->enc_pts + ts->drops, ts->enc_pts + ts->drops);

      if(ts->tctx->coded_frame && ts->tctx->coded_frame->pts != AV_NOPTS_VALUE) {
	n->pkt_duration = ts->tctx->coded_frame->pts - ts->enc_pts;
      } else {
	n->pkt_duration = frame_bytes*90000 / (2 * ts->tctx->channels * ts->tctx->sample_rate);
      }

      n->pkt_commercial = pkt->pkt_commercial;
      n->pkt_componentindex = ts->tindex;
      n->pkt_frametype = pkt->pkt_frametype;
      n->pkt_field = pkt->pkt_field;
      n->pkt_channels = ts->tctx->channels;
      n->pkt_sri = pkt->pkt_sri;
      n->pkt_aspect_num = pkt->pkt_aspect_num;
      n->pkt_aspect_den = pkt->pkt_aspect_den;

      if(ts->tctx->extradata_size) {
	n->pkt_header = pktbuf_alloc(ts->tctx->extradata, ts->tctx->extradata_size);
      }

      ts->enc_pts += n->pkt_duration;

      transcoder_pkt_deliver(ts->target, n);
    }

    ts->dec_offset -= frame_bytes;
  }

  if(ts->dec_offset) {
    memmove(ts->dec_sample, ts->dec_sample + len - ts->dec_offset, ts->dec_offset);
  }

 cleanup:
  av_free_packet(&packet);
  return n;
}


/**
 * transcode a video stream
 */
static void
transcoder_stream_video(transcoder_stream_t *ts, th_pkt_t *pkt)
{
  uint8_t *buf = NULL;
  uint8_t *out = NULL;
  uint8_t *deint = NULL;
  th_pkt_t *n = NULL;
  AVDictionary *opts = NULL;
  AVPacket packet;
  AVPicture deint_pic;
  int length, len;
  int got_picture;

  // Open the decoder
  if(ts->sctx->codec_id == CODEC_ID_NONE) {
    ts->sctx->codec_id = ts->scodec->id;

    if(avcodec_open(ts->sctx, ts->scodec) < 0) {
      tvhlog(LOG_ERR, "transcode", "Unable to open %s decoder", ts->scodec->name);
      // Disable the stream
      ts->sindex = 0;
      goto cleanup;
    }
  }

  av_init_packet(&packet);
  packet.data = pktbuf_ptr(pkt->pkt_payload);
  packet.size = pktbuf_len(pkt->pkt_payload);
  packet.pts  = pkt->pkt_pts;
  packet.dts  = pkt->pkt_dts;
  packet.duration = pkt->pkt_duration;

  ts->enc_frame->pts = packet.pts;
  ts->enc_frame->pkt_dts = packet.dts;
  ts->enc_frame->pkt_pts = packet.pts;

  ts->dec_frame->pts = packet.pts;
  ts->dec_frame->pkt_dts = packet.dts;
  ts->dec_frame->pkt_pts = packet.pts;

  ts->sctx->reordered_opaque = packet.pts;

  length = avcodec_decode_video2(ts->sctx, ts->dec_frame, &got_picture, &packet);
  if(length <= 0) {
    tvhlog(LOG_ERR, "transcode", "Unable to decode video (%d)", length);
    goto cleanup;
  }

  if(!got_picture) {
    goto cleanup;
  }

  // Set codec parameters that are unknown until first packet has been decoded
  ts->tctx->sample_aspect_ratio.num = ts->sctx->sample_aspect_ratio.num;
  ts->tctx->sample_aspect_ratio.den = ts->sctx->sample_aspect_ratio.den;
  ts->tctx->sample_aspect_ratio.num = ts->dec_frame->sample_aspect_ratio.num;
  ts->tctx->sample_aspect_ratio.den = ts->dec_frame->sample_aspect_ratio.den;

  //Open the encoder
  if(ts->tctx->codec_id == CODEC_ID_NONE) {
 
    // Common settings
    ts->tctx->time_base.den = 25;
    ts->tctx->time_base.num = 1;
    ts->tctx->has_b_frames = ts->sctx->has_b_frames;

    switch(ts->ttype) {
    case SCT_MPEG2VIDEO:
      ts->tctx->codec_id       = CODEC_ID_MPEG2VIDEO;
      ts->tctx->pix_fmt        = PIX_FMT_YUV420P;
      ts->tctx->bit_rate       = 2 * ts->tctx->width * ts->tctx->height;
      ts->tctx->flags         |= CODEC_FLAG_GLOBAL_HEADER;

      ts->tctx->qmin           = 1;
      ts->tctx->qmax           = FF_LAMBDA_MAX;

      ts->tctx->bit_rate       = 2 * ts->tctx->width * ts->tctx->height;
      ts->tctx->rc_buffer_size = 8 * 1024 * 224;
      ts->tctx->rc_max_rate    = 2 * ts->tctx->bit_rate;

      break;
    case SCT_MPEG4VIDEO:
      ts->tctx->codec_id       = CODEC_ID_MPEG4;
      ts->tctx->pix_fmt        = PIX_FMT_YUV420P;
      ts->tctx->bit_rate       = 2 * ts->tctx->width * ts->tctx->height;
      //ts->tctx->flags         |= CODEC_FLAG_GLOBAL_HEADER;

      ts->tctx->qmin = 1;
      ts->tctx->qmax = 5;

      ts->tctx->bit_rate       = 2 * ts->tctx->width * ts->tctx->height;
      ts->tctx->rc_buffer_size = 8 * 1024 * 224;
      ts->tctx->rc_max_rate    = 2 * ts->tctx->bit_rate;
      break;
      
    case SCT_VP8:
      ts->tctx->codec_id       = CODEC_ID_VP8;
      ts->tctx->pix_fmt        = PIX_FMT_YUV420P;

      ts->tctx->qmin = 10;
      ts->tctx->qmax = 20;

      av_dict_set(&opts, "quality",  "realtime", 0);

      ts->tctx->bit_rate       = 3 * ts->tctx->width * ts->tctx->height;
      ts->tctx->rc_buffer_size = 8 * 1024 * 224;
      ts->tctx->rc_max_rate    = 2 * ts->tctx->bit_rate;
      break;

    case SCT_H264:
      ts->tctx->codec_id       = CODEC_ID_H264;
      ts->tctx->pix_fmt        = PIX_FMT_YUV420P;
      ts->tctx->flags          |= CODEC_FLAG_GLOBAL_HEADER;

      // Qscale difference between I-frames and P-frames. 
      // Note: -i_qfactor is handled a little differently than --ipratio. 
      // Recommended: -i_qfactor 0.71
      ts->tctx->i_quant_factor = 0.71;

      // QP curve compression: 0.0 => CBR, 1.0 => CQP.
      // Recommended default: -qcomp 0.60
      ts->tctx->qcompress = 0.6;

      // Minimum quantizer. Doesn't need to be changed.
      // Recommended default: -qmin 10
      ts->tctx->qmin = 10;

      // Maximum quantizer. Doesn't need to be changed.
      // Recommended default: -qmax 51
      ts->tctx->qmax = 30;

      av_dict_set(&opts, "preset",  "medium", 0);
      av_dict_set(&opts, "profile", "baseline", 0);

      ts->tctx->bit_rate       = 2 * ts->tctx->width * ts->tctx->height;
      ts->tctx->rc_buffer_size = 8 * 1024 * 224;
      ts->tctx->rc_max_rate    = 2 * ts->tctx->rc_buffer_size;

      break;
    default:
      ts->tctx->codec_id = CODEC_ID_NONE;
      break;
    }

    if(avcodec_open2(ts->tctx, ts->tcodec, &opts) < 0) {
      tvhlog(LOG_ERR, "transcode", "Unable to open %s encoder", ts->tcodec->name);
      // Disable the stream
      ts->sindex = 0;
      goto cleanup;
    }
  }

  len = avpicture_get_size(ts->sctx->pix_fmt, ts->sctx->width, ts->sctx->height);
  deint = av_malloc(len);

  avpicture_fill(&deint_pic,
		 deint, 
		 ts->sctx->pix_fmt, 
		 ts->sctx->width, 
		 ts->sctx->height);

  if(-1 == avpicture_deinterlace(&deint_pic,
				 (AVPicture *)ts->dec_frame,
				 ts->sctx->pix_fmt,
				 ts->sctx->width,
				 ts->sctx->height)) {
    tvhlog(LOG_ERR, "transcode", "Cannot deinterlace frame");
    goto cleanup;
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
            (const uint8_t * const*)deint_pic.data, 
            deint_pic.linesize,
            0, 
            ts->sctx->height, 
            ts->enc_frame->data, 
            ts->enc_frame->linesize);
 
  len = avpicture_get_size(ts->tctx->pix_fmt, ts->sctx->width, ts->sctx->height);
  out = av_malloc(len + FF_INPUT_BUFFER_PADDING_SIZE);
  memset(out, 0, len);

  ts->enc_frame->pkt_pts = ts->dec_frame->pkt_pts;
  ts->enc_frame->pkt_dts = ts->dec_frame->pkt_dts;

  if(ts->dec_frame->reordered_opaque != AV_NOPTS_VALUE) {
    ts->enc_frame->pts = ts->dec_frame->reordered_opaque;
  }
  else if(ts->sctx->coded_frame && ts->sctx->coded_frame->pts != AV_NOPTS_VALUE) {
    ts->enc_frame->pts = ts->dec_frame->pts;
  }
 
  length = avcodec_encode_video(ts->tctx, out, len, ts->enc_frame);
  if(length <= 0) {
    if(length)
      tvhlog(LOG_ERR, "transcode", "Unable to encode video (%d)", length);

    goto cleanup;
  }
  
  n = pkt_alloc(out, length, ts->enc_frame->pkt_pts, ts->enc_frame->pkt_dts);

  if(ts->enc_frame->pict_type == AV_PICTURE_TYPE_I)
    n->pkt_frametype = PKT_I_FRAME;
  else if(ts->enc_frame->pict_type == AV_PICTURE_TYPE_P)
    n->pkt_frametype = PKT_P_FRAME;
  else if(ts->enc_frame->pict_type == AV_PICTURE_TYPE_B)
    n->pkt_frametype = PKT_B_FRAME;

  n->pkt_duration = pkt->pkt_duration;

  n->pkt_commercial = pkt->pkt_commercial;
  n->pkt_componentindex = ts->tindex;
  n->pkt_field = pkt->pkt_field;

  n->pkt_channels = pkt->pkt_channels;
  n->pkt_sri = pkt->pkt_sri;

  n->pkt_aspect_num = pkt->pkt_aspect_num;
  n->pkt_aspect_den = pkt->pkt_aspect_den;
  
  if(ts->tctx->coded_frame && ts->tctx->coded_frame->pts != AV_NOPTS_VALUE) {
    n->pkt_dts -= n->pkt_pts;
    n->pkt_pts = ts->tctx->coded_frame->pts;
    n->pkt_dts += n->pkt_pts;
  }
  if(ts->tctx->extradata_size) {
    n->pkt_header = pktbuf_alloc(ts->tctx->extradata, ts->tctx->extradata_size);
  }

  transcoder_pkt_deliver(ts->target, n);

 cleanup:
  av_free_packet(&packet);
  if(buf)
    av_free(buf);
  if(out)
    av_free(out);
  if(deint)
    av_free(deint);

}


/**
 * transcode a packet
 */
static void
transcoder_packet(transcoder_t *t, th_pkt_t *pkt)
{
  transcoder_stream_t *ts = NULL;
  int i = 0;

  if(t->ts_video && pkt->pkt_componentindex == t->ts_video->sindex) {
    ts = t->ts_video;
  } else if (t->ts_audio && pkt->pkt_componentindex == t->ts_audio->sindex) {
    ts = t->ts_audio;
  }

  if(ts && ts->sctx->codec_type == AVMEDIA_TYPE_AUDIO) {
    transcoder_stream_audio(ts, pkt);
    return;
  } else if(ts && ts->sctx->codec_type == AVMEDIA_TYPE_VIDEO) {
    transcoder_stream_video(ts, pkt);
    return;
  }

  // Look for passthrough streams
  for(i=0; i<t->pt_count; i++) {
    if(t->pt_streams[i].sindex == pkt->pkt_componentindex) {
      transcoder_stream_passthrough(&t->pt_streams[i], pkt);
      return;
    }
  }
}

#define IS_PASSTHROUGH(t, ssc) ((SCT_ISAUDIO(ssc->ssc_type) && t->atype == SCT_UNKNOWN) || \
				(SCT_ISVIDEO(ssc->ssc_type) && t->vtype == SCT_UNKNOWN) || \
				(SCT_ISSUBTITLE(ssc->ssc_type) && t->stype == SCT_UNKNOWN))


/**
 * Figure out how many streams we will use.
 */
static int
transcoder_get_stream_count(transcoder_t *t, streaming_start_t *ss) {
  int i = 0;
  int video = 0;
  int audio = 0;
  int passthrough = 0;
  streaming_start_component_t *ssc = NULL;

  for(i = 0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    if(IS_PASSTHROUGH(t, ssc))
      passthrough++;
    else if(SCT_ISVIDEO(ssc->ssc_type))
      video = 1;
    else if(SCT_ISAUDIO(ssc->ssc_type))
      audio = 1;
  }

  passthrough = MIN(passthrough, MAX_PASSTHROUGH_STREAMS);

  return (video + audio + passthrough);
}



/**
 * initializes eatch transcoding stream
 */
static streaming_start_t *
transcoder_start(transcoder_t *t, streaming_start_t *src)
{
  int i = 0;
  int j = 0;
  int stream_count = transcoder_get_stream_count(t, src);
  streaming_start_t *ss = NULL;

  t->feedback_clock = dispatch_clock;

  ss = malloc(sizeof(streaming_start_t) + sizeof(streaming_start_component_t) * stream_count);
  memset(ss, 0, sizeof(streaming_start_t) + sizeof(streaming_start_component_t) * stream_count);

  ss->ss_num_components = stream_count;
  ss->ss_refcount = 1;
  ss->ss_pcr_pid = src->ss_pcr_pid;
  service_source_info_copy(&ss->ss_si, &src->ss_si);

  for(i = 0; i < src->ss_num_components; i++) {
    streaming_start_component_t *ssc_src = &src->ss_components[i];

    // Sanity check, should not happend
    if(j >= stream_count) {
      break;
    }

    streaming_start_component_t *ssc = &ss->ss_components[j];

    if (IS_PASSTHROUGH(t, ssc_src)) {
      int pt_index = t->pt_count++;

      t->pt_streams[pt_index].target = t->t_output;

      // Use same index for both source and target, globalheaders seems to need it.
      t->pt_streams[pt_index].sindex = ssc_src->ssc_index;
      t->pt_streams[pt_index].tindex = ssc_src->ssc_index;

      ssc->ssc_index          = ssc_src->ssc_index;
      ssc->ssc_type           = ssc_src->ssc_type;
      ssc->ssc_composition_id = ssc_src->ssc_composition_id;
      ssc->ssc_ancillary_id   = ssc_src->ssc_ancillary_id;
      ssc->ssc_pid            = ssc_src->ssc_pid;
      ssc->ssc_width          = ssc_src->ssc_width;
      ssc->ssc_height         = ssc_src->ssc_height;
      ssc->ssc_aspect_num     = ssc_src->ssc_aspect_num;
      ssc->ssc_aspect_den     = ssc_src->ssc_aspect_den;
      ssc->ssc_sri            = ssc_src->ssc_sri;
      ssc->ssc_channels       = ssc_src->ssc_channels;
      ssc->ssc_disabled       = ssc_src->ssc_disabled;

      memcpy(ssc->ssc_lang, ssc_src->ssc_lang, 4);
      j++;

      tvhlog(LOG_INFO, "transcode", "%d:%s ==> %d:PASSTHROUGH", 
	     t->pt_streams[pt_index].sindex,
	     streaming_component_type2txt(ssc_src->ssc_type),
	     t->pt_streams[pt_index].tindex);

    } else if (!t->ts_audio && SCT_ISAUDIO(ssc_src->ssc_type) && SCT_ISAUDIO(t->atype)) {
      transcoder_stream_t *ts = transcoder_stream_create(ssc_src->ssc_type, t->atype);
      if(!ts)
	continue;

      ts->target = t->t_output;

      // Use same index for both source and target, globalheaders seems to need it.
      ts->sindex = ssc_src->ssc_index;
      ts->tindex = ssc_src->ssc_index;

      ssc->ssc_index    = ts->tindex;
      ssc->ssc_type     = ts->ttype;
      ssc->ssc_sri      = ssc_src->ssc_sri;
      ssc->ssc_channels = MIN(2, ssc_src->ssc_channels);
      memcpy(ssc->ssc_lang, ssc_src->ssc_lang, 4);

      tvhlog(LOG_INFO, "transcode", "%d:%s ==> %d:%s", 
	     ts->sindex,
	     streaming_component_type2txt(ts->stype),
	     ts->tindex,
	     streaming_component_type2txt(ts->ttype));

      j++;
      t->ts_audio = ts;

    } else if (!t->ts_video && SCT_ISVIDEO(ssc_src->ssc_type) && SCT_ISVIDEO(t->vtype)) {
      transcoder_stream_t *ts = transcoder_stream_create(ssc_src->ssc_type, t->vtype);
      if(!ts)
	continue;

      ts->target = t->t_output;

      // Use same index for both source and target, globalheaders seems to need it.
      ts->sindex = ssc_src->ssc_index;
      ts->tindex = ssc_src->ssc_index;

      ssc->ssc_index         = ts->tindex;
      ssc->ssc_type          = ts->ttype;
      ssc->ssc_aspect_num    = ssc_src->ssc_aspect_num;
      ssc->ssc_aspect_den    = ssc_src->ssc_aspect_den;
      ssc->ssc_frameduration = ssc_src->ssc_frameduration;
      ssc->ssc_height        = MIN(t->max_height, ssc_src->ssc_height);
      if(ssc->ssc_height&1) // Must be even
	ssc->ssc_height++;

      ssc->ssc_width         = ssc->ssc_height * ((double)ssc_src->ssc_width / ssc_src->ssc_height);
      if(ssc->ssc_width&1) // Must be even
	ssc->ssc_width++;

      ts->tctx->width        = ssc->ssc_width;
      ts->tctx->height       = ssc->ssc_height;

      tvhlog(LOG_INFO, "transcode", "%d:%s %dx%d ==> %d:%s %dx%d", 
	     ts->sindex,
	     streaming_component_type2txt(ts->stype),
	     ssc_src->ssc_width,
	     ssc_src->ssc_height,
	     ts->tindex,
	     streaming_component_type2txt(ts->ttype),
	     ssc->ssc_width,
	     ssc->ssc_height);

      j++;
      t->ts_video = ts;
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
  if(t->ts_audio) {
    transcoder_stream_destroy(t->ts_audio);
    t->ts_audio = NULL;
  }

  if(t->ts_video) {
    transcoder_stream_destroy(t->ts_video);
    t->ts_video = NULL;
  }
}


/**
 * handle a streaming message
 */
static void
transcoder_input(void *opaque, streaming_message_t *sm)
{
  transcoder_t *t;
  streaming_start_t *ss;
  th_pkt_t *pkt;

  t = opaque;

  switch(sm->sm_type) {
  case SMT_PACKET:
    pkt = pkt_merge_header(sm->sm_data);
    transcoder_packet(t, pkt);
    pkt_ref_dec(pkt);
    break;

  case SMT_START:
    ss = transcoder_start(t, sm->sm_data);
    streaming_start_unref(sm->sm_data);
    sm->sm_data = ss;

    streaming_target_deliver2(t->t_output, sm);
    break;

  case SMT_STOP:
    transcoder_stop(t);
    // Fallthrough

  case SMT_EXIT:
  case SMT_SERVICE_STATUS:
  case SMT_SIGNAL_STATUS:
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
		  size_t max_resolution,
		  streaming_component_type_t vtype, 
		  streaming_component_type_t atype,
		  streaming_component_type_t stype
		  )
{
  transcoder_t *t = calloc(1, sizeof(transcoder_t));

  memset(t, 0, sizeof(transcoder_t));
  t->t_output = output;
  t->max_height = max_resolution;
  t->atype = atype;
  t->vtype = vtype;
  t->stype = stype;

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
  transcoder_stream_t *ts = t->ts_video;

  if(!ts || !ts->tctx)
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

  q = MIN(q, ts->tctx->qmin);
  q = MAX(q, ts->tctx->qmax);

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

  transcoder_stop(t);
  free(t);
}

/**
 *
 */
static void
transcoder_log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
    char message[8192];
    char *nl;
    char *l;

    memset(message, 0, sizeof(message));
    vsnprintf(message, sizeof(message), fmt, vl);

    l = message;

    if(level == AV_LOG_DEBUG)
      level = LOG_DEBUG;
    else if(level == AV_LOG_VERBOSE)
      level = LOG_INFO;
    else if(level == AV_LOG_INFO)
      level = LOG_NOTICE;
    else if(level == AV_LOG_WARNING)
      level = LOG_WARNING;
    else if(level == AV_LOG_ERROR)
      level = LOG_ERR;
    else if(level == AV_LOG_FATAL)
      level = LOG_CRIT;
    else if(level == AV_LOG_PANIC)
      level = LOG_EMERG;

    while(l < message + sizeof(message)) {
      nl = strstr(l, "\n");
      if(nl)
	*nl = '\0';

      if(!strlen(l))
	break;

      tvhlog(level, "libav", "%s", l);

      l += strlen(message);

      if(!nl)
	break;
    }
}


/**
 * 
 */ 
void
transcoder_init(void)
{
  av_log_set_callback(transcoder_log_callback);
  av_log_set_level(AV_LOG_VERBOSE);
  av_register_all();
}

