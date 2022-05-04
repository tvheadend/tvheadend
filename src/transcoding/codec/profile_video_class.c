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
    if (self->height) {
        self->size.den = self->height;
        self->size.den += self->size.den & 1;
        self->size.num = self->size.den * ((double)ssc->es_width / ssc->es_height);
        self->size.num += self->size.num & 1;
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
    AV_DICT_SET_INT(opts, "tvh_filter_deint", self->deinterlace, 0);
    // video_size
    AV_DICT_SET_INT(opts, "width", self->size.num, 0);
    AV_DICT_SET_INT(opts, "height", self->size.den, 0);
    // crf
    if (self->crf) {
        AV_DICT_SET_INT(opts, "crf", self->crf, AV_DICT_DONT_OVERWRITE);
    }
    // pix_fmt
    AV_DICT_SET_PIX_FMT(opts, self->pix_fmt, AV_PIX_FMT_YUV420P);
    // max_b_frames
    AV_DICT_SET_INT(opts, "bf", 3, AV_DICT_DONT_OVERWRITE);
    return 0;
}


/* codec_profile_video_class ================================================ */

/* codec_profile_video_class.deinterlace */

static int
codec_profile_video_class_deinterlace_set(void *obj, const void *val)
{
    TVHVideoCodecProfile *self = (TVHVideoCodecProfile *)obj;
    AVCodec *avcodec = NULL;

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
                .type     = PT_BOOL,
                .id       = "deinterlace",
                .name     = N_("Deinterlace"),
                .desc     = N_("Deinterlace."),
                .group    = 2,
                .off      = offsetof(TVHVideoCodecProfile, deinterlace),
                .set      = codec_profile_video_class_deinterlace_set,
                .def.i    = 1,
            },
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
