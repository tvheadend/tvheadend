/*
 *  TV headend - Timeshift - channel cache, raw MPEG-TS of a running service
 *  Copyright (C) 2026 Vincent Fortier
 *  Copyright (C) 2026 LiBe-net
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

/*
 * While a service runs, keep the last timeshift "Maximum period" of its
 * MPEG-TS -- exactly what the service hands its subscribers, before any
 * per-subscription parsing -- so a subscription joining late can be given
 * what it missed.  A recording started after its programme began (Kodi's
 * record button, typically) uses it to begin at the programme's start
 * instead of the moment it was requested, whatever its stream profile:
 * packet profiles parse the replayed TS exactly as they parse live TS.
 *
 * The data is kept in blocks, filled from the service pad and flushed by a
 * writer thread into segment files (or kept in RAM with "RAM only").  A
 * gate placed at the head of the joining subscription's chain replays the
 * blocks from the requested time, drops the live data meanwhile -- the
 * cache receives the very same data -- and hands over to live once it has
 * caught up.  Appending to the cache and handing over both happen under
 * the service's s_stream_mutex, so the join has neither a gap nor a
 * duplicate.
 */

#include "tvheadend.h"
#include "streaming.h"
#include "service.h"
#include "timeshift.h"
#include "timeshift/private.h"
#include "timeshift/timeshift_svcbuf.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define SVCBUF_BLOCK_SIZE (256 * 1024)        ///< Block allocation
#define SVCBUF_BLOCK_AGE  sec2mono(1)         ///< Seal a block after this
#define SVCBUF_SEG_SIZE   (64 * 1024 * 1024)  ///< Segment file size
#define SVCBUF_READ_SIZE  (188 * 348)         ///< Replay chunk, whole TS packets
#define SVCBUF_QUEUE_MAX  (32 * 1024 * 1024)  ///< Consumer backlog to wait on

typedef struct svcbuf_seg {
  TAILQ_ENTRY(svcbuf_seg) link;
  int       fd;
  off_t     size;
  int       nblocks;   ///< Blocks stored in it
  int       readers;   ///< Readers in the middle of a read from it
  int       dead;      ///< Close once the last reader is out
  char     *path;
} svcbuf_seg_t;

typedef struct svcbuf_block {
  TAILQ_ENTRY(svcbuf_block) link;
  uint64_t      seq;
  uint64_t      start_seq; ///< Stream-map generation
  streaming_start_t *start; ///< Stream map active for this block
  time_t        wall;     ///< Wall clock of the first byte
  int64_t       mono;     ///< Monotonic clock of the first byte
  uint8_t      *data;     ///< RAM copy, NULL once only on disk
  size_t        alloc;
  size_t        size;
  int           sealed;   ///< Complete, no more appends
  int           settled;  ///< Passed by the writer (on disk, or kept in RAM)
  svcbuf_seg_t *seg;
  off_t         off;      ///< Offset in seg
} svcbuf_block_t;

TAILQ_HEAD(svcbuf_block_queue, svcbuf_block);
TAILQ_HEAD(svcbuf_seg_queue, svcbuf_seg);

enum {
  GATE_WAIT,     ///< Waiting for the start message
  GATE_REPLAY,   ///< Replaying the cache, live data dropped
  GATE_LIVE,     ///< Pass-through
  GATE_DONE,     ///< Bounded historical replay completed; drop live data
  GATE_STOPPED   ///< Being torn down
};

struct svcbuf_gate {
  streaming_target_t   input;
  streaming_target_t  *output;
  svcbuf_t            *sb;
  time_t               from;
  time_t               to;      ///< Historical end; 0 means catch up to live
  int                  all;     ///< Begin at oldest available block
  int                  state;   ///< Changed under s_stream_mutex
  int                 *replaying; ///< Mirrors state == GATE_REPLAY
  time_t              *replay_start; ///< Set to where the replay begins
  streaming_queue_t   *sq;      ///< Consumer queue to pace on, or NULL
  svcbuf_block_t      *blk;     ///< Replay position, pins it (sb->lock)
  size_t               off;
  uint64_t             start_seq; ///< Last stream map sent downstream
  pthread_t            thread;
  int                  thread_started;
  LIST_ENTRY(svcbuf_gate) link;
};

struct svcbuf {
  streaming_target_t        input;
  service_t                *service;
  int                       refcount;
  int                       id;
  tvh_mutex_t               lock;
  tvh_cond_t                cond;
  struct svcbuf_block_queue blocks;
  struct svcbuf_seg_queue   segs;
  svcbuf_block_t           *cur;       ///< Being filled
  svcbuf_block_t           *wr_next;   ///< Next block for the writer
  svcbuf_seg_t             *wr_seg;    ///< Segment being written
  uint64_t                  next_seq;
  streaming_start_t        *start;      ///< Current service stream map
  uint64_t                  start_seq;  ///< Current map generation
  uint64_t                  size;
  char                     *dir;
  int                       seg_index;
  int                       ram_only;
  int                       wr_error;
  int                       run;
  pthread_t                 writer;
  LIST_HEAD(, svcbuf_gate)  gates;
  LIST_HEAD(, svcbuf_reader) readers;
};

