/**
 *  TV headend - Timeshift File Manager
 *  Copyright (C) 2012 Adam Sutton
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "tvheadend.h"
#include "streaming.h"
#include "timeshift.h"
#include "timeshift/private.h"
#include "config.h"
#include "settings.h"
#include "atomic.h"

static int                   timeshift_reaper_run;
static timeshift_file_list_t timeshift_reaper_list;
static pthread_t             timeshift_reaper_thread;
static pthread_mutex_t       timeshift_reaper_lock;
static tvh_cond_t            timeshift_reaper_cond;

uint64_t                     timeshift_total_size;
uint64_t                     timeshift_total_ram_size;

/* **************************************************************************
 * File reaper thread
 * *************************************************************************/

static void* timeshift_reaper_callback ( void *p )
{
  char *dpath;
  timeshift_file_t *tsf;
  timeshift_index_iframe_t *ti;
  timeshift_index_data_t *tid;
  streaming_message_t *sm;
  pthread_mutex_lock(&timeshift_reaper_lock);
  while (timeshift_reaper_run) {

    /* Get next */
    tsf = TAILQ_FIRST(&timeshift_reaper_list);
    if (!tsf) {
      tvh_cond_wait(&timeshift_reaper_cond, &timeshift_reaper_lock);
      continue;
    }
    TAILQ_REMOVE(&timeshift_reaper_list, tsf, link);
    pthread_mutex_unlock(&timeshift_reaper_lock);

    if (tsf->path) {
      tvhtrace(LS_TIMESHIFT, "remove file %s", tsf->path);

      /* Remove */
      unlink(tsf->path);
      dpath = dirname(tsf->path);
      if (rmdir(dpath) == -1)
        if (errno != ENOTEMPTY)
          tvherror(LS_TIMESHIFT, "failed to remove %s [e=%s]", dpath, strerror(errno));
    } else {
      tvhtrace(LS_TIMESHIFT, "remove RAM segment (time %"PRId64", size %"PRId64")",
               tsf->time, (int64_t)tsf->size);
    }

    /* Free memory */
    while ((ti = TAILQ_FIRST(&tsf->iframes))) {
      TAILQ_REMOVE(&tsf->iframes, ti, link);
      free(ti);
    }
    while ((tid = TAILQ_FIRST(&tsf->sstart))) {
      TAILQ_REMOVE(&tsf->sstart, tid, link);
      sm = tid->data;
      streaming_msg_free(sm);
      free(tid);
    }
    free(tsf->path);
    free(tsf->ram);
    free(tsf);

    pthread_mutex_lock(&timeshift_reaper_lock);
  }
  pthread_mutex_unlock(&timeshift_reaper_lock);
  tvhtrace(LS_TIMESHIFT, "reaper thread exit");
  return NULL;
}

static void timeshift_reaper_remove ( timeshift_file_t *tsf )
{
  if (tvhtrace_enabled()) {
    if (tsf->path)
      tvhtrace(LS_TIMESHIFT, "queue file for removal %s", tsf->path);
    else
      tvhtrace(LS_TIMESHIFT, "queue file for removal - RAM segment time %li", (long)tsf->time);
  }
  pthread_mutex_lock(&timeshift_reaper_lock);
  TAILQ_INSERT_TAIL(&timeshift_reaper_list, tsf, link);
  tvh_cond_signal(&timeshift_reaper_cond, 0);
  pthread_mutex_unlock(&timeshift_reaper_lock);
}

/* **************************************************************************
 * File Handling
 * *************************************************************************/

void
timeshift_filemgr_dump0 ( timeshift_t *ts )
{
  timeshift_file_t *tsf;

  if (TAILQ_EMPTY(&ts->files)) {
    tvhtrace(LS_TIMESHIFT, "ts %d file dump - EMPTY", ts->id);
    return;
  }
  TAILQ_FOREACH(tsf, &ts->files, link) {
    tvhtrace(LS_TIMESHIFT, "ts %d (full=%d) file dump tsf %p time %4"PRId64" last %10"PRId64" bad %d refcnt %d",
             ts->id, ts->full, tsf, tsf->time, tsf->last, tsf->bad, tsf->refcount);
  }
}

