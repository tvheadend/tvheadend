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

// matched with src/webui/static/app/codec.js --> function filter_based_on_rc_nvenc(form)
#define NV_ENC_PARAMS_RC_AUTO                  0
#define NV_ENC_PARAMS_RC_CONSTQP               1
#define NV_ENC_PARAMS_RC_VBR                   2
#define NV_ENC_PARAMS_RC_CBR                   3
#define NV_ENC_PARAMS_RC_VBR_MINQP             4   
#define NV_ENC_PARAMS_RC_CBR_LD_HQ             8
#define NV_ENC_PARAMS_RC_CBR_HQ                16
#define NV_ENC_PARAMS_RC_VBR_HQ                32

// matched with ffmpeg/libavcodec/nvenc.h
#define NV_ENC_H264_PROFILE_BASELINE			    0
#define NV_ENC_H264_PROFILE_MAIN			        1
#define NV_ENC_H264_PROFILE_HIGH			        2
#define NV_ENC_H264_PROFILE_HIGH_444P           	3

// matched with ffmpeg/libavcodec/nvenc.h
#define NV_ENC_HEVC_PROFILE_MAIN			        0
#define NV_ENC_HEVC_PROFILE_MAIN_10 			    1
#define NV_ENC_HEVC_PROFILE_REXT			        2

#define NV_ENC_LEVEL_AUTOSELECT                     0

#define NV_ENC_LEVEL_H264_1                         10
#define NV_ENC_LEVEL_H264_1b                        9
#define NV_ENC_LEVEL_H264_11                        11
#define NV_ENC_LEVEL_H264_12                        12
#define NV_ENC_LEVEL_H264_13                        13
#define NV_ENC_LEVEL_H264_2                         20
#define NV_ENC_LEVEL_H264_21                        21
#define NV_ENC_LEVEL_H264_22                        22
#define NV_ENC_LEVEL_H264_3                         30
#define NV_ENC_LEVEL_H264_31                        31
#define NV_ENC_LEVEL_H264_32                        32
#define NV_ENC_LEVEL_H264_4                         40
#define NV_ENC_LEVEL_H264_41                        41
#define NV_ENC_LEVEL_H264_42                        42
#define NV_ENC_LEVEL_H264_5                         50
#define NV_ENC_LEVEL_H264_51                        51
#define NV_ENC_LEVEL_H264_6                         60
#define NV_ENC_LEVEL_H264_61                        61
#define NV_ENC_LEVEL_H264_62                        62

#define NV_ENC_LEVEL_HEVC_1                         30
#define NV_ENC_LEVEL_HEVC_2                         60
#define NV_ENC_LEVEL_HEVC_21                        63
#define NV_ENC_LEVEL_HEVC_3                         90
#define NV_ENC_LEVEL_HEVC_31                        93
#define NV_ENC_LEVEL_HEVC_4                         120
#define NV_ENC_LEVEL_HEVC_41                        123
#define NV_ENC_LEVEL_HEVC_5                         150
#define NV_ENC_LEVEL_HEVC_51                        153
#define NV_ENC_LEVEL_HEVC_52                        156
#define NV_ENC_LEVEL_HEVC_6                         180
#define NV_ENC_LEVEL_HEVC_61                        183
#define NV_ENC_LEVEL_HEVC_62                        186

#define NV_ENC_TUNING_INFO_UNDEFINED                0   //< Undefined tuningInfo. Invalid value for encoding.
#define NV_ENC_TUNING_INFO_HIGH_QUALITY             1   //< Tune presets for latency tolerant encoding.
#define NV_ENC_TUNING_INFO_LOW_LATENCY              2   //< Tune presets for low latency streaming.
#define NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY        3   //< Tune presets for ultra low latency streaming.
#define NV_ENC_TUNING_INFO_LOSSLESS                 4   //< Tune presets for lossless encoding.

/* nvenc ==================================================================== */

typedef struct {
    TVHVideoCodecProfile;
/**
 * NVENC Constant QP [qp]
 * @note
 * int:
 * VALUE - Constant QP value (1-51, 0=skip)
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
 * VALUE - encoding tuning info (from 1 to 4) (default skip) (from nvEncodeAPI.h, 0=skip)
 */
    int tune;
/**
 * NVENC Maximum bitrate [maxrate]
 * @note
 * double:
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
 * VALUE - rc mode accepted values (from 0 to 32) (default auto)
 * Note: some rc modes are deprecated
 */
    int rc;
/**
 * NVENC Set target quality level for constant quality mode in VBR rate control [cq]
 * @note
 * int:
 * VALUE - quality level (0 to 51, 0 means automatic)
 */
    int cq;
/**
 * NVENC Set the encoding level restriction [level]
 * @note
 * int:
 * VALUE - level restriction (from 0 to 62)
 */
    int level;
/**
 * NVENC Sets the lowest possible quantization level [qmin]
 * @note
 * int:
 * VALUE - quantization restriction (from 0 to 51)
 */
    int qmin;
/**
 * NVENC Sets the highest possible quantization level [qmax]
 * @note
 * int:
 * VALUE - quantization restriction (from 0 to 51)
 */
    int qmax;
/**
 * NVENC Sets the maximum allowed difference between the Quantization Parameter (QP) of consecutive frames [qdiff]
 * @note
 * int:
 * VALUE - quantization restriction (from -1 to 69)
 */
    int qdiff;
} tvh_codec_profile_nvenc_t;

