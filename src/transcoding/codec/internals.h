/*
 *  tvheadend - Codec Profiles
 *
 *  Copyright (C) 2016 Tvheadend
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


#ifndef TVH_TRANSCODING_CODEC_INTERNALS_H__
#define TVH_TRANSCODING_CODEC_INTERNALS_H__


#include "transcoding/codec.h"
#include "transcoding/memutils.h"


#define AUTO_STR "auto"

#define ADD_ENTRY(l, m, kt, k, vt, v) \
    do { \
        htsmsg_add_##kt((m), "key", (k)); \
        htsmsg_add_##vt((m), "val", (v)); \
        htsmsg_add_msg((l), NULL, (m)); \
    } while (0)

#define ADD_VAL(l, m, t, v) ADD_ENTRY(l, m, t, v, t, v)

#define ADD_STR_VAL(l, m, v) ADD_VAL(l, m, str, v)
#define ADD_S32_VAL(l, m, v) ADD_VAL(l, m, s32, v)
#define ADD_U32_VAL(l, m, v) ADD_VAL(l, m, u32, v)
#define ADD_S64_VAL(l, m, v) ADD_VAL(l, m, s64, v)


#define _tvh_codec_get_opts(c, a, o, T) \
    ((c) ? tvh_codec_base_get_opts((c), (o), ((((T *)(c))->a) != NULL)) : (o))

#define tvh_codec_get_opts(c, a, o) \
    _tvh_codec_get_opts(c, a, o, TVHCodec)

#define tvh_codec_video_get_opts(c, a, o) \
    _tvh_codec_get_opts(c, a, o, TVHVideoCodec)

#define tvh_codec_audio_get_opts(c, a, o) \
    _tvh_codec_get_opts(c, a, o, TVHAudioCodec)


#define _tvh_codec_get_list(c, a, T, n) \
    (((c) && tvh_codec_is_enabled((c))) ? tvh_codec##n##get_list_##a(((T *)(c))) : NULL)

#define tvh_codec_get_list(c, a) \
    _tvh_codec_get_list(c, a, TVHCodec, _)

#define tvh_codec_video_get_list(c, a) \
    _tvh_codec_get_list(c, a, TVHVideoCodec, _video_)

#define tvh_codec_audio_get_list(c, a) \
    _tvh_codec_get_list(c, a, TVHAudioCodec, _audio_)


#define _tvh_codec_getattr(c, a, t, T) \
    (((c) && tvh_codec_is_enabled((c)) && tvh_codec_get_type((c)) == (t)) ? (((T *)(c))->a) : NULL)

#define tvh_codec_video_getattr(c, a) \
    _tvh_codec_getattr(c, a, AVMEDIA_TYPE_VIDEO, TVHVideoCodec)

#define tvh_codec_audio_getattr(c, a) \
    _tvh_codec_getattr(c, a, AVMEDIA_TYPE_AUDIO, TVHAudioCodec)


#define AV_DICT_SET(d, k, v, f) \
    do { \
        if (av_dict_set((d), (k), (v), (f)) < 0) { \
            return -1; \
        } \
    } while (0)

#define AV_DICT_SET_INT(d, k, v, f) \
    do { \
        if (av_dict_set_int((d), (k), (v), (f)) < 0) { \
            return -1; \
        } \
    } while (0)

#define AV_DICT_SET_TVH_REQUIRE_META(d, v) \
    AV_DICT_SET_INT((d), "tvh_require_meta", (v), AV_DICT_DONT_OVERWRITE)

#define AV_DICT_SET_FLAGS(d, v) \
    AV_DICT_SET((d), "flags", (v), AV_DICT_APPEND)

#define AV_DICT_SET_FLAGS_GLOBAL_HEADER(d) \
    AV_DICT_SET_FLAGS((d), "+global_header")

#define AV_DICT_SET_BIT_RATE(d, v) \
    AV_DICT_SET_INT((d), "b", (v) * 1000, AV_DICT_DONT_OVERWRITE)

#define AV_DICT_SET_GLOBAL_QUALITY(d, v, a) \
    do { \
        AV_DICT_SET_FLAGS((d), "+qscale"); \
        AV_DICT_SET_INT((d), "global_quality", ((v) ? (v) : (a)) * FF_QP2LAMBDA, \
                        AV_DICT_DONT_OVERWRITE); \
    } while (0)

#define AV_DICT_SET_CRF(d, v, a) \
    AV_DICT_SET_INT((d), "crf", (v) ? (v) : (a), AV_DICT_DONT_OVERWRITE)

#define AV_DICT_SET_PIX_FMT(d, v, a) \
    AV_DICT_SET_INT((d), "pix_fmt", ((v) != AV_PIX_FMT_NONE) ? (v) : (a), \
                    AV_DICT_DONT_OVERWRITE)

#define HWACCEL_AUTO        0
#if ENABLE_VAAPI
#define HWACCEL_PRIORITIZE_VAAPI 1
#endif
#if ENABLE_NVENC
#define HWACCEL_PRIORITIZE_NVDEC 2
#endif
#if ENABLE_MMAL
#define HWACCEL_PRIORITIZE_MMAL  3
#endif

/* codec_profile_class ====================================================== */

