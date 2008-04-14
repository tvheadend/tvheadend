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


static void
ajax_channelgroupmenu_content(tcp_queue_t *tq, int current)
{
  th_channel_group_t *tcg;

  tcp_qprintf(tq, "<ul class=\"menubar\">");

  TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {
    if(tcg->tcg_hidden)
      continue;

    if(current < 1)
      current = tcg->tcg_tag;

    tcp_qprintf(tq,
		"<li%s>"
		"<a href=\"javascript:switchtab('channelgroup', '%d')\">%s</a>"
		"</li>",
		current == tcg->tcg_tag ? " style=\"font-weight:bold;\"" : "",
		tcg->tcg_tag, tcg->tcg_name);
  }
  tcp_qprintf(tq, "</ul>");  
}


/*
 * Channelgroup menu bar
 */
static int
ajax_channelgroup_menu(http_connection_t *hc, http_reply_t *hr, 
		       const char *remain, void *opaque)
{
  tcp_queue_t *tq = &hr->hr_tq;
  
  if(remain == NULL)
    return HTTP_STATUS_NOT_FOUND;

  ajax_channelgroupmenu_content(tq, atoi(remain));
  http_output_html(hc, hr);
  return 0;
}





/*
 * Display a list of all channels within the given group
 *
 * Group is given by 'tag' as an ASCII string in remain
 */
int
ajax_channel_tab(http_connection_t *hc, http_reply_t *hr,
		 const char *remain, void *opaque)
{
  //  tcp_queue_t *tq = &hr->hr_tq;
  th_channel_t *ch;
  th_channel_group_t *tcg;

  if(remain == NULL)
    return HTTP_STATUS_NOT_FOUND;
  
  if((tcg = channel_group_by_tag(atoi(remain))) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link) {
    if(LIST_FIRST(&ch->ch_transports) == NULL)
      continue;

#if 0
    ajax_box_begin(&tq, "x", ch->ch_name);

    tcp_qprintf(&tq, "<div style=\"height: 100px\"></div>");
    ajax_sidebox_end(&tq);
#endif
  }

  http_output_html(hc, hr);
  return 0;
}




/*
 * Channel (group)s AJAX page
 *
 * This is the top level menu for this c-file
 */
int
ajax_channelgroup_tab(http_connection_t *hc, http_reply_t *hr)
{
  tcp_queue_t *tq = &hr->hr_tq;

  ajax_box_begin(tq, AJAX_BOX_FILLED, "channelgroupmenu", NULL, NULL);
  ajax_box_end(tq, AJAX_BOX_FILLED);

  tcp_qprintf(tq, "<div id=\"channelgroupdeck\"></div>");

  tcp_qprintf(tq,
	      "<script type=\"text/javascript\">"
	      "switchtab('channelgroup', '0')"
	      "</script>");

  http_output_html(hc, hr);
  return 0;
}



/**
 *
 */
void
ajax_channels_init(void)
{
  http_path_add("/ajax/channelgroupmenu",    NULL, ajax_channelgroup_menu);
  http_path_add("/ajax/channelgrouptab",     NULL, ajax_channel_tab);
}
