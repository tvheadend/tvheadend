/*
 *  Tvheadend - poll/select wrapper
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
  tvh_mutex_t lock;
#if ENABLE_TRACE
  int trace_subsys;
  int trace_type;
#endif
  uint8_t *events;
  uint32_t events_off;
  uint32_t nevents;
#if ENABLE_EPOLL
  int fd;
  struct epoll_event *ev;
  uint32_t nev;
#elif ENABLE_KQUEUE
  int fd;
  struct kevent *ev;
  uint32_t nev;
#else
#endif
};

static void
tvhpoll_alloc_events ( tvhpoll_t *tp, int fd )
{
  tp->events = calloc(1, tp->nevents = 8);
  tp->events_off = fd;
}

static void
tvhpoll_realloc_events1 ( tvhpoll_t *tp, int fd )
{
  uint32_t diff = tp->events_off - fd;
  uint8_t *evs = malloc(tp->nevents + diff);
  memset(evs, 0, diff);
  memcpy(evs + diff, tp->events, tp->nevents);
  free(tp->events);
  tp->events = evs;
  tp->events_off = fd;
  tp->nevents += diff;
}

static void
tvhpoll_realloc_events2 ( tvhpoll_t *tp, int fd )
{
  uint32_t size = (fd - tp->events_off) + 4;
  tp->events = realloc(tp->events, size);
  memset(tp->events + tp->nevents, 0, size - tp->nevents);
  tp->nevents = size;
}

static inline void
tvhpoll_set_events ( tvhpoll_t *tp, int fd, uint32_t events )
{
  if (tp->nevents == 0) {
    tvhpoll_alloc_events(tp, fd);
  } else if (fd < tp->events_off) {
    tvhpoll_realloc_events1(tp, fd);
  } else if (fd - tp->events_off >= tp->nevents) {
    tvhpoll_realloc_events2(tp, fd);
  }
  assert((events & 0xffffff00) == 0);
  tp->events[fd - tp->events_off] = events;
}

static inline uint32_t
tvhpoll_get_events( tvhpoll_t *tp, int fd )
{
  const uint32_t off = tp->events_off;
  if (fd < off)
    return 0;
  fd -= off;
  if (fd >= tp->nevents)
    return 0;
  return tp->events[fd];
}

static void
tvhpoll_alloc ( tvhpoll_t *tp, uint32_t n )
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
  tvh_mutex_init(&tp->lock, NULL);
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
  free(tp->events);
  tvh_mutex_destroy(&tp->lock);
  free(tp);
}

void tvhpoll_set_trace ( tvhpoll_t *tp, int subsys, int type )
{
#if ENABLE_TRACE
  assert(type == 0 || type == 1);
  tp->trace_subsys = subsys;
  tp->trace_type = type;
#endif
}

static int tvhpoll_add0
  ( tvhpoll_t *tp, tvhpoll_event_t *evs, size_t num )
{
#if ENABLE_EPOLL
  int i;
  for (i = 0; i < num; i++) {
    struct epoll_event ev = { 0 };
    const int fd = evs[i].fd;
    const uint32_t events = evs[i].events;
    const uint32_t oevents = tvhpoll_get_events(tp, fd);
    if (oevents == events) continue;
    ev.data.ptr = evs[i].ptr;
    if (events & TVHPOLL_IN)  ev.events |= EPOLLIN;
    if (events & TVHPOLL_OUT) ev.events |= EPOLLOUT;
    if (events & TVHPOLL_PRI) ev.events |= EPOLLPRI;
    if (events & TVHPOLL_ERR) ev.events |= EPOLLERR;
    if (events & TVHPOLL_HUP) ev.events |= EPOLLHUP;
    if (oevents) {
#if ENABLE_TRACE
      if (tp->trace_type == 1)
        tvhtrace(tp->trace_subsys, "epoll mod: fd=%d events=%x oevents=%x ptr=%p",
                                   fd, events, oevents, evs[i].ptr);
#endif
      if (epoll_ctl(tp->fd, EPOLL_CTL_MOD, fd, &ev)) {
        tvherror(LS_TVHPOLL, "epoll mod failed [%s]", strerror(errno));
        break;
      }
    } else {
#if ENABLE_TRACE
      if (tp->trace_type == 1)
        tvhtrace(tp->trace_subsys, "epoll add: fd=%d events=%x ptr=%p",
                                   fd, events, evs[i].ptr);
#endif
      if (epoll_ctl(tp->fd, EPOLL_CTL_ADD, fd, &ev)) {
        tvherror(LS_TVHPOLL, "epoll add failed [%s]", strerror(errno));
        break;
      }
    }
    tvhpoll_set_events(tp, fd, events);
  }
  return i >= num ? 0 : -1;
#elif ENABLE_KQUEUE
  int i, j;
  struct kevent *ev = alloca(EV_SIZE * num * 2);
  for (i = j = 0; i < num; i++) {
    const int fd = evs[i].fd;
    void *ptr = evs[i].ptr;
    const uint32_t events = evs[i].events;
    const uint32_t oevents = tvhpoll_get_events(tp, fd);
    if (events == oevents) continue;
    tvhpoll_set_events(tp, fd, events);
    /* Unlike poll, the kevent is not a bitmask (on FreeBSD,
     * EVILT_READ=-1, EVFILT_WRITE=-2). That means if you OR them
     * together then you only actually register READ, not WRITE. So,
     * register them separately here.
     */
    if (events & TVHPOLL_OUT) {
      EV_SET(ev+j, fd, EVFILT_WRITE, EV_ADD, 0, 0, ptr);
      j++;
    } else if (oevents & TVHPOLL_OUT) {
      EV_SET(ev+j, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
      j++;
    }
    if (events & TVHPOLL_IN) {
      EV_SET(ev+j, fd, EVFILT_READ, EV_ADD, 0, 0, ptr);
      j++;
    } else if (oevents & TVHPOLL_IN) {
      EV_SET(ev+j, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
      j++;
    }
  }
  return kevent(tp->fd, ev, j, NULL, 0, NULL) >= 0 ? 0 : -1;
#else
  return -1;
#endif
}

