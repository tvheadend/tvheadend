/*
 *  tvheadend, HTML user interface
 *  Copyright (C) 2007 Andreas Öman
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
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "tvhead.h"
#include "htmlui.h"
#include "channels.h"
#include "epg.h"
#include "pvr.h"
#include "strtab.h"
#include "dvb.h"
#include "v4l.h"
#include "iptv_input.h"
#include "transports.h"
#include "autorec.h"

#define MAIN_WIDTH 800

static struct strtab recstatustxt[] = {
  { "Recording scheduled",HTSTV_PVR_STATUS_SCHEDULED      },
  { "Recording",          HTSTV_PVR_STATUS_RECORDING      },
  { "Done",               HTSTV_PVR_STATUS_DONE           },

  { "Recording aborted",  HTSTV_PVR_STATUS_ABORTED        },

  { "No transponder",     HTSTV_PVR_STATUS_NO_TRANSPONDER },
  { "File error",         HTSTV_PVR_STATUS_FILE_ERROR     },
  { "Disk full",          HTSTV_PVR_STATUS_DISK_FULL      },
  { "Buffer error",       HTSTV_PVR_STATUS_BUFFER_ERROR   },
};

static struct strtab recintstatustxt[] = {
  { "Stopped",                        PVR_REC_STOP                    },
  { "Waiting for transponder",        PVR_REC_WAIT_SUBSCRIPTION       },
  { "Waiting for program start",      PVR_REC_WAIT_FOR_START          },
  { "Waiting for valid audio frames", PVR_REC_WAIT_AUDIO_LOCK         },
  { "Waiting for valid video frames", PVR_REC_WAIT_VIDEO_LOCK         },
  { "Recording",                      PVR_REC_RUNNING                 },
  { "Commercial break",               PVR_REC_COMMERCIAL              }
};


static struct strtab recstatuscolor[] = {
  { "#3333aa",      HTSTV_PVR_STATUS_SCHEDULED      },
  { "#aa3333",      HTSTV_PVR_STATUS_RECORDING      },
  { "#33aa33",      HTSTV_PVR_STATUS_DONE           },
  { "#aa3333",      HTSTV_PVR_STATUS_ABORTED        },
  { "#aa3333",      HTSTV_PVR_STATUS_NO_TRANSPONDER },
  { "#aa3333",      HTSTV_PVR_STATUS_FILE_ERROR     },
  { "#aa3333",      HTSTV_PVR_STATUS_DISK_FULL      },
  { "#aa3333",      HTSTV_PVR_STATUS_BUFFER_ERROR   },
};


/**
 * Compare start for two events, used as argument to qsort(3)
 */
static int
eventcmp(const void *A, const void *B)
{
  const event_t *a = *(const event_t **)A;
  const event_t *b = *(const event_t **)B;

  return a->e_start - b->e_start;
}


/*
 * Return 1 if current user have access to a feature
 */

static int
html_verify_access(http_connection_t *hc, const char *feature)
{
  if(hc->hc_user_config == NULL)
    return 0;
  return atoi(config_get_str_sub(hc->hc_user_config, feature, "0"));
}




static int
pvrstatus_to_html(tv_pvr_status_t pvrstatus, const char **text,
		  const char **col)
{
  *text = val2str(pvrstatus, recstatustxt);
  if(*text == NULL)
    return -1;

  *col = val2str(pvrstatus, recstatuscolor);
  if(*col == NULL)
    return -1;

  return 0;
}

/*
 * Root page
 */
static int
page_css(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  tcp_init_queue(&tq, -1);

  tcp_qprintf(&tq, 
	      "img { border: 0px; }\r\n"
	      "a:link { text-decoration: none}\r\n"
	      "a:visited { text-decoration: none}\r\n"
	      "a:active { text-decoration: none}\r\n"
	      "a:link { text-decoration: none; color: #000000}\r\n"
	      "a:visited { text-decoration: none; color: #000000}\r\n"
	      "a:active { text-decoration: none; color: #000000}\r\n"
	      "a:hover { text-decoration: underline; color: CC3333}\r\n"
	      ""
	      "#box {background: #cccc99;}\r\n"
	      ".roundtop {background: #ffffff;}\r\n"
	      ".roundbottom {background: #ffffff;}\r\n"
	      ".r1{margin: 0 5px; height: 1px; overflow: hidden; "
	      "background: #000000; border-left: 1px solid #000000; "
	      "border-right: 1px solid #000000;}\r\n"
	      ""
	      ".r2{margin: 0 3px; height: 1px; overflow: hidden; "
	      "background: #cccc99; border-left: 1px solid #000000; "
	      "border-right: 1px solid #000000; border-width: 0 2px;}\r\n"
	      ""
	      ".r3{margin: 0 2px; height: 1px; overflow: hidden; "
	      "background: #cccc99; border-left: 1px solid #000000; "
	      "border-right: 1px solid #000000;}\r\n"
	      ""
	      ".r4{margin: 0 1px; height: 2px; overflow: hidden; "
	      "background: #cccc99; border-left: 1px solid #000000; "
	      "border-right: 1px solid #000000;}\r\n"
	      ""
	      ".content3 {padding-left: 3px; height: 60px; "
              "border-left: 1px solid #000000; "
	      "border-right: 1px solid #000000;}\r\n"
	      ""
	      ".content {padding-left: 3px; border-left: 1px solid #000000; "
	      "border-right: 1px solid #000000;}\r\n"
	      ""
	      ".contentbig {padding-left: 3px; "
	      "border-left: 1px solid #000000; "
	      "border-right: 1px solid #000000; "
	      "font: 150% Verdana, Arial, Helvetica, sans-serif;}\r\n"
	      ""
	      ".smalltxt {padding-left: 3px; "
	      "font: 100% Verdana, Arial, Helvetica, sans-serif;}\r\n"
	      ""
	      ".statuscont {float: left; margin: 4px; width: 400px}\r\n"
	      ""
	      ".logo {padding: 2px; width: 60px; height: 56px; "
	      "float: left};\r\n"
	      ""
	      ".over {float: left}\r\n"
	      ".toptxt {float: left; width: 165px; text-align: center}\r\n"
	      ""
	      ".knapp {border: 1px dotted #000000; background: #ddddaa} "
	      ".knapp:hover {border: 1px dotted #000000; background: #aaaa66}"
	      ""
	      ".drop {border: 1px dotted #000000; background: #ddddaa} "
	      ""
	      ".prioval {border: 0px; background: #ddddaa} "
	      ""
	      "#meny {margin: 0; padding: 0}\r\n"
	      "#meny li{display: inline; list-style-type: none;}\r\n"
	      "#meny a{padding: 1.15em 0.8em; text-decoration: none;}\r\n"
	      );

  http_output_queue(hc, &tq, "text/css", 60);
  return 0;
}







static void
html_header(tcp_queue_t *tq, const char *title, int javascript, int width,
	    int autorefresh, const char *extrascript)
{
  char w[30];

  if(width > 0)
    snprintf(w, sizeof(w), "width: %dpx; ", width);
  else
    w[0] = 0;

  tcp_qprintf(tq, 
	      "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" "
	      "http://www.w3.org/TR/html4/strict.dtd\">\r\n"
	      "<html><head>\r\n"
	      "<title>%s</title>\r\n"
	      "<meta http-equiv=\"Content-Type\" "
	      "content=\"text/html; charset=utf-8\">\r\n", title);

  if(autorefresh) 
    tcp_qprintf(tq, 
		"<meta http-equiv=\"refresh\" content=\"%d\">\r\n",
		autorefresh);

  tcp_qprintf(tq, 
	      "<link href=\"/css.css\" rel=\"stylesheet\" type=\"text/css\">"
	      );

  tcp_qprintf(tq,
             "<style type=\"text/css\">\r\n"
            "<!--\r\n"
	      "body {margin: 4px 4px; "
	      "font: 75% Verdana, Arial, Helvetica, sans-serif; "
	      "%s margin-right: auto; margin-left: auto;}\r\n"
	      "-->\r\n"
	      "</style>", w);


  if(javascript) {
    tcp_qprintf(tq, 
		"<script type=\"text/javascript\" "
		"src=\"http://www.olebyn.nu/hts/overlib.js\"></script>\r\n");
  }

  if(extrascript)
    tcp_qprintf(tq, "%s", extrascript);

  /* BODY start */
  
  tcp_qprintf(tq, "</head><body>\r\n");

  if(javascript)
    tcp_qprintf(tq, 
		"<div id=\"overDiv\" style=\"position:absolute; "
		"visibility:hidden; z-index:1000;\"></div>\r\n");

}

