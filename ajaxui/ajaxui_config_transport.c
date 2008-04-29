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

#include <linux/dvb/frontend.h>

#include "tvhead.h"
#include "http.h"
#include "ajaxui.h"
#include "channels.h"
#include "psi.h"
#include "transports.h"


/**
 * 
 */
int
ajax_transport_build_list(http_connection_t *hc, tcp_queue_t *tq,
			  struct th_transport_list *tlist, int numtransports)
{
  th_transport_t *t;
  th_stream_t *st;
  const char *extra;
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
  tcp_qprintf(tq, "selected_do = function(op) {\r\n");
  LIST_FOREACH(t, tlist, tht_tmp_link) {
    tcp_qprintf(tq, 
		"if($('sel_%s').checked) {\r\n"
		"  new Ajax.Request('/ajax/transport_op/%s', "
		"{parameters: {action: op}});\r\n}\r\n",
		t->tht_identifier, t->tht_identifier);
  }
  tcp_qprintf(tq, "}\r\n");


  tcp_qprintf(tq, 
	      "\r\n//]]>\r\n"
	      "</script>\r\n");

  /* Top */

  tcp_qprintf(tq, "<form id=\"transports\">");

  ajax_table_top(&ta, hc, tq,
		 (const char *[]){"", "ServiceID", "Crypto",
				    "Type", "Source service",
				    "", "Target channel", "", NULL},
		 (int[]){3,5,4,4,12,3,12,1});

  LIST_FOREACH(t, tlist, tht_tmp_link) {
    ajax_table_row_start(&ta, t->tht_identifier);
    ajax_table_cell_detail_toggler(&ta);

    ajax_table_cell(&ta, NULL, "%d", t->tht_dvb_service_id);
    ajax_table_cell(&ta, NULL, "%s", t->tht_scrambled ? "Yes" : "No");
    ajax_table_cell(&ta, NULL, "%s", transport_servicetype_txt(t));
    ajax_table_cell(&ta, NULL, "%s", t->tht_servicename ?: "");

    ajax_table_cell(&ta, NULL, 
		    "<a href=\"javascript:void(0)\" "
		    "onClick=\"new Ajax.Request('/ajax/transport_op/%s', "
		    "{parameters: {action: 'toggle'}})\">"
		    "<img id=\"map_%s\" src=\"/gfx/%smapped.png\"></a>",
		    t->tht_identifier, t->tht_identifier,
		    t->tht_channel ? "" : "un");

    if(t->tht_channel == NULL) {
      /* Unmapped */
      ajax_table_cell(&ta, "chname", 
		      "<a href=\"javascript:void(0)\" "
		      "onClick=\"tentative_chname('chname%s', "
		      "'/ajax/transport_rename_channel/%s', '%s')\">"
		      "%s</a>",
		      t->tht_identifier, t->tht_identifier,
		      t->tht_channelname, t->tht_channelname);
    } else {
      ajax_table_cell(&ta, "chname", "%s", t->tht_channel->ch_name);
    }

    ajax_table_cell_checkbox(&ta);

    ajax_table_details_start(&ta);

    ajax_table_subrow_start(&ta);

    ajax_table_cell(&ta, NULL, NULL);
    ajax_table_cell(&ta, NULL, "PID");
    ajax_table_cell(&ta, NULL, NULL);
    ajax_table_cell(&ta, NULL, NULL);
    ajax_table_cell(&ta, NULL, "Payload");
    ajax_table_cell(&ta, NULL, NULL);
    ajax_table_cell(&ta, NULL, "Details");
    ajax_table_subrow_end(&ta);

    LIST_FOREACH(st, &t->tht_streams, st_link) {

      ajax_table_subrow_start(&ta);
      
      ajax_table_cell(&ta, NULL, NULL);
      ajax_table_cell(&ta, NULL, "%d", st->st_pid);
      ajax_table_cell(&ta, NULL, NULL);
      ajax_table_cell(&ta, NULL, NULL);
      ajax_table_cell(&ta, NULL, "%s", htstvstreamtype2txt(st->st_type));
  
      switch(st->st_type) {
      case HTSTV_MPEG2AUDIO:
      case HTSTV_AC3:
	extra = st->st_lang;
	break;
      case HTSTV_CA:
	extra = psi_caid2name(st->st_caid);
	break;
      default:
	extra = NULL;
	break;
      }

      ajax_table_cell(&ta, NULL, NULL);
      ajax_table_cell(&ta, NULL, extra);
      ajax_table_subrow_end(&ta);
    }
    ajax_table_details_end(&ta);
  }

