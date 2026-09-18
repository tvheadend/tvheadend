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
#define LDDCI_MTD_FAIR_BURST_PKTS    1      /* one TS packet per CAM queue item */
#define LDDCI_MTD_RR_BURST_PKTS      7      /* writer quantum per active MTD context */
#define LDDCI_MTD_RR_LANES           31     /* one lane per MTD namespace */
/* Keep MTD queue items small enough for the writer to interleave active
 * virtual contexts without draining one mux-local batch first. */
#define LDDCI_TS_SYNC_BYTE          0x47
#define LDDCI_TS_SIZE               188
#define LDDCI_TS_SCRAMBLING_CONTROL 0xC0

#define LDDCI_TO_THREAD(_t)  (linuxdvb_ddci_thread_t *)(_t)

#define LDDCI_WR_THREAD_STAT_TMO  10  /* sec */
#define LDDCI_RD_THREAD_STAT_TMO  10  /* sec */

#define LDDCI_MIN_TS_PKT (100 * LDDCI_TS_SIZE)
#define LDDCI_MIN_TS_SYN (5 * LDDCI_TS_SIZE)

/* dvbcam.c owns the active CA-PMT/program state. */

typedef struct linuxdvb_ddci_thread
{
  linuxdvb_ddci_t          *lddci;
  int                       lddci_thread_running;
  int                       lddci_thread_stop;
  pthread_t                 lddci_thread;
  tvh_mutex_t           lddci_thread_lock;
  tvh_cond_t                lddci_thread_cond;
} linuxdvb_ddci_thread_t;

typedef struct linuxdvb_ddci_send_packet
{
  TAILQ_ENTRY(linuxdvb_ddci_send_packet)  lddci_send_pkt_link;
  size_t                                  lddci_send_pkt_len;
  int                                     lddci_send_pkt_ctx_id; /* -1 = native/non-MTD */
  uint8_t                                 lddci_send_pkt_data[0];
} linuxdvb_ddci_send_packet_t;

typedef struct linuxdvb_ddci_send_lane
{
  TAILQ_HEAD(,linuxdvb_ddci_send_packet)  queue;
  int                                     ctx_id;
  uint64_t                                bytes;
} linuxdvb_ddci_send_lane_t;

typedef struct linuxdvb_ddci_send_buffer
{
  /* MTD traffic uses persistent per-context lanes. Native traffic retains
   * its own FIFO; the writer performs bounded round-robin scheduling. */
  TAILQ_HEAD(,linuxdvb_ddci_send_packet)  lddci_send_buf_native;
  linuxdvb_ddci_send_lane_t               lddci_send_lanes[LDDCI_MTD_RR_LANES];
  int                                     lddci_send_rr_lane;
  int                                     lddci_send_rr_burst;
  uint64_t                                lddci_send_buf_size_max;
  uint64_t                                lddci_send_buf_size;
  tvh_mutex_t                         lddci_send_buf_lock;
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

/* --------------------------------------------------------------------
 * MTD runtime state. There is intentionally no hard-coded mux/service
 * count: Tvheadend's per-CAM Service limit remains the user-facing
 * admission limit, while bookkeeping grows dynamically. Each mux gets a
 * stable ctx_id for its lifetime, independent of its array position.
 *
 * PID remapping follows VDR MTD: each active mux gets one of 31 virtual
 * slots encoded in PID bits 8..12, with an independent 8-bit local PID
 * namespace. CAT PID 1 is the only fixed-PID exception. SIDs use the same
 * slot identity in CA-PMT control messages, with an 8-bit local SID index.
 * ------------------------------------------------------------------ */

#define LDDCI_MTD_PID_SPACE      8192
#define LDDCI_MTD_PID_FIRST      0x0000
#define LDDCI_MTD_PID_LAST       0x1FFE /* 0x1FFF is the NULL PID */
#define LDDCI_MTD_PID_BITS       ((LDDCI_MTD_PID_SPACE + 7) / 8)
#define LDDCI_MTD_SLOT_MAX       31     /* five high PID bits, like VDR MTD */
#define LDDCI_MTD_LOCAL_PIDS     256    /* low eight PID bits per mux slot */
#define LDDCI_MTD_LOCAL_SIDS     256
#define LDDCI_MTD_SLOT_QUARANTINE_MS 2000

typedef struct linuxdvb_ddci_pidmap
{
  /* Every ordinary PID from the mux, including DVB PSI/SI PIDs, is mapped
   * into the mux slot namespace. CAT PID 1 is handled separately. Mappings
   * intentionally live for the entire mux-slot lifetime, like VDR's mapper,
   * so service/subscription churn cannot reshuffle a live CAM namespace. */
  uint16_t map_r2u[LDDCI_MTD_PID_SPACE];
  uint8_t  map_pinned[LDDCI_MTD_PID_BITS]; /* mux-pinned CAT/EMM PID mappings */
} linuxdvb_ddci_pidmap_t;

#define LDDCI_CAT_MAX_PACKETS  8
#define LDDCI_CAT_MAX_SECTION  1024

typedef struct linuxdvb_ddci_cat_reasm
{
  uint8_t  pkts[LDDCI_CAT_MAX_PACKETS][LDDCI_TS_SIZE];
  int      npkts;
  uint8_t  section[LDDCI_CAT_MAX_SECTION];
  int      seclen;
  int      have;
  /* VDR cCaPidReceiver semantics: remember the last fully rewritten CAT
   * and its version. Repeated broadcasts of the same CAT version are sent
   * from this stable image instead of being reparsed/remapped every time. */
  int      cache_valid;
  int      cache_version;
  int      cache_npkts;
  uint8_t  cache_pkts[LDDCI_CAT_MAX_PACKETS][LDDCI_TS_SIZE];
} linuxdvb_ddci_cat_reasm_t;

typedef struct linuxdvb_ddci_mux_ctx
{
  int             ctx_id;       /* stable diagnostic/lifetime identity */
  int             ctx_slot;     /* 1..31 encoded in bits 8..12 of CAM PIDs */
  mpegts_mux_t   *ctx_mux;
  mpegts_input_t *ctx_input;
  int             ctx_refcount;
  linuxdvb_ddci_pidmap_t     ctx_pidmap;
  uint16_t                   ctx_sid_real[LDDCI_MTD_LOCAL_SIDS];
  int                        ctx_sid_count;
  linuxdvb_ddci_cat_reasm_t  ctx_cat;
  int                        ctx_cat_multi_warned;
} linuxdvb_ddci_mux_ctx_t;

typedef struct linuxdvb_ddci_svc_ctx
{
  service_t *svc_t;
  int        svc_ctx_id;    /* stable ctx_id, not an array index */
  /* CAM-visible PIDs referenced by this service's CA-PMT. */
  uint8_t    svc_cam_pids[LDDCI_MTD_PID_BITS];
  /* Synthetic present/following EIT startup state. Keep it per service
   * because multiple services may share one virtual transponder/context. */
  int64_t    svc_mtd_eit_start_mono;
  int64_t    svc_mtd_eit_last_mono;
  uint16_t   svc_mtd_eit_sid;
  uint8_t    svc_mtd_eit_cc;
  uint8_t    svc_mtd_eit_version;
  uint32_t   svc_mtd_eit_sent;
} linuxdvb_ddci_svc_ctx_t;

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

  /* Protects all MTD runtime state below. */
  tvh_mutex_t                 lddci_mux_lock;

  /* Dynamic mux/service tracking. Arrays may compact; ctx_id does not. */
  linuxdvb_ddci_mux_ctx_t    *lddci_mux_ctx;
  int                         lddci_mux_ctx_count;
  /* Present one coherent CAT on physical PID 1. Each active MTD context
   * contributes its rewritten CA descriptors so all mapped EMM namespaces
   * remain advertised without interleaved independent CAT state machines. */
  uint32_t                    lddci_mtd_cat_union_hash;
  int                         lddci_mtd_cat_union_valid;
  uint8_t                     lddci_mtd_cat_union_version;
  uint8_t                     lddci_mtd_cat_union_cc;
  int                         lddci_mux_ctx_alloc;
  int                         lddci_next_ctx_id;
  linuxdvb_ddci_svc_ctx_t    *lddci_svc_ctx;
  int                         lddci_svc_ctx_count;
  int                         lddci_svc_ctx_alloc;

  /* Reverse PID mapping and ownership indexed by CAM-visible PID. */
  uint16_t                    lddci_pid_u2r[LDDCI_MTD_PID_SPACE];
  uint32_t                    lddci_pid_owner[LDDCI_MTD_PID_SPACE];
  /* VDR-style slot-local allocation cursor. Deliberately survives slot
   * reuse so recently released synthetic PID values are not immediately
   * recycled while CAM packets may still be in flight. */
  uint8_t                     lddci_slot_next_pid[LDDCI_MTD_SLOT_MAX + 1];
  /* A CAM may return packets after a mux context has been destroyed.
   * Keep the CAM-visible slot namespace quarantined briefly so a delayed
   * packet can never be mistaken for data from an immediately reused slot.
   * These deadlines deliberately survive the all-services-idle cleanup. */
  int64_t                     lddci_slot_quarantine_until[LDDCI_MTD_SLOT_MAX + 1];

  const uint8_t              *lddci_prev_tsb;
  int                         lddci_prev_ctx_id;

};

/* ------------------------------------------------------------------- *
 * MTD helper functions
 * ------------------------------------------------------------------- */

static int
linuxdvb_ddci_find_mux_ctx ( const linuxdvb_ddci_t *lddci, const mpegts_mux_t *mm )
{
  int i;
  for (i = 0; i < lddci->lddci_mux_ctx_count; i++)
    if (lddci->lddci_mux_ctx[i].ctx_mux == mm)
      return i;
  return -1;
}

static int
linuxdvb_ddci_find_mux_ctx_id ( linuxdvb_ddci_t *lddci, int ctx_id )
{
  int i;
  for (i = 0; i < lddci->lddci_mux_ctx_count; i++)
    if (lddci->lddci_mux_ctx[i].ctx_id == ctx_id)
      return i;
  return -1;
}

/* Physical CAT clock owner.  Only this context's recurring CAT cadence is used
 * to emit the synthesized physical CAT; CAT descriptors from every active MTD
 * context are merged into that one stream below.  The oldest surviving array
 * entry supplies the cadence, and another context takes over automatically if
 * it disappears. */
