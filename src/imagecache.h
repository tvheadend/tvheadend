/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2012 Andy Brown
 *
 * Icon file serve operations
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
