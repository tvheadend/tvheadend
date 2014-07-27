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

#include "input.h"
#include "service.h"
#include "mpegts/dvb.h"
#include "subscriptions.h"

#define MPEGTS_ONID_NONE        0xFFFF
#define MPEGTS_TSID_NONE        0xFFFF
#define MPEGTS_PSI_SECTION_SIZE 5000
#define MPEGTS_FULLMUX_PID      0x2000
#define MPEGTS_PID_NONE         0xFFFF

/* Types */
typedef struct mpegts_table         mpegts_table_t;
typedef struct mpegts_psi_section   mpegts_psi_section_t;
typedef struct mpegts_network       mpegts_network_t;
typedef struct mpegts_mux           mpegts_mux_t;
typedef struct mpegts_service       mpegts_service_t;
typedef struct mpegts_mux_instance  mpegts_mux_instance_t;
typedef struct mpegts_mux_sub       mpegts_mux_sub_t;
typedef struct mpegts_input         mpegts_input_t;
typedef struct mpegts_table_feed    mpegts_table_feed_t;
typedef struct mpegts_network_link  mpegts_network_link_t;
typedef struct mpegts_packet        mpegts_packet_t;
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
extern const idclass_t mpegts_service_class;
extern const idclass_t mpegts_input_class;

/* **************************************************************************
 * Setup / Tear down
 * *************************************************************************/

void mpegts_init ( int linuxdvb_mask, str_list_t *satip_client,
                   str_list_t *tsfiles, int tstuners );
void mpegts_done ( void );

/* **************************************************************************
 * Data / SI processing
 * *************************************************************************/

struct mpegts_packet
{
  TAILQ_ENTRY(mpegts_packet)  mp_link;
  size_t                      mp_len;
  mpegts_mux_t               *mp_mux;
  uint8_t                     mp_data[0];
};

typedef int (*mpegts_table_callback_t)
  ( mpegts_table_t*, const uint8_t *buf, int len, int tableid );

typedef void (*mpegts_psi_section_callback_t)
  ( const uint8_t *tsb, size_t len, void *opaque );

struct mpegts_table_mux_cb
{
  int tag;
  int (*cb) ( mpegts_table_t*, mpegts_mux_t *mm,
              const uint8_t dtag, const uint8_t *dptr, int dlen );
};

struct mpegts_psi_section
{
  int     ps_offset;
  int     ps_lock;
  uint8_t ps_data[MPEGTS_PSI_SECTION_SIZE];
};

typedef struct mpegts_table_state
{
  int      tableid;
  uint64_t extraid;
  int      version;
  int      complete;
  uint32_t sections[8];
  RB_ENTRY(mpegts_table_state)   link;
} mpegts_table_state_t;

typedef struct mpegts_pid_sub
{
  RB_ENTRY(mpegts_pid_sub) mps_link;
#define MPS_NONE   0x0
#define MPS_STREAM 0x1
#define MPS_TABLE  0x2
#define MPS_FTABLE 0x4
  int                       mps_type;
  void                     *mps_owner;
} mpegts_pid_sub_t;

typedef struct mpegts_pid
{
  int                      mp_pid;
  int                      mp_fd;   // linuxdvb demux fd
  int8_t                   mp_cc;
  RB_HEAD(,mpegts_pid_sub) mp_subs; // subscribers to pid
  RB_ENTRY(mpegts_pid)     mp_link;
} mpegts_pid_t;

struct mpegts_table
{
  /**
   * Flags, must never be changed after creation.
   * We inspect it without holding global_lock
   */
  int mt_flags;

#define MT_CRC      0x0001
#define MT_FULL     0x0002
#define MT_QUICKREQ 0x0004
#define MT_RECORD   0x0008
#define MT_SKIPSUBS 0x0010
#define MT_SCANSUBS 0x0020
#define MT_FAST     0x0040
#define MT_SLOW     0x0080
#define MT_DEFER    0x0100

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

  LIST_ENTRY(mpegts_table) mt_link;
  LIST_ENTRY(mpegts_table) mt_defer_link;
  mpegts_mux_t *mt_mux;

