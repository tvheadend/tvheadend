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
    AV_DICT_SET_TVH_REQUIRE_META(LST_MP2, opts, 0);
    // bit_rate
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(LST_MP2, opts, self->bit_rate);
    }
    return 0;
}


// see avpriv_mpa_bitrate_tab and avpriv_mpa_freq_tab
// in ffmpeg-3.0.2/libavcodec/mpegaudiodata.c
static const int mp2_hi_bit_rates[] = {
    32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0
};
static const int mp2_lo_bit_rates[] = {
    8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0
};


static htsmsg_t *
codec_profile_mp2_class_bit_rate_list(void *obj, const char *lang)
{
    TVHAudioCodecProfile *self = (TVHAudioCodecProfile *)obj;
    const int *rates = mp2_hi_bit_rates;
    htsmsg_t *list = htsmsg_create_list(), *map = htsmsg_create_map();
    char buf[16];
    int i;

    // the lower sample rates (MPEG-2 LSF) use the lower bitrate table
    if (self && self->sample_rate && self->sample_rate < 32000)
        rates = mp2_lo_bit_rates;
    ADD_ENTRY(list, map, s32, 0, str, AUTO_STR);
    for (i = 0; rates[i]; i++) {
        map = htsmsg_create_map();
        snprintf(buf, sizeof(buf), "%d", rates[i]);
        ADD_ENTRY(list, map, s32, rates[i], str, buf);
    }
    return list;
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
