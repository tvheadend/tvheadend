/*
 *  tvheadend, MPEG transport stream demuxer
 *  Copyright (C) 2007 Andreas Öman
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
#define _GNU_SOURCE
#include <stdlib.h>

#include <pthread.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "tvheadend.h"
#include "subscriptions.h"
#include "parsers.h"
#include "streaming.h"
#include "input.h"
#include "parsers/parser_teletext.h"
#include "tsdemux.h"

#define TS_REMUX_BUFSIZE (188 * 100)

static void ts_remux(mpegts_service_t *t, const uint8_t *tsb, int len, int errors);
static void ts_skip(mpegts_service_t *t, const uint8_t *tsb, int len);

/**
 * Continue processing of transport stream packets
 */
static void
ts_recv_packet0
  (mpegts_service_t *t, elementary_stream_t *st, const uint8_t *tsb, int len)
{
  mpegts_service_t *m;
  int len2, off, pusi, cc, pid, error, errors = 0;
  const uint8_t *tsb2;

  service_set_streaming_status_flags((service_t*)t, TSS_MUX_PACKETS);

  if (!st)
    goto skip_cc;

  for (tsb2 = tsb, len2 = len; len2 > 0; tsb2 += 188, len2 -= 188) {

    pusi    = (tsb2[1] >> 6) & 1; /* 0x40 */
    error   = (tsb2[1] >> 7) & 1; /* 0x80 */
    errors += error;

    /* Check CC */

    if(tsb2[3] & 0x10) {
      cc = tsb2[3] & 0xf;
      if(st->es_cc != -1 && cc != st->es_cc) {
        /* Let the hardware to stabilize and don't flood the log */
        if (t->s_start_time + sec2mono(1) < mclk() &&
            tvhlog_limit(&st->es_cc_log, 10))
          tvhwarn("TS", "%s Continuity counter error (total %zi)",
                        service_component_nicename(st), st->es_cc_log.count);
        if (!error)
          errors++;
        error |= 2;
      }
      st->es_cc = (cc + 1) & 0xf;
    }

    if(!streaming_pad_probe_type(&t->s_streaming_pad, SMT_PACKET))
      continue;

    if (st->es_type == SCT_CA)
      continue;

    if (tsb2[3] & 0xc0) /* scrambled */
      continue;

    off = tsb2[3] & 0x20 ? tsb2[4] + 5 : 4;

    if(off <= 188 && t->s_status == SERVICE_RUNNING)
      parse_mpeg_ts((service_t*)t, st, tsb2 + off, 188 - off, pusi, error);

  }

skip_cc:
  if(streaming_pad_probe_type(&t->s_streaming_pad, SMT_MPEGTS))
    ts_remux(t, tsb, len, errors);

  for(off = 0; off < t->s_masters.is_count; off++) {
    m = (mpegts_service_t *)t->s_masters.is_array[off];
    pthread_mutex_lock(&m->s_stream_mutex);
    if(streaming_pad_probe_type(&m->s_streaming_pad, SMT_MPEGTS)) {
      pid = (tsb[1] & 0x1f) << 8 | tsb[2];
      if (mpegts_pid_rexists(m->s_slaves_pids, pid))
        ts_remux(m, tsb, len, errors);
    }
    pthread_mutex_unlock(&m->s_stream_mutex);
    /* mark service live even without real subscribers */
    service_set_streaming_status_flags((service_t*)t, TSS_PACKETS);
    t->s_streaming_live |= TSS_LIVE;
  }
}

/**
 * Continue processing of skipped packets
 */
