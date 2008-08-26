/*
 *  Output functions for fixed multicast streaming
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

#include <assert.h>
#include <pthread.h>
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

#include "tvhead.h"
#include "iptv_output.h"
#include "dispatch.h"
#include "channels.h"
#include "subscriptions.h"
#include "serviceprobe.h"
#include "transports.h"
#include "mux.h"

static void serviceprobe_engage(void);

TAILQ_HEAD(sp_queue, sp);

static struct sp_queue probequeue;

static struct sp *sp_current;

static dtimer_t sp_engage_timer;

typedef struct sp {
  TAILQ_ENTRY(sp) sp_link;

  th_muxer_t *sp_muxer;
  th_transport_t *sp_t;
  dtimer_t sp_timer;

  th_subscription_t *sp_s;

} sp_t;


static void
sp_done_callback(void *aux, int64_t now)
{
  sp_t *sp = aux;
  th_subscription_t *s = sp->sp_s;

  if(s != NULL) {
    assert(sp == sp_current);
    sp_current = NULL;
    subscription_unsubscribe(s);
  }

  serviceprobe_engage();

  sp->sp_t->tht_sp = NULL;

  TAILQ_REMOVE(&probequeue, sp, sp_link);
  free(sp);
}

/**
 * Got a packet, map it
 */
static void
sp_packet_input(void *opaque, th_muxstream_t *tms, th_pkt_t *pkt)
{
  sp_t *sp = opaque;
  th_transport_t *t = sp->sp_t;
  channel_t *ch;

  tvhlog(LOG_INFO, "serviceprobe", "Probed \"%s\" -- Ok", t->tht_svcname);

  if(t->tht_ch == NULL && t->tht_svcname != NULL) {
    ch = channel_find_by_name(t->tht_svcname, 1);
    transport_map_channel(t, ch);
    
    t->tht_config_change(t);
  }
  dtimer_arm(&sp->sp_timer, sp_done_callback, sp, 0);
}

/**
 * Callback when transport changes status
 */
static void
sp_status_callback(struct th_subscription *s, int status, void *opaque)
{
  sp_t *sp = opaque;
  th_transport_t *t = sp->sp_t;
  char *errtxt;

  s->ths_status_callback = NULL;

  switch(status) {
  case TRANSPORT_STATUS_OK:
    return;
  case TRANSPORT_STATUS_NO_DESCRAMBLER:
    errtxt = "No descrambler for stream";
    break;
  case TRANSPORT_STATUS_NO_ACCESS:
    errtxt = "Access denied";
    break;
  case TRANSPORT_STATUS_NO_INPUT:
    errtxt = "No video detected";
    break;
  default:
    errtxt = "Other error";
    break;
  }

  tvhlog(LOG_INFO, "serviceprobe",
	 "Probed \"%s\" -- %s", t->tht_svcname, errtxt);
  dtimer_arm(&sp->sp_timer, sp_done_callback, sp, 0);
}


/**
 * Called when a subscription gets/loses access to a transport
 */
static void
sp_subscription_callback(struct th_subscription *s,
			 subscription_event_t event, void *opaque)
{
  sp_t *sp = opaque;

  switch(event) {
  case TRANSPORT_AVAILABLE:
    break;

  case TRANSPORT_UNAVAILABLE:
    muxer_deinit(sp->sp_muxer, s);
    break;
  }
}

/**
 * Setup IPTV (TS over UDP) output
 */

static void
serviceprobe_start(void *aux, int64_t now)
{
  th_subscription_t *s;
  th_muxer_t *tm;
  th_transport_t *t;
  sp_t *sp;

  assert(sp_current == NULL);

  if((sp = TAILQ_FIRST(&probequeue)) == NULL) {
    tvhlog(LOG_NOTICE, "serviceprobe", "Nothing more to probe");
    return;
  }
  s = sp->sp_s = calloc(1, sizeof(th_subscription_t));
  t = sp->sp_t;

  sp_current = sp;

  s->ths_title     = strdup("probe");
  s->ths_weight    = INT32_MAX;
  s->ths_opaque    = sp;
  s->ths_callback  = sp_subscription_callback;
  LIST_INSERT_HEAD(&subscriptions, s, ths_global_link);


  if(t->tht_runstatus != TRANSPORT_RUNNING)
    transport_start(t, INT32_MAX, 1);

  s->ths_transport = t;
  LIST_INSERT_HEAD(&t->tht_subscriptions, s, ths_transport_link);

  sp->sp_muxer = tm = muxer_init(s, sp_packet_input, sp);
  muxer_play(tm, AV_NOPTS_VALUE);

  s->ths_status_callback = sp_status_callback;
}

/**
 *
 */
static void
serviceprobe_engage(void)
{
  dtimer_arm(&sp_engage_timer, serviceprobe_start, NULL, 0);
}


/**
 *
 */
void
serviceprobe_add(th_transport_t *t)
{
  sp_t *sp;

  if(!transport_is_tv(t))
    return;

  if(t->tht_sp != NULL)
    return;

  sp = calloc(1, sizeof(sp_t));

  TAILQ_INSERT_TAIL(&probequeue, sp, sp_link);
  t->tht_sp = sp;
  sp->sp_t = t;

  if(sp_current == NULL)
    serviceprobe_engage();
}

/**
 *
 */
void
serviceprobe_delete(th_transport_t *t)
{
  if(t->tht_sp == NULL)
    return;

  sp_done_callback(t->tht_sp, 0);
}


void 
serviceprobe_setup(void)
{
  TAILQ_INIT(&probequeue);
}
