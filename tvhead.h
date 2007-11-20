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
#include <syslog.h>
#include <netinet/in.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>
#include <libhts/htsq.h>
#include <libhts/htstv.h>
#include <libhts/htscfg.h>
#include <libhts/avg.h>
#include "refstr.h"

/*
 * Commercial status
 */
typedef enum {
  COMMERCIAL_UNKNOWN,
  COMMERCIAL_YES,
  COMMERCIAL_NO,
} th_commercial_advice_t;


/*
 * Dispatch timer
 */
typedef void (dti_callback_t)(void *opaque, int64_t now);

typedef struct dtimer {
  LIST_ENTRY(dtimer) dti_link;
  dti_callback_t *dti_callback;
  void *dti_opaque;
  int64_t dti_expire;
} dtimer_t;



/*
 * List / Queue header declarations
 */

LIST_HEAD(th_subscription_list, th_subscription);
TAILQ_HEAD(th_channel_queue, th_channel);
LIST_HEAD(th_dvb_adapter_list, th_dvb_adapter);
LIST_HEAD(th_v4l_adapter_list, th_v4l_adapter);
LIST_HEAD(event_list, event);
TAILQ_HEAD(event_queue, event);
LIST_HEAD(pvr_rec_list, pvr_rec);
TAILQ_HEAD(ref_update_queue, ref_update);
LIST_HEAD(th_transport_list, th_transport);
LIST_HEAD(th_dvb_mux_list, th_dvb_mux);
LIST_HEAD(th_dvb_mux_instance_list, th_dvb_mux_instance);
LIST_HEAD(th_stream_list, th_stream);
TAILQ_HEAD(th_pkt_queue, th_pkt);
LIST_HEAD(th_pkt_list, th_pkt);
LIST_HEAD(th_muxer_list, th_muxer);
LIST_HEAD(th_muxstream_list, th_muxstream);


extern time_t dispatch_clock;
extern int startupcounter;
extern struct th_transport_list all_transports;
extern struct th_channel_queue channels;
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
  int tva_running;

  pthread_cond_t tva_run_cond;

  int tva_fd;

  void *tva_dispatch_handle;


  uint32_t tva_startcode;
  uint16_t tva_packet_len;
  int tva_lenlock;

} th_v4l_adapter_t;


/*
 * Mux instance modes
 */
typedef enum {
    TDMI_CONFIGURED,
    TDMI_INITIAL_SCAN,
    TDMI_IDLE,
    TDMI_RUNNING,
} tdmi_state_t;

/*
 * DVB Mux instance
 */
typedef struct th_dvb_mux_instance {
  LIST_ENTRY(th_dvb_mux_instance) tdmi_mux_link;
  LIST_ENTRY(th_dvb_mux_instance) tdmi_adapter_link;

  struct th_dvb_mux *tdmi_mux;
  struct th_dvb_adapter *tdmi_adapter;

  fe_status_t tdmi_fe_status;
  uint16_t tdmi_snr, tdmi_signal;
  uint32_t tdmi_ber, tdmi_uncorrected_blocks;

  uint32_t tdmi_fec_err_per_sec;

  time_t tdmi_time;
  LIST_HEAD(, th_dvb_table) tdmi_tables;

  tdmi_state_t tdmi_state;

  dtimer_t tdmi_initial_scan_timer;
  const char *tdmi_status;

  time_t tdmi_got_adapter;
  time_t tdmi_lost_adapter;

} th_dvb_mux_instance_t;

/*
 *
 */
typedef struct th_dvb_table {
  LIST_ENTRY(th_dvb_table) tdt_link;
  char *tdt_name;
  void *tdt_handle;
  struct th_dvb_mux_instance *tdt_tdmi;
  int tdt_count; /* times seen */
  void *tdt_opaque;
  int (*tdt_callback)(th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len,
		      uint8_t tableid, void *opaque);
} th_dvb_table_t;


/*
 * DVB Mux (more or less the DVB frontend configuration on)
 */
typedef struct th_dvb_mux {
  LIST_ENTRY(th_dvb_mux) tdm_global_link;

  struct th_dvb_mux_instance_list tdm_instances;

  struct dvb_frontend_parameters tdm_fe_params;

  const char *tdm_name;
  const char *tdm_title;

} th_dvb_mux_t;


/*
 * DVB Adapter (one of these per physical adapter)
 */
