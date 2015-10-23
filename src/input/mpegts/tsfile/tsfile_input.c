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
#include "input.h"
#include "input/mpegts/dvb.h"
#include "tvhpoll.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sched.h>

extern const idclass_t mpegts_input_class;


static void *
tsfile_input_thread ( void *aux )
{
  int fd = -1, nfds;
  size_t len, rem;
  ssize_t c;
  tvhpoll_t *efd;
  tvhpoll_event_t ev;
  struct stat st;
  sbuf_t buf;
  int64_t pcr, pcr_last = PTS_UNSET;
#if PLATFORM_LINUX
  int64_t pcr_last_realtime = 0;
#endif
  tsfile_input_t *mi = aux;
  mpegts_mux_instance_t *mmi;
  tsfile_mux_instance_t *tmi;

  /* Open file */
  pthread_mutex_lock(&global_lock);

  if ((mmi = LIST_FIRST(&mi->mi_mux_active))) {
    tmi = (tsfile_mux_instance_t*)mmi;
    fd  = tvh_open(tmi->mmi_tsfile_path, O_RDONLY | O_NONBLOCK, 0);
    if (fd == -1)
      tvhlog(LOG_ERR, "tsfile", "open(%s) failed %d (%s)",
             tmi->mmi_tsfile_path, errno, strerror(errno));
    else
      tvhtrace("tsfile", "adapter %d opened %s", mi->mi_instance, tmi->mmi_tsfile_path);
  }
  pthread_mutex_unlock(&global_lock);
  if (fd == -1) return NULL;
  
  /* Polling */
  memset(&ev, 0, sizeof(ev));
  efd = tvhpoll_create(2);
  ev.events          = TVHPOLL_IN;
  ev.fd = ev.data.fd = mi->ti_thread_pipe.rd;
  tvhpoll_add(efd, &ev, 1);

  /* Alloc memory */
  sbuf_init_fixed(&buf, 18800);

  /* Get file length */
  if (fstat(fd, &st)) {
    tvhlog(LOG_ERR, "tsfile", "stat() failed %d (%s)",
           errno, strerror(errno));
    goto exit;
  }

  /* Check for extra (incomplete) packet at end */
  rem = st.st_size % 188;
  len = 0;
  tvhtrace("tsfile", "adapter %d file size %jd rem %zu",
           mi->mi_instance, (intmax_t)st.st_size, rem);
  
  /* Process input */
  while (1) {
      
    /* Find PCR PID */
    if (tmi->mmi_tsfile_pcr_pid == MPEGTS_PID_NONE) { 
      mpegts_service_t *s;
      pthread_mutex_lock(&tsfile_lock);
      LIST_FOREACH(s, &tmi->mmi_mux->mm_services, s_dvb_mux_link) {
        if (s->s_pcr_pid)
          tmi->mmi_tsfile_pcr_pid = s->s_pcr_pid;
      }
      pthread_mutex_unlock(&tsfile_lock);
    }
    
    /* Check for terminate */
    nfds = tvhpoll_wait(efd, &ev, 1, 0);
    if (nfds == 1) break;
    
    /* Read */
    c = sbuf_read(&buf, fd);
    if (c < 0) {
      if (ERRNO_AGAIN(errno))
        continue;
      tvhlog(LOG_ERR, "tsfile", "read() error %d (%s)",
             errno, strerror(errno));
      break;
    }
    len += c;

    /* Reset */
    if (len >= st.st_size) {
      len = 0;
      c -= rem;
      tvhtrace("tsfile", "adapter %d reached eof, resetting", mi->mi_instance);
      lseek(fd, 0, SEEK_SET);
      pcr_last = PTS_UNSET;
    }

    /* Process */
    if (c > 0) {
      pcr = PTS_UNSET;
      mpegts_input_recv_packets((mpegts_input_t*)mi, mmi, &buf,
                                &pcr, &tmi->mmi_tsfile_pcr_pid);

      /* Delay */
      if (pcr != PTS_UNSET) {
        if (pcr_last != PTS_UNSET) {
          struct timespec slp;
          int64_t delta;

          delta = pcr - pcr_last;

          if (delta < 0)
            delta = 0;
          else if (delta > 90000)
            delta = 90000;
          delta *= 11;

#if PLATFORM_LINUX
          delta += pcr_last_realtime;
          slp.tv_sec  = (delta / 1000000);
          slp.tv_nsec = (delta % 1000000) * 1000;
          clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &slp, NULL);
#else
          slp.tv_sec  = (delta / 1000000);
          slp.tv_nsec = (delta % 1000000) * 1000;
          nanosleep(&slp, NULL);
#endif
        }
        pcr_last          = pcr;
#if PLATFORM_LINUX
        pcr_last_realtime = getmonoclock();
#endif
      }
    }
    sched_yield();
  }

