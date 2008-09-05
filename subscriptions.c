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

#include <assert.h>
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

#include "tvhead.h"
#include "dispatch.h"
#include "transports.h"
#include "subscriptions.h"

struct th_subscription_list subscriptions;
static pthread_cond_t subscription_janitor_cond;
static pthread_mutex_t subscription_janitor_mutex;
static int subscription_janitor_work;
static gtimer_t subscription_reschedule_timer;


static int
subscription_sort(th_subscription_t *a, th_subscription_t *b)
{
  return b->ths_weight - a->ths_weight;
}

/**
 *
 */
static void
subscription_link_transport(th_subscription_t *s, th_transport_t *t)
{
  s->ths_transport = t;
  LIST_INSERT_HEAD(&t->tht_subscriptions, s, ths_transport_link);
  s->ths_event_callback(s, SUBSCRIPTION_TRANSPORT_RUN, s->ths_opaque);
 
  s->ths_last_status = t->tht_last_status;
  if(s->ths_last_status != SUBSCRIPTION_EVENT_INVALID)
    s->ths_event_callback(s, s->ths_last_status, s->ths_opaque);
}

/**
 *
 */
static void
subscription_reschedule(void *aux)
{
  th_subscription_t *s;
  th_transport_t *t;

  lock_assert(&global_lock);

  gtimer_arm(&subscription_reschedule_timer, subscription_reschedule, NULL, 2);

  LIST_FOREACH(s, &subscriptions, ths_global_link) {
    if(s->ths_transport != NULL)
      continue; /* Got a transport, we're happy */

    if(s->ths_channel == NULL)
      continue; /* stale entry, channel has been destroyed */

    t = transport_find(s->ths_channel, s->ths_weight);

    if(t == NULL) {
      /* No transport available */
      s->ths_event_callback(s, SUBSCRIPTION_TRANSPORT_NOT_AVAILABLE,
			    s->ths_opaque);
      continue;
    }

    subscription_link_transport(s, t);
  }
}

/**
 *
 */
void 
subscription_unsubscribe(th_subscription_t *s)
{
  th_transport_t *t = s->ths_transport;

  lock_assert(&global_lock);

  LIST_REMOVE(s, ths_global_link);

  if(s->ths_channel != NULL)
    LIST_REMOVE(s, ths_channel_link);

  if(t != NULL)
    transport_remove_subscriber(t, s);

  free(s->ths_title);
  free(s);

  subscription_reschedule(NULL);
}

/**
 *
 */
static th_subscription_t *
subscription_create(int weight, const char *name,
		    ths_event_callback_t *cb, void *opaque)
{
  th_subscription_t *s = calloc(1, sizeof(th_subscription_t));

  s->ths_weight          = weight;
  s->ths_event_callback  = cb;
  s->ths_opaque          = opaque;
  s->ths_title           = strdup(name);
  s->ths_total_err       = 0;

  time(&s->ths_start);
  LIST_INSERT_SORTED(&subscriptions, s, ths_global_link, subscription_sort);

  return s;
}


/**
 *
 */
th_subscription_t *
subscription_create_from_channel(channel_t *ch,
				 unsigned int weight, const char *name,
				 ths_event_callback_t *cb, void *opaque)
{
  th_subscription_t *s = subscription_create(weight, name, cb, opaque);

  s->ths_channel = ch;
  LIST_INSERT_HEAD(&ch->ch_subscriptions, s, ths_channel_link);
  s->ths_transport = NULL;

  subscription_reschedule(NULL);

  if(s->ths_transport == NULL)
    tvhlog(LOG_NOTICE, "subscription", 
	   "No transponder available for subscription \"%s\" "
	   "to channel \"%s\"",
	   s->ths_title, s->ths_channel->ch_name);

  return s;
}


/**
 *
 */
th_subscription_t *
subscription_create_from_transport(th_transport_t *t, const char *name,
				   ths_event_callback_t *cb, void *opaque)
{
  th_subscription_t *s = subscription_create(INT32_MAX, name, cb, opaque);

  if(t->tht_runstatus != TRANSPORT_RUNNING)
    transport_start(t, INT32_MAX, 1);
  
  subscription_link_transport(s, t);
  return s;
}


/**
 *
 */
void
subscription_janitor_has_duty(void)
{
  pthread_mutex_lock(&subscription_janitor_mutex);
  subscription_janitor_work++;
  pthread_cond_signal(&subscription_janitor_cond);
  pthread_mutex_unlock(&subscription_janitor_mutex);
}



/**
 *
 */
static void *
subscription_janitor(void *aux)
{
  int v;
  th_subscription_t *s;
  th_transport_t *t;

  pthread_mutex_lock(&subscription_janitor_mutex);

  v = subscription_janitor_work;

  while(1) {

    while(v == subscription_janitor_work)
      pthread_cond_wait(&subscription_janitor_cond,
			&subscription_janitor_mutex);
    
    v = subscription_janitor_work;
    pthread_mutex_unlock(&subscription_janitor_mutex);

    pthread_mutex_lock(&global_lock);

    LIST_FOREACH(s, &subscriptions, ths_global_link) {
      if((t = s->ths_transport) == NULL)
	continue;
      
      if(s->ths_last_status != t->tht_last_status) {
	s->ths_last_status = t->tht_last_status;
	s->ths_event_callback(s, s->ths_last_status, s->ths_opaque);
      }
    }

    pthread_mutex_unlock(&global_lock);
  }
}




/**
 *
 */
void
subscriptions_init(void)
{
  pthread_t ptid;

  pthread_cond_init(&subscription_janitor_cond, NULL);
  pthread_mutex_init(&subscription_janitor_mutex, NULL);

  pthread_create(&ptid, NULL, subscription_janitor, NULL);
}

