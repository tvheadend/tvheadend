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
#include "psi.h"
#include "transports.h"

#include "ajaxui_mailbox.h"


static struct strtab accesstypetab[] = {
  { "stream",  ACCESS_STREAMING },
  { "rec",     ACCESS_RECORDER_VIEW },
  { "recedit", ACCESS_RECORDER_CHANGE },
  { "admin",   ACCESS_ADMIN },
  { "webui",   ACCESS_WEB_INTERFACE },
  { "access",  ACCESS_ADMIN_ACCESS },
};


/**
 * Access configuration
 */
int
ajax_config_access_tab(http_connection_t *hc, http_reply_t *hr)
{
  htsbuf_queue_t *tq = &hr->hr_q;

  if(access_verify(hc->hc_username, hc->hc_password,
		(struct sockaddr *)&hc->hc_tcp_session.tcp_peer_addr,
		   AJAX_ACCESS_ACCESSCTRL))
    return HTTP_STATUS_UNAUTHORIZED;

  htsbuf_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, "Access control");

  htsbuf_qprintf(tq, "<div id=\"alist\"></div>");
  
  ajax_js(tq,  
	  "new Ajax.Updater('alist', '/ajax/accesslist', "
	  "{method: 'get', evalScripts: true});");

  htsbuf_qprintf(tq, "<hr><div style=\"overflow: auto; width: 100%\">");

  htsbuf_qprintf(tq,
	      "<div style=\"height: 20px;\">"
	      "<div style=\"float: left; margin-right: 4px\">"
	      "<input type=\"text\" id=\"newuser\">"
	      "<input type=\"button\" value=\"Add\" "
	      "onClick=\"new Ajax.Request('/ajax/accessadd', "
	      "{parameters: {name: $F('newuser')}, method: 'post'})\">"
	      "</div>"
	      "</div>");

  htsbuf_qprintf(tq, "</div>\r\n");
  
  ajax_box_end(tq, AJAX_BOX_SIDEBOX);
  
  htsbuf_qprintf(tq, "</div>");
  http_output_html(hc, hr);
  return 0;
}

static void
ajax_access_checkbox(ajax_table_t *ta, access_entry_t *ae, int mask,
		     const char *n)
{
  ajax_table_cell(ta, NULL,
		  "<input %stype=\"checkbox\" class=\"nicebox\" "
		  "onChange=\"new Ajax.Request('/ajax/accesschange/%d', "
		  "{parameters: {entry: '%s', checked: this.checked}})\">",
		  ae->ae_rights & mask ? "checked " : "",
		  ae->ae_tally, n);
}


/**
 *
 */
static int
ajax_accesslist(http_connection_t *hc, http_reply_t *hr, 
		const char *remain, void *opaque)
{
  htsbuf_queue_t *tq = &hr->hr_q;
  access_entry_t *ae;
  ajax_table_t ta;
  char id[100];

  ajax_table_top(&ta, hc, tq,
		 (const char *[]){"User or Prefix", 
				    "Password",
				    "Streaming",
				    "Recorder",
				    "Recorder edit",
				    "Admin",
				    "Web UI",
				    "Accessedit",
				    "",
				    NULL}, 
		 (int[]){2, 2, 1, 1, 1, 1, 1, 1, 1, 1});

  TAILQ_FOREACH(ae, &access_entries, ae_link) {
    snprintf(id, sizeof(id), "%d", ae->ae_tally);
    ajax_table_row_start(&ta, id);

    ajax_table_cell(&ta, NULL, "%s", ae->ae_title);
    if(access_is_prefix(ae)) {
      ajax_table_cell(&ta, NULL, "%s", "n/a");
    } else {
      ajax_table_cell(&ta, "password",
		      "<a href=\"javascript:void(0)\" "
		      "onClick=\"makedivinput('password_%d', "
		      "'/ajax/accesssetpw/%d')\">"
		      "%s</a>",
		      ae->ae_tally, ae->ae_tally,
		      ae->ae_password != NULL ? "Change..." : "Set...");
    }

    ajax_access_checkbox(&ta, ae, ACCESS_STREAMING, "stream");
    ajax_access_checkbox(&ta, ae, ACCESS_RECORDER_VIEW, "rec");
    ajax_access_checkbox(&ta, ae, ACCESS_RECORDER_CHANGE, "recedit");

    ajax_access_checkbox(&ta, ae, ACCESS_ADMIN, "admin");
    ajax_access_checkbox(&ta, ae, ACCESS_WEB_INTERFACE, "webui");
    ajax_access_checkbox(&ta, ae, ACCESS_ADMIN_ACCESS, "access");
    

    ajax_table_cell(&ta, NULL,
		    "<a href=\"javascript:void(0)\" "
		    "onClick=\"dellistentry('/ajax/accessdel', '%d', '%s')\">"
		    "Delete...</a>",
		    ae->ae_tally, ae->ae_title);
  }

  ajax_table_bottom(&ta);
  http_output_html(hc, hr);
  return 0;
}


