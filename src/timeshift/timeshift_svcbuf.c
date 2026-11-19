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
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#define SVCBUF_BLOCK_SIZE (256 * 1024)        ///< Block allocation
#define SVCBUF_BLOCK_AGE  sec2mono(1)         ///< Seal a block after this
#define SVCBUF_SEG_SIZE   (64 * 1024 * 1024)  ///< Segment file size
#define SVCBUF_WR_RETRY   30                  ///< s: before trying the disk again
#define SVCBUF_READ_SIZE  (188 * 348)         ///< Replay chunk, whole TS packets
#define SVCBUF_QUEUE_MAX  (32 * 1024 * 1024)  ///< Consumer backlog to wait on
#define SVCBUF_STAGING_DEFAULT (32 * 1024 * 1024) ///< Global disk write-behind RAM

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
  size_t        staging;  ///< Bytes charged to global disk staging budget
  int           keep_ram; ///< Retain payload in configured Timeshift RAM
  int           sealed;   ///< Complete, no more appends
  int           settled;  ///< Passed by the writer (on disk, or kept in RAM)
  svcbuf_seg_t *seg;
  off_t         off;      ///< Offset in seg
} svcbuf_block_t;

TAILQ_HEAD(svcbuf_block_queue, svcbuf_block);
TAILQ_HEAD(svcbuf_seg_queue, svcbuf_seg);

/*
 * Disk-backed cache blocks must live in RAM until their writer has
 * completed. Keep that transient write-behind memory separate from the
 * configured retained Timeshift RAM, but bound it globally across all
 * services so several caches can move to disk at the same time without
 * each acquiring an unbounded private backlog.
 */
static tvh_mutex_t svcbuf_staging_lock =
  TVH_THREAD_MUTEX_INITIALIZER;
static uint64_t svcbuf_staging_size;

/* Configured budget, or the default when unset */
static uint64_t
svcbuf_staging_max ( void )
{
  const uint64_t mb = timeshift_conf.cache_staging;

  return mb ? mb * (uint64_t)1048576 : SVCBUF_STAGING_DEFAULT;
}

static int
svcbuf_staging_reserve ( size_t size )
{
  const uint64_t max = svcbuf_staging_max();
  int ok = 0;

  if (size == 0)
    return 1;

  tvh_mutex_lock(&svcbuf_staging_lock);

  if ((uint64_t)size <= max &&
      svcbuf_staging_size <= max - (uint64_t)size) {
    svcbuf_staging_size += size;
    ok = 1;
  }

  tvh_mutex_unlock(&svcbuf_staging_lock);
  return ok;
}

static void
svcbuf_staging_release ( size_t size )
{
  if (size == 0)
    return;

  tvh_mutex_lock(&svcbuf_staging_lock);

  assert(svcbuf_staging_size >= (uint64_t)size);
  svcbuf_staging_size -= size;

  tvh_mutex_unlock(&svcbuf_staging_lock);
}

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
  volatile int         state;   ///< Owned by s_stream_mutex; the replay
                                ///< thread reads it under sb->lock as a
                                ///< hint and re-checks under that mutex
                                ///< before acting, so those reads go
                                ///< through atomic_get()
  int                 *replaying; ///< Mirrors state == GATE_REPLAY
  time_t              *replay_start; ///< Set to where the replay begins
  streaming_queue_t   *sq;      ///< Consumer queue to pace on, or NULL
  svcbuf_block_t      *blk;     ///< Replay position, pins it (sb->lock)
  size_t               off;
  int                  no_space;  ///< Hard cache limit requires replay stop
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
  uint64_t                  ram_used;   ///< Allocations of its keep_ram blocks
  int                       disk_blocks; ///< Blocks held on disk
  int                       wr_busy;    ///< Writing a block, the lock released
  /* the sharing of the budgets (svcbuf_share_lock): what it last said it
     holds of each, and when it was last short of it (mclk, 0: never) */
  LIST_ENTRY(svcbuf)        share_link;
  uint64_t                  share_held[2];
  uint64_t                  share_credit[2]; ///< given back beyond what was due
  int64_t                   share_short[2];
  char                     *dir;
  int                       seg_index;
  int                       ram_only;
  int                       wr_error;
  int64_t                   wr_retry;   ///< ... when to try it again, or 0
  int                       run;
  int                       gap;        ///< Cache input was discontinuous
  int                       limit_warned;
  uint64_t                  limit_dropped;
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
static int svcbuf_live;          ///< Caches allocated and not yet freed

/* The budgets the caches share evenly: the RAM under "RAM only", and the
 * size.  Room asked for by the caches under their share of each, and when
 * it was last asked for. */
enum { SVCBUF_RAM, SVCBUF_SIZE };
static tvh_mutex_t svcbuf_share_lock = TVH_THREAD_MUTEX_INITIALIZER;
static LIST_HEAD(, svcbuf) svcbuf_sharers;
static uint64_t svcbuf_wanted[2];
static int64_t svcbuf_wanted_at[2];
#define SVCBUF_HUNGER  sec2mono(3)    ///< How long a request, or being short, stands
#define SVCBUF_ASK     4              ///< Room asked for, per byte short
#define SVCBUF_ASK_MAX (4 * 1048576)  ///< Room asked for at most

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
  if (seg == NULL) {
    close(fd);                  /* the descriptor is ours already */
    unlink(path);
    return NULL;
  }
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

/*
 * A block's data lives in pages of its own, out of the heap.  Blocks are
 * brought down to what they hold once sealed (svcbuf_block_shrink), so
 * they end up of every size; in the heap the tails they give back would be
 * holes between blocks still in use, that small allocations settle in and
 * keep from merging -- fragmentation, and memory that does not go back to
 * the system.  Mapped, a tail is returned with munmap() and the block stays
 * where it is; freed, the whole goes back to the kernel.  Plain POSIX.
 */
static size_t
svcbuf_page_round ( size_t size )
{
  static size_t page;

  if (page == 0) {
    const long p = sysconf(_SC_PAGESIZE);
    page = p > 0 ? (size_t)p : 4096;
  }
  return (size + page - 1) & ~(page - 1);
}

static uint8_t *
svcbuf_data_alloc ( size_t size )
{
  void *p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  return p == MAP_FAILED ? NULL : p;
}

static void
svcbuf_data_free ( uint8_t *data, size_t size )
{
  if (data != NULL)
    munmap(data, size);
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
      r->lost = 1;
      r->skipped = 1;
    }
  TAILQ_REMOVE(&sb->blocks, b, link);
  sb->size -= b->size;

  timeshift_size_release(b->size);

  if (b->keep_ram) {
    timeshift_ram_release(b->alloc);
    sb->ram_used -= b->alloc;
  }

  if (b->staging) {
    svcbuf_staging_release(b->staging);
    b->staging = 0;
  }

  if (b->data)
    memoryinfo_free(&timeshift_memoryinfo_ram, b->alloc);
  if (sb->cur == b)
    sb->cur = NULL;
  if (sb->wr_next == b)
    sb->wr_next = next;
  if (b->seg) {
    sb->disk_blocks--;
    b->seg->nblocks--;
    if (b->seg->nblocks == 0 && b->seg != sb->wr_seg)
      svcbuf_seg_release(sb, b->seg);
  }
  if (b->start)
    streaming_start_unref(b->start);
  svcbuf_data_free(b->data, b->alloc);
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

/* Let go of a block's RAM copy: it is on disk as well.  sb->lock held. */
static void
svcbuf_copy_drop ( svcbuf_t *sb, svcbuf_block_t *b )
{
  timeshift_ram_release(b->alloc);
  sb->ram_used -= b->alloc;
  b->keep_ram = 0;
  memoryinfo_free(&timeshift_memoryinfo_ram, b->alloc);
  svcbuf_data_free(b->data, b->alloc);
  b->data = NULL;
}

