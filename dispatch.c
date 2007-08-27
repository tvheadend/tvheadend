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

static int epoll_fd;

typedef struct epoll_entry {
  void (*callback)(int events, void *opaque, int fd);
  void *opaque;
  struct epoll_event event;
  int fd;
  int refcnt;
} epoll_entry_t;



typedef struct stimer {
  LIST_ENTRY(stimer) link;
  void (*callback)(void *aux);
  void *aux;
  time_t t;
} stimer_t;

LIST_HEAD(, stimer) dispatch_timers;


static int
stimercmp(stimer_t *a, stimer_t *b)
{
  return a->t - b->t;
}


void *
stimer_add(void (*callback)(void *aux), void *aux, int delta)
{
  time_t now;
  stimer_t *ti = malloc(sizeof(stimer_t));

  time(&now);

  ti->t = now + delta;
  ti->aux = aux;
  ti->callback = callback;

  LIST_INSERT_SORTED(&dispatch_timers, ti, link, stimercmp);
  return ti;
}

void
stimer_del(void *handle)
{
  stimer_t *ti = handle;
  LIST_REMOVE(ti, link);
  free(ti);
}


static int
stimer_next(void)
{
  stimer_t *ti = LIST_FIRST(&dispatch_timers);
  struct timeval tv;
  int64_t next, now, delta;
  
  if(ti == NULL)
    return -1;

  next = (uint64_t)ti->t * 1000000ULL;

  gettimeofday(&tv, NULL);

  now = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;

  delta = next - now;
  return delta < 0 ? 0 : delta / 1000ULL;
}







static void
stimer_dispatch(time_t now)
{
  stimer_t *ti;

  while((ti = LIST_FIRST(&dispatch_timers)) != NULL) {
    if(ti->t > now)
      break;

    LIST_REMOVE(ti, link);
    ti->callback(ti->aux);
    free(ti);
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





#define EPOLL_FDS_PER_ROUND 100

time_t dispatch_clock;

void
dispatcher(void)
{
  struct epoll_entry *e;
  int i, n;

  static struct epoll_event events[EPOLL_FDS_PER_ROUND];

  n = epoll_wait(epoll_fd, events, EPOLL_FDS_PER_ROUND, stimer_next());

  time(&dispatch_clock);

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
		((events[i].events & EPOLLIN)  ? DISPATCH_READ : 0 ) |
		((events[i].events & EPOLLOUT) ? DISPATCH_WRITE : 0 ) |
		((events[i].events & EPOLLPRI) ? DISPATCH_PRI : 0 ),
		e->opaque, e->fd);
  }

  for(i = 0; i < n; i++) {
    e = events[i].data.ptr;
    dispatch_deref(e);
  }

  stimer_dispatch(dispatch_clock);
}
