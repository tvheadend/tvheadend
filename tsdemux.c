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

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include <libhts/htscfg.h>

#include <ffmpeg/avcodec.h>

#include "tvhead.h"
#include "dispatch.h"
#include "teletext.h"
#include "transports.h"
#include "subscriptions.h"
#include "psi.h"
#include "buffer.h"
#include "tsdemux.h"
#include "parsers.h"


/**
 * Code for dealing with a complete section
 */

static void
got_section(th_transport_t *t, th_stream_t *st)
{
  th_descrambler_t *td;

  LIST_FOREACH(td, &t->tht_descramblers, td_transport_link)
    td->td_table(td, t, st, 
		 st->st_section->ps_data, st->st_section->ps_offset);

  if(st->st_got_section != NULL)
    st->st_got_section(t, st, st->st_section->ps_data,
		       st->st_section->ps_offset);
}



/**
 * Continue processing of transport stream packets
 */
static void
ts_recv_packet0(th_transport_t *t, th_stream_t *st, uint8_t *tsb)
{
  th_subscription_t *s;
  int off, len, pusi, cc, err = 0;

  LIST_FOREACH(s, &t->tht_subscriptions, ths_transport_link)
    if(s->ths_raw_input != NULL)
      s->ths_raw_input(s, tsb, 188, st, s->ths_opaque);

  /* Check CC */

  if(tsb[3] & 0x10) {
    cc = tsb[3] & 0xf;
    if(st->st_cc_valid && cc != st->st_cc) {
      /* Incorrect CC */
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

  case HTSTV_TABLE:
    if(st->st_section == NULL)
      st->st_section = calloc(1, sizeof(struct psi_section));

    if(err || off >= 188) {
      st->st_section->ps_offset = -1; /* hold parser until next pusi */
      break;
    }

    if(pusi) {
      len = tsb[off++];
      if(len > 0) {
	if(len > 188 - off)
	  break;
	if(!psi_section_reassemble(st->st_section, tsb + off, len, 0,
				   st->st_section_docrc))
	  got_section(t, st);
	off += len;
      }
    }
    
    if(!psi_section_reassemble(st->st_section, tsb + off, 188 - off, pusi,
			       st->st_section_docrc))
      got_section(t, st);
    break;

  case HTSTV_TELETEXT:
    teletext_input(t, tsb);
    break;

  default:
    if(off > 188)
      break;

    parse_raw_mpeg(t, st, tsb + off, 188 - off, pusi, err);
    break;
  }
}


/**
 * Process transport stream packets, extract PCR and optionally descramble
 */
static void
ts_extract_pcr(th_transport_t *t, th_stream_t *st, uint8_t *tsb)
{
  int64_t real = getclock_hires();
  int64_t pcr;

  pcr  = tsb[6] << 25;
  pcr |= tsb[7] << 17;
  pcr |= tsb[8] << 9;
  pcr |= tsb[9] << 1;
  pcr |= (tsb[10] >> 7) & 0x01;

  pcr = av_rescale_q(pcr, st->st_tb, AV_TIME_BASE_Q);


  {
    static int64_t pcr0;
    static int64_t real0, dv;

    int64_t pcr_d, real_d, dx;

    pcr_d = pcr - pcr0;
    real_d = real - real0;

    dx = pcr_d - real_d;

    if(real0 && dx > -1000000LL && dx < 1000000LL)
      dv += dx;
#if 0
    printf("B: realtime = %-12lld pcr = %-12lld delta = %lld\n",
	   real_d, pcr_d, dv / 1000);
#endif
    pcr0 = pcr;
    real0 = real;
  }
}

/**
 * Process transport stream packets, extract PCR and optionally descramble
 */
void
ts_recv_packet1(th_transport_t *t, uint8_t *tsb)
{
  th_stream_t *st;
  int pid;
  th_descrambler_t *td;

  pid = (tsb[1] & 0x1f) << 8 | tsb[2];

  LIST_FOREACH(st, &t->tht_streams, st_link) 
    if(st->st_pid == pid)
      break;

  if(st == NULL)
    return;

  avgstat_add(&t->tht_rate, 188, dispatch_clock);

  /* Extract PCR */
  if(tsb[3] & 0x20 && tsb[4] > 0 && tsb[5] & 0x10)
    ts_extract_pcr(t, st, tsb);


  if(tsb[3] & 0xc0) {
    /* scrambled stream */
    LIST_FOREACH(td, &t->tht_descramblers, td_transport_link)
      if(td->td_descramble(td, t, st, tsb))
	break;
    
    return;
  }

  ts_recv_packet0(t, st, tsb);
}


/*
 * Process transport stream packets, simple version
 */
void
ts_recv_packet2(th_transport_t *t, uint8_t *tsb)
{
  th_stream_t *st;
  int pid;

  pid = (tsb[1] & 0x1f) << 8 | tsb[2];
  LIST_FOREACH(st, &t->tht_streams, st_link) 
    if(st->st_pid == pid)
      break;

  if(st == NULL)
    return;

  ts_recv_packet0(t, st, tsb);
}
