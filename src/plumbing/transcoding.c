/**
 *  Transcoding
 *  Copyright (C) 2013 John Törnblom
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
#include "transcoding.h"
#include "libav.h"

LIST_HEAD(transcoder_stream_list, transcoder_stream);

typedef struct transcoder_stream {
  int                           ts_index;
  streaming_component_type_t    ts_type;
  streaming_target_t           *ts_target;
  LIST_ENTRY(transcoder_stream) ts_link;

  void (*ts_handle_pkt) (struct transcoder_stream *, th_pkt_t *);
  void (*ts_destroy)    (struct transcoder_stream *);
} transcoder_stream_t;


typedef struct audio_stream {
  transcoder_stream_t;

  AVCodecContext *aud_ictx;
  AVCodec        *aud_icodec;

  AVCodecContext *aud_octx;
  AVCodec        *aud_ocodec;

  uint8_t        *aud_dec_sample;
  uint32_t        aud_dec_size;
  uint32_t        aud_dec_offset;

  uint8_t        *aud_enc_sample; 
  uint32_t        aud_enc_size;

  uint64_t        aud_dec_pts;
  uint64_t        aud_enc_pts;

  int8_t          aud_channels;
  int32_t         aud_bitrate;
} audio_stream_t;


typedef struct video_stream {
  transcoder_stream_t;

  AVCodecContext            *vid_ictx;
  AVCodec                   *vid_icodec;

  AVCodecContext            *vid_octx;
  AVCodec                   *vid_ocodec;

  AVFrame                   *vid_dec_frame;
  struct SwsContext         *vid_scaler;
  AVFrame                   *vid_enc_frame;

  int16_t                    vid_width;
  int16_t                    vid_height;
} video_stream_t;


typedef struct subtitle_stream {
  transcoder_stream_t;

  AVCodecContext            *sub_ictx;
  AVCodec                   *sub_icodec;

  AVCodecContext            *sub_octx;
  AVCodec                   *sub_ocodec;
} subtitle_stream_t;



typedef struct transcoder {
  streaming_target_t  t_input;  // must be first
  streaming_target_t *t_output;

  transcoder_props_t            t_props;
  struct transcoder_stream_list t_stream_list;
} transcoder_t;



#define WORKING_ENCODER(x) (x == CODEC_ID_H264 || x == CODEC_ID_MPEG2VIDEO || \
			    x == CODEC_ID_AAC || x == CODEC_ID_MP2)


uint32_t transcoding_enabled = 0;

/**
 * 
 */
static AVCodec *
transcoder_get_decoder(streaming_component_type_t ty)
{
  enum CodecID codec_id;
  AVCodec *codec;

  codec_id = streaming_component_type2codec_id(ty);
  if (codec_id == CODEC_ID_NONE) {
    tvhlog(LOG_ERR, "transcode", "Unsupported input codec %s", 
	   streaming_component_type2txt(ty));
    return NULL;
  }

  codec = avcodec_find_decoder(codec_id);
  if (!codec) {
    tvhlog(LOG_ERR, "transcode", "Unable to find %s decoder", 
	   streaming_component_type2txt(ty));
    return NULL;
  }

  tvhlog(LOG_DEBUG, "transcode", "Using decoder %s", codec->name);

  return codec;
}


/**
 * 
 */
static AVCodec *
transcoder_get_encoder(streaming_component_type_t ty)
{
  enum CodecID codec_id;
  AVCodec *codec;

  codec_id = streaming_component_type2codec_id(ty);
  if (codec_id == CODEC_ID_NONE) {
    tvhlog(LOG_ERR, "transcode", "Unable to find %s codec", 
	   streaming_component_type2txt(ty));
    return NULL;
  }

  if (!WORKING_ENCODER(codec_id)) {
    tvhlog(LOG_WARNING, "transcode", "Unsupported output codec %s", 
	   streaming_component_type2txt(ty));
    return NULL;
  }

  codec = avcodec_find_encoder(codec_id);
  if (!codec) {
    tvhlog(LOG_ERR, "transcode", "Unable to find %s encoder", 
	   streaming_component_type2txt(ty));
    return NULL;
  }

  tvhlog(LOG_DEBUG, "transcode", "Using encoder %s", codec->name);

  return codec;
}


/**
 *
 */
static void
transcoder_stream_packet(transcoder_stream_t *ts, th_pkt_t *pkt)
{
  streaming_message_t *sm;

  sm = streaming_msg_create_pkt(pkt);
  streaming_target_deliver2(ts->ts_target, sm);
}


/**
 *
 */
