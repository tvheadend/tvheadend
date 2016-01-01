/*
 *  Tvheadend - internationalization (locale)
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
#ifndef __TVH_LOCALE_H__
#define __TVH_LOCALE_H__

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