static inline int
svcbuf_is_copy ( const svcbuf_block_t *b )
{
  return b->keep_ram && b->seg != NULL && b->data != NULL;
}

/* Whether a recording still replays from the cache: a gate that has
 * joined live needs it no more.  sb->lock held. */
static int
svcbuf_replaying ( svcbuf_t *sb )
{
  svcbuf_gate_t *g;

  LIST_FOREACH(g, &sb->gates, link)
    if (atomic_get(&g->state) == GATE_REPLAY ||
        atomic_get(&g->state) == GATE_WAIT)
      return 1;
  return 0;
}

/* A budget's total: the RAM, or the size -- the RAM size under "RAM only"
 * -- and 0 when the size is unlimited */
static uint64_t
svcbuf_total ( int budget )
{
  if (budget == SVCBUF_RAM || timeshift_conf.ram_only)
    return timeshift_conf.ram_size;
  return timeshift_conf.unlimited_size ? 0 : timeshift_conf.max_size;
}

/* What is free of a budget */
static uint64_t
svcbuf_free ( int budget )
{
  const uint64_t total = svcbuf_total(budget);
  const uint64_t used = (budget == SVCBUF_RAM || timeshift_conf.ram_only)
                          ? timeshift_ram_used() : timeshift_size_used();
  return total > used ? total - used : 0;
}

static uint64_t
svcbuf_held ( const svcbuf_t *sb, int budget )
{
  return budget == SVCBUF_RAM ? sb->ram_used : sb->size;
}

/* Whether a cache was short of a budget lately.  svcbuf_share_lock held. */
static int
svcbuf_is_short ( const svcbuf_t *sb, int budget, int64_t now )
{
  return sb->share_short[budget] && now - sb->share_short[budget] < SVCBUF_HUNGER;
}

/*
 * Each cache's share of a budget, max-min fair: a cache that is not short
 * of the budget counts for what it holds, and what is left goes evenly to
 * those that are -- so that a slow channel's cache, needing less than an
 * even split, leaves the rest to the others, and they give it back as it
 * grows.  `sb` says what it holds on the way.  0 when the size is
 * unlimited.  sb->lock held.
 */
static uint64_t
svcbuf_share ( svcbuf_t *sb, int budget )
{
  const uint64_t total = svcbuf_total(budget);
  const int64_t now = mclk();
  uint64_t level;
  uint64_t sum;
  uint64_t next;
  svcbuf_t *s;
  int n = 0;
  int m;
  int i;
  int done;

  if (total == 0)
    return 0;
  tvh_mutex_lock(&svcbuf_share_lock);
  sb->share_held[budget] = svcbuf_held(sb, budget);
  LIST_FOREACH(s, &svcbuf_sharers, share_link)
    n++;
  level = total / MAX(1, n);
  for (i = 0; i < n; i++) {
    sum = 0;
    m = 0;
    LIST_FOREACH(s, &svcbuf_sharers, share_link)
      if (!svcbuf_is_short(s, budget, now) && s->share_held[budget] < level) {
        sum += s->share_held[budget];
        m++;
      }
    /* nobody short: no cache holds another back */
    next = m >= n ? total : (total - MIN(sum, total)) / (uint64_t)(n - m);
    done = m >= n || next == level;
    level = next;
    if (done)
      break;
  }
  tvh_mutex_unlock(&svcbuf_share_lock);
  return level;
}

/* A cache could not get room of a budget, and would hold more */
static void
svcbuf_short ( svcbuf_t *sb, int budget )
{
  tvh_mutex_lock(&svcbuf_share_lock);
  sb->share_short[budget] = mclk();
  tvh_mutex_unlock(&svcbuf_share_lock);
}

/* A cache under its share of a budget is short of `n` bytes of it.  It
 * asks for a block's worth at least, as an append alone is too little for
 * the room to keep up with it. */
static void
svcbuf_ask ( int budget, size_t n )
{
  n = MAX(n, SVCBUF_BLOCK_SIZE);
  tvh_mutex_lock(&svcbuf_share_lock);
  svcbuf_wanted[budget] = MIN(svcbuf_wanted[budget] + SVCBUF_ASK * (uint64_t)n,
                              (uint64_t)SVCBUF_ASK_MAX);
  svcbuf_wanted_at[budget] = mclk();
  tvh_mutex_unlock(&svcbuf_share_lock);
}

/* Room asked for of a budget and not had yet.  A request nobody has
 * renewed for a while lapses. */
static uint64_t
svcbuf_wanted_now ( int budget )
{
  uint64_t r;

  tvh_mutex_lock(&svcbuf_share_lock);
  if (mclk() - svcbuf_wanted_at[budget] >= SVCBUF_HUNGER)
    svcbuf_wanted[budget] = 0;
  r = svcbuf_wanted[budget];
  tvh_mutex_unlock(&svcbuf_share_lock);
  return r;
}

/* A cache under its share got `n` bytes of a budget: so much of the
 * request is met.  Only then -- the room given back meanwhile stays for
 * the caches that asked, the others over their share keeping out of it. */
static void
svcbuf_served ( int budget, uint64_t n )
{
  tvh_mutex_lock(&svcbuf_share_lock);
  svcbuf_wanted[budget] -= MIN(svcbuf_wanted[budget], n);
  tvh_mutex_unlock(&svcbuf_share_lock);
}

/*
 * The caches share each budget evenly.  A cache only ever evicts its own
 * blocks, so one under its share that is short of a budget asks for room,
 * and one over its share gives back what is asked, from its oldest blocks
 * the gates have passed and no further than its share, and meanwhile frees
 * what it appends rather than grow.  The room moves as fast as the cache
 * that asks fills it: one that is only passing through takes little.  No
 * cache takes another's lock.  `need` is what the caller is about to
 * reserve of the budget.  sb->lock held.
 */
static void
svcbuf_yield_budget ( svcbuf_t *sb, int budget, size_t need )
{
  const uint64_t share = svcbuf_share(sb, budget);
  const uint64_t held = svcbuf_held(sb, budget);
  const int copies = budget == SVCBUF_RAM && !sb->ram_only;
  uint64_t wanted;
  uint64_t room;
  uint64_t excess;
  uint64_t keep;
  uint64_t target;
  uint64_t pin;
  svcbuf_block_t *b;
  svcbuf_block_t *nb;

  if (share == 0 || held + need <= share) {
    sb->share_credit[budget] = 0;
    return;
  }
  wanted = svcbuf_wanted_now(budget);
  if (wanted == 0)
    return;
  /* what is asked and not free yet: what is free already stays for the
     caches that asked */
  room = svcbuf_free(budget);
  excess = held + need - share;
  keep = MIN(need, excess);
  target = keep + (wanted > room ? MIN(wanted - room, excess - keep) : 0);
  /* blocks go whole, a quarter of a MiB for an append of a few kB: what one
     gave beyond what was due counts towards the next calls, else a cache
     over its share gave back many times what was asked and what it took */
  if (sb->share_credit[budget] >= target) {
    sb->share_credit[budget] -= target;
    return;
  }
  target -= sb->share_credit[budget];
  sb->share_credit[budget] = 0;
  pin = svcbuf_pinned(sb);
  b = TAILQ_FIRST(&sb->blocks);
  while (b != NULL && held - svcbuf_held(sb, budget) < target) {
    nb = TAILQ_NEXT(b, link);
    if (copies) {
      /* with a disk, the RAM holds copies: giving one up loses nothing */
      if (svcbuf_is_copy(b))
        svcbuf_copy_drop(sb, b);
    } else if (b->settled && b->seq < pin) {
      svcbuf_block_remove(sb, b);
    } else {
      break;
    }
    b = nb;
  }
  if (held - svcbuf_held(sb, budget) > target)
    sb->share_credit[budget] = held - svcbuf_held(sb, budget) - target;
}

