/*****************************************************************************
 * matroska_ebml.c:
 *****************************************************************************
 * Copyright (C) 2005 Mike Matsnev
 * Copyright (C) 2010 Andreas Öman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *****************************************************************************/

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ebml.h"

void
ebml_append_id(htsbuf_queue_t *q, uint32_t id)
{
  uint8_t u8[4] = {id >> 24, id >> 16, id >> 8, id};

  if(u8[0])
    return htsbuf_append(q, u8, 4);
  if(u8[1])
    return htsbuf_append(q, u8+1, 3);
  if(u8[2])
    return htsbuf_append(q, u8+2, 2);
  return htsbuf_append(q, u8+3, 1);
}

void
ebml_append_size(htsbuf_queue_t *q, uint32_t size)
{
  uint8_t u8[5] = { 0x08, size >> 24, size >> 16, size >> 8, size };

  if(size < 0x7f) {
    u8[4] |= 0x80;
    return htsbuf_append(q, u8+4, 1);
  }

  if(size < 0x3fff) {
    u8[3] |= 0x40;
    return htsbuf_append(q, u8+3, 2);
  }
  if(size < 0x1fffff) {
    u8[2] |= 0x20;
    return htsbuf_append(q, u8+2, 3);
  }
  if(size < 0x0fffffff) {
    u8[1] |= 0x10;
    return htsbuf_append(q, u8+1, 4);
  }
  return htsbuf_append(q, u8, 5);
}


void
ebml_append_xiph_size(htsbuf_queue_t *q, int size)
{
  int i;
  uint8_t u8[4] = {0xff, size % 0xff};

  for(i=0; i<size/0xff; i++)
    htsbuf_append(q, u8, 1);

  return htsbuf_append(q, u8+1, 1);
}


void
ebml_append_bin(htsbuf_queue_t *q, unsigned id, const void *data, size_t len)
{
  ebml_append_id(q, id);
  ebml_append_size(q, len);
  return htsbuf_append(q, data, len);
}

void
ebml_append_string(htsbuf_queue_t *q, unsigned id, const char *str)
{
  return ebml_append_bin(q, id, str, strlen(str));
}

void
ebml_append_uint(htsbuf_queue_t *q, unsigned id, int64_t ui)
{
  uint8_t u8[8] = {ui >> 56, ui >> 48, ui >> 40, ui >> 32,
		   ui >> 24, ui >> 16, ui >>  8, ui };
  int i = 0;
  while( i < 7 && !u8[i] )
    ++i;
  return ebml_append_bin(q, id, u8 + i, 8 - i);
}

void
ebml_append_float(htsbuf_queue_t *q, unsigned id, float f)
{
    union
    {
        float f;
        unsigned u;
    } u;
    unsigned char c_f[4];

    u.f = f;
    c_f[0] = u.u >> 24;
    c_f[1] = u.u >> 16;
    c_f[2] = u.u >> 8;
    c_f[3] = u.u;

    return ebml_append_bin(q, id, c_f, 4);
}

void
ebml_append_master(htsbuf_queue_t *q, uint32_t id, htsbuf_queue_t *p)
{
  ebml_append_id(q, id);
  ebml_append_size(q, p->hq_size);
  htsbuf_appendq(q, p);
  free(p);
}

void
ebml_append_void(htsbuf_queue_t *q)
{
  char n[1] = {0};
  ebml_append_bin(q, 0xec, n, 1);
}


void
ebml_append_pad(htsbuf_queue_t *q, size_t pad)
{
  assert(pad > 2);
  pad -= 2;
  assert(pad < 0x3fff);
  pad -= pad > 0x7e;

  void *data = alloca(pad);
  memset(data, 0, pad);
  ebml_append_bin(q, 0xec, data, pad);
}


void
ebml_append_idid(htsbuf_queue_t *q, uint32_t id0, uint32_t id)
{
  uint8_t u8[4] = {id >> 24, id >> 16, id >> 8, id};

  if(u8[0])
    return ebml_append_bin(q, id0, u8, 4);
  if(u8[1])
    return ebml_append_bin(q, id0, u8+1, 3);
  if(u8[2])
    return ebml_append_bin(q, id0, u8+2, 2);
  return ebml_append_bin(q, id0, u8+3, 1);
}
