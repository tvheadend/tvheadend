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
#include "channels.h"
#include "psi.h"

#include "dvb/dvb.h"
#include "dvb/dvb_support.h"
#include "dvb/dvb_preconf.h"
#include "transports.h"
#include "serviceprobe.h"

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
extjs_root(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;

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
  extjs_load(hq, "static/app/chconf.js");

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
  http_output_html(hc);
  return 0;
}


/**
 *
 */
static int
extjs_tablemgr(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  dtable_t *dt;
  htsmsg_t *out = NULL, *in, *array;

  const char *tablename = http_arg_get(&hc->hc_req_args, "table");
  const char *op        = http_arg_get(&hc->hc_req_args, "op");
  const char *entries   = http_arg_get(&hc->hc_req_args, "entries");

  if(tablename == NULL || (dt = dtable_find(tablename)) == NULL)
    return 404;
  
  in = entries != NULL ? htsmsg_json_deserialize(entries) : NULL;

  pthread_mutex_lock(&global_lock);

  if(!strcmp(op, "create")) {
    out = dtable_record_create(dt);

  } else if(!strcmp(op, "get")) {
    array = dtable_record_get_all(dt);

    out = htsmsg_create();
    htsmsg_add_msg(out, "entries", array);

  } else if(!strcmp(op, "update")) {
    if(in == NULL)
      goto bad;

    dtable_record_update_by_array(dt, in);

  } else if(!strcmp(op, "delete")) {
    if(in == NULL)
      goto bad;

    dtable_record_delete_by_array(dt, in);

  } else {
  bad:
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_BAD_REQUEST;
  }

  pthread_mutex_unlock(&global_lock);

  if(in != NULL)
    htsmsg_destroy(in);

  if(out != NULL) {
    htsmsg_json_serialize(out, hq, 0);
    htsmsg_destroy(out);
  }
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}


/**
 *
 */
