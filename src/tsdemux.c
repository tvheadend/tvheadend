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

#include "tvhead.h"
#include "teletext.h"
#include "transports.h"
#include "subscriptions.h"
#include "psi.h"
#include "tsdemux.h"
#include "parsers.h"
#include "streaming.h"

static void ts_remux(th_transport_t *t, const uint8_t *tsb);

/**
 * Code for dealing with a complete section
 */
static void
got_section(const uint8_t *data, size_t len, void *opaque)
{
  th_descrambler_t *td;
  th_stream_t *st = opaque;
  th_transport_t *t = st->st_transport;

  if(st->st_type == SCT_CA) {
    LIST_FOREACH(td, &t->tht_descramblers, td_transport_link)
      td->td_table(td, t, st, data, len);
  } else if(st->st_got_section != NULL) {
    st->st_got_section(t, st, data, len);
  }
}


/**
 * Continue processing of transport stream packets
 */
static void
ts_recv_packet0(th_transport_t *t, th_stream_t *st, const uint8_t *tsb)
{
  int off, pusi, cc, err = 0;

  transport_set_streaming_status_flags(t, TSS_MUX_PACKETS);

  if(streaming_pad_probe_type(&t->tht_streaming_pad, SMT_MPEGTS))
    ts_remux(t, tsb);

  /* Check CC */

  if(tsb[3] & 0x10) {
    cc = tsb[3] & 0xf;
    if(st->st_cc_valid && cc != st->st_cc) {
      /* Incorrect CC */
      limitedlog(&st->st_loglimit_cc, "TS", transport_component_nicename(st),
		 "Continuity counter error");
      avgstat_add(&t->tht_cc_errors, 1, dispatch_clock);
      avgstat_add(&st->st_cc_errors, 1, dispatch_clock);
      err = 1;
    }
    st->st_cc_valid = 1;
    st->st_cc = (cc + 1) & 0xf;
  }

  off = tsb[3] & 0x20 ? tsb[4] + 5 : 4;
  pusi = tsb[1] & 0x40;

  switch(st->st_type) {

  case SCT_CA:
  case SCT_PAT:
  case SCT_PMT:
    if(st->st_section == NULL)
      st->st_section = calloc(1, sizeof(struct psi_section));

    psi_section_reassemble(st->st_section, tsb, st->st_section_docrc,
			   got_section, st);
    break;

  case SCT_TELETEXT:
    teletext_input(t, st, tsb);
    break;

  default:
    if(off > 188)
      break;

    if(t->tht_status == TRANSPORT_RUNNING)
      parse_mpeg_ts(t, st, tsb + off, 188 - off, pusi, err);
    break;
  }
}

static const AVRational mpeg_tc = {1, 90000};

/**
 * Recover PCR
 *
 * st->st_pcr_drift will increase if our (system clock) runs faster
 * than the stream PCR
 */
static void
ts_extract_pcr(th_transport_t *t, th_stream_t *st, const uint8_t *tsb, 
	       int64_t *pcrp)
{
  int64_t real, pcr, d;

  pcr  = (uint64_t)tsb[6] << 25;
  pcr |= (uint64_t)tsb[7] << 17;
  pcr |= (uint64_t)tsb[8] << 9;
  pcr |= (uint64_t)tsb[9] << 1;
  pcr |= ((uint64_t)tsb[10] >> 7) & 0x01;
  
  pcr = av_rescale_q(pcr, mpeg_tc, AV_TIME_BASE_Q);

  if(pcrp != NULL)
    *pcrp = pcr;

  if(st == NULL)
    return;

  real = getmonoclock();

  if(st->st_pcr_real_last != AV_NOPTS_VALUE) {
    d = (real - st->st_pcr_real_last) - (pcr - st->st_pcr_last);
    
    if(d < -1000000LL || d > 1000000LL) {
      st->st_pcr_recovery_fails++;
      if(st->st_pcr_recovery_fails > 10) {
	st->st_pcr_recovery_fails = 0;
	st->st_pcr_real_last = AV_NOPTS_VALUE;
      }
      return;
    }
    st->st_pcr_recovery_fails = 0;
    st->st_pcr_drift += d;
    
    if(t->tht_pcr_pid == st->st_pid) {
      /* This is the registered PCR PID, adjust transport PCR drift
	 via an IIR filter */
      
      t->tht_pcr_drift = (t->tht_pcr_drift * 255 + st->st_pcr_drift) / 256;
    }
  }
  st->st_pcr_last = pcr;
  st->st_pcr_real_last = real;
}

/**
 * Process transport stream packets, extract PCR and optionally descramble
 */
void
ts_recv_packet1(th_transport_t *t, const uint8_t *tsb, int64_t *pcrp)
{
  th_stream_t *st;
  int pid, n, m, r;
  th_descrambler_t *td;

  if(t->tht_status != TRANSPORT_RUNNING)
    return;

  pthread_mutex_lock(&t->tht_stream_mutex);

  transport_set_streaming_status_flags(t, TSS_INPUT_HARDWARE);

  if(tsb[1] & 0x80) {
    /* Transport Error Indicator */
    limitedlog(&t->tht_loglimit_tei, "TS", transport_nicename(t),
	       "Transport error indicator");
    pthread_mutex_unlock(&t->tht_stream_mutex);
    return;
  }

  pid = (tsb[1] & 0x1f) << 8 | tsb[2];

  st = transport_find_stream_by_pid(t, pid);

  /* Extract PCR */
  if(tsb[3] & 0x20 && tsb[4] > 0 && tsb[5] & 0x10)
    ts_extract_pcr(t, st, tsb, pcrp);

  if(st == NULL) {
    pthread_mutex_unlock(&t->tht_stream_mutex);
    return;
  }

  transport_set_streaming_status_flags(t, TSS_INPUT_SERVICE);

  avgstat_add(&t->tht_rate, 188, dispatch_clock);

  if((tsb[3] & 0xc0) ||
      (t->tht_scrambled_seen && st->st_type != SCT_CA &&
       st->st_type != SCT_PAT && st->st_type != SCT_PMT)) {

    t->tht_scrambled_seen = t->tht_scrambled;

    /* scrambled stream */
    n = m = 0;

    LIST_FOREACH(td, &t->tht_descramblers, td_transport_link) {
      n++;
      
      r = td->td_descramble(td, t, st, tsb);
      if(r == 0) {
	pthread_mutex_unlock(&t->tht_stream_mutex);
	return;
      }

      if(r == 1)
	m++;
    }

    if(n == 0) {
      transport_set_streaming_status_flags(t, TSS_NO_DESCRAMBLER);
    } else if(m == n) {
      transport_set_streaming_status_flags(t, TSS_NO_ACCESS);
    }
  } else {
    ts_recv_packet0(t, st, tsb);
  }
  pthread_mutex_unlock(&t->tht_stream_mutex);
}


/*
 * Process transport stream packets, simple version
 */
void
ts_recv_packet2(th_transport_t *t, const uint8_t *tsb)
{
  th_stream_t *st;
  int pid = (tsb[1] & 0x1f) << 8 | tsb[2];

  if((st = transport_find_stream_by_pid(t, pid)) != NULL)
    ts_recv_packet0(t, st, tsb);
}


/**
 *
 */
static void
ts_remux(th_transport_t *t, const uint8_t *src)
{
  uint8_t tsb[188];
  memcpy(tsb, src, 188);

  streaming_message_t sm;
  sm.sm_type = SMT_MPEGTS;
  sm.sm_data = tsb;
  streaming_pad_deliver(&t->tht_streaming_pad, &sm);
}
