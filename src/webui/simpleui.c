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
#include <limits.h>

#include "tvheadend.h"
#include "http.h"
#include "webui.h"
#include "access.h"
#include "epg.h"
#include "dvr/dvr.h"
#include "config.h"

#define ACCESS_SIMPLE \
(ACCESS_WEB_INTERFACE | ACCESS_RECORDER)

static struct strtab recstatustxt[] = {
  { "Recording scheduled", DVR_SCHEDULED  },
  { "Recording",           DVR_RECORDING, },
  { "Recording completed", DVR_COMPLETED, },
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

typedef struct dvr_query_result {
  dvr_entry_t **dqr_array;
  int dqr_entries;
  int dqr_alloced;
} dvr_query_result_t;

typedef int (dvr_entry_filter)(dvr_entry_t *entry);
typedef int (dvr_entry_comparator)(const void *a, const void *b);

/**
 *
 */
static void
dvr_query_add_entry(dvr_query_result_t *dqr, dvr_entry_t *de)
{
  if(dqr->dqr_entries == dqr->dqr_alloced) {
    /* Need to alloc more space */

    dqr->dqr_alloced = MAX(100, dqr->dqr_alloced * 2);
    dqr->dqr_array = realloc(dqr->dqr_array,
			     dqr->dqr_alloced * sizeof(dvr_entry_t *));
  }
  dqr->dqr_array[dqr->dqr_entries++] = de;
}

static void
dvr_query_filter(dvr_query_result_t *dqr, dvr_entry_filter filter)
{
  dvr_entry_t *de;

  memset(dqr, 0, sizeof(dvr_query_result_t));

  LIST_FOREACH(de, &dvrentries, de_global_link)
    if (filter(de))
      dvr_query_add_entry(dqr, de);
}

static int all_filter(dvr_entry_t *entry)
{
  return 1;
}

/**
 *
 */
static void
dvr_query(dvr_query_result_t *dqr)
{
  return dvr_query_filter(dqr, all_filter);
}

/**
 *
 */
static void
dvr_query_free(dvr_query_result_t *dqr)
{
  free(dqr->dqr_array);
}

/**
 *
 */
static dvr_entry_t *
dvr_find_by_event(epg_broadcast_t *e)
{
  dvr_entry_t *de;

  if (e->channel == NULL)
    return NULL;

  LIST_FOREACH(de, &e->channel->ch_dvrs, de_channel_link)
    if (de->de_bcast == e)
      return de;
  return NULL;
}

/**
 * Sorting functions
 */
static int
dvr_sort_start_descending(const void *A, const void *B)
{
  dvr_entry_t *a = *(dvr_entry_t **)A;
  dvr_entry_t *b = *(dvr_entry_t **)B;
  return b->de_start - a->de_start;
}

#if 0
static int
dvr_sort_start_ascending(const void *A, const void *B)
{
  return -dvr_sort_start_descending(A, B);
}
#endif

/**
 *
 */
static void
dvr_query_sort_cmp(dvr_query_result_t *dqr, dvr_entry_comparator sf)
{
  if(dqr->dqr_array == NULL)
    return;

  qsort(dqr->dqr_array, dqr->dqr_entries, sizeof(dvr_entry_t *), sf);
}

static void
dvr_query_sort(dvr_query_result_t *dqr)
{
  dvr_query_sort_cmp(dqr, dvr_sort_start_descending);
}

/**
 * Root page, we direct the client to different pages depending
 * on if it is a full blown browser or just some mobile app
 */
static int
page_simple(http_connection_t *hc,
	  const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *s = http_arg_get(&hc->hc_req_args, "s");
  epg_broadcast_t *e;
  int c, k, i;
  struct tm a, b, day;
  dvr_entry_t *de;
  dvr_query_result_t dqr;
  const char *rstatus = NULL;
  const char *lang  = http_arg_get(&hc->hc_args, "Accept-Language");
  char ubuf[UUID_HEX_SIZE];

  htsbuf_qprintf(hq, "<html>");
  htsbuf_qprintf(hq, "<body>");

  htsbuf_qprintf(hq, "<form>");
  htsbuf_qprintf(hq, "Event: <input type=\"text\" ");
  if(s != NULL)
    htsbuf_qprintf(hq, "value=\"%s\" ", s);
  
  htsbuf_qprintf(hq, "name=\"s\">");
  htsbuf_qprintf(hq, "<input type=\"submit\" value=\"Search\">");
  
  htsbuf_qprintf(hq, "</form><hr>");

  pthread_mutex_lock(&global_lock);


  if(s != NULL) {
    epg_query_t eq;

    memset(&eq, 0, sizeof(eq));
    eq.lang = strdup(lang);
    eq.fulltext = 1;
    eq.stitle = s ? strdup(s) : NULL;

    //Note: force min/max durations for this interface to 0 and INT_MAX seconds respectively
    epg_query(&eq, hc->hc_access);

    c = eq.entries;

    if(eq.entries == 0) {
      htsbuf_qprintf(hq, "<b>No matching entries found</b>");
    } else {

      htsbuf_qprintf(hq, "<b>%d entries found", c);

      if(c > 25) {
	c = 25;
	htsbuf_qprintf(hq, ", %d entries shown", c);
      }

      htsbuf_qprintf(hq, "</b>");

      memset(&day, -1, sizeof(struct tm));
      for(k = 0; k < c; k++) {
	e = eq.result[k];
      
	localtime_r(&e->start, &a);
	localtime_r(&e->stop, &b);

	if(a.tm_wday != day.tm_wday || a.tm_mday != day.tm_mday  ||
	   a.tm_mon  != day.tm_mon  || a.tm_year != day.tm_year) {
	  memcpy(&day, &a, sizeof(struct tm));
	  htsbuf_qprintf(hq, 
		      "<br><i>%s, %d/%d</i><br>",
		      days[day.tm_wday], day.tm_mday, day.tm_mon + 1);
	}

	de = dvr_find_by_event(e);
	rstatus = de != NULL ? val2str(de->de_sched_state,
				       recstatustxt) : NULL;

        s = epg_broadcast_get_title(e, lang);
	htsbuf_qprintf(hq, 
		    "<a href=\"/eventinfo/%u\">"
		    "%02d:%02d-%02d:%02d&nbsp;%s%s%s</a><br>",
		    e->id,
		    a.tm_hour, a.tm_min, b.tm_hour, b.tm_min,
        s ?: "",
		    rstatus ? "&nbsp;" : "", rstatus ?: "");
      }
    }
    htsbuf_qprintf(hq, "<hr>");
    epg_query_free(&eq);
  }


  htsbuf_qprintf(hq, "<b>Recorder log:</b><br>\n");

  dvr_query(&dqr);
  dvr_query_sort(&dqr);

  c = dqr.dqr_entries;

  if(c == 0) {
    htsbuf_qprintf(hq, "No entries<br>");
  }
  
  memset(&day, -1, sizeof(struct tm));

  for(i = 0; i < c; i++) {
    de = dqr.dqr_array[i];

    localtime_r(&de->de_start, &a);
    localtime_r(&de->de_stop, &b);


    if(a.tm_wday != day.tm_wday || a.tm_mday != day.tm_mday  ||
       a.tm_mon  != day.tm_mon  || a.tm_year != day.tm_year) {
      memcpy(&day, &a, sizeof(struct tm));
      htsbuf_qprintf(hq, "<br><i>%s, %d/%d</i><br>",
		  days[day.tm_wday], day.tm_mday, day.tm_mon + 1);
    }

    rstatus = val2str(de->de_sched_state, recstatustxt);


    htsbuf_qprintf(hq, "<a href=\"/pvrinfo/%s\">", idnode_uuid_as_str(&de->de_id, ubuf));
    
    htsbuf_qprintf(hq, 
		"%02d:%02d-%02d:%02d&nbsp; %s",
		a.tm_hour, a.tm_min, b.tm_hour, b.tm_min, lang_str_get(de->de_title, NULL));

    htsbuf_qprintf(hq, "</a>");

    htsbuf_qprintf(hq, "&nbsp; (%s)<br><br>", rstatus);
  }

  dvr_query_free(&dqr);

  pthread_mutex_unlock(&global_lock);

  htsbuf_qprintf(hq, "</body></html>");
  http_output_html(hc);
  return 0;
}

/**
 * Event info, deliver info about the given event
 */
static int
page_einfo(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  epg_broadcast_t *e;
  struct tm a, b;
  dvr_entry_t *de;
  const char *rstatus;
  dvr_entry_sched_state_t dvr_status;
  const char *lang = http_arg_get(&hc->hc_args, "Accept-Language");
  const char *s;

  pthread_mutex_lock(&global_lock);

  if(remain == NULL || (e = epg_broadcast_find_by_id(strtoll(remain, NULL, 10))) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  de = dvr_find_by_event(e);

  if((http_arg_get(&hc->hc_req_args, "rec")) != NULL) {
    de = dvr_entry_create_by_event(1, NULL, e, 0, 0, hc->hc_username ?: NULL,
                                   hc->hc_representative ?: NULL, NULL,
                                   DVR_PRIO_NORMAL, DVR_RET_DVRCONFIG,
				   DVR_RET_DVRCONFIG, "simpleui");
  } else if(de != NULL && (http_arg_get(&hc->hc_req_args, "cancel")) != NULL) {
    de = dvr_entry_cancel(de, 0);
  }

  htsbuf_qprintf(hq, "<html>");
  htsbuf_qprintf(hq, "<body>");

  localtime_r(&e->start, &a);
  localtime_r(&e->stop, &b);

  htsbuf_qprintf(hq, 
	      "%s, %d/%d %02d:%02d - %02d:%02d<br>",
	      days[a.tm_wday], a.tm_mday, a.tm_mon + 1,
	      a.tm_hour, a.tm_min, b.tm_hour, b.tm_min);

  s = epg_episode_get_title(e->episode, lang);
  htsbuf_qprintf(hq, "<hr><b>\"%s\": \"%s\"</b><br><br>",
	      channel_get_name(e->channel), s ?: "");
  
  dvr_status = de != NULL ? de->de_sched_state : DVR_NOSTATE;

  if((rstatus = val2str(dvr_status, recstatustxt)) != NULL)
    htsbuf_qprintf(hq, "Recording status: %s<br>", rstatus);

  htsbuf_qprintf(hq, "<form method=\"post\" action=\"/eventinfo/%u\">",
		 e->id);

  switch(dvr_status) {
  case DVR_SCHEDULED:
    htsbuf_qprintf(hq, "<input type=\"submit\" "
		"name=\"cancel\" value=\"Remove from schedule\">");
    break;
    
  case DVR_RECORDING:
    htsbuf_qprintf(hq, "<input type=\"submit\" "
		"name=\"cancel\" value=\"Cancel recording\">");
    break;

  case DVR_NOSTATE:
    htsbuf_qprintf(hq, "<input type=\"submit\" "
		"name=\"rec\" value=\"Record\">");
    break;

  case DVR_MISSED_TIME:
  case DVR_COMPLETED:
    break;
  }

  htsbuf_qprintf(hq, "</form>");

  if ( (s = epg_broadcast_get_description(e, lang)) )
    htsbuf_qprintf(hq, "%s", s);
  else if ( (s = epg_broadcast_get_summary(e, lang)) )
    htsbuf_qprintf(hq, "%s", s);
  

  pthread_mutex_unlock(&global_lock);

  htsbuf_qprintf(hq, "<hr><a href=\"/simple.html\">To main page</a><br>");
  htsbuf_qprintf(hq, "</body></html>");
  http_output_html(hc);
  return 0;
}

/**
 * PVR info, deliver info about the given PVR entry
 */
static int
page_pvrinfo(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  struct tm a, b;
  dvr_entry_t *de;
  const char *rstatus;
  char ubuf[UUID_HEX_SIZE];

  pthread_mutex_lock(&global_lock);

  if(remain == NULL || (de = dvr_entry_find_by_id(atoi(remain))) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }
  if((http_arg_get(&hc->hc_req_args, "clear")) != NULL) {
    de = dvr_entry_cancel(de, 0);
  } else if((http_arg_get(&hc->hc_req_args, "cancel")) != NULL) {
    de = dvr_entry_cancel(de, 0);
  }

  if(de == NULL) {
    pthread_mutex_unlock(&global_lock);
    http_redirect(hc, "/simple.html", &hc->hc_req_args, 0);
    return 0;
  }

  htsbuf_qprintf(hq, "<html>");
  htsbuf_qprintf(hq, "<body>");

  localtime_r(&de->de_start, &a);
  localtime_r(&de->de_stop, &b);

  htsbuf_qprintf(hq, 
	      "%s, %d/%d %02d:%02d - %02d:%02d<br>",
	      days[a.tm_wday], a.tm_mday, a.tm_mon + 1,
	      a.tm_hour, a.tm_min, b.tm_hour, b.tm_min);

  htsbuf_qprintf(hq, "<hr><b>\"%s\": \"%s\"</b><br><br>",
	      DVR_CH_NAME(de), lang_str_get(de->de_title, NULL));
  
  if((rstatus = val2str(de->de_sched_state, recstatustxt)) != NULL)
    htsbuf_qprintf(hq, "Recording status: %s<br>", rstatus);

  htsbuf_qprintf(hq, "<form method=\"post\" action=\"/pvrinfo/%s\">",
	         idnode_uuid_as_str(&de->de_id, ubuf));

  switch(de->de_sched_state) {
  case DVR_SCHEDULED:
    htsbuf_qprintf(hq, "<input type=\"submit\" "
		"name=\"clear\" value=\"Remove from schedule\">");
    break;
    
  case DVR_RECORDING:
    htsbuf_qprintf(hq, "<input type=\"submit\" "
		"name=\"cancel\" value=\"Cancel recording\">");
    break;

  case DVR_COMPLETED:
  case DVR_MISSED_TIME:
  case DVR_NOSTATE:
    break;
  }

  htsbuf_qprintf(hq, "</form>");
  htsbuf_qprintf(hq, "%s", lang_str_get(de->de_desc, NULL));

  pthread_mutex_unlock(&global_lock);

  htsbuf_qprintf(hq, "<hr><a href=\"/simple.html\">To main page</a><br>");
  htsbuf_qprintf(hq, "</body></html>");
  http_output_html(hc);
  return 0;
}

/**
 * 
 */
static int
page_status(http_connection_t *hc,
	    const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  int c, i, cc, timeleft, timelefttemp;
  struct tm a, b;
  dvr_entry_t *de;
  dvr_query_result_t dqr;
  const char *rstatus;
  time_t     now;
  char buf[500];

#ifdef ENABLE_GETLOADAVG
  int loads;
  double avg[3];
#endif

  htsbuf_qprintf(hq, "<?xml version=\"1.0\"?>\n"
                 "<currentload>\n");

#ifdef ENABLE_GETLOADAVG
  loads = getloadavg (avg, 3); 
  if (loads == -1) {
        tvhlog(LOG_DEBUG, "webui",  "Error getting load average from getloadavg()");
        loads = 0;
        /* should we return an error or a 0 on error */
        htsbuf_qprintf(hq, "<systemload>0</systemload>\n");
  } else {
        htsbuf_qprintf(hq, "<systemload>%f,%f,%f</systemload>\n",avg[0],avg[1],avg[2]);
  };
#endif
  htsbuf_qprintf(hq,"<recordings>\n");

  pthread_mutex_lock(&global_lock);

  dvr_query(&dqr);
  dvr_query_sort(&dqr);

  c = dqr.dqr_entries;
  cc = 0;
  timeleft = INT_MAX;

  now = time(NULL);

  for(i = 0; i < c; i++) {
    de = dqr.dqr_array[i];

    if (DVR_SCHEDULED == de->de_sched_state)
    {
      timelefttemp = (int) ((dvr_entry_get_start_time(de, 0) - now) / 60); // output minutes
      if (timelefttemp < timeleft)
        timeleft = timelefttemp;
    }
    else if (DVR_RECORDING == de->de_sched_state)
    {
      localtime_r(&de->de_start, &a);
      localtime_r(&de->de_stop, &b);

      html_escape(buf, lang_str_get(de->de_title, NULL), sizeof(buf));
      htsbuf_qprintf(hq, 
		    "<recording>\n"
		     "<start>\n"
		     "<date>%d/%02d/%02d</date>\n"
		     "<time>%02d:%02d</time>\n"
		     "<unixtime>%"PRItime_t"</unixtime>\n"
		     "<extra_start>%"PRItime_t"</extra_start>\n"
		     "</start>\n"
		     "<stop>\n"
		     "<date>%d/%02d/%02d</date>\n"
		     "<time>%02d:%02d</time>\n"
		     "<unixtime>%"PRItime_t"</unixtime>\n"
		     "<extra_stop>%"PRItime_t"</extra_stop>\n"
		     "</stop>\n"
		     "<title>%s</title>\n",
		     a.tm_year + 1900, a.tm_mon + 1, a.tm_mday,
		     a.tm_hour, a.tm_min, 
		     de->de_start, 
		     (time_t)dvr_entry_get_extra_time_pre(de),
		     b.tm_year+1900, b.tm_mon + 1, b.tm_mday,
		     b.tm_hour, b.tm_min, 
		     de->de_stop, 
		     (time_t)dvr_entry_get_extra_time_post(de),
         buf);

      rstatus = val2str(de->de_sched_state, recstatustxt);
      html_escape(buf, rstatus, sizeof(buf));
      htsbuf_qprintf(hq, "<status>%s</status>\n</recording>\n", buf);
      cc++;
      timeleft = -1;
    }
  }

  if ((cc==0) && (timeleft < INT_MAX)) {
    htsbuf_qprintf(hq, "<recording>\n<next>%d</next>\n</recording>\n",timeleft);
  }

  dvr_query_free(&dqr);

  htsbuf_qprintf(hq, "</recordings>\n<subscriptions>");
  htsbuf_qprintf(hq, "%d</subscriptions>\n",subscriptions_active());

  pthread_mutex_unlock(&global_lock);

  htsbuf_qprintf(hq, "</currentload>");
  http_output_content(hc, "text/xml");

  return 0;
}

/**
 * flush epgdb to disk on call
 */
static int
page_epgsave(http_connection_t *hc,
	    const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;

  htsbuf_qprintf(hq, "<?xml version=\"1.0\"?>\n"
                 "<epgflush>1</epgflush>\n");

  pthread_mutex_lock(&global_lock);
  epg_save();
  pthread_mutex_unlock(&global_lock);

  http_output_content(hc, "text/xml");

  return 0;
}



/**
 * Simple web ui
 */
void
simpleui_start(void)
{
  http_path_add("/simple.html", NULL, page_simple,  ACCESS_SIMPLE);
  http_path_add("/eventinfo",   NULL, page_einfo,   ACCESS_SIMPLE);
  http_path_add("/pvrinfo",     NULL, page_pvrinfo, ACCESS_SIMPLE);
  http_path_add("/status.xml",  NULL, page_status,  ACCESS_SIMPLE);
  http_path_add("/epgsave",	NULL, page_epgsave,     ACCESS_SIMPLE);
}
