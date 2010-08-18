/*
 *  TV headend - structures
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

#ifndef TV_HEAD_H
#define TV_HEAD_H

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
LIST_HEAD(dvr_entry_list, dvr_entry);
TAILQ_HEAD(ref_update_queue, ref_update);
LIST_HEAD(th_transport_list, th_transport);
RB_HEAD(th_transport_tree, th_transport);
TAILQ_HEAD(th_transport_queue, th_transport);
LIST_HEAD(th_stream_list, th_stream);
TAILQ_HEAD(th_stream_queue, th_stream);
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
} streaming_component_type_t;

#define SCT_ISVIDEO(t) ((t) == SCT_MPEG2VIDEO || (t) == SCT_H264)
#define SCT_ISAUDIO(t) ((t) == SCT_MPEG2AUDIO || (t) == SCT_AC3 || \
                        (t) == SCT_AAC)
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
   * Transport status
   *
   * Notification about status of source, see TSS_ flags
   */
  SMT_TRANSPORT_STATUS,

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
#define SM_CODE_NO_TRANSPORT              207

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



/**
 * Descrambler superclass
 *
 * Created/Destroyed on per-transport basis upon transport start/stop
 */
typedef struct th_descrambler {
  LIST_ENTRY(th_descrambler) td_transport_link;

  void (*td_table)(struct th_descrambler *d, struct th_transport *t,
		   struct th_stream *st, 
		   const uint8_t *section, int section_len);

  int (*td_descramble)(struct th_descrambler *d, struct th_transport *t,
		       struct th_stream *st, const uint8_t *tsb);

  void (*td_stop)(struct th_descrambler *d);

} th_descrambler_t;



/*
 * Section callback, called when a PSI table is fully received
 */
typedef void (pid_section_callback_t)(struct th_transport *t,
				      struct th_stream *pi,
				      const uint8_t *section, int section_len);

LIST_HEAD(caid_list, caid);
/**
 *
 */
typedef struct caid {
  LIST_ENTRY(caid) link;

  uint8_t delete_me;
  uint16_t caid;
  uint32_t providerid;

} caid_t;

/*
 * Stream, one media component for a transport.
 *
 * XXX: This should be renamed to 'elementary_stream' or something
 */
typedef struct th_stream {

  TAILQ_ENTRY(th_stream) st_link;
  int st_position;
  struct th_transport *st_transport;

  streaming_component_type_t st_type;
  int st_index;

  char st_lang[4];           /* ISO 639 3-letter language code */
  uint16_t st_composition_id;
  uint16_t st_ancillary_id;

  int16_t st_pid;
  uint16_t st_parent_pid;    /* For subtitle streams originating from 
				a teletext stream. this is the pid
				of the teletext stream */

  uint8_t st_cc;             /* Last CC */
  uint8_t st_cc_valid;       /* Is CC valid at all? */

  avgstat_t st_cc_errors;
  avgstat_t st_rate;

  int st_demuxer_fd;
  int st_peak_presentation_delay; /* Max seen diff. of DTS and PTS */

  struct psi_section *st_section;
  int st_section_docrc;           /* Set if we should verify CRC on tables */
  pid_section_callback_t *st_got_section;
  void *st_got_section_opaque;

  /* PCR recovery */

  int st_pcr_recovery_fails;
  int64_t st_pcr_real_last;     /* realtime clock when we saw last PCR */
  int64_t st_pcr_last;          /* PCR clock when we saw last PCR */
  int64_t st_pcr_drift;

  /* For transport stream packet reassembly */

  sbuf_t st_buf;

  uint32_t st_startcond;
  uint32_t st_startcode;
  uint32_t st_startcode_offset;
  int st_parser_state;
  int st_parser_ptr;
  void *st_priv;          /* Parser private data */

  sbuf_t st_buf_ps;       // program stream reassembly (analogue adapters)
  sbuf_t st_buf_a;        // Audio packet reassembly

  uint8_t *st_global_data;
  int st_global_data_len;

  int st_ssc_intercept;
  int st_ssc_ptr;
  uint8_t st_ssc_buf[32];

  struct th_pkt *st_curpkt;
  int64_t st_curpts;
  int64_t st_curdts;
  int64_t st_prevdts;
  int64_t st_nextdts;
  int st_frame_duration;
  int st_width;
  int st_height;

  int st_meta_change;

  /* CA ID's on this stream */
  struct caid_list st_caids;

  int st_vbv_size;        /* Video buffer size (in bytes) */
  int st_vbv_delay;       /* -1 if CBR */

  /* */

  int st_delete_me;      /* Temporary flag for deleting streams */

  /* Error log limiters */

  loglimiter_t st_loglimit_cc;
  loglimiter_t st_loglimit_pes;
  
  char *st_nicename;

} th_stream_t;


