/**
 *  Global header modification
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

#include <assert.h>
#include "tvheadend.h"
#include "streaming.h"
#include "globalheaders.h"

typedef struct globalheaders {
  streaming_target_t gh_input;

  streaming_target_t *gh_output;

  struct th_pktref_queue gh_holdq;

  streaming_start_t *gh_ss;

  int gh_passthru;

} globalheaders_t;

/* note: there up to 2.5 sec diffs in some sources! */
#define MAX_SCAN_TIME   5000  // in ms
#define MAX_NOPKT_TIME  2500  // in ms

/**
 *
 */
static inline int
gh_require_meta(int type)
{
  return type == SCT_HEVC ||
         type == SCT_H264 ||
         type == SCT_MPEG2VIDEO ||
         type == SCT_MP4A ||
         type == SCT_AAC ||
         type == SCT_VORBIS ||
         type == SCT_THEORA;
}

/**
 *
 */
static inline int
gh_is_audiovideo(int type)
{
  return SCT_ISVIDEO(type) || SCT_ISAUDIO(type);
}

/**
 *
 */
static void
gh_flush(globalheaders_t *gh)
{
  if(gh->gh_ss != NULL) {
    streaming_start_unref(gh->gh_ss);
    gh->gh_ss = NULL;
  }

  pktref_clear_queue(&gh->gh_holdq);
}


/**
 * @brief Initializes stream metadata and ensures a global header exists for the component.
 *
 * This function extracts essential stream parameters (frame duration, audio channels, 
 * sample rate, and video aspect ratio) from an incoming packet if they are not already 
 * defined in the streaming start component (ssc).
 * * It is primarily responsible for ensuring the 'ssc_gh' (Global Header/Extradata) is 
 * populated. If the packet contains metadata (pkt_meta), it increments the reference 
 * count and assigns it. 
 * * Special Case: AAC/MP4A
 * If an AAC stream lacks a global header, this function manually constructs a 2-byte 
 * (or 5-byte for SBR) AudioSpecificConfig (AAC-LC) using the packet's sample rate 
 * index and channel configuration. This is required for many decoders to initialize 
 * correctly.
 *
 * @param ssc  The streaming start component context to be updated.
 * @param pkt  The input packet containing raw media data and technical attributes.
 *
 * @note This function returns immediately if ssc->ssc_gh is already set.
 */
