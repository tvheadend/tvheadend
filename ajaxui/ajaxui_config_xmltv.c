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
  ajax_table_t ta;

  tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");

  switch(xmltv_globalstatus) {
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

  ajax_table_top(&ta, hc, tq,
		 (const char *[]){"Grabber", "Status", NULL},
		 (int[]){4,2});

  LIST_FOREACH(xg, &xmltv_grabbers, xg_link) {

    ajax_table_row_start(&ta, xg->xg_identifier);
    ajax_table_cell(&ta, NULL,
	     "<a href=\"javascript:void(0);\" "
	     "onClick=\"new Ajax.Updater('grabberpane', "
	     "'/ajax/xmltvgrabber/%s', {method: 'get', evalScripts: true})\""
	     ">%s</a>", xg->xg_identifier, xg->xg_title);

    ajax_table_cell(&ta, "status", xmltv_grabber_status(xg));
  }
  ajax_table_bottom(&ta);

  ajax_box_end(tq, AJAX_BOX_SIDEBOX);

  tcp_qprintf(tq, "</div>"
	      "<div id=\"grabberpane\" style=\"float: left; width:55%\">"
	      "</div>");

  tcp_qprintf(tq, "</div>");
  http_output_html(hc, hr);
  return 0;
}

/**
 * Generate displaylisting
 */
static void
xmltv_grabber_chlist(tcp_queue_t *tq, xmltv_grabber_t *xg)
{
  xmltv_channel_t *xc;
  channel_group_t *tcg;
  channel_t *ch;

  tcp_qprintf(tq,
	      "<div style=\"overflow: auto; height: 450px\">");

  TAILQ_FOREACH(xc, &xg->xg_channels, xc_link) {

    tcp_qprintf(tq,
		"<div style=\"overflow: auto; width: 100%%\">");

    tcp_qprintf(tq, "<div class=\"iconbackdrop\">");
    if(xc->xc_icon_url != NULL) {
      tcp_qprintf(tq,
		  "<img style=\"border: 0px;\" src=\"%s\" height=62px\">",
		  xc->xc_icon_url);
    } else {
      tcp_qprintf(tq,
		  "<div style=\"margin-top: 20px; text-align: center\">"
		  "No icon</div>");
    }
    tcp_qprintf(tq, "</div>"); /* iconbackdrop */


    tcp_qprintf(tq,
		"<div class=\"infoprefixwide\">Name:</div>"
		"<div>%s (%s)</div>", xc->xc_displayname, xc->xc_name);

    tcp_qprintf(tq,
		"<div class=\"infoprefixwide\">Auto mapper:</div>"
		"<div>%s</div>", xc->xc_bestmatch ?: "(no channel)");

    tcp_qprintf(tq,
		"<div class=\"infoprefixwidefat\">Channel:</div>"
		"<select class=\"textinput\" "
		"onChange=\"new Ajax.Request('/ajax/xmltvgrabberchmap/%s', "
		"{parameters: { xmltvch: '%s', channel: this.value }})\">",
		xg->xg_identifier, xc->xc_name);

    tcp_qprintf(tq, "<option value=\"auto\">Automatic mapper</option>",
		!xc->xc_disabled && xc->xc_channel == NULL ? " selected" : "");

    tcp_qprintf(tq, "<option value=\"none\"%s>No channel</option>",
		xc->xc_disabled ? " selected" : "");
 
    TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {
      if(tcg->tcg_hidden)
	continue;

      TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link) {
	if(LIST_FIRST(&ch->ch_transports) == NULL)
	  continue;

	tcp_qprintf(tq, "<option value=\"%d\"%s>%s</option>",
		    ch->ch_tag,
		    !strcmp(ch->ch_name, xc->xc_channel ?: "")
		    ? " selected " : "",  ch->ch_name);
      }
    }
    tcp_qprintf(tq, "</select>");
    tcp_qprintf(tq, "</div><hr>\r\n");
  }
  tcp_qprintf(tq, "</div>");
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

  if(xg->xg_enabled == 0) {
    tcp_qprintf(tq,
		"<p>This grabber is currently not enabled, click "
		"<a href=\"javascript:void(0);\" "
		"onClick=\"new Ajax.Request('/ajax/xmltvgrabbermode/%s', "
		"{parameters: {'mode': 'enable'}})\">here</a> "
		"to enable it</p>");
  } else if(xg->xg_status == XMLTV_GRAB_OK) {
    xmltv_grabber_chlist(tq, xg);
  } else {
    tcp_qprintf(tq, "<p>%s</p>", xmltv_grabber_status_long(xg));
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

  tcp_qprintf(tq,"$('details_%s').innerHTML='Please wait...';",
	      xg->xg_identifier);

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  return 0;
}



/**
 * Enable / Disable a grabber
 */
static int
ajax_xmltvgrabberlist(http_connection_t *hc, http_reply_t *hr, 
		      const char *remain, void *opaque)
{
  xmltv_grabber_t *xg;
  tcp_queue_t *tq = &hr->hr_tq;

  if(remain == NULL || (xg = xmltv_grabber_find(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  xmltv_grabber_chlist(tq, xg);

  http_output_html(hc, hr);
  return 0;
}


/**
 * Change mapping of a channel for a grabber
 */
static int
ajax_xmltvgrabberchmap(http_connection_t *hc, http_reply_t *hr, 
		       const char *remain, void *opaque)
{
  xmltv_grabber_t *xg;
  xmltv_channel_t *xc;
  const char *xmltvname;
  const char *chname;
  channel_t *ch;
  //  tcp_queue_t *tq = &hr->hr_tq;
  
  if(remain == NULL || (xg = xmltv_grabber_find(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if((xmltvname = http_arg_get(&hc->hc_req_args, "xmltvch")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if((chname = http_arg_get(&hc->hc_req_args, "channel")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  TAILQ_FOREACH(xc, &xg->xg_channels, xc_link)
    if(!strcmp(xc->xc_name, xmltvname))
      break;

  if(xc == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  pthread_mutex_lock(&xg->xg_mutex);

  free(xc->xc_channel);
  xc->xc_channel = NULL;
  xc->xc_disabled = 0;
    
  if(!strcmp(chname, "none")) {
    xc->xc_disabled = 1;
  } else if(!strcmp(chname, "auto")) {
  } else if((ch = channel_by_tag(atoi(chname))) != NULL) {
    xc->xc_disabled = 0;
    xc->xc_channel = strdup(ch->ch_name);
  }
  pthread_mutex_unlock(&xg->xg_mutex);

  xmltv_config_save();

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  return 0;
}



/**
 *
 */
void
ajax_config_xmltv_init(void)
{
  http_path_add("/ajax/xmltvgrabber" ,      NULL, ajax_xmltvgrabber,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/xmltvgrabbermode" ,  NULL, ajax_xmltvgrabbermode,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/xmltvgrabberlist" ,  NULL, ajax_xmltvgrabberlist,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/xmltvgrabberchmap" , NULL, ajax_xmltvgrabberchmap,
		AJAX_ACCESS_CONFIG);

}
