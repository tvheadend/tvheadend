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
 * @brief Represents a single Elementary Stream (Video, Audio, or Subtitle).
 * 
 * Tracks the timestamp state for a specific component within the multiplex.
 * Because different streams (e.g., audio vs video) can have slight time drifts,
 * this structure maintains the stream's local history to ensure its timestamps
 * remain monotonic.
 */
typedef struct tfstream {

  LIST_ENTRY(tfstream) tfs_link;

  // Unique index for this component in the mux
  int tfs_index;

  // Is this MPEG2, H264, AC3, AAC, etc.?
  streaming_component_type_t tfs_type;
  // Boolean flag: 1 if video
  uint8_t tfs_video;
  // Boolean flag: 1 if audio
  uint8_t tfs_audio;
  // Boolean flag: 1 if subtitle
  uint8_t tfs_subtitle;

  // Counter for consecutive DTS jump errors
  int tfs_bad_dts;
  // Stream-specific reference timestamp (used if drifting)
  int64_t tfs_local_ref;
  // The last output (normalized) DTS
  int64_t tfs_last_dts_norm;
  // Tracks 33-bit wraparounds (multiples of PTS_MASK + 1)
  int64_t tfs_dts_epoch;

  // The raw DTS as it arrived from the source
  int64_t tfs_last_dts_in;

  // Flag indicating if we've processed at least one valid packet
  int tfs_seen;

  // If this stream relies on another's timing (e.g., subs tied to audio)
  struct tfstream *tfs_parent;

} tfstream_t;


/**
 * @brief Main context for the Timestamp Fixer (tsfix).
 * 
 * Handles the normalization of the entire transport stream. It establishes a 
 * "master clock reference" (tf_tsref) when the stream starts so that all output
 * timestamps can be neatly offset to start near zero, making life easier for
 * downstream decoders and muxers.
 */
typedef struct tsfix {
  // Input pad for receiving stream messages
  streaming_target_t tf_input;

  // Output pad to send fixed packets downstream
  streaming_target_t *tf_output;

  // Linked list of all tfstream_t components
  struct tfstream_list tf_streams;
  // Flag: Does this stream contain video?
  int tf_hasvideo;
  // Flag: Should we buffer until the first video frame?
  int tf_wait_for_video;
  // Master reference clock (starting timestamp of the stream)
  int64_t tf_tsref;
  // System clock time when processing started
  int64_t tf_start_time;
  // Offset used during timeshifting/seeking
  int64_t dts_offset;
  // Flag to apply the timeshift offset
  int dts_offset_apply;

  // Queue for packets waiting for PTS calculation (e.g. MPEG2 B-frames)
  struct th_pktref_queue tf_ptsq;
  // Queue for early packets that arrived before tf_tsref was established
  struct th_pktref_queue tf_backlog;

} tsfix_t;


/**
 * @brief Computes the absolute difference between two 33-bit timestamps.
 * 
 * Handles the edge case where the 33-bit clock rolls over from its maximum 
 * value (0x1FFFFFFFF) back to 0.
 * 
 * @param ts1 First timestamp
 * @param ts2 Second timestamp
 * @return int64_t The absolute difference in 90kHz ticks.
 */
static int64_t
tsfix_ts_diff(int64_t ts1, int64_t ts2)
{
  int64_t r;
  // Restrict to 33 bits
  ts1 &= PTS_MASK;
  ts2 &= PTS_MASK;

  r = llabs(ts1 - ts2);

  // If the difference is greater than half the max 33-bit value, 
  // it means a wraparound occurred between ts1 and ts2.
  if (r > (PTS_MASK / 2)) {
    // try to wrap the lowest value
    if (ts1 < ts2)
      ts1 += PTS_MASK + 1;
    else
      ts2 += PTS_MASK + 1;
    return llabs(ts1 - ts2);
  }
  return r;
}