  char *mt_name;

  void *mt_opaque;
  mpegts_table_callback_t mt_callback;

  RB_HEAD(,mpegts_table_state) mt_state;
  int mt_complete;
  int mt_incomplete;
  uint8_t mt_finished;
  uint8_t mt_subscribed;
  uint8_t mt_defer_cmd;

#define MT_DEFER_OPEN_PID  1
#define MT_DEFER_CLOSE_PID 2

  int mt_count;

  int mt_pid;

  int mt_id;
 
  int mt_table; // SI table id (base)
  int mt_mask;  //              mask

  int mt_destroyed; // Refcounting
  int mt_refcount;

  int8_t mt_cc;

  mpegts_psi_section_t mt_sect;

  struct mpegts_table_mux_cb *mt_mux_cb;

  mpegts_service_t *mt_service;
  
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
  uint8_t mtf_tsb[188];
  mpegts_mux_t *mtf_mux;
};

/*
 * Assemble SI section
 */
void mpegts_psi_section_reassemble
  ( mpegts_psi_section_t *ps, const uint8_t *tsb, int crc, int ccerr,
    mpegts_psi_section_callback_t cb, void *opaque );

/* **************************************************************************
 * Logical network
 * *************************************************************************/

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
  mpegts_mux_queue_t mn_scan_active;  // Active muxes
  gtimer_t           mn_scan_timer;   // Timer for activity

  /*
   * Functions
   */
  void              (*mn_display_name) (mpegts_network_t*, char *buf, size_t len);
  void              (*mn_config_save)  (mpegts_network_t*);
  mpegts_mux_t*     (*mn_create_mux)
    (mpegts_mux_t*, uint16_t onid, uint16_t tsid, void *conf);
  mpegts_service_t* (*mn_create_service)
    (mpegts_mux_t*, uint16_t sid, uint16_t pmt_pid);
  const idclass_t*  (*mn_mux_class)   (mpegts_network_t*);
  mpegts_mux_t *    (*mn_mux_create2) (mpegts_network_t *mn, htsmsg_t *conf);

  /*
   * Configuration
   */
  uint16_t mn_nid;
  int      mn_autodiscovery;
  int      mn_skipinitscan;
  char    *mn_charset;
  int      mn_idlescan;
};

typedef enum mpegts_mux_scan_state
{
  MM_SCAN_STATE_IDLE,     // Nothing
  MM_SCAN_STATE_PEND,     // Queue'd pending scan
  MM_SCAN_STATE_ACTIVE,   // Scan is active
} mpegts_mux_scan_state_t;

typedef enum mpegts_mux_scan_result
{
  MM_SCAN_NONE,
  MM_SCAN_OK,
  MM_SCAN_FAIL
} mpegts_mux_scan_result_t;

enum mpegts_mux_epg_flag
{
  MM_EPG_DISABLE,
  MM_EPG_ENABLE,
  MM_EPG_FORCE,
  MM_EPG_FORCE_EIT,
  MM_EPG_FORCE_UK_FREESAT,
  MM_EPG_FORCE_UK_FREEVIEW,
  MM_EPG_FORCE_VIASAT_BALTIC,
  MM_EPG_FORCE_OPENTV_SKY_UK,
  MM_EPG_FORCE_OPENTV_SKY_ITALIA,
  MM_EPG_FORCE_OPENTV_SKY_AUSAT,
};
#define MM_EPG_LAST MM_EPG_FORCE_OPENTV_SKY_AUSAT

/* Multiplex */
struct mpegts_mux
{
  idnode_t mm_id;

  /*
   * Identification
   */
  
  LIST_ENTRY(mpegts_mux)  mm_network_link;
  mpegts_network_t        *mm_network;
  uint16_t                mm_onid;
  uint16_t                mm_tsid;

  /*
   * Services
   */
  
  LIST_HEAD(,mpegts_service) mm_services;

  /*
   * Scanning
   */

