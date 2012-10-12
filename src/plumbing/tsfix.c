/**
 *  Timestamp fixup
 *  Copyright (C) 2010 Andreas Öman
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
#include "streaming.h"
#include "tsfix.h"

LIST_HEAD(tfstream_list, tfstream);

#define tsfixprintf(fmt...) // printf(fmt)

/**
 *
 */
typedef struct tfstream {

  LIST_ENTRY(tfstream) tfs_link;

  int tfs_index;

  streaming_component_type_t tfs_type;

  int tfs_bad_dts;
  int64_t tfs_last_dts_norm;
  int64_t tfs_dts_epoch;

  int64_t tfs_last_dts_in;

} tfstream_t;


/**
 *
 */
typedef struct tsfix {
  streaming_target_t tf_input;

  streaming_target_t *tf_output;

  struct tfstream_list tf_streams;
  int tf_hasvideo;
  int64_t tf_tsref;
  time_t tf_start_time;

  struct th_pktref_queue tf_ptsq;

} tsfix_t;


/**
 *
 */
static void
tsfix_destroy_streams(tsfix_t *tf)
{
  tfstream_t *tfs;
  pktref_clear_queue(&tf->tf_ptsq);
  while((tfs = LIST_FIRST(&tf->tf_streams)) != NULL) {
    LIST_REMOVE(tfs, tfs_link);
    free(tfs);
  }
}


static tfstream_t *
tfs_find(tsfix_t *tf, th_pkt_t *pkt)
{
  tfstream_t *tfs;
  LIST_FOREACH(tfs, &tf->tf_streams, tfs_link)
    if(pkt->pkt_componentindex == tfs->tfs_index)
      break;
  return tfs;
}

/**
 *
 */
static void
tsfix_add_stream(tsfix_t *tf, int index, streaming_component_type_t type)
{
  tfstream_t *tfs = calloc(1, sizeof(tfstream_t));

  tfs->tfs_type = type;
  tfs->tfs_index = index;
  tfs->tfs_last_dts_norm = PTS_UNSET;
  tfs->tfs_last_dts_in = PTS_UNSET;
  tfs->tfs_dts_epoch = 0; 

  LIST_INSERT_HEAD(&tf->tf_streams, tfs, tfs_link);
}


/**
 *
 */
static void
tsfix_start(tsfix_t *tf, streaming_start_t *ss)
{
  int i;
  int hasvideo = 0;

  for(i = 0; i < ss->ss_num_components; i++) {
    const streaming_start_component_t *ssc = &ss->ss_components[i];
    tsfix_add_stream(tf, ssc->ssc_index, ssc->ssc_type);
    hasvideo |= SCT_ISVIDEO(ssc->ssc_type);
  }

  TAILQ_INIT(&tf->tf_ptsq);

  tf->tf_tsref = PTS_UNSET;
  tf->tf_hasvideo = hasvideo;
}


/**
 *
 */
static void
tsfix_stop(tsfix_t *tf)
{
  tsfix_destroy_streams(tf);
}


#define PTS_MASK 0x1ffffffffLL

/**
 *
 */
