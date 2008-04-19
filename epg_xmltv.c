/*
 *  Electronic Program Guide - xmltv interface
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "channels.h"
#include "epg.h"
#include "epg_xmltv.h"
#include "refstr.h"
#include "spawn.h"
#include "intercom.h"
#include "notify.h"

int xmltv_status;
struct xmltv_grabber_list xmltv_grabbers;
struct xmltv_grabber_queue xmltv_grabber_workq;
static pthread_mutex_t xmltv_work_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t xmltv_work_cond = PTHREAD_COND_INITIALIZER;


static xmltv_channel_t *
xc_find(xmltv_grabber_t *xg, const char *name)
{
  xmltv_channel_t *xc;

  LIST_FOREACH(xc, &xg->xg_channels, xc_link)
    if(!strcmp(xc->xc_name, name))
      return xc;

  xc = calloc(1, sizeof(xmltv_channel_t));
  xc->xc_name = strdup(name);
  TAILQ_INIT(&xc->xc_events);
  LIST_INSERT_HEAD(&xg->xg_channels, xc, xc_link);
  return xc;
}




#define XML_FOREACH(n) for(; (n) != NULL; (n) = (n)->next)

static void
xmltv_parse_channel(xmltv_grabber_t *xg, xmlNode *n, char *chid)
{
  char *t, *c;
  xmltv_channel_t *xc;

  xc = xc_find(xg, chid);

  XML_FOREACH(n) {

    c = (char *)xmlNodeGetContent(n);
    
    if(!strcmp((char *)n->name, "display-name")) {
      free((void *)xc->xc_displayname);
      xc->xc_displayname = strdup(c);
    }

    if(!strcmp((char *)n->name, "icon")) {
      t = (char *)xmlGetProp(n, (unsigned char *)"src");

      free(xc->xc_icon_url);
      xc->xc_icon_url = strdup(t);
      xmlFree(t);
    }
    xmlFree(c);
  }
}


/* 20060427110000 */
/* 0123456789abcd */
 
static time_t
str2time(char *str)
{
  char str0[30];
  struct tm tim;

  strcpy(str0, str);

  tim.tm_sec = atoi(str0 + 0xc);
  str0[0xc] = 0;
  tim.tm_min = atoi(str0 + 0xa);
  str0[0xa] = 0;
  tim.tm_hour = atoi(str0 + 0x8);
  str0[0x8] = 0;
  tim.tm_mday = atoi(str0 + 0x6);
  str0[0x6] = 0;
  tim.tm_mon = atoi(str0 + 0x4) - 1;
  str0[0x4] = 0;
  tim.tm_year = atoi(str0) - 1900;

  tim.tm_isdst = -1;
  return mktime(&tim);

}


static void
xmltv_parse_programme(xmltv_grabber_t *xg, xmlNode *n,
		      char *chid, char *startstr, char *stopstr)
{
  char *c;
  event_t *e;
  time_t start, stop;
  int duration;
  xmltv_channel_t *xc;

  xc = xc_find(xg, chid);
  start = str2time(startstr);
  stop  = str2time(stopstr);

  duration = stop - start;

  e = epg_event_build(&xc->xc_events, start, duration);
  if(e == NULL)
    return;

  XML_FOREACH(n) {

    c = (char *)xmlNodeGetContent(n);
    
    if(!strcmp((char *)n->name, "title")) {
      epg_event_set_title(e, c);
    } else if(!strcmp((char *)n->name, "desc")) {
      epg_event_set_desc(e, c);
    }
    xmlFree(c);
  }
}


static void
xmltv_parse_tv(xmltv_grabber_t *xg, xmlNode *n)
{
  char *chid;
  char *start;
  char *stop;

  XML_FOREACH(n) {

    if(n->type != XML_ELEMENT_NODE)
      continue;

    if(!strcmp((char *)n->name, "channel")) {
      chid = (char *)xmlGetProp(n, (unsigned char *)"id");
      if(chid != NULL) {
	xmltv_parse_channel(xg, n->children, chid);
	xmlFree(chid);
      }
    } else if(!strcmp((char *)n->name, "programme")) {
      chid = (char *)xmlGetProp(n, (unsigned char *)"channel");
      start = (char *)xmlGetProp(n, (unsigned char *)"start");
      stop = (char *)xmlGetProp(n, (unsigned char *)"stop");
      
      xmltv_parse_programme(xg, n->children, chid, start, stop);

      xmlFree(chid);
      xmlFree(start);
      xmlFree(stop);
    }
  }
}