/* `alloc` and `len`: what the caller is about to reserve of the RAM and of
 * the size.  sb->lock held. */
static void
svcbuf_yield ( svcbuf_t *sb, size_t alloc, size_t len )
{
  svcbuf_yield_budget(sb, SVCBUF_RAM, sb->ram_only ? alloc : 0);
  svcbuf_yield_budget(sb, SVCBUF_SIZE, len);
}

/*
 * Make room for another cache payload.
 *
 * svcbuf_size_lock and sb->lock are held by the caller.  Returning zero
 * means that satisfying the hard configured limit would require removing
 * data which is not yet settled or which is pinned by an active DVR replay.
 */
static int
svcbuf_make_room_locked ( svcbuf_t *sb, size_t reserve )
{
  svcbuf_block_t *b;
  const uint64_t pin = svcbuf_pinned(sb);
  const uint64_t share = svcbuf_share(sb, SVCBUF_SIZE);
  const int over = share && sb->size + reserve > share;

  /*
   * Reserve the global Classic + shared Timeshift budget atomically.
   * If it is full, roll this service cache from its chronological head
   * and retry. The successful reservation remains charged until the
   * retained bytes are removed again by svcbuf_block_remove().  Over its
   * share while others wait, it stays out of the free room, which is
   * theirs, and makes its own.
   */
  if (over && svcbuf_wanted_now(SVCBUF_SIZE)) {
    svcbuf_yield_budget(sb, SVCBUF_SIZE, reserve);
    svcbuf_short(sb, SVCBUF_SIZE);
    if (timeshift_size_reserve(reserve))
      return 1;
  } else if (timeshift_size_reserve(reserve)) {
    if (!over)
      svcbuf_served(SVCBUF_SIZE, reserve);
    return 1;
  } else {
    svcbuf_short(sb, SVCBUF_SIZE);
    /* short of room within its share: ask the caches over theirs */
    if (share && !over)
      svcbuf_ask(SVCBUF_SIZE, reserve);
  }

  for (;;) {
    /*
     * Cache history is chronological. Only evict from its head; never
     * punch a silent hole in the middle of a service cache.
     *
     * An unsettled block cannot be discarded yet. A DVR backfill gate
     * may also pin the oldest block until its replay advances.
     */
    b = TAILQ_FIRST(&sb->blocks);

    if (b == NULL ||
        !b->settled ||
        b->seq >= pin)
      return 0;

    svcbuf_block_remove(sb, b);

    if (timeshift_size_reserve(reserve))
      return 1;
  }
}


/*
 * A DVR cache gate pins historical blocks.  If the configured hard size
 * limit cannot be satisfied without deleting those blocks, terminate the
 * affected replay explicitly.  Its worker will emit SM_CODE_NO_SPACE
 * rather than producing a recording containing a silent cache gap.
 *
 * Called from svcbuf_input() while s_stream_mutex and sb->lock are held.
 */
static int
svcbuf_mark_replays_no_space_locked ( svcbuf_t *sb )
{
  svcbuf_gate_t *g;
  int n = 0;

  LIST_FOREACH(g, &sb->gates, link) {
    if (atomic_get(&g->state) == GATE_REPLAY && g->blk != NULL) {
      g->no_space = 1;
      n++;
    }
  }

  return n;
}



/*
 * A dropped cache payload creates a timeline discontinuity.  Never append
 * later data to the old retained history and pretend that it is contiguous.
 */
static void
svcbuf_enter_gap_locked ( svcbuf_t *sb, size_t dropped, const char *reason )
{
  svcbuf_block_t *b;
  int stopped;

  b = sb->cur;
  if (b) {
    b->sealed = 1;
    sb->cur = NULL;
  }

  sb->gap = 1;
  sb->limit_dropped += dropped;
  stopped = svcbuf_mark_replays_no_space_locked(sb);

  if (!sb->limit_warned) {
    tvhwarn(LS_TIMESHIFT,
            "svcbuf: %s: cache retention interrupted (%s); "
            "dropping new cache data%s",
            sb->service->s_nicename, reason,
            stopped ? " and stopping active cache replay" : "");
    sb->limit_warned = 1;
  }

  tvh_cond_signal(&sb->cond, 0);
}


/*
 * Resume only with a fresh contiguous history.  Wait until the writer has
 * settled everything and no DVR replay still pins the old timeline, then
 * discard that old timeline completely.  svcbuf readers are moved forward
 * by svcbuf_block_remove() and observe their existing 'lost' indication.
 */
static int
svcbuf_reset_gap_locked ( svcbuf_t *sb )
{
  svcbuf_block_t *b;
  svcbuf_seg_t *seg;

  if (!sb->gap)
    return 1;

  /* not while the writer is busy, sequentially or catching up: the block
     it writes, and its segment, must outlive the write */
  if (sb->wr_next != NULL || sb->wr_busy ||
      svcbuf_pinned(sb) != UINT64_MAX)
    return 0;

  while ((b = TAILQ_FIRST(&sb->blocks)) != NULL)
    svcbuf_block_remove(sb, b);

  seg = sb->wr_seg;
  if (seg && seg->nblocks == 0) {
    sb->wr_seg = NULL;
    svcbuf_seg_release(sb, seg);
  }

  sb->gap = 0;
  return 1;
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
    /*
     * Compare each budget with its own total: under "RAM only" max_size is
     * the RAM size, and timeshift_size_used() counts the bytes staged to
     * disk, which is zero there -- so the size arm never fired and only the
     * period bounded the cache.
     */
    used = sb->ram_only ? timeshift_ram_used() : timeshift_size_used();

    if (!(limit && b->wall < limit) &&
        !(max_size && used > max_size))
      break;
    nb = TAILQ_NEXT(b, link);
    svcbuf_block_remove(sb, b);
  }
}

/*
 * Make room in the retained RAM budget for one block.  The writer thread
 * expires when the cache is already over its budget, while a reservation
 * fails when it would take it over: between the two lies a window where
 * nothing is evicted, and the append would declare a gap -- which drops the
 * whole history -- rather than give up the oldest block.  The RAM copies go
 * first, their blocks staying on disk; then settled blocks the gates have
 * passed, from the oldest only: one taken from behind a block on disk alone
 * -- the cache just turned to "RAM only", or its disk failing -- would leave
 * a hole that no gap marks.  sb->lock held.
 */
static int
svcbuf_ram_make_room ( svcbuf_t *sb, size_t alloc )
{
  const uint64_t pin = svcbuf_pinned(sb);
  svcbuf_block_t *b;
  int ram;

  /* a dropped copy leaves its block in the list */
  TAILQ_FOREACH(b, &sb->blocks, link) {
    if (svcbuf_is_copy(b)) {
      svcbuf_copy_drop(sb, b);
      if (timeshift_ram_reserve(alloc))
        return 1;
    }
  }

  while ((b = TAILQ_FIRST(&sb->blocks)) != NULL && b->settled && b->seq < pin) {
    ram = b->keep_ram;
    svcbuf_block_remove(sb, b);
    if (ram && timeshift_ram_reserve(alloc))
      return 1;
  }

  return 0;
}

/*
 * Room in the RAM budget for the copy of a block just written, giving up
 * this cache's oldest copies if need be; a cache under its share of the
 * RAM asks the others for some.  One over its share while a request is
 * open stays out of the free room, which is for the caches that asked:
 * it gives back from its oldest copies what is asked and not free yet,
 * and makes this copy's room from them too.  Else a fast cache, which
 * reserves a copy ten times as often as a slow one, takes what is given
 * back first, and the slow one never gets its share.  sb->lock held.
 */
