/**
 *  TV headend - Timeshift Reader
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
#include "atomic.h"
#include "tvhpoll.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

#if ENABLE_EPOLL
#include <sys/epoll.h>
#elif ENABLE_KQUEUE
#include <sys/event.h>
#include <sys/time.h>
#endif

/* **************************************************************************
 * File Reading
 * *************************************************************************/

static ssize_t _read_buf ( timeshift_file_t *tsf, int fd, void *buf, size_t size )
{
  if (tsf && tsf->ram) {
    if (tsf->roff == tsf->woff) return 0;
    if (tsf->roff + size > tsf->woff) return -1;
    pthread_mutex_lock(&tsf->ram_lock);
    memcpy(buf, tsf->ram + tsf->roff, size);
    tsf->roff += size;
    pthread_mutex_unlock(&tsf->ram_lock);
    return size;
  } else {
    size = read(tsf ? tsf->rfd : fd, buf, size);
    if (size > 0 && tsf)
      tsf->roff += size;
    return size;
  }
}

static ssize_t _read_pktbuf ( timeshift_file_t *tsf, int fd, pktbuf_t **pktbuf )
{
  ssize_t r, cnt = 0;
  size_t sz;

  /* Size */
  r = _read_buf(tsf, fd, &sz, sizeof(sz));
  if (r < 0) return -1;
  if (r != sizeof(sz)) return 0;
  cnt += r;

  /* Empty And Sanity Check */
  if (!sz || sz > 1024 * 1024) {
    *pktbuf = NULL;
    return cnt;
  }

  /* Data */
  *pktbuf = pktbuf_alloc(NULL, sz);
  r = _read_buf(tsf, fd, (*pktbuf)->pb_data, sz);
  if (r != sz) {
    free((*pktbuf)->pb_data);
    free(*pktbuf);
    return r < 0 ? -1 : 0;
  }
  cnt += r;

  return cnt;
}


static ssize_t _read_msg ( timeshift_file_t *tsf, int fd, streaming_message_t **sm )
{
  ssize_t r, cnt = 0;
  size_t sz;
  streaming_message_type_t type;
  int64_t time;
  void *data;
  int code;

  /* Clear */
  *sm = NULL;

  /* Size */
  r = _read_buf(tsf, fd, &sz, sizeof(sz));
  if (r < 0) return -1;
  if (r != sizeof(sz)) return 0;
  cnt += r;

  /* EOF */
  if (sz == 0) return cnt;

  /* Wrong data size */
  if (sz > 1024 * 1024) return -1;

  /* Type */
  r = _read_buf(tsf, fd, &type, sizeof(type));
  if (r < 0) return -1;
  if (r != sizeof(type)) return 0;
  cnt += r;

  /* Time */
  r = _read_buf(tsf, fd, &time, sizeof(time));
  if (r < 0) return -1;
  if (r != sizeof(time)) return 0;
  cnt += r;

  /* Adjust size */
  sz -= sizeof(type) + sizeof(time);
  cnt += sz;

  /* Standard messages */
  switch (type) {

    /* Unhandled */
    case SMT_START:
    case SMT_NOSTART:
    case SMT_NOSTART_WARN:
    case SMT_SERVICE_STATUS:
    case SMT_DESCRAMBLE_INFO:
      return -1;
      break;

    /* Code */
    case SMT_STOP:
    case SMT_EXIT:
    case SMT_SPEED:
      if (sz != sizeof(code)) return -1;
      r = _read_buf(tsf, fd, &code, sz);
      if (r != sz) {
        if (r < 0) return -1;
        return 0;
      }
      *sm = streaming_msg_create_code(type, code);
      break;

    /* Data */
    case SMT_SKIP:
    case SMT_SIGNAL_STATUS:
    case SMT_MPEGTS:
    case SMT_PACKET:
      data = malloc(sz);
      r = _read_buf(tsf, fd, data, sz);
      if (r != sz) {
        free(data);
        if (r < 0) return -1;
        return 0;
      }
      if (type == SMT_PACKET) {
        th_pkt_t *pkt = data;
        pkt->pkt_payload  = pkt->pkt_meta = NULL;
        pkt->pkt_refcount = 0;
        *sm = streaming_msg_create_pkt(pkt);
        r   = _read_pktbuf(tsf, fd, &pkt->pkt_meta);
        if (r < 0) {
          streaming_msg_free(*sm);
          return r;
        }
        cnt += r;
        r   = _read_pktbuf(tsf, fd, &pkt->pkt_payload);
        if (r < 0) {
          streaming_msg_free(*sm);
          return r;
        }
        cnt += r;
      } else {
        *sm = streaming_msg_create_data(type, data);
      }
      (*sm)->sm_time = time;
      break;

    default:
      return -1;
  }

  /* OK */
  return cnt;
}

