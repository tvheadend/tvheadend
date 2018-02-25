 /*
 *  Tvheadend - Linux DVB DDCI
 *
 *  Copyright (C) 2017 Jasmin Jessich
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
#include "linuxdvb_private.h"
#include "tvhpoll.h"
#include "input/mpegts/tsdemux.h"

#include <fcntl.h>

/* DD CI send Buffer size in number of 188 byte packages */
// FIXME: make a config parameter
#define LDDCI_SEND_BUF_NUM_DEF  1500
#define LDDCI_RECV_BUF_NUM_DEF  1500

#define LDDCI_SEND_BUFFER_POLL_TMO  150    /* ms */
#define LDDCI_TS_SYNC_BYTE          0x47
#define LDDCI_TS_SIZE               188
#define LDDCI_TS_SCRAMBLING_CONTROL 0xC0

#define LDDCI_TO_THREAD(_t)  (linuxdvb_ddci_thread_t *)(_t)

#define LDDCI_WR_THREAD_STAT_TMO  10  /* sec */
#define LDDCI_RD_THREAD_STAT_TMO  10  /* sec */

#define LDDCI_MIN_TS_PKT (100 * LDDCI_TS_SIZE)
#define LDDCI_MIN_TS_SYN (5 * LDDCI_TS_SIZE)

typedef struct linuxdvb_ddci_thread
{
  linuxdvb_ddci_t          *lddci;
  int                       lddci_thread_running;
  int                       lddci_thread_stop;
  pthread_t                 lddci_thread;
  pthread_mutex_t           lddci_thread_lock;
  tvh_cond_t                lddci_thread_cond;
} linuxdvb_ddci_thread_t;

typedef struct linuxdvb_ddci_send_packet
{
  TAILQ_ENTRY(linuxdvb_ddci_send_packet)  lddci_send_pkt_link;
  size_t                                  lddci_send_pkt_len;
  uint8_t                                 lddci_send_pkt_data[0];
} linuxdvb_ddci_send_packet_t;

typedef struct linuxdvb_ddci_send_buffer
{
  TAILQ_HEAD(,linuxdvb_ddci_send_packet)  lddci_send_buf_queue;
  uint64_t                                lddci_send_buf_size_max;
  uint64_t                                lddci_send_buf_size;
  pthread_mutex_t                         lddci_send_buf_lock;
  tvh_cond_t                              lddci_send_buf_cond;
  tvhlog_limit_t                          lddci_send_buf_loglimit;
  int                                     lddci_send_buf_pkgCntW;
  int                                     lddci_send_buf_pkgCntR;
  int                                     lddci_send_buf_pkgCntWL;
  int                                     lddci_send_buf_pkgCntRL;
} linuxdvb_ddci_send_buffer_t;

typedef struct linuxdvb_ddci_wr_thread
{
  linuxdvb_ddci_thread_t;    /* have to be at first */
  int                          lddci_cfg_send_buffer_sz; /* in TS packages */
  linuxdvb_ddci_send_buffer_t  lddci_send_buffer;
  mtimer_t                     lddci_send_buf_stat_tmo;
} linuxdvb_ddci_wr_thread_t;

typedef struct linuxdvb_ddci_rd_thread
{
  linuxdvb_ddci_thread_t;    /* have to be at first */
  int                        lddci_cfg_recv_buffer_sz; /* in TS packages */
  mtimer_t                   lddci_recv_stat_tmo;
  int                        lddci_recv_pkgCntR;
  int                        lddci_recv_pkgCntW;
  int                        lddci_recv_pkgCntRL;
  int                        lddci_recv_pkgCntWL;
  int                        lddci_recv_pkgCntS;
  int                        lddci_recv_pkgCntSL;
} linuxdvb_ddci_rd_thread_t;

struct linuxdvb_ddci
{
  linuxdvb_transport_t      *lcat;    /* back link to the associated CA transport */
  char                      *lddci_path;
  char                       lddci_id[6];
  int                        lddci_fdW;
  int                        lddci_fdR;
  linuxdvb_ddci_wr_thread_t  lddci_wr_thread;
  linuxdvb_ddci_rd_thread_t  lddci_rd_thread;

  /* list of services assigned to this DD CI instance */
  idnode_set_t               *lddci_services;
  /* the same mux used for all services */
  mpegts_mux_t               *lddci_mm;
  /* the same input used for all services */
  mpegts_input_t             *lddci_mi;
  const uint8_t              *lddci_prev_tsb;
};


/* When a DD CI is disabled on the WEB UI, the global lock is held
 * when the threads are stopped. But this is not the case when THV is closed.
 * So it is OK to check if the global lock is held and to omit the locking
 * in this case. It can't happen, that the global lock is unlocked just after
 * the lock check.
 */
