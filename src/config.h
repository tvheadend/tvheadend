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

#include "htsmsg.h"

void        config_init    ( const char *path );
void        config_done    ( void );
void        config_save    ( void );

htsmsg_t   *config_get_all ( void );

const char *config_get_muxconfpath ( void );
int         config_set_muxconfpath ( const char *str )
  __attribute__((warn_unused_result));

const char *config_get_language    ( void );
int         config_set_language    ( const char *str )
  __attribute__((warn_unused_result));

const char *config_get_picon_path  ( void );
int         config_set_picon_path  ( const char *str )
  __attribute__((warn_unused_result));

#endif /* __TVH_CONFIG__H__ */
