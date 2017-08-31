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
#include <assert.h>

#include "tvheadend.h"
#include "streaming.h"
#include "packet.h"
#include "atomic.h"
#include "service.h"
#include "timeshift.h"

static memoryinfo_t streaming_msg_memoryinfo = { .my_name = "Streaming message" };

void
streaming_pad_init(streaming_pad_t *sp)
{
  LIST_INIT(&sp->sp_targets);
  sp->sp_ntargets = 0;
  sp->sp_reject_filter = ~0;
}

/**
 *
 */
void
streaming_target_init(streaming_target_t *st, streaming_ops_t *ops,
                      void *opaque, int reject_filter)
{
  st->st_ops = *ops;
  st->st_opaque = opaque;
  st->st_reject_filter = reject_filter;
}

/**
 *
 */
static size_t
streaming_message_data_size(streaming_message_t *sm)
{
  if (sm->sm_type == SMT_PACKET) {
    th_pkt_t *pkt = sm->sm_data;
    if (pkt && pkt->pkt_payload)
      return pktbuf_len(pkt->pkt_payload);
  } else if (sm->sm_type == SMT_MPEGTS) {
    pktbuf_t *pkt_payload = sm->sm_data;
    if (pkt_payload)
      return pktbuf_len(pkt_payload);
  }
  return 0;
}

/**
 *
 */
static void
streaming_queue_deliver(void *opauqe, streaming_message_t *sm)
{
  streaming_queue_t *sq = opauqe;

  pthread_mutex_lock(&sq->sq_mutex);

  /* queue size protection */
  if (sq->sq_maxsize && sq->sq_maxsize < sq->sq_size) {
    streaming_msg_free(sm);
  } else {
    TAILQ_INSERT_TAIL(&sq->sq_queue, sm, sm_link);
    sq->sq_size += streaming_message_data_size(sm);
  }

  tvh_cond_signal(&sq->sq_cond, 0);
  pthread_mutex_unlock(&sq->sq_mutex);
}

/**
 *
 */
static htsmsg_t *
streaming_queue_info(void *opaque, htsmsg_t *list)
{
  streaming_queue_t *sq = opaque;
  char buf[256];
  snprintf(buf, sizeof(buf), "streaming queue %p size %zd", sq, sq->sq_size);
  htsmsg_add_str(list, NULL, buf);
  return list;
}

/**
 *
 */
void
streaming_queue_remove(streaming_queue_t *sq, streaming_message_t *sm)
{
  sq->sq_size -= streaming_message_data_size(sm);
  TAILQ_REMOVE(&sq->sq_queue, sm, sm_link);
}

/**
 *
 */
void
streaming_queue_init(streaming_queue_t *sq, int reject_filter, size_t maxsize)
{
  static streaming_ops_t ops = {
    .st_cb   = streaming_queue_deliver,
    .st_info = streaming_queue_info
  };

  streaming_target_init(&sq->sq_st, &ops, sq, reject_filter);

  pthread_mutex_init(&sq->sq_mutex, NULL);
  tvh_cond_init(&sq->sq_cond);
  TAILQ_INIT(&sq->sq_queue);

  sq->sq_maxsize = maxsize;
  sq->sq_size = 0;
}

/**
 *
 */
