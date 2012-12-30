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

extern uint32_t imagecache_enabled;
extern uint32_t imagecache_ok_period;
extern uint32_t imagecache_fail_period;

extern pthread_mutex_t imagecache_mutex;

void     imagecache_init     ( void );

void     imagecache_save     ( void );

int      imagecache_set_enabled     ( uint32_t e )
  __attribute__((warn_unused_result));
int      imagecache_set_ok_period   ( uint32_t e )
  __attribute__((warn_unused_result));
int      imagecache_set_fail_period ( uint32_t e )
  __attribute__((warn_unused_result));

// Note: will return 0 if invalid (must serve original URL)
uint32_t imagecache_get_id  ( const char *url );

int      imagecache_open    ( uint32_t id );

#define htsmsg_add_imageurl(_msg, _fld, _fmt, _url)\
  {\
    char _tmp[64];\
    uint32_t _id = imagecache_get_id(_url);\
    if (_id) {\
      snprintf(_tmp, sizeof(_tmp), _fmt, _id);\
    } else {\
      htsmsg_add_str(_msg, _fld, _url);\
    }\
  }

#endif /* __IMAGE_CACHE_H__ */
