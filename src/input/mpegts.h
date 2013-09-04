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

#include "service.h"
#include "mpegts/dvb.h"

#define MPEGTS_ONID_NONE        0xFFFF
#define MPEGTS_TSID_NONE        0xFFFF
#define MPEGTS_PSI_SECTION_SIZE 5000

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

/* Lists */
typedef LIST_HEAD (,mpegts_network)                mpegts_network_list_t;
typedef LIST_HEAD (mpegts_input_list,mpegts_input) mpegts_input_list_t;
typedef TAILQ_HEAD(mpegts_mux_queue,mpegts_mux)    mpegts_mux_queue_t;
typedef LIST_HEAD (mpegts_mux_list,mpegts_mux)     mpegts_mux_list_t;
TAILQ_HEAD(mpegts_table_feed_queue, mpegts_table_feed);

/* Classes */
extern const idclass_t mpegts_network_class;
extern const idclass_t mpegts_mux_class;
extern const idclass_t mpegts_service_class;
extern const idclass_t mpegts_input_class;

/* **************************************************************************
 * SI processing
 * *************************************************************************/

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

struct mpegts_table
{
  /**
   * Flags, must never be changed after creation.
   * We inspect it without holding global_lock
   */
  int mt_flags;

#define MT_CRC      0x1
#define MT_FULL     0x2
#define MT_QUICKREQ 0x4

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
  int mt_fd;

  LIST_ENTRY(mpegts_table) mt_link;
  mpegts_mux_t *mt_mux;

  char *mt_name;

  void *mt_opaque;
  mpegts_table_callback_t mt_callback;

  RB_HEAD(,mpegts_table_state) mt_state;
  int mt_complete;
  int mt_incomplete;

  int mt_count;

  int mt_pid;

  int mt_id;
 
  int mt_table; // SI table id (base)
  int mt_mask;  //              mask

  int mt_destroyed; // Refcounting
  int mt_refcount;

  int mt_cc;

  mpegts_psi_section_t mt_sect;

  struct mpegts_table_mux_cb *mt_mux_cb;
  
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
  ( mpegts_psi_section_t *ps, const uint8_t *tsb, int crc,
    mpegts_psi_section_callback_t cb, void *opaque );

/* **************************************************************************
 * Logical network
 * *************************************************************************/

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
   * Scanning
   */
  mpegts_mux_queue_t      mn_initial_scan_pending_queue;
  mpegts_mux_queue_t      mn_initial_scan_current_queue;
  int                     mn_initial_scan_num;
  gtimer_t                mn_initial_scan_timer;

  /*
   * Inputs
   */
  mpegts_input_list_t     mn_inputs;

  /*
   * Multiplexes
   */
  mpegts_mux_list_t       mn_muxes;

  /*
   * Functions
   */
  void              (*mn_display_name) (mpegts_network_t*, char *buf, size_t len);
  void              (*mn_config_save)  (mpegts_network_t*);
  mpegts_mux_t*     (*mn_create_mux)
    (mpegts_mux_t*, uint16_t onid, uint16_t tsid, dvb_mux_conf_t *conf);
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
};

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

  gtimer_t                mm_initial_scan_timeout; // TODO: really? here?
  TAILQ_ENTRY(mpegts_mux) mm_initial_scan_link;
  enum {
    MM_SCAN_DONE,     // All done
    MM_SCAN_PENDING,  // Waiting to be tuned for initial scan
    MM_SCAN_CURRENT,  // Currently tuned for initial scan
  }                       mm_initial_scan_status;
  int                     mm_initial_scan_done;

  /*
   * Physical instances
   */

  LIST_HEAD(, mpegts_mux_instance) mm_instances;
  mpegts_mux_instance_t *mm_active;

  /*
   * Table processing
   */

  int                         mm_num_tables;
  LIST_HEAD(, mpegts_table)   mm_tables;
  TAILQ_HEAD(, mpegts_table)  mm_table_queue;
  uint8_t                     mm_table_filter[8192];

  /*
   * Functions
   */

  void (*mm_delete)           (mpegts_mux_t *mm);
  void (*mm_config_save)      (mpegts_mux_t *mm);
  void (*mm_display_name)     (mpegts_mux_t*, char *buf, size_t len);
  int  (*mm_is_enabled)       (mpegts_mux_t *mm);
  int  (*mm_start)            (mpegts_mux_t *mm, const char *r, int w);
  void (*mm_stop)             (mpegts_mux_t *mm, int force);
  void (*mm_open_table)       (mpegts_mux_t*,mpegts_table_t*);
  void (*mm_close_table)      (mpegts_mux_t*,mpegts_table_t*);
  void (*mm_create_instances) (mpegts_mux_t*);

  /*
   * Configuration
   */
  char *mm_crid_authority;
  int   mm_enabled;
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
   * Descrambling support
   */