  ajax_table_bottom(&ta);

  tcp_qprintf(tq, "<hr>\r\n");

  tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");

  tcp_qprintf(tq, "<div class=\"infoprefix\">Select:</div><div>");

  ajax_a_jsfuncf(tq, "All",                       "select_all();");
  tcp_qprintf(tq, " / ");
  ajax_a_jsfuncf(tq, "None",                      "select_none();");
  tcp_qprintf(tq, " / ");
  ajax_a_jsfuncf(tq, "Invert",                    "select_invert();");
  tcp_qprintf(tq, " / ");
  ajax_a_jsfuncf(tq, "All TV-services",           "select_tv();");
  tcp_qprintf(tq, " / ");
  ajax_a_jsfuncf(tq, "All uncrypted TV-services", "select_tv_nocrypt();");

  tcp_qprintf(tq, "</div></div>\r\n");
  
  tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");
  tcp_qprintf(tq, "<div class=\"infoprefix\">&nbsp;</div><div>");

  ajax_a_jsfuncf(tq, "Map selected",    "selected_do('map');");
  tcp_qprintf(tq, " / ");
  ajax_a_jsfuncf(tq, "Unmap selected",  "selected_do('unmap');");

  tcp_qprintf(tq, "</div></div>");

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

  free((void *)t->tht_channelname);
  t->tht_channelname = strdup(newname);

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
static void
dvb_map_channel(th_transport_t *t, tcp_queue_t *tq)
{
  transport_set_channel(t, t->tht_channelname);

  printf("Mapped transport %s to channel %s\n",
	 t->tht_servicename, t->tht_channel->ch_name);

  tcp_qprintf(tq, 
	      "$('chname_%s').innerHTML='%s';\n\r"
	      "$('map_%s').src='/gfx/mapped.png';\n\r",
	      t->tht_identifier, t->tht_channel->ch_name,
	      t->tht_identifier);
}


/**
 *
 */
static void
dvb_unmap_channel(th_transport_t *t, tcp_queue_t *tq)
{
  transport_unset_channel(t);

  printf("Unmapped transport %s\n", t->tht_servicename);

  tcp_qprintf(tq, 
	      "$('chname_%s').innerHTML='"
	      "<a href=\"javascript:void(0)\" "
	      "onClick=\"javascript:tentative_chname(\\'chname_%s\\', "
	      "\\'/ajax/transport_rename_channel/%s\\', \\'%s\\')\">%s</a>"
	      "';\n\r"
	      "$('map_%s').src='/gfx/unmapped.png';\n\r",
	      t->tht_identifier, t->tht_identifier, t->tht_identifier,
	      t->tht_channelname, t->tht_channelname, t->tht_identifier);
}



/**
 *
 */
int
ajax_transport_op(http_connection_t *hc, http_reply_t *hr, 
		  const char *remain, void *opaque)
{
  th_transport_t *t;
  tcp_queue_t *tq = &hr->hr_tq;
  const char *op;

  if(remain == NULL || (t = transport_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if((op = http_arg_get(&hc->hc_req_args, "action")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if(!strcmp(op, "toggle")) {
    if(t->tht_channel)
      dvb_unmap_channel(t, tq);
    else
      dvb_map_channel(t, tq);
  } else if(!strcmp(op, "map") && t->tht_channel == NULL) {
    dvb_map_channel(t, tq);
  } else if(!strcmp(op, "unmap") && t->tht_channel != NULL) {
    dvb_unmap_channel(t, tq);
  }

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);

  t->tht_config_change(t);
  return 0;
}


/**
 *
 */
int
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