/*
 * Get root directory
 *
 * TODO: should this be fixed on startup?
 */
static int
timeshift_filemgr_get_root ( char *buf, size_t len )
{
  const char *path = timeshift_conf.path;
  if (!path || !*path) {
    return hts_settings_buildpath(buf, len, "timeshift/buffer");
  } else {
    snprintf(buf, len, "%s/buffer", path);
  }
  return 0;
}

/*
 * Create timeshift directories (for a given instance)
 */
int timeshift_filemgr_makedirs ( int index, char *buf, size_t len )
{
  if (timeshift_filemgr_get_root(buf, len))
    return 1;
  snprintf(buf+strlen(buf), len-strlen(buf), "/%d", index);
  return makedirs(LS_TIMESHIFT, buf, 0700, 0, -1, -1);
}

/*
 * Close file
 */
void timeshift_filemgr_close ( timeshift_file_t *tsf )
{
  uint8_t *ram;
  ssize_t r = timeshift_write_eof(tsf);
  if (r > 0) {
    tsf->size += r;
    atomic_add_u64(&timeshift_total_size, r);
    if (tsf->ram)
      atomic_add_u64(&timeshift_total_ram_size, r);
  }
  if (tsf->ram) {
    /* maintain unused memory block */
    ram = realloc(tsf->ram, tsf->woff);
    if (ram) {
      tsf->ram = ram;
      tsf->ram_size = tsf->woff;
    }
  }
  if (tsf->wfd >= 0)
    close(tsf->wfd);
  tsf->wfd = -1;
}

/*
 * Remove file
 */
void timeshift_filemgr_remove
  ( timeshift_t *ts, timeshift_file_t *tsf, int force )
{
  if (tsf->wfd >= 0)
    close(tsf->wfd);
  assert(tsf->rfd < 0);
  if (tvhtrace_enabled()) {
    if (tsf->path)
      tvhdebug(LS_TIMESHIFT, "ts %d remove %s (size %"PRId64")", ts->id, tsf->path, (int64_t)tsf->size);
    else
      tvhdebug(LS_TIMESHIFT, "ts %d RAM segment remove time %"PRId64" (size %"PRId64", alloc size %"PRId64")",
               ts->id, tsf->time, (int64_t)tsf->size, (int64_t)tsf->ram_size);
  }
  TAILQ_REMOVE(&ts->files, tsf, link);
  if (tsf->path) {
    assert(ts->file_segments > 0);
    ts->file_segments--;
  } else {
    assert(ts->ram_segments > 0);
    ts->ram_segments--;
  }
  atomic_dec_u64(&timeshift_total_size, tsf->size);
  if (tsf->ram)
    atomic_dec_u64(&timeshift_total_ram_size, tsf->size);
  timeshift_reaper_remove(tsf);
}

/*
 * Flush all files
 */
void timeshift_filemgr_flush ( timeshift_t *ts, timeshift_file_t *end )
{
  timeshift_file_t *tsf;
  while ((tsf = TAILQ_FIRST(&ts->files))) {
    if (tsf == end) break;
    timeshift_filemgr_remove(ts, tsf, 1);
  }
}

/*
 *
 */
static timeshift_file_t * timeshift_filemgr_file_init
  ( timeshift_t *ts, int64_t start_time )
{
  timeshift_file_t *tsf;

  tsf = calloc(1, sizeof(timeshift_file_t));
  tsf->time     = mono2sec(start_time) / TIMESHIFT_FILE_PERIOD;
  tsf->last     = start_time;
  tsf->wfd      = -1;
  tsf->rfd      = -1;
  TAILQ_INIT(&tsf->iframes);
  TAILQ_INIT(&tsf->sstart);
  TAILQ_INSERT_TAIL(&ts->files, tsf, link);
  pthread_mutex_init(&tsf->ram_lock, NULL);
  return tsf;
}

/*
 * Get current / new file
 */
