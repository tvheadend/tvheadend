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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <libhts/htsq.h>
#include <libhts/htstv.h>
#include <libhts/avg.h>
#include <libhts/hts_strtab.h>
#include <libavcodec/avcodec.h>
#include <libhts/redblack.h>
#include <linux/dvb/frontend.h>

extern pthread_mutex_t global_lock;

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

void gtimer_disarm(gtimer_t *gti);


/*
 * List / Queue header declarations
 */
LIST_HEAD(th_subscription_list, th_subscription);
RB_HEAD(channel_tree, channel);
TAILQ_HEAD(channel_queue, channel);
LIST_HEAD(channel_list, channel);
TAILQ_HEAD(th_dvb_adapter_queue, th_dvb_adapter);
LIST_HEAD(th_v4l_adapter_list, th_v4l_adapter);
LIST_HEAD(event_list, event);
RB_HEAD(event_tree, event);
LIST_HEAD(pvr_rec_list, pvr_rec);
TAILQ_HEAD(ref_update_queue, ref_update);
LIST_HEAD(th_transport_list, th_transport);
RB_HEAD(th_transport_tree, th_transport);
TAILQ_HEAD(th_transport_queue, th_transport);
RB_HEAD(th_dvb_mux_instance_tree, th_dvb_mux_instance);
LIST_HEAD(th_stream_list, th_stream);
TAILQ_HEAD(th_pkt_queue, th_pkt);
LIST_HEAD(th_pkt_list, th_pkt);
LIST_HEAD(th_muxer_list, th_muxer);
LIST_HEAD(th_muxstream_list, th_muxstream);
LIST_HEAD(th_descrambler_list, th_descrambler);
TAILQ_HEAD(th_refpkt_queue, th_refpkt);
TAILQ_HEAD(th_muxpkt_queue, th_muxpkt);
LIST_HEAD(autorec_list, autorec);

extern time_t dispatch_clock;
extern int startupcounter;
extern struct th_transport_list all_transports;
extern struct channel_tree channel_name_tree;
extern struct pvr_rec_list pvrr_global_list;
extern struct th_subscription_list subscriptions;

struct th_transport;
struct th_stream;

/*
 * Video4linux adapter
 */
typedef struct th_v4l_adapter {

  const char *tva_name;

  LIST_ENTRY(th_v4l_adapter) tva_link;

  const char *tva_path;

  pthread_t tva_ptid;

  struct th_transport_list tva_transports;

  int tva_frequency;

  pthread_cond_t tva_run_cond;

  int tva_fd;

  void *tva_dispatch_handle;


  uint32_t tva_startcode;
  uint16_t tva_packet_len;
  int tva_lenlock;

} th_v4l_adapter_t;

/*
 * DVB Mux instance
 */
typedef struct th_dvb_mux_instance {

  enum {
    TDMI_QUICKSCAN_NONE,
    TDMI_QUICKSCAN_RUNNING,
    TDMI_QUICKSCAN_WAITING,
  } tdmi_quickscan;

  RB_ENTRY(th_dvb_mux_instance) tdmi_global_link;
  RB_ENTRY(th_dvb_mux_instance) tdmi_adapter_link;
  RB_ENTRY(th_dvb_mux_instance) tdmi_qscan_link;

  struct th_dvb_adapter *tdmi_adapter;

  uint16_t tdmi_snr, tdmi_signal;
  uint32_t tdmi_ber, tdmi_uncorrected_blocks;

#define TDMI_FEC_ERR_HISTOGRAM_SIZE 10
  uint32_t tdmi_fec_err_histogram[TDMI_FEC_ERR_HISTOGRAM_SIZE];
  int      tdmi_fec_err_ptr;

  time_t tdmi_time;
  LIST_HEAD(, th_dvb_table) tdmi_tables;

  enum {
    TDMI_FE_UNKNOWN,
    TDMI_FE_NO_SIGNAL,
    TDMI_FE_FAINT_SIGNAL,
    TDMI_FE_BAD_SIGNAL,
    TDMI_FE_CONSTANT_FEC,
    TDMI_FE_BURSTY_FEC,
    TDMI_FE_OK,
  } tdmi_fe_status;

  int tdmi_quality;

  time_t tdmi_got_adapter;
  time_t tdmi_lost_adapter;

  struct dvb_frontend_parameters tdmi_fe_params;
  uint8_t tdmi_polarisation;  /* for DVB-S */
  uint8_t tdmi_switchport;    /* for DVB-S */

  uint16_t tdmi_transport_stream_id;

  char *tdmi_identifier;
  char *tdmi_network;     /* Name of network, from NIT table */

  struct th_transport_list tdmi_transports; /* via tht_mux_link */

} th_dvb_mux_instance_t;


