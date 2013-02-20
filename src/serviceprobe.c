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

#include "tvheadend.h"
#include "channels.h"
#include "subscriptions.h"
#include "serviceprobe.h"
#include "streaming.h"
#include "service.h"
#include "dvb/dvb.h"

/* List of transports to be probed, protected with global_lock */
static struct service_queue serviceprobe_queue;  
static pthread_cond_t serviceprobe_cond;

/**
 *
 */
void
serviceprobe_enqueue(service_t *t)
{
  if(!service_is_tv(t) && !service_is_radio(t))
    return; /* Don't even consider non-tv/non-radio channels */

  if(t->s_sp_onqueue)
    return;

  if(t->s_ch != NULL)
    return; /* Already mapped */

  t->s_sp_onqueue = 1;
  TAILQ_INSERT_TAIL(&serviceprobe_queue, t, s_sp_link);
  pthread_cond_signal(&serviceprobe_cond);
}


/**
 *
 */
void
serviceprobe_delete(service_t *t)
{
  if(!t->s_sp_onqueue)
    return;
  TAILQ_REMOVE(&serviceprobe_queue, t, s_sp_link);
  t->s_sp_onqueue = 0;
}


/**
 *
 */
static void *
serviceprobe_thread(void *aux)
{
  service_t *t;
  th_subscription_t *s;
  int was_doing_work = 0;
  streaming_queue_t sq;
  streaming_message_t *sm;
  //  transport_feed_status_t status;
  int run;
  const char *err;
  channel_t *ch;
  uint32_t checksubscr;

  pthread_mutex_lock(&global_lock);

  streaming_queue_init(&sq, 0);

  err = NULL;
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

    checksubscr = !t->s_dvb_mux_instance->tdmi_adapter->tda_skip_checksubscr;

    if (checksubscr) {
      tvhlog(LOG_INFO, "serviceprobe", "%20s: checking...",
       t->s_svcname);

      s = subscription_create_from_service(t, "serviceprobe", &sq.sq_st, 0, 
					   NULL, NULL, "serviceprobe");
      if(s == NULL) {
        t->s_sp_onqueue = 0;
        TAILQ_REMOVE(&serviceprobe_queue, t, s_sp_link);
        tvhlog(LOG_INFO, "serviceprobe", "%20s: could not subscribe",
         t->s_svcname);
        continue;
      }
    }

    service_ref(t);
    pthread_mutex_unlock(&global_lock);

    if (checksubscr) {
      run = 1;
      pthread_mutex_lock(&sq.sq_mutex);

      while(run) {

        while((sm = TAILQ_FIRST(&sq.sq_queue)) == NULL)
          pthread_cond_wait(&sq.sq_cond, &sq.sq_mutex);

        TAILQ_REMOVE(&sq.sq_queue, sm, sm_link);

        pthread_mutex_unlock(&sq.sq_mutex);

        if(sm->sm_type == SMT_SERVICE_STATUS) {
          int status = sm->sm_code;

          if(status & TSS_PACKETS) {
            run = 0;
            err = NULL;
          } else if(status & (TSS_GRACEPERIOD | TSS_ERRORS)) {
            run = 0;
            err = service_tss2text(status);
          }
        }

        streaming_msg_free(sm);
        pthread_mutex_lock(&sq.sq_mutex);
      }

      streaming_queue_clear(&sq.sq_queue);
      pthread_mutex_unlock(&sq.sq_mutex);
    } else {
      err = NULL;
    }
 
    pthread_mutex_lock(&global_lock);

    if (checksubscr) {
      subscription_unsubscribe(s);
    }

    if(t->s_status != SERVICE_ZOMBIE) {

      if(err != NULL) {
        tvhlog(LOG_INFO, "serviceprobe", "%20s: skipped: %s",
        t->s_svcname, err);
      } else if(t->s_ch == NULL) {
        int channum = t->s_channel_number;
        const char *str;
        
        if (!channum && t->s_dvb_mux_instance->tdmi_adapter->tda_sidtochan)
          channum = t->s_dvb_service_id;

        ch = channel_find_by_name(t->s_svcname, 1, channum);
        service_map_channel(t, ch, 1);
      
        tvhlog(LOG_INFO, "serviceprobe", "%20s: mapped to channel \"%s\"",
               t->s_svcname, t->s_svcname);

        if(service_is_tv(t)) {
           channel_tag_map(ch, channel_tag_find_by_name("TV channels", 1), 1);
          tvhlog(LOG_INFO, "serviceprobe", "%20s: joined tag \"%s\"",
                 t->s_svcname, "TV channels");
        }

        switch(t->s_servicetype) {
          case ST_SDTV:
          case ST_AC_SDTV:
          case ST_EX_SDTV:
          case ST_DN_SDTV:
          case ST_SK_SDTV:
          case ST_NE_SDTV:
            str = "SDTV";
            break;
          case ST_HDTV:
          case ST_AC_HDTV:
          case ST_EX_HDTV:
          case ST_EP_HDTV:
          case ST_ET_HDTV:
          case ST_DN_HDTV:
            str = "HDTV";
            break;
          case ST_RADIO:
            str = "Radio";
            break;
          default:
            str = NULL;
        }

        if(str != NULL) {
          channel_tag_map(ch, channel_tag_find_by_name(str, 1), 1);
          tvhlog(LOG_INFO, "serviceprobe", "%20s: joined tag \"%s\"",
                 t->s_svcname, str);
        }

        if(t->s_provider != NULL) {
          channel_tag_map(ch, channel_tag_find_by_name(t->s_provider, 1), 1);
          tvhlog(LOG_INFO, "serviceprobe", "%20s: joined tag \"%s\"",
                 t->s_svcname, t->s_provider);
        }
        channel_save(ch);
      }

      t->s_sp_onqueue = 0;
      TAILQ_REMOVE(&serviceprobe_queue, t, s_sp_link);
    }
    service_unref(t);
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
