/*
 *  tvheadend, continuous timestamps for pass recordings
 *  Copyright (C) 2026 Bernhard Berger
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
#include "muxer_pass_timeline.h"

#define PASS_TIMELINE_PACKETS 4096
#define PASS_TIMELINE_EPOCHS 8
#define PASS_OUTPUT_PACKETS 256
#define PASS_CLOCK_MASK ((UINT64_C(1) << 33) - 1)
#define PASS_CLOCK_JUMP (INT64_C(30) * 90000)
#define PASS_CLOCK_TAIL (INT64_C(2) * 90000)
#define PASS_CLOCK_DELAY INT64_C(90000)
#define PASS_CLOCK_CADENCE 3600 /* 40 ms */
#define PASS_CLOCK_CADENCE_MAX 9000 /* 100 ms */
#define PASS_CLOCK_INTERVAL_MAX (INT64_C(1) << 31) /* Below half a wrap. */
#define PASS_PTS_HIGH_MASK 0x0eU
#define PASS_PTS_BYTE_MASK 0xfeU
#define PASS_PES_HEADER 9
#define PASS_PTS_HEADER 14
#define PASS_PTSDTS_HEADER 19
#define PASS_TIMESTAMP_SIZE 5

typedef struct pass_epoch {
  int64_t first;
  int64_t last;
  int64_t offset;
} pass_epoch_t;

struct pass_timeline {
  uint8_t packets[PASS_TIMELINE_PACKETS][188];
  int64_t queued_at[PASS_TIMELINE_PACKETS];
  uint8_t pcr[PASS_TIMELINE_PACKETS];
  uint8_t output[PASS_OUTPUT_PACKETS * 188];
  uint8_t classified[8192 / 8];
  unsigned head;
  unsigned count;
  unsigned output_len;
  pass_epoch_t epochs[PASS_TIMELINE_EPOCHS];
  unsigned epoch_count;
  int64_t cadence;
  int64_t clock;
  int64_t output_pcr;
  int64_t output_pcr_offset;
  int output_pcr_valid;
  int bypassed;
  int failed;
  const char *name;
  pass_timeline_write_cb_t write_cb;
  void *opaque;
};

/* PCR epochs select a common 90 kHz offset for programme timestamps. Owned,
 * bounded look-ahead handles early PES; ambiguity or split PES headers latch
 * lossless passthrough for the rest of the recording. */
static unsigned
pass_ts_pid(const uint8_t *buf)
{
  return (buf[1] & 0x1f) << 8 | buf[2];
}

static int64_t
pass_clock_delta(int64_t a, int64_t b)
{
  uint64_t d = (uint64_t)(a - b) & PASS_CLOCK_MASK;
  return d >= (UINT64_C(1) << 32) ? (int64_t)d - (INT64_C(1) << 33) : (int64_t)d;
}

static int64_t
pass_clock_wrap(int64_t value)
{
  return (int64_t)((uint64_t)value & PASS_CLOCK_MASK);
}

static int64_t
pass_clock_abs(int64_t value)
{
  return value < 0 ? -value : value;
}

static int64_t
pass_pcr_read(const uint8_t *p)
{
  return ((int64_t)p[0] << 25) | ((int64_t)p[1] << 17) |
         ((int64_t)p[2] << 9) | ((int64_t)p[3] << 1) | ((int64_t)p[4] >> 7);
}

static int64_t
pass_pts_read(const uint8_t *p)
{
  return ((int64_t)(p[0] & PASS_PTS_HIGH_MASK) << 29) | ((int64_t)p[1] << 22) |
         ((int64_t)(p[2] & PASS_PTS_BYTE_MASK) << 14) |
         ((int64_t)p[3] << 7) | ((int64_t)p[4] >> 1);
}

static void
pass_pcr_write(uint8_t *p, int64_t value)
{
  uint64_t v = (uint64_t)value & PASS_CLOCK_MASK;
  p[0] = (uint8_t)(v >> 25);
  p[1] = (uint8_t)(v >> 17);
  p[2] = (uint8_t)(v >> 9);
  p[3] = (uint8_t)(v >> 1);
  p[4] = (uint8_t)((p[4] & 0x7f) | ((v & 1) << 7));
}

static void
pass_pts_write(uint8_t *p, int64_t value)
{
  uint64_t v = (uint64_t)value & PASS_CLOCK_MASK;
  p[0] = (uint8_t)((p[0] & ~PASS_PTS_HIGH_MASK) | ((v >> 29) & PASS_PTS_HIGH_MASK));
  p[1] = (uint8_t)(v >> 22);
  p[2] = (uint8_t)((p[2] & 1) | ((v >> 14) & PASS_PTS_BYTE_MASK));
  p[3] = (uint8_t)(v >> 7);
  p[4] = (uint8_t)((p[4] & 1) | ((v << 1) & PASS_PTS_BYTE_MASK));
}

