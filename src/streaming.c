/**
 *  Streaming helpers
 *  Copyright (C) 2008 Andreas Öman
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

#include <string.h>

#include "tvheadend.h"
#include "streaming.h"
#include "packet.h"
#include "atomic.h"
#include "service.h"

void
streaming_pad_init(streaming_pad_t *sp)
{
  LIST_INIT(&sp->sp_targets);
}

/**
 *
 */
void
streaming_target_init(streaming_target_t *st, st_callback_t *cb, void *opaque,
		      int reject_filter)
{
  st->st_cb = cb;
  st->st_opaque = opaque;
  st->st_reject_filter = reject_filter;
  st->st_clone = 1;
}


/**
 *
 */
static void 
streaming_queue_deliver(void *opauqe, streaming_message_t *sm)
{
  streaming_queue_t *sq = opauqe;

  pthread_mutex_lock(&sq->sq_mutex);
  TAILQ_INSERT_TAIL(&sq->sq_queue, sm, sm_link);
  pthread_cond_signal(&sq->sq_cond);
  pthread_mutex_unlock(&sq->sq_mutex);
}


/**
 *
 */
static void 
streaming_queue_deliver2(void *opauqe, streaming_message_t *sm)
{
  streaming_queue_t *sq = opauqe;

  if (sm->sm_type == SMT_MPEGTS) {
    if (((streaming_tsbuf_t *)sm->sm_data)->ts_cnt == 1) {
      streaming_tsbuf_t *tsbuf = sq->sq_tsbuf;
      if (tsbuf == NULL) {
        sq->sq_tsbuf = tsbuf = malloc(sizeof(*tsbuf) + 100*188);
        tsbuf->ts_data = (uint8_t *)tsbuf + sizeof(*tsbuf);
        tsbuf->ts_cnt = 0;
      }
      memcpy(tsbuf->ts_data + tsbuf->ts_cnt * 188,
                        ((streaming_tsbuf_t *)sm->sm_data)->ts_data, 188);
      tsbuf->ts_cnt++;
      if (tsbuf->ts_cnt < 100)
        return;
      sm = NULL;
    } else {
      sm = streaming_msg_clone(sm);
    }
  }
  if (sq->sq_tsbuf) {
    streaming_message_t *sm2 = streaming_msg_create_data(SMT_MPEGTS, sq->sq_tsbuf);
    sq->sq_tsbuf = NULL;
    pthread_mutex_lock(&sq->sq_mutex);
    TAILQ_INSERT_TAIL(&sq->sq_queue, sm2, sm_link);
    goto insert;
  }
  pthread_mutex_lock(&sq->sq_mutex);
insert:
  if (sm != NULL)
    TAILQ_INSERT_TAIL(&sq->sq_queue, sm, sm_link);
  pthread_cond_signal(&sq->sq_cond);
  pthread_mutex_unlock(&sq->sq_mutex);
}


/**
 *
 */
void
streaming_queue_init(streaming_queue_t *sq, int reject_filter)
{
  if ((reject_filter & SMT_TO_MASK(SMT_PACKET)) == 0) {
    streaming_target_init(&sq->sq_st, streaming_queue_deliver, sq, reject_filter);
  } else {
    streaming_target_init(&sq->sq_st, streaming_queue_deliver2, sq, reject_filter);
    sq->sq_st.st_clone = 0;
  }

  pthread_mutex_init(&sq->sq_mutex, NULL);
  pthread_cond_init(&sq->sq_cond, NULL);
  TAILQ_INIT(&sq->sq_queue);
  sq->sq_tsbuf = NULL;
}


/**
 *
 */
void
streaming_queue_deinit(streaming_queue_t *sq)
{
  streaming_queue_clear(&sq->sq_queue);
  pthread_mutex_destroy(&sq->sq_mutex);
  pthread_cond_destroy(&sq->sq_cond);
  if (sq->sq_tsbuf)
    free(sq->sq_tsbuf);
}


/**
 *
 */