struct svcbuf_reader {
  svcbuf_t            *sb;
  svcbuf_block_t      *blk;     ///< Position (sb->lock); NULL = unset
  size_t               off;
  int                  skipped; ///< Its block expired under it (this read)
  int                  lost;    ///< ... since svcbuf_reader_lost() last asked
  LIST_ENTRY(svcbuf_reader) link;
};

static int svcbuf_index;

/* **************************************************************************
 * Storage
 * *************************************************************************/

static svcbuf_seg_t *
svcbuf_seg_open ( svcbuf_t *sb )
{
  char path[PATH_MAX];
  svcbuf_seg_t *seg;
  int fd;

  if (!sb->dir) {
    if (timeshift_filemgr_get_root(path, sizeof(path)))
      return NULL;
    snprintf(path + strlen(path), sizeof(path) - strlen(path),
             "/svcbuf-%d", sb->id);
    if (makedirs(LS_TIMESHIFT, path, 0700, 0, -1, -1))
      return NULL;
    sb->dir = strdup(path);
  }
  snprintf(path, sizeof(path), "%s/%d.ts", sb->dir, sb->seg_index++);
  fd = tvh_open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) {
    tvherror(LS_TIMESHIFT, "svcbuf: unable to create '%s': %s",
             path, strerror(errno));
    return NULL;
  }
  seg = calloc(1, sizeof(*seg));
  seg->fd   = fd;
  seg->path = strdup(path);
  TAILQ_INSERT_TAIL(&sb->segs, seg, link);
  tvhdebug(LS_TIMESHIFT, "svcbuf: %s: new segment %s",
           sb->service->s_nicename, path);
  return seg;
}

static void
svcbuf_seg_close ( svcbuf_t *sb, svcbuf_seg_t *seg )
{
  TAILQ_REMOVE(&sb->segs, seg, link);
  close(seg->fd);
  unlink(seg->path);
  free(seg->path);
  free(seg);
}

static int
svcbuf_write_all ( int fd, const uint8_t *data, size_t size )
{
  ssize_t r;
  while (size > 0) {
    r = write(fd, data, size);
    if (r < 0) {
      if (ERRNO_AGAIN(errno)) continue;
      return -1;
    }
    data += r;
    size -= r;
  }
  return 0;
}

static int
svcbuf_read_all ( int fd, uint8_t *data, size_t size, off_t off )
{
  ssize_t r;
  while (size > 0) {
    r = pread(fd, data, size, off);
    if (r < 0) {
      if (ERRNO_AGAIN(errno)) continue;
      return -1;
    }
    if (r == 0)
      return -1;
    data += r;
    size -= r;
    off  += r;
  }
  return 0;
}

/* Close a segment no block needs any more, once nobody reads from it */
static void
svcbuf_seg_release ( svcbuf_t *sb, svcbuf_seg_t *seg )
{
  if (seg->readers)
    seg->dead = 1;
  else
    svcbuf_seg_close(sb, seg);
}

/* Remove a block (sb->lock held) */
static void
svcbuf_block_remove ( svcbuf_t *sb, svcbuf_block_t *b )
{
  svcbuf_block_t *next = TAILQ_NEXT(b, link);
  svcbuf_reader_t *r;

  /* readers do not hold data back: one still there moves on */
  LIST_FOREACH(r, &sb->readers, link)
    if (r->blk == b) {
      r->blk = next;
      r->off = 0;
      r->skipped = r->lost = 1;
    }
  TAILQ_REMOVE(&sb->blocks, b, link);
  sb->size -= b->size;

  atomic_dec_u64(&timeshift_total_size, b->size);
  if (b->data)
    atomic_dec_u64(&timeshift_total_ram_size, b->size);
  if (sb->cur == b)
    sb->cur = NULL;
  if (sb->wr_next == b)
    sb->wr_next = next;
  if (b->seg) {
    b->seg->nblocks--;
    if (b->seg->nblocks == 0 && b->seg != sb->wr_seg)
      svcbuf_seg_release(sb, b->seg);
  }
  if (b->start)
    streaming_start_unref(b->start);
  free(b->data);
  free(b);
}

/* Oldest block a gate still has to replay (sb->lock held) */
static uint64_t
svcbuf_pinned ( svcbuf_t *sb )
{
  svcbuf_gate_t *g;
  uint64_t pin = UINT64_MAX;
  LIST_FOREACH(g, &sb->gates, link)
    if (g->blk && g->blk->seq < pin)
      pin = g->blk->seq;
  return pin;
}

/* Drop what is beyond the configured period or size (sb->lock held) */
static void
svcbuf_expire ( svcbuf_t *sb )
{
  svcbuf_block_t *b;
  svcbuf_block_t *nb;
  const time_t limit = timeshift_conf.unlimited_period ? 0 :
                       gclk() - (time_t)timeshift_conf.max_period * 60;
  const uint64_t pin = svcbuf_pinned(sb);
  uint64_t max_size = timeshift_conf.unlimited_size ? 0 : timeshift_conf.max_size;
  uint64_t used;

  if (sb->ram_only)
    max_size = timeshift_conf.ram_size;

  for (b = TAILQ_FIRST(&sb->blocks); b != NULL && b->settled && b->seq < pin;
       b = nb) {
    used = atomic_pre_add_u64(sb->ram_only ?
                                &timeshift_total_ram_size :
                                &timeshift_total_size, 0);

    if (!(limit && b->wall < limit) &&
        !(max_size && used > max_size))
      break;
    nb = TAILQ_NEXT(b, link);
    svcbuf_block_remove(sb, b);
  }
}

