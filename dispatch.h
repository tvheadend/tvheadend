/*
 *  socket fd dispathcer
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

#ifndef DISPATCH_H
#define DISPATCH_H

#include <sys/time.h>
#include "tvhead.h"

extern time_t dispatch_clock;

#define DISPATCH_READ  0x1
#define DISPATCH_WRITE 0x2
#define DISPATCH_ERR   0x4
#define DISPATCH_PRI   0x8

extern inline int64_t 
getclock_hires(void)
{
  int64_t now;
  struct timeval tv;

  gettimeofday(&tv, NULL);
  now = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
  return now;
}

int dispatch_init(void);
void *dispatch_addfd(int fd, 
		     void (*callback)(int events, void *opaque, int fd),
		     void *opaque, int flags);
int dispatch_delfd(void *handle);
void dispatch_set(void *handle, int flags);
void dispatch_clr(void *handle, int flags);
void dispatcher(void);

void dtimer_arm(dtimer_t *ti, dti_callback_t *callback, void *aux, int delta);
void dtimer_arm_hires(dtimer_t *ti, dti_callback_t *callback,
		      void *aux, int64_t t);


extern inline void
dtimer_disarm(dtimer_t *ti)
{
  if(ti->dti_callback) {
    LIST_REMOVE(ti, dti_link);
    ti->dti_callback = NULL;
  }
}

#define dtimer_isarmed(ti) ((ti)->dti_callback ? 1 : 0)

#endif /* DISPATCH_H */
