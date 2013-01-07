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
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>

#include "tvheadend.h"
#include "streaming.h"
#include "epg.h"
#include "channels.h"
#include "libav.h"
#include "muxer_libav.h"

typedef struct lav_muxer {
  muxer_t;
  AVFormatContext *lm_oc;
  AVBitStreamFilterContext *lm_h264_filter;
  int lm_fd;
} lav_muxer_t;

#define MUX_BUF_SIZE 4096

static const AVRational mpeg_tc = {1, 90000};


/**
 * Callback function for libavformat
 */
static int 
lav_muxer_write(void *opaque, uint8_t *buf, int buf_size)
{
  int r;
  lav_muxer_t *lm = (lav_muxer_t*)opaque;
  
  r = write(lm->lm_fd, buf, buf_size);
  lm->m_errors += (r != buf_size);
  
  return r;
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

  st = avformat_new_stream(lm->lm_oc, NULL);
  if (!st)
    return -1;

  st->id = ssc->ssc_index;
  c = st->codec;
  c->codec_id = streaming_component_type2codec_id(ssc->ssc_type);

  switch(lm->m_container) {
  case MC_MATROSKA:
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
    c->extradata_size = pktbuf_len(ssc->ssc_gh);
    c->extradata = av_malloc(c->extradata_size);
    memcpy(c->extradata, pktbuf_ptr(ssc->ssc_gh), 
	   pktbuf_len(ssc->ssc_gh));
  }

  if(SCT_ISAUDIO(ssc->ssc_type)) {
    c->codec_type    = AVMEDIA_TYPE_AUDIO;
    c->sample_fmt    = AV_SAMPLE_FMT_S16;

    c->sample_rate   = sri_to_rate(ssc->ssc_sri);
    c->channels      = ssc->ssc_channels;

    c->time_base.num = 1;
    c->time_base.den = c->sample_rate;

    av_dict_set(&st->metadata, "language", ssc->ssc_lang, 0);

  } else if(SCT_ISVIDEO(ssc->ssc_type)) {
    c->codec_type = AVMEDIA_TYPE_VIDEO;
    c->width      = ssc->ssc_width;
    c->height     = ssc->ssc_height;

    c->time_base.num  = 1;
    c->time_base.den = 25;

    c->sample_aspect_ratio.num = ssc->ssc_aspect_num;
    c->sample_aspect_ratio.den = ssc->ssc_aspect_den;

    st->sample_aspect_ratio.num = c->sample_aspect_ratio.num;
    st->sample_aspect_ratio.den = c->sample_aspect_ratio.den;

  } else if(SCT_ISSUBTITLE(ssc->ssc_type)) {
    c->codec_type = AVMEDIA_TYPE_SUBTITLE;
    av_dict_set(&st->metadata, "language", ssc->ssc_lang, 0);
  }

  if(lm->lm_oc->oformat->flags & AVFMT_GLOBALHEADER)
    c->flags |= CODEC_FLAG_GLOBAL_HEADER;

  return 0;
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
    ret |= SCT_ISAUDIO(type);
    ret |= SCT_ISVIDEO(type);
    ret |= SCT_ISSUBTITLE(type);
    break;

  case MC_MPEGTS:
    ret |= (type == SCT_MPEG2VIDEO);
    ret |= (type == SCT_H264);

    ret |= (type == SCT_MPEG2AUDIO);
    ret |= (type == SCT_AC3);
    ret |= (type == SCT_AAC);
    ret |= (type == SCT_MP4A);
    ret |= (type == SCT_EAC3);

    //Some pids lack pts, disable for now
    //ret |= (type == SCT_TELETEXT);
    ret |= (type == SCT_DVBSUB);
    break;

  case MC_MPEGPS:
    ret |= (type == SCT_MPEG2VIDEO);
    ret |= (type == SCT_MPEG2AUDIO);
    ret |= (type == SCT_AC3);

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

    if(!lav_muxer_support_stream(m->m_container, ssc->ssc_type))
      continue;

    has_video |= SCT_ISVIDEO(ssc->ssc_type);
    has_audio |= SCT_ISAUDIO(ssc->ssc_type);
  }

  if(has_video)
    return muxer_container_type2mime(m->m_container, 1);
  else if(has_audio)
    return muxer_container_type2mime(m->m_container, 0);
  else
    return muxer_container_type2mime(MC_UNKNOWN, 0);
}


/**
 * Init the muxer with streams
 */
