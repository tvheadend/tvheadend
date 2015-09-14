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
#include "libav.h"
#include "satip/server.h"

#define EXTJSPATH "static/extjs"

/**
 *
 */
static void
extjs_load(htsbuf_queue_t *hq, const char *script, ...)
{
  va_list ap;
  htsbuf_qprintf(hq, "<script type=\"text/javascript\" src=\"");

  va_start(ap, script);
  htsbuf_vqprintf(hq, script, ap);
  va_end(ap);

  htsbuf_qprintf(hq, "\"></script>\n");
}

/**
 *
 */
static void
extjs_lcss(htsbuf_queue_t *hq, const char *css, ...)
{
  va_list ap;

  htsbuf_qprintf(hq, "<link rel=\"stylesheet\" type=\"text/css\" href=\"");

  va_start(ap, css);
  htsbuf_vqprintf(hq, css, ap);
  va_end(ap);

  htsbuf_qprintf(hq, "\"/>\n");
}

/**
 *
 */
static void
extjs_exec(htsbuf_queue_t *hq, const char *fmt, ...)
{
  va_list ap;

  htsbuf_qprintf(hq, "<script type=\"text/javascript\">\n");

  va_start(ap, fmt);
  htsbuf_vqprintf(hq, fmt, ap);
  va_end(ap);

  htsbuf_qprintf(hq, "\n</script>\n");
}

/**
 * EXTJS root page
 */
static int
extjs_root(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;

  htsbuf_qprintf(hq, "<html>\n");
  htsbuf_qprintf(hq, "<head>\n");

  htsbuf_qprintf(hq, "<meta name=\"apple-itunes-app\" content=\"app-id=638900112\">\n");

  if (tvheadend_webui_debug) {
  
#include "extjs-debug.c"

  } else {

#include "extjs-std.c"

  }

  extjs_exec(hq, "\
Ext.BLANK_IMAGE_URL = \'" EXTJSPATH "/resources/images/default/s.gif';\r\n\
Ext.onReady(tvheadend.app.init, tvheadend.app);\
");


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

  if (tvheadend_webui_debug) {

#include "extjs-tv-debug.c"

  } else {

#include "extjs-tv-std.c"

  }

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
  const char *lang = hc->hc_access->aa_lang;

  htsbuf_qprintf(hq, "<center>\n\
<div class=\"about-title\">HTS Tvheadend %s</div>\n\
<p>&copy; 2006 - 2015 Andreas \303\226man, et al.</p>\n\
<p><img src=\"docresources/tvheadendlogo.png\"></p>\n\
<p><a href=\"https://tvheadend.org\">https://tvheadend.org</a></p>\n",
    tvheadend_version);

  htsbuf_qprintf(hq, "<p>%s \n\
<a target=\"_blank\" href=\"http://www.extjs.com/\">ExtJS</a>. \
%s <a target=\"_blank\" href=\"http://www.famfamfam.com/lab/icons/silk/\">\
FamFamFam</a>\n\
</p>\n",
    tvh_gettext_lang(lang, N_("Based on software from")),
    tvh_gettext_lang(lang, N_("Icons from")));

  htsbuf_qprintf(hq, "<p>%s: %s (%s) <a href=\"javascript:void(0)\"\
 onclick=\"Ext.get('textarea_build_config').setVisibilityMode(Ext.Element.DISPLAY).toggle()\">%s</a></p>\n",
    tvh_gettext_lang(lang, N_("Build")),
    tvheadend_version,
    build_timestamp,
    tvh_gettext_lang(lang, N_("Toggle details")));

  htsbuf_qprintf(hq,
"<textarea id=\"textarea_build_config\" rows=\"20\" cols=\"80\" readonly \
 style=\"display: none; margin: 5px auto 10px\">\n%s\n</textarea>\n",
    build_config_str);

  htsbuf_qprintf(hq, "<p>\n\
%s<br/>\n\
%s\n\
</p>\n\
<a href='https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=3Z87UEHP3WZK2'><img src='https://www.paypalobjects.com/en_US/GB/i/btn/btn_donateCC_LG.gif' alt='' /></a>\n\
</center>\n",
   tvh_gettext_lang(lang, N_("If you'd like to support the project, please consider a donation.")),
   tvh_gettext_lang(lang, N_("All proceeds are used to support server infrastructure and buy test equipment.")));

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
    return HTTP_STATUS_BAD_REQUEST;

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

  /* OTA EPG trigger */
  } else if (!strcmp(op, "otaepgTrigger") ) {

    str = http_arg_get(&hc->hc_req_args, "after");
    if (!str)
      return HTTP_STATUS_BAD_REQUEST;

    pthread_mutex_lock(&global_lock);
    epggrab_ota_trigger(atoi(str));
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
    return HTTP_STATUS_BAD_REQUEST;

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
                   !!(tvhlog_options & TVHLOG_OPT_DBG_SYSLOG));
    htsmsg_add_u32(m, "tvhlog_libav",
                   !!(tvhlog_options & TVHLOG_OPT_LIBAV));
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
    if ((str = http_arg_get(&hc->hc_req_args, "tvhlog_libav")))
      tvhlog_options |= TVHLOG_OPT_LIBAV;
    else
      tvhlog_options &= ~TVHLOG_OPT_LIBAV;
    libav_set_loglevel();
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
    return HTTP_STATUS_BAD_REQUEST;

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
    htsmsg_add_u32(m, "timeshift_ram_size", timeshift_ram_size / 1048576);
    htsmsg_add_u32(m, "timeshift_ram_only", timeshift_ram_only);
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
    if ((str = http_arg_get(&hc->hc_req_args, "timeshift_ram_size"))) {
      timeshift_ram_size         = atol(str) * 1048576LL;
      timeshift_ram_segment_size = timeshift_ram_size / 10;
    }
    timeshift_ram_only = http_arg_get(&hc->hc_req_args, "timeshift_ram_only") ? 1 : 0;
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
#if ENABLE_TIMESHIFT
  http_path_add("/timeshift",        NULL, extjs_timeshift,        ACCESS_ADMIN);
#endif
  http_path_add("/tvhlog",           NULL, extjs_tvhlog,           ACCESS_ADMIN);
}
