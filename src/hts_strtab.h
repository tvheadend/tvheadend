/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2007 Andreas Öman
 *
 * tvheadend
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

struct strtab_u32 {
  const char *str;
  uint32_t val;
};

struct strtab_str {
  const char *str;
  const char *val;
};

static int str2val0(const char *str, const struct strtab tab[], int l)
     __attribute((unused));

static inline int
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

static inline int
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

static inline const char *
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

static inline htsmsg_t *
strtab2htsmsg0_u32(const struct strtab_u32 tab[], uint32_t n, int i18n, const char *lang)
{
  uint32_t i;
  htsmsg_t *e, *l = htsmsg_create_list();
  for (i = 0; i < n; i++) {
    e = htsmsg_create_map();
    htsmsg_add_u32(e, "key", tab[i].val);
    htsmsg_add_str(e, "val", i18n ? tvh_gettext_lang(lang, tab[i].str) : tab[i].str);
    htsmsg_add_msg(l, NULL, e);
  }
  return l;
}

#define strtab2htsmsg_u32(tab,i18n,lang) strtab2htsmsg0_u32(tab, sizeof(tab) / sizeof(tab[0]), i18n, lang)

static inline htsmsg_t *
strtab2htsmsg0_str(const struct strtab_str tab[], uint32_t n, int i18n, const char *lang)
{
  uint32_t i;
  htsmsg_t *e, *l = htsmsg_create_list();
  for (i = 0; i < n; i++) {
    e = htsmsg_create_key_val(tab[i].val, i18n ? tvh_gettext_lang(lang, tab[i].str) : tab[i].str);
    htsmsg_add_msg(l, NULL, e);
  }
  return l;
}

#define strtab2htsmsg_str(tab,i18n,lang) strtab2htsmsg0_str(tab, sizeof(tab) / sizeof(tab[0]), i18n, lang)

#endif /* STRTAB_H_ */