/* **************************************************************************
 * Utilities
 * *************************************************************************/

static streaming_message_t *_timeshift_find_sstart
  ( timeshift_file_t *tsf, int64_t time )
{
  timeshift_index_data_t *ti;

  /* Find the SMT_START message that relates (comes before) the given time */
  ti = TAILQ_LAST(&tsf->sstart, timeshift_index_data_list);
  while (ti && ti->data->sm_time > time)
    ti = TAILQ_PREV(ti, timeshift_index_data_list, link);

  return ti ? ti->data : NULL;
}

static timeshift_index_iframe_t *_timeshift_first_frame
  ( timeshift_t *ts )
{ 
  int end;
  timeshift_index_iframe_t *tsi = NULL;
  timeshift_file_t *tsf = timeshift_filemgr_oldest(ts);
  while (tsf && !tsi) {
    if (!(tsi = TAILQ_FIRST(&tsf->iframes))) {
      tsf = timeshift_filemgr_next(tsf, &end, 0);
    }
  }
  if (tsf)
    tsf->refcount--;
  return tsi;
}

static timeshift_index_iframe_t *_timeshift_last_frame
  ( timeshift_t *ts )
{
  int end;
  timeshift_index_iframe_t *tsi = NULL;
  timeshift_file_t *tsf = timeshift_filemgr_get(ts, -1);
  while (tsf && !tsi) {
    if (!(tsi = TAILQ_LAST(&tsf->iframes, timeshift_index_iframe_list))) {
      tsf = timeshift_filemgr_prev(tsf, &end, 0);
    }
  }
  if (tsf)
    tsf->refcount--;
  return tsi;
}

static int _timeshift_skip
  ( timeshift_t *ts, int64_t req_time, int64_t cur_time,
    timeshift_file_t *cur_file, timeshift_file_t **new_file,
    timeshift_index_iframe_t **iframe )
{
  timeshift_index_iframe_t *tsi  = *iframe;
  timeshift_file_t         *tsf  = cur_file, *tsf_last;
  int64_t                   sec  = req_time / (1000000 * TIMESHIFT_FILE_PERIOD);
  int                       back = (req_time < cur_time) ? 1 : 0;
  int                       end  = 0;
  
  /* Hold local ref */
  if (cur_file)
    cur_file->refcount++;

  /* Coarse search */
  if (!tsi) {
    while (tsf && !end) {
      if (back) {
        if ((tsf->time <= sec) &&
            (tsi = TAILQ_LAST(&tsf->iframes, timeshift_index_iframe_list)))
          break;
        tsf = timeshift_filemgr_prev(tsf, &end, 1);
      } else {
        if ((tsf->time >= sec) &&
            (tsi = TAILQ_FIRST(&tsf->iframes)))
          break;
        tsf = timeshift_filemgr_next(tsf, &end, 0);
      }
      tsi = NULL;
    }
  }

  /* Fine search */
  if (back) {
    while (!end && tsf && tsi && (tsi->time > req_time)) {
      tsi = TAILQ_PREV(tsi, timeshift_index_iframe_list, link);
      while (!end && tsf && !tsi) {
        if ((tsf = timeshift_filemgr_prev(tsf, &end, 1)))
          tsi = TAILQ_LAST(&tsf->iframes, timeshift_index_iframe_list);
      }
    }
  } else {
    while (!end && tsf && tsi && (tsi->time < req_time)) {
      tsi = TAILQ_NEXT(tsi, link);
      while (!end && tsf && !tsi) {
        if ((tsf = timeshift_filemgr_next(tsf, &end, 0)))
          tsi = TAILQ_FIRST(&tsf->iframes);
      }
    }
  }

  /* End */
  if (!tsf || !tsi)
    end = 1;

  /* Find start/end of buffer */
  if (end) {
    if (back) {
      tsf = tsf_last = timeshift_filemgr_oldest(ts);
      tsi = NULL;
      while (tsf && !tsi) {
        tsf_last = tsf;
        if (!(tsi = TAILQ_FIRST(&tsf->iframes)))
          tsf = timeshift_filemgr_next(tsf, &end, 0);
      }
      if (!tsf)
        tsf = tsf_last;
      end = -1;
    } else {
      tsf = tsf_last = timeshift_filemgr_newest(ts);
      tsi = NULL;
      while (tsf && !tsi) {
        tsf_last = tsf;
        if (!(tsi = TAILQ_LAST(&tsf->iframes, timeshift_index_iframe_list)))
          tsf = timeshift_filemgr_prev(tsf, &end, 0);
      }
      if (!tsf)
        tsf = tsf_last;
      end = 1;
    }
  }

  if (cur_file)
    cur_file->refcount--;

  /* Done */
  *new_file = tsf;
  *iframe   = tsi;
  return end;
}

