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

extern struct th_dvb_adapter_list dvb_adapters;
extern struct th_dvb_mux_instance_list dvb_muxes;

void dvb_init(void);

void dvb_tune_tdmi(th_dvb_mux_instance_t *tdmi, int maylog,
		   tdmi_state_t state);

void dvb_table_add_default(th_dvb_mux_instance_t *tdmi);

void dvb_table_add_transport(th_dvb_mux_instance_t *tdmi, th_transport_t *t,
			     int pmt_pid);

void dvb_tdt_destroy(th_dvb_table_t *tdt);

void dvb_fe_start(th_dvb_adapter_t *tda);

void tdmi_check_scan_status(th_dvb_mux_instance_t *tdmi);

th_transport_t *dvb_find_transport(th_dvb_mux_instance_t *tdmi,
				   uint16_t sid, int pmt_pid, int *created);

th_dvb_mux_instance_t *dvb_mux_create(th_dvb_adapter_t *tda,
				      struct dvb_frontend_parameters *fe_param,
				      int polarisation, int switchport,
				      int save, uint16_t tsid);

void dvb_tdmi_save(th_dvb_mux_instance_t *tdmi);

void dvb_tda_save(th_dvb_adapter_t *tda);

#endif /* DVB_H_ */
