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
#define NV_ENC_PARAMS_RC_VBR_MINQP             4
#define NV_ENC_PARAMS_RC_2_PASS_QUALITY        5
#define NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP  6
#define NV_ENC_PARAMS_RC_2_PASS_VBR            7

#define AV_DICT_SET_CQ(d, v, a) \
    AV_DICT_SET_INT((d), "cq", (v) ? (v) : (a), AV_DICT_DONT_OVERWRITE)


/* nvenc ==================================================================== */

typedef struct {
    TVHVideoCodecProfile;
    int nvenc_profile;
    int devicenum;
    int preset;
    int rc;
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
        {"vbr",               NV_ENC_PARAMS_RC_VBR},
        {"cbr",               NV_ENC_PARAMS_RC_CBR},
        {"vbr_minqp",         NV_ENC_PARAMS_RC_VBR_MINQP},
        {"ll_2pass_quality",  NV_ENC_PARAMS_RC_2_PASS_QUALITY},
        {"ll_2pass_size",     NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP},
        {"vbr_2pass",         NV_ENC_PARAMS_RC_2_PASS_VBR},
    };
    const char *s;

    AV_DICT_SET_INT(opts, "gpu", MINMAX(self->devicenum, 0, 15), 0);
    if (self->preset != PRESET_DEFAULT &&
        (s = val2str(self->profile, presettab)) != NULL) {
        AV_DICT_SET(opts, "preset", s, 0);
    }
    if (self->rc != NV_ENC_PARAMS_RC_AUTO &&
        (s = val2str(self->rc, rctab)) != NULL) {
        AV_DICT_SET(opts, "rc", s, 0);
    }
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(opts, self->bit_rate);
    }
    AV_DICT_SET_INT(opts, "quality", self->quality, 0);
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
        {N_("Constant QP mode"),		  NV_ENC_PARAMS_RC_CONSTQP},
        {N_("VBR mode"),   			  NV_ENC_PARAMS_RC_VBR},
        {N_("CBR mode"), 	  		  NV_ENC_PARAMS_RC_CBR},
        {N_("VBR mode with MinQP"), 	          NV_ENC_PARAMS_RC_VBR_MINQP},
        {N_("VBR multi-pass LL quality mode"), 	  NV_ENC_PARAMS_RC_2_PASS_QUALITY},
        {N_("VBR multi-pass LL frame size mode"), NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP},
        {N_("VBR multi-pass mode"), 		  NV_ENC_PARAMS_RC_2_PASS_VBR},
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
                .def.i    = FF_PROFILE_UNKNOWN,
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
    { FF_PROFILE_H264_BASELINE,             "Baseline" },
    { FF_PROFILE_H264_MAIN,                 "Main" },
    { FF_PROFILE_H264_HIGH,                 "High" },
    { FF_PROFILE_H264_HIGH_444_PREDICTIVE,  "High 444P" },
    { FF_PROFILE_UNKNOWN },
};

static int
tvh_codec_profile_nvenc_h264_open(tvh_codec_profile_nvenc_t *self,
                                  AVDictionary **opts)
{
    static const struct strtab profiletab[] = {
        {"baseline",    FF_PROFILE_H264_BASELINE},
        {"main",        FF_PROFILE_H264_MAIN},
        {"high",        FF_PROFILE_H264_HIGH,},
        {"high444p",    FF_PROFILE_H264_HIGH_444_PREDICTIVE},
    };
    const char *s;

    if (self->nvenc_profile != FF_PROFILE_UNKNOWN &&
        (s = val2str(self->nvenc_profile, profiletab)) != NULL)
      AV_DICT_SET(opts, "profile", s, 0);
    return 0;
}


static const codec_profile_class_t codec_profile_nvenc_h264_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_nvenc_class,
        .ic_class      = "codec_profile_nvenc_h264",
        .ic_caption    = N_("nvenc_h264"),
        .ic_properties = (const property_t[]){
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
    { FF_PROFILE_HEVC_MAIN,    "Main" },
    { FF_PROFILE_HEVC_MAIN_10, "Main 10" },
    { FF_PROFILE_UNKNOWN },
};

static int
tvh_codec_profile_nvenc_hevc_open(tvh_codec_profile_nvenc_t *self,
                                  AVDictionary **opts)
{
    static const struct strtab profiletab[] = {
        {"main",        FF_PROFILE_HEVC_MAIN},
        {"main10",      FF_PROFILE_HEVC_MAIN_10,},
    };
    const char *s;

    if (self->nvenc_profile != FF_PROFILE_UNKNOWN &&
        (s = val2str(self->nvenc_profile, profiletab)) != NULL)
      AV_DICT_SET(opts, "profile", s, 0);
    return 0;
}


static const codec_profile_class_t codec_profile_nvenc_hevc_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_nvenc_class,
        .ic_class      = "codec_profile_nvenc_hevc",
        .ic_caption    = N_("nvenc_hevc")
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
