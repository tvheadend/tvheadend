/*
 *  Stream plumbing, connects individual streaming components to each other
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

#ifndef STREAMING_H_
#define STREAMING_H_

#include "packet.h"
#include "htsmsg.h"

/**
 *
 */
struct service;

/**
 *
 */
typedef enum commercial_advice commercial_advice_t;
typedef enum signal_status_scale signal_status_scale_t;
typedef struct signal_status signal_status_t;
typedef struct descramble_info descramble_info_t;
typedef struct timeshift_status timeshift_status_t;
typedef struct streaming_skip streaming_skip_t;
typedef struct streaming_pad streaming_pad_t;
typedef enum streaming_message_type streaming_message_type_t;
typedef enum signal_state signal_state_t;
typedef struct streaming_message streaming_message_t;
typedef struct streaming_ops streaming_ops_t;
typedef struct streaming_queue streaming_queue_t;
typedef struct source_info source_info_t;
typedef struct streaming_start_component streaming_start_component_t;
typedef struct streaming_start streaming_start_t;

/**
 *
 */
LIST_HEAD(streaming_target_list, streaming_target);
TAILQ_HEAD(streaming_message_queue, streaming_message);

/*
 * Commercial status
 */
enum commercial_advice {
  COMMERCIAL_UNKNOWN,
  COMMERCIAL_YES,
  COMMERCIAL_NO,
};

/*
 * Scales for signal status values
 */
enum signal_status_scale {
  SIGNAL_STATUS_SCALE_UNKNOWN = 0,
  SIGNAL_STATUS_SCALE_RELATIVE, // value is unsigned, where 0 means 0% and 65535 means 100%
  SIGNAL_STATUS_SCALE_DECIBEL   // value is measured in dB * 1000
};

/**
 * The signal status of a tuner
 */
struct signal_status {
  const char *status_text; /* adapter status text */
  int snr;      /* signal/noise ratio */
  signal_status_scale_t snr_scale;
  int signal;   /* signal strength */
  signal_status_scale_t signal_scale;
  int ber;      /* bit error rate */
  int unc;      /* uncorrected blocks */
  int ec_bit;   /* error bit count */
  int tc_bit;   /* total bit count */
  int ec_block; /* error block count */
  int tc_block; /* total block count */
};

/**
 * Descramble info
 */
struct descramble_info {
  uint16_t pid;
  uint16_t caid;
  uint32_t provid;
  uint32_t ecmtime;
  uint16_t hops;
  char cardsystem[128];
  char reader    [128];
  char from      [128];
  char protocol  [128];
};

/**
 *
 */
struct timeshift_status
{
  int     full;
  int64_t shift;
  int64_t pts_start;
  int64_t pts_end;
};

/**
 * Streaming skip
 */
struct streaming_skip
{
  enum {
    SMT_SKIP_ERROR,
    SMT_SKIP_REL_TIME,
    SMT_SKIP_ABS_TIME,
    SMT_SKIP_REL_SIZE,
    SMT_SKIP_ABS_SIZE,
    SMT_SKIP_LIVE
  } type;
  union {
    off_t   size;
    int64_t time;
  };
#if ENABLE_TIMESHIFT
  timeshift_status_t timeshift;
#endif
};


/**
 * A streaming pad generates data.
 * It has one or more streaming targets attached to it.
 *
 * We support two different streaming target types:
 * One is callback driven and the other uses a queue + thread.
 *
 * Targets which already has a queueing intrastructure in place (such
 * as HTSP) does not need any interim queues so it would be a waste. That
 * is why we have the callback target.
 *
 */
struct streaming_pad {
  struct streaming_target_list sp_targets;
  int sp_ntargets;
  int sp_reject_filter;
};

/**
 * Streaming messages types
 */
enum streaming_message_type {

  /**
   * Packet with data.
   *
   * sm_data points to a th_pkt. th_pkt will be unref'ed when
   * the message is destroyed
   */
  SMT_PACKET,

  /**
   * Stream grace period
   *
   * sm_code contains number of seconds to settle things down
   */

  SMT_GRACE,

  /**
   * Stream start
   *
   * sm_data points to a stream_start struct.
   * See transport_build_stream_start()
   */

