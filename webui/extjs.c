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

#include <libhts/htsmsg.h>
#include <libhts/htsmsg_json.h>

#include "tvhead.h"
#include "http.h"
#include "webui.h"
#include "access.h"
#include "dtable.h"

#include "dvb/dvb.h"
#include "dvb/dvb_support.h"
#include "dvb/dvb_preconf.h"
#include "transports.h"

extern const char *htsversion;

static void
extjs_load(htsbuf_queue_t *hq, const char *script)
{
  htsbuf_qprintf(hq,
		 "<script type=\"text/javascript\" "
		 "src=\"%s\">"
		 "</script>\n", script);
		 
}

/**
 *
 */
static void
extjs_exec(htsbuf_queue_t *hq, const char *fmt, ...)
{
  va_list ap;

  htsbuf_qprintf(hq, "<script type=\"text/javascript\">\r\n");

  va_start(ap, fmt);
  htsbuf_vqprintf(hq, fmt, ap);
  va_end(ap);

  htsbuf_qprintf(hq, "\r\n</script>\r\n");
}


/**
 * PVR info, deliver info about the given PVR entry
 */
static int
extjs_root(http_connection_t *hc, http_reply_t *hr, 
	   const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hr->hr_q;

#define EXTJSPATH "static/extjs"

  htsbuf_qprintf(hq, "<html><body>\n"
		 "<link rel=\"stylesheet\" type=\"text/css\" href=\""EXTJSPATH"/resources/css/ext-all.css\">\n"
		 "<link rel=\"stylesheet\" type=\"text/css\" href=\"static/app/ext.css\">\n"
		 "<script type=\"text/javascript\" src=\""EXTJSPATH"/adapter/ext/ext-base.js\"></script>\n"
		 "<script type=\"text/javascript\" src=\""EXTJSPATH"/ext-all-debug.js\"></script>\n");

  extjs_exec(hq, "Ext.BLANK_IMAGE_URL = "
	     "'"EXTJSPATH"/resources/images/default/s.gif';");

  /**
   * Load extjs extensions
   */
  extjs_load(hq, "static/app/extensions.js");

  /**
   * Create a namespace for our app
   */
  extjs_exec(hq, "Ext.namespace('tvheadend');");

  /**
   * Load all components
   */
  extjs_load(hq, "static/app/acleditor.js");
  extjs_load(hq, "static/app/cwceditor.js");
  extjs_load(hq, "static/app/dvb.js");

  /**
   * Finally, the app itself
   */
  extjs_load(hq, "static/app/tvheadend.js");
  extjs_exec(hq, "Ext.onReady(tvheadend.app.init, tvheadend.app);");
  



  htsbuf_qprintf(hq,
		 "<style type=\"text/css\">\n"
		 "html, body {\n"
		 "\tfont:normal 12px verdana;\n"
		 "\tmargin:0;\n"
		 "\tpadding:0;\n"
		 "\tborder:0 none;\n"
		 "\toverflow:hidden;\n"
		 "\theight:100%;\n"
		 "}\n"
		 "#systemlog {\n"
		 "\tfont:normal 12px courier; font-weight: bold;\n"
		 "}\n"
		 "p {\n"
		 "\tmargin:5px;\n"
		 "}\n"
		 "</style>\n"
		 "<title>HTS Tvheadend %s</title>\n"
		 "</head>\n"
		 "<body>\n"
		 "<div id=\"systemlog\"></div>\n"
		 "</body></html>\n",
		 htsversion);
  http_output_html(hc, hr);
  return 0;
}


/**
 *
 */
static int
extjs_tablemgr(http_connection_t *hc, http_reply_t *hr, 
	       const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hr->hr_q;
  dtable_t *dt;
  htsmsg_t *out = NULL, *in, *array;

  const char *tablename = http_arg_get(&hc->hc_req_args, "table");
  const char *op        = http_arg_get(&hc->hc_req_args, "op");
  const char *entries   = http_arg_get(&hc->hc_req_args, "entries");

  if(tablename == NULL || (dt = dtable_find(tablename)) == NULL)
    return 404;
  
  in = entries != NULL ? htsmsg_json_deserialize(entries) : NULL;

  if(!strcmp(op, "create")) {
    out = dtable_record_create(dt);

  } else if(!strcmp(op, "get")) {
    array = dtable_record_get_all(dt);

    out = htsmsg_create();
    htsmsg_add_msg(out, "entries", array);

  } else if(!strcmp(op, "update")) {
    if(in == NULL)
      return HTTP_STATUS_BAD_REQUEST;

    dtable_record_update_by_array(dt, in);

  } else if(!strcmp(op, "delete")) {
    if(in == NULL)
      return HTTP_STATUS_BAD_REQUEST;

    dtable_record_delete_by_array(dt, in);

  } else {
    return HTTP_STATUS_BAD_REQUEST;
  }

  if(in != NULL)
    htsmsg_destroy(in);

  if(out != NULL) {
    htsmsg_json_serialize(out, hq, 0);
    htsmsg_destroy(out);
  }
  http_output(hc, hr, "text/x-json; charset=UTF-8", NULL, 0);
  return 0;
}

/**
 *
 */
