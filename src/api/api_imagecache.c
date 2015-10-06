/*
 *  API - Imagecache related calls
 *
 *  Copyright (C) 2013 Adam Sutton
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; withm even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tvheadend.h"
#include "channels.h"
#include "access.h"
#include "api.h"
#include "imagecache.h"

#if ENABLE_IMAGECACHE

static int
api_imagecache_clean
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int b;
  if (htsmsg_get_bool(args, "clean", &b))
    return EINVAL;
  if (b) {
    pthread_mutex_lock(&global_lock);
    imagecache_clean();
    pthread_mutex_unlock(&global_lock);
  }
  return 0;
}

static int
api_imagecache_trigger
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int b;
  if (htsmsg_get_bool(args, "trigger", &b))
    return EINVAL;
  if (b) {
    pthread_mutex_lock(&global_lock);
    imagecache_trigger();
    pthread_mutex_unlock(&global_lock);
  }
  return 0;
}

void
api_imagecache_init ( void )
{
  static api_hook_t ah[] = {
    { "imagecache/config/load",    ACCESS_ADMIN, api_idnode_load_simple, &imagecache_conf },
    { "imagecache/config/save",    ACCESS_ADMIN, api_idnode_save_simple, &imagecache_conf },
    { "imagecache/config/clean"  , ACCESS_ADMIN, api_imagecache_clean, NULL },
    { "imagecache/config/trigger", ACCESS_ADMIN, api_imagecache_trigger, NULL },
    { NULL },
  };

  api_register_all(ah);
}

#else /* ENABLE_IMAGECACHE */

void 
api_imagecache_init ( void )
{
}

#endif
