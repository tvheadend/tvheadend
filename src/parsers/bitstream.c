/*
 *  Bit stream reader
 *  Copyright (C) 2007 Andreas Öman
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
      r |= 1 << num;

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

  for(b = 0; !b && !bs_eof(bs); lzb++)
    b = read_bits1(bs);

  if (lzb < 0)
    return 0;
  return (1 << lzb) - 1 + read_bits(bs, lzb);
}


int32_t
read_golomb_se(bitstream_t *bs)
{
  uint32_t v, pos;
  v = read_golomb_ue(bs);
  if(v == 0)
    return 0;

  pos = v & 1;
  v = (v + 1) >> 1;
  return pos ? v : -v;
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
