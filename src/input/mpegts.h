/*
 *  Tvheadend - TS file input system
 *
 *  Copyright (C) 2013 Adam Sutton
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

#ifndef __TVH_MPEGTS_H__
#define __TVH_MPEGTS_H__

#ifndef __TVH_INPUT_H__
#error "Use header file input.h not input/mpegts.h"
#endif

#include "atomic.h"
#include "tprofile.h"
#include "sbuf.h"
#include "input.h"
#include "service.h"
#include "mpegts/dvb.h"
#include "subscriptions.h"

#define MPEGTS_ONID_NONE        0x10000
#define MPEGTS_TSID_NONE        0x10000
#define MPEGTS_FULLMUX_PID      0x2000
#define MPEGTS_TABLES_PID       0x2001
#define MPEGTS_PID_NONE         0xFFFF

/* Types */
typedef struct mpegts_apid          mpegts_apid_t;
typedef struct mpegts_apids         mpegts_apids_t;
typedef struct mpegts_table         mpegts_table_t;
typedef struct mpegts_network       mpegts_network_t;
typedef struct mpegts_mux           mpegts_mux_t;
typedef struct mpegts_service       mpegts_service_t;
typedef struct mpegts_mux_instance  mpegts_mux_instance_t;
typedef struct mpegts_mux_sub       mpegts_mux_sub_t;
typedef struct mpegts_input         mpegts_input_t;
typedef struct mpegts_table_feed    mpegts_table_feed_t;
typedef struct mpegts_network_link  mpegts_network_link_t;
typedef struct mpegts_packet        mpegts_packet_t;
typedef struct mpegts_pcr           mpegts_pcr_t;
typedef struct mpegts_buffer        mpegts_buffer_t;

/* Lists */
typedef LIST_HEAD (,mpegts_network)             mpegts_network_list_t;
typedef LIST_HEAD (,mpegts_input)               mpegts_input_list_t;
typedef TAILQ_HEAD(mpegts_mux_queue,mpegts_mux) mpegts_mux_queue_t;
typedef LIST_HEAD (,mpegts_mux)                 mpegts_mux_list_t;
typedef LIST_HEAD (,mpegts_network_link)        mpegts_network_link_list_t;
typedef TAILQ_HEAD(mpegts_table_feed_queue, mpegts_table_feed)
                                                mpegts_table_feed_queue_t;

/* Classes */
extern const idclass_t mpegts_network_class;
extern const idclass_t mpegts_mux_class;
extern const idclass_t mpegts_mux_instance_class;
extern const idclass_t mpegts_service_class;
extern const idclass_t mpegts_service_raw_class;
extern const idclass_t mpegts_input_class;

/* **************************************************************************
 * Setup / Tear down
 * *************************************************************************/

void mpegts_init ( int linuxdvb_mask, int nosatip, str_list_t *satip_client,
                   str_list_t *tsfiles, int tstuners );
void mpegts_done ( void );

/* **************************************************************************
 * PIDs
 * *************************************************************************/

struct mpegts_apid {
  uint16_t pid;
  uint16_t weight;
};

struct mpegts_apids {
  mpegts_apid_t *pids;
  int alloc;
  int count;
  int all;
  int sorted;
};

int mpegts_pid_init ( mpegts_apids_t *pids );
void mpegts_pid_done ( mpegts_apids_t *pids );
mpegts_apids_t *mpegts_pid_alloc ( void );
void mpegts_pid_destroy ( mpegts_apids_t **pids );
void mpegts_pid_reset ( mpegts_apids_t *pids );
int mpegts_pid_add ( mpegts_apids_t *pids, uint16_t pid, uint16_t weight );
int mpegts_pid_add_group ( mpegts_apids_t *pids, mpegts_apids_t *vals );
int mpegts_pid_del ( mpegts_apids_t *pids, uint16_t pid, uint16_t weight );
int mpegts_pid_del_group ( mpegts_apids_t *pids, mpegts_apids_t *vals );
int mpegts_pid_find_windex ( mpegts_apids_t *pids, uint16_t pid, uint16_t weight );
int mpegts_pid_find_rindex ( mpegts_apids_t *pids, uint16_t pid );
static inline int mpegts_pid_wexists ( mpegts_apids_t *pids, uint16_t pid, uint16_t weight )
  { return pids && (pids->all || mpegts_pid_find_windex(pids, pid, weight) >= 0); }
static inline int mpegts_pid_rexists ( mpegts_apids_t *pids, uint16_t pid )
  { return pids && (pids->all || mpegts_pid_find_rindex(pids, pid) >= 0); }
int mpegts_pid_copy ( mpegts_apids_t *dst, mpegts_apids_t *src );
int mpegts_pid_cmp ( mpegts_apids_t *a, mpegts_apids_t *b );
int mpegts_pid_compare ( mpegts_apids_t *dst, mpegts_apids_t *src,
                         mpegts_apids_t *add, mpegts_apids_t *del );
int mpegts_pid_compare_weight ( mpegts_apids_t *dst, mpegts_apids_t *src,
                                mpegts_apids_t *add, mpegts_apids_t *del );
int mpegts_pid_weighted ( mpegts_apids_t *dst, mpegts_apids_t *src, int limit, int mweight );
int mpegts_pid_dump ( mpegts_apids_t *pids, char *buf, int len, int wflag, int raw );

/* **************************************************************************
 * Data / SI processing
 * *************************************************************************/

struct mpegts_packet
{
  TAILQ_ENTRY(mpegts_packet)  mp_link;
  size_t                      mp_len;
  mpegts_mux_t               *mp_mux;
  uint8_t                     mp_cc_restart;
  uint8_t                     mp_data[0];
};

