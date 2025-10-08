/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2017 Jaroslav Kysela
 *
 * TVheadend - regex wrapper
 */

#ifndef __TVHREGEX_H__
#define __TVHREGEX_H__

#if ENABLE_PCRE

#  include <pcre.h>
#  ifndef PCRE_STUDY_JIT_COMPILE
#  define PCRE_STUDY_JIT_COMPILE 0
#  endif

#define TVHREGEX_TYPE           "pcre1"

#elif ENABLE_PCRE2

#  define PCRE2_CODE_UNIT_WIDTH 8
#  include <pcre2.h>

#define TVHREGEX_TYPE           "pcre2"

#endif

#  include <regex.h>

#define TVHREGEX_MAX_MATCHES    10

/* Compile flags */
#define TVHREGEX_POSIX          1       /* Use POSIX regex engine */
#define TVHREGEX_CASELESS       2       /* Use case-insensitive matching */

typedef struct {
#if ENABLE_PCRE
  pcre *re_code;
  pcre_extra *re_extra;
  int re_match[TVHREGEX_MAX_MATCHES * 3];
  const char *re_text;
  int re_matches;
#if PCRE_STUDY_JIT_COMPILE
  pcre_jit_stack *re_jit_stack;
#endif
#elif ENABLE_PCRE2
  pcre2_code *re_code;
  pcre2_match_data *re_match;
  pcre2_match_context *re_mcontext;
  pcre2_jit_stack *re_jit_stack;
#endif
  regex_t re_posix_code;
  regmatch_t re_posix_match[TVHREGEX_MAX_MATCHES];
  const char *re_posix_text;
#if ENABLE_PCRE || ENABLE_PCRE2
  int is_posix;
#endif
} tvh_regex_t;

void regex_free(tvh_regex_t *regex);
int regex_compile(tvh_regex_t *regex, const char *re_str, int flags, int subsys);
int regex_match(tvh_regex_t *regex, const char *str);
int regex_match_substring(tvh_regex_t *regex, unsigned number, char *buf, size_t size_buf);
int regex_match_substring_length(tvh_regex_t *regex, unsigned number);

#endif /* __TVHREGEX_H__ */
