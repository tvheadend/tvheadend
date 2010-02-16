/*
 *  tvheadend, EXTJS based interface
 *  Copyright (C) 2008 Andreas Öman
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
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <arpa/inet.h>
#include <libavutil/avstring.h>

#include "htsmsg.h"
#include "htsmsg_json.h"

#include "tvhead.h"
#include "http.h"
#include "webui.h"
#include "access.h"
#include "channels.h"
#include "psi.h"

#include "v4l.h"
#include "transports.h"
#include "serviceprobe.h"



/**
 *
 */
static htsmsg_t *
json_single_record(htsmsg_t *rec, const char *root)
{
  htsmsg_t *out, *array;
  
  out = htsmsg_create_map();
  array = htsmsg_create_list();

  htsmsg_add_msg(array, NULL, rec);
  htsmsg_add_msg(out, root, array);
  return out;
}



/**
 *
 */
static int
extjs_v4ladapter(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  v4l_adapter_t *va;
  htsmsg_t *out, *array, *r;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  const char *s;

  pthread_mutex_lock(&global_lock);

  if(remain == NULL) {
    /* Just list all adapters */

    array = htsmsg_create_list();

    TAILQ_FOREACH(va, &v4l_adapters, va_global_link) 
      htsmsg_add_msg(array, NULL, v4l_adapter_build_msg(va));

    pthread_mutex_unlock(&global_lock);
    out = htsmsg_create_map();
    htsmsg_add_msg(out, "entries", array);

    htsmsg_json_serialize(out, hq, 0);
    htsmsg_destroy(out);
    http_output_content(hc, "text/x-json; charset=UTF-8");
    return 0;
  }

  if((va = v4l_adapter_find_by_identifier(remain)) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  if(!strcmp(op, "load")) {
    r = htsmsg_create_map();
    htsmsg_add_str(r, "id", va->va_identifier);
    htsmsg_add_str(r, "device", va->va_path ?: "No hardware attached");
    htsmsg_add_str(r, "name", va->va_displayname);
    htsmsg_add_u32(r, "logging", va->va_logging);
 
    out = json_single_record(r, "v4ladapters");
  } else if(!strcmp(op, "save")) {

    if((s = http_arg_get(&hc->hc_req_args, "name")) != NULL)
      v4l_adapter_set_displayname(va, s);

    s = http_arg_get(&hc->hc_req_args, "logging");
    v4l_adapter_set_logging(va, !!s);

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_BAD_REQUEST;
  }
  pthread_mutex_unlock(&global_lock);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);

  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;  
}

/**
 *
 */
static void
transport_update_v4l(htsmsg_t *in)
{
  htsmsg_field_t *f;
  htsmsg_t *c;
  th_transport_t *t;
  uint32_t u32;
  const char *id;
  int save;

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link) {
    if((c = htsmsg_get_map_by_field(f)) == NULL ||
       (id = htsmsg_get_str(c, "id")) == NULL)
      continue;
    
    if((t = transport_find_by_identifier(id)) == NULL)
      continue;

    save = 0;

    if(!htsmsg_get_u32(c, "frequency", &u32)) {
      t->tht_v4l_frequency = u32;
      save = 1;
    }
    if(save)
      t->tht_config_save(t); // Save config
  }
}



/**
 *
 */
static htsmsg_t *
build_record_v4l(th_transport_t *t)
{
  htsmsg_t *r = htsmsg_create_map();

  htsmsg_add_str(r, "id", t->tht_identifier);

  htsmsg_add_str(r, "channelname", t->tht_ch ? t->tht_ch->ch_name : "");
  htsmsg_add_u32(r, "frequency", t->tht_v4l_frequency);
  htsmsg_add_u32(r, "enabled", t->tht_enabled);
  return r;
}

/**
 *
 */
static int
v4l_transportcmp(const void *A, const void *B)
{
  th_transport_t *a = *(th_transport_t **)A;
  th_transport_t *b = *(th_transport_t **)B;

  return (int)a->tht_v4l_frequency - (int)b->tht_v4l_frequency;
}

/**
 *
 */
static int
extjs_v4lservices(http_connection_t *hc, const char *remain, void *opaque)
{
  v4l_adapter_t *va;
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *in, *array;
  const char *op        = http_arg_get(&hc->hc_req_args, "op");
  const char *entries   = http_arg_get(&hc->hc_req_args, "entries");
  th_transport_t *t, **tvec;
  int count = 0, i = 0;

  pthread_mutex_lock(&global_lock);

  if((va = v4l_adapter_find_by_identifier(remain)) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  in = entries != NULL ? htsmsg_json_deserialize(entries) : NULL;

  if(!strcmp(op, "get")) {

    LIST_FOREACH(t, &va->va_transports, tht_group_link)
      count++;
    tvec = alloca(sizeof(th_transport_t *) * count);
    LIST_FOREACH(t, &va->va_transports, tht_group_link)
      tvec[i++] = t;

    out = htsmsg_create_map();
    array = htsmsg_create_list();

    qsort(tvec, count, sizeof(th_transport_t *), v4l_transportcmp);

    for(i = 0; i < count; i++)
      htsmsg_add_msg(array, NULL, build_record_v4l(tvec[i]));

    htsmsg_add_msg(out, "entries", array);

  } else if(!strcmp(op, "update")) {
    if(in != NULL) {
      extjs_transport_update(in);      // Generic transport parameters
      transport_update_v4l(in);  // V4L speicifc
    }

    out = htsmsg_create_map();

  } else if(!strcmp(op, "create")) {

    out = build_record_v4l(v4l_transport_find(va, NULL, 1));

  } else if(!strcmp(op, "delete")) {
    if(in != NULL)
      extjs_transport_delete(in);

    out = htsmsg_create_map();

  } else {
    pthread_mutex_unlock(&global_lock);
    htsmsg_destroy(in);
    return HTTP_STATUS_BAD_REQUEST;
  }

  htsmsg_destroy(in);

  pthread_mutex_unlock(&global_lock);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}


/**
 *
 */
void
extjs_list_v4l_adapters(htsmsg_t *array)
{
  v4l_adapter_t *va;

  TAILQ_FOREACH(va, &v4l_adapters, va_global_link) 
    htsmsg_add_msg(array, NULL, v4l_adapter_build_msg(va));
}


/**
 * WEB user interface
 */
void
extjs_start_v4l(void)
{
  http_path_add("/v4l/adapter", 
		NULL, extjs_v4ladapter, ACCESS_ADMIN);

  http_path_add("/v4l/services", 
		NULL, extjs_v4lservices, ACCESS_ADMIN);
}
