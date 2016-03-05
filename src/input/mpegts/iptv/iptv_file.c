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
#include <signal.h>

typedef struct file_priv {
  int fd;
  int shutdown;
  pthread_t tid;
  tvh_cond_t cond;
} file_priv_t;

/*
 * Read thread
 */
static void *
iptv_file_thread ( void *aux )
{
  iptv_mux_t *im = aux;
  file_priv_t *fp = im->im_data;
  ssize_t r;
  int fd = fp->fd, pause = 0;
  char buf[32*1024];
  off_t off = 0;
  int64_t mono;
  int e;

#if defined(PLATFORM_DARWIN)
  fcntl(fd, F_NOCACHE, 1);
#endif
  pthread_mutex_lock(&iptv_lock);
  while (!fp->shutdown && fd > 0) {
    while (!fp->shutdown && pause) {
      mono = mclk() + sec2mono(1);
      do {
        e = tvh_cond_timedwait(&fp->cond, &iptv_lock, mono);
        if (e == ETIMEDOUT)
          break;
      } while (ERRNO_AGAIN(e));
    }
    if (fp->shutdown)
      break;
    pause = 0;
    pthread_mutex_unlock(&iptv_lock);
    r = read(fd, buf, sizeof(buf));
    pthread_mutex_lock(&iptv_lock);
    if (r == 0)
      break;
    if (r < 0) {
      if (ERRNO_AGAIN(errno))
        continue;
      break;
    }
    sbuf_append(&im->mm_iptv_buffer, buf, r);
    if (iptv_input_recv_packets(im, r) == 1)
      pause = 1;
#ifndef PLATFORM_DARWIN
#if !ENABLE_ANDROID
    posix_fadvise(fd, off, r, POSIX_FADV_DONTNEED);
#endif
#endif
    off += r;
  }
  pthread_mutex_unlock(&iptv_lock);
  return NULL;
}

/*
 * Open file
 */
static int
iptv_file_start ( iptv_mux_t *im, const char *raw, const url_t *url )
{
  file_priv_t *fp;
  int fd = tvh_open(raw + 7, O_RDONLY | O_NONBLOCK, 0);

  if (fd < 0) {
    tvherror("iptv", "unable to open file '%s'", raw + 7);
    return -1;
  }

  fp = calloc(1, sizeof(*fp));
  fp->fd = fd;
  tvh_cond_init(&fp->cond);
  im->im_data = fp;
  iptv_input_mux_started(im);
  tvhthread_create(&fp->tid, NULL, iptv_file_thread, im, "iptvfile");
  return 0;
}

static void
iptv_file_stop
  ( iptv_mux_t *im )
{
  file_priv_t *fp = im->im_data;
  int rd = fp->fd;
  if (rd > 0)
    close(rd);
  fp->shutdown = 1;
  tvh_cond_signal(&fp->cond, 0);
  pthread_mutex_unlock(&iptv_lock);
  pthread_join(fp->tid, NULL);
  tvh_cond_destroy(&fp->cond);
  pthread_mutex_lock(&iptv_lock);
  free(im->im_data);
  im->im_data = NULL;
}

/*
 * Initialise file handler
 */

void
iptv_file_init ( void )
{
  static iptv_handler_t ih[] = {
    {
      .scheme = "file",
      .buffer_limit = 5000,
      .start  = iptv_file_start,
      .stop   = iptv_file_stop
    },
  };
  iptv_handler_register(ih, ARRAY_SIZE(ih));
}
