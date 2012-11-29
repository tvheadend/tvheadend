/**
 *  TV headend - Timeshift Write Handler
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

/* **************************************************************************
 * File Writing
 * *************************************************************************/

/*
 * Write data (retry on EAGAIN)
 */
static ssize_t _write
  ( int fd, const void *buf, size_t count )
{
  ssize_t r;
  size_t  n = 0;
  while ( n < count ) {
    r = write(fd, buf+n, count-n);
    if (r == -1) {
      if (errno == EAGAIN)
        continue;
      else
        return -1;
    }
    n += r;
  }
  return count == n ? n : -1;
}

/*
 * Write message
 */
static ssize_t _write_msg
  ( int fd, streaming_message_type_t type, int64_t time,
    const void *buf, size_t len )
{
  size_t len2 = len + sizeof(type) + sizeof(time);
  ssize_t err, ret;
  ret = err = _write(fd, &len2, sizeof(len2));
  if (err < 0) return err;
  err = _write(fd, &type, sizeof(type));
  if (err < 0) return err;
  ret += err;
  err = _write(fd, &time, sizeof(time));
  if (err < 0) return err;
  ret += err;
  if (len) {
    err = _write(fd, buf, len);
    if (err < 0) return err;
    ret += err;
  }
  return ret;
}

/*
 * Write packet buffer
 */
static int _write_pktbuf ( int fd, pktbuf_t *pktbuf )
{
  ssize_t ret, err;
  if (pktbuf) {
    ret = err = _write(fd, &pktbuf->pb_size, sizeof(pktbuf->pb_size));
    if (err < 0) return err;
    err = _write(fd, pktbuf->pb_data, pktbuf->pb_size);
    if (err < 0) return err;
    ret += err;
  } else {
    size_t sz = 0;
    ret = _write(fd, &sz, sizeof(sz));
  }
  return ret;
}

/*
 * Write signal status
 */
ssize_t timeshift_write_sigstat
  ( int fd, int64_t time, signal_status_t *sigstat )
{
  return _write_msg(fd, SMT_SIGNAL_STATUS, time, sigstat,
                    sizeof(signal_status_t));
}

/*
 * Write packet
 */
ssize_t timeshift_write_packet ( int fd, int64_t time, th_pkt_t *pkt )
{
  ssize_t ret = 0, err;
  ret = err = _write_msg(fd, SMT_PACKET, time, pkt, sizeof(th_pkt_t));
  if (err <= 0) return err;
  err = _write_pktbuf(fd, pkt->pkt_header);
  if (err <= 0) return err;
  ret += err;
  err = _write_pktbuf(fd, pkt->pkt_payload);
  if (err <= 0) return err;
  ret += err;
  return ret;
}

/*
 * Write MPEGTS data
 */
ssize_t timeshift_write_mpegts ( int fd, int64_t time, void *data )
{
  return _write_msg(fd, SMT_MPEGTS, time, data, 188);
}

/*
 * Write skip message
 */
ssize_t timeshift_write_skip ( int fd, streaming_skip_t *skip )
{
  return _write_msg(fd, SMT_SKIP, 0, skip, sizeof(streaming_skip_t));
}

/*
 * Write speed message
 */
ssize_t timeshift_write_speed ( int fd, int speed )
{
  return _write_msg(fd, SMT_SPEED, 0, &speed, sizeof(speed));
}

/*
 * Stop
 */
ssize_t timeshift_write_stop ( int fd, int code )
{
  return _write_msg(fd, SMT_STOP, 0, &code, sizeof(code));
}

/*
 * Exit
 */
ssize_t timeshift_write_exit ( int fd )
{
  int code = 0;
  return _write_msg(fd, SMT_EXIT, 0, &code, sizeof(code));
}

/*
 * Write end of file (special internal message)
 */
ssize_t timeshift_write_eof ( int fd )
{
  size_t sz = 0;
  return _write(fd, &sz, sizeof(sz));
}

/* **************************************************************************
 * Thread
 * *************************************************************************/

