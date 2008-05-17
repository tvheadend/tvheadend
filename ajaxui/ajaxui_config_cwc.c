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
#include "cwc.h"
/**
 * CWC configuration
 */
int
ajax_config_cwc_tab(http_connection_t *hc, http_reply_t *hr)
{
  tcp_queue_t *tq = &hr->hr_tq;

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, "Code-word Client");

  tcp_qprintf(tq, "<div id=\"cwclist\"></div>");
  
  ajax_js(tq,  
	  "new Ajax.Updater('cwclist', '/ajax/cwclist', "
	  "{method: 'get', evalScripts: true});");

  tcp_qprintf(tq, "<hr><div style=\"overflow: auto; width: 100%\">");

 tcp_qprintf(tq,
	      "<div class=\"cell_100\">"
	      "<div class=\"infoprefixwidewidefat\">Hostname:</div>"
	      "<div>"
	      "<input  type=\"text\" size=40 id=\"hostname\">"
	      "</div></div>");

 tcp_qprintf(tq,
	      "<div class=\"cell_100\">"
	      "<div class=\"infoprefixwidewidefat\">Port:</div>"
	      "<div>"
	      "<input  type=\"text\" id=\"port\">"
	      "</div></div>");


  tcp_qprintf(tq,
	      "<div class=\"cell_100\">"
	      "<div class=\"infoprefixwidewidefat\">Username:</div>"
	      "<div>"
	      "<input  type=\"text\" id=\"username\">"
	      "</div></div>");

  tcp_qprintf(tq,
	      "<div class=\"cell_100\">"
	      "<div class=\"infoprefixwidewidefat\">Password:</div>"
	      "<div>"
	      "<input  type=\"password\" id=\"password\">"
	      "</div></div>");

  tcp_qprintf(tq,
	      "<div class=\"cell_100\">"
	      "<div class=\"infoprefixwidewidefat\">DES-key:</div>"
	      "<div>"
	      "<input  type=\"text\" size=50 id=\"deskey\">"
	      "</div></div>");

  tcp_qprintf(tq,
	      "<br>"
	      "<input type=\"button\" value=\"Add new server entry\" "
	      "onClick=\"new Ajax.Request('/ajax/cwcadd', "
	      "{parameters: {"
	      "'hostname': $F('hostname'), "
	      "'port': $F('port'), "
	      "'username': $F('username'), "
	      "'password': $F('password'), "
	      "'deskey': $F('deskey') "
	      "}})"
	      "\">");

  tcp_qprintf(tq, "</div>\r\n");
  
  ajax_box_end(tq, AJAX_BOX_SIDEBOX);
  
  tcp_qprintf(tq, "</div>");
  http_output_html(hc, hr);
  return 0;
}


/**
 *
 */
static int
ajax_cwclist(http_connection_t *hc, http_reply_t *hr, 
	    const char *remain, void *opaque)
{
  tcp_queue_t *tq = &hr->hr_tq;
  ajax_table_t ta;
  cwc_t *cwc;
  char id[20];

  ajax_table_top(&ta, hc, tq,
		 (const char *[]){"Code-word Server",
				   "Username",
		                   "Enabled",
		                   "Status",
		                   "Crypto",
		                   "",
				    NULL}, 
		 (int[]){3, 2, 1, 6, 6, 1});

  TAILQ_FOREACH(cwc, &cwcs, cwc_link) {
    snprintf(id, sizeof(id), "cwc_%d", cwc->cwc_id);
    ajax_table_row_start(&ta, id);

    ajax_table_cell(&ta, NULL, "%s:%d",
		    cwc->cwc_tcp_session.tcp_hostname,
		    cwc->cwc_tcp_session.tcp_port);

    ajax_table_cell(&ta, NULL, "%s", cwc->cwc_username);

    ajax_table_cell(&ta, NULL, 
		    "<input %stype=\"checkbox\" class=\"nicebox\" "
		    "onChange=\"new Ajax.Request('/ajax/cwcchange', "
		    "{parameters: {checked: this.checked, id: %d}})\">",
		    1 ? "checked " : "", cwc->cwc_id);

    ajax_table_cell(&ta, "status", cwc_status_to_text(cwc));

    ajax_table_cell(&ta, "crypto", cwc_crypto_to_text(cwc));

    ajax_table_cell(&ta, NULL,
		    "<a href=\"javascript:void(0)\" "
		    "onClick=\"dellistentry('/ajax/cwcdel', '%d', '%s')\">"
		    "Delete...</a>",
		    cwc->cwc_id, cwc->cwc_tcp_session.tcp_hostname);
  }

  ajax_table_bottom(&ta);
  http_output_html(hc, hr);
  return 0;
}


/**
 *
 */
static int
ajax_cwcadd(http_connection_t *hc, http_reply_t *hr, 
	    const char *remain, void *opaque)
{
  tcp_queue_t *tq = &hr->hr_tq;
  const char *errtxt;

  errtxt = cwc_add(http_arg_get(&hc->hc_req_args, "hostname"),
		   http_arg_get(&hc->hc_req_args, "port"),
		   http_arg_get(&hc->hc_req_args, "username"),
		   http_arg_get(&hc->hc_req_args, "password"),
		   http_arg_get(&hc->hc_req_args, "deskey"),
		   "1", 1, 1);

  if(errtxt != NULL) {
    tcp_qprintf(tq, "alert('%s');", errtxt);
  } else {
    
    tcp_qprintf(tq, "$('hostname').clear();\r\n");
    tcp_qprintf(tq, "$('port').clear();\r\n");
    tcp_qprintf(tq, "$('username').clear();\r\n");
    tcp_qprintf(tq, "$('password').clear();\r\n");
    tcp_qprintf(tq, "$('deskey').clear();\r\n");
    tcp_qprintf(tq, 
		"new Ajax.Updater('cwclist', '/ajax/cwclist', "
		"{method: 'get', evalScripts: true});");
  }
  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);

  return 0;
}


/**
 *
 */
static int
ajax_cwcdel(http_connection_t *hc, http_reply_t *hr, 
	    const char *remain, void *opaque)
{
  tcp_queue_t *tq = &hr->hr_tq;
  const char *txt;
  cwc_t *cwc;

  if((txt = http_arg_get(&hc->hc_req_args, "id")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if((cwc = cwc_find(atoi(txt))) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  cwc_delete(cwc);
  tcp_qprintf(tq, 
	      "new Ajax.Updater('cwclist', '/ajax/cwclist', "
	      "{method: 'get', evalScripts: true});");

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  return 0;
}

/**
 *
 */
static int
ajax_cwcchange(http_connection_t *hc, http_reply_t *hr, 
	    const char *remain, void *opaque)
{
  //  tcp_queue_t *tq = &hr->hr_tq;
  const char *txt;
  cwc_t *cwc;

  if((txt = http_arg_get(&hc->hc_req_args, "id")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if((cwc = cwc_find(atoi(txt))) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if((txt = http_arg_get(&hc->hc_req_args, "checked")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  //  cwc_set_state(cwc, atoi(txt));
  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  return 0;
}


/**
 *
 */
void
ajax_config_cwc_init(void)
{
  http_path_add("/ajax/cwclist" ,      NULL, ajax_cwclist,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/cwcadd" ,       NULL, ajax_cwcadd,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/cwcdel" ,       NULL, ajax_cwcdel,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/cwcchange" ,       NULL, ajax_cwcchange,
		AJAX_ACCESS_CONFIG);
}
