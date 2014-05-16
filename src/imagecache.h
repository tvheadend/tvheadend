/*
 *  Icon file serve operations
 *  Copyright (C) 2012 Andy Brown
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

#ifndef __IMAGE_CACHE_H__
#define __IMAGE_CACHE_H__

#include <pthread.h>

struct imagecache_config {
  int       enabled;
  int       ignore_sslcert;
  uint32_t  ok_period;
  uint32_t  fail_period;
};

extern struct imagecache_config imagecache_conf;

extern pthread_mutex_t imagecache_mutex;

void     imagecache_init     ( void );
void	 imagecache_done     ( void );

htsmsg_t *imagecache_get_config ( void );
int       imagecache_set_config ( htsmsg_t *c );
void      imagecache_save       ( void );

// Note: will return 0 if invalid (must serve original URL)
uint32_t imagecache_get_id  ( const char *url );

int      imagecache_open    ( uint32_t id );

#endif /* __IMAGE_CACHE_H__ */