static int
svcbuf_copy_reserve ( svcbuf_t *sb, const svcbuf_block_t *keep )
{
  const uint64_t share = svcbuf_share(sb, SVCBUF_RAM);
  const int over = share && sb->ram_used + keep->alloc > share;
  svcbuf_block_t *b;

  if (timeshift_conf.ram_size == 0)
    return 0;
  if (over && svcbuf_wanted_now(SVCBUF_RAM)) {
    svcbuf_yield_budget(sb, SVCBUF_RAM, keep->alloc);
    svcbuf_short(sb, SVCBUF_RAM);
    if (timeshift_ram_reserve(keep->alloc))
      return 1;
  } else if (timeshift_ram_reserve(keep->alloc)) {
    if (!over)
      svcbuf_served(SVCBUF_RAM, keep->alloc);
    return 1;
  } else {
    svcbuf_short(sb, SVCBUF_RAM);
    if (!over)
      svcbuf_ask(SVCBUF_RAM, keep->alloc);
  }
  TAILQ_FOREACH(b, &sb->blocks, link) {
    if (b == keep)
      break;
    if (svcbuf_is_copy(b)) {
      svcbuf_copy_drop(sb, b);
      if (timeshift_ram_reserve(keep->alloc))
        return 1;
    }
  }
  return 0;
}

/*
 * A write to the disk failed -- the volume full, say: new data stays off the
 * disk for SVCBUF_WR_RETRY seconds (wr_error), in RAM when it has room.  The
 * segment it went to takes no more: a partial write left its file position
 * past what its blocks account for, so the next write opens another.
 * sb->lock held.
 */
static void
svcbuf_writer_failed ( svcbuf_t *sb, svcbuf_seg_t *seg, off_t off )
{
  if (seg != NULL) {
    seg->size = off;
    if (seg == sb->wr_seg) {
      if (seg->nblocks == 0)
        svcbuf_seg_release(sb, seg);
      sb->wr_seg = NULL;
    }
  }
  sb->wr_error = 1;
  sb->wr_retry = mclk() + sec2mono(SVCBUF_WR_RETRY);
}

/*
 * Time to try the disk again: what is appended from now goes to it, to a
 * new segment, and what was kept in RAM meanwhile follows as the "RAM only"
 * catch-up writes it (svcbuf_follow_catch_up).  sb->lock held.
 */
static void
svcbuf_writer_retry ( svcbuf_t *sb )
{
  if (sb->wr_error && mclk() >= sb->wr_retry) {
    tvhdebug(LS_TIMESHIFT, "svcbuf: %s: trying the disk again",
             sb->service->s_nicename);
    sb->wr_error = 0;
  }
}

/*
 * Write a sealed block to the current segment, opening a new one when it
 * is full, and let go of its RAM.  A failure keeps new data off the disk
 * for a while (svcbuf_writer_failed).  sb->lock held, released for the
 * write itself.
 */
static void
svcbuf_writer_store ( svcbuf_t *sb, svcbuf_block_t *b )
{
  svcbuf_seg_t *seg = sb->wr_seg;
  const uint8_t *data;
  size_t size;
  off_t off;
  int r;

  if (seg == NULL || seg->size + (off_t)b->size > SVCBUF_SEG_SIZE) {
    if (seg && seg->nblocks == 0)
      svcbuf_seg_release(sb, seg);
    sb->wr_seg = svcbuf_seg_open(sb);
    seg = sb->wr_seg;
  }
  if (seg == NULL) {
    svcbuf_writer_failed(sb, NULL, 0);
    return;
  }

  data = b->data;
  size = b->size;
  off  = seg->size;
  seg->size += size;  /* reserved: nobody else writes this segment */
  sb->wr_busy = 1;
  tvh_mutex_unlock(&sb->lock);
  r = svcbuf_write_all(seg->fd, data, size);
  tvh_mutex_lock(&sb->lock);
  sb->wr_busy = 0;
  if (r != 0) {
    tvherror(LS_TIMESHIFT, "svcbuf: write to '%s' failed: %s; "
             "new data stays off the disk for %d s",
             seg->path, strerror(errno), SVCBUF_WR_RETRY);
    svcbuf_writer_failed(sb, seg, off);
    return;
  }
  if (sb->wr_retry) {
    tvhinfo(LS_TIMESHIFT, "svcbuf: %s: writing to disk again",
            sb->service->s_nicename);
    sb->wr_retry = 0;
  }

  b->seg  = seg;
  b->off  = off;
  seg->nblocks++;
  sb->disk_blocks++;

  if (b->staging) {
    svcbuf_staging_release(b->staging);
    b->staging = 0;
  }

  /* the newest data stays in RAM as well, for pausing and rewinding to
     read it from there; a block already in the RAM budget keeps its */
  if (b->keep_ram)
    return;
  if (!sb->ram_only && svcbuf_copy_reserve(sb, b)) {
    b->keep_ram = 1;
    sb->ram_used += b->alloc;
    return;
  }
  memoryinfo_free(&timeshift_memoryinfo_ram, b->alloc);
  svcbuf_data_free(b->data, b->alloc);
  b->data = NULL;
}

/* A block's disk copy is let go: under "RAM only" it lives in RAM alone.
 * sb->lock held. */
static void
svcbuf_block_detach ( svcbuf_t *sb, svcbuf_block_t *b )
{
  svcbuf_seg_t *seg = b->seg;

  b->seg = NULL;
  sb->disk_blocks--;
  seg->nblocks--;
  if (seg->nblocks == 0 && seg != sb->wr_seg)
    svcbuf_seg_release(sb, seg);
}

/*
 * Take up "RAM only" as the settings say, on the next append or writer
 * pass, so that no append decides under the mode just left.  The segment
 * being written is the writer's to let go of: it may be writing to it with
 * the lock released.  sb->lock held.
 */
static void
svcbuf_follow_mode ( svcbuf_t *sb )
{
  sb->ram_only = timeshift_conf.ram_only;
}

/*
 * Follow the settings as they change.  "RAM only" keeps nothing on disk:
 * what has no RAM copy goes, and what has one lets go of its disk copy.
 * Out of it, what lives in RAM alone is written to disk as well, a few
 * blocks at a time, each marked unsettled meanwhile so that nothing evicts
 * it.  A RAM budget lowered under what the caches keep lets go of the
 * oldest RAM copies, which are on disk already.  sb->lock held.
 */
/* Under "RAM only": nothing on disk.  sb->lock held. */
static void
svcbuf_follow_ram_only ( svcbuf_t *sb )
{
  const uint64_t pin = svcbuf_pinned(sb);
  svcbuf_block_t *b;

  if (sb->wr_seg) {
    if (sb->wr_seg->nblocks == 0)
      svcbuf_seg_release(sb, sb->wr_seg);
    sb->wr_seg = NULL;
  }
  b = TAILQ_FIRST(&sb->blocks);
  while (b != NULL && b->data == NULL && b->settled && b->seq < pin) {
    svcbuf_block_remove(sb, b);
    b = TAILQ_FIRST(&sb->blocks);
  }
  TAILQ_FOREACH(b, &sb->blocks, link)
    if (b->seg != NULL && b->data != NULL)
      svcbuf_block_detach(sb, b);
}

/* Out of "RAM only", or with the disk back after a failed write: what lives
 * in RAM alone -- kept there, or still waiting for the disk -- goes to disk
 * as well, a few blocks at a time.  sb->lock held. */
static void
svcbuf_follow_catch_up ( svcbuf_t *sb )
{
  svcbuf_block_t *b = TAILQ_FIRST(&sb->blocks);
  int moved = 0;

  while (!sb->wr_error && b != NULL && moved < 32) {
    if (!(b->seg == NULL && b->data != NULL && b->settled)) {
      b = TAILQ_NEXT(b, link);
      continue;
    }
    b->settled = 0;
    svcbuf_writer_store(sb, b);
    b->settled = 1;
    if (b->seg == NULL)
      break;                      /* the write failed: wr_error is set */
    moved++;
    b = TAILQ_FIRST(&sb->blocks); /* the list may have changed meanwhile */
  }
}

