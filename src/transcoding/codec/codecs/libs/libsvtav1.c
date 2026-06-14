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


#include "transcoding/codec/internals.h"

/* libsvtav1 =================================================================== */

typedef struct {
    TVHVideoCodecProfile;
    int preset;
    int cpu_used;
    int tune;
    char *params;
} tvh_codec_profile_libsvtav1_t;


static int
tvh_codec_profile_libsvtav1_open(tvh_codec_profile_libsvtav1_t *self,
                                  AVDictionary **opts)
{
    AV_DICT_SET_TVH_REQUIRE_META(LST_LIBSVTAV1, opts, 0);
    if (self->crf) {
        AV_DICT_SET_CRF(LST_LIBSVTAV1, opts, self->crf, 10);
    }
    if (self->preset) {
        AV_DICT_SET_INT(LST_LIBSVTAV1, opts, "preset", self->preset, 0);
    }
    AV_DICT_SET_INT(LST_LIBSVTAV1, opts, "tune", self->tune, 0);
    if (self->params && strlen(self->params)) {
        AV_DICT_SET(LST_LIBSVTAV1, opts, "svtav1-params", self->params, 0);
    }
    return 0;
}

static const codec_profile_class_t codec_profile_libsvtav1_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_video_class,
        .ic_class      = "codec_profile_libsvtav1",
        .ic_caption    = N_("libsvtav1"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "crf",
                .name     = N_("Constant Rate Factor (0=auto)"),
                .desc     = N_("Select the quality for constant quality mode [0-63]."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(TVHVideoCodecProfile, crf),
                .intextra = INTEXTRA_RANGE(0, 63, 1),
                .def.i    = 35,
            },
            {
                .type     = PT_INT,
                .id       = "preset",
                .name     = N_("Preset"),
                .desc     = N_("Select the encoding preset [0-13]."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_libsvtav1_t, preset),
                .intextra = INTEXTRA_RANGE(0, 13, 1),
                .def.i    = 8,
            },
            {
                .type     = PT_INT,
                .id       = "tune",
                .name     = N_("Tune"),
                .desc     = N_("Tune the encoding to a specific scenario."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_libsvtav1_t, tune),
                .intextra = INTEXTRA_RANGE(0, 1, 1),
                .def.i    = 1,
            },
            {
                .type     = PT_INT,
                .id       = "gop_size",
                .name     = N_("GOP size"),
                .desc     = N_("Sets the Group of Pictures (GOP) size in frame (default 0 is 3 sec.)"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(TVHVideoCodecProfile, gop_size),
                .intextra = INTEXTRA_RANGE(0, 1000, 1),
                .def.i    = 0,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_libsvtav1_open,
};

/* libsvtav1 =============================================================== */

TVHVideoCodec tvh_codec_libsvtav1 = {
    .name    = "libsvtav1",
    .size    = sizeof(tvh_codec_profile_libsvtav1_t),
    .idclass = &codec_profile_libsvtav1_class,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_video_destroy,
};