struct mpegts_pcr {
  int64_t  pcr_first;
  int64_t  pcr_last;
  uint16_t pcr_pid;
};

#define MPEGTS_DATA_CC_RESTART		(1<<0)
#define MPEGTS_DATA_REMOVE_SCRAMBLED	(1<<1)

typedef int (*mpegts_table_callback_t)
  ( mpegts_table_t*, const uint8_t *buf, int len, int tableid );

struct mpegts_table_mux_cb
{
  int tag;
  int (*cb) ( mpegts_table_t*, mpegts_mux_t *mm, uint16_t nbid,
              const uint8_t dtag, const uint8_t *dptr, int dlen );
};

typedef struct mpegts_pid_sub
{
  RB_ENTRY(mpegts_pid_sub) mps_link;
  LIST_ENTRY(mpegts_pid_sub) mps_raw_link;
  LIST_ENTRY(mpegts_pid_sub) mps_svcraw_link;
#define MPS_NONE         0x00
#define MPS_ALL          0x01
#define MPS_RAW          0x02
#define MPS_STREAM       0x04
#define MPS_SERVICE      0x08
#define MPS_TABLE        0x10
#define MPS_FTABLE       0x20
#define MPS_TABLES       0x40
#define MPS_NOPOSTDEMUX  0x80
  int   mps_type;
#define MPS_WEIGHT_PAT       1000
#define MPS_WEIGHT_CAT        999
#define MPS_WEIGHT_SDT        999
#define MPS_WEIGHT_NIT        999
#define MPS_WEIGHT_BAT        999
#define MPS_WEIGHT_VCT        999
#define MPS_WEIGHT_EIT        999
#define MPS_WEIGHT_ETT        999
#define MPS_WEIGHT_MGT        999
#define MPS_WEIGHT_PMT        998
#define MPS_WEIGHT_PCR        997
#define MPS_WEIGHT_CA         996
#define MPS_WEIGHT_VIDEO      900
#define MPS_WEIGHT_AUDIO      800
#define MPS_WEIGHT_SUBTITLE   700
#define MPS_WEIGHT_ESOTHER    500
#define MPS_WEIGHT_RAW        400
#define MPS_WEIGHT_NIT2       300
#define MPS_WEIGHT_SDT2       300
#define MPS_WEIGHT_ALLLIMIT   200 /* values under this limit does not switch */
                                  /* input to the unfiltered PIDs (all) mode */
#define MPS_WEIGHT_TDT        102
#define MPS_WEIGHT_STT        102
#define MPS_WEIGHT_PMT_SCAN   101
#define MPS_WEIGHT_HBBTV_SCAN 100
  int   mps_weight;
  void *mps_owner;
} mpegts_pid_sub_t;

typedef struct mpegts_pid
{
  int                      mp_pid;
  int                      mp_type; // mask for all subscribers
  int8_t                   mp_cc;
  RB_HEAD(,mpegts_pid_sub) mp_subs; // subscribers to pid
  LIST_HEAD(,mpegts_pid_sub) mp_raw_subs;
  LIST_HEAD(,mpegts_pid_sub) mp_svc_subs;
  RB_ENTRY(mpegts_pid)     mp_link;
} mpegts_pid_t;

struct mpegts_table
{
  mpegts_psi_table_t;

  /**
   * Flags, must never be changed after creation.
   * We inspect it without holding global_lock
   */
  int mt_flags;

#define MT_CRC        0x0001
#define MT_FULL       0x0002
#define MT_QUICKREQ   0x0004
#define MT_FASTSWITCH 0x0008
#define MT_ONESHOT    0x0010
#define MT_RECORD     0x0020
#define MT_SKIPSUBS   0x0040
#define MT_SCANSUBS   0x0080
#define MT_FAST       0x0100
#define MT_SLOW       0x0200
#define MT_DEFER      0x0400

  /**
   * PID subscription weight
   */
  int mt_weight;

  /**
   * Cycle queue
   * Tables that did not get a fd or filter in hardware will end up here
   * waiting for any other table to be received so it can reuse that fd.
   * Only linked if fd == -1
   */
  TAILQ_ENTRY(mpegts_table) mt_pending_link;

  /**
   * File descriptor for filter
   */

  TAILQ_ENTRY(mpegts_table) mt_defer_link;
  mpegts_mux_t *mt_mux;

  void *mt_bat;
  mpegts_table_callback_t mt_callback;

  uint8_t mt_subscribed;
  uint8_t mt_defer_cmd;

#define MT_DEFER_OPEN_PID  1
#define MT_DEFER_CLOSE_PID 2

  int mt_working;

  int mt_count;

  int mt_id;
 
  int mt_destroyed; // Refcounting
  int mt_arefcount;
  uint32_t mt_priv;

  struct mpegts_table_mux_cb *mt_mux_cb;

  mpegts_service_t *mt_service;

  tprofile_t mt_profile;
  
  void (*mt_destroy) (mpegts_table_t *mt); // Allow customisable destroy hook
                                           // useful for dynamic allocation of
                                           // the opaque field
};

/**
 * When in raw mode we need to enqueue raw TS packet
 * to a different thread because we need to hold
 * global_lock when doing delivery of the tables
 */

struct mpegts_table_feed {
  TAILQ_ENTRY(mpegts_table_feed) mtf_link;
  uint8_t mtf_cc_restart;
  int mtf_len;
  mpegts_mux_t *mtf_mux;
  uint8_t mtf_tsb[0];
};

#define MPEGTS_MTF_ALLOC_CHUNK (21*188)

/* **************************************************************************
 * Logical network
 * *************************************************************************/

typedef enum {
  MN_DISCOVERY_DISABLE = 0,
  MN_DISCOVERY_NEW     = 1,
  MN_DISCOVERY_CHANGE  = 2
} mpegts_discovery_t;

