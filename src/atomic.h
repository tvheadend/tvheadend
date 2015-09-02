/*
 *  Atomic ops
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

extern pthread_mutex_t atomic_lock;

static inline int
atomic_add(volatile int *ptr, int incr)
{
  return __sync_fetch_and_add(ptr, incr);
}

static inline int
atomic_dec(volatile int *ptr, int decr)
{
  return __sync_fetch_and_sub(ptr, decr);
}

static inline int
atomic_exchange(volatile int *ptr, int new)
{
  return  __sync_lock_test_and_set(ptr, new);
}

static inline int
atomic_exchange_u64(volatile uint64_t *ptr, uint64_t new)
{
  return  __sync_lock_test_and_set(ptr, new);
}

static inline uint64_t
atomic_add_u64(volatile uint64_t *ptr, uint64_t incr)
{
#if ENABLE_ATOMIC64
  return __sync_fetch_and_add(ptr, incr);
#else
  uint64_t ret;
  pthread_mutex_lock(&atomic_lock);
  ret = *ptr;
  *ptr += incr;
  pthread_mutex_unlock(&atomic_lock);
  return ret;
#endif
}

static inline uint64_t
atomic_dec_u64(volatile uint64_t *ptr, uint64_t decr)
{
#if ENABLE_ATOMIC64
  return __sync_fetch_and_sub(ptr, decr);
#else
  uint64_t ret;
  pthread_mutex_lock(&atomic_lock);
  ret = *ptr;
  *ptr -= decr;
  pthread_mutex_unlock(&atomic_lock);
  return ret;
#endif
}

static inline uint64_t
atomic_pre_add_u64(volatile uint64_t *ptr, uint64_t incr)
{
#if ENABLE_ATOMIC64
  return __sync_add_and_fetch(ptr, incr);
#else
  uint64_t ret;
  pthread_mutex_lock(&atomic_lock);
  *ptr += incr;
  ret = *ptr;
  pthread_mutex_unlock(&atomic_lock);
  return ret;
#endif
}
