
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

#ifndef __TVHPOLL_H__
#define __TVHPOLL_H__

#include <stdint.h>
#include <sys/types.h>

typedef struct tvhpoll tvhpoll_t;
typedef struct tvhpoll_event
{
  int         fd; // input
  uint32_t    events;
  void       *ptr;
} tvhpoll_event_t;

#define TVHPOLL_IN  0x01
#define TVHPOLL_OUT 0x02
#define TVHPOLL_PRI 0x04
#define TVHPOLL_ERR 0x08
#define TVHPOLL_HUP 0x10

static inline
tvhpoll_event_t *tvhpoll_event
  (tvhpoll_event_t *ev, int fd, uint32_t events, void *ptr)
{
  ev->fd = fd; ev->events = events; ev->ptr = ptr; return ev;
}

static inline
tvhpoll_event_t *tvhpoll_event1(tvhpoll_event_t *ev, int fd)
{
  ev->fd = fd; ev->events = 0; ev->ptr = NULL; return ev;
}

tvhpoll_t *tvhpoll_create(size_t num);
void tvhpoll_destroy(tvhpoll_t *tp);
void tvhpoll_set_trace(tvhpoll_t *tp, int subsys, int trace);
int tvhpoll_set(tvhpoll_t *tp, tvhpoll_event_t *evs, size_t num);
int tvhpoll_add(tvhpoll_t *tp, tvhpoll_event_t *evs, size_t num);
int tvhpoll_add1(tvhpoll_t *tp, int fd, uint32_t events, void *ptr);
int tvhpoll_rem(tvhpoll_t *tp, tvhpoll_event_t *evs, size_t num);
int tvhpoll_rem1(tvhpoll_t *tp, int fd);
int tvhpoll_wait(tvhpoll_t *tp, tvhpoll_event_t *evs, size_t num, int ms);

#endif /* __TVHPOLL_H__ */
