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


#include "internals.h"

#include <libavutil/pixdesc.h>

static htsmsg_t *
scaling_mode_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("Up & Down"),      0 },
        { N_("Up (only)"),      1 },
        { N_("Down (only)"),    2 },
    };
    return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
hwaccel_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("auto (recommended)"),                 HWACCEL_AUTO},
#if ENABLE_VAAPI
        { N_("prioritize VAAPI"),                   HWACCEL_PRIORITIZE_VAAPI},
#endif
#if ENABLE_NVENC
        { N_("prioritize NVDEC"),                   HWACCEL_PRIORITIZE_NVDEC},
#endif
#if ENABLE_MMAL
        { N_("prioritize MMAL"),                    HWACCEL_PRIORITIZE_MMAL},
#endif
    };
    return strtab2htsmsg(tab, 1, lang);
}

#if ENABLE_VAAPI
static htsmsg_t *
deinterlace_vaapi_mode_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("Default"),                                 VAAPI_DEINT_MODE_DEFAULT },
        { N_("Bob Deinterlacing"),                       VAAPI_DEINT_MODE_BOB },
        { N_("Weave Deinterlacing"),                     VAAPI_DEINT_MODE_WEAVE },
        { N_("Motion Adaptive Deinterlacing (MADI)"),    VAAPI_DEINT_MODE_MADI },
        { N_("Motion Compensated Deinterlacing (MCDI)"), VAAPI_DEINT_MODE_MCDI },
    };
    return strtab2htsmsg(tab, 1, lang);
}
#endif

static htsmsg_t *
deinterlace_field_rate_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("Frame Rate"),                         DEINT_RATE_FRAME },
        { N_("Field Rate"),                         DEINT_RATE_FIELD },
    };
    return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
deinterlace_enable_auto_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("Disable"),                            DEINT_AUTO_OFF },
        { N_("Enable"),                             DEINT_AUTO_ON },
    };
    return strtab2htsmsg(tab, 1, lang);
}


/* TVHCodec ================================================================= */

static htsmsg_t *
tvh_codec_video_get_list_pix_fmts(TVHVideoCodec *self)
{
    htsmsg_t *list = NULL, *map = NULL;
    const enum AVPixelFormat *pix_fmts = self->pix_fmts;
    enum AVPixelFormat f = AV_PIX_FMT_NONE;
    const char *f_str = NULL;
    int i;

    if (pix_fmts && (list = htsmsg_create_list())) {
        if (!(map = htsmsg_create_map())) {
            htsmsg_destroy(list);
            list = NULL;
        }
        else {
            ADD_ENTRY(list, map, s32, f, str, AUTO_STR);
            for (i = 0; (f = pix_fmts[i]) != AV_PIX_FMT_NONE; i++) {
                if (!(f_str = av_get_pix_fmt_name(f)) ||
                    !(map = htsmsg_create_map())) {
                    htsmsg_destroy(list);
                    list = NULL;
                    break;
                }
                ADD_ENTRY(list, map, s32, f, str, f_str);
            }
        }
    }
    return list;
}


/* TVHCodecProfile ========================================================== */

static int
tvh_codec_profile_video_setup(TVHVideoCodecProfile *self, tvh_ssc_t *ssc)
{
    self->size.den = ssc->es_height;
    self->size.num = ssc->es_width;
    switch (self->scaling_mode) {
        case 0:
            // scaling up and down
            if (self->height) {
                self->size.den = self->height;
                self->size.den += self->size.den & 1;
                self->size.num = (int)((double)self->size.den * ((double)ssc->es_width / (double)ssc->es_height));
                self->size.num += self->size.num & 1;
            }
            break;
        case 1:
            // scaling up (only)
            if ((self->height > 0) && (self->size.den < self->height)) {
                self->size.den = self->height;
                self->size.den += self->size.den & 1;
                self->size.num = (int)((double)self->size.den * ((double)ssc->es_width / (double)ssc->es_height));
                self->size.num += self->size.num & 1;
            }
            break;
        case 2:
            // scaling down (only)
            if ((self->height > 0) && (self->size.den > self->height)) {
                self->size.den = self->height;
                self->size.den += self->size.den & 1;
                self->size.num = (int)((double)self->size.den * ((double)ssc->es_width / (double)ssc->es_height));
                self->size.num += self->size.num & 1;
            }
            break;
    }
    return 0;
}


static int
tvh_codec_profile_video_is_copy(TVHVideoCodecProfile *self, tvh_ssc_t *ssc)
{
    return (!self->deinterlace && (self->size.den == ssc->es_height));
}


static int
tvh_codec_profile_video_open(TVHVideoCodecProfile *self, AVDictionary **opts)
{
    // video_size
    AV_DICT_SET_INT(LST_VIDEO, opts, "width", self->size.num, 0);
    AV_DICT_SET_INT(LST_VIDEO, opts, "height", self->size.den, 0);
    // crf
    if (self->crf) {
        AV_DICT_SET_INT(LST_VIDEO, opts, "crf", self->crf, AV_DICT_DONT_OVERWRITE);
    }
    // pix_fmt
    AV_DICT_SET_PIX_FMT(LST_VIDEO, opts, self->pix_fmt, AV_PIX_FMT_YUV420P);
    return 0;
}


/* codec_profile_video_class ================================================ */

/* codec_profile_video_class.deinterlace */

static int
codec_profile_video_class_deinterlace_set(void *obj, const void *val)
{
    TVHVideoCodecProfile *self = (TVHVideoCodecProfile *)obj;
    const AVCodec *avcodec = NULL;

    if (self &&
        (avcodec = tvh_codec_profile_get_avcodec((TVHCodecProfile *)self))) {
        self->deinterlace = *(int *)val;
        return 1;
    }
    return 0;
}


