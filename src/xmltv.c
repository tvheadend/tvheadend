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

#include "htsmsg_xml.h"
#include "settings.h"

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


typedef struct parse_stats {
  int ps_channels;
  int ps_programmes;
  int ps_events_created;

} parse_stats_t;




static xmltv_grabber_t *xg_current;


uint32_t xmltv_grab_interval;
uint32_t xmltv_grab_enabled;

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

  if((l = hts_settings_load("xmltv/channels")) == NULL)
    return;

  HTSMSG_FOREACH(f, l) {
    if((c = htsmsg_get_map_by_field(f)) == NULL)
      continue;
    
    xc = xmltv_channel_find(f->hmf_name, 1);
    tvh_str_set(&xc->xc_icon, htsmsg_get_str(c, "icon"));
    
    xc_set_displayname(xc, htsmsg_get_str(c, "displayname"));
  }
  htsmsg_destroy(l);
}


/**
 *
 */
static void
xmltv_save(xmltv_channel_t *xc)
{
  htsmsg_t *m = htsmsg_create_map();

  if(xc->xc_displayname != NULL)
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
  struct tm tm;
  int tz, r;

  memset(&tm, 0, sizeof(tm));

  r = sscanf(str, "%04d%02d%02d%02d%02d%02d %d",
	     &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
	     &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
	     &tz);

  tm.tm_mon  -= 1;
  tm.tm_year -= 1900;
  tm.tm_isdst = -1;

  tz = (tz % 100) + (tz / 100) * 60; // Convert from HHMM to minutes

  if(r == 6) {
    return mktime(&tm);
  } else if(r == 7) {
    return timegm(&tm) - tz * 60;
  } else {
    return 0;
  }
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
    if(!strcmp(xc->xc_displayname, name))
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
  if((sub = htsmsg_get_map(tags, name)) == NULL)
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

  if((attribs = htsmsg_get_map(body, "attrib")) == NULL)
    return;

  tags = htsmsg_get_map(body, "tags");
  
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

  if((subtag  = htsmsg_get_map(tags,    "icon"))   != NULL &&
     (attribs = htsmsg_get_map(subtag,  "attrib")) != NULL &&
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
 * This is probably the most obscure formating there is. From xmltv.dtd:
 *
 *
 * xmltv_ns: This is intended to be a general way to number episodes and
 * parts of multi-part episodes.  It is three numbers separated by dots,
 * the first is the series or season, the second the episode number
 * within that series, and the third the part number, if the programme is
 * part of a two-parter.  All these numbers are indexed from zero, and
 * they can be given in the form 'X/Y' to show series X out of Y series
 * made, or episode X out of Y episodes in this series, or part X of a
 * Y-part episode.  If any of these aren't known they can be omitted.
 * You can put spaces whereever you like to make things easier to read.
 * 
 * (NB 'part number' is not used when a whole programme is split in two
 * for purely scheduling reasons; it's intended for cases where there
 * really is a 'Part One' and 'Part Two'.  The format doesn't currently
 * have a way to represent a whole programme that happens to be split
 * across two or more timeslots.)
 * 
 * Some examples will make things clearer.  The first episode of the
 * second series is '1.0.0/1' .  If it were a two-part episode, then the
 * first half would be '1.0.0/2' and the second half '1.0.1/2'.  If you
 * know that an episode is from the first season, but you don't know
 * which episode it is or whether it is part of a multiparter, you could
 * give the episode-num as '0..'.  Here the second and third numbers have
 * been omitted.  If you know that this is the first part of a three-part
 * episode, which is the last episode of the first series of thirteen,
 * its number would be '0 . 12/13 . 0/3'.  The series number is just '0'
 * because you don't know how many series there are in total - perhaps
 * the show is still being made!
 *
 */

static const char *
xmltv_ns_get_parse_num(const char *s, int *ap, int *bp)
{
  int a = -1, b = -1;

  while(1) {
    if(!*s)
      goto out;

    if(*s == '.') {
      s++;
      goto out;
    }

    if(*s == '/')
      break;

    if(*s >= '0' && *s <= '9') {
      if(a == -1)
	a = 0;
      a = a * 10 + *s - '0';
    }
    s++;
  }

  s++; // slash

  while(1) {
    if(!*s)
      break;

    if(*s == '.') {
      s++;
      break;
    }

    if(*s >= '0' && *s <= '9') {
      if(b == -1)
	b = 0;
      b = b * 10 + *s - '0';
    }
    s++;
  }


 out:
  if(ap) *ap = a;
  if(bp) *bp = b;
  return s;
}




static void
parse_xmltv_ns_episode(const char *s, epg_episode_t *ee)
{
  int season;
  int episode;
  int part;

  s = xmltv_ns_get_parse_num(s, &season, NULL);
  s = xmltv_ns_get_parse_num(s, &episode, NULL);
  xmltv_ns_get_parse_num(s, &part, NULL);

  if(season != -1)
    ee->ee_season = season + 1;
  if(episode != -1)
    ee->ee_episode = episode + 1;
  if(part != -1)
    ee->ee_part = part + 1;
}

/**
 *
 */
static void
get_episode_info(htsmsg_t *tags, epg_episode_t *ee)
{
  htsmsg_field_t *f;
  htsmsg_t *c, *a;
  const char *sys, *cdata;

  memset(ee, 0, sizeof(epg_episode_t));

  HTSMSG_FOREACH(f, tags) {
    if((c = htsmsg_get_map_by_field(f)) == NULL ||
       strcmp(f->hmf_name, "episode-num") ||
       (a = htsmsg_get_map(c, "attrib")) == NULL ||
       (cdata = htsmsg_get_str(c, "cdata")) == NULL ||
       (sys = htsmsg_get_str(a, "system")) == NULL)
      continue;
    
    if(!strcmp(sys, "onscreen"))
      tvh_str_set(&ee->ee_onscreen, cdata);
    else if(!strcmp(sys, "xmltv_ns"))
      parse_xmltv_ns_episode(cdata, ee);
  }
}

/**
 * Parse tags inside of a programme
 */
static void
xmltv_parse_programme_tags(xmltv_channel_t *xc, htsmsg_t *tags, 
			   time_t start, time_t stop, parse_stats_t *ps)
{
  event_t *e;
  channel_t *ch;
  const char *title = xmltv_get_cdata_by_tag(tags, "title");
  const char *desc  = xmltv_get_cdata_by_tag(tags, "desc");
  const char *category = xmltv_get_cdata_by_tag(tags, "category");
  int created;
  epg_episode_t episode;

  get_episode_info(tags, &episode);

  LIST_FOREACH(ch, &xc->xc_channels, ch_xc_link) {
    if((e = epg_event_create(ch, start, stop, -1, &created)) == NULL)
      continue;

    if(created)
      ps->ps_events_created++;

    int changed = 0;

    if(title != NULL)
      changed |= epg_event_set_title(e, title);

    if(desc  != NULL)
      changed |= epg_event_set_desc(e, desc);

    if(category != NULL) {
      uint8_t type = epg_content_group_find_by_name(category);
      if(type)
	changed |= epg_event_set_content_type(e, type);
    }

    changed |= epg_event_set_episode(e, &episode);

    if(changed)
      epg_event_updated(e);
  }

  free(episode.ee_onscreen);
}


/**
 * Parse a <programme> tag from xmltv
 */
static void
xmltv_parse_programme(htsmsg_t *body, parse_stats_t *ps)
{
  htsmsg_t *attribs, *tags;
  const char *s, *chid;
  time_t start, stop;
  xmltv_channel_t *xc;

  if(body == NULL)
    return;

  if((attribs = htsmsg_get_map(body, "attrib")) == NULL)
    return;

  if((tags = htsmsg_get_map(body, "tags")) == NULL)
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
    xmltv_parse_programme_tags(xc, tags, start, stop, ps);
  }
}

/**
 *
 */
static void
xmltv_parse_tv(htsmsg_t *body, parse_stats_t *ps)
{
  htsmsg_t *tags;
  htsmsg_field_t *f;

  if((tags = htsmsg_get_map(body, "tags")) == NULL)
    return;

  HTSMSG_FOREACH(f, tags) {
    if(!strcmp(f->hmf_name, "channel")) {
      xmltv_parse_channel(htsmsg_get_map_by_field(f));
      ps->ps_channels++;
    } else if(!strcmp(f->hmf_name, "programme")) {
      xmltv_parse_programme(htsmsg_get_map_by_field(f), ps);
      ps->ps_programmes++;
    }
  }
}


/**
 *
 */
static void
xmltv_parse(htsmsg_t *body, parse_stats_t *ps)
{
  htsmsg_t *tags, *tv;

  if((tags = htsmsg_get_map(body, "tags")) == NULL)
    return;

  if((tv = htsmsg_get_map(tags, "tv")) == NULL)
    return;

  xmltv_parse_tv(tv, ps);
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
  time_t t1, t2;
  parse_stats_t ps = {0};

  time(&t1);
  outlen = spawn_and_store_stdout(prog, NULL, &outbuf);
  if(outlen < 1) {
    tvhlog(LOG_ERR, "xmltv", "No output from \"%s\"", prog);
    return;
  }

  time(&t2);

  tvhlog(LOG_DEBUG, "xmltv",
	 "%s: completed, took %ld seconds",
	 prog, t2 - t1);

  body = htsmsg_xml_deserialize(outbuf, errbuf, sizeof(errbuf));
  if(body == NULL) {
    tvhlog(LOG_ERR, "xmltv", "Unable to parse output from \"%s\": %s",
	   prog, errbuf);
    return;
  }


  pthread_mutex_lock(&global_lock);
  xmltv_parse(body, &ps);
  pthread_mutex_unlock(&global_lock);

  tvhlog(LOG_INFO, "xmltv",
	 "%s: Parsing completed. XML contained %d channels, %d events, "
	 "%d new events injected in EPG",
	 prog, 
	 ps.ps_channels,
	 ps.ps_programmes,
	 ps.ps_events_created);

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

      if(xg_current == NULL || !xmltv_grab_enabled) {
	pthread_cond_wait(&xmltv_cond, &xmltv_mutex);
	continue;
      }
      if(pthread_cond_timedwait(&xmltv_cond, &xmltv_mutex, &ts) == ETIMEDOUT)
	break;
    }

    confver = xmltv_confver;

    if(xmltv_grab_enabled == 0)
      continue;

    ts.tv_sec = time(NULL) + xmltv_grab_interval * 3600;

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
  return NULL;
}


