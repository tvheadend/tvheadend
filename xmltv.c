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
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>

#include <libhts/htsmsg_xml.h>
#include <libhts/htssettings.h>

#include "tvhead.h"
#include "channels.h"
#include "epg.h"
#include "xmltv.h"
#include "spawn.h"


/**
 *
 */
LIST_HEAD(xmltv_grabber_list, xmltv_grabber);
static struct xmltv_grabber_list xmltv_grabbers;

typedef struct xmltv_grabber {
  LIST_ENTRY(xmltv_grabber) xg_link;
  char *xg_path;
  time_t xg_mtime;

  char *xg_version;
  char *xg_description;

  uint32_t xg_capabilities;
#define XG_CAP_BASELINE     0x1
#define XG_CAP_MANUALCONFIG 0x2
#define XG_CAP_CACHE        0x4
#define XG_CAP_APICONFIG    0x8

  int xg_dirty;

} xmltv_grabber_t;

static xmltv_grabber_t *xg_current;


uint32_t xmltv_grab_interval;

/* xmltv_channels is protected by global_lock */
static struct xmltv_channel_tree xmltv_channels;
struct xmltv_channel_list xmltv_displaylist;

pthread_mutex_t xmltv_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  xmltv_cond;
static int xmltv_confver;

static void xmltv_grabbers_index(void);
static void xmltv_grabbers_load(void);
static void xmltv_grabbers_save(void);

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
xmltv_grab(const char *prog)
{
  int outlen;
  char *outbuf;
  htsmsg_t *body;
  char errbuf[100];

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
  int confver = 0;
  struct timespec ts;
  char *p;

  pthread_mutex_lock(&xmltv_mutex);

  xmltv_grabbers_load();

  xmltv_grabbers_index();

  ts.tv_nsec = 0;
  ts.tv_sec = 0;

  while(1) {

    while(confver == xmltv_confver) {

      if(xg_current == NULL) {
	pthread_cond_wait(&xmltv_cond, &xmltv_mutex);
	continue;
      }
      if(pthread_cond_timedwait(&xmltv_cond, &xmltv_mutex, &ts) == ETIMEDOUT)
	break;
    }

    ts.tv_sec = time(NULL) + xmltv_grab_interval * 3600;

    confver = xmltv_confver;

    if(xg_current != NULL) {
      tvhlog(LOG_INFO, "xmltv",
	     "Grabbing \"%s\" using command \"%s\"",
	     xg_current->xg_description, xg_current->xg_path);

      /* Dup path so we can unlock */
      p = strdup(xg_current->xg_path);

      pthread_mutex_unlock(&xmltv_mutex);
      
      xmltv_grab(p);

      pthread_mutex_lock(&xmltv_mutex);
      free(p);
    }
  }
}


/**
 *
 */
void
xmltv_init(void)
{
  pthread_t ptid;
  xmltv_grab_interval = 12; /* Default half a day */

  /* Load all channels */
  xmltv_load();
 
  pthread_create(&ptid, NULL, xmltv_thread, NULL);
}


/**
 *
 */
static void
xmltv_grabber_destroy(xmltv_grabber_t *xg)
{
  LIST_REMOVE(xg, xg_link);
  free(xg->xg_path);
  free(xg->xg_version);
  free(xg->xg_description);
  free(xg);
}

/**
 *
 */
static char *
xmltv_grabber_get_description(const char *prog)
{
  int i, outlen;
  char *outbuf;
  const char *args[] = {prog, "--description", NULL};

  if((outlen = spawn_and_store_stdout(prog, (void *)args, &outbuf)) < 1)
    return NULL;

  for(i = 0; i < outlen; i++)
    if(outbuf[i] < 32)
      outbuf[i] = 0;
  return outbuf;
}



/**
 *
 */
static char *
xmltv_grabber_get_version(const char *prog)
{
  int outlen;
  char *outbuf;
  const char *args[] = {prog, "--version", NULL};

  if((outlen = spawn_and_store_stdout(prog, (void *)args, &outbuf)) < 1)
    return NULL;
  return outbuf;
}

/**
 *
 */
