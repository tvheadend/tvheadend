/*
 *  Tvheadend - advanced string functions
 *  Copyright (C) 2007 Andreas Öman
 *  Copyright (C) 2014-2018 Jaroslav Kysela
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
#ifndef TVHEADEND_STRING_H
#define TVHEADEND_STRING_H

#include <string.h>
#include <stdlib.h>

#include "build.h"

static inline int strempty(const char *c)
  { return c == NULL || *c == '\0'; }

char *hts_strndup(const char *str, size_t len);

char *htsstr_unescape(char *str);

char *htsstr_unescape_to(const char *src, char *dst, size_t dstlen);

const char *htsstr_escape_find(const char *src, size_t upto_index);

char **htsstr_argsplit(const char *str);

void htsstr_argsplit_free(char **argv);

typedef struct {
  const char *id;
  const char *(*getval)(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen);
} htsstr_substitute_t;

const char *
htsstr_substitute_find(const char *src, int first);

char *
htsstr_substitute(const char *src, char *dst, size_t dstlen,
                  int first, htsstr_substitute_t *sub, const void *aux,
                  char *tmp, size_t tmplen);

static inline size_t tvh_strlen(const char *s)
{
  return (s) ? strlen(s) : 0;
}

static inline const char *tvh_str_default(const char *s, const char *dflt)
{
  return s && s[0] ? s : dflt;
}

void tvh_str_set(char **strp, const char *src);
int tvh_str_update(char **strp, const char *src);

#define tvh_strdupa(n) \
  ({ int tvh_l = strlen(n); \
     char *tvh_b = alloca(tvh_l + 1); \
     memcpy(tvh_b, n, tvh_l + 1); })

static inline const char *tvh_strbegins(const char *s1, const char *s2)
{
  while(*s2)
    if(*s1++ != *s2++)
      return NULL;
  return s1;
}

#ifndef ENABLE_STRLCPY
static inline size_t strlcpy(char *dst, const char *src, size_t size)
{
  size_t ret = strlen(src);
  if (size) {
    size_t len = ret >= size ? size - 1 : ret;
    memcpy(dst, src, len);
    dst[len] = '\0';
  }
  return ret;
}
#endif

#ifndef ENABLE_STRLCAT
static inline size_t strlcat(char *dst, const char *src, size_t count)
{
  size_t dlen = strlen(dst);
  size_t len = strlen(src);
  size_t res = dlen + len;

  dst += dlen;
  count -= dlen;
  if (len >= count)
    len = count - 1;
  memcpy(dst, src, len);
  dst[len] = '\0';
  return res;
}
#endif

#define tvh_strlcatf(buf, size, ptr, fmt...) \
  do { int __r = snprintf((buf) + ptr, (size) - ptr, fmt); \
       ptr = __r >= (size) - ptr ? (size) - 1 : ptr + __r; } while (0)

static inline void mystrset(char **p, const char *s)
{
  free(*p);
  *p = s ? strdup(s) : NULL;
}

static inline unsigned int tvh_strhash(const char *s, unsigned int mod)
{
  unsigned int v = 5381;
  while(*s)
    v += (v << 5) + v + *s++;
  return v % mod;
}

int put_utf8(char *out, int c);

char *utf8_lowercase_inplace(char *s);
char *utf8_validate_inplace(char *s);

#endif /* TVHEADEND_STRING_H */
