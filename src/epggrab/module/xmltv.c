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
#include "spawn.h"
#include "htsstr.h"

#include "epg.h"
#include "epggrab.h"
#include "epggrab/private.h"

#define XMLTV_FIND "tv_find_grabbers"
#define XMLTV_GRAB "tv_grab_"

static epggrab_channel_tree_t _xmltv_channels;
static epggrab_module_t      *_xmltv_module;

static epggrab_channel_t *_xmltv_channel_find
  ( const char *id, int create, int *save )
{
  return epggrab_channel_find(&_xmltv_channels, id, create, save,
                              _xmltv_module);
}

/* **************************************************************************
 * Parsing
 * *************************************************************************/

/**
 *
 */
static time_t _xmltv_str2time(const char *str)
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

static const char *xmltv_ns_get_parse_num
  (const char *s, int *ap, int *bp)
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
  if(ap) *ap = a + 1;
  if(bp) *bp = b + 1;
  return s;
}

static void parse_xmltv_ns_episode
  (const char *s, int *sn, int *sc, int *en, int *ec, int *pn, int *pc)
{
  s = xmltv_ns_get_parse_num(s, sn, sc);
  s = xmltv_ns_get_parse_num(s, en, ec);
  xmltv_ns_get_parse_num(s, pn, pc);
}

static void parse_xmltv_dd_progid
  (epggrab_module_t *mod, const char *s, char **uri, char **suri, int *en )
{
  if (strlen(s) < 2) return;
  *uri = malloc(strlen(mod->id) + 1 + strlen(s));
  sprintf(*uri, "%s-%s", mod->id, s);

  /* Episode */
  if (!strncmp("EP", s, 2) || !strncmp("SH", s, 2)) {
    int e = 0;
    while (s[e] && s[e] != '.') e++;
    *suri = hts_strndup(s, e);
    if (s[e] && s[e+1]) sscanf(s+e+1, "%d", en);
  }
}

/**
 *
 */
static void get_episode_info
  (epggrab_module_t *mod,
   htsmsg_t *tags, char **uri, char **suri, const char **screen,
   int *sn, int *sc, int *en, int *ec, int *pn, int *pc)
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
    
    if(!strcmp(sys, "onscreen"))
      *screen = cdata;
    else if(!strcmp(sys, "xmltv_ns"))
      parse_xmltv_ns_episode(cdata, sn, sc, en, ec, pn, pc);
    else if(!strcmp(sys, "dd_progid"))
      parse_xmltv_dd_progid(mod, cdata, uri, suri, en);
  }
}

/*
 * Process video quality flags
 *
 * Note: this is very rough/approx someone might be able to do a much better
 *       job
 */
static int
parse_vid_quality
  ( epggrab_module_t *mod, epg_broadcast_t *ebc, epg_episode_t *ee,
    htsmsg_t *m )
{
  int save = 0;
  int hd = 0, lines = 0, aspect = 0;
  const char *str;
  if (!ebc || !m) return 0;

  if ((str = htsmsg_xml_get_cdata_str(m, "colour")))
    save |= epg_episode_set_is_bw(ee, strcmp(str, "no") ? 0 : 1, mod);
  if ((str = htsmsg_xml_get_cdata_str(m, "quality"))) {
    if (strstr(str, "HD")) {
      hd    = 1;
    } else if (strstr(str, "480")) {
      lines  = 480;
      aspect = 150;
    } else if (strstr(str, "576")) {
      lines  = 576;
      aspect = 133;
    } else if (strstr(str, "720")) {
      lines  = 720;
      hd     = 1;
      aspect = 178;
    } else if (strstr(str, "1080")) {
      lines  = 1080;
      hd     = 1;
      aspect = 178;
    }
  }
  if ((str = htsmsg_xml_get_cdata_str(m, "aspect"))) {
    int w, h;
    if (sscanf(str, "%d:%d", &w, &h) == 2) {
      aspect = (100 * w) / h;
    }
  }
  save |= epg_broadcast_set_is_hd(ebc, hd, mod);
  if (aspect) {
    save |= epg_broadcast_set_is_widescreen(ebc, hd || aspect > 137, mod);
    save |= epg_broadcast_set_aspect(ebc, aspect, mod);
  }
  if (lines)
    save |= epg_broadcast_set_lines(ebc, lines, mod);
  
  return save;
}

