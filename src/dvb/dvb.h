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

#include "device.h"
#include "htsmsg.h"
#include "redblack.h"
#include "service.h"

TAILQ_HEAD(th_dvb_adapter_queue, th_dvb_adapter);
RB_HEAD(th_dvb_mux_instance_tree, th_dvb_mux_instance);
TAILQ_HEAD(th_dvb_mux_instance_queue, th_dvb_mux_instance);
LIST_HEAD(th_dvb_mux_instance_list, th_dvb_mux_instance);
TAILQ_HEAD(dvb_satconf_queue, dvb_satconf);


/**
 * Satconf
 */
typedef struct dvb_satconf {
  char *sc_id;
  TAILQ_ENTRY(dvb_satconf) sc_adapter_link;
  int sc_port;                   // diseqc switchport (0 - 15)

  char *sc_name;
  char *sc_comment;
  char *sc_lnb;

  struct th_dvb_mux_instance_list sc_tdmis;

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
  dvb_satconf_t *dmc_satconf;
#if DVB_API_VERSION >= 5
  fe_modulation_t dmc_fe_modulation;
  fe_delivery_system_t dmc_fe_delsys;
  fe_rolloff_t dmc_fe_rolloff;
#endif
} dvb_mux_conf_t;


/**
 * DVB Mux instance
 */
typedef struct th_dvb_mux_instance {

  RB_ENTRY(th_dvb_mux_instance) tdmi_global_link;

  LIST_ENTRY(th_dvb_mux_instance) tdmi_adapter_link;
  LIST_ENTRY(th_dvb_mux_instance) tdmi_adapter_hash_link;

  struct th_dvb_adapter *tdmi_adapter;

  uint16_t tdmi_snr, tdmi_signal;
  uint32_t tdmi_ber, tdmi_uncorrected_blocks;

#define TDMI_FEC_ERR_HISTOGRAM_SIZE 10
  uint32_t tdmi_fec_err_histogram[TDMI_FEC_ERR_HISTOGRAM_SIZE];
  int      tdmi_fec_err_ptr;

  time_t tdmi_time;


  LIST_HEAD(, th_dvb_table) tdmi_tables;
  TAILQ_HEAD(, th_dvb_table) tdmi_table_queue;
  int tdmi_table_initial;

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

  int tdmi_enabled;

  time_t tdmi_got_adapter;
  time_t tdmi_lost_adapter;

  dvb_mux_conf_t tdmi_conf;

  /* Linked if tdmi_conf.dmc_satconf != NULL */
  LIST_ENTRY(th_dvb_mux_instance) tdmi_satconf_link;

  uint16_t tdmi_transport_stream_id;

  char *tdmi_identifier;
  char *tdmi_network;     /* Name of network, from NIT table */

  struct service_list tdmi_transports; /* via s_mux_link */


  TAILQ_ENTRY(th_dvb_mux_instance) tdmi_scan_link;
  struct th_dvb_mux_instance_queue *tdmi_scan_queue;


  int tdmi_type;

} th_dvb_mux_instance_t;


/**
 * DVB Adapter (one of these per physical adapter)
 */
#define TDA_MUX_HASH_WIDTH 101

typedef struct dvb_adapter {

  device_t da_dev;

  uint32_t da_diseqc_version;

  char *da_demux_path;
  char *da_dvr_path;



#if 0

  struct th_dvb_mux_instance_list tda_muxes;

  struct th_dvb_mux_instance_queue tda_scan_queues[2];
  int tda_scan_selector;

  th_dvb_mux_instance_t *tda_mux_current;

  int tda_table_epollfd;

  const char *tda_rootpath;
  char *tda_identifier;
  uint32_t tda_autodiscovery;
  uint32_t tda_idlescan;
  uint32_t tda_qmon;
  uint32_t tda_nitoid;
  char *tda_displayname;

  int tda_fe_fd;
  struct dvb_frontend_info *tda_fe_info;

  int tda_adapter_num;

  char *tda_demux_path;

  char *tda_dvr_path;

  int tda_hostconnection;

  gtimer_t tda_mux_scanner_timer;

  pthread_mutex_t tda_delivery_mutex;
  struct service_list tda_transports; /* Currently bound transports */

  gtimer_t tda_fe_monitor_timer;
  int tda_fe_monitor_hold;

  int tda_sat; // Set if this adapter is a satellite receiver (DVB-S, etc) 

  struct dvb_satconf_queue tda_satconfs;


  struct th_dvb_mux_instance_list tda_mux_hash[TDA_MUX_HASH_WIDTH];

  uint32_t tda_dump_muxes;

  int tda_allpids_dmx_fd;
  int tda_dump_fd;
#endif

} dvb_adapter_t;