static void
apply_header(streaming_start_component_t *ssc, th_pkt_t *pkt)
{
  /* 1. Set the elementary stream (ES) frame duration if it hasn't been initialized 
        yet (is 0) and the incoming packet has a valid duration. */
  if(ssc->es_frame_duration == 0 && pkt->pkt_duration != 0)
    ssc->es_frame_duration = pkt->pkt_duration;
  /* 2. Audio-specific initialization: 
        If the stream is an audio type and the channel count hasn't been set yet, 
        copy the channel count and Sample Rate Indices (SRI) from the packet. */
  if(SCT_ISAUDIO(ssc->es_type) && !ssc->es_channels) {
    ssc->es_channels = pkt->a.pkt_channels;
    ssc->es_sri      = pkt->a.pkt_sri;
    ssc->es_ext_sri  = pkt->a.pkt_ext_sri;
  }
  /* 3. Video-specific initialization:
        If the stream is a video type and the packet contains aspect ratio information,
        copy the Sample Aspect Ratio (SAR) and Elementary Stream Aspect Ratio to the component. */
  if(SCT_ISVIDEO(ssc->es_type)) {
    if(pkt->v.pkt_aspect_num && pkt->v.pkt_aspect_den) {
#if ENABLE_LIBAV
      ssc->es_sample_aspect_ratio.num = pkt->v.pkt_sample_aspect_ratio.num;
      ssc->es_sample_aspect_ratio.den = pkt->v.pkt_sample_aspect_ratio.den;
#endif
      ssc->es_aspect_num = pkt->v.pkt_aspect_num;
      ssc->es_aspect_den = pkt->v.pkt_aspect_den;
    }
  }
  /* 4. Global Header (Extradata) Check:
        If the streaming component already has a global header initialized, 
        there is no further work to do. */
  if(ssc->ssc_gh != NULL)
    return;
  /* 5. Apply existing packet metadata as the global header:
        If the packet contains metadata, assign it to the component's global header 
        and increment the reference counter for the packet buffer to prevent it from 
        being freed prematurely. */
  if(pkt->pkt_meta != NULL) {
    ssc->ssc_gh = pkt->pkt_meta;
    pktbuf_ref_inc(ssc->ssc_gh);
    return;
  }
  /* 6. Synthesize AAC AudioSpecificConfig:
        If there was no packet metadata, but the stream is AAC/MP4A, we must manually 
        generate the MPEG-4 AudioSpecificConfig header required by decoders. */
  if (ssc->es_type == SCT_MP4A || ssc->es_type == SCT_AAC) {
    /* Allocate 5 bytes if an extended Sample Rate Index (SBR/HE-AAC) exists, otherwise 2 bytes. */
    ssc->ssc_gh = pktbuf_alloc(NULL, pkt->a.pkt_ext_sri ? 5 : 2);
    uint8_t *d = pktbuf_ptr(ssc->ssc_gh);
    /* Hardcode the Audio Object Type to 2 (AAC-LC - Low Complexity) */
    const int profile = 2; /* AAC LC */
    /* Byte 0: 
       - Top 5 bits: Audio Object Type (profile << 3)
       - Bottom 3 bits: Top 3 bits of the 4-bit Sample Rate Index (SRI) */
    d[0] = (profile << 3) | ((pkt->a.pkt_sri & 0xe) >> 1);
    /* Byte 1:
       - Top 1 bit: Bottom 1 bit of the 4-bit SRI
       - Next 4 bits: Channel Configuration (channels << 3)
       - Bottom 3 bits: 0 (Frame length, DependsOnCoreCoder, ExtensionFlag) */
    d[1] = ((pkt->a.pkt_sri & 0x1) << 7) | (pkt->a.pkt_channels << 3);
    /* Bytes 2-4: SBR (Spectral Band Replication) Extension signaling for HE-AAC */
    if (pkt->a.pkt_ext_sri) { /* SBR extension */
      /* Byte 4:
         - Top 1 bit: 1 (signals SBR is present)
         - Next 4 bits: The extended Sample Rate Index
         - Bottom 3 bits: 0 */
      d[2] = 0x56;
      d[3] = 0xe5;
      d[4] = 0x80 | ((pkt->a.pkt_ext_sri - 1) << 3);
    }
  }
}


/**
 * @brief Validates if sufficient metadata has been collected to initialize the stream.
 *
 * This function checks the streaming start component (ssc) to ensure that all 
 * critical parameters required by decoders or muxers have been populated. 
 * It prevents the stream from starting prematurely with incomplete headers.
 *
 * @param ssc           The streaming start component context to validate.
 * @param not_so_picky  Flag to relax validation. If non-zero, the function will 
 * ignore missing video dimensions (width/height) and aspect 
 * ratios, which is useful for certain "raw" or passthrough 
 * scenarios.
 *
 * @return int          Returns 1 if the metadata is complete and the stream can 
 * proceed; returns 0 if more information is required.
 *
 * @note Validation criteria include:
 * - Existence of frame duration for both audio and video.
 * - Presence of audio channel configuration for audio streams.
 * - (If picky) Presence of width, height, and aspect ratio for video streams.
 * - Presence of a Global Header (extradata) if the codec type requires it 
 * (checked via gh_require_meta).
 */
