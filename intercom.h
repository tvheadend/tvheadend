/*
 *  Async intercom
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

#ifndef INTERCOM_H
#define INTERCOM_H

#include <libhts/htsmsg.h>

typedef void (icom_callback_t)(void *opaque, htsmsg_t *m);

typedef struct icom {
  int to_thread_fd[2];
  int from_thread_fd[2];
  
  void *opaque;
  icom_callback_t *cb;

  uint8_t *rcvbuf;
  int rcvbufptr;
  int rcvbufsize;
} icom_t;

icom_t *icom_create(icom_callback_t *cb, void *opaque);

int icom_send_msg_from_thread(icom_t *ic, htsmsg_t *m);

#endif /* INTERCOM_H */
