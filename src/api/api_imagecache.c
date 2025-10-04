/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Adam Sutton
 *
 * API - Imagecache related calls
 */

#include "tvheadend.h"
#include "channels.h"
#include "access.h"
#include "api.h"
#include "imagecache.h"

static int
api_imagecache_clean
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int b;
  if (htsmsg_get_bool(args, "clean", &b))
    return EINVAL;
  if (b)
    imagecache_clean();
  return 0;
}

static int
api_imagecache_trigger
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int b;
  if (htsmsg_get_bool(args, "trigger", &b))
    return EINVAL;
  if (b)
    imagecache_trigger();
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