static void *
svcbuf_writer ( void *aux )
{
  svcbuf_t *sb = aux;
  svcbuf_block_t *b;
  svcbuf_seg_t *seg;
  const uint8_t *data;
  size_t size;
  off_t off;
  int r;

  tvh_mutex_lock(&sb->lock);
  while (sb->run) {
    while (sb->run && sb->wr_next != NULL && sb->wr_next->sealed) {
      b = sb->wr_next;
      if (!sb->ram_only && !sb->wr_error) {
        seg = sb->wr_seg;
        if (seg == NULL || seg->size + (off_t)b->size > SVCBUF_SEG_SIZE) {
          if (seg && seg->nblocks == 0)
            svcbuf_seg_release(sb, seg);
          seg = sb->wr_seg = svcbuf_seg_open(sb);
        }
        if (seg) {
          data = b->data;
          size = b->size;
          off  = seg->size;
          seg->size += size;  /* reserved: nobody else writes this segment */
          tvh_mutex_unlock(&sb->lock);
          r = svcbuf_write_all(seg->fd, data, size);
          tvh_mutex_lock(&sb->lock);
          if (r == 0) {
            b->seg  = seg;
            b->off  = off;
            seg->nblocks++;

            atomic_dec_u64(&timeshift_total_ram_size, b->size);

            free(b->data);
            b->data = NULL;
          } else {
            tvherror(LS_TIMESHIFT, "svcbuf: write to '%s' failed: %s, "
                     "keeping the cache in RAM", seg->path, strerror(errno));
            sb->wr_error = 1;
          }
        } else {
          sb->wr_error = 1;
        }
      }
      b->settled = 1;
      sb->wr_next = TAILQ_NEXT(b, link);
    }
    svcbuf_expire(sb);
    tvh_cond_timedwait(&sb->cond, &sb->lock, mclk() + sec2mono(1));
  }
  tvh_mutex_unlock(&sb->lock);
  return NULL;
}

/* **************************************************************************
 * Input from the service pad (s_stream_mutex held)
 * *************************************************************************/

static svcbuf_block_t *
svcbuf_block_new ( svcbuf_t *sb, size_t size )
{
  svcbuf_block_t *b = calloc(1, sizeof(*b));

  b->data      = malloc(size);
  b->alloc     = size;
  b->seq       = sb->next_seq++;
  b->start_seq = sb->start_seq;
  b->start     = sb->start;
  if (b->start)
    streaming_start_ref(b->start);
  b->wall      = gclk();
  b->mono      = mclk();

  TAILQ_INSERT_TAIL(&sb->blocks, b, link);
  if (sb->wr_next == NULL)
    sb->wr_next = b;
  return sb->cur = b;
}

static void
svcbuf_input ( void *opaque, streaming_message_t *sm )
{
  svcbuf_t *sb = opaque;
  svcbuf_block_t *b;
  streaming_start_t *ss, *old = NULL;
  pktbuf_t *pb;
  size_t len;

  if (sm->sm_type == SMT_START) {
    /*
     * START precedes MPEG-TS using the new stream layout.  Seal the
     * current block so a block never contains two different PID maps.
     */
    ss = sm->sm_data;
    if (ss)
      streaming_start_ref(ss);

    tvh_mutex_lock(&sb->lock);

    b = sb->cur;
    if (b) {
      b->sealed = 1;
      sb->cur = NULL;
      tvh_cond_signal(&sb->cond, 0);
    }

    old = sb->start;
    sb->start = ss;
    sb->start_seq++;

    tvh_mutex_unlock(&sb->lock);

    if (old)
      streaming_start_unref(old);

  } else if (sm->sm_type == SMT_MPEGTS) {
    pb  = sm->sm_data;
    len = pktbuf_len(pb);

    if (len > 0) {
      tvh_mutex_lock(&sb->lock);

      b = sb->cur;
      if (b && (b->size + len > b->alloc ||
                mclk() - b->mono >= SVCBUF_BLOCK_AGE)) {
        b->sealed = 1;
        sb->cur = b = NULL;
        tvh_cond_signal(&sb->cond, 0);
      }

      if (b == NULL)
        b = svcbuf_block_new(sb, MAX(len, SVCBUF_BLOCK_SIZE));

      memcpy(b->data + b->size, pktbuf_ptr(pb), len);
      b->size += len;
      sb->size += len;

      atomic_add_u64(&timeshift_total_size, len);
      atomic_add_u64(&timeshift_total_ram_size, len);

      tvh_mutex_unlock(&sb->lock);
    }
  }

  streaming_msg_free(sm);
}

static htsmsg_t *
svcbuf_input_info ( void *opaque, htsmsg_t *list )
{
  htsmsg_add_str(list, NULL, "channel cache input");
  return list;
}

static streaming_ops_t svcbuf_input_ops = {
  .st_cb   = svcbuf_input,
  .st_info = svcbuf_input_info
};

/* **************************************************************************
 * Life cycle
 * *************************************************************************/

