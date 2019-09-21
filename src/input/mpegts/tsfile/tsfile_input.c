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
  mpegts_pcr_t pcr;
  int64_t pcr_last = PTS_UNSET;
  int64_t pcr_last_mono = 0;
  tsfile_input_t *mi = aux;
  mpegts_mux_instance_t *mmi;
  tsfile_mux_instance_t *tmi;

  /* Open file */
  tvh_mutex_lock(&global_lock);

  if ((mmi = LIST_FIRST(&mi->mi_mux_active))) {
    tmi = (tsfile_mux_instance_t*)mmi;
    fd  = tvh_open(tmi->mmi_tsfile_path, O_RDONLY | O_NONBLOCK, 0);
    if (fd == -1)
      tvherror(LS_TSFILE, "open(%s) failed %d (%s)",
               tmi->mmi_tsfile_path, errno, strerror(errno));
    else
      tvhtrace(LS_TSFILE, "adapter %d opened %s", mi->mi_instance, tmi->mmi_tsfile_path);
  }
  tvh_mutex_unlock(&global_lock);
  if (fd == -1) return NULL;
  
  /* Polling */
  efd = tvhpoll_create(2);
  tvhpoll_add1(efd, mi->ti_thread_pipe.rd, TVHPOLL_IN, NULL);

  /* Alloc memory */
  sbuf_init_fixed(&buf, 18800);

  /* Get file length */
  if (fstat(fd, &st)) {
    tvherror(LS_TSFILE, "stat() failed %d (%s)",
             errno, strerror(errno));
    goto exit;
  }

  /* Check for extra (incomplete) packet at end */
  rem = st.st_size % 188;
  len = 0;
  tvhtrace(LS_TSFILE, "adapter %d file size %jd rem %zu",
           mi->mi_instance, (intmax_t)st.st_size, rem);

  pcr_last_mono = getfastmonoclock();
  
  /* Process input */
  while (1) {
      
    /* Find PCR PID */
    if (tmi->mmi_tsfile_pcr_pid == MPEGTS_PID_NONE) { 
      mpegts_service_t *s;
      tvh_mutex_lock(&tsfile_lock);
      LIST_FOREACH(s, &tmi->mmi_mux->mm_services, s_dvb_mux_link) {
        if (s->s_components.set_pcr_pid)
          tmi->mmi_tsfile_pcr_pid = s->s_components.set_pcr_pid;
      }
      tvh_mutex_unlock(&tsfile_lock);
    }
    
    /* Check for terminate */
    nfds = tvhpoll_wait(efd, &ev, 1, 0);
    if (nfds == 1) break;
    
    /* Read */
    c = sbuf_read(&buf, fd);
    if (c < 0) {
      if (ERRNO_AGAIN(errno))
        continue;
      tvherror(LS_TSFILE, "read() error %d (%s)",
               errno, strerror(errno));
      break;
    }
    len += c;

    /* Reset */
    if (len >= st.st_size) {
      len = 0;
      c -= rem;
      tvhtrace(LS_TSFILE, "adapter %d reached eof, resetting", mi->mi_instance);
      lseek(fd, 0, SEEK_SET);
      pcr_last = PTS_UNSET;
    }

    /* Process */
    if (c > 0) {
      pcr.pcr_first = PTS_UNSET;
      pcr.pcr_last  = PTS_UNSET;
      pcr.pcr_pid   = tmi->mmi_tsfile_pcr_pid;
      mpegts_input_recv_packets(mmi, &buf, 0, &pcr);
      if (pcr.pcr_pid)
        tmi->mmi_tsfile_pcr_pid = pcr.pcr_pid;

      /* Delay */
      if (pcr.pcr_first != PTS_UNSET) {
        if (pcr_last != PTS_UNSET) {
          int64_t delta, r;

          delta = pcr.pcr_first - pcr_last;

          if (delta < 0)
            delta = 0;
          else if (delta > 90000)
            delta = 90000;
          delta *= 11;

          do {
            r = tvh_usleep_abs(pcr_last_mono + delta);
          } while (ERRNO_AGAIN(r) || r > 0);
        }
        pcr_last      = pcr.pcr_first;
        pcr_last_mono = getfastmonoclock();
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
    tvhtrace(LS_TSFILE, "adapter %d stopping thread", mi->mi_instance);
    err = tvh_write(ti->ti_thread_pipe.wr, "", 1);
    assert(err != -1);
    pthread_join(ti->ti_thread_id, NULL);
    tvh_pipe_close(&ti->ti_thread_pipe);
    tvhtrace(LS_TSFILE, "adapter %d stopped thread", mi->mi_instance);
  }

  mmi->mmi_mux->mm_active = NULL;
  LIST_REMOVE(mmi, mmi_active_link);
}

static int
tsfile_input_start_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *t, int weight )
{
  struct stat st;
  mpegts_mux_t          *mm  = t->mmi_mux;
  tsfile_mux_instance_t *mmi = (tsfile_mux_instance_t*)t;
  tsfile_input_t        *ti  = (tsfile_input_t*)mi;
  tvhtrace(LS_TSFILE, "adapter %d starting mmi %p", mi->mi_instance, mmi);

  /* Already tuned */
  if (mmi->mmi_mux->mm_active == t) {
    tvhtrace(LS_TSFILE, "mmi %p is already active", mmi);
    return 0;
  }
  assert(mmi->mmi_mux->mm_active == NULL);
  assert(LIST_FIRST(&mi->mi_mux_active) == NULL);

  /* Check file is accessible */
  if (lstat(mmi->mmi_tsfile_path, &st)) {
    tvherror(LS_TSFILE, "mmi %p could not stat '%s' (%i)",
             mmi, mmi->mmi_tsfile_path, errno);
    mmi->mmi_tune_failed = 1;
    return SM_CODE_TUNING_FAILED;
  }

  /* Start thread */
  if (ti->ti_thread_pipe.rd == -1) {
    if (tvh_pipe(O_NONBLOCK, &ti->ti_thread_pipe)) {
      mmi->mmi_tune_failed = 1;
      tvherror(LS_TSFILE, "failed to create thread pipe");
      return SM_CODE_TUNING_FAILED;
    }
    tvhtrace(LS_TSFILE, "adapter %d starting thread", mi->mi_instance);
    tvh_thread_create(&ti->ti_thread_id, NULL, tsfile_input_thread, mi, "tsfile");
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
