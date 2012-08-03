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

#include <unistd.h>
#include <libavformat/avformat.h>

#include "tvheadend.h"
#include "streaming.h"
#include "channels.h"
#include "lav_muxer.h"

typedef struct lav_muxer {
  muxer_t;
  AVFormatContext *lm_oc;
} lav_muxer_t;

#define MUX_BUF_SIZE 4096

/**
 * Translate a component type to a libavcodec id
 */
static enum CodecID
lav_muxer_get_codec_id(streaming_component_type_t type)
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
  case SCT_DVBSUB:
    codec_id = CODEC_ID_DVB_SUBTITLE;
    break;
  case SCT_TEXTSUB:
    codec_id = CODEC_ID_TEXT;
    break;
 case SCT_TELETEXT:
    codec_id = CODEC_ID_DVB_TELETEXT;
    break;
  default:
    codec_id = CODEC_ID_NONE;
    break;
  }

  return codec_id;
}



/**
 * Callback function for libavformat
 */
static int 
lav_muxer_write(void *opaque, uint8_t *buf, int buf_size)
{
  int r;
  lav_muxer_t *lm = (lav_muxer_t*)opaque;
  
  r = write(lm->m_fd, buf, buf_size);
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

  st->id        = ssc->ssc_index;
  st->time_base = AV_TIME_BASE_Q;

  c = st->codec;
  c->codec_id = lav_muxer_get_codec_id(ssc->ssc_type);

  if(ssc->ssc_gh) {
    c->extradata = pktbuf_ptr(ssc->ssc_gh);
    c->extradata_size = pktbuf_len(ssc->ssc_gh);
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
    ret |= (type == SCT_MPEG2AUDIO);
    ret |= (type == SCT_H264);
    ret |= (type == SCT_AC3);
    //Some pids lack pts, disable for now
    //ret |= (type == SCT_TELETEXT);
    ret |= (type == SCT_DVBSUB);
    ret |= (type == SCT_AAC);
    ret |= (type == SCT_EAC3);
    break;

  default:
    break;
  }

  return ret;
}


/**
 * Init the muxer with streams
 */
static int
lav_muxer_init(muxer_t* m, const struct streaming_start *ss, const struct channel *ch)
{
  int i;
  int cnt = 0;
  const streaming_start_component_t *ssc;
  AVFormatContext *oc;
  lav_muxer_t *lm = (lav_muxer_t*)m;
  char app[128];

  snprintf(app, sizeof(app), "Tvheadend %s", tvheadend_version);

  oc = lm->lm_oc;

  av_dict_set(&oc->metadata, "title", ch->ch_name, 0);
  av_dict_set(&oc->metadata, "service_name", ch->ch_name, 0);
  av_dict_set(&oc->metadata, "service_provider", app, 0);

  for(i=0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    if(!lav_muxer_support_stream(lm->m_container, ssc->ssc_type))
      continue;

    if(ssc->ssc_disabled)
      continue;

    if(lav_muxer_add_stream(lm, ssc) < 0)
      continue;

    cnt++;
  }

  return cnt;
}


/**
 * Open the muxer and write the header
 */
static int
lav_muxer_open(muxer_t *m)
{
  lav_muxer_t *lm = (lav_muxer_t*)m;

  if(avformat_write_header(lm->lm_oc, NULL) < 0) {
    tvhlog(LOG_WARNING, "mux",  "Failed to write %s header", 
	   muxer_container_type2txt(lm->m_container));
    lm->m_errors++;
  }

  return lm->m_errors;
}


/**
 * Write a packet to the muxer
 */
static int
lav_muxer_write_pkt(muxer_t *m, struct th_pkt *pkt)
{
  int i;
  AVFormatContext *oc;
  AVStream *st;
  AVPacket packet;
  lav_muxer_t *lm = (lav_muxer_t*)m;

  oc = lm->lm_oc;

  for(i=0; i<oc->nb_streams; i++) {
    st = oc->streams[i];

    if(st->id != pkt->pkt_componentindex)
      continue;

    av_init_packet(&packet);

    if(st->codec->codec_id == CODEC_ID_MPEG2VIDEO)
      pkt = pkt_merge_header(pkt);

    packet.data = pktbuf_ptr(pkt->pkt_payload);
    packet.size = pktbuf_len(pkt->pkt_payload);
    packet.stream_index = st->index;

    if(lm->m_container != MC_MPEGTS) {
      packet.pts      = ts_rescale(pkt->pkt_pts, 1000);
      packet.dts      = ts_rescale(pkt->pkt_dts, 1000);
      packet.duration = ts_rescale(pkt->pkt_duration, 1000);
    } else {
      packet.pts      = pkt->pkt_pts;
      packet.dts      = pkt->pkt_dts;
      packet.duration = pkt->pkt_duration;
    }

    if(pkt->pkt_frametype < PKT_P_FRAME)
      packet.flags |= AV_PKT_FLAG_KEY;

    if (av_interleaved_write_frame(oc, &packet) != 0) {
        tvhlog(LOG_WARNING, "mux",  "Failed to write frame");
	lm->m_errors++;
    }

    break;
  }

  return lm->m_errors;
}


/**
 * NOP
 */
static int
lav_muxer_write_meta(muxer_t *m, epg_broadcast_t *eb)
{
  lav_muxer_t *lm = (lav_muxer_t*)m;

  return lm->m_errors;
}


/**
 * Close the muxer and append trailer to output
 */
static int
lav_muxer_close(muxer_t *m)
{
  lav_muxer_t *lm = (lav_muxer_t*)m;

  if(av_write_trailer(lm->lm_oc) < 0) {
    tvhlog(LOG_WARNING, "mux",  "Failed to write %s trailer", 
	   muxer_container_type2txt(lm->m_container));
    lm->m_errors++;
  }

  return lm->m_errors;
}


/**
 * Free all memory associated with the muxer
 */
static void
lav_muxer_destroy(muxer_t *m)
{
  lav_muxer_t *lm = (lav_muxer_t*)m;

  av_free(lm->lm_oc->pb);
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
  AVIOContext *pb;
  AVOutputFormat *fmt;
  uint8_t *buf;

  mux_name = muxer_container_type2txt(mc);
  fmt = av_guess_format(mux_name, NULL, NULL);
  if(!fmt) {
    tvhlog(LOG_ERR, "mux",  "Can't find the '%s' muxer", mux_name);
    return NULL;
  }

  lm = calloc(1, sizeof(lav_muxer_t));
  lm->m_init         = lav_muxer_init;
  lm->m_open         = lav_muxer_open;
  lm->m_close        = lav_muxer_close;
  lm->m_destroy      = lav_muxer_destroy;
  lm->m_write_meta   = lav_muxer_write_meta;
  lm->m_write_pkt    = lav_muxer_write_pkt;
  lm->lm_oc          = avformat_alloc_context();
  lm->lm_oc->oformat = fmt;

  buf = av_malloc(MUX_BUF_SIZE);
  pb = avio_alloc_context(buf, MUX_BUF_SIZE, 1, lm, NULL, 
			  lav_muxer_write, NULL);
  pb->seekable = 0;

  lm->lm_oc->pb = pb;

  return (muxer_t*)lm;
}

