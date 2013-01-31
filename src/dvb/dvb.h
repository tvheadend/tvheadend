/*
 *  TV Input - Linux DVB interface
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

#ifndef DVB_H_
#define DVB_H_

#include <linux/dvb/version.h>
#include <linux/dvb/frontend.h>
#include <pthread.h>
#include "htsmsg.h"
#include "psi.h"
#include "idnode.h"

struct service;
struct th_dvb_table;
struct th_dvb_mux_instance;

#define DVB_VER_INT(maj,min) (((maj) << 16) + (min))

#define DVB_VER_ATLEAST(maj, min) \
 (DVB_VER_INT(DVB_API_VERSION,  DVB_API_VERSION_MINOR) >= DVB_VER_INT(maj, min))

TAILQ_HEAD(th_dvb_adapter_queue, th_dvb_adapter);
LIST_HEAD(th_dvb_adapter_list, th_dvb_adapter);
TAILQ_HEAD(th_dvb_mux_instance_queue, th_dvb_mux_instance);
LIST_HEAD(th_dvb_mux_instance_list, th_dvb_mux_instance);
TAILQ_HEAD(dvb_satconf_queue, dvb_satconf);
LIST_HEAD(dvb_mux_list, dvb_mux);
TAILQ_HEAD(dvb_mux_queue, dvb_mux);
LIST_HEAD(dvb_network_list, dvb_network);

/**
 * Satconf
 */
typedef struct dvb_satconf {
  char *sc_id;
  TAILQ_ENTRY(dvb_satconf) sc_adapter_link;
  int sc_port;                   // diseqc switchport (0 - 63)

  char *sc_name;
  char *sc_comment;
  char *sc_lnb;

} dvb_satconf_t;


enum polarisation {
	POLARISATION_HORIZONTAL     = 0x00,
	POLARISATION_VERTICAL       = 0x01,
	POLARISATION_CIRCULAR_LEFT  = 0x02,
	POLARISATION_CIRCULAR_RIGHT = 0x03
};

#define DVB_FEC_ERROR_LIMIT 20

typedef struct dvb_frontend_parameters dvb_frontend_parameters_t;

/**
 *
 */
typedef struct dvb_mux_conf {
  dvb_frontend_parameters_t dmc_fe_params;
  int dmc_polarisation;
  //  dvb_satconf_t *dmc_satconf;
#if DVB_API_VERSION >= 5
  fe_modulation_t dmc_fe_modulation;
  fe_delivery_system_t dmc_fe_delsys;
  fe_rolloff_t dmc_fe_rolloff;
#endif
} dvb_mux_conf_t;



/**
 *
 */
typedef struct dvb_network {
  idnode_t dn_id;

  LIST_ENTRY(dvb_network) dn_global_link;

  struct dvb_mux_queue dn_initial_scan_pending_queue;
  struct dvb_mux_queue dn_initial_scan_current_queue;
  int dn_initial_scan_num_mux;
  gtimer_t dn_initial_scan_timer;

  struct dvb_mux *dn_mux_epg;

  int dn_fe_type;  // Frontend types for this network (FE_QPSK, etc)

  struct dvb_mux_list dn_muxes;

  uint32_t dn_disable_pmt_monitor;
  uint32_t dn_autodiscovery;
  uint32_t dn_nitoid;

  struct th_dvb_adapter_list dn_adapters;

} dvb_network_t;



/**
 *
 */
typedef struct dvb_mux {
  idnode_t dm_id;
  LIST_ENTRY(dvb_mux) dm_network_link;
  dvb_network_t *dm_dn;

  struct service_list dm_services;

  dvb_mux_conf_t dm_conf;

  uint32_t dm_network_id;
  uint16_t dm_transport_stream_id;
  char *dm_network_name;     /* Name of network, from NIT table */
  char *dm_default_authority;

  TAILQ_HEAD(, epggrab_ota_mux) dm_epg_grab;

  gtimer_t dm_initial_scan_timeout;

  TAILQ_ENTRY(dvb_mux) dm_scan_link;
  enum {
    DM_SCAN_DONE,     // All done
    DM_SCAN_PENDING,  // Waiting to be tuned for initial scan
    DM_SCAN_CURRENT,  // Currently tuned for initial scan
  } dm_scan_status;

  LIST_HEAD(, th_dvb_table) dm_tables;
  int dm_num_tables;

  TAILQ_HEAD(, th_dvb_table) dm_table_queue;
  //  int dm_table_initial;

  struct th_dvb_mux_instance *dm_current_tdmi;

  struct th_dvb_mux_instance_list dm_tdmis;

  // Derived from dm_conf (more or less)
  char *dm_local_identifier;

  int dm_enabled;

} dvb_mux_t;