static void
transcoder_stream_subtitle(transcoder_stream_t *ts, th_pkt_t *pkt)
{
  //streaming_message_t *sm;
  AVCodec *icodec;
  AVCodecContext *ictx;
  AVPacket packet;
  AVSubtitle sub;
  int length,  got_subtitle;

  subtitle_stream_t *ss = (subtitle_stream_t*)ts;

  ictx = ss->sub_ictx;
  //octx = ss->sub_octx;

  icodec = ss->sub_icodec;
  //ocodec = ss->sub_ocodec;

  if (ictx->codec_id == CODEC_ID_NONE) {
    ictx->codec_id = icodec->id;

    if (avcodec_open(ictx, icodec) < 0) {
      tvhlog(LOG_ERR, "transcode", "Unable to open %s decoder", icodec->name);
      ts->ts_index = 0;
      goto cleanup;
    }
  }

  av_init_packet(&packet);
  packet.data     = pktbuf_ptr(pkt->pkt_payload);
  packet.size     = pktbuf_len(pkt->pkt_payload);
  packet.pts      = pkt->pkt_pts;
  packet.dts      = pkt->pkt_dts;
  packet.duration = pkt->pkt_duration;

  length = avcodec_decode_subtitle2(ictx,  &sub, &got_subtitle, &packet);
  if (length <= 0) {
    tvhlog(LOG_ERR, "transcode", "Unable to decode subtitle (%d)", length);
    ts->ts_index = 0;
    goto cleanup;
  }

  if (!got_subtitle)
    goto cleanup;

  //TODO: encoding

 cleanup:
  av_free_packet(&packet);
  avsubtitle_free(&sub);
}


/**
 *
 */
static void
transcoder_stream_audio(transcoder_stream_t *ts, th_pkt_t *pkt)
{
  AVCodec *icodec, *ocodec;
  AVCodecContext *ictx, *octx;
  AVPacket packet;
  int length, len, i;
  uint32_t frame_bytes;
  short *samples;
  streaming_message_t *sm;
  th_pkt_t *n;
  audio_stream_t *as = (audio_stream_t*)ts;

  ictx = as->aud_ictx;
  octx = as->aud_octx;

  icodec = as->aud_icodec;
  ocodec = as->aud_ocodec;

  if (ictx->codec_id == CODEC_ID_NONE) {
    ictx->codec_id = icodec->id;

    if (avcodec_open(ictx, icodec) < 0) {
      tvhlog(LOG_ERR, "transcode", "Unable to open %s decoder", icodec->name);
      ts->ts_index = 0;
      goto cleanup;
    }

    as->aud_dec_pts = pkt->pkt_pts;
  }

  if (pkt->pkt_pts > as->aud_dec_pts) {
    tvhlog(LOG_WARNING, "transcode", "Detected framedrop in audio");
    as->aud_enc_pts += (pkt->pkt_pts - as->aud_dec_pts);
  }

  pkt = pkt_merge_header(pkt);

  av_init_packet(&packet);
  packet.data     = pktbuf_ptr(pkt->pkt_payload);
  packet.size     = pktbuf_len(pkt->pkt_payload);
  packet.pts      = pkt->pkt_pts;
  packet.dts      = pkt->pkt_dts;
  packet.duration = pkt->pkt_duration;

  if ((len = as->aud_dec_size - as->aud_dec_offset) <= 0) {
    tvhlog(LOG_ERR, "transcode", "Decoder buffer overflow");
    ts->ts_index = 0;
    goto cleanup;
  }

  samples = (short*)(as->aud_dec_sample + as->aud_dec_offset);
  if ((length = avcodec_decode_audio3(ictx, samples, &len, &packet)) <= 0) {
    tvhlog(LOG_ERR, "transcode", "Unable to decode audio (%d)", length);
    ts->ts_index = 0;
    goto cleanup;
  }

  as->aud_dec_pts    += pkt->pkt_duration;
  as->aud_dec_offset += len;


  octx->sample_rate     = ictx->sample_rate;
  octx->sample_fmt      = ictx->sample_fmt;

  octx->time_base.den   = 90000;
  octx->time_base.num   = 1;

  octx->channels        = as->aud_channels ? as->aud_channels : ictx->channels;
  octx->bit_rate        = as->aud_bitrate  ? as->aud_bitrate  : ictx->bit_rate;

  octx->channels        = MIN(octx->channels, ictx->channels);
  octx->bit_rate        = MIN(octx->bit_rate,  ictx->bit_rate);

  switch (octx->channels) {
  case 1:
    octx->channel_layout = AV_CH_LAYOUT_MONO; 
    break;

  case 2:
    octx->channel_layout = AV_CH_LAYOUT_STEREO;
    break;

  case 3:
    octx->channel_layout = AV_CH_LAYOUT_SURROUND;
    break;

  case 4:
    octx->channel_layout = AV_CH_LAYOUT_QUAD;
    break;

  case 5:
    octx->channel_layout = AV_CH_LAYOUT_5POINT0;
    break;

  case 6:
    octx->channel_layout = AV_CH_LAYOUT_5POINT1;
    break;

  case 7:
    octx->channel_layout = AV_CH_LAYOUT_6POINT1;
    break;

  case 8:
    octx->channel_layout = AV_CH_LAYOUT_7POINT1;
    break;

  default: 
    break;
  }

  switch (ts->ts_type) {
  case SCT_MPEG2AUDIO:
    octx->channels = MIN(octx->channels, 2);
    if (octx->channels == 1)
      octx->channel_layout = AV_CH_LAYOUT_MONO;
    else
      octx->channel_layout = AV_CH_LAYOUT_STEREO;

    break;

  case SCT_AAC:
    octx->global_quality = 4*FF_QP2LAMBDA;
    octx->flags         |= CODEC_FLAG_QSCALE;
    break;

  default:
    break;
  }

  if (octx->codec_id == CODEC_ID_NONE) {
    octx->codec_id = ocodec->id;

    if (avcodec_open(octx, ocodec) < 0) {
      tvhlog(LOG_ERR, "transcode", "Unable to open %s encoder", ocodec->name);
      ts->ts_index = 0;
      goto cleanup;
    }
  }

  frame_bytes = av_get_bytes_per_sample(octx->sample_fmt) *
    octx->frame_size *
    octx->channels;

  len = as->aud_dec_offset;

  for (i = 0; i <= (len - frame_bytes); i += frame_bytes) {
    length = avcodec_encode_audio(octx,
				  as->aud_enc_sample,
				  as->aud_enc_size,
				  (short *)(as->aud_dec_sample + i));
    if (length < 0) {
      tvhlog(LOG_ERR, "transcode", "Unable to encode audio (%d)", length);
      ts->ts_index = 0;
      goto cleanup;

    } else if (length) {
      n = pkt_alloc(as->aud_enc_sample, length, as->aud_enc_pts, as->aud_enc_pts);
      n->pkt_componentindex = ts->ts_index;
      n->pkt_frametype      = pkt->pkt_frametype;
      n->pkt_channels       = octx->channels;
      n->pkt_sri            = pkt->pkt_sri;

      if (octx->coded_frame && octx->coded_frame->pts != AV_NOPTS_VALUE)
	n->pkt_duration = octx->coded_frame->pts - as->aud_enc_pts;
      else
	n->pkt_duration = frame_bytes*90000 / (2 * octx->channels * octx->sample_rate);

      as->aud_enc_pts += n->pkt_duration;

      if (octx->extradata_size)
	n->pkt_header = pktbuf_alloc(octx->extradata, octx->extradata_size);

      sm = streaming_msg_create_pkt(n);
      streaming_target_deliver2(ts->ts_target, sm);
      pkt_ref_dec(n);
    }

    as->aud_dec_offset -= frame_bytes;
  }

  if (as->aud_dec_offset)
    memmove(as->aud_dec_sample, as->aud_dec_sample + len - as->aud_dec_offset, 
	    as->aud_dec_offset);

 cleanup:
  av_free_packet(&packet);
}


