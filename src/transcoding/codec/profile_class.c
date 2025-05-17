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

#include "access.h"


/* TVHCodec ================================================================= */

uint32_t
tvh_codec_base_get_opts(TVHCodec *self, uint32_t opts, int visible)
{
    if (!tvh_codec_is_enabled(self)) {
        opts |= PO_RDONLY;
    }
    else if (visible) {
        opts &= ~(PO_PHIDDEN);
    }
    return opts;
}


htsmsg_t *
tvh_codec_get_list_profiles(TVHCodec *self)
{
    htsmsg_t *list = NULL, *map = NULL;
    const AVProfile *profiles = self->profiles;
    AVProfile *p = NULL;

    if (profiles && (list = htsmsg_create_list())) {
        if (!(map = htsmsg_create_map())) {
            htsmsg_destroy(list);
            list = NULL;
        }
        else {
            ADD_ENTRY(list, map, s32, FF_AV_PROFILE_UNKNOWN, str, AUTO_STR);
            for (p = (AVProfile *)profiles; p->profile != FF_AV_PROFILE_UNKNOWN; p++) {
                if (!(map = htsmsg_create_map())) {
                    htsmsg_destroy(list);
                    list = NULL;
                    break;
                }
                ADD_ENTRY(list, map, s32, p->profile, str, p->name);
            }
        }
    }
    return list;
}


/* TVHCodecProfile ========================================================== */

static int
tvh_codec_profile_base_is_copy(TVHCodecProfile *self, tvh_ssc_t *ssc)
{
    return (!(self->bit_rate || self->qscale));
}


static int
tvh_codec_profile_base_open(TVHCodecProfile *self, AVDictionary **opts)
{
    AV_DICT_SET_TVH_REQUIRE_META(opts, 1);
    // profile
    if (self->profile != FF_AV_PROFILE_UNKNOWN) {
        AV_DICT_SET_INT(opts, "profile", self->profile, 0);
    }
    return 0;
}


/* codec_profile_class ====================================================== */

static htsmsg_t *
codec_profile_class_save(idnode_t *idnode, char *filename, size_t fsize)
{
    htsmsg_t *map = htsmsg_create_map();
    char uuid[UUID_HEX_SIZE];
    idnode_save(idnode, map);
    if (filename)
      snprintf(filename, fsize, "codec/%s", idnode_uuid_as_str(idnode, uuid));
    return map;
}


static void
codec_profile_class_delete(idnode_t *idnode)
{
    tvh_codec_profile_remove((TVHCodecProfile *)idnode, 1);
}


/* codec_profile_class.name */

static int
codec_profile_class_name_set(void *obj, const void *val)
{
    TVHCodecProfile *self = (TVHCodecProfile *)obj, *other = NULL;
    const char *name = (const char *)val;

    if (self) {
        if (name && name[0] != '\0' && strcmp(name, "copy") &&
            strlen(name) < TVH_NAME_LEN &&
            strcmp(name, self->name ? self->name : "")) {
            if ((other = tvh_codec_profile_find(name)) && other != self) {
                tvherror(LS_CODEC, "profile '%s' already exists", name);
            }
            else {
                free(self->name);
                self->name = strdup(name);
                return 1;
            }
        }
    }
    return 0;
}


/* codec_profile_class.name */

static const void *
codec_profile_class_name_get(void *obj)
{
    static const char *type;

    TVHCodec *codec = tvh_codec_profile_get_codec(obj);
    type = codec ? tvh_codec_get_title(codec) : "<unknown>";
    return &type;
}


/* codec_profile_class.type */

static const void *
codec_profile_class_type_get(void *obj)
{
    static const char *type;

    TVHCodec *codec = tvh_codec_profile_get_codec(obj);
    type = codec ? tvh_codec_get_type_string(codec) : "<unknown>";
    return &type;
}


/* codec_profile_class.enabled */

static const void *
codec_profile_class_enabled_get(void *obj)
{
    static int enabled;

    TVHCodec *codec = tvh_codec_profile_get_codec(obj);
    enabled = codec ? tvh_codec_is_enabled(codec) : 0;
    return &enabled;
}


