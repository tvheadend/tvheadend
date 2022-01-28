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
 * Buffered position handling
 * *************************************************************************/

static timeshift_seek_t *_seek_reset ( timeshift_seek_t *seek )
{
  timeshift_file_t *tsf = seek->file;
  seek->file  = NULL;
  seek->frame = NULL;
  timeshift_file_put(tsf);
  return seek;
}

static timeshift_seek_t *_seek_set_file
  ( timeshift_seek_t *seek, timeshift_file_t *tsf, off_t roff )
{
  seek->file  = tsf;
  seek->frame = NULL;
  if (tsf)
    tsf->roff = roff;
  return seek;
}

static timeshift_seek_t *_read_close ( timeshift_seek_t *seek )
{
  if (seek->file && seek->file->rfd >= 0) {
    close(seek->file->rfd);
    seek->file->rfd = -1;
  }
  return _seek_reset(seek);
}

/* **************************************************************************
 * File Reading
 * *************************************************************************/

static ssize_t _read_buf ( timeshift_file_t *tsf, int fd, void *buf, size_t size )
{
  ssize_t r;
  size_t ret;

  if (tsf && tsf->ram) {
    if (tsf->roff == tsf->woff) return 0;
    if (tsf->roff + size > tsf->woff) return -1;
    tvh_mutex_lock(&tsf->ram_lock);
    memcpy(buf, tsf->ram + tsf->roff, size);
    tsf->roff += size;
    tvh_mutex_unlock(&tsf->ram_lock);
    return size;
  } else {
    ret = 0;
    while (size > 0) {
      r = read(tsf ? tsf->rfd : fd, buf, size);
      if (r < 0) {
        if (ERRNO_AGAIN(errno))
          continue;
        tvhtrace(LS_TIMESHIFT, "read errno %d", errno);
        return -1;
      }
      if (r > 0) {
        size -= r;
        ret += r;
        buf += r;
      }
      if (r == 0)
        return 0;
    }
    if (ret > 0 && tsf)
      tsf->roff += ret;
    return ret;
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
  r = _read_buf(tsf, fd, pktbuf_ptr(*pktbuf), sz);
  if (r != sz) {
    pktbuf_destroy(*pktbuf);
    *pktbuf = NULL;
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
  if (sz > 1024 * 1024) {
    tvhtrace(LS_TIMESHIFT, "wrong msg size (%lld/0x%llx)", (long long)sz, (long long)sz);
    return -1;
  }

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

static int64_t _timeshift_first_time
  ( timeshift_t *ts, int *active )
{ 
  int64_t ret = 0;
  int end;
  timeshift_index_iframe_t *tsi = NULL;
  timeshift_file_t *tsf = timeshift_filemgr_oldest(ts);
  while (tsf && !tsi) {
    if (!(tsi = TAILQ_FIRST(&tsf->iframes)))
      tsf = timeshift_filemgr_next(tsf, &end, 0);
  }
  if (tsi) {
    *active = 1;
    ret = tsi->time;
  }
  timeshift_file_put(tsf);
  return ret;
}

static int _timeshift_skip
  ( timeshift_t *ts, int64_t req_time, int64_t cur_time,
    timeshift_seek_t *seek, timeshift_seek_t *nseek )
{
  timeshift_index_iframe_t *tsi  = seek->frame;
  timeshift_file_t         *tsf  = seek->file, *tsf_last;
  int64_t                   sec  = mono2sec(req_time) / TIMESHIFT_FILE_PERIOD;
  int                       back = (req_time < cur_time) ? 1 : 0;
  int                       end  = 0;

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
    timeshift_file_put(tsf);
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

  /* Done */
  nseek->file  = tsf;
  nseek->frame = tsi;
  return end;
}

/*
 *
 */
static int _timeshift_do_skip
  ( timeshift_t *ts, int64_t req_time, int64_t last_time,
    timeshift_seek_t *seek )
{
  timeshift_seek_t nseek;
  int end;

  tvhdebug(LS_TIMESHIFT, "ts %d skip to %"PRId64" from %"PRId64,
           ts->id, req_time, last_time);

  timeshift_file_get(seek->file);

  /* Find */
  end = _timeshift_skip(ts, req_time, last_time, seek, &nseek);
  if (nseek.frame)
    tvhdebug(LS_TIMESHIFT, "ts %d skip found pkt @ %"PRId64,
             ts->id, nseek.frame->time);

  /* File changed (close) */
  if (nseek.file != seek->file)
    _read_close(seek);

  timeshift_file_put(seek->file);

  /* Position */
  *seek = nseek;
  if (nseek.file != NULL) {
    if (nseek.frame)
      nseek.file->roff = nseek.frame->pos;
    else
      nseek.file->roff = req_time > last_time ? nseek.file->size : 0;
    tvhtrace(LS_TIMESHIFT, "do skip seek->file %p roff %"PRId64,
             nseek.file, (int64_t)nseek.file->roff);
  }

  return end;
}


/*
 * Output packet
 */
static int _timeshift_read
  ( timeshift_t *ts, timeshift_seek_t *seek,
    streaming_message_t **sm, int *wait )
{
  timeshift_file_t *tsf = seek->file;
  ssize_t r;
  off_t off = 0;

  *sm = NULL;

  if (tsf) {

    /* Open file */
    if (tsf->rfd < 0 && !tsf->ram) {
      tsf->rfd = tvh_open(tsf->path, O_RDONLY, 0);
      tvhtrace(LS_TIMESHIFT, "ts %d open file %s (fd %i)", ts->id, tsf->path, tsf->rfd);
      if (tsf->rfd < 0)
        return -1;
    }
    if (tsf->rfd >= 0)
      if ((off = lseek(tsf->rfd, tsf->roff, SEEK_SET)) != tsf->roff)
        tvherror(LS_TIMESHIFT, "ts %d seek to %s failed (off %"PRId64" != %"PRId64"): %s",
                 ts->id, tsf->path, (int64_t)tsf->roff, (int64_t)off, strerror(errno));

    /* Read msg */
    r = _read_msg(tsf, -1, sm);
    if (r < 0) {
      streaming_message_t *e = streaming_msg_create_code(SMT_STOP, SM_CODE_UNDEFINED_ERROR);
      streaming_target_deliver2(ts->output, e);
      tvhtrace(LS_TIMESHIFT, "ts %d seek to %jd (woff %jd) (fd %i)", ts->id, (intmax_t)off, (intmax_t)tsf->woff, tsf->rfd);
      tvherror(LS_TIMESHIFT, "ts %d could not read buffer", ts->id);
      return -1;
    }
    tvhtrace(LS_TIMESHIFT, "ts %d seek to %jd (fd %i) read msg %p/%"PRId64" (%"PRId64")",
             ts->id, (intmax_t)off, tsf->rfd, *sm, *sm ? (*sm)->sm_time : -1, (int64_t)r);

    /* Special case - EOF */
    if (r <= sizeof(size_t) || tsf->roff > tsf->size || *sm == NULL) {
      timeshift_file_get(seek->file); /* _read_close decreases file reference */
      _read_close(seek);
      _seek_set_file(seek, timeshift_filemgr_next(tsf, NULL, 0), 0);
      *wait     = 0;
      tvhtrace(LS_TIMESHIFT, "ts %d eof, seek->file %p (prev %p)", ts->id, seek->file, tsf);
      timeshift_filemgr_dump(ts);
    }
  }
  return 0;
}

/*
 * Flush all data to live
 */
static int _timeshift_flush_to_live
  ( timeshift_t *ts, timeshift_seek_t *seek, int *wait )
{
  streaming_message_t *sm;

  while (seek->file) {
    if (_timeshift_read(ts, seek, &sm, wait) == -1)
      return -1;
    if (!sm) break;
    timeshift_packet_log("ouf", ts, sm);
    streaming_target_deliver2(ts->output, sm);
  }
  return 0;
}

/*
 * Send the status message
 */
static void timeshift_fill_status
  ( timeshift_t *ts, timeshift_status_t *status, int64_t current_time )
{
  int active = 0;
  int64_t start, end;

  start = _timeshift_first_time(ts, &active);
  end   = ts->buf_time;
  if (ts->state <= TS_LIVE) {
    current_time = end;
  } else {
    if (current_time < 0)
      current_time = 0;
    if (current_time > end)
      current_time = end;
  }
  status->full = ts->full;
  tvhtrace(LS_TIMESHIFT, "ts %d status start %"PRId64" end %"PRId64
                        " current %"PRId64" state %d",
           ts->id, start, end, current_time, ts->state);
  status->shift = ts_rescale_inv(end - current_time, 1000000);
  if (active) {
    status->pts_start = ts_rescale_inv(start, 1000000);
    status->pts_end   = ts_rescale_inv(end,   1000000);
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

/* **************************************************************************
 * Thread
 * *************************************************************************/


/*
 * Timeshift thread
 */
void *timeshift_reader ( void *p )
{
  timeshift_t *ts = p;
  int nfds, end, run = 1, wait = -1, state;
  timeshift_seek_t *seek = &ts->seek;
  timeshift_file_t *tmp_file;
  int cur_speed = 100, keyframe_mode = 0;
  int64_t mono_now, mono_play_time = 0, mono_last_status = 0;
  int64_t deliver, deliver0, pause_time = 0, last_time = 0, skip_time = 0;
  int64_t i64;
  streaming_message_t *sm = NULL, *ctrl = NULL;
  streaming_skip_t *skip = NULL;
  tvhpoll_t *pd;
  tvhpoll_event_t ev = { 0 };

  pd = tvhpoll_create(1);
  tvhpoll_add1(pd, ts->rd_pipe.rd, TVHPOLL_IN, NULL);

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
    mono_now  = getfastmonoclock();

    /* Control */
    tvh_mutex_lock(&ts->state_mutex);
    if (nfds == 1) {
      if (_read_msg(NULL, ts->rd_pipe.rd, &ctrl) > 0) {

        /* Exit */
        if (ctrl->sm_type == SMT_EXIT) {
          tvhtrace(LS_TIMESHIFT, "ts %d read exit request", ts->id);
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
            speed = seek->file ? speed : 0;

          /* Process */
          if (cur_speed != speed) {

            /* Live playback */
            state = ts->state;
            if (state == TS_LIVE) {

              /* Reject */
              if (speed >= 100) {
                tvhdebug(LS_TIMESHIFT, "ts %d reject 1x+ in live mode", ts->id);
                speed = 100;

              /* Set position */
              } else {
                tvhdebug(LS_TIMESHIFT, "ts %d enter timeshift mode", ts->id);
                ts->dobuf = 1;
                _seek_reset(seek);
                tmp_file = timeshift_filemgr_newest(ts);
                if (tmp_file != NULL) {
                  i64 = tmp_file->last;
                  timeshift_file_put(tmp_file);
                } else {
                  i64 = ts->buf_time;
                }
                seek->file = timeshift_filemgr_get(ts, i64);
                if (seek->file != NULL) {
                  seek->file->roff = seek->file->size;
                  pause_time       = seek->file->last;
                  last_time        = pause_time;
                } else {
                  pause_time       = i64;
                  last_time        = pause_time;
                }
              }
            }

            /* Check keyframe mode */
            keyframe      = (speed < 0) || (speed > 400);
            if (keyframe != keyframe_mode) {
              tvhdebug(LS_TIMESHIFT, "using keyframe mode? %s", keyframe ? "yes" : "no");
              keyframe_mode = keyframe;
              if (keyframe)
                seek->frame = NULL;
            }

            /* Update */
            if (speed != 100 || state != TS_LIVE) {
              ts->state = speed == 0 ? TS_PAUSE : TS_PLAY;
              tvhtrace(LS_TIMESHIFT, "reader - set %s", speed == 0 ? "TS_PAUSE" : "TS_PLAY");
            }
            if ((ts->state == TS_PLAY && state != TS_PLAY) || (speed != cur_speed)) {
              mono_play_time = mono_now;
              tvhtrace(LS_TIMESHIFT, "update play time TS_LIVE - %"PRId64" play buffer from %"PRId64,
                       mono_now, pause_time);
              if (speed != cur_speed)
                pause_time = last_time;
            } else if (ts->state == TS_PAUSE && state != TS_PAUSE) {
              pause_time = last_time;
            }
            cur_speed = speed;
            tvhdebug(LS_TIMESHIFT, "ts %d change speed %d", ts->id, speed);
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
                  timeshift_filemgr_flush(ts, NULL);
                  _seek_reset(seek);
                  ts->full = 0;
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
              tvhdebug(LS_TIMESHIFT, "ts %d skip %"PRId64" requested %"PRId64, ts->id, skip_time, skip->time);

              /* Live playback (stage1) */
              if (ts->state == TS_LIVE) {
                _seek_reset(seek);
                tmp_file = timeshift_filemgr_newest(ts);
                if (tmp_file) {
                  i64 = tmp_file->last;
                  timeshift_file_put(tmp_file);
                }
                if (tmp_file && (seek->file = timeshift_filemgr_get(ts, i64)) != NULL) {
                  seek->file->roff = seek->file->size;
                  last_time        = seek->file->last;
                } else {
                  last_time        = ts->buf_time;
                }
              }

              /* May have failed */
              if (skip->type == SMT_SKIP_REL_TIME)
                skip_time += last_time;
              tvhdebug(LS_TIMESHIFT, "ts %d skip time %"PRId64, ts->id, skip_time);

              /* Live (stage2) */
              if (ts->state == TS_LIVE) {
                if (skip_time >= ts->buf_time - TIMESHIFT_PLAY_BUF) {
                  tvhdebug(LS_TIMESHIFT, "ts %d skip ignored, already live", ts->id);
                  skip = NULL;
                } else {
                  ts->state = TS_PLAY;
                  ts->dobuf = 1;
                  tvhtrace(LS_TIMESHIFT, "reader - set TS_PLAY");
                }
              }

              /* OK */
              if (skip) {
                /* seek */
                seek->frame = NULL;
                end = _timeshift_do_skip(ts, skip_time, last_time, seek);
                if (seek->frame) {
                  pause_time = seek->frame->time;
                  tvhtrace(LS_TIMESHIFT, "ts %d skip - play buffer from %"PRId64" last_time %"PRId64,
                           ts->id, pause_time, last_time);

                  /* Adjust time */
                  if (mono_play_time != mono_now)
                    tvhtrace(LS_TIMESHIFT, "ts %d update play time skip - %"PRId64, ts->id, mono_now);
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
              tvherror(LS_TIMESHIFT, "ts %d invalid/unsupported skip type: %d", ts->id, skip->type);
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
    if (!run || !seek->file || ((ts->state != TS_PLAY && !skip))) {
      if (mono_now >= (mono_last_status + sec2mono(1))) {
        timeshift_status(ts, last_time);
        mono_last_status = mono_now;
      }
      tvh_mutex_unlock(&ts->state_mutex);
      continue;
    }

    /* Calculate delivery time */
    deliver0 = (mono_now - mono_play_time) + TIMESHIFT_PLAY_BUF;
    deliver = (deliver0 * cur_speed) / 100;
    deliver = (deliver + pause_time);
    tvhtrace(LS_TIMESHIFT, "speed %d now %"PRId64" play_time %"PRId64" deliver %"PRId64" deliver0 %"PRId64,
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

        end = _timeshift_do_skip(ts, req_time, last_time, seek);
      }

      /* Clear old message */
      if (sm) {
        streaming_msg_free(sm);
        sm = NULL;
      }

      /* Find packet */
      if (_timeshift_read(ts, seek, &sm, &wait) == -1) {
        tvh_mutex_unlock(&ts->state_mutex);
        break;
      }
    }

    /* Send skip response */
    if (skip) {
      if (sm) {
        /* Status message */
        skip->time = ts_rescale_inv(sm->sm_time, 1000000);
        skip->type = SMT_SKIP_ABS_TIME;
        tvhdebug(LS_TIMESHIFT, "ts %d skip to pts %"PRId64" ok", ts->id, sm->sm_time);
        /* Update timeshift status */
        timeshift_fill_status(ts, &skip->timeshift, sm->sm_time);
        mono_last_status = mono_now;
      } else {
        /* Report error */
        skip->type = SMT_SKIP_ERROR;
        skip       = NULL;
        tvhdebug(LS_TIMESHIFT, "ts %d skip failed (%d)", ts->id, sm ? sm->sm_type : -1);
      }
      streaming_target_deliver2(ts->output, ctrl);
    } else {
      streaming_msg_free(ctrl);
    }
    ctrl = NULL;

    /* Deliver */
    if (sm && (skip ||
               (((cur_speed < 0) && (sm->sm_time >= deliver)) ||
                ((cur_speed > 0) && (sm->sm_time <= deliver))))) {

      last_time = sm->sm_time;
      if (!skip && keyframe_mode) /* always send status on keyframe mode */
        timeshift_status(ts, last_time);
      timeshift_packet_log("out", ts, sm);
      streaming_target_deliver2(ts->output, sm);
      sm        = NULL;
      wait      = 0;

    } else if (sm) {

      if (cur_speed > 0)
        wait = (sm->sm_time - deliver) / 1000;
      else
        wait = (deliver - sm->sm_time) / 1000;
      if (wait == 0) wait = 1;
      tvhtrace(LS_TIMESHIFT, "ts %d wait %d speed %d sm_time %"PRId64" deliver %"PRId64,
               ts->id, wait, cur_speed, sm->sm_time, deliver);

    }

    /* Periodic timeshift status */
    if (mono_now >= (mono_last_status + sec2mono(1))) {
      timeshift_status(ts, last_time);
      mono_last_status = mono_now;
    }

    /* Terminate */
    if (!seek->file || end != 0) {

      /* Back to live (unless buffer is full) */
      if ((end == 1 && !ts->full) || !seek->file) {
        tvhdebug(LS_TIMESHIFT, "ts %d eob revert to live mode", ts->id);
        cur_speed = 100;
        ctrl      = streaming_msg_create_code(SMT_SPEED, cur_speed);
        streaming_target_deliver2(ts->output, ctrl);
        ctrl      = NULL;
        tvhtrace(LS_TIMESHIFT, "reader - set TS_LIVE");

        /* Flush timeshift buffer to live */
        if (_timeshift_flush_to_live(ts, seek, &wait) == -1) {
          tvh_mutex_unlock(&ts->state_mutex);
          break;
        }

        ts->state = TS_LIVE;

        /* Close file (if open) */
        _read_close(seek);

      /* Pause */
      } else {
        if (cur_speed <= 0) {
          cur_speed = 0;
          tvhtrace(LS_TIMESHIFT, "reader - set TS_PAUSE");
          ts->state = TS_PAUSE;
        } else {
          cur_speed = 100;
          tvhtrace(LS_TIMESHIFT, "reader - set TS_PLAY");
          if (ts->state != TS_PLAY) {
            ts->state = TS_PLAY;
            ts->dobuf = 1;
            if (mono_play_time != mono_now)
              tvhtrace(LS_TIMESHIFT, "update play time (pause) - %"PRId64, mono_now);
            mono_play_time = mono_now;
          }
        }
        tvhdebug(LS_TIMESHIFT, "ts %d sob speed %d last time %"PRId64, ts->id, cur_speed, last_time);
        pause_time = last_time;
        ctrl       = streaming_msg_create_code(SMT_SPEED, cur_speed);
        streaming_target_deliver2(ts->output, ctrl);
        ctrl       = NULL;
      }

    }

    tvh_mutex_unlock(&ts->state_mutex);
  }

  /* Cleanup */
  tvhpoll_destroy(pd);
  _read_close(seek);
  if (sm)       streaming_msg_free(sm);
  if (ctrl)     streaming_msg_free(ctrl);
  tvhtrace(LS_TIMESHIFT, "ts %d exit reader thread", ts->id);

  return NULL;
}
