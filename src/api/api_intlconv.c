/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2014 Jaroslav Kysela
 *
 * API - international character conversions
 */

#ifndef __TVH_API_INTLCONV_H__
#define __TVH_API_INTLCONV_H__

#include "tvheadend.h"
#include "access.h"
#include "api.h"
#include "intlconv.h"

static int
api_intlconv_charset_enum
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  const char **chrst;
  htsmsg_t *l;
  
  l = htsmsg_create_list();
  chrst = intlconv_charsets;
  while (*chrst) {
    htsmsg_add_msg(l, NULL, htsmsg_create_key_val(*chrst, *chrst));
    chrst++;
  }
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  return 0;
}

void api_intlconv_init ( void )
{
  static api_hook_t ah[] = {

    { "intlconv/charsets", ACCESS_ANONYMOUS, api_intlconv_charset_enum, NULL },

    { NULL },
  };

  api_register_all(ah);
}


#endif /* __TVH_API_INTLCONV_H__ */
