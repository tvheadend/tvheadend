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





static void
html_header(tcp_queue_t *tq, const char *title, int javascript, int width,
	    int autorefresh)
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
	      "<style type=\"text/css\">\r\n"
	      "<!--\r\n"
	      "img { border: 0px; }\r\n"
	      "a:link { text-decoration: none}\r\n"
	      "a:visited { text-decoration: none}\r\n"
	      "a:active { text-decoration: none}\r\n"
	      "a:link { text-decoration: none; color: #000000}\r\n"
	      "a:visited { text-decoration: none; color: #000000}\r\n"
	      "a:active { text-decoration: none; color: #000000}\r\n"
	      "a:hover { text-decoration: underline; color: CC3333}\r\n"
	      ""
	      "body {margin: 4px 4px; "
	      "font: 75% Verdana, Arial, Helvetica, sans-serif; "
	      "%s margin-right: auto; margin-left: auto;}\r\n"
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
	      ".statuscont {float: left; margin: 4px; width: 400px}\r\n"
	      ""
	      ".logo {padding: 2px; width: 60px; height: 56px; "
	      "float: left};\r\n"
	      ""
	      ".over {float: left}\r\n"
	      ".toptxt {float: left; width: 165px; text-align: center}\r\n"
	      ""
	      "#meny {margin: 0; padding: 0}\r\n"
	      "#meny li{display: inline; list-style-type: none;}\r\n"
	      "#meny a{padding: 1.15em 0.8em; text-decoration: none;}\r\n"
	      "-->\r\n"
	      "</style>", w);

  if(javascript) {
    tcp_qprintf(tq, 
		"<script type=\"text/javascript\" "
		"src=\"http://www.olebyn.nu/hts/overlib.js\"></script>\r\n");



    tcp_qprintf(tq,
		"<script language=\"javascript\">\r\n"
		"<!-- Begin\r\n"
		"function epop() {\r\n"
		"  props=window.open(epop.arguments[0],"
		"'poppage', 'toolbars=0, scrollbars=0, location=0, "
		"statusbars=0, menubars=0, resizable=0, "
		"width=600, height=300 left = 100, top = 100');\r\n}\r\n"
		"// End -->\r\n"
		"</script>\r\n");
  }
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
  tcp_qprintf(tq, "<div style=\"width: 700px; "
	      "margin-left: auto; margin-right: auto\">");

  box_top(tq, "box");

  tcp_qprintf(tq, 
	      "<div class=\"content\">"
	      "<ul id=\"meny\">");


  tcp_qprintf(tq, "<li><a href=\"/\">TV Guide</a></li>");

  if(html_verify_access(hc, "record-events"))
    tcp_qprintf(tq, "<li><a href=\"/pvrlog\">Recordings</a></li>");
  
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
	     event_t *e, int simple)
{
  char title[100];
  char bufa[4000];
  char overlibstuff[4000];
  struct tm a, b;
  time_t stop;
  event_t *cur;
  tv_pvr_status_t pvrstatus;
  const char *pvr_txt, *pvr_color;

  localtime_r(&e->e_start, &a);
  stop = e->e_start + e->e_duration;
  localtime_r(&stop, &b);

  pvrstatus = pvr_prog_status(e);

  cur = epg_event_get_current(ch);

  if(!simple && e->e_desc != NULL) {
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

  esacpe_char(title, sizeof(title), e->e_title, '"', "'");

  tcp_qprintf(tq, 
	      "<div>"
	      "<a href=\"%s\" %s>"
	      "<span style=\"width: %dpx;float: left%s\">"
	      "%02d:%02d - %02d:%02d</span>"
	      "<span style=\"overflow: hidden; height: 15px; width: %dpx; "
	      "float: left%s\">%s</span></a>",
	      bufa,
	      overlibstuff,
	      simple ? 80 : 100,
	      e == cur ? ";font-weight:bold" : "",
	      a.tm_hour, a.tm_min, b.tm_hour, b.tm_min,
	      simple ? 100 : 350,
	      e == cur ? ";font-weight:bold" : "",
	      title
	      );

  if(!pvrstatus_to_html(pvrstatus, &pvr_txt, &pvr_color)) {
    tcp_qprintf(tq, 
		"<span style=\"font-style:italic;color:%s;font-weight:bold\">"
		"%s</span></div><br>",
		pvr_color, pvr_txt);
  } else {
    tcp_qprintf(tq, "</div><br>");
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

  html_header(&tq, "HTS/tvheadend", !simple, 700, i);
  top_menu(hc, &tq);

  TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {

    box_top(&tq, "box");

    tcp_qprintf(&tq, 
		"<div class=\"contentbig\"><center><b>%s</b></center></div>",
		tcg->tcg_name);
    box_bottom(&tq);
    tcp_qprintf(&tq, "<br>");

    TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link) {
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

      if(tvheadend_streaming_host != NULL) {
	tcp_qprintf(&tq,
		    "<i><a href=\"rtsp://%s:%d/%s\">Watch live</a></i><br>",
		    tvheadend_streaming_host, http_port, ch->ch_sname);
      } else {
	tcp_qprintf(&tq, "<br>");
      }

      e = epg_event_find_current_or_upcoming(ch);

      for(i = 0; i < 3 && e != NULL; i++) {

	output_event(hc, &tq, ch, e, simple);
	e = TAILQ_NEXT(e, e_link);
      }

      tcp_qprintf(&tq, "</div></div>");
      box_bottom(&tq);
      tcp_qprintf(&tq, "<br>\r\n");
    }
  }
  epg_unlock();

  html_footer(&tq);
  http_output_queue(hc, &tq, "text/html; charset=UTF-8");
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

  html_header(&tq, "HTS/tvheadend", !simple, 700, 0);

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
	  output_event(hc, &tq, ch, e, simple);
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
  http_output_queue(hc, &tq, "text/html; charset=UTF-8");
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
  recop_t cmd = -1;
  tv_pvr_status_t pvrstatus;
  const char *pvr_txt, *pvr_color;
  
  if(!html_verify_access(hc, "browse-events"))
    return HTTP_STATUS_UNAUTHORIZED;

  epg_lock();
  e = epg_event_find_by_tag(eventid);
  if(e == NULL) {
    epg_unlock();
    return 404;
  }


  if(http_arg_get(&hc->hc_url_args, "rec")) {
    if(!html_verify_access(hc, "record-events")) {
      epg_unlock();
      return HTTP_STATUS_UNAUTHORIZED;
    }
    cmd = RECOP_ONCE;
  }

  if(http_arg_get(&hc->hc_url_args, "cancel")) {
    if(!html_verify_access(hc, "record-events")) {
      epg_unlock();
      return HTTP_STATUS_UNAUTHORIZED;
    }
    cmd = RECOP_CANCEL;
  }

  if(cmd != -1)
    pvr_event_record_op(e->e_ch, e, cmd);

  pvrstatus = pvr_prog_status(e);

  localtime_r(&e->e_start, &a);
  stop = e->e_start + e->e_duration;
  localtime_r(&stop, &b);

  tcp_init_queue(&tq, -1);

  html_header(&tq, "HTS/tvheadend", 0, 700, 0);
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
		  "<input type=\"submit\" name=\"cancel\" "
		  "value=\"Cancel recording\">");
      break;


    case HTSTV_PVR_STATUS_NONE:
      tcp_qprintf(&tq,
		  "<input type=\"submit\" name=\"rec\" "
		  "value=\"Record\">");
      break;

    default:
      tcp_qprintf(&tq,
		  "<input type=\"submit\" name=\"cancel\" "
		  "value=\"Clear error status\">");
      break;

    }
  }

  tcp_qprintf(&tq, "</div></div></div>");
  box_bottom(&tq);
  tcp_qprintf(&tq, "</form>\r\n");
  html_footer(&tq);
  http_output_queue(hc, &tq, "text/html; charset=UTF-8");

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
 * PVR log
 */