static int
xmltv_grabber_get_capabilities(const char *prog)
{
  int outlen;
  char *outbuf;
  int r;

  const char *args[] = {prog, "--capabilities", NULL};

  if((outlen = spawn_and_store_stdout(prog, (void *)args, &outbuf)) < 1)
    return 0;

  r  = strstr(outbuf, "baseline")     ? XG_CAP_BASELINE     : 0;
  r |= strstr(outbuf, "manualconfig") ? XG_CAP_MANUALCONFIG : 0;
  r |= strstr(outbuf, "cache")        ? XG_CAP_CACHE        : 0;
  r |= strstr(outbuf, "apiconfig")    ? XG_CAP_APICONFIG    : 0;
  return r;
}

/**
 *
 */
static int
xg_desc_cmp(const xmltv_grabber_t *a, const xmltv_grabber_t *b)
{
  return strcmp(a->xg_description, b->xg_description);
}

/**
 *
 */
static int
xmltv_grabber_add(const char *path, time_t mtime)
{
  char *desc;
  xmltv_grabber_t *xg;
  int wasthisgrabber = 0;

  LIST_FOREACH(xg, &xmltv_grabbers, xg_link)
    if(!strcmp(xg->xg_path, path))
      break;

  if(xg != NULL) {
    if(xg->xg_mtime == mtime) {
      xg->xg_dirty = 0;
      return 0; /* Already there */
    }
    wasthisgrabber = xg == xg_current;
    xmltv_grabber_destroy(xg);
    xg_current = NULL;
  }
  if((desc = xmltv_grabber_get_description(path)) == NULL)
    return 0; /* Probably not a working grabber */

  xg = calloc(1, sizeof(xmltv_grabber_t));
  xg->xg_path = strdup(path);
  xg->xg_mtime = mtime;
  xg->xg_description = strdup(desc);
  xg->xg_capabilities = xmltv_grabber_get_capabilities(path);
  tvh_str_set(&xg->xg_version, xmltv_grabber_get_version(path));
  LIST_INSERT_SORTED(&xmltv_grabbers, xg, xg_link, xg_desc_cmp);
  if(wasthisgrabber)
    xg_current = xg;
  return 1;
}


/**
 *
 */
static xmltv_grabber_t *
xmltv_grabber_find_by_path(const char *path)
{
  xmltv_grabber_t *xg;

  LIST_FOREACH(xg, &xmltv_grabbers, xg_link)
    if(!strcmp(xg->xg_path, path))
      break;
  return xg;
}


/**
 *
 */
static int
xmltv_grabber_filter(const struct dirent *d)
{
  if(strncmp(d->d_name, "tv_grab_", 8))
    return 0;
  return 1;

}


/**
 *
 */
static int
xmltv_scan_grabbers(const char *path)
{
  struct dirent **namelist;
  int n, change = 0;
  char fname[300];
  struct stat st;

  if((n = scandir(path, &namelist, xmltv_grabber_filter, NULL)) < 0)
    return 0;

  while(n--) {
    snprintf(fname, sizeof(fname), "%s/%s", path, namelist[n]->d_name);
    if(!stat(fname, &st))
      change |= xmltv_grabber_add(fname, st.st_mtime);
  }
  free(namelist);
  return change;
}


/**
 *
 */
static void
xmltv_grabbers_index(void)
{
  xmltv_grabber_t *xg, *next;
  int change;

  LIST_FOREACH(xg, &xmltv_grabbers, xg_link)
    xg->xg_dirty = 1;

  change =  xmltv_scan_grabbers("/bin");
  change |= xmltv_scan_grabbers("/usr/bin");
  change |= xmltv_scan_grabbers("/usr/local/bin");

  for(xg = LIST_FIRST(&xmltv_grabbers); xg != NULL; xg = next) {
    next = LIST_NEXT(xg, xg_link);
    if(xg->xg_dirty) {
      change = 1;
      xmltv_grabber_destroy(xg);
    }
  }

  if(change)
    xmltv_grabbers_save();
}

/**
 *
 */
