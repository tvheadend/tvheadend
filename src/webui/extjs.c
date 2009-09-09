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
#include "dtable.h"
#include "channels.h"
#include "psi.h"

#include "dvb/dvb.h"
#include "dvb/dvb_support.h"
#include "dvb/dvb_preconf.h"
#include "dvr/dvr.h"
#include "v4l.h"
#include "transports.h"
#include "serviceprobe.h"
#include "xmltv.h"
#include "epg.h"
#include "iptv_input.h"

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
		 "<link rel=\"stylesheet\" type=\"text/css\" href=\"static/livegrid/resources/css/ext-ux-livegrid.css\">\n"
		 "<link rel=\"stylesheet\" type=\"text/css\" href=\"static/app/ext.css\">\n");

  extjs_exec(hq, "Ext.BLANK_IMAGE_URL = "
	     "'"EXTJSPATH"/resources/images/default/s.gif';");

#if 0
  htsbuf_qprintf(hq, 
		 "<script type='text/javascript' "
		 "src='http://getfirebug.com/releases/lite/1.2/firebug-lite-compressed.js'></script>");
#endif


  /**
   * Load extjs extensions
   */
  extjs_load(hq, "static/app/extensions.js");
  extjs_load(hq, "static/livegrid/livegrid-all.js");
  

  /**
   * Create a namespace for our app
   */
  extjs_exec(hq, "Ext.namespace('tvheadend');");

  /**
   * Load all components
   */
  extjs_load(hq, "static/app/comet.js");
  extjs_load(hq, "static/app/tableeditor.js");
  extjs_load(hq, "static/app/cteditor.js");
  extjs_load(hq, "static/app/acleditor.js");
  extjs_load(hq, "static/app/cwceditor.js");
  extjs_load(hq, "static/app/tvadapters.js");
  extjs_load(hq, "static/app/dvb.js");
  extjs_load(hq, "static/app/iptv.js");
  extjs_load(hq, "static/app/v4l.js");
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
		 "&copy; 2006 - 2009 Andreas \303\226man, et al.<br><br>"
		 "<img src=\"docresources/tvheadendlogo.png\"><br>"
		 "<a href=\"http://hts.lonelycoder.com/\">"
		 "http://hts.lonelycoder.com/</a><br><br>"
		 "Based on software from "
		 "<a target=\"_blank\" href=\"http://www.ffmpeg.org/\">FFmpeg</a> and "
		 "<a target=\"_blank\" href=\"http://www.extjs.com/\">ExtJS</a>. "
		 "Icons from "
		 "<a target=\"_blank\" href=\"http://www.famfamfam.com/lab/icons/silk/\">"
		 "FamFamFam</a>"
		 "<br><br>"
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
static void
extjs_channels_delete(htsmsg_t *in)
{
  htsmsg_field_t *f;
  channel_t *ch;

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link)
    if(f->hmf_type == HMF_S64 &&
       (ch = channel_find_by_identifier(f->hmf_s64)) != NULL)
      channel_delete(ch);
}


/**
 *
 */
static void
extjs_channels_update(htsmsg_t *in)
{
  htsmsg_field_t *f;
  channel_t *ch;
  htsmsg_t *c;
  uint32_t id;
  const char *s;

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link) {
    if((c = htsmsg_get_map_by_field(f)) == NULL ||
       htsmsg_get_u32(c, "id", &id))
      continue;

    if((ch = channel_find_by_identifier(id)) == NULL)
      continue;

    if((s = htsmsg_get_str(c, "name")) != NULL)
      channel_rename(ch, s);

    if((s = htsmsg_get_str(c, "xmltvsrc")) != NULL)
      channel_set_xmltv_source(ch, xmltv_channel_find_by_displayname(s));

    if((s = htsmsg_get_str(c, "tags")) != NULL)
      channel_set_tags_from_list(ch, s);
  }
}

/**
 *
 */
