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

#define _GNU_SOURCE
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <libhts/htscfg.h>
#include <syslog.h>

#include "tvhead.h"
#include "channels.h"
#include "epg.h"
#include "epg_xmltv.h"
#include "spawn.h"
#include "intercom.h"
#include "notify.h"
#include "dispatch.h"

int xmltv_globalstatus;
struct xmltv_grabber_list xmltv_grabbers;
struct xmltv_grabber_queue xmltv_grabber_workq;
static pthread_mutex_t xmltv_work_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t xmltv_work_cond = PTHREAD_COND_INITIALIZER;
static void xmltv_config_load(void);


static xmltv_channel_t *
xc_find(xmltv_grabber_t *xg, const char *name)
{
  xmltv_channel_t *xc;

  TAILQ_FOREACH(xc, &xg->xg_channels, xc_link)
    if(!strcmp(xc->xc_name, name))
      return xc;

  xc = calloc(1, sizeof(xmltv_channel_t));
  xc->xc_name = strdup(name);
  TAILQ_INIT(&xc->xc_events);
  TAILQ_INSERT_TAIL(&xg->xg_channels, xc, xc_link);
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
  xc->xc_updated = 1;
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

  TAILQ_FOREACH(xc, &xg->xg_channels, xc_link) {
    while((e = TAILQ_FIRST(&xc->xc_events)) != NULL) {
      TAILQ_REMOVE(&xc->xc_events, e, e_channel_link);
      epg_event_free(e);
    }
  }
}




/*
 *
 */
static channel_t *
xmltv_resolve_by_events(xmltv_channel_t *xc)
{
  channel_t *ch;
  event_t *ec, *ex;
  time_t now;
  int cnt, i;

  time(&now);

  for(i = 0; i < 4; i++) {
    now += 7200;

    ex = epg_event_find_by_time0(&xc->xc_events, now);
    if(ex == NULL)
      break;

    RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
      ec = epg_event_find_by_time0(&ch->ch_epg_events, now);
      cnt = 0;
      
      while(1) {
	if((cnt >= 15 && ec == NULL) || cnt >= 30)
	  return ch;

	if(ec == NULL || ex == NULL)
	  break;

	if(ec->e_start != ex->e_start || ec->e_duration != ex->e_duration)
	  break;

	ec = TAILQ_NEXT(ec, e_channel_link);
	ex = TAILQ_NEXT(ex, e_channel_link);
	cnt++;
      }
    }
  }
  return NULL;
}


/*
 *
 */
static void
xmltv_transfer_events(xmltv_grabber_t *xg)
{
  xmltv_channel_t *xc;
  channel_t *ch;
  int how;

  TAILQ_FOREACH(xc, &xg->xg_channels, xc_link) {
    if(xc->xc_disabled)
      continue;
    
    if(xc->xc_channel != NULL) {
      ch = channel_find_by_name(xc->xc_channel, 0);
      if(ch == NULL)
	continue;

    } else {

      how = 0;
      ch = channel_find_by_name(xc->xc_displayname, 0);
      if(ch == NULL) {
	ch = xmltv_resolve_by_events(xc);
      
	if(ch == NULL)
	  continue;
	how = 1;
      }
      if(strcmp(xc->xc_bestmatch ?: "", ch->ch_name)) {
	tvhlog(LOG_DEBUG, "xmltv", 
	       "xmltv: mapped \"%s\" (%s) to \"%s\" by %s",
	       xc->xc_displayname, xc->xc_name, ch->ch_name,
	       how ? "consequtive-event-matching" : "name");
	free(xc->xc_bestmatch);
	xc->xc_bestmatch = strdup(ch->ch_name);
	xmltv_config_save();
      }
    }

    if(xc->xc_updated == 0)
      continue;

    xc->xc_updated = 0;
    epg_transfer_events(ch, &xc->xc_events, xc->xc_name, xc->xc_icon_url);
  }
}


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
    return XMLTV_GRAB_UNCONFIGURED;
  }

  doc = xmlParseMemory(outbuf, outlen);
  if(doc == NULL) {
    syslog(LOG_ERR, "xmltv: Error while parsing output from  \"%s\"", prog);
    free(outbuf);
    return XMLTV_GRAB_DYSFUNCTIONAL;
  }

  pthread_mutex_lock(&xg->xg_mutex);

  xmltv_flush_events(xg);

  root_element = xmlDocGetRootElement(doc);
  xmltv_parse_root(xg, root_element);

  pthread_mutex_unlock(&xg->xg_mutex);

  xmlFreeDoc(doc);
  xmlCleanupParser();
  syslog(LOG_INFO, "xmltv: EPG sucessfully loaded and parsed");
  free(outbuf);
  return XMLTV_GRAB_OK;
}




