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

extern int xmltvreload;

LIST_HEAD(, xmltv_channel) xmltv_channel_list;

typedef struct xmltv_map {
  LIST_ENTRY(xmltv_map) xm_link;
  th_channel_t *xm_channel; /* Set if we have resolved the channel */

  int xm_isupdated;

} xmltv_map_t;




typedef struct xmltv_channel {
  LIST_ENTRY(xmltv_channel) xc_link;
  const char *xc_name;
  const char *xc_displayname; 
  const char *xc_icon;

  LIST_HEAD(, xmltv_map) xc_maps;

  struct event_queue xc_events;

} xmltv_channel_t;


static xmltv_channel_t *
xc_find(const char *name)
{
  xmltv_channel_t *xc;

  LIST_FOREACH(xc, &xmltv_channel_list, xc_link)
    if(!strcmp(xc->xc_name, name))
      return xc;

  xc = calloc(1, sizeof(xmltv_channel_t));
  xc->xc_name = strdup(name);
  TAILQ_INIT(&xc->xc_events);
  LIST_INSERT_HEAD(&xmltv_channel_list, xc, xc_link);
  return xc;
}




#define XML_FOREACH(n) for(; (n) != NULL; (n) = (n)->next)

static void
xmltv_parse_channel(xmlNode *n, char *chid)
{
  char *t, *c;
  xmltv_channel_t *xc;

  xc = xc_find(chid);

  XML_FOREACH(n) {

    c = (char *)xmlNodeGetContent(n);
    
    if(!strcmp((char *)n->name, "display-name")) {
      free((void *)xc->xc_displayname);
      xc->xc_displayname = strdup(c);
    }

    if(!strcmp((char *)n->name, "icon")) {
      t = (char *)xmlGetProp(n, (unsigned char *)"src");

      free((void *)xc->xc_icon);
      xc->xc_icon = strdup(t);
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
xmltv_parse_programme(xmlNode *n, char *chid, char *startstr, char *stopstr)
{
  char *c;
  event_t *e;
  time_t start, stop;
  int duration;
  xmltv_channel_t *xc;

  xc = xc_find(chid);
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
xmltv_parse_tv(xmlNode *n)
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
	xmltv_parse_channel(n->children, chid);
	xmlFree(chid);
      }
    } else if(!strcmp((char *)n->name, "programme")) {
      chid = (char *)xmlGetProp(n, (unsigned char *)"channel");
      start = (char *)xmlGetProp(n, (unsigned char *)"start");
      stop = (char *)xmlGetProp(n, (unsigned char *)"stop");
      
      xmltv_parse_programme(n->children, chid, start, stop);

      xmlFree(chid);
      xmlFree(start);
      xmlFree(stop);
    }
  }
}



static void
xmltv_parse_root(xmlNode *n)
{
  XML_FOREACH(n) {
    if(n->type == XML_ELEMENT_NODE && !strcmp((char *)n->name, "tv"))
      xmltv_parse_tv(n->children);
  }
}


/*
 *
 */
void
xmltv_flush(void)
{
  xmltv_channel_t *xc;
  xmltv_map_t *xm;
  event_t *e;

  LIST_FOREACH(xc, &xmltv_channel_list, xc_link) {
    while((e = TAILQ_FIRST(&xc->xc_events)) != NULL) {
      TAILQ_REMOVE(&xc->xc_events, e, e_link);
      epg_event_free(e);
    }

    while((xm = LIST_FIRST(&xc->xc_maps)) != NULL) {
      LIST_REMOVE(xm, xm_link);
      free(xm);
    }
  }
}

/*
 *
 */
void
xmltv_load(void)
{
  xmlDoc *doc = NULL;
  xmlNode *root_element = NULL;
  const char *filename = config_get_str("xmltv", "/tmp/tv.xml");

  syslog(LOG_INFO, "Loading xmltv file \"%s\"", filename);
  
  doc = xmlParseFile(filename);
  if(doc == NULL) {
    syslog(LOG_ERR, "Error while loading xmltv file \"%s\"", filename);
    return;
  }

  xmltv_flush();

  root_element = xmlDocGetRootElement(doc);
  xmltv_parse_root(root_element);
  xmlFreeDoc(doc);
  xmlCleanupParser();
}

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

    TAILQ_FOREACH(ch, &channels, ch_global_link) {
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

    ch = channel_find(xc->xc_displayname, 0);
    if(ch != NULL)
      xmltv_map(xc, ch);

    xmltv_resolve_by_events(xc);

    LIST_FOREACH(xm, &xc->xc_maps, xm_link) {
      if(xm->xm_isupdated)
	continue;

      epg_transfer_events(xm->xm_channel, &xc->xc_events, xc->xc_name);
      xm->xm_isupdated = 1;
    }
  }
}


/*
 *
 */
static void *
xmltv_thread(void *aux)
{
  xmltvreload = 1;
  while(1) {

    sleep(10);

    if(xmltvreload) {
      xmltvreload = 0;
      xmltv_load();
    }

    xmltv_transfer();

  }
}


/*
 *
 */
void
xmltv_init(void)
{
  pthread_t ptid;
  pthread_create(&ptid, NULL, xmltv_thread, NULL);
}
