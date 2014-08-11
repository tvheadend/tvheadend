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
#include <sys/socket.h>

#include <arpa/inet.h>

#include "htsmsg.h"
#include "htsmsg_json.h"

#include "tvheadend.h"
#include "http.h"
#include "webui.h"
#include "access.h"
#include "dtable.h"
#include "channels.h"

#include "dvr/dvr.h"
#include "epggrab.h"
#include "epg.h"
#include "muxer.h"
#include "epggrab/private.h"
#include "config.h"
#include "lang_codes.h"
#include "imagecache.h"
#include "timeshift.h"
#include "tvhtime.h"
#include "input.h"

#if ENABLE_LIBAV
#include "plumbing/transcoding.h"
#endif

/**
 *
 */
static void
extjs_load(htsbuf_queue_t *hq, const char *script)
{
  htsbuf_qprintf(hq,
                 "<script type=\"text/javascript\" "
		             "src=\"%s\"></script>\n", script);
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
  htsbuf_qprintf(hq, "<html>\n");
  htsbuf_qprintf(hq, "<head>\n");

  htsbuf_qprintf(hq, "<meta name=\"apple-itunes-app\" content=\"app-id=638900112\">\n");
  
  htsbuf_qprintf(hq, "<script type=\"text/javascript\" src=\""EXTJSPATH"/adapter/ext/ext-base%s.js\"></script>\n"
                     "<script type=\"text/javascript\" src=\""EXTJSPATH"/ext-all%s.js\"></script>\n"
                     "<link rel=\"stylesheet\" type=\"text/css\" href=\""EXTJSPATH"/resources/css/ext-all-notheme%s.css\">\n"
                     "<link rel=\"stylesheet\" type=\"text/css\" href=\""EXTJSPATH"/resources/css/xtheme-blue.css\">\n"
                     "<link rel=\"stylesheet\" type=\"text/css\" href=\"static/livegrid/resources/css/ext-ux-livegrid.css\">\n"
                     "<link rel=\"stylesheet\" type=\"text/css\" href=\"static/extjs/examples/ux/gridfilters/css/GridFilters.css\">\n"
                     "<link rel=\"stylesheet\" type=\"text/css\" href=\"static/extjs/examples/ux/gridfilters/css/RangeMenu.css\">\n"
                     "<link rel=\"stylesheet\" type=\"text/css\" href=\"static/xcheckbox/xcheckbox.css\">\n"
                     "<link rel=\"stylesheet\" type=\"text/css\" href=\"static/app/ext.css\">\n",
                     tvheadend_webui_debug ? "-debug" : "",
                     tvheadend_webui_debug ? "-debug" : "",
                     "");//tvheadend_webui_debug ? ""       : "-min");
  
  extjs_exec(hq, "Ext.BLANK_IMAGE_URL = " "'"EXTJSPATH"/resources/images/default/s.gif';");

  /**
   * Load extjs extensions
   */
  extjs_load(hq, "static/app/extensions.js");
  extjs_load(hq, "static/livegrid/livegrid-all.js");
  extjs_load(hq, "static/lovcombo/lovcombo-all.js");
  extjs_load(hq, "static/multiselect/multiselect.js");
  extjs_load(hq, "static/multiselect/ddview.js");
  extjs_load(hq, "static/xcheckbox/xcheckbox.js");
  extjs_load(hq, "static/checkcolumn/CheckColumn.js");
  extjs_load(hq, "static/extjs/examples/ux/gridfilters/GridFilters.js");
  extjs_load(hq, "static/extjs/examples/ux/gridfilters/filter/Filter.js");
  extjs_load(hq, "static/extjs/examples/ux/gridfilters/filter/BooleanFilter.js");
  extjs_load(hq, "static/extjs/examples/ux/gridfilters/filter/DateFilter.js");
  extjs_load(hq, "static/extjs/examples/ux/gridfilters/filter/ListFilter.js");
  extjs_load(hq, "static/extjs/examples/ux/gridfilters/filter/NumericFilter.js");
  extjs_load(hq, "static/extjs/examples/ux/gridfilters/filter/StringFilter.js");
  extjs_load(hq, "static/extjs/examples/ux/gridfilters/menu/ListMenu.js");
  extjs_load(hq, "static/extjs/examples/ux/gridfilters/menu/RangeMenu.js");

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
#if ENABLE_CWC
  extjs_load(hq, "static/app/cwceditor.js");
#endif
#if ENABLE_CAPMT
  extjs_load(hq, "static/app/capmteditor.js");
#endif
  extjs_load(hq, "static/app/tvadapters.js");
  extjs_load(hq, "static/app/idnode.js");
  extjs_load(hq, "static/app/esfilter.js");
#if ENABLE_MPEGTS
  extjs_load(hq, "static/app/mpegts.js");
#endif
  extjs_load(hq, "static/app/iptv.js");
#if ENABLE_V4L
  extjs_load(hq, "static/app/v4l.js");
#endif
#if ENABLE_TIMESHIFT
  extjs_load(hq, "static/app/timeshift.js");
#endif
  extjs_load(hq, "static/app/chconf.js");
  extjs_load(hq, "static/app/epg.js");
  extjs_load(hq, "static/app/dvr.js");
  extjs_load(hq, "static/app/epggrab.js");
  extjs_load(hq, "static/app/config.js");
  extjs_load(hq, "static/app/tvhlog.js");
  extjs_load(hq, "static/app/status.js");
  extjs_load(hq, "static/tv.js");
  extjs_load(hq, "static/app/servicemapper.js");

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
		 "\theight:100%%;\n"
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
		 tvheadend_version);
  http_output_html(hc);
  return 0;
}


/**
 *
 */
