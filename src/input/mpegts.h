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
#include "src/input/mpegts/psi.h"

#define MPEGTS_ONID_NONE 0xFFFF
#define MPEGTS_TSID_NONE 0xFFFF

/* Types */
typedef struct mpegts_table         mpegts_table_t;
typedef struct mpegts_network       mpegts_network_t;
typedef struct mpegts_mux           mpegts_mux_t;
typedef struct mpegts_service       mpegts_service_t;
typedef struct mpegts_mux_instance  mpegts_mux_instance_t;
typedef struct mpegts_input         mpegts_input_t;
typedef struct mpegts_table_feed    mpegts_table_feed_t;

/* Lists */
typedef LIST_HEAD (mpegts_input_list,mpegts_input) mpegts_input_list_t;
typedef TAILQ_HEAD(mpegts_mux_queue,mpegts_mux)    mpegts_mux_queue_t;
typedef LIST_HEAD (mpegts_mux_list,mpegts_mux)     mpegts_mux_list_t;
TAILQ_HEAD(mpegts_table_feed_queue, mpegts_table_feed);

/* **************************************************************************
 * SI processing
 * *************************************************************************/

typedef int (*mpegts_table_callback)
  ( mpegts_table_t*, const uint8_t *buf, int len, int tableid );

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
  mpegts_table_callback mt_callback;


  // TODO: remind myself of what each field is for
  int mt_count;
  int mt_pid;

  int mt_id;
 
  int mt_table; // SI table id (base)
  int mt_mask;  //              mask

  int mt_destroyed; // Refcounting
  int mt_refcount;

  psi_section_t mt_sect; // Manual reassembly

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


/* **************************************************************************
 * Logical network
 * *************************************************************************/

/* Network */
struct mpegts_network
{
  idnode_t                mn_id;
  LIST_ENTRY(dvb_network) mn_global_link;

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
  mpegts_mux_t*     (*mn_create_mux)
    (mpegts_mux_t*, uint16_t onid, uint16_t tsid, void *aux);
  mpegts_service_t* (*mn_create_service)
    (mpegts_mux_t*, uint16_t sid, uint16_t pmt_pid);

  // Note: the above are slightly odd in that they take mux instead of
  //       network as initial param. This is intentional as we need to
  //       know the mux and can easily get to network from there

#if 0 // TODO: FIXME
  int dn_fe_type;  // Frontend types for this network (FE_QPSK, etc)
#endif

#if 0 // TODO: FIXME CONFIG
  uint32_t dn_disable_pmt_monitor;
  uint32_t dn_autodiscovery;
  uint32_t dn_nitoid;
  uint32_t dn_skip_checksubscr;
#endif

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

  int  (*mm_start)         ( mpegts_mux_t *mm, const char *reason, int weight );
  void (*mm_open_table)    (mpegts_mux_t*,mpegts_table_t*);
  void (*mm_close_table)   (mpegts_mux_t*,mpegts_table_t*);

  
#if 0
  dvb_mux_conf_t dm_conf;

  char *dm_default_authority;

  TAILQ_HEAD(, epggrab_ota_mux) dm_epg_grab;
#endif

#if 0 // TODO: do we need this here? or linuxdvb?
  struct th_dvb_mux_instance *dm_current_tdmi;

  struct th_dvb_mux_instance_list dm_tdmis;
#endif

#if 0 // TODO: what about these?
  // Derived from dm_conf (more or less)
  char *dm_local_identifier;

  int dm_enabled; // TODO: could be derived?
#endif

};
 
/* Service */
struct mpegts_service
{
  service_t; // Parent

  /**
   * PID carrying the programs PCR.
   * XXX: We don't support transports that does not carry
   * the PCR in one of the content streams.
   */
  uint16_t s_pcr_pid;

  /**
   * PID for the PMT of this MPEG-TS stream.
   */
  uint16_t s_pmt_pid;

  /*
   * Fields defined by DVB standard EN 300 468
   */

  uint16_t s_dvb_service_id;
  uint16_t s_dvb_channel_num;
  char    *s_dvb_svcname;
  char    *s_dvb_provider;
  char    *s_dvb_default_authority;
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
  LIST_ENTRY(mpegts_mux_instance) mmi_active_link;

  streaming_pad_t mmi_streaming_pad;
  
  mpegts_mux_t   *mmi_mux;
  mpegts_input_t *mmi_input;

  // TODO: remove this
  int             mmi_tune_failed; // this is really DVB
};

/* Input source */
struct mpegts_input
{
  idnode_t mi_id;

  int mi_instance;

  LIST_ENTRY(mpegts_input) mi_global_link;

  mpegts_network_t *mi_network;
  LIST_ENTRY(mpegts_input) mi_network_link;

