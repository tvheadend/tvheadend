/*
 *  API - service related calls
 *
 *  Copyright (C) 2013 Adam Sutton
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

#ifndef __TVH_API_SERVICE_H__
#define __TVH_API_SERVICE_H__

#include "tvheadend.h"
#include "service.h"
#include "service_mapper.h"
#include "access.h"
#include "api.h"
#include "notify.h"

static int
api_mapper_start
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  service_mapper_conf_t conf = { 0 };
  htsmsg_t *uuids;
#define get_u32(x)\
  conf.x = htsmsg_get_bool_or_default(args, #x, 0)
  
  /* Get config */
  uuids = htsmsg_get_list(args, "uuids");
  get_u32(check_availability);
  get_u32(encrypted);
  get_u32(merge_same_name);
  get_u32(provider_tags);
  get_u32(network_tags);
  
  pthread_mutex_lock(&global_lock);
  service_mapper_start(&conf, uuids);
  pthread_mutex_unlock(&global_lock);

  return 0;
}

static int
api_mapper_stop
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  pthread_mutex_lock(&global_lock);
  service_mapper_stop();
  pthread_mutex_unlock(&global_lock);

  return 0;
}

static htsmsg_t *
api_mapper_status_msg ( void )
{
  htsmsg_t *m;
  service_mapper_status_t stat = service_mapper_status();
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "total",  stat.total);
  htsmsg_add_u32(m, "ok",     stat.ok);
  htsmsg_add_u32(m, "fail",   stat.fail);
  htsmsg_add_u32(m, "ignore", stat.ignore);
  if (stat.active)
    htsmsg_add_str(m, "active", idnode_uuid_as_sstr(&stat.active->s_id));
  return m;
}

static int
api_mapper_status
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  pthread_mutex_lock(&global_lock);
  *resp = api_mapper_status_msg();
  pthread_mutex_unlock(&global_lock);
  return 0;
}

void
api_service_mapper_notify ( void )
{
  notify_by_msg("servicemapper", api_mapper_status_msg(), 0);
}

static htsmsg_t *
api_service_streams_get_one ( elementary_stream_t *es, int use_filter )
{
  htsmsg_t *e = htsmsg_create_map();
  htsmsg_add_u32(e, "index",    es->es_index);
  htsmsg_add_u32(e, "pid",      es->es_pid);
  htsmsg_add_str(e, "type",     streaming_component_type2txt(es->es_type));
  htsmsg_add_str(e, "language", es->es_lang);
  if (SCT_ISSUBTITLE(es->es_type)) {
    htsmsg_add_u32(e, "composition_id", es->es_composition_id);
    htsmsg_add_u32(e, "ancillary_id",   es->es_ancillary_id);
  } else if (SCT_ISAUDIO(es->es_type)) {
    htsmsg_add_u32(e, "audio_type",     es->es_audio_type);
  } else if (SCT_ISVIDEO(es->es_type)) {
    htsmsg_add_u32(e, "width",          es->es_width);
    htsmsg_add_u32(e, "height",         es->es_height);
    htsmsg_add_u32(e, "duration",       es->es_frame_duration);
    htsmsg_add_u32(e, "aspect_num",     es->es_aspect_num);
    htsmsg_add_u32(e, "aspect_den",     es->es_aspect_den);
  } else if (es->es_type == SCT_CA) {
    caid_t *ca;
    htsmsg_t *e2, *l2 = htsmsg_create_list();
    LIST_FOREACH(ca, &es->es_caids, link) {
      if (use_filter && !ca->use)
        continue;
      e2 = htsmsg_create_map();
      htsmsg_add_u32(e2, "caid",     ca->caid);
      htsmsg_add_u32(e2, "provider", ca->providerid);
      htsmsg_add_msg(l2, NULL, e2);
    }
    htsmsg_add_msg(e, "caids", l2);
  }
  return e;
}

static int
api_service_streams
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  const char *uuid;
  htsmsg_t *e, *st, *stf;
  service_t *s;
  elementary_stream_t *es;

  /* No UUID */
  if (!(uuid = htsmsg_get_str(args, "uuid")))
    return EINVAL;

  pthread_mutex_lock(&global_lock);

  /* Couldn't find */
  if (!(s = service_find(uuid))) {
    pthread_mutex_unlock(&global_lock);
    return EINVAL;
  }

  /* Build response */
  pthread_mutex_lock(&s->s_stream_mutex);
  st = htsmsg_create_list();
  stf = htsmsg_create_list();
  if (s->s_pcr_pid) {
    e = htsmsg_create_map();
    htsmsg_add_u32(e, "pid", s->s_pcr_pid);
    htsmsg_add_str(e, "type", "PCR");
    htsmsg_add_msg(st, NULL, e);
  }
  if (s->s_pmt_pid) {
    e = htsmsg_create_map();
    htsmsg_add_u32(e, "pid", s->s_pmt_pid);
    htsmsg_add_str(e, "type", "PMT");
    htsmsg_add_msg(st, NULL, e);
  }
  TAILQ_FOREACH(es, &s->s_components, es_link)
    htsmsg_add_msg(st, NULL, api_service_streams_get_one(es, 0));
  if (TAILQ_FIRST(&s->s_filt_components) == NULL ||
      s->s_status == SERVICE_IDLE)
    service_build_filter(s);
  TAILQ_FOREACH(es, &s->s_filt_components, es_filt_link)
    htsmsg_add_msg(stf, NULL, api_service_streams_get_one(es, 1));
  *resp = htsmsg_create_map();
  htsmsg_add_str(*resp, "name", s->s_nicename);
  htsmsg_add_msg(*resp, "streams", st);
  htsmsg_add_msg(*resp, "fstreams", stf);
  pthread_mutex_unlock(&s->s_stream_mutex);

  /* Done */
  pthread_mutex_unlock(&global_lock);
  return 0;
}

void api_service_init ( void )
{
  extern const idclass_t service_class;
  static api_hook_t ah[] = {
    { "service/mapper/start",   ACCESS_ADMIN, api_mapper_start,  NULL },
    { "service/mapper/stop",    ACCESS_ADMIN, api_mapper_stop,   NULL },
    { "service/mapper/status",  ACCESS_ADMIN, api_mapper_status, NULL },
    { "service/list",           ACCESS_ADMIN, api_idnode_load_by_class,
      (void*)&service_class },
    { "service/streams",        ACCESS_ADMIN, api_service_streams, NULL },
    { NULL },
  };

  api_register_all(ah);
}


#endif /* __TVH_API_SERVICE_H__ */