static void
html_footer(tcp_queue_t *tq)
{
  tcp_qprintf(tq, 
	      "</body></html>\r\n\r\n");
}


static void
box_top(tcp_queue_t *tq, const char *style)
{
  tcp_qprintf(tq, "<div id=\"%s\">"
	      "<div class=\"roundtop\">"
	      "<div class=\"r1\"></div>"
	      "<div class=\"r2\"></div>"
	      "<div class=\"r3\"></div>"
	      "<div class=\"r4\"></div>"
	      "</div>", style);
}

static void
box_bottom(tcp_queue_t *tq)
{
  tcp_qprintf(tq, 
	      "<div class=\"roundbottom\">"
	      "<div class=\"r4\"></div>"
	      "<div class=\"r3\"></div>"
	      "<div class=\"r2\"></div>"
	      "<div class=\"r1\"></div>"
	      "</div></div>");
}


static void
top_menu(http_connection_t *hc, tcp_queue_t *tq)
{
  tcp_qprintf(tq, "<div style=\"width: %dpx; "
	      "margin-left: auto; margin-right: auto\">", MAIN_WIDTH);

  box_top(tq, "box");

  tcp_qprintf(tq, 
	      "<div class=\"content\">"
	      "<ul id=\"meny\">");


  tcp_qprintf(tq, "<li><a href=\"/\">Channel Guide</a></li>");

  tcp_qprintf(tq, "<li><a href=\"/search\">Search</a></li>");

  if(html_verify_access(hc, "record-events"))
    tcp_qprintf(tq, "<li><a href=\"/pvr\">Recorder</a></li>");
 
  if(html_verify_access(hc, "system-status"))
    tcp_qprintf(tq, "<li><a href=\"/status\">System Status</a></li>");

  if(html_verify_access(hc, "admin"))
    tcp_qprintf(tq, "<li><a href=\"/chgrp\">Manage channel groups</a></li>");

  if(html_verify_access(hc, "admin"))
    tcp_qprintf(tq, "<li><a href=\"/chadm\">Manage channels</a></li>");

  tcp_qprintf(tq, "</div>");

  box_bottom(tq);
  tcp_qprintf(tq, "</div><br>\r\n");
}




void
esacpe_char(char *dst, int dstlen, const char *src, char c, 
	    const char *repl)
{
  char v;
  const char *r;

  while((v = *src++) && dstlen > 1) {
    if(v != c) {
      *dst++ = v;
      dstlen--;
    } else {
      r = repl;
      while(dstlen > 1 && *r) {
	*dst++ = *r++;
	dstlen--;
      }
    }
  }
  *dst = 0;
}



static int
is_client_simple(http_connection_t *hc)
{
  char *c;

  if((c = http_arg_get(&hc->hc_args, "UA-OS")) != NULL) {
    if(strstr(c, "Windows CE") || strstr(c, "Pocket PC"))
      return 1;
  }

  if((c = http_arg_get(&hc->hc_args, "x-wap-profile")) != NULL) {
    return 1;
  }
  return 0;
}


/*
 * Output_event
 */

static void
output_event(http_connection_t *hc, tcp_queue_t *tq, th_channel_t *ch,
	     event_t *e, int simple, int print_channel, int print_pvr,
	     int maxsize)
{
  char title[100];
  char bufa[4000];
  char overlibstuff[4000];
  struct tm a, b;
  time_t stop;
  event_t *cur;
  tv_pvr_status_t pvrstatus;
  const char *pvr_txt, *pvr_color;
  pvr_rec_t *pvrr;

  localtime_r(&e->e_start, &a);
  stop = e->e_start + e->e_duration;
  localtime_r(&stop, &b);


  pvrr = pvr_get_by_entry(e);
  pvrstatus = pvrr != NULL ? pvrr->pvrr_status : HTSTV_PVR_STATUS_NONE; 

  cur = epg_event_get_current(ch);

  if(!simple && e->e_desc != NULL && e->e_desc[0] != 0) {
    esacpe_char(bufa, sizeof(bufa), e->e_desc, '\'', "");
	
    snprintf(overlibstuff, sizeof(overlibstuff), 
	     "onmouseover=\"return overlib('%s')\" "
	     "onmouseout=\"return nd();\"",
	     bufa);
  } else {
    overlibstuff[0] = 0;
  }

  if(1 || simple) {
    snprintf(bufa, sizeof(bufa),
	     "/event/%d", e->e_tag);
  } else {
    snprintf(bufa, sizeof(bufa),
	     "javascript:epop('/event/%d')", e->e_tag);
  }

  tcp_qprintf(tq, "<div><a href=\"%s\" %s>", bufa, overlibstuff);

  if(print_channel) {
    esacpe_char(title, sizeof(title), ch->ch_name, '"', "'");
    
    tcp_qprintf(tq, 
		"<span style=\"overflow: hidden; height: 15px;"
		"width: %dpx;float: left\">"
		"%s</span>",
		simple ? 80 : maxsize * 125 / 600, title);
  }


  esacpe_char(title, sizeof(title), e->e_title, '"', "'");
  tcp_qprintf(tq, 
	      "<span style=\"width: %dpx;height: 15px;overflow: hidden;"
	      "float: left%s\">"
	      "%02d:%02d - %02d:%02d</span>"
	      "<span style=\"overflow: hidden; height: 15px; width: %dpx; "
	      "float: left%s\">%s</span>",
	      simple ? 80 : maxsize * 125 / 600,
	      e == cur ? ";font-weight:bold" : "",
	      a.tm_hour, a.tm_min, b.tm_hour, b.tm_min,
	      simple ? 100 : maxsize * 350 / 600,
	      e == cur ? ";font-weight:bold" : "",
	      title
	      );

  if(print_pvr && !pvrstatus_to_html(pvrstatus, &pvr_txt, &pvr_color)) {
    tcp_qprintf(tq, 
		"<span style=\"float:left;font-style:italic;"
		"color:%s;font-weight:bold\">%s</span></a></div><br>",
		pvr_color, pvr_txt);

  } else {
    tcp_qprintf(tq, "</a></div><br>");
  } 
}

/*
 * Root page
 */
static int
page_root(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  th_channel_t *ch;
  event_t *e;
  int i;
  int simple = is_client_simple(hc);
  time_t firstend = INT32_MAX;
  th_channel_group_t *tcg;
  struct sockaddr_in *si;

  if(!html_verify_access(hc, "browse-events"))
    return HTTP_STATUS_UNAUTHORIZED;

  epg_lock();

  tcp_init_queue(&tq, -1);

  LIST_FOREACH(ch, &channels, ch_global_link) {
    e = epg_event_find_current_or_upcoming(ch);
    if(e && e->e_start + e->e_duration < firstend) {
      firstend = e->e_start + e->e_duration;
    }
  }

  i = 5 + firstend - dispatch_clock;
  if(i < 30)
    i = 30;

  html_header(&tq, "HTS/tvheadend", !simple, MAIN_WIDTH, i, NULL);
  top_menu(hc, &tq);

  TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {
    if(tcg->tcg_hidden)
      continue;

    box_top(&tq, "box");

    tcp_qprintf(&tq, 
		"<div class=\"contentbig\"><center><b>%s</b></center></div>",
		tcg->tcg_name);
    box_bottom(&tq);
    tcp_qprintf(&tq, "<br>");

    TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link) {
      if(LIST_FIRST(&ch->ch_transports) == NULL)
	continue;

      box_top(&tq, "box");
      tcp_qprintf(&tq, "<div class=\"content3\">");

      if(!simple) {
	tcp_qprintf(&tq, "<div class=\"logo\">");
	if(ch->ch_icon) {
	  tcp_qprintf(&tq, "<a href=\"channel/%d\">"
		      "<img src=\"%s\" height=56px>"
		      "</a>",
		      ch->ch_tag,
		      refstr_get(ch->ch_icon));
	}
	tcp_qprintf(&tq, "</div>");
      }

      tcp_qprintf(&tq, "<div class=\"over\">");
      tcp_qprintf(&tq, 
		  "<span style=\"overflow: hidden; height: 15px; "
		  "width: 300px; float: left; font-weight:bold\">"
		  "<a href=\"channel/%d\">%s</a></span>",
		  ch->ch_tag, ch->ch_name);

      si = (struct sockaddr_in *)&hc->hc_tcp_session.tcp_self_addr;

      tcp_qprintf(&tq,
		  "<i><a href=\"rtsp://%s:%d/%s\">Watch live</a></i><br>",
		  inet_ntoa(si->sin_addr), ntohs(si->sin_port),
		  ch->ch_sname);

      e = epg_event_find_current_or_upcoming(ch);

      for(i = 0; i < 3 && e != NULL; i++) {

	output_event(hc, &tq, ch, e, simple, 0, 1, 580);
	e = TAILQ_NEXT(e, e_link);
      }

      tcp_qprintf(&tq, "</div></div>");
      box_bottom(&tq);
      tcp_qprintf(&tq, "<br>\r\n");
    }
  }
  epg_unlock();

  html_footer(&tq);
  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}



