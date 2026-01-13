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

#define REF_OK       0
#define REF_ERROR    1
#define REF_DROP     2

LIST_HEAD(tfstream_list, tfstream);

/**
 *
 */
typedef struct tfstream {

  LIST_ENTRY(tfstream) tfs_link;

  int tfs_index;

  streaming_component_type_t tfs_type;
  uint8_t tfs_video;
  uint8_t tfs_audio;
  uint8_t tfs_subtitle;

  int tfs_bad_dts;
  int64_t tfs_local_ref;
  int64_t tfs_last_dts_norm;
  int64_t tfs_dts_epoch;

  int64_t tfs_last_dts_in;

  int tfs_seen;

  struct tfstream *tfs_parent;

} tfstream_t;


/**
 *
 */
typedef struct tsfix {
  streaming_target_t tf_input;

  streaming_target_t *tf_output;

  struct tfstream_list tf_streams;
  int tf_hasvideo;
  int tf_wait_for_video;
  int64_t tf_tsref;
  int64_t tf_start_time;
  int64_t dts_offset;
  int dts_offset_apply;

  struct th_pktref_queue tf_ptsq;
  struct th_pktref_queue tf_backlog;

} tsfix_t;


/**
 * Compute the timestamp deltas
 */
static int64_t
tsfix_ts_diff(int64_t ts1, int64_t ts2)
{
  int64_t r;
  ts1 &= PTS_MASK;
  ts2 &= PTS_MASK;

  r = llabs(ts1 - ts2);
  if (r > (PTS_MASK / 2)) {
    /* try to wrap the lowest value */
    if (ts1 < ts2)
      ts1 += PTS_MASK + 1;
    else
      ts2 += PTS_MASK + 1;
    return llabs(ts1 - ts2);
  }
  return r;
}

/**
 *
 */
