/*
 *  tvheadend, AJAX / HTML user interface
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

#include "tvhead.h"
#include "http.h"
#include "ajaxui.h"
#include "dispatch.h"

#include "obj/ajaxui.cssh"

#include "obj/prototype.jsh"
#include "obj/builder.jsh"
#include "obj/controls.jsh"
#include "obj/dragdrop.jsh"
#include "obj/effects.jsh"
#include "obj/scriptaculous.jsh"
#include "obj/slider.jsh"

#include "obj/tvheadend.jsh"

#include "obj/sbbody_l.gifh"
#include "obj/sbbody_r.gifh"
#include "obj/sbhead_l.gifh"
#include "obj/sbhead_r.gifh"

#include "obj/mapped.pngh"
#include "obj/unmapped.pngh"



const char *ajax_tabnames[] = {
  [AJAX_TAB_CHANNELS]      = "Channels",
  [AJAX_TAB_RECORDER]      = "Recorder",
  [AJAX_TAB_CONFIGURATION] = "Configuration",
  [AJAX_TAB_ABOUT]         = "About",
};

/**
 * AJAX table header
 */
void
ajax_table_header(http_connection_t *hc, tcp_queue_t *tq,
		  const char *names[], int weights[],
		  int scrollbar, int columnsizes[])
{
  int n = 0, i, tw = 0;
  while(names[n]) {
    tw += weights[n];
    n++;
  }

  for(i = 0; i < n; i++)
    columnsizes[i] = 100 * weights[i] / tw;

  if(scrollbar)
    tcp_qprintf(tq, "<div style=\"padding-right: 20px\">");

  tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");

  for(i = 0; i < n; i++)
    tcp_qprintf(tq, "<div style=\"float: left; width: %d%%\">%s</div>",
		columnsizes[i], *names[i] ? names[i]: "&nbsp;");

  tcp_qprintf(tq, "</div>");
  if(scrollbar)
    tcp_qprintf(tq, "</div>");
}
		  
		  
/**
 * AJAX table row
 */
void
ajax_table_row(tcp_queue_t *tq, const char *cells[], int columnsizes[],
	       int *bgptr, const char *idprefix[], const char *idpostfix)
{
  int i = 0;

  tcp_qprintf(tq, "<div style=\"%soverflow: auto; width: 100%\">",
	      *bgptr ? "background: #fff; " : "");

  *bgptr = !*bgptr;

  while(cells[i]) {
    tcp_qprintf(tq, 
		"<div %s%s%s%s%sstyle=\"float: left; width: %d%%\">%s</div>",
		idprefix && idprefix[i]              ? "id=\""     : "",
		idprefix && idprefix[i]              ? idprefix[i] : "",
		idprefix && idprefix[i] && idpostfix ? "_"         : "",
		idprefix && idprefix[i] && idpostfix ? idpostfix   : "",
		idprefix && idprefix[i]              ? "\" "       : "",
		columnsizes[i], cells[i]);
    i++;
  }
  tcp_qprintf(tq, "</div>\r\n");
}




/**
 * AJAX box start
 */
void
ajax_box_begin(tcp_queue_t *tq, ajax_box_t type,
	       const char *id, const char *style, const char *title)
{
  char id0[100], style0[100];
  
  if(id != NULL)
    snprintf(id0, sizeof(id0), "id=\"%s\" ", id);
  else
    id0[0] = 0;

  if(style != NULL)
    snprintf(style0, sizeof(style0), "style=\"%s\" ", style);
  else
    style0[0] = 0;


  switch(type) {
  case AJAX_BOX_SIDEBOX:
    tcp_qprintf(tq,
		"<div class=\"sidebox\">"
		"<div class=\"boxhead\"><h2>%s</h2></div>\r\n"
		"  <div class=\"boxbody\" %s%s>",
		title, id0, style0);
    break;

  case AJAX_BOX_FILLED:
    tcp_qprintf(tq, 
		"<div style=\"margin: 3px\">"
		"<b class=\"filledbox\">"
		"<b class=\"filledbox1\"><b></b></b>"
		"<b class=\"filledbox2\"><b></b></b>"
		"<b class=\"filledbox3\"></b>"
		"<b class=\"filledbox4\"></b>"
		"<b class=\"filledbox5\"></b></b>"
		"<div class=\"filledboxfg\" %s%s>\r\n",
		id0, style0);
    break;

  case AJAX_BOX_BORDER:
   tcp_qprintf(tq, 
		"<div style=\"margin: 3px\">"
		"<b class=\"borderbox\">"
		"<b class=\"borderbox1\"><b></b></b>"
		"<b class=\"borderbox2\"><b></b></b>"
		"<b class=\"borderbox3\"></b></b>"
		"<div class=\"borderboxfg\" %s%s>\r\n",
		id0, style0);

    break;
  }
}

/**
 * AJAX box end
 */