typedef struct th_dvb_adapter {

  LIST_ENTRY(th_dvb_adapter) tda_adapter_to_mux_link;

  struct th_dvb_mux_instance_list tda_muxes_configured;

  struct th_dvb_mux_instance_list tda_muxes_active;
  th_dvb_mux_instance_t *tda_mux_current;

  const char *tda_path;
  const char *tda_name;

  LIST_ENTRY(th_dvb_adapter) tda_link;

  LIST_HEAD(, th_dvb_mux) tda_muxes;

  int tda_running;

  pthread_mutex_t tda_mux_lock;

  int tda_fe_fd;
  struct dvb_frontend_info tda_fe_info;

  char *tda_demux_path;

  char *tda_dvr_path;

  struct th_transport_list tda_transports; /* Currently bound transports */

  dtimer_t tda_fec_monitor_timer;
  dtimer_t tda_mux_scanner_timer;

} th_dvb_adapter_t;






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

  tv_streamtype_t st_type;
  int st_demuxer_fd;
  int st_index;
  int st_peak_presentation_delay; /* Max seen diff. of DTS and PTS */

  struct psi_section *st_section;
  pid_section_callback_t *st_got_section;

  /* For transport stream packet reassembly */

  uint8_t *st_buffer;
  int st_buffer_ptr;
  int st_buffer_size;
  int st_buffer_errors;   /* Errors accumulated for this packet */

  /* DTS generator */

  int32_t st_dts_u;  /* upper bits (auto generated) */
  int64_t st_dts;

  /* Codec */

  struct AVCodecContext *st_ctx;
  struct AVCodecParserContext *st_parser;

  /* All packets currently hanging on to us */

  struct th_pkt_list st_packets;

  /* Temporary frame store for calculating DTS */

  struct th_pkt_queue st_dtsq;
  int st_dtsq_len;
  int64_t st_last_dts;

  /* Temporary frame store for calculating PTS */

  struct th_pkt_queue st_ptsq;
  int st_ptsq_len;

  /* Temporary frame store for calculating duration */

  struct th_pkt_queue st_durationq;
  int st_duration;

  /* Final frame store */

  struct th_pkt_queue st_pktq;

} th_stream_t;


/*
 * A Transport (or in MPEG TS terms: a 'service')
 */
