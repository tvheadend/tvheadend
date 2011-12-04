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

#ifndef HTSATOMIC_H__
#define HTSATOMIC_H__

/**
 * Atomically add 'incr' to *ptr and return the previous value
 */



#if defined(linux) && __GNUC__ >= 4 && __GNUC_MINOR__ >=3 

static inline int
atomic_add(volatile int *ptr, int incr)
{
  return __sync_fetch_and_add(ptr, incr);
}

#elif defined(__i386__) || defined(__x86_64__)
static inline int
atomic_add(volatile int *ptr, int incr)
{
  int r;
  asm volatile("lock; xaddl %0, %1" :
	       "=r"(r), "=m"(*ptr) : "0" (incr), "m" (*ptr) : "memory");
  return r;
}
#elif defined(WII)

#include <ogc/machine/processor.h>

static inline int
atomic_add(volatile int *ptr, int incr)
{
  int r, level;

  /*
   * Last time i checked libogc's context switcher did not do the
   * necessary operations to clear locks held by lwarx/stwcx.
   * Thus we need to resort to other means
   */

  _CPU_ISR_Disable(level);

  r = *ptr;
  *ptr = *ptr + incr;

  _CPU_ISR_Restore(level);

  return r;
}
#elif defined(__ppc__) || defined(__PPC__)

/* somewhat based on code from darwin gcc  */
static inline int
atomic_add (volatile int *ptr, int incr)
{
  int tmp, res;
  asm volatile("0:\n"
               "lwarx  %1,0,%2\n"
               "add%I3 %0,%1,%3\n"
               "stwcx. %0,0,%2\n"
               "bne-   0b\n"
               : "=&r"(tmp), "=&b"(res)
               : "r" (ptr), "Ir"(incr)
               : "cr0", "memory");
  
  return res;
}

#elif defined(__arm__) 

static inline int 
atomic_add(volatile int *ptr, int val)
{
  int a, b, c;

  asm volatile(  "0:\n\t"
		 "ldr %0, [%3]\n\t"
		 "add %1, %0, %4\n\t"
		 "swp %2, %1, [%3]\n\t"
		 "cmp %0, %2\n\t"
		 "swpne %1, %2, [%3]\n\t"
		 "bne 0b"
		 : "=&r" (a), "=&r" (b), "=&r" (c)
		 : "r" (ptr), "r" (val)
		 : "cc", "memory");
  return a;
}

#else
#error Missing atomic ops
#endif

#endif /* HTSATOMIC_H__ */
