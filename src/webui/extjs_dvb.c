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

#include "htsmsg.h"
#include "htsmsg_json.h"

#include "tvheadend.h"
#include "http.h"
#include "webui.h"
#include "access.h"
#include "dtable.h"
#include "channels.h"
#include "serviceprobe.h"

#include "input.h"

typedef struct extjs_grid_conf
{
  int       start;
  int       limit;
  htsmsg_t *filter;
} extjs_grid_conf_t;

static void
extjs_grid_conf
  ( struct http_arg_list *args, extjs_grid_conf_t *conf )
{
  const char *str;
  if ((str = http_arg_get(args, "start")))
    conf->start = atoi(str);
  else
    conf->start = 0;
  if ((str = http_arg_get(args, "limit")))
    conf->limit = atoi(str);
  else
    conf->limit = 50;
  if ((str = http_arg_get(args, "filter")))
    conf->filter = htsmsg_json_deserialize(str);
  else
    conf->filter = NULL;
}

static int
extjs_idnode_filter
  ( idnode_t *in, htsmsg_t *filter )
{
  htsmsg_t *e;
  htsmsg_field_t *f;
  const char *k, *t;
  
  HTSMSG_FOREACH(f, filter) {
    if (!(e = htsmsg_get_map_by_field(f)))      continue;
    if (!(k = htsmsg_get_str(e, "field")))      continue;
    if (!(t = htsmsg_get_str(e, "type"))) continue;
    if (!strcmp(t, "string")) {
      const char *str = idnode_get_str(in, k);
      if (!strstr(str ?: "", htsmsg_get_str(e, "value") ?: ""))
        return 0;
    } else if (!strcmp(t, "numeric")) {
      uint32_t u32, val;
      if (!idnode_get_u32(in, k, &u32)) {
        const char *op = htsmsg_get_str(e, "comparison");
        if (!op) continue;
        if (htsmsg_get_u32(e, "value", &val)) continue;
        if (!strcmp(op, "lt")) {
          if (u32 > val) return 0;
        } else if (!strcmp(op, "gt")) {
          if (u32 < val) return 0;
        } else {
          if (u32 != val) return 0;
        }
      }
    }
  }
  return 1;
}

// TODO: move this
static htsmsg_t *
extjs_idnode_class ( const idclass_t *idc )
{
  htsmsg_t *ret = htsmsg_create_list();
  while (idc) {
    prop_add_params_to_msg(NULL, idc->ic_properties, ret);
    idc = idc->ic_super;
  }
  return ret;
}

static int
extjs_mpegts_service
  (http_connection_t *hc, const char *remain, void *opaque)
{
  //char buf[256];
  mpegts_network_t *mn;
  mpegts_mux_t     *mm;
  mpegts_service_t *ms;
  htsbuf_queue_t   *hq = &hc->hc_reply;
  const char       *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t         *out = htsmsg_create_map();
  extjs_grid_conf_t conf;
  int total = 0;

  if (!strcmp(op, "list")) {
    htsmsg_t *list = htsmsg_create_list();
    extjs_grid_conf(&hc->hc_req_args, &conf);
    pthread_mutex_lock(&global_lock);
    LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
      LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link) {
        LIST_FOREACH(ms, &mm->mm_services, s_dvb_mux_link) {
          if (conf.filter && !extjs_idnode_filter(&ms->s_id, conf.filter))
            continue;
          total++;
          if (conf.start-- > 0)
            continue;
          if (conf.limit) {
            conf.limit--;
            htsmsg_t *e = htsmsg_create_map();
            htsmsg_add_str(e, "uuid", idnode_uuid_as_str(&ms->s_id));
            idnode_save(&ms->s_id, e);
            htsmsg_add_msg(list, NULL, e);
            conf.limit--;
          }
        }
      }
    }
    pthread_mutex_unlock(&global_lock);
    htsmsg_add_msg(out, "entries", list);
    htsmsg_add_u32(out, "total",   total);
  } else if (!strcmp(op, "class")) {
    extern const idclass_t mpegts_service_class;
    htsmsg_t *list= extjs_idnode_class(&mpegts_service_class);
    htsmsg_add_msg(out, "entries", list);
  }

  htsmsg_json_serialize(out, hq, 0);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  htsmsg_destroy(out);

  return 0;
}