/*
 * Parse accessibility data
 */
int
xmltv_parse_accessibility 
  ( epggrab_module_t *mod, epg_broadcast_t *ebc, htsmsg_t *m )
{
  int save = 0;
  htsmsg_t *tag;
  htsmsg_field_t *f;
  const char *str;

  HTSMSG_FOREACH(f, m) {
    if(!strcmp(f->hmf_name, "subtitles")) {
      if ((tag = htsmsg_get_map_by_field(f))) {
        str = htsmsg_xml_get_attr_str(tag, "type");
        if (str && !strcmp(str, "teletext"))
          save |= epg_broadcast_set_is_subtitled(ebc, 1, mod);
        else if (str && !strcmp(str, "deaf-signed"))
          save |= epg_broadcast_set_is_deafsigned(ebc, 1, mod);
      }
    } else if (!strcmp(f->hmf_name, "audio-described")) {
      save |= epg_broadcast_set_is_audio_desc(ebc, 1, mod);
    }
  }
  return save;
}

/*
 * Parse category list
 */
static epg_genre_list_t
*_xmltv_parse_categories ( htsmsg_t *tags )
{
  htsmsg_t *e;
  htsmsg_field_t *f;
  epg_genre_list_t *egl = NULL;
  HTSMSG_FOREACH(f, tags) {
    if (!strcmp(f->hmf_name, "category") && (e = htsmsg_get_map_by_field(f))) {
      if (!egl) egl = calloc(1, sizeof(epg_genre_list_t));
      epg_genre_list_add_by_str(egl, htsmsg_get_str(e, "cdata"));
    }
  }
  return egl;
}

/**
 * Parse tags inside of a programme
 */
static int _xmltv_parse_programme_tags
  (epggrab_module_t *mod, channel_t *ch, htsmsg_t *tags, 
   time_t start, time_t stop, epggrab_stats_t *stats)
{
  int save = 0, save2 = 0, save3 = 0;
  epg_episode_t *ee = NULL;
  epg_season_t *es = NULL;
  epg_broadcast_t *ebc;
  epg_genre_list_t *egl;
  int sn = 0, sc = 0, en = 0, ec = 0, pn = 0, pc = 0;
  const char *onscreen = NULL;
  char *suri = NULL, *uri = NULL;
  const char *title = htsmsg_xml_get_cdata_str(tags, "title");
  const char *desc  = htsmsg_xml_get_cdata_str(tags, "desc");

  /* Ignore */
  if (!title) return 0;

  get_episode_info(mod, tags, &uri, &suri, &onscreen,
                   &sn, &sc, &en, &ec, &pn, &pc);

  /*
   * Season
   */
  if (suri) {
    es = epg_season_find_by_uri(suri, 1, &save3);
    free(suri);
    if (es) stats->seasons.total++;
    if (save3) stats->seasons.created++;
  }

  /*
   * Episode
   */
  if (!uri) uri = epg_hash(title, NULL, desc);
  if (uri) {
    ee  = epg_episode_find_by_uri(uri, 1, &save);
    free(uri);
  }
  if (!ee) return 0;
  stats->episodes.total++;
  if (save) stats->episodes.created++;

  if (es)
    save |= epg_episode_set_season(ee, es, mod);
  if (title)
    save |= epg_episode_set_title(ee, title, mod);
  if (desc)
    save |= epg_episode_set_description(ee, desc, mod);
  if ((egl = _xmltv_parse_categories(tags))) {
    save |= epg_episode_set_genre(ee, egl, mod);
    epg_genre_list_destroy(egl);
  }
  if (pn)   save |= epg_episode_set_part(ee, pn, pc, mod);
  if (en)   save |= epg_episode_set_number(ee, en, mod);
  if (save) stats->episodes.modified++;

  /*
   * Broadcast
   */

  // TODO: need to handle certification and ratings
  // TODO: need to handle season numbering!
  // TODO: need to handle onscreen numbering
  //if (onscreen) save |= epg_episode_set_onscreen(ee, onscreen);

  /* Create/Find broadcast */
  ebc = epg_broadcast_find_by_time(ch, start, stop, 0, 1, &save2);
  if ( ebc ) {
    stats->broadcasts.total++;
    if (save2) stats->broadcasts.created++;
    save2 |= epg_broadcast_set_episode(ebc, ee, mod);

    /* Quality metadata */
    save2 |= parse_vid_quality(mod, ebc, ee, htsmsg_get_map(tags, "video"));

    /* Accessibility */
    save2 |= xmltv_parse_accessibility(mod, ebc, tags);

    /* Misc */
    if (htsmsg_get_map(tags, "previously-shown"))
      save |= epg_broadcast_set_is_repeat(ebc, 1, mod);
    else if (htsmsg_get_map(tags, "premiere") ||
             htsmsg_get_map(tags, "new"))
      save |= epg_broadcast_set_is_new(ebc, 1, mod);
  
    /* Stats */
    if (save2) stats->broadcasts.modified++;
  }
  
  return save | save2 | save3;
}

