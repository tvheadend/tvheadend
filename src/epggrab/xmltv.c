/*
 *  Electronic Program Guide - xmltv grabber
 *  Copyright (C) 2012 Adam Sutton
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
#include <openssl/md5.h>

#include "htsmsg_xml.h"
#include "settings.h"

#include "tvheadend.h"
#include "channels.h"
#include "epg.h"
#include "xmltv.h"
#include "spawn.h"

/* **************************************************************************
 * Parsing
 * *************************************************************************/
#if TODO_XMLTV_GRABBER

/**
 * Hash the description to get a URI for episode
 *
 * Note: converts to an ASCII format
 * TODO: move somewhere else
 */
#if 0
static const char *_xmltv_hash ( const char *str )
{
  size_t i;
  static char ret[(MD5_DIGEST_LENGTH*2)+1];
  static unsigned char md5[MD5_DIGEST_LENGTH];
  (void)MD5((const unsigned char*)str, strlen(str), md5);
  for ( i = 0; i < MD5_DIGEST_LENGTH; i++ ) {
    sprintf(&ret[i*2], "%02X", md5[i]);
  }
  ret[MD5_DIGEST_LENGTH*2] = '\0';
  return ret;
}
#endif

/**
 *
 */
static time_t
_xmltv_str2time(const char *str)
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

#if 0
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
parse_xmltv_ns_episode  
(const char *s, int *season, int *episode, int *part)
{
  s = xmltv_ns_get_parse_num(s, season, NULL);
  s = xmltv_ns_get_parse_num(s, episode, NULL);
  xmltv_ns_get_parse_num(s, part, NULL);

  *season += 1;
  *episode += 1;
  *part += 1;
}

/**
 *
 */
static void
get_episode_info
(htsmsg_t *tags, const char **screen, int *season, int *episode, int *part)
{
  htsmsg_field_t *f;
  htsmsg_t *c, *a;
  const char *sys, *cdata;

  HTSMSG_FOREACH(f, tags) {
    if((c = htsmsg_get_map_by_field(f)) == NULL ||
       strcmp(f->hmf_name, "episode-num") ||
       (a = htsmsg_get_map(c, "attrib")) == NULL ||
       (cdata = htsmsg_get_str(c, "cdata")) == NULL ||
       (sys = htsmsg_get_str(a, "system")) == NULL)
      continue;
    
    if(!strcmp(sys, "onscreen")) *screen = cdata;
    else if(!strcmp(sys, "xmltv_ns"))
      parse_xmltv_ns_episode(cdata, season, episode, part);
  }
}

#endif

/**
 * Parse tags inside of a programme
 */
static int
_xmltv_parse_programme_tags(epg_channel_t *xc, htsmsg_t *tags, 
			   time_t start, time_t stop, epggrab_stats_t *stats)
{
#if TODO_XMLTV
  int save = 0, save2 = 0;
  epg_episode_t *ee;
  epg_broadcast_t *ebc;
  int sn = 0, en = 0, pn = 0;
  const char *onscreen = NULL;
  const char *uri = NULL;
  const char *title = htsmsg_xml_get_cdata_str(tags, "title");
  const char *desc  = htsmsg_xml_get_cdata_str(tags, "desc");
  const char *category = htsmsg_xml_get_cdata_str(tags, "category");
  get_episode_info(tags, &onscreen, &sn, &en, &pn);

  /* Create/Find broadcast */
  ebc = epg_broadcast_find_by_time(xc, start, stop, 1, &save2);
  if ( ebc != NULL ) {
    stats->broadcasts.total++;
    if (save2) stats->broadcasts.created++;
    save2 |= epg_broadcast_set_episode(ebc, ee);
    if (save2) stats->broadcasts.modified++;
  }

  // TODO:
  //   do we attempt to make an episode URI from info?
  //   OR
  //   do we first try to find the broadcast and do a fuzzy check on params?
  //   OR
  //   do we do something else?

  /* Generate a URI */
  if ( desc ) 
    uri = _xmltv_hash(desc);
  if ( !uri ) return 0;
  
  /* Find episode */
  ee = epg_episode_find_by_uri(uri, 1, &save);
  stats->episodes.total++;
  if (save) stats->episodes.created++;
  if (title) save |= epg_episode_set_title(ee, title);
  if (desc)  save |= epg_episode_set_description(ee, desc);
  if (category) save |= epg_episode_set_genre_str(ee, category);
  if (onscreen) save |= epg_episode_set_onscreen(ee, onscreen);
  if (pn) save |= epg_episode_set_part(ee, pn, 0);
  if (en) save |= epg_episode_set_number(ee, en);
  // TODO: how can we handle season number?
  if (save) stats->episodes.modified++;

  
  return save | save2;
#endif
  return 0;
}


