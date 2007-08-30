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

LIST_HEAD(th_subscription_list, th_subscription);
TAILQ_HEAD(th_channel_queue, th_channel);
LIST_HEAD(th_dvb_adapter_list, th_dvb_adapter);
LIST_HEAD(th_v4l_adapter_list, th_v4l_adapter);
LIST_HEAD(client_list, client);
LIST_HEAD(event_list, event);
TAILQ_HEAD(event_queue, event);
LIST_HEAD(pvr_rec_list, pvr_rec);
TAILQ_HEAD(ref_update_queue, ref_update);
LIST_HEAD(th_transport_list, th_transport);
LIST_HEAD(th_dvb_mux_list, th_dvb_mux);
LIST_HEAD(th_dvb_mux_instance_list, th_dvb_mux_instance);
LIST_HEAD(th_pid_list, th_pid);

extern time_t dispatch_clock;
extern struct th_transport_list all_transports;

uint32_t tag_get(void);

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

  struct {
    void *tva_pes_packet;
    uint16_t tva_pes_packet_len;
    uint16_t tva_pes_packet_pos;

    void *tva_parser;
    void *tva_ctx;
    int tva_cc;
  } tva_streams[2];


} th_v4l_adapter_t;


/*
 *
 */

typedef enum {
    TDMI_CONFIGURED,
    TDMI_INITIAL_SCAN,
    TDMI_IDLE,
    TDMI_RUNNING,
} tdmi_state_t;

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

  void *tdmi_initial_scan_timer;
  const char *tdmi_status;

  time_t tdmi_got_adapter;
  time_t tdmi_lost_adapter;

} th_dvb_mux_instance_t;

/*
 *
 */

typedef struct th_dvb_table {
  LIST_ENTRY(th_dvb_table) tdt_link;
  void *tdt_handle;
  struct th_dvb_mux_instance *tdt_tdmi;
  int tdt_count; /* times seen */
  void *tdt_opaque;
  int (*tdt_callback)(th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len,
		      uint8_t tableid, void *opaque);
} th_dvb_table_t;


/*
 *
 */

typedef struct th_dvb_mux {
  LIST_ENTRY(th_dvb_mux) tdm_global_link;

  struct th_dvb_mux_instance_list tdm_instances;

  struct dvb_frontend_parameters tdm_fe_params;

  const char *tdm_name;
  const char *tdm_title;

} th_dvb_mux_t;


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

} th_dvb_adapter_t;





/*
 *
 *
 */


typedef struct th_iptv_adapter {
  

} th_iptv_adapter_t;

/*
 *
 *
 */



typedef struct th_pid {
  LIST_ENTRY(th_pid) tp_link;
  uint16_t tp_pid;
  uint8_t tp_cc;             /* Last CC */
  uint8_t tp_cc_valid;       /* Is CC valid at all? */

  tv_streamtype_t tp_type;
  int tp_demuxer_fd;
  int tp_index;

} th_pid_t;

typedef enum {
  COMMERCIAL_UNKNOWN,
  COMMERCIAL_YES,
  COMMERCIAL_NO,
} th_commercial_advice_t;


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

  } tht_status;

  th_commercial_advice_t tht_tt_commercial_advice;

  int tht_tt_rundown_content_length;
  time_t tht_tt_clock;   /* Network clock as determined by teletext
			    decoder */
  int tht_prio;
  
  struct th_pid_list tht_pids;

  uint16_t tht_dvb_network_id;
  uint16_t tht_dvb_transport_id;
  uint16_t tht_dvb_service_id;

  avgstat_t tht_cc_errors;
  avgstat_t tht_rate;
  int tht_monitor_suspend;

  int tht_cc_error_log_limiter;
  int tht_rate_error_log_limiter;


  LIST_ENTRY(th_transport) tht_adapter_link;

  LIST_ENTRY(th_transport) tht_channel_link;
  struct th_channel *tht_channel;

  LIST_HEAD(, th_subscription) tht_subscriptions;

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

/*
 *  Teletext
 *
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

extern struct th_channel_queue channels;

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

extern struct th_subscription_list subscriptions;

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

  void (*ths_callback)(struct th_subscription *s, uint8_t *pkt, th_pid_t *pi);

  void *ths_opaque;

  char *ths_pkt;
  int ths_pkt_ptr;

  char *ths_title; /* display title */

  time_t ths_start;  /* time when subscription started */
  
  int ths_total_err; /* total errors during entire subscription */

} th_subscription_t;


/*
 * Client
 */

typedef struct client {

  LIST_ENTRY(client) c_global_link;
  int c_fd;
  int c_streamfd;
  pthread_t c_ptid;
  
  LIST_HEAD(, th_subscription) c_subscriptions;

  struct in_addr c_ipaddr;
  int c_port;

  struct ref_update_queue c_refq;

  int c_pkt_maxsiz;

  char c_input_buf[100];
  int c_input_buf_ptr;

  char *c_title;

  void *c_dispatch_handle;

} client_t;



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


/*
 * PVR
 */


typedef struct pvr_data {
  TAILQ_ENTRY(pvr_data) link;
  uint8_t *tsb;
  th_pid_t pi;
} pvr_data_t;


typedef struct pvr_rec {

  LIST_ENTRY(pvr_rec) pvrr_global_link;
  LIST_ENTRY(pvr_rec) pvrr_work_link;

  th_channel_t *pvrr_channel;

  time_t pvrr_start;
  time_t pvrr_stop;

  char *pvrr_filename;       /* May be null if we havent figured out a name
				yet, this happens upon record start.
				Notice that this is full path */
  char *pvrr_title;          /* Title in UTF-8 */
  char *pvrr_desc;           /* Description in UTF-8 */

  char *pvrr_printname;      /* Only ASCII chars, used for logging and such */
  char *pvrr_format;         /* File format trailer */

  char pvrr_status;          /* defined in libhts/htstv.h */

  TAILQ_HEAD(, pvr_data) pvrr_dq;
  int pvrr_dq_len;

  pthread_mutex_t pvrr_dq_mutex;
  pthread_cond_t pvrr_dq_cond;

  int pvrr_ref;

  void *pvrr_opaque;  /* For write out code */

} pvr_rec_t;

#define PVRR_WORK_SCHEDULED 0
#define PVRR_WORK_RECORDING 1
#define PVRR_WORK_DONE      2
#define PVRR_WORK_MAX       3

extern struct pvr_rec_list pvrr_work_list[PVRR_WORK_MAX];
extern struct pvr_rec_list pvrr_global_list;

config_entry_t *find_mux_config(const char *muxtype, const char *muxname);

char *utf8toprintable(const char *in);
char *utf8tofilename(const char *in);

#endif /* TV_HEAD_H */
