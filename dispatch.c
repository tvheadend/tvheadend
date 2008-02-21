/*
 *  socket fd dispatcher and (very simple) timer handling
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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>

#include <libhts/htsq.h>

#include "dispatch.h"

#define EPOLL_FDS_PER_ROUND 100
time_t dispatch_clock;

static int epoll_fd;

typedef struct epoll_entry {
  void (*callback)(int events, void *opaque, int fd);
  void *opaque;
  struct epoll_event event;
  int fd;
  int refcnt;
} epoll_entry_t;

LIST_HEAD(, dtimer) dispatch_timers;

static int
stimercmp(dtimer_t *a, dtimer_t *b)
{
  if(a->dti_expire < b->dti_expire)
    return -1;
  else if(a->dti_expire > b->dti_expire)
    return 1;
  return 0;
}


void
dtimer_arm_hires(dtimer_t *ti, dti_callback_t *callback, void *aux, int64_t t)
{
  if(ti->dti_callback != NULL)
    LIST_REMOVE(ti, dti_link);
  ti->dti_expire   = t;
  ti->dti_opaque   = aux;
  ti->dti_callback = callback;
  LIST_INSERT_SORTED(&dispatch_timers, ti, dti_link, stimercmp);
}


void
dtimer_arm(dtimer_t *ti, dti_callback_t *callback, void *opaque, int delta)
{
  time_t now;
  time(&now);
  dtimer_arm_hires(ti, callback, opaque,
		   (uint64_t)(now + delta) * 1000000ULL);
}



static int
stimer_next(void)
{
  dtimer_t *ti = LIST_FIRST(&dispatch_timers);
  int delta;
  
  if(ti == NULL)
    return -1;

  delta = (ti->dti_expire - getclock_hires() + 999) / 1000;

  return delta > 0 ? delta : 0;
}

static void
stimer_dispatch(int64_t now)
{
  dtimer_t *ti;
  dti_callback_t *cb;

  while((ti = LIST_FIRST(&dispatch_timers)) != NULL) {
    if(ti->dti_expire > now + 100ULL)
      break;
    LIST_REMOVE(ti, dti_link);
    cb = ti->dti_callback;
    ti->dti_callback = NULL;
    cb(ti->dti_opaque, now);
  }
}


int 
dispatch_init(void)
{
  epoll_fd = epoll_create(100);
  if(epoll_fd == -1) {
    perror("epoll_create()");
    exit(1);
  }

  return 0;
}

void *
dispatch_addfd(int fd, void (*callback)(int events, void *opaque, int fd),
	       void *opaque, int flags)
{
  struct epoll_entry *e;

  fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);

  e = calloc(1, sizeof(struct epoll_entry));
  e->callback = callback;
  e->opaque = opaque;
  e->fd = fd;
  e->event.events = EPOLLERR | EPOLLHUP;
  e->event.data.ptr = e;
  e->refcnt = 1;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, e->fd, &e->event);
  dispatch_set(e, flags);
  return e;
}


void 
dispatch_set(void *handle, int flags)
{
  struct epoll_entry *e = handle;

  if(flags & DISPATCH_PRI)
    e->event.events |= EPOLLPRI;

  if(flags & DISPATCH_READ)
    e->event.events |= EPOLLIN;
  
  if(flags & DISPATCH_WRITE)
    e->event.events |= EPOLLOUT;

  epoll_ctl(epoll_fd, EPOLL_CTL_MOD, e->fd, &e->event);
}


void 
dispatch_clr(void *handle, int flags)
{
  struct epoll_entry *e = handle;

  if(flags & DISPATCH_PRI)
    e->event.events &= ~EPOLLPRI;

  if(flags & DISPATCH_READ)
    e->event.events &= ~EPOLLIN;
  
  if(flags & DISPATCH_WRITE)
    e->event.events &= ~EPOLLOUT;

  epoll_ctl(epoll_fd, EPOLL_CTL_MOD, e->fd, &e->event);
}

static void
dispatch_deref(struct epoll_entry *e)
{
  if(e->refcnt > 1)
    e->refcnt--;
  else
    free(e);
}


int
dispatch_delfd(void *handle)
{
  struct epoll_entry *e = handle;
  int fd = e->fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &e->event);

  e->callback = NULL;
  e->opaque = NULL;
  dispatch_deref(e);
  return fd;
}






void
dispatcher(void)
{
  struct epoll_entry *e;
  int i, n, delta;
  struct timeval tv;
  int64_t now;

  static struct epoll_event events[EPOLL_FDS_PER_ROUND];

  delta = stimer_next();
  n = epoll_wait(epoll_fd, events, EPOLL_FDS_PER_ROUND, delta);

  gettimeofday(&tv, NULL);

  dispatch_clock = tv.tv_sec;
  now = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;


  for(i = 0; i < n; i++) {
    e = events[i].data.ptr;
    e->refcnt++;
  }

  for(i = 0; i < n; i++) {
    e = events[i].data.ptr;
    if(e->callback == NULL)
      continue; /* poll entry has been free'd during loop */

    e->callback(((events[i].events & (EPOLLERR | EPOLLHUP)) ?
		 DISPATCH_ERR : 0 ) |
		((events[i].events & EPOLLIN)  ? DISPATCH_READ  : 0 ) |
		((events[i].events & EPOLLOUT) ? DISPATCH_WRITE : 0 ) |
		((events[i].events & EPOLLPRI) ? DISPATCH_PRI   : 0 ),
		e->opaque, e->fd);
  }

  for(i = 0; i < n; i++) {
    e = events[i].data.ptr;
    dispatch_deref(e);
  }

  stimer_dispatch(now);
}
