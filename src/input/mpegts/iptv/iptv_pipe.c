/*
 *  IPTV - pipe handler
 *
 *  Copyright (C) 2014 Jaroslav Kysela
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
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>

/*
 * Connect UDP/RTP
 */
static int
iptv_pipe_start ( iptv_mux_t *im, const char *_raw, const url_t *url )
{
  char *argv[64], *f, *raw, *s, *p, *a;
  int i = 1, rd;
  pid_t pid;

  if (strncmp(_raw, "pipe://", 7))
    return -1;

  s = raw = tvh_strdupa(_raw + 7);

  argv[0] = NULL;

  while (*s && *s != ' ')
    s++;
  if (*s == ' ') {
    *(char *)s = '\0';
    s++;
  }

  while (*s && i < ARRAY_SIZE(argv) - 1) {
    f = s;
    while (*s && *s != ' ') {
      while (*s && *s != ' ' && *s != '\\')
        s++;
      if (*s == '\\')
        memmove(s, s + 1, strlen(s));
    }
    if (f != s) {
      if (*s) {
        *(char *)s = '\0';
        s++;
      }
      p = strstr(f, "${service_name}");
      if (p) {
        a = alloca(strlen(f) + strlen(im->mm_iptv_svcname ?: ""));
        *p = '\0';
        strcpy(a, f);
        strcat(a, im->mm_iptv_svcname ?: "");
        strcat(a, p + 15);
        f = a;
      }
      argv[i++] = f;
    }
  }
  argv[i] = NULL;

  if (spawn_and_give_stdout(raw, argv, &rd, &pid, 1)) {
    tvherror("iptv", "Unable to start pipe '%s' (wrong executable?)", raw);
    return -1;
  }

  fcntl(rd, F_SETFD, fcntl(rd, F_GETFD) | FD_CLOEXEC);
  fcntl(rd, F_SETFL, fcntl(rd, F_GETFL) | O_NONBLOCK);

  im->mm_iptv_fd = rd;
  im->im_data = (void *)(intptr_t)pid;

  iptv_input_mux_started(im);
  return 0;
}

static void
iptv_pipe_stop
  ( iptv_mux_t *im )
{
  int rd = im->mm_iptv_fd;
  pid_t pid = (intptr_t)im->im_data;
  spawn_kill(pid, SIGKILL);
  if (rd > 0)
    close(rd);
  im->mm_iptv_fd = -1;
}

static ssize_t
iptv_pipe_read ( iptv_mux_t *im )
{
  int r, rd = im->mm_iptv_fd;
  ssize_t res = 0;
  char buf[64*1024];
  pid_t pid;

  while (rd > 0 && res < sizeof(buf)) {
    r = read(rd, buf, sizeof(buf));
    if (r < 0) {
      if (errno == EAGAIN)
        break;
      if (ERRNO_AGAIN(errno))
        continue;
    }
    if (r <= 0) {
      tvherror("iptv", "stdin pipe unexpectedly closed: %s",
               r < 0 ? strerror(errno) : "No data");
      close(rd);
      pid = (intptr_t)im->im_data;
      spawn_kill(pid, SIGKILL);
      im->mm_iptv_fd = -1;
      im->im_data = NULL;
      break;
    }
    sbuf_append(&im->mm_iptv_buffer, buf, r);
    res += r;
  }

  return res;
}

/*
 * Initialise pipe handler
 */

void
iptv_pipe_init ( void )
{
  static iptv_handler_t ih[] = {
    {
      .scheme = "pipe",
      .start  = iptv_pipe_start,
      .stop   = iptv_pipe_stop,
      .read   = iptv_pipe_read,
    },
  };
  iptv_handler_register(ih, ARRAY_SIZE(ih));
}