/* Validate the adaptation length and the clock fields we touch. */
static int
pass_ts_payload_offset(const uint8_t *p)
{
  unsigned end;
  unsigned flags;
  unsigned pos;
  if (p[0] != 0x47 || (p[1] & 0x80) || !(p[3] & 0x30))
    return -1;
  if (!(p[3] & 0x20))
    return 4;
  end = 5 + p[4];
  if (end > 188 || ((p[3] & 0x10) && end == 188) ||
      ((p[3] & 0x30) == 0x20 && end != 188))
    return -1;
  if (!p[4])
    return (int)end;
  flags = p[5];
  pos = 6U + ((flags & 0x10) ? 6U : 0U) + ((flags & 0x08) ? 6U : 0U);
  if (pos > end)
    return -1;
  return (int)end;
}

static int64_t
pass_timeline_distance(const pass_epoch_t *e, int64_t clock)
{
  int64_t d = pass_clock_delta(clock, e->first);
  if (d >= 0 && pass_clock_delta(e->last, clock) >= 0)
    return 0;
  return MIN(pass_clock_abs(d), pass_clock_abs(pass_clock_delta(clock, e->last)));
}

static unsigned
pass_timeline_epoch(const pass_timeline_t *t, int64_t clock)
{
  unsigned i;
  unsigned best = 0;
  int64_t distance = INT64_MAX;
  for (i = 0; i < t->epoch_count; i++) {
    int64_t d = pass_timeline_distance(&t->epochs[i], clock);
    if (d <= distance) {
      distance = d;
      best = i;
    }
  }
  return best;
}

static const char *
pass_timeline_observe(pass_timeline_t *t, int64_t clock)
{
  pass_epoch_t *e;
  int64_t d;
  if (!t->epoch_count) {
    t->epochs[0].first = clock;
    t->epochs[0].last = clock;
    t->epoch_count++;
    return NULL;
  }
  e = &t->epochs[t->epoch_count - 1];
  d = pass_clock_delta(clock, e->last);
  if (pass_clock_abs(d) > PASS_CLOCK_JUMP) {
    unsigned previous = pass_timeline_epoch(t, clock);
    int64_t offset;
    if (previous != t->epoch_count - 1 &&
        pass_timeline_distance(&t->epochs[previous], clock) < PASS_CLOCK_TAIL)
      return "ambiguous PCR return/tail";
    offset = pass_clock_wrap(e->last + e->offset + t->cadence - clock);
    /* Queued packets may still need any retained epoch. Never evict one. */
    if (t->epoch_count == PASS_TIMELINE_EPOCHS)
      return "epoch capacity exhausted";
    e = &t->epochs[t->epoch_count++];
    e->offset = offset;
    e->first = clock;
    e->last = clock;
    t->clock += t->cadence;
  } else if (d > 0) {
    unsigned i;
    for (i = 0; i + 1 < t->epoch_count; i++)
      if (t->epochs[i].offset != e->offset &&
          !pass_timeline_distance(&t->epochs[i], clock))
        return "overlapping PCR epochs";
    if (d < PASS_CLOCK_CADENCE_MAX)
      t->cadence = d;
    t->clock += d;
    e->last = clock;
    if (pass_clock_delta(e->last, e->first) > PASS_CLOCK_INTERVAL_MAX)
      e->first = pass_clock_wrap(e->last - PASS_CLOCK_INTERVAL_MAX);
  }
  return NULL;
}

static int
pass_timeline_offset(const pass_timeline_t *t, int64_t clock, int64_t *offset)
{
  unsigned epoch;
  unsigned i;
  int64_t distance;
  if (!t->epoch_count)
    return -1;
  epoch = pass_timeline_epoch(t, clock);
  distance = pass_timeline_distance(&t->epochs[epoch], clock);
  if (distance > PASS_CLOCK_JUMP)
    return -1;
  for (i = 0; i < t->epoch_count; i++)
    if (i != epoch && t->epochs[i].offset != t->epochs[epoch].offset &&
        pass_timeline_distance(&t->epochs[i], clock) == distance)
      return -1; /* Overlapping clocks cannot be classified by proximity. */
  *offset = t->epochs[epoch].offset;
  return 0;
}

