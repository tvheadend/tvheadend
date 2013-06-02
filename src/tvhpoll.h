
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

#ifndef __TVHPOLL_H__
#define __TVHPOLL_H__

#include <sys/types.h>

typedef struct tvhpoll tvhpoll_t;
typedef struct tvhpoll_event
{
  int  fd; // input
  int  events;
  union {
    void     *ptr;
    uint64_t u64;
    uint32_t u32;
    int      fd;
  }    data;
} tvhpoll_event_t;

#define TVHPOLL_IN  0x01
#define TVHPOLL_OUT 0x02
#define TVHPOLL_PRI 0x04
#define TVHPOLL_ERR 0x08
#define TVHPOLL_HUP 0x10

tvhpoll_t *tvhpoll_create  ( size_t num );
void       tvhpoll_destroy ( tvhpoll_t *tp );
int        tvhpoll_add
  ( tvhpoll_t *tp, tvhpoll_event_t *evs, size_t num );
int        tvhpoll_rem
  ( tvhpoll_t *tp, tvhpoll_event_t *evs, size_t num );
int        tvhpoll_wait
  ( tvhpoll_t *tp, tvhpoll_event_t *evs, size_t num, int ms );

#endif /* __TVHPOLL_H__ */