static void
xmltv_parse_root(xmltv_grabber_t *xg, xmlNode *n)
{
  XML_FOREACH(n) {
    if(n->type == XML_ELEMENT_NODE && !strcmp((char *)n->name, "tv"))
      xmltv_parse_tv(xg, n->children);
  }
}


/*
 *
 */
static void
xmltv_flush_events(xmltv_grabber_t *xg)
{
  xmltv_channel_t *xc;
  event_t *e;

  LIST_FOREACH(xc, &xg->xg_channels, xc_link) {
    while((e = TAILQ_FIRST(&xc->xc_events)) != NULL) {
      TAILQ_REMOVE(&xc->xc_events, e, e_link);
      epg_event_free(e);
    }
  }
}


#if 0
/*
 *
 */
static int
xmltv_map(xmltv_channel_t *xc, th_channel_t *ch)
{
  xmltv_map_t *xm;
  LIST_FOREACH(xm, &xc->xc_maps, xm_link)
    if(xm->xm_channel == ch)
      return -1;

  xm = calloc(1, sizeof(xmltv_map_t));
  xm->xm_channel = ch;
  LIST_INSERT_HEAD(&xc->xc_maps, xm, xm_link);
  return 0;
}


/*
 *
 */
static void
xmltv_resolve_by_events(xmltv_channel_t *xc)
{
  th_channel_t *ch;
  event_t *ec, *ex;
  time_t now;
  int cnt, i;

  time(&now);

  for(i = 0; i < 4; i++) {
    now += 7200;

    ex = epg_event_find_by_time0(&xc->xc_events, now);
    if(ex == NULL)
      break;

    LIST_FOREACH(ch, &channels, ch_global_link) {
      ec = epg_event_find_by_time0(&ch->ch_epg_events, now);

      cnt = 0;
      
      while(1) {
	if((cnt >= 10 && ec == NULL) || cnt >= 30) {
	  if(xmltv_map(xc, ch) == 0)
	    syslog(LOG_DEBUG,
		   "xmltv: Heuristically mapped \"%s\" (%s) to \"%s\" "
		   "(%d consequtive events matches)",
		   xc->xc_displayname, xc->xc_name, ch->ch_name, cnt);
	  break;
	}

	if(ec == NULL || ex == NULL)
	  break;
	
	if(ec->e_start != ex->e_start || ec->e_duration != ex->e_duration)
	  break;
	
	ec = TAILQ_NEXT(ec, e_link);
	ex = TAILQ_NEXT(ex, e_link);
	cnt++;
      }
    }
  }
}


/*
 *
 */
void
xmltv_transfer(void)
{
  xmltv_channel_t *xc;
  xmltv_map_t *xm;
  th_channel_t *ch;

  LIST_FOREACH(xc, &xmltv_channel_list, xc_link) {

    ch = channel_find(xc->xc_displayname, 0, NULL);
    if(ch != NULL)
      xmltv_map(xc, ch);

    xmltv_resolve_by_events(xc);

    LIST_FOREACH(xm, &xc->xc_maps, xm_link) {
      if(xm->xm_isupdated)
	continue;

      epg_transfer_events(xm->xm_channel, &xc->xc_events, xc->xc_name,
			  xc->xc_icon);
      xm->xm_isupdated = 1;
    }
  }
}
#endif

/*
 * Execute grabber and parse result,
 *
 * Return nextstate
 */
