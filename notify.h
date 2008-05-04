/*
 *  tvheadend, Notification framework
 *  Copyright (C) 2008 Andreas Öman
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

#ifndef NOTIFY_H_
#define NOTIFY_H_

struct xmltv_grabber;
struct th_dvb_mux_instance;
struct th_dvb_adapterr;

void notify_tdmi_state_change(struct th_dvb_mux_instance *tdmi);

void notify_tdmi_name_change(struct th_dvb_mux_instance *tdmi);

void notify_tdmi_status_change(struct th_dvb_mux_instance *tdmi);

void notify_tdmi_qual_change(struct th_dvb_mux_instance *tdmi);

void notify_tdmi_services_change(struct th_dvb_mux_instance *tdmi);

void notify_tda_change(struct th_dvb_adapter *tda);

void notify_xmltv_grabber_status_change(struct xmltv_grabber *xg);

struct th_transport;
void notify_transprot_status_change(struct th_transport *t);

#endif /* NOTIFY_H_ */
