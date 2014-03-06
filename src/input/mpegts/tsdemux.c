/*
 *  tvheadend, MPEG transport stream demuxer
 *  Copyright (C) 2007 Andreas �man
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
#include "input/mpegts.h"
#include "parsers/parser_teletext.h"
#include "tsdemux.h"

#define TS_REMUX_BUFSIZE (188 * 100)

static void ts_remux(mpegts_service_t *t, const uint8_t *tsb);

/**
 * Code for dealing with a complete section
 */
static void
got_ca_section(const uint8_t *data, size_t len, void *opaque)
{
  th_descrambler_t *td;
  elementary_stream_t *st = opaque;
  assert(st->es_service->s_source_type == S_MPEG_TS);
  mpegts_service_t *t = (mpegts_service_t*)st->es_service;

  LIST_FOREACH(td, &t->s_descramblers, td_service_link)
    td->td_table(td, (service_t*)t, st, data, len);
}

/**
 * Continue processing of transport stream packets
 */
static void
ts_recv_packet0
  (mpegts_service_t *t, elementary_stream_t *st, const uint8_t *tsb)
{
  int off, pusi, cc, error;

  service_set_streaming_status_flags((service_t*)t, TSS_MUX_PACKETS);

  if(streaming_pad_probe_type(&t->s_streaming_pad, SMT_MPEGTS))
    ts_remux(t, tsb);

  if (!st)
    return;

  error = !!(tsb[1] & 0x80);
  pusi  = !!(tsb[1] & 0x40);

  /* Check CC */

  if(tsb[3] & 0x10) {
    cc = tsb[3] & 0xf;
    if(st->es_cc_valid && cc != st->es_cc) {
      /* Incorrect CC */
      limitedlog(&st->es_loglimit_cc, "TS", service_component_nicename(st),
     "Continuity counter error");
      avgstat_add(&t->s_cc_errors, 1, dispatch_clock);
      avgstat_add(&st->es_cc_errors, 1, dispatch_clock);

      // Mark as error if this is not the first packet of a payload
      if(!pusi)
        error |= 0x2;
    }
    st->es_cc_valid = 1;
    st->es_cc = (cc + 1) & 0xf;
  }

  off = tsb[3] & 0x20 ? tsb[4] + 5 : 4;

  switch(st->es_type) {

  case SCT_CA:
    if(st->es_section == NULL)
      st->es_section = calloc(1, sizeof(mpegts_psi_section_t));
    mpegts_psi_section_reassemble(st->es_section, tsb, 0, got_ca_section, st);
    break;

  default:
    if(!streaming_pad_probe_type(&t->s_streaming_pad, SMT_PACKET))
      break;

    if(st->es_type == SCT_TELETEXT)
      teletext_input(t, st, tsb);

    if(off > 188)
      break;

    if(t->s_status == SERVICE_RUNNING)
      parse_mpeg_ts((service_t*)t, st, tsb + off, 188 - off, pusi, error);
    break;
  }
}

/**
 * Recover PCR
 *
 * st->es_pcr_drift will increase if our (system clock) runs faster
 * than the stream PCR
 */
static void
ts_process_pcr(mpegts_service_t *t, elementary_stream_t *st, int64_t pcr)
{
  int64_t real, d;
  
  if(st == NULL)
    return;

  real = getmonoclock();

  if(st->es_pcr_real_last != PTS_UNSET) {
    d = (real - st->es_pcr_real_last) - (pcr - st->es_pcr_last);
    
    if(d < -90000LL || d > 90000LL) {
      st->es_pcr_recovery_fails++;
      if(st->es_pcr_recovery_fails > 10) {
  st->es_pcr_recovery_fails = 0;
  st->es_pcr_real_last = PTS_UNSET;
      }
      return;
    }
    st->es_pcr_recovery_fails = 0;
    st->es_pcr_drift += d;
    
    if(t->s_pcr_pid == st->es_pid) {
      /* This is the registered PCR PID, adjust service PCR drift
   via an IIR filter */
      
      t->s_pcr_drift = (t->s_pcr_drift * 255 + st->es_pcr_drift) / 256;
    }
  }
  st->es_pcr_last = pcr;
  st->es_pcr_real_last = real;
}

/**
 * Process service stream packets, extract PCR and optionally descramble
 */
