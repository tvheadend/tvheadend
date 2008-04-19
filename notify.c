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

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "tvhead.h"
#include "notify.h"
#include "ajaxui/ajaxui_mailbox.h"

void
notify_tdmi_state_change(th_dvb_mux_instance_t *tdmi)
{
  ajax_mailbox_tdmi_state_change(tdmi);
}


void
notify_tdmi_name_change(th_dvb_mux_instance_t *tdmi)
{
  ajax_mailbox_tdmi_name_change(tdmi);
}



void
notify_tdmi_status_change(th_dvb_mux_instance_t *tdmi)
{
  ajax_mailbox_tdmi_status_change(tdmi);
}


void
notify_tdmi_services_change(th_dvb_mux_instance_t *tdmi)
{
  ajax_mailbox_tdmi_services_change(tdmi);
}


void
notify_tda_change(th_dvb_adapter_t *tda)
{
  ajax_mailbox_tda_change(tda);
}

void
notify_xmltv_grabber_status_change(struct xmltv_grabber *xg, int status)
{
  ajax_mailbox_xmltv_grabber_status_change(xg, status);
}
