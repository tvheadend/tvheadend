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

#include "settings.h"


struct TVHCodecProfiles tvh_codec_profiles;


/* TVHCodecProfile ========================================================== */

static TVHCodecProfile *
tvh_codec_profile_alloc(TVHCodec *codec, htsmsg_t *conf)
{
    TVHCodecProfile *self = NULL;

    if ((self = calloc(1, codec->size))) {
        self->codec = codec;
        if (codec->profile_init) {
            if (codec->profile_init(self, conf)) {
                free(self);
                return NULL;
            }
        }
    }
    return self;
}


static void
tvh_codec_profile_free(TVHCodecProfile *self)
{
    TVHCodec *codec;

    if (self) {
        codec = self->codec;
        if (codec && codec->profile_destroy) {
            codec->profile_destroy(self);
        }
        free(self->name);
        free(self->description);
        free(self->codec_name);
        free(self->device);
        free(self);
    }
}


static void
tvh_codec_profile_load(htsmsg_field_t *config)
{
    htsmsg_t *conf = NULL;
    const char *name = NULL;

    if ((conf = htsmsg_field_get_map(config)) &&
        tvh_codec_profile_create(conf, config->hmf_name, 0)) {
        tvherror(LS_CODEC, "unable to load codec profile: '%s'",
                 (name = htsmsg_get_str(conf, "name")) ? name : "<unknown>");
    }
}


static void
tvh_codec_profile_create2(htsmsg_field_t *config)
{
    htsmsg_t *conf = NULL;
    const char *name = NULL;

    if ((conf = htsmsg_field_get_map(config)) != NULL) {
        name = htsmsg_get_str(conf, "name");
        if (name == NULL)
            return;
        if (tvh_codec_profile_find(name))
            return;
        if (tvh_codec_profile_create(conf, NULL, 1))
          tvherror(LS_CODEC, "unable to create codec profile from config tree: '%s'",
                   (name = htsmsg_get_str(conf, "name")) ? name : "<unknown>");
    }
}


static int
tvh_codec_profile_setup(TVHCodecProfile *self, tvh_ssc_t *ssc)
{
    const idclass_t *idclass = (&self->idnode)->in_class;
    const codec_profile_class_t *codec_profile_class = NULL;
    int ret = 0;

    while (idclass) {
        codec_profile_class = (codec_profile_class_t *)idclass;
        if (codec_profile_class->setup &&
            (ret = codec_profile_class->setup(self, ssc))) {
            break;
        }
        idclass = idclass->ic_super;
    }
    return ret;
}


/* exposed */

int
tvh_codec_profile_create(htsmsg_t *conf, const char *uuid, int save)
{
    const char *profile_name = NULL, *codec_name = NULL;
    TVHCodec *codec = NULL;
    TVHCodecProfile *profile = NULL;

    lock_assert(&global_lock);

    if ((!(profile_name = htsmsg_get_str(conf, "name")) ||
         profile_name[0] == '\0' || !strcmp(profile_name, "copy") ||
         strlen(profile_name) >= TVH_NAME_LEN) ||
        (!(codec_name = htsmsg_get_str(conf, "codec_name")) ||
         codec_name[0] == '\0')) {
        tvherror(LS_CODEC, "missing/empty/wrong 'name' or 'codec_name'");
        return EINVAL;
    }
    if ((profile = tvh_codec_profile_find(profile_name))) {
        tvherror(LS_CODEC, "codec profile '%s' already exists", profile_name);
        return EEXIST;
    }
    if (!(codec = tvh_codec_find(codec_name))) {
        tvherror(LS_CODEC, "codec '%s' not found", codec_name);
        return ENOENT;
    }
    if (!(profile = tvh_codec_profile_alloc(codec, conf))) {
        tvherror(LS_CODEC, "failed to allocate TVHCodecProfile");
        return ENOMEM;
    }
    if (idnode_insert(&profile->idnode, uuid, (idclass_t *)codec->idclass, 0)) {
        tvh_codec_profile_free(profile);
        tvherror(LS_CODEC, "failed to insert idnode");
        return EINVAL;
    }
    idnode_load(&profile->idnode, conf);
    LIST_INSERT_HEAD(&tvh_codec_profiles, profile, link);
    if (save) {
        idnode_changed(&profile->idnode);
    }
    tvhinfo(LS_CODEC, "'%s' codec profile created", profile_name);
    return 0;
}


const char *
tvh_codec_profile_get_status(TVHCodecProfile *self)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(self);

    if (codec && tvh_codec_is_enabled(codec)) {
        return "codecEnabled";
    }
    return "codecDisabled";
}


const char *
tvh_codec_profile_get_name(TVHCodecProfile *self)
{
    return self->name;
}


const char *
tvh_codec_profile_get_title(TVHCodecProfile *self)
{
    static char __thread profile_title[TVH_TITLE_LEN];

    memset(profile_title, 0, sizeof(profile_title));
    if (str_snprintf(profile_title, sizeof(profile_title),
            (self->description && strcmp(self->description, "")) ? "%s (%s)" : "%s%s",
            self->name, self->description ? self->description : "")) {
        return NULL;
    }
    return profile_title;
}


AVCodec *
tvh_codec_profile_get_avcodec(TVHCodecProfile *self)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(self);
    return codec ? tvh_codec_get_codec(codec) : NULL;
}