static int
extjs_livetv(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;

  htsbuf_qprintf(hq, "<!DOCTYPE html>\n");
  htsbuf_qprintf(hq, "<html>\n");
  htsbuf_qprintf(hq, "<head>\n");
  htsbuf_qprintf(hq, "<title>HTS Tvheadend %s</title>\n", tvheadend_version);
  htsbuf_qprintf(hq, "<link rel=\"stylesheet\" type=\"text/css\" href=\"static/tv.css\">\n");

  if(tvheadend_webui_debug) {
    extjs_load(hq, "static/extjs/adapter/ext/ext-base-debug.js");
    extjs_load(hq, "static/extjs/ext-all-debug.js");
  } else {
    extjs_load(hq, "static/extjs/adapter/ext/ext-base.js");
    extjs_load(hq, "static/extjs/ext-all.js");
  }

  extjs_load(hq, "static/tv.js");
  extjs_exec(hq, "Ext.onReady(tv.app.init, tv.app);");

  htsbuf_qprintf(hq, "</head>\n");
  htsbuf_qprintf(hq, "<body></body>\n");
  htsbuf_qprintf(hq, "</html>\n");

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
		 "&copy; 2006 - 2013 Andreas \303\226man, et al.<br><br>"
		 "<img src=\"docresources/tvheadendlogo.png\"><br>"
		 "<a href=\"https://tvheadend.org\">"
		 "https://tvheadend.org</a><br><br>"
		 "Based on software from "
		 "<a target=\"_blank\" href=\"http://www.extjs.com/\">ExtJS</a>. "
		 "Icons from "
		 "<a target=\"_blank\" href=\"http://www.famfamfam.com/lab/icons/silk/\">"
		 "FamFamFam</a>"
		 "<br><br>"
		 "Build: %s"
     "<p>"
     "If you'd like to support the project, please consider a donation."
     "<br/>"
     "All proceeds are used to support server infrastructure and buy test "
     "equipment."
     "<br/>"
     "<a href='https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=3Z87UEHP3WZK2'><img src='https://www.paypalobjects.com/en_US/GB/i/btn/btn_donateCC_LG.gif' alt='' /></a>"
		 "</center>",
		 tvheadend_version,
		 tvheadend_version);

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

  if(op == NULL)
    return 400;

  if(tablename == NULL || (dt = dtable_find(tablename)) == NULL)
    return 404;
  
  if(http_access_verify(hc, dt->dt_dtc->dtc_read_access))
    return HTTP_STATUS_UNAUTHORIZED;

  in = entries != NULL ? htsmsg_json_deserialize(entries) : NULL;

  pthread_mutex_lock(dt->dt_dtc->dtc_mutex);

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
    pthread_mutex_unlock(dt->dt_dtc->dtc_mutex);
    return HTTP_STATUS_BAD_REQUEST;

  noaccess:
    pthread_mutex_unlock(dt->dt_dtc->dtc_mutex);
    return HTTP_STATUS_BAD_REQUEST;
  }

  pthread_mutex_unlock(dt->dt_dtc->dtc_mutex);

  if(in != NULL)
    htsmsg_destroy(in);

  if(out == NULL)
    out = htsmsg_create_map();
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
  htsmsg_t *out, *array;

  out   = htsmsg_create_map();
  array = epg_genres_list_all(1, 0);
  htsmsg_add_msg(out, "entries", array);
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
extjs_epggrab(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *array, *r, *e;
  htsmsg_field_t *f;
  const char *str;
  uint32_t u32;

  if(op == NULL)
    return 400;

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_ADMIN)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  pthread_mutex_unlock(&global_lock);

  /* Basic settings (not the advanced schedule) */
  if(!strcmp(op, "loadSettings")) {

    pthread_mutex_lock(&epggrab_mutex);
    r = htsmsg_create_map();
    if (epggrab_module)
      htsmsg_add_str(r, "module", epggrab_module->id);
    htsmsg_add_str(r, "cron", epggrab_cron ? epggrab_cron : "");
    htsmsg_add_u32(r, "channel_rename", epggrab_channel_rename);
    htsmsg_add_u32(r, "channel_renumber", epggrab_channel_renumber);
    htsmsg_add_u32(r, "channel_reicon", epggrab_channel_reicon);
    htsmsg_add_u32(r, "epgdb_periodicsave", epggrab_epgdb_periodicsave / 3600);
    htsmsg_add_str(r, "ota_cron", epggrab_ota_cron ? epggrab_ota_cron : "");
    htsmsg_add_u32(r, "ota_timeout", epggrab_ota_timeout);
    htsmsg_add_u32(r, "ota_initial", epggrab_ota_initial);
    pthread_mutex_unlock(&epggrab_mutex);

    out = json_single_record(r, "epggrabSettings");

  /* List of modules and currently states */
  } else if (!strcmp(op, "moduleList")) {
    out = htsmsg_create_map();
    pthread_mutex_lock(&epggrab_mutex);
    array = epggrab_module_list();
    pthread_mutex_unlock(&epggrab_mutex);
    htsmsg_add_msg(out, "entries", array);

  /* Save settings */
  } else if (!strcmp(op, "saveSettings") ) {
    int save = 0;
    pthread_mutex_lock(&epggrab_mutex);
    str = http_arg_get(&hc->hc_req_args, "channel_rename");
    save |= epggrab_set_channel_rename(str ? 1 : 0);
    str = http_arg_get(&hc->hc_req_args, "channel_renumber");
    save |= epggrab_set_channel_renumber(str ? 1 : 0);
    str = http_arg_get(&hc->hc_req_args, "channel_reicon");
    save |= epggrab_set_channel_reicon(str ? 1 : 0);
    if ( (str = http_arg_get(&hc->hc_req_args, "epgdb_periodicsave")) )
      save |= epggrab_set_periodicsave(atoi(str) * 3600);
    if ( (str = http_arg_get(&hc->hc_req_args, "cron")) )
      save |= epggrab_set_cron(str);
    if ( (str = http_arg_get(&hc->hc_req_args, "ota_cron")) )
      save |= epggrab_ota_set_cron(str, 1);
    if ( (str = http_arg_get(&hc->hc_req_args, "ota_timeout")) )
      save |= epggrab_ota_set_timeout(atoi(str));
    str = http_arg_get(&hc->hc_req_args, "ota_initial");
    save |= epggrab_ota_set_initial(str ? 1 : 0);
    if ( (str = http_arg_get(&hc->hc_req_args, "epgdb_periodicsave")) )
      save |= epggrab_set_periodicsave(atoi(str) * 3600);
    if ( (str = http_arg_get(&hc->hc_req_args, "module")) )
      save |= epggrab_set_module_by_id(str);
    if ( (str = http_arg_get(&hc->hc_req_args, "external")) ) {
      if ( (array = htsmsg_json_deserialize(str)) ) {
        HTSMSG_FOREACH(f, array) {
          if ( (e = htsmsg_get_map_by_field(f)) ) {
            str = htsmsg_get_str(e, "id");
            u32 = 0;
            htsmsg_get_u32(e, "enabled", &u32);
            if ( str ) save |= epggrab_enable_module_by_id(str, u32);
          }
        }
        htsmsg_destroy(array);
      }
    }
    if (save) epggrab_save();
    pthread_mutex_unlock(&epggrab_mutex);
    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

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

  if(op != NULL && !strcmp(op, "listTags")) {

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
extjs_confignames(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *array, *e;
  dvr_config_t *cfg;

  pthread_mutex_lock(&global_lock);

  if(op != NULL && !strcmp(op, "list")) {

    out = htsmsg_create_map();
    array = htsmsg_create_list();

    if (http_access_verify(hc, ACCESS_RECORDER_ALL))
      goto skip;

    LIST_FOREACH(cfg, &dvrconfigs, config_link) {
      e = htsmsg_create_map();
      htsmsg_add_str(e, "identifier", cfg->dvr_config_name);
      if (strlen(cfg->dvr_config_name) == 0)
        htsmsg_add_str(e, "name", "(default)");
      else
        htsmsg_add_str(e, "name", cfg->dvr_config_name);
      htsmsg_add_msg(array, NULL, e);
    }

skip:
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
extjs_dvr_containers(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *array;

  pthread_mutex_lock(&global_lock);

  if(op != NULL && !strcmp(op, "list")) {

    out = htsmsg_create_map();
    array = htsmsg_create_list();

    if (http_access_verify(hc, ACCESS_RECORDER_ALL))
      goto skip;

    muxer_container_list(array);

skip:
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
extjs_dvr_caches(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *array;

  pthread_mutex_lock(&global_lock);

  if(op != NULL && !strcmp(op, "list")) {

    out = htsmsg_create_map();
    array = htsmsg_create_list();

    if (http_access_verify(hc, ACCESS_RECORDER_ALL))
      goto skip;

    muxer_cache_list(array);

skip:
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
extjs_languages(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *array, *e;

  pthread_mutex_lock(&global_lock);

  if(op != NULL && !strcmp(op, "list")) {

    out = htsmsg_create_map();
    array = htsmsg_create_list();

    const lang_code_t *c = lang_codes;
    while (c->code2b) {
      e = htsmsg_create_map();
      htsmsg_add_str(e, "identifier", c->code2b);
      htsmsg_add_str(e, "name", c->desc);
      htsmsg_add_msg(array, NULL, e);
      c++;
    }
  }
  else if(op != NULL && !strcmp(op, "config")) {

    out = htsmsg_create_map();
    array = htsmsg_create_list();

    const lang_code_t **c = lang_code_split2(NULL);
    if(c) {
      int i = 0;
      while (c[i]) {
        e = htsmsg_create_map();
        htsmsg_add_str(e, "identifier", c[i]->code2b);
        htsmsg_add_str(e, "name", c[i]->desc);
        htsmsg_add_msg(array, NULL, e);
        i++;
      }
      free(c);
    }
  }
  else {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_BAD_REQUEST;
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
static int
extjs_epg(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *array, *m;
  epg_query_result_t eqr;
  epg_broadcast_t *e;
  epg_episode_t *ee = NULL;
  epg_genre_t *eg = NULL, genre;
  channel_t *ch;
  int start = 0, end, limit, i;
  const char *s;
  char buf[100];
  const char *channel = http_arg_get(&hc->hc_req_args, "channel");
  const char *tag     = http_arg_get(&hc->hc_req_args, "tag");
  const char *title   = http_arg_get(&hc->hc_req_args, "title");
  const char *lang    = http_arg_get(&hc->hc_args, "Accept-Language");

  int min_duration;
  int max_duration;

  if(channel && !channel[0]) channel = NULL;
  if(tag     && !tag[0])     tag = NULL;

  if((s = http_arg_get(&hc->hc_req_args, "minduration")) != NULL)
    min_duration = atoi(s);
  else
    min_duration = 0;

  if((s = http_arg_get(&hc->hc_req_args, "maxduration")) != NULL)
    max_duration = atoi(s);
  else
    max_duration = INT_MAX;

  if((s = http_arg_get(&hc->hc_req_args, "start")) != NULL)
    start = atoi(s);

  if((s = http_arg_get(&hc->hc_req_args, "limit")) != NULL)
    limit = atoi(s);
  else
    limit = 20; /* XXX */

  if ((s = http_arg_get(&hc->hc_req_args, "contenttype"))) {
    genre.code = atoi(s);
    eg = &genre;
  }

  out = htsmsg_create_map();
  array = htsmsg_create_list();

  pthread_mutex_lock(&global_lock);

  epg_query(&eqr, channel, tag, eg, title, lang, min_duration, max_duration);

  epg_query_sort(&eqr);

  htsmsg_add_u32(out, "totalCount", eqr.eqr_entries);


  start = MIN(start, eqr.eqr_entries);
  end = MIN(start + limit, eqr.eqr_entries);

  for(i = start; i < end; i++) {
    e  = eqr.eqr_array[i];
    ee = e->episode;
    ch = e->channel;
    if (!ch||!ee) continue;

    m = htsmsg_create_map();

    htsmsg_add_str(m, "channel", channel_get_name(ch));
    htsmsg_add_u32(m, "channelid", channel_get_id(ch));
    if(ch->ch_icon != NULL)
      htsmsg_add_imageurl(m, "chicon", "imagecache/%d", ch->ch_icon);

    if((s = epg_episode_get_title(ee, lang)))
      htsmsg_add_str(m, "title", s);
    if((s = epg_episode_get_subtitle(ee, lang)))
      htsmsg_add_str(m, "subtitle", s);

    if((s = epg_broadcast_get_description(e, lang)))
      htsmsg_add_str(m, "description", s);
    else if((s = epg_broadcast_get_summary(e, lang)))
      htsmsg_add_str(m, "description", s);

    if (epg_episode_number_format(ee, buf, 100, NULL, "Season %d", ".",
                                  "Episode %d", "/%d"))
      htsmsg_add_str(m, "episode", buf);

    htsmsg_add_u32(m, "id", e->id);
    htsmsg_add_u32(m, "start", e->start);
    htsmsg_add_u32(m, "end", e->stop);
    htsmsg_add_u32(m, "duration", e->stop - e->start);
    if(ee->star_rating)
    	htsmsg_add_u32(m, "starrating", ee->star_rating);
    if(ee->age_rating)
    	htsmsg_add_u32(m, "agerating", ee->age_rating);

    if(e->serieslink)
      htsmsg_add_str(m, "serieslink", e->serieslink->uri);
    
    if((eg = LIST_FIRST(&ee->genre))) {
      htsmsg_add_u32(m, "contenttype", eg->code);
    }

    dvr_entry_t *de;
    if((de = dvr_entry_find_by_event(e)) != NULL)
      htsmsg_add_str(m, "schedstate", dvr_entry_schedstatus(de));

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

static int
extjs_epgrelated(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *array, *m;
  epg_broadcast_t *e, *ebc;
  epg_episode_t *ee, *ee2;
  channel_t *ch;
  uint32_t count = 0;
  const char *s;
  char buf[100];

  const char *lang  = http_arg_get(&hc->hc_args, "Accept-Language");
  const char *id    = http_arg_get(&hc->hc_req_args, "id");
  const char *type  = http_arg_get(&hc->hc_req_args, "type");

  out = htsmsg_create_map();
  array = htsmsg_create_list();

  pthread_mutex_lock(&global_lock);
  if ( id && type ) {
    e = epg_broadcast_find_by_id(atoi(id), NULL);
    if ( e && e->episode ) {
      ee = e->episode;

      /* Alternative broadcasts */
      if (!strcmp(type, "alternative")) {
        LIST_FOREACH(ebc, &ee->broadcasts, ep_link) {
          ch = ebc->channel;
          if ( !ch ) continue; // skip something not viewable
          if ( ebc == e ) continue; // skip self
          count++;
          m = htsmsg_create_map();
          htsmsg_add_u32(m, "id", ebc->id);
          htsmsg_add_str(m, "channel", channel_get_name(ch));
          if (ch->ch_icon)
            htsmsg_add_imageurl(m, "chicon", "imagecache/%d", ch->ch_icon);
          htsmsg_add_u32(m, "start", ebc->start);
          htsmsg_add_msg(array, NULL, m);
        }
      
      /* Related */
      } else if (!strcmp(type, "related")) {
        if (ee->brand) {
          LIST_FOREACH(ee2, &ee->brand->episodes, blink) {
            if (ee2 == ee) continue;
            if (!ee2->title) continue;
            count++;
            m = htsmsg_create_map();
            htsmsg_add_str(m, "uri", ee2->uri);
            if ((s = epg_episode_get_title(ee2, lang)))
              htsmsg_add_str(m, "title", s);
            if ((s = epg_episode_get_subtitle(ee2, lang)))
              htsmsg_add_str(m, "subtitle", s);
            if (epg_episode_number_format(ee2, buf, 100, NULL, "Season %d",
                                          ".", "Episode %d", "/%d"))
              htsmsg_add_str(m, "episode", buf);
            htsmsg_add_msg(array, NULL, m);
          }
        } else if (ee->season) {
          LIST_FOREACH(ee2, &ee->season->episodes, slink) {
            if (ee2 == ee) continue;
            if (!ee2->title) continue;
            count++;
            m = htsmsg_create_map();
            htsmsg_add_str(m, "uri", ee2->uri);
            if ((s = epg_episode_get_title(ee2, lang)))
              htsmsg_add_str(m, "title", s);
            if ((s = epg_episode_get_subtitle(ee2, lang)))
              htsmsg_add_str(m, "subtitle", s);
            if (epg_episode_number_format(ee2, buf, 100, NULL, "Season %d",
                                          ".", "Episode %d", "/%d"))
              htsmsg_add_str(m, "episode", buf);
            htsmsg_add_msg(array, NULL, m);
          }
        }
      }
    }
  }
  pthread_mutex_unlock(&global_lock);

  htsmsg_add_u32(out, "totalCount", count);
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
extjs_epgobject(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *array;

  if(op == NULL)
    return 400;

  if (!strcmp(op, "brandList")) {
    out   = htsmsg_create_map();
    pthread_mutex_lock(&global_lock);
    array = epg_brand_list();
    pthread_mutex_unlock(&global_lock);
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
extjs_dvr(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *r;
  dvr_entry_t *de;
  const char *s;
  int flags = 0;
  dvr_config_t *cfg;
  epg_broadcast_t *e;
  char buffer[5]; // Permissions buffer: leading zero, three octal digits plus terminating null

  if(op == NULL)
    op = "loadSettings";

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_RECORDER)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  if(!strcmp(op, "recordEvent") || !strcmp(op, "recordSeries")) {

    const char *config_name = http_arg_get(&hc->hc_req_args, "config_name");

    s = http_arg_get(&hc->hc_req_args, "eventId");
    if((e = epg_broadcast_find_by_id(atoi(s), NULL)) == NULL) {
      pthread_mutex_unlock(&global_lock);
      return HTTP_STATUS_BAD_REQUEST;
    }

    if (http_access_verify(hc, ACCESS_RECORDER_ALL)) {
      config_name = NULL;
      LIST_FOREACH(cfg, &dvrconfigs, config_link) {
        if (cfg->dvr_config_name && hc->hc_username &&
            strcmp(cfg->dvr_config_name, hc->hc_username) == 0) {
          config_name = cfg->dvr_config_name;
          break;
        }
      }
      if (config_name == NULL && hc->hc_username)
        tvhlog(LOG_INFO,"dvr","User '%s' has no dvr config with identical name, using default...", hc->hc_username);
    }

    if (!strcmp(op, "recordEvent"))
      dvr_entry_create_by_event(config_name,
                                e, 0, 0, 
                                hc->hc_representative, NULL, DVR_PRIO_NORMAL);
    else
      dvr_autorec_add_series_link(config_name, e, hc->hc_representative, "Created from EPG query");

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

  } else if(!strcmp(op, "deleteEntry")) {
    s = http_arg_get(&hc->hc_req_args, "entryId");

    if((de = dvr_entry_find_by_id(atoi(s))) == NULL) {
      pthread_mutex_unlock(&global_lock);
      return HTTP_STATUS_BAD_REQUEST;
    }

    dvr_entry_delete(de);

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "createEntry")) {

    const char *config_name = http_arg_get(&hc->hc_req_args, "config_name");
    const char *title    = http_arg_get(&hc->hc_req_args, "title");
    const char *datestr  = http_arg_get(&hc->hc_req_args, "date");
    const char *startstr = http_arg_get(&hc->hc_req_args, "starttime");
    const char *stopstr  = http_arg_get(&hc->hc_req_args, "stoptime");
    const char *channel  = http_arg_get(&hc->hc_req_args, "channelid");
    const char *pri      = http_arg_get(&hc->hc_req_args, "pri");

    channel_t *ch = channel ? channel_find(channel) : NULL;

    if(ch == NULL || title == NULL || 
       datestr  == NULL || strlen(datestr)  != 10 ||
       startstr == NULL || strlen(startstr) != 5  ||
       stopstr  == NULL || strlen(stopstr)  != 5) {
      pthread_mutex_unlock(&global_lock);
      return HTTP_STATUS_BAD_REQUEST;
    }

    struct tm t = {0};
    t.tm_year = atoi(datestr + 6) - 1900;
    t.tm_mon = atoi(datestr) - 1;
    t.tm_mday = atoi(datestr + 3);
    t.tm_isdst = -1;

    t.tm_hour = atoi(startstr);
    t.tm_min = atoi(startstr + 3);
    
    time_t start = mktime(&t);

    t.tm_hour = atoi(stopstr);
    t.tm_min = atoi(stopstr + 3);
    
    time_t stop = mktime(&t);

    if(stop < start)
      stop += 86400;

    if (http_access_verify(hc, ACCESS_RECORDER_ALL)) {
      config_name = NULL;
      LIST_FOREACH(cfg, &dvrconfigs, config_link) {
        if (cfg->dvr_config_name && hc->hc_username &&
            strcmp(cfg->dvr_config_name, hc->hc_username) == 0) {
          config_name = cfg->dvr_config_name;
          break;
        }
      }
      if (config_name == NULL && hc->hc_username)
        tvhlog(LOG_INFO,"dvr","User '%s' has no dvr config with identical name, using default...", hc->hc_username);
    }

    dvr_entry_create(config_name,
                     ch, start, stop, 0, 0, title, NULL, NULL,
                     0, hc->hc_representative, 
		                 NULL, dvr_pri2val(pri));

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "createAutoRec")) {
	int min_duration;
	int max_duration;
    epg_genre_t genre, *eg = NULL;

    if ((s = http_arg_get(&hc->hc_req_args, "contenttype"))) {
      genre.code = atoi(s);
      eg = &genre;
    }

    if((s = http_arg_get(&hc->hc_req_args, "minduration")) != NULL)
      min_duration = atoi(s);
    else
      min_duration = 0;

    if((s = http_arg_get(&hc->hc_req_args, "maxduration")) != NULL)
      max_duration = atoi(s);
    else
      max_duration = INT_MAX;

    dvr_autorec_add(http_arg_get(&hc->hc_req_args, "config_name"),
                    http_arg_get(&hc->hc_req_args, "title"),
                    http_arg_get(&hc->hc_req_args, "channel"),
                    http_arg_get(&hc->hc_req_args, "tag"),
                    eg, min_duration,max_duration,
                    hc->hc_representative, "Created from EPG query");

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "loadSettings")) {

    s = http_arg_get(&hc->hc_req_args, "config_name");
    if (s == NULL)
      s = "";
    cfg = dvr_config_find_by_name_default(s);

    r = htsmsg_create_map();
    htsmsg_add_str(r, "storage", cfg->dvr_storage);
    htsmsg_add_str(r, "charset", cfg->dvr_charset ? cfg->dvr_charset : "UTF-8");
    htsmsg_add_str(r, "container", muxer_container_type2txt(cfg->dvr_mc));

/* Convert integer permissions to an octal-format 0xxx string and store it in the config file */

    snprintf(buffer,sizeof(buffer),"%04o",cfg->dvr_muxcnf.m_file_permissions);
    htsmsg_add_str(r, "filePermissions", buffer);
    snprintf(buffer,sizeof(buffer),"%04o",cfg->dvr_muxcnf.m_directory_permissions);
    htsmsg_add_str(r, "dirPermissions", buffer);

    htsmsg_add_u32(r, "cache",     cfg->dvr_muxcnf.m_cache);
    htsmsg_add_u32(r, "rewritePAT",
                   !!(cfg->dvr_muxcnf.m_flags & MC_REWRITE_PAT));
    htsmsg_add_u32(r, "rewritePMT",
                   !!(cfg->dvr_muxcnf.m_flags & MC_REWRITE_PMT));
    if(cfg->dvr_postproc != NULL)
      htsmsg_add_str(r, "postproc", cfg->dvr_postproc);
    htsmsg_add_u32(r, "retention", cfg->dvr_retention_days);
    htsmsg_add_u32(r, "preExtraTime", cfg->dvr_extra_time_pre);
    htsmsg_add_u32(r, "postExtraTime", cfg->dvr_extra_time_post);
    htsmsg_add_u32(r, "dayDirs",        !!(cfg->dvr_flags & DVR_DIR_PER_DAY));
    htsmsg_add_u32(r, "channelDirs",    !!(cfg->dvr_flags & DVR_DIR_PER_CHANNEL));
    htsmsg_add_u32(r, "channelInTitle", !!(cfg->dvr_flags & DVR_CHANNEL_IN_TITLE));
    htsmsg_add_u32(r, "dateInTitle",    !!(cfg->dvr_flags & DVR_DATE_IN_TITLE));
    htsmsg_add_u32(r, "timeInTitle",    !!(cfg->dvr_flags & DVR_TIME_IN_TITLE));
    htsmsg_add_u32(r, "whitespaceInTitle", !!(cfg->dvr_flags & DVR_WHITESPACE_IN_TITLE));
    htsmsg_add_u32(r, "titleDirs", !!(cfg->dvr_flags & DVR_DIR_PER_TITLE));
    htsmsg_add_u32(r, "episodeInTitle", !!(cfg->dvr_flags & DVR_EPISODE_IN_TITLE));
    htsmsg_add_u32(r, "cleanTitle", !!(cfg->dvr_flags & DVR_CLEAN_TITLE));
    htsmsg_add_u32(r, "tagFiles", !!(cfg->dvr_flags & DVR_TAG_FILES));
    htsmsg_add_u32(r, "commSkip", !!(cfg->dvr_flags & DVR_SKIP_COMMERCIALS));
    htsmsg_add_u32(r, "subtitleInTitle", !!(cfg->dvr_flags & DVR_SUBTITLE_IN_TITLE));
    htsmsg_add_u32(r, "episodeBeforeDate", !!(cfg->dvr_flags & DVR_EPISODE_BEFORE_DATE));
    htsmsg_add_u32(r, "episodeDuplicateDetection", !!(cfg->dvr_flags & DVR_EPISODE_DUPLICATE_DETECTION));

    out = json_single_record(r, "dvrSettings");

  } else if(!strcmp(op, "saveSettings")) {

    s = http_arg_get(&hc->hc_req_args, "config_name");
    cfg = dvr_config_find_by_name(s);
    if (cfg == NULL)
      cfg = dvr_config_create(s);

    tvhlog(LOG_INFO,"dvr","Saving configuration '%s'", cfg->dvr_config_name);

    if((s = http_arg_get(&hc->hc_req_args, "storage")) != NULL)
      dvr_storage_set(cfg,s);
    
    if((s = http_arg_get(&hc->hc_req_args, "charset")) != NULL)
      dvr_charset_set(cfg,s);

    if((s = http_arg_get(&hc->hc_req_args, "container")) != NULL)
      dvr_container_set(cfg,s);

/*
 * Convert 0xxx format permission strings to integer for internal use
 * Note no checking that strtol won't overflow int - this should never happen with three-digit numbers
 */

    if((s = http_arg_get(&hc->hc_req_args, "filePermissions")) != NULL)
      dvr_file_permissions_set(cfg,(int)strtol(s,NULL,0));

    if((s = http_arg_get(&hc->hc_req_args, "dirPermissions")) != NULL)
      dvr_directory_permissions_set(cfg,(int)strtol(s,NULL,0));
   
    if((s = http_arg_get(&hc->hc_req_args, "cache")) != NULL)
      dvr_mux_cache_set(cfg,atoi(s));

    if((s = http_arg_get(&hc->hc_req_args, "postproc")) != NULL)
      dvr_postproc_set(cfg,s);

    if((s = http_arg_get(&hc->hc_req_args, "retention")) != NULL)
      dvr_retention_set(cfg,atoi(s));

    if((s = http_arg_get(&hc->hc_req_args, "preExtraTime")) != NULL)
      dvr_extra_time_pre_set(cfg,atoi(s));

    if((s = http_arg_get(&hc->hc_req_args, "postExtraTime")) != NULL)
      dvr_extra_time_post_set(cfg,atoi(s));

    if(http_arg_get(&hc->hc_req_args, "dayDirs") != NULL)
      flags |= DVR_DIR_PER_DAY;
    if(http_arg_get(&hc->hc_req_args, "channelDirs") != NULL)
      flags |= DVR_DIR_PER_CHANNEL;
    if(http_arg_get(&hc->hc_req_args, "channelInTitle") != NULL)
      flags |= DVR_CHANNEL_IN_TITLE;
    if(http_arg_get(&hc->hc_req_args, "cleanTitle") != NULL)
      flags |= DVR_CLEAN_TITLE;
    if(http_arg_get(&hc->hc_req_args, "dateInTitle") != NULL)
      flags |= DVR_DATE_IN_TITLE;
    if(http_arg_get(&hc->hc_req_args, "timeInTitle") != NULL)
      flags |= DVR_TIME_IN_TITLE;
    if(http_arg_get(&hc->hc_req_args, "whitespaceInTitle") != NULL)
      flags |= DVR_WHITESPACE_IN_TITLE;
    if(http_arg_get(&hc->hc_req_args, "titleDirs") != NULL)
      flags |= DVR_DIR_PER_TITLE;
    if(http_arg_get(&hc->hc_req_args, "episodeInTitle") != NULL)
      flags |= DVR_EPISODE_IN_TITLE;
    if(http_arg_get(&hc->hc_req_args, "tagFiles") != NULL)
      flags |= DVR_TAG_FILES;
    if(http_arg_get(&hc->hc_req_args, "commSkip") != NULL)
      flags |= DVR_SKIP_COMMERCIALS;
    if(http_arg_get(&hc->hc_req_args, "subtitleInTitle") != NULL)
      flags |= DVR_SUBTITLE_IN_TITLE;
    if(http_arg_get(&hc->hc_req_args, "episodeBeforeDate") != NULL)
      flags |= DVR_EPISODE_BEFORE_DATE;
    if(http_arg_get(&hc->hc_req_args, "episodeDuplicateDetection") != NULL)
      flags |= DVR_EPISODE_DUPLICATE_DETECTION;


    dvr_flags_set(cfg,flags);

    /* Muxer flags */
    flags = 0;
    if(http_arg_get(&hc->hc_req_args, "rewritePAT") != NULL)
      flags |= MC_REWRITE_PAT;
    if(http_arg_get(&hc->hc_req_args, "rewritePMT") != NULL)
      flags |= MC_REWRITE_PMT;

    dvr_mux_flags_set(cfg, flags);

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "deleteSettings")) {

    s = http_arg_get(&hc->hc_req_args, "config_name");
    dvr_config_delete(s);

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
extjs_dvrlist(http_connection_t *hc, const char *remain, void *opaque,
              dvr_entry_filter filter, dvr_entry_comparator cmp)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *array, *m;
  dvr_query_result_t dqr;
  dvr_entry_t *de;
  int start = 0, end, limit, i;
  const char *s;
  int64_t fsize = 0;
  char buf[100];

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


  dvr_query_filter(&dqr, filter);

  dvr_query_sort_cmp(&dqr, cmp);

  htsmsg_add_u32(out, "totalCount", dqr.dqr_entries);

  start = MIN(start, dqr.dqr_entries);
  end = MIN(start + limit, dqr.dqr_entries);

  for(i = start; i < end; i++) {
    de = dqr.dqr_array[i];

    m = htsmsg_create_map();

    htsmsg_add_str(m, "channel", DVR_CH_NAME(de));
    if(de->de_channel != NULL) {
      htsmsg_add_str(m, "channelid", channel_get_uuid(de->de_channel));
      if (de->de_channel->ch_icon)
        htsmsg_add_imageurl(m, "chicon", "imagecache/%d",
                            de->de_channel->ch_icon);
    }

    htsmsg_add_str(m, "config_name", de->de_config_name);

    if(de->de_title != NULL)
      htsmsg_add_str(m, "title", lang_str_get(de->de_title, NULL));

    if(de->de_desc != NULL)
      htsmsg_add_str(m, "description", lang_str_get(de->de_desc, NULL));

    if (de->de_bcast && de->de_bcast->episode)
      if (epg_episode_number_format(de->de_bcast->episode, buf, 100, NULL, "Season %d", ".", "Episode %d", "/%d"))
        htsmsg_add_str(m, "episode", buf);

    htsmsg_add_u32(m, "id", de->de_id);
    htsmsg_add_u32(m, "start", de->de_start);
    htsmsg_add_u32(m, "end", de->de_stop);
    htsmsg_add_u32(m, "duration", de->de_stop - de->de_start);
    
    htsmsg_add_str(m, "creator", de->de_creator);

    htsmsg_add_str(m, "pri", dvr_val2pri(de->de_pri));

    htsmsg_add_str(m, "status", dvr_entry_status(de));
    htsmsg_add_str(m, "schedstate", dvr_entry_schedstatus(de));


    if(de->de_sched_state == DVR_COMPLETED) {
      fsize = dvr_get_filesize(de);
      if (fsize > 0) {
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

static int is_dvr_entry_finished(dvr_entry_t *entry)
{
  dvr_entry_sched_state_t state = entry->de_sched_state;
  return state == DVR_COMPLETED && !entry->de_last_error && dvr_get_filesize(entry) != -1;
}

static int is_dvr_entry_upcoming(dvr_entry_t *entry)
{
  dvr_entry_sched_state_t state = entry->de_sched_state;
  return state == DVR_RECORDING || state == DVR_SCHEDULED;
}


static int is_dvr_entry_failed(dvr_entry_t *entry)
{
  if (is_dvr_entry_finished(entry))
    return 0;
  if (is_dvr_entry_upcoming(entry))
    return 0;
  return 1;
}

static int
extjs_dvrlist_finished(http_connection_t *hc, const char *remain, void *opaque)
{
  return extjs_dvrlist(hc, remain, opaque, is_dvr_entry_finished, dvr_sort_start_descending);
}

static int
extjs_dvrlist_upcoming(http_connection_t *hc, const char *remain, void *opaque)
{
  return extjs_dvrlist(hc, remain, opaque, is_dvr_entry_upcoming, dvr_sort_start_ascending);
}

static int
extjs_dvrlist_failed(http_connection_t *hc, const char *remain, void *opaque)
{
  return extjs_dvrlist(hc, remain, opaque, is_dvr_entry_failed, dvr_sort_start_descending);
}

/**
 *
 */
void
extjs_service_delete(htsmsg_t *in)
{
  htsmsg_field_t *f;
  service_t *t;
  const char *id;

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link) {
    if((id = htsmsg_field_get_string(f)) != NULL &&
       (t = service_find_by_identifier(id)) != NULL)
      service_destroy(t, 1);
  }
}

/**
 *
 */
static int
extjs_config(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *m;
  const char *str;

  if(op == NULL)
    return 400;

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_ADMIN)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  pthread_mutex_unlock(&global_lock);

  /* Basic settings */
  if(!strcmp(op, "loadSettings")) {

    /* Misc */
    pthread_mutex_lock(&global_lock);
    m = config_get_all();

    /* Time */
    htsmsg_add_u32(m, "tvhtime_update_enabled", tvhtime_update_enabled);
    htsmsg_add_u32(m, "tvhtime_ntp_enabled", tvhtime_ntp_enabled);
    htsmsg_add_u32(m, "tvhtime_tolerance", tvhtime_tolerance);

    /* Transcoding */
#if ENABLE_LIBAV
    htsmsg_add_u32(m, "transcoding_enabled", transcoding_enabled);
#endif

    pthread_mutex_unlock(&global_lock);

    if (!m) return HTTP_STATUS_BAD_REQUEST;
    out = json_single_record(m, "config");

  /* Save settings */
  } else if (!strcmp(op, "saveSettings") ) {
    int save = 0;

    /* Misc settings */
    pthread_mutex_lock(&global_lock);
    if ((str = http_arg_get(&hc->hc_req_args, "muxconfpath")))
      save |= config_set_muxconfpath(str);
    if ((str = http_arg_get(&hc->hc_req_args, "language")))
      save |= config_set_language(str);
    if (save)
      config_save();

    /* Time */
    str = http_arg_get(&hc->hc_req_args, "tvhtime_update_enabled");
    tvhtime_set_update_enabled(!!str);
    str = http_arg_get(&hc->hc_req_args, "tvhtime_ntp_enabled");
    tvhtime_set_ntp_enabled(!!str);
    if ((str = http_arg_get(&hc->hc_req_args, "tvhtime_tolerance")))
      tvhtime_set_tolerance(atoi(str));

    /* Transcoding */
#if ENABLE_LIBAV
    str = http_arg_get(&hc->hc_req_args, "transcoding_enabled");
    save = transcoding_set_enabled(!!str);
    if (save)
      transcoding_save();
#endif

    pthread_mutex_unlock(&global_lock);
  
    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

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
extjs_tvhlog(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *m;

  if(op == NULL)
    return 400;

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_ADMIN)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  pthread_mutex_unlock(&global_lock);

  /* Basic settings */
  if(!strcmp(op, "loadSettings")) {
    char str[2048];

    /* Get config */
    pthread_mutex_lock(&tvhlog_mutex);
    m = htsmsg_create_map();
    htsmsg_add_u32(m, "tvhlog_level",      tvhlog_level);
    htsmsg_add_u32(m, "tvhlog_trace_on",   tvhlog_level > LOG_DEBUG);
    tvhlog_get_trace(str, sizeof(str));
    htsmsg_add_str(m, "tvhlog_trace",      str);
    tvhlog_get_debug(str, sizeof(str));
    htsmsg_add_str(m, "tvhlog_debug",      str);
    htsmsg_add_str(m, "tvhlog_path",       tvhlog_path ?: "");
    htsmsg_add_u32(m, "tvhlog_options",    tvhlog_options);
    htsmsg_add_u32(m, "tvhlog_dbg_syslog",
                   tvhlog_options & TVHLOG_OPT_DBG_SYSLOG);
    pthread_mutex_unlock(&tvhlog_mutex);
    
    if (!m) return HTTP_STATUS_BAD_REQUEST;
    out = json_single_record(m, "config");

  /* Save settings */
  } else if (!strcmp(op, "saveSettings") ) {
    const char *str;

    pthread_mutex_lock(&tvhlog_mutex);
    if ((str = http_arg_get(&hc->hc_req_args, "tvhlog_trace_on")))
      tvhlog_level = LOG_TRACE;
    else
      tvhlog_level = LOG_DEBUG;
    if ((str = http_arg_get(&hc->hc_req_args, "tvhlog_path"))) {
      free(tvhlog_path);
      if (*str)
        tvhlog_path  = strdup(str);
      else
        tvhlog_path  = NULL;
    }
    if ((str = http_arg_get(&hc->hc_req_args, "tvhlog_options")))
      tvhlog_options = atoi(str);
    if ((str = http_arg_get(&hc->hc_req_args, "tvhlog_dbg_syslog")))
      tvhlog_options |= TVHLOG_OPT_DBG_SYSLOG;
    else
      tvhlog_options &= ~TVHLOG_OPT_DBG_SYSLOG;
    if (tvhlog_path && tvhlog_path[0] != '\0')
      tvhlog_options |= TVHLOG_OPT_DBG_FILE;
    tvhlog_set_trace(http_arg_get(&hc->hc_req_args, "tvhlog_trace"));
    tvhlog_set_debug(http_arg_get(&hc->hc_req_args, "tvhlog_debug"));
    pthread_mutex_unlock(&tvhlog_mutex);
  
    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else {
    return HTTP_STATUS_BAD_REQUEST;
  }

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");

  return 0;
}

/**
 * Capability check
 */
static int
extjs_capabilities(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *l = tvheadend_capabilities_list(0);
  htsmsg_json_serialize(l, hq, 0);
  htsmsg_destroy(l);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}

/**
 *
 */
#if ENABLE_TIMESHIFT
static int
extjs_timeshift(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *m;
  const char *str;

  if(op == NULL)
    return 400;

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_ADMIN)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  pthread_mutex_unlock(&global_lock);

  /* Basic settings (not the advanced schedule) */
  if(!strcmp(op, "loadSettings")) {
    pthread_mutex_lock(&global_lock);
    m = htsmsg_create_map();
    htsmsg_add_u32(m, "timeshift_enabled",  timeshift_enabled);
    htsmsg_add_u32(m, "timeshift_ondemand", timeshift_ondemand);
    if (timeshift_path)
      htsmsg_add_str(m, "timeshift_path", timeshift_path);
    htsmsg_add_u32(m, "timeshift_unlimited_period", timeshift_unlimited_period);
    htsmsg_add_u32(m, "timeshift_max_period", timeshift_max_period / 60);
    htsmsg_add_u32(m, "timeshift_unlimited_size", timeshift_unlimited_size);
    htsmsg_add_u32(m, "timeshift_max_size", timeshift_max_size / 1048576);
    pthread_mutex_unlock(&global_lock);
    out = json_single_record(m, "config");

  /* Save settings */
  } else if (!strcmp(op, "saveSettings") ) {
    pthread_mutex_lock(&global_lock);
    timeshift_enabled  = http_arg_get(&hc->hc_req_args, "timeshift_enabled")  ? 1 : 0;
    timeshift_ondemand = http_arg_get(&hc->hc_req_args, "timeshift_ondemand") ? 1 : 0;
    if ((str = http_arg_get(&hc->hc_req_args, "timeshift_path"))) {
      if (timeshift_path)
        free(timeshift_path);
      timeshift_path = strdup(str);
    }
    timeshift_unlimited_period = http_arg_get(&hc->hc_req_args, "timeshift_unlimited_period") ? 1 : 0;
    if ((str = http_arg_get(&hc->hc_req_args, "timeshift_max_period")))
      timeshift_max_period = (uint32_t)atol(str) * 60;
    timeshift_unlimited_size = http_arg_get(&hc->hc_req_args, "timeshift_unlimited_size") ? 1 : 0;
    if ((str = http_arg_get(&hc->hc_req_args, "timeshift_max_size")))
      timeshift_max_size   = atol(str) * 1048576LL;
    timeshift_save();
    pthread_mutex_unlock(&global_lock);

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else {
    return HTTP_STATUS_BAD_REQUEST;
  }

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");

  return 0;
}
#endif

/**
 * WEB user interface
 */
void
extjs_start(void)
{
  http_path_add("/about.html",       NULL, page_about,             ACCESS_WEB_INTERFACE);
  http_path_add("/extjs.html",       NULL, extjs_root,             ACCESS_WEB_INTERFACE);
  http_path_add("/tv.html",          NULL, extjs_livetv,           ACCESS_WEB_INTERFACE);
  http_path_add("/capabilities",     NULL, extjs_capabilities,     ACCESS_WEB_INTERFACE);
  http_path_add("/tablemgr",         NULL, extjs_tablemgr,         ACCESS_WEB_INTERFACE);
  http_path_add("/epggrab",          NULL, extjs_epggrab,          ACCESS_WEB_INTERFACE);
  http_path_add("/channeltags",      NULL, extjs_channeltags,      ACCESS_WEB_INTERFACE);
  http_path_add("/confignames",      NULL, extjs_confignames,      ACCESS_WEB_INTERFACE);
  http_path_add("/epg",              NULL, extjs_epg,              ACCESS_WEB_INTERFACE);
  http_path_add("/epgrelated",       NULL, extjs_epgrelated,       ACCESS_WEB_INTERFACE);
  http_path_add("/epgobject",        NULL, extjs_epgobject,        ACCESS_WEB_INTERFACE);
  http_path_add("/dvr",              NULL, extjs_dvr,              ACCESS_WEB_INTERFACE);
  http_path_add("/dvrlist_upcoming", NULL, extjs_dvrlist_upcoming, ACCESS_WEB_INTERFACE);
  http_path_add("/dvrlist_finished", NULL, extjs_dvrlist_finished, ACCESS_WEB_INTERFACE);
  http_path_add("/dvrlist_failed",   NULL, extjs_dvrlist_failed,   ACCESS_WEB_INTERFACE);
  http_path_add("/dvr_containers",   NULL, extjs_dvr_containers,   ACCESS_WEB_INTERFACE);
  http_path_add("/dvr_caches",       NULL, extjs_dvr_caches,       ACCESS_WEB_INTERFACE);
  http_path_add("/ecglist",          NULL, extjs_ecglist,          ACCESS_WEB_INTERFACE);
  http_path_add("/config",           NULL, extjs_config,           ACCESS_WEB_INTERFACE);
  http_path_add("/languages",        NULL, extjs_languages,        ACCESS_WEB_INTERFACE);
#if ENABLE_TIMESHIFT
  http_path_add("/timeshift",        NULL, extjs_timeshift,        ACCESS_ADMIN);
#endif
  http_path_add("/tvhlog",           NULL, extjs_tvhlog,           ACCESS_ADMIN);

#if ENABLE_V4L
  extjs_start_v4l();
#endif
}