static int
extjs_channels(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *array, *c;
  channel_t *ch;
  char buf[1024];
  channel_tag_mapping_t *ctm;
  const char *op        = http_arg_get(&hc->hc_req_args, "op");
  const char *entries   = http_arg_get(&hc->hc_req_args, "entries");

  htsmsg_autodtor(in) =
    entries != NULL ? htsmsg_json_deserialize(entries) : NULL;

  htsmsg_autodtor(out) = htsmsg_create_map();

  scopedgloballock();

  if(!strcmp(op, "list")) {
    array = htsmsg_create_list();

    RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
      c = htsmsg_create_map();
      htsmsg_add_str(c, "name", ch->ch_name);
      htsmsg_add_u32(c, "chid", ch->ch_id);
      
      if(ch->ch_xc != NULL)
	htsmsg_add_str(c, "xmltvsrc", ch->ch_xc->xc_displayname);

      buf[0] = 0;
      LIST_FOREACH(ctm, &ch->ch_ctms, ctm_channel_link) {
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		 "%s%d", strlen(buf) == 0 ? "" : ",",
		 ctm->ctm_tag->ct_identifier);
      }
      htsmsg_add_str(c, "tags", buf);

      htsmsg_add_msg(array, NULL, c);
    }
    
    htsmsg_add_msg(out, "entries", array);

  } else if(!strcmp(op, "delete") && in != NULL) {
    extjs_channels_delete(in);

  } else if(!strcmp(op, "update") && in != NULL) {
    extjs_channels_update(in);
     
  } else {
    return 400;
  }

  htsmsg_json_serialize(out, hq, 0);
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
    htsmsg_add_u32(r, "preExtraTime", dvr_extra_time_pre);
    htsmsg_add_u32(r, "postExtraTime", dvr_extra_time_post);
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

   if((s = http_arg_get(&hc->hc_req_args, "preExtraTime")) != NULL)
     dvr_extra_time_pre_set(atoi(s));

   if((s = http_arg_get(&hc->hc_req_args, "postExtraTime")) != NULL)
     dvr_extra_time_post_set(atoi(s));

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

	snprintf(url, sizeof(url), "dvrfile/%d", de->de_id);
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
  th_transport_t *t;

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
    htsmsg_add_u32(r, "logging", tda->tda_logging);
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

    s = http_arg_get(&hc->hc_req_args, "logging");
    dvb_adapter_set_logging(tda, !!s);

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
      LIST_FOREACH(t, &tdmi->tdmi_transports, tht_group_link) {
	if(t->tht_enabled)
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
static void
transport_delete(htsmsg_t *in)
{
  htsmsg_field_t *f;
  th_transport_t *t;
  const char *id;

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link) {
    if((id = htsmsg_field_get_string(f)) != NULL &&
       (t = transport_find_by_identifier(id)) != NULL)
      transport_destroy(t);
  }
}


/**
 *
 */
static void
transport_update(htsmsg_t *in)
{
  htsmsg_field_t *f;
  htsmsg_t *c;
  th_transport_t *t;
  uint32_t u32;
  const char *id;
  const char *chname;

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link) {
    if((c = htsmsg_get_map_by_field(f)) == NULL ||
       (id = htsmsg_get_str(c, "id")) == NULL)
      continue;
    
    if((t = transport_find_by_identifier(id)) == NULL)
      continue;

    if(!htsmsg_get_u32(c, "enabled", &u32))
      transport_set_enable(t, u32);

    if((chname = htsmsg_get_str(c, "channelname")) != NULL) 
      transport_map_channel(t, channel_find_by_name(chname, 1), 1);
  }
}


/**
 *
 */