static inline ssize_t _process_msg0
  ( timeshift_t *ts, timeshift_file_t *tsf, streaming_message_t **smp )
{
  int i;
  ssize_t err;
  streaming_start_t *ss;
  streaming_message_t *sm = *smp;
  if (sm->sm_type == SMT_START) {
    err = 0;
    timeshift_index_data_t *ti = calloc(1, sizeof(timeshift_index_data_t));
    ti->pos  = tsf->size;
    ti->data = sm;
    *smp = NULL;
    TAILQ_INSERT_TAIL(&tsf->sstart, ti, link);

    /* Update video index */
    ss = sm->sm_data;
    for (i = 0; i < ss->ss_num_components; i++)
      if (SCT_ISVIDEO(ss->ss_components[i].ssc_type))
        ts->vididx = ss->ss_components[i].ssc_index;
  } else if (sm->sm_type == SMT_SIGNAL_STATUS)
    err = timeshift_write_sigstat(tsf->fd, sm->sm_time, sm->sm_data);
  else if (sm->sm_type == SMT_PACKET) {
    err = timeshift_write_packet(tsf->fd, sm->sm_time, sm->sm_data);
    if (err > 0) {
      th_pkt_t *pkt = sm->sm_data;

      /* Index video iframes */
      if (pkt->pkt_componentindex == ts->vididx &&
          pkt->pkt_frametype      == PKT_I_FRAME) {
        timeshift_index_iframe_t *ti = calloc(1, sizeof(timeshift_index_iframe_t));
        ti->pos  = tsf->size;
        ti->time = sm->sm_time;
        TAILQ_INSERT_TAIL(&tsf->iframes, ti, link);
      }
    }
  } else if (sm->sm_type == SMT_MPEGTS)
    err = timeshift_write_mpegts(tsf->fd, sm->sm_time, sm->sm_data);
  else
    err = 0;

  /* OK */
  if (err > 0) {
    tsf->last  = sm->sm_time;
    tsf->size += err;
  }
  return err;
}

static void _process_msg
  ( timeshift_t *ts, streaming_message_t *sm, int *run )
{
  int err;
  timeshift_file_t *tsf;

  /* Process */
  switch (sm->sm_type) {

    /* Terminate */
    case SMT_EXIT:
      if (run) *run = 0;
      break;
    case SMT_STOP:
      if (sm->sm_code == 0 && run)
        *run = 0;
      break;

    /* Timeshifting */
    case SMT_SKIP:
    case SMT_SPEED:
      break;

    /* Status */
    case SMT_NOSTART:
    case SMT_SERVICE_STATUS:
      break;

    /* Store */
    case SMT_SIGNAL_STATUS:
    case SMT_START:
    case SMT_MPEGTS:
    case SMT_PACKET:
      pthread_mutex_lock(&ts->rdwr_mutex);
      if ((tsf = timeshift_filemgr_get(ts, 1)) && (tsf->fd != -1)) {
        if ((err = _process_msg0(ts, tsf, &sm)) < 0) {
          timeshift_filemgr_close(tsf);
          tsf->bad = 1;
          ts->full = 1; ///< Stop any more writing
        }
      }
      pthread_mutex_unlock(&ts->rdwr_mutex);
      break;
  }

  /* Next */
  if (sm)
    streaming_msg_free(sm);
}

void *timeshift_writer ( void *aux )
{
  int run = 1;
  timeshift_t *ts = aux;
  streaming_queue_t *sq = &ts->wr_queue;
  streaming_message_t *sm;

  pthread_mutex_lock(&sq->sq_mutex);

  while (run) {

    /* Get message */
    sm = TAILQ_FIRST(&sq->sq_queue);
    if (sm == NULL) {
      pthread_cond_wait(&sq->sq_cond, &sq->sq_mutex);
      continue;
    }
    TAILQ_REMOVE(&sq->sq_queue, sm, sm_link);
    pthread_mutex_unlock(&sq->sq_mutex);

    _process_msg(ts, sm, &run);

    pthread_mutex_lock(&sq->sq_mutex);
  }

  pthread_mutex_unlock(&sq->sq_mutex);
  return NULL;
}

/* **************************************************************************
 * Utilities
 * *************************************************************************/

void timeshift_writer_flush ( timeshift_t *ts )

{
  streaming_message_t *sm;
  streaming_queue_t *sq = &ts->wr_queue;

  pthread_mutex_lock(&sq->sq_mutex);
  while ((sm = TAILQ_FIRST(&sq->sq_queue))) {
    TAILQ_REMOVE(&sq->sq_queue, sm, sm_link);
    _process_msg(ts, sm, NULL);
  }
  pthread_mutex_unlock(&sq->sq_mutex);
}

