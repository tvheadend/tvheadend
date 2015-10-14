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
#include <string.h>

/* URL structure */
typedef struct url
{
  char  *scheme;
  char  *user;
  char  *pass;
  char  *host;
  int    port;
  char  *path;
  char  *query;
  char  *frag;
  char  *raw;
} url_t;

static inline void urlinit ( url_t *url ) { memset(url, 0, sizeof(*url)); }
void urlreset ( url_t *url );
int urlparse ( const char *str, url_t *url );
void urlparse_done ( void );
void urlcopy ( url_t *dst, const url_t *src );

#endif
