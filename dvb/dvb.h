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

enum polarisation {
	POLARISATION_HORIZONTAL     = 0x00,
	POLARISATION_VERTICAL       = 0x01,
	POLARISATION_CIRCULAR_LEFT  = 0x02,
	POLARISATION_CIRCULAR_RIGHT = 0x03
};

#define DVB_FEC_ERROR_LIMIT 20

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
void dvb_fe_tune(th_dvb_mux_instance_t *tdmi);

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