  LIST_HEAD(,mpegts_mux_instance) mi_mux_active;

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
  
  int  (*mi_start_mux)      (mpegts_input_t*,mpegts_mux_instance_t*);
  void (*mi_stop_mux)       (mpegts_input_t*,mpegts_mux_instance_t*);
  void (*mi_open_service)   (mpegts_input_t*,mpegts_service_t*);
  void (*mi_close_service)  (mpegts_input_t*,mpegts_service_t*);
  int  (*mi_is_free)        (mpegts_input_t*);
  int  (*mi_current_weight) (mpegts_input_t*);
};

#endif /* __TVH_MPEGTS_H__ */

/* ****************************************************************************
 * Functions
 * ***************************************************************************/

mpegts_input_t *mpegts_input_create0
  ( mpegts_input_t *mi, const idclass_t *idc, const char *uuid );

#define mpegts_input_create(t, u)\
  (struct t*)mpegts_input_create0(calloc(1, sizeof(struct t)), t##_class, u)

#define mpegts_input_create1(u)\
  mpegts_input_create0(calloc(1, sizeof(mpegts_input_t)),\
                       &mpegts_input_class, u)
                 

mpegts_network_t *mpegts_network_create0
  ( mpegts_network_t *mn, const idclass_t *idc, const char *uuid,
    const char *name );

#define mpegts_network_create(t, u, n)\
  (struct t*)mpegts_network_create0(calloc(1, sizeof(struct t)), t##_class, u, n)
  
void mpegts_network_schedule_initial_scan
  ( mpegts_network_t *mm );

void mpegts_network_add_input ( mpegts_network_t *mn, mpegts_input_t *mi );

mpegts_mux_t *mpegts_mux_create0
  ( mpegts_mux_t *mm, const idclass_t *class, const char *uuid,
    mpegts_network_t *mn, uint16_t onid, uint16_t tsid );

#define mpegts_mux_create(type, uuid, mn, onid, tsid)\
  (struct type*)mpegts_mux_create0(calloc(1, sizeof(struct type)),\
                                   &type##_class, uuid,\
                                   mn, onid, tsid)
#define mpegts_mux_create1(uuid, mn, onid, tsid)\
  mpegts_mux_create0(calloc(1, sizeof(mpegts_mux_t)), &mpegts_mux_class, uuid,\
                     mn, onid, tsid)

void mpegts_mux_initial_scan_done ( mpegts_mux_t *mm );

void mpegts_mux_load_one ( mpegts_mux_t *mm, htsmsg_t *c );

mpegts_mux_instance_t *mpegts_mux_instance_create0
  ( mpegts_mux_instance_t *mmi, const idclass_t *class, const char *uuid,
    mpegts_input_t *mi, mpegts_mux_t *mm );

#define mpegts_mux_instance_create(type, uuid, mi, mm)\
  (struct type*)mpegts_mux_instance_create0(calloc(1, sizeof(struct type)),\
                                            &type##_class, uuid,\
                                            mi, mm);

void mpegts_mux_set_tsid ( mpegts_mux_t *mm, uint16_t tsid, int force );

void mpegts_mux_set_onid ( mpegts_mux_t *mm, uint16_t onid, int force );

size_t mpegts_input_recv_packets
  (mpegts_input_t *mi, mpegts_mux_instance_t *mmi, uint8_t *tsb, size_t len,
   int64_t *pcr, uint16_t *pcr_pid);

void *mpegts_input_table_thread ( void *aux );

int mpegts_input_is_free ( mpegts_input_t *mi );

int mpegts_input_current_weight ( mpegts_input_t *mi );

void mpegts_table_dispatch
  (const uint8_t *sec, size_t r, void *mt);
void mpegts_table_release
  (mpegts_table_t *mt);
void mpegts_table_add
  (mpegts_mux_t *mm, int tableid, int mask,
   mpegts_table_callback callback, void *opaque,
   const char *name, int flags, int pid);
void mpegts_table_flush_all
  (mpegts_mux_t *mm);
void mpegts_table_destroy ( mpegts_table_t *mt );

mpegts_service_t *mpegts_service_create0
  ( mpegts_service_t *ms, const idclass_t *class, const char *uuid,
    mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid );

#define mpegts_service_create(t, u, m, s, p)\
  (struct t*)mpegts_service_create0(calloc(1, sizeof(struct t)),\
                                    &t##_class, u, m, s, p)

#define mpegts_service_create1(u, m, s, p)\
  mpegts_service_create0(calloc(1, sizeof(mpegts_service_t)),\
                         &mpegts_service_class, u, m, s, p)

void mpegts_service_load_one ( mpegts_service_t *ms, htsmsg_t *c );

mpegts_service_t *mpegts_service_find ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid, const char *uuid, int *save );


/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
