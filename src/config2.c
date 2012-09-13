/*
 *  TV headend - General configuration settings
 *  Copyright (C) 2012 Adam Sutton
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
#include "settings.h"
#include "config2.h"

#include <string.h>

static htsmsg_t *config;

static const char *PATHFMT = "config";
static const char *KEY_LANGUAGE = "language";
static const char *KEY_MUXCONFPATH = "muxconfpath";
static const char *KEY_FILTER_AUDIO_SUBTITLE = "filteraudiosubtitle";

void config_init ( void )
{
  config = hts_settings_load(PATHFMT);
  if (!config) {
    tvhlog(LOG_WARNING, PATHFMT, "no configuration, loading defaults");
    config = htsmsg_create_map();
    htsmsg_add_str(config, KEY_LANGUAGE, "");
    htsmsg_add_str(config, KEY_MUXCONFPATH, "");
    htsmsg_add_str(config, KEY_FILTER_AUDIO_SUBTITLE, "false");
  }
}

void config_save ( void )
{
  hts_settings_save(config, PATHFMT);
}

htsmsg_t *config_get_all ( void )
{
  return htsmsg_copy(config);
}

const char *config_get_language ( void )
{
  return htsmsg_get_str(config, KEY_LANGUAGE);
}

int config_set_language ( const char *lang )
{
  const char *c = config_get_language();
  if (!c || strcmp(c, lang)) {
    if (c) htsmsg_delete_field(config, KEY_LANGUAGE);
    htsmsg_add_str(config, KEY_LANGUAGE, lang);
    return 1;
  }
  return 0;
}

const char *config_get_muxconfpath ( void )
{
  return htsmsg_get_str(config, KEY_MUXCONFPATH);
}

int config_set_muxconfpath ( const char *path )
{
  const char *c = config_get_muxconfpath();
  if (!c || strcmp(c, path)) {
    if (c) htsmsg_delete_field(config, KEY_MUXCONFPATH);
    htsmsg_add_str(config, KEY_MUXCONFPATH, path);
    return 1;
  }
  return 0;
}

const char *config_get_filteraudiosubtitle ( void )
{
  return htsmsg_get_str(config, KEY_FILTER_AUDIO_SUBTITLE);
}

int config_set_filteraudiosubtitle ( const char *path )
{
  const char *c = config_get_muxconfpath();
  if (!c || strcmp(c, path)) {
    if (c) htsmsg_delete_field(config, KEY_FILTER_AUDIO_SUBTITLE);
    htsmsg_add_str(config, KEY_FILTER_AUDIO_SUBTITLE, path);
    return 1;
  }
  return 0;
}
