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


/* vorbis =================================================================== */

static int
tvh_codec_profile_vorbis_open(TVHCodecProfile *self, AVDictionary **opts)
{
    AV_DICT_SET_GLOBAL_QUALITY(LST_VORBIS, opts, self->qscale, 5);
    ((TVHCodecProfile *)self)->has_support_for_filter2 = 1;
    return 0;
}


#if LIBAVCODEC_VERSION_MAJOR > 59
// see vorbis_encode_init() in ffmpeg-6.1.1/libavcodec/vorbisenc.c
// av_log(avctx, AV_LOG_ERROR, "Current FFmpeg Vorbis encoder only supports 2 channels.\n");
static const AVChannelLayout vorbis_channel_layouts[] = {
    AV_CHANNEL_LAYOUT_STEREO,
    { 0 }
};
#else
// see vorbis_encode_init() in ffmpeg-3.0.2/libavcodec/vorbisenc.c
static const uint64_t vorbis_channel_layouts[] = {
    AV_CH_LAYOUT_STEREO,
    0
};
#endif


static const codec_profile_class_t codec_profile_vorbis_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_audio_class,
        .ic_class      = "codec_profile_vorbis",
        .ic_caption    = N_("vorbis"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_DBL,
                .id       = "qscale",
                .name     = N_("Quality (0=auto)"),
                .desc     = N_("Variable bitrate (VBR) mode [0-10]."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(TVHCodecProfile, qscale),
                .intextra = INTEXTRA_RANGE(0, 10, 1),
                .def.d    = 0,
            },
            {}
        }
    },
    .open = tvh_codec_profile_vorbis_open,
};


TVHAudioCodec tvh_codec_vorbis = {
    .name            = "vorbis",
    .size            = sizeof(TVHAudioCodecProfile),
    .idclass         = &codec_profile_vorbis_class,
    .profiles        = NULL,
    .profile_init    = tvh_codec_profile_audio_init,
    .profile_destroy = tvh_codec_profile_audio_destroy,
    .channel_layouts = vorbis_channel_layouts,
};
