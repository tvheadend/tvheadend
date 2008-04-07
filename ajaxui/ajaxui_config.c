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
#define AJAX_CONFIG_TAB_DVB      1
#define AJAX_CONFIG_TABS         2

const char *ajax_config_tabnames[] = {
  [AJAX_CONFIG_TAB_CHANNELS]      = "Channels & Groups",
  [AJAX_CONFIG_TAB_DVB]           = "DVB adapters",
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
    return ajax_config_channels_tab(hc);
  case AJAX_CONFIG_TAB_DVB:
    return ajax_config_dvb_tab(hc);

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
  http_path_add("/ajax/configmenu",          NULL, ajax_config_menu);
  http_path_add("/ajax/configtab",           NULL, ajax_config_dispatch);

  ajax_config_channels_init();
  ajax_config_dvb_init();
}
