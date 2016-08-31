#include "transcoding/transcode.h"
#include "libav.h"

/**
 *
 */
static void
libav_log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
  int severity = LOG_TVH_NOTIFY;

  if (level != AV_LOG_QUIET &&
      ((level <= AV_LOG_INFO) || (tvhlog_options & TVHLOG_OPT_LIBAV))) {
    switch(level) {
    case AV_LOG_TRACE:
#if ENABLE_TRACE
      severity |= LOG_TRACE;
      break;
#endif
    case AV_LOG_DEBUG:
    case AV_LOG_VERBOSE:
      severity |= LOG_DEBUG;
      break;
    case AV_LOG_INFO:
      severity |= LOG_INFO;
      break;
    case AV_LOG_WARNING:
      severity |= LOG_WARNING;
      break;
    case AV_LOG_ERROR:
      severity |= LOG_ERR;
      break;
    case AV_LOG_FATAL:
      severity |= LOG_CRIT;
      break;
    case AV_LOG_PANIC:
      severity |= LOG_EMERG;
      break;
    default:
      break;
    }
    va_list ap;
    va_copy(ap, vl);
    tvhlogv(__FILE__, __LINE__, severity, LS_LIBAV, fmt, &ap);
    va_end(ap);
  }
}

/**
 * Translate a component type to a libavcodec id
 */
enum AVCodecID
streaming_component_type2codec_id(streaming_component_type_t type)
{
  enum AVCodecID codec_id = AV_CODEC_ID_NONE;

  switch(type) {
  case SCT_H264:
    codec_id = AV_CODEC_ID_H264;
    break;
  case SCT_MPEG2VIDEO:
    codec_id = AV_CODEC_ID_MPEG2VIDEO;
    break;
  case SCT_VP8:
    codec_id = AV_CODEC_ID_VP8;
    break;
  case SCT_VP9:
    codec_id = AV_CODEC_ID_VP9;
    break;
  case SCT_HEVC:
    codec_id = AV_CODEC_ID_HEVC;
    break;
  case SCT_THEORA:
    codec_id = AV_CODEC_ID_THEORA;
    break;
  case SCT_AC3:
    codec_id = AV_CODEC_ID_AC3;
    break;
  case SCT_EAC3:
    codec_id = AV_CODEC_ID_EAC3;
    break;
  case SCT_MP4A:
  case SCT_AAC:
    codec_id = AV_CODEC_ID_AAC;
    break;
  case SCT_MPEG2AUDIO:
    codec_id = AV_CODEC_ID_MP2;
    break;
  case SCT_VORBIS:
    codec_id = AV_CODEC_ID_VORBIS;
    break;
  case SCT_OPUS:
    codec_id = AV_CODEC_ID_OPUS;
    break;
  case SCT_DVBSUB:
    codec_id = AV_CODEC_ID_DVB_SUBTITLE;
    break;
  case SCT_TEXTSUB:
    codec_id = AV_CODEC_ID_TEXT;
    break;
 case SCT_TELETEXT:
    codec_id = AV_CODEC_ID_DVB_TELETEXT;
    break;
  default:
    break;
  }

  return codec_id;
}


/**
 * Translate a libavcodec id to a component type
 */
streaming_component_type_t
codec_id2streaming_component_type(enum AVCodecID id)
{
  streaming_component_type_t type = SCT_UNKNOWN;

  switch(id) {
  case AV_CODEC_ID_H264:
    type = SCT_H264;
    break;
  case AV_CODEC_ID_MPEG2VIDEO:
    type = SCT_MPEG2VIDEO;
    break;
  case AV_CODEC_ID_VP8:
    type = SCT_VP8;
    break;
  case AV_CODEC_ID_VP9:
    type = SCT_VP9;
    break;
  case AV_CODEC_ID_HEVC:
    type = SCT_HEVC;
    break;
  case AV_CODEC_ID_THEORA:
    type = SCT_THEORA;
    break;
  case AV_CODEC_ID_AC3:
    type = SCT_AC3;
    break;
  case AV_CODEC_ID_EAC3:
    type = SCT_EAC3;
    break;
  case AV_CODEC_ID_AAC:
    type = SCT_AAC;
    break;
  case AV_CODEC_ID_MP2:
    type = SCT_MPEG2AUDIO;
    break;
  case AV_CODEC_ID_VORBIS:
    type = SCT_VORBIS;
    break;
  case AV_CODEC_ID_OPUS:
    type = SCT_OPUS;
    break;
  case AV_CODEC_ID_DVB_SUBTITLE:
    type = SCT_DVBSUB;
    break;
  case AV_CODEC_ID_TEXT:
    type = SCT_TEXTSUB;
    break;
  case AV_CODEC_ID_DVB_TELETEXT:
    type = SCT_TELETEXT;
    break;
  case AV_CODEC_ID_NONE:
    type = SCT_NONE;
    break;
  default:
    break;
  }

  return type;
}


/**
 *
 */
int
libav_is_encoder(AVCodec *codec)
{
#if LIBAVCODEC_VERSION_INT >= ((54<<16)+(7<<8)+0)
  return av_codec_is_encoder(codec);
#else
  return codec->encode || codec->encode2;
#endif
}

/**
 *
 */
void
libav_set_loglevel(void)
{
  int level = (tvhlog_options & TVHLOG_OPT_LIBAV) ? AV_LOG_DEBUG : AV_LOG_INFO;
  av_log_set_level(level);
}

/**
 *
 */
void
libav_init(void)
{
  libav_set_loglevel();
  av_log_set_callback(libav_log_callback);
  av_register_all();
  avformat_network_init();
  avfilter_register_all();
  transcode_init();
}

/**
 *
 */
void
libav_done(void)
{
  transcode_done();
  avformat_network_deinit();
}