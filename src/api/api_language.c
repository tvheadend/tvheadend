/*
 *  API - international character conversions
 *
 *  Copyright (C) 2014 Jaroslav Kysela
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
  htsmsg_t *l, *e;

  l = htsmsg_create_list();
  while (c->code2b) {
    e = htsmsg_create_map();
    htsmsg_add_str(e, "key", c->code2b);
    htsmsg_add_str(e, "val", c->desc);
    htsmsg_add_msg(l, NULL, e);
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
  htsmsg_t *l, *e;
  const char *s;
  char buf1[8];
  char buf2[128];

  l = htsmsg_create_list();
  while (c->code2b) {
    e = htsmsg_create_map();
    if (all || tvh_gettext_langcode_valid(c->code2b)) {
      htsmsg_add_str(e, "key", c->code2b);
      htsmsg_add_str(e, "val", c->desc);
      htsmsg_add_msg(l, NULL, e);
    }
    s = c->locale;
    while (s && *s) {
      if (*s == '|')
        s++;
      if (s[0] == '\0' || s[1] == '\0')
        break;
      snprintf(buf1, sizeof(buf1), "%s_%c%c", c->code2b, s[0], s[1]);
      if (all || tvh_gettext_langcode_valid(buf1)) {
        snprintf(buf2, sizeof(buf2), "%s (%c%c)", c->desc, s[0], s[1]);
        e = htsmsg_create_map();
        htsmsg_add_str(e, "key", buf1);
        htsmsg_add_str(e, "val", buf2);
        htsmsg_add_msg(l, NULL, e);
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
