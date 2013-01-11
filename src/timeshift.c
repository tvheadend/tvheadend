/**
 *  TV headend - Timeshift
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

#include "tvheadend.h"
#include "streaming.h"
#include "timeshift.h"
#include "timeshift/private.h"
#include "config2.h"
#include "settings.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

static int timeshift_index = 0;

uint32_t  timeshift_enabled;
int       timeshift_ondemand;
char     *timeshift_path;
int       timeshift_unlimited_period;
uint32_t  timeshift_max_period;
int       timeshift_unlimited_size;
size_t    timeshift_max_size;

/*
 * Intialise global file manager
 */
void timeshift_init ( void )
{
  htsmsg_t *m;
  const char *str;
  uint32_t u32;

  timeshift_filemgr_init();

  /* Defaults */
  timeshift_enabled          = 0;                       // Disabled
  timeshift_ondemand         = 0;                       // Permanent
  timeshift_path             = NULL;                    // setting dir
  timeshift_unlimited_period = 0;
  timeshift_max_period       = 3600;                    // 1Hr
  timeshift_unlimited_size   = 0;
  timeshift_max_size         = 10000 * (size_t)1048576; // 10G

  /* Load settings */
  if ((m = hts_settings_load("timeshift/config"))) {
    if (!htsmsg_get_u32(m, "enabled", &u32))
      timeshift_enabled = u32 ? 1 : 0;
    if (!htsmsg_get_u32(m, "ondemand", &u32))
      timeshift_ondemand = u32 ? 1 : 0;
    if ((str = htsmsg_get_str(m, "path")))
      timeshift_path = strdup(str);
    if (!htsmsg_get_u32(m, "unlimited_period", &u32))
      timeshift_unlimited_period = u32 ? 1 : 0;
    htsmsg_get_u32(m, "max_period", &timeshift_max_period);
    if (!htsmsg_get_u32(m, "unlimited_size", &u32))
      timeshift_unlimited_size = u32 ? 1 : 0;
    if (!htsmsg_get_u32(m, "max_size", &u32))
      timeshift_max_size = 1048576LL * u32;
    htsmsg_destroy(m);
  }
}

/*
 * Terminate global file manager
 */
void timeshift_term ( void )
{
  timeshift_filemgr_term();
}

/*
 * Save settings
 */
void timeshift_save ( void )
{
  htsmsg_t *m;

  m = htsmsg_create_map();
  htsmsg_add_u32(m, "enabled", timeshift_enabled);
  htsmsg_add_u32(m, "ondemand", timeshift_ondemand);
  if (timeshift_path)
    htsmsg_add_str(m, "path", timeshift_path);
  htsmsg_add_u32(m, "unlimited_period", timeshift_unlimited_period);
  htsmsg_add_u32(m, "max_period", timeshift_max_period);
  htsmsg_add_u32(m, "unlimited_size", timeshift_unlimited_size);
  htsmsg_add_u32(m, "max_size", timeshift_max_size / 1048576);

  hts_settings_save(m, "timeshift/config");
}

/*
 * Receive data
 */
static void timeshift_input
  ( void *opaque, streaming_message_t *sm )
{
  int exit = 0;
  timeshift_t *ts = opaque;

  pthread_mutex_lock(&ts->state_mutex);

  /* Control */
  if (sm->sm_type == SMT_SKIP) {
    if (ts->state >= TS_LIVE)
      timeshift_write_skip(ts->rd_pipe.wr, sm->sm_data);
  } else if (sm->sm_type == SMT_SPEED) {
    if (ts->state >= TS_LIVE)
      timeshift_write_speed(ts->rd_pipe.wr, sm->sm_code);
  }

  else {

    /* Start */
    if (sm->sm_type == SMT_START && ts->state == TS_INIT) {
      ts->state  = TS_LIVE;
    }

    /* Pass-thru */
    if (ts->state <= TS_LIVE) {
      streaming_target_deliver2(ts->output, streaming_msg_clone(sm));
    }

    /* Check for exit */
    if (sm->sm_type == SMT_EXIT ||
        (sm->sm_type == SMT_STOP && sm->sm_code == 0))
      exit = 1;

    /* Buffer to disk */
    if ((ts->state > TS_LIVE) || (!ts->ondemand && (ts->state == TS_LIVE))) {
      sm->sm_time = getmonoclock();
      streaming_target_deliver2(&ts->wr_queue.sq_st, sm);
    } else
      streaming_msg_free(sm);

    /* Exit/Stop */
    if (exit) {
      timeshift_write_exit(ts->rd_pipe.wr);
      ts->state = TS_EXIT;
    }
  }

  pthread_mutex_unlock(&ts->state_mutex);
}

/**
 *
 */
void
timeshift_destroy(streaming_target_t *pad)
{
  timeshift_t *ts = (timeshift_t*)pad;
  streaming_message_t *sm;

  /* Must hold global lock */
  lock_assert(&global_lock);

  /* Ensure the threads exits */
  // Note: this is a workaround for the fact the Q might have been flushed
  //       in reader thread (VERY unlikely)
  pthread_mutex_lock(&ts->state_mutex);
  sm = streaming_msg_create(SMT_EXIT);
  streaming_target_deliver2(&ts->wr_queue.sq_st, sm);
  timeshift_write_exit(ts->rd_pipe.wr);
  pthread_mutex_unlock(&ts->state_mutex);

  /* Wait for all threads */
  pthread_join(ts->rd_thread, NULL);
  pthread_join(ts->wr_thread, NULL);

  /* Shut stuff down */
  streaming_queue_deinit(&ts->wr_queue);

  close(ts->rd_pipe.rd);
  close(ts->rd_pipe.wr);

  /* Flush files */
  timeshift_filemgr_flush(ts, NULL);

  free(ts->path);
  free(ts);
}

/**
 * Create timeshift buffer
 *
 * max_period of buffer in seconds (0 = unlimited)
 * max_size   of buffer in bytes   (0 = unlimited)
 */
streaming_target_t *timeshift_create
  (streaming_target_t *out, time_t max_time)
{
  char buf[512];
  timeshift_t *ts = calloc(1, sizeof(timeshift_t));

  /* Must hold global lock */
  lock_assert(&global_lock);

  /* Create directories */
  if (timeshift_filemgr_makedirs(timeshift_index, buf, sizeof(buf)))
    return NULL;

  /* Setup structure */
  TAILQ_INIT(&ts->files);
  ts->output     = out;
  ts->path       = strdup(buf);
  ts->max_time   = max_time;
  ts->state      = TS_INIT;
  ts->full       = 0;
  ts->vididx     = -1;
  ts->id         = timeshift_index;
  ts->ondemand   = timeshift_ondemand;
  pthread_mutex_init(&ts->rdwr_mutex, NULL);
  pthread_mutex_init(&ts->state_mutex, NULL);

  /* Initialise output */
  tvh_pipe(O_NONBLOCK, &ts->rd_pipe);

  /* Initialise input */
  streaming_queue_init(&ts->wr_queue, 0);
  streaming_target_init(&ts->input, timeshift_input, ts, 0);
  pthread_create(&ts->wr_thread, NULL, timeshift_writer, ts);
  pthread_create(&ts->rd_thread, NULL, timeshift_reader, ts);

  /* Update index */
  timeshift_index++;

  return &ts->input;
}