/**
 *
 */
static void
xmltv_thread_grabber_init_done(icom_t *ic, const char *payload)
{
  htsmsg_t *m = htsmsg_create();

  htsmsg_add_u32(m, "initialized", 1);
  if(payload != NULL)
    htsmsg_add_str(m, "payload", payload);
  icom_send_msg_from_thread(ic, m);
  htsmsg_destroy(m);
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
 *
 * We send all these in a message back to the main thread
 */
static int
xmltv_find_grabbers(icom_t *ic, const char *prog)
{
  char *outbuf;
  int outlen;

  outlen = spawn_and_store_stdout(prog, NULL, &outbuf);
  if(outlen < 1)
    return -1;

  xmltv_thread_grabber_init_done(ic, outbuf);
  free(outbuf);
  return 0;
}


/**
 * Parse list of all grabbers
 */
static void
xmltv_parse_grabbers(char *list)
{
  char *s, *a, *b;
  xmltv_grabber_t *xg;
  int n = 0;
  char buf[40];

  s = list;

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
    pthread_mutex_init(&xg->xg_mutex, NULL);

    TAILQ_INIT(&xg->xg_channels);
    xg->xg_path  = strdup(a);
    xg->xg_title = strdup(b);

    snprintf(buf, sizeof(buf), "xmlgrabber_%d", n++);
    xg->xg_identifier = strdup(buf);

    LIST_INSERT_SORTED(&xmltv_grabbers, xg, xg_link, grabbercmp);
  }
  xmltv_config_load();
}



/*
 *
 */
static void *
xmltv_thread(void *aux)
{
  icom_t *ic = aux;
  htsmsg_t *m;
  xmltv_grabber_t *xg;
  int r;

  /* Start by finding all available grabbers, we try with a few
     different locations */
 
  if(xmltv_find_grabbers(ic, "/usr/bin/tv_find_grabbers") &&
     xmltv_find_grabbers(ic, "/bin/tv_find_grabbers") &&
     xmltv_find_grabbers(ic, "/usr/local/bin/tv_find_grabbers")) {
    xmltv_thread_grabber_init_done(ic, NULL);
    return NULL;
  }
  
  pthread_mutex_lock(&xmltv_work_lock);

  while(1) {

    while((xg = TAILQ_FIRST(&xmltv_grabber_workq)) == NULL)
      pthread_cond_wait(&xmltv_work_cond, &xmltv_work_lock);

    xg->xg_on_work_link = 0;
    TAILQ_REMOVE(&xmltv_grabber_workq, xg, xg_work_link);
    pthread_mutex_unlock(&xmltv_work_lock);
    r = xmltv_grab(xg);

    m = htsmsg_create();
    htsmsg_add_u32(m, "grab_completed", r);
    htsmsg_add_str(m, "identifier", xg->xg_identifier);
    icom_send_msg_from_thread(ic, m);
    htsmsg_destroy(m);

    pthread_mutex_lock(&xmltv_work_lock);
  }
  return NULL;
}

/**
 *
 */
static void
xmltv_grabber_enqueue(xmltv_grabber_t *xg)
{
  pthread_mutex_lock(&xmltv_work_lock);

  if(!xg->xg_on_work_link) {
    xg->xg_on_work_link = 1;
    TAILQ_INSERT_TAIL(&xmltv_grabber_workq, xg, xg_work_link);
    pthread_cond_signal(&xmltv_work_cond);
  }
  pthread_mutex_unlock(&xmltv_work_lock);
}