/*
 * Channel page
 */


const char *days[7] = {
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday",
  };


static int
page_channel(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  th_channel_t *ch = NULL;
  event_t *e = NULL;
  int i;
  int simple = is_client_simple(hc);
  int channeltag = -1;
  int w, doff = 0, wday;
  struct tm a;

  if(!html_verify_access(hc, "browse-events"))
    return HTTP_STATUS_UNAUTHORIZED;

  i = sscanf(remain, "%d/%d", &channeltag, &doff);
  ch = channel_by_tag(channeltag);
  if(i != 2)
    doff = 0;

  if(ch == NULL) {
    http_error(hc, 404);
    return 0;
  }

  tcp_init_queue(&tq, -1);

  html_header(&tq, "HTS/tvheadend", !simple, MAIN_WIDTH, 0, NULL);

  top_menu(hc, &tq);

  epg_lock();

  box_top(&tq, "box");

  tcp_qprintf(&tq, "<div class=\"content\">");
  tcp_qprintf(&tq, "<strong><a href=\"channel/%d\">%s</a></strong><br>",
	      ch->ch_tag, ch->ch_name);

  e = epg_event_find_current_or_upcoming(ch);
  if(e != NULL) {
    localtime_r(&e->e_start, &a);
    wday = a.tm_wday;


    for(w = 0; w < 7; w++) {

      tcp_qprintf(&tq, 
		  "<a href=\"/channel/%d/%d\">"
		  "<i><u>%s</i></u></a><br>",
		  ch->ch_tag, w,
		  days[(wday + w) % 7]);

      while(e != NULL) {
	localtime_r(&e->e_start, &a);
	if(a.tm_wday != wday + w)
	  break;

	if(a.tm_wday == wday + doff) {
	  output_event(hc, &tq, ch, e, simple, 0, 1, 580);
	}
	e = TAILQ_NEXT(e, e_link);
      }
    }
  }

  tcp_qprintf(&tq, "</div>");
  box_bottom(&tq);
  tcp_qprintf(&tq, "<br>\r\n");
  epg_unlock();

  html_footer(&tq);
  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}





/**
 * Event page
 */
static int
page_event(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  int simple = is_client_simple(hc);
  int eventid = atoi(remain);
  char title[100];
  event_t *e;
  struct tm a, b;
  time_t stop;
  char desc[4000];
  tv_pvr_status_t pvrstatus;
  pvr_rec_t *pvrr;
  const char *pvr_txt, *pvr_color;
  
  if(!html_verify_access(hc, "browse-events"))
    return HTTP_STATUS_UNAUTHORIZED;

  epg_lock();
  e = epg_event_find_by_tag(eventid);
  if(e == NULL) {
    epg_unlock();
    return 404;
  }

  pvrr = pvr_get_by_entry(e);

  if(http_arg_get(&hc->hc_url_args, "rec")) {
    if(!html_verify_access(hc, "record-events")) {
      epg_unlock();
      return HTTP_STATUS_UNAUTHORIZED;
    }
    pvrr = pvr_schedule_by_event(e);
  }

  if(pvrr != NULL && http_arg_get(&hc->hc_url_args, "cancel")) {
    if(!html_verify_access(hc, "record-events")) {
      epg_unlock();
      return HTTP_STATUS_UNAUTHORIZED;
    }
    pvr_abort(pvrr);
  }

  if(pvrr != NULL && http_arg_get(&hc->hc_url_args, "clear")) {
    if(!html_verify_access(hc, "record-events")) {
      epg_unlock();
      return HTTP_STATUS_UNAUTHORIZED;
    }
    pvr_clear(pvrr);
  }


  pvrstatus = pvrr != NULL ? pvrr->pvrr_status : HTSTV_PVR_STATUS_NONE;

  localtime_r(&e->e_start, &a);
  stop = e->e_start + e->e_duration;
  localtime_r(&stop, &b);

  tcp_init_queue(&tq, -1);

  html_header(&tq, "HTS/tvheadend", 0, MAIN_WIDTH, 0, NULL);
  top_menu(hc, &tq);

  tcp_qprintf(&tq, "<form method=\"get\" action=\"/event/%d\">", eventid);


  box_top(&tq, "box");

  tcp_qprintf(&tq, "<div class=\"content\">");


  esacpe_char(title, sizeof(title), e->e_title, '"', "'");
  esacpe_char(desc, sizeof(desc), e->e_desc, '\'', "");

  tcp_qprintf(&tq, 
	      "<div style=\"width: %dpx;float: left;font-weight:bold\">"
	      "%02d:%02d - %02d:%02d</div>"
	      "<div style=\"width: %dpx; float: left;font-weight:bold\">"
	      "%s</div></a>",
	      simple ? 80 : 100,
	      a.tm_hour, a.tm_min, b.tm_hour, b.tm_min,
	      simple ? 100 : 250,
	      title);

  if(!pvrstatus_to_html(pvrstatus, &pvr_txt, &pvr_color))
    tcp_qprintf(&tq, 
		"<div style=\"font-style:italic;color:%s;font-weight:bold\">"
		"%s</div>",
		pvr_color, pvr_txt);
  else
    tcp_qprintf(&tq, "<br>");
    

  tcp_qprintf(&tq, "<br>%s", desc);

  tcp_qprintf(&tq,"<div style=\"text-align: center\">");

  if(html_verify_access(hc, "record-events")) {
    switch(pvrstatus) {
    case HTSTV_PVR_STATUS_SCHEDULED:
    case HTSTV_PVR_STATUS_RECORDING:
      tcp_qprintf(&tq,
		  "<input type=\"submit\" class=\"knapp\" name=\"cancel\" "
		  "value=\"Cancel recording\">");
      break;


    case HTSTV_PVR_STATUS_NONE:
      tcp_qprintf(&tq,
		  "<input type=\"submit\" class=\"knapp\" name=\"rec\" "
		  "value=\"Record\">");
      break;

    default:
      tcp_qprintf(&tq,
		  "<input type=\"submit\" class=\"knapp\" name=\"clear\" "
		  "value=\"Clear error status\">");
      break;

    }
  }

  tcp_qprintf(&tq, "</div></div></div>");
  box_bottom(&tq);
  tcp_qprintf(&tq, "</form>\r\n");
  html_footer(&tq);
  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);

  epg_unlock();

  return 0;
}



static int
pvrcmp(const void *A, const void *B)
{
  const pvr_rec_t *a = *(const pvr_rec_t **)A;
  const pvr_rec_t *b = *(const pvr_rec_t **)B;

  return a->pvrr_start - b->pvrr_start;
}

/**
 * PVR main page
 */