static int
linuxdvb_ddci_cat_emm_owner_ctx_locked ( const linuxdvb_ddci_t *lddci )
{
  return lddci->lddci_mux_ctx_count > 0 ? lddci->lddci_mux_ctx[0].ctx_id : -1;
}

static int
linuxdvb_ddci_find_mux_ctx_slot ( linuxdvb_ddci_t *lddci, int ctx_slot )
{
  int i;
  for (i = 0; i < lddci->lddci_mux_ctx_count; i++)
    if (lddci->lddci_mux_ctx[i].ctx_slot == ctx_slot)
      return i;
  return -1;
}

static int
linuxdvb_ddci_alloc_mux_slot_locked ( linuxdvb_ddci_t *lddci )
{
  int slot;
  int64_t now = mclk();

  /* A CAM-visible namespace only has to be unique while it is active or
   * while delayed return packets from its previous owner may still arrive.
   * Reuse a free slot once its drain quarantine has expired instead of
   * consuming a new namespace forever.  The slot-local PID cursor survives
   * reuse, so PID values within that namespace also continue moving forward. */
  for (slot = 1; slot <= LDDCI_MTD_SLOT_MAX; slot++) {
    if (linuxdvb_ddci_find_mux_ctx_slot(lddci, slot) >= 0)
      continue;
    if (lddci->lddci_slot_quarantine_until[slot] > now)
      continue;

    return slot;
  }
  tvherror(LS_DDCI, "CAM %s: no MTD namespace available", lddci->lddci_id);
  return -1;
}

static int
linuxdvb_ddci_find_svc_ctx ( const linuxdvb_ddci_t *lddci, const service_t *t )
{
  int i;
  for (i = 0; i < lddci->lddci_svc_ctx_count; i++)
    if (lddci->lddci_svc_ctx[i].svc_t == t)
      return i;
  return -1;
}

static int
linuxdvb_ddci_ensure_mux_capacity ( linuxdvb_ddci_t *lddci, int need )
{
  linuxdvb_ddci_mux_ctx_t *p;
  int n;
  if (need <= lddci->lddci_mux_ctx_alloc)
    return 0;
  n = lddci->lddci_mux_ctx_alloc ? lddci->lddci_mux_ctx_alloc * 2 : 4;
  while (n < need) n *= 2;
  p = realloc(lddci->lddci_mux_ctx, n * sizeof(*p));
  if (p == NULL) return -1;
  lddci->lddci_mux_ctx = p;
  lddci->lddci_mux_ctx_alloc = n;
  return 0;
}

static int
linuxdvb_ddci_ensure_svc_capacity ( linuxdvb_ddci_t *lddci, int need )
{
  linuxdvb_ddci_svc_ctx_t *p;
  int n;
  if (need <= lddci->lddci_svc_ctx_alloc)
    return 0;
  n = lddci->lddci_svc_ctx_alloc ? lddci->lddci_svc_ctx_alloc * 2 : 8;
  while (n < need) n *= 2;
  p = realloc(lddci->lddci_svc_ctx, n * sizeof(*p));
  if (p == NULL) return -1;
  lddci->lddci_svc_ctx = p;
  lddci->lddci_svc_ctx_alloc = n;
  return 0;
}

static inline int
linuxdvb_ddci_pidbit_test ( const uint8_t *bits, uint16_t pid )
{
  return bits && (bits[pid >> 3] & (1U << (pid & 7)));
}

static inline void
linuxdvb_ddci_pidbit_set ( uint8_t *bits, uint16_t pid )
{
  bits[pid >> 3] |= (uint8_t)(1U << (pid & 7));
}

static void
linuxdvb_ddci_release_ctx_maps ( linuxdvb_ddci_t *lddci, int ctx_id )
{
  uint32_t owner = (uint32_t)ctx_id + 1;
  int i;

  for (i = LDDCI_MTD_PID_FIRST; i <= LDDCI_MTD_PID_LAST; i++)
    if (lddci->lddci_pid_owner[i] == owner) {
      lddci->lddci_pid_owner[i] = 0;
      lddci->lddci_pid_u2r[i] = 0;
    }
}

/* Walk CA descriptors (EN 300 468 tag 0x09 CA_descriptor) in the byte
 * range [buf, buf+len), calling cb() with pointers to each one's on-wire
 * PID field (2 bytes: top 3 bits reserved + 13-bit PID) so the callback
 * can read or rewrite it in place. The CA_descriptor format is identical
 * wherever it appears - inside a CAT's own descriptor loop, or inside a
 * CA-PMT's program-level or per-stream descriptor loop - so this one
 * walker is shared by both linuxdvb_ddci.c's CAT rewriting below and
 * dvbcam.c's CA-PMT rewriting (see dvb_ca_descriptors_foreach() in
 * linuxdvb_private.h). Returns 0 on success, -1 if a descriptor's
 * claimed length would run past the buffer (malformed input). */
typedef void (*dvb_cadesc_pid_cb_t)(uint8_t *pid_hi, uint8_t *pid_lo, void *aux);

int
dvb_ca_descriptors_foreach ( uint8_t *buf, int len, dvb_cadesc_pid_cb_t cb, void *aux )
{
  int i;
  for (i = 0; i + 1 < len; ) {
    uint8_t tag  = buf[i];
    uint8_t dlen = buf[i + 1];
    if (i + 2 + dlen > len)
      return -1;
    if (tag == 0x09 && dlen >= 4)
      cb(&buf[i + 4], &buf[i + 5], aux);
    i += 2 + dlen;
  }
  return 0;
}

/* Callback context + callback for remapping a CAT's EMM PIDs via
 * dvb_ca_descriptors_foreach() above. */
static uint16_t linuxdvb_ddci_map_pid_locked ( linuxdvb_ddci_t *lddci,
                                                int ctx_id, uint16_t real_pid );
static uint16_t linuxdvb_ddci_pin_pid_locked ( linuxdvb_ddci_t *lddci,
                                               int ctx_id, uint16_t real_pid );

typedef struct linuxdvb_ddci_cat_emm_ctx {
  linuxdvb_ddci_t *lddci;
  int              ctx_idx;
} linuxdvb_ddci_cat_emm_ctx_t;

static void
linuxdvb_ddci_cat_emm_cb ( uint8_t *pid_hi, uint8_t *pid_lo, void *aux )
{
  linuxdvb_ddci_cat_emm_ctx_t *c = aux;
  uint16_t real_emm = (uint16_t)(((*pid_hi << 8) | *pid_lo) & 0x1FFF);
  uint16_t uniq_emm = linuxdvb_ddci_pin_pid_locked(c->lddci, c->ctx_idx, real_emm);

  if (uniq_emm) {
    *pid_hi = (uint8_t)((*pid_hi & 0xE0) | ((uniq_emm >> 8) & 0x1F));
    *pid_lo = (uint8_t)(uniq_emm & 0xFF);
  }
}

/* rewrite the PID field of one 188-byte TS packet in place, preserving
 * the transport_error_indicator / payload_unit_start / priority flag
 * bits in the top 3 bits of byte 1 */
static inline void
linuxdvb_ddci_ts_set_pid ( uint8_t *pkt, uint16_t pid )
{
  pkt[1] = (uint8_t)((pkt[1] & 0xE0) | ((pid >> 8) & 0x1F));
  pkt[2] = (uint8_t)(pid & 0xFF);
}

static inline uint16_t
linuxdvb_ddci_ts_get_pid ( const uint8_t *pkt )
{
  return (uint16_t)(((pkt[1] << 8) | pkt[2]) & 0x1FFF);
}

/* Allocate (or return) a globally unique synthetic PID for one stable
 * mux context. Every non-system PID from every context, including ctx #0,
 * goes through this allocator; therefore a real PID value can never collide
 * with a synthetic PID inside the CAM namespace. Must be called with
 * lddci_mux_lock held. */
static uint16_t
linuxdvb_ddci_map_pid_locked ( linuxdvb_ddci_t *lddci, int ctx_id,
                               uint16_t real_pid )
{
  linuxdvb_ddci_pidmap_t *pm;
  linuxdvb_ddci_mux_ctx_t *ctx;
  uint32_t owner;
  int ctx_idx;
  int p;
  int local;
  int uniq;
  int slot;

  if (real_pid > LDDCI_MTD_PID_LAST)
    return real_pid;
  ctx_idx = linuxdvb_ddci_find_mux_ctx_id(lddci, ctx_id);
  if (ctx_idx < 0)
    return 0;

  ctx = &lddci->lddci_mux_ctx[ctx_idx];
  pm = &ctx->ctx_pidmap;
  if (pm->map_r2u[real_pid])
    return pm->map_r2u[real_pid];

  slot = ctx->ctx_slot;
  if (slot < 1 || slot > LDDCI_MTD_SLOT_MAX)
    return 0;

  owner = (uint32_t)ctx_id + 1;
  for (p = 0; p < LDDCI_MTD_LOCAL_PIDS; p++) {
    local = (lddci->lddci_slot_next_pid[slot] + p) & 0xFF;
    uniq = (slot << 8) | local;
    /* 0x1FFF is the MPEG-TS NULL PID and cannot be used as a synthetic
     * identity. This only excludes local 0xFF from slot 31. */
    if (uniq == 0x1FFF)
      continue;
    if (lddci->lddci_pid_owner[uniq] == 0) {
      lddci->lddci_pid_owner[uniq] = owner;
      lddci->lddci_pid_u2r[uniq] = real_pid;
      pm->map_r2u[real_pid] = (uint16_t)uniq;
      lddci->lddci_slot_next_pid[slot] = (uint8_t)(local + 1);
      return (uint16_t)uniq;
    }
  }

  tvherror(LS_DDCI,
           "CAM %s: MTD slot %d PID namespace exhausted while mapping "
           "ctx #%d real PID %d (%04X) - dropping packet",
           lddci->lddci_id, slot, ctx_id, real_pid, real_pid);
  return 0;
}

/* CAT/EMM mappings are deliberately mux-pinned. Entitlement updates must
 * keep a stable advertised EMM PID for as long as that real mux is active,
 * even if the service that first caused us to parse the CAT is stopped. */
