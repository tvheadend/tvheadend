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

#include <opus/opus_defines.h>


/* libopus ================================================================== */

typedef struct {
    TVHAudioCodecProfile;
    int vbr;
    int application;
    int complexity;
} tvh_codec_profile_libopus_t;


static int
tvh_codec_profile_libopus_open(tvh_codec_profile_libopus_t *self,
                               AVDictionary **opts)
{
    AV_DICT_SET_BIT_RATE(LST_LIBOPUS, opts, self->bit_rate);
    AV_DICT_SET_INT(LST_LIBOPUS, opts, "vbr", self->vbr, 0);
    AV_DICT_SET_INT(LST_LIBOPUS, opts, "application", self->application, 0);
    AV_DICT_SET_INT(LST_LIBOPUS, opts, "compression_level", self->complexity, 0);
    return 0;
}


static htsmsg_t *
codec_profile_libopus_class_vbr_list(void *obj, const char *lang)
{
    static const struct strtab tab[] = {
        {N_("off: Use constant bit rate"),       0},
        {N_("on: Use variable bit rate"),        1},
        {N_("constrained: Use constrained VBR"), 2}
    };
    return strtab2htsmsg(tab, 1, lang);
}


static htsmsg_t *
codec_profile_libopus_class_application_list(void *obj, const char *lang)
{
    static const struct strtab tab[] = {
        {N_("voip: Favor improved speech intelligibility"),       OPUS_APPLICATION_VOIP},
        {N_("audio: Favor faithfulness to the input"),            OPUS_APPLICATION_AUDIO},
        {N_("lowdelay: Restrict to only the lowest delay modes"), OPUS_APPLICATION_RESTRICTED_LOWDELAY}
    };
    return strtab2htsmsg(tab, 1, lang);
}


static const codec_profile_class_t codec_profile_libopus_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_audio_class,
        .ic_class      = "codec_profile_libopus",
        .ic_caption    = N_("libopus"),
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
                .name     = N_("Bitrate mode"),
                .desc     = N_("Bitrate mode."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_libopus_t, vbr),
                .list     = codec_profile_libopus_class_vbr_list,
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "application",
                .name     = N_("Application"),
                .desc     = N_("Intended application type."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_libopus_t, application),
                .list     = codec_profile_libopus_class_application_list,
                .def.i    = OPUS_APPLICATION_AUDIO,
            },
            {
                .type     = PT_INT,
                .id       = "complexity",
                .name     = N_("Encoding algorithm complexity"),
                .desc     = N_("0 gives the fastest encodes but lower quality, "
                               "while 10 gives the highest quality but slowest "
                               "encoding [0-10]."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_libopus_t, complexity),
                .intextra = INTEXTRA_RANGE(0, 10, 1),
                .def.i    = 10,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_libopus_open,
};


TVHAudioCodec tvh_codec_libopus = {
    .name    = "libopus",
    .size    = sizeof(tvh_codec_profile_libopus_t),
    .idclass = &codec_profile_libopus_class,
    .profile_init = tvh_codec_profile_audio_init,
    .profile_destroy = tvh_codec_profile_audio_destroy,
};
