/*
 *  tvheadend, libavformat based muxer
 *  Copyright (C) 2012 John Törnblom
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
 *  along with this program.  If not, see <htmlui://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#if LIBAVCODEC_VERSION_MAJOR > 58
#include <libavcodec/bsf.h>
#include <libavcodec/avcodec.h>
#endif

#include "tvheadend.h"
#include "streaming.h"
#include "epg.h"
#include "channels.h"
#include "libav.h"
#include "muxer_libav.h"
#include "parsers/parsers.h"
#include "parsers/parser_avc.h"
#include "parsers/parser_hevc.h"

typedef struct lav_muxer {
  muxer_t;
  AVFormatContext *lm_oc;
  AVBSFContext *ctx;
  const AVBitStreamFilter *bsf_h264_filter;
  const AVBitStreamFilter *bsf_hevc_filter;
  const AVBitStreamFilter *bsf_vp9_filter;
  int lm_fd;
  int lm_init;
} lav_muxer_t;

#define MUX_BUF_SIZE 4096

static const AVRational mpeg_tc = {1, 90000};


/**
 * Callback function for libavformat
 */
#if LIBAVFORMAT_VERSION_MAJOR > 60
static int
lav_muxer_write(void *opaque, const uint8_t *buf, int buf_size)
#else
static int
lav_muxer_write(void *opaque, uint8_t *buf, int buf_size)
#endif
{
  int r;
  lav_muxer_t *lm = (lav_muxer_t*)opaque;

  if (lm->m_errors) {
    lm->m_errors++;
    return buf_size;
  }

  r = write(lm->lm_fd, buf, buf_size);
  if (r != buf_size)
    lm->m_errors++;
  
  /* No room to notify about errors here. */
  /* We need to complete av_write_trailer() to free */
  /* all associated structures. */
  return buf_size;
}


/**
 * Add a stream to the muxer
 */