uint32_t
codec_profile_class_get_opts(void *obj, uint32_t opts);

uint32_t
codec_profile_class_profile_get_opts(void *obj, uint32_t opts);


/* AVCodec ================================================================== */

const char *
codec_get_title(const AVCodec *self);


/* TVHCodec ================================================================= */

enum AVMediaType
tvh_codec_get_type(TVHCodec *self);

const char *
tvh_codec_get_type_string(TVHCodec *self);

const AVCodec *
tvh_codec_get_codec(TVHCodec *self);

int
tvh_codec_is_enabled(TVHCodec *self);

TVHCodec *
tvh_codec_find(const char *name);

void
#if ENABLE_VAAPI
tvh_codecs_register(int vainfo_probe_enabled);
#else
tvh_codecs_register(void);
#endif

void
tvh_codecs_forget(void);

/* ffmpeg constants */
#if LIBAVCODEC_VERSION_MAJOR > 59
// **** AUDIO ****
// aac
#define FF_AV_PROFILE_AAC_MAIN                  AV_PROFILE_AAC_MAIN
#define FF_AV_PROFILE_AAC_LOW                   AV_PROFILE_AAC_LOW
#define FF_AV_PROFILE_AAC_LTP                   AV_PROFILE_AAC_LTP
#define FF_AV_PROFILE_MPEG2_AAC_LOW             AV_PROFILE_MPEG2_AAC_LOW
// **** VIDEO ****
// vp9
#define FF_AV_PROFILE_VP9_0                     AV_PROFILE_VP9_0
#define FF_AV_PROFILE_VP9_1                     AV_PROFILE_VP9_1
#define FF_AV_PROFILE_VP9_2                     AV_PROFILE_VP9_2
#define FF_AV_PROFILE_VP9_3                     AV_PROFILE_VP9_3
// h265
#define FF_AV_PROFILE_HEVC_MAIN                 AV_PROFILE_HEVC_MAIN
#define FF_AV_PROFILE_HEVC_MAIN_10              AV_PROFILE_HEVC_MAIN_10
#define FF_AV_PROFILE_HEVC_REXT                 AV_PROFILE_HEVC_REXT
// h264
#define FF_AV_PROFILE_H264_BASELINE             AV_PROFILE_H264_BASELINE
#define FF_AV_PROFILE_H264_CONSTRAINED_BASELINE AV_PROFILE_H264_CONSTRAINED_BASELINE
#define FF_AV_PROFILE_H264_MAIN                 AV_PROFILE_H264_MAIN
#define FF_AV_PROFILE_H264_HIGH                 AV_PROFILE_H264_HIGH
#define FF_AV_PROFILE_H264_HIGH_10              AV_PROFILE_H264_HIGH_10
#define FF_AV_PROFILE_H264_HIGH_422             AV_PROFILE_H264_HIGH_422
#define FF_AV_PROFILE_H264_HIGH_444             AV_PROFILE_H264_HIGH_444
// mpeg2
#define FF_AV_PROFILE_MPEG2_MAIN                AV_PROFILE_MPEG2_MAIN
#define FF_AV_PROFILE_MPEG2_SIMPLE              AV_PROFILE_MPEG2_SIMPLE
// common
#define FF_AV_PROFILE_UNKNOWN                   AV_PROFILE_UNKNOWN
#else
// **** AUDIO ****
// aac
#define FF_AV_PROFILE_AAC_MAIN                  FF_PROFILE_AAC_MAIN
#define FF_AV_PROFILE_AAC_LOW                   FF_PROFILE_AAC_LOW
#define FF_AV_PROFILE_AAC_LTP                   FF_PROFILE_AAC_LTP
#define FF_AV_PROFILE_MPEG2_AAC_LOW             FF_PROFILE_MPEG2_AAC_LOW
// **** VIDEO ****
// vp9
#define FF_AV_PROFILE_VP9_0                     FF_PROFILE_VP9_0
#define FF_AV_PROFILE_VP9_1                     FF_PROFILE_VP9_1
#define FF_AV_PROFILE_VP9_2                     FF_PROFILE_VP9_2
#define FF_AV_PROFILE_VP9_3                     FF_PROFILE_VP9_3
// h265
#define FF_AV_PROFILE_HEVC_MAIN                 FF_PROFILE_HEVC_MAIN
#define FF_AV_PROFILE_HEVC_MAIN_10              FF_PROFILE_HEVC_MAIN_10
#define FF_AV_PROFILE_HEVC_REXT                 FF_PROFILE_HEVC_REXT
// h264
#define FF_AV_PROFILE_H264_BASELINE             FF_PROFILE_H264_BASELINE
#define FF_AV_PROFILE_H264_CONSTRAINED_BASELINE FF_PROFILE_H264_CONSTRAINED_BASELINE
#define FF_AV_PROFILE_H264_MAIN                 FF_PROFILE_H264_MAIN
#define FF_AV_PROFILE_H264_HIGH                 FF_PROFILE_H264_HIGH
#define FF_AV_PROFILE_H264_HIGH_10              FF_PROFILE_H264_HIGH_10
#define FF_AV_PROFILE_H264_HIGH_422             FF_PROFILE_H264_HIGH_422
#define FF_AV_PROFILE_H264_HIGH_444             FF_PROFILE_H264_HIGH_444
// mpeg2
#define FF_AV_PROFILE_MPEG2_MAIN                FF_PROFILE_MPEG2_MAIN
#define FF_AV_PROFILE_MPEG2_SIMPLE              FF_PROFILE_MPEG2_SIMPLE
// common
#define FF_AV_PROFILE_UNKNOWN                   FF_PROFILE_UNKNOWN
#endif