/*
 * DVB Adapter (one of these per physical adapter)
 */
typedef struct th_dvb_adapter {

  TAILQ_ENTRY(th_dvb_adapter) tda_global_link;

  struct th_dvb_mux_instance_tree tda_muxes;
  struct th_dvb_mux_instance_tree tda_muxes_qscan_waiting;

  th_dvb_mux_instance_t *tda_mux_current;

  int tda_table_epollfd;

  const char *tda_rootpath;
  char *tda_identifier;
  uint32_t tda_autodiscovery;
  char *tda_displayname;

  int tda_fe_fd;
  int tda_type;
  struct dvb_frontend_info *tda_fe_info;

  char *tda_demux_path;

  char *tda_dvr_path;

  gtimer_t tda_mux_scanner_timer;

  pthread_mutex_t tda_delivery_mutex;
  struct th_transport_list tda_transports; /* Currently bound transports */

  gtimer_t tda_fe_monitor_timer;
  int tda_fe_monitor_hold;

} th_dvb_adapter_t;


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
 * Stream, one media component for a transport
 */
typedef struct th_stream {
  LIST_ENTRY(th_stream) st_link;
  uint16_t st_pid;
  uint8_t st_cc;             /* Last CC */
  uint8_t st_cc_valid;       /* Is CC valid at all? */

  avgstat_t st_cc_errors;
  avgstat_t st_rate;

  tv_streamtype_t st_type;
  int st_demuxer_fd;
  int st_index;
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

  /* Codec */

  struct AVCodecContext *st_ctx;
  struct AVCodecParserContext *st_parser;

  /* All packets currently hanging on to us */

  struct th_pkt_list st_packets;

  /* Temporary frame store for calculating PTS */

  struct th_pkt_queue st_ptsq;
  int st_ptsq_len;

  /* Temporary frame store for calculating duration */

  struct th_pkt_queue st_durationq;

  /* Final frame store */

  struct th_pkt_queue st_pktq;

  /* ca id for this stream */

  uint16_t st_caid;

  char st_lang[4]; /* ISO 639 3-letter language code */

  /* Remuxing information */
  AVRational st_tb;

  int st_vbv_size;        /* Video buffer size (in bytes) */
  int st_vbv_delay;       /* -1 if CBR */

} th_stream_t;



/**
 * Transport events, these are sent to subscribers via
 * s->ths_event_callback
 */
typedef enum {

  SUBSCRIPTION_EVENT_INVALID = 0, /* mbz */

  /** Transport is receiving data from source */
  SUBSCRIPTION_TRANSPORT_RUN,

  /** No input is received from source */
  SUBSCRIPTION_NO_INPUT,

  /** No descrambler is able to decrypt the stream */
  SUBSCRIPTION_NO_DESCRAMBLER,

  /** Potential descrambler is available, but access is denied */
  SUBSCRIPTION_NO_ACCESS,

  /** Raw input seen but nothing has really been decoded */
  SUBSCRIPTION_RAW_INPUT,

  /** Packet are being parsed. Only signalled if at least one muxer is
      registerd */  
  SUBSCRIPTION_VALID_PACKETS,

  /** No transport is available for delivering subscription */
  SUBSCRIPTION_TRANSPORT_NOT_AVAILABLE,

  /** Transport no longer runs, it was needed by someone with higher
      priority */
  SUBSCRIPTION_TRANSPORT_LOST,

  /** Subscription destroyed */
  SUBSCRIPTION_DESTROYED,

} subscription_event_t;



/**
 * A Transport (or in MPEG TS terms: a 'service')
 */