static int
lav_muxer_add_stream(lav_muxer_t *lm, 
		     const streaming_start_component_t *ssc)
{
  AVStream *st;
  AVCodecContext *c;
  const AVCodec *codec; 
  int rc = -1;

  st = avformat_new_stream(lm->lm_oc, NULL);
  if (!st)
    return -1;

  st->id = ssc->es_index;
  codec = avcodec_find_encoder(streaming_component_type2codec_id(ssc->es_type));
  c = avcodec_alloc_context3(codec);

  switch(lm->m_config.m_type) {
  case MC_MATROSKA:
  case MC_AVMATROSKA:
  case MC_AVMP4:
    st->time_base.num = 1000000;
    st->time_base.den = 1;
    break;

  case MC_MPEGPS:
    c->rc_buffer_size = 224*1024*8;
    //Fall-through
  case MC_MPEGTS:
    st->time_base.num = 90000;
    st->time_base.den = 1;
    break;

  default:
    st->time_base = AV_TIME_BASE_Q;
    break;
  }

  if(ssc->ssc_gh) {
    if (ssc->es_type == SCT_H264 || ssc->es_type == SCT_HEVC) {
      sbuf_t hdr;
      sbuf_init(&hdr);
      if (ssc->es_type == SCT_H264) {
          isom_write_avcc(&hdr, pktbuf_ptr(ssc->ssc_gh),
                          pktbuf_len(ssc->ssc_gh));
      } else {
          isom_write_hvcc(&hdr, pktbuf_ptr(ssc->ssc_gh),
                          pktbuf_len(ssc->ssc_gh));
      }
      c->extradata_size = hdr.sb_ptr;
      c->extradata = av_malloc(hdr.sb_ptr);
      memcpy(c->extradata, hdr.sb_data, hdr.sb_ptr);
      sbuf_free(&hdr);
    } else {
      c->extradata_size = pktbuf_len(ssc->ssc_gh);
      c->extradata = av_malloc(c->extradata_size);
      memcpy(c->extradata, pktbuf_ptr(ssc->ssc_gh),
             pktbuf_len(ssc->ssc_gh));
    }
  }

  if(SCT_ISAUDIO(ssc->es_type)) {
    c->codec_type    = AVMEDIA_TYPE_AUDIO;
    c->sample_fmt    = AV_SAMPLE_FMT_S16;

    c->sample_rate   = sri_to_rate(ssc->es_sri);
#if LIBAVUTIL_VERSION_MAJOR > 57
    c->ch_layout.nb_channels = ssc->es_channels;
#else
    c->channels      = ssc->es_channels;
#endif

#if 0
    c->time_base.num = 1;
    c->time_base.den = c->sample_rate;
#else
    c->time_base     = st->time_base;
#endif

    avcodec_parameters_from_context(st->codecpar, c);
    av_dict_set(&st->metadata, "language", ssc->es_lang, 0);

  } else if(SCT_ISVIDEO(ssc->es_type)) {
    c->codec_type = AVMEDIA_TYPE_VIDEO;
    c->width      = ssc->es_width;
    c->height     = ssc->es_height;

    c->time_base.num = 1;
    c->time_base.den = 25;

    c->sample_aspect_ratio.num = ssc->es_aspect_num;
    c->sample_aspect_ratio.den = ssc->es_aspect_den;

    if (lm->m_config.m_type == MC_AVMP4) {
      /* this is a whole hell */
      AVRational ratio = { c->height, c->width };
      c->sample_aspect_ratio = av_mul_q(c->sample_aspect_ratio, ratio);
    }

    avcodec_parameters_from_context(st->codecpar, c);
    st->sample_aspect_ratio.num = c->sample_aspect_ratio.num;
    st->sample_aspect_ratio.den = c->sample_aspect_ratio.den;

    if (ssc->es_type == SCT_H264) {
      if (av_bsf_alloc(lm->bsf_h264_filter, &lm->ctx)) {
        tvherror(LS_LIBAV,  "Failed to alloc AVBitStreamFilter for h264_mp4toannexb");
        goto fail;
      }
    }
    else if (ssc->es_type == SCT_HEVC) {
      if (av_bsf_alloc(lm->bsf_hevc_filter, &lm->ctx)) {
        tvherror(LS_LIBAV,  "Failed to alloc AVBitStreamFilter for hevc_mp4toannexb");
        goto fail;
      }
    }
    else if (ssc->es_type == SCT_VP9) {
      if (av_bsf_alloc(lm->bsf_vp9_filter, &lm->ctx)) {
        tvherror(LS_LIBAV,  "Failed to alloc AVBitStreamFilter for vp9_superframe ");
        goto fail;
      }
    }
    else {
      if (av_bsf_alloc(lm->bsf_h264_filter, &lm->ctx)) {
        tvherror(LS_LIBAV,  "Failed to alloc AVBitStreamFilter for h264_mp4toannexb");
        goto fail;
      }
    }
    if(avcodec_parameters_copy(lm->ctx->par_in, st->codecpar)) {
      tvherror(LS_LIBAV,  "Failed to copy paramters to AVBSFContext");
      goto fail;
    }
    lm->ctx->time_base_in = st->time_base;
    if (av_bsf_init(lm->ctx)) {
      tvherror(LS_LIBAV,  "Failed to init AVBSFContext");
      goto fail;
    }
  } else if(SCT_ISSUBTITLE(ssc->es_type)) {
    c->codec_type = AVMEDIA_TYPE_SUBTITLE;
    avcodec_parameters_from_context(st->codecpar, c);
    av_dict_set(&st->metadata, "language", ssc->es_lang, 0);
  }

  if(lm->lm_oc->oformat->flags & AVFMT_GLOBALHEADER)
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  rc = 0;

fail:
  av_freep(&c->extradata);
  avcodec_free_context(&c);

  return rc;
}


/**
 * Check if a container supports a given streaming component
 */
