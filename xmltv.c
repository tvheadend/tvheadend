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

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <libhts/htsmsg_xml.h>
#include <libhts/htssettings.h>

#include "tvhead.h"
#include "channels.h"
#include "epg.h"
#include "xmltv.h"
#include "spawn.h"

/* xmltv_channels is protected by global_lock */
static struct xmltv_channel_tree xmltv_channels;
struct xmltv_channel_list xmltv_displaylist;



/**
 *
 */
static int
xc_dn_cmp(const xmltv_channel_t *a, const xmltv_channel_t *b)
{
  return strcmp(a->xc_displayname, b->xc_displayname);
}


/**
 *
 */
static void
xc_set_displayname(xmltv_channel_t *xc, const char *name)
{
  if(xc->xc_displayname != NULL) {
    LIST_REMOVE(xc, xc_displayname_link);
    free(xc->xc_displayname);
  }
  if(name == NULL) {
    xc->xc_displayname = NULL;
    return;
  }

  xc->xc_displayname = strdup(name);
  LIST_INSERT_SORTED(&xmltv_displaylist, xc, xc_displayname_link, xc_dn_cmp);
}

/**
 *
 */
static void
xmltv_load(void)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;
  xmltv_channel_t *xc;

  if((l = hts_settings_load("xmltv/channels")) != NULL) {
    HTSMSG_FOREACH(f, l) {
      if((c = htsmsg_get_msg_by_field(f)) == NULL)
	continue;
      
      xc = xmltv_channel_find(f->hmf_name, 1);
      tvh_str_set(&xc->xc_icon, htsmsg_get_str(c, "icon"));
      
      xc_set_displayname(xc, htsmsg_get_str(c, "displayname"));
    }
  }
}


/**
 *
 */
static void
xmltv_save(xmltv_channel_t *xc)
{
  htsmsg_t *m = htsmsg_create();
  htsmsg_add_str(m, "displayname", xc->xc_displayname);

  if(xc->xc_icon != NULL)
    htsmsg_add_str(m, "icon", xc->xc_icon);

  hts_settings_save(m, "xmltv/channels/%s", xc->xc_identifier);
  htsmsg_destroy(m);
}


/**
 *
 */
static void
xmltv_map_to_channel_by_name(xmltv_channel_t *xc)
{
  channel_t *ch, *next;

  for(ch = LIST_FIRST(&channels_not_xmltv_mapped); ch != NULL; ch = next) {
    next = LIST_NEXT(ch, ch_xc_link);
    assert(ch != next);
    if(!strcasecmp(xc->xc_displayname, ch->ch_name)) {
      tvhlog(LOG_NOTICE, "xmltv", "Channel \"%s\" automapped to xmltv-channel "
	     "\"%s\" (name matches)", ch->ch_name, xc->xc_displayname);
      channel_set_xmltv_source(ch, xc);
    }
  }
}


/**
 *
 */
