/*
 *  TV headend - Timeshift - clients timeshifting through the channel cache
 *  Copyright (C) 2026 Vincent Fortier
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
 * With "Record from cache" each tuned channel already keeps its last
 * "Maximum period" of MPEG-TS (timeshift_svcbuf.c).  A client pausing or
 * rewinding live TV is served from that cache here, instead of writing a
 * buffer of its own -- one cache per channel, however many clients.
 *
 * This target takes the place of timeshift_create() in the client's chain,
 * after tsfix and the global headers, and answers the same speed / skip
 * requests with the same messages.  Live, it passes packets through.  Out
 * of live, it reads the channel cache through a private parser and hands
 * the client packets in the client's own time base: the offset between
 * source and client timestamps is calibrated by matching replayed video
 * frames against the recent live ones (the sharer and tsfix both move the
 * client's time base).  Back to live, packets the client already has are
 * dropped by their DTS, so the join has neither a gap nor a duplicate.
 */

#include "tvheadend.h"
#include "streaming.h"
#include "service.h"
#include "packet.h"
#include "timeshift.h"
#include "timeshift/private.h"
#include "timeshift/timeshift_svcbuf.h"
#include "timeshift/timeshift_svcts.h"
#include "parsers/parsers.h"

#define SVCTS_RING        512       ///< Recent live video packets, to calibrate
#define SVCTS_PREROLL     2000000   ///< us parsed before a target (keyframe)
#define SVCTS_CALIB_BACK  4000000   ///< us back from live to replay on entry
#define SVCTS_REWIND_STEP 1000000   ///< us stepped back for a keyframe
#define SVCTS_EXPIRED_GAP 10000000  ///< us ahead of the oldest data after an
                                    ///< expiry, as both then move at 1x
#define SVCTS_NSTREAMS    256

enum {
  SVCTS_LIVE,
  SVCTS_PAUSE,
  SVCTS_PLAY
};

typedef struct svcts_key {
  int64_t  dts;       ///< Client time base, 90 kHz
  uint32_t crc;
  uint32_t len;
  uint8_t  index;
  uint8_t  used;
} svcts_key_t;

typedef struct svcts_pkt {
  TAILQ_ENTRY(svcts_pkt) link;
  th_pkt_t *pkt;      ///< Timestamps in the client time base
  int64_t   time;     ///< us
} svcts_pkt_t;

TAILQ_HEAD(svcts_pkt_queue, svcts_pkt);

typedef struct svcts {
  streaming_target_t   input;
  streaming_target_t  *output;
  int64_t              max_time;    ///< us the client may go back
  tvh_mutex_t          lock;
  tvh_cond_t           cond;
  pthread_t            thread;
  int                  run;

  /* shared with the live side (lock) */
  svcbuf_t            *sb;          ///< Buffer of the attached service
  svcbuf_t            *sb_old;      ///< Detached, for the thread to release
  int                  reset;       ///< The thread drops its replay
  TAILQ_HEAD(, streaming_message) ctrl;
  int                  state;
  int                  speed;
  int                  keyframe_mode;
  int64_t              pause_time;  ///< us delivered at mono_play
  int64_t              mono_play;
  int64_t              last_time;   ///< us of the last packet out
  int64_t              live_time;   ///< us of the newest live packet
  int64_t              live_mono;
  int64_t              out_dts[SVCTS_NSTREAMS];
  uint8_t              is_video[SVCTS_NSTREAMS];
  svcts_key_t          ring[SVCTS_RING];
  int                  ring_pos;
  int                  calibrated;
  int64_t              off;         ///< Source minus client, 90 kHz
  struct svcts_pkt_queue q;          ///< Replayed, client time base
  int64_t              expect;      ///< 90 kHz, to unwrap source times

  /* thread */
  svcbuf_reader_t     *rd;
  service_t           *rd_service;
  streaming_target_t  *parser;
  streaming_target_t   sink;
  int                  at_head;
  int                  reanchor;    ///< Restart the pacing on the next one
  int64_t              skip_to;     ///< us: drop until a keyframe from here
  int                  skip_back;   ///< ... or the last one before, going back
  streaming_message_t *skip_reply;  ///< Answered with the first packet
  svcts_pkt_t         *rw_pkt;      ///< Rewind: next keyframe to show
  int64_t              mono_status;
} svcts_t;

int
svcts_enabled ( void )
{
  return timeshift_conf.enabled && timeshift_conf.record_cache &&
         !(timeshift_conf.ram_only && timeshift_conf.ram_size == 0);
}

/* **************************************************************************
 * Helpers (lock held)
 * *************************************************************************/

