/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2016 Tvheadend
 *
 * tvheadend - Codec Profiles
 */

#include "transcoding/codec/internals.h"


/* mp2 ====================================================================== */

static int
tvh_codec_profile_mp2_open(TVHCodecProfile *self, AVDictionary **opts)
{
    AV_DICT_SET_TVH_REQUIRE_META(LST_MP2, opts, 0);
    // bit_rate
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(LST_MP2, opts, self->bit_rate);
    }
    return 0;
}


static htsmsg_t *
codec_profile_mp2_class_bit_rate_list(void *obj, const char *lang)
{
    // This is a place holder.
    // The real list is set in src/webui/static/app/codec.js
    return htsmsg_create_list();
}


static const codec_profile_class_t codec_profile_mp2_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_audio_class,
        .ic_class      = "codec_profile_mp2",
        .ic_caption    = N_("mp2"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_DBL,
                .id       = "bit_rate",
                .name     = N_("Bitrate (kb/s)"),
                .desc     = N_("Constant bitrate (CBR) mode."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(TVHCodecProfile, bit_rate),
                .list     = codec_profile_mp2_class_bit_rate_list,
                .def.d    = 0,
            },
            {}
        }
    },
    .open = tvh_codec_profile_mp2_open,
};


TVHAudioCodec tvh_codec_mp2 = {
    .name    = "mp2",
    .size    = sizeof(TVHAudioCodecProfile),
    .idclass = &codec_profile_mp2_class,
    .profile_init = tvh_codec_profile_audio_init,
    .profile_destroy = tvh_codec_profile_audio_destroy,
};
