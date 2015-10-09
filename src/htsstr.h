/*
 *  String helper functions
 *  Copyright (C) 2008 Andreas Öman
 *  Copyright (C) 2008 Mattias Wadman
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


#ifndef HTSSTR_H__
#define HTSSTR_H__

char *hts_strndup(const char *str, size_t len);

char *htsstr_unescape(char *str);

char *htsstr_unescape_to(const char *src, char *dst, size_t dstlen);

const char *htsstr_escape_find(const char *src, size_t upto_index);

char **htsstr_argsplit(const char *str);

void htsstr_argsplit_free(char **argv);

typedef struct {
  const char *id;
  const char *(*getval)(const char *id, const void *aux, char *tmp, size_t tmplen);
} htsstr_substitute_t;

const char *
htsstr_substitute_find(const char *src, int first);

char *
htsstr_substitute(const char *src, char *dst, size_t dstlen,
                  int first, htsstr_substitute_t *sub, const void *aux,
                  char *tmp, size_t tmplen);

#endif /* HTSSTR_H__ */
