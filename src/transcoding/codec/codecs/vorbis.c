/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2016 Tvheadend
 *
 * tvheadend - Codec Profiles
 */

#include "transcoding/codec/internals.h"


/* vorbis =================================================================== */

static int
tvh_codec_profile_vorbis_open(TVHCodecProfile *self, AVDictionary **opts)
{
    AV_DICT_SET_GLOBAL_QUALITY(opts, self->qscale, 5);
    return 0;
}


#if LIBAVCODEC_VERSION_MAJOR > 59
// see vorbis_encode_init() in ffmpeg-6.0/libavcodec/vorbis_data.c
static const AVChannelLayout vorbis_channel_layouts[] = {
    AV_CHANNEL_LAYOUT_STEREO
};
#else
// see vorbis_encode_init() in ffmpeg-3.0.2/libavcodec/vorbisenc.c
static const uint64_t vorbis_channel_layouts[] = {
    AV_CH_LAYOUT_STEREO,
    0
};
#endif


static const codec_profile_class_t codec_profile_vorbis_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_audio_class,
        .ic_class      = "codec_profile_vorbis",
        .ic_caption    = N_("vorbis"),
        .ic_properties = (const property_t[]){
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
    .open = tvh_codec_profile_vorbis_open,
};


TVHAudioCodec tvh_codec_vorbis = {
    .name            = "vorbis",
    .size            = sizeof(TVHAudioCodecProfile),
    .idclass         = &codec_profile_vorbis_class,
    .profile_init    = tvh_codec_profile_audio_init,
    .profile_destroy = tvh_codec_profile_audio_destroy,
    .channel_layouts = vorbis_channel_layouts,
};