/* Network/Input linkage */
struct mpegts_network_link
{
  int                             mnl_mark;
  mpegts_input_t                  *mnl_input;
  mpegts_network_t                *mnl_network;
  LIST_ENTRY(mpegts_network_link) mnl_mn_link;
  LIST_ENTRY(mpegts_network_link) mnl_mi_link;
};

/* Network */
struct mpegts_network
{
  idnode_t                   mn_id;
  LIST_ENTRY(mpegts_network) mn_global_link;

  /*
   * Identification
   */
  char                    *mn_network_name;
  char                    *mn_provider_network_name;
  int                      mn_wizard;
  uint8_t                  mn_wizard_free;

  /*
   * Inputs
   */
  mpegts_network_link_list_t   mn_inputs;

  /*
   * Multiplexes
   */
  mpegts_mux_list_t       mn_muxes;

  /*
   * Scanning
   */
  mpegts_mux_queue_t mn_scan_pend;    // Pending muxes
  mpegts_mux_queue_t mn_scan_ipend;   // Pending muxes (idle)
  mpegts_mux_queue_t mn_scan_active;  // Active muxes
  mtimer_t           mn_scan_timer;   // Timer for activity
  mtimer_t           mn_bouquet_timer;

  /*
   * Functions
   */
  void              (*mn_delete)       (mpegts_network_t*, int delconf);
  void              (*mn_display_name) (mpegts_network_t*, char *buf, size_t len);
  int               (*mn_bouquet_source) (mpegts_network_t*, char *buf, size_t len);
  int               (*mn_bouquet_comment) (mpegts_network_t*, char *buf, size_t len);
  htsmsg_t *        (*mn_config_save)  (mpegts_network_t*, char *filename, size_t fsize);
  mpegts_mux_t*     (*mn_create_mux)
    (mpegts_network_t*, void *origin, uint32_t onid, uint32_t tsid,
     void *conf, int force);
  mpegts_service_t* (*mn_create_service)
    (mpegts_mux_t*, uint16_t sid, uint16_t pmt_pid);
  const idclass_t*  (*mn_mux_class)   (mpegts_network_t*);
  mpegts_mux_t *    (*mn_mux_create2) (mpegts_network_t *mn, htsmsg_t *conf);
  void              (*mn_scan)        (mpegts_network_t*);

  /*
   * Configuration
   */
  int      mn_enabled;
  uint16_t mn_nid;
  uint16_t mn_satip_source;
  int      mn_autodiscovery;
  int      mn_skipinitscan;
  int      mn_bouquet;
  char    *mn_charset;
  int      mn_idlescan;
  int      mn_ignore_chnum;
  int      mn_sid_chnum;
  int      mn_localtime;
  int      mn_satpos;
};

typedef enum mpegts_mux_scan_state
{
  MM_SCAN_STATE_IDLE,     // Nothing
  MM_SCAN_STATE_PEND,     // Queue'd pending scan
  MM_SCAN_STATE_IPEND,    // Queue'd pending scan - idle queue
  MM_SCAN_STATE_ACTIVE,   // Scan is active
} mpegts_mux_scan_state_t;

typedef enum mpegts_mux_scan_result
{
  MM_SCAN_NONE,
  MM_SCAN_OK,
  MM_SCAN_FAIL,
  MM_SCAN_PARTIAL,
  MM_SCAN_IGNORE,
} mpegts_mux_scan_result_t;

#define MM_SCAN_CHECK_OK(mm) \
  ((mm)->mm_scan_result == MM_SCAN_OK || (mm)->mm_scan_result == MM_SCAN_PARTIAL)

enum mpegts_mux_enable
{
  MM_IGNORE  = -1,
  MM_DISABLE =  0,
  MM_ENABLE  =  1,
};

enum mpegts_mux_epg_flag
{
  MM_EPG_DISABLE,
  MM_EPG_ENABLE,
  MM_EPG_FORCE,
  MM_EPG_MANUAL = 100,
  MM_EPG_DETECTED
};

enum mpegts_mux_ac3_flag
{
  MM_AC3_STANDARD,
  MM_AC3_PMT_06,
  MM_AC3_PMT_N05,
};

/* Multiplex */
struct mpegts_mux
{
  idnode_t mm_id;
  int      mm_refcount;

  /*
   * Identification
   */
  
  LIST_ENTRY(mpegts_mux)  mm_network_link;
  mpegts_network_t       *mm_network;
  char                   *mm_nicename;
  char                   *mm_provider_network_name;
  uint32_t                mm_onid;
  uint32_t                mm_tsid;
  int                     mm_tsid_checks;
  int                     mm_tsid_accept_zero_value;
  tvhlog_limit_t          mm_tsid_loglimit;
  int64_t                 mm_start_monoclock;
  time_t                  mm_created;

  int                     mm_update_pids_flag;
  mtimer_t                mm_update_pids_timer;

  /*
   * Services
   */
  
  LIST_HEAD(,mpegts_service) mm_services;

  /*
   * Scanning
   */

  time_t                   mm_scan_first;   ///< Time for the first successful scan
  time_t                   mm_scan_last_seen; ///< Time for the last successful scan

  mpegts_mux_scan_result_t mm_scan_result;  ///< Result of last scan
  int                      mm_scan_weight;  ///< Scan priority
  int                      mm_scan_flags;   ///< Subscription flags
  int                      mm_scan_init;    ///< Flag to timeout handler
  mtimer_t                 mm_scan_timeout; ///< Timer to handle timeout
  TAILQ_ENTRY(mpegts_mux)  mm_scan_link;    ///< Link to Queue
  mpegts_mux_scan_state_t  mm_scan_state;   ///< Scanning state

