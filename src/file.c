/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2008 Andreas Öman
 *
 * Process file functions
 */

#include "tvheadend.h"
#include "file.h"
	
#define MAX_RDBUF_SIZE 8192

size_t file_readall ( int fd, char **outp )
{
  size_t outsize = 0, totalsize = 0;
  char *outbuf = NULL, *n;
  int r;

  while (1) {
    if(totalsize == outsize) {
      n = realloc(outbuf, outsize += MAX_RDBUF_SIZE);
      if (!n) {
        free(outbuf);
        return 0;
      }
      outbuf = n;
    }

    r = read(fd, outbuf + totalsize, outsize - totalsize);
    if(r < 1) {
      if (ERRNO_AGAIN(errno))
        continue;
      break;
    }
    totalsize += r;
  } 

  if (!totalsize) {
    free(outbuf);
    return 0;
  }
	
  *outp = outbuf;
  if (totalsize == outsize) {
    n = realloc(outbuf, outsize += 1);
    if (!n) {
      free(outbuf);
      return 0;
    }
    outbuf = n;
  }
  outbuf[totalsize] = 0;

  return totalsize;
}
