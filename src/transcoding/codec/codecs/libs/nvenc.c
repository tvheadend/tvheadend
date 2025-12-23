/*
 *  tvheadend - Codec Profiles
 *
 *  Copyright (C) 2025 Tvheadend
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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <ffnvcodec/nvEncodeAPI.h>

#define PRESET_SKIP                  -1
// matched with ffmpeg/libavcodec/nvenc.h line 120
#define PRESET_DEFAULT               0
#define PRESET_SLOW                  1
#define PRESET_MEDIUM                2
#define PRESET_FAST                  3
#if NVENCAPI_MAJOR_VERSION < 10
#define PRESET_HP                    4
#define PRESET_HQ                    5
#define PRESET_BD                    6
#define PRESET_LOW_LATENCY_DEFAULT   7
#define PRESET_LOW_LATENCY_HQ        8
#define PRESET_LOW_LATENCY_HP        9
#define PRESET_LOSSLESS_DEFAULT      10
#define PRESET_LOSSLESS_HP           11
#else
#define PRESET_P1                    12
#define PRESET_P2                    13
#define PRESET_P3                    14
#define PRESET_P4                    15
#define PRESET_P5                    16
#define PRESET_P6                    17
#define PRESET_P7                    18
#endif

#define NV_ENC_PARAMS_RC_AUTO                   -1      /**< Automatic rate control mode */
// matched with build.linux/ffmpeg/nv-codec-headers-12.1.14.0/include/ffnvcodec/nvEncodeAPI.h
//#define NV_ENC_PARAMS_RC_CONSTQP                0x0      /**< Constant QP mode */
//#define NV_ENC_PARAMS_RC_VBR                    0x1      /**< Variable bitrate mode */
//#define NV_ENC_PARAMS_RC_CBR                    0x2      /**< Constant bitrate mode */
// from this point all deprecated rate control modes
#define RC_MODE_DEPRECATED                      0x800000 /**< Rate control mode is deprecated */
#define NV_ENC_PARAMS_RC_VBR_MINQP              0x800004 /**< Variable bitrate mode with MinQP */
#define NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ        0x800008 /**< Constant bitrate low delay high quality mode */
#define NV_ENC_PARAMS_RC_CBR_HQ                 0x800010 /**< Multi-pass constant bitrate mode */
#define NV_ENC_PARAMS_RC_VBR_HQ                 0x800020 /**< Multi-pass variable bitrate mode */

// matched with ffmpeg/libavcodec/nvenc.h
#define NV_ENC_H264_PROFILE_BASELINE			    0
#define NV_ENC_H264_PROFILE_MAIN			        1
#define NV_ENC_H264_PROFILE_HIGH			        2
#define NV_ENC_H264_PROFILE_HIGH_444P           	3

// matched with ffmpeg/libavcodec/nvenc.h
#define NV_ENC_HEVC_PROFILE_MAIN			        0
#define NV_ENC_HEVC_PROFILE_MAIN_10 			    1
#define NV_ENC_HEVC_PROFILE_REXT			        2

/* nvenc ==================================================================== */

typedef struct {
    TVHVideoCodecProfile;
/**
 * NVENC Constant QP [qp]
 * @note
 * int:
 * VALUE - Constant QP value (1-51, -1=skip)
 */
    int qp;
/**
 * NVENC Selects which NVENC capable GPU to use. First GPU is 0, second is 1, and so on. [gpu]
 * @note
 * int:
 * VALUE - GPU to use. First GPU is 0, second is 1, and so on. (from -2 to 15) (default any) (from 0 to 15, -1=any, -2=skip)
 */
    int devicenum;
/**
 * NVENC Set the encoding preset [preset]
 * @note
 * int:
 * VALUE - encoding preset (from 0 to 18) (default p4) (from ffmpeg/libavcodec/nvenc.h line 120, -1=skip)
 */
    int preset;
/**
 * NVENC Set the encoding tuning info [tune]
 * @note
 * int:
 * VALUE - encoding tuning info (from 1 to 4) (default hq) (from nvEncodeAPI.h, 0=skip)
 */
    int tune;
/**
 * NVENC Maximum bitrate [maxrate]
 * @note
 * int:
 * VALUE - max bitrate in bps
 */
    double max_bit_rate;
/**
 * NVENC Maximum bitrate scale factor
 * @note
 * double:
 * VALUE - max bitrate scale factor
 */
    double bit_rate_scale_factor;
/**
 * NVENC Override the preset rate-control method [rc]
 * @note
 * int:
 * VALUE - rc mode accepted values from nvEncodeAPI.h (0,1,2)
 * Note: some rc modes are deprecated
 */
    int rc_mode;
/**
 * NVENC Set the encoding level restriction [level]
 * @note
 * int:
 * VALUE - level restriction (from 0 to 62)
 */
    int level;
    int quality;
    int qmin;
    int qmax;
    int qdiff;
} tvh_codec_profile_nvenc_t;