/* A RAM budget lowered under what the caches keep: the oldest RAM copies,
 * on disk already, go.  sb->lock held. */
static void
svcbuf_follow_ram_budget ( svcbuf_t *sb )
{
  svcbuf_block_t *b = TAILQ_FIRST(&sb->blocks);
  svcbuf_block_t *nb;

  while (b != NULL && timeshift_ram_used() > timeshift_conf.ram_size) {
    nb = TAILQ_NEXT(b, link);
    if (svcbuf_is_copy(b))
      svcbuf_copy_drop(sb, b);
    b = nb;
  }
}

/*
 * The shared cache turned off: what no recording replays any more goes,
 * so that the budgets are free again -- Classic timeshift's too, which
 * would otherwise find the size taken by a cache that takes nothing in,
 * and so never evicts anything.  sb->lock held.
 */
static void
svcbuf_follow_off ( svcbuf_t *sb )
{
  const uint64_t pin = svcbuf_pinned(sb);
  svcbuf_block_t *b;

  while ((b = TAILQ_FIRST(&sb->blocks)) != NULL && b->settled && b->seq < pin)
    svcbuf_block_remove(sb, b);
}

static void
svcbuf_follow_settings ( svcbuf_t *sb )
{
  svcbuf_follow_mode(sb);
  if ((!timeshift_conf.enabled || !timeshift_conf.record_cache) &&
      !svcbuf_replaying(sb)) {
    svcbuf_follow_off(sb);
    return;
  }
  if (sb->ram_only) {
    svcbuf_follow_ram_only(sb);
    return;
  }
  svcbuf_follow_catch_up(sb);
  svcbuf_follow_ram_budget(sb);
}

/*
 * A sealed block's RAM down to what it holds.  A block is sealed when full
 * or a second old, so a slow channel's -- standard definition, radio --
 * holds a fraction of its allocation, and the budgets charge the block's
 * pages: such a channel's share of the RAM held its blocks' slack as well
 * as its data.  The pages past the data are unmapped -- the block stays
 * where it is -- and the RAM budget and the write-behind buffer get them
 * back.  sb->lock held; the block takes no more appends.
 */
static void
svcbuf_block_shrink ( svcbuf_t *sb, svcbuf_block_t *b )
{
  const size_t want = svcbuf_page_round(b->size);
  size_t back;

  if (b->data == NULL || want == 0 || want >= b->alloc)
    return;
  if (munmap(b->data + want, b->alloc - want))
    return;                       /* the block keeps its pages, and its charge */
  back = b->alloc - want;
  memoryinfo_free(&timeshift_memoryinfo_ram, b->alloc);
  memoryinfo_alloc(&timeshift_memoryinfo_ram, want);
  if (b->keep_ram) {
    timeshift_ram_release(back);
    sb->ram_used -= back;
  }
  if (b->staging > want) {
    svcbuf_staging_release(b->staging - want);
    b->staging = want;
  }
  b->alloc = want;
}

/*
 * A sealed block's turn at the writer, its RAM brought down to its data
 * first.  One kept in RAM alone has nothing
 * to write: settling merely makes it eligible for normal period/size
 * expiration.  Under "RAM only", one that was waiting for the disk is
 * taken into the RAM budget when there is room, and otherwise stays where
 * it is.  sb->lock held.
 */
static void
svcbuf_writer_settle ( svcbuf_t *sb, svcbuf_block_t *b )
{
  svcbuf_block_shrink(sb, b);
  if (!b->keep_ram && !sb->ram_only && !sb->wr_error) {
    svcbuf_writer_store(sb, b);
  } else if (!b->keep_ram && sb->ram_only &&
             timeshift_ram_reserve(b->alloc)) {
    b->keep_ram = 1;
    sb->ram_used += b->alloc;
    svcbuf_staging_release(b->staging);
    b->staging = 0;
  }
  b->settled = 1;
}

static void *
svcbuf_writer ( void *aux )
{
  svcbuf_t *sb = aux;
  svcbuf_block_t *b;

  tvh_mutex_lock(&sb->lock);
  while (sb->run) {
    svcbuf_writer_retry(sb);
    while (sb->run && sb->wr_next != NULL && sb->wr_next->sealed) {
      b = sb->wr_next;
      svcbuf_writer_settle(sb, b);
      sb->wr_next = TAILQ_NEXT(b, link);
    }
    svcbuf_follow_settings(sb);
    svcbuf_expire(sb);
    /* a cache whose input has stopped appends nothing, and yields here */
    svcbuf_yield(sb, 0, 0);
    tvh_cond_timedwait(&sb->cond, &sb->lock, mclk() + sec2mono(1));
  }
  tvh_mutex_unlock(&sb->lock);
  return NULL;
}

/* **************************************************************************
 * Input from the service pad (s_stream_mutex held)
 * *************************************************************************/

static svcbuf_block_t *
svcbuf_block_new
  ( svcbuf_t *sb, size_t size, int keep_ram, size_t staging )
{
  svcbuf_block_t *b = calloc(1, sizeof(*b));

  if (b == NULL)
    return NULL;

  b->data = svcbuf_data_alloc(size);
  if (b->data == NULL) {
    free(b);
    return NULL;
  }

  b->alloc     = size;
  b->staging   = staging;
  b->keep_ram  = keep_ram;
  if (keep_ram)
    sb->ram_used += size;
  b->seq       = sb->next_seq++;
  b->start_seq = sb->start_seq;
  b->start     = sb->start;

  if (b->start)
    streaming_start_ref(b->start);

  b->wall = gclk();
  b->mono = mclk();

  memoryinfo_alloc(&timeshift_memoryinfo_ram, b->alloc);

  TAILQ_INSERT_TAIL(&sb->blocks, b, link);

  if (sb->wr_next == NULL)
    sb->wr_next = b;

  sb->cur = b;
  return b;
}

/*
 * Room in the RAM budget for an append's block, under "RAM only" or with
 * the disk failing, where the payload lives in RAM.  A cache over its share
 * while a request is open makes its own room and leaves the free room to
 * the caches that asked; one short of RAM within its share asks the caches
 * over theirs.  With nowhere else to put the payload, the oldest block the
 * gates have passed goes rather than the whole history.  1 when reserved.
 * sb->lock held.
 */
static int
svcbuf_ram_append_reserve ( svcbuf_t *sb, size_t alloc )
{
  const uint64_t share = svcbuf_share(sb, SVCBUF_RAM);
  const int over = share && sb->ram_used + alloc > share;
  int reserved = 0;

  if (!(over && svcbuf_wanted_now(SVCBUF_RAM)))
    reserved = timeshift_ram_reserve(alloc);
  if (reserved && !over)
    svcbuf_served(SVCBUF_RAM, alloc);
  if (!reserved && sb->ram_only) {
    svcbuf_short(sb, SVCBUF_RAM);
    if (!over)
      svcbuf_ask(SVCBUF_RAM, alloc);
  }
  return reserved || svcbuf_ram_make_room(sb, alloc);
}

/*
 * A new block for an append of len bytes, its allocation reserved where it
 * will live: retained RAM when there is room or room can be made, the disk
 * writer's staging otherwise.  NULL when the payload has nowhere to go: the
 * size reservation for it is released and the cache has entered a gap.
 * Called with sb->lock held.
 */
