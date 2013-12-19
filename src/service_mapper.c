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
#include "service_mapper.h"
#include "streaming.h"
#include "service.h"
#include "plumbing/tsfix.h"

static pthread_cond_t        service_mapper_cond;
static struct service_queue  service_mapper_queue;
static service_mapper_conf_t service_mapper_conf;

static void service_mapper_process ( service_t *s );
static void *service_mapper_thread ( void *p );

/**
 * Initialise
 */
void
service_mapper_init ( void )
{
  pthread_t tid;
  TAILQ_INIT(&service_mapper_queue);
  pthread_cond_init(&service_mapper_cond, NULL);
  pthread_create(&tid, NULL, service_mapper_thread, NULL);
}

/*
 * Get Q length
 */
int
service_mapper_qlen ( void )
{
  service_t *s;
  int c = 0;
  TAILQ_FOREACH(s, &service_mapper_queue, s_sm_link)
    c++;
  return c;
}

/*
 * Start a new mapping
 */
void
service_mapper_start ( const service_mapper_conf_t *conf )
{
  int e, tr, qd = 0;
  service_t *s;

  /* Copy config */
  service_mapper_conf = *conf;

  /* Check each service */
  TAILQ_FOREACH(s, &service_all, s_all_link) {

    /* Disabled */
    if (!s->s_is_enabled(s)) continue;

    /* Already mapped */
    if (LIST_FIRST(&s->s_channels)) continue;

    /* Get service info */
    pthread_mutex_lock(&s->s_stream_mutex);
    e  = service_is_encrypted(s);
    tr = service_is_tv(s) || service_is_radio(s);
    pthread_mutex_unlock(&s->s_stream_mutex);

    /* Skip non-TV / Radio */
    if (!tr) continue;

    /* Skip encrypted */
    if (!conf->encrypted && e) continue;
    
    /* Queue */
    if (conf->check_availability) {
      if (!s->s_sm_onqueue) {
        qd = 1;
        TAILQ_INSERT_TAIL(&service_mapper_queue, s, s_sm_link);
        s->s_sm_onqueue = 1;
      }
    
    /* Process */
    } else {
      service_mapper_process(s);
    }
  }

  /* Signal */
  if (qd) pthread_cond_signal(&service_mapper_cond);
}

/*
 * Stop everything
 */
void
service_mapper_stop ( void )
{
  service_t *s;
  while ((s = TAILQ_FIRST(&service_mapper_queue)))
    service_mapper_remove(s);
}

/*
 * Remove service
 */
void
service_mapper_remove ( service_t *s )
{
  if (s->s_sm_onqueue) {
    TAILQ_REMOVE(&service_mapper_queue, s, s_sm_link);
    s->s_sm_onqueue = 0;
  }
}

/*
 * Link service and channel
 */
int
service_mapper_link ( service_t *s, channel_t *c )
{
  channel_service_mapping_t *csm;

  /* Already linked */
  LIST_FOREACH(csm, &s->s_channels, csm_chn_link)
    if (csm->csm_chn == c) {
      csm->csm_mark = 0;
      return 0;
    }

  /* Link */
  csm = calloc(1, sizeof(channel_service_mapping_t));
  csm->csm_chn = c;
  csm->csm_svc = s;
  LIST_INSERT_HEAD(&s->s_channels, csm, csm_svc_link);
  LIST_INSERT_HEAD(&c->ch_services, csm, csm_chn_link);
  return 1;
}

void
service_mapper_unlink ( service_t *s, channel_t *c )
{
  channel_service_mapping_t *csm;

  /* Unlink */
  LIST_FOREACH(csm, &s->s_channels, csm_chn_link) {
    if (csm->csm_chn == c) {
      LIST_REMOVE(csm, csm_chn_link);
      LIST_REMOVE(csm, csm_svc_link);
      free(csm);
      break;
    }
  }
}

/*
 * Process a service 
 */
