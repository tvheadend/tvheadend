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

#if 0
/**
 *
 */
static int
extjs_dvblocations(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *s = http_arg_get(&hc->hc_req_args, "node");
  const char *a = http_arg_get(&hc->hc_req_args, "adapter");
  th_dvb_adapter_t *tda;
  htsmsg_t *out = NULL;

  if(s == NULL || a == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_ADMIN)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  if((tda = dvb_adapter_find_by_identifier(a)) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_BAD_REQUEST;
  }

  pthread_mutex_unlock(&global_lock);

  if((out = dvb_mux_preconf_get_node(tda->tda_fe_type, s)) == NULL)
    return 404;

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}
#endif


#if 0
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
extjs_dvbadapter(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  th_dvb_adapter_t *tda, *ref;
  htsmsg_t *out, *array, *r;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  const char *sibling = http_arg_get(&hc->hc_req_args, "sibling");
  const char *s;

  pthread_mutex_lock(&global_lock);

  if(remain == NULL) {
    /* Just list all adapters */

    ref = sibling != NULL ? dvb_adapter_find_by_identifier(sibling) : NULL;

    array = htsmsg_create_list();

    TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link) {
      if(ref == NULL || (ref != tda && ref->tda_fe_type == tda->tda_fe_type))
	htsmsg_add_msg(array, NULL, dvb_adapter_build_msg(tda));
    }
    pthread_mutex_unlock(&global_lock);
    out = htsmsg_create_map();
    htsmsg_add_msg(out, "entries", array);

    htsmsg_json_serialize(out, hq, 0);
    htsmsg_destroy(out);
    http_output_content(hc, "text/x-json; charset=UTF-8");
    return 0;
  }

  if((tda = dvb_adapter_find_by_identifier(remain)) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  if(!strcmp(op, "load")) {
    r = htsmsg_create_map();
    htsmsg_add_str(r, "id", tda->tda_identifier);
    htsmsg_add_u32(r, "enabled", tda->tda_enabled);
    htsmsg_add_str(r, "device", tda->tda_rootpath ?: "No hardware attached");
    htsmsg_add_str(r, "name", tda->tda_displayname);
    htsmsg_add_u32(r, "skip_initialscan", tda->tda_skip_initialscan);
    htsmsg_add_u32(r, "idleclose", tda->tda_idleclose);
    htsmsg_add_u32(r, "qmon", tda->tda_qmon);
    htsmsg_add_u32(r, "poweroff", tda->tda_poweroff);
    htsmsg_add_u32(r, "sidtochan", tda->tda_sidtochan);
    htsmsg_add_u32(r, "full_mux_rx", tda->tda_full_mux_rx+1);
    htsmsg_add_u32(r, "grace_period", tda->tda_grace_period);
    htsmsg_add_str(r, "diseqcversion", 
		   ((const char *[]){"DiSEqC 1.0 / 2.0",
				       "DiSEqC 1.1 / 2.1"})
		   [tda->tda_diseqc_version % 2]);
    htsmsg_add_str(r, "diseqcrepeats",
		   ((const char *[]){"0", "1", "3"})
		   [tda->tda_diseqc_repeats % 3]);
    htsmsg_add_u32(r, "extrapriority", tda->tda_extrapriority);
 
    out = json_single_record(r, "dvbadapters");
  } else if(!strcmp(op, "save")) {

    if((s = http_arg_get(&hc->hc_req_args, "name")) != NULL)
      dvb_adapter_set_displayname(tda, s);

    s = http_arg_get(&hc->hc_req_args, "enabled");
    dvb_adapter_set_enabled(tda, !!s);

    s = http_arg_get(&hc->hc_req_args, "automux");
    dvb_adapter_set_auto_discovery(tda, !!s);

    s = http_arg_get(&hc->hc_req_args, "skip_initialscan");
    dvb_adapter_set_skip_initialscan(tda, !!s);

    s = http_arg_get(&hc->hc_req_args, "idleclose");
    dvb_adapter_set_idleclose(tda, !!s);

    s = http_arg_get(&hc->hc_req_args, "qmon");
    dvb_adapter_set_qmon(tda, !!s);

    s = http_arg_get(&hc->hc_req_args, "poweroff");
    dvb_adapter_set_poweroff(tda, !!s);

    s = http_arg_get(&hc->hc_req_args, "sidtochan");
    dvb_adapter_set_sidtochan(tda, !!s);

    s = http_arg_get(&hc->hc_req_args, "full_mux_rx");
    dvb_adapter_set_full_mux_rx(tda, atoi(s)-1);

    s = http_arg_get(&hc->hc_req_args, "grace_period");
    dvb_adapter_set_grace_period(tda, atoi(s));

    if((s = http_arg_get(&hc->hc_req_args, "nitoid")) != NULL)
      dvb_adapter_set_nitoid(tda, atoi(s));

    if((s = http_arg_get(&hc->hc_req_args, "diseqcversion")) != NULL) {
      if(!strcmp(s, "DiSEqC 1.0 / 2.0"))
	dvb_adapter_set_diseqc_version(tda, 0);
      else if(!strcmp(s, "DiSEqC 1.1 / 2.1"))
	dvb_adapter_set_diseqc_version(tda, 1);
    }

    if((s = http_arg_get(&hc->hc_req_args, "diseqcrepeats")) != NULL) {
      if(!strcmp(s, "0"))
        dvb_adapter_set_diseqc_repeats(tda, 0);
      else if(!strcmp(s, "1"))
        dvb_adapter_set_diseqc_repeats(tda, 1);
      else if(!strcmp(s, "2"))
        dvb_adapter_set_diseqc_repeats(tda, 2);
    }
    if((s = http_arg_get(&hc->hc_req_args, "extrapriority")) != NULL)
      dvb_adapter_set_extrapriority(tda, atoi(s));

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);
  } else if(!strcmp(op, "addnetwork")) {

//    sc = http_arg_get(&hc->hc_req_args, "satconf");

    if((s = http_arg_get(&hc->hc_req_args, "network")) != NULL)
      dvb_mux_preconf_add_network(tda->tda_dn, s);

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "serviceprobe")) {

    tvhlog(LOG_NOTICE, "web interface",
	   "Service probe started on \"%s\"", tda->tda_displayname);

    dvb_mux_t *dm;
    service_t *s;

    LIST_FOREACH(dm, &tda->tda_dn->dn_muxes, dm_network_link) {
      LIST_FOREACH(s, &dm->dm_services, s_group_link) {
        if(s->s_enabled)
	  serviceprobe_enqueue(s);
      }
    }


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
#endif

#if 0

/**
 *
 */
static void
mux_update(htsmsg_t *in)
{
  htsmsg_field_t *f;
  htsmsg_t *c;
  th_dvb_mux_instance_t *tdmi;
  uint32_t u32;
  const char *id;

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link) {
    if((c = htsmsg_get_map_by_field(f)) == NULL ||
       (id = htsmsg_get_str(c, "id")) == NULL)
      continue;
    
    if((tdmi = dvb_mux_find_by_identifier(id)) == NULL)
      continue;

    if(!htsmsg_get_u32(c, "enabled", &u32))
      dvb_mux_set_enable(tdmi, u32);
  }
}