static void
ts_recv_skipped0
  (mpegts_service_t *t, elementary_stream_t *st, const uint8_t *tsb, int len)
{
  mpegts_service_t *m;
  int len2, off, cc, pid;
  const uint8_t *tsb2;

  if (!st)
    goto skip_cc;

  for (tsb2 = tsb, len2 = len; len2 > 0; tsb2 += 188, len2 -= 188) {

    /* Check CC */

    if(tsb2[3] & 0x10) {
      cc = tsb2[3] & 0xf;
      if(st->es_cc != -1 && cc != st->es_cc) {
        /* Let the hardware to stabilize and don't flood the log */
        if (t->s_start_time + sec2mono(1) < mclk() &&
            tvhlog_limit(&st->es_cc_log, 10))
          tvhwarn("TS", "%s Continuity counter error (total %zi)",
                        service_component_nicename(st), st->es_cc_log.count);
      }
      st->es_cc = (cc + 1) & 0xf;
    }

  }

  if(st->es_type != SCT_CA &&
     streaming_pad_probe_type(&t->s_streaming_pad, SMT_PACKET))
    skip_mpeg_ts((service_t*)t, st, tsb, len);

skip_cc:
  if(streaming_pad_probe_type(&t->s_streaming_pad, SMT_MPEGTS))
    ts_skip(t, tsb, len);

  for(off = 0; off < t->s_masters.is_count; off++) {
    m = (mpegts_service_t *)t->s_masters.is_array[off];
    pthread_mutex_lock(&m->s_stream_mutex);
    if(streaming_pad_probe_type(&m->s_streaming_pad, SMT_MPEGTS)) {
      pid = (tsb[1] & 0x1f) << 8 | tsb[2];
      if (mpegts_pid_rexists(t->s_slaves_pids, pid))
        ts_skip(m, tsb, len);
    }
    pthread_mutex_unlock(&m->s_stream_mutex);
  }
}

/**
 * Process service stream packets, extract PCR and optionally descramble
 */
int
ts_recv_packet1
  (mpegts_service_t *t, const uint8_t *tsb, int len, int table)
{
  elementary_stream_t *st;
  int pid, r;
  int error = 0;
  
  /* Error */
  if (tsb[1] & 0x80)
    error = 1;

#if 0
  printf("%02X %02X %02X %02X %02X %02X\n",
         tsb[0], tsb[1], tsb[2], tsb[3], tsb[4], tsb[5]);
#endif

  /* Service inactive - ignore */
  if(t->s_status != SERVICE_RUNNING)
    return 0;

  pthread_mutex_lock(&t->s_stream_mutex);

  service_set_streaming_status_flags((service_t*)t, TSS_INPUT_HARDWARE);

  if(error) {
    /* Transport Error Indicator */
    if (tvhlog_limit(&t->s_tei_log, 10))
      tvhwarn("TS", "%s Transport error indicator (total %zi)",
              service_nicename((service_t*)t), t->s_tei_log.count);
  }

  pid = (tsb[1] & 0x1f) << 8 | tsb[2];

  st = service_stream_find((service_t*)t, pid);

  if((st == NULL) && (pid != t->s_pcr_pid) && !table) {
    pthread_mutex_unlock(&t->s_stream_mutex);
    return 0;
  }

  if(!error)
    service_set_streaming_status_flags((service_t*)t, TSS_INPUT_SERVICE);

  if(!t->s_scrambled_pass &&
     ((tsb[3] & 0xc0) ||
       (t->s_scrambled_seen && st && st->es_type != SCT_CA))) {

    /**
     * Lock for descrambling, but only if packet was not in error
     */
    if(!error)
      t->s_scrambled_seen |= service_is_encrypted((service_t*)t);

    /* scrambled stream */
    r = descrambler_descramble((service_t *)t, st, tsb, len);
    if(r > 0) {
      pthread_mutex_unlock(&t->s_stream_mutex);
      return 1;
    }

    if(!error && service_is_encrypted((service_t*)t)) {
      if(r == 0) {
        service_set_streaming_status_flags((service_t*)t, TSS_NO_DESCRAMBLER);
      } else {
        service_set_streaming_status_flags((service_t*)t, TSS_NO_ACCESS);
      }
    }

  } else {
    ts_recv_packet0(t, st, tsb, len);
  }
  pthread_mutex_unlock(&t->s_stream_mutex);
  return 1;
}


/*
 * Process transport stream packets, simple version
 */
void
ts_recv_packet2(mpegts_service_t *t, const uint8_t *tsb, int len)
{
  elementary_stream_t *st;
  int pid, len2;

  for ( ; len > 0; tsb += len2, len -= len2 ) {
    len2 = mpegts_word_count(tsb, len, 0xFF9FFFD0);
    pid = (tsb[1] & 0x1f) << 8 | tsb[2];
    if((st = service_stream_find((service_t*)t, pid)) != NULL)
      ts_recv_packet0(t, st, tsb, len2);
  }
}