  mpegts_mux_scan_result_t mm_scan_result;  ///< Result of last scan
  int                      mm_scan_weight;  ///< Scan priority
  int                      mm_scan_init;    ///< Flag to timeout handler
  gtimer_t                 mm_scan_timeout; ///< Timer to handle timeout
  TAILQ_ENTRY(mpegts_mux)  mm_scan_link;    ///< Link to Queue
  mpegts_mux_scan_state_t  mm_scan_state;   ///< Scanning state

#if 0
  enum {
    MM_ORIG_USER, ///< Manually added
    MM_ORIG_FILE, ///< Added from scan file
    MM_ORIG_AUTO  ///< From NIT
  }                        mm_dmc_origin2;
#endif
  mpegts_mux_t            *mm_dmc_origin;
  time_t                   mm_dmc_origin_expire;

  /*
   * Physical instances
   */

  LIST_HEAD(, mpegts_mux_instance) mm_instances;
  mpegts_mux_instance_t *mm_active;

  /*
   * Data processing
   */

  RB_HEAD(, mpegts_pid)       mm_pids;
  int                         mm_last_pid;
  mpegts_pid_t               *mm_last_mp;

  int                         mm_num_tables;
  LIST_HEAD(, mpegts_table)   mm_tables;
  LIST_HEAD(, mpegts_table)   mm_defer_tables;
  pthread_mutex_t             mm_tables_lock;
  TAILQ_HEAD(, mpegts_table)  mm_table_queue;

  LIST_HEAD(, caid)           mm_descrambler_caids;
  TAILQ_HEAD(, descrambler_table) mm_descrambler_tables;
  TAILQ_HEAD(, descrambler_emm) mm_descrambler_emms;
  pthread_mutex_t             mm_descrambler_lock;

  /*
   * Functions
   */

  void (*mm_delete)           (mpegts_mux_t *mm, int delconf);
  void (*mm_config_save)      (mpegts_mux_t *mm);
  void (*mm_display_name)     (mpegts_mux_t*, char *buf, size_t len);
  int  (*mm_is_enabled)       (mpegts_mux_t *mm);
  int  (*mm_start)            (mpegts_mux_t *mm, const char *r, int w);
  void (*mm_stop)             (mpegts_mux_t *mm, int force);
  void (*mm_open_table)       (mpegts_mux_t*,mpegts_table_t*,int subscribe);
  void (*mm_close_table)      (mpegts_mux_t*,mpegts_table_t*);
  void (*mm_create_instances) (mpegts_mux_t*);
  int  (*mm_is_epg)           (mpegts_mux_t*);

  /*
   * Configuration
   */
  char *mm_crid_authority;
  int   mm_enabled;
  int   mm_epg;
  char *mm_charset;
};
 
/* Service */
struct mpegts_service
{
  service_t; // Parent

  /*
   * Fields defined by DVB standard EN 300 468
   */

  uint16_t s_dvb_service_id;
  uint16_t s_dvb_channel_num;
  char    *s_dvb_svcname;
  char    *s_dvb_provider;
  char    *s_dvb_cridauth;
  uint16_t s_dvb_servicetype;
  char    *s_dvb_charset;
  uint16_t s_dvb_prefcapid;

  /*
   * EIT/EPG control
   */

  int      s_dvb_eit_enable;

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
   * When a subscription request SMT_MPEGTS, chunk them togeather 
   * in order to recude load.
   */
  sbuf_t s_tsbuf;

  /**
   * Average continuity errors
   */
  avgstat_t s_cc_errors;

  /**
   * PCR drift compensation. This should really be per-packet.
   */
  int64_t  s_pcr_drift;

  /**
   * PMT monitoring
   */

  mpegts_table_t *s_pmt_mon; ///< Table entry for monitoring PMT

};

/* **************************************************************************
 * Physical Network
 * *************************************************************************/

/* Physical mux instance */
struct mpegts_mux_instance
{
  idnode_t mmi_id;

  LIST_ENTRY(mpegts_mux_instance) mmi_mux_link;
  LIST_ENTRY(mpegts_mux_instance) mmi_input_link;
  LIST_ENTRY(mpegts_mux_instance) mmi_active_link;

  streaming_pad_t mmi_streaming_pad;
  
  mpegts_mux_t   *mmi_mux;
  mpegts_input_t *mmi_input;

