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


/* aac ====================================================================== */
// see aacenc_profiles[] ffmpeg-7.0/libavcodec/aacenctab.h + AV_PROFILE_UNKNOWN
static const AVProfile aac_profiles[] = {
    { FF_AV_PROFILE_AAC_MAIN,      "Main" },
    { FF_AV_PROFILE_AAC_LOW,       "LC" },
    { FF_AV_PROFILE_AAC_LTP,       "LTP" },
    { FF_AV_PROFILE_MPEG2_AAC_LOW, "MPEG2_LC" },
    { FF_AV_PROFILE_UNKNOWN },
};

#if LIBAVCODEC_VERSION_MAJOR > 59
// see aac_normal_chan_layouts[7] in ffmpeg-7.0/libavcodec/aacenctab.h + NULL termination
static const AVChannelLayout aac_channel_layouts[] = {
    AV_CHANNEL_LAYOUT_MONO,
    AV_CHANNEL_LAYOUT_STEREO,
    AV_CHANNEL_LAYOUT_SURROUND,
    AV_CHANNEL_LAYOUT_4POINT0,
    AV_CHANNEL_LAYOUT_5POINT0_BACK,
    AV_CHANNEL_LAYOUT_5POINT1_BACK,
    AV_CHANNEL_LAYOUT_7POINT1,
    { 0 },
};
#else
// see aac_chan_configs in ffmpeg-3.0.2/libavcodec/aacenctab.h
static const uint64_t aac_channel_layouts[] = {
    AV_CH_LAYOUT_MONO,
    AV_CH_LAYOUT_STEREO,
    AV_CH_LAYOUT_SURROUND,
    AV_CH_LAYOUT_4POINT0,
    AV_CH_LAYOUT_5POINT0_BACK,
    AV_CH_LAYOUT_5POINT1_BACK,
    AV_CH_LAYOUT_7POINT1_WIDE_BACK,
    0
};
#endif


typedef struct {
    TVHAudioCodecProfile;
    char *coder;
} tvh_codec_profile_aac_t;


static int
tvh_codec_profile_aac_open(tvh_codec_profile_aac_t *self, AVDictionary **opts)
{
    // bit_rate or global_quality
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(opts, self->bit_rate);
    }
    else {
        AV_DICT_SET_GLOBAL_QUALITY(opts, self->qscale, 1);
    }
    AV_DICT_SET(opts, "aac_coder", self->coder, 0);
    return 0;
}


static htsmsg_t *
codec_profile_aac_class_coder_list(void *obj, const char *lang)
{
    static const struct strtab_str tab[] = {
        {N_("anmr: ANMR method (Not currently recommended)"), "anmr"},
        {N_("twoloop: Two loop searching method"),            "twoloop"},
        {N_("fast: Constant quantizer (Not recommended)"),    "fast"}
    };
    return strtab2htsmsg_str(tab, 1, lang);
}


static const codec_profile_class_t codec_profile_aac_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_audio_class,
        .ic_class      = "codec_profile_aac",
        .ic_caption    = N_("aac"),
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
                .desc     = N_("Variable bitrate (VBR) mode [0-2]."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(TVHCodecProfile, qscale),
                .intextra = INTEXTRA_RANGE(0, 2, 1),
                .def.d    = 0,
            },
            {
                .type     = PT_STR,
                .id       = "coder",
                .name     = N_("Coding algorithm"),
                .desc     = N_("Coding algorithm."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_aac_t, coder),
                .list     = codec_profile_aac_class_coder_list,
                .def.s    = "twoloop",
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_aac_open,
};


static void
tvh_codec_profile_aac_destroy(TVHCodecProfile *_self)
{
    tvh_codec_profile_aac_t *self = (tvh_codec_profile_aac_t *)_self;
    tvh_codec_profile_audio_destroy(_self);
    free(self->coder);
}


TVHAudioCodec tvh_codec_aac = {
    .name            = "aac",
    .size            = sizeof(tvh_codec_profile_aac_t),
    .idclass         = &codec_profile_aac_class,
    .profiles        = aac_profiles,
    .profile_init    = tvh_codec_profile_audio_init,
    .profile_destroy = tvh_codec_profile_aac_destroy,
    .channel_layouts = aac_channel_layouts,
};
