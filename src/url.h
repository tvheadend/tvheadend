/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Adam Sutton
 *
 * Tvheadend - URL Processing
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
int urlrecompose ( url_t *url );

#endif
