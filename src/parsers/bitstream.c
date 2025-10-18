/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2007 Andreas Öman
 *
 * Bit stream reader
 */

#include <stdio.h>
#include <inttypes.h>
#include "bitstream.h"


int
init_rbits(bitstream_t *bs, const uint8_t *data, uint32_t bits)
{
  bs->wdata = NULL;
  bs->rdata = data;
  bs->offset = 0;
  bs->len = bits;
  return 0;
}


int
init_wbits(bitstream_t *bs, uint8_t *data, uint32_t bits)
{
  bs->wdata = data;
  bs->rdata = NULL;
  bs->offset = 0;
  bs->len = bits;
  return 0;
}

uint32_t
read_bits(bitstream_t *bs, uint32_t num)
{
  uint32_t r = 0;

  while(num > 0) {
    if(bs->offset >= bs->len)
      return 0;

    num--;

    if(bs->rdata[bs->offset / 8] & (1 << (7 - (bs->offset & 7))))
      r |= 1 << num;

    bs->offset++;
  }
  return r;
}

uint64_t
read_bits64(bitstream_t *bs, uint32_t num)
{
  uint64_t r = 0;

  while(num > 0) {
    if(bs->offset >= bs->len)
      return 0;

    num--;

    if(bs->rdata[bs->offset / 8] & (1 << (7 - (bs->offset & 7))))
      r |= (int64_t)1 << num;

    bs->offset++;
  }
  return r;
}

uint32_t
show_bits(bitstream_t *bs, uint32_t num)
{
  uint32_t r = 0, offset = bs->offset;

  while(num > 0) {
    if(offset >= bs->len)
      return 0;

    num--;

    if(bs->rdata[offset / 8] & (1 << (7 - (offset & 7))))
      r |= 1 << num;

    offset++;
  }
  return r;
}

uint32_t
read_golomb_ue(bitstream_t *bs)
{
  uint32_t b;
  int lzb = -1;

  for(b = 0; !b && !bs_eof(bs) && lzb < 32; lzb++)
    b = read_bits1(bs);

  if (lzb < 0 || lzb > 31)
    return 0;
  return (1 << lzb) - 1 + read_bits(bs, lzb);
}


int32_t
read_golomb_se(bitstream_t *bs)
{
  uint32_t v;
  v = read_golomb_ue(bs);
  if(v == 0)
    return 0;

  return (v & 1) ? ((v + 1) >> 1) : -(v >> 1);
}


void
put_bits(bitstream_t *bs, uint32_t val, uint32_t num)
{
  while(num > 0) {
    if(bs->offset >= bs->len)
      return;

    num--;

    if(val & (1 << num))
      bs->wdata[bs->offset / 8] |= 1 << (7 - (bs->offset & 7));
    else
      bs->wdata[bs->offset / 8] &= ~(1 << (7 - (bs->offset & 7)));

    bs->offset++;
  }
}