int tvhpoll_add
  ( tvhpoll_t *tp, tvhpoll_event_t *evs, size_t num )
{
  int r;

  tvh_mutex_lock(&tp->lock);
  r = tvhpoll_add0(tp, evs, num);
  tvh_mutex_unlock(&tp->lock);
  return r;
}

int tvhpoll_add1
  ( tvhpoll_t *tp, int fd, uint32_t events, void *ptr )
{
  tvhpoll_event_t ev = { 0 };
  ev.fd = fd;
  ev.events = events;
  ev.ptr = ptr;
  return tvhpoll_add(tp, &ev, 1);
}

static int tvhpoll_rem0
  ( tvhpoll_t *tp, tvhpoll_event_t *evs, size_t num )
{
  int r = -1;
#if ENABLE_EPOLL
  int i;
  for (i = 0; i < num; i++) {
    const int fd = evs[i].fd;
    if (tvhpoll_get_events(tp, fd)) {
#if ENABLE_TRACE
      if (tp->trace_type == 1)
        tvhtrace(tp->trace_subsys, "epoll rem: fd=%d events=%x",
                                   fd, tvhpoll_get_events(tp, fd));
#endif
      if (epoll_ctl(tp->fd, EPOLL_CTL_DEL, fd, NULL)) {
        tvherror(LS_TVHPOLL, "epoll del failed [%s]", strerror(errno));
        break;
      }
      tvhpoll_set_events(tp, fd, 0);
    }
  }
  if (i >= num)
    r = 0;
#elif ENABLE_KQUEUE
  int i, j;
  struct kevent *ev = alloca(EV_SIZE * num);
  for (i = j = 0; i < num; i++) {
    const int fd = evs[i].fd;
    if (tvhpoll_get_events(tp, fd)) {
      EV_SET(ev+j, fd, 0, EV_DELETE, 0, 0, NULL);
      j++;
    }
  }
  if (kevent(tp->fd, ev, j, NULL, 0, NULL) >= 0) {
    r = 0;
    for (i = 0; i < j; i++)
      tvhpoll_set_events(tp, ev[i].ident, 0);
  }
#else
#endif
  return r;
}

int tvhpoll_rem
  ( tvhpoll_t *tp, tvhpoll_event_t *evs, size_t num )
{
  int r;

  tvh_mutex_lock(&tp->lock);
  r = tvhpoll_rem0(tp, evs, num);
  tvh_mutex_unlock(&tp->lock);
  return r;
}

int tvhpoll_rem1
  ( tvhpoll_t *tp, int fd )
{
  tvhpoll_event_t ev = { 0 };
  ev.fd = fd;
  return tvhpoll_rem(tp, &ev, 1);
}

int tvhpoll_set
  ( tvhpoll_t *tp, tvhpoll_event_t *evs, size_t num )
{
  tvhpoll_event_t *lev, *ev;
  int i, j, k, r;
  tvh_mutex_lock(&tp->lock);
  lev = alloca(tp->nevents * sizeof(*lev));
  for (i = k = 0; i < tp->nevents; i++)
    if (tp->events[i]) {
      for (j = 0; j < num; j++)
        if (evs[j].fd == i + tp->events_off)
          break;
      if (j >= num) {
        ev = lev + k;
        k++;
        ev->fd = i + tp->events_off;
        ev->events = tp->events[i];
        ev->ptr = 0;
      }
    }
  r = tvhpoll_rem0(tp, lev, k);
  if (r == 0)
    r = tvhpoll_add0(tp, evs, num);
  tvh_mutex_unlock(&tp->lock);
  return r;
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
#if ENABLE_TRACE
    if (tp->trace_type == 1)
      tvhtrace(tp->trace_subsys, "epoll wait: events=%x ptr=%p", events2, tp->ev[i].data.ptr);
#endif
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
