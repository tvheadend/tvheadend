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

#ifndef __EITPATTERN_LIST__
#define __EITPATTERN_LIST__

#include "queue.h"
#include "tvhregex.h"

typedef struct eit_pattern {
  char*       text;
  tvh_regex_t compiled;
  int         filter;
  char*       langs;
  TAILQ_ENTRY(eit_pattern) p_links;
} eit_pattern_t;

TAILQ_HEAD(eit_pattern_list, eit_pattern);
typedef struct eit_pattern_list eit_pattern_list_t;

/* is list empty? */
static inline int eit_pattern_list_empty(eit_pattern_list_t* list) {
  return TAILQ_EMPTY(list);
}

/* Compile a regular expression pattern from a message */
void eit_pattern_compile_list(eit_pattern_list_t* list, htsmsg_t* l, int flags);
/* Compile a regular expression pattern from a named message, applying message location conventions
 */
void eit_pattern_compile_named_list(eit_pattern_list_t* list, htsmsg_t* m, const char* key);
/* Apply the compiled pattern to text. If it matches then return the
 * match in buf which is of size size_buf.
 * Return the buf or NULL if no match.
 */
void* eit_pattern_apply_list(char* buf,
    size_t                         size_buf,
    const char*                    text,
    const char*                    lang,
    eit_pattern_list_t*            l);
void  eit_pattern_free_list(eit_pattern_list_t* l);
#endif