static int
xmltv_grab(xmltv_grabber_t *xg)
{
  xmlDoc *doc = NULL;
  xmlNode *root_element;
  const char *prog;
  char *outbuf;
  int outlen;

  prog = xg->xg_path;

  syslog(LOG_INFO, "xmltv: Starting grabber \"%s\"", prog);
  
  outlen = spawn_and_store_stdout(prog, NULL, &outbuf);
  if(outlen < 1) {
    syslog(LOG_ERR, "xmltv: No output from \"%s\"", prog);
    return XMLTV_GRABBER_UNCONFIGURED;
  }

  doc = xmlParseMemory(outbuf, outlen);
  if(doc == NULL) {
    syslog(LOG_ERR, "xmltv: Error while parsing output from  \"%s\"", prog);
    free(outbuf);
    return XMLTV_GRABBER_DYSFUNCTIONAL;
  }

  xmltv_flush_events(xg);

  root_element = xmlDocGetRootElement(doc);
  xmltv_parse_root(xg, root_element);

  xmlFreeDoc(doc);
  xmlCleanupParser();
  syslog(LOG_INFO, "xmltv: EPG sucessfully loaded and parsed");
  free(outbuf);
  return XMLTV_GRABBER_IDLE;
}



static struct strtab grabberstatustab[] = {
  { "Disabled",              XMLTV_GRABBER_DISABLED      },
  { "Missing configuration", XMLTV_GRABBER_UNCONFIGURED  },
  { "Dysfunctional",         XMLTV_GRABBER_DYSFUNCTIONAL },
  { "Waiting for execution", XMLTV_GRABBER_ENQUEUED      },
  { "Grabbing",              XMLTV_GRABBER_GRABBING      },
  { "Idle",                  XMLTV_GRABBER_IDLE          },
};


const char *
xmltv_grabber_status(xmltv_grabber_t *xg)
{
  return val2str(xg->xg_status, grabberstatustab) ?: "Invalid";
}


/**
 *
 */
static int
grabbercmp(xmltv_grabber_t *a, xmltv_grabber_t *b)
{
  return strcasecmp(a->xg_title, b->xg_title);
}


/**
 * Enlist all active grabbers (for user to choose among)
 */
static int
xmltv_find_grabbers(const char *prog)
{
  char *outbuf;
  int outlen;
  char *s, *a, *b;
  xmltv_grabber_t *xg;
  int n = 0;
  char buf[40];

  outlen = spawn_and_store_stdout(prog, NULL, &outbuf);
  if(outlen < 1) {
    return -1;
  }

  s = outbuf;

  while(1) {
    a = s;

    while(*s && *s != '|')
      s++;

    if(!*s)
      break;
    *s++ = 0;

    b = s;

    while(*s > 31)
      s++;

    if(!*s)
      break;
    *s++ = 0;
    
    while(*s < 31 && *s > 0)
      s++;

    LIST_FOREACH(xg, &xmltv_grabbers, xg_link)
      if(!strcmp(b, xg->xg_title))
	break;

    if(xg != NULL)
      continue;

    xg = calloc(1, sizeof(xmltv_grabber_t));
    xg->xg_path  = a;
    xg->xg_title = b;

    snprintf(buf, sizeof(buf), "xmlgrabber_%d", n++);
    xg->xg_identifier = strdup(buf);

    LIST_INSERT_SORTED(&xmltv_grabbers, xg, xg_link, grabbercmp);
  }

  return 0;
}


/**
 *
 */
static void
xmltv_thread_set_status(icom_t *ic, xmltv_grabber_t *xg, int s)
{
  htsmsg_t *m = htsmsg_create();

  xg->xg_status = s;
  
  htsmsg_add_u32(m, "status", s);
  htsmsg_add_str(m, "identifier", xg->xg_identifier);
  icom_send_msg_from_thread(ic, m);
  htsmsg_destroy(m);
}

/*
 *
 */
