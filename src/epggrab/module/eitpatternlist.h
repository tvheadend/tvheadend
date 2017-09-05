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

#include <regex.h>
#include "queue.h"

typedef struct eit_pattern
{
  char                        *text;
  regex_t                     compiled;
  TAILQ_ENTRY(eit_pattern) p_links;
} eit_pattern_t;
TAILQ_HEAD(eit_pattern_list, eit_pattern);
typedef struct eit_pattern_list eit_pattern_list_t;

/* Compile a regular expression pattern from a message */
void eit_pattern_compile_list ( eit_pattern_list_t *list, htsmsg_t *l );
/* Apply the compiled pattern to text. If it matches then return the
 * match in buf which is of size size_buf.
 * Return the buf or NULL if no match.
 */
void *eit_pattern_apply_list(char *buf, size_t size_buf, const char *text, eit_pattern_list_t *l);
void eit_pattern_free_list ( eit_pattern_list_t *l );
#endif