typedef struct th_transport {
  
  const char *tht_name;

  LIST_ENTRY(th_transport) tht_global_link;

  enum {
    TRANSPORT_DVB,
    TRANSPORT_IPTV,
    TRANSPORT_V4L,
  } tht_type;

  enum {
    TRANSPORT_IDLE,
    TRANSPORT_RUNNING,
    TRANSPORT_PROBING,
  } tht_status;

  th_commercial_advice_t tht_tt_commercial_advice;

  int tht_tt_rundown_content_length;
  time_t tht_tt_clock;   /* Network clock as determined by teletext
			    decoder */
  int tht_prio;
  
  struct th_stream_list tht_streams;
  th_stream_t *tht_video;
  th_stream_t *tht_audio;

  uint16_t tht_pcr_pid;
  uint16_t tht_dvb_network_id;
  uint16_t tht_dvb_transport_id;
  uint16_t tht_dvb_service_id;

  avgstat_t tht_cc_errors;
  avgstat_t tht_rate;
  int tht_monitor_suspend;

  int tht_cc_error_log_limiter;
  int tht_rate_error_log_limiter;

  int64_t tht_dts_start;
  
  LIST_ENTRY(th_transport) tht_adapter_link;

  LIST_ENTRY(th_transport) tht_channel_link;
  struct th_channel *tht_channel;

  LIST_HEAD(, th_subscription) tht_subscriptions;


  struct th_muxer_list tht_muxers; /* muxers */

  /*
   * Per source type structs
   */

  union {

    struct {
      struct th_dvb_mux *mux;
      struct th_dvb_adapter *adapter;
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
  } u;


} th_transport_t;



#define tht_v4l_frequency u.v4l.frequency
#define tht_v4l_adapter   u.v4l.adapter

#define tht_dvb_mux       u.dvb.mux
#define tht_dvb_adapter   u.dvb.adapter

#define tht_iptv_group_addr      u.iptv.group_addr
#define tht_iptv_interface_addr  u.iptv.interface_addr
#define tht_iptv_ifindex         u.iptv.ifindex
#define tht_iptv_port            u.iptv.port
#define tht_iptv_dispatch_handle u.iptv.dispatch_handle
#define tht_iptv_fd              u.iptv.fd
#define tht_iptv_mode            u.iptv.mode


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

  th_stream_t *pkt_stream;

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


/*
 * A mux stream reader
 */
typedef struct th_muxstream {

  LIST_ENTRY(th_muxstream) tms_muxer_link;
  struct th_muxer *tms_muxer;

  th_pkt_t *tms_curpkt;

  int tms_offset;          /* offset in current packet */

  th_stream_t *tms_stream;
  LIST_ENTRY(th_muxstream) tms_muxer_media_link;

  int64_t tms_nextblock;   /* Time for delivery of next block */
  int tms_block_interval;
  int tms_block_rate;

  int tms_mux_offset;

  int tms_index; /* Used as PID or whatever */

  th_pkt_t *tms_tmppkt; /* temporary pkt pointer during lock phaze */

  /* MPEG TS multiplex stuff */

  int tms_sc; /* start code */
  int tms_cc;
  int tms_dopcr;
  int64_t tms_nextpcr;

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

struct th_subscription;

typedef void (th_mux_output_t)(void *opaque, struct th_subscription *s,
			       uint8_t *pkt, int blocks, int64_t pcr);


typedef void (th_mux_newpkt_t)(struct th_muxer *tm, th_stream_t *st,
			       th_pkt_t *pkt);

typedef struct th_muxer {

  th_mux_newpkt_t *tm_new_pkt;

  LIST_ENTRY(th_muxer) tm_transport_link;

  int64_t tm_clockref;  /* Base clock ref */
  int64_t tm_pauseref;  /* Time when we were paused */

  int tm_flags;
#define TM_HTSCLIENTMODE 0x1

  int64_t tm_start_dts;
  int64_t tm_next_pat;

  struct th_muxstream_list tm_media_streams;

  struct th_muxstream_list tm_active_streams;
  struct th_muxstream_list tm_stale_streams;
  struct th_muxstream_list tm_stopped_streams;

  uint8_t *tm_packet;
  int tm_blocks_per_packet;

  struct th_subscription *tm_subscription;

  th_mux_output_t *tm_callback;
  void *tm_opaque;
  
  dtimer_t tm_timer;

  enum {
    TM_IDLE,
    TM_WAITING_FOR_LOCK,
    TM_PLAY,
    TM_PAUSE,
  } tm_status;

  th_muxstream_t *tm_pat;
  th_muxstream_t *tm_pmt;

  struct AVFormatContext *tm_avfctx;
} th_muxer_t;




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


/*
 * Channel definition
 */ 
typedef struct th_channel {
  
  TAILQ_ENTRY(th_channel) ch_global_link;
  LIST_HEAD(, th_transport) ch_transports;
  LIST_HEAD(, th_subscription) ch_subscriptions;

  int ch_index;

  const char *ch_name;

  struct pvr_rec *ch_rec;

  struct tt_decoder ch_tt;

  int ch_tag;

  int ch_teletext_rundown;

  struct event_queue ch_epg_events;
  struct event *ch_epg_cur_event;
  refstr_t *ch_icon;

} th_channel_t;


/*
 * Subscription
 */

typedef enum {
  TRANSPORT_AVAILABLE,
  TRANSPORT_UNAVAILABLE,
} subscription_event_t;

typedef void (subscription_callback_t)(struct th_subscription *s,
				       subscription_event_t event,
				       void *opaque);

typedef void (subscription_raw_input_t)(struct th_subscription *s,
					void *data, int len,
					th_stream_t *st,
					void *opaque);

typedef struct th_subscription {
  LIST_ENTRY(th_subscription) ths_global_link;
  int ths_weight;

  LIST_ENTRY(th_subscription) ths_channel_link;
  struct th_channel *ths_channel;

  LIST_ENTRY(th_subscription) ths_transport_link;
  struct th_transport *ths_transport;   /* if NULL, ths_transport_link
					   is not linked */

  LIST_ENTRY(th_subscription) ths_subscriber_link; /* Caller is responsible
						      for this link */

  char *ths_title; /* display title */
  time_t ths_start;  /* time when subscription started */
  int ths_total_err; /* total errors during entire subscription */

  subscription_callback_t *ths_callback;
  void *ths_opaque;

  subscription_raw_input_t *ths_raw_input;

} th_subscription_t;



/*
 * EPG event
 */
typedef struct event {
  TAILQ_ENTRY(event) e_link;
  LIST_ENTRY(event) e_hash_link;

  time_t e_start;  /* UTC time */
  int e_duration;  /* in seconds */

  const char *e_title;   /* UTF-8 encoded */
  const char *e_desc;    /* UTF-8 encoded */

  uint16_t e_event_id;  /* DVB event id */

  uint32_t e_tag;

  th_channel_t *e_ch;

  int e_source; /* higer is better, and we never downgrade */

#define EVENT_SRC_XMLTV 1
#define EVENT_SRC_DVB   2

  refstr_t *e_icon;

} event_t;

config_entry_t *find_mux_config(const char *muxtype, const char *muxname);
char *utf8toprintable(const char *in);
char *utf8tofilename(const char *in);
const char *htstvstreamtype2txt(tv_streamtype_t s);
uint32_t tag_get(void);

#endif /* TV_HEAD_H */