  LIST_HEAD(,th_subscription) mmi_subs;

  tvh_input_stream_stats_t mmi_stats;

  int             mmi_tune_failed;

  void (*mmi_delete) (mpegts_mux_instance_t *mmi);
};

struct mpegts_mux_sub
{
  RB_ENTRY(mpegts_mux_sub)  mms_link;
  void                     *mms_src;
  int                       mms_weight;
};

/* Input source */
struct mpegts_input
{
  tvh_input_t;

  int mi_enabled;

  int mi_instance;

  char *mi_name;

  int mi_priority;

  int mi_ota_epg;

  LIST_ENTRY(mpegts_input) mi_global_link;

  mpegts_network_link_list_t mi_networks;

  LIST_HEAD(,mpegts_mux_instance) mi_mux_instances;


  /*
   * Status
   */
  gtimer_t mi_status_timer;

  /*
   * Input processing
   */

  uint8_t mi_running;
  uint8_t mi_live;
  time_t mi_last_dispatch;

  /* Data input */
  // Note: this section is protected by mi_input_lock
  pthread_t                       mi_input_tid;
  pthread_mutex_t                 mi_input_lock;
  pthread_cond_t                  mi_input_cond;
  TAILQ_HEAD(,mpegts_packet)      mi_input_queue;

  /* Data processing/output */
  // Note: this lock (mi_output_lock) protects all the remaining
  //       data fields (excluding the callback functions)
  pthread_mutex_t                 mi_output_lock;

  /* Active sources */
  LIST_HEAD(,mpegts_mux_instance) mi_mux_active;
  LIST_HEAD(,service)             mi_transports;
  
  /* Table processing */
  pthread_t                       mi_table_tid;
  pthread_cond_t                  mi_table_cond;
  mpegts_table_feed_queue_t       mi_table_queue;