static time_t
xmltv_str2time(const char *str)
{
  char str0[30];
  struct tm tim;

  snprintf(str0, sizeof(str0), "%s", str);

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


/**
 *
 */
static int
xc_id_cmp(const xmltv_channel_t *a, const xmltv_channel_t *b)
{
  return strcmp(a->xc_identifier, b->xc_identifier);
}


/**
 *
 */
xmltv_channel_t *
xmltv_channel_find(const char *id, int create)
{
  xmltv_channel_t *xc;
  static xmltv_channel_t *skel;

  lock_assert(&global_lock);

  if(skel == NULL)
    skel = calloc(1, sizeof(xmltv_channel_t));

  skel->xc_identifier = (char *)id;
  if(!create)
    return RB_FIND(&xmltv_channels, skel, xc_link, xc_id_cmp);
  
  xc = RB_INSERT_SORTED(&xmltv_channels, skel, xc_link, xc_id_cmp);
  if(xc == NULL) {
    /* New entry was inserted */
    xc = skel;
    skel = NULL;
    xc->xc_identifier = strdup(id);
  }
  return xc;
}


/**
 *
 */
xmltv_channel_t *
xmltv_channel_find_by_displayname(const char *name)
{
  xmltv_channel_t *xc;

  lock_assert(&global_lock);

  LIST_FOREACH(xc, &xmltv_displaylist, xc_displayname_link)
    if(xc->xc_displayname && !strcmp(xc->xc_displayname, name))
      break;
  return xc;
}


/**
 * XXX: Move to libhts?
 */
static const char *
xmltv_get_cdata_by_tag(htsmsg_t *tags, const char *name)
{
  htsmsg_t *sub;
  if((sub = htsmsg_get_msg(tags, name)) == NULL)
    return NULL;
  return htsmsg_get_str(sub, "cdata");
}


/**
 * Parse a <channel> tag from xmltv
 */
static void
xmltv_parse_channel(htsmsg_t *body)
{
  htsmsg_t *attribs, *tags, *subtag;
  const char *id, *name, *icon;
  xmltv_channel_t *xc;
  channel_t *ch;
  int save = 0;

  if(body == NULL)
    return;

  if((attribs = htsmsg_get_msg(body, "attrib")) == NULL)
    return;

  tags = htsmsg_get_msg(body, "tags");
  
  if((id = htsmsg_get_str(attribs, "id")) == NULL)
    return;

  xc = xmltv_channel_find(id, 1);

  if(tags == NULL) 
    return;

  if((name = xmltv_get_cdata_by_tag(tags, "display-name")) != NULL) {

    if(xc->xc_displayname == NULL || strcmp(xc->xc_displayname, name)) {
      xc_set_displayname(xc, name);
      xmltv_map_to_channel_by_name(xc);
      save = 1;
    }
  }

  if((subtag  = htsmsg_get_msg(tags,    "icon"))   != NULL &&
     (attribs = htsmsg_get_msg(subtag,  "attrib")) != NULL &&
     (icon    = htsmsg_get_str(attribs, "src"))    != NULL) {
    
    if(xc->xc_icon == NULL || strcmp(xc->xc_icon, icon)) {
      tvh_str_set(&xc->xc_icon, icon);
      
      LIST_FOREACH(ch, &xc->xc_channels, ch_xc_link)
	channel_set_icon(ch, icon);
      save = 1;
    }
  }

  if(save)
    xmltv_save(xc);
}

/**
 * Parse tags inside of a programme
 */
static void
xmltv_parse_programme_tags(xmltv_channel_t *xc, htsmsg_t *tags, 
			   time_t start, time_t stop)
{
  event_t *e;
  channel_t *ch;
  const char *title = xmltv_get_cdata_by_tag(tags, "title");
  const char *desc  = xmltv_get_cdata_by_tag(tags, "desc");

  LIST_FOREACH(ch, &xc->xc_channels, ch_xc_link) {
    e = epg_event_create(ch, start, stop);

    if(title != NULL) epg_event_set_title(e, title);
    if(desc  != NULL) epg_event_set_desc(e, desc);
  }
}


/**
 * Parse a <programme> tag from xmltv
 */
static void
xmltv_parse_programme(htsmsg_t *body)
{
  htsmsg_t *attribs, *tags;
  const char *s, *chid;
  time_t start, stop;
  xmltv_channel_t *xc;

  if(body == NULL)
    return;

  if((attribs = htsmsg_get_msg(body, "attrib")) == NULL)
    return;

  if((tags = htsmsg_get_msg(body, "tags")) == NULL)
    return;

  if((chid = htsmsg_get_str(attribs, "channel")) == NULL)
    return;

  if((s = htsmsg_get_str(attribs, "start")) == NULL)
    return;
  start = xmltv_str2time(s);

  if((s = htsmsg_get_str(attribs, "stop")) == NULL)
    return;
  stop = xmltv_str2time(s);

  if(stop <= start || stop < dispatch_clock)
    return;

  if((xc = xmltv_channel_find(chid, 0)) != NULL) {
    xmltv_parse_programme_tags(xc, tags, start, stop);
  }
}

/**
 *
 */
static void
xmltv_parse_tv(htsmsg_t *body)
{
  htsmsg_t *tags;
  htsmsg_field_t *f;

  if((tags = htsmsg_get_msg(body, "tags")) == NULL)
    return;

  HTSMSG_FOREACH(f, tags) {
    if(!strcmp(f->hmf_name, "channel")) {
      xmltv_parse_channel(htsmsg_get_msg_by_field(f));
    } else if(!strcmp(f->hmf_name, "programme")) {
      xmltv_parse_programme(htsmsg_get_msg_by_field(f));
    }
  }
}


/**
 *
 */
static void
xmltv_parse(htsmsg_t *body)
{
  htsmsg_t *tags, *tv;

  if((tags = htsmsg_get_msg(body, "tags")) == NULL)
    return;

  if((tv = htsmsg_get_msg(tags, "tv")) == NULL)
    return;

  xmltv_parse_tv(tv);
}

/**
 *
 */
static void
xmltv_grab(void)
{
  const char *prog = "/usr/bin/tv_grab_se_swedb";
  int outlen;
  char *outbuf;
  htsmsg_t *body;
  char errbuf[100];

  tvhlog(LOG_INFO, "xmltv", "Starting grabber \"%s\"", prog);
  
  outlen = spawn_and_store_stdout(prog, NULL, &outbuf);
  if(outlen < 1) {
    tvhlog(LOG_ERR, "xmltv", "No output from \"%s\"", prog);
    return;
  }

  body = htsmsg_xml_deserialize(outbuf, errbuf, sizeof(errbuf));
  if(body == NULL) {
    tvhlog(LOG_ERR, "xmltv", "Unable to parse output from \"%s\": %s",
	   prog, errbuf);
    return;
  }

  pthread_mutex_lock(&global_lock);
  xmltv_parse(body);
  pthread_mutex_unlock(&global_lock);

  htsmsg_destroy(body);
}


/**
 *
 */
static void *
xmltv_thread(void *aux)
{
  while(1) {
    xmltv_grab();
    sleep(3600);
  }
}


/**
 *
 */
void
xmltv_init(void)
{
  pthread_t ptid;
  xmltv_load();
  pthread_create(&ptid, NULL, xmltv_thread, NULL);
}