static uint16_t
linuxdvb_ddci_pin_pid_locked ( linuxdvb_ddci_t *lddci, int ctx_id,
                               uint16_t real_pid )
{
  linuxdvb_ddci_pidmap_t *pm;
  uint16_t uniq;
  int ctx_idx;

  uniq = linuxdvb_ddci_map_pid_locked(lddci, ctx_id, real_pid);
  if (!uniq || real_pid > LDDCI_MTD_PID_LAST)
    return uniq;
  ctx_idx = linuxdvb_ddci_find_mux_ctx_id(lddci, ctx_id);
  if (ctx_idx >= 0) {
    pm = &lddci->lddci_mux_ctx[ctx_idx].ctx_pidmap;
    linuxdvb_ddci_pidbit_set(pm->map_pinned, real_pid);
  }
  return uniq;
}

/* Get a mapping and register one reference owned by a service. The per-
 * service bitset makes repeated PMT/packet visits idempotent. */
static uint16_t
linuxdvb_ddci_map_pid_for_service_locked ( linuxdvb_ddci_t *lddci,
                                           int svc_idx, uint16_t real_pid )
{
  linuxdvb_ddci_svc_ctx_t *svc;

  if (real_pid > LDDCI_MTD_PID_LAST)
    return real_pid;
  if (svc_idx < 0 || svc_idx >= lddci->lddci_svc_ctx_count)
    return 0;

  svc = &lddci->lddci_svc_ctx[svc_idx];
  return linuxdvb_ddci_map_pid_locked(lddci, svc->svc_ctx_id, real_pid);
}

/* Reverse lookup for one stable ctx_id. Since all contexts are mapped,
 * synthetic PID ownership alone identifies which decrypted packets belong
 * to each real mux. */
static uint16_t
linuxdvb_ddci_unmap_pid ( const linuxdvb_ddci_t *lddci, int ctx_id, uint16_t uniq_pid )
{
  uint32_t owner;

  if (uniq_pid > LDDCI_MTD_PID_LAST)
    return 0xFFFF;
  owner = (uint32_t)ctx_id + 1;
  return lddci->lddci_pid_owner[uniq_pid] == owner ?
         lddci->lddci_pid_u2r[uniq_pid] : 0xFFFF;
}

/* Allocate (or return) a slot-encoded SID. SIDs only appear in CA-PMT, so
 * a small per-mux linear table is sufficient and matches VDR's approach. */
static uint16_t
linuxdvb_ddci_map_sid ( linuxdvb_ddci_t *lddci, int ctx_id, uint16_t real_sid )
{
  linuxdvb_ddci_mux_ctx_t *ctx;
  int ctx_idx;
  int i;

  if (ctx_id < 0)
    return real_sid;
  ctx_idx = linuxdvb_ddci_find_mux_ctx_id(lddci, ctx_id);
  if (ctx_idx < 0)
    return real_sid;

  ctx = &lddci->lddci_mux_ctx[ctx_idx];
  for (i = 0; i < ctx->ctx_sid_count; i++)
    if (ctx->ctx_sid_real[i] == real_sid)
      return (uint16_t)((ctx->ctx_slot << 8) | i);

  if (ctx->ctx_sid_count >= LDDCI_MTD_LOCAL_SIDS) {
    tvherror(LS_DDCI,
             "CAM %s: MTD slot %d SID namespace exhausted while mapping "
             "ctx #%d SID %d (%04X)",
             lddci->lddci_id, ctx->ctx_slot, ctx_id, real_sid, real_sid);
    return real_sid;
  }

  i = ctx->ctx_sid_count++;
  ctx->ctx_sid_real[i] = real_sid;
  return (uint16_t)((ctx->ctx_slot << 8) | i);
}

/* Reassemble, remap the EMM PID values inside, recompute the CRC32, and
 * re-emit CAT (PID 1) TS packets for an MTD mux context. CAT
 * itself is never PID-remapped (real CAMs only recognise it on the
 * fixed PID 1), but the EMM PID *values* it advertises collide across
 * real transponders exactly like content PIDs do, so they are remapped
 * the same way, using the same per-context dynamic PID map
 * allocates from.
 *
 * This mirrors VDR's cCaPidReceiver::Receive() (mtd.h/ci.c) - tvheadend
 * has no ready-made utility at this raw-TS-packet reassembly layer, so
 * this is new code following VDR's proven approach rather than adapting
 * an existing tvheadend function.
 *
 * Only a single CAT *table section* (possibly split across multiple TS
 * *packets*, which this handles) is supported - if more than one
 * distinct CAT section is seen, this is logged once and the packet is
 * forwarded with its EMM PIDs unmapped. This is the same limitation
 * VDR's own MTD implementation has (see the multi-table CAT handling
 * note in VDR's ci.c) - not a shortcut unique to this patch.
 *
 * Must be called with lddci->lddci_mux_lock held. Writes up to
 * LDDCI_CAT_MAX_PACKETS 188-byte packets into out_pkts and returns how
 * many of them the caller should send to the CAM (0 if this packet was
 * consumed into an in-progress reassembly with nothing to send yet). */
static int
linuxdvb_ddci_cat_process ( linuxdvb_ddci_t *lddci, int ctx_id,
                            const uint8_t *in_pkt,
                            uint8_t out_pkts[][LDDCI_TS_SIZE] )
{
  int array_idx = linuxdvb_ddci_find_mux_ctx_id(lddci, ctx_id);
  linuxdvb_ddci_mux_ctx_t *ctx;
  linuxdvb_ddci_cat_reasm_t *r;
  int pusi;

  if (array_idx < 0) {
    memcpy(out_pkts[0], in_pkt, LDDCI_TS_SIZE);
    return 1;
  }
  ctx = &lddci->lddci_mux_ctx[array_idx];
  r = &ctx->ctx_cat;
  pusi = in_pkt[1] & 0x40;

  if (pusi) {
    int ptr = in_pkt[4];
    const uint8_t *sec = in_pkt + 5 + ptr;
    int avail = LDDCI_TS_SIZE - 5 - ptr;
    int seclen;
    int version;
    int sec_num;
    int last_sec_num;
    int n;

    if (avail < 8 || ptr > 182 || sec[0] != 0x01 /* table_id CAT */) {
      /* not a CAT table start we understand (stuffing, split pointer,
       * etc) - forward unmodified rather than guess */
      memcpy(out_pkts[0], in_pkt, LDDCI_TS_SIZE);
      return 1;
    }

    seclen = (((sec[1] & 0x0F) << 8) | sec[2]) + 3; /* + table_id/length bytes */
    version      = (sec[5] >> 1) & 0x1F;
    sec_num      = sec[6];
    last_sec_num = sec[7];

    if (sec_num != 0 || last_sec_num != 0) {
      if (!ctx->ctx_cat_multi_warned) {
        ctx->ctx_cat_multi_warned = 1;
        tvherror(LS_DDCI,
                 "CAM %s: MTD ctx #%d sees a multi-section CAT (section "
                 "%d/%d) - EMM PID remap for this CAT is not supported "
                 "(same limitation VDR's own MTD implementation has); "
                 "forwarding it unmapped - entitlement/subscription "
                 "updates carried via this CAT's EMM PIDs may not reach "
                 "the CAM correctly while this CAM is in MTD mode",
                 lddci->lddci_id, ctx_id, sec_num, last_sec_num);
      }
      memcpy(out_pkts[0], in_pkt, LDDCI_TS_SIZE);
      return 1;
    }
    if (seclen < 8 || seclen > LDDCI_CAT_MAX_SECTION) {
      tvherror(LS_DDCI,
               "CAM %s: MTD ctx #%d CAT section length %d out of range, "
               "forwarding unmapped", lddci->lddci_id, ctx_id, seclen);
      memcpy(out_pkts[0], in_pkt, LDDCI_TS_SIZE);
      return 1;
    }

    /* VDR's cCaPidReceiver only rebuilds the CAT when its version changes.
     * For the common one-packet CAT case, replay our already rewritten
     * image verbatim on subsequent broadcasts of the same version. This
     * keeps the CAM-visible CAT/EMM namespace stable and avoids repeatedly
     * running the mapper in Tvheadend's per-packet descrambler path.
     * Multi-packet CATs continue through the reassembler below. */
    if (seclen <= avail && r->cache_valid &&
        r->cache_version == version && r->cache_npkts == 1) {
      memcpy(out_pkts[0], r->cache_pkts[0], LDDCI_TS_SIZE);
      return 1;
    }

    /* a new PUSI always starts a fresh section, even if one was already
     * in progress (e.g. a previous section that never completed) */
    r->npkts = 0;
    r->seclen = seclen;
    n = avail < seclen ? avail : seclen;
    memcpy(r->section, sec, n);
    r->have = n;
    memcpy(r->pkts[r->npkts++], in_pkt, LDDCI_TS_SIZE);

    if (r->have < r->seclen)
      return 0; /* wait for continuation packets */

  } else {
    const uint8_t *payload;
    int avail;
    int need;
    int n;

    if (r->npkts == 0 || r->have >= r->seclen) {
      /* nothing in progress - stray continuation packet, forward as-is */
      memcpy(out_pkts[0], in_pkt, LDDCI_TS_SIZE);
      return 1;
    }
    if (r->npkts >= LDDCI_CAT_MAX_PACKETS) {
      tvherror(LS_DDCI, "CAM %s: MTD ctx #%d CAT reassembly overflow, "
               "forwarding unmapped", lddci->lddci_id, ctx_id);
      r->npkts = 0;
      memcpy(out_pkts[0], in_pkt, LDDCI_TS_SIZE);
      return 1;
    }

    payload = in_pkt + 4;
    avail = LDDCI_TS_SIZE - 4;
    need = r->seclen - r->have;
    n = avail < need ? avail : need;

    if (r->have + n > LDDCI_CAT_MAX_SECTION) {
      tvherror(LS_DDCI, "CAM %s: MTD ctx #%d CAT section overflow, "
               "forwarding unmapped", lddci->lddci_id, ctx_id);
      r->npkts = 0;
      memcpy(out_pkts[0], in_pkt, LDDCI_TS_SIZE);
      return 1;
    }
    memcpy(r->section + r->have, payload, n);
    r->have += n;
    memcpy(r->pkts[r->npkts++], in_pkt, LDDCI_TS_SIZE);

    if (r->have < r->seclen)
      return 0; /* still waiting for more */
  }

  /* section complete - verify, remap, recompute CRC32, re-emit */
  {
    int i;
    int off;
    int n;
    linuxdvb_ddci_cat_emm_ctx_t emm_ctx = { lddci, ctx_id };
    uint8_t crc_scratch[LDDCI_CAT_MAX_SECTION];
    int crc_len;

    /* Verify the incoming CRC32 by recomputing it over section[0..seclen-4)
     * with dvb_table_append_crc32() and comparing against the trailing 4
     * bytes actually present, rather than relying on the "recompute over
     * the whole section including its own CRC32 and expect 0" self-check
     * property - that property does hold for this CRC variant, but
     * recompute-and-compare exercises the exact same tvheadend helper
     * used to generate the new CRC below, so there's only one CRC32
     * implementation in play here, not a second one just for verifying. */
    memcpy(crc_scratch, r->section, r->seclen - 4);
    crc_len = dvb_table_append_crc32(crc_scratch, r->seclen - 4, sizeof(crc_scratch));
    if (crc_len != r->seclen ||
        memcmp(crc_scratch + r->seclen - 4, r->section + r->seclen - 4, 4) != 0) {
      tvherror(LS_DDCI, "CAM %s: MTD ctx #%d received CAT with a bad "
               "CRC32, forwarding unmapped", lddci->lddci_id, ctx_id);
      for (i = 0; i < r->npkts; i++)
        memcpy(out_pkts[i], r->pkts[i], LDDCI_TS_SIZE);
      n = r->npkts;
      r->npkts = 0;
      return n;
    }

    /* remap the EMM PID inside each CA descriptor (tag 0x09) in the
     * descriptor loop, from byte 8 (after table_id..last_section_number)
     * up to seclen-4 (excluding the trailing CRC32) */
    if (dvb_ca_descriptors_foreach(r->section + 8, r->seclen - 4 - 8,
                                   linuxdvb_ddci_cat_emm_cb, &emm_ctx) < 0)
      tvhwarn(LS_DDCI, "CAM %s: MTD ctx #%d CAT descriptor loop malformed "
              "past the point already processed - some EMM PIDs may be "
              "left unmapped", lddci->lddci_id, ctx_id);

    /* recompute CRC32 over the modified section (excluding the old
     * trailing 4 CRC bytes) and patch it in, in place this time */
    dvb_table_append_crc32(r->section, r->seclen - 4, sizeof(r->section));

    /* write the modified section bytes back into the same TS packets we
     * buffered (same packet count, same boundaries/continuity counters
     * as received) */
    off = 0;
    for (i = 0; i < r->npkts; i++) {
      int hdr = (i == 0) ? (5 + r->pkts[0][4]) : 4;
      int room = LDDCI_TS_SIZE - hdr;
      int n2 = (r->seclen - off) < room ? (r->seclen - off) : room;
      memcpy(out_pkts[i], r->pkts[i], LDDCI_TS_SIZE);
      memcpy(out_pkts[i] + hdr, r->section + off, n2);
      off += n2;
    }
    n = r->npkts;
    if (n > 0 && n <= LDDCI_CAT_MAX_PACKETS) {
      int ci;
      r->cache_valid = 1;
      r->cache_version = (r->section[5] >> 1) & 0x1F;
      r->cache_npkts = n;
      for (ci = 0; ci < n; ci++)
        memcpy(r->cache_pkts[ci], out_pkts[ci], LDDCI_TS_SIZE);
    }
    r->npkts = 0;
    return n;
  }
}


