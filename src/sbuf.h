/*
 *  Tvheadend - sbuf routines
 *  Copyright (C) 2007 Andreas Öman
 *  Copyright (C) 2014-2017 Jaroslav Kysela
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

#ifndef __TVH_SBUF_H
#define __TVH_SBUF_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

/**
 * Simple dynamically growing buffer
 */
typedef struct sbuf {
  uint8_t *sb_data;
  int      sb_ptr;
  int      sb_size;
  uint16_t sb_err;
  uint8_t  sb_bswap;
} sbuf_t;


void sbuf_init(sbuf_t *sb);
void sbuf_init_fixed(sbuf_t *sb, int len);

void sbuf_free(sbuf_t *sb);

void sbuf_reset(sbuf_t *sb, int max_len);
void sbuf_reset_and_alloc(sbuf_t *sb, int len);

static inline void sbuf_steal_data(sbuf_t *sb)
{
  sb->sb_data = NULL;
  sb->sb_ptr = sb->sb_size = sb->sb_err = 0;
}

static inline void sbuf_err(sbuf_t *sb, int errors)
{
  sb->sb_err += errors;
}

void sbuf_alloc_(sbuf_t *sb, int len);

static inline void sbuf_alloc(sbuf_t *sb, int len)
{
  if (sb->sb_ptr + len >= sb->sb_size)
    sbuf_alloc_(sb, len);
}

void sbuf_realloc(sbuf_t *sb, int len);

void sbuf_replace(sbuf_t *sb, sbuf_t *src);

void sbuf_append(sbuf_t *sb, const void *data, int len);
void sbuf_append_from_sbuf(sbuf_t *sb, sbuf_t *src);

void sbuf_cut(sbuf_t *sb, int off);

void sbuf_put_be32(sbuf_t *sb, uint32_t u32);
void sbuf_put_be16(sbuf_t *sb, uint16_t u16);
void sbuf_put_byte(sbuf_t *sb, uint8_t u8);

ssize_t sbuf_read(sbuf_t *sb, int fd);

static inline uint8_t sbuf_peek_u8(sbuf_t *sb, int off) { return sb->sb_data[off]; }
static inline  int8_t sbuf_peek_s8(sbuf_t *sb, int off) { return sb->sb_data[off]; }
uint16_t sbuf_peek_u16(sbuf_t *sb, int off);
static inline int16_t sbuf_peek_s16(sbuf_t *sb, int off) { return sbuf_peek_u16(sb, off); }
uint16_t sbuf_peek_u16le(sbuf_t *sb, int off);
static inline int16_t sbuf_peek_s16le(sbuf_t *sb, int off) { return sbuf_peek_u16le(sb, off); }
uint16_t sbuf_peek_u16be(sbuf_t *sb, int off);
static inline int16_t sbuf_peek_s16be(sbuf_t *sb, int off) { return sbuf_peek_u16be(sb, off); }
uint32_t sbuf_peek_u32(sbuf_t *sb, int off);
static inline int32_t sbuf_peek_s32(sbuf_t *sb, int off) { return sbuf_peek_u32(sb, off); }
uint32_t sbuf_peek_u32le(sbuf_t *sb, int off);
static inline int32_t sbuf_peek_s32le(sbuf_t *sb, int off) { return sbuf_peek_u32le(sb, off); }
uint32_t sbuf_peek_u32be(sbuf_t *sb, int off);
static inline  int32_t sbuf_peek_s32be(sbuf_t *sb, int off) { return sbuf_peek_u32be(sb, off); }
static inline uint8_t *sbuf_peek(sbuf_t *sb, int off) { return sb->sb_data + off; }

#endif /* __TVH_SBUF_H */