void
streaming_queue_deinit(streaming_queue_t *sq)
{
  sq->sq_size = 0;
  streaming_queue_clear(&sq->sq_queue);
  pthread_mutex_destroy(&sq->sq_mutex);
  tvh_cond_destroy(&sq->sq_cond);
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
void
streaming_target_connect(streaming_pad_t *sp, streaming_target_t *st)
{
  sp->sp_ntargets++;
  st->st_pad = sp;
  LIST_INSERT_HEAD(&sp->sp_targets, st, st_link);
  sp->sp_reject_filter &= st->st_reject_filter;
}


/**
 *
 */
void
streaming_target_disconnect(streaming_pad_t *sp, streaming_target_t *st)
{
  int filter;

  sp->sp_ntargets--;
  st->st_pad = NULL;

  LIST_REMOVE(st, st_link);

  filter = ~0;
  LIST_FOREACH(st, &sp->sp_targets, st_link)
    filter &= st->st_reject_filter;
  sp->sp_reject_filter = filter;
}


/**
 *
 */
streaming_message_t *
streaming_msg_create(streaming_message_type_t type)
{
  streaming_message_t *sm = malloc(sizeof(streaming_message_t));
  memoryinfo_alloc(&streaming_msg_memoryinfo, sizeof(*sm));
  sm->sm_type = type;
#if ENABLE_TIMESHIFT
  sm->sm_time = 0;
#endif
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

  memoryinfo_alloc(&streaming_msg_memoryinfo, sizeof(*dst));

  dst->sm_type      = src->sm_type;
#if ENABLE_TIMESHIFT
  dst->sm_time      = src->sm_time;
#endif

  switch(src->sm_type) {

  case SMT_PACKET:
    pkt_ref_inc(src->sm_data);
    dst->sm_data = src->sm_data;
    break;

  case SMT_START:
    ss = dst->sm_data = src->sm_data;
    streaming_start_ref(ss);
    break;

  case SMT_SKIP:
    dst->sm_data = malloc(sizeof(streaming_skip_t));
    memcpy(dst->sm_data, src->sm_data, sizeof(streaming_skip_t));
    break;

  case SMT_SIGNAL_STATUS:
    dst->sm_data = malloc(sizeof(signal_status_t));
    memcpy(dst->sm_data, src->sm_data, sizeof(signal_status_t));
    break;

  case SMT_DESCRAMBLE_INFO:
    dst->sm_data = malloc(sizeof(descramble_info_t));
    memcpy(dst->sm_data, src->sm_data, sizeof(descramble_info_t));
    break;

  case SMT_TIMESHIFT_STATUS:
    dst->sm_data = malloc(sizeof(timeshift_status_t));
    memcpy(dst->sm_data, src->sm_data, sizeof(timeshift_status_t));
    break;

  case SMT_GRACE:
  case SMT_SPEED:
  case SMT_STOP:
  case SMT_SERVICE_STATUS:
  case SMT_NOSTART:
  case SMT_NOSTART_WARN:
    dst->sm_code = src->sm_code;
    break;

  case SMT_EXIT:
    break;

  case SMT_MPEGTS:
    pktbuf_ref_inc(src->sm_data);
    dst->sm_data = src->sm_data;
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
  if (!sm)
    return;

  switch(sm->sm_type) {
  case SMT_PACKET:
    if(sm->sm_data)
      pkt_ref_dec(sm->sm_data);
    break;

  case SMT_START:
    if(sm->sm_data)
      streaming_start_unref(sm->sm_data);
    break;

  case SMT_GRACE:
  case SMT_STOP:
  case SMT_EXIT:
  case SMT_SERVICE_STATUS:
  case SMT_NOSTART:
  case SMT_NOSTART_WARN:
  case SMT_SPEED:
    break;

  case SMT_SKIP:
  case SMT_SIGNAL_STATUS:
  case SMT_DESCRAMBLE_INFO:
#if ENABLE_TIMESHIFT
  case SMT_TIMESHIFT_STATUS:
#endif
    free(sm->sm_data);
    break;

  case SMT_MPEGTS:
    if(sm->sm_data)
      pktbuf_ref_dec(sm->sm_data);
    break;

  default:
    abort();
  }
  memoryinfo_free(&streaming_msg_memoryinfo, sizeof(*sm));
  free(sm);
}

/**
 *
 */
void
streaming_target_deliver2(streaming_target_t *st, streaming_message_t *sm)
{
  if (st->st_reject_filter & SMT_TO_MASK(sm->sm_type))
    streaming_msg_free(sm);
  else
    streaming_target_deliver(st, sm);
}

/**
 *
 */
void
streaming_pad_deliver(streaming_pad_t *sp, streaming_message_t *sm)
{
  streaming_target_t *st, *next, *run = NULL;

  for (st = LIST_FIRST(&sp->sp_targets); st; st = next) {
    next = LIST_NEXT(st, st_link);
    assert(next != st);
    if (st->st_reject_filter & SMT_TO_MASK(sm->sm_type))
      continue;
    if (run)
      streaming_target_deliver(run, streaming_msg_clone(sm));
    run = st;
  }
  if (run)
    streaming_target_deliver(run, sm);
  else
    streaming_msg_free(sm);
}

/**
 *
 */
const char *
streaming_code2txt(int code)
{
  static __thread char ret[64];

  switch(code) {
  case SM_CODE_OK:
    return N_("OK");

  case SM_CODE_SOURCE_RECONFIGURED:
    return N_("Source reconfigured");
  case SM_CODE_BAD_SOURCE:
    return N_("Source quality is bad");
  case SM_CODE_SOURCE_DELETED:
    return N_("Source deleted");
  case SM_CODE_SUBSCRIPTION_OVERRIDDEN:
    return N_("Subscription overridden");
  case SM_CODE_INVALID_TARGET:
    return N_("Invalid target");
  case SM_CODE_USER_ACCESS:
    return N_("User access error");
  case SM_CODE_USER_LIMIT:
    return N_("User limit reached");
  case SM_CODE_WEAK_STREAM:
    return N_("Weak stream");
  case SM_CODE_USER_REQUEST:
    return N_("User request");

  case SM_CODE_NO_FREE_ADAPTER:
    return N_("No free adapter");
  case SM_CODE_MUX_NOT_ENABLED:
    return N_("Mux not enabled");
  case SM_CODE_NOT_FREE:
    return N_("Adapter in use by another subscription");
  case SM_CODE_TUNING_FAILED:
    return N_("Tuning failed");
  case SM_CODE_SVC_NOT_ENABLED:
    return N_("No service enabled");
  case SM_CODE_BAD_SIGNAL:
    return N_("Signal quality too poor");
  case SM_CODE_NO_SOURCE:
    return N_("No source available");
  case SM_CODE_NO_SERVICE:
    return N_("No service assigned to channel");
  case SM_CODE_NO_ADAPTERS:
    return N_("No assigned adapters");
  case SM_CODE_INVALID_SERVICE:
    return N_("Invalid service");

  case SM_CODE_ABORTED:
    return N_("Aborted by user");

  case SM_CODE_NO_DESCRAMBLER:
    return N_("No descrambler");
  case SM_CODE_NO_ACCESS:
    return N_("No access");
  case SM_CODE_NO_INPUT:
    return N_("No input detected");
  case SM_CODE_NO_SPACE:
    return N_("Not enough disk space");

  default:
    snprintf(ret, sizeof(ret), _("Unknown reason (%i)"), code);
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

/**
 *
 */
static struct strtab streamtypetab[] = {
  { "NONE",       SCT_NONE },
  { "UNKNOWN",    SCT_UNKNOWN },
  { "RAW",        SCT_RAW },
  { "PCR",        SCT_PCR },
  { "MPEG2VIDEO", SCT_MPEG2VIDEO },
  { "MPEG2AUDIO", SCT_MPEG2AUDIO },
  { "H264",       SCT_H264 },
  { "AC3",        SCT_AC3 },
  { "TELETEXT",   SCT_TELETEXT },
  { "DVBSUB",     SCT_DVBSUB },
  { "CA",         SCT_CA },
  { "AAC",        SCT_AAC },
  { "MPEGTS",     SCT_MPEGTS },
  { "TEXTSUB",    SCT_TEXTSUB },
  { "EAC3",       SCT_EAC3 },
  { "AAC-LATM",   SCT_MP4A },
  { "VP8",        SCT_VP8 },
  { "VORBIS",     SCT_VORBIS },
  { "HEVC",       SCT_HEVC },
  { "VP9",        SCT_VP9 },
  { "HBBTV",      SCT_HBBTV },
  { "THEORA",     SCT_THEORA },
  { "OPUS",       SCT_OPUS },
};

/**
 *
 */
const char *
streaming_component_type2txt(streaming_component_type_t s)
{
  return val2str(s, streamtypetab) ?: "INVALID";
}

streaming_component_type_t
streaming_component_txt2type(const char *s)
{
  return s ? str2val(s, streamtypetab) : SCT_UNKNOWN;
}

const char *
streaming_component_audio_type2desc(int audio_type)
{
  /* From ISO 13818-1 - ISO 639 language descriptor */
  switch(audio_type) {
    case 0: return ""; /* "Undefined" in the standard, but used for normal audio */
    case 1: return N_("Clean effects");
    case 2: return N_("Hearing impaired");
    case 3: return N_("Visually impaired commentary/audio description");
  }

  return N_("Reserved");
}

/*
 *
 */
void streaming_init(void)
{
  memoryinfo_register(&streaming_msg_memoryinfo);
}

void streaming_done(void)
{
  pthread_mutex_lock(&global_lock);
  memoryinfo_unregister(&streaming_msg_memoryinfo);
  pthread_mutex_unlock(&global_lock);
}