typedef struct th_transport {
  
  const char *tht_name;

  LIST_ENTRY(th_transport) tht_hash_link;

  enum {
    TRANSPORT_DVB,
    TRANSPORT_IPTV,
    TRANSPORT_V4L,
    TRANSPORT_AVGEN,
    TRANSPORT_STREAMEDFILE,
  } tht_type;

  enum {
    TRANSPORT_IDLE,
    TRANSPORT_RUNNING,
    TRANSPORT_PROBING,
  } tht_runstatus;

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
   * Set if transport is disabled. If disabled it should not be
   * considered when chasing for available transports during
   * subscription scheduling.
   */
  int tht_disabled;

  
  LIST_ENTRY(th_transport) tht_mux_link;

  LIST_ENTRY(th_transport) tht_active_link;

  LIST_HEAD(, th_subscription) tht_subscriptions;

  int (*tht_start_feed)(struct th_transport *t, unsigned int weight,
			int status, int force_start);

  void (*tht_stop_feed)(struct th_transport *t);

  void (*tht_config_change)(struct th_transport *t);

  const char *(*tht_networkname)(struct th_transport *t);

  const char *(*tht_sourcename)(struct th_transport *t);

  int (*tht_quality_index)(struct th_transport *t);


  /*
   * Per source type structs
   */

  union {

    struct {
      struct th_dvb_mux_instance *mux_instance;
    } dvb;

    struct {
      int frequency;
      struct th_v4l_adapter *adapter;
    } v4l;
    
    struct {
      struct in_addr group_addr;
      struct in_addr interface_addr;
      int ifindex;

      int port;
      int fd;
      void *dispatch_handle;
      enum {
	IPTV_MODE_RAWUDP,
      } mode;
    } iptv;

    struct {
      struct avgen *avgen;
    } avgen;

    struct {
      struct file_input *file_input;
    } file_input;
 } u;

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
  char *tht_chname;

  /**
   * Last known status (or error)
   */			   
  int tht_last_status;

  /**
   * Service probe, see serviceprobe.c for details
   */
  int tht_sp_onqueue;
  TAILQ_ENTRY(th_transport) tht_sp_link;

  /**
   * Timer which is armed at transport start. Once it fires
   * it will check if any packets has been parsed. If not the status
   * will be set to SUBSCRIPTION_NO_INPUT
   */
  gtimer_t tht_receive_timer;

  /**
   * Set as soon as we get some kind of activity
   */
  int tht_packets;

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
   * Condition variable to wakeup transport streaming thread.
   *
   * Currently unused (All streaming is executed on the source
   * thread)
   */
  pthread_cond_t tht_stream_cond;

  /**
   * List of all streams
   */
  struct th_stream_list tht_streams;

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
   * List of muxers
   *
   * The parsing code will invoke each of these whene complete
   * packets have been assembled.
   */
  struct th_muxer_list tht_muxers;

  /**
   * Used by parsing code to normalize timestamp to zero
   */
  int64_t tht_dts_start;

  /**
   * PCR drift compensation. This should really be per-packet.
   */
  int64_t  tht_pcr_drift;

  /**
   * Set if we've seen the PMT. Used to avoid (re)saving the transport
   * for everytime we see a PMT. 
   * XXX: Not very good, should be replaced with more intelligent
   * code in psi.c
   */
  int tht_pmt_seen;

} th_transport_t;



#define tht_v4l_frequency u.v4l.frequency
#define tht_v4l_adapter   u.v4l.adapter

#define tht_dvb_mux_instance u.dvb.mux_instance

#define tht_iptv_group_addr      u.iptv.group_addr
#define tht_iptv_interface_addr  u.iptv.interface_addr
#define tht_iptv_ifindex         u.iptv.ifindex
#define tht_iptv_port            u.iptv.port
#define tht_iptv_dispatch_handle u.iptv.dispatch_handle
#define tht_iptv_fd              u.iptv.fd
#define tht_iptv_mode            u.iptv.mode

#define tht_avgen                u.avgen.avgen

#define tht_file_input           u.file_input.file_input

/*
 * Storage
 */
typedef struct th_storage {
  unsigned int ts_offset;
  unsigned int ts_refcount;
  int ts_fd;
  char *ts_filename;
} th_storage_t;

/*
 * A packet
 */

#define PKT_I_FRAME 1
#define PKT_P_FRAME 2
#define PKT_B_FRAME 3


typedef struct th_pkt {
  TAILQ_ENTRY(th_pkt) pkt_queue_link;
  uint8_t pkt_on_stream_queue;
  uint8_t pkt_frametype;
  uint8_t pkt_commercial;

  int64_t pkt_dts;
  int64_t pkt_pts;
  int pkt_duration;
  int pkt_refcount;

  th_storage_t *pkt_storage;
  TAILQ_ENTRY(th_pkt) pkt_disk_link;
  int pkt_storage_offset;

  uint8_t *pkt_payload;
  int pkt_payloadlen;
  TAILQ_ENTRY(th_pkt) pkt_mem_link;
} th_pkt_t;

/**
 * Referenced packets
 */
typedef struct th_refpkt {
  TAILQ_ENTRY(th_refpkt) trp_link;
  th_pkt_t *trp_pkt;
} th_refpkt_t;


/**
 * Muxed packets
 */