static void
linuxdvb_ddci_mtimer_disarm ( mtimer_t *mti )
{
  int locked;

  locked = ! pthread_mutex_trylock(&global_lock);
  mtimer_disarm(mti);
  if (locked)
    pthread_mutex_unlock(&global_lock);
}

/* When a DD CI is enabled on the WEB UI, the global lock is held when the
 * threads are started. This is also the case when THV is started.
 * So it is OK to check if the global lock is held and to omit the locking
 * in this case. It can't happen, that the global lock is unlocked just after
 * the lock check.
 */
static void
linuxdvb_ddci_mtimer_arm_rel
  ( mtimer_t *mti, mti_callback_t *callback, void *opaque, int64_t delta )
{
  int locked;

  locked = ! pthread_mutex_trylock(&global_lock);
  mtimer_arm_rel(mti, callback, opaque, delta);
  if (locked)
    pthread_mutex_unlock(&global_lock);
}


/*****************************************************************************
 *
 * DD CI Thread functions
 *
 *****************************************************************************/

static void
linuxdvb_ddci_thread_init
  ( linuxdvb_ddci_t *lddci, linuxdvb_ddci_thread_t *ddci_thread )
{
  ddci_thread->lddci = lddci;
  ddci_thread->lddci_thread_running = 0;
  ddci_thread->lddci_thread_stop = 0;
  pthread_mutex_init(&ddci_thread->lddci_thread_lock, NULL);
  tvh_cond_init(&ddci_thread->lddci_thread_cond);
}

static inline int
linuxdvb_ddci_thread_running ( linuxdvb_ddci_thread_t *ddci_thread )
{
  return ddci_thread->lddci_thread_running;
}

static inline void
linuxdvb_ddci_thread_signal ( linuxdvb_ddci_thread_t *ddci_thread )
{
  tvh_cond_signal(&ddci_thread->lddci_thread_cond, 0);
}

static int
linuxdvb_ddci_thread_start
  ( linuxdvb_ddci_thread_t *ddci_thread, void *(*thread_routine) (void *),
    void *arg, const char *name )
{
  int e = -1;

  if (!linuxdvb_ddci_thread_running(ddci_thread)) {
    pthread_mutex_lock(&ddci_thread->lddci_thread_lock);
    tvhthread_create(&ddci_thread->lddci_thread, NULL, thread_routine, arg,
                     name);
    do {
      e = tvh_cond_wait(&ddci_thread->lddci_thread_cond,
                        &ddci_thread->lddci_thread_lock);
      if (e == ETIMEDOUT) {
        tvherror(LS_DDCI, "create thread %s error", name );
        break;
      }
    } while (ERRNO_AGAIN(e));
    pthread_mutex_unlock(&ddci_thread->lddci_thread_lock);
  }

  return e;
}

static void
linuxdvb_ddci_thread_stop ( linuxdvb_ddci_thread_t *ddci_thread )
{
  if (linuxdvb_ddci_thread_running(ddci_thread)) {
    ddci_thread->lddci_thread_stop = 1;
    pthread_join(ddci_thread->lddci_thread, NULL);
  }
}


/*****************************************************************************
 *
 * DD CI Send Buffer functions
 *
 *****************************************************************************/

static void
linuxdvb_ddci_send_buffer_init
  ( linuxdvb_ddci_send_buffer_t *ddci_snd_buf, uint64_t ddci_snd_buf_max )
{
  TAILQ_INIT(&ddci_snd_buf->lddci_send_buf_queue);
  ddci_snd_buf->lddci_send_buf_size_max = ddci_snd_buf_max;
  ddci_snd_buf->lddci_send_buf_size = 0;
  pthread_mutex_init(&ddci_snd_buf->lddci_send_buf_lock, NULL);
  tvh_cond_init(&ddci_snd_buf->lddci_send_buf_cond);
  tvhlog_limit_reset(&ddci_snd_buf->lddci_send_buf_loglimit);
  ddci_snd_buf->lddci_send_buf_pkgCntW = 0;
  ddci_snd_buf->lddci_send_buf_pkgCntR = 0;
  ddci_snd_buf->lddci_send_buf_pkgCntWL = 0;
  ddci_snd_buf->lddci_send_buf_pkgCntRL = 0;

}

/* must be called with locked mutex */
static inline void
linuxdvb_ddci_send_buffer_remove
   ( linuxdvb_ddci_send_buffer_t *ddci_snd_buf, linuxdvb_ddci_send_packet_t *sp )
{
  if (sp) {
    assert( ddci_snd_buf->lddci_send_buf_size >= sp->lddci_send_pkt_len);
    ddci_snd_buf->lddci_send_buf_size -= sp->lddci_send_pkt_len;
    // memoryinfo_free(&mpegts_input_queue_memoryinfo, sizeof(mpegts_packet_t) + mp->mp_len);
    ddci_snd_buf->lddci_send_buf_pkgCntR += sp->lddci_send_pkt_len / LDDCI_TS_SIZE;
    TAILQ_REMOVE(&ddci_snd_buf->lddci_send_buf_queue, sp, lddci_send_pkt_link);
  }
}

