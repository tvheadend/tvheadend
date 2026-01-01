/*
 *  tvheadend - Codec Profiles
 *
 *  Copyright (C) 2017 Tvheadend
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

#define PRESET_DEFAULT               0
#define PRESET_SLOW                  1
#define PRESET_MEDIUM                2
#define PRESET_FAST                  3
#define PRESET_HP                    4
#define PRESET_HQ                    5
#define PRESET_BD                    6
#define PRESET_LOW_LATENCY_DEFAULT   7
#define PRESET_LOW_LATENCY_HQ        8
#define PRESET_LOW_LATENCY_HP        9
#define PRESET_LOSSLESS_DEFAULT      10
#define PRESET_LOSSLESS_HP           11

#define NV_ENC_PARAMS_RC_AUTO                  0
#define NV_ENC_PARAMS_RC_CONSTQP               1
#define NV_ENC_PARAMS_RC_VBR                   2
#define NV_ENC_PARAMS_RC_CBR                   3
#define NV_ENC_PARAMS_RC_CBR_LD_HQ             8
#define NV_ENC_PARAMS_RC_CBR_HQ                16
#define NV_ENC_PARAMS_RC_VBR_HQ                32

#define NV_ENC_H264_PROFILE_BASELINE			    0
#define NV_ENC_H264_PROFILE_MAIN			        1
#define NV_ENC_H264_PROFILE_HIGH			        2
#define NV_ENC_H264_PROFILE_HIGH_444P           	3

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

#define AV_DICT_SET_CQ(d, v, a) \
    AV_DICT_SET_INT(LST_NVENC, (d), "cq", (v) ? (v) : (a), AV_DICT_DONT_OVERWRITE)


/* nvenc ==================================================================== */

typedef struct {
    TVHVideoCodecProfile;
    int nvenc_profile;
    int devicenum;
    int preset;
    int rc;
    int level;
    int quality;
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
        {"hp",		PRESET_HP},
        {"hq",		PRESET_HQ},
        {"bd",		PRESET_BD},
        {"ll",		PRESET_LOW_LATENCY_DEFAULT},
        {"llhq",	PRESET_LOW_LATENCY_HQ},
        {"llhp",	PRESET_LOW_LATENCY_HP},
        {"lossless",	PRESET_LOSSLESS_DEFAULT},
        {"losslesshp",	PRESET_LOSSLESS_HP},
    };
    static const struct strtab rctab[] = {
        {"constqp",	      NV_ENC_PARAMS_RC_CONSTQP},
        {"vbr",           NV_ENC_PARAMS_RC_VBR},
        {"cbr",           NV_ENC_PARAMS_RC_CBR},
        {"cbr_ld_hq",     NV_ENC_PARAMS_RC_CBR_LD_HQ},
        {"cbr_hq",        NV_ENC_PARAMS_RC_CBR_HQ},
        {"vbr_hq",        NV_ENC_PARAMS_RC_VBR_HQ},
    };
    const char *s;

    AV_DICT_SET_INT(LST_NVENC, opts, "gpu", MINMAX(self->devicenum, 0, 15), 0);
    if (self->preset != PRESET_DEFAULT &&
        (s = val2str(self->profile, presettab)) != NULL) {
        AV_DICT_SET(LST_NVENC, opts, "preset", s, 0);
    }
    if (self->rc != NV_ENC_PARAMS_RC_AUTO &&
        (s = val2str(self->rc, rctab)) != NULL) {
        AV_DICT_SET(LST_NVENC, opts, "rc", s, 0);
    }
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(LST_NVENC, opts, self->bit_rate);
    }
    AV_DICT_SET_INT(LST_NVENC, opts, "quality", self->quality, 0);
    return 0;
}

static htsmsg_t *
codec_profile_nvenc_class_profile_list(void *obj, const char *lang)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(obj);
    return tvh_codec_get_list(codec, profiles);
}

