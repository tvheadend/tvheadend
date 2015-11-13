/*
 *  IPTV - file handler
 *
 *  Copyright (C) 2015 Jaroslav Kysela
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
#include "iptv_private.h"
#include "spawn.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <assert.h>

/*
 * Connect UDP/RTP
 */
static int
iptv_file_start ( iptv_mux_t *im, const char *raw, const url_t *url )
{
  int fd = tvh_open(raw + 7, O_RDONLY | O_DIRECT, 0);

  if (fd < 0) {
    tvherror("iptv", "unable to open file '%s'", raw + 7);
    return -1;
  }

  im->mm_iptv_fd = fd;

  iptv_input_mux_started(im);
  return 0;
}

static void
iptv_file_stop
  ( iptv_mux_t *im )
{
  int rd = im->mm_iptv_fd;
  if (rd > 0)
    close(rd);
  im->mm_iptv_fd = -1;
}

static ssize_t
iptv_file_read ( iptv_mux_t *im )
{
  int r, fd = im->mm_iptv_fd;
  ssize_t res = 0;

  while (fd > 0) {
    sbuf_alloc(&im->mm_iptv_buffer, 32*1024);
    r = sbuf_read(&im->mm_iptv_buffer, fd);
    if (r == 0) {
      close(fd);
      im->mm_iptv_fd = -1;
      break;
    }
    if (r < 0) {
      if (errno == EAGAIN)
        break;
      if (ERRNO_AGAIN(errno))
        continue;
    }
    res += r;
  }

  return res;
}

/*
 * Initialise pipe handler
 */

void
iptv_file_init ( void )
{
  static iptv_handler_t ih[] = {
    {
      .scheme = "file",
      .start  = iptv_file_start,
      .stop   = iptv_file_stop,
      .read   = iptv_file_read,
    },
  };
  iptv_handler_register(ih, ARRAY_SIZE(ih));
}