/**
 * DVB Mux instance
 */
typedef struct th_dvb_mux_instance {

  dvb_mux_t *tdmi_mux;
  LIST_ENTRY(th_dvb_mux_instance) tdmi_mux_link;

  struct th_dvb_adapter *tdmi_adapter;
  LIST_ENTRY(th_dvb_mux_instance) tdmi_adapter_link;


  uint16_t tdmi_snr, tdmi_signal;
  uint32_t tdmi_ber, tdmi_unc;
  float tdmi_unc_avg;

#define TDMI_FEC_ERR_HISTOGRAM_SIZE 10
  uint32_t tdmi_fec_err_histogram[TDMI_FEC_ERR_HISTOGRAM_SIZE];
  int      tdmi_fec_err_ptr;


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

  int tdmi_tune_failed; // Adapter failed to tune this frequency
                        // Don't try again

  int tdmi_weight;

  struct th_subscription_list tdmi_subscriptions;

} th_dvb_mux_instance_t;





/**
 * When in raw mode we need to enqueue raw TS packet
 * to a different thread because we need to hold
 * global_lock when doing delivery of the tables
 */
TAILQ_HEAD(dvb_table_feed_queue, dvb_table_feed);

typedef struct dvb_table_feed {
  TAILQ_ENTRY(dvb_table_feed) dtf_link;
  uint8_t dtf_tsb[188];
} dvb_table_feed_t;




/**
 * DVB Adapter (one of these per physical adapter)
 */
#define TDA_MUX_HASH_WIDTH 101

typedef struct th_dvb_adapter {

  TAILQ_ENTRY(th_dvb_adapter) tda_global_link;

  dvb_network_t *tda_dn;
  LIST_ENTRY(th_dvb_adapter) tda_network_link;

  struct th_dvb_mux_instance_list tda_tdmis;

  th_dvb_mux_instance_t *tda_current_tdmi;
  char *tda_tune_reason; // Reason for last tune

  int tda_table_epollfd;

  const char *tda_rootpath;
  char *tda_identifier;
  uint32_t tda_idleclose;
  uint32_t tda_skip_initialscan;
  uint32_t tda_skip_checksubscr;
  uint32_t tda_qmon;
  uint32_t tda_poweroff;
  uint32_t tda_sidtochan;
  uint32_t tda_diseqc_version;
  uint32_t tda_diseqc_repeats;
  int32_t  tda_full_mux_rx;
  char *tda_displayname;

  int tda_fe_type;
  struct dvb_frontend_info *tda_fe_info; // result of FE_GET_INFO ioctl()

  char *tda_fe_path;
  int tda_fe_fd;

  int tda_adapter_num;

  char *tda_demux_path;

  pthread_t tda_dvr_thread;
  int       tda_dvr_pipe[2];

  int tda_hostconnection;

  pthread_mutex_t tda_delivery_mutex;
  struct service_list tda_transports; /* Currently bound transports */

  gtimer_t tda_fe_monitor_timer;
  int tda_fe_monitor_hold;

  int tda_sat; // Set if this adapter is a satellite receiver (DVB-S, etc) 

  struct dvb_satconf_queue tda_satconfs;


  uint32_t tda_last_fec;

  int tda_unc_is_delta;  /* 1 if we believe FE_READ_UNCORRECTED_BLOCKS
			  * return dela values */

  uint32_t tda_extrapriority; // extra priority for choosing the best adapter/service

  void (*tda_open_service)(struct th_dvb_adapter *tda, struct service *s);
  void (*tda_close_service)(struct th_dvb_adapter *tda, struct service *s);
  void (*tda_open_table)(struct dvb_mux *dm, struct th_dvb_table *s);
  void (*tda_close_table)(struct dvb_mux *dm, struct th_dvb_table *s);

  int tda_rawmode;

  // Full mux streaming, protected via the delivery mutex

  streaming_pad_t tda_streaming_pad;


  struct dvb_table_feed_queue tda_table_feed;
  pthread_cond_t tda_table_feed_cond;  // Bound to tda_delivery_mutex

  // PIDs that needs to be requeued and processed as tables
  uint8_t tda_table_filter[8192];


} th_dvb_adapter_t;

/**
 * DVB table
 */
