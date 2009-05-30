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
#include <errno.h>




#include "tvhead.h"
#include "channels.h"
#include "subscriptions.h"
#include "serviceprobe.h"
#include "transports.h"


/* List of transports to be probed, protected with global_lock */
static struct th_transport_queue serviceprobe_queue;  
static pthread_cond_t serviceprobe_cond;

/**
 *
 */
void
serviceprobe_enqueue(th_transport_t *t)
{
  if(!transport_is_tv(t))
    return; /* Don't even consider non-tv channels */

  if(t->tht_sp_onqueue)
    return;

  if(t->tht_ch != NULL)
    return; /* Already mapped */

  t->tht_sp_onqueue = 1;
  TAILQ_INSERT_TAIL(&serviceprobe_queue, t, tht_sp_link);
  pthread_cond_signal(&serviceprobe_cond);
}


/**
 *
 */
static void
serviceprobe_callback(struct th_subscription *s, subscription_event_t event,
		      void *opaque)
{
  th_transport_t *t = opaque;
  channel_t *ch;
  const char *errmsg;

  switch(event) {
  case SUBSCRIPTION_TRANSPORT_RUN:
    return;

  case SUBSCRIPTION_NO_INPUT:
    errmsg = "No input detected";
    break;

  case SUBSCRIPTION_NO_DESCRAMBLER:
    errmsg = "No descrambler available";
    break;

  case SUBSCRIPTION_NO_ACCESS:
    errmsg = "Access denied";
    break;

  case SUBSCRIPTION_RAW_INPUT:
    errmsg = "Unable to reassemble packets from input";
    break;

  case SUBSCRIPTION_VALID_PACKETS:
    errmsg = NULL; /* All OK */
    break;

  case SUBSCRIPTION_TRANSPORT_NOT_AVAILABLE:
  case SUBSCRIPTION_TRANSPORT_LOST:
    errmsg = "Unable to probe";
    break;

  case SUBSCRIPTION_DESTROYED:
    return; /* All done */

  default:
    abort();
  }

  assert(t == TAILQ_FIRST(&serviceprobe_queue));


  if(errmsg != NULL) {
    tvhlog(LOG_INFO, "serviceprobe", "%20s: skipped: %s",
	   t->tht_svcname, errmsg);
  } else if(t->tht_ch == NULL) {
    ch = channel_find_by_name(t->tht_svcname, 1);
    transport_map_channel(t, ch);
    
      pthread_mutex_lock(&t->tht_stream_mutex);
      t->tht_config_change(t);
      pthread_mutex_unlock(&t->tht_stream_mutex);
      
      tvhlog(LOG_INFO, "serviceprobe", "\"%s\" mapped to channel \"%s\"",
	     t->tht_svcname, t->tht_svcname);
  }

  t->tht_sp_onqueue = 0;
  TAILQ_REMOVE(&serviceprobe_queue, t, tht_sp_link);
  pthread_cond_signal(&serviceprobe_cond);
}

/**
 *
 */
static void *
serviceprobe_thread(void *aux)
{
  th_transport_t *t;
  th_subscription_t *s;
  int was_doing_work = 0;

  pthread_mutex_lock(&global_lock);

  while(1) {

    while((t = TAILQ_FIRST(&serviceprobe_queue)) == NULL) {

      if(was_doing_work) {
	tvhlog(LOG_INFO, "serviceprobe", "Now idle");
	was_doing_work = 0;
      }
      pthread_cond_wait(&serviceprobe_cond, &global_lock);
    }

    if(!was_doing_work) {
      tvhlog(LOG_INFO, "serviceprobe", "Starting");
    }

    s = subscription_create_from_transport(t, "serviceprobe",
					   serviceprobe_callback, t);
    s->ths_force_demux = 1;

    /* Wait for something to happen */
    while(TAILQ_FIRST(&serviceprobe_queue) == t)
      pthread_cond_wait(&serviceprobe_cond, &global_lock);

    subscription_unsubscribe(s);
    was_doing_work = 1;
  }
  return NULL;
}


/**
 *
 */
void 
serviceprobe_init(void)
{
  pthread_t ptid;
  pthread_cond_init(&serviceprobe_cond, NULL);
  TAILQ_INIT(&serviceprobe_queue);
  pthread_create(&ptid, NULL, serviceprobe_thread, NULL);
}