static int
page_pvrlog(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  int simple = is_client_simple(hc);
  pvr_rec_t *pvrr;
  event_t *e;
  char escapebuf[4000], href[4000];
  char title[100];
  char channel[100];
  struct tm a, b, day;
  const char *pvr_txt, *pvr_color;
  int c, i;
  pvr_rec_t **pv;

  if(!html_verify_access(hc, "record-events"))
    return HTTP_STATUS_UNAUTHORIZED;

  tcp_init_queue(&tq, -1);
  html_header(&tq, "HTS/tvheadend", 0, 700, 0);
  top_menu(hc, &tq);

  box_top(&tq, "box");

  epg_lock();

  tcp_qprintf(&tq, "<div class=\"content\">");

  c = 0;
  LIST_FOREACH(pvrr, &pvrr_global_list, pvrr_global_link)
    c++;

  pv = alloca(c * sizeof(pvr_rec_t *));

  i = 0;
  LIST_FOREACH(pvrr, &pvrr_global_list, pvrr_global_link)
    pv[i++] = pvrr;


  qsort(pv, i, sizeof(pvr_rec_t *), pvrcmp);

  memset(&day, -1, sizeof(struct tm));

  for(i = 0; i < c; i++) {
    pvrr = pv[i];

    e = epg_event_find_by_time(pvrr->pvrr_channel, pvrr->pvrr_start);

    if(e != NULL && !simple && e->e_desc != NULL) {
      esacpe_char(escapebuf, sizeof(escapebuf), e->e_desc, '\'', "");
    
      snprintf(href, sizeof(href), 
	       "<a href=\"/event/%d\" onmouseover=\"return overlib('%s')\" "
	       "onmouseout=\"return nd();\">",
	       e->e_tag,
	       escapebuf);

    } else {
      href[0] = 0;
    }

    esacpe_char(channel, sizeof(channel), 
		pvrr->pvrr_channel->ch_name, '"', "'");

    esacpe_char(title, sizeof(title), 
		pvrr->pvrr_title ?: "Unnamed recording", '"', "'");

    localtime_r(&pvrr->pvrr_start, &a);
    localtime_r(&pvrr->pvrr_stop, &b);

    if(a.tm_wday  != day.tm_wday  ||
       a.tm_mday  != day.tm_mday  ||
       a.tm_mon   != day.tm_mon   ||
       a.tm_year  != day.tm_year) {

      memcpy(&day, &a, sizeof(struct tm));

      tcp_qprintf(&tq, 
		  "<br><b><i>%s, %d/%d</i></b><br>",
		  days[day.tm_wday],
		  day.tm_mday,
		  day.tm_mon + 1);
    }


    tcp_qprintf(&tq, 
		"%s"
		"<div style=\"width: %dpx;float: left\">"
		"%s</div>"
		"<div style=\"width: %dpx;float: left\">"
		"%02d:%02d - %02d:%02d</div>"
		"<div style=\"width: %dpx; float: left\">%s</div>%s",
		href,
		simple ? 80 : 100,
		channel,
		simple ? 80 : 100,
		a.tm_hour, a.tm_min, b.tm_hour, b.tm_min,
		simple ? 100 : 250,
		title,
		href[0] ? "</a>" : "");

    if(!pvrstatus_to_html(pvrr->pvrr_status, &pvr_txt, &pvr_color)) {
      tcp_qprintf(&tq, 
		  "<div style=\"font-style:italic;color:%s;font-weight:bold\">"
		  "%s</div>",
		  pvr_color, pvr_txt);
    } else {
      tcp_qprintf(&tq, "<br>");
    } 
  }

  epg_unlock();
  tcp_qprintf(&tq, "<br></div>\r\n");

  box_bottom(&tq);
  tcp_qprintf(&tq, "</form>\r\n");
  html_footer(&tq);
  http_output_queue(hc, &tq, "text/html; charset=UTF-8");

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
  th_muxer_t *tm;
  th_muxstream_t *tms;
  char tmptxt[100];

  if(!html_verify_access(hc, "system-status"))
    return HTTP_STATUS_UNAUTHORIZED;

  tcp_init_queue(&tq, -1);

  html_header(&tq, "HTS/tvheadend", !simple, -1, 0);

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
		  tda->tda_path, tda->tda_info);
      LIST_FOREACH(tdmi, &tda->tda_muxes_active, tdmi_adapter_link) {

	tcp_qprintf(&tq,
		    "<span style=\"overflow: hidden; height: 15px; "
		    "width: 160px; float: left\">"
		    "%s"
		    "</span>",
		    tdmi->tdmi_mux->tdm_title);

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
      t1 = t->tht_dvb_mux->tdm_name;
      t2 = t->tht_dvb_adapter->tda_path;
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
      
      if((tm = s->ths_muxer) != NULL) {
	int64_t i64min = INT64_MAX;
	int64_t i64max = INT64_MIN;

	LIST_FOREACH(tms, &tm->tm_media_streams, tms_muxer_media_link) {
	  if(tms->tms_curpkt == NULL)
	    continue; /* stream is currently stale */

	  if(tms->tms_nextblock < i64min)
	    i64min = tms->tms_nextblock;

	  if(tms->tms_nextblock > i64max)
	    i64max = tms->tms_nextblock;
	}

	tcp_qprintf(&tq,
		    "Internal stream delta: %lld us<br>",
		    i64max - i64min);
      }
    }
    tcp_qprintf(&tq, "</div>");
    box_bottom(&tq);
    tcp_qprintf(&tq, "<br>");
  }

  tcp_qprintf(&tq, "</div>");

  tcp_qprintf(&tq, "</div>");
  html_footer(&tq);
  http_output_queue(hc, &tq, "text/html; charset=UTF-8");
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
    if(!strncmp(ra->key, "delgroup", 8))
      break;
  }

  if(ra != NULL) {
    tcg = channel_group_by_tag(atoi(ra->key + 8));
    if(tcg != NULL) {
      channel_group_destroy(tcg);
    }
  }

  tcp_init_queue(&tq, -1);

  html_header(&tq, "HTS/tvheadend", 0, 700, 0);
  top_menu(hc, &tq);


  TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {

    tcp_qprintf(&tq, "<form method=\"get\" action=\"/chgrp\">");

    box_top(&tq, "box");
    tcp_qprintf(&tq, "<div class=\"content3\">");

    cnt = 0;
    TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link)
      cnt++;

    tcp_qprintf(&tq, "<b>%s</b> (%d channels)<br>", tcg->tcg_name, cnt);

    if(tcg->tcg_cant_delete_me == 0) {
      tcp_qprintf(&tq, 
		  "<input type=\"submit\" name=\"delgroup%d\""
		  " value=\"Delete this group\">"
		  "</div>", tcg->tcg_tag);
    }

    tcp_qprintf(&tq, "</div>");
    box_bottom(&tq);
    tcp_qprintf(&tq, "</form><br>\r\n");
    
  }


  tcp_qprintf(&tq, "<form method=\"get\" action=\"/chgrp\">");

  box_top(&tq, "box");
  tcp_qprintf(&tq, "<div class=\"content3\">"
	      "<input type=\"text\" name=\"newgrpname\"> "
	      "<input type=\"submit\" name=\"newgrp\""
	      " value=\"Add new group\">"
	      "</div>");

  box_bottom(&tq);
  tcp_qprintf(&tq, "</form><br>\r\n");



  http_output_queue(hc, &tq, "text/html; charset=UTF-8");
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
  http_path_add("/pvrlog", NULL, page_pvrlog);
  http_path_add("/status", NULL, page_status);
  http_path_add("/chgrp", NULL, page_chgroups);
}