static int
lav_muxer_init(muxer_t* m, const struct streaming_start *ss, const char *name)
{
  int i;
  const streaming_start_component_t *ssc;
  AVFormatContext *oc;
  lav_muxer_t *lm = (lav_muxer_t*)m;
  char app[128];

  snprintf(app, sizeof(app), "Tvheadend %s", tvheadend_version);

  oc = lm->lm_oc;

  av_dict_set(&oc->metadata, "title", name, 0);
  av_dict_set(&oc->metadata, "service_name", name, 0);
  av_dict_set(&oc->metadata, "service_provider", app, 0);

  if(lm->m_container == MC_MPEGTS)
    lm->lm_h264_filter = av_bitstream_filter_init("h264_mp4toannexb");

  oc->max_delay = 0.7 * AV_TIME_BASE;

  for(i=0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    if(ssc->ssc_disabled)
      continue;

    if(!lav_muxer_support_stream(lm->m_container, ssc->ssc_type)) {
      tvhlog(LOG_WARNING, "libav",  "%s is not supported in %s", 
	     streaming_component_type2txt(ssc->ssc_type), 
	     muxer_container_type2txt(lm->m_container));
      continue;
    }

    if(lav_muxer_add_stream(lm, ssc)) {
      tvhlog(LOG_ERR, "libav",  "Failed to add %s stream", 
	     streaming_component_type2txt(ssc->ssc_type));
      continue;
    }
  }

  if(!lm->lm_oc->nb_streams) {
    tvhlog(LOG_ERR, "libav",  "No supported streams available");
    lm->m_errors++;
    return -1;
  } else if(avformat_write_header(lm->lm_oc, NULL) < 0) {
    tvhlog(LOG_ERR, "libav",  "Failed to write %s header", 
	   muxer_container_type2txt(lm->m_container));
    lm->m_errors++;
    return -1;
  }

  return 0;
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

  oc = lm->lm_oc;
  snprintf(oc->filename, sizeof(oc->filename), "%s", filename);

  if(avio_open(&oc->pb, filename, AVIO_FLAG_WRITE) < 0) {
    tvhlog(LOG_ERR, "libav",  "Could not open %s", filename);
    lm->m_errors++;
    return -1;
  }

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
  th_pkt_t *pkt = (th_pkt_t*)data;
  lav_muxer_t *lm = (lav_muxer_t*)m;

  assert(smt == SMT_PACKET);

  oc = lm->lm_oc;

  if(!oc->nb_streams) {
    tvhlog(LOG_ERR, "libav",  "No streams to mux");
    lm->m_errors++;
    return -1;
  }

  for(i=0; i<oc->nb_streams; i++) {
    st = oc->streams[i];

    if(st->id != pkt->pkt_componentindex)
      continue;

    av_init_packet(&packet);

    if(st->codec->codec_id == CODEC_ID_MPEG2VIDEO)
      pkt = pkt_merge_header(pkt);

    if(lm->lm_h264_filter && st->codec->codec_id == CODEC_ID_H264) {
         av_bitstream_filter_filter(lm->lm_h264_filter, 
				    st->codec, 
				    NULL, 
				    &packet.data, 
				    &packet.size, 
				    pktbuf_ptr(pkt->pkt_payload), 
				    pktbuf_len(pkt->pkt_payload), 
				    pkt->pkt_frametype < PKT_P_FRAME);
    } else {
      packet.data = pktbuf_ptr(pkt->pkt_payload);
      packet.size = pktbuf_len(pkt->pkt_payload);
    }

    packet.stream_index = st->index;
 
    packet.pts      = av_rescale_q(pkt->pkt_pts     , mpeg_tc, st->time_base);
    packet.dts      = av_rescale_q(pkt->pkt_dts     , mpeg_tc, st->time_base);
    packet.duration = av_rescale_q(pkt->pkt_duration, mpeg_tc, st->time_base);

    if(pkt->pkt_frametype < PKT_P_FRAME)
      packet.flags |= AV_PKT_FLAG_KEY;

    if (av_interleaved_write_frame(oc, &packet) != 0) {
        tvhlog(LOG_WARNING, "libav",  "Failed to write frame");
	lm->m_errors++;
	return -1;
    }

    break;
  }

  pkt_ref_dec(pkt);

  return 0;
}


/**
 * NOP
 */
static int
lav_muxer_write_meta(muxer_t *m, struct epg_broadcast *eb)
{
  return 0;
}


/**
 * Close the muxer and append trailer to output
 */
static int
lav_muxer_close(muxer_t *m)
{
  int i;
  int ret = 0;
  lav_muxer_t *lm = (lav_muxer_t*)m;

  if(lm->lm_oc->nb_streams && av_write_trailer(lm->lm_oc) < 0) {
    tvhlog(LOG_WARNING, "libav",  "Failed to write %s trailer", 
	   muxer_container_type2txt(lm->m_container));
    lm->m_errors++;
    ret = -1;
  }

  if(lm->lm_h264_filter)
    av_bitstream_filter_close(lm->lm_h264_filter);

  for(i=0; i<lm->lm_oc->nb_streams; i++)
    av_freep(&lm->lm_oc->streams[i]->codec->extradata);
 
  lm->lm_oc->nb_streams = 0;

  return ret;
}


/**
 * Free all memory associated with the muxer
 */
static void
lav_muxer_destroy(muxer_t *m)
{
  lav_muxer_t *lm = (lav_muxer_t*)m;

  if(lm->lm_oc && lm->lm_oc->pb)
    av_free(lm->lm_oc->pb);

  if(lm->lm_oc)
    av_free(lm->lm_oc);

  free(lm);
}


/**
 * Create a new libavformat based muxer
 */
muxer_t*
lav_muxer_create(muxer_container_type_t mc)
{
  const char *mux_name;
  lav_muxer_t *lm;
  AVOutputFormat *fmt;

  switch(mc) {
  case MC_MPEGPS:
    mux_name = "dvd";
    break;
  default:
    mux_name = muxer_container_type2txt(mc);
    break;
  }

  fmt = av_guess_format(mux_name, NULL, NULL);
  if(!fmt) {
    tvhlog(LOG_ERR, "libav",  "Can't find the '%s' muxer", mux_name);
    return NULL;
  }

  lm = calloc(1, sizeof(lav_muxer_t));
  lm->m_open_stream  = lav_muxer_open_stream;
  lm->m_open_file    = lav_muxer_open_file;
  lm->m_init         = lav_muxer_init;
  lm->m_reconfigure  = lav_muxer_reconfigure;
  lm->m_mime         = lav_muxer_mime;
  lm->m_write_meta   = lav_muxer_write_meta;
  lm->m_write_pkt    = lav_muxer_write_pkt;
  lm->m_close        = lav_muxer_close;
  lm->m_destroy      = lav_muxer_destroy;
  lm->m_container    = mc;
  lm->lm_oc          = avformat_alloc_context();
  lm->lm_oc->oformat = fmt;
  lm->lm_fd          = -1;

  return (muxer_t*)lm;
}

