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
#include "avc.h"

typedef struct globalheaders {
  streaming_target_t gh_input;

  streaming_target_t *gh_output;

  struct th_pktref_queue gh_holdq;

  streaming_start_t *gh_ss;

  int gh_passthru;

} globalheaders_t;

#define MAX_SCAN_TIME 5000  // in ms


/**
 *
 */
static void
gh_flush(globalheaders_t *gh)
{
  if(gh->gh_ss != NULL)
    streaming_start_unref(gh->gh_ss);
  gh->gh_ss = NULL;

  pktref_clear_queue(&gh->gh_holdq);
}



/**
 *
 */
static void
apply_header(streaming_start_component_t *ssc, th_pkt_t *pkt)
{
  uint8_t *d;
  if(ssc->ssc_frameduration == 0 && pkt->pkt_duration != 0)
    ssc->ssc_frameduration = pkt->pkt_duration;

  if(SCT_ISAUDIO(ssc->ssc_type) && !ssc->ssc_channels && !ssc->ssc_sri) {
    ssc->ssc_channels = pkt->pkt_channels;
    ssc->ssc_sri      = pkt->pkt_sri;
  }

  if(SCT_ISVIDEO(ssc->ssc_type)) {
    if(pkt->pkt_aspect_num && pkt->pkt_aspect_den) {
      ssc->ssc_aspect_num = pkt->pkt_aspect_num;
      ssc->ssc_aspect_den = pkt->pkt_aspect_den;
    }
  }

  if(ssc->ssc_gh != NULL)
    return;

  switch(ssc->ssc_type) {
  case SCT_AAC:
    ssc->ssc_gh = pktbuf_alloc(NULL, 2);
    d = pktbuf_ptr(ssc->ssc_gh);

    const int profile = 2;
    d[0] = (profile << 3) | ((pkt->pkt_sri & 0xe) >> 1);
    d[1] = ((pkt->pkt_sri & 0x1) << 7) | (pkt->pkt_channels << 3);
    break;

  case SCT_H264:
  case SCT_MPEG2VIDEO:

    if(pkt->pkt_header != NULL) {
      ssc->ssc_gh = pkt->pkt_header;
      pktbuf_ref_inc(ssc->ssc_gh);
    }
    break;
  }
}


/**
 *
 */
static int
header_complete(streaming_start_component_t *ssc, int not_so_picky)
{
  if((SCT_ISAUDIO(ssc->ssc_type) || SCT_ISVIDEO(ssc->ssc_type)) &&
     ssc->ssc_frameduration == 0)
    return 0;

  if(SCT_ISVIDEO(ssc->ssc_type)) {
    if(!not_so_picky && (ssc->ssc_aspect_num == 0 || ssc->ssc_aspect_den == 0))
      return 0;
  }

  if(SCT_ISAUDIO(ssc->ssc_type) &&
     (ssc->ssc_sri == 0 || ssc->ssc_channels == 0))
    return 0;
  
  if(ssc->ssc_gh == NULL &&
     (ssc->ssc_type == SCT_H264 ||
      ssc->ssc_type == SCT_MPEG2VIDEO ||
      ssc->ssc_type == SCT_AAC))
    return 0;
  return 1;
}

/**
 *
 */
static int
headers_complete(globalheaders_t *gh, int64_t qd)
{
  streaming_start_t *ss = gh->gh_ss;
  streaming_start_component_t *ssc;
  int i, threshold = qd > (MAX_SCAN_TIME * 90);

  assert(ss != NULL);
 
  for(i = 0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    if(!header_complete(ssc, threshold)) {

      if(threshold) {
	ssc->ssc_disabled = 1;
      } else {
	return 0;
      }
    }
  }

  return 1;
}



/**
 *
 */
static th_pkt_t *
convertpkt(streaming_start_component_t *ssc, th_pkt_t *pkt)
{
  switch(ssc->ssc_type) {
  case SCT_H264:
    return avc_convert_pkt(pkt);

  default:
    return pkt;
  }
}


/**
 *
 */
static int64_t 
gh_queue_delay(globalheaders_t *gh)
{
  th_pktref_t *f = TAILQ_FIRST(&gh->gh_holdq);
  th_pktref_t *l = TAILQ_LAST(&gh->gh_holdq, th_pktref_queue);

  return l->pr_pkt->pkt_dts - f->pr_pkt->pkt_dts;
}


/**
 *
 */
static void
gh_hold(globalheaders_t *gh, streaming_message_t *sm)
{
  th_pkt_t *pkt;
  th_pktref_t *pr;
  streaming_start_component_t *ssc;

  switch(sm->sm_type) {
  case SMT_PACKET:
    pkt = sm->sm_data;
    ssc = streaming_start_component_find_by_index(gh->gh_ss, 
						  pkt->pkt_componentindex);
    assert(ssc != NULL);

    pkt = convertpkt(ssc, pkt);

    apply_header(ssc, pkt);

    pr = pktref_create(pkt);
    TAILQ_INSERT_TAIL(&gh->gh_holdq, pr, pr_link);

    free(sm);

    if(!headers_complete(gh, gh_queue_delay(gh))) 
      break;

    // Send our modified start
    sm = streaming_msg_create_data(SMT_START, 
				   streaming_start_copy(gh->gh_ss));
    streaming_target_deliver2(gh->gh_output, sm);
   
    // Send all pending packets
    while((pr = TAILQ_FIRST(&gh->gh_holdq)) != NULL) {
      TAILQ_REMOVE(&gh->gh_holdq, pr, pr_link);
      sm = streaming_msg_create_pkt(pr->pr_pkt);
      streaming_target_deliver2(gh->gh_output, sm);
      pkt_ref_dec(pr->pr_pkt);
      free(pr);
    }
    gh->gh_passthru = 1;
    break;

  case SMT_START:
    assert(gh->gh_ss == NULL);
    gh->gh_ss = streaming_start_copy(sm->sm_data);
    streaming_msg_free(sm);
    break;

  case SMT_STOP:
    gh_flush(gh);
    streaming_msg_free(sm);
    break;

  case SMT_EXIT:
  case SMT_SERVICE_STATUS:
  case SMT_NOSTART:
  case SMT_MPEGTS:
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
  streaming_start_component_t *ssc;

  switch(sm->sm_type) {
  case SMT_START:
    abort(); // Should not happen
    
  case SMT_STOP:
    gh->gh_passthru = 0;
    gh_flush(gh);
    // FALLTHRU
  case SMT_EXIT:
  case SMT_SERVICE_STATUS:
  case SMT_NOSTART:
  case SMT_MPEGTS:
    streaming_target_deliver2(gh->gh_output, sm);
    break;

  case SMT_PACKET:
    pkt = sm->sm_data;
    ssc = streaming_start_component_find_by_index(gh->gh_ss, 
						  pkt->pkt_componentindex);
    sm->sm_data = convertpkt(ssc, pkt);
    streaming_target_deliver2(gh->gh_output, sm);
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


/**
 *
 */
streaming_target_t *
globalheaders_create(streaming_target_t *output)
{
  globalheaders_t *gh = calloc(1, sizeof(globalheaders_t));

  TAILQ_INIT(&gh->gh_holdq);

  gh->gh_output = output;
  streaming_target_init(&gh->gh_input, globalheaders_input, gh, 0);
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

