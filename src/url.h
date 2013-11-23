/*
 *  Tvheadend - URL Processing
 *
 *  Copyright (C) 2013 Adam Sutton
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

#ifndef __TVH_URL_H__
#define __TVH_URL_H__

#include <stdint.h>

// TODO: might be better to find a lib to do this!

/* URL structure */
typedef struct url
{
  char      scheme[16];
  char      user[128];
  char      pass[128];
  char      host[256];
  uint16_t  port;
  char      path[256];
  char      raw[1024];
} url_t;

/* URL regexp - I probably found this online */
#define UC "[a-z0-9_\\-\\.!£$%^&]"
#define PC UC
#define HC "[a-z0-9\\-\\.]"
#define URL_RE "^(\\w+)://(("UC"+)(:("PC"+))?@)?("HC"+)(:([0-9]+))?(/.*)?"

int urlparse ( const char *str, url_t *url );

#endif
