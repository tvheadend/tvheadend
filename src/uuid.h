/*
 *  TV headend - UUID generation routines
 *
 *  Copyright (C) 2014 Adam Sutton
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

#ifndef __TVH_UUID_H__
#define __TVH_UUID_H__

#include <stdint.h>

#define UUID_BIN_SIZE   (16)
#define UUID_HEX_SIZE   (33) // inc NUL

extern uint8_t ___uuid_empty[UUID_BIN_SIZE];

/* Structure to hold UUID */
typedef struct uuid {
  uint8_t bin[UUID_BIN_SIZE];
} tvh_uuid_t;

/* Initialise subsystem */
void uuid_init ( void );

/* Random bytes */
void uuid_random ( uint8_t *buf, size_t bufsize );

/* Set uuid */
int uuid_set ( tvh_uuid_t *u, const char *str );

/* Get hexa uuid, str must have length at least UUID_HEX_SIZE */
char *uuid_get_hex ( const tvh_uuid_t *u, char *dst );

/**
 * Copy
 */
static inline void uuid_duplicate ( tvh_uuid_t *dst, const tvh_uuid_t *src )
{
  *dst = *src;
}

/**
 * Compare
 */
static inline int uuid_cmp ( const tvh_uuid_t *a, const tvh_uuid_t *b )
{
  return memcmp(a->bin, b->bin, UUID_BIN_SIZE);
}

/**
 * Empty
 */
static inline int uuid_empty ( const tvh_uuid_t *a )
{
  return memcmp(a->bin, ___uuid_empty, UUID_BIN_SIZE) == 0;
}

/**
 * Validate the hexadecimal representation of uuid
 */
int uuid_hexvalid ( const char *uuid );

/**
 * Hex string to binary
 */
int hex2bin ( uint8_t *buf, size_t buflen, const char *hex );

/**
 * Binary to hex string
 */
void bin2hex ( char *dst, size_t dstlen, const uint8_t *src, size_t srclen );

#endif /* __TVH_UUID_H__ */