void
service_mapper_process ( service_t *s )
{
  int num;
  channel_t *chn = NULL;

  /* Ignore */
  if (s->s_status == SERVICE_ZOMBIE)
    goto exit;

  /* Safety check (in-case something has been mapped manually in the interim) */
  if (LIST_FIRST(&s->s_channels))
    goto exit;

  /* Find existing channel */
  num = s->s_channel_number(s);
#if 0
  if (service_mapper_conf.merge_same_name)
    chn = channel_find_by_name(s->s_channel_name(s));
#endif
  if (!chn)
    chn = channel_create(NULL, NULL, s->s_channel_name(s));
    
  /* Map */
  if (chn) {
    const char *prov;
    service_mapper_link(s, chn);

    /* Type tags */
    if (service_is_hdtv(s)) {
      channel_tag_map(chn, channel_tag_find_by_name("TV", 1), 1);
      channel_tag_map(chn, channel_tag_find_by_name("HDTV", 1), 1);
    } else if (service_is_sdtv(s)) {
      channel_tag_map(chn, channel_tag_find_by_name("TV channels", 1), 1);
      channel_tag_map(chn, channel_tag_find_by_name("SDTV", 1), 1);
    } else {
      channel_tag_map(chn, channel_tag_find_by_name("Radio", 1), 1);
    }

    /* Set number */
    if (!chn->ch_number && num)
      chn->ch_number = num;

    /* Provider */
    if (service_mapper_conf.provider_tags)
      if ((prov = s->s_provider_name(s)))
        channel_tag_map(chn, channel_tag_find_by_name(prov, 1), 1);

    /* save */
    channel_save(chn);
  }

  /* Remove */
exit:
  service_mapper_remove(s);
}

/**
 *
 */
static void *
service_mapper_thread ( void *aux )
{
  service_t *s;
  th_subscription_t *sub;
  int run, working = 0;
  streaming_queue_t sq;
  streaming_message_t *sm;
  const char *err;
  streaming_target_t *st;

  streaming_queue_init(&sq, 0);

  pthread_mutex_lock(&global_lock);

  while (1) {
    
    /* Wait for work */
    while (!(s = TAILQ_FIRST(&service_mapper_queue))) {
      if (working) {
        working = 0;
        tvhinfo("service_mapper", "idle");
      }
      pthread_cond_wait(&service_mapper_cond, &global_lock);
    }
    service_mapper_remove(s);

    if (!working) {
      working = 1;
      tvhinfo("service_mapper", "starting");
    }

    /* Subscribe */
    tvhinfo("service_mapper", "%s: checking availability", s->s_nicename);
    st  = tsfix_create(&sq.sq_st);
    sub = subscription_create_from_service(s, 2, "service_mapper", st,
                                           0, NULL, NULL, "service_mapper");

    /* Failed */
    if (!sub) {
      tvhinfo("service_mapper", "%s: could not subscribe", s->s_nicename);
      continue;
    }

    service_ref(s);
    pthread_mutex_unlock(&global_lock);

    /* Wait */
    run = 1;
    pthread_mutex_lock(&sq.sq_mutex);
    while(run) {

      /* Wait for message */
      while((sm = TAILQ_FIRST(&sq.sq_queue)) == NULL)
        pthread_cond_wait(&sq.sq_cond, &sq.sq_mutex);
      TAILQ_REMOVE(&sq.sq_queue, sm, sm_link);
      pthread_mutex_unlock(&sq.sq_mutex);

      if(sm->sm_type == SMT_PACKET) {
        run = 0;
        err = NULL;
      } else if (sm->sm_type == SMT_SERVICE_STATUS) {
        int status = sm->sm_code;

        if(status & (TSS_GRACEPERIOD | TSS_ERRORS)) {
          run = 0;
          err = service_tss2text(status);
        }
      }

      streaming_msg_free(sm);
      pthread_mutex_lock(&sq.sq_mutex);
    }

    streaming_queue_clear(&sq.sq_queue);
    pthread_mutex_unlock(&sq.sq_mutex);
 
    pthread_mutex_lock(&global_lock);
    subscription_unsubscribe(sub);
    tsfix_destroy(st);

    if(err)
      tvhinfo("service_mapper", "%s: failed %s", s->s_nicename, err);
    else
      service_mapper_process(s);

    service_unref(s);
  }
  return NULL;
}