/*
 *
 */
static int _timeshift_do_skip
  ( timeshift_t *ts, int64_t req_time, int64_t last_time,
    timeshift_file_t **_cur_file, timeshift_index_iframe_t **_tsi )
{
  timeshift_file_t *tsf = NULL, *cur_file;
  timeshift_index_iframe_t *tsi;
  int end;

  tvhlog(LOG_DEBUG, "timeshift", "ts %d skip to %"PRId64" from %"PRId64,
         ts->id, req_time, last_time);

  /* Find */
  cur_file = *_cur_file;
  pthread_mutex_lock(&ts->rdwr_mutex);
  end = _timeshift_skip(ts, req_time, last_time,
                        cur_file, &tsf, _tsi);
  tsi = *_tsi;
  pthread_mutex_unlock(&ts->rdwr_mutex);
  if (tsi)
    tvhlog(LOG_DEBUG, "timeshift", "ts %d skip found pkt @ %"PRId64,
           ts->id, tsi->time);

  /* File changed (close) */
  if ((tsf != cur_file) && cur_file && cur_file->rfd >= 0) {
    close(cur_file->rfd);
    cur_file->rfd = -1;
  }

  /* Position */
  if (cur_file)
    cur_file->refcount--;
  if ((cur_file = *_cur_file = tsf) != NULL) {
    if (tsi)
      cur_file->roff = tsi->pos;
    else
      cur_file->roff = req_time > last_time ? cur_file->size : 0;
    tvhtrace("timeshift", "do skip cur_file %p roff %"PRId64, cur_file, cur_file->roff);
  }

  return end;
}


/*
 * Output packet
 */
