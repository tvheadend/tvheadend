/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2015 Jaroslav Kysela
 *
 * Tvheadend - internationalization (locale)
 */

#ifndef __TVH_LOCALE_H__
#define __TVH_LOCALE_H__

#include <stdio.h>

const char *tvh_gettext_get_lang(const char *lang);
const char *tvh_gettext_lang(const char *lang, const char *s);
static inline const char *tvh_gettext(const char *s)
  { return tvh_gettext_lang(NULL, s); }

#define _(s) tvh_gettext(s)
#define N_(s) (s)

int tvh_gettext_langcode_valid(const char *code);

void tvh_gettext_init(void);
void tvh_gettext_done(void);

#endif /* __TVH_LOCALE_H__ */
