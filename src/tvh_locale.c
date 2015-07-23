/*
 *  tvheadend, internationalization (locale)
 *  Copyright (C) 2015 Jaroslav Kysela
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "pthread.h"
#include "tvh_locale.h"
#include "redblack.h"

struct tvh_locale {
  const char *lang;
  const char **strings;
};

#include "tvh_locale_inc.c"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

pthread_mutex_t tvh_gettext_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 *
 */

struct msg {
  RB_ENTRY(msg) link;
  const char *src;
  const char *dst;
};

struct lng {
  RB_ENTRY(lng) link;
  const char *tvh_lang;
  const char *locale_lang;
  int msgs_initialized;
  RB_HEAD(, msg) msgs;
};

static RB_HEAD(, lng) lngs;

static struct lng *lng_default = NULL;
static struct lng *lng_last = NULL;

/*
 * Message RB tree
 */

static inline int msg_cmp(const struct msg *a, const struct msg *b)
{
  return strcmp(a->src, b->src);
}

static void msg_add_strings(struct lng *lng, const char **strings)
{
  struct msg *m;
  const char **p;

  for (p = strings; *p; p += 2) {
    m = calloc(1, sizeof(*m));
    m->src = p[0];
    m->dst = p[1];
    if (RB_INSERT_SORTED(&lng->msgs, m, link, msg_cmp))
      abort();
  }
}

static inline const char *msg_find(struct lng *lng, const char *msg)
{
  struct msg *m, ms;

  ms.src = msg;
  m = RB_FIND(&lng->msgs, &ms, link, msg_cmp);
  if (m)
    return m->dst;
  return msg;
}

/*
 *  Language RB tree
 */

static inline int lng_cmp(const struct lng *a, const struct lng *b)
{
  return strcmp(a->tvh_lang, b->tvh_lang);
}

static struct lng *lng_add(const char *tvh_lang, const char *locale_lang)
{
  struct lng *l = calloc(1, sizeof(*l));
  l->tvh_lang = tvh_lang;
  l->locale_lang = locale_lang;
  if (RB_INSERT_SORTED(&lngs, l, link, lng_cmp))
    abort();
  return l;
}

static void lng_init(struct lng *l)
{
  struct tvh_locale *tl;
  int i;

  l->msgs_initialized = 1;
  for (i = 0, tl = tvh_locales; i < ARRAY_SIZE(tvh_locales); i++, tl++)
    if (strcmp(tl->lang, l->locale_lang) == 0) {
      msg_add_strings(l, tl->strings);
      break;
    }
}

static struct lng *lng_get(const char *tvh_lang)
{
  struct lng *l, ls;
  char *s;

  if (tvh_lang != NULL && tvh_lang[0] != '\0') {
    s = alloca(strlen(tvh_lang) + 1);
    ls.tvh_lang = s;
    for ( ; *tvh_lang && *tvh_lang != ','; s++, tvh_lang++)
      *s = *tvh_lang;
    *s = '\0';
    l = RB_FIND(&lngs, &ls, link, lng_cmp);
    if (l) {
      if (!l->msgs_initialized)
        lng_init(l);
      return l;
    }
  }
  return lng_get("eng");
}

static struct lng *lng_get_locale(char *locale_lang)
{
  struct lng *l;

  if (locale_lang != NULL && locale_lang[0] != '\0') {
    RB_FOREACH(l, &lngs, link)
      if (!strcmp(l->locale_lang, locale_lang)) {
        if (!l->msgs_initialized)
          lng_init(l);
        return l;
      }
  }
  return lng_get("eng");
}

/*
 *
 */

const char *tvh_gettext_get_lang(const char *lang)
{
  struct lng *l = lng_get(lang);
  return l->locale_lang;
}

static void tvh_gettext_default_init(void)
{
  static char dflt[16];
  char *p;

  p = getenv("LC_ALL");
  if (p == NULL)
    p = getenv("LANG");
  if (p == NULL)
    p = getenv("LANGUAGE");
  if (p == NULL)
    p = (char *)"en";

  strncpy(dflt, p, sizeof(dflt)-1);
  dflt[sizeof(dflt)-1] = '\0';
  for (p = dflt; *p && *p != '.'; p++);
  if (*p == '.') *p = '\0';

  if ((lng_default = lng_get_locale(dflt)) != NULL)
    return;

  for (p = dflt; *p && *p != '_'; p++);
  if (*p == '_') *p = '\0';

  if ((lng_default = lng_get_locale(dflt)) != NULL)
    return;

  lng_default = lng_add(dflt, dflt);
  if (!lng_default)
    return;
}

const char *tvh_gettext_lang(const char *lang, const char *s)
{
  pthread_mutex_lock(&tvh_gettext_mutex);
  if (lang == NULL) {
    s = msg_find(lng_default, s);
  } else {
    if (!strcmp(lng_last->locale_lang, lang)) {
      s = msg_find(lng_last, s);
    } else {
      if ((lng_last = lng_get(lang)) == NULL)
        s = msg_find(lng_default, s);
      else
        s = msg_find(lng_last, s);
    }
  }
  pthread_mutex_unlock(&tvh_gettext_mutex);
  return s;
}

/*
 *
 */

void tvh_gettext_init(void)
{
  static const char *tbl[] = {
    "bul",    "bg",
    "cze",    "cs",
    "ger",    "de",
    "eng",    "en",
    "eng_GB", "en_GB",
    "eng_US", "en",
    "spa",    "es",
    "per",    "fa",
    "fre",    "fr",
    "heb",    "he",
    "hrv",    "hr",
    "hun",    "hu",
    "ita",    "it",
    "lav",    "lv",
    "dut",    "nl",
    "pol",    "pl",
    "por",    "pt",
    "rus",    "ru",
    "srp",    "sr",
    "swe",    "sv",
    NULL, NULL
  };
  const char **p;
  for (p = tbl; *p; p += 2)
    lng_add(p[0], p[1]);
  tvh_gettext_default_init();
  lng_last = lng_default;
}

void tvh_gettext_done(void)
{
  struct lng *l;
  struct msg *m;

  while ((l = RB_FIRST(&lngs)) != NULL) {
    while ((m = RB_FIRST(&l->msgs)) != NULL) {
      RB_REMOVE(&l->msgs, m, link);
      free(m);
    }
    RB_REMOVE(&lngs, l, link);
    free(l);
  }
}
