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
#include "config.h"
#include "http.h"
#include "webui.h"

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
		 "<title>%s</title>\n"
		 "</head>\n"
		 "<body>\n"
		 "<div id=\"systemlog\"></div>\n"
		 "</body></html>\n",
		 config.server_name);

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
  htsbuf_qprintf(hq, "<title>%s</title>\n", config.server_name);

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
 * WEB user interface
 */
void
extjs_start(void)
{
  http_path_add("/about.html",       NULL, page_about,             ACCESS_WEB_INTERFACE);
  http_path_add("/extjs.html",       NULL, extjs_root,             ACCESS_WEB_INTERFACE);
  http_path_add("/tv.html",          NULL, extjs_livetv,           ACCESS_WEB_INTERFACE);
}
