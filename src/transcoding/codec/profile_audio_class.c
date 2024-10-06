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

#include "lang_codes.h"

/* TVHCodec ================================================================= */

static htsmsg_t *
tvh_codec_audio_get_list_sample_fmts(TVHAudioCodec *self)
{
    htsmsg_t *list = NULL, *map = NULL;
    const enum AVSampleFormat *sample_fmts = self->sample_fmts;
    enum AVSampleFormat f = AV_SAMPLE_FMT_NONE;
    const char *f_str = NULL;
    int i;

    if (sample_fmts && (list = htsmsg_create_list())) {
        if (!(map = htsmsg_create_map())) {
            htsmsg_destroy(list);
            list = NULL;
        }
        else {
            ADD_ENTRY(list, map, s32, f, str, AUTO_STR);
            for (i = 0; (f = sample_fmts[i]) != AV_SAMPLE_FMT_NONE; i++) {
                if (!(f_str = av_get_sample_fmt_name(f)) ||
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


static htsmsg_t *
tvh_codec_audio_get_list_sample_rates(TVHAudioCodec *self)
{
    htsmsg_t *list = NULL, *map = NULL;
    const int *sample_rates = self->sample_rates;
    int r = 0, i;

   if (sample_rates && (list = htsmsg_create_list())) {
        if (!(map = htsmsg_create_map())) {
            htsmsg_destroy(list);
            list = NULL;
        }
        else {
            ADD_ENTRY(list, map, s32, r, str, AUTO_STR);
            for (i = 0; (r = sample_rates[i]); i++) {
                if (!(map = htsmsg_create_map())) {
                    htsmsg_destroy(list);
                    list = NULL;
                    break;
                }
                ADD_S32_VAL(list, map, r);
            }
        }
    }
    return list;
}


static htsmsg_t *
tvh_codec_audio_get_list_channel_layouts(TVHAudioCodec *self)
{
    htsmsg_t *list = NULL, *map = NULL;
#if LIBAVCODEC_VERSION_MAJOR > 59
    const AVChannelLayout *channel_layouts = self->channel_layouts;
    const AVChannelLayout *l;
#else
    const uint64_t *channel_layouts = self->channel_layouts;
    uint64_t l = 0;
    int i;
#endif
    char l_buf[16];

    if (channel_layouts && (list = htsmsg_create_list())) {
        if (!(map = htsmsg_create_map())) {
            htsmsg_destroy(list);
            list = NULL;
        }
        else {
#if LIBAVCODEC_VERSION_MAJOR > 59
            l = channel_layouts;
            ADD_ENTRY(list, map, s64, 0, str, AUTO_STR);
            while (l->nb_channels != 0) {
                if (!(map = htsmsg_create_map())) {
                    htsmsg_destroy(list);
                    list = NULL;
                    break;
                }
                l_buf[0] = '\0';
                if(av_channel_layout_describe(l, l_buf, sizeof(l_buf)) > 0)
                    ADD_ENTRY(list, map, s64, l->u.mask, str, l_buf);
                l++;
            }
#else
            ADD_ENTRY(list, map, s64, l, str, AUTO_STR);
            for (i = 0; (l = channel_layouts[i]); i++) {
                if (l < INT64_MAX) {
                    if (!(map = htsmsg_create_map())) {
                        htsmsg_destroy(list);
                        list = NULL;
                        break;
                    }
                    l_buf[0] = '\0';
                    av_get_channel_layout_string(l_buf, sizeof(l_buf), 0, l);
                    ADD_ENTRY(list, map, s64, l, str, l_buf);
                }
            }
#endif
        }
    }
    return list;
}


/* TVHCodecProfile ========================================================== */

static int
tvh_codec_profile_audio_is_copy(TVHAudioCodecProfile *self, tvh_ssc_t *ssc)
{
    // TODO: fix me
    // assuming default channel_layout (AV_CH_LAYOUT_STEREO)
    // and sample_rate (48kHz) for input
    int ssc_channels = ssc->es_channels ? ssc->es_channels : 2;
    int ssc_sr = ssc->es_sri ? sri_to_rate(ssc->es_sri) : 48000;
#if LIBAVCODEC_VERSION_MAJOR > 59
    AVChannelLayout ch_layout = {0};
    av_channel_layout_from_mask(&ch_layout, self->channel_layout);
    if ((self->channel_layout && ssc_channels != ch_layout.nb_channels) ||
        (self->sample_rate      && ssc_sr != self->sample_rate)) {
        av_channel_layout_uninit(&ch_layout);
        return 0;
    }
    av_channel_layout_uninit(&ch_layout);
#else
    if ((self->channel_layout &&
         ssc_channels != av_get_channel_layout_nb_channels(self->channel_layout)) ||
        (self->sample_rate && ssc_sr != self->sample_rate)) {
        return 0;
    }
#endif
    return 1;
}


static int
tvh_codec_profile_audio_open(TVHAudioCodecProfile *self, AVDictionary **opts)
{
    if (self->sample_fmt != AV_SAMPLE_FMT_NONE) {
        AV_DICT_SET_INT(opts, "sample_fmt", self->sample_fmt,
                        AV_DICT_DONT_OVERWRITE);
    }
    if (self->sample_rate) {
        AV_DICT_SET_INT(opts, "sample_rate", self->sample_rate,
                        AV_DICT_DONT_OVERWRITE);
    }
    if (self->channel_layout) {
#if LIBAVCODEC_VERSION_MAJOR > 59
        AV_DICT_SET_INT(opts, "ch_layout_u_mask", self->channel_layout, AV_DICT_DONT_OVERWRITE);
#else
        AV_DICT_SET_INT(opts, "channel_layout", self->channel_layout, AV_DICT_DONT_OVERWRITE);
#endif
    }
    return 0;
}


/* codec_profile_audio_class ================================================ */

/* codec_profile_audio_class.language */

static htsmsg_t *
codec_profile_audio_class_language_list(void *obj, const char *lang)
{
    htsmsg_t *list = htsmsg_create_list();
    lang_code_t *lc = (lang_code_t *)lang_codes;
    char lc_buf[128];

    while (lc->code2b) {
        htsmsg_t *map = NULL;
        if (!strcmp(lc->code2b, "und")) {
            map = htsmsg_create_key_val("", tvh_gettext_lang(lang, N_("Use original")));
        }
        else {
            memset(lc_buf, 0, sizeof(lc_buf));
            if (!str_snprintf(lc_buf, sizeof(lc_buf), "%s (%s)", lc->desc, lc->code2b)) {
                map = htsmsg_create_key_val(lc->code2b, lc_buf);
            }
            else {
                htsmsg_destroy(list);
                list = NULL;
                break;
            }
        }
        htsmsg_add_msg(list, NULL, map);
        lc++;
    }
    return list;
}


/* codec_profile_audio_class.sample_fmt */

static uint32_t
codec_profile_audio_class_sample_fmt_get_opts(void *obj, uint32_t opts)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(obj);
    return tvh_codec_audio_get_opts(codec, sample_fmts, opts);
}


static htsmsg_t *
codec_profile_audio_class_sample_fmt_list(void *obj, const char *lang)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(obj);
    return tvh_codec_audio_get_list(codec, sample_fmts);
}


/* codec_profile_audio_class.sample_rate */

static uint32_t
codec_profile_audio_class_sample_rate_get_opts(void *obj, uint32_t opts)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(obj);
    return tvh_codec_audio_get_opts(codec, sample_rates, opts);
}


static htsmsg_t *
codec_profile_audio_class_sample_rate_list(void *obj, const char *lang)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(obj);
    return tvh_codec_audio_get_list(codec, sample_rates);
}


/* codec_profile_audio_class.channel_layout */

static uint32_t
codec_profile_audio_class_channel_layout_get_opts(void *obj, uint32_t opts)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(obj);
    return tvh_codec_audio_get_opts(codec, channel_layouts, opts);
}


