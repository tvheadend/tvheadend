/*
 *  Service Mapper functions
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
#include "profile.h"
#include "bouquet.h"
#include "api.h"

static service_mapper_status_t service_mapper_stat; 
static pthread_cond_t          service_mapper_cond;
static struct service_queue    service_mapper_queue;
static service_mapper_conf_t   service_mapper_conf;

static void *service_mapper_thread ( void *p );

/**
 * Initialise
 */
pthread_t service_mapper_tid;

void
service_mapper_init ( void )
{
  TAILQ_INIT(&service_mapper_queue);
  pthread_cond_init(&service_mapper_cond, NULL);
  tvhthread_create(&service_mapper_tid, NULL, service_mapper_thread, NULL);
}

void
service_mapper_done ( void )
{
  pthread_cond_signal(&service_mapper_cond);
  pthread_join(service_mapper_tid, NULL);
}

/*
 * Get status
 */
service_mapper_status_t
service_mapper_status ( void )
{
  return service_mapper_stat;
}

/*
 * Start a new mapping
 */
void
service_mapper_start ( const service_mapper_conf_t *conf, htsmsg_t *uuids )
{
  int e, tr, qd = 0;
  service_t *s;

  /* Reset stat counters */
  if (TAILQ_EMPTY(&service_mapper_queue))
    service_mapper_reset_stats();

  /* Store config */
  service_mapper_conf = *conf;

  /* Check each service */
  TAILQ_FOREACH(s, &service_all, s_all_link) {
    if (uuids) {
      htsmsg_field_t *f;
      const char *str;
      const char *uuid = idnode_uuid_as_str(&s->s_id);
      HTSMSG_FOREACH(f, uuids) {
        if (!(str = htsmsg_field_get_str(f))) continue;
        if (!strcmp(str, uuid)) break;
      }
      if (!f) continue;
    }
    tvhtrace("service_mapper", "check service %s (%s)",
             s->s_nicename, idnode_uuid_as_str(&s->s_id));

    /* Already mapped (or in progress) */
    if (s->s_sm_onqueue) continue;
    if (LIST_FIRST(&s->s_channels)) continue;
    tvhtrace("service_mapper", "  not mapped");
    service_mapper_stat.total++;
    service_mapper_stat.ignore++;

    /* Disabled */
    if (!s->s_is_enabled(s, 0)) continue;
    tvhtrace("service_mapper", "  enabled");

    /* Get service info */
    pthread_mutex_lock(&s->s_stream_mutex);
    e  = service_is_encrypted(s);
    tr = service_is_tv(s) || service_is_radio(s);
    pthread_mutex_unlock(&s->s_stream_mutex);

    /* Skip non-TV / Radio */
    if (!tr) continue;
    tvhtrace("service_mapper", "  radio or tv");

    /* Skip encrypted */
    if (!conf->encrypted && e) continue;
    service_mapper_stat.ignore--;
    
    /* Queue */
    if (conf->check_availability) {
      tvhtrace("service_mapper", "  queue for checking");
      qd = 1;
      TAILQ_INSERT_TAIL(&service_mapper_queue, s, s_sm_link);
      s->s_sm_onqueue = 1;
    
    /* Process */
    } else {
      tvhtrace("service_mapper", "  process");
      service_mapper_process(s, NULL);
    }
  }
  
  /* Notify */
  api_service_mapper_notify();

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
  while ((s = TAILQ_FIRST(&service_mapper_queue))) {
    service_mapper_stat.total--;
    service_mapper_remove(s);
  }

  /* Notify */
  api_service_mapper_notify();
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

  /* Notify */
  api_service_mapper_notify();
}

/*
 * Link service and channel
 */
int
service_mapper_link ( service_t *s, channel_t *c, void *origin )
{
  idnode_list_mapping_t *ilm;

  ilm = idnode_list_link(&s->s_id, &s->s_channels,
                         &c->ch_id, &c->ch_services,
                         origin);
  if (ilm) {
    service_mapped(s);
    ilm->ilm_in2_save = 1; /* channel */
    return 1;
  }
  return 0;
}

int
service_mapper_create ( idnode_t *s, idnode_t *c, void *origin )
{
  return service_mapper_link((service_t *)s, (channel_t *)c, origin);
}

/*
 * Process a service 
 */
