/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2008 Andreas Öman
 *
 * Functions for storing program settings
 */

#ifndef HTSSETTINGS_H__
#define HTSSETTINGS_H__

#include <stdarg.h>

#include "htsmsg.h"

#define HTS_SETTINGS_OPEN_WRITE		(1<<0)
#define HTS_SETTINGS_OPEN_DIRECT	(1<<1)

void hts_settings_init(const char *confpath);

void hts_settings_done(void);

void hts_settings_save(htsmsg_t *record, const char *pathfmt, ...);

htsmsg_t *hts_settings_load(const char *pathfmt, ...);

htsmsg_t *hts_settings_load_r(int depth, const char *pathfmt, ...);

void hts_settings_remove(const char *pathfmt, ...);

const char *hts_settings_get_root(void);

int hts_settings_open_file(int flags, const char *pathfmt, ...);

int hts_settings_buildpath(char *dst, size_t dstsize, const char *pathfmt, ...);

int hts_settings_makedirs ( const char *path );

int hts_settings_exists ( const char *pathfmt, ... );

char *hts_settings_get_xdg_dir_lookup (const char *name);
char *hts_settings_get_xdg_dir_with_fallback (const char *name, const char *fallback);

#endif /* HTSSETTINGS_H__ */