static int
page_pvr(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  int simple = is_client_simple(hc);
  pvr_rec_t *pvrr, *pvrr_tgt;
  char escapebuf[4000];
  char title[100];
  char channel[100];
  struct tm a, b, day;
  const char *pvr_txt, *pvr_color, *buttontxt, *cmd, *txt;
  char buttonname[100];
  int c, i;
  pvr_rec_t **pv;
  int divid = 1;
  int size = 600;
  http_arg_t *ra;

  if(!html_verify_access(hc, "record-events"))
    return HTTP_STATUS_UNAUTHORIZED;

  if(http_arg_get(&hc->hc_url_args, "clearall")) {
    pvr_clear_all_completed();
  }

  pvrr_tgt = NULL;
  LIST_FOREACH(ra, &hc->hc_url_args, link) {
    c = 0;
    if(!strncmp(ra->key, "clear_", 6)) {
      txt = ra->key + 6;
    } else if(!strncmp(ra->key, "desched_", 8)) {
      txt = ra->key + 8;
    } else if(!strncmp(ra->key, "abort_", 6)) {
      c = 1;
      txt = ra->key + 6;
    } else {
      continue;
    }
    pvrr_tgt = pvr_get_tag_entry(atoi(txt));
    if(pvrr_tgt != NULL) {
      if(c)
	pvr_abort(pvrr_tgt);
      else
	pvr_clear(pvrr_tgt);
    }
    break;
  }

  tcp_init_queue(&tq, -1);
  html_header(&tq, "HTS/tvheadend", 0, MAIN_WIDTH, 0,
	      "<script type=\"text/javascript\">\n"
	      "function expcol(id)\n"
	      "{\n"
	      " var div = document.getElementById(id);\n"
	      " if (div.style.display != \"none\") {\n"
	      "   div.style.display = \"none\";\n"
	      " } else {\n"
	      "   div.style.display = \"block\";\n"
	      "  }\n"
	      "}\n"
	      "</script>\n");

  top_menu(hc, &tq);

  tcp_qprintf(&tq, "<form method=\"get\" action=\"/pvr\">");

  box_top(&tq, "box");

  tcp_qprintf(&tq, 
	      "<div class=\"contentbig\"><center><b>%s</b></center></div>",
	      "Recorder Log");

  tcp_qprintf(&tq, "<div class=\"content\">");

  c = 0;
  LIST_FOREACH(pvrr, &pvrr_global_list, pvrr_global_link)
    c++;

  if(c == 0) {
    tcp_qprintf(&tq, "<center>No entries</center><br>");
  }

  pv = alloca(c * sizeof(pvr_rec_t *));

  i = 0;
  LIST_FOREACH(pvrr, &pvrr_global_list, pvrr_global_link)
    pv[i++] = pvrr;


  qsort(pv, i, sizeof(pvr_rec_t *), pvrcmp);

  memset(&day, -1, sizeof(struct tm));

  for(i = 0; i < c; i++) {
    pvrr = pv[i];

    localtime_r(&pvrr->pvrr_start, &a);
    localtime_r(&pvrr->pvrr_stop, &b);

    if(a.tm_wday != day.tm_wday || a.tm_mday != day.tm_mday  ||
       a.tm_mon  != day.tm_mon  || a.tm_year != day.tm_year) {
      memcpy(&day, &a, sizeof(struct tm));
      tcp_qprintf(&tq, "<br><b><i>%s, %d/%d</i></b><br>",
		  days[day.tm_wday], day.tm_mday, day.tm_mon + 1);
    }


    tcp_qprintf(&tq, 
		"<div><a href=\"javascript:expcol('div%d')\"",
		divid);
    
    if(!simple && pvrr->pvrr_desc != NULL && pvrr->pvrr_desc[0] != 0) {
      esacpe_char(escapebuf, sizeof(escapebuf), pvrr->pvrr_desc, '\'', "");
      
      tcp_qprintf(&tq, 
		  " onmouseover=\"return overlib('%s')\""
		  " onmouseout=\"return nd();\"",
		  escapebuf);
    }
    
    tcp_qprintf(&tq, ">");

    esacpe_char(channel, sizeof(channel), 
		pvrr->pvrr_channel->ch_name, '"', "'");


    tcp_qprintf(&tq, 
		"<span style=\"overflow: hidden; height: 15px;"
		"width: %dpx;float: left\">"
		"%s</span>",
		size * 125 / 600,
		channel);

    esacpe_char(title, sizeof(title), pvrr->pvrr_title, '"', "'");
    tcp_qprintf(&tq, 
		"<span style=\"width: %dpx;height: 15px;overflow: hidden;"
		"float: left\">"
		"%02d:%02d - %02d:%02d</span>"
		"<span style=\"overflow: hidden; height: 15px; width: %dpx; "
		"float: left\">%s</span>",
		size * 125 / 600,
		a.tm_hour, a.tm_min, b.tm_hour, b.tm_min,
		size * 350 / 600,
		title);

    if(!pvrstatus_to_html(pvrr->pvrr_status, &pvr_txt, &pvr_color)) {
      tcp_qprintf(&tq, 
		  "<span style=\"float:left;font-style:italic;"
		  "color:%s;font-weight:bold\">%s</span>",
		  pvr_color, pvr_txt);
    }

    tcp_qprintf(&tq, "</a></div><br>");


    tcp_qprintf(&tq,
		"<div id=\"div%d\" style=\"display:%s; "
		"border-bottom-width:thin; border-bottom-style:solid\">",
		divid, pvrr_tgt == pvrr ? "block" : "none");

    tcp_qprintf(&tq, 
		"<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\" "
		"style=\"font: 85% Verdana, Arial, Helvetica, sans-serif\">");

    tcp_qprintf(&tq, 
		"<tr>"
		"<td width=125><span style=\"text-align: right\">"
		"Filename:</span><td>"
		"<td>%s</td>"
		"</tr>",
		pvrr->pvrr_filename ?: "<i>not set</i>");

    tcp_qprintf(&tq, 
		"<tr>"
		"<td width=125><span style=\"text-align: right\">"
		"Recorder status:</span><td>"
		"<td>%s</td>"
		"</tr>",
		val2str(pvrr->pvrr_rec_status, recintstatustxt) ?: "invalid");

    tcp_qprintf(&tq, 
		"</table>");

    switch(pvrr->pvrr_status) {
    case HTSTV_PVR_STATUS_SCHEDULED:
      buttontxt = "Remove from schedule";
      cmd = "desched";
      break;

    case HTSTV_PVR_STATUS_RECORDING:
      buttontxt = "Abort recording";
      cmd = "abort";
      break;

    default:
      buttontxt = "Remove log entry";
      cmd = "clear";
      break;
    }
    snprintf(buttonname, sizeof(buttonname), "%s_%d", cmd, pvrr->pvrr_ref);

    tcp_qprintf(&tq,"<div style=\"text-align: center\">"
		"<input type=\"submit\" class=\"knapp\" name=\"%s\" "
		"value=\"%s\"></div>", buttonname, buttontxt);

    tcp_qprintf(&tq, "<br></div>\n");
    divid++;
  }

  tcp_qprintf(&tq,
	      "<br><div style=\"text-align: center\">"
	      "<input type=\"submit\" class=\"knapp\" name=\"clearall\" "
	      "value=\"Remove all completed recording log entries\"></div>");

  tcp_qprintf(&tq, "</div>\r\n");

  box_bottom(&tq);
  tcp_qprintf(&tq, "</form>\r\n");
  html_footer(&tq);
  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);

  return 0;
}




static void
html_iptv_status(tcp_queue_t *tq, th_transport_t *t, const char *status)
{
  tcp_qprintf(tq,
	      "<span style=\"overflow: hidden; height: 15px; width: 270px; "
	      "float:left; font-weight:bold\">"
	      "%s (%s)"
	      "</span>",
	      inet_ntoa(t->tht_iptv_group_addr),
	      t->tht_channel->ch_name);

  tcp_qprintf(tq,
	      "<span style=\"overflow: hidden; height: 15px; width: 100px; "
	      "float:left\">"
	      "%s"
	      "</span><br>",
	      status);

}


/**
 * System status
 */
