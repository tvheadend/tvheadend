/*
 *  Bit stream reader
 *  Copyright (C) 2007 Andreas �man
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


void
init_rbits(bitstream_t *bs, const uint8_t *data, int bits)
{
  bs->wdata = NULL;
  bs->rdata = data;
  bs->offset = 0;
  bs->len = bits;
}


void
init_wbits(bitstream_t *bs, uint8_t *data, int bits)
{
  bs->wdata = data;
  bs->rdata = NULL;
  bs->offset = 0;
  bs->len = bits;
}

void
skip_bits(bitstream_t *bs, int num)
{
  bs->offset += num;
}

int
bs_eof(const bitstream_t *bs)
{
  return bs->offset >= bs->len;
}


unsigned int
read_bits(bitstream_t *bs, int num)
{
  int r = 0;

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

unsigned int
read_bits1(bitstream_t *bs)
{
  return read_bits(bs, 1);
}

unsigned int
read_golomb_ue(bitstream_t *bs)
{
  int b, lzb = -1;

  for(b = 0; !b && !bs_eof(bs); lzb++)
    b = read_bits1(bs);

  return (1 << lzb) - 1 + read_bits(bs, lzb);
}


signed int
read_golomb_se(bitstream_t *bs)
{
  int v, pos;
  v = read_golomb_ue(bs);
  if(v == 0)
    return 0;

  pos = v & 1;
  v = (v + 1) >> 1;
  return pos ? v : -v;
}


unsigned int
remaining_bits(bitstream_t *bs)
{
  return bs->len - bs->offset;
}


void
put_bits(bitstream_t *bs, int val, int num)
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