/**
 *
 */
static void
mux_delete(htsmsg_t *in)
{
  htsmsg_field_t *f;
  th_dvb_mux_instance_t *tdmi;
  const char *id;

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link) {
    if((id = htsmsg_field_get_string(f)) != NULL &&
       (tdmi = dvb_mux_find_by_identifier(id)) != NULL)
      dvb_mux_destroy(tdmi);
  }
}


/**
 *
 */
static int
extjs_dvbmuxes(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  th_dvb_adapter_t *tda;
  htsmsg_t *out, *array, *in;
  const char *op        = http_arg_get(&hc->hc_req_args, "op");
  const char *entries   = http_arg_get(&hc->hc_req_args, "entries");
  th_dvb_mux_instance_t *tdmi;

  pthread_mutex_lock(&global_lock);

  if(remain == NULL ||
     (tda = dvb_adapter_find_by_identifier(remain)) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  in = entries != NULL ? htsmsg_json_deserialize(entries) : NULL;

  out = htsmsg_create_map();

  if(!strcmp(op, "get")) {
    array = htsmsg_create_list();

    LIST_FOREACH(tdmi, &tda->tda_dn->dn_mux_instances, tdmi_adapter_link)
      htsmsg_add_msg(array, NULL, dvb_mux_build_msg(tdmi));

    htsmsg_add_msg(out, "entries", array);
  } else if(!strcmp(op, "update")) {
    if(in != NULL)
      mux_update(in);

    out = htsmsg_create_map();

  } else if(!strcmp(op, "delete")) {
    if(in != NULL)
      mux_delete(in);
    
    out = htsmsg_create_map();

  } else {
    pthread_mutex_unlock(&global_lock);
    if(in != NULL)
      htsmsg_destroy(in);
    htsmsg_destroy(out);
    return HTTP_STATUS_BAD_REQUEST;
  }

  pthread_mutex_unlock(&global_lock);
 
  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");

  if(in != NULL)
    htsmsg_destroy(in);

  return 0;
}




/**
 *
 */
static int
transportcmp(const void *A, const void *B)
{
  service_t *a = *(service_t **)A;
  service_t *b = *(service_t **)B;

  return strcasecmp(a->s_svcname ?: "\0377", b->s_svcname ?: "\0377");
}

/**
 *
 */
static int
extjs_dvbservices(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  th_dvb_adapter_t *tda;
  htsmsg_t *out, *array, *in;
  const char *op        = http_arg_get(&hc->hc_req_args, "op");
  const char *entries   = http_arg_get(&hc->hc_req_args, "entries");
  th_dvb_mux_instance_t *tdmi;
  service_t *t, **tvec;
  int count = 0, i = 0;

  pthread_mutex_lock(&global_lock);

  if(remain == NULL ||
     (tda = dvb_adapter_find_by_identifier(remain)) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  in = entries != NULL ? htsmsg_json_deserialize(entries) : NULL;

  if(!strcmp(op, "get")) {

    out = htsmsg_create_map();
    array = htsmsg_create_list();

    LIST_FOREACH(tdmi, &tda->tda_dn->dn_mux_instances, tdmi_adapter_link) {
      LIST_FOREACH(t, &tdmi->tdmi_mux->dm_services, s_group_link) {
	count++;
      }
    }

    tvec = alloca(sizeof(service_t *) * count);

    LIST_FOREACH(tdmi, &tda->tda_dn->dn_mux_instances, tdmi_adapter_link) {
      LIST_FOREACH(t, &tdmi->tdmi_mux->dm_services, s_group_link) {
	tvec[i++] = t;
      }
    }

    qsort(tvec, count, sizeof(service_t *), transportcmp);

    for(i = 0; i < count; i++)
      htsmsg_add_msg(array, NULL, dvb_service_build_msg(tvec[i]));

    htsmsg_add_msg(out, "entries", array);

  } else if(!strcmp(op, "update")) {
    if(in != NULL)
      extjs_service_update(in);

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
static int
extjs_lnbtypes(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out;

  out = htsmsg_create_map();

  htsmsg_add_msg(out, "entries", dvb_lnblist_get());

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}  




/**
 *
 */
static int
extjs_dvbsatconf(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  th_dvb_adapter_t *tda;
  htsmsg_t *out;
  const char *adapter = http_arg_get(&hc->hc_req_args, "adapter");

  pthread_mutex_lock(&global_lock);

  if((remain == NULL ||
      (tda = dvb_adapter_find_by_identifier(remain)) == NULL) &&
     (adapter == NULL ||
      (tda = dvb_adapter_find_by_identifier(adapter)) == NULL)) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  out = htsmsg_create_map();
  htsmsg_add_msg(out, "entries", dvb_satconf_list(tda));

  pthread_mutex_unlock(&global_lock);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);

  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;  
}


/**
 *
 */
static int
extjs_dvb_feopts(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  char *a, *r;
  th_dvb_adapter_t *tda;
  htsmsg_t *e, *out;

  if(remain == NULL)
    return 400;

  r = tvh_strdupa(remain);
  if((a = strchr(r, '/')) == NULL)
    return 400;
  *a++ = 0;

  pthread_mutex_lock(&global_lock);

  if((tda = dvb_adapter_find_by_identifier(a)) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  e = dvb_fe_opts(tda, r);

  if(e == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 400;
  }

  out = htsmsg_create_map();
  htsmsg_add_msg(out, "entries", e);

  pthread_mutex_unlock(&global_lock);

  htsmsg_json_serialize(out, hq, 0);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  htsmsg_destroy(out);
  return 0;
}

/**
 *
 */
static int
extjs_dvb_addmux(http_connection_t *hc, const char *remain, void *opaque)
{
  htsmsg_t *out;
  htsbuf_queue_t *hq = &hc->hc_reply;
  struct http_arg_list *args = &hc->hc_req_args;
  th_dvb_adapter_t *tda;
  const char *err;
  pthread_mutex_lock(&global_lock);
 
  if(remain == NULL ||
     (tda = dvb_adapter_find_by_identifier(remain)) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  err =
    dvb_mux_add_by_params(tda,
			  atoi(http_arg_get(args, "frequency")?:"-1"),
			  atoi(http_arg_get(args, "symbolrate")?: "-1"),
			  atoi(http_arg_get(args, "bandwidthID")?: "-1"),
			  atoi(http_arg_get(args, "constellationID")?: "-1"),
			  atoi(http_arg_get(args, "delsysID")?: "-1"),
			  atoi(http_arg_get(args, "tmodeID")?: "-1"),
			  atoi(http_arg_get(args, "guardintervalID")?: "-1"),
			  atoi(http_arg_get(args, "hierarchyID")?: "-1"),
			  atoi(http_arg_get(args, "fechiID")?: "-1"),
			  atoi(http_arg_get(args, "fecloID")?: "-1"),
			  atoi(http_arg_get(args, "fecID")?: "-1"),
			  atoi(http_arg_get(args, "polarisationID")?: "-1"),
			  http_arg_get(args, "satconfID") ?: NULL);
			
  if(err != NULL)
    tvhlog(LOG_ERR, "web interface",
	   "Unable to create mux on %s: %s",
	   tda->tda_displayname, err);

  pthread_mutex_unlock(&global_lock);

  out = htsmsg_create_map();
  htsmsg_json_serialize(out, hq, 0);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  htsmsg_destroy(out);

  return 0;
}

#if 0
/**
 *
 */
static int
extjs_dvb_copymux(http_connection_t *hc, const char *remain, void *opaque)
{
  htsmsg_t *out;
  htsbuf_queue_t *hq = &hc->hc_reply;
  th_dvb_adapter_t *tda;
  htsmsg_t *in;
  const char *entries   = http_arg_get(&hc->hc_req_args, "entries");
  const char *satconf   = http_arg_get(&hc->hc_req_args, "satconf");
  const char *id;
  htsmsg_field_t *f;
  th_dvb_mux_instance_t *tdmi;
  dvb_satconf_t *sc = NULL;

  in = entries != NULL ? htsmsg_json_deserialize(entries) : NULL;

  if(in == NULL)
    return 400;

  pthread_mutex_lock(&global_lock);
 
  if(remain == NULL ||
     (tda = dvb_adapter_find_by_identifier(remain)) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  if (satconf) {
    sc = dvb_satconf_entry_find(tda, satconf, 0);
    if (sc == NULL) {
      pthread_mutex_unlock(&global_lock);
      return 404;
    }
  }

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link) {
    if((id = htsmsg_field_get_string(f)) != NULL &&
       (tdmi = dvb_mux_find_by_identifier(id)) != NULL &&
       tda != tdmi->tdmi_adapter) {

      if(dvb_mux_copy(tda, tdmi, sc)) {
	char buf[100];
	dvb_mux_nicename(buf, sizeof(buf), tdmi);

	tvhlog(LOG_NOTICE, "DVB", 
	       "Skipped copy of mux %s to adapter %s, mux already exist",
	       buf, tda->tda_displayname);
      }
    }
  }

  pthread_mutex_unlock(&global_lock);

  out = htsmsg_create_map();
  htsmsg_json_serialize(out, hq, 0);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  htsmsg_destroy(out);

  return 0;
}
#endif

#endif

/**
 *
 */
#if 0
static int
extjs_dvbnetworks(http_connection_t *hc, const char *remain, void *opaque)
{
  return extjs_get_idnode(hc, remain, opaque, &dvb_network_root);
}
#endif

#if 0
static int
extjs_mpegts_services_filter
  (mpegts_service_t *s, htsmsg_t *filter)
{
  htsmsg_t *e;
  htsmsg_field_t *f;
  const char *n;
  HTSMSG_FOREACH(f, filter) {
    if (!(e = htsmsg_get_map_by_field(f))) continue;
    if (!(n = htsmsg_get_str(e, "field"))) continue;
    if (!strcmp(n, "name"))
      if (!strstr(s->s_dvb_svcname ?: "", htsmsg_get_str(e, "value") ?: "")) return 1;
    if (!strcmp(n, "sid")) {
      const char *t = htsmsg_get_str(e, "comparison");
      const char *v = htsmsg_get_str(e, "value");
      if (!strcmp(t ?: "", "gt")) {
        if (s->s_dvb_service_id < atoi(v)) return 1;
      } else if (!strcmp(t ?: "", "lt")) {
        if (s->s_dvb_service_id > atoi(v)) return 1;
      } else if (!strcmp(t ?: "", "eq")) {
        if (s->s_dvb_service_id != atoi(v)) return 1;
      }
    }
  }
  return 0;
}
#endif

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

static int
extjs_mpegts_services
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
            mm->mm_display_name(mm, buf, sizeof(buf));
            htsmsg_add_str(e, "mux", buf);
            htsmsg_add_bool(e, "enabled", ms->s_enabled);
            htsmsg_add_u32(e, "sid", ms->s_dvb_service_id);
            htsmsg_add_u32(e, "pmt", ms->s_pmt_pid);
            htsmsg_add_u32(e, "lcn", ms->s_dvb_channel_num);
            if (ms->s_dvb_svcname)
              htsmsg_add_str(e, "name", ms->s_dvb_svcname);
            if (ms->s_dvb_provider)
              htsmsg_add_str(e, "provider", ms->s_dvb_provider);
            if (ms->s_dvb_cridauth)
              htsmsg_add_str(e, "crid_auth", ms->s_dvb_cridauth);
            if (ms->s_dvb_charset)
              htsmsg_add_str(e, "charset", ms->s_dvb_charset);
            htsmsg_add_u32(e, "type", ms->s_dvb_servicetype);
            htsmsg_add_msg(list, NULL, e);
            conf.limit--;
          }
        }
      }
    }
    pthread_mutex_unlock(&global_lock);
    htsmsg_add_msg(out, "entries", list);
    htsmsg_add_u32(out, "total",   total);
  }

  htsmsg_json_serialize(out, hq, 0);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  htsmsg_destroy(out);

  return 0;
}


