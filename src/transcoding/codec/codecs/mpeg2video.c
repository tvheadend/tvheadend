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


/* mpeg2video =============================================================== */

static int
tvh_codec_profile_mpeg2video_open(TVHCodecProfile *self, AVDictionary **opts)
{
    // bit_rate or global_quality
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(opts, self->bit_rate);
    }
    else {
        AV_DICT_SET_GLOBAL_QUALITY(opts, self->qscale, 5);
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
