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
#include "webui/webui.h"

void
notify_by_msg(const char *class, htsmsg_t *m)
{
  htsmsg_add_str(m, "notificationClass", class);
  comet_mailbox_add_message(m);
  htsmsg_destroy(m);
}

/**
 *
 */
void
notify_transprot_status_change(struct th_transport *t)
{
  th_subscription_t *s;

  LIST_FOREACH(s, &t->tht_subscriptions, ths_transport_link)
    if(s->ths_status_callback != NULL)
      s->ths_status_callback(s, t->tht_last_status, s->ths_opaque);
 
  //  ajax_mailbox_transport_status_change(t);
}