static int
header_complete(streaming_start_component_t *ssc, int not_so_picky)
{
  int is_video = SCT_ISVIDEO(ssc->es_type);
  int is_audio = !is_video && SCT_ISAUDIO(ssc->es_type);

  if((is_audio || is_video) && ssc->es_frame_duration == 0)
    return 0;

  if(is_video) {
    if(!not_so_picky && (ssc->es_aspect_num == 0 || ssc->es_aspect_den == 0 ||
                         ssc->es_width == 0 || ssc->es_height == 0))
      return 0;
  }

  if(is_audio && !ssc->es_channels)
    return 0;

  if(ssc->ssc_gh == NULL && gh_require_meta(ssc->es_type))
    return 0;

  return 1;
}


/**
 * @brief Calculates the time duration (delay) represented by packets in the hold queue.
 * * This function determines the time spread between the earliest and latest queued 
 * packets for a specific stream component (identified by `index`). It does this by 
 * scanning the queue from both ends to find the first and last packets belonging to 
 * the target stream that have valid Decoding Time Stamps (DTS), and computing the 
 * difference while accounting for timestamp wraparound.
 *
 * @param gh Pointer to the global headers context containing the packet hold queue.
 * @param index The elementary stream (ES) index to calculate the delay for.
 * @return int64_t The calculated time difference (delay) in DTS units. Returns 0 if 
 * valid timestamps aren't found, or 1 for special payloadless transcode packets.
 */
static int64_t
gh_queue_delay(globalheaders_t *gh, int index)
{
  /* Initialize pointers to the first (head) and last (tail) elements of the hold queue */
  th_pktref_t *f = TAILQ_FIRST(&gh->gh_holdq);
  th_pktref_t *l = TAILQ_LAST(&gh->gh_holdq, th_pktref_queue);
  streaming_start_component_t *ssc;
  int64_t diff;
  int8_t found_f = 0, found_l = 0;

  /*
   * Find only packets which require the meta data. Ignore others.
   */
  /*
   * 1. Forward Search: Find the EARLIEST packet in the queue for this stream.
   * Iterate from the front of the queue towards the back. We are looking for the 
   * first packet that has a valid DTS and belongs to the requested stream index.
   */
  while (f != l) {
    if (f->pr_pkt->pkt_dts != PTS_UNSET) {
      /* Retrieve the stream component context using the packet's internal component index */
      ssc = streaming_start_component_find_by_index
              (gh->gh_ss, f->pr_pkt->pkt_componentindex);
      /* If the packet belongs to the requested elementary stream index, stop searching */
      if (ssc && ssc->es_index == index){
        found_f = 1;
        break;
      }
    }
    /* Move to the next packet in the queue */
    f = TAILQ_NEXT(f, pr_link);
  }
  /*
   * 2. Backward Search: Find the LATEST packet in the queue for this stream.
   * Iterate from the back of the queue towards the front. Stop when we find the 
   * last packet matching our stream index with a valid DTS.
   */
  while (l != f) {
    if (l->pr_pkt->pkt_dts != PTS_UNSET) {
      ssc = streaming_start_component_find_by_index
              (gh->gh_ss, l->pr_pkt->pkt_componentindex);
      if (ssc && ssc->es_index == index){
        found_l = 1;
        break;
      }
    }
    /* Move to the previous packet in the queue */
    l = TAILQ_PREV(l, th_pktref_queue, pr_link);
  }
  /* * 3. Calculate Delay Difference:
   * If both a first and last packet with valid timestamps were found, compute the delay.
   */
  if (l->pr_pkt->pkt_dts != PTS_UNSET && f->pr_pkt->pkt_dts != PTS_UNSET) {
    /* Mask the DTS values (usually 33-bit for MPEG-TS) to drop overflow bits, 
       then subtract the early timestamp from the late timestamp. */
    if (found_f && found_l) {
      /* Calculate raw difference */
      diff = (l->pr_pkt->pkt_dts & PTS_MASK) - (f->pr_pkt->pkt_dts & PTS_MASK);
    } else {
        return 0;
    }
    /* Handle Timestamp Rollover/Wraparound: 
       If the difference is negative, the timestamp counter rolled over its maximum 
       value (PTS_MASK+1) between the first and last packet. Add the mask to correct it. */
    if (diff < 0)
      /* When a 33-bit counter overflows (wraps around), it goes from the maximum value back to 0. 
        The total number of unique values in that clock cycle is 233, which is PTS_MASK + 1.*/
      diff += (int64_t)PTS_MASK + 1;
    /*  The V4L2/Discontinuity Fix:
        If the diff is greater than half the mask (roughly 13 hours),
        it means f was actually significantly ahead of l. 
        This is usually a startup discontinuity. */
    if (diff > (PTS_MASK >> 1)) {
      diff = 0; 
    }
  } else {
    /* If no valid matching packets were found, there is no determinable delay. */
    diff = 0;
  }

  /* special noop packet from transcoder, increase decision limit */
  /* * 4. Edge Case - Empty Transcoder Packet:
   * special noop packet from transcoder, increase decision limit.
   * If the queue resolved to a single packet (first == last) and it has no actual 
   * media payload, artificially return a delay of 1. This prevents the system from 
   * treating it as a standard zero-delay scenario.
   */
  if (l == f && l->pr_pkt->pkt_payload == NULL)
    return 1;

  return diff;
}


