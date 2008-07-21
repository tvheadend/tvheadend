/*
 *  tvheadend, WEBUI / HTML user interface
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
#include "webui.h"
#include "access.h"
#include "epg.h"
#include "pvr.h"

#define ACCESS_SIMPLE \
(ACCESS_WEB_INTERFACE | ACCESS_RECORDER_VIEW | ACCESS_RECORDER_CHANGE)

static struct strtab recstatustxt[] = {
  { "Recording scheduled", HTSTV_PVR_STATUS_SCHEDULED      },
  { "Recording",           HTSTV_PVR_STATUS_RECORDING      },
  { "Recording completed", HTSTV_PVR_STATUS_DONE           },
  { "Recording aborted",   HTSTV_PVR_STATUS_ABORTED        },
  { "No transponder",      HTSTV_PVR_STATUS_NO_TRANSPONDER },
  { "File error",          HTSTV_PVR_STATUS_FILE_ERROR     },
  { "Disk full",           HTSTV_PVR_STATUS_DISK_FULL      },
  { "Buffer error",        HTSTV_PVR_STATUS_BUFFER_ERROR   },
};

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
pvrcmp(const void *A, const void *B)
{
  const pvr_rec_t *a = *(const pvr_rec_t **)A;
  const pvr_rec_t *b = *(const pvr_rec_t **)B;

  return a->pvrr_start - b->pvrr_start;
}

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



/**
 * Root page, we direct the client to different pages depending
 * on if it is a full blown browser or just some mobile app
 */
static int
page_root(http_connection_t *hc, http_reply_t *hr, 
	  const char *remain, void *opaque)
{
  if(is_client_simple(hc)) {
    http_redirect(hc, hr, "/simple.html");
  } else {
    http_redirect(hc, hr, "/ajax/index.html");
  }
  return 0;
}

/**
 * Root page, we direct the client to different pages depending
 * on if it is a full blown browser or just some mobile app
 */
static int
page_simple(http_connection_t *hc, http_reply_t *hr, 
	  const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hr->hr_q;
  const char *s = http_arg_get(&hc->hc_req_args, "s");
  struct event_list events;
  event_t *e, **ev;
  int c, k, i;
  struct tm a, b, day;
  time_t stop;
  pvr_rec_t *pvrr, **pv;
  const char *rstatus;

  htsbuf_qprintf(hq, "<html>");
  htsbuf_qprintf(hq, "<body>");

  htsbuf_qprintf(hq, "<form>");
  htsbuf_qprintf(hq, "Event: <input type=\"text\" ");
  if(s != NULL)
    htsbuf_qprintf(hq, "value=\"%s\" ", s);
  
  htsbuf_qprintf(hq, "name=\"s\">");
  htsbuf_qprintf(hq, "<input type=\"submit\" value=\"Search\">");
  
  htsbuf_qprintf(hq, "</form><hr>");

  if(s != NULL) {
    
    LIST_INIT(&events);
    c = epg_search(&events, s, NULL, NULL, NULL);

    if(c == -1) {
      htsbuf_qprintf(hq, "<b>Event title: Regular expression syntax error</b>");
    } else if(c == 0) {
      htsbuf_qprintf(hq, "<b>No matching entries found</b>");
    } else {

      htsbuf_qprintf(hq, "<b>%d entries found", c);

      ev = alloca(c * sizeof(event_t *));
      c = 0;
      LIST_FOREACH(e, &events, e_tmp_link)
	ev[c++] = e;

      qsort(ev, c, sizeof(event_t *), eventcmp);

      if(c > 25) {
	c = 25;
	htsbuf_qprintf(hq, ", %d entries shown", c);
      }

      htsbuf_qprintf(hq, "</b>");

      memset(&day, -1, sizeof(struct tm));
      for(k = 0; k < c; k++) {
	e = ev[k];
      
	localtime_r(&e->e_start, &a);
        stop = e->e_start + e->e_duration;
	localtime_r(&stop, &b);

	if(a.tm_wday != day.tm_wday || a.tm_mday != day.tm_mday  ||
	   a.tm_mon  != day.tm_mon  || a.tm_year != day.tm_year) {
	  memcpy(&day, &a, sizeof(struct tm));
	  htsbuf_qprintf(hq, 
		      "<br><i>%s, %d/%d</i><br>",
		      days[day.tm_wday], day.tm_mday, day.tm_mon + 1);
	}

	pvrr = pvr_get_by_entry(e);
	rstatus = pvrr != NULL ? val2str(pvrr->pvrr_status,
					 recstatustxt) : NULL;

	htsbuf_qprintf(hq, 
		    "<a href=\"/eventinfo/%d\">"
		    "%02d:%02d-%02d:%02d&nbsp;%s%s%s</a><br>",
		    e->e_tag,
		    a.tm_hour, a.tm_min, b.tm_hour, b.tm_min,
		    e->e_title,
		    rstatus ? "&nbsp;" : "", rstatus ?: "");
      }
    }
    htsbuf_qprintf(hq, "<hr>");
  }

  htsbuf_qprintf(hq, "<b>Recorder log:</b><br>\n");

  c = 0;
  LIST_FOREACH(pvrr, &pvrr_global_list, pvrr_global_link)
    c++;

  if(c == 0) {
    htsbuf_qprintf(hq, "No entries<br>");
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
      htsbuf_qprintf(hq, "<br><i>%s, %d/%d</i><br>",
		  days[day.tm_wday], day.tm_mday, day.tm_mon + 1);
    }

    rstatus = val2str(pvrr->pvrr_status, recstatustxt);


    htsbuf_qprintf(hq, "<a href=\"/pvrinfo/%d\">", pvrr->pvrr_ref);
    
    htsbuf_qprintf(hq, 
		"%02d:%02d-%02d:%02d&nbsp; %s",
		a.tm_hour, a.tm_min, b.tm_hour, b.tm_min, pvrr->pvrr_title);

    htsbuf_qprintf(hq, "</a>");

    htsbuf_qprintf(hq, "<br>(%s)<br><br>", rstatus);
  }

  htsbuf_qprintf(hq, "</body></html>");
  http_output_html(hc, hr);
  return 0;
}