static int
extjs_mpegts_mux
  (http_connection_t *hc, const char *remain, void *opaque)
{
  char buf[256];
  mpegts_network_t *mn;
  mpegts_mux_t     *mm;
  mpegts_service_t *ms;
  htsbuf_queue_t   *hq = &hc->hc_reply;
  const char       *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t         *out = htsmsg_create_map();
  extjs_grid_conf_t conf;
  int s, total = 0;

  if (!strcmp(op, "list")) {
    htsmsg_t *list = htsmsg_create_list();
    extjs_grid_conf(&hc->hc_req_args, &conf);

    pthread_mutex_lock(&global_lock);
    LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
      LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link) {
        if (conf.filter && !extjs_idnode_filter(&mm->mm_id, conf.filter))
          continue;
        total++;
        if (conf.start-- > 0)
          continue;
        if (conf.limit) {
          s = 0;
          conf.limit--;
          htsmsg_t *e = htsmsg_create_map();
          htsmsg_add_str(e, "uuid", idnode_uuid_as_str(&mm->mm_id));
          idnode_save(&mm->mm_id, e);
          mn->mn_display_name(mn, buf, sizeof(buf));
          htsmsg_add_str(e, "network", buf);
          LIST_FOREACH(ms, &mm->mm_services, s_dvb_mux_link)
            s++;
          htsmsg_add_u32(e, "num_svc", s);
          htsmsg_add_msg(list, NULL, e);
        }
      }
    }
    pthread_mutex_unlock(&global_lock);
    htsmsg_add_msg(out, "entries", list);
    htsmsg_add_u32(out, "total",   total);
  } else if (!strcmp(op, "class")) {
    extern const idclass_t mpegts_mux_class;
    htsmsg_t *list= extjs_idnode_class(&mpegts_mux_class);
    htsmsg_add_msg(out, "entries", list);
  }

  htsmsg_json_serialize(out, hq, 0);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  htsmsg_destroy(out);

  return 0;
}

static int
extjs_mpegts_network
  (http_connection_t *hc, const char *remain, void *opaque)
{
  mpegts_network_t *mn;
  mpegts_mux_t     *mm;
  mpegts_service_t *ms;
  htsbuf_queue_t   *hq  = &hc->hc_reply;
  const char       *op  = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t         *out = htsmsg_create_map();
  extjs_grid_conf_t conf;
  int s, m, total = 0;

  if (!strcmp(op, "list")) {
    htsmsg_t *list = htsmsg_create_list();
    extjs_grid_conf(&hc->hc_req_args, &conf);

    pthread_mutex_lock(&global_lock);
    LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
      if (conf.filter && !extjs_idnode_filter(&mn->mn_id, conf.filter))
        continue;
      total++;
      if (conf.start-- > 0)
        continue;
      if (conf.limit) {
        s = m = 0;
        conf.limit--;
        htsmsg_t *e = htsmsg_create_map();
        htsmsg_add_str(e, "uuid", idnode_uuid_as_str(&mn->mn_id));
        idnode_save(&mn->mn_id, e);
        LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link) {
          m++;
          LIST_FOREACH(ms, &mm->mm_services, s_dvb_mux_link)
            s++;
        }
        htsmsg_add_u32(e, "num_mux", m);
        htsmsg_add_u32(e, "num_svc", s);
        htsmsg_add_msg(list, NULL, e);
      }
    }
    pthread_mutex_unlock(&global_lock);
    htsmsg_add_msg(out, "entries", list);
    htsmsg_add_u32(out, "total",   total);
  } else if (!strcmp(op, "class")) {
    extern const idclass_t mpegts_network_class;
    htsmsg_t *list= extjs_idnode_class(&mpegts_network_class);
    htsmsg_add_msg(out, "entries", list);
  }

  htsmsg_json_serialize(out, hq, 0);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  htsmsg_destroy(out);

  return 0;
}

