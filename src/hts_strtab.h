/*
 *  tvheadend
 *  Copyright (C) 2007 Andreas Öman
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

#ifndef STRTAB_H_
#define STRTAB_H_

#include "htsmsg.h"
#include "tvh_locale.h"

#include <strings.h>

struct strtab {
  const char *str;
  int val;
};

static int str2val0(const char *str, const struct strtab tab[], int l)
     __attribute((unused));

static int
str2val0(const char *str, const struct strtab tab[], int l)
{
  int i;
  for(i = 0; i < l; i++)
    if(!strcasecmp(str, tab[i].str))
      return tab[i].val;

  return -1;
}

#define str2val(str, tab) str2val0(str, tab, sizeof(tab) / sizeof(tab[0]))



static int str2val0_def(const char *str, struct strtab tab[], int l, int def)
     __attribute((unused));

static int
str2val0_def(const char *str, struct strtab tab[], int l, int def)
{
  int i;
  if(str) 
    for(i = 0; i < l; i++)
      if(!strcasecmp(str, tab[i].str))
	return tab[i].val;
  return def;
}

#define str2val_def(str, tab, def) \
 str2val0_def(str, tab, sizeof(tab) / sizeof(tab[0]), def)


static const char * val2str0(int val, const struct strtab tab[], int l)
     __attribute__((unused));

static const char *
val2str0(int val, const struct strtab tab[], int l)
{
  int i;
  for(i = 0; i < l; i++)
    if(tab[i].val == val)
      return tab[i].str;
  return NULL;
} 

#define val2str(val, tab) val2str0(val, tab, sizeof(tab) / sizeof(tab[0]))

static inline htsmsg_t *
strtab2htsmsg0(const struct strtab tab[], int n, int i18n, const char *lang)
{
  int i;
  htsmsg_t *e, *l = htsmsg_create_list();
  for (i = 0; i < n; i++) {
    e = htsmsg_create_map();
    htsmsg_add_s32(e, "key", tab[i].val);
    htsmsg_add_str(e, "val", i18n ? tvh_gettext_lang(lang, tab[i].str) : tab[i].str);
    htsmsg_add_msg(l, NULL, e);
  }
  return l;
}

#define strtab2htsmsg(tab,i18n,lang) strtab2htsmsg0(tab, sizeof(tab) / sizeof(tab[0]), i18n, lang)

#endif /* STRTAB_H_ */