/**
 *
 */
static void
transcoder_stream_video(transcoder_stream_t *ts, th_pkt_t *pkt)
{
  AVCodec *icodec, *ocodec;
  AVCodecContext *ictx, *octx;
  AVDictionary *opts;
  AVPacket packet;
  AVPicture deint_pic;
  uint8_t *buf, *out, *deint;
  int length, len, got_picture;
  streaming_message_t *sm;
  th_pkt_t *n;
  video_stream_t *vs = (video_stream_t*)ts;

  ictx = vs->vid_ictx;
  octx = vs->vid_octx;

  icodec = vs->vid_icodec;
  ocodec = vs->vid_ocodec;

  buf = out = deint = NULL;
  opts = NULL;

  if (ictx->codec_id == CODEC_ID_NONE) {
    ictx->codec_id = icodec->id;

    if (avcodec_open(ictx, icodec) < 0) {
      tvhlog(LOG_ERR, "transcode", "Unable to open %s decoder", icodec->name);
      ts->ts_index = 0;
      goto cleanup;
    }
  }

  pkt = pkt_merge_header(pkt);

  av_init_packet(&packet);
  packet.data     = pktbuf_ptr(pkt->pkt_payload);
  packet.size     = pktbuf_len(pkt->pkt_payload);
  packet.pts      = pkt->pkt_pts;
  packet.dts      = pkt->pkt_dts;
  packet.duration = pkt->pkt_duration;

  vs->vid_enc_frame->pts = packet.pts;
  vs->vid_enc_frame->pkt_dts = packet.dts;
  vs->vid_enc_frame->pkt_pts = packet.pts;

  vs->vid_dec_frame->pts = packet.pts;
  vs->vid_dec_frame->pkt_dts = packet.dts;
  vs->vid_dec_frame->pkt_pts = packet.pts;

  ictx->reordered_opaque = packet.pts;

  length = avcodec_decode_video2(ictx, vs->vid_dec_frame, &got_picture, &packet);
  if (length <= 0) {
    tvhlog(LOG_ERR, "transcode", "Unable to decode video (%d)", length);
    ts->ts_index = 0;
    goto cleanup;
  }

  if (!got_picture)
    goto cleanup;

  octx->sample_aspect_ratio.num = ictx->sample_aspect_ratio.num;
  octx->sample_aspect_ratio.den = ictx->sample_aspect_ratio.den;

  vs->vid_enc_frame->sample_aspect_ratio.num = vs->vid_dec_frame->sample_aspect_ratio.num;
  vs->vid_enc_frame->sample_aspect_ratio.den = vs->vid_dec_frame->sample_aspect_ratio.den;

  if(octx->codec_id == CODEC_ID_NONE) {
    // Common settings
    octx->width           = vs->vid_width  ? vs->vid_width  : ictx->width;
    octx->height          = vs->vid_height ? vs->vid_height : ictx->height;
    octx->gop_size        = 25;
    octx->time_base.den   = 25;
    octx->time_base.num   = 1;
    octx->has_b_frames    = ictx->has_b_frames;

    switch (ts->ts_type) {
    case SCT_MPEG2VIDEO:
      octx->codec_id       = CODEC_ID_MPEG2VIDEO;
      octx->pix_fmt        = PIX_FMT_YUV420P;
      octx->flags         |= CODEC_FLAG_GLOBAL_HEADER;

      octx->qmin           = 1;
      octx->qmax           = FF_LAMBDA_MAX;

      octx->bit_rate       = 2 * octx->width * octx->height;
      octx->rc_max_rate    = 4 * octx->bit_rate;
      octx->rc_buffer_size = 2 * octx->rc_max_rate;
      break;
 
    case SCT_H264:
      octx->codec_id       = CODEC_ID_H264;
      octx->pix_fmt        = PIX_FMT_YUV420P;
      octx->flags          |= CODEC_FLAG_GLOBAL_HEADER;

      // Qscale difference between I-frames and P-frames. 
      // Note: -i_qfactor is handled a little differently than --ipratio. 
      // Recommended: -i_qfactor 0.71
      octx->i_quant_factor = 0.71;

      // QP curve compression: 0.0 => CBR, 1.0 => CQP.
      // Recommended default: -qcomp 0.60
      octx->qcompress = 0.6;

      // Minimum quantizer. Doesn't need to be changed.
      // Recommended default: -qmin 10
      octx->qmin = 10;

      // Maximum quantizer. Doesn't need to be changed.
      // Recommended default: -qmax 51
      octx->qmax = 30;

      av_dict_set(&opts, "preset",  "medium", 0);
      av_dict_set(&opts, "profile", "baseline", 0);

      octx->bit_rate       = 2 * octx->width * octx->height;
      octx->rc_buffer_size = 8 * 1024 * 224;
      octx->rc_max_rate    = 2 * octx->rc_buffer_size;
      break;

    default:
      break;
    }

    octx->codec_id = ocodec->id;

    if (avcodec_open2(octx, ocodec, &opts) < 0) {
      tvhlog(LOG_ERR, "transcode", "Unable to open %s encoder", ocodec->name);
      ts->ts_index = 0;
      goto cleanup;
    }
  }

  len = avpicture_get_size(ictx->pix_fmt, ictx->width, ictx->height);
  deint = av_malloc(len);

  avpicture_fill(&deint_pic,
		 deint, 
		 ictx->pix_fmt, 
		 ictx->width, 
		 ictx->height);

  if (avpicture_deinterlace(&deint_pic,
			    (AVPicture *)vs->vid_dec_frame,
			    ictx->pix_fmt,
			    ictx->width,
			    ictx->height) < 0) {
    tvhlog(LOG_ERR, "transcode", "Cannot deinterlace frame");
    ts->ts_index = 0;
    goto cleanup;
  }

  len = avpicture_get_size(octx->pix_fmt, octx->width, octx->height);
  buf = av_malloc(len + FF_INPUT_BUFFER_PADDING_SIZE);
  memset(buf, 0, len);

  avpicture_fill((AVPicture *)vs->vid_enc_frame, 
                 buf, 
                 octx->pix_fmt,
                 octx->width, 
                 octx->height);
 
  vs->vid_scaler = sws_getCachedContext(vs->vid_scaler,
				    ictx->width,
				    ictx->height,
				    ictx->pix_fmt,
				    octx->width,
				    octx->height,
				    octx->pix_fmt,
				    1,
				    NULL,
				    NULL,
				    NULL);
 
  if (sws_scale(vs->vid_scaler, 
		(const uint8_t * const*)deint_pic.data, 
		deint_pic.linesize, 
		0, 
		ictx->height, 
		vs->vid_enc_frame->data, 
		vs->vid_enc_frame->linesize) < 0) {
    tvhlog(LOG_ERR, "transcode", "Cannot scale frame");
    ts->ts_index = 0;
    goto cleanup;
  }
      
 
  len = avpicture_get_size(octx->pix_fmt, ictx->width, ictx->height);
  out = av_malloc(len + FF_INPUT_BUFFER_PADDING_SIZE);
  memset(out, 0, len);

  vs->vid_enc_frame->pkt_pts = vs->vid_dec_frame->pkt_pts;
  vs->vid_enc_frame->pkt_dts = vs->vid_dec_frame->pkt_dts;

  if (vs->vid_dec_frame->reordered_opaque != AV_NOPTS_VALUE)
    vs->vid_enc_frame->pts = vs->vid_dec_frame->reordered_opaque;

  else if (ictx->coded_frame && ictx->coded_frame->pts != AV_NOPTS_VALUE)
    vs->vid_enc_frame->pts = vs->vid_dec_frame->pts;
 
  length = avcodec_encode_video(octx, out, len, vs->vid_enc_frame);
  if (length <= 0) {
    if (length) {
      tvhlog(LOG_ERR, "transcode", "Unable to encode video (%d)", length);
      ts->ts_index = 0;
    }

    goto cleanup;
  }

  if (!octx->coded_frame)
    goto cleanup;

  n = pkt_alloc(out, length, octx->coded_frame->pkt_pts, octx->coded_frame->pkt_dts);

  switch (octx->coded_frame->pict_type) {
  case AV_PICTURE_TYPE_I:
    n->pkt_frametype = PKT_I_FRAME;
    break;

  case AV_PICTURE_TYPE_P:
    n->pkt_frametype = PKT_P_FRAME;
    break;

  case AV_PICTURE_TYPE_B:
    n->pkt_frametype = PKT_B_FRAME;
    break;

  default:
    break;
  }

  n->pkt_duration       = pkt->pkt_duration;
  n->pkt_commercial     = pkt->pkt_commercial;
  n->pkt_componentindex = pkt->pkt_componentindex;
  n->pkt_field          = pkt->pkt_field;
  n->pkt_aspect_num     = pkt->pkt_aspect_num;
  n->pkt_aspect_den     = pkt->pkt_aspect_den;
  
  if(octx->coded_frame && octx->coded_frame->pts != AV_NOPTS_VALUE) {
    if(n->pkt_dts != PTS_UNSET)
      n->pkt_dts -= n->pkt_pts;

    n->pkt_pts = octx->coded_frame->pts;

    if(n->pkt_dts != PTS_UNSET)
      n->pkt_dts += n->pkt_pts;
  }

  if (octx->extradata_size)
    n->pkt_header = pktbuf_alloc(octx->extradata, octx->extradata_size);

  sm = streaming_msg_create_pkt(n);
  streaming_target_deliver2(ts->ts_target, sm);
  pkt_ref_dec(n);

 cleanup:
  av_free_packet(&packet);

  if(buf)
    av_free(buf);

  if(out)
    av_free(out);

  if(deint)
    av_free(deint);

  if(opts)
    av_dict_free(&opts);
}


