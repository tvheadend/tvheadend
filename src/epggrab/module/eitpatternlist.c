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

void eit_pattern_compile_list ( eit_pattern_list_t *list, htsmsg_t *l, int flags )
{
  eit_pattern_t *pattern;
  htsmsg_field_t *f;
  const char *s;

  TAILQ_INIT(list);
  if (!l) return;
  HTSMSG_FOREACH(f, l) {
    s = htsmsg_field_get_str(f);
    if (s == NULL) continue;
    pattern = calloc(1, sizeof(eit_pattern_t));
    pattern->text = strdup(s);
    if (regex_compile(&pattern->compiled, pattern->text, flags, LS_EPGGRAB)) {
      tvhwarn(LS_EPGGRAB, "error compiling pattern \"%s\"", pattern->text);
      free(pattern->text);
      free(pattern);
    } else {
      tvhtrace(LS_EPGGRAB, "compiled pattern \"%s\"", pattern->text);
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

void *eit_pattern_apply_list(char *buf, size_t size_buf, const char *text, eit_pattern_list_t *l)
{
  eit_pattern_t *p;
  char matchbuf[2048];
  int matchno;

  assert(buf);
  assert(text);

  if (!l) return NULL;

  /* search and concatenate all subgroup matches - there must be at least one */
  TAILQ_FOREACH(p, l, p_links)
    if (!regex_match(&p->compiled, text) &&
        !regex_match_substring(&p->compiled, 1, buf, size_buf)) {
      for (matchno = 2; ; ++matchno) {
        if (regex_match_substring(&p->compiled, matchno, matchbuf, sizeof(matchbuf)))
          break;
        size_t len = strlen(buf);
        strncat(buf, matchbuf, size_buf - len - 1);
      }
      rtrim(buf);
      tvhtrace(LS_EPGGRAB,"  pattern \"%s\" matches with '%s'", p->text, buf);
      return buf;
    }
  return NULL;
}

void eit_pattern_free_list ( eit_pattern_list_t *l )
{
  eit_pattern_t *p;

  if (!l) return;
  while ((p = TAILQ_FIRST(l)) != NULL) {
    TAILQ_REMOVE(l, p, p_links);
    free(p->text);
    regex_free(&p->compiled);
    free(p);
  }
}