  void                    *mm_dmc_origin;
  int64_t                  mm_dmc_origin_expire;

  char                    *mm_fastscan_muxes;

  /*
   * Physical instances
   */

  LIST_HEAD(, mpegts_mux_instance) mm_instances;
  mpegts_mux_instance_t      *mm_active;
  LIST_HEAD(,service)         mm_transports;

  /*
   * Raw subscriptions
   */

  LIST_HEAD(, th_subscription) mm_raw_subs;

  /*
   * Data processing
   */

  uint64_t                    mm_input_pos;
  RB_HEAD(, mpegts_pid)       mm_pids;
  LIST_HEAD(, mpegts_pid_sub) mm_all_subs;
  int                         mm_last_pid;
  mpegts_pid_t               *mm_last_mp;

  int                         mm_num_tables;
  LIST_HEAD(, mpegts_table)   mm_tables;
  TAILQ_HEAD(, mpegts_table)  mm_defer_tables;
  tvh_mutex_t                 mm_tables_lock;
  TAILQ_HEAD(, mpegts_table)  mm_table_queue;

  LIST_HEAD(, caid)           mm_descrambler_caids;
  TAILQ_HEAD(, descrambler_table) mm_descrambler_tables;
  TAILQ_HEAD(, descrambler_emm) mm_descrambler_emms;
  tvh_mutex_t                 mm_descrambler_lock;
  int                         mm_descrambler_flush;

  /*
   * Functions
   */

  void (*mm_delete)           (mpegts_mux_t *mm, int delconf);
  void (*mm_free)             (mpegts_mux_t *mm);
  htsmsg_t *(*mm_config_save) (mpegts_mux_t *mm, char *filename, size_t fsize);
  void (*mm_display_name)     (mpegts_mux_t*, char *buf, size_t len);
  int  (*mm_is_enabled)       (mpegts_mux_t *mm);
  void (*mm_stop)             (mpegts_mux_t *mm, int force, int reason);
  void (*mm_open_table)       (mpegts_mux_t*,mpegts_table_t*,int subscribe);
  void (*mm_unsubscribe_table)(mpegts_mux_t*,mpegts_table_t*);
  void (*mm_close_table)      (mpegts_mux_t*,mpegts_table_t*);
  void (*mm_create_instances) (mpegts_mux_t*);
  int  (*mm_is_epg)           (mpegts_mux_t*);

  /*
   * Configuration
   */
  char    *mm_crid_authority;
  int      mm_enabled;
  int      mm_epg;
  char    *mm_epg_module_id;
  char    *mm_charset;
  int      mm_pmt_ac3;
  int      mm_eit_tsid_nocheck;
  uint16_t mm_sid_filter;
};

#define PREFCAPID_OFF      0
#define PREFCAPID_ON       1
#define PREFCAPID_FORCE    2

#define SVC_PROCESS_SUBTITLE_NONE     0
#define SVC_PROCESS_SUBTITLE_DESC     1
#define SVC_PROCESS_SUBTITLE_APPEND   2
#define SVC_PROCESS_SUBTITLE_PREPEND  3

/* Service */
struct mpegts_service
{
  service_t; // Parent

  int (*s_update_pids)(mpegts_service_t *t, struct mpegts_apids *pids);
  int (*s_link)(mpegts_service_t *master, mpegts_service_t *slave);
  int (*s_unlink)(mpegts_service_t *master, mpegts_service_t *slave);

  int      s_dvb_subscription_flags;
  int      s_dvb_subscription_weight;

  mpegts_apids_t             *s_pids;
  idnode_set_t                s_masters;
  idnode_set_t                s_slaves;
  mpegts_apids_t             *s_slaves_pids;

  /*
   * Fields defined by DVB standard EN 300 468
   */

  uint32_t s_dvb_channel_num;
  uint16_t s_dvb_channel_minor;
  uint8_t  s_dvb_channel_dtag;
  char    *s_dvb_svcname;
  char    *s_dvb_provider;
  char    *s_dvb_cridauth;
  uint16_t s_dvb_servicetype;
  int      s_dvb_ignore_eit;
  int      s_dvb_subtitle_processing;       //Various options for replacing/augmenting the desc from the sub-title
  int      s_dvb_ignore_matching_subtitle;  //Ignore the sub-title if same as title
  char    *s_dvb_charset;
  uint16_t s_dvb_prefcapid;
  int      s_dvb_prefcapid_lock;
  time_t   s_dvb_created;
  time_t   s_dvb_last_seen;
  time_t   s_dvb_check_seen;

  /*
   * EIT/EPG control
   */

  int      s_dvb_eit_enable;
  uint64_t s_dvb_opentv_chnum;
  uint16_t s_dvb_opentv_id;
  uint16_t s_atsc_source_id;

  /*
   * Link to carrying multiplex and active adapter
   */

  LIST_ENTRY(mpegts_service) s_dvb_mux_link;
  mpegts_mux_t               *s_dvb_mux;
  mpegts_input_t             *s_dvb_active_input;

  /*
   * Streaming elements
   *
   * see service.h for locking rules
   */

  /**
   * When a subscription request SMT_MPEGTS, chunk them together
   * in order to reduce load.
   */
  sbuf_t s_tsbuf;
  int64_t s_tsbuf_last;

  /**
   * PCR drift compensation. This should really be per-packet.
   */
  int64_t  s_pcr_drift;

  /**
   * PMT/CAT monitoring
   */

  uint8_t s_cat_opened;
  mpegts_table_t *s_pmt_mon; ///< Table entry for monitoring PMT
  mpegts_table_t *s_cat_mon; ///< Table entry for monitoring CAT

};