/**
 * @brief Destroys all stream contexts and clears queues.
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


/**
 * @brief Looks up a stream context by its component index.
 */
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
 * @brief Allocates and initializes a new stream context (video, audio, etc.).
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
  // Initialize states to "unset"
  tfs->tfs_local_ref = PTS_UNSET;
  tfs->tfs_last_dts_norm = PTS_UNSET;
  tfs->tfs_last_dts_in = PTS_UNSET;
  tfs->tfs_dts_epoch = 0;
  tfs->tfs_seen = 0;

  LIST_INSERT_HEAD(&tf->tf_streams, tfs, tfs_link);
  return tfs;
}


/**
 * @brief Initializes the tsfix module when a new stream starts.
 * 
 * Sets up expected streams and determines if the pipeline should block
 * until the first valid video packet arrives.
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
      // If we don't know the resolution yet, wait for video headers
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
 * @brief Cleans up when the stream stops.
 */
static void
tsfix_stop(tsfix_t *tf)
{
  tsfix_destroy_streams(tf);
}


/**
 * @brief Safely drops a packet and logs the reason.
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
 * @brief The core timestamp normalization engine.
 * 
 * 1. Subtracts the initial starting offset (`tf_tsref`) so timestamps start near 0.
 * 2. Monitors for 33-bit rollovers and increments `tfs_dts_epoch` to maintain a 
 *    continuously growing 64-bit timestamp.
 * 3. Enforces monotonic output (drops timestamps that jump backward into the past).
 * 
 * @param tf Context
 * @param tfs Stream state
 * @param pkt Packet to normalize
 * @param backlog Flag indicating if we are processing the startup backlog
 */
static void
normalize_ts(tsfix_t *tf, tfstream_t *tfs, th_pkt_t *pkt, int backlog)
{
  int64_t ref, dts, odts, opts, d;

  // If we haven't found a valid starting clock yet, park the packet in backlog
  if(tf->tf_tsref == PTS_UNSET) {
    if (backlog) {
      if (pkt->pkt_dts != PTS_UNSET)
        tfs->tfs_seen = 1;
      pktref_enqueue(&tf->tf_backlog, pkt);
    } else
      pkt_ref_dec(pkt);
    return;
  }

  // Use the stream's local reference if it drifted, otherwise use master ref
  ref = tfs->tfs_local_ref != PTS_UNSET ? tfs->tfs_local_ref : tf->tf_tsref;
  odts = pkt->pkt_dts;
  opts = pkt->pkt_pts;

  // Fallback: Use PTS if DTS is missing
  if (pkt->pkt_dts == PTS_UNSET) {
    if (pkt->pkt_pts != PTS_UNSET)
      pkt->pkt_dts = pkt->pkt_pts;
    else
      goto deliver;
  }

  pkt->pkt_dts &= PTS_MASK;

  // Subtract the transport wide start offset (this is where timestamps are zeroed out)
  if (tf->dts_offset_apply)
    dts = pts_diff(ref, pkt->pkt_dts + tf->dts_offset);
  else
    dts = pts_diff(ref, pkt->pkt_dts);

  // Validate monotonic behavior
  if (tfs->tfs_last_dts_norm == PTS_UNSET) {
    if (dts < 0 || pkt->pkt_err) {
      // Early packet with negative time stamp, drop those
      tsfix_packet_drop(tfs, pkt, "negative/error");
      return;
    }
  } else {
    const int64_t nlimit =      -1; // allow negative values - rounding errors?
    int64_t low          = 2*90000; // two second
    int64_t upper        = 3*90000; // three seconds

    // d = distance between current calculated DTS and the last output DTS
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
      // If the jump is massive, check if it's a 33-bit wraparound
      if (d < -PTS_MASK || d > -PTS_MASK + upper) {
        if (pkt->pkt_err) {
          tsfix_packet_drop(tfs, pkt, "possible wrong discontinuity");
          return;
        }
	      tfs->tfs_bad_dts++;
        if (tfs->tfs_bad_dts < 5) {
          char _dts[22], _dts_old[22];
          tvhwarn(LS_TSFIX,
            "transport stream %s, DTS discontinuity. DTS = %s, last = %s",
            streaming_component_type2txt(tfs->tfs_type),
            pts_to_string(dts, _dts),
            pts_to_string(tfs->tfs_last_dts_norm, _dts_old));
        }
      } else {
        // DTS wrapped (passed 26.5 hours). Increase the epoch so the 64-bit int keeps growing
        tfs->tfs_dts_epoch += PTS_MASK + 1;
        tfs->tfs_bad_dts = 0;
      }
    } else {
      tfs->tfs_bad_dts = 0;
    }
  }

  dts += tfs->tfs_dts_epoch;
  tfs->tfs_last_dts_norm = dts;

  // Fix up PTS relative to our newly calculated 64-bit DTS
  if(pkt->pkt_pts != PTS_UNSET) {
    // Compute delta between PTS and DTS (and watch out for 33 bit wrap)
    d = ((pkt->pkt_pts & PTS_MASK) - pkt->pkt_dts) & PTS_MASK;
    // Add that delta back to the clean DTS
    pkt->pkt_pts = dts + d;
  }

  // Fix up PCR (Program Clock Reference) relative to DTS
  if(pkt->pkt_pcr != PTS_UNSET) {
    // Compute delta between PCR and DTS (and watch out for 33 bit wrap)
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
    char _odts[22], _opts[22], _ref[22];
    pkt_trace(LS_TSFIX, pkt,
              "deliver odts %s opts %s ref %s epoch %"PRId64,
              pts_to_string(odts, _odts),
              pts_to_string(opts, _opts),
              pts_to_string(ref, _ref),
              tfs->tfs_dts_epoch);
  }

  // Send corrected packet out of the module
  streaming_message_t *sm = streaming_msg_create_pkt(pkt);
  streaming_target_deliver2(tf->tf_output, sm);
  pkt_ref_dec(pkt);
}