static int
tvh_codec_profile_nvenc_open(tvh_codec_profile_nvenc_t *self,
                             AVDictionary **opts)
{
    static const struct strtab presettab[] = {
        {"default",     PRESET_DEFAULT},
        {"slow",        PRESET_SLOW},
        {"medium",      PRESET_MEDIUM},
        {"fast",        PRESET_FAST},
#if NVENCAPI_MAJOR_VERSION < 10
        {"hp",		    PRESET_HP},
        {"hq",		    PRESET_HQ},
        {"bd",		    PRESET_BD},
        {"ll",		    PRESET_LOW_LATENCY_DEFAULT},
        {"llhq",	    PRESET_LOW_LATENCY_HQ},
        {"llhp",	    PRESET_LOW_LATENCY_HP},
        {"lossless",	PRESET_LOSSLESS_DEFAULT},
        {"losslesshp",	PRESET_LOSSLESS_HP},
#else
        {"p1",          PRESET_P1},
        {"p2",          PRESET_P2},
        {"p3",          PRESET_P3},
        {"p4",          PRESET_P4},
        {"p5",          PRESET_P5},
        {"p6",          PRESET_P6},
        {"p7",          PRESET_P7},
#endif
    };
    static const struct strtab rctab[] = {
        {"constqp",	      NV_ENC_PARAMS_RC_CONSTQP},
        {"vbr",           NV_ENC_PARAMS_RC_VBR},
        {"cbr",           NV_ENC_PARAMS_RC_CBR},
#if NVENCAPI_MAJOR_VERSION < 10
        {"vbr_minqp",     NV_ENC_PARAMS_RC_VBR_MINQP},
        {"cbr_ld_hq",     NV_ENC_PARAMS_RC_CBR_LD_HQ},
        {"cbr_hq",        NV_ENC_PARAMS_RC_CBR_HQ},
        {"vbr_hq",        NV_ENC_PARAMS_RC_VBR_HQ},
#endif
    };
#if NVENCAPI_MAJOR_VERSION >= 10
    static const struct strtab tunetab[] = {
        {"hq",	            NV_ENC_PARAMS_RC_CONSTQP},
        {"ll",              NV_ENC_PARAMS_RC_VBR},
        {"ull",             NV_ENC_PARAMS_RC_CBR},
        {"lossless",        NV_ENC_PARAMS_RC_VBR_MINQP},
    };
#endif
    const char *s;

    if (self->devicenum != -2) {
        AV_DICT_SET_INT(LST_NVENC, opts, "gpu", self->devicenum, 0);
    }
    if (self->preset != PRESET_SKIP &&
        (s = val2str(self->preset, presettab)) != NULL) {
            AV_DICT_SET(LST_NVENC, opts, "preset", s, 0);
        }
#if NVENCAPI_MAJOR_VERSION >= 10
    if (self->tune != NV_ENC_TUNING_INFO_UNDEFINED &&
        (s = val2str(self->tune, tunetab)) != NULL) {
            AV_DICT_SET(LST_NVENC, opts, "tune", s, 0);
        }
#endif
    if (self->rc != NV_ENC_PARAMS_RC_AUTO &&
        (s = val2str(self->rc, rctab)) != NULL) {
            AV_DICT_SET(LST_NVENC, opts, "rc", s, 0);
        }
    
    int int_bitrate = (int)((self->bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    int int_max_bitrate = (int)((self->max_bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    int int_qp = self->qp;
    int int_cq = self->cq;
    // disable fields based on rc mode to match the UI behavior
    // skip and auto are not filtering
    if (self->rc == NV_ENC_PARAMS_RC_VBR || self->rc == NV_ENC_PARAMS_RC_AUTO
#if NVENCAPI_MAJOR_VERSION < 10
        || self->rc == NV_ENC_PARAMS_RC_VBR_MINQP || self->rc == NV_ENC_PARAMS_RC_VBR_HQ
#endif
        ) {
        // force max_bitrate to be >= with bitrate (to avoid crash)
        if (int_bitrate > int_max_bitrate) {
            tvherror_transcode(LST_NVENC, "Bitrate %d kbps is greater than Max bitrate %d kbps, increase Max bitrate to %d kbps", int_bitrate / 1024, int_max_bitrate / 1024, int_bitrate / 1024);
            int_max_bitrate = int_bitrate;
        }
        if (int_bitrate) {
            tvhinfo_transcode(LST_NVENC, "Bitrate = %d kbps; Max bitrate = %d kbps", int_bitrate / 1024, int_max_bitrate / 1024);
        }
        int_qp = 0; // disable qp setting
    }
    else 
        if(self->rc == NV_ENC_PARAMS_RC_CBR
#if NVENCAPI_MAJOR_VERSION < 10
            || self->rc == NV_ENC_PARAMS_RC_CBR_LD_HQ || self->rc == NV_ENC_PARAMS_RC_CBR_HQ
#endif
            ) {
            // in CBR mode max_bitrate is ignored by nvenc, so we only use bitrate
            if (int_bitrate) {
                tvhinfo_transcode(LST_NVENC, "Bitrate = %d kbps", int_bitrate / 1024);
            }
            int_max_bitrate = 0; // disable max_bitrate setting
            int_qp = 0;         // disable qp setting
            int_cq = 0;         // disable cq setting
        }
        else if(self->rc == NV_ENC_PARAMS_RC_CONSTQP) {
            // in CQP mode both bitrate and max_bitrate are ignored by nvenc, so we only use qp
            int_bitrate = 0;    // disable bitrate setting
            int_max_bitrate = 0; // disable max_bitrate setting
            int_cq = 0;         // disable cq setting
        }
    if (int_bitrate) {
        AV_DICT_SET_INT(LST_NVENC, opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
    }
    if (int_max_bitrate) {
        AV_DICT_SET_INT(LST_NVENC, opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
    }
    if (int_qp) {
        AV_DICT_SET_INT(LST_NVENC, opts, "qp", int_qp, 0);
    }
    if (int_cq) {
        AV_DICT_SET_INT(LST_NVENC, opts, "cq", int_cq, 0);
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
        {N_("Constant QP"),                     NV_ENC_PARAMS_RC_CONSTQP},
        {N_("Variable Bitrate"),                NV_ENC_PARAMS_RC_VBR},
        {N_("Constant Bitrate"),                NV_ENC_PARAMS_RC_CBR},
#if NVENCAPI_MAJOR_VERSION < 10
        {N_("VBR min Q (deprecated)"),          NV_ENC_PARAMS_RC_VBR_MINQP},
        {N_("CBR LD HQ (deprecated)"),          NV_ENC_PARAMS_RC_CBR_LD_HQ},
        {N_("CBR High Quality (deprecated)"),   NV_ENC_PARAMS_RC_CBR_HQ},
        {N_("VBR High Quality (deprecated)"),   NV_ENC_PARAMS_RC_VBR_HQ},
#endif
    };
    return strtab2htsmsg(tab, 1, lang);
}

// NOTE:
// the names below are used in codec.js (/src/webui/static/app/codec.js)
static const codec_profile_class_t codec_profile_nvenc_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_video_class,
        .ic_class      = "codec_profile_nvenc",
        .ic_caption    = N_("nvenc"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "devicenum",     // Don't change
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
                .id       = "preset",     // Don't change
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
                .id       = "tune",     // Don't change
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
                .id       = "rc_mode",     // Don't change
                .name     = N_("Rate control mode"),
                .group    = 3,
                .desc     = N_("Override the preset rate control."),
                .opts     = PO_EXPERT,
                .off      = offsetof(tvh_codec_profile_nvenc_t, rc),
                .list     = codec_profile_nvenc_class_rc_list,
                .def.i    = NV_ENC_PARAMS_RC_AUTO,
            },
            {
                .type     = PT_INT,
                .id       = "qp",     // Don't change
                .name     = N_("Constant QP"),
                .group    = 3,
                .desc     = N_("Fixed QP of P frames (from 0 to 51, 0=skip).[if disabled will not send parameter to libav]"),
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
                .desc     = N_("Maximum QP change between adjacent frames (from -2 to 69, -1=default, -2=skip)"),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_nvenc_t, qdiff),
                .intextra = INTEXTRA_RANGE(-2, 69, 1),
                .def.i    = -2,
            },
            {
                .type     = PT_DBL,
                .id       = "bit_rate",     // Don't change
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
                .desc     = N_("Maximum bitrate (0=skip).[if disabled will not send parameter to libav]"),
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
                .id       = "cq",     // Don't change
                .name     = N_("Quality (0=auto)"),
                .desc     = N_("Set target quality level (0 to 51, 0 means automatic) for constant quality mode in VBR rate control."),
                .group    = 3,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_nvenc_t, cq),
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
        static const struct strtab leveltab[] = {
        {"auto",	      NV_ENC_LEVEL_AUTOSELECT},
        {"1.0",           NV_ENC_LEVEL_H264_1},
        {"1.0b",          NV_ENC_LEVEL_H264_1b},
        {"1.1",           NV_ENC_LEVEL_H264_11},
        {"1.2",           NV_ENC_LEVEL_H264_12},
        {"1.3",           NV_ENC_LEVEL_H264_13},
        {"2.0",           NV_ENC_LEVEL_H264_2},
        {"2.1",           NV_ENC_LEVEL_H264_21},
        {"2.2",           NV_ENC_LEVEL_H264_22},
        {"3.0",           NV_ENC_LEVEL_H264_3},
        {"3.1",           NV_ENC_LEVEL_H264_31},
        {"3.2",           NV_ENC_LEVEL_H264_32},
        {"4.0",           NV_ENC_LEVEL_H264_4},
        {"4.1",           NV_ENC_LEVEL_H264_41},
        {"4.2",           NV_ENC_LEVEL_H264_42},
        {"5.0",           NV_ENC_LEVEL_H264_5},
        {"5.1",           NV_ENC_LEVEL_H264_51},
#if NVENCAPI_MAJOR_VERSION >= 10
        {"6.0",           NV_ENC_LEVEL_H264_6},
        {"6.1",           NV_ENC_LEVEL_H264_61},
        {"6.2",           NV_ENC_LEVEL_H264_62},
#endif
    };

    const char *s;

    if (self->level != NV_ENC_LEVEL_AUTOSELECT &&
        (s = val2str(self->level, leveltab)) != NULL) {
        AV_DICT_SET(LST_NVENC, opts, "level", s, 0);
    }

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
        {N_("6.0"),           NV_ENC_LEVEL_H264_6},
        {N_("6.1"),           NV_ENC_LEVEL_H264_61},
        {N_("6.2"),           NV_ENC_LEVEL_H264_62},
#endif
    };
    return strtab2htsmsg(tab, 1, lang);
}

// NOTE:
// the names below are used in codec.js (/src/webui/static/app/codec.js)
static const codec_profile_class_t codec_profile_nvenc_h264_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_nvenc_class,
        .ic_class      = "codec_profile_nvenc_h264",
        .ic_caption    = N_("nvenc_h264"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "level",     // Don't change
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
    { NV_ENC_HEVC_PROFILE_MAIN,    "Main" },
    { NV_ENC_HEVC_PROFILE_MAIN_10, "Main 10" },
    { NV_ENC_HEVC_PROFILE_REXT, "Rext" },
    { FF_AV_PROFILE_UNKNOWN },
};

static int
tvh_codec_profile_nvenc_hevc_open(tvh_codec_profile_nvenc_t *self,
                                  AVDictionary **opts)
{
    static const struct strtab leveltab[] = {
        {"auto",	   NV_ENC_LEVEL_AUTOSELECT},
        {"1.0",           NV_ENC_LEVEL_HEVC_1},
        {"2.0",           NV_ENC_LEVEL_HEVC_2},
        {"2.1",           NV_ENC_LEVEL_HEVC_21},
        {"3.0",           NV_ENC_LEVEL_HEVC_3},
        {"3.1",           NV_ENC_LEVEL_HEVC_31},
        {"4.0",           NV_ENC_LEVEL_HEVC_4},
        {"4.1",           NV_ENC_LEVEL_HEVC_41},
        {"5.0",           NV_ENC_LEVEL_HEVC_5},
        {"5.1",           NV_ENC_LEVEL_HEVC_51},
        {"5.2",           NV_ENC_LEVEL_HEVC_52},
#if NVENCAPI_MAJOR_VERSION >= 10
        {"6.0",           NV_ENC_LEVEL_HEVC_6},
        {"6.1",           NV_ENC_LEVEL_HEVC_61},
        {"6.2",           NV_ENC_LEVEL_HEVC_62},
#endif
    };

    const char *s;

    if (self->level != NV_ENC_LEVEL_AUTOSELECT &&
        (s = val2str(self->level, leveltab)) != NULL) {
        AV_DICT_SET(LST_NVENC, opts, "level", s, 0);
        }

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
#if NVENCAPI_MAJOR_VERSION >= 10
        {N_("6.0"),           NV_ENC_LEVEL_HEVC_6},
        {N_("6.1"),           NV_ENC_LEVEL_HEVC_61},
        {N_("6.2"),           NV_ENC_LEVEL_HEVC_62},
#endif
    };
    return strtab2htsmsg(tab, 1, lang);
}

// NOTE:
// the names below are used in codec.js (/src/webui/static/app/codec.js)
static const codec_profile_class_t codec_profile_nvenc_hevc_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_nvenc_class,
        .ic_class      = "codec_profile_nvenc_hevc",
        .ic_caption    = N_("nvenc_hevc"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "level",     // Don't change
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