/* transcode api */
int
tvh_codec_profile_is_copy(TVHCodecProfile *self, tvh_ssc_t *ssc)
{
    const idclass_t *idclass = NULL;
    const codec_profile_class_t *codec_profile_class = NULL;
    AVCodec *avcodec = NULL;
    tvh_sct_t out_type = SCT_UNKNOWN;


    if (tvh_codec_profile_setup(self, ssc)) {
        return -1;
    }
    if (!(avcodec = tvh_codec_profile_get_avcodec(self))) {
        tvherror(LS_CODEC, "profile '%s' is disabled", self->name);
        return -1;
    }
    if ((out_type = codec_id2streaming_component_type(avcodec->id)) == SCT_UNKNOWN) {
        tvherror(LS_CODEC, "unknown type for profile '%s'", self->name);
        return -1;
    }
    if (out_type == ssc->ssc_type) {
        idclass = (&self->idnode)->in_class;
        while (idclass) {
            codec_profile_class = (codec_profile_class_t *)idclass;
            if (codec_profile_class->is_copy &&
                !codec_profile_class->is_copy(self, ssc)) {
                return 0;
            }
            idclass = idclass->ic_super;
        }
        return 1;
    }
    return 0;
}


int
tvh_codec_profile_open(TVHCodecProfile *self, AVDictionary **opts)
{
    const idclass_t *idclass = (&self->idnode)->in_class;
    const codec_profile_class_t *codec_profile_class = NULL;
    int ret = 0;

    while (idclass) {
        codec_profile_class = (codec_profile_class_t *)idclass;
        if (codec_profile_class->open &&
            (ret = codec_profile_class->open(self, opts))) {
            break;
        }
        idclass = idclass->ic_super;
    }
    return ret;
}


/* video */
int
tvh_codec_profile_video_init(TVHCodecProfile *_self, htsmsg_t *conf)
{
    TVHVideoCodecProfile *self = (TVHVideoCodecProfile *)_self;
    self->pix_fmt = AV_PIX_FMT_NONE;
    return 0;
}

void
tvh_codec_profile_video_destroy(TVHCodecProfile *_self)
{
}

int
tvh_codec_profile_video_get_hwaccel(TVHCodecProfile *self)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(self);
    if (codec && tvh_codec_is_enabled(codec) &&
        tvh_codec_get_type(codec) == AVMEDIA_TYPE_VIDEO) {
        return ((TVHVideoCodecProfile *)self)->hwaccel;
    }
    return -1;
}

const enum AVPixelFormat *
tvh_codec_profile_video_get_pix_fmts(TVHCodecProfile *self)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(self);
    return tvh_codec_video_getattr(codec, pix_fmts);
}


/* audio */
int
tvh_codec_profile_audio_init(TVHCodecProfile *_self, htsmsg_t *conf)
{
    TVHAudioCodecProfile *self = (TVHAudioCodecProfile *)_self;
    self->sample_fmt = AV_SAMPLE_FMT_NONE;
    self->tracks = 1;
    return 0;
}

void
tvh_codec_profile_audio_destroy(TVHCodecProfile *_self)
{
    TVHAudioCodecProfile *self = (TVHAudioCodecProfile *)_self;
    free(self->language1);
    free(self->language2);
    free(self->language3);
}

const enum AVSampleFormat *
tvh_codec_profile_audio_get_sample_fmts(TVHCodecProfile *self)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(self);
    return tvh_codec_audio_getattr(codec, sample_fmts);
}

const int *
tvh_codec_profile_audio_get_sample_rates(TVHCodecProfile *self)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(self);
    return tvh_codec_audio_getattr(codec, sample_rates);
}

const uint64_t *
tvh_codec_profile_audio_get_channel_layouts(TVHCodecProfile *self)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(self);
    return tvh_codec_audio_getattr(codec, channel_layouts);
}


/* internal api */
void
tvh_codec_profile_remove(TVHCodecProfile *self, int delete)
{
    char uuid[UUID_HEX_SIZE];

    memset(uuid, 0, sizeof(uuid));
    idnode_save_check(&self->idnode, delete);
    if (delete) {
        hts_settings_remove("codec/%s", idnode_uuid_as_str(&self->idnode, uuid));
    }
    LIST_REMOVE(self, link);
    idnode_unlink(&self->idnode);
    tvh_codec_profile_free(self);
}


TVHCodec *
tvh_codec_profile_get_codec(TVHCodecProfile *self)
{
    return self ? self->codec : NULL;
}


TVHCodecProfile *
tvh_codec_profile_find(const char *name)
{
    TVHCodecProfile *profile = NULL;

    LIST_FOREACH(profile, &tvh_codec_profiles, link) {
        if (!strcmp(profile->name, name)) {
            return profile;
        }
    }
    return NULL;
}


void
tvh_codec_profiles_load()
{
    htsmsg_t *settings = NULL;
    htsmsg_field_t *config = NULL;

    LIST_INIT(&tvh_codec_profiles);
    if ((settings = hts_settings_load("codec"))) {
        HTSMSG_FOREACH(config, settings) {
            tvh_codec_profile_load(config);
        }
        htsmsg_destroy(settings);
    }
    if ((settings = hts_settings_load("transcoder/codecs"))) {
        HTSMSG_FOREACH(config, settings) {
            tvh_codec_profile_create2(config);
        }
        htsmsg_destroy(settings);
    }
}


void
tvh_codec_profiles_remove()
{
    tvhinfo(LS_CODEC, "removing codec profiles");
    while (!LIST_EMPTY(&tvh_codec_profiles)) {
        tvh_codec_profile_remove(LIST_FIRST(&tvh_codec_profiles), 0);
    }
}
