/*
 *  Tvheadend - structures
 *  Copyright (C) 2007 Andreas Öman
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

#ifndef TVHEADEND_H
#define TVHEADEND_H

#include "config.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/time.h>

#include "queue.h"
#include "avg.h"
#include "hts_strtab.h"

#include "redblack.h"

#define PTS_UNSET INT64_C(0x8000000000000000)

extern pthread_mutex_t global_lock;
extern pthread_mutex_t ffmpeg_lock;
extern pthread_mutex_t fork_lock;

typedef struct source_info {
  char *si_device;
  char *si_adapter;
  char *si_network;
  char *si_mux;
  char *si_provider;
  char *si_service;
} source_info_t;

static inline void
lock_assert0(pthread_mutex_t *l, const char *file, int line)
{
  if(pthread_mutex_trylock(l) == EBUSY)
    return;

  fprintf(stderr, "Mutex not held at %s:%d\n", file, line);
  abort();
}

#define lock_assert(l) lock_assert0(l, __FILE__, __LINE__)


/*
 * Commercial status
 */
typedef enum {
  COMMERCIAL_UNKNOWN,
  COMMERCIAL_YES,
  COMMERCIAL_NO,
} th_commercial_advice_t;


/*
 * global timer
 */


typedef void (gti_callback_t)(void *opaque);

typedef struct gtimer {
  LIST_ENTRY(gtimer) gti_link;
  gti_callback_t *gti_callback;
  void *gti_opaque;
  time_t gti_expire;
} gtimer_t;

void gtimer_arm(gtimer_t *gti, gti_callback_t *callback, void *opaque,
		int delta);

void gtimer_arm_abs(gtimer_t *gti, gti_callback_t *callback, void *opaque,
		    time_t when);

void gtimer_disarm(gtimer_t *gti);


/*
 * List / Queue header declarations
 */
LIST_HEAD(th_subscription_list, th_subscription);
RB_HEAD(channel_tree, channel);
TAILQ_HEAD(channel_queue, channel);
LIST_HEAD(channel_list, channel);
LIST_HEAD(event_list, event);
RB_HEAD(event_tree, event);
LIST_HEAD(dvr_config_list, dvr_config);
LIST_HEAD(dvr_entry_list, dvr_entry);
TAILQ_HEAD(ref_update_queue, ref_update);
LIST_HEAD(service_list, service);
RB_HEAD(service_tree, service);
TAILQ_HEAD(service_queue, service);
LIST_HEAD(elementary_stream_list, elementary_stream);
TAILQ_HEAD(elementary_stream_queue, elementary_stream);
LIST_HEAD(th_muxer_list, th_muxer);
LIST_HEAD(th_muxstream_list, th_muxstream);
LIST_HEAD(th_descrambler_list, th_descrambler);
TAILQ_HEAD(th_refpkt_queue, th_refpkt);
TAILQ_HEAD(th_muxpkt_queue, th_muxpkt);
LIST_HEAD(dvr_autorec_entry_list, dvr_autorec_entry);
TAILQ_HEAD(th_pktref_queue, th_pktref);
LIST_HEAD(streaming_target_list, streaming_target);

/**
 * Log limiter
 */
typedef struct loglimter {
  time_t last;
  int events;
} loglimiter_t;

void limitedlog(loglimiter_t *ll, const char *sys, 
		const char *o, const char *event);


/**
 * Device connection types
 */
#define HOSTCONNECTION_UNKNOWN    0
#define HOSTCONNECTION_USB12      1
#define HOSTCONNECTION_USB480     2
#define HOSTCONNECTION_PCI        3

const char *hostconnection2str(int type);
int get_device_connection(const char *dev);


/**
 * Stream component types
 */
typedef enum {
  SCT_UNKNOWN = 0,
  SCT_MPEG2VIDEO = 1,
  SCT_MPEG2AUDIO,
  SCT_H264,
  SCT_AC3,
  SCT_TELETEXT,
  SCT_DVBSUB,
  SCT_CA,
  SCT_PAT,
  SCT_PMT,
  SCT_AAC,
  SCT_MPEGTS,
  SCT_TEXTSUB,
  SCT_EAC3,
} streaming_component_type_t;

#define SCT_ISVIDEO(t) ((t) == SCT_MPEG2VIDEO || (t) == SCT_H264)
#define SCT_ISAUDIO(t) ((t) == SCT_MPEG2AUDIO || (t) == SCT_AC3 || \
                        (t) == SCT_AAC)

/**
 * The signal status of a tuner
 */
typedef struct signal_status {
  const char *status_text; /* adapter status text */
  int snr;      /* signal/noise ratio */
  int signal;   /* signal strength */
  int ber;      /* bit error rate */
  int unc;      /* uncorrected blocks */
} signal_status_t;

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
typedef struct streaming_pad {
  struct streaming_target_list sp_targets;
  int sp_ntargets;
} streaming_pad_t;


TAILQ_HEAD(streaming_message_queue, streaming_message);

/**
 * Streaming messages types
 */
typedef enum {
  /**
   * Packet with data.
   *
   * sm_data points to a th_pkt. th_pkt will be unref'ed when 
   * the message is destroyed
   */
  SMT_PACKET,

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
   * Raw MPEG TS data
   */
  SMT_MPEGTS,

  /**
   * Internal message to exit receiver
   */
  SMT_EXIT,
} streaming_message_type_t;

#define SMT_TO_MASK(x) (1 << ((unsigned int)x))


#define SM_CODE_OK                        0

#define SM_CODE_UNDEFINED_ERROR           1

