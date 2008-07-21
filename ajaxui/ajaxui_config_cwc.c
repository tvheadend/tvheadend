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
  htsbuf_queue_t *q = &hr->hr_q;

  ajax_box_begin(q, AJAX_BOX_SIDEBOX, NULL, NULL, "Code-word Client");

  htsbuf_qprintf(q, "<div id=\"cwclist\"></div>");
  
  ajax_js(q,  
	  "new Ajax.Updater('cwclist', '/ajax/cwclist', "
	  "{method: 'get', evalScripts: true});");

  htsbuf_qprintf(q, "<hr><div style=\"overflow: auto; width: 100%\">");

 htsbuf_qprintf(q,
	      "<div class=\"cell_100\">"
	      "<div class=\"infoprefixwidewidefat\">Hostname:</div>"
	      "<div>"
	      "<input  type=\"text\" size=40 id=\"hostname\">"
	      "</div></div>");

 htsbuf_qprintf(q,
	      "<div class=\"cell_100\">"
	      "<div class=\"infoprefixwidewidefat\">Port:</div>"
	      "<div>"
	      "<input  type=\"text\" id=\"port\">"
	      "</div></div>");


  htsbuf_qprintf(q,
	      "<div class=\"cell_100\">"
	      "<div class=\"infoprefixwidewidefat\">Username:</div>"
	      "<div>"
	      "<input  type=\"text\" id=\"username\">"
	      "</div></div>");

  htsbuf_qprintf(q,
	      "<div class=\"cell_100\">"
	      "<div class=\"infoprefixwidewidefat\">Password:</div>"
	      "<div>"
	      "<input  type=\"password\" id=\"password\">"
	      "</div></div>");

  htsbuf_qprintf(q,
	      "<div class=\"cell_100\">"
	      "<div class=\"infoprefixwidewidefat\">DES-key:</div>"
	      "<div>"
	      "<input  type=\"text\" size=50 id=\"deskey\">"
	      "</div></div>");

  htsbuf_qprintf(q,
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

  htsbuf_qprintf(q, "</div>\r\n");
  
  ajax_box_end(q, AJAX_BOX_SIDEBOX);
  
  htsbuf_qprintf(q, "</div>");
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
  htsbuf_queue_t *q = &hr->hr_q;
  ajax_table_t ta;
  cwc_t *cwc;
  char id[20];

  ajax_table_top(&ta, hc, q,
		 (const char *[]){"Code-word Server",
				   "Username",
		                   "Enabled",
		                   "Status",
		                   "",
				    NULL}, 
		 (int[]){3, 2, 1, 12, 1});

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
		    cwc->cwc_tcp_session.tcp_enabled
		    ? "checked " : "", cwc->cwc_id);

    ajax_table_cell(&ta, "status", cwc_status_to_text(cwc));

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
  htsbuf_queue_t *q = &hr->hr_q;
  const char *errtxt;

  errtxt = cwc_add(http_arg_get(&hc->hc_req_args, "hostname"),
		   http_arg_get(&hc->hc_req_args, "port"),
		   http_arg_get(&hc->hc_req_args, "username"),
		   http_arg_get(&hc->hc_req_args, "password"),
		   http_arg_get(&hc->hc_req_args, "deskey"),
		   "1", 1, 1);

  if(errtxt != NULL) {
    htsbuf_qprintf(q, "alert('%s');", errtxt);
  } else {
    
    htsbuf_qprintf(q, "$('hostname').clear();\r\n");
    htsbuf_qprintf(q, "$('port').clear();\r\n");
    htsbuf_qprintf(q, "$('username').clear();\r\n");
    htsbuf_qprintf(q, "$('password').clear();\r\n");
    htsbuf_qprintf(q, "$('deskey').clear();\r\n");
    htsbuf_qprintf(q, 
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
  htsbuf_queue_t *q = &hr->hr_q;
  const char *txt;
  cwc_t *cwc;

  if((txt = http_arg_get(&hc->hc_req_args, "id")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if((cwc = cwc_find(atoi(txt))) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  cwc_delete(cwc);
  htsbuf_qprintf(q, 
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
  //  htsbuf_queue_t *q = &hr->hr_q;
  const char *txt;
  cwc_t *cwc;

  if((txt = http_arg_get(&hc->hc_req_args, "id")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if((cwc = cwc_find(atoi(txt))) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if((txt = http_arg_get(&hc->hc_req_args, "checked")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  cwc_set_enable(cwc, !strcmp(txt, "true"));
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