static void
tsfix_destroy_streams(tsfix_t *tf)
{
  tfstream_t *tfs;
  pktref_clear_queue(&tf->tf_ptsq);
  pktref_clear_queue(&tf->tf_backlog);
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
static tfstream_t *
tsfix_add_stream(tsfix_t *tf, int index, streaming_component_type_t type)
{
  tfstream_t *tfs = calloc(1, sizeof(tfstream_t));

  tfs->tfs_type = type;
  if (SCT_ISVIDEO(type))
    tfs->tfs_video = 1;
  else if (SCT_ISAUDIO(type))
    tfs->tfs_audio = 1;
  else if (SCT_ISSUBTITLE(type))
    tfs->tfs_subtitle = 1;

  tfs->tfs_index = index;
  tfs->tfs_local_ref = PTS_UNSET;
  tfs->tfs_last_dts_norm = PTS_UNSET;
  tfs->tfs_last_dts_in = PTS_UNSET;
  tfs->tfs_dts_epoch = 0;
  tfs->tfs_seen = 0;

  LIST_INSERT_HEAD(&tf->tf_streams, tfs, tfs_link);
  return tfs;
}


/**
 *
 */
static void
tsfix_start(tsfix_t *tf, streaming_start_t *ss)
{
  int i, hasvideo = 0, vwait = 0;
  tfstream_t *tfs;

  for(i = 0; i < ss->ss_num_components; i++) {
    const streaming_start_component_t *ssc = &ss->ss_components[i];
    tfs = tsfix_add_stream(tf, ssc->es_index, ssc->es_type);
    if (tfs->tfs_video) {
      if (ssc->es_width == 0 || ssc->es_height == 0)
        /* only first video stream may be valid */
        vwait = !hasvideo ? 1 : 0;
      hasvideo = 1;
    }
  }

  TAILQ_INIT(&tf->tf_ptsq);
  TAILQ_INIT(&tf->tf_backlog);

  tf->tf_tsref = PTS_UNSET;
  tf->tf_hasvideo = hasvideo;
  tf->tf_wait_for_video = vwait;
}


/**
 *
 */
static void
tsfix_stop(tsfix_t *tf)
{
  tsfix_destroy_streams(tf);
}


/**
 *
 */
static void
tsfix_packet_drop(tfstream_t *tfs, th_pkt_t *pkt, const char *reason)
{
  if (tvhtrace_enabled()) {
    char buf[64];
    snprintf(buf, sizeof(buf), "drop %s", reason);
    pkt_trace(LS_TSFIX, pkt, buf);
  }
  pkt_ref_dec(pkt);
}

/**
 *
 */
static void
normalize_ts(tsfix_t *tf, tfstream_t *tfs, th_pkt_t *pkt, int backlog)
{
  int64_t ref, dts, odts, opts, d;

  if(tf->tf_tsref == PTS_UNSET) {
    if (backlog) {
      if (pkt->pkt_dts != PTS_UNSET)
        tfs->tfs_seen = 1;
      pktref_enqueue(&tf->tf_backlog, pkt);
    } else
      pkt_ref_dec(pkt);
    return;
  }

  ref = tfs->tfs_local_ref != PTS_UNSET ? tfs->tfs_local_ref : tf->tf_tsref;
  odts = pkt->pkt_dts;
  opts = pkt->pkt_pts;

  if (pkt->pkt_dts == PTS_UNSET) {
    if (pkt->pkt_pts != PTS_UNSET)
      pkt->pkt_dts = pkt->pkt_pts;
    else
      goto deliver;
  }

  pkt->pkt_dts &= PTS_MASK;

  /* Subtract the transport wide start offset */
  if (tf->dts_offset_apply)
    dts = pts_diff(ref, pkt->pkt_dts + tf->dts_offset);
  else
    dts = pts_diff(ref, pkt->pkt_dts);

  if (tfs->tfs_last_dts_norm == PTS_UNSET) {
    if (dts < 0 || pkt->pkt_err) {
      /* Early packet with negative time stamp, drop those */
      tsfix_packet_drop(tfs, pkt, "negative/error");
      return;
    }
  } else {
    const int64_t nlimit =      -1; /* allow negative values - rounding errors? */
    int64_t low          =   90000; /* one second */
    int64_t upper        = 2*90000; /* two seconds */
    d = dts + tfs->tfs_dts_epoch - tfs->tfs_last_dts_norm;

    if (tfs->tfs_subtitle) {
      /*
       * special conditions for subtitles, because they may be broadcasted
       * with large time gaps
       */
      low   = PTS_MASK / 2; /* more than 13 hours */
      upper = low - 1;
    }

    if (d < nlimit || d > low) {
      if (d < -PTS_MASK || d > -PTS_MASK + upper) {
        if (pkt->pkt_err) {
          tsfix_packet_drop(tfs, pkt, "possible wrong discontinuity");
          return;
        }
	tfs->tfs_bad_dts++;
        if (tfs->tfs_bad_dts < 5) {
	  tvhwarn(LS_TSFIX,
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
    d = ((pkt->pkt_pts & PTS_MASK) - pkt->pkt_dts) & PTS_MASK;
    pkt->pkt_pts = dts + d;
  }
  if(pkt->pkt_pcr != PTS_UNSET) {
    /* Compute delta between PCR and DTS (and watch out for 33 bit wrap) */
    d = ((pkt->pkt_pcr & PTS_MASK) - pkt->pkt_dts) & PTS_MASK;
    if (d > PTS_MASK / 2)
      d = -(PTS_MASK - d);
    pkt->pkt_pcr = dts + d;
  }

  pkt->pkt_dts = dts;

  if (pkt->pkt_pts < 0 || pkt->pkt_dts < 0 || pkt->pkt_pcr < 0) {
    tsfix_packet_drop(tfs, pkt, "negative2/error");
    return;
  }

deliver:
  if (tvhtrace_enabled()) {
    char _odts[22], _opts[22];
    pkt_trace(LS_TSFIX, pkt,
              "deliver odts %s opts %s ref %"PRId64" epoch %"PRId64,
              pts_to_string(odts, _odts), pts_to_string(opts, _opts),
              ref, tfs->tfs_dts_epoch);
  }

  streaming_message_t *sm = streaming_msg_create_pkt(pkt);
  streaming_target_deliver2(tf->tf_output, sm);
  pkt_ref_dec(pkt);
}

/**
 *
 */
static inline int
txfix_need_to_update_ref(tsfix_t *tf, tfstream_t *tfs, th_pkt_t *pkt)
{
  return tfs->tfs_local_ref == PTS_UNSET &&
         tf->tf_tsref != PTS_UNSET &&
         pkt->pkt_dts != PTS_UNSET;
}

/**
 *
 */
static int
tsfix_update_ref(tsfix_t *tf, tfstream_t *tfs, th_pkt_t *pkt)
{
  tfstream_t *tfs2;
  int64_t diff;

  if (pkt->pkt_err)
    return REF_ERROR;

  if (tfs->tfs_audio) {
    diff = tsfix_ts_diff(tf->tf_tsref, pkt->pkt_dts);
    if (diff > 3 * 90000) {
      tvhwarn(LS_TSFIX, "The timediff for %s is big (%"PRId64"), using current dts",
              streaming_component_type2txt(tfs->tfs_type), diff);
      tfs->tfs_local_ref = pkt->pkt_dts;
    } else {
      tfs->tfs_local_ref = tf->tf_tsref;
    }
  } else if (tfs->tfs_type == SCT_DVBSUB || tfs->tfs_type == SCT_TEXTSUB) {
    /* find first valid audio stream and check the dts timediffs */
    LIST_FOREACH(tfs2, &tf->tf_streams, tfs_link)
      if (tfs2->tfs_audio && tfs2->tfs_last_dts_in != PTS_UNSET) {
        diff = tsfix_ts_diff(tfs2->tfs_last_dts_in, pkt->pkt_dts);
        if (diff > 6 * 90000) {
          tvhwarn(LS_TSFIX, "The timediff for %s is big (%"PRId64"), using audio dts",
                  streaming_component_type2txt(tfs->tfs_type), diff);
          tfs->tfs_parent = tfs2;
          tfs->tfs_local_ref = tfs2->tfs_local_ref;
        } else {
          tfs->tfs_local_ref = tf->tf_tsref;
        }
        break;
      }
    if (tfs2 == NULL)
      return REF_DROP;
  } else if (tfs->tfs_type == SCT_TELETEXT) {
    diff = tsfix_ts_diff(tf->tf_tsref, pkt->pkt_dts);
    if (diff > 2 * 90000) {
      tvhwarn(LS_TSFIX, "The timediff for TELETEXT is big (%"PRId64"), using current dts", diff);
      tfs->tfs_local_ref = pkt->pkt_dts;
    } else {
      tfs->tfs_local_ref = tf->tf_tsref;
    }
  }
  return REF_OK;
}

/**
 *
 */
static void
tsfix_backlog(tsfix_t *tf)
{
  th_pkt_t *pkt;
  tfstream_t *tfs;
  int r;

  while((pkt = pktref_get_first(&tf->tf_backlog)) != NULL) {
    tfs = tfs_find(tf, pkt);
    if (txfix_need_to_update_ref(tf, tfs, pkt)) {
      r = tsfix_update_ref(tf, tfs, pkt);
      if (r != REF_OK) {
        tsfix_packet_drop(tfs, pkt, r == REF_ERROR ? "bckle" : "bckld");
        continue;
      }
    }
    normalize_ts(tf, tfs, pkt, 0);
  }
}


/**
 *
 */
static int64_t
tsfix_backlog_diff(tsfix_t *tf)
{
  th_pkt_t *pkt;
  th_pktref_t *pr;
  tfstream_t *tfs;
  int64_t res = 0;

  PKTREF_FOREACH(pr, &tf->tf_backlog) {
    pkt = pr->pr_pkt;
    if (pkt->pkt_dts == PTS_UNSET) continue;
    if (pkt->pkt_dts >= tf->tf_tsref) continue;
    if (tf->tf_tsref > (PTS_MASK * 3) / 4 &&
        pkt->pkt_dts < PTS_MASK / 4) continue;
    tfs = tfs_find(tf, pkt);
    if (!tfs->tfs_audio && !tfs->tfs_video) continue;
    res = MAX(tsfix_ts_diff(pkt->pkt_dts, tf->tf_tsref), res);
  }
  return res;
}


/**
 *
 * Recover unset PTS.
 *
 * MPEG2 DTS/PTS example (rpts = result pts):
 * 01: I dts 4922701936 pts 4922712736 rpts 4922712736
 * 02: B dts 4922705536 pts <unset>    rpts 4922705536
 * 03: B dts 4922709136 pts <unset>    rpts 4922709136
 * 04: P dts 4922712736 pts <unset>    rpts 4922723536
 * 05: B dts 4922716336 pts <unset>    rpts 4922716336
 * 06: B dts 4922719936 pts <unset>    rpts 4922719936
 * 07: P dts 4922723536 pts <unset>    rpts 4922734336
 * 08: B dts 4922727136 pts <unset>    rpts 4922727136
 * 09: B dts 4922730736 pts <unset>    rpts 4922730736
 * 10: P dts 4922734336 pts <unset>    rpts 4922745136
 * 11: B dts 4922737936 pts <unset>    rpts 4922737936
 * 12: B dts 4922741536 pts <unset>    rpts 4922741536
 * 13: I dts 4922745136 pts 4922755936 rpts 4922755936
 */
static void
recover_pts(tsfix_t *tf, tfstream_t *tfs, th_pkt_t *pkt)
{
  th_pktref_t *srch;
  int total;

  pktref_enqueue(&tf->tf_ptsq, pkt);
  while((pkt = pktref_get_first(&tf->tf_ptsq)) != NULL) {
    tfs = tfs_find(tf, pkt);

    switch(tfs->tfs_type) {

    case SCT_MPEG2VIDEO:

      switch(pkt->v.pkt_frametype) {
      case PKT_B_FRAME:
        if (pkt->pkt_pts == PTS_UNSET) {
	  /* B-frames have same PTS as DTS, pass them on */
	  tvhtrace(LS_TSFIX, "%-12s PTS b-frame set to %"PRId64" (old %"PRId64")",
		   streaming_component_type2txt(tfs->tfs_type),
		   pkt->pkt_dts, pkt->pkt_pts);
	  pkt->pkt_pts = pkt->pkt_dts;
        }
	break;
      
      case PKT_I_FRAME:
      case PKT_P_FRAME:
        if (pkt->pkt_pts == PTS_UNSET) {
	  /* Presentation occurs at DTS of next I or P frame,
	     try to find it */
          total = 0;
          PKTREF_FOREACH(srch, &tf->tf_ptsq) {
            if (pkt->pkt_componentindex != tfs->tfs_index)
              continue;
            total++;
            if (srch->pr_pkt->v.pkt_frametype <= PKT_P_FRAME &&
                pts_is_greater_or_equal(pkt->pkt_dts, srch->pr_pkt->pkt_dts) > 0 &&
                pts_diff(pkt->pkt_dts, srch->pr_pkt->pkt_dts) < 10 * 90000) {
              tvhtrace(LS_TSFIX, "%-12s PTS *-frame set to %"PRId64" (old %"PRId64"), DTS %"PRId64,
                          streaming_component_type2txt(tfs->tfs_type),
                          srch->pr_pkt->pkt_dts, pkt->pkt_pts, pkt->pkt_dts);
              pkt->pkt_pts = srch->pr_pkt->pkt_dts;
              break;
            }	  
          }
          if (srch == NULL) {
            if (total < 50) {
              /* return packet back to tf_ptsq */
              pktref_insert_head(&tf->tf_ptsq, pkt);
            } else {
              tsfix_packet_drop(tfs, pkt, "mpeg2video overflow");
            }
            return; /* not arrived yet or invalid, wait */
          }
        }
      }
      break;

    default:
      break;
    }

    normalize_ts(tf, tfs, pkt, 1);
  }
}


/**
 * Compute PTS (if not known)
 */
static void
compute_pts(tsfix_t *tf, tfstream_t *tfs, th_pkt_t *pkt)
{
  // If PTS is missing, set it to DTS if not video
  if(pkt->pkt_pts == PTS_UNSET && !tfs->tfs_video) {
    pkt->pkt_pts = pkt->pkt_dts;
    tvhtrace(LS_TSFIX, "%-12s PTS set to %"PRId64,
		streaming_component_type2txt(tfs->tfs_type),
		pkt->pkt_pts);
  }

  /* PTS known and no other packets in queue, deliver at once */
  if(pkt->pkt_pts != PTS_UNSET && TAILQ_FIRST(&tf->tf_ptsq) == NULL)
    normalize_ts(tf, tfs, pkt, 1);
  else
    recover_pts(tf, tfs, pkt);
}


/**
 *
 */
static void
tsfix_input_packet(tsfix_t *tf, streaming_message_t *sm)
{
  th_pkt_t *pkt;
  tfstream_t *tfs, *tfs2;
  int64_t diff, diff2, threshold;
  int r;

  pkt = pkt_copy_shallow(sm->sm_data);
  tfs = tfs_find(tf, pkt);
  streaming_msg_free(sm);

  if (tfs == NULL || mclk() < tf->tf_start_time) {
    tsfix_packet_drop(tfs, pkt, "start time");
    return;
  }

  if (tf->tf_tsref == PTS_UNSET && pkt->pkt_dts != PTS_UNSET &&
      ((!tf->tf_hasvideo && tfs->tfs_audio) ||
       (tfs->tfs_video && pkt->v.pkt_frametype == PKT_I_FRAME))) {
    if (pkt->pkt_err) {
      tsfix_packet_drop(tfs, pkt, "ref1");
      return;
    }
    threshold = 22500;
    LIST_FOREACH(tfs2, &tf->tf_streams, tfs_link)
      if (tfs != tfs2 && (tfs2->tfs_audio || tfs2->tfs_video) && !tfs2->tfs_seen) {
        threshold = 90000;
        break;
      }
    tf->tf_tsref = pkt->pkt_dts & PTS_MASK;
    if (pts_is_greater_or_equal(pkt->pkt_pcr, pkt->pkt_dts) > 0)
      tf->tf_tsref = pkt->pkt_pcr & PTS_MASK;
    diff = diff2 = tsfix_backlog_diff(tf);
    if (diff > threshold) {
      if (diff > 160000)
        diff = 160000;
      tf->tf_tsref = (tf->tf_tsref - diff) % PTS_MASK;
      tvhtrace(LS_TSFIX, "reference clock set to %"PRId64" (dts %"PRId64" pcr %"PRId64" backlog %"PRId64")", tf->tf_tsref, pkt->pkt_dts, pkt->pkt_pcr, diff2);
      tsfix_backlog(tf);
    } else {
      tvhtrace(LS_TSFIX, "reference clock set to %"PRId64" (dts %"PRId64" pcr %"PRId64" backlog %"PRId64")", tf->tf_tsref, pkt->pkt_dts, pkt->pkt_pcr, diff2);
    }
  } else if (txfix_need_to_update_ref(tf, tfs, pkt)) {
    r = tsfix_update_ref(tf, tfs, pkt);
    if (r != REF_OK) {
      tsfix_packet_drop(tfs, pkt, r == REF_ERROR ? "refe" : "refd");
      return;
    }
  }

  int pdur = pkt->pkt_duration >> pkt->v.pkt_field;

  if (pkt->pkt_dts == PTS_UNSET) {
    if (tfs->tfs_last_dts_in == PTS_UNSET) {
      if (tfs->tfs_type == SCT_TELETEXT) {
        sm = streaming_msg_create_pkt(pkt);
        streaming_target_deliver2(tf->tf_output, sm);
      }
      pkt_ref_dec(pkt);
      return;
    }

    if (pkt->pkt_payload != NULL || pkt->pkt_pts != PTS_UNSET) {
      pkt->pkt_dts = (tfs->tfs_last_dts_in + pdur) & PTS_MASK;
      tvhtrace(LS_TSFIX, "%-12s DTS set to last %"PRId64" +%d == %"PRId64", PTS = %"PRId64,
		streaming_component_type2txt(tfs->tfs_type),
		tfs->tfs_last_dts_in, pdur, pkt->pkt_dts, pkt->pkt_pts);
    }
  }

  if (tfs->tfs_parent)
    pkt->pkt_dts = pkt->pkt_pts = tfs->tfs_parent->tfs_last_dts_in;

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
    if (tf->tf_wait_for_video) {
      streaming_msg_free(sm);
      return;
    }
    tsfix_input_packet(tf, sm);
    return;
  case SMT_START:
    tsfix_stop(tf);
    tsfix_start(tf, sm->sm_data);
    if (tf->tf_wait_for_video) {
      streaming_msg_free(sm);
      return;
    }
    break;
  case SMT_STOP:
    tsfix_stop(tf);
    break;
  case SMT_TIMESHIFT_STATUS:
    if(tf->dts_offset == PTS_UNSET) {
      timeshift_status_t *status;
      status = sm->sm_data;
      tf->dts_offset = status->shift;
    }
    streaming_msg_free(sm);
    return;
  case SMT_SKIP:
    if(tf->dts_offset != PTS_UNSET) {
      tf->dts_offset_apply = 1;
    }
    break;
  case SMT_GRACE:
  case SMT_EXIT:
  case SMT_SERVICE_STATUS:
  case SMT_SIGNAL_STATUS:
  case SMT_DESCRAMBLE_INFO:
  case SMT_NOSTART:
  case SMT_NOSTART_WARN:
  case SMT_MPEGTS:
  case SMT_SPEED:
    break;
  }

  streaming_target_deliver2(tf->tf_output, sm);
}

static htsmsg_t *
tsfix_input_info(void *opaque, htsmsg_t *list)
{
  tsfix_t *tf = opaque;
  streaming_target_t *st = tf->tf_output;
  htsmsg_add_str(list, NULL, "tsfix input");
  return st->st_ops.st_info(st->st_opaque, list);
}

static streaming_ops_t tsfix_input_ops = {
  .st_cb   = tsfix_input,
  .st_info = tsfix_input_info
};


/**
 *
 */
streaming_target_t *
tsfix_create(streaming_target_t *output)
{
  tsfix_t *tf = calloc(1, sizeof(tsfix_t));

  TAILQ_INIT(&tf->tf_ptsq);

  tf->tf_output = output;
  tf->tf_start_time = mclk();
  tf->dts_offset = PTS_UNSET;
  streaming_target_init(&tf->tf_input, &tsfix_input_ops, tf, 0);
  return &tf->tf_input;
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
