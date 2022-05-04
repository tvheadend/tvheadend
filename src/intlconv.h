/*
 *  tvheadend, iconv interface
 *  Copyright (C) 2014 Jaroslav Kysela
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