  SMT_START,

  /**
   * Service status
   *
   * Notification about status of source, see TSS_ flags
   */
  SMT_SERVICE_STATUS,

  /**
   * Signal status
   *
   * Notification about frontend signal status
   */
  SMT_SIGNAL_STATUS,

  /**
   * Descrambler info message
   *
   * Notification about descrambler
   */
  SMT_DESCRAMBLE_INFO,

  /**
   * Streaming stop.
   *
   * End of streaming. If sm_code is 0 this was a result to an
   * unsubscription. Otherwise the reason was external and the
   * subscription scheduler will attempt to start a new streaming
   * session.
   */
  SMT_STOP,

  /**
   * Streaming unable to start.
   *
   * sm_code indicates reason. Scheduler will try to restart
   */
  SMT_NOSTART,

  /**
   * Streaming unable to start (non-fatal).
   *
   * sm_code indicates reason. Scheduler will try to restart
   */
  SMT_NOSTART_WARN,

  /**
   * Raw MPEG TS data
   */
  SMT_MPEGTS,

  /**
   * Internal message to exit receiver
   */
  SMT_EXIT,

  /**
   * Set stream speed
   */
  SMT_SPEED,

  /**
   * Skip the stream
   */
  SMT_SKIP,

  /**
   * Timeshift status
   */
  SMT_TIMESHIFT_STATUS,

};

#define SMT_TO_MASK(x) (1 << ((unsigned int)x))

/**
 * Streaming codes
 */
#define SM_CODE_OK                        0

#define SM_CODE_UNDEFINED_ERROR           1

#define SM_CODE_FORCE_OK                  10

#define SM_CODE_SOURCE_RECONFIGURED       100
#define SM_CODE_BAD_SOURCE                101
#define SM_CODE_SOURCE_DELETED            102
#define SM_CODE_SUBSCRIPTION_OVERRIDDEN   103
#define SM_CODE_INVALID_TARGET            104
#define SM_CODE_USER_ACCESS               105
#define SM_CODE_USER_LIMIT                106
#define SM_CODE_WEAK_STREAM               107
#define SM_CODE_USER_REQUEST              108
#define SM_CODE_PREVIOUSLY_RECORDED       109

#define SM_CODE_NO_FREE_ADAPTER           200
#define SM_CODE_MUX_NOT_ENABLED           201
#define SM_CODE_NOT_FREE                  202
#define SM_CODE_TUNING_FAILED             203
#define SM_CODE_SVC_NOT_ENABLED           204
#define SM_CODE_BAD_SIGNAL                205
#define SM_CODE_NO_SOURCE                 206
#define SM_CODE_NO_SERVICE                207
#define SM_CODE_NO_VALID_ADAPTER          208
#define SM_CODE_NO_ADAPTERS               209
#define SM_CODE_INVALID_SERVICE           210
#define SM_CODE_CHN_NOT_ENABLED           211
#define SM_CODE_DATA_TIMEOUT              212
#define SM_CODE_OTHER_SERVICE             213

#define SM_CODE_ABORTED                   300

#define SM_CODE_NO_DESCRAMBLER            400
#define SM_CODE_NO_ACCESS                 401
#define SM_CODE_NO_INPUT                  402
#define SM_CODE_NO_SPACE                  403

/**
 * Signal
 */
enum signal_state
{
  SIGNAL_UNKNOWN,
  SIGNAL_GOOD,
  SIGNAL_BAD,
  SIGNAL_FAINT,
  SIGNAL_NONE
};

const char * signal2str ( signal_state_t st );

/**
 * Streaming messages are sent from the pad to its receivers
 */
struct streaming_message {
  TAILQ_ENTRY(streaming_message) sm_link;
  streaming_message_type_t sm_type;
#if ENABLE_TIMESHIFT
  int64_t sm_time;
#endif
  union {
    void *sm_data;
    int sm_code;
  };
};

/**
 * A streaming target receives data.
 */
typedef void (st_callback_t)(void *opaque, streaming_message_t *sm);

struct streaming_ops {
  st_callback_t *st_cb;
  htsmsg_t *(*st_info)(void *opaque, htsmsg_t *list);
};