/**
 * A Transport (or in MPEG TS terms: a 'service')
 */
typedef struct th_transport {

  LIST_ENTRY(th_transport) tht_hash_link;

  enum {
    TRANSPORT_DVB,
    TRANSPORT_IPTV,
    TRANSPORT_V4L,
  } tht_type;

  enum {
    /**
     * Transport is idle.
     */
    TRANSPORT_IDLE,

    /**
     * Transport producing output
     */
    TRANSPORT_RUNNING,

    /**
     * Destroyed, but pointer is till valid. 
     * This would be the case if transport_destroy() did not actually free 
     * the transport because there are references held to it.
     *
     * Reference counts can be used so that code can hold a pointer to 
     * a transport without having the global lock.
     *
     * Note: No fields in the transport may be accessed without the
     * global lock held. Thus, the global_lock must be reaquired and
     * then tht_status must be checked. If it is ZOMBIE the code must
     * just drop the refcount and pretend that the transport never
     * was there in the first place.
     */
    TRANSPORT_ZOMBIE, 
  } tht_status;

  /**
   * Refcount, operated using atomic.h ops.
   */ 
  int tht_refcount;

  /**
   *
   */
  int tht_flags;

#define THT_DEBUG 0x1

  /**
   * Source type is used to determine if an output requesting
   * MPEG-TS can shortcut all the parsing and remuxing.
   */ 
  enum {
    THT_MPEG_TS,
    THT_OTHER,
  } tht_source_type;

 
  /**
   * PID carrying the programs PCR.
   * XXX: We don't support transports that does not carry
   * the PCR in one of the content streams.
   */
  uint16_t tht_pcr_pid;

  /**
   * PID for the PMT of this MPEG-TS stream.
   */
  uint16_t tht_pmt_pid;

  /**
   * Set if transport is enabled (the default).  If disabled it should
   * not be considered when chasing for available transports during
   * subscription scheduling.
   */
  int tht_enabled;

  /**
   * Last PCR seen, we use it for a simple clock for rawtsinput.c
   */
  int64_t tht_pcr_last;
  int64_t tht_pcr_last_realtime;
  
  LIST_ENTRY(th_transport) tht_group_link;

  LIST_ENTRY(th_transport) tht_active_link;

  LIST_HEAD(, th_subscription) tht_subscriptions;

  int (*tht_start_feed)(struct th_transport *t, unsigned int weight,
			int force_start);

  void (*tht_refresh_feed)(struct th_transport *t);

  void (*tht_stop_feed)(struct th_transport *t);

  void (*tht_config_save)(struct th_transport *t);

  void (*tht_setsourceinfo)(struct th_transport *t, struct source_info *si);

  int (*tht_quality_index)(struct th_transport *t);

  int (*tht_grace_period)(struct th_transport *t);

  void (*tht_dtor)(struct th_transport *t);

  /*
   * Per source type structs
   */
  struct th_dvb_mux_instance *tht_dvb_mux_instance;

  /**
   * Unique identifer (used for storing on disk, etc)
   */
  char *tht_identifier;

  /**
   * Name usable for displaying to user
   */
  char *tht_nicename;

  /**
   * Service ID according to EN 300 468
   */
  uint16_t tht_dvb_service_id;

  uint16_t tht_channel_number;

  /**
   * Service name (eg. DVB service name as specified by EN 300 468)
   */
  char *tht_svcname;

  /**
   * Provider name (eg. DVB provider name as specified by EN 300 468)
   */
  char *tht_provider;

  enum {
    /* Service types defined in EN 300 468 */

    ST_SDTV       = 0x1,    /* SDTV (MPEG2) */
    ST_RADIO      = 0x2,
    ST_HDTV       = 0x11,   /* HDTV (MPEG2) */
    ST_AC_SDTV    = 0x16,   /* Advanced codec SDTV */
    ST_AC_HDTV    = 0x19,   /* Advanced codec HDTV */
  } tht_servicetype;


  /**
   * Teletext...
   */
  th_commercial_advice_t tht_tt_commercial_advice;
  int tht_tt_rundown_content_length;
  time_t tht_tt_clock;   /* Network clock as determined by teletext decoder */
 
  /**
   * Channel mapping
   */
  LIST_ENTRY(th_transport) tht_ch_link;
  struct channel *tht_ch;

  /**
   * Service probe, see serviceprobe.c for details
   */
  int tht_sp_onqueue;
  TAILQ_ENTRY(th_transport) tht_sp_link;

  /**
   * Pending save.
   *
   * transport_request_save() will enqueue the transport here.
   * We need to do this if we don't hold the global lock.
   * This happens when we update PMT from within the TS stream itself.
   * Then we hold the stream mutex, and thus, can not obtain the global lock
   * as it would cause lock inversion.
   */
  int tht_ps_onqueue;
  TAILQ_ENTRY(th_transport) tht_ps_link;

  /**
   * Timer which is armed at transport start. Once it fires
   * it will check if any packets has been parsed. If not the status
   * will be set to TRANSPORT_STATUS_NO_INPUT
   */
  gtimer_t tht_receive_timer;

  /**
   * IPTV members
   */
  char *tht_iptv_iface;
  struct in_addr tht_iptv_group;
  uint16_t tht_iptv_port;
  int tht_iptv_fd;

  /**
   * For per-transport PAT/PMT parsers, allocated on demand
   * Free'd by transport_destroy
   */
  struct psi_section *tht_pat_section;
  struct psi_section *tht_pmt_section;

  /**
   * V4l members
   */

  struct v4l_adapter *tht_v4l_adapter;
  int tht_v4l_frequency; // In Hz
  

  /*********************************************************
   *
   * Streaming part of transport
   *
   * All access to fields below this must be protected with
   * tht_stream_mutex held.
   *
   * Note: Code holding tht_stream_mutex should _never_ 
   * acquire global_lock while already holding tht_stream_mutex.
   *
   */

  /**
   * Mutex to be held during streaming.
   * This mutex also protects all th_stream_t instances for this
   * transport.
   */
  pthread_mutex_t tht_stream_mutex;


  /**
   * Condition variable to singal when streaming_status changes
   * interlocked with tht_stream_mutex
   */
  pthread_cond_t tht_tss_cond;
  /**
   *
   */			   
  int tht_streaming_status;

  // Progress
#define TSS_INPUT_HARDWARE   0x1
#define TSS_INPUT_SERVICE    0x2
#define TSS_MUX_PACKETS      0x4
#define TSS_PACKETS          0x8

#define TSS_GRACEPERIOD      0x8000

  // Errors
#define TSS_NO_DESCRAMBLER   0x10000
#define TSS_NO_ACCESS        0x20000

#define TSS_ERRORS           0xffff0000


  /**
   * For simple streaming sources (such as video4linux) keeping
   * track of the video and audio stream is convenient.
   */
  th_stream_t *tht_video;
  th_stream_t *tht_audio;
 
  /**
   * Average continuity errors
   */
  avgstat_t tht_cc_errors;

  /**
   * Average bitrate
   */
  avgstat_t tht_rate;

  /**
   * Descrambling support
   */

  struct th_descrambler_list tht_descramblers;
  int tht_scrambled;
  int tht_scrambled_seen;
  int tht_caid;

  /**
   * PCR drift compensation. This should really be per-packet.
   */
  int64_t  tht_pcr_drift;

  /**
   * List of all components.
   */
  struct th_stream_queue tht_components;


  /**
   * Delivery pad, this is were we finally deliver all streaming output
   */
  streaming_pad_t tht_streaming_pad;


  loglimiter_t tht_loglimit_tei;

} th_transport_t;

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
extern struct th_transport_list all_transports;
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

void sbuf_free(sbuf_t *sb);

void sbuf_reset(sbuf_t *sb);

void sbuf_err(sbuf_t *sb);

void sbuf_alloc(sbuf_t *sb, int len);

void sbuf_append(sbuf_t *sb, const uint8_t *data, int len);

void sbuf_cut(sbuf_t *sb, int off);

#endif /* TV_HEAD_H */