/* Build one physical CAT from the latest VDR-style rewritten CAT image of
 * every active MTD context.  This is deliberately a broker rather than an
 * interleaver: PID 0x0001 has one continuity/version state, while its CA
 * descriptor loop advertises every context's already-mapped EMM PIDs.
 *
 * VDR establishes the important split we preserve here: CAT itself remains
 * PID 1, while EMM PID values inside CAT are mapped per virtual MTD slot.
 * Tvheadend's physical DDCI path differs in that interleaving several
 * independent CATs on PID 1 can confuse CAM-side CAT state.  The broker keeps
 * VDR's mapped-EMM semantics but presents them as one coherent physical CAT.
 *
 * Must be called with lddci_mux_lock held. */
static int
linuxdvb_ddci_cat_union_build_locked ( linuxdvb_ddci_t *lddci,
                                       uint8_t out_pkts[][LDDCI_TS_SIZE] )
{
  uint8_t sec[LDDCI_CAT_MAX_SECTION];
  int owner_ctx = linuxdvb_ddci_cat_emm_owner_ctx_locked(lddci);
  int owner_idx = linuxdvb_ddci_find_mux_ctx_id(lddci, owner_ctx);
  int i;
  int j;
  int n = 8;
  int npkts;
  int off;
  uint32_t h = 2166136261U;

  if (owner_idx < 0 || !lddci->lddci_mux_ctx[owner_idx].ctx_cat.cache_valid)
    return 0;

  /* Keep the syntax/header identity from the current owner, but version and
   * CRC are ours because the descriptor loop below is a synthesized union. */
  memcpy(sec, lddci->lddci_mux_ctx[owner_idx].ctx_cat.section, 8);
  sec[6] = 0; /* section_number */
  sec[7] = 0; /* last_section_number */

  /* Canonicalize the union by virtual slot rather than by the compactable
   * mux-context array.  Context removal swaps the last array element into
   * the hole; using array order would therefore bump the physical CAT
   * version even when its effective descriptor set had not changed. */
  for (j = 1; j <= LDDCI_MTD_SLOT_MAX; j++) {
    int ctx_idx = linuxdvb_ddci_find_mux_ctx_slot(lddci, j);
    const linuxdvb_ddci_cat_reasm_t *r;
    int pos;
    int end;
    if (ctx_idx < 0)
      continue;
    r = &lddci->lddci_mux_ctx[ctx_idx].ctx_cat;
    if (!r->cache_valid || r->seclen < 12)
      continue;
    end = r->seclen - 4; /* exclude CRC */
    for (pos = 8; pos + 1 < end; ) {
      int dlen = r->section[pos + 1];
      int dsz = dlen + 2;
      int duplicate = 0;
      if (pos + dsz > end)
        break;
      /* Exact-descriptor de-duplication.  Mapped EMM PIDs normally make
       * descriptors context-unique, but this also avoids needless repeats. */
      {
        int k;
        for (k = 8; k + dsz <= n; ) {
          int ksz = sec[k + 1] + 2;
          if (ksz <= 1 || k + ksz > n)
            break;
          if (ksz == dsz && !memcmp(sec + k, r->section + pos, dsz)) {
            duplicate = 1;
            break;
          }
          k += ksz;
        }
      }
      if (!duplicate) {
        if (n + dsz + 4 > (int)sizeof(sec)) {
          tvherror(LS_DDCI,
                   "CAM %s: synthesized MTD CAT exceeds %d bytes; keeping previous physical CAT",
                   lddci->lddci_id, LDDCI_CAT_MAX_SECTION);
          return 0;
        }
        memcpy(sec + n, r->section + pos, dsz);
        n += dsz;
      }
      pos += dsz;
    }
  }

  /* Hash only the union payload.  A topology/CAT change bumps the single
   * physical CAT version, exactly what a receiver expects for changed CAT
   * contents instead of unrelated same/different versions being interleaved. */
  for (i = 8; i < n; i++) {
    h ^= sec[i];
    h *= 16777619U;
  }
  if (!lddci->lddci_mtd_cat_union_valid || h != lddci->lddci_mtd_cat_union_hash) {
    lddci->lddci_mtd_cat_union_hash = h;
    lddci->lddci_mtd_cat_union_version =
      lddci->lddci_mtd_cat_union_valid ?
      (uint8_t)((lddci->lddci_mtd_cat_union_version + 1) & 0x1F) :
      (uint8_t)((sec[5] >> 1) & 0x1F);
    lddci->lddci_mtd_cat_union_valid = 1;
    tvhtrace(LS_DDCI,
              "MTD CAT broker cam=%s emitter_ctx#%d contexts=%d version=%u hash=%08X descriptors_bytes=%d",
              lddci->lddci_id, owner_ctx, lddci->lddci_mux_ctx_count,
              lddci->lddci_mtd_cat_union_version, h, n - 8);
  }
  sec[5] = (uint8_t)((sec[5] & 0xC1) |
                     ((lddci->lddci_mtd_cat_union_version & 0x1F) << 1));

  /* section_length counts bytes after the section_length field through CRC. */
  {
    int total = n + 4;
    int slen = total - 3;
    sec[1] = (uint8_t)((sec[1] & 0xF0) | ((slen >> 8) & 0x0F));
    sec[2] = (uint8_t)slen;
    n = dvb_table_append_crc32(sec, n, sizeof(sec));
    if (n != total)
      return 0;
  }

  npkts = n <= 183 ? 1 : 1 + (n - 183 + 183) / 184;
  /* first packet carries pointer_field + 183 section bytes; continuations 184 */
  if (npkts > LDDCI_CAT_MAX_PACKETS)
    return 0;
  off = 0;
  for (i = 0; i < npkts; i++) {
    int hdr;
    int room;
    int take;
    memset(out_pkts[i], 0xFF, LDDCI_TS_SIZE);
    out_pkts[i][0] = 0x47;
    out_pkts[i][1] = (uint8_t)(0x00 | (i == 0 ? 0x40 : 0x00));
    out_pkts[i][2] = 0x01;
    out_pkts[i][3] = (uint8_t)(0x10 | (lddci->lddci_mtd_cat_union_cc++ & 0x0F));
    hdr = 4;
    if (i == 0)
      out_pkts[i][hdr++] = 0x00; /* pointer_field */
    room = LDDCI_TS_SIZE - hdr;
    take = (n - off) < room ? (n - off) : room;
    memcpy(out_pkts[i] + hdr, sec + off, take);
    off += take;
  }
  return npkts;
}


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

  locked = ! tvh_mutex_trylock(&global_lock);
  mtimer_disarm(mti);
  if (locked)
    tvh_mutex_unlock(&global_lock);
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

  locked = ! tvh_mutex_trylock(&global_lock);
  mtimer_arm_rel(mti, callback, opaque, delta);
  if (locked)
    tvh_mutex_unlock(&global_lock);
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
  tvh_mutex_init(&ddci_thread->lddci_thread_lock, NULL);
  tvh_cond_init(&ddci_thread->lddci_thread_cond, 1);
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
    tvh_mutex_lock(&ddci_thread->lddci_thread_lock);
    tvh_thread_create(&ddci_thread->lddci_thread, NULL, thread_routine, arg, name);
    do {
      e = tvh_cond_wait(&ddci_thread->lddci_thread_cond,
                        &ddci_thread->lddci_thread_lock);
      if (e == ETIMEDOUT) {
        tvherror(LS_DDCI, "create thread %s error", name );
        break;
      }
    } while (ERRNO_AGAIN(e));
    tvh_mutex_unlock(&ddci_thread->lddci_thread_lock);
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
  int i;
  TAILQ_INIT(&ddci_snd_buf->lddci_send_buf_native);
  for (i = 0; i < LDDCI_MTD_RR_LANES; i++) {
    TAILQ_INIT(&ddci_snd_buf->lddci_send_lanes[i].queue);
    ddci_snd_buf->lddci_send_lanes[i].ctx_id = -1;
    ddci_snd_buf->lddci_send_lanes[i].bytes = 0;
  }
  ddci_snd_buf->lddci_send_rr_lane = -1;
  ddci_snd_buf->lddci_send_rr_burst = 0;
  ddci_snd_buf->lddci_send_buf_size_max = ddci_snd_buf_max;
  ddci_snd_buf->lddci_send_buf_size = 0;
  tvh_mutex_init(&ddci_snd_buf->lddci_send_buf_lock, NULL);
  tvh_cond_init(&ddci_snd_buf->lddci_send_buf_cond, 1);
  tvhlog_limit_reset(&ddci_snd_buf->lddci_send_buf_loglimit);
  ddci_snd_buf->lddci_send_buf_pkgCntW = 0;
  ddci_snd_buf->lddci_send_buf_pkgCntR = 0;
  ddci_snd_buf->lddci_send_buf_pkgCntWL = 0;
  ddci_snd_buf->lddci_send_buf_pkgCntRL = 0;

}

