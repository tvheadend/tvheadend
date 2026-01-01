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

#include <vpx/vp8cx.h>


/* libvpx =================================================================== */

typedef struct {
    TVHVideoCodecProfile;
    int deadline;
    int cpu_used;
    int tune;
} tvh_codec_profile_libvpx_t;


static int
tvh_codec_profile_libvpx_open(tvh_codec_profile_libvpx_t *self,
                              AVDictionary **opts)
{
    AV_DICT_SET_TVH_REQUIRE_META(LST_LIBVPX, opts, 0);
    AV_DICT_SET_BIT_RATE(LST_LIBVPX, opts, self->bit_rate ? self->bit_rate : 2560);
    if (self->crf) {
        AV_DICT_SET_CRF(LST_LIBVPX, opts, self->crf, 10);
    }
    AV_DICT_SET_INT(LST_LIBVPX, opts, "deadline", self->deadline, 0);
    AV_DICT_SET_INT(LST_LIBVPX, opts, "cpu-used", self->cpu_used, 0);
    AV_DICT_SET_INT(LST_LIBVPX, opts, "tune", self->tune, 0);
    AV_DICT_SET_INT(LST_LIBVPX, opts, "threads", 0, 0);
    return 0;
}


static htsmsg_t *
codec_profile_libvpx_class_deadline_list(void *obj, const char *lang)
{
    static const struct strtab tab[] = {
        {N_("best"),     VPX_DL_BEST_QUALITY},
        {N_("good"),     VPX_DL_GOOD_QUALITY},
        {N_("realtime"), VPX_DL_REALTIME}
    };
    return strtab2htsmsg(tab, 1, lang);
}


static htsmsg_t *
codec_profile_libvpx_class_tune_list(void *obj, const char *lang)
{
    static const struct strtab tab[] = {
        {N_("psnr"), VP8_TUNE_PSNR},
        {N_("ssim"), VP8_TUNE_SSIM}
    };
    return strtab2htsmsg(tab, 1, lang);
}


static const codec_profile_class_t codec_profile_libvpx_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_video_class,
        .ic_class      = "codec_profile_libvpx",
        .ic_caption    = N_("libvpx"),
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
                .type     = PT_INT,
                .id       = "crf",
                .name     = N_("Constant Rate Factor (0=auto)"),
                .desc     = N_("Select the quality for constant quality mode [0-63]."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(TVHVideoCodecProfile, crf),
                .intextra = INTEXTRA_RANGE(0, 63, 1),
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "deadline",
                .name     = N_("Quality"),
                .desc     = N_("Time to spend encoding, in microseconds."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_libvpx_t, deadline),
                .list     = codec_profile_libvpx_class_deadline_list,
                .def.i    = VPX_DL_REALTIME,
            },
            {
                .type     = PT_INT,
                .id       = "cpu-used",
                .name     = N_("Speed"),
                .desc     = N_("Quality/Speed ratio modifier."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_libvpx_t, cpu_used),
                .intextra = INTEXTRA_RANGE(-16, 16, 1),
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
                .off      = offsetof(tvh_codec_profile_libvpx_t, tune),
                .list     = codec_profile_libvpx_class_tune_list,
                .def.i    = VP8_TUNE_PSNR,
            },
            {
                .type     = PT_INT,
                .id       = "gop_size",     // Don't change
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
    .open = (codec_profile_open_meth)tvh_codec_profile_libvpx_open,
};


/* libvpx_vp8 =============================================================== */

TVHVideoCodec tvh_codec_libvpx_vp8 = {
    .name     = "libvpx",
    .size     = sizeof(tvh_codec_profile_libvpx_t),
    .idclass  = &codec_profile_libvpx_class,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_video_destroy,
};


/* libvpx_vp9 =============================================================== */

TVHVideoCodec tvh_codec_libvpx_vp9 = {
    .name    = "libvpx-vp9",
    .size    = sizeof(tvh_codec_profile_libvpx_t),
    .idclass = &codec_profile_libvpx_class,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_video_destroy,
};
