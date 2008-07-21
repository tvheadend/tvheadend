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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
ajax_channelgroupmenu_content(htsbuf_queue_t *tq, int current)
{
  channel_group_t *tcg;

  htsbuf_qprintf(tq, "<ul class=\"menubar\">");

  TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {
    if(tcg->tcg_hidden)
      continue;

    if(current < 1)
      current = tcg->tcg_tag;

    htsbuf_qprintf(tq,
		"<li%s>"
		"<a href=\"javascript:switchtab('channelgroup', '%d')\">%s</a>"
		"</li>",
		current == tcg->tcg_tag ? " style=\"font-weight:bold;\"" : "",
		tcg->tcg_tag, tcg->tcg_name);
  }
  htsbuf_qprintf(tq, "</ul>");  
}


/*
 * Channelgroup menu bar
 */
static int
ajax_channelgroup_menu(http_connection_t *hc, http_reply_t *hr, 
		       const char *remain, void *opaque)
{
  htsbuf_queue_t *tq = &hr->hr_q;
  
  if(remain == NULL)
    return HTTP_STATUS_NOT_FOUND;

  ajax_channelgroupmenu_content(tq, atoi(remain));
  http_output_html(hc, hr);
  return 0;
}


static void
ajax_output_event(htsbuf_queue_t *tq, event_t *e, int flags, int color)
{
  struct tm a, b;
  time_t stop;

  htsbuf_qprintf(tq, "<div class=\"fullrow\"%s>",
	      color ? "style=\"background: #fff\" " : "");

  localtime_r(&e->e_start, &a);
  stop = e->e_start + e->e_duration;
  localtime_r(&stop, &b);
  
  htsbuf_qprintf(tq, 
	      "<div class=\"compact\" style=\"width: 35%%\">"
	      "%02d:%02d-%02d:%02d"
	      "</div>",
	      a.tm_hour, a.tm_min, b.tm_hour, b.tm_min);
 
  htsbuf_qprintf(tq, 
	      "<div class=\"compact\" style=\"width: 65%%\">"
	      "%s"
	      "</div>",
	      e->e_title);
    
  htsbuf_qprintf(tq, "</div>");
}


static void
ajax_list_events(htsbuf_queue_t *tq, channel_t *ch, int lines)
{
  event_t *e;
  int i;

  e = epg_event_find_current_or_upcoming(ch);

  for(i = 0; i < 3 && e != NULL; i++) {
    ajax_output_event(tq, e, 0, !(i & 1));
    e = TAILQ_NEXT(e, e_channel_link);
  }
}


/*
 * Display a list of all channels within the given group
 *
 * Group is given by 'tag' as an ASCII string in remain
 */
static int
ajax_channel_tab(http_connection_t *hc, http_reply_t *hr,
		 const char *remain, void *opaque)
{
  
  htsbuf_queue_t *tq = &hr->hr_q;
  channel_t *ch;
  channel_group_t *tcg;
  char dispname[20];
  struct sockaddr_in *si;
  int nchs = 0;

  if(remain == NULL || (tcg = channel_group_by_tag(atoi(remain))) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link) {
    if(LIST_FIRST(&ch->ch_transports) == NULL)
      continue;

    nchs++;

    htsbuf_qprintf(tq, "<div style=\"float:left; width: 25%%\">");

    snprintf(dispname, sizeof(dispname), "%s", ch->ch_name);
    strcpy(dispname + sizeof(dispname) - 4, "...");

    ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, dispname);

    /* inner */

    htsbuf_qprintf(tq, 
		"<div style=\"width: 100%%; overflow: hidden; height:36px\">");

    htsbuf_qprintf(tq, 
		"<div style=\"float: left; height:32px; width:32px; "
		"margin: 2px\">");

    if(ch->ch_icon != NULL) {
      htsbuf_qprintf(tq, "<img src=\"%s\" style=\"width:32px\">",
		  ch->ch_icon);
    }

    htsbuf_qprintf(tq, "</div>");

    htsbuf_qprintf(tq, "<div style=\"float:left; text-align: right\">");

    si = (struct sockaddr_in *)&hc->hc_tcp_session.tcp_self_addr;

    htsbuf_qprintf(tq,
		"<a href=\"rtsp://%s:%d/%s\">Stream</a>",
		inet_ntoa(si->sin_addr), ntohs(si->sin_port),
		ch->ch_sname);

    htsbuf_qprintf(tq, "</div>");
    htsbuf_qprintf(tq, "</div>");


    htsbuf_qprintf(tq, "<div id=\"events%d\" style=\"height:42px\">", ch->ch_tag);
    ajax_list_events(tq, ch, 3);
    htsbuf_qprintf(tq, "</div>");

    ajax_box_end(tq, AJAX_BOX_SIDEBOX);
    htsbuf_qprintf(tq, "</div>");
  }

  if(nchs == 0)
    htsbuf_qprintf(tq, "<div style=\"text-align: center; font-weight: bold\">"
		"No channels in this group</div>");

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
  htsbuf_queue_t *tq = &hr->hr_q;

  ajax_box_begin(tq, AJAX_BOX_FILLED, "channelgroupmenu", NULL, NULL);
  ajax_box_end(tq, AJAX_BOX_FILLED);

  htsbuf_qprintf(tq, "<div id=\"channelgroupdeck\"></div>");

  htsbuf_qprintf(tq,
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
