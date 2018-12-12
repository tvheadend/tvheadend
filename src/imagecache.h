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

#include "tvh_thread.h"
#include "idnode.h"

struct imagecache_config {
  idnode_t  idnode;
  int       enabled;
  int       ignore_sslcert;
  uint32_t  expire;
  uint32_t  ok_period;
  uint32_t  fail_period;
};

extern struct imagecache_config imagecache_conf;
extern const idclass_t imagecache_class;

extern tvh_mutex_t imagecache_mutex;

void imagecache_init     ( void );
void imagecache_done     ( void );

void imagecache_clean    ( void );
void imagecache_trigger  ( void );

// Note: will return 0 if invalid (must serve original URL)
int imagecache_get_id  ( const char *url );

const char *imagecache_get_propstr ( const char *image, char *buf, size_t buflen );

int imagecache_filename ( int id, char *name, size_t len );

#endif /* __IMAGE_CACHE_H__ */