/*
 * Process transport stream packets, simple version
 */
void
ts_skip_packet2(mpegts_service_t *t, const uint8_t *tsb, int len)
{
  elementary_stream_t *st;
  int pid, len2;

  for ( ; len > 0; tsb += len2, len -= len2 ) {
    len2 = mpegts_word_count(tsb, len, 0xFF9FFFD0);
    pid = (tsb[1] & 0x1f) << 8 | tsb[2];
    if((st = service_stream_find((service_t*)t, pid)) != NULL)
      ts_recv_skipped0(t, st, tsb, len2);
  }
}

/*
 *
 */
void
ts_recv_raw(mpegts_service_t *t, const uint8_t *tsb, int len)
{
  int pid, parent = 0;

  pthread_mutex_lock(&t->s_stream_mutex);
  service_set_streaming_status_flags((service_t*)t, TSS_MUX_PACKETS);
  if (!LIST_EMPTY(&t->s_slaves)) {
    /* If PID is owned by a slave service, let parent service to
     * deliver this PID (decrambling)
     */
    pid = (tsb[1] & 0x1f) << 8 | tsb[2];
    parent = mpegts_pid_rexists(t->s_slaves_pids, pid);
    service_set_streaming_status_flags((service_t*)t, TSS_PACKETS);
    t->s_streaming_live |= TSS_LIVE;
  }
  if (!parent) {
    if (streaming_pad_probe_type(&t->s_streaming_pad, SMT_MPEGTS)) {
      ts_remux(t, tsb, len, 0);
    } else {
      /* No subscriber - set OK markers */
      service_set_streaming_status_flags((service_t*)t, TSS_PACKETS);
      t->s_streaming_live |= TSS_LIVE;
    }
  }
  pthread_mutex_unlock(&t->s_stream_mutex);
}

/**
 *
 */
static void
ts_flush(mpegts_service_t *t, sbuf_t *sb)
{
  streaming_message_t sm;
  pktbuf_t *pb;

  t->s_tsbuf_last = mclk();

  pb = pktbuf_alloc(sb->sb_data, sb->sb_ptr);
  pb->pb_err = sb->sb_err;

  sm.sm_type = SMT_MPEGTS;
  sm.sm_data = pb;
  streaming_pad_deliver(&t->s_streaming_pad, streaming_msg_clone(&sm));

  pktbuf_ref_dec(pb);

  service_set_streaming_status_flags((service_t*)t, TSS_PACKETS);
  t->s_streaming_live |= TSS_LIVE;

  sbuf_reset(sb, 2*TS_REMUX_BUFSIZE);
}

/**
 *
 */
static void
ts_remux(mpegts_service_t *t, const uint8_t *src, int len, int errors)
{
  sbuf_t *sb = &t->s_tsbuf;

  if (sb->sb_data == NULL)
    sbuf_init_fixed(sb, TS_REMUX_BUFSIZE);

  sbuf_append(sb, src, len);
  sb->sb_err += errors;

  if(monocmpfastsec(mclk(), t->s_tsbuf_last) && sb->sb_ptr < TS_REMUX_BUFSIZE)
    return;

  ts_flush(t, sb);
}

/**
 *
 */
static void
ts_skip(mpegts_service_t *t, const uint8_t *src, int len)
{
  sbuf_t *sb = &t->s_tsbuf;

  if (len < 188)
    return;

  if (sb->sb_data == NULL)
    sbuf_init_fixed(sb, TS_REMUX_BUFSIZE);

  sb->sb_err += len / 188;

  if(monocmpfastsec(mclk(), t->s_tsbuf_last) &&
     sb->sb_err < (TS_REMUX_BUFSIZE / 188))
    return;

  ts_flush(t, sb);
  service_send_streaming_status((service_t *)t);
}

/*
 * Attempt to re-sync a ts stream (3 valid sync's in a row)
 */
int
ts_resync ( const uint8_t *tsb, int *len, int *idx )
{
  int err = 1;
  while (err && (*len > 376)) {
    (*idx)++; (*len)--;
    err = (tsb[*idx] != 0x47) || (tsb[*idx+188] != 0x47) || 
          (tsb[*idx+376] != 0x47);
  }
  return err;
}
