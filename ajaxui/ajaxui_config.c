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
#include "channels.h"


#define AJAX_CONFIG_TAB_CHANNELS 0
#define AJAX_CONFIG_TAB_ADAPTERS 1
#define AJAX_CONFIG_TABS         2

const char *ajax_config_tabnames[] = {
  [AJAX_CONFIG_TAB_CHANNELS]      = "Channels & Groups",
  [AJAX_CONFIG_TAB_ADAPTERS]      = "DVB adapters",
};


/*
 * Titlebar AJAX page
 */
static int
ajax_config_menu(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  
  if(remain == NULL)
    return HTTP_STATUS_NOT_FOUND;

  tcp_init_queue(&tq, -1);
  ajax_menu_bar_from_array(&tq, "config", 
			   ajax_config_tabnames, AJAX_CONFIG_TABS,
			   atoi(remain));
  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}

/**
 * Render a channel group widget
 */
static void
ajax_chgroup_build(tcp_queue_t *tq, th_channel_group_t *tcg)
{
  tcp_qprintf(tq, "<li id=\"chgrp_%d\">", tcg->tcg_tag);
  
  ajax_box_begin(tq, AJAX_BOX_BORDER, NULL, NULL, NULL);
  
  tcp_qprintf(tq,
	      "<div %s>%s</div>",
	      tcg == defgroup ? "" : "style=\"float: left\" ",
	      tcg->tcg_name);

  if(tcg != defgroup) {
    tcp_qprintf(tq,
		"<div class=\"chgroupaction\">"
		"<a href=\"javascript:void(0)\" "
		"onClick=\"dellistentry('/ajax/chgroup_del','%d', '%s');\""
		">Delete</a></div>",
		tcg->tcg_tag, tcg->tcg_name);
  }
  

  ajax_box_end(tq, AJAX_BOX_BORDER);
  tcp_qprintf(tq, "</li>");
}

/**
 * Update order of channel groups
 */
static int
ajax_chgroup_updateorder(http_connection_t *hc, const char *remain,
			 void *opaque)
{
  th_channel_group_t *tcg;
  tcp_queue_t tq;
  http_arg_t *ra;

  tcp_init_queue(&tq, -1);

  TAILQ_FOREACH(ra, &hc->hc_req_args, link) {
    if(strcmp(ra->key, "channelgrouplist[]") ||
       (tcg = channel_group_by_tag(atoi(ra->val))) == NULL)
      continue;
    
    TAILQ_REMOVE(&all_channel_groups, tcg, tcg_global_link);
    TAILQ_INSERT_TAIL(&all_channel_groups, tcg, tcg_global_link);
  }

  tcp_qprintf(&tq, "<span id=\"updatedok\">Updated on server</span>");
  ajax_js(&tq, "Effect.Fade('updatedok')");
  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}



/**
 * Add a new channel group
 */
static int
ajax_chgroup_add(http_connection_t *hc, const char *remain, void *opaque)
{
  th_channel_group_t *tcg;
  tcp_queue_t tq;
  const char *name;
  
  tcp_init_queue(&tq, -1);

  if((name = http_arg_get(&hc->hc_req_args, "name")) != NULL) {

    TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link)
      if(!strcmp(name, tcg->tcg_name))
	break;

    if(tcg == NULL) {
      tcg = channel_group_find(name, 1);

      ajax_chgroup_build(&tq, tcg);

      /* We must recreate the Sortable object */

      ajax_js(&tq, "Sortable.destroy(\"channelgrouplist\")");

      ajax_js(&tq, "Sortable.create(\"channelgrouplist\", "
	      "{onUpdate:function(){updatelistonserver("
	      "'channelgrouplist', "
	      "'/ajax/chgroup_updateorder', "
	      "'list-info'"
	      ")}});");
    }
  }
  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}



/**
 * Delete a channel group
 */