static inline int
svcts_is_keyframe ( const th_pkt_t *pkt )
{
  return SCT_ISVIDEO(pkt->pkt_type) && pkt->v.pkt_frametype == PKT_I_FRAME;
}

static inline int64_t
svcts_pkt_time ( const th_pkt_t *pkt )
{
  return ts_rescale(pkt->pkt_pts != PTS_UNSET ? pkt->pkt_pts : pkt->pkt_dts,
                    1000000);
}

static int
svcts_key ( th_pkt_t *pkt, svcts_key_t *k )
{
  size_t len = pktbuf_len(pkt->pkt_payload);
  if (!SCT_ISVIDEO(pkt->pkt_type) || len < 64 || pkt->pkt_dts == PTS_UNSET)
    return 0;
  k->index = pkt->pkt_componentindex;
  k->len   = len;
  k->crc   = tvh_crc32(pktbuf_ptr(pkt->pkt_payload), MIN(len, 188), 0);
  k->used  = 1;
  return 1;
}

/* Bring a source time (33 bits) next to where the client time base is */
static int64_t
svcts_unwrap ( int64_t v, int64_t expect )
{
  const int64_t wrap = PTS_MASK + 1;
  if (expect == PTS_UNSET)
    return v;
  while (v < expect - wrap / 2) v += wrap;
  while (v > expect + wrap / 2) v -= wrap;
  return v;
}

static void
svcts_out_reset ( svcts_t *st )
{
  int i;
  for (i = 0; i < SVCTS_NSTREAMS; i++)
    st->out_dts[i] = PTS_UNSET;
}

/* A packet the client does not have yet: remember its DTS */
static int
svcts_out_new ( svcts_t *st, const th_pkt_t *pkt )
{
  int64_t *last = &st->out_dts[pkt->pkt_componentindex];
  if (pkt->pkt_dts == PTS_UNSET)
    return 1;
  if (*last != PTS_UNSET && pkt->pkt_dts <= *last)
    return 0;
  *last = pkt->pkt_dts;
  return 1;
}

static void
svcts_queue_clear ( svcts_t *st )
{
  svcts_pkt_t *p;
  svcts_pkt_t *n;

  for (p = TAILQ_FIRST(&st->q); p != NULL; p = n) {
    n = TAILQ_NEXT(p, link);
    TAILQ_REMOVE(&st->q, p, link);
    pkt_ref_dec(p->pkt);
    free(p);
  }
  if (st->rw_pkt) {
    pkt_ref_dec(st->rw_pkt->pkt);
    free(st->rw_pkt);
    st->rw_pkt = NULL;
  }
}

/* Oldest time (us, client base) the client may go back to, 0 if none */
static int64_t
svcts_oldest ( svcts_t *st )
{
  int64_t oldest, newest, t;
  if (st->sb == NULL || st->live_time == 0 ||
      !svcbuf_span(st->sb, &oldest, &newest))
    return 0;
  t = st->live_time - (st->live_mono - oldest) + SVCTS_PREROLL;
  if (st->max_time && t < st->live_time - st->max_time)
    t = st->live_time - st->max_time;
  return MIN(t, st->live_time);
}

static void
svcts_fill_status ( svcts_t *st, timeshift_status_t *status )
{
  int64_t cur = st->state == SVCTS_LIVE ? st->live_time : st->last_time;
  int64_t start = svcts_oldest(st);

  status->full = 0;
  status->shift = ts_rescale_inv(MAX(0, st->live_time - cur), 1000000);
  if (start) {
    status->pts_start = ts_rescale_inv(start, 1000000);
    status->pts_end   = ts_rescale_inv(st->live_time, 1000000);
  } else {
    status->pts_start = PTS_UNSET;
    status->pts_end   = PTS_UNSET;
  }
}

static void
svcts_status ( svcts_t *st )
{
  timeshift_status_t *status = calloc(1, sizeof(*status));
  svcts_fill_status(st, status);
  streaming_target_deliver2(st->output,
                            streaming_msg_create_data(SMT_TIMESHIFT_STATUS, status));
}

static void
svcts_speed_notify ( svcts_t *st )
{
  streaming_target_deliver2(st->output,
                            streaming_msg_create_code(SMT_SPEED, st->speed));
}

/* **************************************************************************
 * Live side
 * *************************************************************************/

static void
svcts_live_reset ( svcts_t *st )
{
  if (st->state != SVCTS_LIVE) {
    st->state = SVCTS_LIVE;
    st->speed = 100;
    st->reset = 1;
    svcts_speed_notify(st);
    tvh_cond_signal(&st->cond, 0);
  }
  svcts_out_reset(st);
  memset(st->ring, 0, sizeof(st->ring));
  st->calibrated = 0;
  st->live_time = 0;
}