typedef struct th_dvb_table {
  /**
   * Flags, must never be changed after creation.
   * We inspect it without holding global_lock
   */
  int tdt_flags;

  /**
   * Cycle queue
   * Tables that did not get a fd or filter in hardware will end up here
   * waiting for any other table to be received so it can reuse that fd.
   * Only linked if fd == -1
   */
  TAILQ_ENTRY(th_dvb_table) tdt_pending_link;

  /**
   * File descriptor for filter
   */
  int tdt_fd;

  LIST_ENTRY(th_dvb_table) tdt_link;
  th_dvb_mux_instance_t *tdt_tdmi;

  char *tdt_name;

  void *tdt_opaque;
  int (*tdt_callback)(dvb_mux_t *dm, uint8_t *buf, int len,
		      uint8_t tableid, void *opaque);


  int tdt_count;
  int tdt_pid;

  int tdt_id;

  int tdt_table;
  int tdt_mask;

  int tdt_destroyed;
  int tdt_refcount;

  psi_section_t tdt_sect; // Manual reassembly

} th_dvb_table_t;


extern struct dvb_network_list dvb_networks;
extern struct th_dvb_adapter_queue dvb_adapters;
//extern struct th_dvb_mux_instance_tree dvb_muxes;

void dvb_init(uint32_t adapter_mask, const char *rawfile);

/**
 * DVB Adapter
 */
void dvb_adapter_init(uint32_t adapter_mask, const char *rawfile);

void dvb_adapter_start (th_dvb_adapter_t *tda);

void dvb_adapter_stop (th_dvb_adapter_t *tda);

void dvb_adapter_set_displayname(th_dvb_adapter_t *tda, const char *s);

void dvb_adapter_set_auto_discovery(th_dvb_adapter_t *tda, int on);

void dvb_adapter_set_skip_initialscan(th_dvb_adapter_t *tda, int on);

void dvb_adapter_set_skip_checksubscr(th_dvb_adapter_t *tda, int on);

void dvb_adapter_set_qmon(th_dvb_adapter_t *tda, int on);

void dvb_adapter_set_idleclose(th_dvb_adapter_t *tda, int on);

void dvb_adapter_set_poweroff(th_dvb_adapter_t *tda, int on);

void dvb_adapter_set_sidtochan(th_dvb_adapter_t *tda, int on);

void dvb_adapter_set_nitoid(th_dvb_adapter_t *tda, int nitoid);

void dvb_adapter_set_diseqc_version(th_dvb_adapter_t *tda, unsigned int v);

void dvb_adapter_set_diseqc_repeats(th_dvb_adapter_t *tda,
                                    unsigned int repeats);

void dvb_adapter_set_disable_pmt_monitor(th_dvb_adapter_t *tda, int on);

void dvb_adapter_set_full_mux_rx(th_dvb_adapter_t *tda, int r);

void dvb_adapter_clone(th_dvb_adapter_t *dst, th_dvb_adapter_t *src);

void dvb_adapter_clean(th_dvb_adapter_t *tda);

int dvb_adapter_destroy(th_dvb_adapter_t *tda);

void dvb_adapter_notify(th_dvb_adapter_t *tda);

htsmsg_t *dvb_adapter_build_msg(th_dvb_adapter_t *tda);

htsmsg_t *dvb_fe_opts(th_dvb_adapter_t *tda, const char *which);

void dvb_adapter_set_extrapriority(th_dvb_adapter_t *tda, int extrapriority);

void dvb_adapter_poweroff(th_dvb_adapter_t *tda);

void dvb_input_filtered_setup(th_dvb_adapter_t *tda);

void dvb_input_raw_setup(th_dvb_adapter_t *tda);



/**
 * DVB Multiplex
 */
const char* dvb_mux_fec2str(int fec);
const char* dvb_mux_delsys2str(int delsys);
const char* dvb_mux_qam2str(int qam);
const char* dvb_mux_rolloff2str(int rolloff);

int dvb_mux_str2bw(const char *str);
int dvb_mux_str2qam(const char *str);
int dvb_mux_str2fec(const char *str);
int dvb_mux_str2mode(const char *str);
int dvb_mux_str2guard(const char *str);
int dvb_mux_str2hier(const char *str);

void dvb_mux_save(dvb_mux_t *dm);

void dvb_mux_load(dvb_network_t *dn);

void dvb_mux_destroy(dvb_mux_t *dm);

dvb_mux_t *dvb_mux_create(dvb_network_t *tda,
                          const struct dvb_mux_conf *dmc,
                          uint16_t onid, uint16_t tsid, const char *network,
                          const char *logprefix, int enabled,
                          int initialscan, const char *uuid);