/* **************************************************************************
 * Physical Network
 * *************************************************************************/

/* Physical mux instance */
struct mpegts_mux_instance
{
  tvh_input_instance_t;

  LIST_ENTRY(mpegts_mux_instance) mmi_mux_link;
  LIST_ENTRY(mpegts_mux_instance) mmi_active_link;

  streaming_pad_t mmi_streaming_pad;
  
  mpegts_mux_t   *mmi_mux;
  mpegts_input_t *mmi_input;

  int             mmi_start_weight;
  int             mmi_tune_failed;
};

struct mpegts_mux_sub
{
  RB_ENTRY(mpegts_mux_sub)  mms_link;
  void                     *mms_src;
  int                       mms_weight;
};

enum mpegts_input_is_enabled {
  MI_IS_ENABLED_RETRY = -1,
  MI_IS_ENABLED_NEVER = 0,
  MI_IS_ENABLED_OK = 1,
};

/* Input source */
struct mpegts_input
{
  tvh_input_t;

  int mi_enabled;

  int mi_instance;

  char *mi_name;

  int mi_priority;
  int mi_streaming_priority;

  int mi_ota_epg;

  int mi_initscan;
  int mi_idlescan;
  uint32_t mi_free_weight;

  char *mi_linked;

  LIST_ENTRY(mpegts_input) mi_global_link;

  mpegts_network_link_list_t mi_networks;

  LIST_HEAD(,tvh_input_instance) mi_mux_instances;


  /*
   * Status
   */
  mtimer_t mi_status_timer;

  /*
   * Input processing
   */

  int mi_running;            /* threads running */
  int64_t mi_last_dispatch;

  /* Data input */
  // Note: this section is protected by mi_input_lock
  pthread_t                       mi_input_tid;
  mtimer_t                        mi_input_thread_start;
  tvh_mutex_t                     mi_input_lock;
  tvh_cond_t                      mi_input_cond;
  TAILQ_HEAD(,mpegts_packet)      mi_input_queue;
  uint64_t                        mi_input_queue_size;
  tvhlog_limit_t                  mi_input_queue_loglimit;
  qprofile_t                      mi_qprofile;
  int                             mi_remove_scrambled_bits;

  /* Data processing/output */
  // Note: this lock (mi_output_lock) protects all the remaining
  //       data fields (excluding the callback functions)
  tvh_mutex_t                     mi_output_lock;

  /* Active sources */
  LIST_HEAD(,mpegts_mux_instance) mi_mux_active;

  /* Table processing */
  pthread_t                       mi_table_tid;
  tvh_cond_t                      mi_table_cond;
  mpegts_table_feed_queue_t       mi_table_queue;
  uint64_t                        mi_table_queue_size;
  tvhlog_limit_t                  mi_table_queue_loglimit;

  /* DBus */
#if ENABLE_DBUS_1
  int64_t                         mi_dbus_subs;
#endif

  /*
   * Functions
   */
  int  (*mi_is_enabled)     (mpegts_input_t*, mpegts_mux_t *mm, int flags, int weight);
  void (*mi_enabled_updated)(mpegts_input_t*);
  void (*mi_display_name)   (mpegts_input_t*, char *buf, size_t len);
  int  (*mi_get_weight)     (mpegts_input_t*, mpegts_mux_t *mm, int flags, int weight);
  int  (*mi_get_priority)   (mpegts_input_t*, mpegts_mux_t *mm, int flags);
  int  (*mi_get_grace)      (mpegts_input_t*, mpegts_mux_t *mm);
  int  (*mi_warm_mux)       (mpegts_input_t*,mpegts_mux_instance_t*);
  int  (*mi_start_mux)      (mpegts_input_t*,mpegts_mux_instance_t*, int weight);
  void (*mi_stop_mux)       (mpegts_input_t*,mpegts_mux_instance_t*);
  void (*mi_open_service)   (mpegts_input_t*,mpegts_service_t*, int flags, int first, int weight);
  void (*mi_close_service)  (mpegts_input_t*,mpegts_service_t*);
  void (*mi_update_pids)    (mpegts_input_t*,mpegts_mux_t*);
  void (*mi_create_mux_instance) (mpegts_input_t*,mpegts_mux_t*);
  void (*mi_started_mux)    (mpegts_input_t*,mpegts_mux_instance_t*);
  void (*mi_stopping_mux)   (mpegts_input_t*,mpegts_mux_instance_t*);
  void (*mi_stopped_mux)    (mpegts_input_t*,mpegts_mux_instance_t*);
  int  (*mi_has_subscription) (mpegts_input_t*, mpegts_mux_t *mm);
  void (*mi_error)          (mpegts_input_t*, mpegts_mux_t *, int tss_flags);
  void (*mi_empty_status)   (mpegts_input_t*, tvh_input_stream_t *);
  idnode_set_t *(*mi_network_list) (mpegts_input_t*);
};

/* ****************************************************************************
 * Lists
 * ***************************************************************************/

extern mpegts_network_list_t mpegts_network_all;

typedef struct mpegts_network_builder {
  LIST_ENTRY(mpegts_network_builder) link;
  const idclass_t     *idc;
  mpegts_network_t * (*build) ( const idclass_t *idc, htsmsg_t *conf );
} mpegts_network_builder_t;


typedef LIST_HEAD(,mpegts_network_builder) mpegts_network_builder_list_t;

extern mpegts_network_builder_list_t mpegts_network_builders;

extern mpegts_input_list_t mpegts_input_all;

/* ****************************************************************************
 * Functions
 * ***************************************************************************/

mpegts_input_t *mpegts_input_create0
  ( mpegts_input_t *mi, const idclass_t *idc, const char *uuid, htsmsg_t *c );

