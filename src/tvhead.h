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

#include <libavcodec/avcodec.h>

#include "redblack.h"

extern pthread_mutex_t global_lock;
extern pthread_mutex_t ffmpeg_lock;

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
LIST_HEAD(th_muxer_list, th_muxer);
LIST_HEAD(th_muxstream_list, th_muxstream);
LIST_HEAD(th_descrambler_list, th_descrambler);
TAILQ_HEAD(th_refpkt_queue, th_refpkt);
TAILQ_HEAD(th_muxpkt_queue, th_muxpkt);
LIST_HEAD(dvr_autorec_entry_list, dvr_autorec_entry);
TAILQ_HEAD(th_pktref_queue, th_pktref);
LIST_HEAD(streaming_target_list, streaming_target);


typedef enum {
  SCT_MPEG2VIDEO = 1,
  SCT_MPEG2AUDIO,
  SCT_H264,
  SCT_AC3,
  SCT_TELETEXT,
  SCT_SUBTITLES,
  SCT_CA,
  SCT_PAT,
  SCT_PMT,
  SCT_AAC,
} streaming_component_type_t;


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
  SMT_PACKET,       // sm_data is a th_pkt. Unref when destroying msg
  SMT_START,        // sm_data is a htsmsg, see transport_build_stream_msg()
  SMT_STOP,         // sm_data is a htsmsg
  SMT_TRANSPORT_STATUS, // sm_code is TRANSPORT_STATUS_
  SMT_EXIT,             // Used to signal exit to threads
  SMT_NOSOURCE,
} streaming_message_type_t;


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
 * Descrambler superclass
 *
 * Created/Destroyed on per-transport basis upon transport start/stop
 */
typedef struct th_descrambler {
  LIST_ENTRY(th_descrambler) td_transport_link;

  void (*td_table)(struct th_descrambler *d, struct th_transport *t,
		   struct th_stream *st, uint8_t *section, int section_len);

  int (*td_descramble)(struct th_descrambler *d, struct th_transport *t,
		       struct th_stream *st, uint8_t *tsb);

  void (*td_stop)(struct th_descrambler *d);

} th_descrambler_t;



/*
 * Section callback, called when a PSI table is fully received
 */
typedef void (pid_section_callback_t)(struct th_transport *t,
				      struct th_stream *pi,
				      uint8_t *section, int section_len);

/*
 * Stream, one media component for a transport.
 *
 * XXX: This should be renamed to 'elementary_stream' or something
 */
typedef struct th_stream {

  LIST_ENTRY(th_stream) st_link;
  
  streaming_component_type_t st_type;
  int st_index;

  char st_lang[4];           /* ISO 639 3-letter language code */

  uint16_t st_pid;
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

  uint8_t *st_buffer;
  int st_buffer_ptr;
  int st_buffer_size;
  int st_buffer_errors;   /* Errors accumulated for this packet */
  uint32_t st_startcond;
  uint32_t st_startcode;
  uint32_t st_startcode_offset;
  int st_parser_state;
  int st_parser_ptr;
  void *st_priv;          /* Parser private data */

  struct th_pkt *st_curpkt;
  int64_t st_curpts;
  int64_t st_curdts;
  int64_t st_prevdts;
  int64_t st_nextdts;
  int st_frame_duration;

  /* DTS generator */

  int64_t st_dts_epoch;  /* upper bits (auto generated) */
  int64_t st_last_dts;
  int st_bad_dts;

  /* Codec */

  struct AVCodecContext *st_ctx;
  struct AVCodecParserContext *st_parser;

  /* Temporary frame store for calculating PTS */

  struct th_pktref_queue st_ptsq;
  int st_ptsq_len;

  /* Temporary frame store for calculating duration */

  struct th_pktref_queue st_durationq;

  /* ca id for this stream */

  uint16_t st_caid;
  uint32_t st_providerid;

  /* Remuxing information */
  AVRational st_tb;

  int st_vbv_size;        /* Video buffer size (in bytes) */
  int st_vbv_delay;       /* -1 if CBR */

  /* */

  int st_delete_me;      /* Temporary flag for deleting streams */

} th_stream_t;


/**
 *
 */
typedef enum {

  /** No status known */
  TRANSPORT_FEED_UNKNOWN,

  /** No packets are received from source at all */
  TRANSPORT_FEED_NO_INPUT,

  /** No input is received from source destined for this transport */
  TRANSPORT_FEED_NO_DEMUXED_INPUT,

  /** Raw input seen but nothing has really been decoded */
  TRANSPORT_FEED_RAW_INPUT,

  /** No descrambler is able to decrypt the stream */
  TRANSPORT_FEED_NO_DESCRAMBLER,

  /** Potential descrambler is available, but access is denied */
  TRANSPORT_FEED_NO_ACCESS,

  /** Packet are being parsed. */
  TRANSPORT_FEED_VALID_PACKETS,

} transport_feed_status_t;


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

  
  LIST_ENTRY(th_transport) tht_group_link;

  LIST_ENTRY(th_transport) tht_active_link;

  LIST_HEAD(, th_subscription) tht_subscriptions;

  int (*tht_start_feed)(struct th_transport *t, unsigned int weight,
			int status, int force_start);

  void (*tht_refresh_feed)(struct th_transport *t);

  void (*tht_stop_feed)(struct th_transport *t);

  void (*tht_config_save)(struct th_transport *t);

  struct htsmsg *(*tht_sourceinfo)(struct th_transport *t);

  int (*tht_quality_index)(struct th_transport *t);


  /*
   * Per source type structs
   */
  struct th_dvb_mux_instance *tht_dvb_mux_instance;

  /**
   * Unique identifer (used for storing on disk, etc)
   */
  char *tht_identifier;

  /**
   * Service ID according to EN 300 468
   */
  uint16_t tht_dvb_service_id;

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
   * Last known data status (or error)
   */			   
  transport_feed_status_t tht_feed_status;

  /**
   * Set as soon as we get some kind of activity
   */
  transport_feed_status_t tht_input_status;


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
  int tht_caid;

  /**
   * Used by parsing code to normalize timestamp to zero
   */
  int64_t tht_dts_start;

  /**
   * PCR drift compensation. This should really be per-packet.
   */
  int64_t  tht_pcr_drift;

  /**
   * List of all components.
   */
  struct th_stream_list tht_components;


  /**
   * Delivery pad, this is were we finally deliver all streaming output
   */
  streaming_pad_t tht_streaming_pad;


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
getclock_hires(void)
{
  int64_t now;
  struct timeval tv;

  gettimeofday(&tv, NULL);
  now = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
  return now;
}




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

#endif /* TV_HEAD_H */
