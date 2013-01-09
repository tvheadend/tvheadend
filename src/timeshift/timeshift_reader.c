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

static int _timeshift_skip
  ( timeshift_t *ts, int64_t req_time, int64_t cur_time,
    timeshift_file_t *cur_file, timeshift_file_t **new_file,
    timeshift_index_iframe_t **iframe )
{
  timeshift_index_iframe_t *tsi  = *iframe;
  timeshift_file_t         *tsf  = cur_file;
  int64_t                   sec  = req_time / 1000000;
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
    if (back) {
      tsf = timeshift_filemgr_last(ts);
      tsi = NULL;
      while (tsf && !tsi) {
        if (!(tsi = TAILQ_FIRST(&tsf->iframes)))
          tsf = timeshift_filemgr_next(tsf, &end, 0);
      }
      end = -1;
    } else {
      tsf = timeshift_filemgr_get(ts, ts->ondemand);
      tsi = NULL;
      while (tsf && !tsi) {
        if (!(tsi = TAILQ_LAST(&tsf->iframes, timeshift_index_iframe_list)))
          tsf = timeshift_filemgr_prev(tsf, &end, 0);
      }
      end = 1;
    }
  }

  /* Done */
  *new_file = tsf;
  *iframe   = tsi;
  return end;
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

  /* Poll */
  struct epoll_event ev = { 0 };
  efd        = epoll_create(1);
  ev.events  = EPOLLIN;
  ev.data.fd = ts->rd_pipe.rd;
  epoll_ctl(efd, EPOLL_CTL_ADD, ev.data.fd, &ev);

  /* Output */
  while (run) {

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
                if ((cur_file   = timeshift_filemgr_get(ts, ts->ondemand))) {
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
            case SMT_SKIP_REL_TIME:
              tvhlog(LOG_DEBUG, "timeshift", "ts %d skip %"PRId64" requested", ts->id, skip->time);

              /* Must handle live playback case */
              if (ts->state == TS_LIVE) {
                if (skip->time < 0) {
                  pthread_mutex_lock(&ts->rdwr_mutex);
                  if ((cur_file   = timeshift_filemgr_get(ts, ts->ondemand))) {
                    ts->state  = TS_PLAY;
                    cur_off    = cur_file->size;
                    last_time  = cur_file->last;
                  } else {
                    tvhlog(LOG_ERR, "timeshift", "ts %d failed to get current file", ts->id);
                    skip = NULL;
                  }
                  pthread_mutex_unlock(&ts->rdwr_mutex);
                } else {
                  tvhlog(LOG_DEBUG, "timeshift", "ts %d skip ignored, already live", ts->id);
                  skip = NULL;
                }
              }

              /* OK */
              if (skip) {
                /* Adjust time */
                play_time  = now;
                pause_time = skip_time = last_time + skip->time;
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

    /* Done */
    if (!run || !cur_file || ((ts->state != TS_PLAY && !skip))) {
      pthread_mutex_unlock(&ts->state_mutex);
      continue;
    }

    /* File processing lock */
    pthread_mutex_lock(&ts->rdwr_mutex);

    /* Calculate delivery time */
    deliver = (now - play_time) + TS_PLAY_BUF;
    deliver = (deliver * cur_speed) / 100;
    deliver = (deliver + pause_time);

    /* Determine next packet */
    if (!sm) {

      /* Rewind or Fast forward (i-frame only) */
      if (skip || keyframe_mode) {
        timeshift_file_t *tsf = NULL;
        time_t req_time;

        /* Time */
        if (!skip)
          req_time = last_time + ((cur_speed < 0) ? -1 : 1);
        else
          req_time = skip_time;

        /* Find */
        end = _timeshift_skip(ts, req_time, last_time,
                              cur_file, &tsf, &tsi);

        /* File changed (close) */
        if ((tsf != cur_file) && (fd != -1)) {
          close(fd);
          fd = -1;
        }

        /* Position */
        cur_file = tsf;
        if (tsi)
          cur_off = tsi->pos;
        else
          cur_off = 0;
      }

      /* Find packet */
      if (cur_file) {

        /* Open file */
        if (fd == -1) {
#ifdef TSHFT_TRACE
          tvhlog(LOG_DEBUG, "timeshift", "ts %d open file %s",
                 ts->id, cur_file->path);
#endif
          fd = open(cur_file->path, O_RDONLY);
        }
        if (cur_off) {
#ifdef TSHFT_TRACE
          tvhlog(LOG_DEBUG, "timeshift", "ts %d seek to %lu", ts->id, cur_off);
#endif
          lseek(fd, cur_off, SEEK_SET);
        }

        /* Read msg */
        ssize_t r = _read_msg(fd, &sm);
        if (r < 0) {
          streaming_message_t *e = streaming_msg_create_code(SMT_STOP, SM_CODE_UNDEFINED_ERROR);
          streaming_target_deliver2(ts->output, e);
          tvhlog(LOG_ERR, "timeshift", "ts %d could not read buffer", ts->id);
          pthread_mutex_unlock(&ts->rdwr_mutex);
          pthread_mutex_unlock(&ts->state_mutex);
          break;
        }
#ifdef TSHFT_TRACE
        tvhlog(LOG_DEBUG, "timeshift", "ts %d read msg %p (%ld)",
               ts->id, sm, r);
#endif

        /* Incomplete */
        if (r == 0)
          lseek(fd, cur_off, SEEK_SET);
        else
          cur_off += r;

        /* Special case - EOF */
        if (r == sizeof(size_t) || cur_off > cur_file->size) {
          close(fd);
          fd       = -1;
          cur_file = timeshift_filemgr_next(cur_file, NULL, 0);
          cur_off  = 0; // reset
          wait     = 0;
        }
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

      sm->sm_timeshift = now - sm->sm_time;
#ifdef TSHFT_TRACE
      {
        time_t pts = 0;
        if (sm->sm_type == SMT_PACKET)
          pts = ((th_pkt_t*)sm->sm_data)->pkt_pts;
        tvhlog(LOG_DEBUG, "timeshift", "ts %d deliver %"PRItime_t" pts=%"PRItime_t " shift=%"PRIu64,
               ts->id, sm->sm_time, pts, sm->sm_timeshift );
      }
#endif
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
        end = (cur_file > 0) ? 1 : -1;

      /* Back to live */
      if (end == 1) {
        tvhlog(LOG_DEBUG, "timeshift", "ts %d eob revert to live mode", ts->id);
        ts->state = TS_LIVE;
        cur_speed = 100;
        ctrl      = streaming_msg_create_code(SMT_SPEED, cur_speed);
        streaming_target_deliver2(ts->output, ctrl);

        /* Flush ALL files */
        if (ts->ondemand)
          timeshift_filemgr_flush(ts, NULL);

      /* Pause */
      } else {
        tvhlog(LOG_DEBUG, "timeshift", "ts %d sob pause stream", ts->id);
        cur_speed  = 0;
        ts->state  = TS_PAUSE;
        pause_time = now;
        ctrl       = streaming_msg_create_code(SMT_SPEED, cur_speed);
        streaming_target_deliver2(ts->output, ctrl);
      }
      ctrl = NULL;

    /* Flush unwanted */
    } else if (ts->ondemand && cur_file) {
      timeshift_filemgr_flush(ts, cur_file);
    }

    pthread_mutex_unlock(&ts->rdwr_mutex);
    pthread_mutex_unlock(&ts->state_mutex);
  }

  /* Cleanup */
  if (sm)   streaming_msg_free(sm);
  if (ctrl) streaming_msg_free(ctrl);
#ifdef TSHFT_TRACE
  tvhlog(LOG_DEBUG, "timeshift", "ts %d exit reader thread", ts->id);
#endif

  return NULL;
}
