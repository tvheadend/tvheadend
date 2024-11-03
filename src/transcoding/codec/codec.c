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


#include "internals.h"

#if ENABLE_VAAPI_OLD || ENABLE_VAAPI
#include "vainfo.h"
#endif

struct TVHCodecs tvh_codecs;


/* encoders ================================================================= */

extern TVHCodec tvh_codec_mpeg2video;
extern TVHCodec tvh_codec_mp2;
extern TVHCodec tvh_codec_aac;
extern TVHCodec tvh_codec_vorbis;
extern TVHCodec tvh_codec_flac;

#if ENABLE_LIBX264
extern TVHCodec tvh_codec_libx264;
#endif
#if ENABLE_LIBX265
extern TVHCodec tvh_codec_libx265;
#endif

#if ENABLE_LIBVPX
extern TVHCodec tvh_codec_libvpx_vp8;
extern TVHCodec tvh_codec_libvpx_vp9;
#endif

#if ENABLE_LIBTHEORA
extern TVHCodec tvh_codec_libtheora;
#endif
#if ENABLE_LIBVORBIS
extern TVHCodec tvh_codec_libvorbis;
#endif

#if ENABLE_LIBFDKAAC
extern TVHCodec tvh_codec_libfdk_aac;
#endif

#if ENABLE_LIBOPUS
extern TVHCodec tvh_codec_libopus;
#endif

#if ENABLE_VAAPI_OLD || ENABLE_VAAPI
extern TVHCodec tvh_codec_vaapi_h264;
extern TVHCodec tvh_codec_vaapi_hevc;
extern TVHCodec tvh_codec_vaapi_vp8;
extern TVHCodec tvh_codec_vaapi_vp9;
#endif

#if ENABLE_NVENC
extern TVHCodec tvh_codec_nvenc_h264;
extern TVHCodec tvh_codec_nvenc_hevc;
#endif

#if ENABLE_OMX
extern TVHCodec tvh_codec_omx_h264;
#endif


/* AVCodec ================================================================== */

static enum AVMediaType
codec_get_type(const AVCodec *self)
{
    return self->type;
}


static const char *
codec_get_type_string(const AVCodec *self)
{
    return av_get_media_type_string(self->type);
}


const char *
codec_get_title(const AVCodec *self)
{
    static __thread char codec_title[TVH_TITLE_LEN];

    memset(codec_title, 0, sizeof(codec_title));
    if (
        str_snprintf(codec_title, sizeof(codec_title),
            self->long_name ? "%s: %s%s" : "%s%s%s",
            self->name, self->long_name ? self->long_name : "",
            (self->capabilities & AV_CODEC_CAP_EXPERIMENTAL) ? " (Experimental)" : "")
       ) {
        return NULL;
    }
    return codec_title;
}


/* TVHCodec ================================================================= */

static void
tvh_codec_video_init(TVHVideoCodec *self, const AVCodec *codec)
{
    if (!self->pix_fmts) {
        self->pix_fmts = codec->pix_fmts;
    }
}

static void
tvh_codec_audio_init(TVHAudioCodec *self, const AVCodec *codec)
{
    static int default_sample_rates[] = {
        44100, 48000, 96000, 192000, 0
    };

    if (!self->sample_fmts) {
        self->sample_fmts = codec->sample_fmts;
    }
    if (!self->sample_rates) {
        self->sample_rates = codec->supported_samplerates;
        if (!self->sample_rates)
            self->sample_rates = default_sample_rates;
    }
    if (!self->channel_layouts) {
#if LIBAVCODEC_VERSION_MAJOR > 59
        self->channel_layouts = codec->ch_layouts;
#else
        self->channel_layouts = codec->channel_layouts;
#endif
    }
}


static void
tvh_codec_init(TVHCodec *self, const AVCodec *codec)
{
    if (!self->profiles) {
        self->profiles = codec->profiles;
    }
    switch (codec->type) {
        case AVMEDIA_TYPE_VIDEO:
            tvh_codec_video_init((TVHVideoCodec *)self, codec);
            break;
        case AVMEDIA_TYPE_AUDIO:
            tvh_codec_audio_init((TVHAudioCodec *)self, codec);
            break;
        default:
            break;
    }
}


static void
tvh_codec_register(TVHCodec *self)
{
    static const size_t min_size = sizeof(TVHCodecProfile);
    const AVCodec *codec = NULL;

    if (tvh_str_default(self->name, NULL) == NULL ||
        self->size < min_size || !self->idclass) {
        tvherror(LS_CODEC, "incomplete/wrong definition for '%s' codec",
                 self->name ? self->name : "<missing name>");
        return;
    }

    if ((codec = avcodec_find_encoder_by_name(self->name)) &&
        !(codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)) {
        tvh_codec_init(self, codec);
        self->codec = codec; // enabled
    }
    idclass_register((idclass_t *)self->idclass);
    SLIST_INSERT_HEAD(&tvh_codecs, self, link);
    tvhinfo(LS_CODEC, "'%s' encoder registered", self->name);
}


