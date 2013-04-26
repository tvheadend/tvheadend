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

/* Types */
typedef struct mpegts_table         mpegts_table_t;
typedef struct mpegts_network       mpegts_network_t;
typedef struct mpegts_mux           mpegts_mux_t;
typedef struct mpegts_service       mpegts_service_t;
typedef struct mpegts_mux_instance  mpegts_mux_instance_t;
typedef struct mpegts_input         mpegts_input_t;

/* Lists */
typedef TAILQ_HEAD(mpegts_mux_queue,mpegts_mux) mpegts_mux_queue_t;
typedef LIST_HEAD (mpegts_mux_list,mpegts_mux)  mpegts_mux_list_t;

/* **************************************************************************
 * SI processing
 * *************************************************************************/

struct mpegts_table
{
  /**
   * Flags, must never be changed after creation.
   * We inspect it without holding global_lock
   */
  int mt_flags;

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
  int (*mt_callback)(mpegts_mux_t *m, uint8_t *buf, int len,
		      uint8_t tableid, void *opaque);


  // TODO: remind myself of what each field is for
  int mt_count;
  int mt_pid;

  int mt_id;
 
  int mt_table; // SI table id (base)
  int mt_mask;  //              mask

  int mt_destroyed; // Refcounting
  int mt_refcount;

  //psi_section_t mt_sect; // Manual reassembly

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
  uint16_t                mn_network_id; // ONID/NID??

  /*
   * Scanning
   */
  mpegts_mux_queue_t      mn_initial_scan_pending_queue;
  mpegts_mux_queue_t      mn_initial_scan_current_queue;
  int                     mn_initial_scan_num;
  gtimer_t                mn_initial_scan_timer;

  /*
   * Multiplexes
   */
  mpegts_mux_list_t       mn_muxes;

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
  uint16_t                mm_mux_id;

  /*
   * Services
   */
  
  struct service_list     mm_services;

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
   * Input processing
   */
  
  int                         mm_num_tables;
  LIST_HEAD(, mpegts_table)   mm_tables;
  TAILQ_HEAD(, mpegts_table)  mm_table_queue;
  // TODO: remind myself of what the queue/list's are for

  /*
   * Physical instances
   */

  LIST_HEAD(, mpegts_mux_instance) mm_instances;
  
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

  mpegts_mux_t    *s_dvb_mux;
  mpegts_input_t  *s_dvb_active_input;

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

/* Create */
mpegts_service_t * mpegts_service_create0
  ( size_t alloc, const idclass_t *class, const char *uuid );
#define mpegts_service_create(uuid)\
  mpegts_service_create0(sizeof(mpegts_service_t), &mpegts_service_class, uuid)
#define mpegts_service_create1(type, uuid)\
  (type##_t*)mpegts_service_create0(sizeof(type##_t), &type##_class, uuid)

/* **************************************************************************
 * Physical Network
 * *************************************************************************/

// TODO: do we need the concept of multiple physical networks that provide
//       the same logical network (this is common for DVB-T)

struct mpegts_phy_network
{
};

/* Physical mux instance */
struct mpegts_mux_instance
{
  LIST_ENTRY(mpegts_mux_instance) mmi_mux_link;
  LIST_ENTRY(mpegts_mux_instance) mmi_active_link;
  
  mpegts_mux_t   *mmi_mux;
  mpegts_input_t *mmi_input;

  int             mmi_tune_failed; // this is really DVB
};

/* Input source */
struct mpegts_input
{
  int mi_instance;

  LIST_HEAD(,service) mi_transports;

  pthread_mutex_t mi_delivery_mutex;
  
  int  (*mi_start_mux)     (mpegts_input_t*,mpegts_mux_instance_t*);
  void (*mi_open_service)  (mpegts_input_t*,mpegts_service_t*);
  void (*mi_close_service) (mpegts_input_t*,mpegts_service_t*);
  void (*mi_open_table)    (mpegts_input_t*,mpegts_table_t*);
  void (*mi_close_table)   (mpegts_input_t*,mpegts_table_t*);
};

#endif /* __TVH_MPEGTS_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