static int
extjs_mpegts_input
  (http_connection_t *hc, const char *remain, void *opaque)
{
  extern const idclass_t mpegts_input_class;
  mpegts_input_t   *mi;
  mpegts_network_t *mn;
  htsbuf_queue_t   *hq  = &hc->hc_reply;
  const char       *op  = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t         *out = htsmsg_create_map();
  extjs_grid_conf_t conf;
  int total = 0;

  if (!op) return 404;

  if (!strcmp(op, "list")) {
    htsmsg_t *list = htsmsg_create_list();
    extjs_grid_conf(&hc->hc_req_args, &conf);

    pthread_mutex_lock(&global_lock);
    LIST_FOREACH(mi, &mpegts_input_all, mi_global_link) {
      if (conf.filter && !extjs_idnode_filter(&mi->mi_id, conf.filter))
        continue;
      total++;
      if (conf.start-- > 0)
        continue;
      if (conf.limit != 0) {
        if (conf.limit > 0) conf.limit--;
        htsmsg_t *e = htsmsg_create_map();
        htsmsg_add_str(e, "uuid", idnode_uuid_as_str(&mi->mi_id));
        idnode_save(&mi->mi_id, e);
        htsmsg_add_msg(list, NULL, e);
      }
    }
    pthread_mutex_unlock(&global_lock);
    htsmsg_add_msg(out, "entries", list);
    htsmsg_add_u32(out, "total",   total);
  } else if (!strcmp(op, "class")) {
    htsmsg_t *list= extjs_idnode_class(&mpegts_input_class);
    htsmsg_add_msg(out, "entries", list);
  } else if (!strcmp(op, "network_class")) {
    const char *uuid = http_arg_get(&hc->hc_req_args, "uuid");
    if (!uuid) return 404;
    mpegts_input_t *mi = idnode_find(uuid, &mpegts_input_class);
    if (!mi) return 404;
    htsmsg_t *list= extjs_idnode_class(mi->mi_network_class(mi));
    htsmsg_add_msg(out, "entries", list);
  } else if (!strcmp(op, "network_create")) {
    const char *uuid = http_arg_get(&hc->hc_req_args, "uuid");
    const char *conf = http_arg_get(&hc->hc_req_args, "conf");
    if (!uuid || !conf) return 404;
    mi = idnode_find(uuid, &mpegts_input_class);
    if (!mi) return 404;
    mn = mi->mi_network_create(mi, htsmsg_json_deserialize(conf));
    if (mn)
      mn->mn_config_save(mn);
    else {
      // TODO: Check for error
    }
  }

  htsmsg_json_serialize(out, hq, 0);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  htsmsg_destroy(out);

  return 0;
}

/**
 * DVB WEB user interface
 */
void
extjs_start_dvb(void)
{
  http_path_add("/api/mpegts/network", 
		NULL, extjs_mpegts_network, ACCESS_WEB_INTERFACE);
  http_path_add("/api/mpegts/mux", 
		NULL, extjs_mpegts_mux, ACCESS_WEB_INTERFACE);
  http_path_add("/api/mpegts/service", 
		NULL, extjs_mpegts_service, ACCESS_WEB_INTERFACE);
  http_path_add("/api/mpegts/input", 
		NULL, extjs_mpegts_input, ACCESS_WEB_INTERFACE);
#if 0
  http_path_add("/dvb/locations", 
		NULL, extjs_dvblocations, ACCESS_WEB_INTERFACE);

  http_path_add("/dvb/adapter", 
		NULL, extjs_dvbadapter, ACCESS_ADMIN);


  http_path_add("/dvb/muxes", 
		NULL, extjs_dvbmuxes, ACCESS_ADMIN);

  http_path_add("/dvb/services", 
		NULL, extjs_dvbservices, ACCESS_ADMIN);

  http_path_add("/dvb/lnbtypes", 
		NULL, extjs_lnbtypes, ACCESS_ADMIN);

  http_path_add("/dvb/satconf", 
		NULL, extjs_dvbsatconf, ACCESS_ADMIN);

  http_path_add("/dvb/feopts", 
		NULL, extjs_dvb_feopts, ACCESS_ADMIN);

  http_path_add("/dvb/addmux", 
		NULL, extjs_dvb_addmux, ACCESS_ADMIN);
  http_path_add("/dvb/copymux", 
		NULL, extjs_dvb_copymux, ACCESS_ADMIN);
#endif

}
