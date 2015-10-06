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

#ifndef BITSTREAM_H_
#define BITSTREAM_H_

typedef struct bitstream {
  const uint8_t *rdata;
  uint8_t *wdata;
  uint32_t offset;
  uint32_t len;
} bitstream_t;

static inline void skip_bits(bitstream_t *bs, uint32_t num)
  { bs->offset += num; }

static inline void skip_bits1(bitstream_t *bs)
  { bs->offset++; }

int init_rbits(bitstream_t *bs, const uint8_t *data, uint32_t bits);

int init_wbits(bitstream_t *bs, uint8_t *data, uint32_t bits);

uint32_t read_bits(bitstream_t *gb, uint32_t num);

uint64_t read_bits64(bitstream_t *gb, uint32_t num);

uint32_t show_bits(bitstream_t *gb, uint32_t num);

static inline unsigned int read_bits1(bitstream_t *gb)
  { return read_bits(gb, 1); }

unsigned int read_golomb_ue(bitstream_t *gb);

signed int read_golomb_se(bitstream_t *gb);

static inline uint32_t remaining_bits(bitstream_t *gb)
  { return gb->len - gb->offset; }

void put_bits(bitstream_t *bs, uint32_t val, uint32_t num);

static inline int bs_eof(const bitstream_t *bs)
  { return bs->offset >= bs->len; }

static inline uint32_t
RB32(const uint8_t *d)
{
  return (d[0] << 24) | (d[1] << 16) | (d[2] << 8) | d[3];
}

static inline uint32_t
RB24(const uint8_t *d)
{
  return (d[0] << 16) | (d[1] << 8) | d[2];
}

#endif /* BITSTREAM_H_ */
