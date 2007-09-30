/*
 *  tvheadend, transport and subscription functions
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include <libhts/htscfg.h>

#include <ffmpeg/avcodec.h>

#include "tvhead.h"
#include "dispatch.h"
#include "dvb_dvr.h"
#include "teletext.h"
#include "transports.h"
#include "subscriptions.h"

#include "v4l.h"
#include "dvb_dvr.h"
#include "iptv_input.h"
#include "psi.h"

void
transport_purge(th_transport_t *t)
{
  if(LIST_FIRST(&t->tht_subscriptions))
    return;

  switch(t->tht_type) {
#ifdef ENABLE_INPUT_DVB
  case TRANSPORT_DVB:
    dvb_stop_feed(t);
    break;
#endif
#ifdef ENABLE_INPUT_IPTV
  case TRANSPORT_IPTV:
    iptv_stop_feed(t);
    break;
#endif
#ifdef ENABLE_INPUT_V4L
  case TRANSPORT_V4L:
    v4l_stop_feed(t);
    break;
#endif
  default:
    break;
  }

  t->tht_tt_commercial_advice = COMMERCIAL_UNKNOWN;
}




static int
transport_start(th_transport_t *t, unsigned int weight)
{
  t->tht_monitor_suspend = 10;

  switch(t->tht_type) {
#ifdef ENABLE_INPUT_DVB
  case TRANSPORT_DVB:
    return dvb_start_feed(t, weight);
#endif
#ifdef ENABLE_INPUT_IPTV
  case TRANSPORT_IPTV:
    return iptv_start_feed(t, TRANSPORT_RUNNING);
#endif
#ifdef ENABLE_INPUT_V4L
  case TRANSPORT_V4L:
    return v4l_start_feed(t, weight);
#endif
  default:
    return 1;
  }
  return 1;
}




th_transport_t *
transport_find(th_channel_t *ch, unsigned int weight)
{
  th_transport_t *t;

  /* First, try all transports without stealing */

  LIST_FOREACH(t, &ch->ch_transports, tht_channel_link) {
    if(t->tht_status == TRANSPORT_RUNNING) 
      return t;

    if(!transport_start(t, 0))
      return t;
  }

  /* Ok, nothing, try again, but supply our weight and thus, try to steal
     transponders */

  LIST_FOREACH(t, &ch->ch_transports, tht_channel_link) {
    if(!transport_start(t, weight))
      return t;
  }
  return NULL;
}





/*
 * 
 */

void
transport_flush_subscribers(th_transport_t *t)
{
  th_subscription_t *s;
  
  while((s = LIST_FIRST(&t->tht_subscriptions)) != NULL) {
    LIST_REMOVE(s, ths_transport_link);
    s->ths_transport = NULL;
    s->ths_callback(s, NULL, NULL, AV_NOPTS_VALUE);
  }
}

unsigned int 
transport_compute_weight(struct th_transport_list *head)
{
  th_transport_t *t;
  th_subscription_t *s;
  int w = 0;

  LIST_FOREACH(t, head, tht_adapter_link) {
    LIST_FOREACH(s, &t->tht_subscriptions, ths_transport_link) {
      if(s->ths_weight > w)
	w = s->ths_weight;
    }
  }
  return w;
}





static void
transport_monitor(void *aux)
{
  th_transport_t *t = aux;
  int v;

  stimer_add(transport_monitor, t, 1);

  if(t->tht_status == TRANSPORT_IDLE)
    return;

  if(t->tht_monitor_suspend > 0) {
    t->tht_monitor_suspend--;
    return;
  }

  v = avgstat_read_and_expire(&t->tht_rate, dispatch_clock) * 8 / 1000 / 10;

  if(v < 500) {
    switch(t->tht_rate_error_log_limiter) {
    case 0:
      syslog(LOG_WARNING, "\"%s\" on \"%s\", very low bitrate: %d kb/s",
	     t->tht_channel->ch_name, t->tht_name, v);
      /* FALLTHRU */
    case 1 ... 9:
      t->tht_rate_error_log_limiter++;
      break;
    }
  } else {
    switch(t->tht_rate_error_log_limiter) {
    case 0:
      break;

    case 1:
      syslog(LOG_NOTICE, "\"%s\" on \"%s\", bitrate ok (%d kb/s)",
	     t->tht_channel->ch_name, t->tht_name, v);
      /* FALLTHRU */
    default:
      t->tht_rate_error_log_limiter--;
    }
  }


  v = avgstat_read(&t->tht_cc_errors, 10, dispatch_clock);
  if(v > t->tht_cc_error_log_limiter) {
    t->tht_cc_error_log_limiter += v * 3;

    syslog(LOG_WARNING, "\"%s\" on \"%s\", "
	   "%.2f continuity errors/s (10 sec average)",
	   t->tht_channel->ch_name, t->tht_name, (float)v / 10.);

  } else if(v == 0) {
    switch(t->tht_cc_error_log_limiter) {
    case 0:
      break;
    case 1:
      syslog(LOG_NOTICE, "\"%s\" on \"%s\", no continuity errors",
	     t->tht_channel->ch_name, t->tht_name);
      /* FALLTHRU */
    default:
      t->tht_cc_error_log_limiter--;
    }
  }
}


void
transport_monitor_init(th_transport_t *t)
{
  avgstat_init(&t->tht_cc_errors, 3600);
  avgstat_init(&t->tht_rate, 10);

  stimer_add(transport_monitor, t, 5);
}

th_pid_t *
transport_add_pid(th_transport_t *t, uint16_t pid, tv_streamtype_t type)
{
  th_pid_t *pi;
  int i = 0;
  LIST_FOREACH(pi, &t->tht_pids, tp_link) {
    i++;
    if(pi->tp_pid == pid)
      return pi;
  }

  pi = calloc(1, sizeof(th_pid_t));
  pi->tp_index = i;
  pi->tp_pid = pid;
  pi->tp_type = type;
  pi->tp_demuxer_fd = -1;

  LIST_INSERT_HEAD(&t->tht_pids, pi, tp_link);
  return pi;
}
