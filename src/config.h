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

// TODO: expand this, possibly integrate command line

#ifndef __TVH_CONFIG__H__
#define __TVH_CONFIG__H__

#include <unistd.h>

#include "build.h"
#include "htsmsg.h"
#include "idnode.h"

typedef struct config {
  idnode_t idnode;
  uint32_t version;
  char *full_version;
  char *server_name;
  char *language;
  char *muxconf_path;
  int prefer_picon;
  char *chicon_path;
  char *picon_path;
  int tvhtime_update_enabled;
  int tvhtime_ntp_enabled;
  uint32_t tvhtime_tolerance;
  char *cors_origin;
} config_t;

extern const idclass_t config_class;
extern config_t config;

void        config_boot    ( const char *path, gid_t gid, uid_t uid );
void        config_init    ( int backup );
void        config_done    ( void );
void        config_save    ( void );

const char *config_get_server_name ( void );
const char *config_get_language    ( void );

#endif /* __TVH_CONFIG__H__ */
