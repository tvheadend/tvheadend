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
#if ENABLE_LOCKOWNER || ENABLE_ANDROID
#include <sys/syscall.h>
#endif
#include "queue.h"
#include "hts_strtab.h"
#include "htsmsg.h"
#include "tvhlog.h"

#include "redblack.h"

#include "tvh_locale.h"

#define STRINGIFY(s) # s
#define SRCLINEID() SRCLINEID2(__FILE__, __LINE__)
#define SRCLINEID2(f,l) f ":" STRINGIFY(l)

#define ERRNO_AGAIN(e) ((e) == EAGAIN || (e) == EINTR || (e) == EWOULDBLOCK)

#include "compat.h"

typedef struct {
  const char     *name;
  const uint32_t *enabled;
} tvh_caps_t;

extern int              tvheadend_running;
extern const char      *tvheadend_version;
extern const char      *tvheadend_cwd;
extern const char      *tvheadend_webroot;
extern const tvh_caps_t tvheadend_capabilities[];

static inline int tvheadend_is_running(void)
{
  return atomic_get(&tvheadend_running);
}

htsmsg_t *tvheadend_capabilities_list(int check);

typedef struct str_list
{
  int max;
  int num;
  char **str;
} str_list_t;

#define PTS_UNSET INT64_C(0x8000000000000000)
#define PTS_MASK  INT64_C(0x00000001ffffffff)

extern pthread_mutex_t global_lock;
extern pthread_mutex_t tasklet_lock;
extern pthread_mutex_t fork_lock;

extern int tvheadend_webui_port;
extern int tvheadend_webui_debug;
extern int tvheadend_htsp_port;

static inline void
lock_assert0(pthread_mutex_t *l, const char *file, int line)
{
#if 0 && ENABLE_LOCKOWNER
  assert(l->__data.__owner == syscall(SYS_gettid));
#else
  if(pthread_mutex_trylock(l) == EBUSY)
    return;

  fprintf(stderr, "Mutex not held at %s:%d\n", file, line);
  abort();
#endif
}

#define lock_assert(l) lock_assert0(l, __FILE__, __LINE__)

#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define CLANG_SANITIZER 1
#endif
#endif

/*
 *
 */

typedef struct {
  pthread_cond_t cond;
} tvh_cond_t;


/*
 * Commercial status
 */
typedef enum {
  COMMERCIAL_UNKNOWN,
  COMMERCIAL_YES,
  COMMERCIAL_NO,
} th_commercial_advice_t;


/*
 *
 */
#define UILEVEL_DEFAULT  (-1)
#define UILEVEL_BASIC    0
#define UILEVEL_ADVANCED 1
#define UILEVEL_EXPERT   2

/*
 *
 */
#define CHICON_NONE      0
#define CHICON_LOWERCASE 1
#define CHICON_SVCNAME   2

/*
 *
 */
#define PICON_STANDARD   0
#define PICON_ISVCTYPE   1

/*
 * timer support functions
 */

#if ENABLE_GTIMER_CHECK
#define GTIMER_TRACEID_ const char *id, const char *fcn,
#define GTIMER_FCN(n) check_##n
#else
#define GTIMER_TRACEID_
#define GTIMER_FCN(n) n
#endif

/*
 * global timer - monotonic
 */

typedef void (mti_callback_t)(void *opaque);

typedef struct mtimer {
  LIST_ENTRY(mtimer) mti_link;
  mti_callback_t *mti_callback;
  void *mti_opaque;
  int64_t mti_expire;
#if ENABLE_GTIMER_CHECK
  const char *mti_id;
  const char *mti_fcn;
#endif
} mtimer_t;

void GTIMER_FCN(mtimer_arm_rel)
  (GTIMER_TRACEID_ mtimer_t *mti, mti_callback_t *callback, void *opaque, int64_t delta);
void GTIMER_FCN(mtimer_arm_abs)
  (GTIMER_TRACEID_ mtimer_t *mti, mti_callback_t *callback, void *opaque, int64_t when);

#if ENABLE_GTIMER_CHECK
#define mtimer_arm_rel(a, b, c, d) GTIMER_FCN(mtimer_arm_rel)(SRCLINEID(), __func__, a, b, c, d)
#define mtimer_arm_abs(a, b, c, d) GTIMER_FCN(mtimer_arm_abs)(SRCLINEID(), __func__, a, b, c, d)
#endif

void mtimer_disarm(mtimer_t *mti);