static int
extjs_mpegts_muxes
  (http_connection_t *hc, const char *remain, void *opaque)
{
  char buf[256];
  mpegts_network_t *mn;
  mpegts_mux_t *mm;
  mpegts_service_t *ms;
  int s;
  htsmsg_t *out, *list = htsmsg_create_list();
  htsbuf_queue_t *hq = &hc->hc_reply;
  pthread_mutex_lock(&global_lock);
  LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
    LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link) {
      s = 0;
      htsmsg_t *e = htsmsg_create_map();
      htsmsg_add_str(e, "uuid", idnode_uuid_as_str(&mm->mm_id));
      mn->mn_display_name(mn, buf, sizeof(buf));
      htsmsg_add_str(e, "network", buf);
      mm->mm_display_name(mm, buf, sizeof(buf));
      htsmsg_add_str(e, "name", buf);
      htsmsg_add_bool(e, "enabled", mm->mm_enabled);
      htsmsg_add_u32(e, "onid", mm->mm_onid);
      htsmsg_add_u32(e, "tsid", mm->mm_tsid);
      htsmsg_add_bool(e, "initscan", mm->mm_initial_scan_done);
      htsmsg_add_str(e, "crid_auth", mm->mm_crid_authority ?: "");
      LIST_FOREACH(ms, &mm->mm_services, s_dvb_mux_link)
        s++;
      htsmsg_add_u32(e, "num_svc", s);
      htsmsg_add_msg(list, NULL, e);
    }
  }
  pthread_mutex_unlock(&global_lock);
  out = htsmsg_create_map();
  htsmsg_add_msg(out, "entries", list);
  htsmsg_json_serialize(out, hq, 0);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  htsmsg_destroy(out);

  return 0;
}

