#include "plumbing/transcoding.h"
#include "libav.h"

/**
 *
 */
static void
libav_log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
    char message[8192];
    char *nl;
    char *l;

    if ((level == AV_LOG_DEBUG) && !(tvhlog_options & TVHLOG_OPT_LIBAV))
      return;

    memset(message, 0, sizeof(message));
    vsnprintf(message, sizeof(message), fmt, vl);

    l = message;

    if(level == AV_LOG_DEBUG)
#if ENABLE_TRACE
      level = LOG_TRACE;
#else
      level = LOG_DEBUG;
#endif
    else if(level == AV_LOG_VERBOSE)
      level = LOG_DEBUG;
    else if(level == AV_LOG_INFO)
      level = LOG_INFO;
    else if(level == AV_LOG_WARNING)
      level = LOG_WARNING;
    else if(level == AV_LOG_ERROR)
      level = LOG_ERR;
    else if(level == AV_LOG_FATAL)
      level = LOG_CRIT;
    else if(level == AV_LOG_PANIC)
      level = LOG_EMERG;

    if (level == LOG_INFO) {
      if (!strncmp(message, "--prefix=/", 10))
        return;
    }

    while(l < message + sizeof(message)) {
      nl = strstr(l, "\n");
      if(nl)
        *nl = '\0';

      if(!strlen(l))
        break;

      tvhlog(level, "libav", "%s", l);

      if(!nl)
        break;

      l = nl + 1;
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
    codec_id = AV_CODEC_ID_NONE;
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
  streaming_component_type_t type = AV_CODEC_ID_NONE;

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
    type = SCT_UNKNOWN;
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
  int level = AV_LOG_VERBOSE;

  if (tvhlog_options & TVHLOG_OPT_LIBAV)
    level = AV_LOG_DEBUG;

  av_log_set_level(level);
}

/**
 *
 */
void
libav_init(void)
{
  av_log_set_callback(libav_log_callback);
  libav_set_loglevel();
  av_register_all();
  transcoding_init();
}