static int
tvh_codec_profile_nvenc_open(tvh_codec_profile_nvenc_t *self,
                             AVDictionary **opts)
{
    if (self->devicenum != -2) {
        AV_DICT_SET_INT(LST_NVENC, opts, "gpu", self->devicenum, 0);
    }
    if (self->preset != PRESET_SKIP) {
        AV_DICT_SET_INT(LST_NVENC, opts, "preset", self->preset, 0);
    }
#if NVENCAPI_MAJOR_VERSION >= 10
    if (self->tune != NV_ENC_TUNING_INFO_UNDEFINED) {
        AV_DICT_SET_INT(LST_NVENC, opts, "tune", self->tune, 0);
    }
#endif
    if (self->rc_mode != NV_ENC_PARAMS_RC_AUTO) {
        AV_DICT_SET_INT(LST_NVENC, opts, "rc", self->rc_mode, 0);
    }
    if (self->qp) {
        AV_DICT_SET_INT(LST_NVENC, opts, "qp", self->qp, 0);
    }
    int int_bitrate = (int)((double)(self->bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    int int_max_bitrate = (int)((double)(self->max_bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    // force max_bitrate to be >= with bitrate (to avoid crash)
    if (int_bitrate > int_max_bitrate) {
        tvherror_transcode(LST_NVENC, "Bitrate %d kbps is greater than Max bitrate %d kbps, increase Max bitrate to %d kbps", int_bitrate / 1024, int_max_bitrate / 1024, int_bitrate / 1024);
        int_max_bitrate = int_bitrate;
    }
    if (self->bit_rate || self->max_bit_rate) {
        tvhinfo_transcode(LST_NVENC, "Bitrate = %d kbps; Max bitrate = %d kbps", int_bitrate / 1024, int_max_bitrate / 1024);
    }
    if (self->bit_rate) {
        AV_DICT_SET_INT(LST_NVENC, opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
    }
    if (self->max_bit_rate) {
        AV_DICT_SET_INT(LST_NVENC, opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
    }
    AV_DICT_SET_INT(LST_NVENC, opts, "quality", self->quality, 0);
    
    if (self->level != NV_ENC_LEVEL_AUTOSELECT) {
        AV_DICT_SET_INT(LST_NVENC, opts, "level", self->level, 0);
    }
    if (self->qmin) {
        AV_DICT_SET_INT(LST_NVENC, opts, "qmin", self->qmin, 0);
    }
    if (self->qmax) {
        AV_DICT_SET_INT(LST_NVENC, opts, "qmax", self->qmax, 0);
    }
    if (self->qdiff != -2) {
        AV_DICT_SET_INT(LST_NVENC, opts, "qdiff", self->qdiff, 0);
    }
    return 0;
}


static htsmsg_t *
codec_profile_nvenc_class_preset_list(void *obj, const char *lang)
{
    static const struct strtab tab[] = {
        {N_("skip"),                    PRESET_SKIP},
        {N_("Default"),                 PRESET_DEFAULT},
        {N_("Slow"),                    PRESET_SLOW},
        {N_("Medium"),                  PRESET_MEDIUM},
        {N_("Fast"),                    PRESET_FAST},
#if NVENCAPI_MAJOR_VERSION < 10
        {N_("HP"),                      PRESET_HP},
        {N_("HQ"),                      PRESET_HQ},
        {N_("BD"),                      PRESET_BD},
        {N_("Low latency"),             PRESET_LOW_LATENCY_DEFAULT},
        {N_("Low latency HQ"),          PRESET_LOW_LATENCY_HQ},
        {N_("Low latency HP"),          PRESET_LOW_LATENCY_HP},
        {N_("Lossless"),                PRESET_LOSSLESS_DEFAULT},
        {N_("Lossless HP"),             PRESET_LOSSLESS_HP},
#else
        {N_("P1 (lowest quality)"),     PRESET_P1},
        {N_("P2"),                      PRESET_P2},
        {N_("P3"),                      PRESET_P3},
        {N_("P4"),                      PRESET_P4},
        {N_("P5"),                      PRESET_P5},
        {N_("P6"),                      PRESET_P6},
        {N_("P7 (highest quality)"),    PRESET_P7},
#endif
    };
    return strtab2htsmsg(tab, 1, lang);
}

#if NVENCAPI_MAJOR_VERSION >= 10
static htsmsg_t *
codec_profile_nvenc_class_tune_list(void *obj, const char *lang)
{
    static const struct strtab tab[] = {
        {N_("skip"),		        NV_ENC_TUNING_INFO_UNDEFINED},
        {N_("High quality"),		NV_ENC_TUNING_INFO_HIGH_QUALITY},
        {N_("Low latency"),	        NV_ENC_TUNING_INFO_LOW_LATENCY},
        {N_("Ultra low latency"),	NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY},
        {N_("Lossless"),	        NV_ENC_TUNING_INFO_LOSSLESS},
    };
    return strtab2htsmsg(tab, 1, lang);
}
#endif

static htsmsg_t *
codec_profile_nvenc_class_rc_list(void *obj, const char *lang)
{
    static const struct strtab tab[] = {
        {N_("auto"),                            NV_ENC_PARAMS_RC_AUTO},
        {N_("CQP"),                             NV_ENC_PARAMS_RC_CONSTQP},
        {N_("VBR"),                             NV_ENC_PARAMS_RC_VBR},
        {N_("CBR"),                             NV_ENC_PARAMS_RC_CBR},
#if NVENCAPI_MAJOR_VERSION < 10
        {N_("VBR min Q (deprecated)"),          NV_ENC_PARAMS_RC_VBR_MINQP},
        {N_("CBR LD HQ (deprecated)"),          NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ},
        {N_("CBR High Quality (deprecated)"),   NV_ENC_PARAMS_RC_CBR_HQ},
        {N_("VBR High Quality (deprecated)"),   NV_ENC_PARAMS_RC_VBR_HQ},
#endif
    };
    return strtab2htsmsg(tab, 1, lang);
}

static const codec_profile_class_t codec_profile_nvenc_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_video_class,
        .ic_class      = "codec_profile_nvenc",
        .ic_caption    = N_("nvenc"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "devicenum",
                .name     = N_("GPU number"),
                .group    = 3,
                .desc     = N_("Select GPU number (from 0 to 15, -1=any, -2=skip)."),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_nvenc_t, devicenum),
                .intextra = INTEXTRA_RANGE(-2, 15, 1),
                .def.i    = -1,
            },
            {
                .type     = PT_INT,
                .id       = "preset",
                .name     = N_("Preset"),
                .group    = 3,
                .desc     = N_("Override the preset rate control."),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_nvenc_t, preset),
                .list     = codec_profile_nvenc_class_preset_list,
                .def.i    = PRESET_DEFAULT,
            },
#if NVENCAPI_MAJOR_VERSION >= 10
            {
                .type     = PT_INT,
                .id       = "tune",
                .name     = N_("Tune"),
                .group    = 3,
                .desc     = N_("Set the encoding tuning info."),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_nvenc_t, tune),
                .list     = codec_profile_nvenc_class_tune_list,
                .def.i    = PRESET_DEFAULT,
            },
#endif
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
            {
                .type     = PT_INT,
                .id       = "rc_mode",
                .name     = N_("Rate control mode"),
                .group    = 3,
                .desc     = N_("Override the preset rate control."),
                .opts     = PO_EXPERT,
                .off      = offsetof(tvh_codec_profile_nvenc_t, rc_mode),
                .list     = codec_profile_nvenc_class_rc_list,
                .def.i    = NV_ENC_PARAMS_RC_AUTO,
            },
            {
                .type     = PT_INT,
                .id       = "qp",     // Don't change
                .name     = N_("Constant QP"),
                .group    = 3,
                .desc     = N_("Fixed QP of P frames (from 0 to 51, 0=skip).[if disabled will not send paramter to libav]"),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_nvenc_t, qp),
                .intextra = INTEXTRA_RANGE(0, 51, 1),
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "qmin",     // Don't change
                .name     = N_("Minimum QP"),
                .group    = 5,
                .desc     = N_("Minimum QP of P frames (from 0 to 51, 0=skip)"),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_nvenc_t, qmin),
                .intextra = INTEXTRA_RANGE(0, 51, 1),
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "qmax",     // Don't change
                .name     = N_("Maximum QP"),
                .group    = 5,
                .desc     = N_("Maximum QP of P frames (from 0 to 51, 0=skip)"),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_nvenc_t, qmax),
                .intextra = INTEXTRA_RANGE(0, 51, 1),
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "qdiff",     // Don't change
                .name     = N_("QP difference"),
                .group    = 5,
                .desc     = N_("Maximum QP change between adjacent frames (from 0 to 32, -1=default, -2=skip)"),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_nvenc_t, qdiff),
                .intextra = INTEXTRA_RANGE(-2, 32, 1),
                .def.i    = -2,
            },
            {
                .type     = PT_DBL,
                .id       = "bit_rate",
                .name     = N_("Bitrate (kb/s) (0=auto)"),
                .desc     = N_("Target bitrate."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(TVHCodecProfile, bit_rate),
                .def.d    = 0,
            },
            {
                .type     = PT_DBL,
                .id       = "max_bit_rate",     // Don't change
                .name     = N_("Max bitrate (kb/s)"),
                .desc     = N_("Maximum bitrate (0=skip).[if disabled will not send paramter to libav]"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_nvenc_t, max_bit_rate),
                .def.d    = 0,
            },
            {
                .type     = PT_DBL,
                .id       = "bit_rate_scale_factor",     // Don't change
                .name     = N_("Bitrate scale factor"),
                .desc     = N_("Bitrate & Max bitrate scaler with resolution (0=no scale; 1=proportional_change). Relative to 480."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_nvenc_t, bit_rate_scale_factor),
                .def.d    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "quality",
                .name     = N_("Quality (0=auto)"),
                .desc     = N_("Set encode quality (trades off against speed, "
                               "higher is faster) [0-51]."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_nvenc_t, quality),
                .intextra = INTEXTRA_RANGE(0, 51, 1),
                .def.i    = 0,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_nvenc_open,
};


/* h264_nvenc =============================================================== */

static const AVProfile nvenc_h264_profiles[] = {
    { NV_ENC_H264_PROFILE_BASELINE,             "Baseline" },
    { NV_ENC_H264_PROFILE_MAIN,                 "Main" },
    { NV_ENC_H264_PROFILE_HIGH,                 "High" },
    { NV_ENC_H264_PROFILE_HIGH_444P,            "High 444P" },
    { FF_AV_PROFILE_UNKNOWN },
};

static int
tvh_codec_profile_nvenc_h264_open(tvh_codec_profile_nvenc_t *self,
                                  AVDictionary **opts)
{
// this is a little outdated but is worth mentioning:
// https://blog.wmspanel.com/2016/10/nvidia-nvenc-gpu-nimble-streamer-transcoder.html
// ffmpeg -hide_banner -h encoder=h264_nvenc
//Encoder h264_nvenc [NVIDIA NVENC H.264 encoder]:
//    General capabilities: dr1 delay hardware
//    Threading capabilities: none
//    Supported hardware devices: cuda cuda
//    Supported pixel formats: yuv420p nv12 p010le yuv444p p016le yuv444p16le bgr0 rgb0 cuda
//h264_nvenc AVOptions:
//  -preset            <int>        E..V....... Set the encoding preset (from 0 to 18) (default p4)
//     default         0            E..V.......
//     slow            1            E..V....... hq 2 passes
//     medium          2            E..V....... hq 1 pass
//     fast            3            E..V....... hp 1 pass
//     hp              4            E..V.......
//     hq              5            E..V.......
//     bd              6            E..V.......
//     ll              7            E..V....... low latency
//     llhq            8            E..V....... low latency hq
//     llhp            9            E..V....... low latency hp
//     lossless        10           E..V.......
//     losslesshp      11           E..V.......
//     p1              12           E..V....... fastest (lowest quality)
//     p2              13           E..V....... faster (lower quality)
//     p3              14           E..V....... fast (low quality)
//     p4              15           E..V....... medium (default)
//     p5              16           E..V....... slow (good quality)
//     p6              17           E..V....... slower (better quality)
//     p7              18           E..V....... slowest (best quality)
//  -tune              <int>        E..V....... Set the encoding tuning info (from 1 to 4) (default hq)
//     hq              1            E..V....... High quality
//     ll              2            E..V....... Low latency
//     ull             3            E..V....... Ultra low latency
//     lossless        4            E..V....... Lossless
//  -profile           <int>        E..V....... Set the encoding profile (from 0 to 3) (default main)
//     baseline        0            E..V.......
//     main            1            E..V.......
//     high            2            E..V.......
//     high444p        3            E..V.......
//  -level             <int>        E..V....... Set the encoding level restriction (from 0 to 62) (default auto)
//     auto            0            E..V.......
//     1               10           E..V.......
//     1.0             10           E..V.......
//     1b              9            E..V.......
//     1.0b            9            E..V.......
//     1.1             11           E..V.......
//     1.2             12           E..V.......
//     1.3             13           E..V.......
//     2               20           E..V.......
//     2.0             20           E..V.......
//     2.1             21           E..V.......
//     2.2             22           E..V.......
//     3               30           E..V.......
//     3.0             30           E..V.......
//     3.1             31           E..V.......
//     3.2             32           E..V.......
//     4               40           E..V.......
//     4.0             40           E..V.......
//     4.1             41           E..V.......
//     4.2             42           E..V.......
//     5               50           E..V.......
//     5.0             50           E..V.......
//     5.1             51           E..V.......
//     5.2             52           E..V.......
//     6.0             60           E..V.......
//     6.1             61           E..V.......
//     6.2             62           E..V.......
//  -rc                <int>        E..V....... Override the preset rate-control (from -1 to INT_MAX) (default -1)
//     constqp         0            E..V....... Constant QP mode
//     vbr             1            E..V....... Variable bitrate mode
//     cbr             2            E..V....... Constant bitrate mode
//     vbr_minqp       8388612      E..V....... Variable bitrate mode with MinQP (deprecated)
//     ll_2pass_quality 8388616      E..V....... Multi-pass optimized for image quality (deprecated)
//     ll_2pass_size   8388624      E..V....... Multi-pass optimized for constant frame size (deprecated)
//     vbr_2pass       8388640      E..V....... Multi-pass variable bitrate mode (deprecated)
//     cbr_ld_hq       8388616      E..V....... Constant bitrate low delay high quality mode
//     cbr_hq          8388624      E..V....... Constant bitrate high quality mode
//     vbr_hq          8388640      E..V....... Variable bitrate high quality mode
//  -rc-lookahead      <int>        E..V....... Number of frames to look ahead for rate-control (from 0 to INT_MAX) (default 0)
//  -surfaces          <int>        E..V....... Number of concurrent surfaces (from 0 to 64) (default 0)
//  -cbr               <boolean>    E..V....... Use cbr encoding mode (default false)
//  -2pass             <boolean>    E..V....... Use 2pass encoding mode (default auto)
//  -gpu               <int>        E..V....... Selects which NVENC capable GPU to use. First GPU is 0, second is 1, and so on. (from -2 to INT_MAX) (default any)
//     any             -1           E..V....... Pick the first device available
//     list            -2           E..V....... List the available devices
//  -delay             <int>        E..V....... Delay frame output by the given amount of frames (from 0 to INT_MAX) (default INT_MAX)
//  -no-scenecut       <boolean>    E..V....... When lookahead is enabled, set this to 1 to disable adaptive I-frame insertion at scene cuts (default false)
//  -forced-idr        <boolean>    E..V....... If forcing keyframes, force them as IDR frames. (default false)
//  -b_adapt           <boolean>    E..V....... When lookahead is enabled, set this to 0 to disable adaptive B-frame decision (default true)
//  -spatial-aq        <boolean>    E..V....... set to 1 to enable Spatial AQ (default false)
//  -spatial_aq        <boolean>    E..V....... set to 1 to enable Spatial AQ (default false)
//  -temporal-aq       <boolean>    E..V....... set to 1 to enable Temporal AQ (default false)
//  -temporal_aq       <boolean>    E..V....... set to 1 to enable Temporal AQ (default false)
//  -zerolatency       <boolean>    E..V....... Set 1 to indicate zero latency operation (no reordering delay) (default false)
//  -nonref_p          <boolean>    E..V....... Set this to 1 to enable automatic insertion of non-reference P-frames (default false)
//  -strict_gop        <boolean>    E..V....... Set 1 to minimize GOP-to-GOP rate fluctuations (default false)
//  -aq-strength       <int>        E..V....... When Spatial AQ is enabled, this field is used to specify AQ strength. AQ strength scale is from 1 (low) - 15 (aggressive) (from 1 to 15) (default 8)
//  -cq                <float>      E..V....... Set target quality level (0 to 51, 0 means automatic) for constant quality mode in VBR rate control (from 0 to 51) (default 0)
//  -aud               <boolean>    E..V....... Use access unit delimiters (default false)
//  -bluray-compat     <boolean>    E..V....... Bluray compatibility workarounds (default false)
//  -init_qpP          <int>        E..V....... Initial QP value for P frame (from -1 to 51) (default -1)
//  -init_qpB          <int>        E..V....... Initial QP value for B frame (from -1 to 51) (default -1)
//  -init_qpI          <int>        E..V....... Initial QP value for I frame (from -1 to 51) (default -1)
//  -qp                <int>        E..V....... Constant quantization parameter rate control method (from -1 to 51) (default -1)
//  -weighted_pred     <int>        E..V....... Set 1 to enable weighted prediction (from 0 to 1) (default 0)
//  -coder             <int>        E..V....... Coder type (from -1 to 2) (default default)
//     default         -1           E..V.......
//     auto            0            E..V.......
//     cabac           1            E..V.......
//     cavlc           2            E..V.......
//     ac              1            E..V.......
//     vlc             2            E..V.......
//  -b_ref_mode        <int>        E..V....... Use B frames as references (from 0 to 2) (default disabled)
//     disabled        0            E..V....... B frames will not be used for reference
//     each            1            E..V....... Each B frame will be used for reference
//     middle          2            E..V....... Only (number of B frames)/2 will be used for reference
//  -a53cc             <boolean>    E..V....... Use A53 Closed Captions (if available) (default true)
//  -dpb_size          <int>        E..V....... Specifies the DPB size used for encoding (0 means automatic) (from 0 to INT_MAX) (default 0)
//  -multipass         <int>        E..V....... Set the multipass encoding (from 0 to 2) (default disabled)
//     disabled        0            E..V....... Single Pass
//     qres            1            E..V....... Two Pass encoding is enabled where first Pass is quarter resolution
//     fullres         2            E..V....... Two Pass encoding is enabled where first Pass is full resolution
//  -ldkfs             <int>        E..V....... Low delay key frame scale; Specifies the Scene Change frame size increase allowed in case of single frame VBV and CBR (from 0 to 255) (default 0)

    
    // ------ Set Defaults ---------
    AV_DICT_SET_INT(LST_NVENC, opts, "qblur", -1, 0);
    AV_DICT_SET_INT(LST_NVENC, opts, "qcomp", -1, 0);
    AV_DICT_SET_INT(LST_NVENC, opts, "bf", 0, 0);
    AV_DICT_SET_INT(LST_NVENC, opts, "refs", 0, 0);
    return 0;
}

static htsmsg_t *
codec_profile_nvenc_class_level_list_h264(void *obj, const char *lang)
{
    static const struct strtab tab[] = {
        {N_("auto"),	      NV_ENC_LEVEL_AUTOSELECT},
        {N_("1.0"),           NV_ENC_LEVEL_H264_1},
        {N_("1.0b"),          NV_ENC_LEVEL_H264_1b},
        {N_("1.1"),           NV_ENC_LEVEL_H264_11},
        {N_("1.2"),           NV_ENC_LEVEL_H264_12},
        {N_("1.3"),           NV_ENC_LEVEL_H264_13},
        {N_("2.0"),           NV_ENC_LEVEL_H264_2},
        {N_("2.1"),           NV_ENC_LEVEL_H264_21},
        {N_("2.2"),           NV_ENC_LEVEL_H264_22},
        {N_("3.0"),           NV_ENC_LEVEL_H264_3},
        {N_("3.1"),           NV_ENC_LEVEL_H264_31},
        {N_("3.2"),           NV_ENC_LEVEL_H264_32},
        {N_("4.0"),           NV_ENC_LEVEL_H264_4},
        {N_("4.1"),           NV_ENC_LEVEL_H264_41},
        {N_("4.2"),           NV_ENC_LEVEL_H264_42},
        {N_("5.0"),           NV_ENC_LEVEL_H264_5},
        {N_("5.1"),           NV_ENC_LEVEL_H264_51},
#if NVENCAPI_MAJOR_VERSION >= 10
        {N_("6.0"),           NV_ENC_LEVEL_H264_60},
        {N_("6.1"),           NV_ENC_LEVEL_H264_61},
        {N_("6.2"),           NV_ENC_LEVEL_H264_62},
#endif
    };
    return strtab2htsmsg(tab, 1, lang);
}

static const codec_profile_class_t codec_profile_nvenc_h264_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_nvenc_class,
        .ic_class      = "codec_profile_nvenc_h264",
        .ic_caption    = N_("nvenc_h264"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "level",
                .name     = N_("Level"),
                .group    = 5,
                .desc     = N_("Override the preset level."),
                .opts     = PO_EXPERT,
                .off      = offsetof(tvh_codec_profile_nvenc_t, level),
                .list     = codec_profile_nvenc_class_level_list_h264,
                .def.i    = NV_ENC_LEVEL_AUTOSELECT,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_nvenc_h264_open,
};


TVHVideoCodec tvh_codec_nvenc_h264 = {
    .name     = "h264_nvenc",
    .size     = sizeof(tvh_codec_profile_nvenc_t),
    .idclass  = &codec_profile_nvenc_h264_class,
    .profiles = nvenc_h264_profiles,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_video_destroy,
};


/* hevc_nvenc =============================================================== */

static const AVProfile nvenc_hevc_profiles[] = {
    { NV_ENC_HEVC_PROFILE_MAIN,     "Main" },
    { NV_ENC_HEVC_PROFILE_MAIN_10,  "Main 10" },
    { NV_ENC_HEVC_PROFILE_REXT,     "Rext" },
    { FF_AV_PROFILE_UNKNOWN },
};

static int
tvh_codec_profile_nvenc_hevc_open(tvh_codec_profile_nvenc_t *self,
                                  AVDictionary **opts)
{
// ffmpeg -hide_banner -h encoder=hevc_nvenc
//Encoder hevc_nvenc [NVIDIA NVENC hevc encoder]:
//    General capabilities: dr1 delay hardware
//    Threading capabilities: none
//    Supported hardware devices: cuda cuda
//    Supported pixel formats: yuv420p nv12 p010le yuv444p p016le yuv444p16le bgr0 rgb0 cuda
//hevc_nvenc AVOptions:
//  -preset            <int>        E..V....... Set the encoding preset (from 0 to 18) (default p4)
//     default         0            E..V.......
//     slow            1            E..V....... hq 2 passes
//     medium          2            E..V....... hq 1 pass
//     fast            3            E..V....... hp 1 pass
//     hp              4            E..V.......
//     hq              5            E..V.......
//     bd              6            E..V.......
//     ll              7            E..V....... low latency
//     llhq            8            E..V....... low latency hq
//     llhp            9            E..V....... low latency hp
//     lossless        10           E..V....... lossless
//     losslesshp      11           E..V....... lossless hp
//     p1              12           E..V....... fastest (lowest quality)
//     p2              13           E..V....... faster (lower quality)
//     p3              14           E..V....... fast (low quality)
//     p4              15           E..V....... medium (default)
//     p5              16           E..V....... slow (good quality)
//     p6              17           E..V....... slower (better quality)
//     p7              18           E..V....... slowest (best quality)
//  -tune              <int>        E..V....... Set the encoding tuning info (from 1 to 4) (default hq)
//     hq              1            E..V....... High quality
//     ll              2            E..V....... Low latency
//     ull             3            E..V....... Ultra low latency
//     lossless        4            E..V....... Lossless
//  -profile           <int>        E..V....... Set the encoding profile (from 0 to 4) (default main)
//     main            0            E..V.......
//     main10          1            E..V.......
//     rext            2            E..V.......
//  -level             <int>        E..V....... Set the encoding level restriction (from 0 to 186) (default auto)
//     auto            0            E..V.......
//     1               30           E..V.......
//     1.0             30           E..V.......
//     2               60           E..V.......
//     2.0             60           E..V.......
//     2.1             63           E..V.......
//     3               90           E..V.......
//     3.0             90           E..V.......
//     3.1             93           E..V.......
//     4               120          E..V.......
//     4.0             120          E..V.......
//     4.1             123          E..V.......
//     5               150          E..V.......
//     5.0             150          E..V.......
//     5.1             153          E..V.......
//     5.2             156          E..V.......
//     6               180          E..V.......
//     6.0             180          E..V.......
//     6.1             183          E..V.......
//     6.2             186          E..V.......
//  -tier              <int>        E..V....... Set the encoding tier (from 0 to 1) (default main)
//     main            0            E..V.......
//     high            1            E..V.......
//  -rc                <int>        E..V....... Override the preset rate-control (from -1 to INT_MAX) (default -1)
//     constqp         0            E..V....... Constant QP mode
//     vbr             1            E..V....... Variable bitrate mode
//     cbr             2            E..V....... Constant bitrate mode
//     vbr_minqp       8388612      E..V....... Variable bitrate mode with MinQP (deprecated)
//     ll_2pass_quality 8388616      E..V....... Multi-pass optimized for image quality (deprecated)
//     ll_2pass_size   8388624      E..V....... Multi-pass optimized for constant frame size (deprecated)
//     vbr_2pass       8388640      E..V....... Multi-pass variable bitrate mode (deprecated)
//     cbr_ld_hq       8388616      E..V....... Constant bitrate low delay high quality mode
//     cbr_hq          8388624      E..V....... Constant bitrate high quality mode
//     vbr_hq          8388640      E..V....... Variable bitrate high quality mode
//  -rc-lookahead      <int>        E..V....... Number of frames to look ahead for rate-control (from 0 to INT_MAX) (default 0)
//  -surfaces          <int>        E..V....... Number of concurrent surfaces (from 0 to 64) (default 0)
//  -cbr               <boolean>    E..V....... Use cbr encoding mode (default false)
//  -2pass             <boolean>    E..V....... Use 2pass encoding mode (default auto)
//  -gpu               <int>        E..V....... Selects which NVENC capable GPU to use. First GPU is 0, second is 1, and so on. (from -2 to INT_MAX) (default any)
//     any             -1           E..V....... Pick the first device available
//     list            -2           E..V....... List the available devices
//  -delay             <int>        E..V....... Delay frame output by the given amount of frames (from 0 to INT_MAX) (default INT_MAX)
//  -no-scenecut       <boolean>    E..V....... When lookahead is enabled, set this to 1 to disable adaptive I-frame insertion at scene cuts (default false)
//  -forced-idr        <boolean>    E..V....... If forcing keyframes, force them as IDR frames. (default false)
//  -spatial_aq        <boolean>    E..V....... set to 1 to enable Spatial AQ (default false)
//  -spatial-aq        <boolean>    E..V....... set to 1 to enable Spatial AQ (default false)
//  -temporal_aq       <boolean>    E..V....... set to 1 to enable Temporal AQ (default false)
//  -temporal-aq       <boolean>    E..V....... set to 1 to enable Temporal AQ (default false)
//  -zerolatency       <boolean>    E..V....... Set 1 to indicate zero latency operation (no reordering delay) (default false)
//  -nonref_p          <boolean>    E..V....... Set this to 1 to enable automatic insertion of non-reference P-frames (default false)
//  -strict_gop        <boolean>    E..V....... Set 1 to minimize GOP-to-GOP rate fluctuations (default false)
//  -aq-strength       <int>        E..V....... When Spatial AQ is enabled, this field is used to specify AQ strength. AQ strength scale is from 1 (low) - 15 (aggressive) (from 1 to 15) (default 8)
//  -cq                <float>      E..V....... Set target quality level (0 to 51, 0 means automatic) for constant quality mode in VBR rate control (from 0 to 51) (default 0)
//  -aud               <boolean>    E..V....... Use access unit delimiters (default false)
//  -bluray-compat     <boolean>    E..V....... Bluray compatibility workarounds (default false)
//  -init_qpP          <int>        E..V....... Initial QP value for P frame (from -1 to 51) (default -1)
//  -init_qpB          <int>        E..V....... Initial QP value for B frame (from -1 to 51) (default -1)
//  -init_qpI          <int>        E..V....... Initial QP value for I frame (from -1 to 51) (default -1)
//  -qp                <int>        E..V....... Constant quantization parameter rate control method (from -1 to 51) (default -1)
//  -weighted_pred     <int>        E..V....... Set 1 to enable weighted prediction (from 0 to 1) (default 0)
//  -b_ref_mode        <int>        E..V....... Use B frames as references (from 0 to 2) (default disabled)
//     disabled        0            E..V....... B frames will not be used for reference
//     each            1            E..V....... Each B frame will be used for reference
//     middle          2            E..V....... Only (number of B frames)/2 will be used for reference
//  -a53cc             <boolean>    E..V....... Use A53 Closed Captions (if available) (default true)
//  -s12m_tc           <boolean>    E..V....... Use timecode (if available) (default false)
//  -dpb_size          <int>        E..V....... Specifies the DPB size used for encoding (0 means automatic) (from 0 to INT_MAX) (default 0)
//  -multipass         <int>        E..V....... Set the multipass encoding (from 0 to 2) (default disabled)
//     disabled        0            E..V....... Single Pass
//     qres            1            E..V....... Two Pass encoding is enabled where first Pass is quarter resolution
//     fullres         2            E..V....... Two Pass encoding is enabled where first Pass is full resolution
//  -ldkfs             <int>        E..V....... Low delay key frame scale; Specifies the Scene Change frame size increase allowed in case of single frame VBV and CBR (from 0 to 255) (default 0)


    // ------ Set Defaults ---------
    AV_DICT_SET_INT(LST_NVENC, opts, "qblur", -1, 0);
    AV_DICT_SET_INT(LST_NVENC, opts, "qcomp", -1, 0);
    AV_DICT_SET_INT(LST_NVENC, opts, "bf", 0, 0);
    AV_DICT_SET_INT(LST_NVENC, opts, "refs", 0, 0);
    return 0;
}

static htsmsg_t *
codec_profile_nvenc_class_level_list_hevc(void *obj, const char *lang)
{

    static const struct strtab tab[] = {
        {N_("auto"),	      NV_ENC_LEVEL_AUTOSELECT},
        {N_("1.0"),           NV_ENC_LEVEL_HEVC_1},
        {N_("2.0"),           NV_ENC_LEVEL_HEVC_2},
        {N_("2.1"),           NV_ENC_LEVEL_HEVC_21},
        {N_("3.0"),           NV_ENC_LEVEL_HEVC_3},
        {N_("3.1"),           NV_ENC_LEVEL_HEVC_31},
        {N_("4.0"),           NV_ENC_LEVEL_HEVC_4},
        {N_("4.1"),           NV_ENC_LEVEL_HEVC_41},
        {N_("5.0"),           NV_ENC_LEVEL_HEVC_5},
        {N_("5.1"),           NV_ENC_LEVEL_HEVC_51},
        {N_("5.2"),           NV_ENC_LEVEL_HEVC_52},
#if NVENCAPI_MAJOR_VERSION >= 12
        {N_("6.0"),           NV_ENC_LEVEL_HEVC_6},
        {N_("6.1"),           NV_ENC_LEVEL_HEVC_61},
        {N_("6.2"),           NV_ENC_LEVEL_HEVC_62},
#endif
    };
    return strtab2htsmsg(tab, 1, lang);
}

static const codec_profile_class_t codec_profile_nvenc_hevc_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_nvenc_class,
        .ic_class      = "codec_profile_nvenc_hevc",
        .ic_caption    = N_("nvenc_hevc"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "level",
                .name     = N_("Level"),
                .group    = 5,
                .desc     = N_("Override the preset level."),
                .opts     = PO_EXPERT,
                .off      = offsetof(tvh_codec_profile_nvenc_t, level),
                .list     = codec_profile_nvenc_class_level_list_hevc,
                .def.i    = NV_ENC_LEVEL_AUTOSELECT,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_nvenc_hevc_open,
};

TVHVideoCodec tvh_codec_nvenc_hevc = {
    .name     = "hevc_nvenc",
    .size     = sizeof(tvh_codec_profile_nvenc_t),
    .idclass  = &codec_profile_nvenc_hevc_class,
    .profiles = nvenc_hevc_profiles,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_video_destroy,
};
