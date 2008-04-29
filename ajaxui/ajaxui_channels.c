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
#include "epg.h"

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


static void
ajax_output_event(tcp_queue_t *tq, event_t *e, int flags, int color)
{
  struct tm a, b;
  time_t stop;

  tcp_qprintf(tq, "<div class=\"fullrow\"%s>",
	      color ? "style=\"background: #fff\" " : "");

  localtime_r(&e->e_start, &a);
  stop = e->e_start + e->e_duration;
  localtime_r(&stop, &b);
  
  tcp_qprintf(tq, 
	      "<div class=\"compact\" style=\"width: 35%%\">"
	      "%02d:%02d-%02d:%02d"
	      "</div>",
	      a.tm_hour, a.tm_min, b.tm_hour, b.tm_min);
 
  tcp_qprintf(tq, 
	      "<div class=\"compact\" style=\"width: 65%%\">"
	      "%s"
	      "</div>",
	      e->e_title);
    
  tcp_qprintf(tq, "</div>");
}


static void
ajax_list_events(tcp_queue_t *tq, th_channel_t *ch, int lines)
{
  event_t *e;
  int i;


  e = epg_event_find_current_or_upcoming(ch);

  for(i = 0; i < 3 && e != NULL; i++) {
    ajax_output_event(tq, e, 0, !(i & 1));
    e = TAILQ_NEXT(e, e_link);
  }
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
  
  tcp_queue_t *tq = &hr->hr_tq;
  th_channel_t *ch;
  th_channel_group_t *tcg;
  char dispname[20];

  if(remain == NULL || (tcg = channel_group_by_tag(atoi(remain))) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link) {
    if(LIST_FIRST(&ch->ch_transports) == NULL)
      continue;

    tcp_qprintf(tq, "<div style=\"float:left; width: 25%%\">");

    snprintf(dispname, sizeof(dispname), "%s", ch->ch_name);
    strcpy(dispname + sizeof(dispname) - 4, "...");

    ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, dispname);

    tcp_qprintf(tq, 
		"<div style=\"width: 100%%; overflow: hidden; height:36px\">");

    tcp_qprintf(tq, 
		"<div style=\"float: left; height:32px; width:32px; "
		"margin: 2px\">");

    if(ch->ch_icon != NULL) {
      tcp_qprintf(tq, "<img src=\"%s\" style=\"width:32px\">",
		  ch->ch_icon);
    }

    tcp_qprintf(tq, "</div>");
    tcp_qprintf(tq, "</div>");


    tcp_qprintf(tq, "<div id=\"events%d\" style=\"height:42px\">", ch->ch_tag);
    ajax_list_events(tq, ch, 3);
    tcp_qprintf(tq, "</div>");

    ajax_box_end(tq, AJAX_BOX_SIDEBOX);
    tcp_qprintf(tq, "</div>");
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
  http_path_add("/ajax/channelgroupmenu",    NULL, ajax_channelgroup_menu,
		ACCESS_WEB_INTERFACE);
  http_path_add("/ajax/channelgrouptab",     NULL, ajax_channel_tab,
		ACCESS_WEB_INTERFACE);

}
