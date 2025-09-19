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


/* v4l2m2m ====================================================================== */

typedef struct {
    TVHVideoCodecProfile;
    char *libname;
    char *libprefix;
    int zerocopy;
} tvh_codec_profile_v4l2m2m_t;


static int
tvh_codec_profile_v4l2m2m_open(tvh_codec_profile_v4l2m2m_t *self, AVDictionary **opts)
{
    AV_DICT_SET_FLAGS_GLOBAL_HEADER(opts);
    // bit_rate
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(opts, self->bit_rate);
    }

    // max_b_frames
    // Encoder does not support b-frames yet, setting to 0
    // remove when b-frames handling in FFmpeg h264_v4l2m2m is fixed
    AV_DICT_SET_INT(opts, "bf", 0, 0);


//ffmpeg -hide_banner -h encoder=h264_v4l2m2m
// Encoder h264_v4l2m2m [V4L2 mem2mem H.264 encoder wrapper]:
//    General capabilities: delay hardware
//    Threading capabilities: none
// h264_v4l2m2m_encoder AVOptions:
//  -num_output_buffers <int>        E..V....... Number of buffers in the output context (from 0 to INT_MAX) (default 0)
//  -num_capture_buffers <int>        E..V....... Number of buffers in the capture context (from 8 to INT_MAX) (default 8)

    return 0;
}


static const codec_profile_class_t codec_profile_v4l2m2m_class = {
    {
        .ic_super   = (idclass_t *)&codec_profile_video_class,
        .ic_class   = "codec_profile_v4l2m2m",
        .ic_caption = N_("v4l2m2m_h264"),
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
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_v4l2m2m_open,
};


/* h264_v4l2m2m ================================================================= */

static void
tvh_codec_profile_v4l2m2m_destroy(TVHCodecProfile *_self)
{
    tvh_codec_profile_v4l2m2m_t *self = (tvh_codec_profile_v4l2m2m_t *)_self;
    tvh_codec_profile_video_destroy(_self);
    free(self->libname);
    free(self->libprefix);
}

TVHVideoCodec tvh_codec_v4l2m2m_h264 = {
    .name    = "h264_v4l2m2m",
    .size    = sizeof(tvh_codec_profile_v4l2m2m_t),
    .idclass = &codec_profile_v4l2m2m_class,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_v4l2m2m_destroy,
};