typedef struct th_muxpkt {
  TAILQ_ENTRY(th_muxpkt) tm_link;
  int64_t tm_pcr;
  int64_t tm_dts;

  int tm_contentsize;
  int64_t tm_deadline;   /* Packet transmission deadline */

  uint8_t tm_pkt[0];
} th_muxpkt_t;


/*
 * A mux stream reader
 */


struct th_subscription;
struct th_muxstream;

typedef void (th_mux_output_t)(void *opaque, struct th_muxstream *tms,
			       th_pkt_t *pkt);


typedef struct th_muxfifo {
  struct th_muxpkt_queue tmf_queue;
  uint32_t tmf_len;
  int tmf_contentsize;

} th_muxfifo_t;



typedef struct th_muxstream {

  LIST_ENTRY(th_muxstream) tms_muxer_link0;
  struct th_muxer *tms_muxer;
  th_stream_t *tms_stream;
  int tms_index; /* Used as PID or whatever */

  struct th_refpkt_queue tms_lookahead;

  int tms_lookahead_depth;  /* bytes in lookahead queue */
  int tms_lookahead_packets;

  th_muxfifo_t tms_cbr_fifo;
  th_muxfifo_t tms_delivery_fifo;

  int64_t tms_deadline;
  int64_t tms_delay;
  int64_t tms_delta;
  int64_t tms_mux_offset;

			   //dtimer_t tms_mux_timer;

  /* MPEG TS multiplex stuff */

  int tms_sc; /* start code */
  int tms_cc;

  int64_t tms_corruption_interval;
  int64_t tms_corruption_last;
  int tms_corruption_counter;

  /* Memebers used when running with ffmpeg */
  
  struct AVStream *tms_avstream;
  int tms_decoded;

  int tms_blockcnt;
  int64_t tms_dl;
  int64_t tms_staletime;
} th_muxstream_t;


/*
 *
 */


typedef void (th_mux_newpkt_t)(struct th_muxer *tm, th_stream_t *st,
			       th_pkt_t *pkt);

typedef struct th_muxer {

  th_mux_newpkt_t *tm_new_pkt;

  LIST_ENTRY(th_muxer) tm_transport_link;
  th_transport_t *tm_transport;

  int64_t tm_offset;

  struct th_muxstream_list tm_streams;

  th_mux_output_t *tm_output;
  void *tm_opaque;
  

  enum {
    TM_IDLE,
    TM_WAITING_FOR_LOCK,
    TM_PLAY,
    TM_PAUSE,
  } tm_status;

} th_muxer_t;


/**
 * Output muxer for usage via ffmpeg (avformat)
 */
typedef struct th_ffmuxer {
  th_muxer_t tffm_muxer;
  
  enum {
    TFFM_STOP,
    TFFM_WAIT_SUBSCRIPTION,
    TFFM_WAIT_FOR_START,
    TFFM_WAIT_AUDIO_LOCK,
    TFFM_WAIT_VIDEO_LOCK,
    TFFM_RUNNING,
    TFFM_COMMERCIAL,

  } tffm_state;

  int tffm_header_written;

  char *tffm_printname;

  struct AVFormatContext *tffm_avfctx;
} th_ffmuxer_t;




/*
 *  Teletext
 */
typedef struct tt_page {
  int ttp_page;
  int ttp_subpage;
  int ttp_ver;
  unsigned char ttp_pagebuf[23*40 + 1];
} tt_page_t;

typedef struct tt_mag {
  tt_page_t *pageptr;
} tt_mag_t;

typedef struct tt_decoder {
  tt_mag_t mags[8];
  tt_page_t *pages[900];
  int magazine_serial;
} tt_decoder_t;




char *utf8toprintable(const char *in);
char *utf8tofilename(const char *in);
const char *htstvstreamtype2txt(tv_streamtype_t s);

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

void tvhlog(int severity, const char *subsys, const char *fmt, ...);

#define	LOG_EMERG	0	/* system is unusable */
#define	LOG_ALERT	1	/* action must be taken immediately */
#define	LOG_CRIT	2	/* critical conditions */
#define	LOG_ERR		3	/* error conditions */
#define	LOG_WARNING	4	/* warning conditions */
#define	LOG_NOTICE	5	/* normal but significant condition */
#define	LOG_INFO	6	/* informational */
#define	LOG_DEBUG	7	/* debug-level messages */


static inline int64_t 
getclock_hires(void)
{
  int64_t now;
  struct timeval tv;

  gettimeofday(&tv, NULL);
  now = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
  return now;
}


#endif /* TV_HEAD_H */