static htsmsg_t *
codec_profile_nvenc_class_preset_list(void *obj, const char *lang)
{
    static const struct strtab tab[] = {
        {N_("Default"),		PRESET_DEFAULT},
        {N_("Slow"),		PRESET_SLOW},
        {N_("Medium"),		PRESET_MEDIUM},
        {N_("Fast"),		PRESET_FAST},
        {N_("HP"),		PRESET_HP},
        {N_("HQ"),		PRESET_HQ},
        {N_("BD"),		PRESET_BD},
        {N_("Low latency"),	PRESET_LOW_LATENCY_DEFAULT},
        {N_("Low latency HQ"),	PRESET_LOW_LATENCY_HQ},
        {N_("Low latency HP"),	PRESET_LOW_LATENCY_HP},
        {N_("Lossless"),	PRESET_LOSSLESS_DEFAULT},
        {N_("Lossless HP"),	PRESET_LOSSLESS_HP},
    };
    return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
codec_profile_nvenc_class_rc_list(void *obj, const char *lang)
{
    static const struct strtab tab[] = {
        {N_("Auto"),				  NV_ENC_PARAMS_RC_AUTO},
        {N_("Constant QP mode"),      NV_ENC_PARAMS_RC_CONSTQP},
        {N_("VBR mode"),   			  NV_ENC_PARAMS_RC_VBR},
        {N_("CBR mode"), 	  		  NV_ENC_PARAMS_RC_CBR},
        {N_("CBR LD HQ"),			  NV_ENC_PARAMS_RC_CBR_LD_HQ},
        {N_("CBR High Quality"),	  NV_ENC_PARAMS_RC_CBR_HQ},
        {N_("VBR High Quality"),   	  NV_ENC_PARAMS_RC_VBR_HQ},
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
                .desc     = N_("GPU number (starts with zero)."),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_nvenc_t, devicenum),
            },
            {
                .type     = PT_INT,
                .id       = "profile",
                .name     = N_("Profile"),
                .desc     = N_("Profile."),
                .group    = 4,
                .opts     = PO_ADVANCED | PO_PHIDDEN,
                .get_opts = codec_profile_class_profile_get_opts,
                .off      = offsetof(tvh_codec_profile_nvenc_t, nvenc_profile),
                .list     = codec_profile_nvenc_class_profile_list,
                .def.i    = FF_AV_PROFILE_UNKNOWN,
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
                .id       = "rc",
                .name     = N_("Rate control"),
                .group    = 3,
                .desc     = N_("Override the preset rate control."),
                .opts     = PO_EXPERT,
                .off      = offsetof(tvh_codec_profile_nvenc_t, rc),
                .list     = codec_profile_nvenc_class_rc_list,
                .def.i    = NV_ENC_PARAMS_RC_AUTO,
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
    static const struct strtab profiletab[] = {
        {"baseline",    NV_ENC_H264_PROFILE_BASELINE},
        {"main",        NV_ENC_H264_PROFILE_MAIN},
        {"high",        NV_ENC_H264_PROFILE_HIGH},
        {"high444p",    NV_ENC_H264_PROFILE_HIGH_444P},
    };
 
    static const struct strtab leveltab[] = {
        {"Auto",	      NV_ENC_LEVEL_AUTOSELECT},
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
        {"6.0",           NV_ENC_LEVEL_H264_6},
        {"6.1",           NV_ENC_LEVEL_H264_61},
        {"6.2",           NV_ENC_LEVEL_H264_62},
    };

    const char *s;

    if (self->level != NV_ENC_LEVEL_AUTOSELECT &&
        (s = val2str(self->level, leveltab)) != NULL) {
        AV_DICT_SET(LST_NVENC, opts, "level", s, 0);
    }

    if (self->nvenc_profile != FF_AV_PROFILE_UNKNOWN &&
        (s = val2str(self->nvenc_profile, profiletab)) != NULL) {
        AV_DICT_SET(LST_NVENC, opts, "profile", s, 0);
    }
    
    // ------ Set Defaults ---------
    AV_DICT_SET_INT(LST_NVENC, opts, "qmin", -1, 0);
    AV_DICT_SET_INT(LST_NVENC, opts, "qmax", -1, 0);
    AV_DICT_SET_INT(LST_NVENC, opts, "qdiff", -1, 0);
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
        {N_("Auto"),	      NV_ENC_LEVEL_AUTOSELECT},
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
        {N_("6.0"),           NV_ENC_LEVEL_H264_6},
        {N_("6.1"),           NV_ENC_LEVEL_H264_61},
        {N_("6.2"),           NV_ENC_LEVEL_H264_62},
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
                .group    = 4,
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
    static const struct strtab profiletab[] = {
        {"main",        NV_ENC_HEVC_PROFILE_MAIN},
        {"main10",      NV_ENC_HEVC_PROFILE_MAIN_10},
        {"rext",        NV_ENC_HEVC_PROFILE_REXT},
    };

    static const struct strtab leveltab[] = {
        {"Auto",	   NV_ENC_LEVEL_AUTOSELECT},
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
        {"6.0",           NV_ENC_LEVEL_HEVC_6},
        {"6.1",           NV_ENC_LEVEL_HEVC_61},
        {"6.2",           NV_ENC_LEVEL_HEVC_62},
    };

    const char *s;

    if (self->level != NV_ENC_LEVEL_AUTOSELECT &&
        (s = val2str(self->level, leveltab)) != NULL) {
        AV_DICT_SET(LST_NVENC, opts, "level", s, 0);
        }

    if (self->nvenc_profile != FF_AV_PROFILE_UNKNOWN &&
        (s = val2str(self->nvenc_profile, profiletab)) != NULL) {
        AV_DICT_SET(LST_NVENC, opts, "profile", s, 0);
        }
    
    // ------ Set Defaults ---------
    AV_DICT_SET_INT(LST_NVENC, opts, "qmin", -1, 0);
    AV_DICT_SET_INT(LST_NVENC, opts, "qmax", -1, 0);
    AV_DICT_SET_INT(LST_NVENC, opts, "qdiff", -1, 0);
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
        {N_("Auto"),	      NV_ENC_LEVEL_AUTOSELECT},
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
        {N_("6.0"),           NV_ENC_LEVEL_HEVC_6},
        {N_("6.1"),           NV_ENC_LEVEL_HEVC_61},
        {N_("6.2"),           NV_ENC_LEVEL_HEVC_62},
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
                .group    = 4,
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
