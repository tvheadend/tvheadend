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
const char *tvh_gettext_default_lang = NULL;
const char *tvh_gettext_last_lang = NULL;
const char **tvh_gettext_last_strings = NULL;

const char *tvh_gettext_get_lang(const char *lang)
{
  if (lang == NULL || lang[0] == '\0')
    return "en";
  if (!strcmp(lang, "eng_US"))
    return "en";
  if (!strcmp(lang, "eng_GB"))
    return "en_GB";
  if (!strcmp(lang, "ger"))
    return "de";
  if (!strcmp(lang, "fre"))
    return "fr";
  if (!strcmp(lang, "cze"))
    return "cs";
  if (!strcmp(lang, "pol"))
    return "pl";
  if (!strcmp(lang, "bul"))
    return "bg";
  return lang;
}

static void tvh_gettext_init(void)
{
  struct tvh_locale *l;
  static char dflt[16];
  char *p;
  int i;

  tvh_gettext_default_lang = getenv("LC_ALL");
  if (tvh_gettext_default_lang == NULL)
    tvh_gettext_default_lang = getenv("LANG");
  if (tvh_gettext_default_lang == NULL)
    tvh_gettext_default_lang = getenv("LANGUAGE");
  if (tvh_gettext_default_lang == NULL)
    tvh_gettext_default_lang = "en";

  strncpy(dflt, tvh_gettext_default_lang, sizeof(dflt)-1);
  dflt[sizeof(dflt)-1] = '\0';
  for (p = dflt; p && *p != '.'; p++);
  if (*p == '.') *p = '\0';

  tvh_gettext_default_lang = NULL;
  for (i = 0, l = tvh_locales; i < ARRAY_SIZE(tvh_locales); i++)
    if (strcmp(dflt, l->lang) == 0) {
      tvh_gettext_default_lang = l->lang;
      tvh_gettext_last_lang = l->lang;
      tvh_gettext_last_strings = l->strings;
      return;
    }

  for (p = dflt; p && *p != '_'; p++);
  if (*p == '_') *p = '\0';

  tvh_gettext_default_lang = NULL;
  for (i = 0, l = tvh_locales; i < ARRAY_SIZE(tvh_locales); i++)
    if (strcmp(dflt, l->lang) == 0) {
      tvh_gettext_default_lang = l->lang;
      tvh_gettext_last_lang = l->lang;
      tvh_gettext_last_strings = l->strings;
      return;
    }

  tvh_gettext_default_lang = dflt;
  tvh_gettext_last_lang = dflt;
  tvh_gettext_last_strings = NULL;
}

static void tvh_gettext_new_lang(const char *lang)
{
  struct tvh_locale *l;
  int i;
    
  tvh_gettext_last_lang = NULL;
  tvh_gettext_last_strings = NULL;
  for (i = 0, l = tvh_locales; i < ARRAY_SIZE(tvh_locales); i++, l++)
    if (strcmp(lang, l->lang) == 0) {
      tvh_gettext_last_lang = l->lang;
      tvh_gettext_last_strings = l->strings;
      break;
    }
}

const char *tvh_gettext_lang(const char *lang, const char *s)
{
  const char **strings;
  static char unklng[8];

  pthread_mutex_lock(&tvh_gettext_mutex);
  if (lang == NULL) {
    if (tvh_gettext_default_lang == NULL)
      tvh_gettext_init();
    lang = tvh_gettext_default_lang;
  } else {
    lang = tvh_gettext_get_lang(lang);
  }
  if (tvh_gettext_last_lang == NULL || strcmp(tvh_gettext_last_lang, lang)) {
    tvh_gettext_new_lang(lang);
    if (tvh_gettext_last_lang == NULL) {
      if (tvh_gettext_default_lang == NULL)
        tvh_gettext_init();
      tvh_gettext_new_lang(tvh_gettext_default_lang);
      strncpy(unklng, lang, sizeof(unklng));
      unklng[sizeof(unklng)-1] = '\0';
      tvh_gettext_last_lang = unklng;
    }
  }
  if ((strings = tvh_gettext_last_strings) != NULL) {
    for ( ; strings[0]; strings += 2)
      if (strcmp(strings[0], s) == 0) {
        s = strings[1];
        break;
      }
  }
  pthread_mutex_unlock(&tvh_gettext_mutex);
  return s;
}