void
ajax_box_end(tcp_queue_t *tq, ajax_box_t type)
{
  switch(type) {
  case AJAX_BOX_SIDEBOX:
    tcp_qprintf(tq,"</div></div>");
    break;
    
  case AJAX_BOX_FILLED:
    tcp_qprintf(tq,
		"</div>"
		"<b class=\"filledbox\">"
		"<b class=\"filledbox5\"></b>"
		"<b class=\"filledbox4\"></b>"
		"<b class=\"filledbox3\"></b>"
		"<b class=\"filledbox2\"><b></b></b>"
		"<b class=\"filledbox1\"><b></b></b></b>"
		"</div>\r\n");
    break;

 case AJAX_BOX_BORDER:
    tcp_qprintf(tq,
		"</div>"
		"<b class=\"borderbox\">"
		"<b class=\"borderbox3\"></b>"
		"<b class=\"borderbox2\"><b></b></b>"
		"<b class=\"borderbox1\"><b></b></b></b>"
		"</div>\r\n");
    break;
 
  }
}

/**
 *
 */
void
ajax_js(tcp_queue_t *tq, const char *fmt, ...)
{
  va_list ap;

  tcp_qprintf(tq, 
	      "<script type=\"text/javascript\">\r\n"
	      "//<![CDATA[\r\n");

  va_start(ap, fmt);
  tcp_qvprintf(tq, fmt, ap);
  va_end(ap);

  tcp_qprintf(tq, 
	      "\r\n//]]>\r\n"
	      "</script>\r\n");
}



/**
 * Based on the given char[] array, generate a menu bar
 */
void
ajax_menu_bar_from_array(tcp_queue_t *tq, const char *name, 
			 const char **vec, int num, int cur)
{
  int i;
 
  tcp_qprintf(tq, "<ul class=\"menubar\">");

  for(i = 0; i < num; i++) {
    tcp_qprintf(tq,
		"<li%s>"
		"<a href=\"javascript:switchtab('%s', '%d')\">%s</a>"
		"</li>",
		cur == i ? " style=\"font-weight:bold;\"" : "", name,
		i, vec[i]);
  }
  tcp_qprintf(tq, "</ul>");
}


/**
 *
 */
void
ajax_a_jsfunc(tcp_queue_t *tq, const char *innerhtml, const char *func,
	      const char *trailer)
{
  tcp_qprintf(tq, "<a href=\"javascript:void(0)\" "
	      "onClick=\"javascript:%s\">%s</a>%s\r\n",
	      func, innerhtml, trailer);
}


/*
 * Titlebar AJAX page
 */
static int
ajax_page_titlebar(http_connection_t *hc, http_reply_t *hr, 
		   const char *remain, void *opaque)
{
  if(remain == NULL)
    return HTTP_STATUS_NOT_FOUND;

  ajax_menu_bar_from_array(&hr->hr_tq, "top", 
			   ajax_tabnames, AJAX_TABS, atoi(remain));
  http_output_html(hc, hr);
  return 0;
}



/**
 * About
 */
static int
ajax_about_tab(http_connection_t *hc, http_reply_t *hr)
{
  tcp_queue_t *tq = &hr->hr_tq;
  
  tcp_qprintf(tq, "<center>");
  tcp_qprintf(tq, "<div style=\"padding: auto; width: 400px\">");

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, "About");

  tcp_qprintf(tq, "<div style=\"text-align: center\">");

  tcp_qprintf(tq, 
	      "<p>HTS / Tvheadend</p>"
	      "<p>(c) 2006-2008 Andreas \303\226man</p>"
	      "<p>Latest release and information is available at:</p>"
	      "<p><a href=\"http://www.lonelycoder.com/hts/\">"
	      "http://www.lonelycoder.com/hts/</a></p>"
	      "<p>&nbsp;</p>"
	      "<p>This webinterface is powered by</p>"
	      "<p><a href=\"http://www.prototypejs.org/\">Prototype</a>"
	      " and "
	      "<a href=\"http://script.aculo.us/\">script.aculo.us</a>"
	      "</p>"
	      "<p>&nbsp;</p>"
	      "<p>Media formats and codecs by</p>"
	      "<p><a href=\"http://www.ffmpeg.org/\">FFmpeg</a></p>"
	      );

  tcp_qprintf(tq, "</div>");
  ajax_box_end(tq, AJAX_BOX_SIDEBOX);
  tcp_qprintf(tq, "</div>");
  tcp_qprintf(tq, "</center>");

  http_output_html(hc, hr);
  return 0;
}



/*
 * Tab AJAX page
 *
 * Find the 'tab' id and continue with tab specific code
 */
static int
ajax_page_tab(http_connection_t *hc, http_reply_t *hr, 
	      const char *remain, void *opaque)
{
  int tab;

  if(remain == NULL)
    return HTTP_STATUS_NOT_FOUND;

  tab = atoi(remain);

  switch(tab) {
  case AJAX_TAB_CHANNELS:
    return ajax_channelgroup_tab(hc, hr);

  case AJAX_TAB_CONFIGURATION:
    return ajax_config_tab(hc, hr);

  case AJAX_TAB_ABOUT:
    return ajax_about_tab(hc, hr);

  default:
    return HTTP_STATUS_NOT_FOUND;
  }
  return 0;
}



