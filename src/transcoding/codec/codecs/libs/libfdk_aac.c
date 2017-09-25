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


/* libfdk_aac =============================================================== */

typedef struct {
    TVHAudioCodecProfile;
    int vbr;
    int afterburner;
    int eld_sbr;
    int signaling;
} tvh_codec_profile_libfdk_aac_t;


static int
tvh_codec_profile_libfdk_aac_open(tvh_codec_profile_libfdk_aac_t *self,
                                  AVDictionary **opts)
{
    AV_DICT_SET_FLAGS_GLOBAL_HEADER(opts);
    // bit_rate or vbr
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(opts, self->bit_rate);
    }
    else {
        AV_DICT_SET_INT(opts, "vbr", self->vbr ? self->vbr : 3, 0);
    }
    AV_DICT_SET_INT(opts, "afterburner", self->afterburner, 0);
    AV_DICT_SET_INT(opts, "eld_sbr", self->eld_sbr, 0);
    AV_DICT_SET_INT(opts, "signaling", self->signaling, 0);
    return 0;
}


static htsmsg_t *
codec_profile_libfdk_aac_class_signaling_list(void *obj, const char *lang)
{
    static const struct strtab tab[] = {
        {N_("default"),                                               -1},
        {N_("implicit: Implicit backwards compatible signaling"),      0},
        {N_("explicit_sbr: Explicit SBR, implicit PS signaling"),      1},
        {N_("explicit_hierarchical: Explicit hierarchical signaling"), 2}
    };
    return strtab2htsmsg(tab, 1, lang);
}


static const codec_profile_class_t codec_profile_libfdk_aac_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_audio_class,
        .ic_class      = "codec_profile_libfdk_aac",
        .ic_caption    = N_("libfdk_aac"),
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
                .id       = "vbr",
                .name     = N_("Quality (0=auto)"),
                .desc     = N_("Variable bitrate (VBR) mode [0-5]."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_libfdk_aac_t, vbr),
                .intextra = INTEXTRA_RANGE(0, 5, 1),
                .def.i    = 0,
            },
            {
                .type     = PT_BOOL,
                .id       = "afterburner",
                .name     = N_("Afterburner (improved quality)"),
                .desc     = N_("Afterburner (improved quality)."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_libfdk_aac_t, afterburner),
                .def.i    = 1,
            },
            {
                .type     = PT_BOOL,
                .id       = "eld_sbr",
                .name     = N_("Enable SBR for ELD"),
                .desc     = N_("Enable SBR for ELD."),
                .group    = 5,
                .opts     = PO_EXPERT | PO_PHIDDEN,
                .get_opts = codec_profile_class_profile_get_opts,
                .off      = offsetof(tvh_codec_profile_libfdk_aac_t, eld_sbr),
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "signaling",
                .name     = N_("Signaling"),
                .desc     = N_("SBR/PS signaling style."),
                .group    = 5,
                .opts     = PO_EXPERT | PO_PHIDDEN,
                .get_opts = codec_profile_class_profile_get_opts,
                .off      = offsetof(tvh_codec_profile_libfdk_aac_t, signaling),
                .list     = codec_profile_libfdk_aac_class_signaling_list,
                .def.i    = -1,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_libfdk_aac_open,
};


TVHAudioCodec tvh_codec_libfdk_aac = {
    .name    = "libfdk_aac",
    .size    = sizeof(tvh_codec_profile_libfdk_aac_t),
    .idclass = &codec_profile_libfdk_aac_class,
    .profile_init = tvh_codec_profile_audio_init,
    .profile_destroy = tvh_codec_profile_audio_destroy,
};