static void
xmltv_grabbers_load(void)
{
  xmltv_grabber_t *xg;
  htsmsg_t *l, *c, *m;
  htsmsg_field_t *f;
  const char *path, *desc;
  uint32_t u32;

  if((m = hts_settings_load("xmltv/config")) == NULL)
    return;

  htsmsg_get_u32(m, "grab-interval", &xmltv_grab_interval);

  if((l = htsmsg_get_array(m, "grabbers")) != NULL) {

    HTSMSG_FOREACH(f, l) {
      if((c = htsmsg_get_msg_by_field(f)) == NULL)
	continue;

      path = htsmsg_get_str(c, "path");
      desc = htsmsg_get_str(c, "description");
      if(path == NULL || desc == NULL)
	continue;

      xg = calloc(1, sizeof(xmltv_grabber_t));
      xg->xg_path = strdup(path);
      xg->xg_description = strdup(desc);

      if(!htsmsg_get_u32(c, "mtime", &u32))
	xg->xg_mtime = u32;

      tvh_str_set(&xg->xg_version, htsmsg_get_str(c, "version"));
      htsmsg_get_u32(c, "capabilities", &xg->xg_capabilities);
      LIST_INSERT_SORTED(&xmltv_grabbers, xg, xg_link, xg_desc_cmp);
    }
  }

  if((path = htsmsg_get_str(m, "current-grabber")) != NULL) {
    if((xg = xmltv_grabber_find_by_path(path)) != NULL)
      xg_current = xg;
  }
}


static void
xmltv_grabbers_save(void)
{
  htsmsg_t *array, *e, *m;
  xmltv_grabber_t *xg;

  m = htsmsg_create();

  array = htsmsg_create_array();

  LIST_FOREACH(xg, &xmltv_grabbers, xg_link) {
    e = htsmsg_create();
    htsmsg_add_str(e, "path", xg->xg_path);
    htsmsg_add_str(e, "description", xg->xg_description);
    if(xg->xg_version != NULL)
      htsmsg_add_str(e, "version", xg->xg_version);
    htsmsg_add_u32(e, "mtime", xg->xg_mtime);
    htsmsg_add_u32(e, "capabilities", xg->xg_capabilities);

    htsmsg_add_msg(array, NULL, e);
  }

  htsmsg_add_msg(m, "grabbers", array);

  htsmsg_add_u32(m, "grab-interval", xmltv_grab_interval);
  if(xg_current != NULL)
    htsmsg_add_str(m, "current-grabber", xg_current->xg_path);

  hts_settings_save(m, "xmltv/config");
  htsmsg_destroy(m);
}


/**
 *
 */
htsmsg_t *
xmltv_list_grabbers(void)
{
  htsmsg_t *array, *m;
  xmltv_grabber_t *xg;

  lock_assert(&xmltv_mutex);

  xmltv_grabbers_index();

  array = htsmsg_create_array();

  LIST_FOREACH(xg, &xmltv_grabbers, xg_link) {
    m = htsmsg_create();
    htsmsg_add_str(m, "identifier", xg->xg_path);
    htsmsg_add_str(m, "name", xg->xg_description);
    htsmsg_add_str(m, "version", xg->xg_version ?: "Unknown version");
    htsmsg_add_u32(m, "apiconfig", !!(xg->xg_capabilities & XG_CAP_APICONFIG));
    htsmsg_add_msg(array, NULL, m);
  }

  return array;
}


/**
 *
 */
const char *
xmltv_get_current_grabber(void)
{
  lock_assert(&xmltv_mutex);
  return xg_current ? xg_current->xg_description : NULL;
}


/**
 *
 */
void
xmltv_set_current_grabber(const char *desc)
{
  xmltv_grabber_t *xg;

  lock_assert(&xmltv_mutex);

  if(desc != NULL) {
    LIST_FOREACH(xg, &xmltv_grabbers, xg_link)
      if(!strcmp(xg->xg_description, desc))
	break;
    if(xg == NULL)
      return;
  } else {
    xg = NULL;
  }

  if(xg_current == xg)
    return;

  xg_current = xg;

  xmltv_confver++;
  pthread_cond_signal(&xmltv_cond);
  xmltv_grabbers_save();
}
