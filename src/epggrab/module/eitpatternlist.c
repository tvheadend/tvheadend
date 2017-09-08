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
    if (regcomp(&pattern->compiled, pattern->text, REG_EXTENDED)) {
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
  regmatch_t match[2];
  eit_pattern_t *p;
  ssize_t size;

  if (!l) return NULL;
  /* search and report the first match */
  TAILQ_FOREACH(p, l, p_links)
    if (!regexec(&p->compiled, text, 2, match, 0) && match[1].rm_so != -1) {
      size = MIN(match[1].rm_eo - match[1].rm_so, size_buf - 1);
      if (size <= 0)
        continue;
      while (isspace(text[match[1].rm_so + size - 1]))
        size--;
      memcpy(buf, text + match[1].rm_so, size);
      buf[size] = '\0';
      if (size) {
         tvhtrace(LS_EPGGRAB,"  pattern \"%s\" matches with '%s'", p->text, buf);
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
    free(p->text);
    regfree(&p->compiled);
    free(p);
  }
}
