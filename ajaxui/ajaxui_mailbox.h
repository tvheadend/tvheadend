/*
 *  tvheadend, AJAX user interface
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
 *  along with this program.  If not, see <htmlui://www.gnu.org/licenses/>.
 */

#ifndef AJAXUI_MAILBOX_H_
#define AJAXUI_MAILBOX_H_

#include "tcp.h"

int ajax_mailbox_create(const char *subscriptionid);

void ajax_mailbox_tdmi_state_change(th_dvb_mux_instance_t *tdmi);

void ajax_mailbox_tdmi_name_change(th_dvb_mux_instance_t *tdmi);

void ajax_mailbox_tdmi_status_change(th_dvb_mux_instance_t *tdmi);

void ajax_mailbox_tdmi_services_change(th_dvb_mux_instance_t *tdmi);

void ajax_mailbox_tda_change(th_dvb_adapter_t *tda);

void ajax_mailbox_start(tcp_queue_t *tq, const char *identifier);

#endif /* AJAXUI_MAILBOX_H_ */