static int
lav_muxer_support_stream(muxer_container_type_t mc, 
			 streaming_component_type_t type)
{
  int ret = 0;

  switch(mc) {
  case MC_MATROSKA:
  case MC_AVMATROSKA:
    ret |= SCT_ISAUDIO(type);
    ret |= SCT_ISVIDEO(type);
    ret |= SCT_ISSUBTITLE(type);
    break;

  case MC_WEBM:
  case MC_AVWEBM:
    ret |= type == SCT_VP8;
    ret |= type == SCT_VP9;
    ret |= type == SCT_VORBIS;
    ret |= type == SCT_OPUS;
    break;

  case MC_MPEGTS:
    ret |= (type == SCT_MPEG2VIDEO);
    ret |= (type == SCT_H264);
    ret |= (type == SCT_HEVC);

    ret |= (type == SCT_MPEG2AUDIO);
    ret |= (type == SCT_AC3);
    ret |= (type == SCT_AAC);
    ret |= (type == SCT_MP4A);
    ret |= (type == SCT_EAC3);
    ret |= (type == SCT_AC4);

    //Some pids lack pts, disable for now
    //ret |= (type == SCT_TELETEXT);
    ret |= (type == SCT_DVBSUB);
    break;

  case MC_MPEGPS:
    ret |= (type == SCT_MPEG2VIDEO);
    ret |= (type == SCT_MPEG2AUDIO);
    ret |= (type == SCT_AC3);
    break;

  case MC_AVMP4:
    ret |= (type == SCT_MPEG2VIDEO);
    ret |= (type == SCT_H264);
    ret |= (type == SCT_HEVC);

    ret |= (type == SCT_MPEG2AUDIO);
    ret |= (type == SCT_AC3);
    ret |= (type == SCT_AAC);
    ret |= (type == SCT_MP4A);
    ret |= (type == SCT_EAC3);
    ret |= (type == SCT_AC4);
    break;

  default:
    break;
  }

  return ret;
}


/**
 * Figure out the mime-type for the muxed data stream
 */
static const char*
lav_muxer_mime(muxer_t* m, const struct streaming_start *ss)
{
  int i;
  int has_audio;
  int has_video;
  const streaming_start_component_t *ssc;
  
  has_audio = 0;
  has_video = 0;

  for(i=0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    if(ssc->ssc_disabled)
      continue;

    if(!lav_muxer_support_stream(m->m_config.m_type, ssc->es_type))
      continue;

    has_video |= SCT_ISVIDEO(ssc->es_type);
    has_audio |= SCT_ISAUDIO(ssc->es_type);
  }

  if(has_video)
    return muxer_container_type2mime(m->m_config.m_type, 1);
  else if(has_audio)
    return muxer_container_type2mime(m->m_config.m_type, 0);
  else
    return muxer_container_type2mime(MC_UNKNOWN, 0);
}


/**
 * Init the muxer with streams
 */