void
streaming_target_connect(streaming_pad_t *sp, streaming_target_t *st)
{
  sp->sp_ntargets++;
  st->st_pad = sp;
  LIST_INSERT_HEAD(&sp->sp_targets, st, st_link);
}


/**
 *
 */
void
streaming_target_disconnect(streaming_pad_t *sp, streaming_target_t *st)
{
  sp->sp_ntargets--;
  st->st_pad = NULL;

  LIST_REMOVE(st, st_link);
}


/**
 *
 */
streaming_message_t *
streaming_msg_create(streaming_message_type_t type)
{
  streaming_message_t *sm = malloc(sizeof(streaming_message_t));
  sm->sm_type = type;
  return sm;
}


/**
 *
 */
streaming_message_t *
streaming_msg_create_pkt(th_pkt_t *pkt)
{
  streaming_message_t *sm = streaming_msg_create(SMT_PACKET);
  sm->sm_data = pkt;
  pkt_ref_inc(pkt);
  return sm;
}


/**
 *
 */
streaming_message_t *
streaming_msg_create_data(streaming_message_type_t type, void *data)
{
  streaming_message_t *sm = streaming_msg_create(type);
  sm->sm_data = data;
  return sm;
}


/**
 *
 */
streaming_message_t *
streaming_msg_create_code(streaming_message_type_t type, int code)
{
  streaming_message_t *sm = streaming_msg_create(type);
  sm->sm_code = code;
  return sm;
}



/**
 *
 */
streaming_message_t *
streaming_msg_clone(streaming_message_t *src)
{
  streaming_message_t *dst = malloc(sizeof(streaming_message_t));
  streaming_start_t *ss;

  dst->sm_type = src->sm_type;

  switch(src->sm_type) {

  case SMT_PACKET:
    pkt_ref_inc(src->sm_data);
    dst->sm_data = src->sm_data;
    break;

  case SMT_START:
    ss = dst->sm_data = src->sm_data;
    atomic_add(&ss->ss_refcount, 1);
    break;

  case SMT_STOP:
  case SMT_SERVICE_STATUS:
  case SMT_NOSTART:
    dst->sm_code = src->sm_code;
    break;

  case SMT_EXIT:
    break;

  case SMT_MPEGTS: {
      streaming_tsbuf_t *src_tsbuf = src->sm_data;
      streaming_tsbuf_t *dst_tsbuf;
      uint32_t size = src_tsbuf->ts_cnt * 188;
      
      dst_tsbuf = dst->sm_data = malloc(sizeof(*dst_tsbuf) + size);
      dst_tsbuf->ts_cnt = src_tsbuf->ts_cnt;
      dst_tsbuf->ts_data = (uint8_t *)dst_tsbuf + sizeof(*dst_tsbuf);
      
      memcpy(dst_tsbuf->ts_data, src_tsbuf->ts_data, size);
    }
    break;

  default:
    abort();
  }
  return dst;
}


/**
 *
 */
void
streaming_start_unref(streaming_start_t *ss)
{
  int i;

  if((atomic_add(&ss->ss_refcount, -1)) != 1)
    return;

  service_source_info_free(&ss->ss_si);
  for(i = 0; i < ss->ss_num_components; i++)
    if(ss->ss_components[i].ssc_gh)
      pktbuf_ref_dec(ss->ss_components[i].ssc_gh);
  free(ss);
}

/**
 *
 */
void
streaming_msg_free(streaming_message_t *sm)
{
  switch(sm->sm_type) {
  case SMT_PACKET:
    if(sm->sm_data)
      pkt_ref_dec(sm->sm_data);
    break;

  case SMT_START:
    if(sm->sm_data)
      streaming_start_unref(sm->sm_data);
    break;

  case SMT_STOP:
    break;

  case SMT_EXIT:
    break;

  case SMT_SERVICE_STATUS:
    break;

  case SMT_NOSTART:
    break;

  case SMT_MPEGTS:
    free(sm->sm_data);
    break;

  default:
    abort();
  }
  free(sm);
}

/**
 *
 */