channel_t *
service_mapper_process ( service_t *s, bouquet_t *bq )
{
  channel_t *chn = NULL;
  const char *name;

  /* Ignore */
  if (s->s_status == SERVICE_ZOMBIE) {
    if (!bq)
      service_mapper_stat.ignore++;
    goto exit;
  }

  /* Safety check (in-case something has been mapped manually in the interim) */
  if (!bq && LIST_FIRST(&s->s_channels)) {
    service_mapper_stat.ignore++;
    goto exit;
  }

  /* Find existing channel */
  name = service_get_channel_name(s);
  if (!bq && service_mapper_conf.merge_same_name && name && *name)
    chn = channel_find_by_name(name);
  if (!chn) {
    chn = channel_create(NULL, NULL, NULL);
    chn->ch_bouquet = bq;
  }
    
  /* Map */
  if (chn) {
    const char *prov;
    service_mapper_link(s, chn, chn);

    /* Type tags */
    if (service_is_hdtv(s)) {
      channel_tag_map(channel_tag_find_by_name("TV channels", 1), chn, chn);
      channel_tag_map(channel_tag_find_by_name("HDTV", 1), chn, chn);
    } else if (service_is_sdtv(s)) {
      channel_tag_map(channel_tag_find_by_name("TV channels", 1), chn, chn);
      channel_tag_map(channel_tag_find_by_name("SDTV", 1), chn, chn);
    } else if (service_is_radio(s)) {
      channel_tag_map(channel_tag_find_by_name("Radio", 1), chn, chn);
    }

    /* Provider */
    if (service_mapper_conf.provider_tags)
      if ((prov = s->s_provider_name(s)))
        channel_tag_map(channel_tag_find_by_name(prov, 1), chn, chn);

    /* save */
    idnode_notify_changed(&chn->ch_id);
    channel_save(chn);
  }
  if (!bq) {
    service_mapper_stat.ok++;
    tvhinfo("service_mapper", "%s: success", s->s_nicename);
  } else {
    tvhinfo("bouquet", "%s: mapped service from %s", s->s_nicename, bq->bq_name ?: "<unknown>");
  }

  /* Remove */
exit:
  service_mapper_remove(s);
  return chn;
}

/**
 *
 */
static void *
service_mapper_thread ( void *aux )
{
  service_t *s;
  profile_chain_t prch;
  th_subscription_t *sub;
  int run, working = 0;
  streaming_queue_t *sq;
  streaming_message_t *sm;
  const char *err = NULL;

  profile_chain_init(&prch, NULL, NULL);
  prch.prch_st = &prch.prch_sq.sq_st;
  sq = &prch.prch_sq;

  pthread_mutex_lock(&global_lock);

  while (tvheadend_running) {
    
    /* Wait for work */
    while (!(s = TAILQ_FIRST(&service_mapper_queue))) {
      if (working) {
        working = 0;
        tvhinfo("service_mapper", "idle");
      }
      pthread_cond_wait(&service_mapper_cond, &global_lock);
      if (!tvheadend_running)
        break;
    }
    if (!tvheadend_running)
      break;
    service_mapper_remove(s);

    if (!working) {
      working = 1;
      tvhinfo("service_mapper", "starting");
    }

    /* Subscribe */
    tvhinfo("service_mapper", "checking %s", s->s_nicename);
    prch.prch_id = s;
    sub = subscription_create_from_service(&prch, NULL,
                                           SUBSCRIPTION_PRIO_MAPPER,
                                           "service_mapper",
                                           SUBSCRIPTION_PACKET,
                                           NULL, NULL, "service_mapper", NULL);

    /* Failed */
    if (!sub) {
      tvhinfo("service_mapper", "%s: could not subscribe", s->s_nicename);
      continue;
    }

    tvhinfo("service_mapper", "waiting for input");
    service_ref(s);
    service_mapper_stat.active = s;
    api_service_mapper_notify();
    pthread_mutex_unlock(&global_lock);

    /* Wait */
    run = 1;
    pthread_mutex_lock(&sq->sq_mutex);
    while(tvheadend_running && run) {

      /* Wait for message */
      while((sm = TAILQ_FIRST(&sq->sq_queue)) == NULL) {
        pthread_cond_wait(&sq->sq_cond, &sq->sq_mutex);
        if (!tvheadend_running)
          break;
      }
      if (!tvheadend_running)
        break;

      TAILQ_REMOVE(&sq->sq_queue, sm, sm_link);
      pthread_mutex_unlock(&sq->sq_mutex);

      if(sm->sm_type == SMT_PACKET) {
        run = 0;
        err = NULL;
      } else if (sm->sm_type == SMT_SERVICE_STATUS) {
        int status = sm->sm_code;

        if(status & TSS_ERRORS) {
          run = 0;
          err = service_tss2text(status);
        }
      } else if (sm->sm_type == SMT_NOSTART) {
        run = 0;
        err = streaming_code2txt(sm->sm_code);
      }

      streaming_msg_free(sm);
      pthread_mutex_lock(&sq->sq_mutex);
    }
    if (!tvheadend_running)
      break;

    streaming_queue_clear(&sq->sq_queue);
    pthread_mutex_unlock(&sq->sq_mutex);
 
    pthread_mutex_lock(&global_lock);
    subscription_unsubscribe(sub, 0);

    if(err) {
      tvhinfo("service_mapper", "%s: failed [err %s]", s->s_nicename, err);
      service_mapper_stat.fail++;
    } else
      service_mapper_process(s, NULL);

    service_unref(s);
    service_mapper_stat.active = NULL;
    api_service_mapper_notify();
  }

  pthread_mutex_unlock(&global_lock);
  profile_chain_close(&prch);
  return NULL;
}

void
service_mapper_reset_stats (void)
{
  service_mapper_stat.total  = 0;
  service_mapper_stat.ok     = 0;
  service_mapper_stat.ignore = 0;
  service_mapper_stat.fail   = 0;
  service_mapper_stat.active = NULL;
}