static int
extjs_chlist(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *array, *c;
  channel_t *ch;

  out = htsmsg_create();

  array = htsmsg_create_array();

  pthread_mutex_lock(&global_lock);

  RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
    c = htsmsg_create();
    htsmsg_add_str(c, "name", ch->ch_name);
    htsmsg_add_u32(c, "chid", ch->ch_id);
    htsmsg_add_msg(array, NULL, c);
  }

  pthread_mutex_unlock(&global_lock);

  htsmsg_add_msg(out, "entries", array);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
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
extjs_dvbtree(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *s = http_arg_get(&hc->hc_req_args, "node");
  htsmsg_t *out = NULL;
  char buf[200];
  th_dvb_adapter_t *tda;
  th_dvb_mux_instance_t *tdmi;
  th_transport_t *t;

  if(s == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  out = htsmsg_create_array();
  pthread_mutex_lock(&global_lock);

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
			 dvb_mux_status(tdmi),
			 tdmi->tdmi_quality, "mux");
    }
  } else if((tdmi = dvb_mux_find_by_identifier(s)) != NULL) {

    LIST_FOREACH(t, &tdmi->tdmi_transports, tht_mux_link) {

      if(transport_servicetype_txt(t) == NULL)
	continue;

      extjs_dvbtree_node(out, 1,
			 t->tht_identifier, t->tht_svcname,
			 transport_servicetype_txt(t),
			 t->tht_ch ? "Mapped" : "Unmapped",
			 100, "transport");
    }
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

  if((tda = dvb_adapter_find_by_identifier(a)) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_BAD_REQUEST;
  }

  pthread_mutex_unlock(&global_lock);

  out = dvb_mux_preconf_get_node(tda->tda_type, s);

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
  
  out = htsmsg_create();
  array = htsmsg_create_array();

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
  const char *s  = http_arg_get(&hc->hc_req_args, "adapterId");
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  th_dvb_adapter_t *tda = s ? dvb_adapter_find_by_identifier(s) : NULL;
  //th_dvb_mux_instance_t *tdmi;
  //  th_transport_t *t;

  htsmsg_t *r, *out;

  if(tda == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  pthread_mutex_lock(&global_lock);

  if(!strcmp(op, "load")) {
    r = htsmsg_create();
    htsmsg_add_str(r, "id", tda->tda_identifier);
    htsmsg_add_str(r, "device", tda->tda_rootpath ?: "No hardware attached");
    htsmsg_add_str(r, "name", tda->tda_displayname);
    htsmsg_add_u32(r, "automux", tda->tda_autodiscovery);
 
    out = json_single_record(r, "dvbadapters");
  } else if(!strcmp(op, "save")) {

    if((s = http_arg_get(&hc->hc_req_args, "name")) != NULL)
      dvb_adapter_set_displayname(tda, s);

    if((s = http_arg_get(&hc->hc_req_args, "automux")) != NULL)
      dvb_adapter_set_auto_discovery(tda, 1);
    else
      dvb_adapter_set_auto_discovery(tda, 0);

    out = htsmsg_create();
    htsmsg_add_u32(out, "success", 1);
  } else if(!strcmp(op, "addnetwork")) {
    if((s = http_arg_get(&hc->hc_req_args, "network")) != NULL)
      dvb_mux_preconf_add_network(tda, s);

    out = htsmsg_create();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "serviceprobe")) {

    tvhlog(LOG_NOTICE, "web interface",
	   "Service probe started on \"%s\"", tda->tda_displayname);
#if 0
    RB_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
      LIST_FOREACH(t, &tdmi->tdmi_transports, tht_mux_link) {
	serviceprobe_add(t);
      }
    }
#endif

    out = htsmsg_create();
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
static htsmsg_t *
build_transport_msg(th_transport_t *t)
{
  htsmsg_t *r = htsmsg_create();
  th_stream_t *st;

  char video[200];
  char audio[200];
  char subtitles[200];
  char scrambling[200];

  htsmsg_add_u32(r, "enabled", !t->tht_disabled);
  htsmsg_add_str(r, "name", t->tht_svcname);

  htsmsg_add_str(r, "provider", t->tht_provider ?: "");
  htsmsg_add_str(r, "network", t->tht_networkname(t));
  htsmsg_add_str(r, "source", t->tht_sourcename(t));

  htsmsg_add_str(r, "status", transport_status_to_text(t->tht_last_status));

  video[0] = 0;
  audio[0] = 0;
  subtitles[0] = 0;
  scrambling[0] = 0;

  LIST_FOREACH(st, &t->tht_streams, st_link) {
    switch(st->st_type) {
    case HTSTV_TELETEXT:
    case HTSTV_SUBTITLES:
    case HTSTV_PAT:
    case HTSTV_PMT:
      break;

    case HTSTV_MPEG2VIDEO:
      snprintf(video + strlen(video), sizeof(video) - strlen(video),
	       "%sMPEG-2 (PID:%d", strlen(video) > 0 ? ", " : "",
	       st->st_pid);

    video:
      if(st->st_frame_duration) {
	snprintf(video + strlen(video), sizeof(video) - strlen(video),
		 ", %d Hz)", 90000 / st->st_frame_duration);
      } else {
	snprintf(video + strlen(video), sizeof(video) - strlen(video),
		 ")");
      }

      break;

    case HTSTV_H264:
      snprintf(video + strlen(video), sizeof(video) - strlen(video),
	       "%sH.264 (PID:%d", strlen(video) > 0 ? ", " : "",
	       st->st_pid);
      goto video;

    case HTSTV_MPEG2AUDIO:
      snprintf(audio + strlen(audio), sizeof(audio) - strlen(audio),
	       "%sMPEG-2 (PID:%d", strlen(audio) > 0 ? ", " : "",
	       st->st_pid);
    audio:
      if(st->st_lang[0]) {
	snprintf(audio + strlen(audio), sizeof(audio) - strlen(audio),
		 ", languange: \"%s\")", st->st_lang);
      } else {
	snprintf(audio + strlen(audio), sizeof(audio) - strlen(audio),
		 ")");
      }
      break;

    case HTSTV_AC3:
      snprintf(audio + strlen(audio), sizeof(audio) - strlen(audio),
	       "%sAC3 (PID:%d", strlen(audio) > 0 ? ", " : "",
	       st->st_pid);
      goto audio;

    case HTSTV_CA:
      snprintf(scrambling + strlen(scrambling),
	       sizeof(scrambling) - strlen(scrambling),
	       "%s%s", strlen(scrambling) > 0 ? ", " : "",
	       psi_caid2name(st->st_caid));
      break;
    }
  }
   
  htsmsg_add_str(r, "video", video);
  htsmsg_add_str(r, "audio", audio);
  htsmsg_add_str(r, "scrambling", scrambling[0] ? scrambling : "none");
  return r;
}


/**
 *
 */
static int
extjs_channel(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *s  = http_arg_get(&hc->hc_req_args, "chid");
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  channel_t *ch;
  channel_t *ch2;
  th_transport_t *t;
  int reloadchlist = 0;
  htsmsg_t *out, *array, *r;

  pthread_mutex_lock(&global_lock);

  ch = s ? channel_find_by_identifier(atoi(s)) : NULL;
  if(ch == NULL) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_BAD_REQUEST;
  }

  if(!strcmp(op, "load")) {
    r = htsmsg_create();
    htsmsg_add_u32(r, "id", ch->ch_id);
    htsmsg_add_str(r, "name", ch->ch_name);
    htsmsg_add_str(r, "comdetect", "tt192");
    out = json_single_record(r, "channels");

  } else if(!strcmp(op, "gettransports")) {
    out = htsmsg_create();
    array = htsmsg_create_array();
    LIST_FOREACH(t, &ch->ch_transports, tht_ch_link)
      htsmsg_add_msg(array, NULL, build_transport_msg(t));

    htsmsg_add_msg(out, "entries", array);

  } else if(!strcmp(op, "delete")) {

    channel_delete(ch);

    out = htsmsg_create();
    htsmsg_add_u32(out, "reloadchlist", 1);
    htsmsg_add_u32(out, "success", 1);
 
  } else if(!strcmp(op, "mergefrom")) {
    
    if((s = http_arg_get(&hc->hc_req_args, "srcch")) == NULL)
      return HTTP_STATUS_BAD_REQUEST;
    
    ch2 = channel_find_by_identifier(atoi(s));
    if(ch2 == NULL || ch2 == ch)
      return HTTP_STATUS_BAD_REQUEST;

    channel_merge(ch, ch2); /* ch2 goes away here */

    out = htsmsg_create();
    htsmsg_add_u32(out, "reloadchlist", 1);
    htsmsg_add_u32(out, "success", 1);
 	

  } else if(!strcmp(op, "save")) {

    if((s = http_arg_get(&hc->hc_req_args, "name")) != NULL &&
       strcmp(s, ch->ch_name)) {
      
      if(channel_rename(ch, s)) {
	out = htsmsg_create();
	htsmsg_add_u32(out, "success", 0);
	htsmsg_add_str(out, "errormsg", "Channel name already exist");
	goto response;
      } else {
	reloadchlist = 1;
      }
    }
    
    out = htsmsg_create();
    htsmsg_add_u32(out, "reloadchlist", 1);
    htsmsg_add_u32(out, "success", 1);
 
  } else {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_BAD_REQUEST;
  }

 response:
  pthread_mutex_unlock(&global_lock);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);

  http_output_content(hc, "text/x-json; charset=UTF-8");
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
  http_path_add("/chlist",      NULL, extjs_chlist,      ACCESS_WEB_INTERFACE);
  http_path_add("/channel",     NULL, extjs_channel,     ACCESS_WEB_INTERFACE);
}
