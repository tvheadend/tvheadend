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
#include "epg_xmltv.h"
#include "output_client.h"

extern int xmltvreload;


#define XML_FOREACH(n) for(; (n) != NULL; (n) = (n)->next)

static void
xmltv_parse_channel(xmlNode *n, char *chid)
{
  char *t, *c;
  char *dname = NULL;
  char *iname = NULL;
  th_channel_t *tdc;
  th_proglist_t *tpl;

  XML_FOREACH(n) {

    c = (char *)xmlNodeGetContent(n);
    
    if(!strcmp((char *)n->name, "display-name")) {
      if(dname != NULL)
	free(dname);
      dname = strdup(c);
    }

    if(!strcmp((char *)n->name, "icon")) {
      t = (char *)xmlGetProp(n, (unsigned char *)"src");

      iname = t ? strdup(t) : NULL;
      xmlFree(t);
    }
    xmlFree(c);
  }


  if(dname != NULL) {

    TAILQ_FOREACH(tdc, &channels, ch_global_link) {
      if(strcmp(tdc->ch_name, dname))
	continue;

      pthread_mutex_lock(&tdc->ch_epg_mutex);
      tpl = &tdc->ch_xmltv;

      if(tpl->tpl_refname != NULL)
	free((void *)tpl->tpl_refname);
      tpl->tpl_refname = strdup(chid);

      if(tdc->ch_icon != NULL)
	free((void *)tdc->ch_icon);
      tdc->ch_icon = iname ? strdup(iname) : NULL;

      pthread_mutex_unlock(&tdc->ch_epg_mutex);
      break;
    }
    free(dname);
  }

  if(iname != NULL)
    free(iname);
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
xmltv_parse_programme(xmlNode *n, char *chid, char *start, char *stop)
{
  char *c;
  th_channel_t *tdc;
  programme_t *pr, *t;
  th_proglist_t *tpl;

  TAILQ_FOREACH(tdc, &channels, ch_global_link) {
    pthread_mutex_lock(&tdc->ch_epg_mutex);
    tpl = &tdc->ch_xmltv;
    if(tpl->tpl_refname != NULL && !strcmp(tpl->tpl_refname, chid))
      break;
    pthread_mutex_unlock(&tdc->ch_epg_mutex);
  }

  if(tdc == NULL)
    return;

  pr = calloc(1, sizeof(programme_t));

  pr->pr_start = str2time(start);
  pr->pr_stop = str2time(stop);

  XML_FOREACH(n) {

    c = (char *)xmlNodeGetContent(n);
    
    if(!strcmp((char *)n->name, "title")) {
      pr->pr_title = strdup(c);
    } else if(!strcmp((char *)n->name, "desc")) {
      pr->pr_desc = strdup(c);
    }
    xmlFree(c);
  }


  LIST_FOREACH(t, &tpl->tpl_programs, pr_link) {
    if(pr->pr_start == t->pr_start &&
       pr->pr_stop == t->pr_stop) 
      break;
  }
    
  if(t != NULL) {
    free(t->pr_title);
    free(t->pr_desc);
    
    t->pr_title = pr->pr_title;
    t->pr_desc = pr->pr_desc;
    
    free(pr);
    
    t->pr_delete_me = 0;
  
  } else {

    tpl->tpl_nprograms++;

    pr->pr_ref = ++reftally;

    pr->pr_ch = tdc;
    LIST_INSERT_HEAD(&tpl->tpl_programs, pr, pr_link);
  }
  pthread_mutex_unlock(&tdc->ch_epg_mutex);
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
 *  Sorting functions
 *
 */

static int
xmltvpcmp(const void *A, const void *B)
{
  programme_t *a = *(programme_t **)A;
  programme_t *b = *(programme_t **)B;
  
  return a->pr_start - b->pr_start;
}


static void
xmltv_sort_channel(th_proglist_t *tpl)
{
  programme_t *pr;
  int i;

  if(tpl->tpl_prog_vec != NULL)
    free(tpl->tpl_prog_vec);

  if(tpl->tpl_nprograms == 0)
    return;
  
  tpl->tpl_prog_vec = malloc(tpl->tpl_nprograms * sizeof(void *));
  i = 0;

  LIST_FOREACH(pr, &tpl->tpl_programs, pr_link)
    tpl->tpl_prog_vec[i++] = pr;

  qsort(tpl->tpl_prog_vec, tpl->tpl_nprograms, sizeof(void *), xmltvpcmp);

  for(i = 0; i < tpl->tpl_nprograms; i++) {
    tpl->tpl_prog_vec[i]->pr_index = i;
  }
}


static void
xmltv_insert_dummies(th_channel_t *ch, th_proglist_t *tpl)
{
  programme_t *pr2, *pr;
  int i, delta = 0;

  time_t nu, *prev;
  struct tm tm;

  if(tpl->tpl_nprograms < 1)
    return;

  nu = tpl->tpl_prog_vec[0]->pr_start;

  localtime_r(&nu, &tm);
  
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;

  nu = mktime(&tm);

  prev = &nu;

  for(i = 0; i < tpl->tpl_nprograms - 1; i++) {

    nu = *prev;

    pr2 = tpl->tpl_prog_vec[i];

    if(nu != pr2->pr_start) {

      pr = calloc(1, sizeof(programme_t));
    
      pr->pr_start = nu;
      pr->pr_stop = pr2->pr_start;

      pr->pr_title = NULL;
      pr->pr_desc = NULL;
    
      delta++;

      pr->pr_ch = ch;
      pr->pr_ref = ++reftally;
      LIST_INSERT_HEAD(&tpl->tpl_programs, pr, pr_link);
    }
    prev = &pr2->pr_stop;
  }
  tpl->tpl_nprograms += delta;
}



static void
xmltv_purge_channel(th_proglist_t *tpl)
{
  programme_t *pr, *t;

  for(pr = LIST_FIRST(&tpl->tpl_programs) ; pr != NULL; pr = t) {
    t = LIST_NEXT(pr, pr_link);

    if(pr->pr_delete_me == 0)
      continue;

    free(pr->pr_title);
    free(pr->pr_desc);

    LIST_REMOVE(pr, pr_link);
    free(pr);
    tpl->tpl_nprograms--;
  }
}

static void
xmltv_prep_reload(void)
{
  programme_t *pr;
  th_channel_t *tdc;
  th_proglist_t *tpl;

  TAILQ_FOREACH(tdc, &channels, ch_global_link) {
    pthread_mutex_lock(&tdc->ch_epg_mutex);
    tpl = &tdc->ch_xmltv;

    LIST_FOREACH(pr, &tpl->tpl_programs, pr_link)
      pr->pr_delete_me = 1;
    pthread_mutex_unlock(&tdc->ch_epg_mutex);
  }
}



static void
xmltv_set_current(th_proglist_t *tpl)
{
  time_t now;
  programme_t *pr;
  programme_t **vec = tpl->tpl_prog_vec;
  int i, len = tpl->tpl_nprograms;
  
  time(&now);

  pr = NULL;

  for(i = len - 1; i >= 0; i--) {

    pr = vec[i];
    if(pr->pr_start < now)
      break;
  }

  if(i == len || pr == NULL || pr->pr_start > now) {
    tpl->tpl_prog_current = NULL;
  } else {
    tpl->tpl_prog_current = pr;
  }
}


static void
xmltv_sort_programs(void)
{
  th_channel_t *tdc;
  th_proglist_t *tpl;

  TAILQ_FOREACH(tdc, &channels, ch_global_link) {

    pthread_mutex_lock(&tdc->ch_epg_mutex);

    tpl = &tdc->ch_xmltv;

    if(tpl->tpl_refname != NULL) {
      syslog(LOG_DEBUG, "xmltv: %s (%s) %d programs loaded",
	     tdc->ch_name, tpl->tpl_refname, tpl->tpl_nprograms);

      xmltv_purge_channel(tpl);
      xmltv_sort_channel(tpl);
      xmltv_insert_dummies(tdc, tpl);
      xmltv_sort_channel(tpl);
      xmltv_set_current(tpl);
    }

    pthread_mutex_unlock(&tdc->ch_epg_mutex);
    
    clients_enq_ref(tdc->ch_ref);
  }
}


void
xmltv_update_current(void)
{
  th_channel_t *tdc;
  th_proglist_t *tpl;
  programme_t *pr;
  time_t now;
  int idx;

  time(&now);

  TAILQ_FOREACH(tdc, &channels, ch_global_link) {

    pthread_mutex_lock(&tdc->ch_epg_mutex);
    tpl = &tdc->ch_xmltv;

    pr = tpl->tpl_prog_current;

    if(pr != NULL && pr->pr_stop < now) {

      clients_enq_ref(pr->pr_ref);
      
      idx = pr->pr_index + 1;
      if(idx < tpl->tpl_nprograms) {

	pr = tpl->tpl_prog_vec[idx];
	tpl->tpl_prog_current = pr;

	clients_enq_ref(pr->pr_ref);
	clients_enq_ref(tdc->ch_ref);
      }
    }
    pthread_mutex_unlock(&tdc->ch_epg_mutex);
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

  xmltv_prep_reload();

  root_element = xmlDocGetRootElement(doc);
  xmltv_parse_root(root_element);
  xmlFreeDoc(doc);
  xmlCleanupParser();

  xmltv_sort_programs();
}




/*
 *
 */
static void *
xmltv_thread(void *aux)
{
  xmltvreload = 1;
  while(1) {

    sleep(1);

    if(xmltvreload) {
      xmltvreload = 0;
      xmltv_load();
    }

    xmltv_update_current();
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
