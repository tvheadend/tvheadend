/*
 *  Bit ops
 *  Copyright (C) 2008 Andreas Öman
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

#pragma once

#include <stdint.h>

#if ENABLE_BITOPS64
#define BITS_PER_LONG 64
typedef uint64_t bitops_ulong_t;
#else
#define BITS_PER_LONG 32
typedef uint32_t bitops_ulong_t;
#endif

#define BIT_WORD(bit) ((bit) / BITS_PER_LONG)
#define BIT_MASK(bit) (1UL << ((bit) % BITS_PER_LONG))

static inline void set_bit(int bit, void *addr)
{
  bitops_ulong_t *p = ((bitops_ulong_t *)addr) + BIT_WORD(bit);
  *p |= BIT_MASK(bit);
}

static inline void clear_bit(int bit, void *addr)
{
  bitops_ulong_t *p = ((bitops_ulong_t *)addr) + BIT_WORD(bit);
  *p &= ~BIT_MASK(bit);
}

static inline int test_bit(int bit, void *addr)
{
  return 1UL & (((bitops_ulong_t *)addr)[BIT_WORD(bit)] >> (bit & (BITS_PER_LONG-1)));
}
