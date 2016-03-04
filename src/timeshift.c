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
#include "config.h"
#include "settings.h"
#include "atomic.h"
#include "access.h"
#include "atomic.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

static int timeshift_index = 0;

struct timeshift_conf timeshift_conf;

/*
 * Packet log
 */
void
timeshift_packet_log0
  ( const char *source, timeshift_t *ts, streaming_message_t *sm )
{
  th_pkt_t *pkt = sm->sm_data;
  tvhtrace("timeshift",
           "ts %d pkt %s - stream %d type %c pts %10"PRId64
           " dts %10"PRId64" dur %10d len %6zu time %14"PRId64,
           ts->id, source,
           pkt->pkt_componentindex,
           pkt_frametype_to_char(pkt->pkt_frametype),
           ts_rescale(pkt->pkt_pts, 1000000),
           ts_rescale(pkt->pkt_dts, 1000000),
           pkt->pkt_duration,
           pktbuf_len(pkt->pkt_payload),
           sm->sm_time);
}

/*
 * Safe values for RAM configuration
 */
static void timeshift_fixup ( void )
{
  if (timeshift_conf.ram_only)
    timeshift_conf.max_size = timeshift_conf.ram_size;
}

/*
 * Intialise global file manager
 */
void timeshift_init ( void )
{
  htsmsg_t *m;

  timeshift_filemgr_init();

  /* Defaults */
  memset(&timeshift_conf, 0, sizeof(timeshift_conf));
  timeshift_conf.idnode.in_class = &timeshift_conf_class;
  timeshift_conf.max_period       = 60;                      // Hr (60mins)
  timeshift_conf.max_size         = 10000 * (size_t)1048576; // 10G

  /* Load settings */
  if ((m = hts_settings_load("timeshift/config"))) {
    idnode_load(&timeshift_conf.idnode, m);
    htsmsg_destroy(m);
    timeshift_fixup();
  }
}

/*
 * Terminate global file manager
 */
void timeshift_term ( void )
{
  timeshift_filemgr_term();
  free(timeshift_conf.path);
  timeshift_conf.path = NULL;
}

/*
 * Changed settings
 */
static void
timeshift_conf_class_changed ( idnode_t *self )
{
  timeshift_fixup();
}

/*
 * Save settings
 */
static htsmsg_t *
timeshift_conf_class_save ( idnode_t *self, char *filename, size_t fsize )
{
  htsmsg_t *m = htsmsg_create_map();
  idnode_save(&timeshift_conf.idnode, m);
  snprintf(filename, fsize, "timeshift/config");
  return m;
}

/*
 * Class
 */
static const void *
timeshift_conf_class_max_size_get ( void *o )
{
  static uint64_t r;
  r = timeshift_conf.max_size / 1048576LL;
  return &r;
}

static int
timeshift_conf_class_max_size_set ( void *o, const void *v )
{
  uint64_t u64 = *(uint64_t *)v * 1048576LL;
  if (u64 != timeshift_conf.max_size) {
    timeshift_conf.max_size = u64;
    return 1;
  }
  return 0;
}

static const void *
timeshift_conf_class_ram_size_get ( void *o )
{
  static uint64_t r;
  r = timeshift_conf.ram_size / 1048576LL;
  return &r;
}

static int
timeshift_conf_class_ram_size_set ( void *o, const void *v )
{
  uint64_t u64 = *(uint64_t *)v * 1048576LL;
  timeshift_conf.ram_segment_size = u64 / 10;
  if (u64 != timeshift_conf.ram_size) {
    timeshift_conf.ram_size = u64;
    return 1;
  }
  return 0;
}

