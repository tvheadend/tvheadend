/*
 *  Functions for storing program settings
 *  Copyright (C) 2008 Andreas Öman
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

#ifndef HTSSETTINGS_H__
#define HTSSETTINGS_H__

#include <stdarg.h>

#include "htsmsg.h"

void hts_settings_init(const char *confpath);

void hts_settings_done(void);

void hts_settings_save(htsmsg_t *record, const char *pathfmt, ...);

htsmsg_t *hts_settings_load(const char *pathfmt, ...);

htsmsg_t *hts_settings_load_r(int depth, const char *pathfmt, ...);

void hts_settings_remove(const char *pathfmt, ...);

const char *hts_settings_get_root(void);

int hts_settings_open_file(int for_write, const char *pathfmt, ...);

int hts_settings_buildpath(char *dst, size_t dstsize, const char *pathfmt, ...);

int hts_settings_makedirs ( const char *path );

int hts_settings_exists ( const char *pathfmt, ... );

#endif /* HTSSETTINGS_H__ */ 
