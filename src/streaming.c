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
streaming_target_init(streaming_target_t *st, st_callback_t *cb, void *opaque)
{
  st->st_cb = cb;
  st->st_opaque = opaque;
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
  streaming_target_init(&sq->sq_st, streaming_queue_deliver, sq);

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
    dst->sm_code = src->sm_code;
    break;

  case SMT_EXIT:
    break;

  default:
    abort();
  }
  return dst;
}


/**
 *
 */
static void
streaming_start_deref(streaming_start_t *ss)
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
    streaming_start_deref(sm->sm_data);
    break;

  case SMT_STOP:
    break;

  case SMT_EXIT:
    break;

  case SMT_TRANSPORT_STATUS:
    break;

  case SMT_NOSOURCE:
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

  if(sp->sp_ntargets == 0)
    return;

  for(st = LIST_FIRST(&sp->sp_targets);; st = next) {

    if((next = LIST_NEXT(st, st_link)) == NULL)
      break;
    
    st->st_cb(st->st_opaque, streaming_msg_clone(sm));
  }
  st->st_cb(st->st_opaque, sm);
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