void
streaming_target_deliver2(streaming_target_t *st, streaming_message_t *sm)
{
  if(st->st_reject_filter & SMT_TO_MASK(sm->sm_type))
    streaming_msg_free(sm);
  else
    st->st_cb(st->st_opaque, sm);
}

/**
 *
 */
void
streaming_pad_deliver(streaming_pad_t *sp, streaming_message_t *sm)
{
  streaming_target_t *st, *next;

  for(st = LIST_FIRST(&sp->sp_targets);st; st = next) {

    next = LIST_NEXT(st, st_link);
    if(st->st_reject_filter & SMT_TO_MASK(sm->sm_type))
      continue;
    if (st->st_clone || sm->sm_type != SMT_MPEGTS)
      st->st_cb(st->st_opaque, streaming_msg_clone(sm));
    else
      st->st_cb(st->st_opaque, sm);
  }
}


/**
 *
 */
int
streaming_pad_probe_type(streaming_pad_t *sp, streaming_message_type_t smt)
{
  streaming_target_t *st;

  LIST_FOREACH(st, &sp->sp_targets, st_link) {
    if(!(st->st_reject_filter & SMT_TO_MASK(smt)))
      return 1;
  }
  return 0;
}


/**
 *
 */
void
streaming_queue_clear(struct streaming_message_queue *q)
{
  streaming_message_t *sm;

  while((sm = TAILQ_FIRST(q)) != NULL) {
    TAILQ_REMOVE(q, sm, sm_link);
    streaming_msg_free(sm);
  }
}


/**
 *
 */
const char *
streaming_code2txt(int code)
{
  static __thread char ret[64];

  switch(code) {
  case SM_CODE_OK: return "OK";
    
  case SM_CODE_SOURCE_RECONFIGURED:
    return "Source reconfigured";
  case SM_CODE_BAD_SOURCE:
    return "Source quality is bad";
  case SM_CODE_SOURCE_DELETED:
    return "Source deleted";
  case SM_CODE_SUBSCRIPTION_OVERRIDDEN:
    return "Subscription overridden";

  case SM_CODE_NO_HW_ATTACHED:
    return "No hardware present";
  case SM_CODE_MUX_NOT_ENABLED:
    return "Mux not enabled";
  case SM_CODE_NOT_FREE:
    return "Adapter in use by other subscription";
  case SM_CODE_TUNING_FAILED:
    return "Tuning failed";
  case SM_CODE_SVC_NOT_ENABLED:
    return "No service enabled";
  case SM_CODE_BAD_SIGNAL:
    return "Too bad signal quality";
  case SM_CODE_NO_SOURCE:
    return "No source available";
  case SM_CODE_NO_SERVICE:
    return "No service assigned to channel";

  case SM_CODE_ABORTED:
    return "Aborted by user";

  case SM_CODE_NO_DESCRAMBLER:
    return "No descrambler";
  case SM_CODE_NO_ACCESS:
    return "No access";
  case SM_CODE_NO_INPUT:
    return "No input detected";

  default:
    snprintf(ret, sizeof(ret), "Unknown reason (%i)", code);
    return ret;
  }
}


/**
 *
 */
streaming_start_t *
streaming_start_copy(const streaming_start_t *src)
{
  int i;
  size_t siz = sizeof(streaming_start_t) + 
    sizeof(streaming_start_component_t) * src->ss_num_components;
  
  streaming_start_t *dst = malloc(siz);
  
  memcpy(dst, src, siz);
  service_source_info_copy(&dst->ss_si, &src->ss_si);

  for(i = 0; i < dst->ss_num_components; i++) {
    streaming_start_component_t *ssc = &dst->ss_components[i];
    if(ssc->ssc_gh != NULL)
      pktbuf_ref_inc(ssc->ssc_gh);
  }

  dst->ss_refcount = 1;
  return dst;
}


/**
 *
 */
streaming_start_component_t *
streaming_start_component_find_by_index(streaming_start_t *ss, int idx)
{
  int i;
  for(i = 0; i < ss->ss_num_components; i++) {
    streaming_start_component_t *ssc = &ss->ss_components[i];
    if(ssc->ssc_index == idx)
      return ssc;
  }
  return NULL;
}