timeshift_file_t *timeshift_filemgr_get ( timeshift_t *ts, int64_t start_time )
{
  int fd;
  timeshift_file_t *tsf_tl, *tsf_hd, *tsf_tmp;
  timeshift_index_data_t *ti;
  streaming_message_t *sm;
  char path[PATH_MAX];
  int64_t time;

  /* Return last file */
  if (start_time < 0)
    return timeshift_filemgr_newest(ts);

  /* No space */
  if (ts->full)
    return NULL;

  /* Store to file */
  tsf_tl = TAILQ_LAST(&ts->files, timeshift_file_list);
  time = mono2sec(start_time) / TIMESHIFT_FILE_PERIOD;
  if (!tsf_tl || tsf_tl->time < time ||
      (tsf_tl->ram && tsf_tl->woff >= timeshift_conf.ram_segment_size)) {
    tsf_hd = TAILQ_FIRST(&ts->files);

    /* Close existing */
    if (tsf_tl)
      timeshift_filemgr_close(tsf_tl);

    /* Check period */
    if (!timeshift_conf.unlimited_period &&
        ts->max_time && tsf_hd && tsf_tl) {
      time_t d = (tsf_tl->time - tsf_hd->time) * TIMESHIFT_FILE_PERIOD;
      if (d > (ts->max_time+5)) {
        if (!tsf_hd->refcount) {
          timeshift_filemgr_remove(ts, tsf_hd, 0);
          tsf_hd = NULL;
        } else {
          tvhdebug(LS_TIMESHIFT, "ts %d buffer full", ts->id);
          ts->full = 1;
        }
      }
    }

    /* Check size */
    if (!timeshift_conf.unlimited_size &&
        atomic_pre_add_u64(&timeshift_conf.total_size, 0) >= timeshift_conf.max_size) {

      /* Remove the last file (if we can) */
      if (tsf_hd && !tsf_hd->refcount) {
        timeshift_filemgr_remove(ts, tsf_hd, 0);

      /* Full */
      } else {
        tvhdebug(LS_TIMESHIFT, "ts %d buffer full", ts->id);
        ts->full = 1;
      }
    }

    /* Create new file */
    tsf_tmp = NULL;
    if (!ts->full) {

      tvhtrace(LS_TIMESHIFT, "ts %d RAM total %"PRId64" requested %"PRId64" segment %"PRId64,
                   ts->id, atomic_pre_add_u64(&timeshift_total_ram_size, 0),
                   timeshift_conf.ram_size, timeshift_conf.ram_segment_size);
      while (1) {
        if (timeshift_conf.ram_size >= 8*1024*1024 &&
            atomic_pre_add_u64(&timeshift_total_ram_size, 0) <
              timeshift_conf.ram_size + (timeshift_conf.ram_segment_size / 2)) {
          tsf_tmp = timeshift_filemgr_file_init(ts, start_time);
          tsf_tmp->ram_size = MIN(16*1024*1024, timeshift_conf.ram_segment_size);
          tsf_tmp->ram = malloc(tsf_tmp->ram_size);
          if (!tsf_tmp->ram) {
            free(tsf_tmp);
            tsf_tmp = NULL;
          } else {
            tvhtrace(LS_TIMESHIFT, "ts %d create RAM segment with %"PRId64" bytes (time %"PRId64")",
                     ts->id, tsf_tmp->ram_size, start_time);
            ts->ram_segments++;
          }
          break;
        } else {
          tsf_hd = TAILQ_FIRST(&ts->files);
          if (timeshift_conf.ram_fit && tsf_hd && !tsf_hd->refcount &&
              tsf_hd->ram && ts->file_segments == 0) {
            tvhtrace(LS_TIMESHIFT, "ts %d remove RAM segment %"PRId64" (fit)", ts->id, tsf_hd->time);
            timeshift_filemgr_remove(ts, tsf_hd, 0);
          } else {
            break;
          }
        }
      }
      
      if (!tsf_tmp && !timeshift_conf.ram_only) {
        /* Create directories */
        if (!ts->path) {
          if (timeshift_filemgr_makedirs(ts->id, path, sizeof(path)))
            return NULL;
          ts->path = strdup(path);
        }

        /* Create File */
        snprintf(path, sizeof(path), "%s/tvh-%"PRId64, ts->path, start_time);
        tvhtrace(LS_TIMESHIFT, "ts %d create file %s", ts->id, path);
        if ((fd = tvh_open(path, O_WRONLY | O_CREAT, 0600)) > 0) {
          tsf_tmp = timeshift_filemgr_file_init(ts, start_time);
          tsf_tmp->wfd = fd;
          tsf_tmp->path = strdup(path);
          ts->file_segments++;
        }
      }

      if (tsf_tmp && tsf_tl) {
        /* Copy across last start message */
        if ((ti = TAILQ_LAST(&tsf_tl->sstart, timeshift_index_data_list)) || ts->smt_start) {
          tvhtrace(LS_TIMESHIFT, "ts %d copy smt_start to new file%s",
                   ts->id, ti ? " (from last file)" : "");
          timeshift_index_data_t *ti2 = calloc(1, sizeof(timeshift_index_data_t));
          if (ti) {
            sm = streaming_msg_clone(ti->data);
          } else {
            sm = streaming_msg_create(SMT_START);
            streaming_start_ref(ts->smt_start);
            sm->sm_data = ts->smt_start;
          }
          ti2->data = sm;
          TAILQ_INSERT_TAIL(&tsf_tmp->sstart, ti2, link);
        }
      }
    }
    timeshift_filemgr_dump(ts);
    tsf_tl = tsf_tmp;
  }

  return timeshift_file_get(tsf_tl);
}

