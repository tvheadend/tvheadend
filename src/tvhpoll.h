/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Adam Sutton
 *
 * TVheadend - poll/select wrapper
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
