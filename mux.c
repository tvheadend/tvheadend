/*
 *  tvheadend, Muxing
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

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "dispatch.h"
#include "teletext.h"
#include "transports.h"
#include "subscriptions.h"
#include "psi.h"
#include "buffer.h"




/*
 * pause playback
 */
void
muxer_pause(th_muxer_t *tm)
{
}




/*
 * playback start
 */
void
muxer_play(th_muxer_t *tm, int64_t toffset)
{
  th_subscription_t *s = tm->tm_subscription;

  if(!tm->tm_linked) {
    LIST_INSERT_HEAD(&s->ths_transport->tht_muxers, tm, tm_transport_link);
    tm->tm_linked = 1;
  }

  if(toffset == AV_NOPTS_VALUE) {
    /* continue from last playback */
    tm->tm_offset = 0;
  } else {
    tm->tm_offset = toffset;
  }
  tm->tm_status = TM_PLAY;
}

/**
 *
 */
static void
mux_new_packet_for_stream(th_muxer_t *tm, th_muxstream_t *tms, th_pkt_t *pkt)
{
  if(tm->tm_offset == 0) {
    /* Direct playback, pass it on at once */
    tm->tm_output(tm->tm_opaque, tms, pkt);
    return;
  }
}



/**
 *
 */
static void
mux_new_packet(th_muxer_t *tm, th_stream_t *st, th_pkt_t *pkt)
{
  th_muxstream_t *tms;

  pkt_store(pkt);  /* need to keep packet around */

  switch(tm->tm_status) {
  case TM_IDLE:
    break;
    
  case TM_WAITING_FOR_LOCK:
    break;

  case TM_PLAY:
    LIST_FOREACH(tms, &tm->tm_streams, tms_muxer_link0) {
      if(tms->tms_stream == st) {
	mux_new_packet_for_stream(tm, tms, pkt);
	break;
      }
    }
    break;
    
  case TM_PAUSE:
    break;
  }
}





/*
 * TS Muxer
 */
th_muxer_t *
muxer_init(th_subscription_t *s, th_mux_output_t *cb, void *opaque,
	      int flags)
{
  th_transport_t *t = s->ths_transport;
  th_stream_t *st;
  th_muxer_t *tm;
  th_muxstream_t *tms;

  tm = calloc(1, sizeof(th_muxer_t));
  tm->tm_subscription = s;

  tm->tm_output = cb;
  tm->tm_opaque = opaque;
  tm->tm_new_pkt = mux_new_packet;


  LIST_FOREACH(st, &t->tht_streams, st_link) {

    switch(st->st_type) {
    case HTSTV_MPEG2VIDEO:
    case HTSTV_MPEG2AUDIO:
    case HTSTV_AC3:
    case HTSTV_H264:
      break;

    default:
      continue;
    }

    tms = calloc(1, sizeof(th_muxstream_t));
    tms->tms_muxer = tm;
    tms->tms_stream = st;

 
    LIST_INSERT_HEAD(&tm->tm_streams, tms, tms_muxer_link0);
  }

  s->ths_muxer = tm;
  return tm;
}


/*
 *
 */
static void
tms_destroy(th_muxstream_t *tms)
{
  LIST_REMOVE(tms, tms_muxer_link0);

  //  dtimer_disarm(&tms->tms_timer);
  free(tms);
}


/*
 *
 */
void
muxer_deinit(th_muxer_t *tm, th_subscription_t *s)
{
  th_muxstream_t *tms;

  s->ths_raw_input = NULL;
  s->ths_muxer = NULL;

  if(tm->tm_linked)
    LIST_REMOVE(tm, tm_transport_link);

  while((tms = LIST_FIRST(&tm->tm_streams)) != NULL)
    tms_destroy(tms);

  free(tm);
}

