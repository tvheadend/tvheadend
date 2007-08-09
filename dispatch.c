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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
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
} epoll_entry_t;


int 
dispatch_init(void)
{
  int fdflag;

  epoll_fd = epoll_create(100);
  if(epoll_fd == -1) {
    perror("epoll_create()");
    exit(1);
  }

  fdflag = fcntl(epoll_fd, F_GETFD);
  fdflag |= FD_CLOEXEC;
  fcntl(epoll_fd, F_SETFD, fdflag);
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
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, e->fd, &e->event);
  dispatch_set(e, flags);
  return e;
}


void 
dispatch_set(void *handle, int flags)
{
  struct epoll_entry *e = handle;

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

  if(flags & DISPATCH_READ)
    e->event.events &= ~EPOLLIN;
  
  if(flags & DISPATCH_WRITE)
    e->event.events &= ~EPOLLOUT;

  epoll_ctl(epoll_fd, EPOLL_CTL_MOD, e->fd, &e->event);
}



void 
dispatch_delfd(void *handle)
{
  struct epoll_entry *e = handle;

  epoll_ctl(epoll_fd, EPOLL_CTL_DEL, e->fd, &e->event);
  free(e);
}




typedef struct dtimer {
  LIST_ENTRY(dtimer) link;
  void (*callback)(void *aux);
  void *aux;
} dtimer_t;

LIST_HEAD(, dtimer) dispatcher_timers;

void *
dispatch_add_1sec_event(void (*callback)(void *aux), void *aux)
{
  dtimer_t *ti = malloc(sizeof(dtimer_t));
  LIST_INSERT_HEAD(&dispatcher_timers, ti, link);
  ti->callback = callback;
  ti->aux = aux;
  return ti;
}

void
dispatch_del_1sec_event(void *p)
{
  dtimer_t *ti = p;
  LIST_REMOVE(ti, link);
  free(ti);
}

static void
run_1sec_events(void)
{
  dtimer_t *ti;
  LIST_FOREACH(ti, &dispatcher_timers, link)
    ti->callback(ti->aux);
}



#define EPOLL_FDS_PER_ROUND 100

time_t dispatch_clock;

void
dispatcher(void)
{
  struct epoll_entry *e;
  int i, n;
  time_t now;

  static struct epoll_event events[EPOLL_FDS_PER_ROUND];

  n = epoll_wait(epoll_fd, events, EPOLL_FDS_PER_ROUND, 1000);

  time(&now);
  if(now != dispatch_clock) {
    run_1sec_events();
    dispatch_clock = now;
  }

  for(i = 0; i < n; i++) {
    e = events[i].data.ptr;

    e->callback(
		((events[i].events & (EPOLLERR | EPOLLHUP)) ?
		 DISPATCH_ERR : 0 ) |
		
		((events[i].events & EPOLLIN) ?
		 DISPATCH_READ : 0 ) |
		
		((events[i].events & EPOLLOUT) ?
		 DISPATCH_WRITE : 0 ),
		
		e->opaque, e->fd);
  }
}
