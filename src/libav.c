#include "transcoding/transcode.h"
#include "libav.h"
#if ENABLE_VAAPI
#include <va/va.h>
#endif

/**
 *
 */
static void
libav_log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
  int severity = LOG_TVH_NOTIFY, l1, l2;
  const char *class_name;
  char *fmt1;

  if (level != AV_LOG_QUIET &&
      ((level <= AV_LOG_INFO) || (tvhlog_options & TVHLOG_OPT_LIBAV))) {

    class_name = ptr && *(void **)ptr ? av_default_item_name(ptr) : "";

    if (fmt == NULL)
      return;

    l1 = strlen(fmt);
    l2 = strlen(class_name);
    fmt1 = alloca(l1 + l2 + 3);

    strcpy(fmt1, class_name);
    if (class_name[0])
      strcat(fmt1, ": ");
    strcat(fmt1, fmt);

    /* remove trailing newline */
    if (fmt[l1-1] == '\n')
      fmt1[l1 + l2 + 1] = '\0';

    if (strcmp(class_name, "AVFormatContext") == 0) {
      if (strcmp(fmt, "Negative cts, previous timestamps might be wrong.\n") == 0) {
        level = AV_LOG_TRACE;
      } else if (strcmp(fmt, "Invalid timestamps stream=%d, pts=%s, dts=%s, size=%d\n") == 0 &&
                 strstr(fmt, ", pts=-") == 0) {
        level = AV_LOG_TRACE;
      }
    } else if (strcmp(class_name, "AVCodecContext") == 0) {
      if (strcmp(fmt, "forced frame type (%d) at %d was changed to frame type (%d)\n") == 0) {
        level = AV_LOG_TRACE;
      }
    }

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
    tvhlogv(__FILE__, __LINE__, severity, LS_LIBAV, fmt1, &ap);
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
  // Enable once supported, see https://trac.ffmpeg.org/ticket/8349
  // case SCT_AC4:
  //   codec_id = AV_CODEC_ID_AC4;
  //   break;
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
  // Enable once supported, see https://trac.ffmpeg.org/ticket/8349
  // case AV_CODEC_ID_AC4:
  //   type = SCT_AC4;
  //   break;
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
  case AV_CODEC_ID_FLAC:
    type = SCT_FLAC;
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
#if ENABLE_VAAPI
#ifdef VA_FOURCC_I010
static void libav_va_log(int severity, const char *msg)
{
  char *s;
  int l;

  if (msg == NULL || *msg == '\0')
    return;
  s = tvh_strdupa(msg);
  l = strlen(s);
  if (s[l-1] == '\n')
    s[l-1] = '\0';
  tvhlog(severity, LS_LIBAV, "%s", s);
}

#if VA_CHECK_VERSION(1, 0, 0)
static void libav_va_error_callback(void *context, const char *msg)
#else
static void libav_va_error_callback(const char *msg)
#endif
{
  libav_va_log(LOG_ERR, msg);
}

#if VA_CHECK_VERSION(1, 0, 0)
static void libav_va_info_callback(void *context, const char *msg)
#else
static void libav_va_info_callback(const char *msg)
#endif
{
  libav_va_log(LOG_INFO, msg);
}
#endif
#endif

/**
 *
 */
static void
libav_vaapi_init(void)
{
#if ENABLE_VAAPI
#ifdef VA_FOURCC_I010
#if !VA_CHECK_VERSION(1, 0, 0)
  vaSetErrorCallback(libav_va_error_callback);
  vaSetInfoCallback(libav_va_info_callback);
#endif
#endif
#endif
}

/**
 *
 */
void
libav_vaapi_init_context(void *context)
{
#if ENABLE_VAAPI
#ifdef VA_FOURCC_I010
#if VA_CHECK_VERSION(1, 0, 0)
  vaSetErrorCallback(context, libav_va_error_callback, NULL);
  vaSetInfoCallback(context, libav_va_info_callback, NULL);
#endif
#endif
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
  libav_vaapi_init();
  libav_set_loglevel();
  av_log_set_callback(libav_log_callback);
  avformat_network_init();
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