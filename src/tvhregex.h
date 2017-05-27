/*
 *  TVheadend - regex wrapper
 *
 *  Copyright (C) 2017 Jaroslav Kysela
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TVHREGEX_H__
#define __TVHREGEX_H__

#if ENABLE_PCRE

#  include <pcre.h>
#  ifndef PCRE_STUDY_JIT_COMPILE
#  define PCRE_STUDY_JIT_COMPILE 0
#  endif

#elif ENABLE_PCRE2

#  define PCRE2_CODE_UNIT_WIDTH 8
#  include <pcre2.h>

#else

#  include <regex.h>

#endif

typedef struct {
#if ENABLE_PCRE
  pcre *re_code;
  pcre_extra *re_extra;
#if PCRE_STUDY_JIT_COMPILE
  pcre_jit_stack *re_jit_stack;
#endif
#elif ENABLE_PCRE2
  pcre2_code *re_code;
  pcre2_match_data *re_match;
  pcre2_match_context *re_mcontext;
  pcre2_jit_stack *re_jit_stack;
#else
  regex_t re_code;
#endif
} tvh_regex_t;

static inline int regex_match(tvh_regex_t *regex, const char *str)
{
#if ENABLE_PCRE
  int vec[30];
  return pcre_exec(regex->re_code, regex->re_extra,
                   str, strlen(str), 0, 0, vec, ARRAY_SIZE(vec)) < 0;
#elif ENABLE_PCRE2
  return pcre2_match(regex->re_code, (PCRE2_SPTR8)str, -1, 0, 0,
                     regex->re_match, regex->re_mcontext) <= 0;
#else
  return regexec(&regex->re_code, str, 0, NULL, 0);
#endif
}

void regex_free(tvh_regex_t *regex);
int regex_compile(tvh_regex_t *regex, const char *re_str, int subsys);

#endif /* __TVHREGEX_H__ */