static int _timeshift_read
  ( timeshift_t *ts, timeshift_file_t **cur_file,
    streaming_message_t **sm, int *wait )
{
  timeshift_file_t *tsf = *cur_file;
  ssize_t r;
  off_t off;

  *sm = NULL;

  if (tsf) {

    /* Open file */
    if (tsf->rfd < 0 && !tsf->ram) {
      tsf->rfd = open(tsf->path, O_RDONLY);
      tvhtrace("timeshift", "ts %d open file %s (fd %i)", ts->id, tsf->path, tsf->rfd);
      if (tsf->rfd < 0)
        return -1;
    }
    if (tsf->rfd >= 0)
      if ((off = lseek(tsf->rfd, tsf->roff, SEEK_SET)) != tsf->roff)
        tvherror("timeshift", "ts %d seek to %s failed (off %"PRId64" != %"PRId64"): %s",
                 ts->id, tsf->path, (int64_t)tsf->roff, (int64_t)off, strerror(errno));

    /* Read msg */
    r = _read_msg(tsf, -1, sm);
    if (r < 0) {
      streaming_message_t *e = streaming_msg_create_code(SMT_STOP, SM_CODE_UNDEFINED_ERROR);
      streaming_target_deliver2(ts->output, e);
      tvhtrace("timeshift", "ts %d seek to %jd (fd %i)", ts->id, (intmax_t)tsf->roff, tsf->rfd);
      tvhlog(LOG_ERR, "timeshift", "ts %d could not read buffer", ts->id);
      return -1;
    }
    tvhtrace("timeshift", "ts %d seek to %jd (fd %i) read msg %p/%"PRId64" (%"PRId64")",
             ts->id, (intmax_t)tsf->roff, tsf->rfd, *sm, *sm ? (*sm)->sm_time : -1, (int64_t)r);

    /* Special case - EOF */
    if (r <= sizeof(size_t) || tsf->roff > tsf->size || *sm == NULL) {
      if (tsf->rfd >= 0)
        close(tsf->rfd);
      tsf->rfd  = -1;
      pthread_mutex_lock(&ts->rdwr_mutex);
      *cur_file = tsf = timeshift_filemgr_next(tsf, NULL, 0);
      pthread_mutex_unlock(&ts->rdwr_mutex);
      if (tsf)
        tsf->roff = 0; // reset
      *wait     = 0;
      tvhtrace("timeshift", "ts %d eof, cur_file %p", ts->id, *cur_file);

    /* Check SMT_START index */
    } else {
      streaming_message_t *ssm = _timeshift_find_sstart(*cur_file, (*sm)->sm_time);
      if (ssm && ssm->sm_data != ts->smt_start) {
        streaming_target_deliver2(ts->output, streaming_msg_clone(ssm));
        if (ts->smt_start)
          streaming_start_unref(ts->smt_start);
        ts->smt_start = ssm->sm_data;
        atomic_add(&ts->smt_start->ss_refcount, 1);
      }
    }
  }
  return 0;
}


/*
 * Flush all data to live
 */
static int _timeshift_flush_to_live
  ( timeshift_t *ts, timeshift_file_t **cur_file, int *wait )
{
  streaming_message_t *sm;

  time_t pts = 0;
  while (*cur_file) {
    if (_timeshift_read(ts, cur_file, &sm, wait) == -1)
      return -1;
    if (!sm) break;
    if (sm->sm_type == SMT_PACKET) {
      pts = ((th_pkt_t*)sm->sm_data)->pkt_pts;
      tvhlog(LOG_DEBUG, "timeshift", "ts %d deliver %"PRId64" pts=%"PRItime_t,
             ts->id, sm->sm_time, pts);
    }
    streaming_target_deliver2(ts->output, sm);
  }
  return 0;
}

/*
 * Write packets from temporary queues
 */
static void _timeshift_write_queues
  ( timeshift_t *ts )
{
  struct streaming_message_queue sq;
  streaming_message_t *sm;

  TAILQ_INIT(&sq);
  timeshift_writer_clone(ts, &sq);
  timeshift_packets_clone(ts, &sq, 0);
  while ((sm = TAILQ_FIRST(&sq)) != NULL) {
    TAILQ_REMOVE(&sq, sm, sm_link);
    streaming_target_deliver2(ts->output, sm);
  }
}

/*
 * Send the status message
 */
static void timeshift_fill_status
  ( timeshift_t *ts, timeshift_status_t *status, int64_t current_time )
{
  timeshift_index_iframe_t *fst, *lst;
  int64_t shift;

  fst    = _timeshift_first_frame(ts);
  lst    = _timeshift_last_frame(ts);
  status->full  = ts->full;
  tvhtrace("timeshift", "status last->time %"PRId64" current time %"PRId64" state %d",
           lst ? lst->time : -1, current_time, ts->state);
  shift  = lst ? ts_rescale_inv(lst->time - current_time, 1000000) : -1;
  status->shift = (ts->state <= TS_LIVE || shift < 0 || !lst) ? 0 : shift;
  if (lst && fst && lst != fst) {
    status->pts_start = ts_rescale_inv(fst->time, 1000000);
    status->pts_end   = ts_rescale_inv(lst->time, 1000000);
  } else {
    status->pts_start = PTS_UNSET;
    status->pts_end   = PTS_UNSET;
  }
}

