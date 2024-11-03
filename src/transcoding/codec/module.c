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


/* module level ============================================================= */

TVHCodecProfile *
codec_find_profile(const char *name)
{
    return name ? tvh_codec_profile_find(name) : NULL;
}


htsmsg_t *
codec_get_profiles_list(enum AVMediaType media_type)
{
    htsmsg_t *list = NULL, *map = NULL;
    TVHCodecProfile *profile = NULL;
    TVHCodec *codec = NULL;


    if ((list = htsmsg_create_list())) {
        LIST_FOREACH(profile, &tvh_codec_profiles, link) {
            if ((codec = tvh_codec_profile_get_codec(profile)) &&
                tvh_codec_get_type(codec) == media_type) {
                if (!(map = htsmsg_create_map())) {
                    htsmsg_destroy(list);
                    list = NULL;
                    break;
                }
                ADD_ENTRY(list, map,
                          str, tvh_codec_profile_get_name(profile),
                          str, tvh_codec_profile_get_title(profile));
            }
        }
    }
    return list;
}


void
#if ENABLE_VAAPI_OLD || ENABLE_VAAPI
codec_init(int vainfo_probe_enabled)
#else
codec_init(void)
#endif
{
    // codecs
#if ENABLE_VAAPI_OLD || ENABLE_VAAPI
    tvh_codecs_register(vainfo_probe_enabled);
#else
    tvh_codecs_register();
#endif
    // codec profiles
    tvh_codec_profiles_load();
}


void
codec_done(void)
{
    tvh_mutex_lock(&global_lock);

    // codec profiles
    tvh_codec_profiles_remove();
    // codecs
    tvh_codecs_forget();

    tvh_mutex_unlock(&global_lock);
}
