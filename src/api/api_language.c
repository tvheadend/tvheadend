/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2014 Jaroslav Kysela
 *
 * API - international character conversions
 */

#ifndef __TVH_API_LANGUAGE_H__
#define __TVH_API_LANGUAGE_H__

#include "tvheadend.h"
#include "access.h"
#include "api.h"
#include "lang_codes.h"

static int
api_language_enum
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  const lang_code_t *c = lang_codes;
  htsmsg_t *l;

  l = htsmsg_create_list();
  while (c->code2b) {
    htsmsg_add_msg(l, NULL, htsmsg_create_key_val(c->code2b, c->desc));
    c++;
  }
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  return 0;
}

static int
_api_language_locale_enum
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp, int all )
{
  const lang_code_t *c = lang_codes;
  htsmsg_t *l;
  const char *s;
  char buf1[8];
  char buf2[128];

  l = htsmsg_create_list();
  while (c->code2b) {
    if (all || tvh_gettext_langcode_valid(c->code2b))
      htsmsg_add_msg(l, NULL, htsmsg_create_key_val(c->code2b, c->desc));
    s = c->locale;
    while (s && *s) {
      if (*s == '|')
        s++;
      if (s[0] == '\0' || s[1] == '\0')
        break;
      snprintf(buf1, sizeof(buf1), "%s_%c%c", c->code2b, s[0], s[1]);
      if (all || tvh_gettext_langcode_valid(buf1)) {
        snprintf(buf2, sizeof(buf2), "%s (%c%c)", c->desc, s[0], s[1]);
        htsmsg_add_msg(l, NULL, htsmsg_create_key_val(buf1, buf2));
      }
      s += 2;
    }
    c++;
  }
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  return 0;
}

static int
api_language_locale_enum
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return _api_language_locale_enum(perm, opaque, op, args, resp, 1);
}

static int
api_language_ui_locale_enum
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return _api_language_locale_enum(perm, opaque, op, args, resp, 0);
}

void api_language_init ( void )
{
  static api_hook_t ah[] = {

    { "language/list",      ACCESS_ANONYMOUS, api_language_enum, NULL },
    { "language/locale",    ACCESS_ANONYMOUS, api_language_locale_enum, NULL },
    { "language/ui_locale", ACCESS_ANONYMOUS, api_language_ui_locale_enum, NULL },

    { NULL },
  };

  api_register_all(ah);
}


#endif /* __TVH_API_LANGUAGE_H__ */