exit:
  sbuf_free(&buf);
  tvhpoll_destroy(efd);
  close(fd);
  return NULL;
}

static void
tsfile_input_stop_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  int err;
  tsfile_input_t *ti = (tsfile_input_t*)mi;

  /* Stop thread */
  if (ti->ti_thread_pipe.rd != -1) {
    tvhtrace("tsfile", "adapter %d stopping thread", mi->mi_instance);
    err = tvh_write(ti->ti_thread_pipe.wr, "", 1);
    assert(err != -1);
    pthread_join(ti->ti_thread_id, NULL);
    tvh_pipe_close(&ti->ti_thread_pipe);
    tvhtrace("tsfile", "adapter %d stopped thread", mi->mi_instance);
  }

  mmi->mmi_mux->mm_active = NULL;
  LIST_REMOVE(mmi, mmi_active_link);
}

static int
tsfile_input_start_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *t )
{
  struct stat st;
  mpegts_mux_t          *mm  = t->mmi_mux;
  tsfile_mux_instance_t *mmi = (tsfile_mux_instance_t*)t;
  tsfile_input_t        *ti  = (tsfile_input_t*)mi;
  tvhtrace("tsfile", "adapter %d starting mmi %p", mi->mi_instance, mmi);

  /* Already tuned */
  if (mmi->mmi_mux->mm_active == t) {
    tvhtrace("tsfile", "mmi %p is already active", mmi);
    return 0;
  }
  assert(mmi->mmi_mux->mm_active == NULL);
  assert(LIST_FIRST(&mi->mi_mux_active) == NULL);

  /* Check file is accessible */
  if (lstat(mmi->mmi_tsfile_path, &st)) {
    tvhlog(LOG_ERR, "tsfile", "mmi %p could not stat '%s' (%i)",
           mmi, mmi->mmi_tsfile_path, errno);
    mmi->mmi_tune_failed = 1;
    return SM_CODE_TUNING_FAILED;
  }

  /* Start thread */
  if (ti->ti_thread_pipe.rd == -1) {
    if (tvh_pipe(O_NONBLOCK, &ti->ti_thread_pipe)) {
      mmi->mmi_tune_failed = 1;
      tvhlog(LOG_ERR, "tsfile", "failed to create thread pipe");
      return SM_CODE_TUNING_FAILED;
    }
    tvhtrace("tsfile", "adapter %d starting thread", mi->mi_instance);
    tvhthread_create(&ti->ti_thread_id, NULL, tsfile_input_thread, mi, "tsfile");
  }

  /* Current */
  mmi->mmi_mux->mm_active = t;

  /* Install table handlers */
  psi_tables_install(mi, mm, DVB_SYS_UNKNOWN);

  return 0;
}

tsfile_input_t *
tsfile_input_create ( int idx )
{
  tsfile_input_t *mi;

  /* Create object */
  mi = (tsfile_input_t*)
    mpegts_input_create0(calloc(1, sizeof(tsfile_input_t)),
                         &mpegts_input_class,
                         NULL, NULL);
  mi->mi_instance       = idx;
  mi->mi_enabled        = 1;
  mi->mi_start_mux      = tsfile_input_start_mux;
  mi->mi_stop_mux       = tsfile_input_stop_mux;
  LIST_INSERT_HEAD(&tsfile_inputs, mi, tsi_link);
  if (!mi->mi_name)
    mi->mi_name = strdup("TSFile");

  mi->ti_thread_pipe.rd = mi->ti_thread_pipe.wr = -1;

  /* Start table thread */
  return mi;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