static linuxdvb_ddci_send_packet_t *
linuxdvb_ddci_send_buffer_get
  ( linuxdvb_ddci_send_buffer_t *ddci_snd_buf, int64_t tmo )
{
  linuxdvb_ddci_send_packet_t  *sp;

  pthread_mutex_lock(&ddci_snd_buf->lddci_send_buf_lock);

  /* packet present? */
  sp = TAILQ_FIRST(&ddci_snd_buf->lddci_send_buf_queue);
  if (!sp) {
    int r;

    do {
      int64_t mono = mclk() + ms2mono(tmo);

      /* Wait for a packet */
      r = tvh_cond_timedwait(&ddci_snd_buf->lddci_send_buf_cond,
                             &ddci_snd_buf->lddci_send_buf_lock, mono);
      if (r == ETIMEDOUT) {
        break;
      }
    } while (ERRNO_AGAIN(r));

    sp = TAILQ_FIRST(&ddci_snd_buf->lddci_send_buf_queue);
  }

  linuxdvb_ddci_send_buffer_remove(ddci_snd_buf, sp);

  pthread_mutex_unlock(&ddci_snd_buf->lddci_send_buf_lock);

  return sp;
}

static void
linuxdvb_ddci_send_buffer_put
  ( linuxdvb_ddci_send_buffer_t *ddci_snd_buf, const uint8_t *tsb, int len )
{
  pthread_mutex_lock(&ddci_snd_buf->lddci_send_buf_lock);

  if (ddci_snd_buf->lddci_send_buf_size < ddci_snd_buf->lddci_send_buf_size_max) {
    linuxdvb_ddci_send_packet_t  *sp;

#if 0
    /* Note: This debug output will work only for one DD CI instance! */
    {
      static uint8_t pid_seen[ 8192];
      int pid, idx = 0;

      while (idx < len) {
        pid = (tsb[idx+1] & 0x1f) << 8 | tsb[idx+2];
        if (!pid_seen[pid]) {
          tvhtrace(LS_DDCI, "CAM PID %d", pid);
          pid_seen[pid] = 1;
        }
        idx += LDDCI_TS_SIZE;
      }
    }
#endif

    sp = malloc(sizeof(linuxdvb_ddci_send_packet_t) + len);
    sp->lddci_send_pkt_len = len;
    memcpy(sp->lddci_send_pkt_data, tsb, len);
    ddci_snd_buf->lddci_send_buf_size += len;
    ddci_snd_buf->lddci_send_buf_pkgCntW += len / LDDCI_TS_SIZE;
    // memoryinfo_alloc(&mpegts_input_queue_memoryinfo, sizeof(mpegts_packet_t) + len2);
    TAILQ_INSERT_TAIL(&ddci_snd_buf->lddci_send_buf_queue, sp, lddci_send_pkt_link);
    tvh_cond_signal(&ddci_snd_buf->lddci_send_buf_cond, 0);
  } else {
    if (tvhlog_limit(&ddci_snd_buf->lddci_send_buf_loglimit, 10))
      tvhwarn(LS_DDCI, "too much queued output data in send buffer, discarding new");
  }

  pthread_mutex_unlock(&ddci_snd_buf->lddci_send_buf_lock);
}

static void
linuxdvb_ddci_send_buffer_clear ( linuxdvb_ddci_send_buffer_t *ddci_snd_buf )
{
  linuxdvb_ddci_send_packet_t  *sp;

  pthread_mutex_lock(&ddci_snd_buf->lddci_send_buf_lock);

  while ((sp = TAILQ_FIRST(&ddci_snd_buf->lddci_send_buf_queue)))
  {
    linuxdvb_ddci_send_buffer_remove(ddci_snd_buf, sp);
    free(sp);
  }
  ddci_snd_buf->lddci_send_buf_pkgCntW = 0;
  ddci_snd_buf->lddci_send_buf_pkgCntR = 0;
  ddci_snd_buf->lddci_send_buf_pkgCntWL = 0;
  ddci_snd_buf->lddci_send_buf_pkgCntRL = 0;

  pthread_mutex_unlock(&ddci_snd_buf->lddci_send_buf_lock);
}

static void
linuxdvb_ddci_send_buffer_statistic
  ( linuxdvb_ddci_send_buffer_t *ddci_snd_buf, char *ci_id )
{
  int   pkgCntR = ddci_snd_buf->lddci_send_buf_pkgCntR;
  int   pkgCntW = ddci_snd_buf->lddci_send_buf_pkgCntW;

  if ((pkgCntR != ddci_snd_buf->lddci_send_buf_pkgCntRL) ||
      (pkgCntW != ddci_snd_buf->lddci_send_buf_pkgCntWL)) {
    tvhtrace(LS_DDCI, "CAM %s send buff rd(-> CAM):%d, wr:%d",
             ci_id, pkgCntR, pkgCntW);
    ddci_snd_buf->lddci_send_buf_pkgCntRL = pkgCntR;
    ddci_snd_buf->lddci_send_buf_pkgCntWL = pkgCntW;
  }
}