/*
 * global timer (based on the current system time - time())
 */

typedef void (gti_callback_t)(void *opaque);

typedef struct gtimer {
  LIST_ENTRY(gtimer) gti_link;
  gti_callback_t *gti_callback;
  void *gti_opaque;
  time_t gti_expire;
#if ENABLE_GTIMER_CHECK
  const char *gti_id;
  const char *gti_fcn;
#endif
} gtimer_t;

void GTIMER_FCN(gtimer_arm_rel)
  (GTIMER_TRACEID_ gtimer_t *gti, gti_callback_t *callback, void *opaque, time_t delta);
void GTIMER_FCN(gtimer_arm_absn)
  (GTIMER_TRACEID_ gtimer_t *gti, gti_callback_t *callback, void *opaque, time_t when);

#if ENABLE_GTIMER_CHECK
#define gtimer_arm_rel(a, b, c, d) GTIMER_FCN(gtimer_arm_rel)(SRCLINEID(), __func__, a, b, c, d)
#define gtimer_arm_absn(a, b, c, d) GTIMER_FCN(gtimer_arm_absn)(SRCLINEID(), __func__, a, b, c, d)
#endif

void gtimer_disarm(gtimer_t *gti);


/*
 * tasklet
 */

typedef void (tsk_callback_t)(void *opaque, int disarmed);

typedef struct tasklet {
  TAILQ_ENTRY(tasklet) tsk_link;
  tsk_callback_t *tsk_callback;
  void *tsk_opaque;
  int tsk_allocated;
} tasklet_t;

tasklet_t *tasklet_arm_alloc(tsk_callback_t *callback, void *opaque);
void tasklet_arm(tasklet_t *tsk, tsk_callback_t *callback, void *opaque);
void tasklet_disarm(tasklet_t *gti);


/*
 * List / Queue header declarations
 */
LIST_HEAD(memoryinfo_list, memoryinfo);
LIST_HEAD(access_entry_list, access_entry);
LIST_HEAD(th_subscription_list, th_subscription);
LIST_HEAD(dvr_vfs_list, dvr_vfs);
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
LIST_HEAD(dvr_timerec_entry_list, dvr_timerec_entry);
TAILQ_HEAD(th_pktref_queue, th_pktref);
LIST_HEAD(streaming_target_list, streaming_target);

/**
 * Stream component types
 */
typedef enum {
  SCT_NONE = -1,
  SCT_UNKNOWN = 0,
  SCT_RAW = 1,
  SCT_MPEG2VIDEO,
  SCT_MPEG2AUDIO,
  SCT_H264,
  SCT_AC3,
  SCT_TELETEXT,
  SCT_DVBSUB,
  SCT_CA,
  SCT_AAC,     /* AAC-LATM in MPEG-TS, ADTS + AAC in packet form */
  SCT_MPEGTS,
  SCT_TEXTSUB,
  SCT_EAC3,
  SCT_MP4A,    /* ADTS + AAC in MPEG-TS and packet form */
  SCT_VP8,
  SCT_VORBIS,
  SCT_HEVC,
  SCT_VP9,
  SCT_LAST = SCT_VP9
} streaming_component_type_t;

#define SCT_MASK(t) (1 << (t))

#define SCT_ISVIDEO(t) ((t) == SCT_MPEG2VIDEO || (t) == SCT_H264 ||	\
			(t) == SCT_VP8 || (t) == SCT_HEVC || (t) == SCT_VP9)

#define SCT_ISAUDIO(t) ((t) == SCT_MPEG2AUDIO || (t) == SCT_AC3 || \
                        (t) == SCT_AAC  || (t) == SCT_MP4A ||	   \
			(t) == SCT_EAC3 || (t) == SCT_VORBIS)

#define SCT_ISAV(t) (SCT_ISVIDEO(t) || SCT_ISAUDIO(t))

#define SCT_ISSUBTITLE(t) ((t) == SCT_TEXTSUB || (t) == SCT_DVBSUB)

/*
 * Scales for signal status values
 */
typedef enum {
  SIGNAL_STATUS_SCALE_UNKNOWN = 0,
  SIGNAL_STATUS_SCALE_RELATIVE, // value is unsigned, where 0 means 0% and 65535 means 100%
  SIGNAL_STATUS_SCALE_DECIBEL   // value is measured in dB * 1000
} signal_status_scale_t;

/**
 * The signal status of a tuner
 */