static int
extjs_mpegts_networks
  (http_connection_t *hc, const char *remain, void *opaque)
{
  char buf[256];
  mpegts_network_t *mn;
  mpegts_mux_t *mm;
  mpegts_service_t *ms;
  int m, s;
  htsmsg_t *out, *list = htsmsg_create_list();
  htsbuf_queue_t *hq = &hc->hc_reply;
  pthread_mutex_lock(&global_lock);
  LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
    m = s = 0;
    htsmsg_t *e = htsmsg_create_map();
    htsmsg_add_str(e, "uuid", idnode_uuid_as_str(&mn->mn_id));
    mn->mn_display_name(mn, buf, sizeof(buf));
    htsmsg_add_str(e, "name", buf);
    htsmsg_add_u32(e, "nid",  mn->mn_nid);
    htsmsg_add_u32(e, "autodiscovery",  mn->mn_autodiscovery);
    htsmsg_add_u32(e, "skipinitscan",   mn->mn_skipinitscan);
    LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link) {
      m++;
      LIST_FOREACH(ms, &mm->mm_services, s_dvb_mux_link)
        s++;
    }
    htsmsg_add_u32(e, "num_mux", m);
    htsmsg_add_u32(e, "num_svc", s);
    htsmsg_add_msg(list, NULL, e);
  }
  pthread_mutex_unlock(&global_lock);
  out = htsmsg_create_map();
  htsmsg_add_msg(out, "entries", list);
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
  printf("extjs_start_dvb()\n");
  http_path_add("/api/mpegts/networks", 
		NULL, extjs_mpegts_networks, ACCESS_WEB_INTERFACE);
  http_path_add("/api/mpegts/muxes", 
		NULL, extjs_mpegts_muxes, ACCESS_WEB_INTERFACE);
  http_path_add("/api/mpegts/services", 
		NULL, extjs_mpegts_services, ACCESS_WEB_INTERFACE);
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
