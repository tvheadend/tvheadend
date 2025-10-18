/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2008 Andreas Öman
 *
 * Bit ops
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
#define BIT_MASK(bit) (((bitops_ulong_t)1) << ((bit) % BITS_PER_LONG))

#define TVHLOG_BITARRAY ((LS_LAST + (BITS_PER_LONG - 1)) / BITS_PER_LONG)  //For tvhlog.c and api/api_config.c

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
