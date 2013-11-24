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

// TODO: limits are a bit arbitrary and it's a bit inflexible, but it
//       does keep things simple, not having dynamically allocated strings

/* URL structure */
typedef struct url
{
  char  scheme[32];
  char  user[128];
  char  pass[128];
  char  host[256];
  short port;
  char  path[256];
  char  query[1024];
  char  frag[256];
  char  raw[2048];
} url_t;

int urlparse ( const char *str, url_t *url );

#endif