static int
page_status(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  int simple = is_client_simple(hc);
  th_dvb_adapter_t *tda;
  th_v4l_adapter_t *tva;
  th_subscription_t *s;
  th_transport_t *t;
  th_dvb_mux_instance_t *tdmi;
  th_stream_t *st;
  const char *txt, *t1, *t2;
  char tmptxt[100];

  if(!html_verify_access(hc, "system-status"))
    return HTTP_STATUS_UNAUTHORIZED;

  tcp_init_queue(&tq, -1);

  html_header(&tq, "HTS/tvheadend", !simple, -1, 0, NULL);

  top_menu(hc, &tq);

  tcp_qprintf(&tq, "<div style=\"width: 1300px; margin: auto\">");

  tcp_qprintf(&tq, "<div class=\"statuscont\">");


  box_top(&tq, "box");
  tcp_qprintf(&tq, "<div class=\"contentbig\">");
  tcp_qprintf(&tq, "<b><center>Input devices</b><br>");
  tcp_qprintf(&tq, "</div>");
  box_bottom(&tq);
  tcp_qprintf(&tq, "<br>");

  /* DVB adapters */

  box_top(&tq, "box");
  tcp_qprintf(&tq, "<div class=\"content\">");
  tcp_qprintf(&tq, "<b><center>DVB adapters</center></b>");

  if(LIST_FIRST(&dvb_adapters_running) == NULL) {
    tcp_qprintf(&tq, "No DVB adapters configured<br>");
  } else {
    LIST_FOREACH(tda, &dvb_adapters_running, tda_link) {
      tcp_qprintf(&tq, "<br><b>%s</b><br>%s<br>",
		  tda->tda_rootpath, tda->tda_info);
      LIST_FOREACH(tdmi, &tda->tda_muxes_active, tdmi_adapter_link) {

	tcp_qprintf(&tq,
		    "<span style=\"overflow: hidden; height: 15px; "
		    "width: 160px; float: left\">"
		    "%s"
		    "</span>",
		    tdmi->tdmi_shortname);

	txt = tdmi->tdmi_status ?: "Ok";
	if(tdmi->tdmi_fec_err_per_sec > DVB_FEC_ERROR_LIMIT)
	  txt = "Too high FEC rate";

	tcp_qprintf(&tq,
		    "<span style=\"overflow: hidden; height: 15px; "
		    "width: 120px; float: left\">"
		    "%s"
		    "</span>",
		    txt);

	switch(tdmi->tdmi_state) {
	default:
	  txt = "???";
	  break;
	case TDMI_IDLE:
	  txt = "Idle";
	  break;
	case TDMI_RUNNING:
	  txt = "Running";
	  break;
	case TDMI_IDLESCAN:
	  txt = "IdleScan";
	  break;
	}

	tcp_qprintf(&tq,
		    "<span style=\"overflow: hidden; height: 15px; "
		    "width: 60px; float: left\">"
		    "%s"
		    "</span>",
		    txt);

	tcp_qprintf(&tq,
		    "<span style=\"overflow: hidden; height: 15px; "
		    "width: 50px; float: left\">"
		    "%d"
		    "</span><br>",
		    tdmi->tdmi_fec_err_per_sec);
      }
    }
  }
  tcp_qprintf(&tq, "</div>");
  box_bottom(&tq);

  tcp_qprintf(&tq, "<br>");

  /* IPTV adapters */

  box_top(&tq, "box");
  tcp_qprintf(&tq, "<div class=\"content\">");
  tcp_qprintf(&tq, "<b><center>IPTV sources</center></b>");

  LIST_FOREACH(t, &all_transports, tht_global_link) {
    if(t->tht_type != TRANSPORT_IPTV)
      continue;
    html_iptv_status(&tq, t, 
		     t->tht_status == TRANSPORT_IDLE ? "Idle" : "Running");
  }

  LIST_FOREACH(t, &iptv_stale_transports, tht_adapter_link)
    html_iptv_status(&tq, t, "Probe failed");

  tcp_qprintf(&tq, "</div>");
  box_bottom(&tq);
  tcp_qprintf(&tq, "<br>");

  /* Video4Linux adapters */

  box_top(&tq, "box");
  tcp_qprintf(&tq, "<div class=\"content\">");
  tcp_qprintf(&tq, "<b><center>Video4Linux adapters</center></b>");

  LIST_FOREACH(tva, &v4l_adapters, tva_link) {

    tcp_qprintf(&tq,
		"<span style=\"overflow: hidden; height: 15px; width: 270px; "
		"float:left; font-weight:bold\">"
		"%s"
		"</span>",
		tva->tva_path);


    if(tva->tva_dispatch_handle == NULL) {
      snprintf(tmptxt, sizeof(tmptxt), "Idle");
    } else {
      snprintf(tmptxt, sizeof(tmptxt), "Tuned to %.3f MHz",
	       (float)tva->tva_frequency/1000000.0);
    }
    tcp_qprintf(&tq,
		"<span style=\"overflow: hidden; height: 15px; width: 100px; "
		"float:left\">"
		"%s"
		"</span><br>",
		tmptxt);
  }

  tcp_qprintf(&tq, "</div>");
  box_bottom(&tq);
  tcp_qprintf(&tq, "</div>");

  



  /* Active transports */

  tcp_qprintf(&tq, "<div class=\"statuscont\">");
  box_top(&tq, "box");
  tcp_qprintf(&tq, "<div class=\"contentbig\">");
  tcp_qprintf(&tq, "<b><center>Active transports</b><br>");
  tcp_qprintf(&tq, "</div>");
  box_bottom(&tq);
  tcp_qprintf(&tq, "<br>");

  LIST_FOREACH(t, &all_transports, tht_global_link) {
    if(t->tht_status != TRANSPORT_RUNNING)
      continue;

    box_top(&tq, "box");
    tcp_qprintf(&tq, "<div class=\"content\">");

    tcp_qprintf(&tq,
		"<span style=\"overflow: hidden; height: 15px; "
		"width: 200px; float: left; font-weight:bold\">"
		"%s"
		"</span>"
		"<span style=\"overflow: hidden; height: 15px; "
		"width: 190px; float: left\">"
		"%s"
		"</span><br>",
		t->tht_name,
		t->tht_channel->ch_name);

 
    switch(t->tht_type) {
    case TRANSPORT_IPTV:
      t1 = tmptxt;
      snprintf(tmptxt, sizeof(tmptxt), "IPTV: %s",
	       inet_ntoa(t->tht_iptv_group_addr));
      t2 = "";
      break;

    case TRANSPORT_V4L:
      t1 = tmptxt;
      snprintf(tmptxt, sizeof(tmptxt), "V4L: %.3f MHz",
	       (float)t->tht_v4l_frequency / 1000000.0f);
      t2 = t->tht_v4l_adapter->tva_path;
      break;

    case TRANSPORT_DVB:
      t1 = t->tht_dvb_mux_instance->tdmi_shortname;
      t2 = t->tht_dvb_mux_instance->tdmi_adapter->tda_rootpath;
      break;

    case TRANSPORT_AVGEN:
      t1 = "A/V Generator";
      t2 = "";
      break;

    default:
      continue;
    }

    tcp_qprintf(&tq,
		"<span style=\"overflow: hidden; height: 15px; "
		"width: 200px; float: left;\">"
		"%s"
		"</span>"
		"<span style=\"overflow: hidden; height: 15px; "
		"width: 190px; float: left\">"
		"%s"
		"</span><br><br>",
		t1, t2);

    LIST_FOREACH(st, &t->tht_streams, st_link) {

      tcp_qprintf(&tq,
		  "<span style=\"overflow: hidden; height: 15px; "
		  "width: 100px; float: left\">"
		  "%s"
		  "</span>",
		  htstvstreamtype2txt(st->st_type));

      tcp_qprintf(&tq,
		  "<span style=\"overflow: hidden; height: 15px; "
		  "width: 100px; float: left\">"
		  "%d kb/s"
		  "</span>",
		  avgstat_read_and_expire(&st->st_rate, dispatch_clock) 
		  / 1000);


      tcp_qprintf(&tq,
		  "<span style=\"overflow: hidden; height: 15px; "
		  "width: 100px; float: left\">"
		  "%d errors/s"
		  "</span><br>",
		  avgstat_read_and_expire(&st->st_cc_errors, dispatch_clock));
    }

    tcp_qprintf(&tq, "</div>");
    box_bottom(&tq);
    tcp_qprintf(&tq, "<br>");
  }
  tcp_qprintf(&tq, "</div>");
  

  /* Subscribers */

  tcp_qprintf(&tq, "<div class=\"statuscont\">");
  box_top(&tq, "box");
  tcp_qprintf(&tq, "<div class=\"contentbig\">");
  tcp_qprintf(&tq, "<b><center>Subscriptions</b><br>");
  tcp_qprintf(&tq, "</div>");
  box_bottom(&tq);
  tcp_qprintf(&tq, "<br>");

  LIST_FOREACH(s, &subscriptions, ths_global_link) {

    box_top(&tq, "box");
    tcp_qprintf(&tq, "<div class=\"content\">");

    tcp_qprintf(&tq,
		"<span style=\"overflow: hidden; height: 15px; "
		"width: 230px; float: left; font-weight:bold\">"
		"%s"
		"</span>",
		s->ths_title);

    tcp_qprintf(&tq,
		"<span style=\"overflow: hidden; height: 15px; "
		"width: 160px; float: left\">"
		"%s"
		"</span><br>",
		s->ths_channel->ch_name);

    if((t = s->ths_transport) == NULL) {
      tcp_qprintf(&tq,
		  "<i>No transport available</i><br>");
    } else {

      tcp_qprintf(&tq,
		  "Using transport \"%s\"<br>",
		  t->tht_name);
     
     }
    tcp_qprintf(&tq, "</div>");
    box_bottom(&tq);
    tcp_qprintf(&tq, "<br>");
  }

  tcp_qprintf(&tq, "</div>");

  tcp_qprintf(&tq, "</div>");
  html_footer(&tq);
  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}



