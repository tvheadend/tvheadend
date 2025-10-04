/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2014 Jaroslav Kysela
 *
 * tvheadend, iconv interface
 */

#ifndef INTLCONV_H_
#define INTLCONV_H_

extern const char *intlconv_charsets[];

void intlconv_init( void );
void intlconv_done( void );
const char *
intlconv_filesystem_charset( void );
char *
intlconv_charset_id( const char *charset,
                     int transil,
                     int ignore_bad_chars );
ssize_t
intlconv_utf8( char *dst, size_t dst_size,
               const char *dst_charset_id,
               const char *src_utf8 );
char *
intlconv_utf8safestr( const char *dst_charset_id,
                      const char *src_utf8,
                      size_t max_size );

ssize_t
intlconv_to_utf8( char *dst, size_t dst_size,
                  const char *src_charset_id,
                  const char *src, size_t src_size );

char *
intlconv_to_utf8safestr( const char *src_charset_id,
                         const char *src_str,
                         size_t max_size );

#endif /* INTLCONV_H_ */
