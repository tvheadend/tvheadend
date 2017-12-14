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

void eit_pattern_compile_list ( eit_pattern_list_t *list, htsmsg_t *l )
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
    if (regex_compile(&pattern->compiled, pattern->text, 0, LS_EPGGRAB)) {
      tvhwarn(LS_EPGGRAB, "error compiling pattern \"%s\"", pattern->text);
      free(pattern->text);
      free(pattern);
    } else {
      tvhtrace(LS_EPGGRAB, "compiled pattern \"%s\"", pattern->text);
      TAILQ_INSERT_TAIL(list, pattern, p_links);
    }
  }
}

void *eit_pattern_apply_list(char *buf, size_t size_buf, const char *text, eit_pattern_list_t *l)
{
  char *b[2] = { buf, NULL };
  size_t s[2] = { size_buf, 0 };
  return eit_pattern_apply_list_2(b, s, text, l);
}

static void rtrim(char *buf)
{
  size_t len = strlen(buf);
  while (len > 0 && isspace(buf[len - 1]))
    --len;
  buf[len] = '\0';
}

void *eit_pattern_apply_list_2(char *buf[2], size_t size_buf[2], const char *text, eit_pattern_list_t *l)
{
  eit_pattern_t *p;

  assert(buf[0]);
  assert(text);

  if (!l) return NULL;
  /* search and report the first match */
  TAILQ_FOREACH(p, l, p_links)
    if (!regex_match(&p->compiled, text) &&
        !regex_match_substring(&p->compiled, 1, buf[0], size_buf[0])) {
      rtrim(buf[0]);
      if (buf[1] && !regex_match_substring(&p->compiled, 2, buf[1], size_buf[1])) {
        rtrim(buf[1]);
        tvhtrace(LS_EPGGRAB,"  pattern \"%s\" matches with '%s' & '%s'", p->text, buf[0], buf[1]);
      } else {
        buf[1] = NULL;
        tvhtrace(LS_EPGGRAB,"  pattern \"%s\" matches with '%s'", p->text, buf[0]);
      }
      return buf[0];
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
