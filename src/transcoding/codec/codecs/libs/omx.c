/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2016 Tvheadend
 *
 * tvheadend - Codec Profiles
 */

#include "transcoding/codec/internals.h"


/* omx ====================================================================== */

typedef struct {
    TVHVideoCodecProfile;
    char *libname;
    char *libprefix;
    int zerocopy;
} tvh_codec_profile_omx_t;


static int
tvh_codec_profile_omx_open(tvh_codec_profile_omx_t *self, AVDictionary **opts)
{
    AV_DICT_SET_FLAGS_GLOBAL_HEADER(LST_OMX, opts);
    // bit_rate
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(LST_OMX, opts, self->bit_rate);
    }
    if (self->libname && strlen(self->libname)) {
        AV_DICT_SET(LST_OMX, opts, "omx_libname", self->libname, 0);
    }
    if (self->libprefix && strlen(self->libprefix)) {
        AV_DICT_SET(LST_OMX, opts, "omx_libprefix", self->libprefix, 0);
    }
    AV_DICT_SET_INT(LST_OMX, opts, "zerocopy", self->zerocopy, 0);
    return 0;
}


static const codec_profile_class_t codec_profile_omx_class = {
    {
        .ic_super   = (idclass_t *)&codec_profile_video_class,
        .ic_class   = "codec_profile_omx",
        .ic_caption = N_("omx_h264"),
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
                .type     = PT_STR,
                .id       = "omx_libname",
                .name     = N_("Library name"),
                .desc     = N_("OpenMAX library name."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_omx_t, libname),
            },
            {
                .type     = PT_STR,
                .id       = "omx_libprefix",
                .name     = N_("Library prefix"),
                .desc     = N_("OpenMAX library prefix."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_omx_t, libprefix),
            },
            {
                .type     = PT_BOOL,
                .id       = "zerocopy",
                .name     = N_("Zerocopy"),
                .desc     = N_("Try to avoid copying input frames if possible."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_omx_t, zerocopy),
                .def.i    = 0,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_omx_open,
};


/* h264_omx ================================================================= */

static void
tvh_codec_profile_omx_destroy(TVHCodecProfile *_self)
{
    tvh_codec_profile_omx_t *self = (tvh_codec_profile_omx_t *)_self;
    tvh_codec_profile_video_destroy(_self);
    free(self->libname);
    free(self->libprefix);
}

TVHVideoCodec tvh_codec_omx_h264 = {
    .name    = "h264_omx",
    .size    = sizeof(tvh_codec_profile_omx_t),
    .idclass = &codec_profile_omx_class,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_omx_destroy,
};
