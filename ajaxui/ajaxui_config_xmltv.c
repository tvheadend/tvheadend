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

#define _GNU_SOURCE

#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "tvhead.h"
#include "http.h"
#include "ajaxui.h"
#include "channels.h"
#include "epg_xmltv.h"
#include "psi.h"
#include "transports.h"

#include "ajaxui_mailbox.h"



/**
 * XMLTV configuration
 */
int
ajax_config_xmltv_tab(http_connection_t *hc, http_reply_t *hr)
{
  tcp_queue_t *tq = &hr->hr_tq;
  xmltv_grabber_t *xg;
  int ngrabbers = 0;
  int displines = 21;
  int csize[10];
  const char *cells[10];
  int o = 1;
  char buf[200];

  tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");

  switch(xmltv_status) {
  default:
    tcp_qprintf(tq, "<p style=\"text-align: center; font-weight: bold\">"
		"XMLTV subsystem is not yet fully initialized, please retry "
		"in a few seconds</p></div>");
    http_output_html(hc, hr);
    return 0;

  case XMLTVSTATUS_FIND_GRABBERS_NOT_FOUND:
    tcp_qprintf(tq, "<p style=\"text-align: center; font-weight: bold\">"
		"XMLTV subsystem can not initialize</p>"
		"<p style=\"text-align: center\">"
		"Make sure that the 'tv_find_grabbers' executable is in "
		"the system path</p></div>");
    http_output_html(hc, hr);
    return 0;

  case XMLTVSTATUS_RUNNING:
    break;
  }

  tcp_qprintf(tq, "<div style=\"float: left; width:45%\">");

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, "XMLTV grabbers");

  LIST_FOREACH(xg, &xmltv_grabbers, xg_link)
    ngrabbers++;

  ajax_table_header(hc, tq,
		    (const char *[]){"Grabber", "Status", NULL},
		    (int[]){4,2}, ngrabbers > displines, csize);

  tcp_qprintf(tq, "<hr><div "
	      "style=\"height: %dpx; overflow: auto\" class=\"normallist\">",
	      MAX(displines, ngrabbers) * 14);

  LIST_FOREACH(xg, &xmltv_grabbers, xg_link) {

    snprintf(buf, sizeof(buf), 
	     "<a href=\"javascript:void(0);\" "
	     "onClick=\"new Ajax.Updater('grabberpane', "
	     "'/ajax/xmltvgrabber/%s', {method: 'get', evalScripts: true})\""
	     ">%s</a>", xg->xg_identifier, xg->xg_title);

    cells[0] = buf;
    cells[1] = xmltv_grabber_status(xg);
    cells[2] = NULL;

    ajax_table_row(tq, cells, csize, &o,
		   (const char *[]){NULL, "status"},
		   xg->xg_identifier);
 
  }
  tcp_qprintf(tq, "</div>");

  ajax_box_end(tq, AJAX_BOX_SIDEBOX);

  tcp_qprintf(tq, "</div>"
	      "<div id=\"grabberpane\" style=\"float: left; width:55%\">"
	      "</div>");

  tcp_qprintf(tq, "</div>");
  http_output_html(hc, hr);
  return 0;
}


/**
 * Display detailes about a grabber
 */
static int
ajax_xmltvgrabber(http_connection_t *hc, http_reply_t *hr, 
		  const char *remain, void *opaque)
{
  xmltv_grabber_t *xg;
  tcp_queue_t *tq = &hr->hr_tq;


  if(remain == NULL || (xg = xmltv_grabber_find(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, xg->xg_title);
  
  tcp_qprintf(tq,"<div id=\"details_%s\">", xg->xg_identifier);

  switch(xg->xg_status) {
  case XMLTV_GRABBER_DISABLED:
    tcp_qprintf(tq,
		"<p>This grabber is currently not enabled, click "
		"<a href=\"javascript:void(0);\" "
		"onClick=\"new Ajax.Request('/ajax/xmltvgrabbermode/%s', "
		"{parameters: {'mode': 'enable'}})\">here</a> "
		"to enable it</p>");
    break;


  case XMLTV_GRABBER_ENQUEUED:
  case XMLTV_GRABBER_GRABBING:
  case XMLTV_GRABBER_UNCONFIGURED:
  case XMLTV_GRABBER_DYSFUNCTIONAL:
  case XMLTV_GRABBER_IDLE:
    tcp_qprintf(tq, "<p>%s</p>", xmltv_grabber_status_long(xg, xg->xg_status));
    break;

  }
  

  tcp_qprintf(tq,"</div>");

  ajax_box_end(tq, AJAX_BOX_SIDEBOX);
  http_output_html(hc, hr);
  return 0;
}



/**
 * Enable / Disable a grabber
 */
static int
ajax_xmltvgrabbermode(http_connection_t *hc, http_reply_t *hr, 
		      const char *remain, void *opaque)
{
  xmltv_grabber_t *xg;
  tcp_queue_t *tq = &hr->hr_tq;

  if(remain == NULL || (xg = xmltv_grabber_find(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  xmltv_grabber_enable(xg);

  tcp_qprintf(tq,"$('details_%s').innerHTML='Ok, please wait...';",
	      xg->xg_identifier);

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  return 0;
}



/**
 *
 */
void
ajax_config_xmltv_init(void)
{
  http_path_add("/ajax/xmltvgrabber" ,     NULL, ajax_xmltvgrabber);
  http_path_add("/ajax/xmltvgrabbermode" , NULL, ajax_xmltvgrabbermode);
 
}