static int
transportcmp(const void *A, const void *B)
{
  th_transport_t *a = *(th_transport_t **)A;
  th_transport_t *b = *(th_transport_t **)B;

  return strcasecmp(a->tht_svcname ?: "\0377", b->tht_svcname ?: "\0377");
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
  th_transport_t *t, **tvec;
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
      LIST_FOREACH(t, &tdmi->tdmi_transports, tht_group_link) {
	count++;
      }
    }

    tvec = alloca(sizeof(th_transport_t *) * count);

    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
      LIST_FOREACH(t, &tdmi->tdmi_transports, tht_group_link) {
	tvec[i++] = t;
      }
    }

    qsort(tvec, count, sizeof(th_transport_t *), transportcmp);

    for(i = 0; i < count; i++)
      htsmsg_add_msg(array, NULL, dvb_transport_build_msg(tvec[i]));

    htsmsg_add_msg(out, "entries", array);

  } else if(!strcmp(op, "update")) {
    if(in != NULL)
      transport_update(in);

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
extjs_servicedetails(http_connection_t *hc, 
		     const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *streams, *c;
  th_transport_t *t;
  th_stream_t *st;
  char buf[20];

  pthread_mutex_lock(&global_lock);

  if(remain == NULL || (t = transport_find_by_identifier(remain)) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  streams = htsmsg_create_list();

  LIST_FOREACH(st, &t->tht_components, st_link) {
    c = htsmsg_create_map();

    htsmsg_add_u32(c, "pid", st->st_pid);

    htsmsg_add_str(c, "type", streaming_component_type2txt(st->st_type));

    switch(st->st_type) {
    default:
      htsmsg_add_str(c, "details", "");
      break;

    case SCT_CA:
      htsmsg_add_str(c, "details", psi_caid2name(st->st_caid));
      break;

    case SCT_AC3:
    case SCT_AAC:
    case SCT_MPEG2AUDIO:
      htsmsg_add_str(c, "details", st->st_lang);
      break;

    case SCT_MPEG2VIDEO:
    case SCT_H264:
      buf[0] = 0;
      if(st->st_frame_duration)
	snprintf(buf, sizeof(buf), "%2.2f Hz",
		 90000.0 / st->st_frame_duration);
      htsmsg_add_str(c, "details", buf);
      break;
    }

    htsmsg_add_msg(streams, NULL, c);
  }

  out = htsmsg_create_map();
  htsmsg_add_str(out, "title", t->tht_svcname ?: "unnamed transport");

  htsmsg_add_msg(out, "streams", streams);

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
static int
extjs_mergechannel(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *target = http_arg_get(&hc->hc_req_args, "targetID");
  htsmsg_t *out;
  channel_t *src, *dst;

  if(remain == NULL || target == NULL)
    return 400;

  pthread_mutex_lock(&global_lock);

  src = channel_find_by_identifier(atoi(remain));
  dst = channel_find_by_identifier(atoi(target));

  if(src == NULL || dst == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  out = htsmsg_create_map();

  if(src != dst) {
    channel_merge(dst, src);
    htsmsg_add_u32(out, "success", 1);
  } else {

    htsmsg_add_u32(out, "success", 0);
    htsmsg_add_str(out, "msg", "Target same as source");
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
transport_update_iptv(htsmsg_t *in)
{
  htsmsg_field_t *f;
  htsmsg_t *c;
  th_transport_t *t;
  uint32_t u32;
  const char *id, *s;
  int save;

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link) {
    if((c = htsmsg_get_map_by_field(f)) == NULL ||
       (id = htsmsg_get_str(c, "id")) == NULL)
      continue;
    
    if((t = transport_find_by_identifier(id)) == NULL)
      continue;

    save = 0;

    if(!htsmsg_get_u32(c, "port", &u32)) {
      t->tht_iptv_port = u32;
      save = 1;
    }

    if((s = htsmsg_get_str(c, "group")) != NULL) {
      inet_pton(AF_INET, s, &t->tht_iptv_group.s_addr);
      save = 1;
    }
    

    save |= tvh_str_update(&t->tht_iptv_iface, htsmsg_get_str(c, "interface"));
    if(save)
      t->tht_config_save(t); // Save config
  }
}



/**
 *
 */
static htsmsg_t *
build_record_iptv(th_transport_t *t)
{
  htsmsg_t *r = htsmsg_create_map();
  char abuf[INET_ADDRSTRLEN];

  htsmsg_add_str(r, "id", t->tht_identifier);

  htsmsg_add_str(r, "channelname", t->tht_ch ? t->tht_ch->ch_name : "");
  htsmsg_add_str(r, "interface", t->tht_iptv_iface ?: "");

  inet_ntop(AF_INET, &t->tht_iptv_group, abuf, sizeof(abuf));
  htsmsg_add_str(r, "group", t->tht_iptv_group.s_addr ? abuf : "");

  htsmsg_add_u32(r, "port", t->tht_iptv_port);
  htsmsg_add_u32(r, "enabled", t->tht_enabled);
  return r;
}

/**
 *
 */
static int
iptv_transportcmp(const void *A, const void *B)
{
  th_transport_t *a = *(th_transport_t **)A;
  th_transport_t *b = *(th_transport_t **)B;

  return memcmp(&a->tht_iptv_group, &b->tht_iptv_group, 4);
}

/**
 *
 */
static int
extjs_iptvservices(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *in, *array;
  const char *op        = http_arg_get(&hc->hc_req_args, "op");
  const char *entries   = http_arg_get(&hc->hc_req_args, "entries");
  th_transport_t *t, **tvec;
  int count = 0, i = 0;

  pthread_mutex_lock(&global_lock);

  in = entries != NULL ? htsmsg_json_deserialize(entries) : NULL;

  if(!strcmp(op, "get")) {

    LIST_FOREACH(t, &iptv_all_transports, tht_group_link)
      count++;
    tvec = alloca(sizeof(th_transport_t *) * count);
    LIST_FOREACH(t, &iptv_all_transports, tht_group_link)
      tvec[i++] = t;

    out = htsmsg_create_map();
    array = htsmsg_create_list();

    qsort(tvec, count, sizeof(th_transport_t *), iptv_transportcmp);

    for(i = 0; i < count; i++)
      htsmsg_add_msg(array, NULL, build_record_iptv(tvec[i]));

    htsmsg_add_msg(out, "entries", array);

  } else if(!strcmp(op, "update")) {
    if(in != NULL) {
      transport_update(in);      // Generic transport parameters
      transport_update_iptv(in); // IPTV speicifc
    }

    out = htsmsg_create_map();

  } else if(!strcmp(op, "create")) {

    out = build_record_iptv(iptv_transport_find(NULL, 1));

  } else if(!strcmp(op, "delete")) {
    if(in != NULL)
      transport_delete(in);

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
      transport_update(in);      // Generic transport parameters
      transport_update_v4l(in);  // V4L speicifc
    }

    out = htsmsg_create_map();

  } else if(!strcmp(op, "create")) {

    out = build_record_v4l(v4l_transport_find(va, NULL, 1));

  } else if(!strcmp(op, "delete")) {
    if(in != NULL)
      transport_delete(in);

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
extjs_tvadapter(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *array;
  th_dvb_adapter_t *tda;
  v4l_adapter_t *va;

  pthread_mutex_lock(&global_lock);

  /* Just list all adapters */
  array = htsmsg_create_list();

  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link)
    htsmsg_add_msg(array, NULL, dvb_adapter_build_msg(tda));

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


/**
 * WEB user interface
 */
void
extjs_start(void)
{
  http_path_add("/about.html",  NULL, page_about,        ACCESS_WEB_INTERFACE);
  http_path_add("/extjs.html",  NULL, extjs_root,        ACCESS_WEB_INTERFACE);
  http_path_add("/tablemgr",    NULL, extjs_tablemgr,    ACCESS_WEB_INTERFACE);
  http_path_add("/dvbnetworks", NULL, extjs_dvbnetworks, ACCESS_WEB_INTERFACE);
  http_path_add("/channels",    NULL, extjs_channels,    ACCESS_WEB_INTERFACE);
  http_path_add("/xmltv",       NULL, extjs_xmltv,       ACCESS_WEB_INTERFACE);
  http_path_add("/channeltags", NULL, extjs_channeltags, ACCESS_WEB_INTERFACE);
  http_path_add("/epg",         NULL, extjs_epg,         ACCESS_WEB_INTERFACE);
  http_path_add("/dvr",         NULL, extjs_dvr,         ACCESS_WEB_INTERFACE);
  http_path_add("/dvrlist",     NULL, extjs_dvrlist,     ACCESS_WEB_INTERFACE);
  http_path_add("/ecglist",     NULL, extjs_ecglist,     ACCESS_WEB_INTERFACE);

  http_path_add("/mergechannel",
		NULL, extjs_mergechannel, ACCESS_ADMIN);

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

  http_path_add("/iptv/services", 
		NULL, extjs_iptvservices, ACCESS_ADMIN);

  http_path_add("/servicedetails", 
		NULL, extjs_servicedetails, ACCESS_ADMIN);


  http_path_add("/v4l/adapter", 
		NULL, extjs_v4ladapter, ACCESS_ADMIN);

  http_path_add("/v4l/services", 
		NULL, extjs_v4lservices, ACCESS_ADMIN);

  http_path_add("/tv/adapter", 
		NULL, extjs_tvadapter, ACCESS_ADMIN);

}