  /*
   * Functions
   */
  int  (*mi_is_enabled)     (mpegts_input_t*, mpegts_mux_t *mm, const char *reason);
  void (*mi_enabled_updated)(mpegts_input_t*);
  void (*mi_display_name)   (mpegts_input_t*, char *buf, size_t len);
  int  (*mi_is_free)        (mpegts_input_t*);
  int  (*mi_get_weight)     (mpegts_input_t*);
  int  (*mi_get_priority)   (mpegts_input_t*, mpegts_mux_t *mm);
  int  (*mi_get_grace)      (mpegts_input_t*, mpegts_mux_t *mm);
  int  (*mi_start_mux)      (mpegts_input_t*,mpegts_mux_instance_t*);
  void (*mi_stop_mux)       (mpegts_input_t*,mpegts_mux_instance_t*);
  void (*mi_open_service)   (mpegts_input_t*,mpegts_service_t*,int first);
  void (*mi_close_service)  (mpegts_input_t*,mpegts_service_t*);
  mpegts_pid_t *(*mi_open_pid)(mpegts_input_t*,mpegts_mux_t*,int,int,void*);
  void (*mi_close_pid)      (mpegts_input_t*,mpegts_mux_t*,int,int,void*);
  void (*mi_create_mux_instance) (mpegts_input_t*,mpegts_mux_t*);
  void (*mi_started_mux)    (mpegts_input_t*,mpegts_mux_instance_t*);
  void (*mi_stopped_mux)    (mpegts_input_t*,mpegts_mux_instance_t*);
  int  (*mi_has_subscription) (mpegts_input_t*, mpegts_mux_t *mm);
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

#define mpegts_input_find(u) idnode_find(u, &mpegts_input_class);

int mpegts_input_set_networks ( mpegts_input_t *mi, htsmsg_t *msg );

int mpegts_input_add_network  ( mpegts_input_t *mi, mpegts_network_t *mn );

void mpegts_input_open_service ( mpegts_input_t *mi, mpegts_service_t *s, int init );
void mpegts_input_close_service ( mpegts_input_t *mi, mpegts_service_t *s );

void mpegts_input_status_timer ( void *p );

int mpegts_input_grace ( mpegts_input_t * mi, mpegts_mux_t * mm );

int mpegts_input_is_enabled ( mpegts_input_t * mi, mpegts_mux_t *mm, const char *reason );

/* TODO: exposing these class methods here is a bit of a hack */
const void *mpegts_input_class_network_get  ( void *o );
int         mpegts_input_class_network_set  ( void *o, const void *p );
htsmsg_t   *mpegts_input_class_network_enum ( void *o );
char       *mpegts_input_class_network_rend ( void *o );

void mpegts_network_register_builder
  ( const idclass_t *idc,
    mpegts_network_t *(*build)(const idclass_t *idc, htsmsg_t *conf) );

void mpegts_network_unregister_builder
  ( const idclass_t *idc );

mpegts_network_t *mpegts_network_build
  ( const char *clazz, htsmsg_t *conf );
                 
mpegts_network_t *mpegts_network_create0
  ( mpegts_network_t *mn, const idclass_t *idc, const char *uuid,
    const char *name, htsmsg_t *conf );

#define mpegts_network_create(t, u, n, c)\
  (struct t*)mpegts_network_create0(calloc(1, sizeof(struct t)), &t##_class, u, n, c)

extern const idclass_t mpegts_network_class;

#define mpegts_network_find(u)\
  idnode_find(u, &mpegts_network_class)

mpegts_mux_t *mpegts_network_find_mux
  (mpegts_network_t *mn, uint16_t onid, uint16_t tsid);

void mpegts_network_class_delete ( const idclass_t *idc, int delconf );

void mpegts_network_delete ( mpegts_network_t *mn, int delconf );

int mpegts_network_set_nid          ( mpegts_network_t *mn, uint16_t nid );
int mpegts_network_set_network_name ( mpegts_network_t *mn, const char *name );

mpegts_mux_t *mpegts_mux_create0
  ( mpegts_mux_t *mm, const idclass_t *class, const char *uuid,
    mpegts_network_t *mn, uint16_t onid, uint16_t tsid,
    htsmsg_t *conf );

#define mpegts_mux_create(type, uuid, mn, onid, tsid, conf)\
  (struct type*)mpegts_mux_create0(calloc(1, sizeof(struct type)),\
                                   &type##_class, uuid,\
                                   mn, onid, tsid, conf)
#define mpegts_mux_create1(uuid, mn, onid, tsid, conf)\
  mpegts_mux_create0(calloc(1, sizeof(mpegts_mux_t)), &mpegts_mux_class, uuid,\
                     mn, onid, tsid, conf)

#define mpegts_mux_find(u)\
  idnode_find(u, &mpegts_mux_class)

#define mpegts_mux_delete_by_uuid(u, delconf)\
  { mpegts_mux_t *mm = mpegts_mux_find(u); if (mm) mm->mm_delete(mm, delconf); }

void mpegts_mux_delete ( mpegts_mux_t *mm, int delconf );

void mpegts_mux_save ( mpegts_mux_t *mm, htsmsg_t *c );

mpegts_mux_instance_t *mpegts_mux_instance_create0
  ( mpegts_mux_instance_t *mmi, const idclass_t *class, const char *uuid,
    mpegts_input_t *mi, mpegts_mux_t *mm );

mpegts_service_t *mpegts_mux_find_service(mpegts_mux_t *ms, uint16_t sid);

#define mpegts_mux_instance_create(type, uuid, mi, mm)\
  (struct type*)mpegts_mux_instance_create0(calloc(1, sizeof(struct type)),\
                                            &type##_class, uuid,\
                                            mi, mm);
int mpegts_mux_instance_start
  ( mpegts_mux_instance_t **mmiptr );

int mpegts_mux_instance_weight ( mpegts_mux_instance_t *mmi );

int mpegts_mux_set_tsid ( mpegts_mux_t *mm, uint16_t tsid, int force );
int mpegts_mux_set_onid ( mpegts_mux_t *mm, uint16_t onid );
int mpegts_mux_set_crid_authority ( mpegts_mux_t *mm, const char *defauth );

void mpegts_mux_open_table ( mpegts_mux_t *mm, mpegts_table_t *mt, int subscribe );
void mpegts_mux_close_table ( mpegts_mux_t *mm, mpegts_table_t *mt );

void mpegts_mux_remove_subscriber(mpegts_mux_t *mm, th_subscription_t *s, int reason);
int  mpegts_mux_subscribe(mpegts_mux_t *mm, const char *name, int weight);
void mpegts_mux_unsubscribe_by_name(mpegts_mux_t *mm, const char *name);

void mpegts_mux_scan_done ( mpegts_mux_t *mm, const char *buf, int res );

void mpegts_mux_nice_name( mpegts_mux_t *mm, char *buf, size_t len );

mpegts_pid_t *mpegts_mux_find_pid_(mpegts_mux_t *mm, int pid, int create);

static inline mpegts_pid_t *
mpegts_mux_find_pid(mpegts_mux_t *mm, int pid, int create)
{
  if (mm->mm_last_pid != pid)
    return mpegts_mux_find_pid_(mm, pid, create);
  else
    return mm->mm_last_mp;
}

void mpegts_input_recv_packets
  (mpegts_input_t *mi, mpegts_mux_instance_t *mmi, sbuf_t *sb,
   int64_t *pcr, uint16_t *pcr_pid);

int mpegts_input_is_free ( mpegts_input_t *mi );

int mpegts_input_get_weight ( mpegts_input_t *mi );
int mpegts_input_get_priority ( mpegts_input_t *mi, mpegts_mux_t *mm );
int mpegts_input_get_grace ( mpegts_input_t *mi, mpegts_mux_t *mm );

void mpegts_input_save ( mpegts_input_t *mi, htsmsg_t *c );

void mpegts_input_flush_mux ( mpegts_input_t *mi, mpegts_mux_t *mm );

mpegts_pid_t * mpegts_input_open_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner );

void mpegts_input_close_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner );

void mpegts_table_dispatch
  (const uint8_t *sec, size_t r, void *mt);
static inline void mpegts_table_grab
  (mpegts_table_t *mt) { mt->mt_refcount++; }
void mpegts_table_release_
  (mpegts_table_t *mt);
static inline void mpegts_table_release
  (mpegts_table_t *mt)
{
  assert(mt->mt_refcount > 0);
  if(--mt->mt_refcount == 0) mpegts_table_release_(mt);
}
int mpegts_table_type
  ( mpegts_table_t *mt );
mpegts_table_t *mpegts_table_add
  (mpegts_mux_t *mm, int tableid, int mask,
   mpegts_table_callback_t callback, void *opaque,
   const char *name, int flags, int pid);
void mpegts_table_flush_all
  (mpegts_mux_t *mm);
void mpegts_table_destroy ( mpegts_table_t *mt );

mpegts_service_t *mpegts_service_create0
  ( mpegts_service_t *ms, const idclass_t *class, const char *uuid,
    mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid, htsmsg_t *conf );

#define mpegts_service_create(t, u, m, s, p, c)\
  (struct t*)mpegts_service_create0(calloc(1, sizeof(struct t)),\
                                    &t##_class, u, m, s, p, c)

#define mpegts_service_create1(u, m, s, p, c)\
  mpegts_service_create0(calloc(1, sizeof(mpegts_service_t)),\
                         &mpegts_service_class, u, m, s, p, c)

mpegts_service_t *mpegts_service_find 
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid, int create, int *save );

#define mpegts_service_find_by_uuid(u)\
  idnode_find(u, &mpegts_service_class)

void mpegts_service_delete ( service_t *s, int delconf );

/*
 * MPEG-TS event handler
 */

typedef struct mpegts_listener
{
  LIST_ENTRY(mpegts_listener) ml_link;
  void *ml_opaque;
  void (*ml_mux_start)  (mpegts_mux_t *mm, void *p);
  void (*ml_mux_stop)   (mpegts_mux_t *mm, void *p);
  void (*ml_mux_create) (mpegts_mux_t *mm, void *p);
  void (*ml_mux_delete) (mpegts_mux_t *mm, void *p);
} mpegts_listener_t;

LIST_HEAD(,mpegts_listener) mpegts_listeners;

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

#endif /* __TVH_MPEGTS_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/