static void
svcbuf_unref ( svcbuf_t *sb )
{
  svcbuf_block_t *b;
  svcbuf_block_t *nb;
  svcbuf_seg_t *seg;
  svcbuf_seg_t *nseg;

  if (atomic_dec(&sb->refcount, 1) > 1)
    return;

  tvh_mutex_lock(&sb->lock);
  sb->run = 0;
  tvh_cond_signal(&sb->cond, 0);
  tvh_mutex_unlock(&sb->lock);
  pthread_join(sb->writer, NULL);

  sb->wr_seg = NULL;
  for (b = TAILQ_FIRST(&sb->blocks); b != NULL; b = nb) {
    nb = TAILQ_NEXT(b, link);
    svcbuf_block_remove(sb, b);
  }
  for (seg = TAILQ_FIRST(&sb->segs); seg != NULL; seg = nseg) {
    nseg = TAILQ_NEXT(seg, link);
    svcbuf_seg_close(sb, seg);
  }
  if (sb->start)
    streaming_start_unref(sb->start);

  if (sb->dir) {
    rmdir(sb->dir);
    free(sb->dir);
  }
  tvh_cond_destroy(&sb->cond);
  tvh_mutex_destroy(&sb->lock);
  free(sb);
}

/* Called when the service starts (s_stream_mutex held) */
void
svcbuf_service_start ( service_t *t )
{
  svcbuf_t *sb;

  /* channels only: not the raw mux services used for scanning and EPG;
   * and "RAM only" needs a RAM size to stay bounded */
  if (!timeshift_conf.enabled || !timeshift_conf.record_cache ||
      t->s_type != STYPE_STD || t->s_svcbuf != NULL ||
      (timeshift_conf.ram_only && timeshift_conf.ram_size == 0))
    return;

  sb = calloc(1, sizeof(*sb));
  sb->service  = t;
  sb->refcount = 1;
  sb->id       = svcbuf_index++;
  sb->ram_only = timeshift_conf.ram_only;
  sb->run      = 1;
  sb->start    = service_build_streaming_start(t);
  sb->start_seq = 1;
  tvh_mutex_init(&sb->lock, NULL);
  tvh_cond_init(&sb->cond, 1);
  TAILQ_INIT(&sb->blocks);
  TAILQ_INIT(&sb->segs);
  LIST_INIT(&sb->gates);
  LIST_INIT(&sb->readers);
  streaming_target_init(&sb->input, &svcbuf_input_ops, sb,
                        ~(SMT_TO_MASK(SMT_MPEGTS) |
                          SMT_TO_MASK(SMT_START)));
  tvh_thread_create(&sb->writer, NULL, svcbuf_writer, sb, "svcbuf-wr");

  t->s_svcbuf = sb;
  streaming_target_connect(&t->s_streaming_pad, &sb->input);
  tvhdebug(LS_TIMESHIFT, "svcbuf: caching %s", t->s_nicename);
}

/* Called when the service stops (s_stream_mutex held) */
svcbuf_t *
svcbuf_service_stop ( service_t *t )
{
  svcbuf_t *sb = t->s_svcbuf;

  if (sb == NULL)
    return NULL;
  streaming_target_disconnect(&t->s_streaming_pad, &sb->input);
  t->s_svcbuf = NULL;
  return sb;
}

/* Release what svcbuf_service_stop() returned (s_stream_mutex not held) */
void
svcbuf_release ( svcbuf_t *sb )
{
  if (sb)
    svcbuf_unref(sb);
}

/* **************************************************************************
 * Gate
 * *************************************************************************/

/* Change the gate state (s_stream_mutex held) */
static void
svcbuf_gate_set_state ( svcbuf_gate_t *g, int state )
{
  g->state = state;
  *g->replaying = state == GATE_REPLAY;
}

/* s_stream_mutex held; ownership of ss is transferred to the message. */
static void
svcbuf_gate_deliver_start ( svcbuf_gate_t *g, streaming_start_t *ss,
                            uint64_t seq, int reconfigured, int replay )
{
  streaming_start_t *out;

  if (ss == NULL) {
    g->start_seq = seq;
    return;
  }

  /*
   * Cached maps are shared by blocks/readers and therefore immutable.
   * Add replay context only to the START sent down this consumer's chain.
   */
  if (replay) {
    out = streaming_start_copy(ss);
    out->ss_flags |= STREAMING_START_CACHE_REPLAY;
    streaming_start_unref(ss);
    ss = out;
  }

  if (reconfigured)
    streaming_target_deliver2
      (g->output,
       streaming_msg_create_code(SMT_STOP, SM_CODE_SOURCE_RECONFIGURED));

  streaming_target_deliver2
    (g->output, streaming_msg_create_data(SMT_START, ss));

  g->start_seq = seq;
}


/* s_stream_mutex held */
static void
svcbuf_gate_sync_live_start ( svcbuf_gate_t *g )
{
  svcbuf_t *sb = g->sb;
  streaming_start_t *ss = NULL;
  uint64_t seq;

  tvh_mutex_lock(&sb->lock);

  seq = sb->start_seq;
  if (seq != g->start_seq && sb->start) {
    ss = sb->start;
    streaming_start_ref(ss);
  }

  tvh_mutex_unlock(&sb->lock);

  if (seq != g->start_seq)
    svcbuf_gate_deliver_start(g, ss, seq, 1, 0);
}