/* codec_profile_video_class.pix_fmt */

static uint32_t
codec_profile_video_class_pix_fmt_get_opts(void *obj, uint32_t opts)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(obj);
    return tvh_codec_video_get_opts(codec, pix_fmts, opts);
}


static htsmsg_t *
codec_profile_video_class_pix_fmt_list(void *obj, const char *lang)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(obj);
    return tvh_codec_video_get_list(codec, pix_fmts);
}


/* codec_profile_video_class */

const codec_profile_class_t codec_profile_video_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_class,
        .ic_class      = "codec_profile_video",
        .ic_caption    = N_("video"),
        .ic_properties = (const property_t[]) {
            {
                .type     = PT_INT,
                .id       = "height",
                .name     = N_("Height (pixels) (0=no scaling)"),
                .desc     = N_("Height of the output video stream. Horizontal resolution "
                               "is adjusted automatically to preserve aspect ratio. "
                               "When set to 0, the input resolution is used."),
                .group    = 2,
                .off      = offsetof(TVHVideoCodecProfile, height),
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "scaling_mode",
                .name     = N_("Scaling mode"),
                .desc     = N_("Allow control for scaling Up&Down, Up or Down"),
                .group    = 2,
                .off      = offsetof(TVHVideoCodecProfile, scaling_mode),
                .list     = scaling_mode_get_list,
                .def.i    = 0,
            },
            {
                .type     = PT_BOOL,
                .id       = "hwaccel",
                .name     = N_("Hardware acceleration"),
                .desc     = N_("Use hardware acceleration for decoding if available."),
                .group    = 2,
                .off      = offsetof(TVHVideoCodecProfile, hwaccel),
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "hwaccel_details",
                .name     = N_("Hardware acceleration details"),
                .desc     = N_("Force hardware acceleration."),
                .group    = 2,
                .off      = offsetof(TVHVideoCodecProfile, hwaccel_details),
                .list     = hwaccel_get_list,
                .def.i    = 0,
            },
            {
                .type     = PT_BOOL,
                .id       = "deinterlace",
                .name     = N_("Deinterlace"),
                .desc     = N_("Deinterlace."),
                .group    = 2,
                .off      = offsetof(TVHVideoCodecProfile, deinterlace),
                .set      = codec_profile_video_class_deinterlace_set,
                .def.i    = 1,
            },
#if ENABLE_VAAPI
            {
                .type     = PT_INT,
                .id       = "deinterlace_vaapi_mode",
                .name     = N_("VAAPI Deinterlace mode"),
                .desc     = N_("Mode to use for VAAPI Deinterlacing. "
                               "'Default' selects the most advanced deinterlacer, i.e. the mode appearing last in this list. "
                               "Tip: MADI and MCDI usually yield the smoothest results, especially when used with field rate output."),
                .group    = 2,
                .opts     = PO_ADVANCED,
                .off      = offsetof(TVHVideoCodecProfile, deinterlace_vaapi_mode),
                .list     = deinterlace_vaapi_mode_get_list,
                .def.i    = VAAPI_DEINT_MODE_DEFAULT,
            },
#endif
            {
                .type     = PT_INT,
                .id       = "deinterlace_field_rate",
                .name     = N_("Deinterlace rate type"),
                .desc     = N_("Frame rate combines the two interlaced fields to create a single frame. "
                               "Field rate processes each field independently, outputting as individual frames, "
                               "which enables higher temporal resolution by producing one frame per field. "
                               "Note: with field rate deinterlacing the resulting stream will have double "
                               "frame-rate (for example 25i becomes 50p), which can result in smoother video "
                               "since the original temporal properties of the interlaced video are retained."),
                .group    = 2,
                .opts     = PO_ADVANCED,
                .off      = offsetof(TVHVideoCodecProfile, deinterlace_field_rate),
                .list     = deinterlace_field_rate_get_list,
                .def.i    = DEINT_RATE_FRAME,
            },
            {
                .type     = PT_INT,
                .id       = "deinterlace_enable_auto",
                .name     = N_("Deinterlace fields only"),
                .desc     = N_("Enable this option to only deinterlace fields, passing progressive frames "
                               "unchanged. This is useful for mixed content, allowing progressive frames "
                               "to bypass deinterlacing for improved efficiency and quality."),
                .group    = 2,
                .opts     = PO_EXPERT,
                .off      = offsetof(TVHVideoCodecProfile, deinterlace_enable_auto),
                .list     = deinterlace_enable_auto_get_list,
                .def.i    = DEINT_AUTO_OFF,
            },
            {
                .type     = PT_INT,
                .id       = "pix_fmt",
                .name     = N_("Pixel format"),
                .desc     = N_("Video pixel format."),
                .group    = 4,
                .opts     = PO_ADVANCED | PO_PHIDDEN,
                .get_opts = codec_profile_video_class_pix_fmt_get_opts,
                .off      = offsetof(TVHVideoCodecProfile, pix_fmt),
                .list     = codec_profile_video_class_pix_fmt_list,
                .def.i    = AV_PIX_FMT_NONE,
            },
            {}
        }
    },
    .setup       = (codec_profile_is_copy_meth)tvh_codec_profile_video_setup,
    .is_copy     = (codec_profile_is_copy_meth)tvh_codec_profile_video_is_copy,
    .open        = (codec_profile_open_meth)tvh_codec_profile_video_open,
};
