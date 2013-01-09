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
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "tvheadend.h"
#include "streaming.h"
#include "timeshift.h"
#include "timeshift/private.h"
#include "config2.h"
#include "settings.h"

static int                   timeshift_reaper_run;
static timeshift_file_list_t timeshift_reaper_list;
static pthread_t             timeshift_reaper_thread;
static pthread_mutex_t       timeshift_reaper_lock;
static pthread_cond_t        timeshift_reaper_cond;

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
      pthread_cond_wait(&timeshift_reaper_cond, &timeshift_reaper_lock);
      continue;
    }
    TAILQ_REMOVE(&timeshift_reaper_list, tsf, link);
    pthread_mutex_unlock(&timeshift_reaper_lock);

#ifdef TSHFT_TRACE
    tvhlog(LOG_DEBUG, "timeshift", "remove file %s", tsf->path);
#endif

    /* Remove */
    unlink(tsf->path);
    dpath = dirname(tsf->path);
    if (rmdir(dpath) == -1)
      if (errno != ENOTEMPTY)
        tvhlog(LOG_ERR, "timeshift", "failed to remove %s [e=%s]",
               dpath, strerror(errno));

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
    free(tsf);
  }
  pthread_mutex_unlock(&timeshift_reaper_lock);
#ifdef TSHFT_TRACE
  tvhlog(LOG_DEBUG, "timeshift", "reaper thread exit");
#endif
  return NULL;
}

static void timeshift_reaper_remove ( timeshift_file_t *tsf )
{
#ifdef TSHFT_TRACE
  tvhlog(LOG_DEBUG, "timeshift", "queue file for removal %s", tsf->path);
#endif
  pthread_mutex_lock(&timeshift_reaper_lock);
  TAILQ_INSERT_TAIL(&timeshift_reaper_list, tsf, link);
  pthread_cond_signal(&timeshift_reaper_cond);
  pthread_mutex_unlock(&timeshift_reaper_lock);
}

/* **************************************************************************
 * File Handling
 * *************************************************************************/

/*
 * Get root directory
 *
 * TODO: should this be fixed on startup?
 */
static void timeshift_filemgr_get_root ( char *buf, size_t len )
{
  const char *path = timeshift_path;
  if (!path || !*path) {
    path = hts_settings_get_root();
    snprintf(buf, len, "%s/timeshift/buffer", path);
  } else {
    snprintf(buf, len, "%s/buffer", path);
  }
}

/*
 * Create timeshift directories (for a given instance)
 */
int timeshift_filemgr_makedirs ( int index, char *buf, size_t len )
{
  timeshift_filemgr_get_root(buf, len);
  snprintf(buf+strlen(buf), len-strlen(buf), "/%d", index);
  return makedirs(buf, 0700);
}

/*
 * Close file
 */
void timeshift_filemgr_close ( timeshift_file_t *tsf )
{
  ssize_t r = timeshift_write_eof(tsf->fd);
  if (r > 0)
    tsf->size += r;
  close(tsf->fd);
  tsf->fd = -1;
}

/*
 * Remove file
 */