/**
 * Parse a <programme> tag from xmltv
 */
static int _xmltv_parse_programme
  (epggrab_module_t *mod, htsmsg_t *body, epggrab_stats_t *stats)
{
  int save = 0;
  htsmsg_t *attribs, *tags;
  const char *s, *chid;
  time_t start, stop;
  epggrab_channel_t *ch;

  if(body == NULL) return 0;

  if((attribs = htsmsg_get_map(body,    "attrib"))  == NULL) return 0;
  if((tags    = htsmsg_get_map(body,    "tags"))    == NULL) return 0;
  if((chid    = htsmsg_get_str(attribs, "channel")) == NULL) return 0;
  if((ch      = _xmltv_channel_find(chid, 0, NULL))   == NULL) return 0;
  if (ch->channel == NULL) return 0;
  if((s       = htsmsg_get_str(attribs, "start"))   == NULL) return 0;
  start = _xmltv_str2time(s);
  if((s       = htsmsg_get_str(attribs, "stop"))    == NULL) return 0;
  stop  = _xmltv_str2time(s);

  if(stop <= start || stop <= dispatch_clock) return 0;

  save |= _xmltv_parse_programme_tags(mod, ch->channel, tags,
                                      start, stop, stats);
  return save;
}

/**
 * Parse a <channel> tag from xmltv
 */
static int _xmltv_parse_channel
  (epggrab_module_t *mod, htsmsg_t *body, epggrab_stats_t *stats)
{
  int save =0;
  htsmsg_t *attribs, *tags, *subtag;
  const char *id, *name, *icon;
  epggrab_channel_t *ch;

  if(body == NULL) return 0;

  if((attribs = htsmsg_get_map(body, "attrib"))  == NULL) return 0;
  if((id      = htsmsg_get_str(attribs, "id"))   == NULL) return 0;
  if((tags    = htsmsg_get_map(body, "tags"))    == NULL) return 0;
  if((ch      = _xmltv_channel_find(id, 1, &save)) == NULL) return 0;
  stats->channels.total++;
  if (save) stats->channels.created++;
  
  if((name = htsmsg_xml_get_cdata_str(tags, "display-name")) != NULL) {
    save |= epggrab_channel_set_name(ch, name);
  }

  if((subtag  = htsmsg_get_map(tags,    "icon"))   != NULL &&
     (attribs = htsmsg_get_map(subtag,  "attrib")) != NULL &&
     (icon    = htsmsg_get_str(attribs, "src"))    != NULL) {
    save |= epggrab_channel_set_icon(ch, icon);
  }
  if (save) {
    epggrab_channel_updated(ch);
    stats->channels.modified++;
  }
  return save;
}