/*****************************************************************************
 *
 * DD CI Writer Thread functions
 *
 *****************************************************************************/
static void
linuxdvb_ddci_wr_thread_statistic ( void *aux )
{
  linuxdvb_ddci_wr_thread_t *ddci_wr_thread = aux;
  linuxdvb_ddci_thread_t    *ddci_thread = aux;
  char                      *ci_id = ddci_thread->lddci->lddci_id;

  /* timer callback is executed with global lock */
  lock_assert(&global_lock);

  linuxdvb_ddci_send_buffer_statistic(&ddci_wr_thread->lddci_send_buffer, ci_id);

  mtimer_arm_rel(&ddci_wr_thread->lddci_send_buf_stat_tmo,
                 linuxdvb_ddci_wr_thread_statistic, ddci_wr_thread,
                 sec2mono(LDDCI_WR_THREAD_STAT_TMO));
}

static void
linuxdvb_ddci_wr_thread_statistic_clr ( linuxdvb_ddci_wr_thread_t *ddci_wr_thread )
{
  linuxdvb_ddci_thread_t    *ddci_thread = (linuxdvb_ddci_thread_t *)ddci_wr_thread;
  char                      *ci_id = ddci_thread->lddci->lddci_id;

  linuxdvb_ddci_send_buffer_statistic(&ddci_wr_thread->lddci_send_buffer, ci_id);
  linuxdvb_ddci_send_buffer_clear(&ddci_wr_thread->lddci_send_buffer);

}

static void *
linuxdvb_ddci_write_thread ( void *arg )
{
  linuxdvb_ddci_wr_thread_t *ddci_wr_thread = arg;
  linuxdvb_ddci_thread_t    *ddci_thread = arg;

  int                        fd = ddci_thread->lddci->lddci_fdW;
  char                      *ci_id = ddci_thread->lddci->lddci_id;

  ddci_thread->lddci_thread_running = 1;
  ddci_thread->lddci_thread_stop = 0;
  linuxdvb_ddci_mtimer_arm_rel(&ddci_wr_thread->lddci_send_buf_stat_tmo,
                               linuxdvb_ddci_wr_thread_statistic,
                               ddci_wr_thread,
                               sec2mono(LDDCI_WR_THREAD_STAT_TMO));
  tvhtrace(LS_DDCI, "CAM %s write thread started", ci_id);
  linuxdvb_ddci_thread_signal(ddci_thread);
  while (tvheadend_is_running() && !ddci_thread->lddci_thread_stop) {
    linuxdvb_ddci_send_packet_t *sp;

    sp = linuxdvb_ddci_send_buffer_get(&ddci_wr_thread->lddci_send_buffer,
                                       LDDCI_SEND_BUFFER_POLL_TMO);
    if (sp) {
      int r = tvh_write(fd, sp->lddci_send_pkt_data, sp->lddci_send_pkt_len);
      if (r)
        tvhwarn(LS_DDCI, "couldn't write to CAM %s:%m", ci_id);
      free(sp);
    }
  }

  linuxdvb_ddci_mtimer_disarm(&ddci_wr_thread->lddci_send_buf_stat_tmo);
  tvhtrace(LS_DDCI, "CAM %s write thread finished", ci_id);
  ddci_thread->lddci_thread_stop = 0;
  ddci_thread->lddci_thread_running = 0;
  return NULL;
}

static inline void
linuxdvb_ddci_wr_thread_init ( linuxdvb_ddci_t *lddci )
{
  linuxdvb_ddci_thread_init(lddci, LDDCI_TO_THREAD(&lddci->lddci_wr_thread));
}

static int
linuxdvb_ddci_wr_thread_start ( linuxdvb_ddci_wr_thread_t *ddci_wr_thread )
{
  int e;

  // FIXME: Use a configuration parameter
  ddci_wr_thread->lddci_cfg_send_buffer_sz = LDDCI_SEND_BUF_NUM_DEF * LDDCI_TS_SIZE;
  linuxdvb_ddci_send_buffer_init(&ddci_wr_thread->lddci_send_buffer,
                                 ddci_wr_thread->lddci_cfg_send_buffer_sz);
  e = linuxdvb_ddci_thread_start(LDDCI_TO_THREAD(ddci_wr_thread),
                                 linuxdvb_ddci_write_thread, ddci_wr_thread,
                                 "lnx-ddci-wr");

  return e;
}

