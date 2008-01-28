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
#include "pes.h"
#include "psi.h"
#include "buffer.h"
#include "tsdemux.h"


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



/*
 * Process transport stream packets
 */
void
ts_recv_packet(th_transport_t *t, int pid, uint8_t *tsb, int dodescramble)
{
  th_stream_t *st = NULL;
  th_subscription_t *s;
  int cc, err = 0, afc, afl = 0;
  int len, pusi;
  th_descrambler_t *td;

  LIST_FOREACH(st, &t->tht_streams, st_link) 
    if(st->st_pid == pid)
      break;

  if(st == NULL)
    return;

  if(((tsb[3] >> 6) & 3) && dodescramble)
    LIST_FOREACH(td, &t->tht_descramblers, td_transport_link)
      if(td->td_descramble(td, t, st, tsb))
	return;
    
  avgstat_add(&t->tht_rate, 188, dispatch_clock);
  if((tsb[3] >> 6) & 3)
    return; /* channel is encrypted */


  LIST_FOREACH(s, &t->tht_subscriptions, ths_transport_link)
    if(s->ths_raw_input != NULL)
      s->ths_raw_input(s, tsb, 188, st, s->ths_opaque);

  afc = (tsb[3] >> 4) & 3;

  if(afc & 1) {
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


  if(afc & 2)
    afl = tsb[4] + 1;
  
  pusi = tsb[1] & 0x40;

  afl += 4;

  switch(st->st_type) {

  case HTSTV_TABLE:
    if(st->st_section == NULL)
      st->st_section = calloc(1, sizeof(struct psi_section));

    if(err || afl >= 188) {
      st->st_section->ps_offset = -1; /* hold parser until next pusi */
      break;
    }

    if(pusi) {
      len = tsb[afl++];
      if(len > 0) {
	if(len > 188 - afl)
	  break;
	if(!psi_section_reassemble(st->st_section, tsb + afl, len, 0,
				   st->st_section_docrc))
	  got_section(t, st);
	afl += len;
      }
    }
    
    if(!psi_section_reassemble(st->st_section, tsb + afl, 188 - afl, pusi,
			       st->st_section_docrc))
      got_section(t, st);
    break;

  case HTSTV_TELETEXT:
    teletext_input(t, tsb);
    break;

  default:
    if(afl > 188)
      break;

    pes_reassembly(t, st, tsb + afl, 188 - afl, pusi, err);
    break;
  }
}