/**
 * Manage channel groups
 */
static int
page_chgroups(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  th_channel_group_t *tcg;
  th_channel_t *ch;
  int cnt;
  const char *grp;
  http_arg_t *ra;

  if(!html_verify_access(hc, "admin"))
    return HTTP_STATUS_UNAUTHORIZED;

  if((grp = http_arg_get(&hc->hc_url_args, "newgrpname")) != NULL)
    channel_group_find(grp, 1);

  LIST_FOREACH(ra, &hc->hc_url_args, link) {
    if(!strncmp(ra->key, "delgroup", 8)) {
      tcg = channel_group_by_tag(atoi(ra->key + 8));
      if(tcg != NULL)
	channel_group_destroy(tcg);
      break;
    }

    if(!strncmp(ra->key, "up", 2)) {
      tcg = channel_group_by_tag(atoi(ra->key + 2));
      if(tcg != NULL)
	channel_group_move_prev(tcg);
      break;
    }

    if(!strncmp(ra->key, "down", 4)) {
      tcg = channel_group_by_tag(atoi(ra->key + 4));
      if(tcg != NULL)
	channel_group_move_next(tcg);
      break;
    }
  }

  tcp_init_queue(&tq, -1);

  html_header(&tq, "HTS/tvheadend", 0, MAIN_WIDTH, 0, NULL);
  top_menu(hc, &tq);


  TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {
    if(tcg->tcg_hidden)
      continue;

    tcp_qprintf(&tq, "<form method=\"get\" action=\"/chgrp\">");

    box_top(&tq, "box");
    tcp_qprintf(&tq, "<div class=\"content3\">");

    cnt = 0;
    TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link)
      cnt++;

    tcp_qprintf(&tq, "<b>%s</b> (%d channels)<br><br>", tcg->tcg_name, cnt);

     tcp_qprintf(&tq, 
		  "<input type=\"submit\" class=\"knapp\" name=\"up%d\""
		  " value=\"Move up\"> ", tcg->tcg_tag);

     tcp_qprintf(&tq, 
		  "<input type=\"submit\" class=\"knapp\" name=\"down%d\""
		  " value=\"Move down\"> ", tcg->tcg_tag);

    if(tcg->tcg_cant_delete_me == 0) {
      tcp_qprintf(&tq, 
		  "<input type=\"submit\" class=\"knapp\" name=\"delgroup%d\""
		  " value=\"Delete this group\">", tcg->tcg_tag);
    }

    tcp_qprintf(&tq, "</div>");
    box_bottom(&tq);
    tcp_qprintf(&tq, "</form>\r\n");
  }


  tcp_qprintf(&tq, "<form method=\"get\" action=\"/chgrp\">");

  box_top(&tq, "box");
  tcp_qprintf(&tq, "<div class=\"content\">"
	      "<input type=\"text\" name=\"newgrpname\""
	      " style=\"border: 1px dotted #000000\"> "
	      "<input type=\"submit\" class=\"knapp\" name=\"newgrp\""
	      " value=\"Add new group\">"
	      "</div>");

  box_bottom(&tq);
  tcp_qprintf(&tq, "</form><br>\r\n");

  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}


/**
 * Manage channels
 */
static int
page_chadm(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  int simple = is_client_simple(hc);

  if(!html_verify_access(hc, "admin"))
    return HTTP_STATUS_UNAUTHORIZED;

  tcp_init_queue(&tq, -1);
  html_header(&tq, "HTS/tvheadend", !simple, MAIN_WIDTH, 0, NULL);
  top_menu(hc, &tq);

  tcp_qprintf(&tq,
	      "<table border=\"0\" cellspacing=\"0\" cellpadding=\"5\"> "
	      "<tr><td>");

  box_top(&tq, "box");

  tcp_qprintf(&tq,
	      "<div class=\"content\" style=\"width: 260px\">"
	      "<iframe src=\"/chadm2\" frameborder=\"0\" "
	      "width=\"250\" height=\"600\"></iframe>"
	      "</div>");

  box_bottom(&tq);

  tcp_qprintf(&tq,
	      "</td><td>"
	      "<iframe name=\"chf\" frameborder=\"0\" "
	      "width=\"530\" height=\"600\" scrolling=\"no\"></iframe>"
	      "</td></tr></table>");

  html_footer(&tq);
  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}



static int
page_chadm2(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  th_channel_t *ch;
  int simple = is_client_simple(hc);
  
  if(!html_verify_access(hc, "admin"))
    return HTTP_STATUS_UNAUTHORIZED;

  tcp_init_queue(&tq, -1);
  html_header(&tq, "HTS/tvheadend", !simple, 230, 0, NULL);

  LIST_FOREACH(ch, &channels, ch_global_link) {
    tcp_qprintf(&tq, 
		"<a href=\"/editchannel/%d\" target=\"chf\">%s</a><br>",
		ch->ch_tag, ch->ch_name);
  }

  html_footer(&tq);
  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}


/**
 * Edit a single channel
 */
