/*
 *  tvheadend, HTSP streaming
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

#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <libhts/htsp.h>

#include "tvhead.h"
#include "channels.h"
#include "subscriptions.h"
#include "dispatch.h"
#include "epg.h"
#include "pvr.h"
#include "htsp_muxer.h"



/*
 * Called when a subscription gets/loses access to a transport
 */
static void
client_subscription_callback(struct th_subscription *s,
			     subscription_event_t event, void *opaque)
{
  htsp_connection_t *hc = opaque;

  switch(event) {
  case TRANSPORT_AVAILABLE:
    break;

  case TRANSPORT_UNAVAILABLE:
    break;
  }
}

/*
 *
 */
void
htsp_muxer_subscribe(htsp_connection_t *hc, th_channel_t *ch, int weight)
{
  th_subscription_t *s;
  th_muxer_t *m;

  LIST_FOREACH(s, &hc->hc_subscriptions, ths_subscriber_link) {
    if(s->ths_channel == ch) {
      subscription_set_weight(s, weight);
      return;
    }
  }

  m = calloc(1, sizeof(th_muxer_t));

  LIST_INSERT_HEAD(&hc->hc_subscriptions, s, ths_subscriber_link);
}

/*
 *
 */
void
htsp_muxer_unsubscribe(htsp_connection_t *hc, th_channel_t *ch)
{
  th_subscription_t *s;
  htsp_muxerer_t *htsp;

  LIST_FOREACH(s, &hc->hc_subscriptions, ths_subscriber_link) {
    if(s->ths_channel == ch)
      break;
  }

  if(s != NULL) {
    LIST_REMOVE(s, ths_subscriber_link);
    free(s->hs_opaque);
    subscription_unsubscribe(s);
  }
}

/*
 *
 */
void
htsp_muxer_cleanup(htsp_connection_t *hc)
{
  th_subscription_t *s;

  while((s = LIST_FIRST(&hc->hc_subscriptions)) != NULL) {
    LIST_REMOVE(s, ths_subscriber_link);
    subscription_unsubscribe(s);
  }
}

