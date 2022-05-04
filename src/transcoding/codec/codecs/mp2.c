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


/* mp2 ====================================================================== */

static int
tvh_codec_profile_mp2_open(TVHCodecProfile *self, AVDictionary **opts)
{
    AV_DICT_SET_TVH_REQUIRE_META(opts, 0);
    // bit_rate
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(opts, self->bit_rate);
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