static void
svcts_live_seen ( svcts_t *st, th_pkt_t *pkt )
{
  svcts_key_t *k;
  int64_t t;

  if (pkt->pkt_type == SCT_TELETEXT)
    return;
  if (SCT_ISVIDEO(pkt->pkt_type))
    st->is_video[pkt->pkt_componentindex] = 1;
  if (pkt->pkt_pts != PTS_UNSET || pkt->pkt_dts != PTS_UNSET) {
    t = svcts_pkt_time(pkt);
    if (t > st->live_time) {
      st->live_time = t;
      st->live_mono = mclk();
    }
  }
  k = &st->ring[st->ring_pos];
  if (svcts_key(pkt, k)) {
    k->dts = pkt->pkt_dts;
    st->ring_pos = (st->ring_pos + 1) % SVCTS_RING;
  }
}

static void
svcts_input ( void *opaque, streaming_message_t *sm )
{
  svcts_t *st = opaque;
  th_pkt_t *pkt;

  tvh_mutex_lock(&st->lock);
  switch (sm->sm_type) {
  case SMT_PACKET:
    pkt = sm->sm_data;
    svcts_live_seen(st, pkt);
    if (st->state == SVCTS_LIVE && svcts_out_new(st, pkt)) {
      if (pkt->pkt_type != SCT_TELETEXT)
        st->last_time = svcts_pkt_time(pkt);
      streaming_target_deliver2(st->output, sm);
    } else {
      streaming_msg_free(sm);
    }
    break;
  case SMT_SPEED:
  case SMT_SKIP:
    TAILQ_INSERT_TAIL(&st->ctrl, sm, sm_link);
    tvh_cond_signal(&st->cond, 0);
    break;
  case SMT_START:
  case SMT_STOP:
    /* new streams, new time base */
    svcts_live_reset(st);
    streaming_target_deliver2(st->output, sm);
    break;
  default:
    streaming_target_deliver2(st->output, sm);
    break;
  }
  tvh_mutex_unlock(&st->lock);
}

static htsmsg_t *
svcts_input_info ( void *opaque, htsmsg_t *list )
{
  svcts_t *st = opaque;
  streaming_target_t *out = st->output;
  htsmsg_add_str(list, NULL, "channel cache timeshift input");
  return out->st_ops.st_info(out->st_opaque, list);
}

static streaming_ops_t svcts_input_ops = {
  .st_cb   = svcts_input,
  .st_info = svcts_input_info
};

/* **************************************************************************
 * Replay: the private parser's output, in the client's time base
 * *************************************************************************/

/* Line a replayed video frame up with the same frame sent live.  Only a
 * frame seen once counts: identical ones (black frames...) would mislead. */
static void
svcts_calibrate ( svcts_t *st, th_pkt_t *pkt )
{
  svcts_key_t k;
  const svcts_key_t *match = NULL;
  int i;

  if (!svcts_key(pkt, &k))
    return;
  for (i = 0; i < SVCTS_RING; i++) {
    const svcts_key_t *r = &st->ring[i];
    if (r->used && r->index == k.index && r->len == k.len && r->crc == k.crc) {
      if (match)
        return;
      match = r;
    }
  }
  if (match == NULL)
    return;
  st->off = (pkt->pkt_dts & PTS_MASK) - match->dts;
  st->expect = match->dts;
  st->calibrated = 1;
  tvhdebug(LS_TIMESHIFT, "svcts: calibrated, source - client = %"PRId64, st->off);
}

static void
svcts_sink ( void *opaque, streaming_message_t *sm )
{
  svcts_t *st = opaque;
  th_pkt_t *pkt, *n;
  svcts_pkt_t *p;

  if (sm->sm_type != SMT_PACKET) {
    streaming_msg_free(sm);
    return;
  }
  pkt = sm->sm_data;
  tvh_mutex_lock(&st->lock);
  if (!st->calibrated)
    svcts_calibrate(st, pkt);
  if (st->calibrated && pkt->pkt_dts != PTS_UNSET) {
    n = pkt_copy_shallow(pkt);
    n->pkt_dts = svcts_unwrap((pkt->pkt_dts & PTS_MASK) - st->off, st->expect);
    st->expect = n->pkt_dts;
    n->pkt_pts = pkt->pkt_pts == PTS_UNSET ? n->pkt_dts :
                 svcts_unwrap((pkt->pkt_pts & PTS_MASK) - st->off, n->pkt_dts);
    if (pkt->pkt_pcr != PTS_UNSET)
      n->pkt_pcr = svcts_unwrap((pkt->pkt_pcr & PTS_MASK) - st->off, n->pkt_dts);
    p = malloc(sizeof(*p));
    p->pkt = n;
    p->time = svcts_pkt_time(n);
    TAILQ_INSERT_TAIL(&st->q, p, link);
  }
  tvh_mutex_unlock(&st->lock);
  streaming_msg_free(sm);
}

