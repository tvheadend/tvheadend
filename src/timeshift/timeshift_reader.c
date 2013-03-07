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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

//#define TSHFT_TRACE

/* **************************************************************************
 * File Reading
 * *************************************************************************/

static ssize_t _read_pktbuf ( int fd, pktbuf_t **pktbuf )
{
  ssize_t r, cnt = 0;
  size_t sz;

  /* Size */
  r = read(fd, &sz, sizeof(sz));
  if (r < 0) return -1;
  if (r != sizeof(sz)) return 0;
  cnt += r;

  /* Empty */
  if (!sz) {
    *pktbuf = NULL;
    return cnt;
  }

  /* Data */
  *pktbuf = pktbuf_alloc(NULL, sz);
  r = read(fd, (*pktbuf)->pb_data, sz);
  if (r != sz) {
    free((*pktbuf)->pb_data);
    free(*pktbuf);
    return r < 0 ? -1 : 0;
  }
  cnt += r;

  return cnt;
}


static ssize_t _read_msg ( int fd, streaming_message_t **sm )
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
  r = read(fd, &sz, sizeof(sz));
  if (r < 0) return -1;
  if (r != sizeof(sz)) return 0;
  cnt += r;

  /* EOF */
  if (sz == 0) return cnt;

  /* Type */
  r = read(fd, &type, sizeof(type));
  if (r < 0) return -1;
  if (r != sizeof(type)) return 0;
  cnt += r;

  /* Time */
  r = read(fd, &time, sizeof(time));
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
    case SMT_SERVICE_STATUS:
      return -1;
      break;

    /* Code */
    case SMT_STOP:
    case SMT_EXIT:
    case SMT_SPEED:
      if (sz != sizeof(code)) return -1;
      r = read(fd, &code, sz);
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
      r = read(fd, data, sz);
      if (r != sz) {
        free(data);
        if (r < 0) return -1;
        return 0;
      }
      if (type == SMT_PACKET) {
        th_pkt_t *pkt = data;
        pkt->pkt_payload  = pkt->pkt_header = NULL;
        pkt->pkt_refcount = 0;
        *sm = streaming_msg_create_pkt(pkt);
        r   = _read_pktbuf(fd, &pkt->pkt_header);
        if (r < 0) {
          streaming_msg_free(*sm);
          return r;
        }
        cnt += r;
        r   = _read_pktbuf(fd, &pkt->pkt_payload);
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
  timeshift_file_t *tsf = timeshift_filemgr_get(ts, 0);
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
  timeshift_file_t         *tsf  = cur_file;
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
      tsf = timeshift_filemgr_oldest(ts);
      tsi = NULL;
      while (tsf && !tsi) {
        if (!(tsi = TAILQ_FIRST(&tsf->iframes)))
          tsf = timeshift_filemgr_next(tsf, &end, 0);
      }
      end = -1;
    } else {
      tsf = timeshift_filemgr_get(ts, 0);
      tsi = NULL;
      while (tsf && !tsi) {
        if (!(tsi = TAILQ_LAST(&tsf->iframes, timeshift_index_iframe_list)))
          tsf = timeshift_filemgr_prev(tsf, &end, 0);
      }
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
 * Output packet
 */
static int _timeshift_read
  ( timeshift_t *ts, timeshift_file_t **cur_file, off_t *cur_off, int *fd,
    streaming_message_t **sm, int *wait )
{
  if (*cur_file) {

    /* Open file */
    if (*fd == -1) {
#ifdef TSHFT_TRACE
      tvhlog(LOG_DEBUG, "timeshift", "ts %d open file %s",
             ts->id, (*cur_file)->path);
#endif
      *fd = open((*cur_file)->path, O_RDONLY);
    }
#ifdef TSHFT_TRACE
    tvhlog(LOG_DEBUG, "timeshift", "ts %d seek to %lu", ts->id, *cur_off);
#endif
    lseek(*fd, *cur_off, SEEK_SET);

    /* Read msg */
    ssize_t r = _read_msg(*fd, sm);
    if (r < 0) {
      streaming_message_t *e = streaming_msg_create_code(SMT_STOP, SM_CODE_UNDEFINED_ERROR);
      streaming_target_deliver2(ts->output, e);
      tvhlog(LOG_ERR, "timeshift", "ts %d could not read buffer", ts->id);
      return -1;
    }
#ifdef TSHFT_TRACE
    tvhlog(LOG_DEBUG, "timeshift", "ts %d read msg %p (%ld)",
           ts->id, *sm, r);
#endif

    /* Incomplete */
    if (r == 0) {
      lseek(*fd, *cur_off, SEEK_SET);
      return 0;
    }

    /* Update */
    *cur_off += r;

    /* Special case - EOF */
    if (r == sizeof(size_t) || *cur_off > (*cur_file)->size) {
      close(*fd);
      *fd       = -1;
      pthread_mutex_lock(&ts->rdwr_mutex);
      *cur_file = timeshift_filemgr_next(*cur_file, NULL, 0);
      pthread_mutex_unlock(&ts->rdwr_mutex);
      *cur_off  = 0; // reset
      *wait     = 0;

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
  ( timeshift_t *ts, timeshift_file_t **cur_file, off_t *cur_off, int *fd,
    streaming_message_t **sm, int *wait )
{
  time_t pts = 0;
  while (*cur_file) {
    if (_timeshift_read(ts, cur_file, cur_off, fd, sm, wait) == -1)
      return -1;
    if (!*sm) break;
    if ((*sm)->sm_type == SMT_PACKET) {
      pts = ((th_pkt_t*)(*sm)->sm_data)->pkt_pts;
      tvhlog(LOG_DEBUG, "timeshift", "ts %d deliver %"PRId64" pts=%"PRItime_t,
             ts->id, (*sm)->sm_time, pts);
    }
    streaming_target_deliver2(ts->output, *sm);
    *sm = NULL;
  }
  return 0;
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
  int efd, nfds, end, fd = -1, run = 1, wait = -1;
  timeshift_file_t *cur_file = NULL;
  off_t cur_off = 0;
  int cur_speed = 100, keyframe_mode = 0;
  int64_t pause_time = 0, play_time = 0, last_time = 0;
  int64_t now, deliver, skip_time = 0;
  streaming_message_t *sm = NULL, *ctrl = NULL;
  timeshift_index_iframe_t *tsi = NULL;
  streaming_skip_t *skip = NULL;
  time_t last_status = 0;

  /* Poll */
  struct epoll_event ev = { 0 };
  efd        = epoll_create(1);
  ev.events  = EPOLLIN;
  ev.data.fd = ts->rd_pipe.rd;
  epoll_ctl(efd, EPOLL_CTL_ADD, ev.data.fd, &ev);

  /* Output */
  while (run) {

    // Note: Previously we allowed unlimited wait, but we now must wake periodically
    //       to output status message
    if (wait < 0 || wait > 1000)
      wait = 1000;

    /* Wait for data */
    if(wait)
      nfds = epoll_wait(efd, &ev, 1, wait);
    else
      nfds = 0;
    wait      = -1;
    end       = 0;
    skip      = NULL;
    now       = getmonoclock();

    /* Control */
    pthread_mutex_lock(&ts->state_mutex);
    if (nfds == 1) {
      if (_read_msg(ev.data.fd, &ctrl) > 0) {

        /* Exit */
        if (ctrl->sm_type == SMT_EXIT) {
#ifdef TSHFT_TRACE
          tvhlog(LOG_DEBUG, "timeshift", "ts %d read exit request", ts->id);
#endif
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
          if (ts->ondemand && (speed < 0))
            speed = 0;

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
                pthread_mutex_lock(&ts->rdwr_mutex);
                if ((cur_file   = timeshift_filemgr_get(ts, 1))) {
                  cur_off    = cur_file->size;
                  pause_time = cur_file->last;
                  last_time  = pause_time;
                }
                pthread_mutex_unlock(&ts->rdwr_mutex);
              }

            /* Buffer playback */
            } else if (ts->state == TS_PLAY) {
              pause_time = last_time;

            /* Paused */
            } else {
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
            play_time  = getmonoclock();
            cur_speed  = speed;
            if (speed != 100 || ts->state != TS_LIVE)
              ts->state = speed == 0 ? TS_PAUSE : TS_PLAY;
            tvhlog(LOG_DEBUG, "timeshift", "ts %d change speed %d",
                   ts->id, speed);
          }

          /* Send on the message */
          ctrl->sm_code = speed;
          streaming_target_deliver2(ts->output, ctrl);
          ctrl = NULL;

        /* Skip/Seek */
        } else if (ctrl->sm_type == SMT_SKIP) {
          skip = ctrl->sm_data;
          switch (skip->type) {
            case SMT_SKIP_ABS_TIME:
              if (ts->pts_delta == PTS_UNSET) {
                tvhlog(LOG_ERR, "timeshift", "ts %d abs skip not possible no PTS delta", ts->id);
                skip = NULL;
                break;
              }
              /* -fallthrough */
            case SMT_SKIP_REL_TIME:

              /* Convert */
              skip_time =  ts_rescale(skip->time, 1000000);
              tvhlog(LOG_DEBUG, "timeshift", "ts %d skip %"PRId64" requested %"PRId64, ts->id, skip_time, skip->time);

              /* Live playback (stage1) */
              if (ts->state == TS_LIVE) {
                pthread_mutex_lock(&ts->rdwr_mutex);
                if ((cur_file   = timeshift_filemgr_get(ts, !ts->ondemand))) {
                  cur_off    = cur_file->size;
                  last_time  = cur_file->last;
                } else {
                  tvhlog(LOG_ERR, "timeshift", "ts %d failed to get current file", ts->id);
                  skip = NULL;
                }
                pthread_mutex_unlock(&ts->rdwr_mutex);
              }

              /* May have failed */
              if (skip) {
                tvhlog(LOG_DEBUG, "timeshift", "ts %d skip last_time %"PRId64" pts_delta %"PRId64,
                       ts->id, last_time, ts->pts_delta);
                skip_time += (skip->type == SMT_SKIP_ABS_TIME) ? ts->pts_delta : last_time;

               /* Live (stage2) */
                if (ts->state == TS_LIVE) {
                  if (skip_time >= now) {
                    tvhlog(LOG_DEBUG, "timeshift", "ts %d skip ignored, already live", ts->id);
                    skip = NULL;
                  } else {
                    ts->state = TS_PLAY;
                  }
                }
              }

              /* OK */
              if (skip) {

                /* Adjust time */
                play_time  = now;
                pause_time = skip_time;
                tsi        = NULL;

                /* Clear existing packet */
                if (sm)
                  streaming_msg_free(sm);
                sm = NULL;
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

    /* Status message */
    if (now >= (last_status + 1000000)) {
      streaming_message_t *tsm;
      timeshift_status_t *status;
      timeshift_index_iframe_t *fst, *lst;
      status = calloc(1, sizeof(timeshift_status_t));
      fst    = _timeshift_first_frame(ts);
      lst    = _timeshift_last_frame(ts);
      status->full  = ts->full;
      status->shift = ts->state <= TS_LIVE ? 0 : ts_rescale_i(now - last_time, 1000000);
      if (lst && fst && lst != fst && ts->pts_delta != PTS_UNSET) {
        status->pts_start = ts_rescale_i(fst->time - ts->pts_delta, 1000000);
        status->pts_end   = ts_rescale_i(lst->time - ts->pts_delta, 1000000);
      } else {
        status->pts_start = PTS_UNSET;
        status->pts_end   = PTS_UNSET;
      }
      tsm = streaming_msg_create_data(SMT_TIMESHIFT_STATUS, status);
      streaming_target_deliver2(ts->output, tsm);
      last_status = now;
    }

    /* Done */
    if (!run || !cur_file || ((ts->state != TS_PLAY && !skip))) {
      pthread_mutex_unlock(&ts->state_mutex);
      continue;
    }

    /* Calculate delivery time */
    deliver = (now - play_time) + TIMESHIFT_PLAY_BUF;
    deliver = (deliver * cur_speed) / 100;
    deliver = (deliver + pause_time);

    /* Determine next packet */
    if (!sm) {

      /* Rewind or Fast forward (i-frame only) */
      if (skip || keyframe_mode) {
        timeshift_file_t *tsf = NULL;
        int64_t req_time;

        /* Time */
        if (!skip)
          req_time = last_time + ((cur_speed < 0) ? -1 : 1);
        else
          req_time = skip_time;
        tvhlog(LOG_DEBUG, "timeshift", "ts %d skip to %"PRId64" from %"PRId64, ts->id, req_time, last_time);

        /* Find */
        pthread_mutex_lock(&ts->rdwr_mutex);
        end = _timeshift_skip(ts, req_time, last_time,
                              cur_file, &tsf, &tsi);
        pthread_mutex_unlock(&ts->rdwr_mutex);
        if (tsi)
          tvhlog(LOG_DEBUG, "timeshift", "ts %d skip found pkt @ %"PRId64, ts->id, tsi->time);

        /* File changed (close) */
        if ((tsf != cur_file) && (fd != -1)) {
          close(fd);
          fd = -1;
        }

        /* Position */
        if (cur_file)
          cur_file->refcount--;
        cur_file = tsf;
        if (tsi)
          cur_off = tsi->pos;
        else
          cur_off = 0;
      }

      /* Find packet */
      if (_timeshift_read(ts, &cur_file, &cur_off, &fd, &sm, &wait) == -1) {
        pthread_mutex_unlock(&ts->state_mutex);
        break;
      }
    }

    /* Send skip response */
    if (skip) {
      if (sm && sm->sm_type == SMT_PACKET) {
        th_pkt_t *pkt = sm->sm_data;
        skip->time = pkt->pkt_pts;
        skip->type = SMT_SKIP_ABS_TIME;
        tvhlog(LOG_DEBUG, "timeshift", "ts %d skip to %"PRId64" ok", ts->id, skip->time);
      } else {
        /* Report error */
        skip->type = SMT_SKIP_ERROR;
        skip       = NULL;
        tvhlog(LOG_DEBUG, "timeshift", "ts %d skip failed", ts->id);
      }
      streaming_target_deliver2(ts->output, ctrl);
      ctrl = NULL;
    }

    /* Deliver */
    if (sm && (skip ||
               (((cur_speed < 0) && (sm->sm_time >= deliver)) ||
               ((cur_speed > 0) && (sm->sm_time <= deliver))))) {

#ifndef TSHFT_TRACE
      if (skip)
#endif
      {
        time_t pts = 0;
        int64_t delta = now - sm->sm_time;
        if (sm->sm_type == SMT_PACKET)
          pts = ((th_pkt_t*)sm->sm_data)->pkt_pts;
        tvhlog(LOG_DEBUG, "timeshift", "ts %d deliver %"PRId64" pts=%"PRItime_t " shift=%"PRIu64,
               ts->id, sm->sm_time, pts, delta);
      }
      streaming_target_deliver2(ts->output, sm);
      last_time = sm->sm_time;
      sm        = NULL;
      wait      = 0;
    } else if (sm) {
      if (cur_speed > 0)
        wait = (sm->sm_time - deliver) / 1000;
      else
        wait = (deliver - sm->sm_time) / 1000;
      if (wait == 0) wait = 1;
#ifdef TSHFT_TRACE
      tvhlog(LOG_DEBUG, "timeshift", "ts %d wait %d",
             ts->id, wait);
#endif
    }

    /* Terminate */
    if (!cur_file || end != 0) {
      if (!end)
        end = (cur_speed > 0) ? 1 : -1;

      /* Back to live */
      if (end == 1) {
        tvhlog(LOG_DEBUG, "timeshift", "ts %d eob revert to live mode", ts->id);
        ts->state = TS_LIVE;
        cur_speed = 100;
        ctrl      = streaming_msg_create_code(SMT_SPEED, cur_speed);
        streaming_target_deliver2(ts->output, ctrl);

        /* Flush timeshift buffer to live */
        if (_timeshift_flush_to_live(ts, &cur_file, &cur_off, &fd, &sm, &wait) == -1)
          break;

        /* Close file (if open) */
        if (fd != -1) {
          close(fd);
          fd = -1;
        }

        /* Flush ALL files */
        if (ts->ondemand)
          timeshift_filemgr_flush(ts, NULL);

      /* Pause */
      } else {
        if (cur_speed <= 0) {
          cur_speed = 0;
          ts->state = TS_PAUSE;
        } else {
          ts->state = TS_PLAY;
          play_time = now;
        }
        tvhlog(LOG_DEBUG, "timeshift", "ts %d sob speed %d", ts->id, cur_speed);
        pause_time = last_time;
        ctrl       = streaming_msg_create_code(SMT_SPEED, cur_speed);
        streaming_target_deliver2(ts->output, ctrl);
      }
      ctrl = NULL;

    /* Flush unwanted */
    } else if (ts->ondemand && cur_file) {
      pthread_mutex_lock(&ts->rdwr_mutex);
      timeshift_filemgr_flush(ts, cur_file);
      pthread_mutex_unlock(&ts->rdwr_mutex);
    }

    pthread_mutex_unlock(&ts->state_mutex);
  }

  /* Cleanup */
  if (fd != -1) close(fd);
  if (sm)       streaming_msg_free(sm);
  if (ctrl)     streaming_msg_free(ctrl);
#ifdef TSHFT_TRACE
  tvhlog(LOG_DEBUG, "timeshift", "ts %d exit reader thread", ts->id);
#endif

  return NULL;
}