static inline void
linuxdvb_ddci_wr_thread_stop ( linuxdvb_ddci_wr_thread_t *ddci_wr_thread )
{
  /* See function linuxdvb_ddci_wr_thread_buffer_put why we lock here.
   */

  pthread_mutex_lock(&ddci_wr_thread->lddci_thread_lock);
  linuxdvb_ddci_thread_stop(LDDCI_TO_THREAD(ddci_wr_thread));
  linuxdvb_ddci_send_buffer_clear(&ddci_wr_thread->lddci_send_buffer);
  pthread_mutex_unlock(&ddci_wr_thread->lddci_thread_lock);
}

static inline void
linuxdvb_ddci_wr_thread_buffer_put
  ( linuxdvb_ddci_wr_thread_t *ddci_wr_thread, const uint8_t *tsb, int len )
{
  /* We need to lock this function against linuxdvb_ddci_wr_thread_stop, because
   * linuxdvb_ddci_wr_thread_buffer_put may be executed by another thread
   * simultaneously, although the stop function is already running. Due to the
   * race condition with the thread_running flag, it may happen, that the buffer
   * is not empty after the stop function is finished. The next execution of
   * linuxdvb_ddci_wr_thread_start will then re-init the queue and the wrongly
   * stored data is lost -> memory leak.
   */
  pthread_mutex_lock(&ddci_wr_thread->lddci_thread_lock);
  if (linuxdvb_ddci_thread_running(LDDCI_TO_THREAD(ddci_wr_thread)))
    linuxdvb_ddci_send_buffer_put(&ddci_wr_thread->lddci_send_buffer, tsb, len );
  pthread_mutex_unlock(&ddci_wr_thread->lddci_thread_lock);
}


/*****************************************************************************
 *
 * DD CI Reader Thread functions
 *
 *****************************************************************************/
static void
linuxdvb_ddci_rd_thread_statistic ( void *aux )
{
  linuxdvb_ddci_rd_thread_t *ddci_rd_thread = aux;
  linuxdvb_ddci_thread_t    *ddci_thread = aux;
  char                      *ci_id = ddci_thread->lddci->lddci_id;
  int pkgCntR = ddci_rd_thread->lddci_recv_pkgCntR;
  int pkgCntW = ddci_rd_thread->lddci_recv_pkgCntW;
  int pkgCntS = ddci_rd_thread->lddci_recv_pkgCntS;

  /* timer callback is executed with global lock */
  lock_assert(&global_lock);

  if ((pkgCntR != ddci_rd_thread->lddci_recv_pkgCntRL) ||
      (pkgCntW != ddci_rd_thread->lddci_recv_pkgCntWL)) {
    tvhtrace(LS_DDCI, "CAM %s recv rd(CAM ->):%d, wr:%d",
             ci_id, pkgCntR, pkgCntW);
    ddci_rd_thread->lddci_recv_pkgCntRL = pkgCntR;
    ddci_rd_thread->lddci_recv_pkgCntWL = pkgCntW;
  }
  if ((pkgCntS != ddci_rd_thread->lddci_recv_pkgCntSL)) {
    tvhtrace(LS_DDCI, "CAM %s got %d scrambled packets from CAM",
             ci_id, pkgCntS);
    ddci_rd_thread->lddci_recv_pkgCntSL = pkgCntS;
  }

  mtimer_arm_rel(&ddci_rd_thread->lddci_recv_stat_tmo,
                 linuxdvb_ddci_rd_thread_statistic, ddci_rd_thread,
                 sec2mono(LDDCI_RD_THREAD_STAT_TMO));
}

static void
linuxdvb_ddci_rd_thread_statistic_clr ( linuxdvb_ddci_rd_thread_t *ddci_rd_thread )
{
  ddci_rd_thread->lddci_recv_pkgCntR = 0;
  ddci_rd_thread->lddci_recv_pkgCntW = 0;
  ddci_rd_thread->lddci_recv_pkgCntRL = 0;
  ddci_rd_thread->lddci_recv_pkgCntWL = 0;
  ddci_rd_thread->lddci_recv_pkgCntS = 0;
  ddci_rd_thread->lddci_recv_pkgCntSL = 0;
}