static svcbuf_block_t *
svcbuf_block_for_append ( svcbuf_t *sb, size_t len )
{
  /*
   * Reserve what the block's pages will actually take, not what this
   * append puts in it.  A block is sealed after SVCBUF_BLOCK_AGE whether
   * it is full or not, so on a low-bitrate service a 256 KiB allocation
   * can hold a tenth of that; charging the budget per appended byte
   * let the retained set grow to several times "Maximum RAM size",
   * which is the only bound under "RAM only".  Sealed, the block gives
   * back what it does not hold (svcbuf_block_shrink).
   */
  size_t alloc = svcbuf_page_round(MAX(len, SVCBUF_BLOCK_SIZE));
  size_t staging_reserved = 0;
  int ram_reserved = 0;
  svcbuf_block_t *b;

  /*
   * With a disk, everything goes to it through the write-behind, and the
   * writer keeps a RAM copy of the newest blocks (svcbuf_copy_reserve).
   * Under "RAM only", or with the disk failing, the payload lives in RAM.
   */
  svcbuf_yield(sb, alloc, 0);
  if (sb->ram_only || sb->wr_error) {
    ram_reserved = svcbuf_ram_append_reserve(sb, alloc);
    if (!ram_reserved) {
      timeshift_size_release(len);

      svcbuf_enter_gap_locked
        (sb, len,
         sb->ram_only ? "RAM-only timeshift RAM limit" :
                        "storage write error");
      return NULL;
    }
  }

  /*
   * Disk staging is transient write-behind RAM, not retained cache
   * RAM. Reserve the complete allocation globally before mapping it
   * so simultaneous RAM-to-disk transitions from several services
   * share one bounded backlog instead of each hitting a tiny
   * per-service block-count limit.
   */
  if (!ram_reserved) {
    if (!svcbuf_staging_reserve(alloc)) {
      timeshift_size_release(len);

      svcbuf_enter_gap_locked
        (sb, len, "storage writer backlog");
      return NULL;
    }

    staging_reserved = alloc;
  }

  b = svcbuf_block_new
        (sb, alloc, ram_reserved, staging_reserved);

  if (b == NULL) {
    timeshift_size_release(len);

    if (ram_reserved)
      timeshift_ram_release(alloc);

    if (staging_reserved)
      svcbuf_staging_release(staging_reserved);

    svcbuf_enter_gap_locked
      (sb, len, "memory allocation failure");
  }

  return b;
}

static void
svcbuf_input ( void *opaque, streaming_message_t *sm )
{
  svcbuf_t *sb = opaque;
  svcbuf_block_t *b;
  streaming_start_t *ss;
  streaming_start_t *old = NULL;
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
      svcbuf_follow_mode(sb);

      /* turned off, the cache takes no more, unless a recording still
         replays from it: what comes meanwhile is what it will join */
      if ((!timeshift_conf.enabled || !timeshift_conf.record_cache) &&
          !svcbuf_replaying(sb)) {
        tvh_mutex_unlock(&sb->lock);
        streaming_msg_free(sm);
        return;
      }

      /*
       * After a dropped payload, do not pretend later data is contiguous
       * with the retained history.  A previous disk error does not by
       * itself prevent a fresh RAM-backed timeline from starting later.
       */
      if (!svcbuf_reset_gap_locked(sb)) {
        sb->limit_dropped += len;
        tvh_mutex_unlock(&sb->lock);
        streaming_msg_free(sm);
        return;
      }

      b = sb->cur;

      if (b && (b->size + len > b->alloc ||
                mclk() - b->mono >= SVCBUF_BLOCK_AGE)) {
        b->sealed = 1;
        b = NULL;
        sb->cur = NULL;
        tvh_cond_signal(&sb->cond, 0);
      }

      /*
       * Reserve the combined logical Classic + shared Timeshift size
       * before retaining these bytes; svcbuf_make_room_locked() gives back
       * of the size what is asked, the RAM copies are given back here.
       */
      svcbuf_yield_budget(sb, SVCBUF_RAM, 0);
      if (!svcbuf_make_room_locked(sb, len)) {
        svcbuf_enter_gap_locked
          (sb, len, "combined timeshift size limit");

        tvh_mutex_unlock(&sb->lock);
        streaming_msg_free(sm);
        return;
      }

      /*
       * A block's whole allocation was reserved when it was made -- in
       * RAM, or in the write-behind on its way to disk -- so appending
       * into the room already paid for costs the budget nothing more.
       */
      if (b == NULL)
        b = svcbuf_block_for_append(sb, len);

      if (b == NULL) {
        tvh_mutex_unlock(&sb->lock);
        streaming_msg_free(sm);
        return;
      }

      if (sb->limit_warned) {
        tvhinfo(LS_TIMESHIFT,
                "svcbuf: %s: cache retention resumed with a fresh "
                "timeline after dropping %"PRIu64" kB",
                sb->service->s_nicename,
                sb->limit_dropped / 1024);

        sb->limit_warned  = 0;
        sb->limit_dropped = 0;
      }

      memcpy(b->data + b->size, pktbuf_ptr(pb), len);

      b->size  += len;
      sb->size += len;

      /*
       * For keep_ram blocks the retained RAM budget was reserved by
       * svcbuf_block_for_append().
       * Disk staging is deliberately outside that retained-cache budget
       * and remains bounded by the configured write-behind budget.
       */

      tvh_mutex_unlock(&sb->lock);
    }
  }

  streaming_msg_free(sm);
}

static htsmsg_t *
svcbuf_input_info ( void *opaque, htsmsg_t *list )
{
  (void)opaque;
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
  b = TAILQ_FIRST(&sb->blocks);
  while (b != NULL) {
    nb = TAILQ_NEXT(b, link);
    svcbuf_block_remove(sb, b);
    b = nb;
  }
  seg = TAILQ_FIRST(&sb->segs);
  while (seg != NULL) {
    nseg = TAILQ_NEXT(seg, link);
    svcbuf_seg_close(sb, seg);
    seg = nseg;
  }
  if (sb->start)
    streaming_start_unref(sb->start);

  if (sb->dir) {
    rmdir(sb->dir);
    free(sb->dir);
  }
  tvh_cond_destroy(&sb->cond);
  tvh_mutex_destroy(&sb->lock);
  if (sb->service)
    service_unref(sb->service);
  tvh_mutex_lock(&svcbuf_share_lock);
  LIST_REMOVE(sb, share_link);
  tvh_mutex_unlock(&svcbuf_share_lock);
  free(sb);
  atomic_dec(&svcbuf_live, 1);
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
  if (sb == NULL)
    return;                     /* no cache for this service, as if disabled */

  /* The cache outlives the service stopping -- a reader or a keepalive can
   * still hold it -- and its threads dereference sb->service for the stream
   * mutex and the name, so hold the service for as long as the cache lives */
  service_ref(t);
  sb->service  = t;
  sb->refcount = 1;
  sb->id       = svcbuf_index++;
  atomic_add(&svcbuf_live, 1);
  tvh_mutex_lock(&svcbuf_share_lock);
  LIST_INSERT_HEAD(&svcbuf_sharers, sb, share_link);
  tvh_mutex_unlock(&svcbuf_share_lock);
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
  atomic_set(&g->state, state);
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

/*
 * Read n bytes of a block that is on disk, from `at` into it.  The segment
 * stays open for the read even if the block expires meanwhile.  sb->lock
 * held, released for the read itself; 0, or -1 as svcbuf_read_all().
 */
static int
svcbuf_block_read_disk ( svcbuf_t *sb, svcbuf_block_t *b, size_t at,
                         uint8_t *dst, size_t n )
{
  svcbuf_seg_t *seg = b->seg;
  int fd = seg->fd;
  off_t off = b->off + at;
  int e;

  seg->readers++;
  tvh_mutex_unlock(&sb->lock);
  e = svcbuf_read_all(fd, dst, n, off);
  tvh_mutex_lock(&sb->lock);

  if (--seg->readers == 0 && seg->dead)
    svcbuf_seg_close(sb, seg);
  return e;
}

/* Undo a read that failed: drop its buffer and the START taken for it */
static void
svcbuf_reader_read_failed ( pktbuf_t **pb, streaming_start_t **ss )
{
  pktbuf_ref_dec(*pb);
  *pb = NULL;
  if (ss && *ss) {
    streaming_start_unref(*ss);
    *ss = NULL;
  }
}

/* Finish a bounded historical replay.  s_stream_mutex held. */
static void
svcbuf_gate_finish_history ( svcbuf_gate_t *g, int code )
{
  if (g->state != GATE_REPLAY && g->state != GATE_WAIT)
    return;

  svcbuf_gate_set_state(g, GATE_DONE);
  /*
   * A failure goes straight to the queue the DVR thread reads, when there
   * is one: sent down the chain, globalheaders drops a stop that comes
   * before any start, as it does for Matroska, and the recording would
   * wait for a stop that never comes.
   */
  streaming_target_deliver2
    (code != SM_CODE_OK && g->sq ? &g->sq->sq_st : g->output,
     streaming_msg_create_code(SMT_STOP, code));
}

/*
 * The replay has caught up with the head of the cache (s_stream_mutex
 * held): a bounded historical recording is done, anything else joins live.
 * 1 when it went live.
 */
static int
svcbuf_gate_at_head ( svcbuf_gate_t *g )
{
  service_t *t = g->sb->service;

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
    return 0;
  }

  svcbuf_gate_sync_live_start(g);
  svcbuf_gate_set_state(g, GATE_LIVE);
  return 1;
}