/**
 * Parse a <programme> tag from xmltv
 */
static int
_xmltv_parse_programme(htsmsg_t *body, epggrab_stats_t *stats)
{
  int save = 0;
  htsmsg_t *attribs, *tags;
  const char *s, *chid;
  time_t start, stop;
  epg_channel_t *xc;

  if(body == NULL) return 0;

  if((attribs = htsmsg_get_map(body,    "attrib"))  == NULL) return 0;
  if((tags    = htsmsg_get_map(body,    "tags"))    == NULL) return 0;
  if((chid    = htsmsg_get_str(attribs, "channel")) == NULL) return 0;
  if((xc      = epg_channel_find_by_uri(chid, 0, NULL))   == NULL) return 0;
  if((s       = htsmsg_get_str(attribs, "start"))   == NULL) return 0;
  start = _xmltv_str2time(s);
  if((s       = htsmsg_get_str(attribs, "stop"))    == NULL) return 0;
  stop = _xmltv_str2time(s);

  if(stop <= start || stop < dispatch_clock) return 0;

  save |= _xmltv_parse_programme_tags(xc, tags, start, stop, stats);
  return save;
}

/**
 * Parse a <channel> tag from xmltv
 */
static int
_xmltv_parse_channel(htsmsg_t *body, epggrab_stats_t *stats)
{
  int save =0;
  htsmsg_t *attribs, *tags, *subtag;
  const char *id, *name, *icon;
  epg_channel_t *xc;

  if(body == NULL) return 0;

  if((attribs = htsmsg_get_map(body, "attrib"))  == NULL) return 0;
  if((id      = htsmsg_get_str(attribs, "id"))   == NULL) return 0;
  if((tags    = htsmsg_get_map(body, "tags"))    == NULL) return 0;
  if((xc      = epg_channel_find_by_uri(id, 1, &save))  == NULL) return 0;
  stats->channels.total++;
  if (save) stats->channels.created++;
  
  if((name = htsmsg_xml_get_cdata_str(tags, "display-name")) != NULL) {
    save |= epg_channel_set_name(xc, name);
  }

  if((subtag  = htsmsg_get_map(tags,    "icon"))   != NULL &&
     (attribs = htsmsg_get_map(subtag,  "attrib")) != NULL &&
     (icon    = htsmsg_get_str(attribs, "src"))    != NULL) {
    save |= epg_channel_set_icon(xc, icon);
  }
  if (save) stats->channels.modified++;
  return save;
}

/**
 *
 */
static int
_xmltv_parse_tv(htsmsg_t *body, epggrab_stats_t *stats)
{
  int save = 0;
  htsmsg_t *tags;
  htsmsg_field_t *f;

  if((tags = htsmsg_get_map(body, "tags")) == NULL)
    return 0;

  HTSMSG_FOREACH(f, tags) {
    if(!strcmp(f->hmf_name, "channel")) {
      save |= _xmltv_parse_channel(htsmsg_get_map_by_field(f), stats);
    } else if(!strcmp(f->hmf_name, "programme")) {
      save |= _xmltv_parse_programme(htsmsg_get_map_by_field(f), stats);
    }
  }
  return save;
}

/**
 *
 */
static int _xmltv_parse ( htsmsg_t *data, epggrab_stats_t *stats )
{
  htsmsg_t *tags, *tv;

  if((tags = htsmsg_get_map(data, "tags")) == NULL)
    return 0;

  if((tv = htsmsg_get_map(tags, "tv")) == NULL)
    return 0;

  return _xmltv_parse_tv(tv, stats);
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

// TODO: config
// TODO: remove use of hardcoded xmltv script

static epggrab_module_t xmltv_module;

static const char* _xmltv_name ( void )
{
  return "xmltv";
}

static htsmsg_t* _xmltv_grab ( const char *iopts )
{
  size_t outlen;
  char *outbuf;
  char errbuf[100];
  const char *cmd = "/home/aps/tmp/epg.sh";//usr/bin/tv_grab_uk_rt";

  /* Debug */
  tvhlog(LOG_DEBUG, "xmltv", "grab %s", cmd);

  /* Grab */
  outlen = spawn_and_store_stdout(cmd, NULL, &outbuf);
  if ( outlen < 1 ) {
    tvhlog(LOG_ERR, "xmltv", "no output detected");
    return NULL;
  }

  /* Extract */
  return htsmsg_xml_deserialize(outbuf, errbuf, sizeof(errbuf));
}

epggrab_module_t* xmltv_init ( void )
{
  xmltv_module.enable  = NULL;
  xmltv_module.disable = NULL;
  xmltv_module.grab    = _xmltv_grab;
  xmltv_module.parse   = _xmltv_parse;
  xmltv_module.name    = _xmltv_name;
  return &xmltv_module;
}

#endif