/* codec_profile_class */

uint32_t
tvh_codec_base_get_opts(TVHCodec *self, uint32_t opts, int visible);

htsmsg_t *
tvh_codec_get_list_profiles(TVHCodec *self);

/* codec_profile_video_class */

typedef struct tvh_codec_video {
    TVHCodec;
    const enum AVPixelFormat *pix_fmts;
} TVHVideoCodec;


/* codec_profile_audio_class */

typedef struct tvh_codec_audio {
    TVHCodec;
    const enum AVSampleFormat *sample_fmts;
    const int *sample_rates;
#if LIBAVCODEC_VERSION_MAJOR > 59
    const AVChannelLayout *channel_layouts;
#else
    const uint64_t *channel_layouts;
#endif
} TVHAudioCodec;


/* TVHCodecProfile ========================================================== */

void
tvh_codec_profile_remove(TVHCodecProfile *self, int delete);

TVHCodec *
tvh_codec_profile_get_codec(TVHCodecProfile *self);

TVHCodecProfile *
tvh_codec_profile_find(const char *name);

void
tvh_codec_profiles_load(void);

void
tvh_codec_profiles_remove(void);


/* video */

extern const codec_profile_class_t codec_profile_video_class;

typedef struct tvh_codec_profile_video {
    TVHCodecProfile;
    int deinterlace;
    int height;
    int scaling_mode;   // 0 --> up&down; 1 --> up; 2 --> down
    int hwaccel;
    int hwaccel_details;
    int pix_fmt;
    int crf;
    AVRational size;
} TVHVideoCodecProfile;


/* audio */

extern const codec_profile_class_t codec_profile_audio_class;

typedef struct tvh_codec_profile_audio {
    TVHCodecProfile;
    int tracks;
    char *language1;
    char *language2;
    char *language3;
    int sample_fmt;
    int sample_rate;
    // this variable will be used also as ch_layout_u_mask when LIBAVCODEC_VERSION_MAJOR > 59
    int64_t channel_layout;
} TVHAudioCodecProfile;


#endif // TVH_TRANSCODING_CODEC_INTERNALS_H__