static htsmsg_t *
codec_profile_audio_class_channel_layout_list(void *obj, const char *lang)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(obj);
    return tvh_codec_audio_get_list(codec, channel_layouts);
}


/* codec_profile_audio_class */

const codec_profile_class_t codec_profile_audio_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_class,
        .ic_class      = "codec_profile_audio",
        .ic_caption    = N_("audio"),
        .ic_properties = (const property_t[]) {
            {
                .type     = PT_INT,
                .id       = "tracks",
                .name     = N_("Limit audio tracks"),
                .desc     = N_("Use only defined number of audio tracks at maximum."),
                .group    = 2,
                .off      = offsetof(TVHAudioCodecProfile, tracks),
                .def.i    = 1,
            },
            {
                .type     = PT_STR,
                .id       = "language1",
                .name     = N_("1. Language"),
                .desc     = N_("Preferred audio language."),
                .group    = 2,
                .off      = offsetof(TVHAudioCodecProfile, language1),
                .list     = codec_profile_audio_class_language_list,
                .def.s    = "",
            },
            {
                .type     = PT_STR,
                .id       = "language2",
                .name     = N_("2. Language"),
                .desc     = N_("Preferred audio language."),
                .group    = 2,
                .off      = offsetof(TVHAudioCodecProfile, language2),
                .list     = codec_profile_audio_class_language_list,
                .def.s    = "",
            },
            {
                .type     = PT_STR,
                .id       = "language3",
                .name     = N_("3. Language"),
                .desc     = N_("Preferred audio language."),
                .group    = 2,
                .off      = offsetof(TVHAudioCodecProfile, language3),
                .list     = codec_profile_audio_class_language_list,
                .def.s    = "",
            },
            {
                .type     = PT_INT,
                .id       = "sample_fmt",
                .name     = N_("Sample format"),
                .desc     = N_("Audio sample format."),
                .group    = 4,
                .opts     = PO_ADVANCED | PO_PHIDDEN,
                .get_opts = codec_profile_audio_class_sample_fmt_get_opts,
                .off      = offsetof(TVHAudioCodecProfile, sample_fmt),
                .list     = codec_profile_audio_class_sample_fmt_list,
                .def.i    = AV_SAMPLE_FMT_NONE,
            },
            {
                .type     = PT_INT,
                .id       = "sample_rate",
                .name     = N_("Sample rate"),
                .desc     = N_("Samples per second."),
                .group    = 4,
                .opts     = PO_ADVANCED | PO_PHIDDEN,
                .get_opts = codec_profile_audio_class_sample_rate_get_opts,
                .off      = offsetof(TVHAudioCodecProfile, sample_rate),
                .list     = codec_profile_audio_class_sample_rate_list,
                .def.i    = 0,
            },
            {
                .type     = PT_S64,
                .id       = "channel_layout",
                .name     = N_("Channel layout"),
                .desc     = N_("Audio channel layout."),
                .group    = 4,
                .opts     = PO_ADVANCED | PO_PHIDDEN,
                .get_opts = codec_profile_audio_class_channel_layout_get_opts,
                .off      = offsetof(TVHAudioCodecProfile, channel_layout),
                .list     = codec_profile_audio_class_channel_layout_list,
                .def.s64  = 0,
            },
            {}
        }
    },
    .is_copy = (codec_profile_is_copy_meth)tvh_codec_profile_audio_is_copy,
    .open    = (codec_profile_open_meth)tvh_codec_profile_audio_open,
};
