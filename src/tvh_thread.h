/*
 *  Tvheadend - mutex functions
 *  Copyright (C) 2018 Jaroslav Kysela
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
#ifndef TVHEADEND_THREAD_H
#define TVHEADEND_THREAD_H

#include <errno.h>
#include <stdint.h>
#include "pthread.h"

typedef struct {
  pthread_cond_t cond;
} tvh_cond_t;

typedef struct {
  pthread_mutex_t mutex;
} tvh_mutex_t;

/*
 *
 */

void tvh_mutex_not_held(const char *file, int line);

static inline void
lock_assert0(tvh_mutex_t *l, const char *file, int line)
{
#if 0 && ENABLE_LOCKOWNER
  assert(l->mutex.__data.__owner == syscall(SYS_gettid));
#else
  if(pthread_mutex_trylock(&l->mutex) == EBUSY)
    return;
  tvh_mutex_not_held(file, line);
#endif
}

#define lock_assert(l) lock_assert0(l, __FILE__, __LINE__)

/*
 *
 */

int tvh_thread_create
  (pthread_t *thread, const pthread_attr_t *attr,
   void *(*start_routine) (void *), void *arg,
   const char *name);

int tvh_thread_kill(pthread_t thread, int sig);
int tvh_thread_renice(int value);

int tvh_mutex_init(tvh_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr);
int tvh_mutex_destroy(tvh_mutex_t *mutex);
int tvh_mutex_lock(tvh_mutex_t *mutex);
int tvh_mutex_trylock(tvh_mutex_t *mutex);
int tvh_mutex_unlock(tvh_mutex_t *mutex);
int tvh_mutex_timedlock(tvh_mutex_t *mutex, int64_t usec);

int tvh_cond_init(tvh_cond_t *cond, int monotonic);
int tvh_cond_destroy(tvh_cond_t *cond);
int tvh_cond_signal(tvh_cond_t *cond, int broadcast);
int tvh_cond_wait(tvh_cond_t *cond, tvh_mutex_t *mutex);
int tvh_cond_timedwait(tvh_cond_t *cond, tvh_mutex_t *mutex, int64_t clock);
int tvh_cond_timedwait_ts(tvh_cond_t *cond, tvh_mutex_t *mutex, struct timespec *ts);

#ifndef TVH_THREAD_C
#define pthread_cond    __do_not_use_pthread_cond
#define pthread_cond_t  __do_not_use_pthread_cond_t
#define pthread_mutex   __do_not_use_pthread_mutex
#define pthread_mutex_t __do_not_use_pthread_mutex_t
#endif

#endif /* TVHEADEND_THREAD_H */