#define SM_CODE_SOURCE_RECONFIGURED       100
#define SM_CODE_BAD_SOURCE                101
#define SM_CODE_SOURCE_DELETED            102
#define SM_CODE_SUBSCRIPTION_OVERRIDDEN   103

#define SM_CODE_NO_HW_ATTACHED            200
#define SM_CODE_MUX_NOT_ENABLED           201
#define SM_CODE_NOT_FREE                  202
#define SM_CODE_TUNING_FAILED             203
#define SM_CODE_SVC_NOT_ENABLED           204
#define SM_CODE_BAD_SIGNAL                205
#define SM_CODE_NO_SOURCE                 206
#define SM_CODE_NO_SERVICE                207

#define SM_CODE_ABORTED                   300

#define SM_CODE_NO_DESCRAMBLER            400
#define SM_CODE_NO_ACCESS                 401
#define SM_CODE_NO_INPUT                  402

/**
 * Streaming messages are sent from the pad to its receivers
 */
typedef struct streaming_message {
  TAILQ_ENTRY(streaming_message) sm_link;
  streaming_message_type_t sm_type;
  union {
    void *sm_data;
    int sm_code;
  };
} streaming_message_t;

/**
 * A streaming target receives data.
 */

typedef void (st_callback_t)(void *opauqe, streaming_message_t *sm);

typedef struct streaming_target {
  LIST_ENTRY(streaming_target) st_link;
  streaming_pad_t *st_pad;               /* Source we are linked to */

  st_callback_t *st_cb;
  void *st_opaque;
  int st_reject_filter;
} streaming_target_t;


/**
 *
 */
typedef struct streaming_queue {
  
  streaming_target_t sq_st;

  pthread_mutex_t sq_mutex;              /* Protects sp_queue */
  pthread_cond_t  sq_cond;               /* Condvar for signalling new
					    packets */
  
  struct streaming_message_queue sq_queue;

} streaming_queue_t;


/**
 * Simple dynamically growing buffer
 */
typedef struct sbuf {
  uint8_t *sb_data;
  int sb_ptr;
  int sb_size;
  int sb_err;
} sbuf_t;



const char *streaming_component_type2txt(streaming_component_type_t s);

static inline unsigned int tvh_strhash(const char *s, unsigned int mod)
{
  unsigned int v = 5381;
  while(*s)
    v += (v << 5) + v + *s++;
  return v % mod;
}

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

void tvh_str_set(char **strp, const char *src);
int tvh_str_update(char **strp, const char *src);

void tvhlog(int severity, const char *subsys, const char *fmt, ...);

void tvhlog_spawn(int severity, const char *subsys, const char *fmt, ...);

#define	LOG_EMERG	0	/* system is unusable */
#define	LOG_ALERT	1	/* action must be taken immediately */
#define	LOG_CRIT	2	/* critical conditions */
#define	LOG_ERR		3	/* error conditions */
#define	LOG_WARNING	4	/* warning conditions */
#define	LOG_NOTICE	5	/* normal but significant condition */
#define	LOG_INFO	6	/* informational */
#define	LOG_DEBUG	7	/* debug-level messages */

extern int log_debug;

#define DEBUGLOG(subsys, fmt...) do { \
 if(log_debug) \
  tvhlog(LOG_DEBUG, subsys, fmt); \
} while(0)


static inline int64_t 
getmonoclock(void)
{
  struct timespec tp;

  clock_gettime(CLOCK_MONOTONIC, &tp);

  return tp.tv_sec * 1000000ULL + (tp.tv_nsec / 1000);
}

int sri_to_rate(int sri);
int rate_to_sri(int rate);


extern time_t dispatch_clock;
extern struct service_list all_transports;
extern struct channel_tree channel_name_tree;

extern void scopedunlock(pthread_mutex_t **mtxp);

#define scopedlock(mtx) \
 pthread_mutex_t *scopedlock ## __LINE__ \
 __attribute__((cleanup(scopedunlock))) = mtx; \
 pthread_mutex_lock(mtx);

#define scopedgloballock() scopedlock(&global_lock)

#define tvh_strdupa(n) ({ int tvh_l = strlen(n); \
 char *tvh_b = alloca(tvh_l + 1); \
 memcpy(tvh_b, n, tvh_l + 1); })

#define tvh_strlcatf(buf, size, fmt...) \
 snprintf((buf) + strlen(buf), (size) - strlen(buf), fmt)

int tvh_open(const char *pathname, int flags, mode_t mode);

int tvh_socket(int domain, int type, int protocol);

void hexdump(const char *pfx, const uint8_t *data, int len);

uint32_t crc32(uint8_t *data, size_t datalen, uint32_t crc);

int base64_decode(uint8_t *out, const char *in, int out_size);

int put_utf8(char *out, int c);

static inline int64_t ts_rescale(int64_t ts, int tb)
{
  //  return (ts * tb + (tb / 2)) / 90000LL;
  return (ts * tb ) / 90000LL;
}

void sbuf_init(sbuf_t *sb);

void sbuf_free(sbuf_t *sb);

void sbuf_reset(sbuf_t *sb);

void sbuf_err(sbuf_t *sb);

void sbuf_alloc(sbuf_t *sb, int len);

void sbuf_append(sbuf_t *sb, const void *data, int len);

void sbuf_cut(sbuf_t *sb, int off);

void sbuf_put_be32(sbuf_t *sb, uint32_t u32);

void sbuf_put_be16(sbuf_t *sb, uint16_t u16);

void sbuf_put_byte(sbuf_t *sb, uint8_t u8);

#endif /* TV_HEAD_H */
