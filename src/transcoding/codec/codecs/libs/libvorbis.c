/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2016 Tvheadend
 *
 * tvheadend - Codec Profiles
 */

#include "transcoding/codec/internals.h"


/* libvorbis ================================================================ */

#if LIBAVCODEC_VERSION_MAJOR > 59
// see libvorbis_setup() in ffmpeg-6.0/libavcodec/libvorbisenc.c
static const AVChannelLayout libvorbis_channel_layouts[] = {
    AV_CHANNEL_LAYOUT_MONO,
    AV_CHANNEL_LAYOUT_STEREO,
    AV_CHANNEL_LAYOUT_SURROUND,
    AV_CHANNEL_LAYOUT_2_2,
    AV_CHANNEL_LAYOUT_QUAD,
    AV_CHANNEL_LAYOUT_5POINT0,
    AV_CHANNEL_LAYOUT_5POINT0_BACK,
    AV_CHANNEL_LAYOUT_5POINT1,
    AV_CHANNEL_LAYOUT_5POINT1_BACK,
    AV_CHANNEL_LAYOUT_6POINT1,
    AV_CHANNEL_LAYOUT_7POINT1
};
#else
// see libvorbis_setup() in ffmpeg-3.0.2/libavcodec/libvorbisenc.c
static const uint64_t libvorbis_channel_layouts[] = {
    AV_CH_LAYOUT_MONO,
    AV_CH_LAYOUT_STEREO,
    AV_CH_LAYOUT_SURROUND,
    AV_CH_LAYOUT_2_2,
    AV_CH_LAYOUT_QUAD,
    AV_CH_LAYOUT_5POINT0,
    AV_CH_LAYOUT_5POINT0_BACK,
    AV_CH_LAYOUT_5POINT1,
    AV_CH_LAYOUT_5POINT1_BACK,
    AV_CH_LAYOUT_6POINT1,
    AV_CH_LAYOUT_7POINT1,
    //AV_CH_LAYOUT_HEXADECAGONAL,
    //AV_CH_LAYOUT_STEREO_DOWNMIX,
    0
};
#endif


static int
tvh_codec_profile_libvorbis_open(TVHCodecProfile *self, AVDictionary **opts)
{
    // bit_rate or global_quality
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(LST_LIBVORBIS, opts, self->bit_rate);
    }
    else {
        AV_DICT_SET_GLOBAL_QUALITY(LST_LIBVORBIS, opts, self->qscale, 5);
    }
    return 0;
}


static const codec_profile_class_t codec_profile_libvorbis_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_audio_class,
        .ic_class      = "codec_profile_libvorbis",
        .ic_caption    = N_("libvorbis"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_DBL,
                .id       = "bit_rate",
                .name     = N_("Bitrate (kb/s) (0=auto)"),
                .desc     = N_("Average bitrate (ABR) mode."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(TVHCodecProfile, bit_rate),
                .def.d    = 0,
            },
            {
                .type     = PT_DBL,
                .id       = "qscale",
                .name     = N_("Quality (0=auto)"),
                .desc     = N_("Variable bitrate (VBR) mode [0-10]."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(TVHCodecProfile, qscale),
                .intextra = INTEXTRA_RANGE(0, 10, 1),
                .def.d    = 0,
            },
            {}
        }
    },
    .open = tvh_codec_profile_libvorbis_open,
};


TVHAudioCodec tvh_codec_libvorbis = {
    .name            = "libvorbis",
    .size            = sizeof(TVHAudioCodecProfile),
    .idclass         = &codec_profile_libvorbis_class,
    .channel_layouts = libvorbis_channel_layouts,
    .profile_init    = tvh_codec_profile_audio_init,
    .profile_destroy = tvh_codec_profile_audio_destroy,
};
