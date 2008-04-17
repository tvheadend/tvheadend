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
  char buf[1000];
  th_transport_t *t;
  const char *v;
  int o = 1;
  th_stream_t *st;
  const char *extra;
  int displines = 21;
  int csize[10];

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

  ajax_table_header(hc, tq,
		    (const char *[])
		    {"", "ServiceID", "Crypto", "Type", "Source service",
		       "", "Target channel", "", NULL},
		    (int[]){3,5,4,4,12,3,12,1},
		    numtransports > displines,
		    csize);
  tcp_qprintf(tq, "<hr>");


  LIST_FOREACH(t, tlist, tht_tmp_link) {
    tcp_qprintf(tq, "<div%s>", o ? " style=\"background: #fff\"" : "");
    o = !o;

    tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");

    tcp_qprintf(tq, "<div style=\"float: left; width: %d%%\">"
		"<a id=\"toggle_svcinfo%s\" href=\"javascript:void(0)\" "
		"onClick=\"showhide('svcinfo%s')\" >"
		"More</a></div>",
		csize[0], t->tht_identifier, t->tht_identifier);

    tcp_qprintf(tq, "<div style=\"float: left; width: %d%%\">%d</div>",
		csize[1], t->tht_dvb_service_id);

    tcp_qprintf(tq, "<div style=\"float: left; width: %d%%\">%s</div>",
		csize[2], t->tht_scrambled ? "CSA" : "Free");

    tcp_qprintf(tq, "<div style=\"float: left; width: %d%%\">%s</div>",
		csize[3], transport_servicetype_txt(t));

    tcp_qprintf(tq, "<div style=\"float: left; width: %d%%\">%s</div>",
		csize[4], t->tht_servicename ?: "");

    tcp_qprintf(tq, 
		"<div style=\"float: left; width: %d%%; text-align: center\">"
		"<a href=\"javascript:void(0)\" "
		"onClick=\"new "
		"Ajax.Request('/ajax/transport_op/%s', "
		"{parameters: {action: 'toggle'}})\">"
		"<img id=\"map%s\" src=\"/gfx/%smapped.png\"></a></div>",
		csize[5],
		t->tht_identifier, t->tht_identifier,
		t->tht_channel ? "" : "un");

    tcp_qprintf(tq, "<div id=\"chname%s\" style=\"float: left; width: %d%%\">",
		t->tht_identifier, csize[6]);

    if(t->tht_channel == NULL) {
      /* Unmapped */

      v = t->tht_channelname;

      snprintf(buf, sizeof(buf), 
	       "tentative_chname('chname%s', "
	       "'/ajax/transport_rename_channel/%s', '%s')",
	       t->tht_identifier, t->tht_identifier, v);

      ajax_a_jsfunc(tq, v, buf, "");
    } else {
      /* Mapped */
      tcp_qprintf(tq, "%s", t->tht_channel->ch_name);
    }
    tcp_qprintf(tq, "</div>");



    tcp_qprintf(tq, "<div style=\"float: left; width: %d%\">"
		"<input id=\"sel_%s\" type=\"checkbox\" class=\"nicebox\">"
		"</div>", csize[7], t->tht_identifier);
    
    tcp_qprintf(tq, "</div>");

    /* Extra info */
    tcp_qprintf(tq, "<div id=\"svcinfo%s\" style=\"display: none\">",
		t->tht_identifier);

    tcp_qprintf(tq, "<p>");
      
    tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");

    tcp_qprintf(tq, "<div style=\"float: left; width: %d%%\">"
		"&nbsp;</div>", csize[0]);
    tcp_qprintf(tq, "<div class=\"pidheader\" style=\"width: %d%%\">"
		"PID</div>", csize[1]);

    tcp_qprintf(tq, "<div class=\"pidheader\" style=\"width: %d%%\">"
		"Payload</div>", csize[2] + csize[3]);

    tcp_qprintf(tq, "<div class=\"pidheader\" style=\"width: %d%%\">Details"
		"</div>",
		csize[4] + csize[5] + csize[6] + csize[7]);
    tcp_qprintf(tq, "</div>");


    LIST_FOREACH(st, &t->tht_streams, st_link) {
      tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");
      tcp_qprintf(tq, "<div style=\"float: left; width: %d%%\">&nbsp;</div>",
		  csize[0]);
      tcp_qprintf(tq, "<div style=\"float: left; width: %d%%\">%d</div>",
		  csize[1], st->st_pid);
      tcp_qprintf(tq, "<div style=\"float: left; width: %d%%\">%s</div>",
		  csize[2] + csize[3],
		  htstvstreamtype2txt(st->st_type));

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

      if(extra != NULL)
	tcp_qprintf(tq, "<div style=\"float: left; width: %d%%\">%s</div>",
		    csize[4] + csize[5] + csize[6] + csize[7],
		    extra);
      
      tcp_qprintf(tq, "</div>");
    }
    
    tcp_qprintf(tq, "</p></div>");
    tcp_qprintf(tq, "</div>\r\n");
    
  }
  tcp_qprintf(tq, "<hr>\r\n");

  tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");

  tcp_qprintf(tq, "<div class=\"infoprefix\">Select:</div><div>");

  ajax_a_jsfunc(tq, "All",                       "select_all();",      " / ");
  ajax_a_jsfunc(tq, "None",                      "select_none();",     " / ");
  ajax_a_jsfunc(tq, "Invert",                    "select_invert();",   " / ");
  ajax_a_jsfunc(tq, "All TV-services",           "select_tv();",       " / ");
  ajax_a_jsfunc(tq, "All uncrypted TV-services", "select_tv_nocrypt();", "");

  tcp_qprintf(tq, "</div></div>\r\n");
  
  tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");
  tcp_qprintf(tq, "<div class=\"infoprefix\">&nbsp;</div><div>");

  ajax_a_jsfunc(tq, "Map selected",    "selected_do('map');",   " / ");
  ajax_a_jsfunc(tq, "Unmap selected",  "selected_do('unmap');", "");

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
  const char *newname, *v;
  tcp_queue_t *tq = &hr->hr_tq;
  char buf[1000];

  if(remain == NULL || (t = transport_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if((newname = http_arg_get(&hc->hc_req_args, "newname")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  free((void *)t->tht_channelname);
  t->tht_channelname = strdup(newname);

  v = newname;

  snprintf(buf, sizeof(buf), 
	   "tentative_chname('chname%s', "
	   "'/ajax/transport_rename_channel/%s', '%s')",
	   t->tht_identifier, t->tht_identifier, v);
  
  ajax_a_jsfunc(tq, v, buf, "");
      
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
	      "$('chname%s').innerHTML='%s';\n\r"
	      "$('map%s').src='/gfx/mapped.png';\n\r",
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
	      "$('chname%s').innerHTML='"
	      "<a href=\"javascript:void(0)\" "
	      "onClick=\"javascript:tentative_chname(\\'chname%s\\', "
	      "\\'/ajax/transport_rename_channel/%s\\', \\'%s\\')\">%s</a>"
	      "';\n\r"
	      "$('map%s').src='/gfx/unmapped.png';\n\r",
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
		ajax_transport_rename_channel);

  http_path_add("/ajax/transport_op", NULL,
		ajax_transport_op);

  http_path_add("/ajax/transport_chdisable", NULL,
		ajax_transport_chdisable);

}