/**
 *
 */
static int
headers_complete(globalheaders_t *gh)
{
  streaming_start_t *ss = gh->gh_ss;
  streaming_start_component_t *ssc;
  int64_t *qd = alloca(ss->ss_num_components * sizeof(int64_t));
  int64_t qd_max = 0;
  int i, threshold = 0;

  assert(ss != NULL);

  for(i = 0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];
    qd[i] = gh_is_audiovideo(ssc->es_type) ?
              gh_queue_delay(gh, ssc->es_index) : 0;
    if (qd[i] > qd_max)
      qd_max = qd[i];
  }

  if (qd_max <= 0)
    return 0;

  threshold = qd_max > MAX_SCAN_TIME * 90;

  for(i = 0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    if(!header_complete(ssc, threshold)) {
      /*
       * disable stream only when
       * - half timeout is reached without any packets seen
       * - maximal timeout is reached without metadata
       */
      if(threshold || (qd[i] <= 0 && qd_max > (MAX_NOPKT_TIME * 90))) {
	ssc->ssc_disabled = 1;
        tvhdebug(LS_GLOBALHEADERS, "gh disable stream %d %s%s%s (PID %i) threshold %d qd %"PRId64" qd_max %"PRId64,
             ssc->es_index, streaming_component_type2txt(ssc->es_type),
             ssc->es_lang[0] ? " " : "", ssc->es_lang, ssc->es_pid,
             threshold, qd[i], qd_max);
      } else {
	return 0;
      }
    } else {
      ssc->ssc_disabled = 0;
    }
  }

  if (tvhtrace_enabled()) {
    for(i = 0; i < ss->ss_num_components; i++) {
      ssc = &ss->ss_components[i];
      tvhtrace(LS_GLOBALHEADERS, "stream %d %s%s%s (PID %i) complete time %"PRId64"%s",
               ssc->es_index, streaming_component_type2txt(ssc->es_type),
               ssc->es_lang[0] ? " " : "", ssc->es_lang, ssc->es_pid,
               gh_queue_delay(gh, ssc->es_index),
               ssc->ssc_disabled ? " disabled" : "");
    }
  }

  return 1;
}


/**
 *
 */
static void
gh_start(globalheaders_t *gh, streaming_message_t *sm)
{
  gh->gh_ss = streaming_start_copy(sm->sm_data);
  streaming_msg_free(sm);
}


/**
 *
 */