static int
lav_muxer_init(muxer_t* m, struct streaming_start *ss, const char *name)
{
  int i;
  streaming_start_component_t *ssc;
  AVFormatContext *oc;
  AVDictionary *opts = NULL;
  lav_muxer_t *lm = (lav_muxer_t*)m;
  char app[128];
  char mpegts_info[8];
#if LIBAVCODEC_VERSION_MAJOR > 58
  const AVOutputFormat *fmt;
#else
  AVOutputFormat *fmt;
#endif
  const char *muxer_name;
  int rc = -1;

  snprintf(app, sizeof(app), "Tvheadend %s", tvheadend_version);

  oc = lm->lm_oc;

  switch(lm->m_config.m_type) {
  case MC_MPEGPS:
    muxer_name = "dvd";
    break;
  case MC_MPEGTS:
    muxer_name = "mpegts";
    break;
  case MC_MATROSKA:
  case MC_AVMATROSKA:
    muxer_name = "matroska";
    break;
  case MC_WEBM:
  case MC_AVWEBM:
    muxer_name = "webm";
    break;
  case MC_AVMP4:
    muxer_name = "mp4";
    break;
  default:
    muxer_name = muxer_container_type2txt(lm->m_config.m_type);
    break;
  }
  fmt = lm->lm_oc->oformat;
  if(avformat_alloc_output_context2(&oc, fmt, muxer_name, NULL) < 0) {
    tvherror(LS_LIBAV,  "Can't find the '%s' muxer", muxer_name);
    return -1;
  }
  av_dict_set(&oc->metadata, "title", name, 0);
  av_dict_set(&lm->lm_oc->metadata, "title", name, 0);
  if (ss->ss_si.si_service){
    av_dict_set(&lm->lm_oc->metadata, "service_name", ss->ss_si.si_service, 0);
    tvhdebug(LS_LIBAV,  "service_name = %s", ss->ss_si.si_service);
  } else
    av_dict_set(&oc->metadata, "service_name", name, 0);
  if (ss->ss_si.si_provider){
    av_dict_set(&lm->lm_oc->metadata, "service_provider", ss->ss_si.si_provider, 0);
    tvhdebug(LS_LIBAV,  "service_provider = %s", ss->ss_si.si_provider);
  } else
    av_dict_set(&oc->metadata, "service_provider", app, 0);

  lm->bsf_h264_filter = av_bsf_get_by_name("h264_mp4toannexb");
  if (lm->bsf_h264_filter == NULL) {
    tvhwarn(LS_LIBAV,  "Failed to get BSF: h264_mp4toannexb");
  }
  lm->bsf_hevc_filter = av_bsf_get_by_name("hevc_mp4toannexb");
  if (lm->bsf_hevc_filter == NULL) {
    tvhwarn(LS_LIBAV,  "Failed to get BSF: hevc_mp4toannexb");
  }
  lm->bsf_vp9_filter = av_bsf_get_by_name("vp9_superframe");
  if (lm->bsf_vp9_filter == NULL) {
    tvhwarn(LS_LIBAV,  "Failed to get BSF: vp9_superframe ");
  }

  oc->max_delay = 0.7 * AV_TIME_BASE;

  for(i=0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    if(ssc->ssc_disabled)
      continue;

    if(!lav_muxer_support_stream(lm->m_config.m_type, ssc->es_type)) {
      tvhwarn(LS_LIBAV,  "%s is not supported in %s", 
	      streaming_component_type2txt(ssc->es_type), 
	      muxer_container_type2txt(lm->m_config.m_type));
      ssc->ssc_muxer_disabled = 1;
      continue;
    }

    if(lav_muxer_add_stream(lm, ssc)) {
      tvherror(LS_LIBAV,  "Failed to add %s stream", 
	       streaming_component_type2txt(ssc->es_type));
      ssc->ssc_muxer_disabled = 1;
      continue;
    }
  }

  if(lm->m_config.m_type == MC_AVMP4) {
    av_dict_set(&opts, "frag_duration", "1", 0);
    av_dict_set(&opts, "ism_lookahead", "0", 0);
  }

  if(lm->m_config.m_type == MC_MPEGTS) {
    // parameters are required to make mpeg-ts output compliant with mpeg-ts standard
    // implemented using documentation: https://ffmpeg.org/ffmpeg-formats.html#mpegts-1
    if (lm->m_config.u.transcode.m_rewrite_sid > 0) {
      // override from profile requested by the user
      snprintf(mpegts_info, sizeof(mpegts_info), "0x%04x", lm->m_config.u.transcode.m_rewrite_sid);
      tvhdebug(LS_LIBAV,  "MPEGTS: mpegts_service_id = %s", mpegts_info);
      av_dict_set(&opts, "mpegts_service_id", mpegts_info, 0);
      // if override requested by the user we let ffmpeg default to decide (0x1000)
      if (!lm->m_config.u.transcode.m_rewrite_pmt) {
        snprintf(mpegts_info, sizeof(mpegts_info), "0x%04x", ss->ss_pmt_pid);
        tvhdebug(LS_LIBAV,  "MPEGTS: mpegts_pmt_start_pid = %s", mpegts_info);
        av_dict_set(&opts, "mpegts_pmt_start_pid", mpegts_info, 0);
      }
      if (!lm->m_config.u.transcode.m_rewrite_nit) {
        av_dict_set(&opts, "mpegts_flags", "nit", 0);
      }
    }
    else {
      // we transfer as many parameters as possible
      if (ss->ss_si.si_tsid) {
        snprintf(mpegts_info, sizeof(mpegts_info), "0x%04x", ss->ss_si.si_tsid);
        tvhdebug(LS_LIBAV,  "MPEGTS: mpegts_transport_stream_id = %s", mpegts_info);
        av_dict_set(&opts, "mpegts_transport_stream_id", mpegts_info, 0);
      }
      if (ss->ss_si.si_type) {
        snprintf(mpegts_info, sizeof(mpegts_info), "0x%04x", ss->ss_si.si_type);
        tvhdebug(LS_LIBAV,  "MPEGTS: mpegts_service_type = %s", mpegts_info);
        av_dict_set(&opts, "mpegts_service_type", mpegts_info, 0);
      }
      if (ss->ss_pmt_pid) {
        snprintf(mpegts_info, sizeof(mpegts_info), "0x%04x", ss->ss_pmt_pid);
        tvhdebug(LS_LIBAV,  "MPEGTS: mpegts_pmt_start_pid = %s", mpegts_info);
        av_dict_set(&opts, "mpegts_pmt_start_pid", mpegts_info, 0);
      }
      if (ss->ss_pcr_pid) {
        snprintf(mpegts_info, sizeof(mpegts_info), "0x%04x", ss->ss_pcr_pid);
        tvhdebug(LS_LIBAV,  "MPEGTS: mpegts_start_pid = %s", mpegts_info);
        av_dict_set(&opts, "mpegts_start_pid", mpegts_info, 0);
      }
      if (ss->ss_service_id) {
        snprintf(mpegts_info, sizeof(mpegts_info), "0x%04x", ss->ss_service_id);
        tvhdebug(LS_LIBAV,  "MPEGTS: mpegts_service_id = %s", mpegts_info);
        av_dict_set(&opts, "mpegts_service_id", mpegts_info, 0);
      }
      if (ss->ss_si.si_onid) {
        snprintf(mpegts_info, sizeof(mpegts_info), "0x%04x", ss->ss_si.si_onid);
        tvhdebug(LS_LIBAV,  "MPEGTS: mpegts_original_network_id = %s", mpegts_info);
        av_dict_set(&opts, "mpegts_original_network_id", mpegts_info, 0);
      }
      av_dict_set(&opts, "mpegts_flags", "nit", 0);
      av_dict_set(&opts, "mpegts_copyts", "1", 0);
    }
  }
  
  if(!lm->lm_oc->nb_streams) {
    tvherror(LS_LIBAV,  "No supported streams available");
    lm->m_errors++;
    goto fail;
  } else if(avformat_write_header(lm->lm_oc, &opts) < 0) {
    tvherror(LS_LIBAV,  "Failed to write %s header", 
	     muxer_container_type2txt(lm->m_config.m_type));
    lm->m_errors++;
    goto fail;
  }

  lm->lm_init = 1;
  rc = 0;

fail:
  if (opts)
    av_dict_free(&opts);

  return rc;
}