const idclass_t timeshift_conf_class = {
  .ic_snode      = &timeshift_conf.idnode,
  .ic_class      = "timeshift",
  .ic_caption    = N_("Timeshift"),
  .ic_event      = "timeshift",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_changed    = timeshift_conf_class_changed,
  .ic_save       = timeshift_conf_class_save,
  .ic_properties = (const property_t[]){
    {
      .type   = PT_BOOL,
      .id     = "enabled",
      .name   = N_("Enabled"),
      .desc   = N_("Enable/disable timeshift."),
      .off    = offsetof(timeshift_conf_t, enabled),
    },
    {
      .type   = PT_BOOL,
      .id     = "ondemand",
      .name   = N_("On-demand (no first rewind)"),
      /*.desc   = N_("Use timeshift only on-demand. It is started when the first request "
                   "to move in the playback time occurs (fast-forward, rewind, goto)."),
      */
      .desc   = N_("Only activate timeshift when the client makes the first "
                   "rewind, fast-forward or pause request. Note, "
                   "because there is no buffer on the first request "
                   "rewinding is not possible at that point."),
      .off    = offsetof(timeshift_conf_t, ondemand),
    },
    {
      .type   = PT_STR,
      .id     = "path",
      .name   = N_("Storage path"),
      .desc   = N_("Path to where the timeshift data will be stored. "
                   "If nothing is specified this will default to "
                   "CONF_DIR/timeshift/buffer."),
      .off    = offsetof(timeshift_conf_t, path),
    },
    {
      .type   = PT_U32,
      .id     = "max_period",
      .name   = N_("Maximum period (mins)"),
      .desc   = N_("The maximum time period that will be buffered for "
                   "any given (client) subscription."),
      .off    = offsetof(timeshift_conf_t, max_period),
    },
    {
      .type   = PT_BOOL,
      .id     = "unlimited_period",
      .name   = N_("Unlimited time"),
      .desc   = N_("Allow the timeshift buffer to grow unbounded until "
                   "your storage media runs out of space. Warning, "
                   "enabling this option may cause your system to slow "
                   "down or crash completely!"),
      .off    = offsetof(timeshift_conf_t, unlimited_period),
    },
    {
      .type   = PT_S64,
      .id     = "max_size",
      .name   = N_("Maximum size (MB)"),
      .desc   = N_("The maximum combined size of all timeshift buffers. "
                   "If you specify an unlimited period it's highly "
                   "recommended you specify a value here."),
      .set    = timeshift_conf_class_max_size_set,
      .get    = timeshift_conf_class_max_size_get,
    },
    {
      .type   = PT_S64,
      .id     = "ram_size",
      .name   = N_("Maximum RAM size (MB)"),
      .desc   = N_("The maximum RAM (system memory) size for timeshift "
                   "buffers. When free RAM buffers are available, they "
                   "are used for timeshift data in preference to using "
                   "storage."),
      .set    = timeshift_conf_class_ram_size_set,
      .get    = timeshift_conf_class_ram_size_get,
    },
    {
      .type   = PT_BOOL,
      .id     = "unlimited_size",
      .name   = N_("Unlimited size"),
      .desc   = N_("Allow the combined size of all timeshift buffers to "
                   "potentially grow unbounded until your storage media "
                   "runs out of space."),
      .off    = offsetof(timeshift_conf_t, unlimited_size),
    },
    {
      .type   = PT_BOOL,
      .id     = "ram_only",
      .name   = N_("RAM only"),
      .desc   = N_("Only use system RAM for timeshift buffers."),
      .off    = offsetof(timeshift_conf_t, ram_only),
    },
    {
      .type   = PT_BOOL,
      .id     = "ram_fit",
      .name   = N_("Fit to RAM (cut rewind)"),
      .desc   = N_("If possible, maintain the timeshift data in the server memory only. "
                   "This may reduce the amount of allowed rewind time."),
      .off    = offsetof(timeshift_conf_t, ram_fit),
    },
    {}
  }
};

/*
 * Process a packet
 */