static const char *
pass_timeline_pcr(pass_timeline_t *t, uint8_t *p)
{
  int64_t clock = pass_pcr_read(p);
  int64_t offset;
  int64_t output;
  if (pass_timeline_offset(t, clock, &offset))
    return t->pcr[t->head] ? "unclassified designated PCR" : NULL;
  output = pass_clock_wrap(clock + offset);
  if (t->pcr[t->head]) {
    if (t->output_pcr_valid && offset != t->output_pcr_offset &&
        pass_clock_delta(output, t->output_pcr) < 0)
      return "PCR would move backwards";
    t->output_pcr = output;
    t->output_pcr_offset = offset;
    t->output_pcr_valid = 1;
  }
  pass_pcr_write(p, output);
  return NULL;
}

static const char *
pass_timeline_adaptation(pass_timeline_t *t, uint8_t *p)
{
  int64_t offset;
  if (p[5] & 0x10) {
    const char *reason = pass_timeline_pcr(t, p + 6);
    if (reason)
      return reason;
  }
  if (p[5] & 0x08) {
    uint8_t *q = p + ((p[5] & 0x10) ? 12 : 6);
    int64_t clock = pass_pcr_read(q);
    if (!pass_timeline_offset(t, clock, &offset))
      pass_pcr_write(q, clock + offset);
  }
  return NULL;
}

static unsigned
pass_pes_header_size(const uint8_t *h)
{
  unsigned need;
  unsigned pes_len;
  if (h[0] || h[1] || h[2] != 1 || (h[6] & 0xf0) != 0x80)
    return 0; /* Invalid prefix or PES-level scrambling. */
  switch (h[3]) {
  case 0xbc: /* programme_stream_map */
  case 0xbe: /* padding_stream */
  case 0xbf: /* private_stream_2 */
  case 0xf0: /* ECM */
  case 0xf1: /* EMM */
  case 0xf2: /* DSMCC */
  case 0xf8: /* H.222.1 type E */
  case 0xff: /* programme_stream_directory */
    return 0; /* No MPEG-2 optional PES header. */
  default:
    break;
  }
  /* ESCR is untouched. H.222.0 2.4.3.7: previous_PES_packet_CRC
   * covers previous PES data bytes, exclusive of its PES header. */
  if (!(h[7] & 0x80))
    return 0;
  need = (h[7] & 0x40) ? PASS_PTSDTS_HEADER : PASS_PTS_HEADER;
  pes_len = (unsigned)(h[4] << 8 | h[5]);
  return (h[8] < need - PASS_PES_HEADER || (pes_len && pes_len < 3U + h[8])) ? 0 : need;
}

static const char *
pass_timeline_rewrite(pass_timeline_t *t)
{
  uint8_t *p = t->packets[t->head];
  uint8_t *h;
  int pos = pass_ts_payload_offset(p);
  int need;
  int j;
  unsigned prefix;
  unsigned pid = pass_ts_pid(p);
  int64_t offset;
  if (pos < 0)
    return NULL;
  if ((p[3] & 0x20) && p[4]) {
    const char *reason = pass_timeline_adaptation(t, p);
    if (reason)
      return reason;
  }
  if (!(p[1] & 0x40) || !(p[3] & 0x10) || (p[3] & 0xc0))
    return NULL;
  h = p + pos;
  /* PSI starts with pointer_field/table_id/section flags, not a PES prefix. */
  if (pos + 3 > 188 || h[0] || h[1] || h[2] != 1)
    return NULL;
  if (pos + PASS_PES_HEADER > 188)
    return "PES header spans TS packets";
  need = (int)pass_pes_header_size(h);
  if (!need)
    return NULL;
  if (pos + PASS_PES_HEADER + h[8] > 188)
    return "PES header spans TS packets";
  prefix = need == PASS_PTSDTS_HEADER ? 0x3 : 0x2;
  for (j = PASS_PES_HEADER; j < need; j += PASS_TIMESTAMP_SIZE) {
    if (!(h[j] & h[j+2] & h[j+4] & 1) ||
        (h[j] >> 4) != prefix)
      return NULL;
    prefix = 0x1;
  }
  if (pass_timeline_offset(t, pass_pts_read(h + PASS_PES_HEADER), &offset)) {
    if (t->classified[pid / 8] & (1 << (pid % 8)))
      return "unresolved programme PTS (look-ahead exhausted or end of recording)";
    return NULL; /* Unclassified PID may use a foreign clock. */
  }
  t->classified[pid / 8] |= 1 << (pid % 8);
  /* PTS and DTS MUST use the same epoch, including reordered pictures. */
  for (j = PASS_PES_HEADER; j < need; j += PASS_TIMESTAMP_SIZE)
    pass_pts_write(h + j, pass_pts_read(h + j) + offset);
  return NULL;
}

static void
pass_timeline_output(pass_timeline_t *t, const void *data, size_t size)
{
  if (t->write_cb(t->opaque, data, size))
    t->failed = 1;
}