static void timeshift_status
  ( timeshift_t *ts, int64_t current_time )
{
  streaming_message_t *tsm;
  timeshift_status_t *status;

  status = calloc(1, sizeof(timeshift_status_t));
  timeshift_fill_status(ts, status, current_time);
  tsm = streaming_msg_create_data(SMT_TIMESHIFT_STATUS, status);
  streaming_target_deliver2(ts->output, tsm);
}

/*
 * Trace packet
 */
static void timeshift_trace_pkt
  ( timeshift_t *ts, streaming_message_t *sm )
{
  th_pkt_t *pkt = sm->sm_data;
  tvhtrace("timeshift",
           "ts %d pkt out - stream %d type %c pts %10"PRId64
           " dts %10"PRId64 " dur %10d len %6zu time %14"PRId64,
           ts->id,
           pkt->pkt_componentindex,
           pkt_frametype_to_char(pkt->pkt_frametype),
           ts_rescale(pkt->pkt_pts, 1000000),
           ts_rescale(pkt->pkt_dts, 1000000),
           pkt->pkt_duration,
           pktbuf_len(pkt->pkt_payload), sm->sm_time);
}

/* **************************************************************************
 * Thread
 * *************************************************************************/


/*
 * Timeshift thread
 */
void *timeshift_reader ( void *p )
{
  timeshift_t *ts = p;
  int nfds, end, run = 1, wait = -1, skip_delivered = 0;
  timeshift_file_t *cur_file = NULL;
  int cur_speed = 100, keyframe_mode = 0;
  int64_t mono_now, mono_play_time = 0, mono_last_status = 0;
  int64_t deliver, deliver0, pause_time = 0, last_time = 0, skip_time = 0;
  streaming_message_t *sm = NULL, *ctrl = NULL;
  timeshift_index_iframe_t *tsi = NULL;
  streaming_skip_t *skip = NULL;
  tvhpoll_t *pd;
  tvhpoll_event_t ev = { 0 };
  th_pkt_t *pkt;

  pd = tvhpoll_create(1);
  ev.fd     = ts->rd_pipe.rd;
  ev.events = TVHPOLL_IN;
  tvhpoll_add(pd, &ev, 1);

  /* Output */
  while (run) {

    // Note: Previously we allowed unlimited wait, but we now must wake periodically
    //       to output status message
    if (wait < 0 || wait > 1000)
      wait = 1000;

    /* Wait for data */
    if(wait)
      nfds = tvhpoll_wait(pd, &ev, 1, wait);
    else
      nfds = 0;
    wait      = -1;
    end       = 0;
    skip      = NULL;
    mono_now  = getmonoclock();

    /* Control */
    pthread_mutex_lock(&ts->state_mutex);
    if (nfds == 1) {
      if (_read_msg(NULL, ts->rd_pipe.rd, &ctrl) > 0) {

        /* Exit */
        if (ctrl->sm_type == SMT_EXIT) {
          tvhtrace("timeshift", "ts %d read exit request", ts->id);
          run = 0;
          streaming_msg_free(ctrl);
          ctrl = NULL;

        /* Speed */
        } else if (ctrl->sm_type == SMT_SPEED) {
          int speed = ctrl->sm_code;
          int keyframe;

          /* Bound it */
          if (speed > 3200)  speed = 3200;
          if (speed < -3200) speed = -3200;

          /* Ignore negative */
          if (!ts->dobuf && (speed < 0))
            speed = cur_file ? speed : 0;

          /* Process */
          if (cur_speed != speed) {

            /* Live playback */
            if (ts->state == TS_LIVE) {

              /* Reject */
              if (speed >= 100) {
                tvhlog(LOG_DEBUG, "timeshift", "ts %d reject 1x+ in live mode",
                       ts->id);
                speed = 100;

              /* Set position */
              } else {
                tvhlog(LOG_DEBUG, "timeshift", "ts %d enter timeshift mode",
                       ts->id);
                timeshift_writer_flush(ts);
                ts->dobuf = 1;
                skip_delivered = 1;
                pthread_mutex_lock(&ts->rdwr_mutex);
                cur_file = timeshift_filemgr_newest(ts);
                cur_file = timeshift_filemgr_get(ts, cur_file ? cur_file->last :
                                                     atomic_add_s64(&ts->last_time, 0));
                if (cur_file != NULL) {
                  cur_file->roff = cur_file->size;
                  pause_time     = cur_file->last;
                  last_time      = pause_time;
                } else {
                  pause_time     = atomic_add_s64(&ts->last_time, 0);
                  last_time      = pause_time;
                }
                pthread_mutex_unlock(&ts->rdwr_mutex);
              }
            }

            /* Check keyframe mode */
            keyframe      = (speed < 0) || (speed > 400);
            if (keyframe != keyframe_mode) {
              tvhlog(LOG_DEBUG, "timeshift", "using keyframe mode? %s",
                     keyframe ? "yes" : "no");
              keyframe_mode = keyframe;
              if (keyframe) {
                tsi = NULL;
              }
            }

            /* Update */
            cur_speed = speed;
            if (speed != 100 || ts->state != TS_LIVE) {
              ts->state = speed == 0 ? TS_PAUSE : TS_PLAY;
              tvhtrace("timeshift", "reader - set %s", speed == 0 ? "TS_PAUSE" : "TS_PLAY");
            }
            if (ts->state == TS_PLAY) {
              mono_play_time = mono_now;
              tvhtrace("timeshift", "update play time TS_LIVE - %"PRId64" play buffer from %"PRId64, mono_now, pause_time);
            } else if (ts->state == TS_PAUSE) {
              skip_delivered = 1;
            }
            tvhlog(LOG_DEBUG, "timeshift", "ts %d change speed %d", ts->id, speed);
          }

          /* Send on the message */
          ctrl->sm_code = speed;
          streaming_target_deliver2(ts->output, ctrl);
          ctrl = NULL;

        /* Skip/Seek */
        } else if (ctrl->sm_type == SMT_SKIP) {
          skip = ctrl->sm_data;
          switch (skip->type) {
            case SMT_SKIP_LIVE:
              if (ts->state != TS_LIVE) {

                /* Reset */
                if (ts->full) {
                  pthread_mutex_lock(&ts->rdwr_mutex);
                  timeshift_filemgr_flush(ts, NULL);
                  ts->full = 0;
                  pthread_mutex_unlock(&ts->rdwr_mutex);
                }

                /* Release */
                if (sm)
                  streaming_msg_free(sm);

                /* Find end */
                skip_time = 0x7fffffffffffffffLL;
                // TODO: change this sometime!
              }
              break;

            case SMT_SKIP_ABS_TIME:
              /* -fallthrough */
            case SMT_SKIP_REL_TIME:

              /* Convert */
              skip_time = ts_rescale(skip->time, 1000000);
              tvhlog(LOG_DEBUG, "timeshift", "ts %d skip %"PRId64" requested %"PRId64, ts->id, skip_time, skip->time);

              /* Live playback (stage1) */
              if (ts->state == TS_LIVE) {
                pthread_mutex_lock(&ts->rdwr_mutex);
                cur_file = timeshift_filemgr_newest(ts);
                if (cur_file && (cur_file = timeshift_filemgr_get(ts, cur_file->last)) != NULL) {
                  cur_file->roff = cur_file->size;
                  last_time      = cur_file->last;
                } else {
                  last_time      = atomic_add_s64(&ts->last_time, 0);
                }
                skip_delivered = 0;
                pthread_mutex_unlock(&ts->rdwr_mutex);
              }

              /* May have failed */
              if (skip) {
                if (skip->type == SMT_SKIP_REL_TIME)
                  skip_time += last_time;
                tvhlog(LOG_DEBUG, "timeshift", "ts %d skip time %"PRId64, ts->id, skip_time);

                /* Live (stage2) */
                if (ts->state == TS_LIVE) {
                  if (skip_time >= atomic_add_s64(&ts->last_time, 0) - TIMESHIFT_PLAY_BUF) {
                    tvhlog(LOG_DEBUG, "timeshift", "ts %d skip ignored, already live", ts->id);
                    skip = NULL;
                  } else {
                    ts->state = TS_PLAY;
                    ts->dobuf = 1;
                    tvhtrace("timeshift", "reader - set TS_PLAY");
                  }
                }
              }

              /* OK */
              if (skip) {
                /* seek */
                tsi = NULL;
                end = _timeshift_do_skip(ts, skip_time, last_time, &cur_file, &tsi);
                if (tsi) {
                  pause_time = tsi->time;
                  tvhtrace("timeshift", "ts %d skip - play buffer from %"PRId64" last_time %"PRId64,
                           ts->id, pause_time, last_time);

                  /* Adjust time */
                  if (mono_play_time != mono_now)
                    tvhtrace("timeshift", "ts %d update play time skip - %"PRId64, ts->id, mono_now);
                  mono_play_time = mono_now;

                  /* Clear existing packet */
                  if (sm) {
                    streaming_msg_free(sm);
                    sm = NULL;
                  }
                } else {
                  skip = NULL;
                }
              }
              break;
            default:
              tvhlog(LOG_ERR, "timeshift", "ts %d invalid/unsupported skip type: %d", ts->id, skip->type);
              skip = NULL;
              break;
          }

          /* Error */
          if (!skip) {
            ((streaming_skip_t*)ctrl->sm_data)->type = SMT_SKIP_ERROR;
            streaming_target_deliver2(ts->output, ctrl);
            ctrl = NULL;
          }

        /* Ignore */
        } else {
          streaming_msg_free(ctrl);
          ctrl = NULL;
        }
      }
    }


    /* Done */
    if (!run || !cur_file || ((ts->state != TS_PLAY && !skip))) {
      if (mono_now >= (mono_last_status + 1000000)) {
        timeshift_status(ts, last_time);
        mono_last_status = mono_now;
      }
      pthread_mutex_unlock(&ts->state_mutex);
      continue;
    }

    /* Calculate delivery time */
    deliver0 = (mono_now - mono_play_time) + TIMESHIFT_PLAY_BUF;
    deliver = (deliver0 * cur_speed) / 100;
    deliver = (deliver + pause_time);
    tvhtrace("timeshift", "speed %d now %"PRId64" play_time %"PRId64" deliver %"PRId64" deliver0 %"PRId64,
             cur_speed, mono_now, mono_play_time, deliver, deliver0);

    /* Determine next packet */
    if (!sm) {

      /* Rewind or Fast forward (i-frame only) */
      if (skip || keyframe_mode) {
        int64_t req_time;

        /* Time */
        if (!skip)
          req_time = last_time + ((cur_speed < 0) ? -1 : 1);
        else
          req_time = skip_time;

        end = _timeshift_do_skip(ts, req_time, last_time, &cur_file, &tsi);
      }

      /* Clear old message */
      if (sm) {
        streaming_msg_free(sm);
        sm = NULL;
      }

      /* Find packet */
      if (_timeshift_read(ts, &cur_file, &sm, &wait) == -1) {
        pthread_mutex_unlock(&ts->state_mutex);
        break;
      }
    }

    /* Send skip response */
    if (skip) {
      if (sm) {
        /* Status message */
        skip->time = ts_rescale_inv(sm->sm_time, 1000000);
        skip->type = SMT_SKIP_ABS_TIME;
        tvhlog(LOG_DEBUG, "timeshift", "ts %d skip to pts %"PRId64" ok", ts->id, sm->sm_time);
        /* Update timeshift status */
        timeshift_fill_status(ts, &skip->timeshift, sm->sm_time);
        mono_last_status = mono_now;
      } else {
        /* Report error */
        skip->type = SMT_SKIP_ERROR;
        skip       = NULL;
        tvhlog(LOG_DEBUG, "timeshift", "ts %d skip failed (%d)", ts->id, sm ? sm->sm_type : -1);
      }
      streaming_target_deliver2(ts->output, ctrl);
      ctrl = NULL;
    }

    /* Deliver */
    if (sm && (skip ||
               (((cur_speed < 0) && (sm->sm_time >= deliver)) ||
                ((cur_speed > 0) && (sm->sm_time <= deliver))))) {

      last_time = sm->sm_time;
      if (sm->sm_type == SMT_PACKET) {
        pkt = sm->sm_data;
        if (skip_delivered && pkt->pkt_delivered) {
          streaming_msg_free(sm);
          goto skip_pkt;
        }
        if (tvhtrace_enabled())
          timeshift_trace_pkt(ts, sm);
      }
      streaming_target_deliver2(ts->output, sm);
skip_pkt:
      sm        = NULL;
      wait      = 0;

    } else if (sm) {

      if (cur_speed > 0)
        wait = (sm->sm_time - deliver) / 1000;
      else
        wait = (deliver - sm->sm_time) / 1000;
      if (wait == 0) wait = 1;
      tvhtrace("timeshift", "ts %d wait %d speed %d sm_time %"PRId64" deliver %"PRId64,
               ts->id, wait, cur_speed, sm->sm_time, deliver);

    }

    /* Periodic timeshift status */
    if (mono_now >= (mono_last_status + 1000000)) {
      timeshift_status(ts, last_time);
      mono_last_status = mono_now;
    }

    /* Terminate */
    if (!cur_file || end != 0) {
      if (!end)
        end = (cur_speed > 0) ? 1 : -1;

      /* Back to live (unless buffer is full) */
      if (end == 1 && !ts->full) {
        tvhlog(LOG_DEBUG, "timeshift", "ts %d eob revert to live mode", ts->id);
        cur_speed = 100;
        ctrl      = streaming_msg_create_code(SMT_SPEED, cur_speed);
        streaming_target_deliver2(ts->output, ctrl);
        ctrl      = NULL;
        tvhtrace("timeshift", "reader - set TS_LIVE");

        /* Flush timeshift buffer to live */
        if (_timeshift_flush_to_live(ts, &cur_file, &wait) == -1)
          break;

        /* Flush write / backlog queues */
        _timeshift_write_queues(ts);
        
        ts->state = TS_LIVE;

        /* Close file (if open) */
        if (cur_file && cur_file->rfd >= 0) {
          close(cur_file->rfd);
          cur_file->rfd = -1;
        }

      /* Pause */
      } else {
        if (cur_speed <= 0) {
          cur_speed = 0;
          tvhtrace("timeshift", "reader - set TS_PAUSE");
          ts->state = TS_PAUSE;
        } else {
          cur_speed = 100;
          tvhtrace("timeshift", "reader - set TS_PLAY");
          ts->state = TS_PLAY;
          ts->dobuf = 1;
          if (mono_play_time != mono_now)
            tvhtrace("timeshift", "update play time (pause) - %"PRId64, mono_now);
          mono_play_time = mono_now;
        }
        tvhlog(LOG_DEBUG, "timeshift", "ts %d sob speed %d last time %"PRId64, ts->id, cur_speed, last_time);
        pause_time = last_time;
        ctrl       = streaming_msg_create_code(SMT_SPEED, cur_speed);
        streaming_target_deliver2(ts->output, ctrl);
        ctrl       = NULL;
      }

    }

    pthread_mutex_unlock(&ts->state_mutex);
  }

  /* Cleanup */
  tvhpoll_destroy(pd);
  if (cur_file && cur_file->rfd >= 0) {
    close(cur_file->rfd);
    cur_file->rfd = -1;
  }
  if (sm)       streaming_msg_free(sm);
  if (ctrl)     streaming_msg_free(ctrl);
  tvhtrace("timeshift", "ts %d exit reader thread", ts->id);

  return NULL;
}