static int
timeshift_packet( timeshift_t *ts, streaming_message_t *sm )
{
  th_pkt_t *pkt = sm->sm_data;
  int64_t time;

  time = ts_rescale(pkt->pkt_pts, 1000000);
  if (ts->last_wr_time < time)
    ts->last_wr_time = time;
  sm->sm_time = ts->last_wr_time;
  timeshift_packet_log("wr ", ts, sm);
  streaming_target_deliver2(&ts->wr_queue.sq_st, sm);
  return 0;
}

/*
 * Receive data
 */
static void timeshift_input
  ( void *opaque, streaming_message_t *sm )
{
  int type = sm->sm_type;
  timeshift_t *ts = opaque;
  th_pkt_t *pkt, *pkt2;

  if (ts->exit)
    return;

  /* Control */
  if (type == SMT_SKIP) {
    timeshift_write_skip(ts->rd_pipe.wr, sm->sm_data);
    streaming_msg_free(sm);
  } else if (type == SMT_SPEED) {
    timeshift_write_speed(ts->rd_pipe.wr, sm->sm_code);
    streaming_msg_free(sm);
  } else {

    /* Change PTS/DTS offsets */
    if (ts->packet_mode && ts->start_pts && type == SMT_PACKET) {
      pkt = sm->sm_data;
      pkt2 = pkt_copy_shallow(pkt);
      pkt_ref_dec(pkt);
      sm->sm_data = pkt2;
      pkt2->pkt_pts += ts->start_pts;
      pkt2->pkt_dts += ts->start_pts;
    }

    /* Check for exit */
    else if (type == SMT_EXIT ||
        (type == SMT_STOP && sm->sm_code != SM_CODE_SOURCE_RECONFIGURED))
      ts->exit = 1;

    else if (type == SMT_MPEGTS)
      ts->packet_mode = 0;

    /* Send to the writer thread */
    if (ts->packet_mode) {
      sm->sm_time = ts->last_wr_time;
      if ((type == SMT_PACKET) && !timeshift_packet(ts, sm))
        goto _exit;
    } else {
      if (ts->ref_time == 0) {
        ts->ref_time = getfastmonoclock();
        sm->sm_time = 0;
      } else {
        sm->sm_time = getfastmonoclock() - ts->ref_time;
      }
    }
    streaming_target_deliver2(&ts->wr_queue.sq_st, sm);

    /* Exit/Stop */
_exit:
    if (ts->exit)
      timeshift_write_exit(ts->rd_pipe.wr);
  }
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
  if (!ts->exit)
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

  if (ts->smt_start)
    streaming_start_unref(ts->smt_start);

  if (ts->path)
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
  timeshift_t *ts = calloc(1, sizeof(timeshift_t));

  /* Must hold global lock */
  lock_assert(&global_lock);

  /* Setup structure */
  TAILQ_INIT(&ts->files);
  ts->output     = out;
  ts->path       = NULL;
  ts->max_time   = max_time;
  ts->state      = TS_LIVE;
  ts->exit       = 0;
  ts->full       = 0;
  ts->vididx     = -1;
  ts->id         = timeshift_index;
  ts->ondemand   = timeshift_conf.ondemand;
  ts->dobuf      = ts->ondemand ? 0 : 1;
  ts->packet_mode= 1;
  ts->last_wr_time = 0;
  ts->buf_time   = 0;
  ts->start_pts  = 0;
  ts->ref_time   = 0;
  ts->seek.file  = NULL;
  ts->seek.frame = NULL;
  ts->ram_segments = 0;
  ts->file_segments = 0;
  pthread_mutex_init(&ts->state_mutex, NULL);

  /* Initialise output */
  tvh_pipe(O_NONBLOCK, &ts->rd_pipe);

  /* Initialise input */
  streaming_queue_init(&ts->wr_queue, 0, 0);
  streaming_target_init(&ts->input, timeshift_input, ts, 0);
  tvhthread_create(&ts->wr_thread, NULL, timeshift_writer, ts, "tshift-wr");
  tvhthread_create(&ts->rd_thread, NULL, timeshift_reader, ts, "tshift-rd");

  /* Update index */
  timeshift_index++;

  return &ts->input;
}
