/*
 *  Process file functions
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

#define MAX_RDBUF_SIZE 8192

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "queue.h"
#include "file.h"
	
typedef struct file_read_buf {
  TAILQ_ENTRY(file_read_buf) link;
  int                        size;
  char                       buf[MAX_RDBUF_SIZE];
} file_read_buf_t;

TAILQ_HEAD(file_read_buf_queue, file_read_buf);

size_t file_readall ( int fd, char **outp )
{
  int r, totalsize = 0;
  struct file_read_buf_queue bufs;
  file_read_buf_t *b = NULL;
  char *outbuf;

  TAILQ_INIT(&bufs);
  while(1) {
    if(b == NULL) {
      b = malloc(sizeof(file_read_buf_t));
      b->size = 0;
      TAILQ_INSERT_TAIL(&bufs, b, link);
    }

    r = read(fd, b->buf + b->size, MAX_RDBUF_SIZE - b->size);
    if(r < 1)
      break;
    b->size += r;
    totalsize += r;
    if(b->size == MAX_RDBUF_SIZE)
      b = NULL;
  } 

  close(fd);

  if(totalsize == 0) {
    free(b);
    *outp = NULL;
    return 0;
  }

  outbuf = malloc(totalsize + 1);
  r = 0;
  while((b = TAILQ_FIRST(&bufs)) != NULL) {
    memcpy(outbuf + r, b->buf, b->size);
    r+= b->size;
    TAILQ_REMOVE(&bufs, b, link);
    free(b);
  }
  assert(r == totalsize);
  *outp = outbuf;
  outbuf[totalsize] = 0;
  return totalsize;
}
