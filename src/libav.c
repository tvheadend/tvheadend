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
 * Translate a component type to a libavcodec id
 */
enum CodecID
streaming_component_type2codec_id(streaming_component_type_t type)
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
 * 
 */ 
void
libav_init(void)
{
  av_log_set_callback(libav_log_callback);
  av_log_set_level(AV_LOG_VERBOSE);
  av_register_all();
}

