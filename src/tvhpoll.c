/*
 *  TVheadend - poll/select wrapper
 *
 *  Copyright (C) 2013 Adam Sutton
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

#include "tvheadend.h"
#include "tvhpoll.h"

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#if ENABLE_EPOLL
#include <sys/epoll.h>
#elif ENABLE_KQUEUE
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif

#if ENABLE_EPOLL
#define EV_SIZE sizeof(struct epoll_event)
#elif ENABLE_KQUEUE
#define EV_SIZE sizeof(struct kevent)
#endif

struct tvhpoll
{
#if ENABLE_EPOLL
  int fd;
  struct epoll_event *ev;
  int nev;
#elif ENABLE_KQUEUE
  int fd;
  struct kevent *ev;
  int nev;
#else
#endif
};

static void
tvhpoll_alloc ( tvhpoll_t *tp, int n )
{
#if ENABLE_EPOLL || ENABLE_KQUEUE
  if (n > tp->nev) {
    tp->ev  = realloc(tp->ev, n * EV_SIZE);
    tp->nev = n;
  }
#else
#endif
}

tvhpoll_t *
tvhpoll_create ( size_t n )
{
  int fd;
#if ENABLE_EPOLL
  if ((fd = epoll_create1(EPOLL_CLOEXEC)) < 0) {
    tvherror(LS_TVHPOLL, "failed to create epoll [%s]", strerror(errno));
    return NULL;
  }
#elif ENABLE_KQUEUE
  if ((fd = kqueue()) == -1) {
    tvherror(LS_TVHPOLL, "failed to create kqueue [%s]", strerror(errno));
    return NULL;
  }
#else
  fd = -1;
#endif
  tvhpoll_t *tp = calloc(1, sizeof(tvhpoll_t));
  tp->fd = fd;
  tvhpoll_alloc(tp, n);
  return tp;
}

void tvhpoll_destroy ( tvhpoll_t *tp )
{
  if (tp == NULL)
    return;
#if ENABLE_EPOLL || ENABLE_KQUEUE
  free(tp->ev);
  close(tp->fd);
#endif
  free(tp);
}

int tvhpoll_add
  ( tvhpoll_t *tp, tvhpoll_event_t *evs, size_t num )
{
  int i, rc;
#if ENABLE_EPOLL
  for (i = 0; i < num; i++) {
    struct epoll_event ev = { 0 };
    ev.data.ptr = evs[i].ptr;
    if (evs[i].events & TVHPOLL_IN)  ev.events |= EPOLLIN;
    if (evs[i].events & TVHPOLL_OUT) ev.events |= EPOLLOUT;
    if (evs[i].events & TVHPOLL_PRI) ev.events |= EPOLLPRI;
    if (evs[i].events & TVHPOLL_ERR) ev.events |= EPOLLERR;
    if (evs[i].events & TVHPOLL_HUP) ev.events |= EPOLLHUP;
    rc = epoll_ctl(tp->fd, EPOLL_CTL_ADD, evs[i].fd, &ev);
    if (rc && errno == EEXIST) {
      if (epoll_ctl(tp->fd, EPOLL_CTL_MOD, evs[i].fd, &ev))
        return -1;
    }
  }
  return 0;
#elif ENABLE_KQUEUE
  tvhpoll_alloc(tp, num);
  for (i = 0; i < num; i++) {
    if (evs[i].events & TVHPOLL_OUT){
      EV_SET(tp->ev+i, evs[i].fd, EVFILT_WRITE, EV_ADD, 0, 0, evs[i].ptr);
      rc = kevent(tp->fd, tp->ev+i, 1, NULL, 0, NULL);
      if (rc == -1) {
        tvherror(LS_TVHPOLL, "failed to add kqueue WRITE filter [%d|%d]",
           evs[i].fd, rc);
        return -1;
      }
    } else {
      EV_SET(tp->ev+i, evs[i].fd, EVFILT_WRITE,  EV_DELETE, 0, 0, NULL);
      kevent(tp->fd, tp->ev+i, 1, NULL, 0, NULL);
    }
    if (evs[i].events & TVHPOLL_IN){
      EV_SET(tp->ev+i, evs[i].fd, EVFILT_READ, EV_ADD, 0, 0, evs[i].ptr);
      rc = kevent(tp->fd, tp->ev+i, 1, NULL, 0, NULL);
      if (rc == -1) {
        tvherror(LS_TVHPOLL, "failed to add kqueue READ filter [%d|%d]", evs[i].fd, rc);
        return -1;
      }
    } else {
      EV_SET(tp->ev+i, evs[i].fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
      kevent(tp->fd, tp->ev+i, 1, NULL, 0, NULL);
    }
  }
  return 0;
#else
  return -1;
#endif
}

int tvhpoll_add1
  ( tvhpoll_t *tp, int fd, int events, void *ptr )
{
  tvhpoll_event_t ev = { 0 };
  ev.fd = fd;
  ev.events = events;
  ev.ptr = ptr;
  return tvhpoll_add(tp, &ev, 1);
}

int tvhpoll_rem
  ( tvhpoll_t *tp, tvhpoll_event_t *evs, size_t num )
{
#if ENABLE_EPOLL
  int i;
  for (i = 0; i < num; i++)
    epoll_ctl(tp->fd, EPOLL_CTL_DEL, evs[i].fd, NULL);
#elif ENABLE_KQUEUE
  int i;
  for (i = 0; i < num; i++) {
    EV_SET(tp->ev+i, evs[i].fd, 0, EV_DELETE, 0, 0, NULL);
    if (kevent(tp->fd, tp->ev+i, 1, NULL, 0, NULL) == -1)
      return -1;
  }
#else
#endif
  return 0;
}

int tvhpoll_rem1
  ( tvhpoll_t *tp, int fd )
{
  tvhpoll_event_t ev = { 0 };
  ev.fd = fd;
  return tvhpoll_rem(tp, &ev, 1);
}

int tvhpoll_wait
  ( tvhpoll_t *tp, tvhpoll_event_t *evs, size_t num, int ms )
{
  int nfds = 0, i;
  tvhpoll_alloc(tp, num);
#if ENABLE_EPOLL
  nfds = epoll_wait(tp->fd, tp->ev, num, ms);
  for (i = 0; i < nfds; i++) {
    uint32_t events1 = tp->ev[i].events;
    uint32_t events2 = 0;
    if (events1 & EPOLLIN)  events2 |= TVHPOLL_IN;
    if (events1 & EPOLLOUT) events2 |= TVHPOLL_OUT;
    if (events1 & EPOLLERR) events2 |= TVHPOLL_ERR;
    if (events1 & EPOLLPRI) events2 |= TVHPOLL_PRI;
    if (events1 & EPOLLHUP) events2 |= TVHPOLL_HUP;
    evs[i].events = events2;
    evs[i].fd  = -1;
    evs[i].ptr = tp->ev[i].data.ptr;
  }
#elif ENABLE_KQUEUE
  struct timespec tm, *to = NULL;
  if (ms > 0) {
    tm.tv_sec  = ms / 1000;
    tm.tv_nsec = (ms % 1000) * 1000000LL;
    to = &tm;
  }
  nfds = kevent(tp->fd, NULL, 0, tp->ev, num, to);
  for (i = 0; i < nfds; i++) {
    uint32_t events2 = 0;
    if (tp->ev[i].filter == EVFILT_WRITE) events2 |= TVHPOLL_OUT;
    if (tp->ev[i].filter == EVFILT_READ)  events2 |= TVHPOLL_IN;
    if (tp->ev[i].flags  & EV_ERROR)      events2 |= TVHPOLL_ERR;
    if (tp->ev[i].flags  & EV_EOF)        events2 |= TVHPOLL_HUP;
    evs[i].events = events2;
    evs[i].fd  = -1; /* tp->ev[i].ident */;
    evs[i].ptr = tp->ev[i].udata;
  }
#else
#endif
  return nfds;
}