/**
 * Event info, deliver info about the given event
 */
static int
page_einfo(http_connection_t *hc, http_reply_t *hr, 
	   const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hr->hr_q;
  event_t *e;
  struct tm a, b;
  time_t stop;
  pvr_rec_t *pvrr = NULL;
  tv_pvr_status_t pvrstatus;
  const char *rstatus;

  if(remain == NULL || (e = epg_event_find_by_tag(atoi(remain))) == NULL)
    return 404;

  pvrr = pvr_get_by_entry(e);

  if((http_arg_get(&hc->hc_req_args, "rec")) != NULL) {
    pvrr = pvr_schedule_by_event(e, hc->hc_username ?: "anonymous");
  } else if(pvrr && (http_arg_get(&hc->hc_req_args, "clear")) != NULL) {
    pvr_clear(pvrr);
    pvrr = NULL;
  } else if(pvrr && (http_arg_get(&hc->hc_req_args, "cancel")) != NULL) {
    pvr_abort(pvrr);
  }

  htsbuf_qprintf(hq, "<html>");
  htsbuf_qprintf(hq, "<body>");

  localtime_r(&e->e_start, &a);
  stop = e->e_start + e->e_duration;
  localtime_r(&stop, &b);

  htsbuf_qprintf(hq, 
	      "%s, %d/%d %02d:%02d - %02d:%02d<br>",
	      days[a.tm_wday], a.tm_mday, a.tm_mon + 1,
	      a.tm_hour, a.tm_min, b.tm_hour, b.tm_min);

  htsbuf_qprintf(hq, "<hr><b>\"%s\": \"%s\"</b><br><br>",
	      e->e_channel->ch_name, e->e_title);
  
  pvrstatus = pvrr != NULL ? pvrr->pvrr_status : HTSTV_PVR_STATUS_NONE;

  if((rstatus = val2str(pvrstatus, recstatustxt)) != NULL)
    htsbuf_qprintf(hq, "Recording status: %s<br>", rstatus);

  htsbuf_qprintf(hq, "<form method=\"post\" action=\"/eventinfo/%d\">", e->e_tag);

  switch(pvrstatus) {
  case HTSTV_PVR_STATUS_SCHEDULED:
    htsbuf_qprintf(hq, "<input type=\"submit\" "
		"name=\"clear\" value=\"Remove from schedule\">");
    break;
    
  case HTSTV_PVR_STATUS_RECORDING:
    htsbuf_qprintf(hq, "<input type=\"submit\" "
		"name=\"cancel\" value=\"Cancel recording\">");
    break;

  case HTSTV_PVR_STATUS_NONE:
    htsbuf_qprintf(hq, "<input type=\"submit\" "
		"name=\"rec\" value=\"Record\">");
    break;

  default:
    htsbuf_qprintf(hq, "<input type=\"submit\" "
		"name=\"clear\" value=\"Clear error status\">");
    break;
  }

  htsbuf_qprintf(hq, "</form>");
  htsbuf_qprintf(hq, "%s", e->e_desc);

  htsbuf_qprintf(hq, "<hr><a href=\"/simple.html\">New search</a><br>");
  htsbuf_qprintf(hq, "</body></html>");
  http_output_html(hc, hr);
  return 0;
}


