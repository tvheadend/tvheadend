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
#include "streaming.h"


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
void
serviceprobe_delete(th_transport_t *t)
{
  if(!t->tht_sp_onqueue)
    return;
  TAILQ_REMOVE(&serviceprobe_queue, t, tht_sp_link);
  t->tht_sp_onqueue = 0;
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
  streaming_queue_t sq;
  streaming_message_t *sm;
  transport_feed_status_t status;
  int run;
  const char *err;
  channel_t *ch;

  pthread_mutex_lock(&global_lock);

  streaming_queue_init(&sq);

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
      was_doing_work = 1;
    }

    tvhlog(LOG_INFO, "serviceprobe", "%20s: checking...",
	   t->tht_svcname);

    s = subscription_create_from_transport(t, "serviceprobe", &sq.sq_st, 0);

    transport_ref(t);
    pthread_mutex_unlock(&global_lock);

    run = 1;
    pthread_mutex_lock(&sq.sq_mutex);

    while(run) {

      while((sm = TAILQ_FIRST(&sq.sq_queue)) == NULL)
	pthread_cond_wait(&sq.sq_cond, &sq.sq_mutex);
      TAILQ_REMOVE(&sq.sq_queue, sm, sm_link);

      pthread_mutex_unlock(&sq.sq_mutex);

      if(sm->sm_type == SMT_TRANSPORT_STATUS) {
	status = sm->sm_code;

	run = 0;

	if(status == TRANSPORT_FEED_VALID_PACKETS) {
	  err = NULL;
	} else {
	  err = transport_feed_status_to_text(status);
	}
      }

      streaming_msg_free(sm);
      pthread_mutex_lock(&sq.sq_mutex);
    }

    streaming_queue_clear(&sq.sq_queue);
    pthread_mutex_unlock(&sq.sq_mutex);

    pthread_mutex_lock(&global_lock);
    subscription_unsubscribe(s);

    if(t->tht_status != TRANSPORT_ZOMBIE) {

      if(err != NULL) {
	tvhlog(LOG_INFO, "serviceprobe", "%20s: skipped: %s",
	       t->tht_svcname, err);
      } else if(t->tht_ch == NULL) {
	const char *str;

	ch = channel_find_by_name(t->tht_svcname, 1);
	transport_map_channel(t, ch, 1);
      
	tvhlog(LOG_INFO, "serviceprobe", "%20s: mapped to channel \"%s\"",
	       t->tht_svcname, t->tht_svcname);

	channel_tag_map(ch, channel_tag_find_by_name("TV channels", 1), 1);
	tvhlog(LOG_INFO, "serviceprobe", "%20s: joined tag \"%s\"",
	       t->tht_svcname, "TV channels");

	switch(t->tht_servicetype) {
	case ST_SDTV:
	case ST_AC_SDTV:
	  str = "SDTV";
	  break;
	case ST_HDTV:
	case ST_AC_HDTV:
	  str = "HDTV";
	  break;
	default:
	  str = NULL;
	}

	if(str != NULL) {
	  channel_tag_map(ch, channel_tag_find_by_name(str, 1), 1);
	  tvhlog(LOG_INFO, "serviceprobe", "%20s: joined tag \"%s\"",
		 t->tht_svcname, str);
	}

	if(t->tht_provider != NULL) {
	  channel_tag_map(ch, channel_tag_find_by_name(t->tht_provider, 1), 1);
	  tvhlog(LOG_INFO, "serviceprobe", "%20s: joined tag \"%s\"",
		 t->tht_svcname, t->tht_provider);
	}
	channel_save(ch);
      }

      t->tht_sp_onqueue = 0;
      TAILQ_REMOVE(&serviceprobe_queue, t, tht_sp_link);
    }
    transport_unref(t);
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