/**
 * 
 */
static void
transcoder_packet(transcoder_t *t, th_pkt_t *pkt)
{
  transcoder_stream_t *ts;

  LIST_FOREACH(ts, &t->t_stream_list, ts_link) {
    if (pkt->pkt_componentindex != ts->ts_index)
      continue;

    ts->ts_handle_pkt(ts, pkt);
    break;
  }
}


/**
 * 
 */
static void
transcoder_destroy_stream(transcoder_stream_t *ts)
{
  free(ts);
}


/**
 * 
 */
static int
transcoder_init_stream(transcoder_t *t, streaming_start_component_t *ssc)
{
  transcoder_stream_t *ts = calloc(1, sizeof(transcoder_stream_t));

  ts->ts_index      = ssc->ssc_index;
  ts->ts_type       = ssc->ssc_type;
  ts->ts_target     = t->t_output;
  ts->ts_handle_pkt = transcoder_stream_packet;
  ts->ts_destroy    = transcoder_destroy_stream;

  LIST_INSERT_HEAD(&t->t_stream_list, ts, ts_link);

  if(ssc->ssc_gh)
    pktbuf_ref_inc(ssc->ssc_gh);

  tvhlog(LOG_INFO, "transcode", "%d:%s ==> Passthrough", 
	 ssc->ssc_index,
	 streaming_component_type2txt(ssc->ssc_type));

  return 1;
}


