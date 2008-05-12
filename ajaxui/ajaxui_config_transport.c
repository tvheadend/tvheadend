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
#include "serviceprobe.h"


/**
 * 
 */
int
ajax_transport_build_list(http_connection_t *hc, tcp_queue_t *tq,
			  struct th_transport_list *tlist, int numtransports)
{
  th_transport_t *t;
  ajax_table_t ta;

  tcp_qprintf(tq, "<script type=\"text/javascript\">\r\n"
	      "//<![CDATA[\r\n");
  
  /* Select all */
  tcp_qprintf(tq, "select_all = function() {\r\n");
  LIST_FOREACH(t, tlist, tht_tmp_link) {
    tcp_qprintf(tq, 
		"$('sel_%s').checked = true;\r\n",
		t->tht_identifier);
  }
  tcp_qprintf(tq, "}\r\n");

  /* Select none */
  tcp_qprintf(tq, "select_none = function() {\r\n");
  LIST_FOREACH(t, tlist, tht_tmp_link) {
    tcp_qprintf(tq, 
		"$('sel_%s').checked = false;\r\n",
		t->tht_identifier);
  }
  tcp_qprintf(tq, "}\r\n");

  /* Invert selection */
  tcp_qprintf(tq, "select_invert = function() {\r\n");
  LIST_FOREACH(t, tlist, tht_tmp_link) {
    tcp_qprintf(tq, 
		"$('sel_%s').checked = !$('sel_%s').checked;\r\n",
		t->tht_identifier, t->tht_identifier);
  }
  tcp_qprintf(tq, "}\r\n");

  /* Select TV transports */
  tcp_qprintf(tq, "select_tv = function() {\r\n");
  LIST_FOREACH(t, tlist, tht_tmp_link) {
    tcp_qprintf(tq, 
		"$('sel_%s').checked = %s;\r\n",
		t->tht_identifier, 
		transport_is_tv(t) ? "true" : "false");
  }
  tcp_qprintf(tq, "}\r\n");

  /* Select unscrambled TV transports */
  tcp_qprintf(tq, "select_tv_nocrypt = function() {\r\n");
  LIST_FOREACH(t, tlist, tht_tmp_link) {
    tcp_qprintf(tq, 
		"$('sel_%s').checked = %s;\r\n",
		t->tht_identifier, 
		transport_is_tv(t) && !t->tht_scrambled ? "true" : "false");
  }
  tcp_qprintf(tq, "}\r\n");

  /* Perform the given op on all transprots */
  tcp_qprintf(tq, "selected_do = function(op) {\r\n"
	      "var h = new Hash();\r\n"
	      );

  LIST_FOREACH(t, tlist, tht_tmp_link) {
    tcp_qprintf(tq, 
		"if($('sel_%s').checked) {h.set('%s', 'selected') }\r\n",
		t->tht_identifier, t->tht_identifier);
  }

  tcp_qprintf(tq, " new Ajax.Request('/ajax/transport_op/' + op, "
	      "{parameters: h});\r\n");
  tcp_qprintf(tq, "}\r\n");

  tcp_qprintf(tq, 
	      "\r\n//]]>\r\n"
	      "</script>\r\n");

  /* Top */

  tcp_qprintf(tq, "<form id=\"transports\">");

  ajax_table_top(&ta, hc, tq,
		 (const char *[]){"Last status", "Crypto",
				    "Type", "Source service",
				    "", "Target channel", "", NULL},
		 (int[]){8,4,4,12,3,12,1});

  LIST_FOREACH(t, tlist, tht_tmp_link) {
    ajax_table_row_start(&ta, t->tht_identifier);

    ajax_table_cell(&ta, "status", 
		    transport_status_to_text(t->tht_last_status));
    ajax_table_cell(&ta, NULL, "%s", t->tht_scrambled ? "Yes" : "No");
    ajax_table_cell(&ta, NULL, "%s", transport_servicetype_txt(t));
    ajax_table_cell(&ta, NULL, "%s", t->tht_svcname ?: "");

    ajax_table_cell(&ta, NULL, 
		    "<a href=\"javascript:void(0)\" "
		    "onClick=\"new Ajax.Request('/ajax/transport_op/toggle', "
		    "{parameters: {'%s': 'selected'}})\">"
		    "<img id=\"map_%s\" src=\"/gfx/%smapped.png\"></a>",
		    t->tht_identifier, t->tht_identifier,
		    t->tht_ch ? "" : "un");

    if(t->tht_ch == NULL) {
      /* Unmapped */
      ajax_table_cell(&ta, "chname", 
		      "<a href=\"javascript:void(0)\" "
		      "onClick=\"tentative_chname('chname%s', "
		      "'/ajax/transport_rename_channel/%s', '%s')\">"
		      "%s</a>",
		      t->tht_identifier, t->tht_identifier,
		      t->tht_chname, t->tht_chname);
    } else {
      ajax_table_cell(&ta, "chname", "%s", t->tht_ch->ch_name);
    }

    ajax_table_cell_checkbox(&ta);
  }