/* must be called with locked mutex */
static inline void
linuxdvb_ddci_send_buffer_remove
   ( linuxdvb_ddci_send_buffer_t *ddci_snd_buf, linuxdvb_ddci_send_packet_t *sp,
     linuxdvb_ddci_send_lane_t *lane, int native )
{
  if (sp) {
    assert(ddci_snd_buf->lddci_send_buf_size >= sp->lddci_send_pkt_len);
    ddci_snd_buf->lddci_send_buf_size -= sp->lddci_send_pkt_len;
    ddci_snd_buf->lddci_send_buf_pkgCntR += sp->lddci_send_pkt_len / LDDCI_TS_SIZE;
    if (native)
      TAILQ_REMOVE(&ddci_snd_buf->lddci_send_buf_native, sp, lddci_send_pkt_link);
    else {
      assert(lane != NULL && lane->bytes >= sp->lddci_send_pkt_len);
      lane->bytes -= sp->lddci_send_pkt_len;
      TAILQ_REMOVE(&lane->queue, sp, lddci_send_pkt_link);
    }
  }
}

static linuxdvb_ddci_send_packet_t *
linuxdvb_ddci_send_buffer_get
  ( linuxdvb_ddci_send_buffer_t *ddci_snd_buf, int64_t tmo )
{
  linuxdvb_ddci_send_packet_t *sp = NULL;
  linuxdvb_ddci_send_lane_t *lane = NULL;
  int native = 0;
  int i;
  int chosen = -1;

  tvh_mutex_lock(&ddci_snd_buf->lddci_send_buf_lock);

retry:
  /* Native/MCD traffic is not mixed with MTD in normal operation.  Preserve
   * strict FIFO semantics for it and use RR only for virtual contexts. */
  sp = TAILQ_FIRST(&ddci_snd_buf->lddci_send_buf_native);
  if (sp) {
    native = 1;
  } else {
    int cur = ddci_snd_buf->lddci_send_rr_lane;
    if (cur >= 0 && cur < LDDCI_MTD_RR_LANES &&
        ddci_snd_buf->lddci_send_rr_burst < LDDCI_MTD_RR_BURST_PKTS &&
        !TAILQ_EMPTY(&ddci_snd_buf->lddci_send_lanes[cur].queue)) {
      chosen = cur;
    } else {
      for (i = 1; i <= LDDCI_MTD_RR_LANES; i++) {
        int n = (cur + i + LDDCI_MTD_RR_LANES) % LDDCI_MTD_RR_LANES;
        if (!TAILQ_EMPTY(&ddci_snd_buf->lddci_send_lanes[n].queue)) {
          chosen = n;
          break;
        }
      }
      if (chosen >= 0) {
        if (chosen != cur)
          ddci_snd_buf->lddci_send_rr_burst = 0;
        ddci_snd_buf->lddci_send_rr_lane = chosen;
      }
    }
    if (chosen >= 0) {
      lane = &ddci_snd_buf->lddci_send_lanes[chosen];
      sp = TAILQ_FIRST(&lane->queue);
      ddci_snd_buf->lddci_send_rr_burst++;
    }
  }

  if (!sp) {
    int r;
    int64_t mono = mclk() + ms2mono(tmo);
    do {
      r = tvh_cond_timedwait(&ddci_snd_buf->lddci_send_buf_cond,
                             &ddci_snd_buf->lddci_send_buf_lock, mono);
      if (ddci_snd_buf->lddci_send_buf_size > 0)
        goto retry;
      if (r == ETIMEDOUT)
        break;
    } while (ERRNO_AGAIN(r));
  }

  linuxdvb_ddci_send_buffer_remove(ddci_snd_buf, sp, lane, native);
  tvh_mutex_unlock(&ddci_snd_buf->lddci_send_buf_lock);
  return sp;
}