static int inline
ddci_ts_sync_count ( const uint8_t *tsb, int len )
{
  const uint8_t *start = tsb;

#define LDDCI_TS_SIZE_10  (LDDCI_TS_SIZE * 10)

  while (len >= LDDCI_TS_SIZE) {
    if (len >= LDDCI_TS_SIZE_10 &&
        tsb[0*LDDCI_TS_SIZE] == LDDCI_TS_SYNC_BYTE &&
        tsb[1*LDDCI_TS_SIZE] == LDDCI_TS_SYNC_BYTE &&
        tsb[2*LDDCI_TS_SIZE] == LDDCI_TS_SYNC_BYTE &&
        tsb[3*LDDCI_TS_SIZE] == LDDCI_TS_SYNC_BYTE &&
        tsb[4*LDDCI_TS_SIZE] == LDDCI_TS_SYNC_BYTE &&
        tsb[5*LDDCI_TS_SIZE] == LDDCI_TS_SYNC_BYTE &&
        tsb[6*LDDCI_TS_SIZE] == LDDCI_TS_SYNC_BYTE &&
        tsb[7*LDDCI_TS_SIZE] == LDDCI_TS_SYNC_BYTE &&
        tsb[8*LDDCI_TS_SIZE] == LDDCI_TS_SYNC_BYTE &&
        tsb[9*LDDCI_TS_SIZE] == LDDCI_TS_SYNC_BYTE) {
      len -= LDDCI_TS_SIZE_10;
      tsb += LDDCI_TS_SIZE_10;
    } else if (*tsb == LDDCI_TS_SYNC_BYTE) {
      len -= LDDCI_TS_SIZE;
      tsb += LDDCI_TS_SIZE;
    } else {
      break;
    }
  }
  return tsb - start;
}

static int
ddci_ts_sync_search ( const uint8_t *tsb, int len )
{
  int skipped = 0;

  while ((len > LDDCI_MIN_TS_SYN) &&
         (ddci_ts_sync_count(tsb, len) < LDDCI_MIN_TS_SYN)) {
    tsb++;
    len--;
    skipped++;
  }
  return skipped;
}

static inline int
ddci_ts_sync ( const uint8_t *tsb, int len )
{
  /* it is enough to check the first byte for sync, because the data
   * written into the CAM was completely in sync. In fact this check is
   * required only for the first synchronization phase or when another
   * stream has been tuned.
   */
  return *tsb == LDDCI_TS_SYNC_BYTE ? 0 : ddci_ts_sync_search(tsb, len);
}

static inline int
ddci_is_scrambled(const uint8_t *tsb)
{
  return tsb[3] & LDDCI_TS_SCRAMBLING_CONTROL;
}

static void *
linuxdvb_ddci_read_thread ( void *arg )
{
  linuxdvb_ddci_rd_thread_t *ddci_rd_thread = arg;
  linuxdvb_ddci_thread_t    *ddci_thread = arg;
  int                        fd = ddci_thread->lddci->lddci_fdR;
  char                      *ci_id = ddci_thread->lddci->lddci_id;
  tvhpoll_event_t ev[1];
  tvhpoll_t *efd;
  sbuf_t sb;

  /* Setup poll */
  efd = tvhpoll_create(1);
  tvhpoll_add1(efd, fd, TVHPOLL_IN, NULL);

  /* Allocate memory */
  sbuf_init_fixed(&sb, MINMAX(ddci_rd_thread->lddci_cfg_recv_buffer_sz,
                              LDDCI_TS_SIZE * 100, LDDCI_TS_SIZE * 10000));

  ddci_thread->lddci_thread_running = 1;
  ddci_thread->lddci_thread_stop = 0;
  linuxdvb_ddci_rd_thread_statistic_clr(ddci_rd_thread);
  linuxdvb_ddci_mtimer_arm_rel(&ddci_rd_thread->lddci_recv_stat_tmo,
                               linuxdvb_ddci_rd_thread_statistic, ddci_rd_thread,
                               sec2mono(LDDCI_RD_THREAD_STAT_TMO));
  tvhtrace(LS_DDCI, "CAM %s read thread started", ci_id);
  linuxdvb_ddci_thread_signal(ddci_thread);
  while (tvheadend_is_running() && !ddci_thread->lddci_thread_stop) {
    int nfds, num_pkg, pkg_chk = 0, scrambled = 0;
    ssize_t n;

    nfds = tvhpoll_wait(efd, ev, 1, 150);
    if (nfds <= 0) continue;

    /* Read */
    errno = 0;
    if ((n = sbuf_read(&sb, fd)) < 0) {
      if (ERRNO_AGAIN(errno))
        continue;
      if (errno == EOVERFLOW)
        tvhwarn(LS_DDCI, "read buffer overflow on CAM %s:%m", ci_id);
      else
        tvhwarn(LS_DDCI, "couldn't read from CAM %s:%m", ci_id);
      continue;
    }

    if (sb.sb_ptr > 0) {
      int len, skip;
      uint8_t *tsb;

      len = sb.sb_ptr;
      if (len < LDDCI_MIN_TS_PKT)
          continue;

      tsb = sb.sb_data;
      skip = ddci_ts_sync(tsb, len);
      if (skip) {
        tvhwarn(LS_DDCI, "CAM %s skipped %d bytes to sync on start of TS packet",
                ci_id, skip);
        sbuf_cut(&sb, skip);
        len = sb.sb_ptr;
      }

      if (len < LDDCI_MIN_TS_SYN)
          continue;

      /* receive only whole packets */
      len -= len % LDDCI_TS_SIZE;
      num_pkg = len / LDDCI_TS_SIZE;
      ddci_rd_thread->lddci_recv_pkgCntR += num_pkg;

      /* FIXME: Once we implement CI+, this needs to be not executed, because
       *        a CI+ CAM will use the scrambled bits for re-scrambling the TS
       *        stream
       */
      while (pkg_chk < num_pkg) {
        if (ddci_is_scrambled(tsb + (pkg_chk * LDDCI_TS_SIZE)))
          ++scrambled;

        ++pkg_chk;
      }
      ddci_rd_thread->lddci_recv_pkgCntS += scrambled;

      mpegts_input_postdemux(ddci_thread->lddci->lddci_mi,
                             ddci_thread->lddci->lddci_mm, tsb, len);

      ddci_rd_thread->lddci_recv_pkgCntW += num_pkg;

      if (idnode_set_empty(ddci_thread->lddci->lddci_services)) {
        linuxdvb_ddci_rd_thread_statistic(ddci_rd_thread);
        linuxdvb_ddci_rd_thread_statistic_clr(ddci_rd_thread);
      }

      /* handled */
      sbuf_cut(&sb, len);
    }
  }

  sbuf_free(&sb);
  tvhpoll_destroy(efd);
  linuxdvb_ddci_mtimer_disarm(&ddci_rd_thread->lddci_recv_stat_tmo);
  tvhtrace(LS_DDCI, "CAM %s read thread finished", ci_id);
  ddci_thread->lddci_thread_stop = 0;
  ddci_thread->lddci_thread_running = 0;
  return NULL;
}

