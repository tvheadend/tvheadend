/*
 *  tvheadend, MPEG transport stream functions
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

#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include <libhts/htscfg.h>

#include <ffmpeg/avcodec.h>

#include "tvhead.h"
#include "dispatch.h"
#include "dvb_dvr.h"
#include "teletext.h"
#include "transports.h"
#include "subscriptions.h"

#include "v4l.h"
#include "dvb_dvr.h"
#include "iptv_input.h"
#include "psi.h"


/*
 * Process transport stream packets
 */
void
ts_recv_tsb(th_transport_t *t, int pid, uint8_t *tsb, int scanpcr, int64_t pcr)
{
  th_pid_t *pi = NULL;
  th_subscription_t *s;
  int cc, err = 0, afc, afl = 0;
  int len, pusi;

  LIST_FOREACH(pi, &t->tht_pids, tp_link) 
    if(pi->tp_pid == pid)
      break;

  if(pi == NULL)
    return;

  avgstat_add(&t->tht_rate, 188, dispatch_clock);

  afc = (tsb[3] >> 4) & 3;

  if(afc & 1) {
    cc = tsb[3] & 0xf;
    if(pi->tp_cc_valid && cc != pi->tp_cc) {
      /* Incorrect CC */
      avgstat_add(&t->tht_cc_errors, 1, dispatch_clock);
      err = 1;
    }
    pi->tp_cc_valid = 1;
    pi->tp_cc = (cc + 1) & 0xf;
  }

  if(afc & 2) {
    afl = tsb[4] + 1;

    if(afl > 0 && scanpcr && tsb[5] & 0x10) {
      pcr  = (uint64_t)tsb[6] << 25;
      pcr |= (uint64_t)tsb[7] << 17;
      pcr |= (uint64_t)tsb[8] << 9;
      pcr |= (uint64_t)tsb[9] << 1;
      pcr |= (uint64_t)(tsb[10] >> 7) & 0x01;
    }
  }
  
  switch(pi->tp_type) {

  case HTSTV_TABLE:
    if(pi->tp_section == NULL)
      pi->tp_section = calloc(1, sizeof(struct psi_section));

    afl += 4;
    if(err || afl >= 188) {
      pi->tp_section->ps_offset = -1; /* hold parser until next pusi */
      break;
    }

    pusi = tsb[1] & 0x40;

    if(pusi) {
      len = tsb[afl++];
      if(len > 0) {
	if(len > 188 - afl)
	  break;
	if(!psi_section_reassemble(pi->tp_section, tsb + afl, len, 0, 1))
	  pi->tp_got_section(t, pi, pi->tp_section->ps_data,
			     pi->tp_section->ps_offset);

	afl += len;
      }
    }
    
    if(!psi_section_reassemble(pi->tp_section, tsb + afl, 188 - afl, pusi, 1))
      pi->tp_got_section(t, pi, pi->tp_section->ps_data,
			 pi->tp_section->ps_offset);
    break;

  case HTSTV_TELETEXT:
    teletext_input(t, tsb);
    break;

  default:
    LIST_FOREACH(s, &t->tht_subscriptions, ths_transport_link) {
      s->ths_total_err += err;
      s->ths_callback(s, tsb, pi, pcr);
    }
    break;
  }
}



/*
 *
 */
th_pid_t *
ts_add_pid(th_transport_t *t, uint16_t pid, tv_streamtype_t type)
{
  th_pid_t *pi;
  int i = 0;
  LIST_FOREACH(pi, &t->tht_pids, tp_link) {
    i++;
    if(pi->tp_pid == pid)
      return pi;
  }

  pi = calloc(1, sizeof(th_pid_t));
  pi->tp_index = i;
  pi->tp_pid = pid;
  pi->tp_type = type;
  pi->tp_demuxer_fd = -1;

  LIST_INSERT_HEAD(&t->tht_pids, pi, tp_link);
  return pi;
}