int dvb_mux_tune(dvb_mux_t *dm, const char *reason, int weight);

void dvb_mux_set_networkname(dvb_mux_t *dm, const char *name);

void dvb_mux_set_tsid(dvb_mux_t *dm, uint16_t tsid);

void dvb_mux_set_onid(dvb_mux_t *mux, uint16_t onid);

void dvb_mux_set_enable(th_dvb_mux_instance_t *tdmi, int enabled);

void dvb_mux_set_satconf(th_dvb_mux_instance_t *tdmi, const char *scid,
			 int save);

htsmsg_t *dvb_mux_build_msg(dvb_mux_t *dm);

void dvb_mux_notify(dvb_mux_t *dm);

const char *dvb_mux_add_by_params(dvb_network_t *dn,
				  int freq,
				  int symrate,
				  int bw,
				  int constellation,
				  int delsys,
				  int tmode,
				  int guard,
				  int hier,
				  int fechi,
				  int feclo,
				  int fec,
				  int polarisation,
				  const char *satconf);
#if 0
int dvb_mux_copy(th_dvb_adapter_t *dst, th_dvb_mux_instance_t *tdmi_src,
		 dvb_satconf_t *satconf);
#endif

dvb_mux_t *dvb_mux_find(dvb_network_t *dn, const char *netname, uint16_t onid,
                        uint16_t tsid, int enabled);


void dvb_mux_initial_scan_done(dvb_mux_t *dm);

int dvb_fe_tune_tdmi(th_dvb_mux_instance_t *tdmi);


/**
 * DVB Transport (aka DVB service)
 */
void dvb_service_load(dvb_mux_t *dm);

struct service *dvb_service_find(dvb_mux_t *dm,
                                 uint16_t sid, int pmt_pid,
                                 const char *identifier);

struct service *dvb_service_find2(dvb_mux_t *dm,
                                  uint16_t sid, int pmt_pid,
                                  const char *identifier, int *save);

struct service *dvb_service_find3(dvb_network_t *dn,
                                  dvb_mux_t *dm,
                                  const char *netname, uint16_t onid,
                                  uint16_t tsid, uint16_t sid,
                                  int enabled, int epgprimary);

void dvb_service_notify(struct service *t);

void dvb_service_notify_by_adapter(th_dvb_adapter_t *tda);

/**
 * DVB Frontend
 */

//void dvb_fe_stop(th_dvb_adapter_t *tda, int retune);


/**
 * DVB Tables
 */
void dvb_table_init(th_dvb_adapter_t *tda);

void dvb_table_add_default(dvb_mux_t *dm);

void dvb_table_flush_all(dvb_mux_t *dm);

void dvb_table_add_pmt(dvb_mux_t *dm, int pmt_pid);

void dvb_table_rem_pmt(dvb_mux_t *dm, int pmt_pid);

void tdt_add(dvb_mux_t *dm, int table, int mask,
	     int (*callback)(dvb_mux_t *dm, uint8_t *buf, int len,
			     uint8_t tableid, void *opaque), void *opaque,
	     const char *name, int flags, int pid);

int dvb_pidx11_callback(dvb_mux_t *dm, uint8_t *ptr, int len,
                        uint8_t tableid, void *opaque);

#define TDT_CRC           0x1
#define TDT_QUICKREQ      0x2
#define TDT_CA		        0x4
#define TDT_TDT           0x8

void dvb_table_dispatch(uint8_t *sec, int r, th_dvb_table_t *tdt);

void dvb_table_release(th_dvb_table_t *tdt);


/**
 *
 */
dvb_network_t *dvb_network_create(int fe_type, const char *uuid);

//void dvb_network_mux_scanner(void *aux);

void dvb_network_init(void);

idnode_t **dvb_network_root(void);

void dvb_network_schedule_initial_scan(dvb_network_t *dn);


/**
 * Satellite configuration
 */
void dvb_satconf_init(th_dvb_adapter_t *tda);

htsmsg_t *dvb_satconf_list(th_dvb_adapter_t *tda);

htsmsg_t *dvb_lnblist_get(void);

dvb_satconf_t *dvb_satconf_entry_find(th_dvb_adapter_t *tda, 
				      const char *id, int create);

void dvb_lnb_get_frequencies(const char *id, 
			     int *f_low, int *f_hi, int *f_switch);


/**
 * Raw demux
 */
struct th_subscription;
struct th_subscription *dvb_subscription_create_from_tdmi(th_dvb_mux_instance_t *tdmi,
							  const char *name,
							  streaming_target_t *st);

#endif /* DVB_H_ */