/* codec_profile_class.profile */

static htsmsg_t *
codec_profile_class_profile_list(void *obj, const char *lang)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(obj);
    return tvh_codec_get_list(codec, profiles);
}


/* codec_profile_class */
CLASS_DOC(codec_profile)
const codec_profile_class_t codec_profile_class = {
    {
        .ic_class    = "codec_profile",
        .ic_caption  = N_("Stream - Codec Profiles"),
        .ic_event    = "codec_profile",
        .ic_perm_def = ACCESS_ADMIN,
        .ic_doc        = tvh_doc_codec_profile_class,
        .ic_save     = codec_profile_class_save,
        .ic_delete   = codec_profile_class_delete,
        .ic_groups   = (const property_group_t[]) {
            {
                .name   = N_("General Settings"),
                .number = 1,
            },
            {
                .name   = N_("Profile Settings"),
                .number = 2,
            },
            {
                .name   = N_("Codec Settings"),
                .number = 3,
            },
            {
                .name   = N_("Advanced Settings"),
                .number = 4,
            },
            {
                .name   = N_("Expert Settings"),
                .number = 5,
            },
            {}
        },
        .ic_properties = (const property_t[]) {
            {
                .type     = PT_STR,
                .id       = "name",
                .name     = N_("Name"),
                .desc     = N_("Name."),
                .group    = 1,
                .off      = offsetof(TVHCodecProfile, name),
                .set      = codec_profile_class_name_set,
            },
            {
                .type     = PT_STR,
                .id       = "description",
                .name     = N_("Description"),
                .desc     = N_("Profile description."),
                .group    = 1,
                .off      = offsetof(TVHCodecProfile, description),
            },
            {
                .type     = PT_STR,
                .id       = "codec_title",
                .name     = N_("Codec"),
                .desc     = N_("Codec title."),
                .group    = 1,
                .opts     = PO_RDONLY | PO_NOSAVE,
                .get      = codec_profile_class_name_get,
            },
            {
                .type     = PT_STR,
                .id       = "codec_name",
                .name     = N_("Codec name"),
                .desc     = N_("Codec name."),
                .group    = 1,
                .opts     = PO_RDONLY,
                .off      = offsetof(TVHCodecProfile, codec_name),
            },
            {
                .type     = PT_STR,
                .id       = "type",
                .name     = N_("Type"),
                .desc     = N_("Codec type."),
                .group    = 1,
                .opts     = PO_RDONLY | PO_NOSAVE,
                .get      = codec_profile_class_type_get,
            },
            {
                .type     = PT_BOOL,
                .id       = "enabled",
                .name     = N_("Enabled"),
                .desc     = N_("Codec status."),
                .group    = 1,
                .opts     = PO_RDONLY | PO_NOSAVE,
                .get      = codec_profile_class_enabled_get,
            },
            {
                .type     = PT_INT,
                .id       = "profile",
                .name     = N_("Profile"),
                .desc     = N_("Profile."),
                .group    = 4,
                .opts     = PO_ADVANCED | PO_PHIDDEN,
                .get_opts = codec_profile_class_profile_get_opts,
                .off      = offsetof(TVHCodecProfile, profile),
                .list     = codec_profile_class_profile_list,
                .def.i    = FF_AV_PROFILE_UNKNOWN,
            },
            {}
        }
    },
    .is_copy = tvh_codec_profile_base_is_copy,
    .open    = tvh_codec_profile_base_open,
};


/* exposed */

uint32_t
codec_profile_class_get_opts(void *obj, uint32_t opts)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(obj);
    return codec ? tvh_codec_base_get_opts(codec, opts, 0) : opts;
}


uint32_t
codec_profile_class_profile_get_opts(void *obj, uint32_t opts)
{
    TVHCodec *codec = tvh_codec_profile_get_codec(obj);
    return tvh_codec_get_opts(codec, profiles, opts);
}