static htsmsg_t *
svcts_sink_info ( void *opaque, htsmsg_t *list )
{
  htsmsg_add_str(list, NULL, "channel cache timeshift replay");
  return list;
}

static streaming_ops_t svcts_sink_ops = {
  .st_cb   = svcts_sink,
  .st_info = svcts_sink_info
};

/* Drop the replay pipeline (lock held, released meanwhile) */
static void
svcts_replay_drop ( svcts_t *st )
{
  streaming_target_t *parser = st->parser;
  svcbuf_reader_t *rd = st->rd;
  svcbuf_t *old = st->sb_old;

  st->parser = NULL;
  st->rd = NULL;
  st->rd_service = NULL;
  st->sb_old = NULL;
  st->reset = 0;
  svcts_queue_clear(st);
  st->skip_to = 0;
  st->skip_back = 0;
  st->reanchor = 0;
  if (st->skip_reply) {
    ((streaming_skip_t *)st->skip_reply->sm_data)->type = SMT_SKIP_ERROR;
    streaming_target_deliver2(st->output, st->skip_reply);
    st->skip_reply = NULL;
  }
  tvh_mutex_unlock(&st->lock);
  if (parser)
    parser_destroy(parser);
  if (rd)
    svcbuf_reader_destroy(rd);
  if (old)
    svcbuf_release(old);
  tvh_mutex_lock(&st->lock);
}

/* Replay from 'time' (us, client base): a fresh parser, as data restarts
 * at an arbitrary point (lock held, released meanwhile).  0 on failure. */
static int
svcts_replay_open ( svcts_t *st, int64_t time )
{
  streaming_target_t *parser = st->parser;
  svcbuf_t *sb = st->sb;
  service_t *t;
  streaming_start_t *ss;
  int64_t mono;

  if (sb == NULL || st->live_time == 0)
    return 0;
  mono = st->live_mono - (st->live_time - time) - SVCTS_PREROLL;
  st->parser = NULL;
  svcts_queue_clear(st);
  st->at_head = 0;
  st->expect = ts_rescale_inv(time, 1000000);
  if (st->rd == NULL) {
    st->rd = svcbuf_reader_create(sb);
    st->rd_service = svcbuf_service(sb);
  }
  svcbuf_reader_seek(st->rd, mono);
  t = st->rd_service;
  tvh_mutex_unlock(&st->lock);
  if (parser)
    parser_destroy(parser);
  tvh_mutex_lock(&t->s_stream_mutex);
  parser = parser_create_replay(&st->sink, t);
  ss = service_build_streaming_start(t);
  streaming_target_deliver2(parser, streaming_msg_create_data(SMT_START, ss));
  tvh_mutex_unlock(&t->s_stream_mutex);
  tvh_mutex_lock(&st->lock);
  st->parser = parser;
  return 1;
}

/* Parse one more chunk of the cache (lock held, released meanwhile).
 * 1 when data was parsed, 0 at the head, -1 on error, 2 when the position
 * expired under the reader (nothing parsed: the data no longer follows) */
static int
svcts_fill ( svcts_t *st )
{
  svcbuf_reader_t *rd = st->rd;
  streaming_target_t *parser = st->parser;
  service_t *t = st->rd_service;
  pktbuf_t *pb;
  int r;

  if (rd == NULL || parser == NULL)
    return -1;
  tvh_mutex_unlock(&st->lock);
  r = svcbuf_reader_read(rd, &pb);
  if (r > 0 && svcbuf_reader_lost(rd)) {
    pktbuf_ref_dec(pb);
    r = 2;
  } else if (r > 0) {
    tvh_mutex_lock(&t->s_stream_mutex);
    streaming_target_deliver2(parser, streaming_msg_create_data(SMT_MPEGTS, pb));
    tvh_mutex_unlock(&t->s_stream_mutex);
  }
  tvh_mutex_lock(&st->lock);
  return r;
}

/* Going back, start from the last keyframe at or before skip_to, as the
 * packet timeshift does: parse past the target, then drop what precedes
 * that keyframe (lock held, released meanwhile) */