static void
pass_timeline_flush(pass_timeline_t *t)
{
  if (t->output_len) {
    pass_timeline_output(t, t->output, t->output_len);
    t->output_len = 0;
  }
}

static void
pass_timeline_pop(pass_timeline_t *t)
{
  memcpy(t->output + t->output_len, t->packets[t->head], 188);
  t->output_len += 188;
  t->head = (t->head + 1) % PASS_TIMELINE_PACKETS;
  t->count--;
  if (t->output_len == sizeof(t->output))
    pass_timeline_flush(t);
}

static void
pass_timeline_bypass(pass_timeline_t *t, const char *reason)
{
  if (!t->bypassed) {
    tvhwarn(LS_PASS, "%s: continuous timeline disabled (%s), keeping original timestamps for the rest of the recording",
            t->name, reason);
    t->bypassed = 1;
  }
  /* Never discard queued packets. */
  while (t->count)
    pass_timeline_pop(t);
  pass_timeline_flush(t);
}

static void
pass_timeline_emit(pass_timeline_t *t)
{
  const char *reason = pass_timeline_rewrite(t);
  if (reason)
    pass_timeline_bypass(t, reason);
  else
    pass_timeline_pop(t);
}

void
pass_timeline_drain(pass_timeline_t *t)
{
  if (!t)
    return;
  while (t->count && !t->failed)
    pass_timeline_emit(t);
  pass_timeline_flush(t);
}

static void
pass_timeline_observe_pcr(pass_timeline_t *t, const uint8_t *p)
{
  unsigned epochs = t->epoch_count;
  int64_t clock = pass_pcr_read(p + 6);
  int64_t delta = epochs ? pass_clock_delta(clock, t->epochs[epochs - 1].last) : 0;
  const char *reason = pass_timeline_observe(t, clock);
  if (reason)
    pass_timeline_bypass(t, reason);
  else if (epochs && t->epoch_count > epochs)
    tvhinfo(LS_PASS, "%s: PCR jump of %+"PRId64" ms, continuous timeline epoch %u",
            t->name, delta / 90, t->epoch_count - 1);
}

static void
pass_timeline_queue(pass_timeline_t *t, const uint8_t *p, int pcr)
{
  unsigned slot;
  if (!t->epoch_count) {
    memcpy(t->output + t->output_len, p, 188);
    t->output_len += 188;
    if (t->output_len == sizeof(t->output))
      pass_timeline_flush(t);
    return;
  }
  slot = (t->head + t->count) % PASS_TIMELINE_PACKETS;
  memcpy(t->packets[slot], p, 188);
  t->queued_at[slot] = t->clock;
  t->pcr[slot] = (uint8_t)pcr;
  t->count++;
  /* PCR-driven, not replay speed or packet-count driven. */
  while (t->count && t->clock - t->queued_at[t->head] > PASS_CLOCK_DELAY &&
         !t->failed)
    pass_timeline_emit(t);
}

void
pass_timeline_write(pass_timeline_t *t, const uint8_t *data, size_t size, unsigned pcr_pid)
{
  const uint8_t *p = data;
  size_t unaligned = size % 188;
  if (t->bypassed || t->failed) {
    pass_timeline_output(t, data, size);
    return;
  }
  if (unaligned || !pcr_pid || pcr_pid >= PASS_TIMELINE_NO_PCR) {
    pass_timeline_bypass(t, unaligned ? "unaligned transport data" :
                                      "PCR PID no longer available");
    pass_timeline_output(t, data, size);
    return;
  }
  while (size && !t->failed && !t->bypassed) {
    unsigned pid = pass_ts_pid(p);
    int pos = pass_ts_payload_offset(p);
    int pcr = pos >= 0 && (p[3] & 0x20) && p[4] && (p[5] & 0x10) &&
              pid == pcr_pid;
    if (pcr)
      pass_timeline_observe_pcr(t, p);
    if (t->count == PASS_TIMELINE_PACKETS)
      pass_timeline_emit(t);
    if (t->bypassed) {
      pass_timeline_output(t, p, size);
      return;
    }
    pass_timeline_queue(t, p, pcr);
    p += 188;
    size -= 188;
  }
  pass_timeline_flush(t);
  if (size && t->bypassed)
    pass_timeline_output(t, p, size);
}

pass_timeline_t *
pass_timeline_create(const char *name, pass_timeline_write_cb_t write_cb, void *opaque)
{
  pass_timeline_t *t = calloc(1, sizeof(*t));
  if (!t)
    return NULL;
  t->name = name ?: "Pass muxer";
  t->write_cb = write_cb;
  t->opaque = opaque;
  t->cadence = PASS_CLOCK_CADENCE;
  return t;
}

void
pass_timeline_destroy(pass_timeline_t *t)
{
  free(t);
}