/*
 * Root page
 */
static int
ajax_page_root(http_connection_t *hc, http_reply_t *hr, 
	       const char *remain, void *opaque)
{
  tcp_queue_t *tq = &hr->hr_tq;

  tcp_qprintf(tq, 
	      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\r\n"
	      "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">"
	      /*
	      "<!DOCTYPE html "
	      "PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\r\n"
	      "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">"
	      */
	      "\r\n"
	      "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
	      "xml:lang=\"en\" lang=\"en\">"
	      "<head>"
	      "<title>HTS/Tvheadend</title>"
	      "<meta http-equiv=\"Content-Type\" "
	      "content=\"text/html; charset=utf-8\">\r\n"
	      "<link href=\"/ajax/ajaxui.css\" rel=\"stylesheet\" "
	      "type=\"text/css\">\r\n"
	      "<script src=\"/ajax/prototype.js\" type=\"text/javascript\">"
	      "</script>\r\n"
	      "<script src=\"/ajax/effects.js\" type=\"text/javascript\">"
	      "</script>\r\n"
	      "<script src=\"/ajax/dragdrop.js\" type=\"text/javascript\">"
	      "</script>\r\n"
	      "<script src=\"/ajax/controls.js\" type=\"text/javascript\">"
	      "</script>\r\n"
	      "<script src=\"/ajax/tvheadend.js\" type=\"text/javascript\">"
	      "</script>\r\n"
	      "</head>"
	      "<body>");


  tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");

  tcp_qprintf(tq, "<div style=\"float: left; width: 80%\">");

  ajax_box_begin(tq, AJAX_BOX_FILLED, "topmenu", NULL, NULL);
  ajax_box_end(tq, AJAX_BOX_FILLED);

  tcp_qprintf(tq, "<div id=\"topdeck\"></div>");
  
  ajax_js(tq, "switchtab('top', '0')");
#if 0
  tcp_qprintf(tq, "</div><div style=\"float: left; width: 20%\">");

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, "statusbox", NULL, "System status");
  ajax_box_end(tq, AJAX_BOX_SIDEBOX);
#endif
  tcp_qprintf(tq, "</div></div></body></html>");

  http_output_html(hc, hr);
  return 0;
}


/**
 * AJAX user interface
 */
void
ajaxui_start(void)
{
  http_path_add("/ajax/index.html",           NULL, ajax_page_root);

  http_path_add("/ajax/topmenu",              NULL, ajax_page_titlebar);
  http_path_add("/ajax/toptab",               NULL, ajax_page_tab);

  /* Stylesheet */
  http_resource_add("/ajax/ajaxui.css", embedded_ajaxui,
		    sizeof(embedded_ajaxui), "text/css", "gzip");

#define ADD_JS_RESOURCE(path, name) \
  http_resource_add(path, name, sizeof(name), "text/javascript", "gzip")

  /* Prototype */
  ADD_JS_RESOURCE("/ajax/prototype.js",          embedded_prototype);

  /* Scriptaculous */
  ADD_JS_RESOURCE("/ajax/builder.js",            embedded_builder);
  ADD_JS_RESOURCE("/ajax/controls.js",           embedded_controls);
  ADD_JS_RESOURCE("/ajax/dragdrop.js",           embedded_dragdrop);
  ADD_JS_RESOURCE("/ajax/effects.js",            embedded_effects);
  ADD_JS_RESOURCE("/ajax/scriptaculous.js",      embedded_scriptaculous);
  ADD_JS_RESOURCE("/ajax/slider.js",             embedded_slider);

  /* Tvheadend */
  ADD_JS_RESOURCE("/ajax/tvheadend.js",          embedded_tvheadend);

  /* Embedded images */
  http_resource_add("/sidebox/sbbody-l.gif", embedded_sbbody_l,
		    sizeof(embedded_sbbody_l), "image/gif", NULL);
  http_resource_add("/sidebox/sbbody-r.gif", embedded_sbbody_r,
		    sizeof(embedded_sbbody_r), "image/gif", NULL);
  http_resource_add("/sidebox/sbhead-l.gif", embedded_sbhead_l,
		    sizeof(embedded_sbhead_l), "image/gif", NULL);
  http_resource_add("/sidebox/sbhead-r.gif", embedded_sbhead_r,
		    sizeof(embedded_sbhead_r), "image/gif", NULL);

  http_resource_add("/gfx/unmapped.png", embedded_unmapped,
		    sizeof(embedded_unmapped), "image/png", NULL);

  http_resource_add("/gfx/mapped.png", embedded_mapped,
		    sizeof(embedded_mapped), "image/png", NULL);

  ajax_mailbox_init();
  ajax_channels_init();
  ajax_config_init();
  ajax_config_transport_init();
}