static void
svcts_skip_back_select ( svcts_t *st )
{
  svcts_pkt_t *p;
  svcts_pkt_t *n;
  svcts_pkt_t *kf = NULL;

  st->skip_back = 0;
  for (;;) {
    p = TAILQ_LAST(&st->q, svcts_pkt_queue);
    if ((p && p->time > st->skip_to) || st->reset)
      break;
    if (svcts_fill(st) != 1)
      break;
  }
  TAILQ_FOREACH(p, &st->q, link)
    if (p->time <= st->skip_to && svcts_is_keyframe(p->pkt))
      kf = p;
  if (kf == NULL)
    return;   /* none in the preroll: the first one after it */
  for (p = TAILQ_FIRST(&st->q); p != kf; p = n) {
    n = TAILQ_NEXT(p, link);
    TAILQ_REMOVE(&st->q, p, link);
    pkt_ref_dec(p->pkt);
    free(p);
  }
  st->skip_to = kf->time;
}

/* Drop from the head of the queue what is not to be shown -- what comes
 * before the keyframe a skip lands on, or all but keyframes in keyframe
 * mode -- and return the first packet to show, if any (lock held) */
static svcts_pkt_t *
svcts_head ( svcts_t *st )
{
  svcts_pkt_t *p = TAILQ_FIRST(&st->q);
  svcts_pkt_t *n;

  while (p != NULL &&
         ((st->skip_to && !(svcts_is_keyframe(p->pkt) && p->time >= st->skip_to)) ||
          (st->keyframe_mode && !svcts_is_keyframe(p->pkt)))) {
    n = TAILQ_NEXT(p, link);
    TAILQ_REMOVE(&st->q, p, link);
    pkt_ref_dec(p->pkt);
    free(p);
    p = n;
  }
  if (p != NULL)
    st->skip_to = 0;
  return p;
}

/* Next packet to show, parsing as needed (lock held, released meanwhile);
 * NULL at the head of the cache or on error */
static svcts_pkt_t *
svcts_next ( svcts_t *st )
{
  svcts_pkt_t *p;
  int r;

  if (st->skip_back && st->skip_to)
    svcts_skip_back_select(st);
  for (;;) {
    if (st->reset)
      return NULL;
    if ((p = svcts_head(st)) != NULL)
      return p;
    r = svcts_fill(st);
    if (r == 2) {
      /* a pause longer than the cache: the position expired.  Go on a
       * little ahead of the oldest data, as it keeps expiring at 1x. */
      int64_t t = MIN(svcts_oldest(st) + SVCTS_EXPIRED_GAP, st->live_time);
      tvhdebug(LS_TIMESHIFT, "svcts: position expired, going on from %"PRId64"s back",
               (st->live_time - t) / 1000000);
      if (!svcts_replay_open(st, t))
        return NULL;
      svcts_out_reset(st);
      st->skip_to = t;
      st->reanchor = 1;
      continue;
    }
    if (r <= 0) {
      st->at_head = 1;
      return NULL;
    }
    st->at_head = 0;
  }
}

/* Hand a packet to the client (lock held) */
static void
svcts_output ( svcts_t *st, svcts_pkt_t *p )
{
  if (svcts_out_new(st, p->pkt)) {
    if (p->pkt->pkt_type != SCT_TELETEXT)
      st->last_time = p->time;
    streaming_target_deliver2(st->output, streaming_msg_create_pkt(p->pkt));
  } else {
    pkt_ref_dec(p->pkt);
  }
  free(p);
}

/* Answer a pending skip with the first packet found (lock held) */
static void
svcts_skip_answer ( svcts_t *st, const svcts_pkt_t *p )
{
  streaming_skip_t *skip = st->skip_reply->sm_data;

  skip->type = SMT_SKIP_ABS_TIME;
  skip->time = ts_rescale_inv(p->time, 1000000);
  st->last_time = p->time;
  svcts_fill_status(st, &skip->timeshift);
  streaming_target_deliver2(st->output, st->skip_reply);
  st->skip_reply = NULL;
  st->pause_time = p->time;
  st->mono_play = mclk();
  st->mono_status = st->mono_play;
}

/* Back to live: what the client already has is dropped by its DTS */
static void
svcts_go_live ( svcts_t *st )
{
  tvhdebug(LS_TIMESHIFT, "svcts: back to live");
  st->state = SVCTS_LIVE;
  st->speed = 100;
  st->keyframe_mode = 0;
  st->reset = 1;
  svcts_speed_notify(st);
}