/**
 *
 */
void
xmltv_init(void)
{
  pthread_t ptid;

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  xmltv_grab_interval = 12; /* Default half a day */
  xmltv_grab_enabled  = 1;  /* Default on */

  /* Load all channels */
  xmltv_load();
 
  pthread_create(&ptid, &attr, xmltv_thread, NULL);
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
    free(namelist[n]);
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
  char *path, *p, *s;

  LIST_FOREACH(xg, &xmltv_grabbers, xg_link)
    xg->xg_dirty = 1;

  change =  xmltv_scan_grabbers("/bin");
  change |= xmltv_scan_grabbers("/usr/bin");
  change |= xmltv_scan_grabbers("/usr/local/bin");
  // Arch linux puts xmltv grabbers in /usr/bin/perlbin/vendor
  change |= xmltv_scan_grabbers("/usr/bin/perlbin/vendor"); 

  if((path = getenv("PATH")) != NULL) {
    p = path = strdup(path);
 
    while((s = strsep(&p, ":")) != NULL)
      change |= xmltv_scan_grabbers(s);
    free(path);
  }

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
  htsmsg_get_u32(m, "grab-enabled", &xmltv_grab_enabled);

  if((l = htsmsg_get_list(m, "grabbers")) != NULL) {

    HTSMSG_FOREACH(f, l) {
      if((c = htsmsg_get_map_by_field(f)) == NULL)
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

  htsmsg_destroy(m);
}


static void
xmltv_grabbers_save(void)
{
  htsmsg_t *array, *e, *m;
  xmltv_grabber_t *xg;

  m = htsmsg_create_map();

  array = htsmsg_create_list();

  LIST_FOREACH(xg, &xmltv_grabbers, xg_link) {
    e = htsmsg_create_map();
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
  htsmsg_add_u32(m, "grab-enabled", xmltv_grab_enabled);
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

  array = htsmsg_create_list();

  LIST_FOREACH(xg, &xmltv_grabbers, xg_link) {
    m = htsmsg_create_map();
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

/**
 *
 */
void
xmltv_set_grab_interval(int s)
{
  lock_assert(&xmltv_mutex);

  xmltv_grab_interval = s;
  xmltv_confver++;
  pthread_cond_signal(&xmltv_cond);
  xmltv_grabbers_save();

}

/**
 *
 */
void
xmltv_set_grab_enable(int on)
{
  lock_assert(&xmltv_mutex);

  xmltv_grab_enabled = on;
  xmltv_confver++;
  pthread_cond_signal(&xmltv_cond);
  xmltv_grabbers_save();
}