static int
ajax_chgroup_del(http_connection_t *hc, const char *remain, void *opaque)
{
  th_channel_group_t *tcg;
  tcp_queue_t tq;
  const char *id;
  
  if((id = http_arg_get(&hc->hc_req_args, "id")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if((tcg = channel_group_by_tag(atoi(id))) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  tcp_init_queue(&tq, -1);
  tcp_qprintf(&tq, "$('chgrp_%d').remove();", tcg->tcg_tag);
  http_output_queue(hc, &tq, "text/javascript; charset=UTF-8", 0);

  channel_group_destroy(tcg);

  return 0;
}



/**
 * Channel group & channel configuration
 */
static int
ajax_channel_config_tab(http_connection_t *hc)
{
  tcp_queue_t tq;
  th_channel_group_t *tcg;

  tcp_init_queue(&tq, -1);
  
  tcp_qprintf(&tq, "<div style=\"float: left; width: 400px\">");

  ajax_box_begin(&tq, AJAX_BOX_SIDEBOX, "channelgroups",
		 NULL, "Channel groups");

  tcp_qprintf(&tq, "<div style=\"height:15px; text-align:center\" "
	      "id=\"list-info\"></div>");
   
  tcp_qprintf(&tq, "<ul id=\"channelgrouplist\" class=\"draglist\">");

  TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {
    if(tcg->tcg_hidden)
      continue;
    ajax_chgroup_build(&tq, tcg);
  }

  tcp_qprintf(&tq, "</ul>");
 
  ajax_js(&tq, "Sortable.create(\"channelgrouplist\", "
	  "{onUpdate:function(){updatelistonserver("
	  "'channelgrouplist', "
	  "'/ajax/chgroup_updateorder', "
	  "'list-info'"
	  ")}});");

  /**
   * Add new group
   */
  ajax_box_begin(&tq, AJAX_BOX_BORDER, NULL, NULL, NULL);

  tcp_qprintf(&tq,
	      "<div style=\"height: 20px\">"
	      "<div style=\"float: left\">"
	      "<input class=\"textinput\" type=\"text\" id=\"newchgrp\">"
	      "</div>"
	      "<div style=\"padding-top:2px\" class=\"chgroupaction\">"
	      "<a href=\"javascript:void(0)\" "
	      "onClick=\"javascript:addlistentry_by_widget("
	      "'channelgrouplist', 'chgroup_add', 'newchgrp');\">"
	      "Add</a></div>"
	      "</div>");
    
  ajax_box_end(&tq, AJAX_BOX_BORDER);
    
  ajax_box_end(&tq, AJAX_BOX_SIDEBOX);
  tcp_qprintf(&tq, "</div>");
  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}



/*
 * Tab AJAX page
 *
 * Switch to different tabs
 */
static int
ajax_config_dispatch(http_connection_t *hc, const char *remain, void *opaque)
{
  int tab;

  if(remain == NULL)
    return HTTP_STATUS_NOT_FOUND;

  tab = atoi(remain);

  switch(tab) {
  case AJAX_CONFIG_TAB_CHANNELS:
    return ajax_channel_config_tab(hc);

  default:
    return HTTP_STATUS_NOT_FOUND;
  }
  return 0;
}




/*
 * Config root menu AJAX page
 *
 * This is the top level menu for this c-file
 */
int
ajax_config_tab(http_connection_t *hc)
{
  tcp_queue_t tq;

  tcp_init_queue(&tq, -1);

  ajax_box_begin(&tq, AJAX_BOX_FILLED, "configmenu", NULL, NULL);
  ajax_box_end(&tq, AJAX_BOX_FILLED);

  tcp_qprintf(&tq, "<div id=\"configdeck\"></div>");

  tcp_qprintf(&tq,
	      "<script type=\"text/javascript\">"
	      "switchtab('config', '0')"
	      "</script>");

  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}



/**
 *
 */
void
ajax_config_init(void)
{
  http_path_add("/ajax/chgroup_add"        , NULL, ajax_chgroup_add);
  http_path_add("/ajax/chgroup_del"        , NULL, ajax_chgroup_del);
  http_path_add("/ajax/chgroup_updateorder", NULL, ajax_chgroup_updateorder);
  http_path_add("/ajax/configmenu",          NULL, ajax_config_menu);
  http_path_add("/ajax/configtab",           NULL, ajax_config_dispatch);
}