static int
page_editchannel(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  th_channel_t *ch, *ch2;
  th_channel_group_t *tcg, *dis;
  th_transport_t *t;

  if(!html_verify_access(hc, "admin"))
    return HTTP_STATUS_UNAUTHORIZED;

  if(remain == NULL || (ch = channel_by_tag(atoi(remain))) == NULL)
    return 404;
  
  dis = channel_group_find("-disabled-", 1);

  tcp_init_queue(&tq, -1);
  html_header(&tq, "HTS/tvheadend", 0, 530, 0, NULL);

  tcp_qprintf(&tq, 
	      "<form method=\"get\" action=\"/updatechannel/%d\">",
	      ch->ch_tag);

  box_top(&tq, "box");
  tcp_qprintf(&tq, 
	      "<div class=\"contentbig\"><center><b>%s</b></center></div>",
	      ch->ch_name);
  box_bottom(&tq);
  tcp_qprintf(&tq, "<br>");

  box_top(&tq, "box");
  tcp_qprintf(&tq, 
	      "<div class=\"content\">"
	      "<div style=\"width: 170px; float: left\">"
	      "Channel group: "
	      "</div>"
	      "<span style=\"width: 250px\">"
	      "<select name=\"grp\" class=\"drop\" "
	      "onChange=\"this.form.submit()\">");

  TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {
    if(tcg->tcg_hidden)
      continue;
    tcp_qprintf(&tq, "<option%s>%s</option>",
		ch->ch_group == tcg ? " selected" : "",
		tcg->tcg_name);
  }

  tcp_qprintf(&tq, "<option%s>%s</option></select></span>",
	      ch->ch_group == dis ? " selected" : "",
	      dis->tcg_name);

  tcp_qprintf(&tq, "<br><br>\r\n");


  tcp_qprintf(&tq, 
	      "<div style=\"width: 170px; float: left\">"
	      "Merge with channel: "
	      "</div>"
	      "<span style=\"width: 250px\">"
	      "<select name=\"merge\" class=\"drop\" "
	      "onChange=\"this.form.submit()\">");
 
  tcp_qprintf(&tq, "<option selected>-select-</option>");

  LIST_FOREACH(ch2, &channels, ch_global_link) {
    if(ch2 == ch)
      continue;
    tcp_qprintf(&tq, "<option>%s</option>", ch2->ch_name);
  }
  tcp_qprintf(&tq, "</select></span><br><br>\r\n");

  tcp_qprintf(&tq, 
	      "<div style=\"width: 170px; float: left\">"
	      "Teletext rundown page: "
	      "</div>"
	      "<span style=\"width: 250px\">"
	      "<input class=\"drop\" type=\"text\" name=\"ttrp\" "
	      "maxlength=\"3\" value=\"%d\" size=\"4\" "
	      "onChange=\"this.form.submit()\"></span><br>",
	      ch->ch_teletext_rundown);

  tcp_qprintf(&tq, "<br><br>\r\n");


  tcp_qprintf(&tq, 
	      "<span style=\"font-weight:bold;\">"
	      "<center>Transports</center></span><br>"
	      "<table border=\"1\" cellspacing=\"0\" cellpadding=\"0\" "
	      "style=\"font: 75% Verdana, Arial, Helvetica, sans-serif\">");

  tcp_qprintf(&tq, 
	      "<tr>"
	      "<th width=\"45\">Priority</td>"
	      "<th width=\"60\">Scrambled</td>"
	      "<th width=\"130\">Provider</td>"
	      "<th width=\"130\">Network</td>"
	      "<th width=\"140\">Transport ID</td>"
	      "</tr>");


  LIST_FOREACH(t, &ch->ch_transports, tht_channel_link) {
    tcp_qprintf(&tq, 
		"<tr>"
		"<td><center>"
		"<input class=\"prioval\" type=\"text\" name=\"%s\" "
		"maxlength=\"4\" value=\"%d\" size=\"5\" "
		"onChange=\"this.form.submit()\">"
		"</center></td>"
		"<td><center>%s</center></td>"
		"<td><center>%s</center></td>"
		"<td><center>%s</center></td>"
		"<td><center>%s</center></td>"
		"</tr>",
		t->tht_uniquename,
		t->tht_prio,
		t->tht_scrambled ? "Yes" : "No",
		t->tht_provider,
		t->tht_network,
		t->tht_name);

  }

  tcp_qprintf(&tq, "</table>\r\n");

  tcp_qprintf(&tq, "</div>");
  box_bottom(&tq);
  tcp_qprintf(&tq, "</form>");

  html_footer(&tq);
  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}

/**
 * Update a single channel, then redirect back to the edit page
 */
static int
page_updatechannel(http_connection_t *hc, const char *remain, void *opaque)
{
  th_channel_t *ch, *ch2;
  th_transport_t *t, **tv;
  th_channel_group_t *tcg;
  const char *grp, *s;
  char buf[100];
  int pri, i, n;

  if(!html_verify_access(hc, "admin"))
    return HTTP_STATUS_UNAUTHORIZED;

  if(remain == NULL || (ch = channel_by_tag(atoi(remain))) == NULL)
    return 404;

  if((s = http_arg_get(&hc->hc_url_args, "merge")) != NULL) {
    ch2 = channel_find(s, 0, NULL);
    if(ch2 != NULL) {

      if(LIST_FIRST(&ch->ch_subscriptions) == NULL) {
	while((t = LIST_FIRST(&ch->ch_transports)) != NULL) {
	  transport_move(t, ch2);
	}
      }

      /* Redirect to new channel */
      snprintf(buf, sizeof(buf), "/editchannel/%d", ch2->ch_tag);
      http_redirect(hc, buf);
      return 0;
    }
  }

  if((s = http_arg_get(&hc->hc_url_args, "ttrp")) != NULL)
    channel_set_teletext_rundown(ch, atoi(s));

  if((grp = http_arg_get(&hc->hc_url_args, "grp")) != NULL) {
    tcg = channel_group_find(grp, 1);
    channel_set_group(ch, tcg);
  }

  /* We are going to rearrange listorder by changing priority, so we
     cannot just loop the list */

  n = 0;
  LIST_FOREACH(t, &ch->ch_transports, tht_channel_link)
    n++;

  tv = alloca(n * sizeof(th_transport_t *));
  n = 0;
  LIST_FOREACH(t, &ch->ch_transports, tht_channel_link)
    tv[n++] = t;
  
  for(i = 0; i < n; i++) {
    t = tv[i];
    s = http_arg_get(&hc->hc_url_args, t->tht_uniquename);
    if(s != NULL) {
      pri = atoi(s);
      if(pri >= 0 && pri <= 9999) {
	transport_set_priority(t, pri);
      }
    }
  }
  
  snprintf(buf, sizeof(buf), "/editchannel/%d", ch->ch_tag);
  http_redirect(hc, buf);
  return 0;
}


/**
 * Search for a specific event
 */