typedef struct signal_status {
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
} signal_status_t;

/**
 * Descramble info
 */
typedef struct descramble_info {
  uint16_t pid;
  uint16_t caid;
  uint32_t provid;
  uint32_t ecmtime;
  uint16_t hops;
  char cardsystem[128];
  char reader    [128];
  char from      [128];
  char protocol  [128];
} descramble_info_t;

/**
 *
 */
typedef struct timeshift_status
{
  int     full;
  int64_t shift;
  int64_t pts_start;
  int64_t pts_end;
} timeshift_status_t;

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
#if ENABLE_TIMESHIFT
  timeshift_status_t timeshift;
#endif
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

} streaming_message_type_t;

#define SMT_TO_MASK(x) (1 << ((unsigned int)x))


#define SM_CODE_OK                        0

#define SM_CODE_UNDEFINED_ERROR           1

#define SM_CODE_SOURCE_RECONFIGURED       100
#define SM_CODE_BAD_SOURCE                101
#define SM_CODE_SOURCE_DELETED            102
#define SM_CODE_SUBSCRIPTION_OVERRIDDEN   103
#define SM_CODE_INVALID_TARGET            104
#define SM_CODE_USER_ACCESS               105
#define SM_CODE_USER_LIMIT                106
#define SM_CODE_WEAK_STREAM               107
#define SM_CODE_USER_REQUEST              108

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
  { "FAINT",      SIGNAL_FAINT   },
  { "NONE",       SIGNAL_NONE    },
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
  int64_t sm_time;
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
  tvh_cond_t  sq_cond;         /* Condvar for signalling new packets */

  size_t          sq_maxsize;  /* Max queue size (bytes) */
  size_t          sq_size;     /* Actual queue size (bytes) - only data */
  
  struct streaming_message_queue sq_queue;

} streaming_queue_t;


/**
 * Simple dynamically growing buffer
 */
