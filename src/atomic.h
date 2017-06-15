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
#include <time.h>

extern pthread_mutex_t atomic_lock;

/*
 * Atomic FETCH and ADD operation
 */

static inline int
atomic_add(volatile int *ptr, int incr)
{
#if ENABLE_ATOMIC32
  return __sync_fetch_and_add(ptr, incr);
#else
  int ret;
  pthread_mutex_lock(&atomic_lock);
  ret = *ptr;
  *ptr += incr;
  pthread_mutex_unlock(&atomic_lock);
  return ret;
#endif
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

static inline int64_t
atomic_add_s64(volatile int64_t *ptr, int64_t incr)
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

static inline time_t
atomic_add_time_t(volatile time_t *ptr, time_t incr)
{
#if ENABLE_ATOMIC_TIME_T
  return __sync_fetch_and_add(ptr, incr);
#else
  time_t ret;
  pthread_mutex_lock(&atomic_lock);
  ret = *ptr;
  *ptr += incr;
  pthread_mutex_unlock(&atomic_lock);
  return ret;
#endif
}

/*
 * Atomic ADD and FETCH operation
 */

static inline int64_t
atomic_pre_add_s64(volatile int64_t *ptr, int64_t incr)
{
#if ENABLE_ATOMIC64
  return __sync_add_and_fetch(ptr, incr);
#else
  int64_t ret;
  pthread_mutex_lock(&atomic_lock);
  *ptr += incr;
  ret = *ptr;
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

/*
 * Atomic ADD and FETCH operation with PEAK (MAX)
 */

static inline int64_t
atomic_pre_add_s64_peak(volatile int64_t *ptr, int64_t incr,
                        volatile int64_t *peak)
{
#if ENABLE_ATOMIC64
  int64_t ret = __sync_add_and_fetch(ptr, incr);
  if (__sync_fetch_and_add(peak, 0) < ret)
    __sync_lock_test_and_set(peak, ret);
  return ret;
#else
  int64_t ret;
  pthread_mutex_lock(&atomic_lock);
  *ptr += incr;
  ret = *ptr;
  if (*peak < ret)
    *peak = ret;
  pthread_mutex_unlock(&atomic_lock);
  return ret;
#endif
}

/*
 * Atomic DEC operation
 */

static inline int
atomic_dec(volatile int *ptr, int decr)
{
#if ENABLE_ATOMIC32
  return __sync_fetch_and_sub(ptr, decr);
#else
  int ret;
  pthread_mutex_lock(&atomic_lock);
  ret = *ptr;
  *ptr -= decr;
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

static inline int64_t
atomic_dec_s64(volatile int64_t *ptr, int64_t decr)
{
#if ENABLE_ATOMIC64
  return __sync_fetch_and_sub(ptr, decr);
#else
  int64_t ret;
  pthread_mutex_lock(&atomic_lock);
  ret = *ptr;
  *ptr -= decr;
  pthread_mutex_unlock(&atomic_lock);
  return ret;
#endif
}

/*
 * Atomic EXCHANGE operation
 */

static inline int
atomic_exchange(volatile int *ptr, int val)
{
#if ENABLE_ATOMIC32
  return  __sync_lock_test_and_set(ptr, val);
#else
  int ret;
  pthread_mutex_lock(&atomic_lock);
  ret = *ptr;
  *ptr = val;
  pthread_mutex_unlock(&atomic_lock);
  return ret;
#endif
}

static inline uint64_t
atomic_exchange_u64(volatile uint64_t *ptr, uint64_t val)
{
#if ENABLE_ATOMIC64
  return  __sync_lock_test_and_set(ptr, val);
#else
  uint64_t ret;
  pthread_mutex_lock(&atomic_lock);
  ret = *ptr;
  *ptr = val;
  pthread_mutex_unlock(&atomic_lock);
  return ret;
#endif
}

static inline int64_t
atomic_exchange_s64(volatile int64_t *ptr, int64_t val)
{
#if ENABLE_ATOMIC64
  return  __sync_lock_test_and_set(ptr, val);
#else
  int64_t ret;
  pthread_mutex_lock(&atomic_lock);
  ret = *ptr;
  *ptr = val;
  pthread_mutex_unlock(&atomic_lock);
  return ret;
#endif
}

static inline time_t
atomic_exchange_time_t(volatile time_t *ptr, time_t val)
{
#if ENABLE_ATOMIC_TIME_T
  return  __sync_lock_test_and_set(ptr, val);
#else
  time_t ret;
  pthread_mutex_lock(&atomic_lock);
  ret = *ptr;
  *ptr = val;
  pthread_mutex_unlock(&atomic_lock);
  return ret;
#endif
}

/*
 * Atomic get operation
 */

static inline int
atomic_get(volatile int *ptr)
{
  return atomic_add(ptr, 0);
}

static inline uint64_t
atomic_get_u64(volatile uint64_t *ptr)
{
  return atomic_add_u64(ptr, 0);
}

static inline int64_t
atomic_get_s64(volatile int64_t *ptr)
{
  return atomic_add_s64(ptr, 0);
}

static inline time_t
atomic_get_time_t(volatile time_t *ptr)
{
  return atomic_add_time_t(ptr, 0);
}

/*
 * Atomic set operation
 */

static inline int
atomic_set(volatile int *ptr, int val)
{
  return atomic_exchange(ptr, val);
}

static inline uint64_t
atomic_set_u64(volatile uint64_t *ptr, uint64_t val)
{
  return atomic_exchange_u64(ptr, val);
}

static inline int64_t
atomic_set_s64(volatile int64_t *ptr, int64_t val)
{
  return atomic_exchange_s64(ptr, val);
}

static inline time_t
atomic_set_time_t(volatile time_t *ptr, time_t val)
{
  return atomic_exchange_time_t(ptr, val);
}

/*
 * Atomic set operation + peak (MAX)
 */

static inline int64_t
atomic_set_s64_peak(volatile int64_t *ptr, int64_t val, volatile int64_t *peak)
{
#if ENABLE_ATOMIC64
  int64_t ret = atomic_exchange_s64(ptr, val);
  if (val > atomic_get_s64(peak))
    atomic_set_s64(peak, val);
  return ret;
#else
  int64_t ret;
  pthread_mutex_lock(&atomic_lock);
  ret = *ptr;
  *ptr = val;
  if (val > *peak)
    *peak = val;
  pthread_mutex_unlock(&atomic_lock);
  return ret;
#endif
}