/**
 * PVR info, deliver info about the given PVR entry
 */
static int
page_pvrinfo(http_connection_t *hc, http_reply_t *hr, 
	     const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hr->hr_q;
  struct tm a, b;
  time_t stop;
  pvr_rec_t *pvrr = NULL;
  tv_pvr_status_t pvrstatus;
  const char *rstatus;

  if(remain == NULL || (pvrr = pvr_get_tag_entry(atoi(remain))) == NULL)
    return 404;

  if((http_arg_get(&hc->hc_req_args, "clear")) != NULL) {
    pvr_clear(pvrr);
    pvrr = NULL;
    http_redirect(hc, hr, "/simple.html");
    return 0;
  } else if((http_arg_get(&hc->hc_req_args, "cancel")) != NULL) {
    pvr_abort(pvrr);
  }

  htsbuf_qprintf(hq, "<html>");
  htsbuf_qprintf(hq, "<body>");

  localtime_r(&pvrr->pvrr_start, &a);
  stop = pvrr->pvrr_start + pvrr->pvrr_stop;
  localtime_r(&stop, &b);

  htsbuf_qprintf(hq, 
	      "%s, %d/%d %02d:%02d - %02d:%02d<br>",
	      days[a.tm_wday], a.tm_mday, a.tm_mon + 1,
	      a.tm_hour, a.tm_min, b.tm_hour, b.tm_min);

  htsbuf_qprintf(hq, "<hr><b>\"%s\": \"%s\"</b><br><br>",
	      pvrr->pvrr_channel->ch_name, pvrr->pvrr_title);
  
  pvrstatus = pvrr->pvrr_status;

  if((rstatus = val2str(pvrstatus, recstatustxt)) != NULL)
    htsbuf_qprintf(hq, "Recording status: %s<br>", rstatus);

  htsbuf_qprintf(hq, "<form method=\"post\" action=\"/pvrinfo/%d\">", 
	      pvrr->pvrr_ref);

  switch(pvrstatus) {
  case HTSTV_PVR_STATUS_SCHEDULED:
    htsbuf_qprintf(hq, "<input type=\"submit\" "
		"name=\"clear\" value=\"Remove from schedule\">");
    break;
    
  case HTSTV_PVR_STATUS_RECORDING:
    htsbuf_qprintf(hq, "<input type=\"submit\" "
		"name=\"cancel\" value=\"Cancel recording\">");
    break;

  case HTSTV_PVR_STATUS_DONE:
    htsbuf_qprintf(hq, "<input type=\"submit\" "
		"name=\"clear\" value=\"Remove from log\">");
    break;

  default:
    htsbuf_qprintf(hq, "<input type=\"submit\" "
		"name=\"clear\" value=\"Clear error status\">");
    break;
  }

  htsbuf_qprintf(hq, "</form>");
  htsbuf_qprintf(hq, "%s", pvrr->pvrr_desc);

  htsbuf_qprintf(hq, "<hr><a href=\"/simple.html\">New search</a><br>");
  htsbuf_qprintf(hq, "</body></html>");
  http_output_html(hc, hr);
  return 0;
}


/**
 * WEB user interface
 */
void
webui_start(void)
{
  http_path_add("/", NULL, page_root, ACCESS_WEB_INTERFACE);

  http_path_add("/simple.html", NULL, page_simple,  ACCESS_SIMPLE);
  http_path_add("/eventinfo",   NULL, page_einfo,   ACCESS_SIMPLE);
  http_path_add("/pvrinfo",     NULL, page_pvrinfo, ACCESS_SIMPLE);
  
}