typedef struct sbuf {
  uint8_t *sb_data;
  int      sb_ptr;
  int      sb_size;
  uint16_t sb_err;
  uint8_t  sb_bswap;
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
#define MINMAX(a,mi,ma) MAX(mi, MIN(ma, a))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

void tvh_str_set(char **strp, const char *src);
int tvh_str_update(char **strp, const char *src);

int sri_to_rate(int sri);
int rate_to_sri(int rate);

extern struct service_list all_transports;

extern void scopedunlock(pthread_mutex_t **mtxp);

#define scopedlock(mtx) \
 pthread_mutex_t *scopedlock ## __LINE__ \
 __attribute__((cleanup(scopedunlock))) = mtx; \
 pthread_mutex_lock(scopedlock ## __LINE__);

#define scopedgloballock() scopedlock(&global_lock)

#define tvh_strdupa(n) \
  ({ int tvh_l = strlen(n); \
     char *tvh_b = alloca(tvh_l + 1); \
     memcpy(tvh_b, n, tvh_l + 1); })

#define tvh_strlen(s) ((s) ? strlen(s) : 0)

#define tvh_strlcatf(buf, size, ptr, fmt...) \
  do { int __r = snprintf((buf) + ptr, (size) - ptr, fmt); \
       ptr = __r >= (size) - ptr ? (size) - 1 : ptr + __r; } while (0)

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

int tvhthread_create
  (pthread_t *thread, const pthread_attr_t *attr,
   void *(*start_routine) (void *), void *arg,
   const char *name);

int tvhtread_renice(int value);

int tvh_mutex_timedlock(pthread_mutex_t *mutex, int64_t usec);

int tvh_cond_init(tvh_cond_t *cond);

int tvh_cond_destroy(tvh_cond_t *cond);

int tvh_cond_signal(tvh_cond_t *cond, int broadcast);

int tvh_cond_wait(tvh_cond_t *cond, pthread_mutex_t *mutex);

int tvh_cond_timedwait(tvh_cond_t *cond, pthread_mutex_t *mutex, int64_t monoclock);

int tvh_open(const char *pathname, int flags, mode_t mode);

int tvh_socket(int domain, int type, int protocol);

int tvh_pipe(int flags, th_pipe_t *pipe);

void tvh_pipe_close(th_pipe_t *pipe);

int tvh_write(int fd, const void *buf, size_t len);

FILE *tvh_fopen(const char *filename, const char *mode);

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

static inline int64_t ts_rescale_inv(int64_t ts, int tb)
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
  sb->sb_ptr = sb->sb_size = sb->sb_err = 0;
}

static inline void sbuf_err(sbuf_t *sb, int errors)
{
  sb->sb_err += errors;
}

void sbuf_alloc_(sbuf_t *sb, int len);

static inline void sbuf_alloc(sbuf_t *sb, int len)
{
  if (sb->sb_ptr + len >= sb->sb_size)
    sbuf_alloc_(sb, len);
}

void sbuf_realloc(sbuf_t *sb, int len);

void sbuf_append(sbuf_t *sb, const void *data, int len);

void sbuf_cut(sbuf_t *sb, int off);

void sbuf_put_be32(sbuf_t *sb, uint32_t u32);

void sbuf_put_be16(sbuf_t *sb, uint16_t u16);

void sbuf_put_byte(sbuf_t *sb, uint8_t u8);

ssize_t sbuf_read(sbuf_t *sb, int fd);

static inline uint8_t sbuf_peek_u8(sbuf_t *sb, int off) { return sb->sb_data[off]; }
static inline  int8_t sbuf_peek_s8(sbuf_t *sb, int off) { return sb->sb_data[off]; }
uint16_t sbuf_peek_u16(sbuf_t *sb, int off);
static inline int16_t sbuf_peek_s16(sbuf_t *sb, int off) { return sbuf_peek_u16(sb, off); }
uint16_t sbuf_peek_u16le(sbuf_t *sb, int off);
static inline int16_t sbuf_peek_s16le(sbuf_t *sb, int off) { return sbuf_peek_u16le(sb, off); }
uint16_t sbuf_peek_u16be(sbuf_t *sb, int off);
static inline int16_t sbuf_peek_s16be(sbuf_t *sb, int off) { return sbuf_peek_u16be(sb, off); }
uint32_t sbuf_peek_u32(sbuf_t *sb, int off);
static inline int32_t sbuf_peek_s32(sbuf_t *sb, int off) { return sbuf_peek_u32(sb, off); }
uint32_t sbuf_peek_u32le(sbuf_t *sb, int off);
static inline int32_t sbuf_peek_s32le(sbuf_t *sb, int off) { return sbuf_peek_u32le(sb, off); }
uint32_t sbuf_peek_u32be(sbuf_t *sb, int off);
static inline  int32_t sbuf_peek_s32be(sbuf_t *sb, int off) { return sbuf_peek_u32be(sb, off); }
static inline uint8_t *sbuf_peek(sbuf_t *sb, int off) { return sb->sb_data + off; }

char *md5sum ( const char *str );

int makedirs ( const char *subsys, const char *path, int mode, int mstrict, gid_t gid, uid_t uid );

int rmtree ( const char *path );

char *regexp_escape ( const char *str );

#if ENABLE_ZLIB
uint8_t *tvh_gzip_inflate ( const uint8_t *data, size_t size, size_t orig );
uint8_t *tvh_gzip_deflate ( const uint8_t *data, size_t orig, size_t *size );
int      tvh_gzip_deflate_fd ( int fd, const uint8_t *data, size_t orig, size_t *size, int speed );
int      tvh_gzip_deflate_fd_header ( int fd, const uint8_t *data, size_t orig, int speed );
#endif

/* URL decoding */
char to_hex(char code);
char *url_encode(const char *str);

int mpegts_word_count(const uint8_t *tsb, int len, uint32_t mask);

int deferred_unlink(const char *filename, const char *rootdir);

void sha1_calc(uint8_t *dst, const uint8_t *d1, size_t d1_len, const uint8_t *d2, size_t d2_len);

uint32_t gcdU32(uint32_t a, uint32_t b);
static inline int32_t deltaI32(int32_t a, int32_t b) { return (a > b) ? (a - b) : (b - a); }
static inline uint32_t deltaU32(uint32_t a, uint32_t b) { return (a > b) ? (a - b) : (b - a); }
  
#define SKEL_DECLARE(name, type) type *name;
#define SKEL_ALLOC(name) do { if (!name) name = calloc(1, sizeof(*name)); } while (0)
#define SKEL_USED(name) do { name = NULL; } while (0)
#define SKEL_FREE(name) do { free(name); name = NULL; } while (0)

htsmsg_t *network_interfaces_enum(void *obj, const char *lang);

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

#if __WORDSIZE == 32 && defined(PLATFORM_FREEBSD)
#define PRItime_t       "d"
#else
#define PRItime_t       "ld"
#endif

#endif /* TVHEADEND_H */
