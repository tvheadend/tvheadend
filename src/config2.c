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

void config_init ( void )
{
  int save = 0;
  uint32_t u32;

  config = hts_settings_load("config");
  if (!config) {
    tvhlog(LOG_DEBUG, "config", "no configuration, loading defaults");
    config = htsmsg_create_map();
  }

  /* Defaults */
  if (htsmsg_get_u32(config, "timeshiftperiod", &u32))
    save |= config_set_timeshift_period(0);
  if (htsmsg_get_u32(config, "timeshiftsize", &u32))
    save |= config_set_timeshift_size(0);

  /* Save defaults */
  if (save)
    config_save();
}

void config_save ( void )
{
  hts_settings_save(config, "config");
}

htsmsg_t *config_get_all ( void )
{
  return htsmsg_copy(config);
}

static int _config_set_str ( const char *fld, const char *val )
{
  const char *c = htsmsg_get_str(config, fld);
  if (!c || strcmp(c, val)) {
    if (c) htsmsg_delete_field(config, fld);
    htsmsg_add_str(config, fld, val);
    return 1;
  }
  return 0;
}

static int _config_set_u32 ( const char *fld, uint32_t val )
{
  uint32_t u32;
  int ret = htsmsg_get_u32(config, fld, &u32);
  if (ret || (u32 != val)) {
    if (!ret) htsmsg_delete_field(config, fld);
    htsmsg_add_u32(config, fld, val);
    return 1;
  }
  return 0;
}

const char *config_get_language ( void )
{
  return htsmsg_get_str(config, "language");
}

int config_set_language ( const char *lang )
{
  return _config_set_str("language", lang);
}

const char *config_get_muxconfpath ( void )
{
  return htsmsg_get_str(config, "muxconfpath");
}

int config_set_muxconfpath ( const char *path )
{
  return _config_set_str("muxconfpath", path);
}

const char *config_get_timeshift_path ( void )
{
  return htsmsg_get_str(config, "timeshiftpath");
}

int config_set_timeshift_path ( const char *path )
{
  return _config_set_str("timeshiftpath", path);
}

uint32_t config_get_timeshift_period ( void )
{
  return htsmsg_get_u32_or_default(config, "timeshiftperiod", 0);
}

int config_set_timeshift_period ( uint32_t period )
{
  return _config_set_u32("timeshiftperiod", period);
}

uint32_t config_get_timeshift_size ( void )
{
  return htsmsg_get_u32_or_default(config, "timeshiftsize", 0);
}

int config_set_timeshift_size ( uint32_t size )
{
  return _config_set_u32("timeshiftsize", size);
}
