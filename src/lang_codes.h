/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2012 Adam Sutton
 *
 * Multi-language Support - language codes
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

typedef struct lang_code_list
{
  RB_ENTRY(lang_code_list) link;
  const char *langs;
  int codeslen;
  const lang_code_t *codes[0];
} lang_code_list_t;

/* Convert code to preferred internal code */

const char *lang_code_get ( const char *code );
const char *lang_code_get2 ( const char *code, size_t len );
const lang_code_t *lang_code_get3 ( const char *code );

const char *lang_code_preferred( void );
char *lang_code_user( const char *ucode );

/* Split list of codes as per HTTP Language-Accept spec */

const lang_code_list_t *lang_code_split ( const char *codes );

/* Efficient code lookup */

typedef struct lang_code_lookup_element {
  RB_ENTRY(lang_code_lookup_element) link;
  const lang_code_t *lang_code;
} lang_code_lookup_element_t;

typedef RB_HEAD(lang_code_lookup, lang_code_lookup_element) lang_code_lookup_t;

void lang_code_done( void );

#endif /* __TVH_LANG_CODES_H__ */
