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


LIST_HEAD(th_subscription_list, th_subscription);
TAILQ_HEAD(th_channel_queue, th_channel);
LIST_HEAD(th_dvb_adapter_list, th_dvb_adapter);
LIST_HEAD(th_v4l_adapter_list, th_v4l_adapter);
LIST_HEAD(client_list, client);
LIST_HEAD(programme_list, programme);
LIST_HEAD(pvr_rec_list, pvr_rec);
TAILQ_HEAD(ref_update_queue, ref_update);
LIST_HEAD(th_transport_list, th_transport);

extern time_t dispatch_clock;

extern int reftally;

typedef struct th_v4l_adapter {

  const char *tva_name;

  LIST_ENTRY(th_v4l_adapter) tva_link;

  const char *tva_path;

  pthread_t tva_ptid;

  struct th_transport_list tva_transports;

  int tva_frequency;
  int tva_running;

  pthread_cond_t tva_run_cond;

} th_v4l_adapter_t;


typedef struct th_dvb_adapter {

  const char *tda_path;
  const char *tda_name;

  LIST_ENTRY(th_dvb_adapter) tda_link;

  int tda_running;

  struct dvb_frontend_parameters tda_fe_params;

  struct th_transport_list tda_transports;

  int tda_fe_fd;
  struct dvb_frontend_info tda_fe_info;

  fe_status_t tda_fe_status;
  uint16_t tda_snr, tda_signal;
  uint32_t tda_ber, tda_uncorrected_blocks;

  char *tda_demux_path;

  int tda_dvr_fd;
  char *tda_dvr_path;

  time_t tda_time;

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



typedef struct pidinfo {
  int pid;
  tv_streamtype_t type;
  int demuxer_fd;

  int cc;             /* Last CC */
  int cc_valid;       /* Is CC valid at all? */

} pidinfo_t;

typedef enum {
  COMMERCIAL_UNKNOWN,
  COMMERCIAL_YES,
  COMMERCIAL_NO,
} th_commercial_advice_t;


typedef struct th_transport {
  
  const char *tht_name;

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
  
  pidinfo_t *tht_pids;
  int tht_npids;

  avgstat_t tht_cc_errors;
  avgstat_t tht_rate;
  int tht_monitor_suspend;

  LIST_ENTRY(th_transport) tht_adapter_link;

  LIST_ENTRY(th_transport) tht_channel_link;
  struct th_channel *tht_channel;


  LIST_HEAD(, th_subscription) tht_subscriptions;

  union {

    struct {
      struct dvb_frontend_parameters fe_params;
      struct th_dvb_adapter *adapter;
    } dvb;

    struct {
      int frequency;
      struct th_v4l_adapter *adapter;
    } v4l;
    


    struct {

      struct in_addr group_addr;
      struct in_addr interface_addr;
      int port;
      int fd;
      void *dispatch_handle;

    } iptv;

  } u;


} th_transport_t;

#define tht_v4l_frequency u.v4l.frequency
#define tht_v4l_adapter   u.v4l.adapter

#define tht_dvb_fe_params u.dvb.fe_params
#define tht_dvb_adapter   u.dvb.adapter

#define tht_iptv_group_addr      u.iptv.group_addr
#define tht_iptv_interface_addr  u.iptv.interface_addr
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


typedef struct th_proglist {

  struct programme_list tpl_programs;   // linked list of all programs
  unsigned int tpl_nprograms;           // number of programs
  struct programme **tpl_prog_vec;      // array pointing to all programs
  struct programme *tpl_prog_current;   // pointer to current programme
  const char *tpl_refname;              // channel reference name

} th_proglist_t;

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
  const char *ch_icon;

  struct pvr_rec *ch_rec;

  struct tt_decoder ch_tt;

  int ch_ref;

  int ch_teletext_rundown;



  pthread_mutex_t ch_epg_mutex; ///< protects the epg fields

  th_proglist_t ch_xmltv;

} th_channel_t;

/*
 * XXXX: DELETE ME?
 */

typedef struct ref_update {
  TAILQ_ENTRY(ref_update) link;
  int ref;
} ref_update_t;

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

  void (*ths_callback)(struct th_subscription *s, uint8_t *pkt, pidinfo_t *pi,
		       int streamindex);
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
 * EPG Programme
 */

typedef struct programme {
  LIST_ENTRY(programme) pr_link;

  time_t pr_start;
  time_t pr_stop;

  char *pr_title;
  char *pr_desc;

  th_channel_t *pr_ch;

  int pr_ref;
  int pr_index;

  int pr_delete_me;

} programme_t;


/*
 * PVR
 */


typedef struct pvr_data {
  TAILQ_ENTRY(pvr_data) link;
  uint8_t *tsb;
  pidinfo_t pi;
  int streamindex;
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

#endif /* TV_HEAD_H */
