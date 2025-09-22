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


/* flac ====================================================================== */

#if LIBAVCODEC_VERSION_MAJOR > 59
// see flac_channel_layouts ffmpeg-6.0/libavcodec/flac.c
static const AVChannelLayout flac_channel_layouts[] = {
    AV_CHANNEL_LAYOUT_MONO,
    AV_CHANNEL_LAYOUT_STEREO,
    AV_CHANNEL_LAYOUT_SURROUND,
    AV_CHANNEL_LAYOUT_QUAD,
    AV_CHANNEL_LAYOUT_5POINT0,
    AV_CHANNEL_LAYOUT_5POINT1,
    AV_CHANNEL_LAYOUT_6POINT1,
    AV_CHANNEL_LAYOUT_7POINT1
};
#else
// see flac_channel_layouts ffmpeg-3.4/libavcodec/flac.c & flacenc.c
static const uint64_t flac_channel_layouts[] = {
    AV_CH_LAYOUT_MONO,
    AV_CH_LAYOUT_STEREO,
    AV_CH_LAYOUT_SURROUND,
    AV_CH_LAYOUT_QUAD,
    AV_CH_LAYOUT_5POINT0,
    AV_CH_LAYOUT_5POINT0_BACK,
    AV_CH_LAYOUT_5POINT1,
    AV_CH_LAYOUT_5POINT1_BACK,
    0
};
#endif


typedef struct {
    TVHAudioCodecProfile;
    int compression_level;
} tvh_codec_profile_flac_t;


static int
tvh_codec_profile_flac_open(tvh_codec_profile_flac_t *self, AVDictionary **opts)
{
    AV_DICT_SET_INT(LST_FLAC, opts, "compression_level", self->compression_level, 0);
    return 0;
}

static const codec_profile_class_t codec_profile_flac_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_audio_class,
        .ic_class      = "codec_profile_flac",
        .ic_caption    = N_("flac"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "complevel",
                .name     = N_("Compression level"),
                .desc     = N_("Compression level (0-12), -1 means ffmpeg default"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_flac_t, compression_level),
                .intextra = INTEXTRA_RANGE(-1, 12, 1),
                .def.i    = -1,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_flac_open,
};


static int
tvh_codec_profile_flac_init(TVHCodecProfile *_self, htsmsg_t *conf)
{
    tvh_codec_profile_flac_t *self = (tvh_codec_profile_flac_t *)_self;

    self->compression_level = -1;
    return tvh_codec_profile_audio_init(_self, conf);
}


TVHAudioCodec tvh_codec_flac = {
    .name            = "flac",
    .size            = sizeof(tvh_codec_profile_flac_t),
    .idclass         = &codec_profile_flac_class,
    .profiles        = NULL,
    .profile_init    = tvh_codec_profile_flac_init,
    .profile_destroy = tvh_codec_profile_audio_destroy,
    .channel_layouts = flac_channel_layouts,
};