#ifdef TODO_MOVE_THIS_HERE
  struct th_descrambler_list s_descramblers;
  int s_scrambled;
  int s_scrambled_seen;
  int s_caid;
  uint16_t s_prefcapid;
#endif

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

  // TODO: remove this
  int             mmi_tune_failed; // this is really DVB

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
  idnode_t mi_id;

  int mi_enabled;

  int mi_instance;

  char *mi_displayname;

  LIST_ENTRY(mpegts_input) mi_global_link;

  mpegts_network_t *mi_network;
  LIST_ENTRY(mpegts_input) mi_network_link;

  LIST_HEAD(,mpegts_mux_instance) mi_mux_active;

  LIST_HEAD(,mpegts_mux_instance) mi_mux_instances;

  /*
   * Input processing
   */

  pthread_mutex_t mi_delivery_mutex;

  LIST_HEAD(,service) mi_transports;


  int mi_bytes;

  struct mpegts_table_feed_queue mi_table_feed;
  pthread_cond_t mi_table_feed_cond;  // Bound to mi_delivery_mutex


  pthread_t mi_thread_id;
  th_pipe_t mi_thread_pipe;

  /*
   * Functions
   */
  int  (*mi_is_enabled)     (mpegts_input_t*);
  void (*mi_display_name)   (mpegts_input_t*, char *buf, size_t len);
  int  (*mi_is_free)        (mpegts_input_t*);
  int  (*mi_current_weight) (mpegts_input_t*);
  int  (*mi_start_mux)      (mpegts_input_t*,mpegts_mux_instance_t*);
  void (*mi_stop_mux)       (mpegts_input_t*,mpegts_mux_instance_t*);
  void (*mi_open_service)   (mpegts_input_t*,mpegts_service_t*,int first);
  void (*mi_close_service)  (mpegts_input_t*,mpegts_service_t*);
  void (*mi_create_mux_instance) (mpegts_input_t*,mpegts_mux_t*);
  void (*mi_started_mux)    (mpegts_input_t*,mpegts_mux_instance_t*);
  void (*mi_stopped_mux)    (mpegts_input_t*,mpegts_mux_instance_t*);
  int  (*mi_has_subscription) (mpegts_input_t*, mpegts_mux_t *mm);
  int  (*mi_grace_period)   (mpegts_input_t*, mpegts_mux_t *mm);
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

#define mpegts_input_find(u) idnode_find(u, &mpegts_input_class);

void mpegts_input_set_network ( mpegts_input_t *mi, mpegts_network_t *mn );

void mpegts_input_open_service ( mpegts_input_t *mi, mpegts_service_t *s, int init );
void mpegts_input_close_service ( mpegts_input_t *mi, mpegts_service_t *s );

void mpegts_network_register_builder
  ( const idclass_t *idc,
    mpegts_network_t *(*build)(const idclass_t *idc, htsmsg_t *conf) );

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
  
void mpegts_network_delete ( mpegts_network_t *mn );

void mpegts_network_schedule_initial_scan
  ( mpegts_network_t *mm );

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
  idnode_find(u, &mpegts_mux_class);

#define mpegts_mux_delete_by_uuid(u)\
  { mpegts_mux_t *mm = mpegts_mux_find(u); if (mm) mm->mm_delete(mm); }

void mpegts_mux_initial_scan_done ( mpegts_mux_t *mm );

void mpegts_mux_delete ( mpegts_mux_t *mm );

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

int mpegts_mux_set_tsid ( mpegts_mux_t *mm, uint16_t tsid );
int mpegts_mux_set_onid ( mpegts_mux_t *mm, uint16_t onid );
int mpegts_mux_set_crid_authority ( mpegts_mux_t *mm, const char *defauth );

void mpegts_mux_open_table ( mpegts_mux_t *mm, mpegts_table_t *mt );
void mpegts_mux_close_table ( mpegts_mux_t *mm, mpegts_table_t *mt );

size_t mpegts_input_recv_packets
  (mpegts_input_t *mi, mpegts_mux_instance_t *mmi, uint8_t *tsb, size_t len,
   int64_t *pcr, uint16_t *pcr_pid, const char *name);

void *mpegts_input_table_thread ( void *aux );

int mpegts_input_is_free ( mpegts_input_t *mi );

int mpegts_input_current_weight ( mpegts_input_t *mi );

void mpegts_input_save ( mpegts_input_t *mi, htsmsg_t *c );

void mpegts_input_flush_mux ( mpegts_input_t *mi, mpegts_mux_t *mm );

void mpegts_table_dispatch
  (const uint8_t *sec, size_t r, void *mt);
void mpegts_table_release
  (mpegts_table_t *mt);
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

void mpegts_service_save ( mpegts_service_t *s, htsmsg_t *c );

void mpegts_service_delete ( service_t *s );

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
