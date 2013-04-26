/*
 *  Tvheadend - TS file input system
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

#include "tvheadend.h"
#include "tsfile_private.h"

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

static void *
tsfile_input_thread ( void *aux )
{
  int fd = -1, efd, nfds;
  size_t len, rem;
  ssize_t c;
  struct epoll_event ev;
  struct stat st;
  uint8_t tsb[188*10];
  int64_t pcr, pcr_last = PTS_UNSET, pcr_last_realtime = 0;
  uint16_t pcr_pid = 0;
  mpegts_input_t *mi = aux;
  tsfile_mux_instance_t *mmi;

  /* Open file */
  pthread_mutex_lock(&global_lock);
  if (mi->mi_mux_current) {
    mmi = (tsfile_mux_instance_t*)mi->mi_mux_current;
    fd  = tvh_open(mmi->mmi_tsfile_path, O_RDONLY | O_NONBLOCK, 0);
    if (fd == -1) {
      tvhlog(LOG_ERR, "tsfile", "open(%s) failed %d (%s)",
             mmi->mmi_tsfile_path, errno, strerror(errno));
    }
  }
  pthread_mutex_unlock(&global_lock);
  if (fd == -1) return NULL;
  
  /* Polling */
  memset(&ev, 0, sizeof(ev));
  efd = epoll_create(2);
  ev.events  = EPOLLIN;
  ev.data.fd = mi->mi_thread_pipe.rd;
  epoll_ctl(efd, EPOLL_CTL_ADD, ev.data.fd, &ev);

  /* Get file length */
  if (fstat(fd, &st)) {
    tvhlog(LOG_ERR, "tsfile", "stat() failed %d (%s)",
           errno, strerror(errno));
    goto exit;
  }

  /* Check for extra (incomplete) packet at end */
  rem = st.st_size % 188;
  len = 0;
  
  /* Process input */
  while (1) {
    
    /* Check for terminate */
    nfds = epoll_wait(efd, &ev, 1, 0);
    if (nfds == 1) break;
    
    /* Read */
    c = read(fd, tsb, sizeof(tsb));
    if (c < 0) {
      if (errno == EAGAIN || errno == EINTR)
        continue;
      tvhlog(LOG_ERR, "tsfile", "read() error %d (%s)",
             errno, strerror(errno));
      break;
    }
    len += c;

    /* Reset */
    if (len == st.st_size) {
      len = 0;
      c -= rem;
      lseek(fd, 0, SEEK_SET);
      pcr_last = PTS_UNSET;
    }

    /* Process */
    if (c >= 0) {
      pcr = PTS_UNSET;
      mpegts_input_recv_packets(mi, tsb, c, &pcr, &pcr_pid);

      /* Delay */
      if (pcr != PTS_UNSET) {
        if (pcr_last != PTS_UNSET) {
          struct timespec slp;
          int64_t d = pcr - pcr_last;
          if (d < 0)
            d = 0;
          else if (d > 90000)
            d = 90000;
          d *= 11;
          d += pcr_last_realtime;
          slp.tv_sec  = (d / 1000000);
          slp.tv_nsec = (d % 1000000) * 1000;
          clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &slp, NULL);
        }
        pcr_last          = pcr;
        pcr_last_realtime = getmonoclock();
      }
    }
  }

exit:
  close(efd);
  close(fd);
  return NULL;
}

static void
tsfile_input_stop_mux ( mpegts_input_t *mi )
{
  int err;

  /* Stop thread */
  if (mi->mi_thread_pipe.rd != -1) {
    tvhtrace("tsfile", "adapter %d stopping thread", mi->mi_instance);
    err = tvh_write(mi->mi_thread_pipe.wr, "", 1);
    assert(err != -1);
    pthread_join(mi->mi_thread_id, NULL);
    tvh_pipe_close(&mi->mi_thread_pipe);
    tvhtrace("tsfile", "adapter %d stopped thread", mi->mi_instance);
  }
}

static int
tsfile_input_start_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *t )
{
  struct stat st;
  tsfile_mux_instance_t *mmi = (tsfile_mux_instance_t*)t;

  /* Check file is accessible */
  if (lstat(mmi->mmi_tsfile_path, &st))
    return SM_CODE_TUNING_FAILED;

  /* Start thread */
  if (mi->mi_thread_pipe.rd == -1) {
    if (tvh_pipe(O_NONBLOCK, &mi->mi_thread_pipe)) {
      tvhlog(LOG_ERR, "tsfile", "failed to create thread pipe");
      return SM_CODE_TUNING_FAILED;
    }
    tvhtrace("tsfile", "adapter %d starting thread", mi->mi_instance);
    pthread_create(&mi->mi_thread_id, NULL, tsfile_input_thread, mi);
  }

  return 0;
}

static void
tsfile_input_open_service ( mpegts_input_t *mi, mpegts_service_t *t )
{
}

static void
tsfile_input_close_service ( mpegts_input_t *mi, mpegts_service_t *t )
{
}

static void
tsfile_input_open_table ( mpegts_input_t *mi, mpegts_table_t *mt )
{
}

static void
tsfile_input_close_table ( mpegts_input_t *mi, mpegts_table_t *mt )
{
}

mpegts_input_t *
tsfile_input_create ( void )
{
  mpegts_input_t *mi;
  mi = mpegts_input_create0(NULL);
  mi->mi_start_mux     = tsfile_input_start_mux;
  mi->mi_stop_mux      = tsfile_input_stop_mux;
  mi->mi_open_table    = tsfile_input_open_table;
  mi->mi_close_table   = tsfile_input_close_table;
  mi->mi_open_service  = tsfile_input_open_service;
  mi->mi_close_service = tsfile_input_close_service;
  return mi;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
