/*
 *  Multi-language Support - language codes
 *  Copyright (C) 2012 Adam Sutton
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

#ifndef __TVH_LANG_CODES_H__
#define __TVH_LANG_CODES_H__

#include "redblack.h"

typedef struct lang_code
{
  const char *code2b; ///< ISO 639-2 B
  const char *code1;  ///< ISO 639-1
  const char *code2t; ///< ISO 639-2 T
  const char *desc;   ///< Description
  const char *locale; ///< Locale variants (like US|GB or DE|BE)
} lang_code_t;

extern const lang_code_t lang_codes[];

/* Convert code to preferred internal code */
const char *lang_code_get ( const char *code );
const char *lang_code_get2 ( const char *code, size_t len );
const lang_code_t *lang_code_get3 ( const char *code );

const char *lang_code_preferred( void );

/* Split list of codes as per HTTP Language-Accept spec */
const char **lang_code_split ( const char *codes );
const lang_code_t **lang_code_split2 ( const char *codes );

/* Efficient code lookup */
typedef struct lang_code_lookup_element {
  RB_ENTRY(lang_code_lookup_element) link;
  const lang_code_t *lang_code;
} lang_code_lookup_element_t;

typedef RB_HEAD(lang_code_lookup, lang_code_lookup_element) lang_code_lookup_t;

void lang_code_done( void );

#endif /* __TVH_LANG_CODES_H__ */