typedef struct streaming_target {
  LIST_ENTRY(streaming_target) st_link;
  streaming_pad_t *st_pad;               /* Source we are linked to */
  streaming_ops_t st_ops;
  void *st_opaque;
  int st_reject_filter;
} streaming_target_t;

/**
 *
 */
struct streaming_queue {

  streaming_target_t sq_st;

  tvh_mutex_t sq_mutex;    /* Protects sp_queue */
  tvh_cond_t  sq_cond;     /* Condvar for signalling new packets */

  size_t      sq_maxsize;  /* Max queue size (bytes) */
  size_t      sq_size;     /* Actual queue size (bytes) - only data */

  struct streaming_message_queue sq_queue;

};

streaming_component_type_t streaming_component_txt2type(const char *str);
const char *streaming_component_type2txt(streaming_component_type_t s);
streaming_component_type_t streaming_component_txt2type(const char *s);
const char *streaming_component_audio_type2desc(int audio_type);

/**
 * Source information
 */
struct source_info {
  tvh_uuid_t si_adapter_uuid;
  tvh_uuid_t si_network_uuid;
  tvh_uuid_t si_mux_uuid;
  char *si_adapter;
  char *si_network;
  char *si_network_type;
  char *si_satpos;
  char *si_mux;
  char *si_provider;
  char *si_service;
  int   si_type;
  uint16_t si_tsid;
  uint16_t si_onid;
};

/**
 *
 */
struct streaming_start_component {
  elementary_info_t;

  uint8_t ssc_disabled;
  uint8_t ssc_muxer_disabled;
  
  pktbuf_t *ssc_gh;
};


struct streaming_start {
  int ss_refcount;

  int ss_num_components;

  source_info_t ss_si;

  uint16_t ss_pcr_pid;
  uint16_t ss_pmt_pid;
  uint16_t ss_service_id;

  streaming_start_component_t ss_components[0];

};

/**
 *
 */
void streaming_pad_init(streaming_pad_t *sp);

void streaming_target_init(streaming_target_t *st,
			   streaming_ops_t *ops, void *opaque,
			   int reject_filter);

void streaming_queue_init
  (streaming_queue_t *sq, int reject_filter, size_t maxsize);

void streaming_queue_clear(struct streaming_message_queue *q);

void streaming_queue_deinit(streaming_queue_t *sq);

void streaming_queue_remove(streaming_queue_t *sq, streaming_message_t *sm);

void streaming_target_connect(streaming_pad_t *sp, streaming_target_t *st);

void streaming_target_disconnect(streaming_pad_t *sp, streaming_target_t *st);

void streaming_pad_deliver(streaming_pad_t *sp, streaming_message_t *sm);

void streaming_service_deliver(struct service *t, streaming_message_t *sm);

void streaming_msg_free(streaming_message_t *sm);

streaming_message_t *streaming_msg_clone(streaming_message_t *src);

streaming_message_t *streaming_msg_create(streaming_message_type_t type);

streaming_message_t *streaming_msg_create_data(streaming_message_type_t type, 
					       void *data);

streaming_message_t *streaming_msg_create_code(streaming_message_type_t type, 
					       int code);

streaming_message_t *streaming_msg_create_pkt(th_pkt_t *pkt);

static inline void
streaming_target_deliver(streaming_target_t *st, streaming_message_t *sm)
  { st->st_ops.st_cb(st->st_opaque, sm); }

void streaming_target_deliver2(streaming_target_t *st, streaming_message_t *sm);

static inline void streaming_start_ref(streaming_start_t *ss)
{
  atomic_add(&ss->ss_refcount, 1);
}

void streaming_start_unref(streaming_start_t *ss);

streaming_start_t *streaming_start_copy(const streaming_start_t *src);

static inline int
streaming_pad_probe_type(streaming_pad_t *sp, streaming_message_type_t smt)
{
  return (sp->sp_reject_filter & SMT_TO_MASK(smt)) == 0;
}

const char *streaming_code2txt(int code);

streaming_start_component_t *streaming_start_component_find_by_index(streaming_start_t *ss, int idx);

void streaming_init(void);
void streaming_done(void);

#endif /* STREAMING_H_ */