/**
 * Handle changes to the streams (usually PMT updates)
 */
static int
lav_muxer_reconfigure(muxer_t* m, const struct streaming_start *ss)
{
  lav_muxer_t *lm = (lav_muxer_t*)m;

  lm->m_errors++;

  return -1;
}


/**
 * Open the muxer and write the header
 */
static int
lav_muxer_open_stream(muxer_t *m, int fd)
{
  uint8_t *buf;
  AVIOContext *pb;
  lav_muxer_t *lm = (lav_muxer_t*)m;

  buf = av_malloc(MUX_BUF_SIZE);
  pb = avio_alloc_context(buf, MUX_BUF_SIZE, 1, lm, NULL, 
			  lav_muxer_write, NULL);
  pb->seekable = 0;
  lm->lm_oc->pb = pb;
  lm->lm_fd = fd;

  return 0;
}


static int
lav_muxer_open_file(muxer_t *m, const char *filename)
{
  AVFormatContext *oc;
  lav_muxer_t *lm = (lav_muxer_t*)m;
  char buf[256];
  int r;

  lm->lm_fd = -1;
  oc = lm->lm_oc;

  if((r = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE)) < 0) {
    av_strerror(r, buf, sizeof(buf));
    tvherror(LS_LIBAV,  "%s: Could not open -- %s", filename, buf);
    lm->m_errors++;
    return -1;
  }

  /* bypass umask settings */
  if (chmod(filename, lm->m_config.m_file_permissions))
    tvherror(LS_LIBAV, "%s: Unable to change permissions -- %s",
             filename, strerror(errno));

  return 0;
}


