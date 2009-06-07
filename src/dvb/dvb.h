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

#include <linux/dvb/frontend.h>

TAILQ_HEAD(th_dvb_adapter_queue, th_dvb_adapter);
RB_HEAD(th_dvb_mux_instance_tree, th_dvb_mux_instance);
TAILQ_HEAD(th_dvb_mux_instance_queue, th_dvb_mux_instance);


enum polarisation {
	POLARISATION_HORIZONTAL     = 0x00,
	POLARISATION_VERTICAL       = 0x01,
	POLARISATION_CIRCULAR_LEFT  = 0x02,
	POLARISATION_CIRCULAR_RIGHT = 0x03
};

#define DVB_FEC_ERROR_LIMIT 20


/**
 * DVB Mux instance
 */
typedef struct th_dvb_mux_instance {

  RB_ENTRY(th_dvb_mux_instance) tdmi_global_link;
  RB_ENTRY(th_dvb_mux_instance) tdmi_adapter_link;


  struct th_dvb_adapter *tdmi_adapter;

  uint16_t tdmi_snr, tdmi_signal;
  uint32_t tdmi_ber, tdmi_uncorrected_blocks;

#define TDMI_FEC_ERR_HISTOGRAM_SIZE 10
  uint32_t tdmi_fec_err_histogram[TDMI_FEC_ERR_HISTOGRAM_SIZE];
  int      tdmi_fec_err_ptr;

  time_t tdmi_time;
  LIST_HEAD(, th_dvb_table) tdmi_tables;

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

  time_t tdmi_got_adapter;
  time_t tdmi_lost_adapter;

  struct dvb_frontend_parameters tdmi_fe_params;
  uint8_t tdmi_polarisation;  /* for DVB-S */
  uint8_t tdmi_switchport;    /* for DVB-S */

  uint16_t tdmi_transport_stream_id;

  char *tdmi_identifier;
  char *tdmi_network;     /* Name of network, from NIT table */

  struct th_transport_list tdmi_transports; /* via tht_mux_link */


  TAILQ_ENTRY(th_dvb_mux_instance) tdmi_scan_link;
  struct th_dvb_mux_instance_queue *tdmi_scan_queue;

} th_dvb_mux_instance_t;


#define DVB_MUX_SCAN_BAD 0      /* On the bad queue */
#define DVB_MUX_SCAN_OK  1      /* Ok, don't need to scan that often */
#define DVB_MUX_SCAN_INITIAL 2  /* To get a scan directly when a mux
				   is discovered */

/**
 * DVB Adapter (one of these per physical adapter)
 */
typedef struct th_dvb_adapter {

  TAILQ_ENTRY(th_dvb_adapter) tda_global_link;

  struct th_dvb_mux_instance_tree tda_muxes;

  /**
   * We keep our muxes on three queues in order to select how
   * they are to be idle-scanned
   */
  struct th_dvb_mux_instance_queue tda_scan_queues[3];
  int tda_scan_selector;  /* To alternate between bad and ok queue */

  th_dvb_mux_instance_t *tda_mux_current;

  int tda_table_epollfd;

  const char *tda_rootpath;
  char *tda_identifier;
  uint32_t tda_autodiscovery;
  char *tda_displayname;

  int tda_fe_fd;
  int tda_type;
  struct dvb_frontend_info *tda_fe_info;

  char *tda_demux_path;

  char *tda_dvr_path;

  gtimer_t tda_mux_scanner_timer;

  pthread_mutex_t tda_delivery_mutex;
  struct th_transport_list tda_transports; /* Currently bound transports */

  gtimer_t tda_fe_monitor_timer;
  int tda_fe_monitor_hold;

} th_dvb_adapter_t;



extern struct th_dvb_adapter_queue dvb_adapters;
extern struct th_dvb_mux_instance_tree dvb_muxes;

void dvb_init(void);

/**
 * DVB Adapter
 */
void dvb_adapter_init(void);

void dvb_adapter_mux_scanner(void *aux);

void dvb_adapter_notify_reload(th_dvb_adapter_t *tda);

void dvb_adapter_set_displayname(th_dvb_adapter_t *tda, const char *s);

void dvb_adapter_set_auto_discovery(th_dvb_adapter_t *tda, int on);

void dvb_adapter_clone(th_dvb_adapter_t *dst, th_dvb_adapter_t *src);

void dvb_adapter_clean(th_dvb_adapter_t *tda);

int dvb_adapter_destroy(th_dvb_adapter_t *tda);

/**
 * DVB Multiplex
 */
void dvb_mux_save(th_dvb_mux_instance_t *tdmi);

void dvb_mux_load(th_dvb_adapter_t *tda);

void dvb_mux_destroy(th_dvb_mux_instance_t *tdmi);

th_dvb_mux_instance_t *dvb_mux_create(th_dvb_adapter_t *tda,
				      struct dvb_frontend_parameters *fe_param,
				      int polarisation, int switchport,
				      uint16_t tsid, const char *network,
				      const char *logprefix);

void dvb_mux_set_networkname(th_dvb_mux_instance_t *tdmi, const char *name);

/**
 * DVB Transport (aka DVB service)
 */
void dvb_transport_load(th_dvb_mux_instance_t *tdmi);

th_transport_t *dvb_transport_find(th_dvb_mux_instance_t *tdmi,
				   uint16_t sid, int pmt_pid, int *created);


/**
 * DVB Frontend
 */
void dvb_fe_tune(th_dvb_mux_instance_t *tdmi, const char *reason);

void dvb_fe_stop(th_dvb_mux_instance_t *tdmi);


/**
 * DVB Tables
 */
void dvb_table_init(th_dvb_adapter_t *tda);

void dvb_table_add_default(th_dvb_mux_instance_t *tdmi);

void dvb_table_add_transport(th_dvb_mux_instance_t *tdmi, th_transport_t *t,
			     int pmt_pid);

void dvb_table_flush_all(th_dvb_mux_instance_t *tdmi);

#endif /* DVB_H_ */