extern struct th_dvb_adapter_queue dvb_adapters;
extern struct th_dvb_mux_instance_tree dvb_muxes;

void dvb_init(void);

/**
 * DVB Adapter
 */
void dvb_adapter_init(void);

void dvb_adapter_mux_scanner(void *aux);

void dvb_adapter_set_displayname(dvb_adapter_t *tda, const char *s);

void dvb_adapter_set_auto_discovery(dvb_adapter_t *tda, int on);

void dvb_adapter_set_idlescan(dvb_adapter_t *tda, int on);

void dvb_adapter_set_qmon(dvb_adapter_t *tda, int on);

void dvb_adapter_set_dump_muxes(dvb_adapter_t *tda, int on);

void dvb_adapter_set_nitoid(dvb_adapter_t *tda, int nitoid);

void dvb_adapter_set_diseqc_version(dvb_adapter_t *tda, unsigned int v);

void dvb_adapter_clone(dvb_adapter_t *dst, dvb_adapter_t *src);

void dvb_adapter_clean(dvb_adapter_t *tda);

int dvb_adapter_destroy(dvb_adapter_t *tda);

void dvb_adapter_notify(dvb_adapter_t *tda);

htsmsg_t *dvb_adapter_build_msg(dvb_adapter_t *tda);

htsmsg_t *dvb_fe_opts(dvb_adapter_t *tda, const char *which);

void dvb_fullmux_init(dvb_adapter_t *da);


/**
 * DVB Multiplex
 */
const char* dvb_mux_fec2str(int fec);
const char* dvb_mux_delsys2str(int delsys);
const char* dvb_mux_qam2str(int qam);
const char* dvb_mux_rolloff2str(int rolloff);

void dvb_mux_save(th_dvb_mux_instance_t *tdmi);

void dvb_mux_load(dvb_adapter_t *tda);

void dvb_mux_destroy(th_dvb_mux_instance_t *tdmi);

th_dvb_mux_instance_t *dvb_mux_create(dvb_adapter_t *tda,
				      const struct dvb_mux_conf *dmc,
				      uint16_t tsid, const char *network,
				      const char *logprefix, int enabled,
				      int initialscan, const char *identifier);

void dvb_mux_set_networkname(th_dvb_mux_instance_t *tdmi, const char *name);

void dvb_mux_set_tsid(th_dvb_mux_instance_t *tdmi, uint16_t tsid);

void dvb_mux_set_enable(th_dvb_mux_instance_t *tdmi, int enabled);

void dvb_mux_set_satconf(th_dvb_mux_instance_t *tdmi, const char *scid,
			 int save);

htsmsg_t *dvb_mux_build_msg(th_dvb_mux_instance_t *tdmi);

void dvb_mux_notify(th_dvb_mux_instance_t *tdmi);

const char *dvb_mux_add_by_params(dvb_adapter_t *tda,
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

int dvb_mux_copy(dvb_adapter_t *dst, th_dvb_mux_instance_t *tdmi_src);

/**
 * DVB Transport (aka DVB service)
 */
void dvb_transport_load(th_dvb_mux_instance_t *tdmi);

struct service *dvb_transport_find(th_dvb_mux_instance_t *tdmi,
				   uint16_t sid, int pmt_pid,
				   const char *identifier);

void dvb_transport_notify(struct service *t);

void dvb_transport_notify_by_adapter(dvb_adapter_t *tda);

htsmsg_t *dvb_transport_build_msg(struct service *t);

int dvb_transport_get_signal_status(struct service *t,
				    signal_status_t *status);

/**
 * DVB Frontend
 */
int dvb_fe_tune(th_dvb_mux_instance_t *tdmi, const char *reason);

void dvb_fe_stop(th_dvb_mux_instance_t *tdmi);


/**
 * DVB Tables
 */
void dvb_table_init(dvb_adapter_t *tda);

void dvb_table_add_default(th_dvb_mux_instance_t *tdmi);

void dvb_table_flush_all(th_dvb_mux_instance_t *tdmi);

/**
 * Satellite configuration
 */
void dvb_satconf_init(dvb_adapter_t *tda);

htsmsg_t *dvb_satconf_list(dvb_adapter_t *tda);

htsmsg_t *dvb_lnblist_get(void);

dvb_satconf_t *dvb_satconf_entry_find(dvb_adapter_t *tda, 
				      const char *id, int create);

void dvb_lnb_get_frequencies(const char *id, 
			     int *f_low, int *f_hi, int *f_switch);

#endif /* DVB_H_ */