static void
normalize_ts(tsfix_t *tf, tfstream_t *tfs, th_pkt_t *pkt)
{
  int64_t dts, d;

  int checkts = SCT_ISAUDIO(tfs->tfs_type) || SCT_ISVIDEO(tfs->tfs_type);

  if(tf->tf_tsref == PTS_UNSET) {
    pkt_ref_dec(pkt);
    return;
  }

  pkt->pkt_dts &= PTS_MASK;
  pkt->pkt_pts &= PTS_MASK;

  /* Subtract the transport wide start offset */
  dts = pkt->pkt_dts - tf->tf_tsref;

  if(tfs->tfs_last_dts_norm == PTS_UNSET) {
    if(dts < 0) {
      /* Early packet with negative time stamp, drop those */
      pkt_ref_dec(pkt);
      return;
    }
  } else if(checkts) {
    d = dts + tfs->tfs_dts_epoch - tfs->tfs_last_dts_norm;

    if(d < 0 || d > 90000) {

      if(d < -PTS_MASK || d > -PTS_MASK + 180000) {

	tfs->tfs_bad_dts++;

	if(tfs->tfs_bad_dts < 5) {
	  tvhlog(LOG_ERR, "parser", 
		 "transport stream %s, DTS discontinuity. "
		 "DTS = %" PRId64 ", last = %" PRId64,
		 streaming_component_type2txt(tfs->tfs_type),
		 dts, tfs->tfs_last_dts_norm);
	}
      } else {
	/* DTS wrapped, increase upper bits */
	tfs->tfs_dts_epoch += PTS_MASK + 1;
	tfs->tfs_bad_dts = 0;
      }
    } else {
      tfs->tfs_bad_dts = 0;
    }
  }

  dts += tfs->tfs_dts_epoch;
  tfs->tfs_last_dts_norm = dts;

  if(pkt->pkt_pts != PTS_UNSET) {
    /* Compute delta between PTS and DTS (and watch out for 33 bit wrap) */
    int64_t ptsoff = (pkt->pkt_pts - pkt->pkt_dts) & PTS_MASK;
    
    pkt->pkt_pts = dts + ptsoff;
  }

  pkt->pkt_dts = dts;

  tsfixprintf("TSFIX: %-12s %d %10"PRId64" %10"PRId64" %10d %zd\n",
	      streaming_component_type2txt(tfs->tfs_type),
	      pkt->pkt_frametype,
	      pkt->pkt_dts,
	      pkt->pkt_pts,
	      pkt->pkt_duration,
	      pktbuf_len(pkt->pkt_payload));

  streaming_message_t *sm = streaming_msg_create_pkt(pkt);
  streaming_target_deliver2(tf->tf_output, sm);
  pkt_ref_dec(pkt);
}




static void
recover_pts(tsfix_t *tf, tfstream_t *tfs, th_pkt_t *pkt)
{
  th_pktref_t *pr, *srch;

  pktref_enqueue(&tf->tf_ptsq, pkt);

  while((pr = TAILQ_FIRST(&tf->tf_ptsq)) != NULL) {
    
    pkt = pr->pr_pkt;
    tfs = tfs_find(tf, pkt);

    switch(tfs->tfs_type) {

    case SCT_MPEG2VIDEO:

      switch(pkt->pkt_frametype) {
      case PKT_B_FRAME:
	/* B-frames have same PTS as DTS, pass them on */
	pkt->pkt_pts = pkt->pkt_dts;
	tsfixprintf("TSFIX: %-12s PTS b-frame set to %"PRId64"\n",
		    streaming_component_type2txt(tfs->tfs_type),
		    pkt->pkt_dts);
	break;
      
      case PKT_I_FRAME:
      case PKT_P_FRAME:
	/* Presentation occures at DTS of next I or P frame,
	   try to find it */
	srch = TAILQ_NEXT(pr, pr_link);
	while(1) {
	  if(srch == NULL)
	    return; /* not arrived yet, wait */
	  if(tfs_find(tf, srch->pr_pkt) == tfs && 
	     srch->pr_pkt->pkt_frametype <= PKT_P_FRAME) {
	    pkt->pkt_pts = srch->pr_pkt->pkt_dts;
	    tsfixprintf("TSFIX: %-12s PTS *-frame set to %"PRId64"\n",
			streaming_component_type2txt(tfs->tfs_type),
			pkt->pkt_pts);
	    break;
	  }
	  srch = TAILQ_NEXT(srch, pr_link);
	}
	break;
      }
      break;

    default:
      break;
    }

    TAILQ_REMOVE(&tf->tf_ptsq, pr, pr_link);
    normalize_ts(tf, tfs, pkt);
    free(pr);
  }
}


/**
 * Compute PTS (if not known)
 */
