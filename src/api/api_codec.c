/*
 *  tvheadend - API access to Codec Profiles
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


#include "tvheadend.h"
#include "access.h"
#include "api.h"

#include "transcoding/codec.h"


static int
api_codec_list(access_t *perm, void *opaque, const char *op,
               htsmsg_t *args, htsmsg_t **resp)
{
    htsmsg_t *list = NULL, *map = NULL;
    TVHCodec *tvh_codec = NULL;
    int err = ENOMEM;

    if ((list = htsmsg_create_list())) {
        tvh_mutex_lock(&global_lock);
        SLIST_FOREACH(tvh_codec, &tvh_codecs, link) {
            if (!(map = idclass_serialize(tvh_codec_get_class(tvh_codec),
                                          perm->aa_lang_ui))) {
                htsmsg_destroy(list);
                list = NULL;
                break;
            }
            htsmsg_add_str(map, "name", tvh_codec_get_name(tvh_codec));
            htsmsg_add_str(map, "title", tvh_codec_get_title(tvh_codec));
            htsmsg_add_msg(list, NULL, map);
        }
        tvh_mutex_unlock(&global_lock);
        if (list) {
            if ((*resp = htsmsg_create_map())) {
                htsmsg_add_msg(*resp, "entries", list);
                err = 0;
            }
            else {
                htsmsg_destroy(list);
                list = NULL;
            }
        }
    }
    return err;
}


static int
api_codec_profile_list(access_t *perm, void *opaque, const char *op,
                       htsmsg_t *args, htsmsg_t **resp)
{
    htsmsg_t *list = NULL, *map = NULL;
    TVHCodecProfile *tvh_profile = NULL;
    char buf[UUID_HEX_SIZE];
    int err = ENOMEM;

    if ((list = htsmsg_create_list())) {
        tvh_mutex_lock(&global_lock);
        LIST_FOREACH(tvh_profile, &tvh_codec_profiles, link) {
            if (!(map = htsmsg_create_map())) {
                htsmsg_destroy(list);
                list = NULL;
                break;
            }
            htsmsg_add_str(map, "uuid",
                           idnode_uuid_as_str((idnode_t *)tvh_profile, buf));
            htsmsg_add_str(map, "title",
                           tvh_codec_profile_get_title(tvh_profile));
            htsmsg_add_str(map, "status",
                           tvh_codec_profile_get_status(tvh_profile));
            htsmsg_add_msg(list, NULL, map);
        }
        tvh_mutex_unlock(&global_lock);
        if (list) {
            if ((*resp = htsmsg_create_map())) {
                htsmsg_add_msg(*resp, "entries", list);
                err = 0;
            }
            else {
                htsmsg_destroy(list);
                list = NULL;
            }
        }
    }
    return err;
}


static int
api_codec_profile_create(access_t *perm, void *opaque, const char *op,
                         htsmsg_t *args, htsmsg_t **resp)
{
    const char *codec_name = NULL;
    htsmsg_t *conf = NULL;
    int err = EINVAL;

    if ((codec_name = htsmsg_get_str(args, "class")) &&
        (conf = htsmsg_get_map(args, "conf"))) {
        htsmsg_set_str(conf, "codec_name", codec_name);
        tvh_mutex_lock(&global_lock);
        err = (err = tvh_codec_profile_create(conf, NULL, 1)) ? -err : 0;
        tvh_mutex_unlock(&global_lock);
    }
    return err;
}


void
api_codec_init(void)
{
    static api_hook_t ah[] = {
        {"codec/list", ACCESS_ADMIN, api_codec_list, NULL},
        {"codec_profile/list", ACCESS_ANONYMOUS, api_codec_profile_list, NULL},
        {"codec_profile/create", ACCESS_ADMIN, api_codec_profile_create, NULL},
        {"codec_profile/class", ACCESS_ADMIN, api_idnode_class,
         (void*)&codec_profile_class},
        {NULL},
    };

    api_register_all(ah);
}
