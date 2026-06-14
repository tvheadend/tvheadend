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

#define V4L2_H264_PROFILE_BASELINE			    0
#define V4L2_H264_PROFILE_MAIN			        1
#define V4L2_H264_PROFILE_HIGH			        2

/* h264 v4l2m2m ====================================================================== */

typedef struct {
    TVHVideoCodecProfile;
    int num_output_buffers;
    int num_capture_buffers;
} tvh_codec_profile_v4l2m2m_t;


static const AVProfile v4l2m2m_h264_profiles[] = {
    { V4L2_H264_PROFILE_BASELINE,  "Baseline" },
    { V4L2_H264_PROFILE_MAIN,      "Main" },
    { V4L2_H264_PROFILE_HIGH,      "High" },
    { FF_AV_PROFILE_UNKNOWN },
};

static int
tvh_codec_profile_v4l2m2m_open(tvh_codec_profile_v4l2m2m_t *self, AVDictionary **opts)
{
    // pix_fmt
    AV_DICT_SET_PIX_FMT(LST_V4l2M2M, opts, ((TVHVideoCodecProfile *)self)->pix_fmt, AV_PIX_FMT_NV12);

    AV_DICT_SET_FLAGS_GLOBAL_HEADER(LST_V4l2M2M, opts);
    // bit_rate
    if (((TVHCodecProfile *)self)->bit_rate) {
        AV_DICT_SET_BIT_RATE(LST_V4l2M2M, opts, ((TVHCodecProfile *)self)->bit_rate);
    }

    // max_b_frames
    // Encoder does not support b-frames yet, setting to 0
    AV_DICT_SET_INT(LST_V4l2M2M, opts, "bf", 0, 0);


    if (self->num_output_buffers > -1){
        AV_DICT_SET_INT(LST_V4l2M2M, opts, "num_output_buffers", self->num_output_buffers, 0);
    }

    if (self->num_capture_buffers > -1){
        AV_DICT_SET_INT(LST_V4l2M2M, opts, "num_capture_buffers", self->num_capture_buffers, 0);
    }
    // force keyframe at t=0 and t=1s (used for flashing the encoder)
    //AV_DICT_SET(LST_VAAPI, opts, "force_key_frames", "expr:eq(t,0)+eq(t,1)+eq(t,2)", AV_DICT_DONT_OVERWRITE);

//ffmpeg -hide_banner -h encoder=h264_v4l2m2m
// Encoder h264_v4l2m2m [V4L2 mem2mem H.264 encoder wrapper]:
//    General capabilities: delay hardware
//    Threading capabilities: none
// h264_v4l2m2m_encoder AVOptions:
//  -num_output_buffers <int>        E..V....... Number of buffers in the output context (from 0 to INT_MAX) (default 0)
//  -num_capture_buffers <int>        E..V....... Number of buffers in the capture context (from 8 to INT_MAX) (default 8)

    ((TVHCodecProfile *)self)->has_support_for_filter2 = 1;
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
            {
                .type     = PT_INT,
                .id       = "num_output_buffers",
                .name     = N_("Number of output buffers"),
                .desc     = N_("Number of buffers in the output context"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_v4l2m2m_t, num_output_buffers),
                .intextra = INTEXTRA_RANGE(-1, 64, 1),
                .def.i    = 4,
            },
            {
                .type     = PT_INT,
                .id       = "num_capture_buffers",
                .name     = N_("Number of capture buffers"),
                .desc     = N_("Number of buffers in the capture context"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_v4l2m2m_t, num_capture_buffers),
                .intextra = INTEXTRA_RANGE(-1, 64, 1),
                .def.i    = 8,
            },
            {
                .type     = PT_INT,
                .id       = "encoder_hwaccel_type",
                .name     = N_("Encoder hardware acceleration type"),
                .desc     = N_("Encoder hardware acceleration type"),
                .group    = 2,
                .opts     = PO_PHIDDEN,
                .off      = offsetof(TVHVideoCodecProfile, encoder_hwaccel_type),
                .def.i    = AV_HWDEVICE_TYPE_DRM,
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
    tvh_codec_profile_video_destroy(_self);
}

TVHVideoCodec tvh_codec_v4l2m2m_h264 = {
    .name    = "h264_v4l2m2m",
    .size    = sizeof(tvh_codec_profile_v4l2m2m_t),
    .idclass = &codec_profile_v4l2m2m_class,
    .profiles = v4l2m2m_h264_profiles,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_v4l2m2m_destroy,
};