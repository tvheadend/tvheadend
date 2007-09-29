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
 * subscriptions_mutex protects all operations concerning subscription lists
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


struct th_subscription_list subscriptions;

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


void
subscriptions_init(void)
{
  stimer_add(auto_reschedule, NULL, 60);
}