int
ts_recv_packet1
  (mpegts_service_t *t, const uint8_t *tsb, int64_t *pcrp, int table)
{
  elementary_stream_t *st;
  int pid, n, m, r;
  th_descrambler_t *td;
  int error = 0;
  int64_t pcr = PTS_UNSET;
  
  /* Error */
  if (tsb[1] & 0x80)
    error = 1;

#if 0
  printf("%02X %02X %02X %02X %02X %02X\n",
         tsb[0], tsb[1], tsb[2], tsb[3], tsb[4], tsb[5]);
#endif

  /* Extract PCR (do this early for tsfile) */
  if(tsb[3] & 0x20 && tsb[4] > 0 && tsb[5] & 0x10 && !error) {
    pcr  = (uint64_t)tsb[6] << 25;
    pcr |= (uint64_t)tsb[7] << 17;
    pcr |= (uint64_t)tsb[8] << 9;
    pcr |= (uint64_t)tsb[9] << 1;
    pcr |= ((uint64_t)tsb[10] >> 7) & 0x01;
    if (pcrp) *pcrp = pcr;
  }

  /* Nothing - special case for tsfile to get PCR */
  if (!t) return 0;

  /* Service inactive - ignore */
  if(t->s_status != SERVICE_RUNNING)
    return 0;

  pthread_mutex_lock(&t->s_stream_mutex);

  service_set_streaming_status_flags((service_t*)t, TSS_INPUT_HARDWARE);

  if(error) {
    /* Transport Error Indicator */
    limitedlog(&t->s_loglimit_tei, "TS", service_nicename((service_t*)t),
         "Transport error indicator");
  }

  pid = (tsb[1] & 0x1f) << 8 | tsb[2];

  st = service_stream_find((service_t*)t, pid);

  /* Extract PCR */
  if (pcr != PTS_UNSET)
    ts_process_pcr(t, st, pcr);

  if((st == NULL) && (pid != t->s_pcr_pid) && !table) {
    pthread_mutex_unlock(&t->s_stream_mutex);
    return 0;
  }

  if(!error)
    service_set_streaming_status_flags((service_t*)t, TSS_INPUT_SERVICE);

  avgstat_add(&t->s_rate, 188, dispatch_clock);

  if((tsb[3] & 0xc0) ||
      (t->s_scrambled_seen && st && st->es_type != SCT_CA)) {

    /**
     * Lock for descrambling, but only if packet was not in error
     */
    if(!error)
      t->s_scrambled_seen = t->s_scrambled;

    /* scrambled stream */
    n = m = 0;

    LIST_FOREACH(td, &t->s_descramblers, td_service_link) {
      n++;
      
      r = td->td_descramble(td, (service_t*)t, st, tsb);
      if(r == 0) {
        pthread_mutex_unlock(&t->s_stream_mutex);
        return 1;
      }

      if(r == 1)
        m++;
    }

    if(!error && t->s_scrambled != 0) {
      if(n == 0) {
        service_set_streaming_status_flags((service_t*)t, TSS_NO_DESCRAMBLER);
      } else if(m == n) {
        service_set_streaming_status_flags((service_t*)t, TSS_NO_ACCESS);
      }
    }

  } else {
    ts_recv_packet0(t, st, tsb);
  }
  pthread_mutex_unlock(&t->s_stream_mutex);
  return 1;
}


/*
 * Process transport stream packets, simple version
 */
void
ts_recv_packet2(mpegts_service_t *t, const uint8_t *tsb)
{
  elementary_stream_t *st;
  int pid = (tsb[1] & 0x1f) << 8 | tsb[2];

  if((st = service_stream_find((service_t*)t, pid)) != NULL)
    ts_recv_packet0(t, st, tsb);
}


/**
 *
 */
static void
ts_remux(mpegts_service_t *t, const uint8_t *src)
{
  streaming_message_t sm;
  pktbuf_t *pb;
  sbuf_t *sb = &t->s_tsbuf;

  sbuf_append(sb, src, 188);

  if(sb->sb_ptr < TS_REMUX_BUFSIZE) 
    return;

  pb = pktbuf_alloc(sb->sb_data, sb->sb_ptr);

  sm.sm_type = SMT_MPEGTS;
  sm.sm_data = pb;
  streaming_pad_deliver(&t->s_streaming_pad, &sm);

  pktbuf_ref_dec(pb);

  service_set_streaming_status_flags((service_t*)t, TSS_PACKETS);

  sbuf_reset(sb);
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