/**
 * Write a packet to the muxer
 */
static int
lav_muxer_write_pkt(muxer_t *m, streaming_message_type_t smt, void *data)
{
  int i;
  AVFormatContext *oc;
  AVStream *st;
  AVPacket packet;
  enum AVCodecID codec_id;
  th_pkt_t *pkt = (th_pkt_t*)data, *opkt;
  lav_muxer_t *lm = (lav_muxer_t*)m;
  unsigned char *tofree;
  int rc = 0;

  assert(smt == SMT_PACKET);

  oc = lm->lm_oc;

  if(!oc->nb_streams) {
    tvherror(LS_LIBAV, "No streams to mux");
    rc = -1;
    goto ret;
  }

  if(!lm->lm_init) {
    tvherror(LS_LIBAV, "Muxer not initialized correctly");
    rc = -1;
    goto ret;
  }

  for(i=0; i<oc->nb_streams; i++) {
    st = oc->streams[i];

    if(st->id != pkt->pkt_componentindex)
      continue;
    if(pkt->pkt_payload == NULL)
      continue;

    tofree = NULL;
    memset(&packet, 0, sizeof(packet));
    packet.pos = -1;
    codec_id = st->codecpar->codec_id;

    if (codec_id == AV_CODEC_ID_AAC) {
      /* remove ADTS header */
      packet.data = pktbuf_ptr(pkt->pkt_payload) + 7;
      packet.size = pktbuf_len(pkt->pkt_payload) - 7;
    } else {
      if (lm->m_config.m_type == MC_AVMP4 &&
          (codec_id == AV_CODEC_ID_H264 || codec_id == AV_CODEC_ID_HEVC)) {
        pkt = avc_convert_pkt(opkt = pkt);
        pkt_ref_dec(opkt);
      }
      packet.data = pktbuf_ptr(pkt->pkt_payload);
      packet.size = pktbuf_len(pkt->pkt_payload);
    }


    packet.stream_index = st->index;
 
    packet.pts      = av_rescale_q_rnd(pkt->pkt_pts, mpeg_tc, st->time_base,
                                       AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    packet.dts      = av_rescale_q_rnd(pkt->pkt_dts, mpeg_tc, st->time_base,
                                       AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    packet.duration = av_rescale_q_rnd(pkt->pkt_duration, mpeg_tc, st->time_base,
                                       AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);

    if(!SCT_ISVIDEO(pkt->pkt_type) || pkt->v.pkt_frametype < PKT_P_FRAME)
      packet.flags |= AV_PKT_FLAG_KEY;

    if((rc = av_interleaved_write_frame(oc, &packet)))
      tvhwarn(LS_LIBAV,  "Failed to write frame");

    if(tofree && tofree != pktbuf_ptr(pkt->pkt_payload))
      av_free(tofree);

    break;
  }

 ret:
  lm->m_errors += (rc != 0);
  pkt_ref_dec(pkt);

  return rc;
}


/**
 * NOP
 */
static int
lav_muxer_write_meta(muxer_t *m, struct epg_broadcast *eb, const char *comment)
{
  return 0;
}


/**
 * NOP
 */
static int
lav_muxer_add_marker(muxer_t* m)
{
  return 0;
}


/**
 * Close the muxer and append trailer to output
 */
static int
lav_muxer_close(muxer_t *m)
{
  AVFormatContext *oc;
  int ret = 0;
  lav_muxer_t *lm = (lav_muxer_t*)m;

  if(lm->lm_init && av_write_trailer(lm->lm_oc) < 0) {
    tvhwarn(LS_LIBAV,  "Failed to write %s trailer", 
	    muxer_container_type2txt(lm->m_config.m_type));
    lm->m_errors++;
    ret = -1;
  }

  oc = lm->lm_oc;
  if (lm->lm_fd >= 0) {
    av_freep(&oc->pb->buffer);
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 80, 100)
    avio_context_free(&oc->pb);
#else
    av_freep(&oc->pb);
#endif
    lm->lm_fd = -1;
  } else {
    avio_closep(&oc->pb);
  }

  return ret;
}


/**
 * Free all memory associated with the muxer
 */
static void
lav_muxer_destroy(muxer_t *m)
{
  int i;
  lav_muxer_t *lm = (lav_muxer_t*)m;

  if(lm->ctx){
    if (av_bsf_send_packet(lm->ctx, NULL))
      tvhwarn(LS_LIBAV,  "Failed to send packet to AVBSFContext (close)");
    av_bsf_free(&lm->ctx);
  }

  if (lm->lm_oc) {
    for(i=0; i<lm->lm_oc->nb_streams; i++)
      av_freep(&lm->lm_oc->streams[i]->codecpar->extradata);
  }

  if(lm->lm_oc) {
    avformat_free_context(lm->lm_oc);
    lm->lm_oc = NULL;
  }

  muxer_config_free(&lm->m_config);
  muxer_hints_free(lm->m_hints);
  free(lm);
}


/**
 * Create a new libavformat based muxer
 */
muxer_t*
lav_muxer_create(const muxer_config_t *m_cfg,
                 const muxer_hints_t *hints)
{
  const char *mux_name;
  lav_muxer_t *lm;
#if LIBAVCODEC_VERSION_MAJOR > 58
  const AVOutputFormat *fmt;
#else
  AVOutputFormat *fmt;
#endif

  switch(m_cfg->m_type) {
  case MC_MPEGPS:
    mux_name = "dvd";
    break;
  case MC_MATROSKA:
  case MC_AVMATROSKA:
    mux_name = "matroska";
    break;
  case MC_WEBM:
  case MC_AVWEBM:
    mux_name = "webm";
    break;
  case MC_AVMP4:
    mux_name = "mp4";
    break;
  default:
    mux_name = muxer_container_type2txt(m_cfg->m_type);
    break;
  }

  fmt = av_guess_format(mux_name, NULL, NULL);
  if(!fmt) {
    tvherror(LS_LIBAV,  "Can't find the '%s' muxer", mux_name);
    return NULL;
  }

  lm = calloc(1, sizeof(lav_muxer_t));
  lm->m_open_stream  = lav_muxer_open_stream;
  lm->m_open_file    = lav_muxer_open_file;
  lm->m_init         = lav_muxer_init;
  lm->m_reconfigure  = lav_muxer_reconfigure;
  lm->m_mime         = lav_muxer_mime;
  lm->m_add_marker   = lav_muxer_add_marker;
  lm->m_write_meta   = lav_muxer_write_meta;
  lm->m_write_pkt    = lav_muxer_write_pkt;
  lm->m_close        = lav_muxer_close;
  lm->m_destroy      = lav_muxer_destroy;
  lm->lm_oc          = avformat_alloc_context();
  lm->lm_oc->oformat = fmt;
  lm->lm_fd          = -1;
  lm->lm_init        = 0;

  return (muxer_t*)lm;
}
