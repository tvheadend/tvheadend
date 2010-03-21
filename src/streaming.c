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

#include "tvhead.h"
#include "streaming.h"
#include "packet.h"
#include "atomic.h"
#include "transports.h"

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
void
streaming_queue_init(streaming_queue_t *sq)
{
  streaming_target_init(&sq->sq_st, streaming_queue_deliver, sq, 0);

  pthread_mutex_init(&sq->sq_mutex, NULL);
  pthread_cond_init(&sq->sq_cond, NULL);
  TAILQ_INIT(&sq->sq_queue);
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
  case SMT_TRANSPORT_STATUS:
  case SMT_NOSTART:
    dst->sm_code = src->sm_code;
    break;

  case SMT_EXIT:
    break;

  case SMT_MPEGTS:
    dst->sm_data = malloc(188);
    memcpy(dst->sm_data, src->sm_data, 188);
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
  if((atomic_add(&ss->ss_refcount, -1)) != 1)
    return;

  transport_source_info_free(&ss->ss_si);
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

  case SMT_TRANSPORT_STATUS:
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
streaming_pad_deliver(streaming_pad_t *sp, streaming_message_t *sm)
{
  streaming_target_t *st, *next;

  for(st = LIST_FIRST(&sp->sp_targets);st; st = next) {

    next = LIST_NEXT(st, st_link);
    if(st->st_reject_filter & SMT_TO_MASK(sm->sm_type))
      continue;
    st->st_cb(st->st_opaque, streaming_msg_clone(sm));
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
  switch(code) {
  case SM_CODE_OK: return "OK";
    
  case SM_CODE_SOURCE_RECONFIGURED:
    return "Soruce reconfigured";
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

  case SM_CODE_ABORTED:
    return "Aborted by user";

  case SM_CODE_NO_DESCRAMBLER:
    return "No descrambler";

  case SM_CODE_NO_ACCESS:
    return "No access";


  default:
    return "Unknown reason";
  }
}