/* What a step of the replay leaves the gate thread to do */
typedef enum {
  GATE_STEP_ON,     ///< go on to the next step of this pass
  GATE_STEP_AGAIN,  ///< start a new pass
  GATE_STEP_OUT     ///< the replay is over
} svcbuf_gate_step_t;

/* What the replay has done so far */
typedef struct svcbuf_gate_run {
  uint64_t bytes;
  int      live;         ///< it went live at the head of the cache
  int      read_failed;  ///< it stopped on a failed read
} svcbuf_gate_run_t;

/* From sb->lock to s_stream_mutex, to act on the gate's output, and back.
 * The steps below hold sb->lock on entry and on return. */
static inline void
svcbuf_gate_to_stream ( svcbuf_gate_t *g )
{
  tvh_mutex_unlock(&g->sb->lock);
  tvh_mutex_lock(&g->sb->service->s_stream_mutex);
}

static inline void
svcbuf_gate_from_stream ( svcbuf_gate_t *g )
{
  tvh_mutex_unlock(&g->sb->service->s_stream_mutex);
  tvh_mutex_lock(&g->sb->lock);
}

/* The size limit was reached under the replay: it stops there */
static svcbuf_gate_step_t
svcbuf_gate_step_space ( svcbuf_gate_t *g )
{
  if (!g->no_space)
    return GATE_STEP_ON;

  svcbuf_gate_to_stream(g);
  if (g->state == GATE_REPLAY) {
    tvherror(LS_TIMESHIFT,
             "svcbuf: %s: stopping cache replay because the "
             "timeshift size limit was reached",
             g->sb->service->s_nicename);
    svcbuf_gate_finish_history(g, SM_CODE_NO_SPACE);
  }
  svcbuf_gate_from_stream(g);
  return GATE_STEP_OUT;
}

/* The consumer's queue is backed up: the replay waits */
static svcbuf_gate_step_t
svcbuf_gate_step_queue ( svcbuf_gate_t *g )
{
  size_t n;

  if (g->sq == NULL)
    return GATE_STEP_ON;

  tvh_mutex_lock(&g->sq->sq_mutex);
  n = g->sq->sq_size;
  tvh_mutex_unlock(&g->sq->sq_mutex);
  if (n <= SVCBUF_QUEUE_MAX)
    return GATE_STEP_ON;

  tvh_mutex_unlock(&g->sb->lock);
  tvh_safe_usleep(20000);
  tvh_mutex_lock(&g->sb->lock);
  return GATE_STEP_AGAIN;
}

/*
 * A bounded retrospective recording never reaches live.  Blocks are
 * sealed roughly once a second, so stopping at the first block whose
 * wall-clock start is >= 'to' gives the same practical granularity as
 * the cache itself and never records the following programme wholesale.
 */
static svcbuf_gate_step_t
svcbuf_gate_step_bound ( svcbuf_gate_t *g )
{
  const svcbuf_block_t *b = g->blk;

  if (!(g->to && b->wall >= g->to))
    return GATE_STEP_ON;

  svcbuf_gate_to_stream(g);
  /* the stop may have moved meanwhile (svcbuf_gate_set_stop) */
  if (g->state == GATE_REPLAY && g->to && b->wall >= g->to)
    svcbuf_gate_finish_history(g, SM_CODE_OK);
  svcbuf_gate_from_stream(g);
  return GATE_STEP_AGAIN;
}

/*
 * The next cached block belongs to a new service stream map.
 * Reconfigure downstream before sending any MPEG-TS from that map.
 */
static svcbuf_gate_step_t
svcbuf_gate_step_start ( svcbuf_gate_t *g )
{
  const svcbuf_block_t *b = g->blk;
  streaming_start_t *ss = b->start;
  const uint64_t start_seq = b->start_seq;

  if (start_seq == g->start_seq)
    return GATE_STEP_ON;

  if (ss)
    streaming_start_ref(ss);
  svcbuf_gate_to_stream(g);
  if (g->state == GATE_REPLAY)
    svcbuf_gate_deliver_start(g, ss, start_seq, 1, 1);
  else if (ss)
    streaming_start_unref(ss);
  svcbuf_gate_from_stream(g);
  return GATE_STEP_AGAIN;
}

/* The next chunk of the block, from RAM or from the disk, to the output */
static svcbuf_gate_step_t
svcbuf_gate_step_chunk ( svcbuf_gate_t *g, svcbuf_gate_run_t *r )
{
  svcbuf_t *sb = g->sb;
  service_t *t = sb->service;
  svcbuf_block_t *b = g->blk;
  time_t wall;
  pktbuf_t *pb;
  size_t n;
  int restart = 0;

  if (g->off >= b->size)
    return GATE_STEP_ON;

  n = MIN(b->size - g->off, SVCBUF_READ_SIZE);
  pb = pktbuf_alloc(NULL, n);
  if (b->data) {
    memcpy(pktbuf_ptr(pb), b->data + g->off, n);
  } else if (svcbuf_block_read_disk(sb, b, g->off, pktbuf_ptr(pb), n)) {
    tvherror(LS_TIMESHIFT,
             "svcbuf: read failed: %s, joining live",
             strerror(errno));
    pktbuf_ref_dec(pb);
    r->read_failed = 1;
    return GATE_STEP_OUT;
  }
  g->off += n;
  wall = b->wall;

  svcbuf_gate_to_stream(g);
  /* the stop may have moved before this block meanwhile */
  if (g->state == GATE_REPLAY && g->to && wall >= g->to)
    svcbuf_gate_finish_history(g, SM_CODE_OK);
  if (g->state == GATE_REPLAY) {
    streaming_target_deliver2
      (g->output, streaming_msg_create_data(SMT_MPEGTS, pb));
    r->bytes += n;
    restart = atomic_set(&t->s_pending_restart, 0);
  } else {
    pktbuf_ref_dec(pb);
  }
  if (restart)
    service_restart_streams(t);
  svcbuf_gate_from_stream(g);
  return GATE_STEP_AGAIN;
}

/*
 * On to the next block.  A bounded retrospective recording stops at its
 * historical DVR end, not at the current cache head.  Blocks are
 * wall-clock stamped at their beginning; after the current block has been
 * delivered, do not enter a following block which starts at or beyond the
 * requested stop time.
 */
