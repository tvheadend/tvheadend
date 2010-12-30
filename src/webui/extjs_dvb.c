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
#include "psi.h"
#include "serviceprobe.h"

#include "dvb/dvb.h"
#include "dvb/dvb_support.h"
#include "dvb/dvb_preconf.h"
#include "dvr/dvr.h"

#if 0

/**
 *
 */
static int
extjs_dvbnetworks(http_connection_t *hc, const char *remain, void *opaque)
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

  if((out = dvb_mux_preconf_get_node(tda->tda_type, s)) == NULL)
    return 404;

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}

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
  const char *s, *sc;
  th_dvb_mux_instance_t *tdmi;
  service_t *t;

  pthread_mutex_lock(&global_lock);

  if(remain == NULL) {
    /* Just list all adapters */

    ref = sibling != NULL ? dvb_adapter_find_by_identifier(sibling) : NULL;

    array = htsmsg_create_list();

    TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link) {
      if(ref == NULL || (ref != tda && ref->tda_type == tda->tda_type))
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
    htsmsg_add_str(r, "device", tda->tda_rootpath ?: "No hardware attached");
    htsmsg_add_str(r, "name", tda->tda_displayname);
    htsmsg_add_u32(r, "automux", tda->tda_autodiscovery);
    htsmsg_add_u32(r, "idlescan", tda->tda_idlescan);
    htsmsg_add_u32(r, "qmon", tda->tda_qmon);
    htsmsg_add_u32(r, "dumpmux", tda->tda_dump_muxes);
    htsmsg_add_u32(r, "nitoid", tda->tda_nitoid);
    htsmsg_add_str(r, "diseqcversion", 
		   ((const char *[]){"DiSEqC 1.0 / 2.0",
				       "DiSEqC 1.1 / 2.1"})
		   [tda->tda_diseqc_version % 2]);
 
    out = json_single_record(r, "dvbadapters");
  } else if(!strcmp(op, "save")) {

    if((s = http_arg_get(&hc->hc_req_args, "name")) != NULL)
      dvb_adapter_set_displayname(tda, s);

    s = http_arg_get(&hc->hc_req_args, "automux");
    dvb_adapter_set_auto_discovery(tda, !!s);

    s = http_arg_get(&hc->hc_req_args, "idlescan");
    dvb_adapter_set_idlescan(tda, !!s);

    s = http_arg_get(&hc->hc_req_args, "qmon");
    dvb_adapter_set_qmon(tda, !!s);

    s = http_arg_get(&hc->hc_req_args, "dumpmux");
    dvb_adapter_set_dump_muxes(tda, !!s);

    if((s = http_arg_get(&hc->hc_req_args, "nitoid")) != NULL)
      dvb_adapter_set_nitoid(tda, atoi(s));

    if((s = http_arg_get(&hc->hc_req_args, "diseqcversion")) != NULL) {
      if(!strcmp(s, "DiSEqC 1.0 / 2.0"))
	dvb_adapter_set_diseqc_version(tda, 0);
      else if(!strcmp(s, "DiSEqC 1.1 / 2.1"))
	dvb_adapter_set_diseqc_version(tda, 1);
    }

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);
  } else if(!strcmp(op, "addnetwork")) {

    sc = http_arg_get(&hc->hc_req_args, "satconf");

    if((s = http_arg_get(&hc->hc_req_args, "network")) != NULL)
      dvb_mux_preconf_add_network(tda, s, sc);

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "serviceprobe")) {

    tvhlog(LOG_NOTICE, "web interface",
	   "Service probe started on \"%s\"", tda->tda_displayname);

    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
      LIST_FOREACH(t, &tdmi->tdmi_transports, s_group_link) {
	if(t->s_enabled)
	  serviceprobe_enqueue(t);
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

    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link)
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

    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
      LIST_FOREACH(t, &tdmi->tdmi_transports, s_group_link) {
	count++;
      }
    }

    tvec = alloca(sizeof(service_t *) * count);

    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
      LIST_FOREACH(t, &tdmi->tdmi_transports, s_group_link) {
	tvec[i++] = t;
      }
    }

    qsort(tvec, count, sizeof(service_t *), transportcmp);

    for(i = 0; i < count; i++)
      htsmsg_add_msg(array, NULL, dvb_transport_build_msg(tvec[i]));

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

  pthread_mutex_lock(&global_lock);

  if(remain == NULL ||
     (tda = dvb_adapter_find_by_identifier(remain)) == NULL) {
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
  const char *id;
  htsmsg_field_t *f;
  th_dvb_mux_instance_t *tdmi;

  in = entries != NULL ? htsmsg_json_deserialize(entries) : NULL;

  if(in == NULL)
    return 400;

  pthread_mutex_lock(&global_lock);
 
  if(remain == NULL ||
     (tda = dvb_adapter_find_by_identifier(remain)) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }


  TAILQ_FOREACH(f, &in->hm_fields, hmf_link) {
    if((id = htsmsg_field_get_string(f)) != NULL &&
       (tdmi = dvb_mux_find_by_identifier(id)) != NULL &&
       tda != tdmi->tdmi_adapter) {

      if(dvb_mux_copy(tda, tdmi)) {
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

/**
 *
 */
void
extjs_list_dvb_adapters(htsmsg_t *array)
{
  th_dvb_adapter_t *tda;
  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link)
    htsmsg_add_msg(array, NULL, dvb_adapter_build_msg(tda));
}


/**
 * DVB WEB user interface
 */
void
extjs_start_dvb(void)
{
  http_path_add("/dvbnetworks", 
		NULL, extjs_dvbnetworks, ACCESS_WEB_INTERFACE);

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

}
#endif