/**
 * 
 */
static void
transcoder_destroy_subtitle(transcoder_stream_t *ts)
{
  subtitle_stream_t *ss = (subtitle_stream_t*)ts;

  if(ss->sub_ictx) {
    avcodec_close(ss->sub_ictx);
    av_free(ss->sub_ictx);
  }

  if(ss->sub_octx) {
    avcodec_close(ss->sub_octx);
    av_free(ss->sub_octx);
  }

  free(ts);
}


/**
 * 
 */
static int
transcoder_init_subtitle(transcoder_t *t, streaming_start_component_t *ssc)
{
  subtitle_stream_t *ss;
  AVCodec *icodec, *ocodec;
  transcoder_props_t *tp = &t->t_props;

  if (tp->tp_scodec == SCT_NONE)
    return 0;

  else if (tp->tp_scodec == SCT_UNKNOWN)
    return transcoder_init_stream(t, ssc);

  else if (!(icodec = transcoder_get_decoder(ssc->ssc_type)))
    return transcoder_init_stream(t, ssc);

  else if (!(ocodec = transcoder_get_encoder(tp->tp_scodec)))
    return transcoder_init_stream(t, ssc);

  if (tp->tp_scodec == ssc->ssc_type)
    return transcoder_init_stream(t, ssc);

  ss = calloc(1, sizeof(subtitle_stream_t));

  ss->ts_index      = ssc->ssc_index;
  ss->ts_type       = tp->tp_scodec;
  ss->ts_target     = t->t_output;
  ss->ts_handle_pkt = transcoder_stream_subtitle;
  ss->ts_destroy    = transcoder_destroy_subtitle;

  ss->sub_icodec = icodec;
  ss->sub_ocodec = ocodec;

  ss->sub_ictx = avcodec_alloc_context();
  ss->sub_octx = avcodec_alloc_context();

  ss->sub_ictx->codec_type = AVMEDIA_TYPE_SUBTITLE;
  ss->sub_octx->codec_type = AVMEDIA_TYPE_SUBTITLE;

  avcodec_get_context_defaults3(ss->sub_ictx, icodec);
  avcodec_get_context_defaults3(ss->sub_octx, ocodec);

  LIST_INSERT_HEAD(&t->t_stream_list, (transcoder_stream_t*)ss, ts_link);

  tvhlog(LOG_INFO, "transcode", "%d:%s ==> %s", 
	 ssc->ssc_index,
	 streaming_component_type2txt(ssc->ssc_type),
	 streaming_component_type2txt(ss->ts_type));

  ssc->ssc_type = tp->tp_scodec;
  ssc->ssc_gh = NULL;

  return 1;
}