  ajax_table_bottom(&ta);

  tcp_qprintf(tq, "<hr>\r\n");

  tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");

  ajax_button(tq, "Select all",  "select_all()");
  ajax_button(tq, "Select none",  "select_none()");

  //  tcp_qprintf(tq, "</div>\r\n");
  //tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");

  ajax_button(tq, "Map selected",   "selected_do('map');");
  ajax_button(tq, "Unmap selected", "selected_do('unmap');");
  ajax_button(tq, "Test and map selected", "selected_do('probe');");
  tcp_qprintf(tq, "</div>");

  tcp_qprintf(tq, "</form>");
  return 0;
}

/**
 *  Rename of unmapped channel
 */
static int
ajax_transport_rename_channel(http_connection_t *hc, http_reply_t *hr,
			      const char *remain, void *opaque)
{
  th_transport_t *t;
  const char *newname;
  tcp_queue_t *tq = &hr->hr_tq;

  if(remain == NULL || (t = transport_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if((newname = http_arg_get(&hc->hc_req_args, "newname")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  free((void *)t->tht_chname);
  t->tht_chname = strdup(newname);

  ajax_a_jsfuncf(tq, newname,
		 "tentative_chname('chname_%s', "
		 "'/ajax/transport_rename_channel/%s', '%s')",
		 t->tht_identifier, t->tht_identifier, newname);
       
  http_output_html(hc, hr);
  t->tht_config_change(t);
  return 0;
}


/**
 *
 */
void
ajax_transport_build_mapper_state(char *buf, size_t siz, th_transport_t *t,
				  int mapped)
{
  if(mapped) {
    snprintf(buf, siz,
	     "$('chname_%s').innerHTML='%s';\n\r"
	     "$('map_%s').src='/gfx/mapped.png';\n\r",
		t->tht_identifier, t->tht_ch->ch_name,
		t->tht_identifier);
  } else {
    snprintf(buf, siz,
	     "$('chname_%s').innerHTML='"
	     "<a href=\"javascript:void(0)\" "
	     "onClick=\"javascript:tentative_chname(\\'chname_%s\\', "
	     "\\'/ajax/transport_rename_channel/%s\\', \\'%s\\')\">%s</a>"
	     "';\n\r"
	     "$('map_%s').src='/gfx/unmapped.png';\n\r",
	     t->tht_identifier, t->tht_identifier, t->tht_identifier,
	     t->tht_chname, t->tht_chname, t->tht_identifier);
  }
}


/**
 *
 */
static void
ajax_map_unmap_channel(th_transport_t *t, tcp_queue_t *tq, int map)
{
  char buf[1000];

  if(map)
    transport_map_channel(t, NULL);
  else
    transport_unmap_channel(t);

  ajax_transport_build_mapper_state(buf, sizeof(buf), t, map);
  tcp_qprintf(tq, "%s", buf);
}


/**
 *
 */
static int
ajax_transport_op(http_connection_t *hc, http_reply_t *hr, 
		  const char *remain, void *opaque)
{
  th_transport_t *t;
  tcp_queue_t *tq = &hr->hr_tq;
  const char *op = remain;
  http_arg_t *ra;

  if(op == NULL)
    return HTTP_STATUS_NOT_FOUND;
  
  TAILQ_FOREACH(ra, &hc->hc_req_args, link) {
    if(strcmp(ra->val, "selected"))
      continue;

    if((t = transport_find_by_identifier(ra->key)) == NULL)
      continue;

    if(!strcmp(op, "toggle")) {
      ajax_map_unmap_channel(t, tq, t->tht_ch ? 0 : 1);
    } else if(!strcmp(op, "map") && t->tht_ch == NULL) {
      ajax_map_unmap_channel(t, tq, 1);
    } else if(!strcmp(op, "unmap") && t->tht_ch != NULL) {
      ajax_map_unmap_channel(t, tq, 0);
    } else if(!strcmp(op, "probe")) {
      serviceprobe_add(t);
      continue;
    }
    t->tht_config_change(t);
  }

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);

  return 0;
}


/**
 *
 */
static int
ajax_transport_chdisable(http_connection_t *hc, http_reply_t *hr, 
			 const char *remain, void *opaque)
{
  th_transport_t *t;
  const char *s;

  if(remain == NULL || (t = transport_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if((s = http_arg_get(&hc->hc_req_args, "enabled")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  t->tht_disabled = !strcasecmp(s, "false");
  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  t->tht_config_change(t);
  return 0;
}


/**
 *
 */
void
ajax_config_transport_init(void)
{
  http_path_add("/ajax/transport_rename_channel", NULL,
		ajax_transport_rename_channel,
		AJAX_ACCESS_CONFIG);

  http_path_add("/ajax/transport_op", NULL,
		ajax_transport_op,
		AJAX_ACCESS_CONFIG);

  http_path_add("/ajax/transport_chdisable", NULL,
		ajax_transport_chdisable,
		AJAX_ACCESS_CONFIG);

}
