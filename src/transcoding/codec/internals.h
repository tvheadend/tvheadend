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


/* codec_profile_class ====================================================== */

uint32_t
codec_profile_class_get_opts(void *obj, uint32_t opts);

uint32_t
codec_profile_class_profile_get_opts(void *obj, uint32_t opts);


/* AVCodec ================================================================== */

const char *
codec_get_title(AVCodec *self);


/* TVHCodec ================================================================= */

enum AVMediaType
tvh_codec_get_type(TVHCodec *self);

const char *
tvh_codec_get_type_string(TVHCodec *self);

AVCodec *
tvh_codec_get_codec(TVHCodec *self);

int
tvh_codec_is_enabled(TVHCodec *self);

TVHCodec *
tvh_codec_find(const char *name);

void
tvh_codecs_register(void);

void
tvh_codecs_forget(void);


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
    const uint64_t *channel_layouts;
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
    int hwaccel;
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
    int64_t channel_layout;
} TVHAudioCodecProfile;


#endif // TVH_TRANSCODING_CODEC_INTERNALS_H__
