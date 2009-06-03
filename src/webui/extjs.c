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

#include "htsmsg.h"
#include "htsmsg_json.h"

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
#include "dvr/dvr.h"
#include "transports.h"
#include "serviceprobe.h"
#include "xmltv.h"
#include "epg.h"

extern const char *htsversion;
extern const char *htsversion_full;

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
		 "<script type=\"text/javascript\" src=\""EXTJSPATH"/adapter/ext/ext-base.js\"></script>\n"
		 "<script type=\"text/javascript\" src=\""EXTJSPATH"/ext-all-debug.js\"></script>\n"
		 "<link rel=\"stylesheet\" type=\"text/css\" href=\""EXTJSPATH"/resources/css/ext-all.css\">\n"
		 "<link rel=\"stylesheet\" type=\"text/css\" href=\"static/app/ext.css\">\n");

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
  extjs_load(hq, "static/app/tableeditor.js");
  extjs_load(hq, "static/app/cteditor.js");
  extjs_load(hq, "static/app/acleditor.js");
  extjs_load(hq, "static/app/cwceditor.js");
  extjs_load(hq, "static/app/dvb.js");
  extjs_load(hq, "static/app/chconf.js");
  extjs_load(hq, "static/app/epg.js");
  extjs_load(hq, "static/app/dvr.js");
  extjs_load(hq, "static/app/xmltv.js");

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
page_about(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;

  htsbuf_qprintf(hq, 
		 "<center>"
		 "<div class=\"about-title\">"
		 "HTS Tvheadend %s"
		 "</div><br>"
		 "&copy; 2006 - 2008 Andreas \303\226man, et al.<br><br>"
		 "<img src=\"docresources/tvheadendlogo.png\"><br>"
		 "<a href=\"http://hts.lonelycoder.com/\">"
		 "http://hts.lonelycoder.com/</a><br><br>"
		 "Based on software from "
		 "<a href=\"http://www.ffmpeg.org/\">FFmpeg</a> and "
		 "<a href=\"http://www.extjs.org/\">ExtJS</a>.<br>"
		 "<br>"
		 "Build: %s"
		 "</center>",
		 htsversion,
		 htsversion_full);

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
  
  if(http_access_verify(hc, dt->dt_dtc->dtc_read_access))
    return HTTP_STATUS_UNAUTHORIZED;

  in = entries != NULL ? htsmsg_json_deserialize(entries) : NULL;

  pthread_mutex_lock(&global_lock);

  if(!strcmp(op, "create")) {
    if(http_access_verify(hc, dt->dt_dtc->dtc_write_access))
      goto noaccess;

    out = dtable_record_create(dt);

  } else if(!strcmp(op, "get")) {
    array = dtable_record_get_all(dt);

    out = htsmsg_create_map();
    htsmsg_add_msg(out, "entries", array);

  } else if(!strcmp(op, "update")) {
    if(http_access_verify(hc, dt->dt_dtc->dtc_write_access))
      goto noaccess;

    if(in == NULL)
      goto bad;

    dtable_record_update_by_array(dt, in);

  } else if(!strcmp(op, "delete")) {
    if(http_access_verify(hc, dt->dt_dtc->dtc_write_access))
      goto noaccess;

    if(in == NULL)
      goto bad;

    dtable_record_delete_by_array(dt, in);

  } else {
  bad:
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_BAD_REQUEST;

  noaccess:
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

  out = htsmsg_create_map();

  array = htsmsg_create_list();

  pthread_mutex_lock(&global_lock);

  RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
    c = htsmsg_create_map();
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
 * EPG Content Groups
 */
static int
extjs_ecglist(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *array, *c;
  const char *s;
  int i;

  out = htsmsg_create_map();
  array = htsmsg_create_list();

  for(i = 0; i < 16; i++) {
    if((s = epg_content_group_get_name(i)) == NULL)
      continue;

    c = htsmsg_create_map();
    htsmsg_add_str(c, "name", s);
    htsmsg_add_msg(array, NULL, c);
  }

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
  htsmsg_t *e = htsmsg_create_map();

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

  out = htsmsg_create_list();
  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_ADMIN)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

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

  if(http_access_verify(hc, ACCESS_ADMIN)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

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
  const char *s  = http_arg_get(&hc->hc_req_args, "adapterId");
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  th_dvb_adapter_t *tda = s ? dvb_adapter_find_by_identifier(s) : NULL;
  th_dvb_mux_instance_t *tdmi;
  th_transport_t *t;

  htsmsg_t *r, *out;

  if(tda == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_ADMIN)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  if(!strcmp(op, "load")) {
    r = htsmsg_create_map();
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

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);
  } else if(!strcmp(op, "addnetwork")) {
    if((s = http_arg_get(&hc->hc_req_args, "network")) != NULL)
      dvb_mux_preconf_add_network(tda, s);

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "serviceprobe")) {

    tvhlog(LOG_NOTICE, "web interface",
	   "Service probe started on \"%s\"", tda->tda_displayname);

    RB_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
      LIST_FOREACH(t, &tdmi->tdmi_transports, tht_mux_link) {
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
static htsmsg_t *
build_transport_msg(th_transport_t *t)
{
  htsmsg_t *r = htsmsg_create_map();
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

  htsmsg_add_str(r, "status", transport_status_to_text(t->tht_feed_status));

  video[0] = 0;
  audio[0] = 0;
  subtitles[0] = 0;
  scrambling[0] = 0;

  LIST_FOREACH(st, &t->tht_components, st_link) {

    switch(st->st_type) {
    case SCT_TELETEXT:
    case SCT_SUBTITLES:
    case SCT_PAT:
    case SCT_PMT:
      break;

    case SCT_MPEG2VIDEO:
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

    case SCT_H264:
      snprintf(video + strlen(video), sizeof(video) - strlen(video),
	       "%sH.264 (PID:%d", strlen(video) > 0 ? ", " : "",
	       st->st_pid);
      goto video;

    case SCT_MPEG2AUDIO:
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

    case SCT_AC3:
      snprintf(audio + strlen(audio), sizeof(audio) - strlen(audio),
	       "%sAC3 (PID:%d", strlen(audio) > 0 ? ", " : "",
	       st->st_pid);
      goto audio;

    case SCT_CA:
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
  channel_tag_mapping_t *ctm;
  char buf[200];

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_ADMIN)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  ch = s ? channel_find_by_identifier(atoi(s)) : NULL;
  if(ch == NULL) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_BAD_REQUEST;
  }

  if(!strcmp(op, "load")) {
    r = htsmsg_create_map();
    htsmsg_add_u32(r, "id", ch->ch_id);
    htsmsg_add_str(r, "name", ch->ch_name);
    htsmsg_add_str(r, "comdetect", "tt192");

    if(ch->ch_xc != NULL)
      htsmsg_add_str(r, "xmltvchannel", ch->ch_xc->xc_displayname);

    buf[0] = 0;
    LIST_FOREACH(ctm, &ch->ch_ctms, ctm_channel_link) {
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
	       "%s%d", strlen(buf) == 0 ? "" : ",",
	       ctm->ctm_tag->ct_identifier);
    }
    htsmsg_add_str(r, "tags", buf);

    out = json_single_record(r, "channels");

  } else if(!strcmp(op, "gettransports")) {
    out = htsmsg_create_map();
    array = htsmsg_create_list();
    LIST_FOREACH(t, &ch->ch_transports, tht_ch_link)
      htsmsg_add_msg(array, NULL, build_transport_msg(t));

    htsmsg_add_msg(out, "entries", array);

  } else if(!strcmp(op, "delete")) {

    channel_delete(ch);

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "reloadchlist", 1);
    htsmsg_add_u32(out, "success", 1);
 
  } else if(!strcmp(op, "mergefrom")) {
    
    if((s = http_arg_get(&hc->hc_req_args, "srcch")) == NULL)
      return HTTP_STATUS_BAD_REQUEST;
    
    ch2 = channel_find_by_identifier(atoi(s));
    if(ch2 == NULL || ch2 == ch)
      return HTTP_STATUS_BAD_REQUEST;

    channel_merge(ch, ch2); /* ch2 goes away here */

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "reloadchlist", 1);
    htsmsg_add_u32(out, "success", 1);
 	

  } else if(!strcmp(op, "save")) {

    if((s = http_arg_get(&hc->hc_req_args, "tags")) != NULL)
      channel_set_tags_from_list(ch, s);

    s = http_arg_get(&hc->hc_req_args, "xmltvchannel");
    channel_set_xmltv_source(ch, s?xmltv_channel_find_by_displayname(s):NULL);

    if((s = http_arg_get(&hc->hc_req_args, "name")) != NULL &&
       strcmp(s, ch->ch_name)) {
      
      if(channel_rename(ch, s)) {
	out = htsmsg_create_map();
	htsmsg_add_u32(out, "success", 0);
	htsmsg_add_str(out, "errormsg", "Channel name already exist");
	goto response;
      } else {
	reloadchlist = 1;
      }
    }
    
    out = htsmsg_create_map();
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
 *
 */
static int
extjs_xmltv(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  xmltv_channel_t *xc;
  htsmsg_t *out, *array, *e, *r;
  const char *s;

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_ADMIN)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  pthread_mutex_unlock(&global_lock);

  if(!strcmp(op, "listChannels")) {

    out = htsmsg_create_map();
    array = htsmsg_create_list();
    
    e = htsmsg_create_map();
    htsmsg_add_str(e, "xcTitle", "None");
    htsmsg_add_msg(array, NULL, e);

    pthread_mutex_lock(&global_lock);
    LIST_FOREACH(xc, &xmltv_displaylist, xc_displayname_link) {
      e = htsmsg_create_map();
      htsmsg_add_str(e, "xcTitle", xc->xc_displayname);
      htsmsg_add_msg(array, NULL, e);
    }
    pthread_mutex_unlock(&global_lock);

    htsmsg_add_msg(out, "entries", array);

  } else if(!strcmp(op, "loadSettings")) {

    pthread_mutex_lock(&xmltv_mutex);
    r = htsmsg_create_map();

    if((s = xmltv_get_current_grabber()) != NULL)
      htsmsg_add_str(r, "grabber", s);

    htsmsg_add_u32(r, "grabinterval", xmltv_grab_interval);
    htsmsg_add_u32(r, "grabenable", xmltv_grab_enabled);
    pthread_mutex_unlock(&xmltv_mutex);

    out = json_single_record(r, "xmltvSettings");

  } else if(!strcmp(op, "saveSettings")) {

    pthread_mutex_lock(&xmltv_mutex);

    s = http_arg_get(&hc->hc_req_args, "grabber");
    xmltv_set_current_grabber(s);

    s = http_arg_get(&hc->hc_req_args, "grabinterval");
    xmltv_set_grab_interval(atoi(s));

    s = http_arg_get(&hc->hc_req_args, "grabenable");
    xmltv_set_grab_enable(!!s);

    pthread_mutex_unlock(&xmltv_mutex);

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "listGrabbers")) {

    out = htsmsg_create_map();

    pthread_mutex_lock(&xmltv_mutex);
    array = xmltv_list_grabbers();
    pthread_mutex_unlock(&xmltv_mutex);
    if(array != NULL)
      htsmsg_add_msg(out, "entries", array);

  } else {
    return HTTP_STATUS_BAD_REQUEST;
  }

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;

}