timeshift_file_t *timeshift_filemgr_next
  ( timeshift_file_t *tsf, int *end, int keep )
{
  timeshift_file_t *nxt = TAILQ_NEXT(tsf, link);
  if (!nxt && end)  *end = 1;
  if (!nxt && keep) return tsf;
  timeshift_file_put(tsf);
  return timeshift_file_get(nxt);
}

timeshift_file_t *timeshift_filemgr_prev
  ( timeshift_file_t *tsf, int *end, int keep )
{
  timeshift_file_t *prev = TAILQ_PREV(tsf, timeshift_file_list, link);
  if (!prev && end)  *end = 1;
  if (!prev && keep) return tsf;
  timeshift_file_put(tsf);
  return timeshift_file_get(prev);
}

/*
 * Get the oldest file
 */
timeshift_file_t *timeshift_filemgr_oldest ( timeshift_t *ts )
{
  timeshift_file_t *tsf = TAILQ_FIRST(&ts->files);
  return timeshift_file_get(tsf);
}

/*
 * Get the newest file
 */
timeshift_file_t *timeshift_filemgr_newest ( timeshift_t *ts )
{
  timeshift_file_t *tsf = TAILQ_LAST(&ts->files, timeshift_file_list);
  return timeshift_file_get(tsf);
}

/* **************************************************************************
 * Setup / Teardown
 * *************************************************************************/

/*
 * Initialise global structures
 */
void timeshift_filemgr_init ( void )
{
  char path[512];

  /* Try to remove any rubbish left from last run */
  if(!timeshift_filemgr_get_root(path, sizeof(path)))
    rmtree(path);

  /* Size processing */
  timeshift_total_size = 0;
  timeshift_conf.ram_size = 0;

  /* Start the reaper thread */
  timeshift_reaper_run = 1;
  pthread_mutex_init(&timeshift_reaper_lock, NULL);
  tvh_cond_init(&timeshift_reaper_cond);
  TAILQ_INIT(&timeshift_reaper_list);
  tvhthread_create(&timeshift_reaper_thread, NULL,
                   timeshift_reaper_callback, NULL, "tshift-reap");
}

/*
 * Terminate
 */
void timeshift_filemgr_term ( void )
{
  char path[512];

  /* Wait for thread */
  pthread_mutex_lock(&timeshift_reaper_lock);
  timeshift_reaper_run = 0;
  tvh_cond_signal(&timeshift_reaper_cond, 0);
  pthread_mutex_unlock(&timeshift_reaper_lock);
  pthread_join(timeshift_reaper_thread, NULL);

  /* Remove the lot */
  if (!timeshift_filemgr_get_root(path, sizeof(path)))
    rmtree(path);
}
