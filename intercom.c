/*
 *  Intercom, used to send HTSMSG's between main thread 'master' and 'slaves'
 *  Copyright (C) 2008 Andreas Öman
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

#include <pthread.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>

#include <libhts/htsmsg.h>
#include <libhts/htsmsg_binary.h>

#include <libhts/htsq.h>

#include "intercom.h"
#include "dispatch.h"

/**
 * Process response from resolver thread and invoke callback
 */
static void 
icom_socket_callback(int events, void *opaque, int fd)
{
  icom_t *ic = opaque;
  htsmsg_t *m;
  int r, msglen;

  if(ic->rcvbuf == NULL) {
    /* Alloc initial buffer */
    ic->rcvbuf = malloc(4);
    ic->rcvbufsize = 4;
  }

  if(ic->rcvbufptr < 4) {
    msglen = 4;
  } else {
    msglen = (ic->rcvbuf[0] << 24 | ic->rcvbuf[1] << 16 |
	      ic->rcvbuf[2] << 8  | ic->rcvbuf[3]) + 4;
    if(msglen < 4 || msglen > 65536) {
      fprintf(stderr, "Intercom error, invalid message length %d\n", msglen);
      abort();
    }

    if(ic->rcvbufsize < msglen) {
      ic->rcvbufsize = msglen;
      ic->rcvbuf = realloc(ic->rcvbuf, msglen);
    }
  }

  r = read(ic->from_thread_fd[0], ic->rcvbuf + ic->rcvbufptr, 
	   msglen - ic->rcvbufptr);
  if(r < 1) {
    fprintf(stderr, "Intercom error, reading failed (%d)\n", r);
    perror("read");
    abort();
  }

  ic->rcvbufptr += r;

  if(msglen > 4 && ic->rcvbufptr == msglen) {
    m = htsmsg_binary_deserialize(ic->rcvbuf + 4, msglen - 4, NULL);

    ic->cb(ic->opaque, m);
    ic->rcvbufptr = 0;
  }
}

/**
 * Setup async resolver
 */
icom_t *
icom_create(icom_callback_t *cb, void *opaque)
{
  icom_t *ic = calloc(1, sizeof(icom_t));
  int fd;

  ic->cb = cb;
  ic->opaque = opaque;

  if(pipe(ic->to_thread_fd) == -1)
    return NULL;

  if(pipe(ic->from_thread_fd) == -1)
    return NULL;

  fd = ic->from_thread_fd[0];
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
  dispatch_addfd(fd, icom_socket_callback, ic, DISPATCH_READ);
  return ic;
}


/**
 * Send a message from the blocking side of the intercom
 */
int
icom_send_msg_from_thread(icom_t *ic, htsmsg_t *m)
{
  void *buf;
  size_t len;

  if(htsmsg_binary_serialize(m, &buf, &len, -1) < 0)
    return -1;

  write(ic->from_thread_fd[1], buf, len);
  free(buf);
  return 0;
}