/* Finish a bounded historical replay.  s_stream_mutex held. */
static void
svcbuf_gate_finish_history ( svcbuf_gate_t *g, int code )
{
  if (g->state != GATE_REPLAY && g->state != GATE_WAIT)
    return;

  svcbuf_gate_set_state(g, GATE_DONE);
  streaming_target_deliver2
    (g->output, streaming_msg_create_code(SMT_STOP, code));
}

static void *
svcbuf_gate_thread ( void *aux )
{
  svcbuf_gate_t *g = aux;
  svcbuf_t *sb = g->sb;
  service_t *t = sb->service;
  svcbuf_block_t *b, *nb;
  streaming_start_t *ss;
  pktbuf_t *pb;
  uint64_t bytes = 0;
  uint64_t start_seq;
  size_t n;
  off_t off;
  int fd, r, live = 0;

  tvh_mutex_lock(&sb->lock);

  while (g->state == GATE_REPLAY) {
    if (g->sq) {
      tvh_mutex_lock(&g->sq->sq_mutex);
      n = g->sq->sq_size;
      tvh_mutex_unlock(&g->sq->sq_mutex);

      if (n > SVCBUF_QUEUE_MAX) {
        tvh_mutex_unlock(&sb->lock);
        tvh_safe_usleep(20000);
        tvh_mutex_lock(&sb->lock);
        continue;
      }
    }

    b = g->blk;

    /*
     * A bounded retrospective recording never reaches live.  Blocks are
     * sealed roughly once a second, so stopping at the first block whose
     * wall-clock start is >= 'to' gives the same practical granularity as
     * the cache itself and never records the following programme wholesale.
     */
    if (g->to && b->wall >= g->to) {
      tvh_mutex_unlock(&sb->lock);
      tvh_mutex_lock(&t->s_stream_mutex);

      if (g->state == GATE_REPLAY)
        svcbuf_gate_finish_history(g, SM_CODE_OK);

      tvh_mutex_unlock(&t->s_stream_mutex);
      tvh_mutex_lock(&sb->lock);
      break;
    }

    /*
     * The next cached block belongs to a new service stream map.
     * Reconfigure downstream before sending any MPEG-TS from that map.
     */
    if (b->start_seq != g->start_seq) {
      ss = b->start;
      start_seq = b->start_seq;

      if (ss)
        streaming_start_ref(ss);

      tvh_mutex_unlock(&sb->lock);
      tvh_mutex_lock(&t->s_stream_mutex);

      if (g->state == GATE_REPLAY)
        svcbuf_gate_deliver_start(g, ss, start_seq, 1, 1);
      else if (ss)
        streaming_start_unref(ss);

      tvh_mutex_unlock(&t->s_stream_mutex);
      tvh_mutex_lock(&sb->lock);
      continue;
    }

    if (g->off < b->size) {
      n = MIN(b->size - g->off, SVCBUF_READ_SIZE);
      pb = pktbuf_alloc(NULL, n);

      if (b->data) {
        memcpy(pktbuf_ptr(pb), b->data + g->off, n);
      } else {
        fd = b->seg->fd;
        off = b->off + g->off;

        tvh_mutex_unlock(&sb->lock);
        r = svcbuf_read_all(fd, pktbuf_ptr(pb), n, off);
        tvh_mutex_lock(&sb->lock);

        if (r) {
          tvherror(LS_TIMESHIFT,
                   "svcbuf: read failed: %s, joining live",
                   strerror(errno));
          pktbuf_ref_dec(pb);
          break;
        }
      }

      g->off += n;

      tvh_mutex_unlock(&sb->lock);
      tvh_mutex_lock(&t->s_stream_mutex);

      if (g->state == GATE_REPLAY) {
        streaming_target_deliver2
          (g->output, streaming_msg_create_data(SMT_MPEGTS, pb));

        bytes += n;

        if (atomic_set(&t->s_pending_restart, 0))
          service_restart_streams(t);
      } else {
        pktbuf_ref_dec(pb);
      }

      tvh_mutex_unlock(&t->s_stream_mutex);
      tvh_mutex_lock(&sb->lock);
      continue;
    }

    nb = TAILQ_NEXT(b, link);

    /*
     * A bounded retrospective recording stops at its historical DVR
     * end, not at the current cache head.  Blocks are wall-clock
     * stamped at their beginning; after the current block has been
     * delivered, do not enter a following block which starts at or
     * beyond the requested stop time.
     */
    if (nb && g->to && nb->wall >= g->to) {
      tvh_mutex_unlock(&sb->lock);
      tvh_mutex_lock(&t->s_stream_mutex);

      if (g->state == GATE_REPLAY) {
        tvhinfo(LS_TIMESHIFT,
                "svcbuf: %s: retrospective replay reached stop boundary "
                "%"PRItime_t,
                t->s_nicename, g->to);
        svcbuf_gate_finish_history(g, SM_CODE_OK);
      }

      tvh_mutex_unlock(&t->s_stream_mutex);
      tvh_mutex_lock(&sb->lock);
      break;
    }

    if (nb) {
      g->blk = nb;
      g->off = 0;
      continue;
    }

    /*
     * Caught up.  s_stream_mutex prevents new live data from being
     * appended while the replay/live handover is decided.
     */
    tvh_mutex_unlock(&sb->lock);
    tvh_mutex_lock(&t->s_stream_mutex);
    tvh_mutex_lock(&sb->lock);

    if (TAILQ_NEXT(g->blk, link) == NULL &&
        g->off == g->blk->size) {
      tvh_mutex_unlock(&sb->lock);

      if (g->state == GATE_REPLAY) {
        if (g->to && !(g->all && g->to > gclk())) {
          /*
           * A bounded historical recording is best-effort.  If the
           * requested tail has already disappeared (or was never cached),
           * finish successfully with the portion we did recover.
           */
          if (g->to > g->blk->wall + 2)
            tvhinfo(LS_TIMESHIFT,
                    "svcbuf: %s: retrospective replay reached cache end "
                    "before requested stop; keeping partial recording",
                    t->s_nicename);

          svcbuf_gate_finish_history(g, SM_CODE_OK);
        } else {
          svcbuf_gate_sync_live_start(g);
          svcbuf_gate_set_state(g, GATE_LIVE);
          live = 1;
        }
      }

      tvh_mutex_unlock(&t->s_stream_mutex);
      tvh_mutex_lock(&sb->lock);
      break;
    }

    tvh_mutex_unlock(&t->s_stream_mutex);
  }

  if (!live && g->state == GATE_REPLAY) {
    tvh_mutex_unlock(&sb->lock);
    tvh_mutex_lock(&t->s_stream_mutex);

    if (g->state == GATE_REPLAY) {
      if (g->to && !(g->all && g->to > gclk()))
        svcbuf_gate_finish_history(g, SM_CODE_NO_INPUT);
      else {
        svcbuf_gate_sync_live_start(g);
        svcbuf_gate_set_state(g, GATE_LIVE);
      }
    }

    tvh_mutex_unlock(&t->s_stream_mutex);
    tvh_mutex_lock(&sb->lock);
  }

  g->blk = NULL;
  tvh_mutex_unlock(&sb->lock);

  tvhinfo(LS_TIMESHIFT,
          "svcbuf: %s: replayed %"PRIu64" kB from the cache%s",
          t->s_nicename, bytes / 1024, live ? ", now live" : "");

  return NULL;
}

