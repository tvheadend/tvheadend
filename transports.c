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

#include "v4l.h"
#include "dvb_dvr.h"
#include "iptv_input.h"
#include "psi.h"

/*
 * transport_mutex protects all operations concerning subscription lists
 */

static pthread_mutex_t subscription_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 *
 */
void
subscription_lock(void)
{
  pthread_mutex_lock(&subscription_mutex);
}

void
subscription_unlock(void)
{
  pthread_mutex_unlock(&subscription_mutex);
}


/*
 *
 */

static void
subscription_lock_check(const char *file, const int line)
{
  if(pthread_mutex_trylock(&subscription_mutex) == EBUSY)
    return;

  fprintf(stderr, "GLW lock not held at %s : %d, crashing\n",
	  file, line);
  abort();
}

#define subscription_lock_assert() subscription_lock_check(__FILE__, __LINE__)


struct th_subscription_list subscriptions;

static void
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




static th_transport_t *
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



static void
subscription_reschedule(void)
{
  th_subscription_t *s;
  th_transport_t *t;

  LIST_FOREACH(s, &subscriptions, ths_global_link) {
    if(s->ths_transport != NULL)
      continue; /* Got a transport, we're happy */

    t = transport_find(s->ths_channel, s->ths_weight);

    if(t == NULL)
      continue;

    s->ths_transport = t;
    LIST_INSERT_HEAD(&t->tht_subscriptions, s, ths_transport_link);
  }
}




static void
auto_reschedule(void *aux)
{
  stimer_add(auto_reschedule, NULL, 10);

  pthread_mutex_lock(&subscription_mutex);
  subscription_reschedule();
  pthread_mutex_unlock(&subscription_mutex);
}





void 
subscription_unsubscribe(th_subscription_t *s)
{
  pthread_mutex_lock(&subscription_mutex);

  s->ths_callback(s, NULL, NULL, AV_NOPTS_VALUE);

  LIST_REMOVE(s, ths_global_link);
  LIST_REMOVE(s, ths_channel_link);

  if(s->ths_transport != NULL) {
    LIST_REMOVE(s, ths_transport_link);
    transport_purge(s->ths_transport);
  }

  if(s->ths_pkt != NULL)
    free(s->ths_pkt);

  free(s->ths_title);
  free(s);

  subscription_reschedule();

  pthread_mutex_unlock(&subscription_mutex);
}





static int
subscription_sort(th_subscription_t *a, th_subscription_t *b)
{
  return b->ths_weight - a->ths_weight;
}


th_subscription_t *
subscription_create(th_channel_t *ch, void *opaque,
		    void (*callback)(struct th_subscription *s, 
				     uint8_t *pkt, th_pid_t *pi, int64_t pcr),
		    unsigned int weight,
		    const char *name)
{
  th_subscription_t *s;

  pthread_mutex_lock(&subscription_mutex);

  s = malloc(sizeof(th_subscription_t));
  s->ths_pkt = NULL;
  s->ths_callback = callback;
  s->ths_opaque = opaque;
  s->ths_title = strdup(name);
  s->ths_total_err = 0;
  time(&s->ths_start);
  s->ths_weight = weight;
  LIST_INSERT_SORTED(&subscriptions, s, ths_global_link, subscription_sort);

  s->ths_channel = ch;
  LIST_INSERT_HEAD(&ch->ch_subscriptions, s, ths_channel_link);

  s->ths_transport = NULL;

  subscription_reschedule();

  if(s->ths_transport == NULL)
    syslog(LOG_NOTICE, "No transponder available for subscription \"%s\" "
	   "to channel \"%s\"",
	   s->ths_title, s->ths_channel->ch_name);

  pthread_mutex_unlock(&subscription_mutex);

  return s;
}

void
subscription_set_weight(th_subscription_t *s, unsigned int weight)
{
  if(s->ths_weight == weight)
    return;

  pthread_mutex_lock(&subscription_mutex);

  LIST_REMOVE(s, ths_global_link);
  s->ths_weight = weight;
  LIST_INSERT_SORTED(&subscriptions, s, ths_global_link, subscription_sort);

  subscription_reschedule();

  pthread_mutex_unlock(&subscription_mutex);
}


/*
 * 
 */

void
transport_flush_subscribers(th_transport_t *t)
{
  th_subscription_t *s;
  
  subscription_lock_assert();

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

  subscription_lock_assert();

  LIST_FOREACH(t, head, tht_adapter_link) {
    LIST_FOREACH(s, &t->tht_subscriptions, ths_transport_link) {
      if(s->ths_weight > w)
	w = s->ths_weight;
    }
  }
  return w;
}



/*
 * Process transport stream packets
 */

void
transport_recv_tsb(th_transport_t *t, int pid, uint8_t *tsb, int scanpcr,
		   int64_t pcr)
{
  th_pid_t *pi = NULL;
  th_subscription_t *s;
  int cc, err = 0, afc, afl = 0;
  int len, pusi;

  LIST_FOREACH(pi, &t->tht_pids, tp_link) 
    if(pi->tp_pid == pid)
      break;

  if(pi == NULL)
    return;

  avgstat_add(&t->tht_rate, 188, dispatch_clock);

  afc = (tsb[3] >> 4) & 3;

  if(afc & 1) {
    cc = tsb[3] & 0xf;
    if(pi->tp_cc_valid && cc != pi->tp_cc) {
      /* Incorrect CC */
      avgstat_add(&t->tht_cc_errors, 1, dispatch_clock);
      err = 1;
    }
    pi->tp_cc_valid = 1;
    pi->tp_cc = (cc + 1) & 0xf;
  }

  if(afc & 2) {
    afl = tsb[4] + 1;

    if(afl > 0 && scanpcr && tsb[5] & 0x10) {
      pcr  = (uint64_t)tsb[6] << 25;
      pcr |= (uint64_t)tsb[7] << 17;
      pcr |= (uint64_t)tsb[8] << 9;
      pcr |= (uint64_t)tsb[9] << 1;
      pcr |= (uint64_t)(tsb[10] >> 7) & 0x01;
    }
  }
  
  switch(pi->tp_type) {

  case HTSTV_TABLE:
    if(pi->tp_section == NULL)
      pi->tp_section = calloc(1, sizeof(struct psi_section));

    afl += 4;
    if(err || afl >= 188) {
      pi->tp_section->ps_offset = -1; /* hold parser until next pusi */
      break;
    }

    pusi = tsb[1] & 0x40;

    if(pusi) {
      len = tsb[afl++];
      if(len > 0) {
	if(len > 188 - afl)
	  break;
	if(!psi_section_reassemble(pi->tp_section, tsb + afl, len, 0, 1))
	  pi->tp_got_section(t, pi, pi->tp_section->ps_data,
			     pi->tp_section->ps_offset);

	afl += len;
      }
    }
    
    if(!psi_section_reassemble(pi->tp_section, tsb + afl, 188 - afl, pusi, 1))
      pi->tp_got_section(t, pi, pi->tp_section->ps_data,
			 pi->tp_section->ps_offset);
    break;

  case HTSTV_TELETEXT:
    teletext_input(t, tsb);
    break;

  default:
    pthread_mutex_lock(&subscription_mutex);
    LIST_FOREACH(s, &t->tht_subscriptions, ths_transport_link) {
      s->ths_total_err += err;
      s->ths_callback(s, tsb, pi, pcr);
    }
    pthread_mutex_unlock(&subscription_mutex);
    break;
  }
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


void
transport_scheduler_init(void)
{
  stimer_add(auto_reschedule, NULL, 60);
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