static void
gh_hold(globalheaders_t *gh, streaming_message_t *sm)
{
  th_pkt_t *pkt;
  streaming_start_component_t *ssc;

  switch(sm->sm_type) {
  case SMT_PACKET:
    pkt = sm->sm_data;
    ssc = streaming_start_component_find_by_index(gh->gh_ss,
						  pkt->pkt_componentindex);
    if (ssc == NULL) {
      tvherror(LS_GLOBALHEADERS, "Unable to find component %d", pkt->pkt_componentindex);
      streaming_msg_free(sm);
      return;
    }

    pkt_trace(LS_GLOBALHEADERS, pkt, "hold receive");

    pkt_ref_inc(pkt);

    if (pkt->pkt_err == 0)
      apply_header(ssc, pkt);

    pktref_enqueue(&gh->gh_holdq, pkt);

    streaming_msg_free(sm);

    if(!gh_is_audiovideo(ssc->es_type))
      break;

    if(!headers_complete(gh))
      break;

    // Send our modified start
    sm = streaming_msg_create_data(SMT_START,
				   streaming_start_copy(gh->gh_ss));
    streaming_target_deliver2(gh->gh_output, sm);

    // Send all pending packets
    while((pkt = pktref_get_first(&gh->gh_holdq)) != NULL) {
      if (pkt->pkt_payload) {
        sm = streaming_msg_create_pkt(pkt);
        streaming_target_deliver2(gh->gh_output, sm);
      }
      pkt_ref_dec(pkt);
    }
    gh->gh_passthru = 1;
    break;

  case SMT_START:
    if (gh->gh_ss)
      gh_flush(gh);
    gh_start(gh, sm);
    break;

  case SMT_STOP:
    gh_flush(gh);
    streaming_msg_free(sm);
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
  case SMT_SKIP:
  case SMT_TIMESHIFT_STATUS:
    streaming_target_deliver2(gh->gh_output, sm);
    break;
  }
}


/**
 *
 */
static void
gh_pass(globalheaders_t *gh, streaming_message_t *sm)
{
  th_pkt_t *pkt;
  switch(sm->sm_type) {
  case SMT_START:
    /* stop */
    gh->gh_passthru = 0;
    gh_flush(gh);
    /* restart */
    gh_start(gh, sm);
    break;

  case SMT_STOP:
    gh->gh_passthru = 0;
    gh_flush(gh);
    // FALLTHRU
  case SMT_GRACE:
  case SMT_EXIT:
  case SMT_SERVICE_STATUS:
  case SMT_SIGNAL_STATUS:
  case SMT_DESCRAMBLE_INFO:
  case SMT_NOSTART:
  case SMT_NOSTART_WARN:
  case SMT_MPEGTS:
  case SMT_SKIP:
  case SMT_SPEED:
  case SMT_TIMESHIFT_STATUS:
    streaming_target_deliver2(gh->gh_output, sm);
    break;
  case SMT_PACKET:
    pkt = sm->sm_data;
    if (pkt->pkt_payload || pkt->pkt_err)
      streaming_target_deliver2(gh->gh_output, sm);
    else
      streaming_msg_free(sm);
    break;
  }
}


/**
 *
 */
static void
globalheaders_input(void *opaque, streaming_message_t *sm)
{
  globalheaders_t *gh = opaque;

  if(gh->gh_passthru)
    gh_pass(gh, sm);
  else
    gh_hold(gh, sm);
}

static htsmsg_t *
globalheaders_input_info(void *opaque, htsmsg_t *list)
{
  globalheaders_t *gh = opaque;
  streaming_target_t *st = gh->gh_output;
  htsmsg_add_str(list, NULL, "globalheaders input");
  return st->st_ops.st_info(st->st_opaque, list);
}

static streaming_ops_t globalheaders_input_ops = {
  .st_cb   = globalheaders_input,
  .st_info = globalheaders_input_info
};


/**
 *
 */
streaming_target_t *
globalheaders_create(streaming_target_t *output)
{
  globalheaders_t *gh = calloc(1, sizeof(globalheaders_t));

  TAILQ_INIT(&gh->gh_holdq);

  gh->gh_output = output;
  streaming_target_init(&gh->gh_input, &globalheaders_input_ops, gh, 0);
  return &gh->gh_input;
}


/**
 *
 */
void
globalheaders_destroy(streaming_target_t *pad)
{
  globalheaders_t *gh = (globalheaders_t *)pad;
  gh_flush(gh);
  free(gh);
}