/* Position the replay at 'from' and start it (s_stream_mutex held) */
static int
svcbuf_gate_start ( svcbuf_gate_t *g )
{
  svcbuf_t *sb = g->sb;
  svcbuf_block_t *b, *first, *last, *pos = NULL;
  streaming_start_t *ss;
  uint64_t start_seq;

  /*
   * A bounded retrospective request must never turn into a live
   * recording merely because its old cache disappeared meanwhile.
   */
  if (sb == NULL) {
    svcbuf_gate_finish_history(g, SM_CODE_NO_INPUT);
    return 1;
  }

  tvh_mutex_lock(&sb->lock);

  first = TAILQ_FIRST(&sb->blocks);
  last  = TAILQ_LAST(&sb->blocks, svcbuf_block_queue);

  /*
   * Retrospective recordings are best-effort.  Missing pre-padding,
   * programme start, programme end or post-padding must not prevent us
   * from saving the part which is still present in the channel cache.
   *
   * Reject only when there is no useful overlap at all.  The two-second
   * tolerance matches the cache block sealing granularity.
   */
  if (g->to && !g->all &&
      (first == NULL || last == NULL ||
       first->wall >= g->to ||
       last->wall + 2 < g->from)) {
    tvh_mutex_unlock(&sb->lock);
    svcbuf_gate_finish_history(g, SM_CODE_NO_INPUT);
    return 1;
  }

  if (g->to && !g->all) {
    if (first->wall > g->from + 2)
      tvhinfo(LS_TIMESHIFT,
              "svcbuf: %s: retrospective recording starts %"PRItime_t
              "s late; older cache data is no longer available",
              sb->service->s_nicename, first->wall - g->from);

    if (last->wall + 2 < g->to)
      tvhinfo(LS_TIMESHIFT,
              "svcbuf: %s: retrospective recording tail is incomplete "
              "by about %"PRItime_t"s",
              sb->service->s_nicename, g->to - (last->wall + 2));
  }

  pos = first;

  if (!g->all) {
    TAILQ_FOREACH(b, &sb->blocks, link) {
      if (b->wall > g->from)
        break;
      pos = b;
    }
  }

  /*
   * Never replay raw MPEG-TS without the stream map that belongs to it.
   */
  if (pos == NULL || pos->start == NULL) {
    tvh_mutex_unlock(&sb->lock);

    if (g->to && !g->all) {
      svcbuf_gate_finish_history(g, SM_CODE_NO_INPUT);
      return 1;
    }

    svcbuf_gate_set_state(g, GATE_LIVE);
    return 0;
  }

  tvhinfo(LS_TIMESHIFT,
          "svcbuf: %s: recording from %"PRItime_t"s ago",
          sb->service->s_nicename, gclk() - pos->wall);

  ss = pos->start;
  streaming_start_ref(ss);
  start_seq = pos->start_seq;

  *g->replay_start = pos->wall;
  g->blk = pos;
  g->off = 0;

  svcbuf_gate_set_state(g, GATE_REPLAY);

  tvh_mutex_unlock(&sb->lock);

  /*
   * Replace the subscription's current START with the historical map
   * valid at the beginning of the backfill.
   */
  svcbuf_gate_deliver_start(g, ss, start_seq, 0, 1);

  tvh_thread_create(&g->thread, NULL,
                    svcbuf_gate_thread, g, "svcbuf-rd");
  g->thread_started = 1;

  return 1;
}