/* exposed */

const idclass_t *
tvh_codec_get_class(TVHCodec *self)
{
    return (idclass_t *)self->idclass;
}


const char *
tvh_codec_get_name(TVHCodec *self)
{
    return self->name;
}


const char *
tvh_codec_get_title(TVHCodec *self)
{
    return self->codec ? codec_get_title(self->codec) : self->name;
}


enum AVMediaType
tvh_codec_get_type(TVHCodec *self)
{
    return self->codec ? codec_get_type(self->codec) : AVMEDIA_TYPE_UNKNOWN;
}


const char *
tvh_codec_get_type_string(TVHCodec *self)
{
    return self->codec ? codec_get_type_string(self->codec) : "<unknown>";
}


const AVCodec *
tvh_codec_get_codec(TVHCodec *self)
{
    return self->codec;
}


int
tvh_codec_is_enabled(TVHCodec *self)
{
    return self->codec ? 1 : 0;
}


TVHCodec *
tvh_codec_find(const char *name)
{
    TVHCodec *codec = NULL;

    SLIST_FOREACH(codec, &tvh_codecs, link) {
        if (!strcmp(codec->name, name)) {
            return codec;
        }
    }
    return NULL;
}


void
#if ENABLE_VAAPI_OLD || ENABLE_VAAPI
tvh_codecs_register(int vainfo_probe_enabled)
#else
tvh_codecs_register()
#endif
{
    SLIST_INIT(&tvh_codecs);
    tvh_codec_register(&tvh_codec_mpeg2video);
    tvh_codec_register(&tvh_codec_mp2);
    tvh_codec_register(&tvh_codec_aac);
    tvh_codec_register(&tvh_codec_vorbis);
    tvh_codec_register(&tvh_codec_flac);

#if ENABLE_LIBX264
    tvh_codec_register(&tvh_codec_libx264);
#endif
#if ENABLE_LIBX265
    tvh_codec_register(&tvh_codec_libx265);
#endif

#if ENABLE_LIBVPX
    tvh_codec_register(&tvh_codec_libvpx_vp8);
    tvh_codec_register(&tvh_codec_libvpx_vp9);
#endif

#if ENABLE_LIBTHEORA
    tvh_codec_register(&tvh_codec_libtheora);
#endif
#if ENABLE_LIBVORBIS
    tvh_codec_register(&tvh_codec_libvorbis);
#endif

#if ENABLE_LIBFDKAAC
    tvh_codec_register(&tvh_codec_libfdk_aac);
#endif

#if ENABLE_LIBOPUS
    tvh_codec_register(&tvh_codec_libopus);
#endif

#if ENABLE_VAAPI_OLD || ENABLE_VAAPI
    if (vainfo_probe_enabled && !vainfo_init(VAINFO_SHOW_LOGS)) {
        if (vainfo_encoder_isavailable(VAINFO_H264) || 
            vainfo_encoder_isavailable(VAINFO_H264_LOW_POWER))
            tvh_codec_register(&tvh_codec_vaapi_h264);
        if (vainfo_encoder_isavailable(VAINFO_HEVC) || 
            vainfo_encoder_isavailable(VAINFO_HEVC_LOW_POWER))
            tvh_codec_register(&tvh_codec_vaapi_hevc);
        if (vainfo_encoder_isavailable(VAINFO_VP8) || 
            vainfo_encoder_isavailable(VAINFO_VP8_LOW_POWER))
            tvh_codec_register(&tvh_codec_vaapi_vp8);
        if (vainfo_encoder_isavailable(VAINFO_VP9) || 
            vainfo_encoder_isavailable(VAINFO_VP9_LOW_POWER))
            tvh_codec_register(&tvh_codec_vaapi_vp9);
    }
    else {
        tvh_codec_register(&tvh_codec_vaapi_h264);
        tvh_codec_register(&tvh_codec_vaapi_hevc);
        tvh_codec_register(&tvh_codec_vaapi_vp8);
        tvh_codec_register(&tvh_codec_vaapi_vp9);
    }
#endif

#if ENABLE_NVENC
    tvh_codec_register(&tvh_codec_nvenc_h264);
    tvh_codec_register(&tvh_codec_nvenc_hevc);
#endif

#if ENABLE_OMX
    tvh_codec_register(&tvh_codec_omx_h264);
#endif
}


void
tvh_codecs_forget()
{
    tvhinfo(LS_CODEC, "forgetting codecs");
    while (!SLIST_EMPTY(&tvh_codecs)) {
        SLIST_REMOVE_HEAD(&tvh_codecs, link);
    }
}
