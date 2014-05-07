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

#include "build.h"

#define _GNU_SOURCE
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <libgen.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>
#if ENABLE_LOCKOWNER
#include <sys/syscall.h>
#endif

#include "queue.h"
#include "avg.h"
#include "hts_strtab.h"
#include "htsmsg.h"
#include "tvhlog.h"

#include "redblack.h"

typedef struct {
  const char     *name;
  const uint32_t *enabled;
} tvh_caps_t;
extern int              tvheadend_running;
extern const char      *tvheadend_version;
extern const char      *tvheadend_cwd;
extern const char      *tvheadend_webroot;
extern const tvh_caps_t tvheadend_capabilities[];

static inline htsmsg_t *tvheadend_capabilities_list(int check)
{
  int i = 0;
  htsmsg_t *r = htsmsg_create_list();
  while (tvheadend_capabilities[i].name) {
    if (!check ||
        !tvheadend_capabilities[i].enabled ||
        *tvheadend_capabilities[i].enabled)
      htsmsg_add_str(r, NULL, tvheadend_capabilities[i].name);
    i++;
  }
  return r;
}

typedef struct str_list
{
  int max;
  int num;
  char **str;
} str_list_t;

#define PTS_UNSET INT64_C(0x8000000000000000)

extern int tvheadend_running;

extern pthread_mutex_t global_lock;
extern pthread_mutex_t ffmpeg_lock;
extern pthread_mutex_t fork_lock;
extern pthread_mutex_t atomic_lock;

extern int tvheadend_webui_port;
extern int tvheadend_webui_debug;
extern int tvheadend_htsp_port;

typedef struct source_info {
  char *si_device;
  char *si_adapter;
  char *si_network;
  char *si_mux;
  char *si_provider;
  char *si_service;
  int   si_type;
} source_info_t;


static inline void
lock_assert0(pthread_mutex_t *l, const char *file, int line)
{
#if ENABLE_LOCKOWNER
  assert(l->__data.__owner == syscall(SYS_gettid));
#else
  if(pthread_mutex_trylock(l) == EBUSY)
    return;

  fprintf(stderr, "Mutex not held at %s:%d\n", file, line);
  abort();
#endif
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
  struct timespec gti_expire;
} gtimer_t;

void gtimer_arm(gtimer_t *gti, gti_callback_t *callback, void *opaque,
		int delta);

void gtimer_arm_ms(gtimer_t *gti, gti_callback_t *callback, void *opaque,
  long delta_ms);

void gtimer_arm_abs(gtimer_t *gti, gti_callback_t *callback, void *opaque,
		    time_t when);

void gtimer_arm_abs2(gtimer_t *gti, gti_callback_t *callback, void *opaque,
  struct timespec *when);

void gtimer_disarm(gtimer_t *gti);


/*
 * List / Queue header declarations
 */
LIST_HEAD(th_subscription_list, th_subscription);
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
  SCT_NONE = -1,
  SCT_UNKNOWN = 0,
  SCT_MPEG2VIDEO = 1,
  SCT_MPEG2AUDIO,
  SCT_H264,
  SCT_AC3,
  SCT_TELETEXT,
  SCT_DVBSUB,
  SCT_CA,
  SCT_AAC,
  SCT_MPEGTS,
  SCT_TEXTSUB,
  SCT_EAC3,
  SCT_MP4A,
  SCT_VP8,
  SCT_VORBIS,
  SCT_LAST = SCT_VORBIS
} streaming_component_type_t;

#define SCT_MASK(t) (1 << (t))

#define SCT_ISVIDEO(t) ((t) == SCT_MPEG2VIDEO || (t) == SCT_H264 ||	\
			(t) == SCT_VP8)

#define SCT_ISAUDIO(t) ((t) == SCT_MPEG2AUDIO || (t) == SCT_AC3 || \
                        (t) == SCT_AAC  || (t) == SCT_MP4A ||	   \
			(t) == SCT_EAC3 || (t) == SCT_VORBIS)

#define SCT_ISSUBTITLE(t) ((t) == SCT_TEXTSUB || (t) == SCT_DVBSUB)

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
 * Streaming skip
 */
typedef struct streaming_skip
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
} streaming_skip_t;


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
  int sp_reject_filter;
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
   * Signal status
   *
   * Notification about frontend signal status
   */
  SMT_SIGNAL_STATUS,

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

} streaming_message_type_t;

#define SMT_TO_MASK(x) (1 << ((unsigned int)x))