/**
 * @brief Checks if a specific stream has drifted far enough to need a new local reference.
 */
static inline int
txfix_need_to_update_ref(tsfix_t *tf, tfstream_t *tfs, th_pkt_t *pkt)
{
  return tfs->tfs_local_ref == PTS_UNSET &&
         tf->tf_tsref != PTS_UNSET &&
         pkt->pkt_dts != PTS_UNSET;
}


/**
 * @brief Calculates a local reference for streams that get wildly out of sync 
 * (like audio drifting from video or subs syncing to audio).
 */
static int
tsfix_update_ref(tsfix_t *tf, tfstream_t *tfs, th_pkt_t *pkt)
{
  tfstream_t *tfs2;
  int64_t diff;

  if (pkt->pkt_err)
    return REF_ERROR;

  if (tfs->tfs_audio) {
    // If audio is more than 3 seconds off the master clock, reset its local clock
    diff = tsfix_ts_diff(tf->tf_tsref, pkt->pkt_dts);
    if (diff > 3 * 90000) {
      char _dts[22];
      tvhwarn(LS_TSFIX, "The timediff for %s is big (%"PRId64"), using current dts: %s",
              streaming_component_type2txt(tfs->tfs_type), diff, pts_to_string(pkt->pkt_dts, _dts));
      tfs->tfs_local_ref = pkt->pkt_dts;
    } else {
      tfs->tfs_local_ref = tf->tf_tsref;
    }
  } else if (tfs->tfs_type == SCT_DVBSUB || tfs->tfs_type == SCT_TEXTSUB) {
    // Subs are often timed to audio. Find the first valid audio stream and compare.
    LIST_FOREACH(tfs2, &tf->tf_streams, tfs_link)
      if (tfs2->tfs_audio && tfs2->tfs_last_dts_in != PTS_UNSET) {
        diff = tsfix_ts_diff(tfs2->tfs_last_dts_in, pkt->pkt_dts);
        if (diff > 6 * 90000) {
          char _dts[22];
          tvhwarn(LS_TSFIX, "The timediff for %s is big (%"PRId64"), using audio dts: %s",
                  streaming_component_type2txt(tfs->tfs_type), diff, pts_to_string(pkt->pkt_dts, _dts));
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
      char _dts[22];
      tvhwarn(LS_TSFIX, "The timediff for TELETEXT is big (%"PRId64"), using current dts: %s", 
              diff, pts_to_string(pkt->pkt_dts, _dts));
      tfs->tfs_local_ref = pkt->pkt_dts;
    } else {
      tfs->tfs_local_ref = tf->tf_tsref;
    }
  }
  return REF_OK;
}


/**
 * @brief Flushes the backlog of early packets once a master reference clock is found.
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
 * @brief Determines the maximum timespan covered by the packets currently in the backlog.
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
/**
 * @brief Recovers missing Presentation Time Stamps (PTS) based on MPEG2 frame ordering.
 *
 * In MPEG2, decoding order (DTS) and presentation order (PTS) differ because of B-frames.
 * I and P frames decode early so subsequent B-frames can use them for reference.
 * If the source stream omits PTS, this infers it by looking ahead at the next I/P frame.
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
        char _pts[22], _pts_old[22], _dts[22];
      case PKT_B_FRAME:
        if (pkt->pkt_pts == PTS_UNSET) {
        // B-frames are presented as soon as they are decoded
        tvhtrace(LS_TSFIX, "%-12s PTS %c-frame set to %s (old %s), DTS %s",
          streaming_component_type2txt(tfs->tfs_type),
          pkt_frametype_to_char(pkt->v.pkt_frametype),
          pts_to_string(pkt->pkt_dts, _pts),
          pts_to_string(pkt->pkt_pts, _pts_old),
          pts_to_string(pkt->pkt_dts, _dts));
        pkt->pkt_pts = pkt->pkt_dts;
        }
        break;
      
      case PKT_I_FRAME:
      case PKT_P_FRAME:
        if (pkt->pkt_pts == PTS_UNSET) {
          /* Presentation occurs at DTS of next I or P frame.
            Look ahead in the queue for the first subsequent I/P frame. */
          total = 0;
          PKTREF_FOREACH(srch, &tf->tf_ptsq) {
            // Does this queued packet we are currently looking at belong to the stream we are trying to fix?
            if (srch->pr_pkt->pkt_componentindex != tfs->tfs_index)
              continue;
            total++;
            // Skip the frame itself
            if (srch->pr_pkt == pkt)
              continue;

            // Match the NEXT I or P frame ahead in the stream (frametype <= PKT_P_FRAME)
            if (srch->pr_pkt->v.pkt_frametype <= PKT_P_FRAME &&
                pts_is_greater_or_equal(pkt->pkt_dts, srch->pr_pkt->pkt_dts) > 0 &&
                pts_diff(pkt->pkt_dts, srch->pr_pkt->pkt_dts) < 10 * 90000) {
              tvhtrace(LS_TSFIX, "%-12s PTS %c-frame set to %s (old %s), DTS %s",
                          streaming_component_type2txt(tfs->tfs_type),
                          pkt_frametype_to_char(pkt->v.pkt_frametype),
                          pts_to_string(srch->pr_pkt->pkt_dts, _pts),
                          pts_to_string(pkt->pkt_pts, _pts_old),
                          pts_to_string(pkt->pkt_dts, _dts));
              // We found the next I/P frame. Use its Decode time as our Presentation time.
              pkt->pkt_pts = srch->pr_pkt->pkt_dts;
              break;
            }	  
          }
          if (srch == NULL) {
            if (total < 50) {
              // return packet back to tf_ptsq
              // We haven't received the next I/P frame yet. Put this back in the queue and wait.
              pktref_insert_head(&tf->tf_ptsq, pkt);
            } else {
              // Safety valve to prevent unbounded memory growth if the stream is corrupted
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
 * @brief Evaluates if PTS needs complex recovery (MPEG2) or simple matching (Audio).
 */
static void
compute_pts(tsfix_t *tf, tfstream_t *tfs, th_pkt_t *pkt)
{
  // If PTS is missing, set it to DTS if not video
  // Audio frames decode and present immediately. PTS = DTS.
  if(pkt->pkt_pts == PTS_UNSET && !tfs->tfs_video) {
    pkt->pkt_pts = pkt->pkt_dts;
    char _pts[22];
    tvhtrace(LS_TSFIX, "%-12s PTS set to %s",
		streaming_component_type2txt(tfs->tfs_type),
		pts_to_string(pkt->pkt_pts, _pts));
  }

  // PTS known and no other packets in queue, deliver at once
  // If we have PTS and no other video packets waiting in line, deliver directly.
  if(pkt->pkt_pts != PTS_UNSET && TAILQ_FIRST(&tf->tf_ptsq) == NULL){
    // pring good frames
    char _pts[22], _pts_old[22], _dts[22];
    tvhtrace(LS_TSFIX, "%-12s PTS %c-frame set to %s (old %s), DTS %s",
      streaming_component_type2txt(tfs->tfs_type),
      pkt_frametype_to_char(pkt->v.pkt_frametype),
      pts_to_string(pkt->pkt_pts, _pts),
      pts_to_string(pkt->pkt_pts, _pts_old),
      pts_to_string(pkt->pkt_dts, _dts));
    normalize_ts(tf, tfs, pkt, 1);
  }
  else
    recover_pts(tf, tfs, pkt);
}


/**
 * @brief Processes an incoming streaming packet for timestamp correction.
 *
 * This function ingests a raw media packet into the timestamp fixer (tsfix) pipeline.
 * It is responsible for establishing the initial reference clock for the stream by
 * waiting for a valid video keyframe (I-Frame) or audio frame. Once the reference
 * is set, subsequent packets are evaluated against it to maintain A/V sync and 
 * filter out packets with corrupted or severely misaligned timestamps.
 *
 * @param tf Pointer to the timestamp fixer context/state.
 * @param sm Pointer to the incoming streaming message containing the packet payload.
 *           The streaming message wrapper is freed by this function, but the 
 *           underlying packet is shallow-copied and processed.
 *
 * @note The function operates on a 90kHz clock scale (standard for MPEG-TS). 
 *       Thresholds like 90000 (1 sec) and 22500 (250 ms) are used to cap 
 *       backlog processing and prevent excessive clock skewing on startup.
 */
static void
tsfix_input_packet(tsfix_t *tf, streaming_message_t *sm)
{
  th_pkt_t *pkt;
  tfstream_t *tfs, *tfs2;
  int64_t diff, diff2, threshold;
  int r;

  // Shallow copy so we can manipulate fields without affecting upstream caches
  pkt = pkt_copy_shallow(sm->sm_data);
  tfs = tfs_find(tf, pkt);
  streaming_msg_free(sm);

  // Drop if it arrived before the module fully started
  if (tfs == NULL || mclk() < tf->tf_start_time) {
    tsfix_packet_drop(tfs, pkt, "start time");
    return;
  }

  // --- REFERENCE CLOCK ACQUISITION ---
  // If we don't have a master clock yet, try to use this packet to set it.
  if (tf->tf_tsref == PTS_UNSET && pkt->pkt_dts != PTS_UNSET &&
      ((!tf->tf_hasvideo && tfs->tfs_audio) ||
       (tfs->tfs_video && pkt->v.pkt_frametype == PKT_I_FRAME))) {
    if (pkt->pkt_err) {
      tsfix_packet_drop(tfs, pkt, "ref1");
      return;
    }

    // Set backlog threshold (250ms default, 1s if we are waiting on other streams)
    threshold = 22500;
    LIST_FOREACH(tfs2, &tf->tf_streams, tfs_link)
      if (tfs != tfs2 && (tfs2->tfs_audio || tfs2->tfs_video) && !tfs2->tfs_seen) {
        threshold = 90000;
        break;
      }
    
    // Set master reference to this packet's DTS (or PCR if available)
    tf->tf_tsref = pkt->pkt_dts & PTS_MASK;
    if (pts_is_greater_or_equal(pkt->pkt_pcr, pkt->pkt_dts) > 0)
      tf->tf_tsref = pkt->pkt_pcr & PTS_MASK;
    
    // Rewind the clock slightly based on what accumulated in the backlog
    diff = diff2 = tsfix_backlog_diff(tf);
    if (diff > threshold) {
      if (diff > 160000)
        diff = 160000;
      tf->tf_tsref = (tf->tf_tsref - diff) % PTS_MASK;
      char _tsref[22], _dts[22], _pcr[22];
      tvhtrace(LS_TSFIX, "reference clock set to %s (dts %s pcr %s backlog %"PRId64")",
              pts_to_string(tf->tf_tsref, _tsref),
              pts_to_string(pkt->pkt_dts, _dts),
              pts_to_string(pkt->pkt_pcr, _pcr),
              diff2);
      // Flush backlog now that reference is set
      tsfix_backlog(tf);
    } else {
      char _tsref[22], _dts[22], _pcr[22];
      tvhtrace(LS_TSFIX, "reference clock set to %s (dts %s pcr %s backlog %"PRId64")",
              pts_to_string(tf->tf_tsref, _tsref),
              pts_to_string(pkt->pkt_dts, _dts),
              pts_to_string(pkt->pkt_pcr, _pcr),
              diff2);
    }
  } else if (txfix_need_to_update_ref(tf, tfs, pkt)) {
    // Handling for streams that drift out of sync mid-stream
    r = tsfix_update_ref(tf, tfs, pkt);
    if (r != REF_OK) {
      tsfix_packet_drop(tfs, pkt, r == REF_ERROR ? "refe" : "refd");
      return;
    }
  }

  int64_t pdur = pkt->pkt_duration >> pkt->v.pkt_field;

  // --- MISSING DTS FALLBACK ---
  if (pkt->pkt_dts == PTS_UNSET) {
    if (tfs->tfs_last_dts_in == PTS_UNSET) {
      // If we don't have a previous DTS to extrapolate from, we can't save it
      if (tfs->tfs_type == SCT_TELETEXT) {
        sm = streaming_msg_create_pkt(pkt);
        streaming_target_deliver2(tf->tf_output, sm);
      }
      pkt_ref_dec(pkt);
      return;
    }

    // Extrapolate DTS by adding frame duration to the last known DTS
    if (pkt->pkt_payload != NULL || pkt->pkt_pts != PTS_UNSET) {
      pkt->pkt_dts = (tfs->tfs_last_dts_in + pdur) & PTS_MASK;
      char _dts_old[22], _dts[22], _pts[22];
      tvhtrace(LS_TSFIX, "%-12s DTS set from last %s + %"PRId64" = %s, PTS = %s",
        streaming_component_type2txt(tfs->tfs_type),
        pts_to_string(tfs->tfs_last_dts_in, _dts_old),
        pdur,
        pts_to_string(pkt->pkt_dts, _dts),
        pts_to_string(pkt->pkt_pts, _pts));
    }
  }

  // Force subtitle timing to match its parent audio track if synced
  if (tfs->tfs_parent)
    pkt->pkt_dts = pkt->pkt_pts = tfs->tfs_parent->tfs_last_dts_in;

  tfs->tfs_last_dts_in = pkt->pkt_dts;

  compute_pts(tf, tfs, pkt);
}


/**
 * @brief Input event loop. Handles packets, start/stop events, and timeshifts.
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
    // User scrubbed the DVR. Pick up the jump offset so we can apply it.
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
  // Ignore status updates
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
 * @brief Constructor. Allocates and hooks the timestamp fixer into the chain.
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
 * @brief Destructor. Frees all memory related to this tsfix instance.
 */
void
tsfix_destroy(streaming_target_t *pad)
{
  tsfix_t *tf = (tsfix_t *)pad;

  tsfix_destroy_streams(tf);
  free(tf);
}
