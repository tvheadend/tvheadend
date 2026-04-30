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
  htsbuf_append_str(hq, "<script type=\"text/javascript\" src=\"");

  va_start(ap, script);
  htsbuf_vqprintf(hq, script, ap);
  va_end(ap);

  htsbuf_append_str(hq, "\"></script>\n");
}

/**
 *
 */
static void
extjs_lcss(htsbuf_queue_t *hq, const char *css, ...)
{
  va_list ap;

  htsbuf_append_str(hq, "<link rel=\"stylesheet\" type=\"text/css\" href=\"");

  va_start(ap, css);
  htsbuf_vqprintf(hq, css, ap);
  va_end(ap);

  htsbuf_append_str(hq, "\"/>\n");
}

/**
 *
 */
static void
extjs_exec(htsbuf_queue_t *hq, const char *fmt, ...)
{
  va_list ap;

  htsbuf_append_str(hq, "<script type=\"text/javascript\">\n");

  va_start(ap, fmt);
  htsbuf_vqprintf(hq, fmt, ap);
  va_end(ap);

  htsbuf_append_str(hq, "\n</script>\n");
}

/**
 * EXTJS root page
 */
static int
extjs_root(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;

  htsbuf_append_str(hq, "<!DOCTYPE html>\n");
  htsbuf_append_str(hq, "<html>\n");
  htsbuf_append_str(hq, "<head>\n");

  htsbuf_append_str(hq, "<link rel=\"shortcut icon\" href=\"static/img/logo.png\" type=\"image/png\">\n");
  htsbuf_append_str(hq, "<meta name=\"apple-itunes-app\" content=\"app-id=638900112\">\n");

  if (tvheadend_webui_debug) {
  
#include "extjs-debug.c"

  } else {

#include "extjs-std.c"

  }

  extjs_exec(hq, "\
Ext.BLANK_IMAGE_URL = \'" EXTJSPATH "/resources/images/default/s.gif';\n\
Ext.onReady(tvheadend.app.init, tvheadend.app);\
");


  htsbuf_append_str(hq,
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
		 "<title>");
  htsbuf_append_str(hq, config_get_server_name());
  htsbuf_append_str(hq,
		 "</title>\n"
		 "</head>\n"
		 "<body>\n"
		 "<div id=\"systemlog\"></div>\n"
		 "</body></html>\n");

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

  htsbuf_append_str(hq, "<!DOCTYPE html>\n");
  htsbuf_append_str(hq, "<html>\n");
  htsbuf_append_str(hq, "<head>\n");
  htsbuf_append_str(hq, "<link rel=\"shortcut icon\" href=\"static/img/logo.png\" type=\"image/png\">\n");
  htsbuf_append_str(hq, "<title>");
  htsbuf_append_str(hq, config_get_server_name());
  htsbuf_append_str(hq, "</title>\n");

  if (tvheadend_webui_debug) {

#include "extjs-tv-debug.c"

  } else {

#include "extjs-tv-std.c"

  }

  extjs_exec(hq, "Ext.onReady(tv.app.init, tv.app);");

  htsbuf_append_str(hq, "</head>\n");
  htsbuf_append_str(hq, "<body></body>\n");
  htsbuf_append_str(hq, "</html>\n");

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
  const char *lang = hc->hc_access->aa_lang_ui;

  htsbuf_qprintf(hq, "<center class=\"about-tab\">\n\
<div class=\"about-title\">HTS Tvheadend %s</div>\n\
<p>&copy; 2006 - %.4s Andreas Smas, Jaroslav Kysela, Adam Sutton, et al.</p>\n\
<p><img class=\"logobig\" src=\"static/img/logobig.png\"></p>\n\
<p><a href=\"https://tvheadend.org\">https://tvheadend.org</a></p>\n",
    tvheadend_version, build_timestamp);

  htsbuf_qprintf(hq, "<p>%s \n\
<a target=\"_blank\" href=\"http://www.extjs.com/\">ExtJS</a>. \
%s <a target=\"_blank\" href=\"http://www.famfamfam.com/lab/icons/silk/\">\
FamFamFam</a>, "\
"<a target=\"_blank\" href=\"https://www.google.com/get/noto/help/emoji/\">Google Noto Color Emoji</a> "\
"<a target=\"_blank\" href=\"https://raw.githubusercontent.com/googlei18n/noto-emoji/master/LICENSE\">(Apache Licence v2.0)</a>.\n"\
"<p>This product uses the TMDB and TheTVDB.com API to provide TV information and images.</p>"\
"<p>It is not endorsed or certified by <a target=\"_blank\" href=\"https://www.themoviedb.org\">TMDb</a> <img src=\"static/img/tmdb.png\" class=\"tmdb\"> or by <a target=\"_blank\" href=\"https://thetvdb.com\">TheTVDB.com</a> <img src=\"static/img/tvdb.png\" class=\"tvdb\">.</p>\n",
    tvh_gettext_lang(lang, N_("Based on software from")),
    tvh_gettext_lang(lang, N_("Icons from")));

  htsbuf_qprintf(hq, "<p>%s: %s (%s)",
    tvh_gettext_lang(lang, N_("Build")),
    tvheadend_version,
    build_timestamp);
  if (!http_access_verify(hc, ACCESS_ADMIN)) {
    htsbuf_qprintf(hq,
" <a href=\"javascript:void(0)\"\
 onclick=\"Ext.get('textarea_build_config').setVisibilityMode(Ext.Element.DISPLAY).toggle()\">%s</a>\
</p>\n<textarea id=\"textarea_build_config\" rows=\"20\" cols=\"80\" readonly \
 style=\"display: none; margin: 5px auto 10px\">\n%s\n</textarea>\n",
    tvh_gettext_lang(lang, N_("Toggle details")),
    build_config_str);
  } else {
    htsbuf_qprintf(hq, "</p>\n");
  }

  htsbuf_qprintf(hq, "<p>\n\
%s<br/>\n\
%s\n\
</p>\n\
<a target=\"_blank\" href='https://opencollective.com/tvheadend/donate'><img src='static/img/opencollective.png' alt='' /></a>\n\
</center>\n",
   tvh_gettext_lang(lang, N_("To support Tvheadend development please consider making a donation")),
   tvh_gettext_lang(lang, N_("towards project operating costs.")));

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
