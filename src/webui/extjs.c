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
#if ENABLE_CWC || ENABLE_CAPMT
  extjs_load(hq, "static/app/caclient.js");
#endif
  extjs_load(hq, "static/app/tvadapters.js");
  extjs_load(hq, "static/app/idnode.js");
  extjs_load(hq, "static/app/esfilter.js");
#if ENABLE_MPEGTS
  extjs_load(hq, "static/app/mpegts.js");
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
		 "&copy; 2006 - 2014 Andreas \303\226man, et al.<br><br>"
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
    if (!m) {
      pthread_mutex_unlock(&global_lock);
      return HTTP_STATUS_BAD_REQUEST;
    }

    /* Time */
    htsmsg_add_u32(m, "tvhtime_update_enabled", tvhtime_update_enabled);
    htsmsg_add_u32(m, "tvhtime_ntp_enabled", tvhtime_ntp_enabled);
    htsmsg_add_u32(m, "tvhtime_tolerance", tvhtime_tolerance);

    /* Transcoding */
#if ENABLE_LIBAV
    htsmsg_add_u32(m, "transcoding_enabled", transcoding_enabled);
#endif

    pthread_mutex_unlock(&global_lock);

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
    if ((str = http_arg_get(&hc->hc_req_args, "piconpath")))
      save |= config_set_picon_path(str);
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
    if (!m) {
      pthread_mutex_unlock(&tvhlog_mutex);
      return HTTP_STATUS_BAD_REQUEST;
    }
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
  http_path_add("/epggrab",          NULL, extjs_epggrab,          ACCESS_WEB_INTERFACE);
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