/**
 * Enqueue a new grabing
 */
static void
regrab(void *aux, int64_t now)
{
  xmltv_grabber_t *xg = aux;
  xmltv_grabber_enqueue(xg);
}

/**
 *
 */
static void
xmltv_xfer(void *aux, int64_t now)
{
  xmltv_grabber_t *xg = aux;
  int t;

  /* We don't want to stall waiting for the xml decoding which
     can take quite some time, instead retry in a second if we fail
     to obtain mutex */

  if(pthread_mutex_trylock(&xg->xg_mutex) == EBUSY) {
    t = 1;
  } else {
    xmltv_transfer_events(xg);
    pthread_mutex_unlock(&xg->xg_mutex);
    t = 60;
  }

  dtimer_arm(&xg->xg_xfer_timer, xmltv_xfer, xg, t);
}

/**
 *
 */
static void
icom_cb(void *opaque, htsmsg_t *m)
{
  const char *s;
  char *t;
  xmltv_grabber_t *xg;
  uint32_t v;

  if(!htsmsg_get_u32(m, "initialized", &v)) {
    /* Grabbers loaded */

    if((s = htsmsg_get_str(m, "payload")) != NULL) {
      t = strdupa(s);
      xmltv_parse_grabbers(t);
      xmltv_globalstatus = XMLTVSTATUS_RUNNING;
      
      LIST_FOREACH(xg, &xmltv_grabbers, xg_link)
	if(xg->xg_enabled)
	  xmltv_grabber_enqueue(xg);

    } else {
      xmltv_globalstatus = XMLTVSTATUS_FIND_GRABBERS_NOT_FOUND;
    }
  } else if(!htsmsg_get_u32(m, "grab_completed", &v) &&
	    (s = htsmsg_get_str(m, "identifier")) != NULL &&
	    (xg = xmltv_grabber_find(s)) != NULL) {

    xg->xg_status = v;
    dtimer_arm(&xg->xg_grab_timer, regrab, xg, v == XMLTV_GRAB_OK ? 3600 : 60);
    dtimer_arm(&xg->xg_xfer_timer, xmltv_xfer, xg, 1);
    //    notify_xmltv_grabber_status_change(xg);
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
xmltv_grabber_status_long(xmltv_grabber_t *xg)
{
  static char buf[1000];

  if(xg->xg_enabled == 0) {
    return "Disabled";
  } else if(xg->xg_on_work_link) {
    return "Enqueued for grabbing, please wait";
  } else switch(xg->xg_status) {

  case XMLTV_GRAB_WORKING:
    return "Grabbing, please wait";

  case XMLTV_GRAB_UNCONFIGURED:
    snprintf(buf, sizeof(buf),
	     "This grabber is not configured.</p><p>You need to configure it "
	     "manually by running '%s --configure' in a shell",
	     xg->xg_path);
    return buf;

  case XMLTV_GRAB_DYSFUNCTIONAL:
    snprintf(buf, sizeof(buf),
	     "This grabber does not produce valid XML, please check "
	     "manually by running '%s' in a shell",
	     xg->xg_path);
    return buf;

  case XMLTV_GRAB_OK:
    return "Idle";
  }

  return "Unknown status";
}


/**
 *
 */
const char *
xmltv_grabber_status(xmltv_grabber_t *xg)
{
  if(xg->xg_enabled == 0) {
    return "Disabled";
  } else if(xg->xg_on_work_link) {
    return "Grabbing";
  } else switch(xg->xg_status) {
  case XMLTV_GRAB_WORKING:
    return "Grabbing";

  case XMLTV_GRAB_UNCONFIGURED:
    return  "Unconfigured";
  case XMLTV_GRAB_DYSFUNCTIONAL:
    return  "Dysfunctional";
  case XMLTV_GRAB_OK:
    return "Idle";
  }
  return "Unknown";
}


/*
 * Write grabber configuration
 */
void
xmltv_config_save(void)
{
  char buf[PATH_MAX];
  xmltv_grabber_t *xg;
  xmltv_channel_t *xc;
  FILE *fp;

  snprintf(buf, sizeof(buf), "%s/xmltv-settings.cfg", settings_dir);

  if((fp = settings_open_for_write(buf)) != NULL) {
    LIST_FOREACH(xg, &xmltv_grabbers, xg_link) {

      fprintf(fp, "grabber {\n");
      fprintf(fp, "\ttitle = %s\n", xg->xg_title);
      fprintf(fp, "\tenabled = %d\n", xg->xg_enabled);

    
      TAILQ_FOREACH(xc, &xg->xg_channels, xc_link) {
	fprintf(fp, "\tchannel {\n");
	fprintf(fp, "\t\tdisplayname = %s\n", xc->xc_displayname);
	if(xc->xc_icon_url != NULL)
	  fprintf(fp, "\t\ticon = %s\n", xc->xc_icon_url);
	if(xc->xc_bestmatch != NULL)
	  fprintf(fp, "\t\tbestmatch = %s\n", xc->xc_bestmatch);
	fprintf(fp, "\t\tname = %s\n", xc->xc_name);
	if(xc->xc_disabled)
	  fprintf(fp, "\t\tmapping = disabled\n");
	else
	  fprintf(fp, "\t\tmapping = %s\n", xc->xc_channel ?: "auto");
	fprintf(fp, "\t}\n");
      }
      fprintf(fp, "}\n");
    }
    fclose(fp);
  }
}

/**
 *
 */
static void
xmltv_config_load(void)
{
  struct config_head cl;
  config_entry_t *ce, *ce2;
  char buf[PATH_MAX];
  const char *title, *name, *s;
  xmltv_channel_t *xc;
  xmltv_grabber_t *xg;

  TAILQ_INIT(&cl);

  snprintf(buf, sizeof(buf), "%s/xmltv-settings.cfg", settings_dir);

  config_read_file0(buf, &cl);
  
  pthread_mutex_lock(&xmltv_work_lock);
  
  TAILQ_FOREACH(ce, &cl, ce_link) {
    if(ce->ce_type != CFG_SUB || strcasecmp("grabber", ce->ce_key))
      continue;

    if((title = config_get_str_sub(&ce->ce_sub, "title", NULL)) == NULL)
      continue;

    LIST_FOREACH(xg, &xmltv_grabbers, xg_link)
      if(!strcmp(title, xg->xg_title))
	break;
    if(xg == NULL)
      continue;

    xg->xg_enabled = atoi(config_get_str_sub(&ce->ce_sub, "enabled", "0"));

    TAILQ_FOREACH(ce2, &ce->ce_sub, ce_link) {
      if(ce2->ce_type != CFG_SUB || strcasecmp("channel", ce2->ce_key))
	continue;

      if((name = config_get_str_sub(&ce2->ce_sub, "name", NULL)) == NULL)
	continue;

      xc = xc_find(xg, name);

      if((s = config_get_str_sub(&ce2->ce_sub, "displayname", NULL)) != NULL)
	xc->xc_displayname = strdup(s);

      if((s = config_get_str_sub(&ce2->ce_sub, "bestmatch", NULL)) != NULL)
	xc->xc_bestmatch = strdup(s);

     if((s = config_get_str_sub(&ce2->ce_sub, "icon", NULL)) != NULL)
	xc->xc_icon_url = strdup(s);

      if((s = config_get_str_sub(&ce2->ce_sub, "mapping", NULL)) != NULL) {
	xc->xc_channel = NULL;
	if(!strcmp(s, "disabled")) {
	  xc->xc_disabled = 1;
	} else if(!strcmp(s, "auto")) {
	} else {
	  xc->xc_channel = strdup(s);
	}
      }
    }
  }

  pthread_mutex_unlock(&xmltv_work_lock);

  config_free0(&cl);
}

void
xmltv_grabber_enable(xmltv_grabber_t *xg)
{
  xg->xg_enabled = 1;
  xmltv_grabber_enqueue(xg);
  xmltv_config_save();
}
