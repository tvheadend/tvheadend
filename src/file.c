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