static void
extjs_dvbtree_node(htsmsg_t *array, int leaf, const char *id, const char *name,
		   const char *type, const char *status, int quality,
		   const char *itype)
{
  htsmsg_t *e = htsmsg_create();

  htsmsg_add_str(e, "uiProvider", "col");
  htsmsg_add_str(e, "id", id);
  htsmsg_add_u32(e, "leaf", leaf);
  htsmsg_add_str(e, "itype", itype);

  htsmsg_add_str(e, "name", name);
  htsmsg_add_str(e, "type", type);
  htsmsg_add_str(e, "status", status);
  htsmsg_add_u32(e, "quality", quality);

  htsmsg_add_msg(array, NULL, e);
}

/**
 *
 */
static int
extjs_dvbtree(http_connection_t *hc, http_reply_t *hr, 
	      const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hr->hr_q;
  const char *s = http_arg_get(&hc->hc_req_args, "node");
  htsmsg_t *out = NULL;
  char buf[200];
  th_dvb_adapter_t *tda;
  th_dvb_mux_instance_t *tdmi;
  th_transport_t *t;

  if(s == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  out = htsmsg_create_array();

  if(!strcmp(s, "root")) {
    /**
     * List of all adapters
     */

    TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link) {

      snprintf(buf, sizeof(buf), "%s adapter",
	       dvb_adaptertype_to_str(tda->tda_type));

      extjs_dvbtree_node(out, 0,
			 tda->tda_identifier, tda->tda_displayname,
			 buf, tda->tda_rootpath != NULL ? "OK" : "No H/W",
			 100, "adapter");
    }
  } else if((tda = dvb_adapter_find_by_identifier(s)) != NULL) {

    RB_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
      dvb_mux_nicename(buf, sizeof(buf), tdmi);
     
      extjs_dvbtree_node(out, 0,
			 tdmi->tdmi_identifier, buf, "DVB Mux",
			 tdmi->tdmi_last_status,
			 100 + tdmi->tdmi_quality * 2, "mux");
    }
  } else if((tdmi = dvb_mux_find_by_identifier(s)) != NULL) {

    LIST_FOREACH(t, &tdmi->tdmi_transports, tht_mux_link) {

      if(transport_servicetype_txt(t) == NULL)
	continue;

      extjs_dvbtree_node(out, 1,
			 t->tht_identifier, t->tht_svcname,
			 transport_servicetype_txt(t),
			 "OK", 100, "transport");
    }
  }
 
  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output(hc, hr, "text/x-json; charset=UTF-8", NULL, 0);
  return 0;
}


/**
 *
 */
static int
extjs_dvbnetworks(http_connection_t *hc, http_reply_t *hr, 
		  const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hr->hr_q;
  const char *s = http_arg_get(&hc->hc_req_args, "node");
  const char *a = http_arg_get(&hc->hc_req_args, "adapter");
  th_dvb_adapter_t *tda;
  htsmsg_t *out = NULL;

  if(s == NULL || a == NULL)
    return HTTP_STATUS_BAD_REQUEST;
  
  if((tda = dvb_adapter_find_by_identifier(a)) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  out = dvb_mux_preconf_get_node(tda->tda_type, s);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output(hc, hr, "text/x-json; charset=UTF-8", NULL, 0);
  return 0;
}

/**
 *
 */
static htsmsg_t *
json_single_record(htsmsg_t *rec, const char *root)
{
  htsmsg_t *out, *array;
  
  out = htsmsg_create();
  array = htsmsg_create_array();

  htsmsg_add_msg(array, NULL, rec);
  htsmsg_add_msg(out, "dvbadapters", array);
  return out;

}

/**
 *
 */
static int
extjs_dvbadapter(http_connection_t *hc, http_reply_t *hr, 
		 const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hr->hr_q;
  const char *s  = http_arg_get(&hc->hc_req_args, "adapterId");
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  th_dvb_adapter_t *tda = s ? dvb_adapter_find_by_identifier(s) : NULL;

  htsmsg_t *r, *out;

  if(tda == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if(!strcmp(op, "load")) {
    r = htsmsg_create();
    htsmsg_add_str(r, "id", tda->tda_identifier);
    htsmsg_add_str(r, "device", tda->tda_rootpath ?: "No hardware attached");
    htsmsg_add_str(r, "name", tda->tda_displayname);
    htsmsg_add_u32(r, "automux", 1);
 
    out = json_single_record(r, "dvbadapters");
  } else if(!strcmp(op, "save")) {

    if((s = http_arg_get(&hc->hc_req_args, "name")) != NULL)
      dvb_adapter_set_displayname(tda, s);

    out = htsmsg_create();
    htsmsg_add_u32(out, "success", 1);
  } else if(!strcmp(op, "addnetwork")) {
    if((s = http_arg_get(&hc->hc_req_args, "network")) != NULL)
      dvb_mux_preconf_add_network(tda, s);

    out = htsmsg_create();
    htsmsg_add_u32(out, "success", 1);

  } else {
    return HTTP_STATUS_BAD_REQUEST;
  }

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);

  http_output(hc, hr, "text/x-json; charset=UTF-8", NULL, 0);
  return 0;
}

/**
 * WEB user interface
 */
void
extjs_start(void)
{
  http_path_add("/extjs.html",  NULL, extjs_root,        ACCESS_WEB_INTERFACE);
  http_path_add("/tablemgr",    NULL, extjs_tablemgr,    ACCESS_WEB_INTERFACE);
  http_path_add("/dvbtree",     NULL, extjs_dvbtree,     ACCESS_WEB_INTERFACE);
  http_path_add("/dvbadapter",  NULL, extjs_dvbadapter,  ACCESS_WEB_INTERFACE);
  http_path_add("/dvbnetworks", NULL, extjs_dvbnetworks, ACCESS_WEB_INTERFACE);
}