#define mpegts_input_create(t, u, c)\
  (struct t*)mpegts_input_create0(calloc(1, sizeof(struct t)), &t##_class, u, c)

#define mpegts_input_create1(u, c)\
  mpegts_input_create0(calloc(1, sizeof(mpegts_input_t)),\
                       &mpegts_input_class, u, c)

void mpegts_input_stop_all ( mpegts_input_t *mi );

void mpegts_input_delete ( mpegts_input_t *mi, int delconf );

static inline mpegts_input_t *mpegts_input_find(const char *uuid)
  { return idnode_find(uuid, &mpegts_input_class, NULL); }

int mpegts_input_set_networks ( mpegts_input_t *mi, htsmsg_t *msg );

int mpegts_input_add_network  ( mpegts_input_t *mi, mpegts_network_t *mn );

void mpegts_input_open_service ( mpegts_input_t *mi, mpegts_service_t *s, int flags, int init, int weight );
void mpegts_input_close_service ( mpegts_input_t *mi, mpegts_service_t *s );

void mpegts_input_status_timer ( void *p );

int mpegts_input_grace ( mpegts_input_t * mi, mpegts_mux_t * mm );

int mpegts_input_is_enabled ( mpegts_input_t * mi, mpegts_mux_t *mm, int flags, int weight );

void mpegts_input_set_enabled ( mpegts_input_t *mi, int enabled );

void mpegts_input_empty_status ( mpegts_input_t *mi, tvh_input_stream_t *st );


/* TODO: exposing these class methods here is a bit of a hack */
const void *mpegts_input_class_network_get  ( void *o );
int         mpegts_input_class_network_set  ( void *o, const void *p );
htsmsg_t   *mpegts_input_class_network_enum ( void *o, const char *lang );
char       *mpegts_input_class_network_rend ( void *o, const char *lang );
const void *mpegts_input_class_active_get   ( void *o );

int mpegts_mps_weight(elementary_stream_t *st);

int mpegts_mps_cmp( mpegts_pid_sub_t *a, mpegts_pid_sub_t *b );

void mpegts_network_register_builder
  ( const idclass_t *idc,
    mpegts_network_t *(*build)(const idclass_t *idc, htsmsg_t *conf) );

void mpegts_network_unregister_builder
  ( const idclass_t *idc );

mpegts_network_builder_t *mpegts_network_builder_find ( const char *clazz );

mpegts_network_t *mpegts_network_build
  ( const char *clazz, htsmsg_t *conf );
                 
mpegts_network_t *mpegts_network_create0
  ( mpegts_network_t *mn, const idclass_t *idc, const char *uuid,
    const char *name, htsmsg_t *conf );

#define mpegts_network_create(t, u, n, c)\
  (struct t*)mpegts_network_create0(calloc(1, sizeof(struct t)), &t##_class, u, n, c)

extern const idclass_t mpegts_network_class;

static inline mpegts_network_t *mpegts_network_find(const char *uuid)
  { return idnode_find(uuid, &mpegts_network_class, NULL); }

mpegts_mux_t *mpegts_network_find_mux
  (mpegts_network_t *mn, uint32_t onid, uint32_t tsid, int check);

mpegts_service_t *mpegts_network_find_active_service
  (mpegts_network_t *mn, uint16_t sid, mpegts_mux_t **rmm);

void mpegts_network_class_delete ( const idclass_t *idc, int delconf );

void mpegts_network_delete ( mpegts_network_t *mn, int delconf );

int mpegts_network_set_nid          ( mpegts_network_t *mn, uint16_t nid );
int mpegts_network_set_network_name ( mpegts_network_t *mn, const char *name );
void mpegts_network_scan ( mpegts_network_t *mn );
void mpegts_network_get_type_str( mpegts_network_t *mn, char *buf, size_t buflen );

void mpegts_network_bouquet_trigger0(mpegts_network_t *mn, int timeout);
static inline void mpegts_network_bouquet_trigger(mpegts_network_t *mn, int timeout)
{ if (mn->mn_bouquet) mpegts_network_bouquet_trigger0(mn, timeout); }

htsmsg_t * mpegts_network_wizard_get ( mpegts_input_t *mi, const idclass_t *idc,
                                       mpegts_network_t *mn, const char *lang );
void mpegts_network_wizard_create ( const char *clazz, htsmsg_t **nlist, const char *lang );

mpegts_mux_t *mpegts_mux_create0
  ( mpegts_mux_t *mm, const idclass_t *class, const char *uuid,
    mpegts_network_t *mn, uint32_t onid, uint32_t tsid,
    htsmsg_t *conf );

#define mpegts_mux_create(type, uuid, mn, onid, tsid, conf)\
  (struct type*)mpegts_mux_create0(calloc(1, sizeof(struct type)),\
                                   &type##_class, uuid,\
                                   mn, onid, tsid, conf)
#define mpegts_mux_create1(uuid, mn, onid, tsid, conf)\
  mpegts_mux_create0(calloc(1, sizeof(mpegts_mux_t)), &mpegts_mux_class, uuid,\
                     mn, onid, tsid, conf)

mpegts_mux_t *mpegts_mux_post_create(mpegts_mux_t *mm);

static inline mpegts_mux_t *mpegts_mux_find0(tvh_uuid_t *uuid)
  { return idnode_find0(uuid, &mpegts_mux_class, NULL); }

static inline mpegts_mux_t *mpegts_mux_find(const char *uuid)
  { return idnode_find(uuid, &mpegts_mux_class, NULL); }

#define mpegts_mux_delete_by_uuid(u, delconf)\
  { mpegts_mux_t *mm = mpegts_mux_find(u); if (mm) mm->mm_delete(mm, delconf); }

void mpegts_mux_delete ( mpegts_mux_t *mm, int delconf );

void mpegts_mux_free ( mpegts_mux_t *mm );

static inline void mpegts_mux_grab ( mpegts_mux_t *mm )
{
  int v = atomic_add(&mm->mm_refcount, 1);
  assert(v > 0);
}

static inline int mpegts_mux_release ( mpegts_mux_t *mm )
{
  int v = atomic_dec(&mm->mm_refcount, 1);
  assert(v > 0);
  if (v == 1) {
    mm->mm_free(mm);
    return 1;
  }
  return 0;
}

void mpegts_mux_save ( mpegts_mux_t *mm, htsmsg_t *c, int refs );

void mpegts_mux_tuning_error( const char *mux_uuid, mpegts_mux_instance_t *mmi_match );

mpegts_mux_instance_t *mpegts_mux_instance_create0
  ( mpegts_mux_instance_t *mmi, const idclass_t *class, const char *uuid,
    mpegts_input_t *mi, mpegts_mux_t *mm );

mpegts_service_t *mpegts_mux_find_service(mpegts_mux_t *ms, uint16_t sid);

#define mpegts_mux_instance_create(type, uuid, mi, mm)\
  (struct type*)mpegts_mux_instance_create0(calloc(1, sizeof(struct type)),\
                                            &type##_class, uuid,\
                                            mi, mm);

void mpegts_mux_instance_delete ( tvh_input_instance_t *tii );

int mpegts_mux_instance_start
  ( mpegts_mux_instance_t **mmiptr, service_t *t, int weight );

int mpegts_mux_instance_weight ( mpegts_mux_instance_t *mmi );

int mpegts_mux_set_network_name ( mpegts_mux_t *mm, const char *name );
int mpegts_mux_set_tsid ( mpegts_mux_t *mm, uint32_t tsid, int force );
int mpegts_mux_set_onid ( mpegts_mux_t *mm, uint32_t onid );
int mpegts_mux_set_crid_authority ( mpegts_mux_t *mm, const char *defauth );
int mpegts_mux_set_epg_module ( mpegts_mux_t *mm, const char *modid );

void mpegts_mux_open_table ( mpegts_mux_t *mm, mpegts_table_t *mt, int subscribe );
void mpegts_mux_unsubscribe_table ( mpegts_mux_t *mm, mpegts_table_t *mt );
void mpegts_mux_close_table ( mpegts_mux_t *mm, mpegts_table_t *mt );

void mpegts_mux_remove_subscriber(mpegts_mux_t *mm, th_subscription_t *s, int reason);
int  mpegts_mux_subscribe(mpegts_mux_t *mm, mpegts_input_t *mi,
                          const char *name, int weight, int flags);
void mpegts_mux_unsubscribe_by_name(mpegts_mux_t *mm, const char *name);
th_subscription_t *mpegts_mux_find_subscription_by_name(mpegts_mux_t *mm, const char *name);

void mpegts_mux_unsubscribe_linked(mpegts_input_t *mi, service_t *t);

void mpegts_mux_scan_done ( mpegts_mux_t *mm, const char *buf, int res );

void mpegts_mux_bouquet_rescan ( const char *src, const char *extra );

void mpegts_mux_nice_name( mpegts_mux_t *mm, char *buf, size_t len );
void mpegts_mux_update_nice_name( mpegts_mux_t *mm );

int mpegts_mux_class_scan_state_set ( void *, const void * );

static inline int mpegts_mux_scan_state_set ( mpegts_mux_t *m, int state )
  { return mpegts_mux_class_scan_state_set ( m, &state ); }

mpegts_pid_t *mpegts_mux_find_pid_(mpegts_mux_t *mm, int pid, int create);

static inline mpegts_pid_t *
mpegts_mux_find_pid(mpegts_mux_t *mm, int pid, int create)
{
  if (mm->mm_last_pid != pid)
    return mpegts_mux_find_pid_(mm, pid, create);
  else
    return mm->mm_last_mp;
}

void mpegts_mux_update_pids ( mpegts_mux_t *mm );

void mpegts_input_create_mux_instance ( mpegts_input_t *mi, mpegts_mux_t *mm );

int mpegts_mux_compare ( mpegts_mux_t *a, mpegts_mux_t *b );

void mpegts_input_recv_packets
  (mpegts_mux_instance_t *mmi, sbuf_t *sb,
   int flags, mpegts_pcr_t *pcr);

void mpegts_input_postdemux
  ( mpegts_input_t *mi, mpegts_mux_t *mm, uint8_t *data, int len );

int mpegts_input_get_weight ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags, int weight );
int mpegts_input_get_priority ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags );
int mpegts_input_get_grace ( mpegts_input_t *mi, mpegts_mux_t *mm );
int mpegts_input_warm_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi );

void mpegts_input_save ( mpegts_input_t *mi, htsmsg_t *c );

void mpegts_input_flush_mux ( mpegts_input_t *mi, mpegts_mux_t *mm );

mpegts_pid_t * mpegts_input_open_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, int weight,
    void *owner, int reopen );

int mpegts_input_close_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner );

mpegts_pid_t * mpegts_input_update_pid_weight
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, int weight,
    void *owner );

void mpegts_input_close_pids
  ( mpegts_input_t *mi, mpegts_mux_t *mm, void *owner, int all );

elementary_stream_t *mpegts_input_open_service_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm, service_t *s,
    streaming_component_type_t stype, int pid, int weight, int create );

void mpegts_input_open_pmt_monitor ( mpegts_mux_t *mm, mpegts_service_t *s );
void mpegts_input_open_cat_monitor ( mpegts_mux_t *mm, mpegts_service_t *s );

void tsdebug_encode_keys
  ( uint8_t *dst, uint16_t sid, uint16_t pid,
    uint8_t keytype, uint8_t keylen, uint8_t *even, uint8_t *odd );

void tsdebug_check_tspkt( mpegts_mux_t *mm, uint8_t *pkt, int len );

void mpegts_table_dispatch(const uint8_t *sec, size_t r, void *mt);
static inline void mpegts_table_grab(mpegts_table_t *mt)
{
  int v = atomic_add(&mt->mt_arefcount, 1);
  assert(v > 0);
}
void mpegts_table_release_(mpegts_table_t *mt);
static inline int mpegts_table_release(mpegts_table_t *mt)
{
  int v = atomic_dec(&mt->mt_arefcount, 1);
  assert(v > 0);
  if (v == 1) {
    assert(mt->mt_destroyed == 1);
    mpegts_table_release_(mt);
    return 1;
  }
  return 0;
}
int mpegts_table_type(mpegts_table_t *mt);
mpegts_table_t *mpegts_table_add
  (mpegts_mux_t *mm, int tableid, int mask,
   mpegts_table_callback_t callback, void *opaque,
   const char *name, int subsys, int flags, int pid, int weight);