/* Start of the cache reached going back: pause there */
static void
svcts_at_start ( svcts_t *st )
{
  tvhdebug(LS_TIMESHIFT, "svcts: start of the cache, pause");
  st->state = SVCTS_PAUSE;
  st->speed = 0;
  st->keyframe_mode = 0;
  st->pause_time = st->last_time;
  svcts_speed_notify(st);
}

/* Rewind: find the last keyframe before last_time (lock held) */
static svcts_pkt_t *
svcts_rewind_find ( svcts_t *st )
{
  svcts_pkt_t *p;
  svcts_pkt_t *n;
  svcts_pkt_t *kf;
  int64_t step = SVCTS_REWIND_STEP, from, oldest;
  int at_start;

  for (;;) {
    oldest = svcts_oldest(st);
    from = st->last_time - step;
    at_start = from <= oldest;
    if (at_start)
      from = oldest;
    if (!svcts_replay_open(st, from + SVCTS_PREROLL))
      return NULL;
    kf = NULL;
    p = TAILQ_FIRST(&st->q);
    for (;;) {
      if (st->reset)
        break;
      if (p == NULL) {
        /* all looked at: parse on */
        if (svcts_fill(st) != 1)
          break;
        p = TAILQ_FIRST(&st->q);
        continue;
      }
      if (p->time >= st->last_time)
        break;
      n = TAILQ_NEXT(p, link);
      TAILQ_REMOVE(&st->q, p, link);
      if (svcts_is_keyframe(p->pkt)) {
        if (kf) {
          pkt_ref_dec(kf->pkt);
          free(kf);
        }
        kf = p;
      } else {
        pkt_ref_dec(p->pkt);
        free(p);
      }
      p = n;
    }
    if (kf || at_start || st->reset)
      return kf;
    step *= 2;
  }
}

/* PLAY: hand over what is due; returns how long to wait (lock held) */
static int64_t
svcts_play ( svcts_t *st )
{
  svcts_pkt_t *p;
  int64_t now, deliver, w;

  for (;;) {
    now = mclk();

    if (st->speed < 0) {
      if (st->rw_pkt == NULL) {
        st->rw_pkt = svcts_rewind_find(st);
        if (st->reset)
          return 0;
        if (st->rw_pkt == NULL) {
          svcts_at_start(st);
          return 0;
        }
      }
      p = st->rw_pkt;
    } else {
      p = svcts_next(st);
      if (st->reset || st->state != SVCTS_PLAY)
        return 0;
      if (p == NULL) {
        if (st->at_head && st->skip_reply == NULL) {
          svcts_go_live(st);
          return 0;
        }
        return ms2mono(100);
      }
    }

    if (st->skip_reply)
      svcts_skip_answer(st, p);
    if (st->reanchor) {
      st->reanchor = 0;
      st->pause_time = p->time;
      st->mono_play = now;
      svcts_out_reset(st);
      svcts_status(st);
    }

    deliver = st->pause_time +
              ((now - st->mono_play) + TIMESHIFT_PLAY_BUF) * st->speed / 100;
    if (st->speed > 0 ? p->time > deliver : p->time < deliver) {
      w = llabs(p->time - deliver) * 100 / llabs(st->speed);
      return MIN(MAX(w, ms2mono(1)), sec2mono(1));
    }

    if (p == st->rw_pkt) {
      st->rw_pkt = NULL;
      st->out_dts[p->pkt->pkt_componentindex] = PTS_UNSET;  /* going back */
    } else {
      TAILQ_REMOVE(&st->q, p, link);
    }
    svcts_output(st, p);
    if (st->keyframe_mode)
      svcts_status(st);   /* as the packet timeshift does in keyframe mode */
  }
}

/* PAUSE after a skip: show the frame skipped to (lock held) */
static void
svcts_pause_skip ( svcts_t *st )
{
  svcts_pkt_t *p = svcts_next(st);
  if (p == NULL || st->reset)
    return;
  svcts_skip_answer(st, p);
  TAILQ_REMOVE(&st->q, p, link);
  svcts_output(st, p);
}

/* **************************************************************************
 * Requests
 * *************************************************************************/

/* Leave live: replay from just before the live point, calibrate against
 * the packets the client got, and go on from the last one (lock held) */
static int
svcts_enter ( svcts_t *st )
{
  if (st->sb == NULL || st->live_time == 0)
    return 0;
  if (!svcts_replay_open(st, st->live_time - SVCTS_CALIB_BACK + SVCTS_PREROLL))
    return 0;
  while (!st->calibrated && !st->reset) {
    if (svcts_fill(st) != 1)
      break;
  }
  if (!st->calibrated) {
    tvhwarn(LS_TIMESHIFT, "svcts: unable to line the cache up with the stream");
    return 0;
  }
  /* what the client already has is dropped on the way out, by its DTS */
  st->pause_time = st->last_time;
  st->mono_play = mclk();
  return 1;
}