static void
svcbuf_gate_input ( void *opaque, streaming_message_t *sm )
{
  svcbuf_gate_t *g = opaque;

  switch (g->state) {
  case GATE_WAIT:
    if (sm->sm_type == SMT_MPEGTS) {
      streaming_msg_free(sm);
      return;
    }

    if (sm->sm_type == SMT_START) {
      /*
       * If cache replay starts, replace this current START with the
       * historical START belonging to the cached MPEG-TS.
       */
      if (svcbuf_gate_start(g)) {
        streaming_msg_free(sm);
        return;
      }
    }
    break;

  case GATE_REPLAY:
    if (sm->sm_type == SMT_MPEGTS ||
        sm->sm_type == SMT_START ||
        (sm->sm_type == SMT_STOP &&
         sm->sm_code == SM_CODE_SOURCE_RECONFIGURED)) {
      /*
       * Live data and live reconfiguration are already represented by
       * the cache timeline and must not overtake historical MPEG-TS.
       */
      streaming_msg_free(sm);
      return;
    }
    break;

  case GATE_DONE:
    /* Historical recording has ended.  Never leak present-day live data. */
    streaming_msg_free(sm);
    return;

  default:
    break;
  }

  streaming_target_deliver2(g->output, sm);
}

static htsmsg_t *
svcbuf_gate_info ( void *opaque, htsmsg_t *list )
{
  svcbuf_gate_t *g = opaque;
  streaming_target_t *st = g->output;
  htsmsg_add_str(list, NULL, "channel cache gate");
  return st->st_ops.st_info(st->st_opaque, list);
}

static streaming_ops_t svcbuf_gate_ops = {
  .st_cb   = svcbuf_gate_input,
  .st_info = svcbuf_gate_info
};

/*
 * Create a gate replaying the channel cache from 'from' before passing
 * the live data to 'output'; *replaying is set while it replays and
 * *replay_start to where the replayed data begins; the replay waits
 * whenever 'sq' (the consumer's queue, if any) is backed up.  NULL when
 * the service has no cache.  s_stream_mutex held.
 */
streaming_target_t *
svcbuf_gate_create ( service_t *t, time_t from, time_t to, int all,
                     streaming_target_t *output,
                     int *replaying, time_t *replay_start,
                     streaming_queue_t *sq )
{
  svcbuf_t *sb = t->s_svcbuf;
  svcbuf_gate_t *g;

  /*
   * Ordinary backfill keeps its old behaviour when there is no cache.
   * Bounded retrospective recording gets a gate even without a cache so
   * it can fail closed instead of accidentally recording current live TV.
   */
  if (sb == NULL && (to == 0 || all))
    return NULL;

  g = calloc(1, sizeof(*g));
  g->output = output;
  g->from   = from;
  g->to     = to;
  g->all    = all;
  g->state  = GATE_WAIT;
  g->replaying = replaying;
  g->replay_start = replay_start;
  g->sq     = sq;
  g->sb     = sb;

  if (sb) {
    atomic_add(&sb->refcount, 1);
    tvh_mutex_lock(&sb->lock);
    LIST_INSERT_HEAD(&sb->gates, g, link);
    tvh_mutex_unlock(&sb->lock);
  }

  streaming_target_init(&g->input, &svcbuf_gate_ops, g, 0);
  return &g->input;
}

/* Stop replaying, return the gate's output (s_stream_mutex held) */
streaming_target_t *
svcbuf_gate_stop ( streaming_target_t *pad )
{
  svcbuf_gate_t *g = (svcbuf_gate_t *)pad;
  svcbuf_gate_set_state(g, GATE_STOPPED);
  return g->output;
}

/* Free a stopped gate (s_stream_mutex not held) */
void
svcbuf_gate_destroy ( streaming_target_t *pad )
{
  svcbuf_gate_t *g = (svcbuf_gate_t *)pad;
  svcbuf_t *sb = g->sb;

  if (g->thread_started)
    pthread_join(g->thread, NULL);

  if (sb) {
    tvh_mutex_lock(&sb->lock);
    LIST_REMOVE(g, link);
    tvh_mutex_unlock(&sb->lock);
    svcbuf_unref(sb);
  }

  free(g);
}

/* **************************************************************************
 * Readers (timeshift clients)
 * *************************************************************************/

/* A reference to the channel cache, NULL when it has none
 * (s_stream_mutex held) */
svcbuf_t *
svcbuf_acquire ( service_t *t )
{
  svcbuf_t *sb = t->s_svcbuf;
  if (sb)
    atomic_add(&sb->refcount, 1);
  return sb;
}

service_t *
svcbuf_service ( svcbuf_t *sb )
{
  return sb->service;
}

/* Monotonic time of the oldest and newest data held, 0 when empty */
int
svcbuf_span ( svcbuf_t *sb, int64_t *oldest, int64_t *newest )
{
  const svcbuf_block_t *first;
  const svcbuf_block_t *last;
  int r = 0;

  tvh_mutex_lock(&sb->lock);
  first = TAILQ_FIRST(&sb->blocks);
  last  = TAILQ_LAST(&sb->blocks, svcbuf_block_queue);
  if (first) {
    *oldest = first->mono;
    *newest = last->mono;
    r = 1;
  }
  tvh_mutex_unlock(&sb->lock);
  return r;
}