/**
 * 
 */
static void
transcoder_destroy_audio(transcoder_stream_t *ts)
{
  audio_stream_t *as = (audio_stream_t*)ts;

  if(as->aud_ictx) {
    avcodec_close(as->aud_ictx);
    av_free(as->aud_ictx);
  }

  if(as->aud_octx) {
    avcodec_close(as->aud_octx);
    av_free(as->aud_octx);
  }

  if(as->aud_dec_sample)
    av_free(as->aud_dec_sample);

  if(as->aud_enc_sample)
    av_free(as->aud_enc_sample);

  free(ts);
}


/**
 * 
 */
static int
transcoder_init_audio(transcoder_t *t, streaming_start_component_t *ssc)
{
  audio_stream_t *as;
  transcoder_stream_t *ts;
  AVCodec *icodec, *ocodec;
  transcoder_props_t *tp = &t->t_props;

  if (tp->tp_acodec == SCT_NONE)
    return 0;

  else if (tp->tp_acodec == SCT_UNKNOWN)
    return transcoder_init_stream(t, ssc);

  else if (!(icodec = transcoder_get_decoder(ssc->ssc_type)))
    return transcoder_init_stream(t, ssc);

  else if (!(ocodec = transcoder_get_encoder(tp->tp_acodec)))
    return transcoder_init_stream(t, ssc);

  LIST_FOREACH(ts, &t->t_stream_list, ts_link)
    if (SCT_ISAUDIO(ts->ts_type))
       return 0;

  if (tp->tp_acodec == ssc->ssc_type)
    return transcoder_init_stream(t, ssc);

  as = calloc(1, sizeof(audio_stream_t));

  as->ts_index      = ssc->ssc_index;
  as->ts_type       = tp->tp_acodec;
  as->ts_target     = t->t_output;
  as->ts_handle_pkt = transcoder_stream_audio;
  as->ts_destroy    = transcoder_destroy_audio;

  as->aud_icodec = icodec;
  as->aud_ocodec = ocodec;

  as->aud_ictx = avcodec_alloc_context();
  as->aud_octx = avcodec_alloc_context();

  as->aud_ictx->codec_type = AVMEDIA_TYPE_AUDIO;
  as->aud_octx->codec_type = AVMEDIA_TYPE_AUDIO;

  as->aud_ictx->thread_count = sysconf(_SC_NPROCESSORS_ONLN);
  as->aud_octx->thread_count = sysconf(_SC_NPROCESSORS_ONLN);

  avcodec_get_context_defaults3(as->aud_ictx, icodec);
  avcodec_get_context_defaults3(as->aud_octx, ocodec);

  as->aud_ictx->codec_type = AVMEDIA_TYPE_AUDIO;
  as->aud_octx->codec_type = AVMEDIA_TYPE_AUDIO;

  as->aud_dec_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*2;
  as->aud_enc_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*2;

  as->aud_dec_sample = av_malloc(as->aud_dec_size + FF_INPUT_BUFFER_PADDING_SIZE);
  as->aud_enc_sample = av_malloc(as->aud_enc_size + FF_INPUT_BUFFER_PADDING_SIZE);

  memset(as->aud_dec_sample, 0, as->aud_dec_size + FF_INPUT_BUFFER_PADDING_SIZE);
  memset(as->aud_enc_sample, 0, as->aud_enc_size + FF_INPUT_BUFFER_PADDING_SIZE);

  LIST_INSERT_HEAD(&t->t_stream_list, (transcoder_stream_t*)as, ts_link);

  tvhlog(LOG_INFO, "transcode", "%d:%s ==> %s", 
	 ssc->ssc_index,
	 streaming_component_type2txt(ssc->ssc_type),
	 streaming_component_type2txt(as->ts_type));

  ssc->ssc_type     = tp->tp_acodec;
  ssc->ssc_gh       = NULL;

  // resampling not implemented yet
  if(tp->tp_channels > 0)
    as->aud_channels = 0; //tp->tp_channels; 
  else
    as->aud_channels = 0;

  as->aud_bitrate = as->aud_channels * 64000;
  return 1;
}


