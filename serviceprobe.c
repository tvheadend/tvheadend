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

struct th_transport_queue probequeue;

typedef struct sp {
  dtimer_t sp_timer;
  th_muxer_t *sp_muxer;
  th_subscription_t *sp_s;
  int sp_ok;
} sp_t;


static void
sp_done(sp_t *sp)
{
  th_subscription_t *s = sp->sp_s;
  th_transport_t *t = sp->sp_s->ths_transport;

  dtimer_disarm(&sp->sp_timer);

  muxer_deinit(sp->sp_muxer, s);

  LIST_REMOVE(s, ths_transport_link);
  free(s);

  TAILQ_REMOVE(&probequeue, t, tht_probe_link);
  t->tht_on_probe_queue = 0;

  transport_stop(t, 0);

  subscription_reschedule();

  serviceprobe_engage();
  free(sp);
}


/**
 *
 */
static void
sp_timeout(void *aux, int64_t now)
{
  sp_t *sp = aux;
  th_transport_t *t = sp->sp_s->ths_transport;

  syslog(LOG_INFO,
	 "Probe %6s %s\n", 
	 sp->sp_ok ? "Ok" : "Failed", t->tht_servicename);
 
  if(sp->sp_ok) {
    if(t->tht_channel == NULL && t->tht_servicename != NULL) {
      transport_set_channel(t, t->tht_servicename);
      t->tht_config_change(t);
    }
  }

  sp_done(sp);
}


static void
sp_packet_input(void *opaque, th_muxstream_t *tms, th_pkt_t *pkt)
{
  sp_t *sp = opaque;

  if(tms->tms_stream->st_type == HTSTV_MPEG2VIDEO ||
     tms->tms_stream->st_type == HTSTV_H264) {
    sp->sp_ok = 1;
    dtimer_arm(&sp->sp_timer, sp_timeout, sp, 0);
  }
}

/**
 * Callback when transport hits an error
 */
static void
sp_err_callback(struct th_subscription *s, int errorcode, void *opaque)
{
  sp_t *sp = opaque;

  sp->sp_ok = 0;
  dtimer_arm(&sp->sp_timer, sp_timeout, sp, 0);
}


/**
 * Setup IPTV (TS over UDP) output
 */

static void
serviceprobe_engage(void)
{
  th_transport_t *t;
  th_subscription_t *s;
  th_muxer_t *tm;
  sp_t *sp;

  if((t = TAILQ_FIRST(&probequeue)) == NULL)
    return;

  sp = calloc(1, sizeof(sp_t));

  sp->sp_s = s = calloc(1, sizeof(th_subscription_t));
  s->ths_title     = "probe";
  s->ths_weight    = INT32_MAX;
  s->ths_opaque    = sp;

  if(t->tht_status != TRANSPORT_RUNNING)
    transport_start(t, INT32_MAX);

  s->ths_transport = t;
  LIST_INSERT_HEAD(&t->tht_subscriptions, s, ths_transport_link);

  sp->sp_muxer = tm = muxer_init(s, sp_packet_input, sp);
  muxer_play(tm, AV_NOPTS_VALUE);

  s->ths_err_callback = sp_err_callback;

  dtimer_arm(&sp->sp_timer, sp_timeout, sp, 4);
}






/**
 *
 */
void
serviceprobe_add(th_transport_t *t)
{
  int was_first = TAILQ_FIRST(&probequeue) == NULL;

  if(t->tht_on_probe_queue)
    return;

  TAILQ_INSERT_TAIL(&probequeue, t, tht_probe_link);
  t->tht_on_probe_queue = 1;

  if(was_first)
    serviceprobe_engage();
}



void 
serviceprobe_setup(void)
{
  TAILQ_INIT(&probequeue);
}