static void *
xmltv_thread(void *aux)
{
  icom_t *ic = aux;
  struct timespec ts;
  xmltv_grabber_t *xg;
  int r;
  time_t n;

  /* Start by finding all available grabbers, we try with a few
     different locations */
 
  if(xmltv_find_grabbers("/bin/tv_find_grabbers") &&
     xmltv_find_grabbers("/usr/bin/tv_find_grabbers") &&
     xmltv_find_grabbers("/usr/local/bin/tv_find_grabbers")) {
    xmltv_status = XMLTVSTATUS_FIND_GRABBERS_NOT_FOUND;
    return NULL;
  }

  xmltv_status = XMLTVSTATUS_RUNNING;
  
  pthread_mutex_lock(&xmltv_work_lock);

  while(1) {
    
    xg = TAILQ_FIRST(&xmltv_grabber_workq);
    if(xg != NULL) {
      xmltv_thread_set_status(ic, xg, XMLTV_GRABBER_GRABBING);
      TAILQ_REMOVE(&xmltv_grabber_workq, xg, xg_work_link);
      pthread_mutex_unlock(&xmltv_work_lock);
      r = xmltv_grab(xg);
      pthread_mutex_lock(&xmltv_work_lock);

      xmltv_thread_set_status(ic, xg, r);
       
      time(&n);
      if(r == XMLTV_GRABBER_IDLE) {
	/* Completed load */
	xg->xg_nextgrab = n + 3600;
      } else {
	xg->xg_nextgrab = n + 60;
      }
      continue;
    }
    
    n = INT32_MAX;
    LIST_FOREACH(xg, &xmltv_grabbers, xg_link)
      n = xg->xg_nextgrab ? MIN(n, xg->xg_nextgrab) : n;

    ts.tv_sec = n;
    ts.tv_nsec = 0;

    pthread_cond_timedwait(&xmltv_work_cond, &xmltv_work_lock, &ts);
    
    time(&n);
 
    LIST_FOREACH(xg, &xmltv_grabbers, xg_link) {
      switch(xg->xg_status) {
      case XMLTV_GRABBER_GRABBING:
	abort();

      case XMLTV_GRABBER_DISABLED:
      case XMLTV_GRABBER_ENQUEUED:
	break;

      case XMLTV_GRABBER_UNCONFIGURED:
      case XMLTV_GRABBER_DYSFUNCTIONAL:
      case XMLTV_GRABBER_IDLE:
	if(xg->xg_nextgrab <= n + 1)
	  TAILQ_INSERT_TAIL(&xmltv_grabber_workq, xg, xg_work_link);
	break;
      }
    }
  }
  
  sleep(100000);
  return NULL;
}

void
xmltv_grabber_enable(xmltv_grabber_t *xg)
{
  pthread_mutex_lock(&xmltv_work_lock);

  if(xg->xg_status == XMLTV_GRABBER_DISABLED) {
    xg->xg_status = XMLTV_GRABBER_ENQUEUED;
    TAILQ_INSERT_TAIL(&xmltv_grabber_workq, xg, xg_work_link);
    pthread_cond_signal(&xmltv_work_cond);
  }

  pthread_mutex_unlock(&xmltv_work_lock);
}

/**
 *
 */
static void
icom_cb(void *opaque, htsmsg_t *m)
{
  const char *s;
  uint32_t status;
  xmltv_grabber_t *xg;

  if(!htsmsg_get_u32(m, "status", &status)) {
    s = htsmsg_get_str(m, "identifier");
    if(s != NULL && (xg = xmltv_grabber_find(s)) != NULL) {
      notify_xmltv_grabber_status_change(xg, status);
    }
  }
  htsmsg_destroy(m);
}

/**
 * Locate a grabber by its id
 */
xmltv_grabber_t *
xmltv_grabber_find(const char *id)
{
  xmltv_grabber_t *xg;
  LIST_FOREACH(xg, &xmltv_grabbers, xg_link)
    if(!strcmp(id, xg->xg_identifier))
      return xg;
  return NULL;
}



/**
 *
 */
void
xmltv_init(void)
{
  pthread_t ptid;
  icom_t *ic = icom_create(icom_cb, NULL);

  TAILQ_INIT(&xmltv_grabber_workq);

  pthread_create(&ptid, NULL, xmltv_thread, ic);
}

/**
 *
 */
const char *
xmltv_grabber_status_long(xmltv_grabber_t *xg, int status)
{
  static char buf[1000];
  switch(status) {
  case XMLTV_GRABBER_UNCONFIGURED:
    snprintf(buf, sizeof(buf),
	     "This grabber is not configured.</p><p> You need to configure it "
	     "manually by running '%s --configure' in a shell",
	     xg->xg_path);
    return buf;

  case XMLTV_GRABBER_DYSFUNCTIONAL:
    snprintf(buf, sizeof(buf),
	     "This grabber does not produce valid XML, please check "
	     "manually by running '%s' in a shell",
	     xg->xg_path);
    return buf;

  case XMLTV_GRABBER_ENQUEUED:
  case XMLTV_GRABBER_GRABBING:
    return "Grabbing, please wait";


  default:
    return "Unknown status";
  }
}