/**
 *
 */
static int _xmltv_parse_tv
  (epggrab_module_t *mod, htsmsg_t *body, epggrab_stats_t *stats)
{
  int save = 0;
  htsmsg_t *tags;
  htsmsg_field_t *f;

  if((tags = htsmsg_get_map(body, "tags")) == NULL)
    return 0;

  HTSMSG_FOREACH(f, tags) {
    if(!strcmp(f->hmf_name, "channel")) {
      save |= _xmltv_parse_channel(mod, htsmsg_get_map_by_field(f), stats);
    } else if(!strcmp(f->hmf_name, "programme")) {
      save |= _xmltv_parse_programme(mod, htsmsg_get_map_by_field(f), stats);
    }
  }
  return save;
}

static int _xmltv_parse
  ( void *mod, htsmsg_t *data, epggrab_stats_t *stats )
{
  htsmsg_t *tags, *tv;

  if((tags = htsmsg_get_map(data, "tags")) == NULL)
    return 0;

  if((tv = htsmsg_get_map(tags, "tv")) == NULL)
    return 0;

  return _xmltv_parse_tv(mod, tv, stats);
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static void _xmltv_load_grabbers ( void )
{
  int outlen;
  size_t i, p, n;
  char *outbuf;
  char name[1000];
  char *tmp, *path;

  /* Load data */
  outlen = spawn_and_store_stdout(XMLTV_FIND, NULL, &outbuf);

  /* Process */
  if ( outlen > 0 ) {
    p = n = i = 0;
    while ( i < outlen ) {
      if ( outbuf[i] == '\n' || outbuf[i] == '\0' ) {
        outbuf[i] = '\0';
        sprintf(name, "XMLTV: %s", &outbuf[n]);
        epggrab_module_int_create(NULL, &outbuf[p], name, 3, &outbuf[p],
                                NULL, _xmltv_parse, NULL, NULL);
        p = n = i + 1;
      } else if ( outbuf[i] == '|' ) {
        outbuf[i] = '\0';
        n = i + 1;
      }
      i++;
    }
    free(outbuf);

  /* Internal search */
  } else if ((tmp = getenv("PATH"))) {
    tvhlog(LOG_DEBUG, "epggrab", "using internal grab search");
    char bin[256];
    char desc[] = "--description";
    char *argv[] = {
      NULL,
      desc,
      NULL
    };
    path = strdup(tmp);
    tmp  = strtok(path, ":");
    while (tmp) {
      DIR *dir;
      struct dirent *de;
      struct stat st;
      if ((dir = opendir(tmp))) {
        while ((de = readdir(dir))) {
          if (strstr(de->d_name, XMLTV_GRAB) != de->d_name) continue;
          snprintf(bin, sizeof(bin), "%s/%s", tmp, de->d_name);
          if (lstat(bin, &st)) continue;
          if (!(st.st_mode & S_IEXEC)) continue;
          if (!S_ISREG(st.st_mode)) continue;
          if ((outlen = spawn_and_store_stdout(bin, argv, &outbuf)) > 0) {
            if (outbuf[outlen-1] == '\n') outbuf[outlen-1] = '\0';
            snprintf(name, sizeof(name), "XMLTV: %s", outbuf);
            epggrab_module_int_create(NULL, bin, name, 3, bin,
                                      NULL, _xmltv_parse, NULL, NULL);
            free(outbuf);
          }
        }
      }
      closedir(dir);
      tmp = strtok(NULL, ":");
    }
    free(path);
  }
}

void xmltv_init ( void )
{
  /* External module */
  _xmltv_module = (epggrab_module_t*)
    epggrab_module_ext_create(NULL, "xmltv", "XMLTV", 3, "xmltv",
                              _xmltv_parse, NULL,
                              &_xmltv_channels);

  /* Standard modules */
  _xmltv_load_grabbers();
}

void xmltv_load ( void )
{
  epggrab_module_channels_load(epggrab_module_find_by_id("xmltv"));
}