svcbuf_reader_t *
svcbuf_reader_create ( svcbuf_t *sb )
{
  svcbuf_reader_t *r = calloc(1, sizeof(*r));
  r->sb = sb;
  atomic_add(&sb->refcount, 1);
  tvh_mutex_lock(&sb->lock);
  LIST_INSERT_HEAD(&sb->readers, r, link);
  tvh_mutex_unlock(&sb->lock);
  return r;
}

void
svcbuf_reader_destroy ( svcbuf_reader_t *r )
{
  svcbuf_t *sb = r->sb;
  tvh_mutex_lock(&sb->lock);
  LIST_REMOVE(r, link);
  tvh_mutex_unlock(&sb->lock);
  svcbuf_unref(sb);
  free(r);
}

/* Position at the block holding monotonic time 'mono', or the oldest one
 * when the cache does not reach back that far; returns the block's time */
int64_t
svcbuf_reader_seek ( svcbuf_reader_t *r, int64_t mono )
{
  svcbuf_t *sb = r->sb;
  svcbuf_block_t *b, *pos;
  int64_t ret = 0;

  tvh_mutex_lock(&sb->lock);
  pos = TAILQ_FIRST(&sb->blocks);
  TAILQ_FOREACH(b, &sb->blocks, link) {
    if (b->mono > mono)
      break;
    pos = b;
  }
  r->blk = pos;
  r->off = 0;
  r->lost = 0;
  if (pos)
    ret = pos->mono;
  tvh_mutex_unlock(&sb->lock);
  return ret;
}

/* Next chunk of whole TS packets together with the stream map that was
 * active for the block: 1 with *pb set, 0 at the head (nothing more yet,
 * or not positioned), -1 on a read error.  The caller owns the returned
 * streaming_start_t reference. */
int
svcbuf_reader_read_map ( svcbuf_reader_t *r, pktbuf_t **pb,
                         streaming_start_t **ss, uint64_t *start_seq,
                         int64_t *block_mono )
{
  svcbuf_t *sb = r->sb;
  svcbuf_block_t *b, *nb;
  svcbuf_seg_t *seg;
  size_t n;
  off_t off;
  int fd, e;

  *pb = NULL;
  if (ss)
    *ss = NULL;
  if (start_seq)
    *start_seq = 0;
  if (block_mono)
    *block_mono = 0;

  tvh_mutex_lock(&sb->lock);
  while ((b = r->blk) != NULL) {
    if (r->off < b->size) {
      r->skipped = 0;

      if (ss && b->start) {
        *ss = b->start;
        streaming_start_ref(*ss);
      }
      if (start_seq)
        *start_seq = b->start_seq;
      if (block_mono)
        *block_mono = b->mono;

      n = MIN(b->size - r->off, SVCBUF_READ_SIZE);
      *pb = pktbuf_alloc(NULL, n);

      if (b->data) {
        memcpy(pktbuf_ptr(*pb), b->data + r->off, n);
      } else {
        /* The block may expire while the disk read is in progress.
         * The START has already been referenced above and the segment
         * remains open until this read finishes. */
        seg = b->seg;
        seg->readers++;
        fd  = seg->fd;
        off = b->off + r->off;

        tvh_mutex_unlock(&sb->lock);
        e = svcbuf_read_all(fd, pktbuf_ptr(*pb), n, off);
        tvh_mutex_lock(&sb->lock);

        if (--seg->readers == 0 && seg->dead)
          svcbuf_seg_close(sb, seg);

        if (e) {
          tvherror(LS_TIMESHIFT, "svcbuf: read failed: %s",
                   strerror(errno));
          pktbuf_ref_dec(*pb);
          *pb = NULL;
          if (ss && *ss) {
            streaming_start_unref(*ss);
            *ss = NULL;
          }
          tvh_mutex_unlock(&sb->lock);
          return -1;
        }
      }

      if (!r->skipped)
        r->off += n;

      tvh_mutex_unlock(&sb->lock);
      return 1;
    }

    if ((nb = TAILQ_NEXT(b, link)) == NULL)
      break;

    r->blk = nb;
    r->off = 0;
  }

  tvh_mutex_unlock(&sb->lock);
  return 0;
}

/* Compatibility wrapper for callers which only need MPEG-TS. */
int
svcbuf_reader_read ( svcbuf_reader_t *r, pktbuf_t **pb )
{
  streaming_start_t *ss = NULL;
  int ret;

  ret = svcbuf_reader_read_map(r, pb, &ss, NULL, NULL);
  if (ss)
    streaming_start_unref(ss);
  return ret;
}

/* Whether data was lost under the reader -- its position expired -- since
 * the last call: what it reads next does not follow on */
int
svcbuf_reader_lost ( svcbuf_reader_t *r )
{
  svcbuf_t *sb = r->sb;
  int lost;

  tvh_mutex_lock(&sb->lock);
  lost = r->lost;
  r->lost = 0;
  tvh_mutex_unlock(&sb->lock);
  return lost;
}
