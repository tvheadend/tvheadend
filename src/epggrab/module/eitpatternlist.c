/*
 *  Electronic Program Guide - Regular Expression Pattern Functions
 *  Copyright (C) 2012-2017 Adam Sutton
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

#include <assert.h>
#include <ctype.h>
#include "tvheadend.h"
#include "eitpatternlist.h"
#include "htsmsg.h"

#define MAX_TEXT_LEN    2048

static char *get_languages_string(htsmsg_field_t *field)
{
  const char *s;
  htsmsg_t *langlist;

  if (field == NULL)
    return NULL;

  s = htsmsg_field_get_str(field);
  if (s) {
    return strdup(s);
  } else {
    langlist = htsmsg_field_get_list(field);
    if (langlist) {
      htsmsg_field_t *item;
      char langbuf[MAX_TEXT_LEN];
      size_t l = 0;
      langbuf[0] = '\0';
      HTSMSG_FOREACH(item, langlist) {
        s = htsmsg_field_get_str(item);
        if (s)
          tvh_strlcatf(langbuf, MAX_TEXT_LEN, l, "%s|", s);
      }
      if (l > 0)
        return strdup(langbuf);
    }
  }
  return NULL;
}

void eit_pattern_compile_list ( eit_pattern_list_t *list, htsmsg_t *l, int flags )
{
  eit_pattern_t *pattern;
  htsmsg_field_t *f;
  const char *text;
  int filter;
  char *langs;

  TAILQ_INIT(list);
  if (!l) return;
  HTSMSG_FOREACH(f, l) {
    text = htsmsg_field_get_str(f);
    filter = 0;
    langs = NULL;
    if (text == NULL) {
      htsmsg_t *m = htsmsg_field_get_map(f);
      if (m == NULL) continue;
      text = htsmsg_get_str(m, "pattern");
      if (text == NULL) continue;
      filter = htsmsg_get_bool_or_default(m, "filter", 0);
      langs = get_languages_string(htsmsg_field_find(m, "lang"));
    }
    pattern = calloc(1, sizeof(eit_pattern_t));
    pattern->text = strdup(text);
    pattern->filter = filter;
    pattern->langs = langs;
    if (regex_compile(&pattern->compiled, pattern->text, flags, LS_EPGGRAB)) {
      tvhwarn(LS_EPGGRAB, "error compiling pattern \"%s\"", pattern->text);
      free(pattern->langs);
      free(pattern->text);
      free(pattern);
    } else {
      tvhtrace(LS_EPGGRAB, "compiled pattern \"%s\", filter %d", pattern->text, pattern->filter);
      TAILQ_INSERT_TAIL(list, pattern, p_links);
    }
  }
}

void eit_pattern_compile_named_list ( eit_pattern_list_t *list, htsmsg_t *m, const char *key)
{
#if defined(TVHREGEX_TYPE)
  htsmsg_t *m_alt = htsmsg_get_map(m, TVHREGEX_TYPE);
  if (!m_alt)
    m_alt = htsmsg_get_map(m, "pcre");
  if (m_alt) {
    htsmsg_t *res = htsmsg_get_list(m_alt, key);
    if (res) {
      eit_pattern_compile_list(list, res, 0);
      return;
    }
  }
#endif
  eit_pattern_compile_list(list, htsmsg_get_list(m, key), TVHREGEX_POSIX);
}

static void rtrim(char *buf)
{
  size_t len = strlen(buf);
  while (len > 0 && isspace(buf[len - 1]))
    --len;
  buf[len] = '\0';
}

void *eit_pattern_apply_list(char *buf, size_t size_buf, const char *text, const char *lang, eit_pattern_list_t *l)
{
  eit_pattern_t *p;
  char textbuf[MAX_TEXT_LEN];
  char matchbuf[MAX_TEXT_LEN];
  int matchno;

  assert(buf);
  assert(text);

  if (!l) return NULL;

  /* search and concatenate all subgroup matches - there must be at least one */
  TAILQ_FOREACH(p, l, p_links) {
    if (p->langs && lang) {
      if (strstr(p->langs, lang) == NULL) {
        continue;
      }
    }
    if (!regex_match(&p->compiled, text) &&
        !regex_match_substring(&p->compiled, 1, buf, size_buf)) {
      for (matchno = 2; ; ++matchno) {
        if (regex_match_substring(&p->compiled, matchno, matchbuf, sizeof(matchbuf)))
          break;
        size_t len = strlen(buf);
        strlcat(buf, matchbuf, size_buf - len);
      }
      rtrim(buf);
      tvhtrace(LS_EPGGRAB,"  pattern \"%s\" matches '%s' from '%s'", p->text, buf, text);
      if (p->filter) {
        strlcpy(textbuf, buf, MAX_TEXT_LEN);
        text = textbuf;
        continue;
      }
      return buf;
    }
  }
  return NULL;
}

void eit_pattern_free_list ( eit_pattern_list_t *l )
{
  eit_pattern_t *p;

  if (!l) return;
  while ((p = TAILQ_FIRST(l)) != NULL) {
    TAILQ_REMOVE(l, p, p_links);
    free(p->langs);
    free(p->text);
    regex_free(&p->compiled);
    free(p);
  }
}