/**
 *
 */
static int
ajax_accessadd(http_connection_t *hc, http_reply_t *hr, 
	       const char *remain, void *opaque)
{
  htsbuf_queue_t *tq = &hr->hr_q;
  access_entry_t *ae;
  const char *t;

  if((t = http_arg_get(&hc->hc_req_args, "name")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  htsbuf_qprintf(tq, "$('newuser').clear();\r\n");

  if(t == NULL || strlen(t) < 1 || strchr(t, '\'') || strchr(t, '"')) {
    htsbuf_qprintf(tq, "alert('Invalid username');\r\n");
  } else {
    ae = access_add(t);
    if(ae == NULL) {
      htsbuf_qprintf(tq, "alert('Invalid prefix');\r\n");
    } else {
      htsbuf_qprintf(tq, 
		  "new Ajax.Updater('alist', '/ajax/accesslist', "
		"{method: 'get', evalScripts: true});\r\n");
    }
  }

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);

  return 0;
}

/**
 *
 */
static int
ajax_accesschange(http_connection_t *hc, http_reply_t *hr, 
		  const char *remain, void *opaque)
{
  //  htsbuf_queue_t *tq = &hr->hr_tq;
  access_entry_t *ae;
  const char *e, *c;
  int bit;

  if(remain == NULL || (ae = access_by_id(atoi(remain))) == NULL) 
    return HTTP_STATUS_BAD_REQUEST;

  if((e = http_arg_get(&hc->hc_req_args, "entry")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if((bit = str2val(e, accesstypetab)) < 0)
    return HTTP_STATUS_BAD_REQUEST;

  if((c = http_arg_get(&hc->hc_req_args, "checked")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if(!strcasecmp(c, "false")) {
    ae->ae_rights &= ~bit;
  } else if(!strcasecmp(c, "true")) {
    ae->ae_rights |= bit;
  } else {
    return HTTP_STATUS_BAD_REQUEST;
  }

  access_save();

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  return 0;
}


/**
 *
 */
static int
ajax_accessdel(http_connection_t *hc, http_reply_t *hr, 
		  const char *remain, void *opaque)
{
  htsbuf_queue_t *tq = &hr->hr_q;
  access_entry_t *ae;
  const char *e;

  if((e = http_arg_get(&hc->hc_req_args, "id")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if((ae = access_by_id(atoi(e))) == NULL) 
    return HTTP_STATUS_BAD_REQUEST;

  access_delete(ae);

  htsbuf_qprintf(tq, 
	      "new Ajax.Updater('alist', '/ajax/accesslist', "
	      "{method: 'get', evalScripts: true});\r\n");

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  return 0;
}



/**
 *
 */
static int
ajax_accesssetpw(http_connection_t *hc, http_reply_t *hr, 
		  const char *remain, void *opaque)
{
  htsbuf_queue_t *tq = &hr->hr_q;
  access_entry_t *ae;
  const char *e;

  if(remain == NULL || (ae = access_by_id(atoi(remain))) == NULL) 
    return HTTP_STATUS_BAD_REQUEST;

  if((e = http_arg_get(&hc->hc_req_args, "value")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  free(ae->ae_password);
  ae->ae_password = strdup(e);

  access_save();
  
  htsbuf_qprintf(tq, 
	      "$('password_%d').innerHTML= '"
	      "<a href=\"javascript:void(0)\" "
	      "onClick=\"makedivinput(\\'password_%d\\', "
	      "\\'/ajax/accesssetpw/%d\\')\">"
	      "%s</a>';",
	      ae->ae_tally, ae->ae_tally, ae->ae_tally, "Change...");

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  return 0;
}


/**
 *
 */
void
ajax_config_access_init(void)
{
  http_path_add("/ajax/accesslist" ,      NULL, ajax_accesslist,
		AJAX_ACCESS_ACCESSCTRL);
  http_path_add("/ajax/accessadd" ,       NULL, ajax_accessadd,
		AJAX_ACCESS_ACCESSCTRL);
  http_path_add("/ajax/accesschange" ,    NULL, ajax_accesschange,
		AJAX_ACCESS_ACCESSCTRL);
  http_path_add("/ajax/accessdel" ,       NULL, ajax_accessdel,
		AJAX_ACCESS_ACCESSCTRL);
  http_path_add("/ajax/accesssetpw" ,     NULL, ajax_accesssetpw,
		AJAX_ACCESS_ACCESSCTRL);
}