mpegts_table_t *mpegts_table_find
  (mpegts_mux_t *mm, const char *name, void *opaque);
void mpegts_table_flush_all(mpegts_mux_t *mm);
void mpegts_table_destroy(mpegts_table_t *mt);
static inline void mpegts_table_reset(mpegts_table_t *mt)
  { dvb_table_reset((mpegts_psi_table_t *)mt); }
void mpegts_table_consistency_check(mpegts_mux_t *mm);

void dvb_bat_destroy(struct mpegts_table *mt);

void dvb_cat_decode( const uint8_t *data, int len,
                     void (*add_emm)(void *aux, uint16_t caid, uint32_t prov, uint16_t pid),
                     void *aux );

int dvb_pat_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);
int dvb_cat_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);
int dvb_pmt_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tabelid);
int dvb_nit_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);
int dvb_bat_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);
int dvb_fs_sdt_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);
int dvb_sdt_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);
int dvb_tdt_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);
int dvb_tot_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);
int atsc_vct_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);
int atsc_stt_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);

void psi_tables_install
  (mpegts_input_t *mi, mpegts_mux_t *mm, dvb_fe_delivery_system_t delsys);

mpegts_service_t *mpegts_service_create0
  ( mpegts_service_t *ms, const idclass_t *class, const char *uuid,
    mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid, htsmsg_t *conf );

#define mpegts_service_create(t, u, m, s, p, c)\
  (struct t*)mpegts_service_create0(calloc(1, sizeof(struct t)),\
                                    &t##_class, u, m, s, p, c)

#define mpegts_service_create1(u, m, s, p, c)\
  mpegts_service_create0(calloc(1, sizeof(mpegts_service_t)),\
                         &mpegts_service_class, u, m, s, p, c)

mpegts_service_t *mpegts_service_create_raw(mpegts_mux_t *mm);

mpegts_service_t *mpegts_service_find 
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid, int create, int *save );

service_t *
mpegts_service_find_e2
  ( uint32_t stype, uint32_t sid, uint32_t tsid, uint32_t onid, uint32_t hash);

mpegts_service_t *
mpegts_service_find_by_pid ( mpegts_mux_t *mm, int pid );

void mpegts_service_autoenable( mpegts_service_t *s, const char *where );

void mpegts_service_update_slave_pids
  ( mpegts_service_t *t, mpegts_service_t *master_filter, int del );

static inline mpegts_service_t *mpegts_service_find_by_uuid0(tvh_uuid_t *uuid)
  { return idnode_find0(uuid, &mpegts_service_class, NULL); }

static inline mpegts_service_t *mpegts_service_find_by_uuid(const char *uuid)
  { return idnode_find(uuid, &mpegts_service_class, NULL); }

void mpegts_service_unref ( service_t *s );

void mpegts_service_delete ( service_t *s, int delconf );

int64_t mpegts_service_channel_number ( service_t *s );

/*
 * MPEG-TS event handler
 */

typedef struct mpegts_listener
{
  LIST_ENTRY(mpegts_listener) ml_link;
  void *ml_opaque;
  void (*ml_mux_start)  (mpegts_mux_t *mm, void *p);
  void (*ml_mux_stop)   (mpegts_mux_t *mm, void *p, int reason);
  void (*ml_mux_create) (mpegts_mux_t *mm, void *p);
  void (*ml_mux_delete) (mpegts_mux_t *mm, void *p);
} mpegts_listener_t;

extern LIST_HEAD(mpegts_listeners, mpegts_listener) mpegts_listeners;

#define mpegts_add_listener(ml)\
  LIST_INSERT_HEAD(&mpegts_listeners, ml, ml_link)

#define mpegts_rem_listener(ml)\
  LIST_REMOVE(ml, ml_link)

#define mpegts_fire_event(t, op)\
{\
  mpegts_listener_t *ml;\
  LIST_FOREACH(ml, &mpegts_listeners, ml_link)\
    if (ml->op) ml->op(t, ml->ml_opaque);\
} (void)0

#define mpegts_fire_event1(t, op, arg1)\
{\
  mpegts_listener_t *ml;\
  LIST_FOREACH(ml, &mpegts_listeners, ml_link)\
    if (ml->op) ml->op(t, ml->ml_opaque, arg1);\
} (void)0

/*
 * misc
 */
void eit_nit_callback(mpegts_table_t *mt, uint16_t nbid, const char *name, uint32_t priv);
void eit_sdt_callback(mpegts_table_t *mt, uint32_t priv);

#endif /* __TVH_MPEGTS_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
