/*
 *  Electronic Program Guide - pyepg grabber
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

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "htsmsg_xml.h"
#include "tvheadend.h"
#include "spawn.h"
#include "epg.h"
#include "epggrab/pyepg.h"
#include "channels.h"

epggrab_channel_tree_t _pyepg_channels;
epggrab_module_t       _pyepg_sync;
epggrab_module_t       _pyepg_async;

static channel_t *_pyepg_channel_create ( epggrab_channel_t *skel, int *save )
{
  return NULL;
}

static channel_t *_pyepg_channel_find ( const char *id )
{
  return NULL;
}

/* **************************************************************************
 * Parsing
 * *************************************************************************/

static int _pyepg_parse_time ( const char *str, time_t *out )
{
  int ret = 0;
  struct tm tm; 
  tm.tm_isdst = 0;
  if ( strptime(str, "%F %T %z", &tm) != NULL ) {
    *out = mktime(&tm);
    ret  = 1;
  }
  return ret;
}

static int _pyepg_parse_channel ( htsmsg_t *data, epggrab_stats_t *stats )
{
  int save = 0, save2 = 0;
  htsmsg_t *attr, *tags;
  const char *icon;
  channel_t *ch;
  epggrab_channel_t skel;

  if ( data == NULL ) return 0;

  if ((attr    = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((skel.id = (char*)htsmsg_get_str(attr, "id")) == NULL) return 0;
  if ((tags    = htsmsg_get_map(data, "tags")) == NULL) return 0;
  skel.name = (char*)htsmsg_xml_get_cdata_str(tags, "name");
  icon      = htsmsg_xml_get_cdata_str(tags, "image");

  ch = _pyepg_channel_create(&skel, &save2);
  stats->channels.total++;
  if (save2) {
    stats->channels.created++;
    save |= 1;
  }

  /* Update values */
  if (ch) {
    if (skel.name) {
      if(!ch->ch_name || strcmp(ch->ch_name, skel.name)) {
        save |= channel_rename(ch, skel.name);
      }
    }
    if (icon)      channel_set_icon(ch, icon);
  }
  if (save) stats->channels.modified++;

  return save;
}

static int _pyepg_parse_brand ( htsmsg_t *data, epggrab_stats_t *stats )
{
  int save = 0;
  htsmsg_t *attr, *tags;
  epg_brand_t *brand;
  const char *str;
  uint32_t u32;

  if ( data == NULL ) return 0;

  if ((attr = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str  = htsmsg_get_str(attr, "id")) == NULL) return 0;
  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;
  
  /* Find brand */
  if ((brand = epg_brand_find_by_uri(str, 1, &save)) == NULL) return 0;
  stats->brands.total++;
  if (save) stats->brands.created++;

  /* Set title */
  if ((str = htsmsg_xml_get_cdata_str(tags, "title"))) {
    save |= epg_brand_set_title(brand, str);
  }

  /* Set summary */
  if ((str = htsmsg_xml_get_cdata_str(tags, "summary"))) {
    save |= epg_brand_set_summary(brand, str);
  }
  
  /* Set icon */
#if TODO
  if ((str = htsmsg_xml_get_cdata_str(tags, "icon"))) {
    save |= epg_brand_set_icon(brand, str);
  }
#endif

  /* Set season count */
  if (htsmsg_xml_get_cdata_u32(tags, "series-count", &u32) == 0) {
    save |= epg_brand_set_season_count(brand, u32);
  }

  if (save) stats->brands.modified++;

  return save;
}

static int _pyepg_parse_season ( htsmsg_t *data, epggrab_stats_t *stats )
{
  int save = 0;
  htsmsg_t *attr, *tags;
  epg_season_t *season;
  epg_brand_t *brand;
  const char *str;
  uint32_t u32;

  if ( data == NULL ) return 0;

  if ((attr = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str  = htsmsg_get_str(attr, "id")) == NULL) return 0;
  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  /* Find series */
  if ((season = epg_season_find_by_uri(str, 1, &save)) == NULL) return 0;
  stats->seasons.total++;
  if (save) stats->seasons.created++;
  
  /* Set brand */
  if ((str = htsmsg_get_str(attr, "brand"))) {
    if ((brand = epg_brand_find_by_uri(str, 0, NULL))) {
      save |= epg_season_set_brand(season, brand, 1);
    }
  }

  /* Set title */
#if TODO
  if ((str = htsmsg_xml_get_cdata_str(tags, "title"))) {
    save |= epg_season_set_title(season, str);
  } 

  /* Set summary */
  if ((str = htsmsg_xml_get_cdata_str(tags, "summary"))) {
    save |= epg_season_set_summary(season, str);
  }
  
  /* Set icon */
  if ((str = htsmsg_xml_get_cdata_str(tags, "icon"))) {
    save |= epg_season_set_icon(season, str);
  }
#endif

  /* Set season number */
  if (htsmsg_xml_get_cdata_u32(tags, "number", &u32) == 0) {
    save |= epg_season_set_number(season, u32);
  }

  /* Set episode count */
  if (htsmsg_xml_get_cdata_u32(tags, "episode-count", &u32) == 0) {
    save |= epg_season_set_episode_count(season, u32);
  }

  if(save) stats->seasons.modified++;

  return save;
}

static int _pyepg_parse_episode ( htsmsg_t *data, epggrab_stats_t *stats )
{
  int save = 0;
  htsmsg_t *attr, *tags;
  epg_episode_t *episode;
  epg_season_t *season;
  epg_brand_t *brand;
  const char *str;
  uint32_t u32;

  if ( data == NULL ) return 0;

  if ((attr = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str  = htsmsg_get_str(attr, "id")) == NULL) return 0;
  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  /* Find episode */
  if ((episode = epg_episode_find_by_uri(str, 1, &save)) == NULL) return 0;
  stats->episodes.total++;
  if (save) stats->episodes.created++;
  
  /* Set brand */
  if ((str = htsmsg_get_str(attr, "brand"))) {
    if ((brand = epg_brand_find_by_uri(str, 0, NULL))) {
      save |= epg_episode_set_brand(episode, brand);
    }
  }

  /* Set season */
  if ((str = htsmsg_get_str(attr, "series"))) {
    if ((season = epg_season_find_by_uri(str, 0, NULL))) {
      save |= epg_episode_set_season(episode, season);
    }
  }

  /* Set title/subtitle */
  if ((str = htsmsg_xml_get_cdata_str(tags, "title"))) {
    save |= epg_episode_set_title(episode, str);
  } 
  if ((str = htsmsg_xml_get_cdata_str(tags, "subtitle"))) {
    save |= epg_episode_set_subtitle(episode, str);
  } 

  /* Set summary */
  if ((str = htsmsg_xml_get_cdata_str(tags, "summary"))) {
    save |= epg_episode_set_summary(episode, str);
  }

  /* Number */
  if (htsmsg_xml_get_cdata_u32(tags, "number", &u32) == 0) {
    save |= epg_episode_set_number(episode, u32);
  }

  /* Genre */
  // TODO: can actually have multiple!
#if TODO
  if ((str = htsmsg_xml_get_cdata_str(tags, "genre"))) {
    // TODO: conversion?
    save |= epg_episode_set_genre(episode, str);
  }
#endif

  /* TODO: extra metadata */

  if (save) stats->episodes.modified++;

  return save;
}

static int _pyepg_parse_broadcast 
  ( htsmsg_t *data, channel_t *channel, epggrab_stats_t *stats )
{
  int save = 0;
  htsmsg_t *attr;//, *tags;
  epg_episode_t *episode;
  epg_broadcast_t *broadcast;
  const char *id, *start, *stop;
  time_t tm_start, tm_stop;

  if ( data == NULL || channel == NULL ) return 0;

  if ((attr    = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((id      = htsmsg_get_str(attr, "episode")) == NULL) return 0;
  if ((start   = htsmsg_get_str(attr, "start")) == NULL ) return 0;
  if ((stop    = htsmsg_get_str(attr, "stop")) == NULL ) return 0;

  /* Find episode */
  if ((episode = epg_episode_find_by_uri(id, 1, &save)) == NULL) return 0;

  /* Parse times */
  if (!_pyepg_parse_time(start, &tm_start)) return 0;
  if (!_pyepg_parse_time(stop, &tm_stop)) return 0;

  /* Find broadcast */
  broadcast = epg_broadcast_find_by_time(channel, tm_start, tm_stop, 1, &save);
  if ( broadcast == NULL ) return 0;
  stats->broadcasts.total++;
  if ( save ) stats->broadcasts.created++;

  /* Set episode */
  save |= epg_broadcast_set_episode(broadcast, episode);

  /* TODO: extra metadata */
  
  if (save) stats->broadcasts.modified++;  

  return save;
}

static int _pyepg_parse_schedule ( htsmsg_t *data, epggrab_stats_t *stats )
{
  int save = 0;
  htsmsg_t *attr, *tags;
  htsmsg_field_t *f;
  channel_t *channel;
  const char *str;

  if ( data == NULL ) return 0;

  if ((attr    = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str     = htsmsg_get_str(attr, "channel")) == NULL) return 0;
  if ((channel = _pyepg_channel_find(str)) == NULL) return 0;
  if ((tags    = htsmsg_get_map(data, "tags")) == NULL) return 0;
  stats->channels.total++;

  HTSMSG_FOREACH(f, tags) {
    if (strcmp(f->hmf_name, "broadcast") == 0) {
      save |= _pyepg_parse_broadcast(htsmsg_get_map_by_field(f),
                                     channel, stats);
    }
  }

  return save;
}

static int _pyepg_parse_epg ( htsmsg_t *data, epggrab_stats_t *stats )
{
  int save = 0, chsave = 0;
  htsmsg_t *tags;
  htsmsg_field_t *f;

  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  HTSMSG_FOREACH(f, tags) {
    if (strcmp(f->hmf_name, "channel") == 0 ) {
      chsave |= _pyepg_parse_channel(htsmsg_get_map_by_field(f), stats);
    } else if (strcmp(f->hmf_name, "brand") == 0 ) {
      save |= _pyepg_parse_brand(htsmsg_get_map_by_field(f), stats);
    } else if (strcmp(f->hmf_name, "series") == 0 ) {
      save |= _pyepg_parse_season(htsmsg_get_map_by_field(f), stats);
    } else if (strcmp(f->hmf_name, "episode") == 0 ) {
      save |= _pyepg_parse_episode(htsmsg_get_map_by_field(f), stats);
    } else if (strcmp(f->hmf_name, "schedule") == 0 ) {
      save |= _pyepg_parse_schedule(htsmsg_get_map_by_field(f), stats);
    }
  }
#if 0
  save |= chsave;
  if (chsave) pyepg_save();
#endif

  return save;
}

static int _pyepg_parse 
  ( epggrab_module_t *mod, htsmsg_t *data, epggrab_stats_t *stats )
{
  htsmsg_t *tags, *epg;

  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  /* PyEPG format */
  if ((epg = htsmsg_get_map(tags, "epg")) != NULL)
    return _pyepg_parse_epg(epg, stats);

  /* XMLTV format */
  if ((epg = htsmsg_get_map(tags, "tv")) != NULL) {
    mod = epggrab_module_find_by_id("xmltv_sync");
    if (mod) return mod->parse(mod, epg, stats);
  }
    
  return 0;
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static void _pyepg_enable ( epggrab_module_t *mod, uint8_t e )
{
}


static htsmsg_t* _pyepg_grab ( epggrab_module_t *mod, const char *icmd, const char *iopts )
{
  int        i, outlen;
  char       *outbuf;
  char       errbuf[100];
  const char *argv[32]; // 32 args max!
  char       *toksave, *tok;
  char       *opts = NULL;
  htsmsg_t   *ret;

  /* TODO: do something (much) better! */
  if (iopts) opts = strdup(iopts);
  i = 1;
  argv[0] = "/usr/bin/pyepg";
  if ( opts ) {
    tok = strtok_r(opts, " ", &toksave);
    while ( tok != NULL ) {
      argv[i++] = tok;
      tok = strtok_r(NULL, " ", &toksave);
    }
  }
  argv[i] = NULL;

  /* Debug */
  tvhlog(LOG_DEBUG, "pyepg", "grab %s %s", argv[0], iopts ? iopts : ""); 

  /* Grab */
#if 0
  outlen = spawn_and_store_stdout(argv[0], (char *const*)argv, &outbuf);
#else
  outlen = spawn_and_store_stdout("/home/aps/tmp/epg.sh", NULL, &outbuf);
#endif
  free(opts);
  if ( outlen < 1 ) {
    tvhlog(LOG_ERR, "pyepg", "no output detected");
    return NULL;
  }

  /* Extract */
  ret = htsmsg_xml_deserialize(outbuf, errbuf, sizeof(errbuf));
  if (!ret)
    tvhlog(LOG_ERR, "pyepg", "htsmsg_xml_deserialize error %s", errbuf);
  return ret;
}

void pyepg_init ( epggrab_module_list_t *list )
{
  /* Common routines */
  _pyepg_sync.channels = _pyepg_async.channels = &_pyepg_channels;
  _pyepg_sync.ch_save  = _pyepg_async.ch_save  = epggrab_module_channels_save;
  _pyepg_sync.ch_add   = _pyepg_async.ch_add   = epggrab_module_channel_add;
  _pyepg_sync.ch_rem   = _pyepg_async.ch_rem   = epggrab_module_channel_rem;
  _pyepg_sync.ch_mod   = _pyepg_async.ch_mod   = epggrab_module_channel_mod;
  _pyepg_sync.name     = _pyepg_async.name     = strdup("PyEPG");

  /* Sync */
  _pyepg_sync.id       = strdup("pyepg_sync");
  _pyepg_sync.path     = strdup("/usr/bin/pyepg");
  _pyepg_sync.grab     = _pyepg_grab;
  _pyepg_sync.parse    = _pyepg_parse;

  /* Async */
  _pyepg_async.id      = strdup("pyepg_async");
  _pyepg_async.enable  = _pyepg_enable;
  *((uint8_t*)&_pyepg_async.async) = 1;

  /* Add to list */
  LIST_INSERT_HEAD(list, &_pyepg_sync, link);
  LIST_INSERT_HEAD(list, &_pyepg_async, link);
}