/**
 * 
 */
static void
transcoder_destroy_video(transcoder_stream_t *ts)
{
  video_stream_t *vs = (video_stream_t*)ts;

  if(vs->vid_ictx) {
    avcodec_close(vs->vid_ictx);
    av_free(vs->vid_ictx);
  }

  if(vs->vid_octx) {
    avcodec_close(vs->vid_octx);
    av_free(vs->vid_octx);
  }

  if(vs->vid_dec_frame)
    av_free(vs->vid_dec_frame);

  if(vs->vid_scaler)
    sws_freeContext(vs->vid_scaler);

  if(vs->vid_enc_frame)
    av_free(vs->vid_enc_frame);

  free(ts);
}


/**
 * 
 */
static int
transcoder_init_video(transcoder_t *t, streaming_start_component_t *ssc)
{
  video_stream_t *vs;
  AVCodec *icodec, *ocodec;
  double aspect;
  transcoder_props_t *tp = &t->t_props;

  if (tp->tp_vcodec == SCT_NONE)
    return 0;

  else if (tp->tp_vcodec == SCT_UNKNOWN)
    return transcoder_init_stream(t, ssc);

  else if (!(icodec = transcoder_get_decoder(ssc->ssc_type)))
    return transcoder_init_stream(t, ssc);

  else if (!(ocodec = transcoder_get_encoder(tp->tp_vcodec)))
    return transcoder_init_stream(t, ssc);

  vs = calloc(1, sizeof(video_stream_t));

  vs->ts_index      = ssc->ssc_index;
  vs->ts_type       = tp->tp_vcodec;
  vs->ts_target     = t->t_output;
  vs->ts_handle_pkt = transcoder_stream_video;
  vs->ts_destroy    = transcoder_destroy_video;

  vs->vid_icodec = icodec;
  vs->vid_ocodec = ocodec;

  vs->vid_ictx = avcodec_alloc_context();
  vs->vid_octx = avcodec_alloc_context();

  vs->vid_ictx->thread_count = sysconf(_SC_NPROCESSORS_ONLN);
  vs->vid_octx->thread_count = sysconf(_SC_NPROCESSORS_ONLN);
 
  avcodec_get_context_defaults3(vs->vid_ictx, icodec);
  avcodec_get_context_defaults3(vs->vid_octx, ocodec);

  vs->vid_dec_frame = avcodec_alloc_frame();
  vs->vid_enc_frame = avcodec_alloc_frame();

  avcodec_get_frame_defaults(vs->vid_dec_frame);
  avcodec_get_frame_defaults(vs->vid_enc_frame);

  vs->vid_ictx->codec_type = AVMEDIA_TYPE_VIDEO;
  vs->vid_octx->codec_type = AVMEDIA_TYPE_VIDEO;

  LIST_INSERT_HEAD(&t->t_stream_list, (transcoder_stream_t*)vs, ts_link);

  aspect = (double)ssc->ssc_width / ssc->ssc_height;

  if(tp->tp_resolution > 0) {
    vs->vid_height = MIN(tp->tp_resolution, ssc->ssc_height);
    if (vs->vid_height&1) // Must be even
      vs->vid_height++;

    vs->vid_width = vs->vid_height * aspect;
    if (vs->vid_width&1) // Must be even
      vs->vid_width++;
  } else {
     vs->vid_height = ssc->ssc_height;
     vs->vid_width  = ssc->ssc_width;
  }

  tvhlog(LOG_INFO, "transcode", "%d:%s %dx%d ==> %s %dx%d", 
	 ssc->ssc_index,
	 streaming_component_type2txt(ssc->ssc_type),
	 ssc->ssc_width,
	 ssc->ssc_height,
	 streaming_component_type2txt(vs->ts_type),
	 vs->vid_width,
	 vs->vid_height);

  ssc->ssc_type   = tp->tp_vcodec;
  ssc->ssc_width  = vs->vid_width;
  ssc->ssc_height = vs->vid_height;
  ssc->ssc_gh     = NULL;

  return 1;
}


/**
 * Figure out how many streams we will use.
 */