static int
page_search(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  epg_content_group_t *ecg, *s_ecg;
  th_channel_group_t *tcg, *s_tcg;
  th_channel_t *ch, *s_ch;
  int simple = is_client_simple(hc);
  int i, c, k;
  char escapebuf[1000];
  struct event_list events;
  event_t *e, **ev;
  struct tm a, day;
  
  const char *search  = http_arg_get(&hc->hc_url_args, "s");
  const char *autorec = http_arg_get(&hc->hc_url_args, "ar");
  const char *title   = http_arg_get(&hc->hc_url_args, "n");
  const char *content = http_arg_get(&hc->hc_url_args, "c");
  const char *chgroup = http_arg_get(&hc->hc_url_args, "g");
  const char *channel = http_arg_get(&hc->hc_url_args, "ch");
  const char *ar_name = http_arg_get(&hc->hc_url_args, "ar_name");
  const char *ar_prio = http_arg_get(&hc->hc_url_args, "ar_prio");

  if(title   != NULL && *title == 0)               title   = NULL;
  if(content != NULL && !strcmp(content, "-All-")) content = NULL;
  if(chgroup != NULL && !strcmp(chgroup, "-All-")) chgroup = NULL;
  if(channel != NULL && !strcmp(channel, "-All-")) channel = NULL;

  if(ar_name != NULL && *ar_name == 0)             ar_name = NULL;
  if(ar_prio != NULL && *ar_prio == 0)             ar_prio = NULL;

  s_ecg = content ? epg_content_group_find_by_name(content) : NULL;
  s_tcg = chgroup ? channel_group_find(chgroup, 0)          : NULL;
  s_ch  = channel ? channel_find(channel, 0, NULL)          : NULL;

  if(http_arg_get(&hc->hc_url_args, "ar_create")) {
    /* Create autorecording */

    
    if(ar_name == NULL || ar_prio == NULL || atoi(ar_prio) < 1) {
      /* Invalid arguments */

      tcp_init_queue(&tq, -1);
      html_header(&tq, "HTS/tvheadend", !simple, MAIN_WIDTH, 0, NULL);
      top_menu(hc, &tq);
      tcp_qprintf(&tq, "<form method=\"get\" action=\"/search\">");
      box_top(&tq, "box");
      tcp_qprintf(&tq, "<div class=\"content\"><br>"
		  "<b>Invalid / Missing arguments:<br><br>");
      if(ar_name == NULL)
	tcp_qprintf(&tq, 
		    "- Name is missing<br><br>");

      if(ar_prio == NULL || atoi(ar_prio) < 1)
	tcp_qprintf(&tq, 
		    "- Priority is missing or not a positive number<br><br>");
      
      tcp_qprintf(&tq, "</div>");
      box_bottom(&tq);
      html_footer(&tq);
      http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
      return 0;
    }

    /* Create autorec rule .. */
    autorec_create(ar_name, atoi(ar_prio), title, s_ecg, s_tcg, s_ch);

    /* .. and redirect user to video recorder page */
    http_redirect(hc, "/pvr");
    return 0;
  }

  tcp_init_queue(&tq, -1);
  html_header(&tq, "HTS/tvheadend", !simple, MAIN_WIDTH, 0, NULL);
  top_menu(hc, &tq);

  tcp_qprintf(&tq, 
	      "<form method=\"get\" action=\"/search\">");

  box_top(&tq, "box");

  tcp_qprintf(&tq, 
	      "<div class=\"content\">"
	      "<br>");

  /* Title */

  tcp_qprintf(&tq, 
	      "<div>"
	      "<span style=\"text-align: right; width: 120px; float: left\">"
	      "Event title:"
	      "</span>"
	      "<span style=\"width: 275px; float: left\">"
	      "<input type=\"text\" size=40 name=\"n\"");

  if(title != NULL) {
    esacpe_char(escapebuf, sizeof(escapebuf), title, '"', "&quot;");
    tcp_qprintf(&tq, " value=\"%s\"", escapebuf);
  }

  tcp_qprintf(&tq,
	      " style=\"border: 1px dotted #000000\"> "
	      "</span>");

  
  /* Specific content */

  tcp_qprintf(&tq, 
	      "<span style=\"text-align: right; width: 120px; float: left\">"
	      "Content:"
	      "</span>"
	      "<span style=\"width: 275px\">"
	      "<select name=\"c\" class=\"drop\">");
  
  tcp_qprintf(&tq, "<option%s>-All-</option>", content ? "" : " selected");

  for(i = 0; i < 16; i++) {
    ecg = epg_content_groups[i];

    if(ecg->ecg_name == NULL)
      continue;
    tcp_qprintf(&tq, "<option%s>%s</option>", 
		ecg == s_ecg ? " selected" : "", ecg->ecg_name);
  }
  tcp_qprintf(&tq, "</select></span></div><br>");



  /* Specific channel group */

  tcp_qprintf(&tq, 
	      "<div>"
	      "<span style=\"text-align: right; width: 120px; float: left\">"
	      "Channel group:"
	      "</span>"
	      "<span style=\"width: 275px; float: left\">"
	      "<select name=\"g\" class=\"drop\">");

  tcp_qprintf(&tq, "<option%s>-All-</option>", chgroup ? "" : " selected");

  TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {
    if(tcg->tcg_hidden)
      continue;
    tcp_qprintf(&tq, "<option%s>%s</option>", 
		tcg == s_tcg ? " selected" : "", tcg->tcg_name);
  }
  tcp_qprintf(&tq, "</select></span>");

  /* Specific channel */

  tcp_qprintf(&tq, 
	      "<span style=\"text-align: right; width: 120px; float: left\">"
	      "Channel:"
	      "</span>"
	      "<span style=\"width: 275px; float: left\">"
	      "<select name=\"ch\" class=\"drop\">");
  
  tcp_qprintf(&tq, "<option%s>-All-</option>", channel ? "" : " selected");

  TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {
    if(tcg->tcg_hidden)
      continue;
    
    TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link) {
      if(LIST_FIRST(&ch->ch_transports) == NULL)
	continue;
      tcp_qprintf(&tq, "<option%s>%s</option>",
		  ch == s_ch ? " selected" : "", ch->ch_name);
    }
  }
  tcp_qprintf(&tq, "</select></span></div><br><br>");


  if(autorec != NULL) {

    tcp_qprintf(&tq, "</div>");
    box_bottom(&tq);
    tcp_qprintf(&tq, "<br>");
    box_top(&tq, "box");
    tcp_qprintf(&tq, 
		"<div class=\"content\">");


    /* User want to create an autorecording, ask for supplemental fields
       and add some buttons */

    /* Name of recording */
    
    tcp_qprintf(&tq, 
		"<br><div>"
		"<span style=\"text-align: right; width: 120px; float: left\">"
		"Recording name:"
		"</span>"
		"<span style=\"width: 275px; float: left\">"
		"<input type=\"text\" size=40 name=\"ar_name\""
		" style=\"border: 1px dotted #000000\"> "
		"</span>");

    /* Priority of recorded events */

    tcp_qprintf(&tq, 
		"<span style=\"text-align: right; width: 120px; float: left\">"
		"Priority:"
		"</span>"
		"<span style=\"width: 275px; float: left\">"
		"<input type=\"text\" size=5 name=\"ar_prio\""
		" style=\"border: 1px dotted #000000\"> "
		"</span></div><br><br>");

    /* Create button */
    
    tcp_qprintf(&tq, 
		"<div>"
		"<span style=\"text-align: right; width: 120px; float: left\">"
		"<p></span>"
		"<span style=\"width: 275px; float: left\">"

		"<input type=\"submit\" class=\"knapp\" name=\"ar_create\" "
		"value=\"Create autorecording rule\">"
		"</span>");

    /* Cancel button */

    tcp_qprintf(&tq, 
		"<span style=\"text-align: right; width: 120px; float: left\">"
		"<p></span>"
		"<span style=\"width: 275px; float: left\">"
		"<input type=\"submit\" class=\"knapp\" name=\"ar_cancel\" "
		"value=\"Cancel\">"
		"</span></div><br><br>");

  } else {

    /* Search button */
    tcp_qprintf(&tq,
		"<center><input type=\"submit\" class=\"knapp\" name=\"s\" "
		"value=\"Search\"></center>");
  }

  tcp_qprintf(&tq, "</div>");
  box_bottom(&tq);
  tcp_qprintf(&tq, "<br>");

  /* output search result, if we've done a query */

  if(search != NULL || autorec != NULL) {

    box_top(&tq, "box");
    tcp_qprintf(&tq, 
		"<div class=\"content\">");

    epg_lock();
    LIST_INIT(&events);
    c = epg_search(&events, title, s_ecg, s_tcg, s_ch);
    if(c == -1) {
      tcp_qprintf(&tq,
		  "<center><b>"
		  "Event title: Regular expression syntax error"
		  "<b></center>");
    } else if(c == 0) {
      tcp_qprintf(&tq,
		  "<center><b>"
		  "No matching entries found"
		  "<b></center>");
    } else {
    
      tcp_qprintf(&tq,
		  "<div>"
		  "<span style=\"text-align: left; width: 630px; "
		  "float: left;font-weight:bold\"> %d entries found", c);

      ev = alloca(c * sizeof(event_t *));
      c = 0;
      LIST_FOREACH(e, &events, e_tmp_link)
	ev[c++] = e;
      qsort(ev, c, sizeof(event_t *), eventcmp);

      if(c > 100) {
	c = 100;
	tcp_qprintf(&tq,
		    ", %d entries shown", c);
      }

      if(autorec == NULL)
	tcp_qprintf(&tq,
		    "</span>"
		    "<span style=\"float: left\">"
		    "<input type=\"submit\" class=\"knapp\" name=\"ar\" "
		    "value=\"Create autorecording\">");

      tcp_qprintf(&tq, "</span></div>");

      memset(&day, -1, sizeof(struct tm));
      for(k = 0; k < c; k++) {
	e = ev[k];
      
	localtime_r(&e->e_start, &a);
      
	if(a.tm_wday != day.tm_wday || a.tm_mday != day.tm_mday  ||
	   a.tm_mon  != day.tm_mon  || a.tm_year != day.tm_year) {
	  memcpy(&day, &a, sizeof(struct tm));
	  tcp_qprintf(&tq, 
		      "<br><b><i>%s, %d/%d</i></b><br>",
		      days[day.tm_wday], day.tm_mday, day.tm_mon + 1);
	}
	output_event(hc, &tq, e->e_ch, e, simple, 1, 1, 700);
      }
    }
    epg_unlock();
    tcp_qprintf(&tq, "</div>");
    box_bottom(&tq);
    tcp_qprintf(&tq, "</form>");
  }


  html_footer(&tq);
  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}



/**
 * HTML user interface setup code
 */
void
htmlui_start(void)
{
  http_path_add("/", NULL, page_root);
  http_path_add("/event", NULL, page_event);
  http_path_add("/channel", NULL, page_channel);
  http_path_add("/pvr", NULL, page_pvr);
  http_path_add("/status", NULL, page_status);
  http_path_add("/chgrp", NULL, page_chgroups);
  http_path_add("/chadm", NULL, page_chadm);
  http_path_add("/chadm2", NULL, page_chadm2);
  http_path_add("/editchannel", NULL, page_editchannel);
  http_path_add("/updatechannel", NULL, page_updatechannel);
  http_path_add("/css.css", NULL, page_css);
  http_path_add("/search", NULL, page_search);
}