void timeshift_filemgr_remove
  ( timeshift_t *ts, timeshift_file_t *tsf, int force )
{
  if (tsf->fd != -1)
    close(tsf->fd);
  TAILQ_REMOVE(&ts->files, tsf, link);
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
 * Get current / new file
 */
timeshift_file_t *timeshift_filemgr_get ( timeshift_t *ts, int create )
{
  int fd;
  struct timespec tp;
  timeshift_file_t *tsf_tl, *tsf_hd, *tsf_tmp;
  timeshift_index_data_t *ti;
  char path[512];

  /* Return last file */
  if (!create)
    return TAILQ_LAST(&ts->files, timeshift_file_list);

  /* No space */
  if (ts->full)
    return NULL;

  /* Store to file */
  clock_gettime(CLOCK_MONOTONIC_COARSE, &tp);
  tsf_tl = TAILQ_LAST(&ts->files, timeshift_file_list);
  if (!tsf_tl || tsf_tl->time != tp.tv_sec) {
    tsf_hd = TAILQ_FIRST(&ts->files);

    /* Close existing */
    if (tsf_tl && tsf_tl->fd != -1)
      timeshift_filemgr_close(tsf_tl);

    /* Check period */
    if (ts->max_time && tsf_hd && tsf_tl) {
      time_t d = tsf_tl->time - tsf_hd->time;
      if (d > (ts->max_time+5)) {
        if (!tsf_hd->refcount) {
          timeshift_filemgr_remove(ts, tsf_hd, 0);
        } else {
#ifdef TSHFT_TRACE
          tvhlog(LOG_DEBUG, "timeshift", "ts %d buffer full", ts->id);
#endif
          ts->full = 1;
        }
      }
    }

    /* Check size */
    // TODO: need to implement this

    /* Create new file */
    tsf_tmp = NULL;
    if (!ts->full) {
      snprintf(path, sizeof(path), "%s/tvh-%"PRItime_t, ts->path, tp.tv_sec);
#ifdef TSHFT_TRACE
      tvhlog(LOG_DEBUG, "timeshift", "ts %d create file %s", ts->id, path);
#endif
      if ((fd = open(path, O_WRONLY | O_CREAT, 0600)) > 0) {
        tsf_tmp = calloc(1, sizeof(timeshift_file_t));
        tsf_tmp->time     = tp.tv_sec;
        tsf_tmp->fd       = fd;
        tsf_tmp->path     = strdup(path);
        tsf_tmp->refcount = 0;
        tsf_tmp->last     = getmonoclock();
        TAILQ_INIT(&tsf_tmp->iframes);
        TAILQ_INIT(&tsf_tmp->sstart);
        TAILQ_INSERT_TAIL(&ts->files, tsf_tmp, link);

        /* Copy across last start message */
        if (tsf_tl && (ti = TAILQ_LAST(&tsf_tl->sstart, timeshift_index_data_list))) {
#ifdef TSHFT_TRACE
          tvhlog(LOG_DEBUG, "timeshift", "ts %d copy smt_start to new file",
                 ts->id);
#endif
          timeshift_index_data_t *ti2 = calloc(1, sizeof(timeshift_index_data_t));
          ti2->data = streaming_msg_clone(ti->data);
          TAILQ_INSERT_TAIL(&tsf_tmp->sstart, ti2, link);
        }
      }
    }
    tsf_tl = tsf_tmp;
  }

  return tsf_tl;
}

timeshift_file_t *timeshift_filemgr_next
  ( timeshift_file_t *tsf, int *end, int keep )
{
  timeshift_file_t *nxt = TAILQ_NEXT(tsf, link);
  if (!nxt && end)  *end = 1;
  if (!nxt && keep) return tsf;
  tsf->refcount--;
  if (nxt)
    nxt->refcount++;
  return nxt;
}

timeshift_file_t *timeshift_filemgr_prev
  ( timeshift_file_t *tsf, int *end, int keep )
{
  timeshift_file_t *nxt = TAILQ_PREV(tsf, timeshift_file_list, link);
  if (!nxt && end)  *end = 1;
  if (!nxt && keep) return tsf;
  tsf->refcount--;
  if (nxt)
    nxt->refcount++;
  return nxt;
}

/*
 * Get the oldest file
 */
timeshift_file_t *timeshift_filemgr_last ( timeshift_t *ts )
{
  return TAILQ_FIRST(&ts->files);
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
  timeshift_filemgr_get_root(path, sizeof(path));
  rmtree(path);

  /* Start the reaper thread */
  timeshift_reaper_run = 1;
  pthread_mutex_init(&timeshift_reaper_lock, NULL);
  pthread_cond_init(&timeshift_reaper_cond, NULL);
  TAILQ_INIT(&timeshift_reaper_list);
  pthread_create(&timeshift_reaper_thread, NULL,
                 timeshift_reaper_callback, NULL);
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
  pthread_cond_signal(&timeshift_reaper_cond);
  pthread_mutex_unlock(&timeshift_reaper_lock);
  pthread_join(timeshift_reaper_thread, NULL);

  /* Remove the lot */
  timeshift_filemgr_get_root(path, sizeof(path));
  rmtree(path);
}


