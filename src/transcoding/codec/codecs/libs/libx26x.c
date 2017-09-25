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


/* utils ==================================================================== */

static htsmsg_t *
strlist2htsmsg(const char * const *values)
{
    htsmsg_t *list = NULL, *map = NULL;
    int i;
    const char *value = NULL;

    if ((list = htsmsg_create_list())) {
        for (i = 0; (value = values[i]); i++) {
            if (!(map = htsmsg_create_map())) {
                htsmsg_destroy(list);
                list = NULL;
                break;
            }
            ADD_STR_VAL(list, map, value);
        }
    }
    return list;
}


/* libx26x ================================================================== */

typedef struct {
    TVHVideoCodecProfile;
    char *preset;
    char *tune;
    char *params;
} tvh_codec_profile_libx26x_t;


static int
tvh_codec_profile_libx26x_open(tvh_codec_profile_libx26x_t *self,
                               AVDictionary **opts)
{
    AV_DICT_SET(opts, "preset", self->preset, 0);
    AV_DICT_SET(opts, "tune", self->tune, 0);
    return 0;
}


static const codec_profile_class_t codec_profile_libx26x_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_video_class,
        .ic_class      = "codec_profile_libx26x",
        .ic_caption    = N_("libx26x"),
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
                .type     = PT_INT,
                .id       = "crf",
                .name     = N_("Constant Rate Factor (0=auto)"),
                .desc     = N_("Select the quality for constant quality mode [0-51]."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(TVHVideoCodecProfile, crf),
                .intextra = INTEXTRA_RANGE(0, 51, 1),
                .def.i    = 0,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_libx26x_open,
};


/* libx264 ================================================================== */

#if ENABLE_LIBX264

#include <x264.h>

static const AVProfile libx264_profiles[] = {
    { FF_PROFILE_H264_BASELINE, "Baseline" },
    { FF_PROFILE_H264_MAIN,     "Main" },
    { FF_PROFILE_H264_HIGH,     "High" },
    { FF_PROFILE_H264_HIGH_10,  "High 10" },
    { FF_PROFILE_H264_HIGH_422, "High 4:2:2" },
    { FF_PROFILE_H264_HIGH_444, "High 4:4:4" },
    { FF_PROFILE_UNKNOWN },
};


static int
tvh_codec_profile_libx264_open(tvh_codec_profile_libx26x_t *self,
                               AVDictionary **opts)
{
    // bit_rate or crf
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(opts, self->bit_rate);
    }
    else {
        AV_DICT_SET_CRF(opts, self->crf, 15);
    }
    // params
    if (self->params && strlen(self->params)) {
        AV_DICT_SET(opts, "x264-params", self->params, 0);
    }
    return 0;
}


static htsmsg_t *
codec_profile_libx264_class_preset_list(void *obj, const char *lang)
{
    return strlist2htsmsg(x264_preset_names);
}


static htsmsg_t *
codec_profile_libx264_class_tune_list(void *obj, const char *lang)
{
    return strlist2htsmsg(x264_tune_names);
}


static const codec_profile_class_t codec_profile_libx264_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_libx26x_class,
        .ic_class      = "codec_profile_libx264",
        .ic_caption    = N_("libx264"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_STR,
                .id       = "preset",
                .name     = N_("Preset"),
                .desc     = N_("Set the encoding preset (cf. x264 --fullhelp)."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_libx26x_t, preset),
                .list     = codec_profile_libx264_class_preset_list,
                .def.s    = "faster",
            },
            {
                .type     = PT_STR,
                .id       = "tune",
                .name     = N_("Tune"),
                .desc     = N_("Tune the encoding params (cf. x264 --fullhelp)."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_libx26x_t, tune),
                .list     = codec_profile_libx264_class_tune_list,
                .def.s    = "zerolatency",
            },
            {
                .type     = PT_STR,
                .id       = "params",
                .name     = N_("Parameters"),
                .desc     = N_("Override the configuration using a ':' separated "
                               "list of key=value parameters."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_libx26x_t, params),
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_libx264_open,
};


TVHVideoCodec tvh_codec_libx264 = {
    .name     = "libx264",
    .size     = sizeof(tvh_codec_profile_libx26x_t),
    .idclass  = &codec_profile_libx264_class,
    .profiles = libx264_profiles,
};

#endif


/* libx265 ================================================================== */

#if ENABLE_LIBX265

#include <x265.h>


static int
tvh_codec_profile_libx265_open(tvh_codec_profile_libx26x_t *self,
                               AVDictionary **opts)
{
    // bit_rate or crf
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(opts, self->bit_rate);
    }
    else {
        AV_DICT_SET_CRF(opts, self->crf, 18);
    }
    // params
    if (self->params && strlen(self->params)) {
        AV_DICT_SET(opts, "x265-params", self->params, 0);
    }
    return 0;
}


static htsmsg_t *
codec_profile_libx265_class_preset_list(void *obj, const char *lang)
{
    return strlist2htsmsg(x265_preset_names);
}


static htsmsg_t *
codec_profile_libx265_class_tune_list(void *obj, const char *lang)
{
    return strlist2htsmsg(x265_tune_names);
}


static const codec_profile_class_t codec_profile_libx265_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_libx26x_class,
        .ic_class      = "codec_profile_libx265",
        .ic_caption    = N_("libx265"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_STR,
                .id       = "preset",
                .name     = N_("Preset"),
                .desc     = N_("set the x265 preset."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_libx26x_t, preset),
                .list     = codec_profile_libx265_class_preset_list,
                .def.s    = "superfast",
            },
            {
                .type     = PT_STR,
                .id       = "tune",
                .name     = N_("Tune"),
                .desc     = N_("set the x265 tune parameter."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_libx26x_t, tune),
                .list     = codec_profile_libx265_class_tune_list,
                .def.s    = "fastdecode",
            },
            {
                .type     = PT_STR,
                .id       = "params",
                .name     = N_("Parameters"),
                .desc     = N_("Override the configuration using a ':' separated "
                               "list of key=value parameters."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_libx26x_t, params),
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_libx265_open,
};


static void
tvh_codec_profile_libx265_destroy(TVHCodecProfile *_self)
{
    tvh_codec_profile_libx26x_t *self = (tvh_codec_profile_libx26x_t *)_self;
    tvh_codec_profile_video_destroy(_self);
    free(self->preset);
    free(self->tune);
    free(self->params);
}


TVHVideoCodec tvh_codec_libx265 = {
    .name    = "libx265",
    .size    = sizeof(tvh_codec_profile_libx26x_t),
    .idclass = &codec_profile_libx265_class,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_libx265_destroy,
};

#endif