static void
linuxdvb_ddci_send_buffer_put
  ( linuxdvb_ddci_send_buffer_t *ddci_snd_buf, const uint8_t *tsb, int len,
    int ctx_id )
{
  tvh_mutex_lock(&ddci_snd_buf->lddci_send_buf_lock);
  if (ddci_snd_buf->lddci_send_buf_size < ddci_snd_buf->lddci_send_buf_size_max) {
    linuxdvb_ddci_send_packet_t  *sp;

#if 0
    /* Note: This debug output will work only for one DD CI instance! */
    {
      static uint8_t pid_seen[ 8192];
      int pid;
      int idx = 0;

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
    if (!sp) {
      tvh_mutex_unlock(&ddci_snd_buf->lddci_send_buf_lock);
      return;
    }
    sp->lddci_send_pkt_len = len;
    sp->lddci_send_pkt_ctx_id = ctx_id;
    memcpy(sp->lddci_send_pkt_data, tsb, len);
    ddci_snd_buf->lddci_send_buf_size += len;
    ddci_snd_buf->lddci_send_buf_pkgCntW += len / LDDCI_TS_SIZE;
    // memoryinfo_alloc(&mpegts_input_queue_memoryinfo, sizeof(mpegts_packet_t) + len2);
    if (ctx_id < 0) {
      TAILQ_INSERT_TAIL(&ddci_snd_buf->lddci_send_buf_native, sp, lddci_send_pkt_link);
    } else {
      int i;
      int free_lane = -1;
      int lane_idx = -1;
      for (i = 0; i < LDDCI_MTD_RR_LANES; i++) {
        if (ddci_snd_buf->lddci_send_lanes[i].ctx_id == ctx_id) {
          lane_idx = i;
          break;
        }
        if (free_lane < 0 && ddci_snd_buf->lddci_send_lanes[i].ctx_id < 0)
          free_lane = i;
      }
      if (lane_idx < 0) lane_idx = free_lane;
      if (lane_idx < 0) {
        /* No queue lane is available for this MTD context. */
        ddci_snd_buf->lddci_send_buf_size -= len;
        ddci_snd_buf->lddci_send_buf_pkgCntW -= len / LDDCI_TS_SIZE;
        free(sp);
        tvh_mutex_unlock(&ddci_snd_buf->lddci_send_buf_lock);
        return;
      }
      ddci_snd_buf->lddci_send_lanes[lane_idx].ctx_id = ctx_id;
      ddci_snd_buf->lddci_send_lanes[lane_idx].bytes += len;
      TAILQ_INSERT_TAIL(&ddci_snd_buf->lddci_send_lanes[lane_idx].queue, sp, lddci_send_pkt_link);
    }
    tvh_cond_signal(&ddci_snd_buf->lddci_send_buf_cond, 0);
  } else {
    if (tvhlog_limit(&ddci_snd_buf->lddci_send_buf_loglimit, 10))
      tvhwarn(LS_DDCI, "too much queued output data in send buffer, discarding new");
  }

  tvh_mutex_unlock(&ddci_snd_buf->lddci_send_buf_lock);
}

static void
linuxdvb_ddci_send_buffer_clear ( linuxdvb_ddci_send_buffer_t *ddci_snd_buf )
{
  linuxdvb_ddci_send_packet_t  *sp;
  linuxdvb_ddci_send_packet_t  *next;

  tvh_mutex_lock(&ddci_snd_buf->lddci_send_buf_lock);

  for (sp = TAILQ_FIRST(&ddci_snd_buf->lddci_send_buf_native);
       sp; sp = next) {
    next = TAILQ_NEXT(sp, lddci_send_pkt_link);
    linuxdvb_ddci_send_buffer_remove(ddci_snd_buf, sp, NULL, 1);
    free(sp);
  }
  {
    int i;
    for (i = 0; i < LDDCI_MTD_RR_LANES; i++) {
      linuxdvb_ddci_send_lane_t *lane = &ddci_snd_buf->lddci_send_lanes[i];
      for (sp = TAILQ_FIRST(&lane->queue); sp; sp = next) {
        next = TAILQ_NEXT(sp, lddci_send_pkt_link);
        linuxdvb_ddci_send_buffer_remove(ddci_snd_buf, sp, lane, 0);
        free(sp);
      }
      lane->ctx_id = -1;
      lane->bytes = 0;
    }
    ddci_snd_buf->lddci_send_rr_lane = -1;
    ddci_snd_buf->lddci_send_rr_burst = 0;
  }
  ddci_snd_buf->lddci_send_buf_pkgCntW = 0;
  ddci_snd_buf->lddci_send_buf_pkgCntR = 0;
  ddci_snd_buf->lddci_send_buf_pkgCntWL = 0;
  ddci_snd_buf->lddci_send_buf_pkgCntRL = 0;

  tvh_mutex_unlock(&ddci_snd_buf->lddci_send_buf_lock);
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

  tvh_mutex_lock(&ddci_wr_thread->lddci_thread_lock);
  linuxdvb_ddci_thread_stop(LDDCI_TO_THREAD(ddci_wr_thread));
  linuxdvb_ddci_send_buffer_clear(&ddci_wr_thread->lddci_send_buffer);
  tvh_mutex_unlock(&ddci_wr_thread->lddci_thread_lock);
}

static inline void
linuxdvb_ddci_wr_thread_buffer_put_ctx
  ( linuxdvb_ddci_wr_thread_t *ddci_wr_thread, const uint8_t *tsb, int len,
    int ctx_id )
{
  /* Lock against linuxdvb_ddci_wr_thread_stop.  For MTD, split a producer
   * callback into deliberately small packet-aligned queue items.  The write
   * thread can then alternate queued contexts rather than draining one large
   * mux-local batch before considering another transponder. */
  tvh_mutex_lock(&ddci_wr_thread->lddci_thread_lock);
  if (linuxdvb_ddci_thread_running(LDDCI_TO_THREAD(ddci_wr_thread))) {
    if (ctx_id >= 0) {
      const int burst = LDDCI_MTD_FAIR_BURST_PKTS * LDDCI_TS_SIZE;
      while (len > 0) {
        int n = len > burst ? burst : len;
        n -= n % LDDCI_TS_SIZE;
        if (n <= 0)
          break;
        linuxdvb_ddci_send_buffer_put(&ddci_wr_thread->lddci_send_buffer,
                                      tsb, n, ctx_id);
        tsb += n;
        len -= n;
      }
    } else {
      linuxdvb_ddci_send_buffer_put(&ddci_wr_thread->lddci_send_buffer,
                                    tsb, len, -1);
    }
  }
  tvh_mutex_unlock(&ddci_wr_thread->lddci_thread_lock);
}

static inline void
linuxdvb_ddci_wr_thread_buffer_put
  ( linuxdvb_ddci_wr_thread_t *ddci_wr_thread, const uint8_t *tsb, int len )
{
  linuxdvb_ddci_wr_thread_buffer_put_ctx(ddci_wr_thread, tsb, len, -1);
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

static inline int
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
    int nfds;
    int num_pkg;
    int pkg_chk = 0;
    int scrambled = 0;
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
      int len;
      int skip;
      const uint8_t *tsb;

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

      linuxdvb_ddci_t *lddci = ddci_thread->lddci;

      struct ddci_ctx_snapshot {
        mpegts_mux_t *mux;
        mpegts_input_t *input;
        int ctx_id;
        int ctx_slot;
      };
      struct ddci_ctx_snapshot *ctx_snapshot = NULL;
      int ctx_count;
      int i;

      tvh_mutex_lock(&lddci->lddci_mux_lock);
      ctx_count = lddci->lddci_mux_ctx_count;
      if (ctx_count > 0)
        ctx_snapshot = malloc(ctx_count * sizeof(*ctx_snapshot));
      if (ctx_snapshot != NULL)
        for (i = 0; i < ctx_count; i++) {
          ctx_snapshot[i].mux = lddci->lddci_mux_ctx[i].ctx_mux;
          ctx_snapshot[i].input = lddci->lddci_mux_ctx[i].ctx_input;
          ctx_snapshot[i].ctx_id = lddci->lddci_mux_ctx[i].ctx_id;
          ctx_snapshot[i].ctx_slot = lddci->lddci_mux_ctx[i].ctx_slot;
        }
      tvh_mutex_unlock(&lddci->lddci_mux_lock);

      if (ctx_count > 0 && ctx_snapshot == NULL) {
        tvherror(LS_DDCI, "CAM %s: out of memory snapshotting %d MTD contexts",
                 lddci->lddci_id, ctx_count);
      } else {
        uint8_t stackbuf[LDDCI_TS_SIZE * 64];

        for (i = 0; i < ctx_count; i++) {
          int ctx_id = ctx_snapshot[i].ctx_id;
          int ctx_slot = ctx_snapshot[i].ctx_slot;
          uint8_t *buf = (len <= (int)sizeof(stackbuf)) ? stackbuf : malloc(len);
          int off;
          int out_len = 0;

          if (!buf) {
            tvherror(LS_DDCI, "CAM %s: MTD ctx #%d slot %d out of memory demuxing "
                     "%d bytes, dropping this buffer for this context",
                     lddci->lddci_id, ctx_id, ctx_slot, len);
            continue;
          }

          tvh_mutex_lock(&lddci->lddci_mux_lock);
          for (off = 0; off + LDDCI_TS_SIZE <= len; off += LDDCI_TS_SIZE) {
            const uint8_t *pkt = tsb + off;
            uint16_t pid = linuxdvb_ddci_ts_get_pid(pkt);
            uint16_t real_pid;

            /* CAT is the one deliberately shared fixed PID. Like VDR, do
             * not feed the CAM-returned CAT back into a real mux. NULL is
             * also irrelevant to post-demux. */
            if (pid == 1 || pid == 0x1FFF)
              continue;
            if ((pid >> 8) != ctx_slot)
              continue;

            real_pid = linuxdvb_ddci_unmap_pid(lddci, ctx_id, pid);
            if (real_pid == 0xFFFF)
              continue;

            memcpy(buf + out_len, pkt, LDDCI_TS_SIZE);
            linuxdvb_ddci_ts_set_pid(buf + out_len, real_pid);
            out_len += LDDCI_TS_SIZE;
          }
          tvh_mutex_unlock(&lddci->lddci_mux_lock);

          if (out_len)
            mpegts_input_postdemux(ctx_snapshot[i].input, ctx_snapshot[i].mux, buf, out_len);
          if (buf != stackbuf)
            free(buf);
        }
      }
      free(ctx_snapshot);

      ddci_rd_thread->lddci_recv_pkgCntW += num_pkg;

      /* Do not call linuxdvb_ddci_rd_thread_statistic() directly here.
       * It is an mtimer callback and asserts that global_lock is held.
       * The DDCI read thread does not hold global_lock.  The periodic timer
       * will publish the counters normally. */

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
  tvh_mutex_init(&lddci->lddci_mux_lock, NULL);

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

/* --------------------------------------------------------------------
 * MTD mapping API used by CA-PMT construction.
 * ------------------------------------------------------------------ */

/* Returns this service's stable mux ctx_id, or -1 if it is not assigned here.
 * All contexts are symmetrically mapped; ctx_id is no longer a PID namespace. */
int
linuxdvb_ddci_mtd_ctx_for_service ( linuxdvb_ddci_t *lddci, service_t *t )
{
  int svc_idx;
  int ctx_id;

  if (lddci == NULL)
    return -1;

  tvh_mutex_lock(&lddci->lddci_mux_lock);
  svc_idx = linuxdvb_ddci_find_svc_ctx(lddci, t);
  ctx_id = svc_idx >= 0 ? lddci->lddci_svc_ctx[svc_idx].svc_ctx_id : -1;
  tvh_mutex_unlock(&lddci->lddci_mux_lock);

  return ctx_id;
}

void
linuxdvb_ddci_mtd_arm_eit ( linuxdvb_ddci_t *lddci, service_t *t,
                                uint8_t list_management, uint16_t mapped_sid )
{
  int svc_idx;
  if (lddci == NULL || t == NULL)
    return;
  tvh_mutex_lock(&lddci->lddci_mux_lock);
  svc_idx = linuxdvb_ddci_find_svc_ctx(lddci, t);
  if (svc_idx >= 0 && list_management != 0x05) { /* not DELETE */
    linuxdvb_ddci_svc_ctx_t *svc = &lddci->lddci_svc_ctx[svc_idx];
    svc->svc_mtd_eit_sid = mapped_sid;
    svc->svc_mtd_eit_start_mono = mclk();
    svc->svc_mtd_eit_last_mono = 0;
    svc->svc_mtd_eit_sent = 0;
    svc->svc_mtd_eit_version = (svc->svc_mtd_eit_version + 1) & 0x1f;
  }
  tvh_mutex_unlock(&lddci->lddci_mux_lock);
}

/* Allocate (or return) this service's PID in its mux slot namespace.
 * Every context, including ctx #0, is mapped symmetrically. */
uint16_t
linuxdvb_ddci_mtd_map_pid ( linuxdvb_ddci_t *lddci, service_t *t,
                            uint16_t real_pid )
{
  uint16_t r;
  int svc_idx;

  if (lddci == NULL || t == NULL)
    return real_pid;

  tvh_mutex_lock(&lddci->lddci_mux_lock);
  svc_idx = linuxdvb_ddci_find_svc_ctx(lddci, t);
  r = svc_idx >= 0 ?
      linuxdvb_ddci_map_pid_for_service_locked(lddci, svc_idx, real_pid) : 0;
  if (svc_idx >= 0 && r <= LDDCI_MTD_PID_LAST)
    linuxdvb_ddci_pidbit_set(lddci->lddci_svc_ctx[svc_idx].svc_cam_pids, r);
  tvh_mutex_unlock(&lddci->lddci_mux_lock);

  return r;
}

/* Same as above, for the CA-PMT's service_id field. Locks internally. */
uint16_t
linuxdvb_ddci_mtd_map_sid ( linuxdvb_ddci_t *lddci, int ctx_id, uint16_t real_sid )
{
  uint16_t r;

  if (lddci == NULL || ctx_id < 0)
    return real_sid;

  tvh_mutex_lock(&lddci->lddci_mux_lock);
  r = linuxdvb_ddci_map_sid(lddci, ctx_id, real_sid);
  tvh_mutex_unlock(&lddci->lddci_mux_lock);

  return r;
}

/* CAM input PID ownership is a mux property. Multiple services on one mux
 * may receive the same TS batch, so filter against the union of CA-PMT PIDs
 * for every service sharing the MTD context. */
/* Generate a minimal present/following EIT for the mapped service during CAM
 * startup. PID 0x0012 remains a system PID; only the service_id is translated
 * into the MTD namespace. */
static int
linuxdvb_ddci_mtd_make_eit ( uint8_t pkt[LDDCI_TS_SIZE], uint16_t mapped_sid,
                              uint8_t version, uint8_t cc )
{
  uint8_t sec[64];
  int n = 0;
  int slen;

  memset(pkt, 0xff, LDDCI_TS_SIZE);
  pkt[0] = LDDCI_TS_SYNC_BYTE;
  pkt[1] = 0x40;                    /* PUSI, PID high bits are zero */
  pkt[2] = 0x12;                    /* EIT PID */
  pkt[3] = 0x10 | (cc & 0x0f);
  pkt[4] = 0x00;                    /* pointer field */

  sec[n++] = 0x4e;                 /* present/following, actual TS */
  sec[n++] = 0xf0;                 /* section_length filled below */
  sec[n++] = 0x00;
  sec[n++] = mapped_sid >> 8;
  sec[n++] = mapped_sid & 0xff;
  sec[n++] = 0xc1 | ((version & 0x1f) << 1);
  sec[n++] = 0x00;                 /* section_number */
  sec[n++] = 0x00;                 /* last_section_number */
  sec[n++] = 0x00; sec[n++] = 0x00; /* transport_stream_id */
  sec[n++] = 0x00; sec[n++] = 0x00; /* original_network_id */
  sec[n++] = 0x00;                 /* segment_last_section_number */
  sec[n++] = 0x4e;                 /* last_table_id */

  sec[n++] = 0x00; sec[n++] = 0x01; /* event_id */
  sec[n++] = 0xff; sec[n++] = 0xff; sec[n++] = 0xff;
  sec[n++] = 0xff; sec[n++] = 0xff; /* undefined start_time */
  sec[n++] = 0x01; sec[n++] = 0x00; sec[n++] = 0x00; /* 01:00:00 */
  sec[n++] = 0x80;                 /* running_status=4, free_CA=0, len hi */
  sec[n++] = 0x06;                 /* descriptors_loop_length */
  sec[n++] = 0x55;                 /* parental_rating_descriptor */
  sec[n++] = 0x04;
  sec[n++] = '9'; sec[n++] = '0'; sec[n++] = '2'; sec[n++] = 0x00;

  slen = (n - 3) + 4;              /* bytes after section_length + CRC */
  sec[1] |= (slen >> 8) & 0x0f;
  sec[2] = slen & 0xff;
  n = dvb_table_append_crc32(sec, n, sizeof(sec));
  if (n < 0 || n > LDDCI_TS_SIZE - 5)
    return 0;
  memcpy(pkt + 5, sec, n);
  return 1;
}

static int
linuxdvb_ddci_mtd_eit_due_locked ( linuxdvb_ddci_t *lddci, int svc_idx,
                                    uint8_t pkt[LDDCI_TS_SIZE] )
{
  linuxdvb_ddci_svc_ctx_t *svc;
  int64_t now;

  if (svc_idx < 0 || svc_idx >= lddci->lddci_svc_ctx_count)
    return 0;
  svc = &lddci->lddci_svc_ctx[svc_idx];
  if (!svc->svc_mtd_eit_start_mono || !svc->svc_mtd_eit_sid ||
      svc->svc_mtd_eit_sent >= 10)
    return 0;
  now = mclk();
  if (now - svc->svc_mtd_eit_start_mono >= sec2mono(10))
    return 0;
  if (svc->svc_mtd_eit_last_mono &&
      now - svc->svc_mtd_eit_last_mono < sec2mono(1))
    return 0;
  if (!linuxdvb_ddci_mtd_make_eit(pkt, svc->svc_mtd_eit_sid,
                                   svc->svc_mtd_eit_version,
                                   svc->svc_mtd_eit_cc++))
    return 0;
  svc->svc_mtd_eit_last_mono = now;
  svc->svc_mtd_eit_sent++;
  return 1;
}

static int
linuxdvb_ddci_ctx_owns_cam_pid_locked ( linuxdvb_ddci_t *lddci, int ctx_id,
                                        uint16_t cam_pid )
{
  int i;

  if (cam_pid > LDDCI_MTD_PID_LAST)
    return 0;
  for (i = 0; i < lddci->lddci_svc_ctx_count; i++) {
    linuxdvb_ddci_svc_ctx_t *svc = &lddci->lddci_svc_ctx[i];
    if (svc->svc_ctx_id == ctx_id &&
        linuxdvb_ddci_pidbit_test(svc->svc_cam_pids, cam_pid))
      return 1;
  }
  return 0;
}

void
linuxdvb_ddci_put
  ( linuxdvb_ddci_t *lddci, service_t *t, const uint8_t *tsb, int len )
{
  int svc_idx;
  int ctx_id;

  /* Suppress duplicate callbacks only within the same MTD mux context.
   * The same TS buffer address may be reused by another context. */
  tvh_mutex_lock(&lddci->lddci_mux_lock);
  svc_idx = linuxdvb_ddci_find_svc_ctx(lddci, t);
  ctx_id = svc_idx >= 0 ? lddci->lddci_svc_ctx[svc_idx].svc_ctx_id : -1;

  if (lddci->lddci_prev_tsb == tsb &&
      lddci->lddci_prev_ctx_id == ctx_id) {
    tvh_mutex_unlock(&lddci->lddci_mux_lock);
    return;
  }
  lddci->lddci_prev_tsb = tsb;
  lddci->lddci_prev_ctx_id = ctx_id;

  /* Teardown race: preserve native behaviour. */
  if (svc_idx < 0 || ctx_id < 0) {
    tvh_mutex_unlock(&lddci->lddci_mux_lock);
    linuxdvb_ddci_wr_thread_buffer_put(&lddci->lddci_wr_thread, tsb, len);
    return;
  }

  /* Fixed DVB PSI/SI tables contain identifiers from the original mux and
   * must not be forwarded unchanged into the virtual MTD namespace. CAT is
   * synthesized separately, and a mapped-service EIT is generated at startup. */
  {
    uint8_t scratch[LDDCI_TS_SIZE * (LDDCI_CAT_MAX_PACKETS + 1)];
    uint8_t *out = scratch;
    size_t out_cap = sizeof(scratch);
    size_t out_len = 0;
    int out_heap = 0;
    int oom = 0;
    int off;
    uint8_t eit_pkt[LDDCI_TS_SIZE];

    /* VDR starts EIT injection when the encrypted receiver is attached and
     * sends one packet per second for ten seconds.  Put the synthetic EIT
     * ahead of this service's next natural CAM payload when it is due. */
    if (linuxdvb_ddci_mtd_eit_due_locked(lddci, svc_idx, eit_pkt)) {
      memcpy(out + out_len, eit_pkt, LDDCI_TS_SIZE);
      out_len += LDDCI_TS_SIZE;
    }

    for (off = 0; off + LDDCI_TS_SIZE <= len; off += LDDCI_TS_SIZE) {
      const uint8_t *pkt = tsb + off;
      uint16_t pid = linuxdvb_ddci_ts_get_pid(pkt);

      if (pid == 0x0000 || /* PAT */
          pid == 0x0010 || /* NIT/ST */
          pid == 0x0011 || /* SDT/BAT */
          pid == 0x0012 || /* EIT - VDR injects a mapped-SID EIT instead */
          pid == 0x0013 || /* RST */
          pid == 0x0014) { /* TDT/TOT */
        continue;
      } else if (pid == 1 /* DVB_CAT_PID */) {
        uint8_t cat_out[LDDCI_CAT_MAX_PACKETS][LDDCI_TS_SIZE];
        int n;
        n = linuxdvb_ddci_cat_process(lddci, ctx_id, pkt, cat_out);
        int i;

        /* Parse every virtual CAT first so linuxdvb_ddci_cat_process() keeps
         * that context's mapped EMM namespace current.  Never interleave those
         * virtual CAT packets directly.  The physical owner CAT cadence emits
         * one synthesized union CAT instead. */
        if (ctx_id == linuxdvb_ddci_cat_emm_owner_ctx_locked(lddci) && n > 0) {
          n = linuxdvb_ddci_cat_union_build_locked(lddci, cat_out);
        } else {
          n = 0;
        }
        for (i = 0; i < n; i++) {
          if (out_len + LDDCI_TS_SIZE > out_cap) {
            size_t new_cap = out_cap * 2;
            uint8_t *n2 = malloc(new_cap);
            if (!n2) { oom = 1; break; }
            memcpy(n2, out, out_len);
            if (out_heap) free(out);
            out = n2; out_cap = new_cap; out_heap = 1;
          }
          memcpy(out + out_len, cat_out[i], LDDCI_TS_SIZE);
          out_len += LDDCI_TS_SIZE;
        }
        if (oom)
          break;
      } else if (pid == 0x1FFF) {
        /* NULL packets carry no useful CAM payload and 0x1FFF is reserved. */
        continue;
      } else {
        int ctx_idx = linuxdvb_ddci_find_mux_ctx_id(lddci, ctx_id);
        linuxdvb_ddci_mux_ctx_t *dctx = ctx_idx >= 0 ?
          &lddci->lddci_mux_ctx[ctx_idx] : NULL;
        uint16_t uniq_pid = 0;

        /* VDR's virtual CAM only feeds the physical CAM the PIDs that are
         * part of the decrypt program (plus CAT/EMM handling).  Do not create
         * a new synthetic PID merely because Tvheadend happened to deliver
         * another PID belonging to the service.  The CA-PMT mapper has already
         * allocated and marked every ECM/ES PID the CAM was told about. */
        if (dctx != NULL && pid <= LDDCI_MTD_PID_LAST)
          uniq_pid = dctx->ctx_pidmap.map_r2u[pid];

        /* EMM PIDs are mux-pinned by CAT processing, not CA-PMT-referenced.
         * This is an important part of VDR's MTD contract: the CAT is sent to
         * the CAM on fixed PID 1 with the EMM PID values rewritten, and the
         * corresponding EMM TS packets are then sent on those rewritten PIDs.
         * Do this before the per-service CA-PMT filter, because EMM PIDs are
         * intentionally not members of any individual service CA-PMT. */
        if (dctx != NULL && uniq_pid != 0 &&
            linuxdvb_ddci_pidbit_test(dctx->ctx_pidmap.map_pinned, pid)) {
          if (out_len + LDDCI_TS_SIZE > out_cap) {
            size_t new_cap = out_cap * 2;
            uint8_t *n2 = malloc(new_cap);
            if (!n2) { oom = 1; break; }
            memcpy(n2, out, out_len);
            if (out_heap) free(out);
            out = n2; out_cap = new_cap; out_heap = 1;
          }
          memcpy(out + out_len, pkt, LDDCI_TS_SIZE);
          linuxdvb_ddci_ts_set_pid(out + out_len, uniq_pid);
          out_len += LDDCI_TS_SIZE;
          continue;
        }

        if (uniq_pid == 0 ||
            !linuxdvb_ddci_ctx_owns_cam_pid_locked(lddci, ctx_id, uniq_pid))
          continue;

        if (out_len + LDDCI_TS_SIZE > out_cap) {
          size_t new_cap = out_cap * 2;
          uint8_t *n2 = malloc(new_cap);
          if (!n2) { oom = 1; break; }
          memcpy(n2, out, out_len);
          if (out_heap) free(out);
          out = n2; out_cap = new_cap; out_heap = 1;
        }

        memcpy(out + out_len, pkt, LDDCI_TS_SIZE);
        linuxdvb_ddci_ts_set_pid(out + out_len, uniq_pid);
        out_len += LDDCI_TS_SIZE;
      }
    }

    tvh_mutex_unlock(&lddci->lddci_mux_lock);

    if (oom)
      tvherror(LS_DDCI, "CAM %s: out of memory growing MTD output buffer; "
               "dropping remainder of this input buffer", lddci->lddci_id);
    if (out_len)
      linuxdvb_ddci_wr_thread_buffer_put_ctx(&lddci->lddci_wr_thread, out,
                                              out_len, ctx_id);
    if (out_heap)
      free(out);
  }
}

void
linuxdvb_ddci_assign ( linuxdvb_ddci_t *lddci, service_t *t, int svc_limit )
{
  mpegts_service_t *s = (mpegts_service_t *)t;
  int ctx_idx;
  int ctx_id;
  int svc_idx;

  tvh_mutex_lock(&lddci->lddci_mux_lock);


  /* The dynamic service-context table is the authoritative assignment state. */
  if (linuxdvb_ddci_find_svc_ctx(lddci, t) >= 0) {
    tvh_mutex_unlock(&lddci->lddci_mux_lock);
    return;
  }

  /* Defensive duplicate of dvbcam admission control: the user-configured
   * Service limit is the only service-count ceiling. 0 means unlimited. */
  if (svc_limit > 0 && lddci->lddci_svc_ctx_count >= svc_limit) {
    tvherror(LS_DDCI, "CAM %s: configured Service limit %d reached, "
             "refusing to assign %p", lddci->lddci_id, svc_limit, t);
    tvh_mutex_unlock(&lddci->lddci_mux_lock);
    return;
  }

  if (linuxdvb_ddci_ensure_svc_capacity(
        lddci, lddci->lddci_svc_ctx_count + 1) < 0) {
    tvherror(LS_DDCI, "CAM %s: out of memory growing MTD service table",
             lddci->lddci_id);
    tvh_mutex_unlock(&lddci->lddci_mux_lock);
    return;
  }

  ctx_idx = linuxdvb_ddci_find_mux_ctx(lddci, s->s_dvb_mux);
  if (ctx_idx < 0) {
    linuxdvb_ddci_mux_ctx_t *ctx;
    if (linuxdvb_ddci_ensure_mux_capacity(
          lddci, lddci->lddci_mux_ctx_count + 1) < 0) {
      tvherror(LS_DDCI, "CAM %s: out of memory growing MTD mux table",
               lddci->lddci_id);
      tvh_mutex_unlock(&lddci->lddci_mux_lock);
      return;
    }
    {
      int slot = linuxdvb_ddci_alloc_mux_slot_locked(lddci);
      if (slot < 0) {
        tvherror(LS_DDCI,
                 "CAM %s: all %d VDR-style MTD PID slots are active; refusing mux \"%s\"",
                 lddci->lddci_id, LDDCI_MTD_SLOT_MAX,
                 s->s_dvb_mux ? s->s_dvb_mux->mm_nicename : "?");
        tvh_mutex_unlock(&lddci->lddci_mux_lock);
        return;
      }
      ctx_idx = lddci->lddci_mux_ctx_count++;
      ctx = &lddci->lddci_mux_ctx[ctx_idx];
      memset(ctx, 0, sizeof(*ctx));
      ctx->ctx_id = lddci->lddci_next_ctx_id++;
      ctx->ctx_slot = slot;
    }
    ctx->ctx_mux = s->s_dvb_mux;
    ctx->ctx_input = s->s_dvb_active_input;
    ctx_id = ctx->ctx_id;
    tvhnotice(LS_DDCI,
              "CAM %s: MTD new mux context #%d slot %d for mux \"%s\" (%s)",
              lddci->lddci_id, ctx_id, ctx->ctx_slot,
              s->s_dvb_mux ? s->s_dvb_mux->mm_nicename : "?",
              lddci->lddci_mux_ctx_count > 1 ? "MTD active" : "single mux");
  } else {
    ctx_id = lddci->lddci_mux_ctx[ctx_idx].ctx_id;
  }

  svc_idx = lddci->lddci_svc_ctx_count++;
  memset(&lddci->lddci_svc_ctx[svc_idx], 0, sizeof(lddci->lddci_svc_ctx[svc_idx]));
  lddci->lddci_svc_ctx[svc_idx].svc_t = t;
  lddci->lddci_svc_ctx[svc_idx].svc_ctx_id = ctx_id;
  lddci->lddci_mux_ctx[ctx_idx].ctx_refcount++;

  tvhnotice(LS_DDCI,
            "CAM %s: assigned %p to mux context #%d slot %d \"%s\" "
            "(%d muxes, %d services on this ctx)",
            lddci->lddci_id, t, ctx_id, lddci->lddci_mux_ctx[ctx_idx].ctx_slot,
            s->s_dvb_mux ? s->s_dvb_mux->mm_nicename : "?",
            lddci->lddci_mux_ctx_count,
            lddci->lddci_mux_ctx[ctx_idx].ctx_refcount);

  tvh_mutex_unlock(&lddci->lddci_mux_lock);
}

void
linuxdvb_ddci_unassign ( linuxdvb_ddci_t *lddci, service_t *t )
{
  int svc_idx;
  int ctx_idx;
  int ctx_id;
  int idle;

  tvh_mutex_lock(&lddci->lddci_mux_lock);

  svc_idx = linuxdvb_ddci_find_svc_ctx(lddci, t);
  if (svc_idx >= 0) {
    tvhnotice(LS_DDCI, "CAM %s unassigned from %p", lddci->lddci_id, t );

    ctx_id = lddci->lddci_svc_ctx[svc_idx].svc_ctx_id;
    lddci->lddci_svc_ctx_count--;
    if (svc_idx != lddci->lddci_svc_ctx_count)
      lddci->lddci_svc_ctx[svc_idx] =
        lddci->lddci_svc_ctx[lddci->lddci_svc_ctx_count];

    ctx_idx = linuxdvb_ddci_find_mux_ctx_id(lddci, ctx_id);
    if (ctx_idx >= 0) {
      linuxdvb_ddci_mux_ctx_t *ctx = &lddci->lddci_mux_ctx[ctx_idx];
      ctx->ctx_refcount--;
      tvhtrace(LS_DDCI, "CAM %s: mux context #%d now has %d service(s)",
               lddci->lddci_id, ctx_id, ctx->ctx_refcount);
      if (ctx->ctx_refcount <= 0) {
        int slot = ctx->ctx_slot;
        int64_t now = mclk();
        tvhnotice(LS_DDCI, "CAM %s: MTD mux context #%d \"%s\" closed",
                  lddci->lddci_id, ctx_id,
                  ctx->ctx_mux ? ctx->ctx_mux->mm_nicename : "?");
        if (slot >= 1 && slot <= LDDCI_MTD_SLOT_MAX) {
          lddci->lddci_slot_quarantine_until[slot] =
            now + ms2mono(LDDCI_MTD_SLOT_QUARANTINE_MS);
        }
        linuxdvb_ddci_release_ctx_maps(lddci, ctx_id);
        lddci->lddci_mux_ctx[ctx_idx] =
          lddci->lddci_mux_ctx[--lddci->lddci_mux_ctx_count];
      }
    }
  }

  idle = lddci->lddci_svc_ctx_count == 0;
  if (idle) {
    /* No dynamic services remain.  At this point all mux contexts should also
     * be gone; clean only genuinely stale context state. */
    if (lddci->lddci_mux_ctx_count != 0) {
      int i;
      tvherror(LS_DDCI,
               "CAM %s: MTD context mismatch at idle: muxctx=%d svcctx=0; cleaning stale mux state",
               lddci->lddci_id, lddci->lddci_mux_ctx_count);
      for (i = 0; i < lddci->lddci_mux_ctx_count; i++)
        linuxdvb_ddci_release_ctx_maps(lddci, lddci->lddci_mux_ctx[i].ctx_id);
      lddci->lddci_mux_ctx_count = 0;
    }
    free(lddci->lddci_mux_ctx);
    free(lddci->lddci_svc_ctx);
    lddci->lddci_mux_ctx = NULL;
    lddci->lddci_svc_ctx = NULL;
    lddci->lddci_mux_ctx_alloc = 0;
    lddci->lddci_svc_ctx_alloc = 0;
    lddci->lddci_next_ctx_id = 0;
    /* A completely idle CAM starts the next MTD session with a fresh
     * physical CAT state.  Do not carry hash/version/continuity state across
     * an empty interval where the CAM may itself have reset PSI state. */
    lddci->lddci_mtd_cat_union_hash = 0;
    lddci->lddci_mtd_cat_union_valid = 0;
    lddci->lddci_mtd_cat_union_version = 0;
    lddci->lddci_mtd_cat_union_cc = 0;
    memset(lddci->lddci_pid_owner, 0, sizeof(lddci->lddci_pid_owner));
    memset(lddci->lddci_pid_u2r, 0, sizeof(lddci->lddci_pid_u2r));
  }

  tvh_mutex_unlock(&lddci->lddci_mux_lock);

  if (idle)
    linuxdvb_ddci_wr_thread_statistic_clr(&lddci->lddci_wr_thread);
}

int
linuxdvb_ddci_do_not_assign ( linuxdvb_ddci_t *lddci, service_t *t, int multi,
                              int svc_limit )
{
  (void)t;
  tvh_mutex_lock(&lddci->lddci_mux_lock);

  if (lddci->lddci_svc_ctx_count == 0) {
    tvh_mutex_unlock(&lddci->lddci_mux_lock);
    return 0;
  }

  if (!multi) {
    tvh_mutex_unlock(&lddci->lddci_mux_lock);
    return 1;
  }

  if (svc_limit > 0 && lddci->lddci_svc_ctx_count >= svc_limit) {
    tvh_mutex_unlock(&lddci->lddci_mux_lock);
    return 1;
  }

  tvh_mutex_unlock(&lddci->lddci_mux_lock);
  return 0;
}
