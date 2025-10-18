/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2016 Tvheadend
 *
 * tvheadend - Codec Profiles
 */

#include "transcoding/codec/internals.h"


/* mpeg2video =============================================================== */

static int
tvh_codec_profile_mpeg2video_open(TVHCodecProfile *self, AVDictionary **opts)
{
    // bit_rate or global_quality
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(LST_MPEG2VIDEO, opts, self->bit_rate);
    }
    else {
        AV_DICT_SET_GLOBAL_QUALITY(LST_MPEG2VIDEO, opts, self->qscale, 5);
    }
    return 0;
}


static const codec_profile_class_t codec_profile_mpeg2video_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_video_class,
        .ic_class      = "codec_profile_mpeg2video",
        .ic_caption    = N_("mpeg2video"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_DBL,
                .id       = "bit_rate",
                .name     = N_("Bitrate (kb/s) (0=auto)"),
                .desc     = N_("Constant bitrate (CBR) mode."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(TVHCodecProfile, bit_rate),
                .def.d    = 0,
            },
            {
                .type     = PT_DBL,
                .id       = "qscale",
                .name     = N_("Quality (0=auto)"),
                .desc     = N_("Variable bitrate (VBR) mode [0-31]."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(TVHCodecProfile, qscale),
                .intextra = INTEXTRA_RANGE(0, 31, 1),
                .def.d    = 0,
            },
            {}
        }
    },
    .open = tvh_codec_profile_mpeg2video_open,
};


TVHVideoCodec tvh_codec_mpeg2video = {
    .name    = "mpeg2video",
    .size    = sizeof(TVHVideoCodecProfile),
    .idclass = &codec_profile_mpeg2video_class,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_video_destroy,
};