static inline void
linuxdvb_ddci_rd_thread_init ( linuxdvb_ddci_t *lddci )
{
  linuxdvb_ddci_thread_init(lddci, LDDCI_TO_THREAD(&lddci->lddci_rd_thread));
}

static int
linuxdvb_ddci_rd_thread_start ( linuxdvb_ddci_rd_thread_t *ddci_rd_thread )
{
  int e;

  // FIXME: Use a configuration parameter
  ddci_rd_thread->lddci_cfg_recv_buffer_sz = LDDCI_RECV_BUF_NUM_DEF * LDDCI_TS_SIZE;
  e = linuxdvb_ddci_thread_start(LDDCI_TO_THREAD(ddci_rd_thread),
                                 linuxdvb_ddci_read_thread, ddci_rd_thread,
                                 "ldvb-ddci-rd");

  return e;
}

static inline void
linuxdvb_ddci_rd_thread_stop ( linuxdvb_ddci_rd_thread_t *ddci_rd_thread )
{
  linuxdvb_ddci_thread_stop(LDDCI_TO_THREAD(ddci_rd_thread));
}


/*****************************************************************************
 *
 * DD CI API functions
 *
 *****************************************************************************/

linuxdvb_ddci_t *
linuxdvb_ddci_create ( linuxdvb_transport_t *lcat, const char *ci_path)
{
  linuxdvb_ddci_t *lddci;

  lddci = calloc(1, sizeof(*lddci));
  lddci->lcat = lcat;
  lddci->lddci_path = strdup(ci_path);
  snprintf(lddci->lddci_id, sizeof(lddci->lddci_id), "ci%u",
           lcat->lcat_adapter->la_dvb_number);
  lddci->lddci_fdW = -1;
  lddci->lddci_fdR = -1;
  linuxdvb_ddci_wr_thread_init(lddci);
  linuxdvb_ddci_rd_thread_init(lddci);
  lddci->lddci_services = idnode_set_create(0);

  tvhtrace(LS_DDCI, "created %s %s", lddci->lddci_id, lddci->lddci_path);

  return lddci;
}

void
linuxdvb_ddci_destroy ( linuxdvb_ddci_t *lddci )
{
  if (!lddci)
    return;
  tvhtrace(LS_DDCI, "destroy %s %s", lddci->lddci_id, lddci->lddci_path);
  linuxdvb_ddci_close(lddci);
  idnode_set_free(lddci->lddci_services);
  free(lddci->lddci_path);
  free(lddci);
}

void
linuxdvb_ddci_close ( linuxdvb_ddci_t *lddci )
{
  int closed = 0;

  if (lddci->lddci_fdW >= 0) {
    tvhtrace(LS_DDCI, "closing write %s %s (fd %d)",
             lddci->lddci_id, lddci->lddci_path, lddci->lddci_fdW);
    linuxdvb_ddci_wr_thread_stop(&lddci->lddci_wr_thread);
    close(lddci->lddci_fdW);
    lddci->lddci_fdW = -1;
    closed = 1;
  }
  if (lddci->lddci_fdR >= 0) {
    tvhtrace(LS_DDCI, "closing read %s %s (fd %d)",
             lddci->lddci_id, lddci->lddci_path, lddci->lddci_fdR);
    linuxdvb_ddci_rd_thread_stop(&lddci->lddci_rd_thread);
    close(lddci->lddci_fdR);
    lddci->lddci_fdR = -1;
    closed = 1;
  }
  if (closed)
    tvhtrace(LS_DDCI, "CAM %s closed", lddci->lddci_id);
}