static svcbuf_gate_step_t
svcbuf_gate_step_next ( svcbuf_gate_t *g )
{
  svcbuf_block_t *nb = TAILQ_NEXT(g->blk, link);
  time_t next_wall;

  if (nb == NULL)
    return GATE_STEP_ON;
  if (!(g->to && nb->wall >= g->to)) {
    g->blk = nb;
    g->off = 0;
    return GATE_STEP_AGAIN;
  }

  next_wall = nb->wall;
  svcbuf_gate_to_stream(g);
  /* the stop may have moved meanwhile (svcbuf_gate_set_stop) */
  if (g->state == GATE_REPLAY && g->to && next_wall >= g->to) {
    tvhinfo(LS_TIMESHIFT,
            "svcbuf: %s: retrospective replay reached stop boundary "
            "%"PRItime_t,
            g->sb->service->s_nicename, g->to);
    svcbuf_gate_finish_history(g, SM_CODE_OK);
  }
  svcbuf_gate_from_stream(g);
  return GATE_STEP_AGAIN;
}

/*
 * Caught up.  s_stream_mutex prevents new live data from being
 * appended while the replay/live handover is decided.  An append that
 * hit the size limit meanwhile left the tail as it was but dropped
 * data: that is the failure above, not a seamless join.
 */
static svcbuf_gate_step_t
svcbuf_gate_step_head ( svcbuf_gate_t *g, svcbuf_gate_run_t *r )
{
  svcbuf_t *sb = g->sb;
  service_t *t = sb->service;

  tvh_mutex_unlock(&sb->lock);
  tvh_mutex_lock(&t->s_stream_mutex);
  tvh_mutex_lock(&sb->lock);

  if (g->no_space || TAILQ_NEXT(g->blk, link) != NULL ||
      g->off != g->blk->size) {
    tvh_mutex_unlock(&t->s_stream_mutex);
    return GATE_STEP_AGAIN;
  }

  tvh_mutex_unlock(&sb->lock);
  if (g->state == GATE_REPLAY)
    r->live = svcbuf_gate_at_head(g);
  svcbuf_gate_from_stream(g);
  return GATE_STEP_OUT;
}

/*
 * The replay over without having gone live at the head: a bounded
 * recording ends there; one that joins live does so, what could not be
 * read missing from it and counted as one of its errors.
 */
static void
svcbuf_gate_end ( svcbuf_gate_t *g, const svcbuf_gate_run_t *r )
{
  if (r->live || atomic_get(&g->state) != GATE_REPLAY)
    return;

  svcbuf_gate_to_stream(g);
  if (g->state == GATE_REPLAY && g->to && !(g->all && g->to > gclk())) {
    svcbuf_gate_finish_history(g, SM_CODE_NO_INPUT);
  } else if (g->state == GATE_REPLAY) {
    if (r->read_failed && g->sq)
      streaming_target_deliver2
        (&g->sq->sq_st, streaming_msg_create_code(SMT_SERVICE_STATUS,
                                                  TSS_TIMEOUT));
    svcbuf_gate_sync_live_start(g);
    svcbuf_gate_set_state(g, GATE_LIVE);
  }
  svcbuf_gate_from_stream(g);
}

static void *
svcbuf_gate_thread ( void *aux )
{
  svcbuf_gate_t *g = aux;
  svcbuf_t *sb = g->sb;
  svcbuf_gate_run_t r = { 0, 0, 0 };
  svcbuf_gate_step_t step = GATE_STEP_AGAIN;

  tvh_mutex_lock(&sb->lock);

  /* A hint only: every action below re-checks under s_stream_mutex */
  while (step != GATE_STEP_OUT && atomic_get(&g->state) == GATE_REPLAY) {
    step = svcbuf_gate_step_space(g);
    if (step == GATE_STEP_ON)
      step = svcbuf_gate_step_queue(g);
    if (step == GATE_STEP_ON)
      step = svcbuf_gate_step_bound(g);
    if (step == GATE_STEP_ON)
      step = svcbuf_gate_step_start(g);
    if (step == GATE_STEP_ON)
      step = svcbuf_gate_step_chunk(g, &r);
    if (step == GATE_STEP_ON)
      step = svcbuf_gate_step_next(g);
    if (step == GATE_STEP_ON)
      step = svcbuf_gate_step_head(g, &r);
  }

  svcbuf_gate_end(g, &r);
  g->blk = NULL;
  tvh_mutex_unlock(&sb->lock);

  tvhinfo(LS_TIMESHIFT,
          "svcbuf: %s: replayed %"PRIu64" kB from the cache%s",
          sb->service->s_nicename, r.bytes / 1024, r.live ? ", now live" : "");

  return NULL;
}

/* Position the replay at 'from' and start it (s_stream_mutex held) */
static int
svcbuf_gate_start ( svcbuf_gate_t *g )
{
  svcbuf_t *sb = g->sb;
  svcbuf_block_t *b;
  svcbuf_block_t *first;
  const svcbuf_block_t *last;
  svcbuf_block_t *pos = NULL;
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

    /*
     * If cache replay starts, replace this current START with the
     * historical START belonging to the cached MPEG-TS.
     */
    if (sm->sm_type == SMT_START && svcbuf_gate_start(g)) {
      streaming_msg_free(sm);
      return;
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
  if (g == NULL)
    return NULL;                /* the caller keeps its own output */

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

/*
 * A recording made of the whole cache ends at its entry's stop, which an
 * edit may move while the cache is replayed: the gate ends at the new one.
 * Only a bounded replay has a stop to move.  global_lock held.
 */
void
svcbuf_gate_set_stop ( streaming_target_t *pad, time_t to )
{
  svcbuf_gate_t *g = (svcbuf_gate_t *)pad;
  svcbuf_t *sb = g->sb;

  if (sb == NULL || g->to == 0)
    return;
  tvh_mutex_lock(&sb->service->s_stream_mutex);
  tvh_mutex_lock(&sb->lock);
  g->to = to;
  tvh_mutex_unlock(&sb->lock);
  tvh_mutex_unlock(&sb->service->s_stream_mutex);
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

/* Current service stream-map generation. */
uint64_t
svcbuf_start_seq ( svcbuf_t *sb )
{
  uint64_t seq;

  tvh_mutex_lock(&sb->lock);
  seq = sb->start_seq;
  tvh_mutex_unlock(&sb->lock);

  return seq;
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

  if (r == NULL)
    return NULL;

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
  svcbuf_block_t *b;
  svcbuf_block_t *pos;
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

/* A reader whose blocks all expired goes on with the ones since.  sb->lock
 * held. */
static void
svcbuf_reader_reattach ( svcbuf_t *sb, svcbuf_reader_t *r )
{
  if (r->blk == NULL && r->skipped) {
    r->blk = TAILQ_FIRST(&sb->blocks);
    r->off = 0;
  }
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
  svcbuf_block_t *b;
  svcbuf_block_t *nb;
  size_t n;

  *pb = NULL;
  if (ss)
    *ss = NULL;
  if (start_seq)
    *start_seq = 0;
  if (block_mono)
    *block_mono = 0;

  tvh_mutex_lock(&sb->lock);
  svcbuf_reader_reattach(sb, r);
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

      /* The block may expire while a disk read is in progress: the START
       * has already been referenced above, and the read keeps the segment
       * open until it finishes. */
      if (b->data) {
        memcpy(pktbuf_ptr(*pb), b->data + r->off, n);
      } else if (svcbuf_block_read_disk(sb, b, r->off, pktbuf_ptr(*pb), n)) {
        tvherror(LS_TIMESHIFT, "svcbuf: read failed: %s",
                 strerror(errno));
        svcbuf_reader_read_failed(pb, ss);
        tvh_mutex_unlock(&sb->lock);
        return -1;
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