static int
transcoder_calc_stream_count(transcoder_t *t, streaming_start_t *ss) {
  int i = 0;
  int video = 0;
  int audio = 0;
  int subtitle = 0;
  streaming_start_component_t *ssc = NULL;

  for (i = 0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    if (ssc->ssc_disabled)
      continue;

    if (SCT_ISVIDEO(ssc->ssc_type)) {
      if (t->t_props.tp_vcodec == SCT_NONE)
	video = 0;
      else if (t->t_props.tp_vcodec == SCT_UNKNOWN)
	video++;
      else
	video = 1;

    } else if (SCT_ISAUDIO(ssc->ssc_type)) {
      if (t->t_props.tp_acodec == SCT_NONE)
	audio = 0;
      else if (t->t_props.tp_acodec == SCT_UNKNOWN)
	audio++;
      else
	audio = 1;

    } else if (SCT_ISSUBTITLE(ssc->ssc_type)) {
      if (t->t_props.tp_scodec == SCT_NONE)
	subtitle = 0;
      else if (t->t_props.tp_scodec == SCT_UNKNOWN)
	subtitle++;
      else
	subtitle = 1;
    }
  }

  return (video + audio + subtitle);
}


/**
 * 
 */
static streaming_start_t *
transcoder_start(transcoder_t *t, streaming_start_t *src)
{
  int i, j, n, rc;
  streaming_start_t *ss;


  n = transcoder_calc_stream_count(t, src);
  ss = calloc(1, (sizeof(streaming_start_t) +
		  sizeof(streaming_start_component_t) * n));

  ss->ss_refcount       = 1;
  ss->ss_num_components = n;
  ss->ss_pcr_pid        = src->ss_pcr_pid;
  ss->ss_pmt_pid        = src->ss_pmt_pid;
  service_source_info_copy(&ss->ss_si, &src->ss_si);


  for (i = j = 0; i < src->ss_num_components && j < n; i++) {
    streaming_start_component_t *ssc_src = &src->ss_components[i];
    streaming_start_component_t *ssc = &ss->ss_components[j];
    
    if (ssc_src->ssc_disabled)
      continue;

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
    ssc->ssc_frameduration  = ssc_src->ssc_frameduration;
    ssc->ssc_gh             = ssc_src->ssc_gh;

    memcpy(ssc->ssc_lang, ssc_src->ssc_lang, 4);

    if (SCT_ISVIDEO(ssc->ssc_type)) 
      rc = transcoder_init_video(t, ssc);

    else if (SCT_ISAUDIO(ssc->ssc_type))
      rc = transcoder_init_audio(t, ssc);

    else if (SCT_ISSUBTITLE(ssc->ssc_type))
      rc = transcoder_init_subtitle(t, ssc);
    else
      rc = 0;

    if(!rc)
      tvhlog(LOG_INFO, "transcode", "%d:%s ==> Filtered", 
	     ssc->ssc_index,
	     streaming_component_type2txt(ssc->ssc_type));
    else
      j++;
  }

  return ss;
}


/**
 * 
 */
static void
transcoder_stop(transcoder_t *t)
{
  transcoder_stream_t *ts;
  
  while ((ts = LIST_FIRST(&t->t_stream_list))) {
    LIST_REMOVE(ts, ts_link);

    if (ts->ts_destroy)
      ts->ts_destroy(ts);
  }
}


/**
 * 
 */
static void
transcoder_input(void *opaque, streaming_message_t *sm)
{
  transcoder_t *t;
  streaming_start_t *ss;
  th_pkt_t *pkt;

  t = opaque;

  switch (sm->sm_type) {
  case SMT_PACKET:
    pkt = sm->sm_data;
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

  case SMT_SPEED:
  case SMT_SKIP:
  case SMT_TIMESHIFT_STATUS:
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
transcoder_create(streaming_target_t *output)
{
  transcoder_t *t = calloc(1, sizeof(transcoder_t));

  t->t_output = output;

  streaming_target_init(&t->t_input, transcoder_input, t, 0);

  return &t->t_input;
}


/**
 * 
 */
void
transcoder_set_properties(streaming_target_t *st, 
			  transcoder_props_t *props)
{
  transcoder_t *t = (transcoder_t *)st;
  transcoder_props_t *tp = &t->t_props;

  tp->tp_vcodec     = props->tp_vcodec;
  tp->tp_acodec     = props->tp_acodec;
  tp->tp_scodec     = props->tp_scodec;
  tp->tp_channels   = props->tp_channels;
  tp->tp_bandwidth  = props->tp_bandwidth;
  tp->tp_resolution = props->tp_resolution;

  memcpy(tp->tp_language, props->tp_language, 4);
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
void
transcoder_get_capabilities(htsmsg_t *array)
{
  AVCodec *p = NULL;
  const char *name;
  streaming_component_type_t sct;

  while ((p = av_codec_next(p))) {

    if (!libav_is_encoder(p))
      continue;

    if (!WORKING_ENCODER(p->id))
      continue;

    sct = codec_id2streaming_component_type(p->id);
    if (sct == SCT_NONE)
      continue;

    name = streaming_component_type2txt(sct);
    htsmsg_add_str(array, NULL, name);
  }
}