/**
 *
 */
static int
extjs_channeltags(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *array, *e;
  channel_tag_t *ct;

  pthread_mutex_lock(&global_lock);

  if(!strcmp(op, "listTags")) {

    out = htsmsg_create_map();
    array = htsmsg_create_list();

    TAILQ_FOREACH(ct, &channel_tags, ct_link) {
      if(!ct->ct_enabled)
	continue;

      e = htsmsg_create_map();
      htsmsg_add_u32(e, "identifier", ct->ct_identifier);
      htsmsg_add_str(e, "name", ct->ct_name);
      htsmsg_add_msg(array, NULL, e);
    }

    htsmsg_add_msg(out, "entries", array);

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
static int
extjs_epg(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *array, *m;
  epg_query_result_t eqr;
  event_t *e;
  int start = 0, end, limit, i;
  const char *s;
  const char *channel = http_arg_get(&hc->hc_req_args, "channel");
  const char *tag     = http_arg_get(&hc->hc_req_args, "tag");
  const char *cgrp    = http_arg_get(&hc->hc_req_args, "contentgrp");
  const char *title   = http_arg_get(&hc->hc_req_args, "title");

  if(channel && !channel[0]) channel = NULL;
  if(tag     && !tag[0])     tag = NULL;

  if((s = http_arg_get(&hc->hc_req_args, "start")) != NULL)
    start = atoi(s);

  if((s = http_arg_get(&hc->hc_req_args, "limit")) != NULL)
    limit = atoi(s);
  else
    limit = 20; /* XXX */

  out = htsmsg_create_map();
  array = htsmsg_create_list();

  pthread_mutex_lock(&global_lock);

  epg_query(&eqr, channel, tag, cgrp, title);

  epg_query_sort(&eqr);

  htsmsg_add_u32(out, "totalCount", eqr.eqr_entries);


  start = MIN(start, eqr.eqr_entries);
  end = MIN(start + limit, eqr.eqr_entries);

  for(i = start; i < end; i++) {
    e = eqr.eqr_array[i];

    m = htsmsg_create_map();

    if(e->e_channel != NULL) {
      htsmsg_add_str(m, "channel", e->e_channel->ch_name);
      if(e->e_channel->ch_icon != NULL)
	htsmsg_add_str(m, "chicon", e->e_channel->ch_icon);
    }

    if(e->e_title != NULL)
      htsmsg_add_str(m, "title", e->e_title);

    if(e->e_desc != NULL)
      htsmsg_add_str(m, "description", e->e_desc);

    htsmsg_add_u32(m, "id", e->e_id);
    htsmsg_add_u32(m, "start", e->e_start);
    htsmsg_add_u32(m, "end", e->e_stop);
    htsmsg_add_u32(m, "duration", e->e_stop - e->e_start);
    
    if(e->e_content_type != NULL && 
       e->e_content_type->ect_group->ecg_name != NULL)
      htsmsg_add_str(m, "contentgrp", e->e_content_type->ect_group->ecg_name);
    htsmsg_add_msg(array, NULL, m);
  }

  epg_query_free(&eqr);

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
static int
extjs_dvr(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *r;
  event_t *e;
  dvr_entry_t *de;
  const char *s;
  int flags = 0;

  if(op == NULL)
    op = "loadSettings";

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_RECORDER)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  if(!strcmp(op, "recordEvent")) {
    s = http_arg_get(&hc->hc_req_args, "eventId");

    if((e = epg_event_find_by_id(atoi(s))) == NULL) {
      pthread_mutex_unlock(&global_lock);
      return HTTP_STATUS_BAD_REQUEST;
    }

    dvr_entry_create_by_event(e, hc->hc_representative);

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);
  } else if(!strcmp(op, "cancelEntry")) {
    s = http_arg_get(&hc->hc_req_args, "entryId");

    if((de = dvr_entry_find_by_id(atoi(s))) == NULL) {
      pthread_mutex_unlock(&global_lock);
      return HTTP_STATUS_BAD_REQUEST;
    }

    dvr_entry_cancel(de);

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "createAutoRec")) {

    dvr_autorec_add(http_arg_get(&hc->hc_req_args, "title"),
		    http_arg_get(&hc->hc_req_args, "channel"),
		    http_arg_get(&hc->hc_req_args, "tag"),
		    http_arg_get(&hc->hc_req_args, "contentgrp"),
		    hc->hc_representative, "Created from EPG query");

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "loadSettings")) {

    r = htsmsg_create_map();
    htsmsg_add_str(r, "storage", dvr_storage);
    if(dvr_postproc != NULL)
      htsmsg_add_str(r, "postproc", dvr_postproc);
    htsmsg_add_u32(r, "retention", dvr_retention_days);
    htsmsg_add_u32(r, "dayDirs",        !!(dvr_flags & DVR_DIR_PER_DAY));
    htsmsg_add_u32(r, "channelDirs",    !!(dvr_flags & DVR_DIR_PER_CHANNEL));
    htsmsg_add_u32(r, "channelInTitle", !!(dvr_flags & DVR_CHANNEL_IN_TITLE));
    htsmsg_add_u32(r, "dateInTitle",    !!(dvr_flags & DVR_DATE_IN_TITLE));
    htsmsg_add_u32(r, "timeInTitle",    !!(dvr_flags & DVR_TIME_IN_TITLE));

    out = json_single_record(r, "dvrSettings");

  } else if(!strcmp(op, "saveSettings")) {

    if((s = http_arg_get(&hc->hc_req_args, "storage")) != NULL)
      dvr_storage_set(s);
    
    if((s = http_arg_get(&hc->hc_req_args, "postproc")) != NULL)
      dvr_postproc_set(s);

    if((s = http_arg_get(&hc->hc_req_args, "retention")) != NULL)
      dvr_retention_set(atoi(s));

    if(http_arg_get(&hc->hc_req_args, "dayDirs") != NULL)
      flags |= DVR_DIR_PER_DAY;
    if(http_arg_get(&hc->hc_req_args, "channelDirs") != NULL)
      flags |= DVR_DIR_PER_CHANNEL;
    if(http_arg_get(&hc->hc_req_args, "channelInTitle") != NULL)
      flags |= DVR_CHANNEL_IN_TITLE;
    if(http_arg_get(&hc->hc_req_args, "dateInTitle") != NULL)
      flags |= DVR_DATE_IN_TITLE;
    if(http_arg_get(&hc->hc_req_args, "timeInTitle") != NULL)
      flags |= DVR_TIME_IN_TITLE;

    dvr_flags_set(flags);

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
static int
extjs_dvrlist(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *array, *m;
  dvr_query_result_t dqr;
  dvr_entry_t *de;
  int start = 0, end, limit, i;
  const char *s, *t = NULL;
  off_t fsize;

  if((s = http_arg_get(&hc->hc_req_args, "start")) != NULL)
    start = atoi(s);

  if((s = http_arg_get(&hc->hc_req_args, "limit")) != NULL)
    limit = atoi(s);
  else
    limit = 20; /* XXX */

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_RECORDER)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  out = htsmsg_create_map();
  array = htsmsg_create_list();


  dvr_query(&dqr);

  dvr_query_sort(&dqr);

  htsmsg_add_u32(out, "totalCount", dqr.dqr_entries);

  start = MIN(start, dqr.dqr_entries);
  end = MIN(start + limit, dqr.dqr_entries);

  for(i = start; i < end; i++) {
    de = dqr.dqr_array[i];

    m = htsmsg_create_map();

    if(de->de_channel != NULL) {
      htsmsg_add_str(m, "channel", de->de_channel->ch_name);
      if(de->de_channel->ch_icon != NULL)
	htsmsg_add_str(m, "chicon", de->de_channel->ch_icon);
    }

    if(de->de_title != NULL)
      htsmsg_add_str(m, "title", de->de_title);

    if(de->de_desc != NULL)
      htsmsg_add_str(m, "description", de->de_desc);

    htsmsg_add_u32(m, "id", de->de_id);
    htsmsg_add_u32(m, "start", de->de_start);
    htsmsg_add_u32(m, "end", de->de_stop);
    htsmsg_add_u32(m, "duration", de->de_stop - de->de_start);
    
    htsmsg_add_str(m, "creator", de->de_creator);

    switch(de->de_sched_state) {
    case DVR_SCHEDULED:
      s = "Scheduled for recording";
      t = "sched";
      break;
    case DVR_RECORDING:
      s = "Recording";
      t = "rec";
      break;
    case DVR_COMPLETED:
      s = de->de_error ?: "Completed OK";
      t = "done";
      break;
    default:
      s = "Invalid";
      break;
    }
    htsmsg_add_str(m, "status", s);
    if(t != NULL) htsmsg_add_str(m, "schedstate", t);


    if(de->de_sched_state == DVR_COMPLETED) {
      fsize = dvr_get_filesize(de);
      if(fsize > 0) {
	char url[100];
	htsmsg_add_s64(m, "filesize", fsize);

	snprintf(url, sizeof(url), "/dvrfile/%d", de->de_id);
	htsmsg_add_str(m, "url", url);
      }
    }


    htsmsg_add_msg(array, NULL, m);
  }

  dvr_query_free(&dqr);

  pthread_mutex_unlock(&global_lock);

  htsmsg_add_msg(out, "entries", array);

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
  http_path_add("/about.html",  NULL, page_about,        ACCESS_WEB_INTERFACE);
  http_path_add("/extjs.html",  NULL, extjs_root,        ACCESS_WEB_INTERFACE);
  http_path_add("/tablemgr",    NULL, extjs_tablemgr,    ACCESS_WEB_INTERFACE);
  http_path_add("/dvbtree",     NULL, extjs_dvbtree,     ACCESS_WEB_INTERFACE);
  http_path_add("/dvbadapter",  NULL, extjs_dvbadapter,  ACCESS_WEB_INTERFACE);
  http_path_add("/dvbnetworks", NULL, extjs_dvbnetworks, ACCESS_WEB_INTERFACE);
  http_path_add("/chlist",      NULL, extjs_chlist,      ACCESS_WEB_INTERFACE);
  http_path_add("/channel",     NULL, extjs_channel,     ACCESS_WEB_INTERFACE);
  http_path_add("/xmltv",       NULL, extjs_xmltv,       ACCESS_WEB_INTERFACE);
  http_path_add("/channeltags", NULL, extjs_channeltags, ACCESS_WEB_INTERFACE);
  http_path_add("/epg",         NULL, extjs_epg,         ACCESS_WEB_INTERFACE);
  http_path_add("/dvr",         NULL, extjs_dvr,         ACCESS_WEB_INTERFACE);
  http_path_add("/dvrlist",     NULL, extjs_dvrlist,     ACCESS_WEB_INTERFACE);
  http_path_add("/ecglist",     NULL, extjs_ecglist,     ACCESS_WEB_INTERFACE);
}