/* Carry on from last_time again: after a change of direction or of
 * keyframe mode, the queued packets do not follow on (lock held) */
static void
svcts_reposition ( svcts_t *st )
{
  int i;

  if (svcts_replay_open(st, st->last_time)) {
    /* the client has the frame on screen: carry on after it, with the
     * other streams from there */
    for (i = 0; i < SVCTS_NSTREAMS; i++)
      if (!st->is_video[i])
        st->out_dts[i] = PTS_UNSET;
    st->skip_to = st->last_time;
    st->skip_back = 0;
  }
}

static void
svcts_speed ( svcts_t *st, streaming_message_t *ctrl )
{
  int speed = ctrl->sm_code, kf;

  if (speed > 3200)  speed = 3200;
  if (speed < -3200) speed = -3200;

  if (st->state == SVCTS_LIVE) {
    if (speed >= 100 || !svcts_enter(st))
      speed = 100;
    else
      tvhdebug(LS_TIMESHIFT, "svcts: enter timeshift mode");
  }

  if (st->state != SVCTS_LIVE || speed != 100) {
    kf = speed < 0 || speed > 400;
    /* leaving a rewind, or keyframes for every frame: the queue does not
     * follow on from what was last shown */
    if (st->state != SVCTS_LIVE &&
        ((st->speed < 0 && speed >= 0) || (st->keyframe_mode && !kf)))
      svcts_reposition(st);
    if (speed != st->speed || st->state == SVCTS_LIVE) {
      st->pause_time = st->last_time;
      st->mono_play = mclk();
    }
    if (st->rw_pkt && speed >= 0) {
      pkt_ref_dec(st->rw_pkt->pkt);
      free(st->rw_pkt);
      st->rw_pkt = NULL;
    }
    st->keyframe_mode = kf;
    st->state = speed == 0 ? SVCTS_PAUSE : SVCTS_PLAY;
    tvhdebug(LS_TIMESHIFT, "svcts: change speed %d", speed);
  }
  st->speed = speed;
  ctrl->sm_code = speed;
  streaming_target_deliver2(st->output, ctrl);
}

static void
svcts_skip ( svcts_t *st, streaming_message_t *ctrl )
{
  streaming_skip_t *skip = ctrl->sm_data;
  int64_t t, oldest, cur;

  switch (skip->type) {
  case SMT_SKIP_LIVE:
    if (st->state != SVCTS_LIVE)
      svcts_go_live(st);
    skip->type = SMT_SKIP_ABS_TIME;
    skip->time = ts_rescale_inv(st->live_time, 1000000);
    svcts_fill_status(st, &skip->timeshift);
    streaming_target_deliver2(st->output, ctrl);
    return;
  case SMT_SKIP_ABS_TIME:
  case SMT_SKIP_REL_TIME:
    break;
  default:
    goto error;
  }

  t = ts_rescale(skip->time, 1000000);
  if (skip->type == SMT_SKIP_REL_TIME)
    t += st->state == SVCTS_LIVE ? st->live_time : st->last_time;
  tvhdebug(LS_TIMESHIFT, "svcts: skip to %"PRId64" (live %"PRId64")",
           t, st->live_time);
  if (st->sb == NULL || st->live_time == 0)
    goto error;
  if (st->state == SVCTS_LIVE && t >= st->live_time - TIMESHIFT_PLAY_BUF)
    goto error;   /* already live */
  if (t >= st->live_time) {
    svcts_go_live(st);
    skip->type = SMT_SKIP_ABS_TIME;
    skip->time = ts_rescale_inv(st->live_time, 1000000);
    svcts_fill_status(st, &skip->timeshift);
    streaming_target_deliver2(st->output, ctrl);
    return;
  }
  oldest = svcts_oldest(st);
  if (t < oldest)
    t = oldest;

  if (!st->calibrated && !svcts_enter(st))
    goto error;
  cur = st->state == SVCTS_LIVE ? st->live_time : st->last_time;
  if (!svcts_replay_open(st, t))
    goto error;
  svcts_out_reset(st);
  st->skip_to = t;
  st->skip_back = t < cur;
  if (st->skip_reply)
    streaming_msg_free(st->skip_reply);
  st->skip_reply = ctrl;
  if (st->rw_pkt) {
    pkt_ref_dec(st->rw_pkt->pkt);
    free(st->rw_pkt);
    st->rw_pkt = NULL;
  }
  if (st->state == SVCTS_LIVE) {
    st->state = SVCTS_PLAY;
    st->speed = 100;
    st->keyframe_mode = 0;
  }
  st->pause_time = t;
  st->mono_play = mclk();
  return;

error:
  skip->type = SMT_SKIP_ERROR;
  streaming_target_deliver2(st->output, ctrl);
}