int
linuxdvb_ddci_open ( linuxdvb_ddci_t *lddci )
{
  int ret = 0;

  if (lddci->lddci_fdW < 0) {
    lddci->lddci_fdW = tvh_open(lddci->lddci_path, O_WRONLY, 0);
    tvhtrace(LS_DDCI, "opening %s %s for write (fd %d)",
             lddci->lddci_id, lddci->lddci_path, lddci->lddci_fdW);
    lddci->lddci_fdR = tvh_open(lddci->lddci_path, O_RDONLY | O_NONBLOCK, 0);
    tvhtrace(LS_DDCI, "opening %s %s for read (fd %d)",
             lddci->lddci_id, lddci->lddci_path, lddci->lddci_fdR);

    if (lddci->lddci_fdW >= 0 && lddci->lddci_fdR >= 0) {
      ret = linuxdvb_ddci_wr_thread_start(&lddci->lddci_wr_thread);
      if (!ret)
        ret = linuxdvb_ddci_rd_thread_start(&lddci->lddci_rd_thread);
    }
    else {
      tvhtrace(LS_DDCI, "open write/read failed %s %s (fd-W %d, fd-R %d)",
               lddci->lddci_id, lddci->lddci_path, lddci->lddci_fdW,
               lddci->lddci_fdR);
      ret = -1;
    }
  }

  if (ret < 0)
    linuxdvb_ddci_close(lddci);
  else
    tvhtrace(LS_DDCI, "CAM %s opened", lddci->lddci_id);

  return ret;
}

void
linuxdvb_ddci_put
  ( linuxdvb_ddci_t *lddci, service_t *t, const uint8_t *tsb, int len )
{
  /* ignore duplicates
   * Note: Checking only the pointer is possible, because the calling
   *       functions will execute the descrambler for the special PIDs in a
   *       loop for each service.
   *       If this ever changes, this code needs to be adapted!
   */
  /* FIXME: In case of MTD we will send all PIDs to the PID translator.
   * This allows to know which packed is for which service, even if the PAT,
   * CAT or EIT is the same, when they originally came from the same
   * transponder. */
  if (lddci->lddci_prev_tsb == tsb) return;
  lddci->lddci_prev_tsb = tsb;

  /* FIXME: For MTD add here the PID translator
   * And also remove the CAT from the stream and generate a faked one.
   */

  linuxdvb_ddci_wr_thread_buffer_put(&lddci->lddci_wr_thread, tsb, len );
}

void
linuxdvb_ddci_assign ( linuxdvb_ddci_t *lddci, service_t *t )
{
  if (!idnode_set_exists(lddci->lddci_services, &t->s_id)) {
    mpegts_service_t *s = (mpegts_service_t *)t;
    const char *txt = "";

    /* first service? -> store common data (for MCD all services have to
     * have the same MUX/Input */
    if (lddci->lddci_services->is_count == 0) {
      lddci->lddci_mm = s->s_dvb_mux;
      lddci->lddci_mi = s->s_dvb_active_input;
    } else {
      txt = "(MCD) ";
      assert( lddci->lddci_mm == s->s_dvb_mux);
      assert( lddci->lddci_mi == s->s_dvb_active_input);
    }

    tvhnotice(LS_DDCI, "CAM %s %sassigned to %p", lddci->lddci_id, txt, t );

    idnode_set_add(lddci->lddci_services, &t->s_id, NULL, NULL);
  }
}

void
linuxdvb_ddci_unassign ( linuxdvb_ddci_t *lddci, service_t *t )
{
  if (idnode_set_exists(lddci->lddci_services, &t->s_id)) {
    tvhnotice(LS_DDCI, "CAM %s unassigned from %p", lddci->lddci_id, t );

    idnode_set_remove(lddci->lddci_services, &t->s_id);
  }

  if (lddci->lddci_services->is_count == 0) {
    lddci->lddci_mm = NULL;
    lddci->lddci_mi = NULL;
    linuxdvb_ddci_wr_thread_statistic_clr(&lddci->lddci_wr_thread);
  }
}

int
linuxdvb_ddci_do_not_assign ( linuxdvb_ddci_t *lddci, service_t *t, int multi )
{
  mpegts_service_t *s = (mpegts_service_t *)t;

  /* nothing assigned? */
  if (!lddci->lddci_mm) return 0;

  /* CAM can do multi channel decoding and new service uses the same mux? */
  if (multi && (lddci->lddci_mm == s->s_dvb_mux)) return 0;

  return 1;
}
