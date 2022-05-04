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