static void
compute_pts(tsfix_t *tf, tfstream_t *tfs, th_pkt_t *pkt)
{
  // If PTS is missing, set it to DTS if not video
  if(pkt->pkt_pts == PTS_UNSET && !SCT_ISVIDEO(tfs->tfs_type)) {
    pkt->pkt_pts = pkt->pkt_dts;
    tsfixprintf("TSFIX: %-12s PTS set to %"PRId64"\n",
		streaming_component_type2txt(tfs->tfs_type),
		pkt->pkt_pts);
  }

  /* PTS known and no other packets in queue, deliver at once */
  if(pkt->pkt_pts != PTS_UNSET && TAILQ_FIRST(&tf->tf_ptsq) == NULL)
    normalize_ts(tf, tfs, pkt);
  else
    recover_pts(tf, tfs, pkt);
}


/**
 *
 */
static void
tsfix_input_packet(tsfix_t *tf, streaming_message_t *sm)
{
  th_pkt_t *pkt = pkt_copy_shallow(sm->sm_data);
  tfstream_t *tfs = tfs_find(tf, pkt);
  streaming_msg_free(sm);
  
  if(tfs == NULL || dispatch_clock < tf->tf_start_time) {
    pkt_ref_dec(pkt);
    return;
  }

  if(tf->tf_tsref == PTS_UNSET &&
     (!tf->tf_hasvideo ||
      (SCT_ISVIDEO(tfs->tfs_type) && pkt->pkt_frametype == PKT_I_FRAME))) {
      tf->tf_tsref = pkt->pkt_dts & PTS_MASK;
      tsfixprintf("reference clock set to %"PRId64"\n", tf->tf_tsref);
  }

  int pdur = pkt->pkt_duration >> pkt->pkt_field;

  if(pkt->pkt_dts == PTS_UNSET) {
    if(tfs->tfs_last_dts_in == PTS_UNSET) {
      pkt_ref_dec(pkt);
      return;
    }

    pkt->pkt_dts = (tfs->tfs_last_dts_in + pdur) & PTS_MASK;

    tsfixprintf("TSFIX: %-12s DTS set to last %"PRId64" +%d == %"PRId64"\n",
		streaming_component_type2txt(tfs->tfs_type),
		tfs->tfs_last_dts_in, pdur, pkt->pkt_dts);
  }

  tfs->tfs_last_dts_in = pkt->pkt_dts;

  compute_pts(tf, tfs, pkt);
}


/**
 *
 */
static void
tsfix_input(void *opaque, streaming_message_t *sm)
{
  tsfix_t *tf = opaque;

  switch(sm->sm_type) {
  case SMT_PACKET:
    tsfix_input_packet(tf, sm);
    return;

  case SMT_START:
    tsfix_start(tf, sm->sm_data);
    break;

  case SMT_STOP:
    tsfix_stop(tf);
    break;

  case SMT_EXIT:
  case SMT_SERVICE_STATUS:
  case SMT_SIGNAL_STATUS:
  case SMT_NOSTART:
  case SMT_MPEGTS:
  case SMT_SPEED:
  case SMT_SKIP:
    break;
  }

  streaming_target_deliver2(tf->tf_output, sm);
}


/**
 *
 */
streaming_target_t *
tsfix_create(streaming_target_t *output)
{
  tsfix_t *tf = calloc(1, sizeof(tsfix_t));

  TAILQ_INIT(&tf->tf_ptsq);

  tf->tf_output = output;
  tf->tf_start_time = dispatch_clock;

  streaming_target_init(&tf->tf_input, tsfix_input, tf, 0);
  return &tf->tf_input;
}

/**
 *
 */
void tsfix_set_start_time(streaming_target_t *pad, time_t start)
{
  tsfix_t *tf = (tsfix_t *)pad;

  tf->tf_start_time = start;
}


/**
 *
 */
void
tsfix_destroy(streaming_target_t *pad)
{
  tsfix_t *tf = (tsfix_t *)pad;

  tsfix_destroy_streams(tf);
  free(tf);
}