#define SM_CODE_OK                        0

#define SM_CODE_UNDEFINED_ERROR           1

#define SM_CODE_SOURCE_RECONFIGURED       100
#define SM_CODE_BAD_SOURCE                101
#define SM_CODE_SOURCE_DELETED            102
#define SM_CODE_SUBSCRIPTION_OVERRIDDEN   103

#define SM_CODE_NO_FREE_ADAPTER           200
#define SM_CODE_MUX_NOT_ENABLED           201
#define SM_CODE_NOT_FREE                  202
#define SM_CODE_TUNING_FAILED             203
#define SM_CODE_SVC_NOT_ENABLED           204
#define SM_CODE_BAD_SIGNAL                205
#define SM_CODE_NO_SOURCE                 206
#define SM_CODE_NO_SERVICE                207
#define SM_CODE_NO_VALID_ADAPTER          308

#define SM_CODE_ABORTED                   300

#define SM_CODE_NO_DESCRAMBLER            400
#define SM_CODE_NO_ACCESS                 401
#define SM_CODE_NO_INPUT                  402

typedef enum
{
  SIGNAL_UNKNOWN,
  SIGNAL_GOOD,
  SIGNAL_BAD,
  SIGNAL_FAINT,
  SIGNAL_NONE
} signal_state_t;

static struct strtab signal_statetab[] = {
  { "GOOD",       SIGNAL_GOOD    },
  { "BAD",        SIGNAL_BAD     },
  { "FAINT",      SIGNAL_BAD     },
  { "NONE",       SIGNAL_BAD     },
};

static inline const char * signal2str ( signal_state_t st )
{
  const char *r = val2str(st, signal_statetab);
  if (!r) r = "UNKNOWN";
  return r;
}

/**
 * Streaming messages are sent from the pad to its receivers
 */
typedef struct streaming_message {
  TAILQ_ENTRY(streaming_message) sm_link;
  streaming_message_type_t sm_type;
#if ENABLE_TIMESHIFT
  int64_t  sm_time;
#endif
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

  pthread_mutex_t sq_mutex;    /* Protects sp_queue */
  pthread_cond_t  sq_cond;     /* Condvar for signalling new packets */

  size_t          sq_maxsize;  /* Max queue size (bytes) */
  
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


streaming_component_type_t streaming_component_txt2type(const char *str);
const char *streaming_component_type2txt(streaming_component_type_t s);
streaming_component_type_t streaming_component_txt2type(const char *s);
const char *streaming_component_audio_type2desc(int audio_type);

static inline unsigned int tvh_strhash(const char *s, unsigned int mod)
{
  unsigned int v = 5381;
  while(*s)
    v += (v << 5) + v + *s++;
  return v % mod;
}

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

void tvh_str_set(char **strp, const char *src);
int tvh_str_update(char **strp, const char *src);

#ifndef CLOCK_MONOTONIC_COARSE
#define CLOCK_MONOTONIC_COARSE CLOCK_MONOTONIC
#endif

static inline int64_t 
getmonoclock(void)
{
  struct timespec tp;

  clock_gettime(CLOCK_MONOTONIC_COARSE, &tp);

  return tp.tv_sec * 1000000ULL + (tp.tv_nsec / 1000);
}

int sri_to_rate(int sri);
int rate_to_sri(int rate);


extern time_t dispatch_clock;
extern struct service_list all_transports;

extern void scopedunlock(pthread_mutex_t **mtxp);

#define scopedlock(mtx) \
 pthread_mutex_t *scopedlock ## __LINE__ \
 __attribute__((cleanup(scopedunlock))) = mtx; \
 pthread_mutex_lock(scopedlock ## __LINE__);

#define scopedgloballock() scopedlock(&global_lock)

#define tvh_strdupa(n) ({ int tvh_l = strlen(n); \
 char *tvh_b = alloca(tvh_l + 1); \
 memcpy(tvh_b, n, tvh_l + 1); })

#define tvh_strlcatf(buf, size, fmt...) \
 snprintf((buf) + strlen(buf), (size) - strlen(buf), fmt)

static inline const char *tvh_strbegins(const char *s1, const char *s2)
{
  while(*s2)
    if(*s1++ != *s2++)
      return NULL;
  return s1;
}

typedef struct th_pipe
{
  int rd;
  int wr;
} th_pipe_t;

static inline void mystrset(char **p, const char *s)
{
  free(*p);
  *p = s ? strdup(s) : NULL;
}