/* **************************************************************************
 * Thread
 * *************************************************************************/

static void *
svcts_thread ( void *aux )
{
  svcts_t *st = aux;
  streaming_message_t *ctrl;
  int64_t wait, now;

  tvh_mutex_lock(&st->lock);
  while (st->run) {
    if (st->reset || st->sb_old)
      svcts_replay_drop(st);

    while ((ctrl = TAILQ_FIRST(&st->ctrl)) != NULL) {
      TAILQ_REMOVE(&st->ctrl, ctrl, sm_link);
      if (ctrl->sm_type == SMT_SPEED)
        svcts_speed(st, ctrl);
      else
        svcts_skip(st, ctrl);
      if (st->reset)
        svcts_replay_drop(st);
    }

    wait = sec2mono(1);
    if (st->state == SVCTS_PLAY)
      wait = svcts_play(st);
    else if (st->state == SVCTS_PAUSE && st->skip_reply)
      svcts_pause_skip(st);

    now = mclk();
    if (now >= st->mono_status + sec2mono(1)) {
      svcts_status(st);
      st->mono_status = now;
    }
    if (wait > 0 && TAILQ_EMPTY(&st->ctrl) && !st->reset && !st->sb_old)
      tvh_cond_timedwait(&st->cond, &st->lock, mclk() + wait);
  }
  svcts_replay_drop(st);
  tvh_mutex_unlock(&st->lock);
  return NULL;
}

/* **************************************************************************
 * Life cycle
 * *************************************************************************/

streaming_target_t *
svcts_create ( streaming_target_t *out, time_t max_period )
{
  svcts_t *st = calloc(1, sizeof(*st));

  st->output   = out;
  st->max_time = (int64_t)max_period * 1000000;
  st->state    = SVCTS_LIVE;
  st->speed    = 100;
  st->run      = 1;
  tvh_mutex_init(&st->lock, NULL);
  tvh_cond_init(&st->cond, 1);
  TAILQ_INIT(&st->ctrl);
  TAILQ_INIT(&st->q);
  svcts_out_reset(st);
  streaming_target_init(&st->input, &svcts_input_ops, st, 0);
  streaming_target_init(&st->sink, &svcts_sink_ops, st, 0);
  tvh_thread_create(&st->thread, NULL, svcts_thread, st, "svcts");
  return &st->input;
}

void
svcts_destroy ( streaming_target_t *pad )
{
  svcts_t *st = (svcts_t *)pad;
  streaming_message_t *sm;
  streaming_message_t *next;

  tvh_mutex_lock(&st->lock);
  st->run = 0;
  tvh_cond_signal(&st->cond, 0);
  tvh_mutex_unlock(&st->lock);
  pthread_join(st->thread, NULL);

  for (sm = TAILQ_FIRST(&st->ctrl); sm != NULL; sm = next) {
    next = TAILQ_NEXT(sm, sm_link);
    TAILQ_REMOVE(&st->ctrl, sm, sm_link);
    streaming_msg_free(sm);
  }
  if (st->sb)
    svcbuf_release(st->sb);
  tvh_cond_destroy(&st->cond);
  tvh_mutex_destroy(&st->lock);
  free(st);
}

void
svcts_attach ( streaming_target_t *pad, service_t *t )
{
  svcts_t *st = (svcts_t *)pad;
  svcbuf_t *sb = svcbuf_acquire(t);

  tvh_mutex_lock(&st->lock);
  if (st->sb) {
    if (st->sb_old)
      svcbuf_release(st->sb_old);   /* not the thread's: it only has sb_old */
    st->sb_old = st->sb;
  }
  st->sb = sb;
  svcts_live_reset(st);
  st->reset = 1;
  tvh_cond_signal(&st->cond, 0);
  tvh_mutex_unlock(&st->lock);
}

void
svcts_detach ( streaming_target_t *pad )
{
  svcts_t *st = (svcts_t *)pad;

  tvh_mutex_lock(&st->lock);
  if (st->sb) {
    if (st->sb_old)
      svcbuf_release(st->sb_old);
    st->sb_old = st->sb;
    st->sb = NULL;
  }
  svcts_live_reset(st);
  st->reset = 1;
  tvh_cond_signal(&st->cond, 0);
  tvh_mutex_unlock(&st->lock);
}