void doexit(int x);

int tvhthread_create0
  (pthread_t *thread, const pthread_attr_t *attr,
   void *(*start_routine) (void *), void *arg,
   const char *name, int detach);

#define tvhthread_create(a, b, c, d, e)  tvhthread_create0(a, b, c, d, #c, e)

int tvh_open(const char *pathname, int flags, mode_t mode);

int tvh_socket(int domain, int type, int protocol);

int tvh_pipe(int flags, th_pipe_t *pipe);

void tvh_pipe_close(th_pipe_t *pipe);

int tvh_write(int fd, const void *buf, size_t len);

void hexdump(const char *pfx, const uint8_t *data, int len);

uint32_t tvh_crc32(const uint8_t *data, size_t datalen, uint32_t crc);

int base64_decode(uint8_t *out, const char *in, int out_size);

char *base64_encode(char *out, int out_size, const uint8_t *in, int in_size);

/* Calculate the output size needed to base64-encode x bytes. */
#define BASE64_SIZE(x) (((x)+2) / 3 * 4 + 1)

int put_utf8(char *out, int c);

static inline int64_t ts_rescale(int64_t ts, int tb)
{
  //  return (ts * tb + (tb / 2)) / 90000LL;
  return (ts * tb ) / 90000LL;
}

static inline int64_t ts_rescale_i(int64_t ts, int tb)
{
  return (ts * 90000LL) / tb;
}

void sbuf_init(sbuf_t *sb);

void sbuf_init_fixed(sbuf_t *sb, int len);

void sbuf_free(sbuf_t *sb);

void sbuf_reset(sbuf_t *sb, int max_len);

void sbuf_reset_and_alloc(sbuf_t *sb, int len);

static inline void sbuf_steal_data(sbuf_t *sb)
{
  sb->sb_data = NULL;
  sb->sb_ptr = sb->sb_size = 0;
}

static inline void sbuf_err(sbuf_t *sb)
{
  sb->sb_err = 1;
}

void sbuf_alloc_(sbuf_t *sb, int len);

static inline void sbuf_alloc(sbuf_t *sb, int len)
{
  if (sb->sb_ptr + len >= sb->sb_size)
    sbuf_alloc_(sb, len);
}

void sbuf_append(sbuf_t *sb, const void *data, int len);

void sbuf_cut(sbuf_t *sb, int off);

void sbuf_put_be32(sbuf_t *sb, uint32_t u32);

void sbuf_put_be16(sbuf_t *sb, uint16_t u16);

void sbuf_put_byte(sbuf_t *sb, uint8_t u8);

ssize_t sbuf_read(sbuf_t *sb, int fd);

char *md5sum ( const char *str );

int makedirs ( const char *path, int mode );

int rmtree ( const char *path );

char *regexp_escape ( const char *str );

#define SKEL_DECLARE(name, type) type *name;
#define SKEL_ALLOC(name) do { if (!name) name = calloc(1, sizeof(*name)); } while (0)
#define SKEL_USED(name) do { name = NULL; } while (0)
#define SKEL_FREE(name) do { free(name); name = NULL; } while (0)

/* glibc wrapper */
#if ! ENABLE_QSORT_R
void
qsort_r(void *base, size_t nmemb, size_t size,
       int (*cmp)(const void *, const void *, void *), void *aux);
#endif /* ENABLE_QSORT_R */

void tvh_qsort_r(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *arg);

/* printing */
#ifndef __WORDSIZE
# if ULONG_MAX == 0xffffffffffffffff
#  define __WORDSIZE 64
# elif ULONG_MAX == 0xffffffff
#  define __WORDSIZE 32
# endif /* ULONG_MAX */
#endif /* __WORDSIZE */

# if __WORDSIZE == 64
#define PRIsword_t      PRId64
#define PRIuword_t      PRIu64
#else
#define PRIsword_t      PRId32
#define PRIuword_t      PRIu32
#endif
#define PRIslongword_t  "ld"
#define PRIulongword_t  "lu"
#define PRIsize_t       PRIuword_t
#define PRIssize_t      PRIsword_t
#if __WORDSIZE == 32 && defined(PLATFORM_FREEBSD)
#define PRItime_t       PRIsword_t
#else
#define PRItime_t       PRIslongword_t
#endif
#if _FILE_OFFSET_BITS == 64
#define PRIoff_t        PRId64
#else
#define PRIoff_t        PRIslongword_t
#endif

#endif /* TV_HEAD_H */
