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
#include "channels.h"
#include "epg.h"
#include "epggrab.h"
#include "epggrab/private.h"

static epggrab_channel_tree_t _pyepg_channels;
static epggrab_module_t      *_pyepg_module;   // primary module

static epggrab_channel_t *_pyepg_channel_find
  ( const char *id, int create, int *save )
{
  return epggrab_channel_find(&_pyepg_channels, id, create, save,
                              _pyepg_module);
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

static epg_genre_list_t
*_pyepg_parse_genre ( htsmsg_t *tags )
{
  htsmsg_t *e;
  htsmsg_field_t *f;
  epg_genre_list_t *egl = NULL;
  HTSMSG_FOREACH(f, tags) {
    if (!strcmp(f->hmf_name, "genre") && (e = htsmsg_get_map_by_field(f))) {
      if (!egl) { egl = calloc(1, sizeof(epg_genre_list_t)); printf("alloc %p\n", egl); }
      epg_genre_list_add_by_str(egl, htsmsg_get_str(e, "cdata"), NULL);
    }
  }
  return egl;
}

static int _pyepg_parse_channel 
  ( epggrab_module_t *mod, htsmsg_t *data, epggrab_stats_t *stats )
{
  int save = 0;
  epggrab_channel_t *ch;
  htsmsg_t *attr, *tags;
  const char *str;
  uint32_t u32;
  
  if ( data == NULL ) return 0;

  if ((attr    = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str     = htsmsg_get_str(attr, "id")) == NULL) return 0;
  if ((tags    = htsmsg_get_map(data, "tags")) == NULL) return 0;
  if (!(ch     = _pyepg_channel_find(str, 1, &save))) return 0;
  stats->channels.total++;
  if (save) stats->channels.created++;

  /* Update data */
  if ((str = htsmsg_xml_get_cdata_str(tags, "name")))
    save |= epggrab_channel_set_name(ch, str);
  if ((str = htsmsg_xml_get_cdata_str(tags, "image")))
    save |= epggrab_channel_set_icon(ch, str);
  if ((!htsmsg_xml_get_cdata_u32(tags, "number", &u32)))
    save |= epggrab_channel_set_number(ch, u32, 0);
  
  /* Update */
  if (save) {
    epggrab_channel_updated(ch);
    stats->channels.modified++;
  }

  return save;
}

static int _pyepg_parse_brand 
  ( epggrab_module_t *mod, htsmsg_t *data, epggrab_stats_t *stats )
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
    save |= epg_brand_set_title(brand, str, NULL, mod);
  }

  /* Set summary */
  if ((str = htsmsg_xml_get_cdata_str(tags, "summary"))) {
    save |= epg_brand_set_summary(brand, str, NULL, mod);
  }
  
  /* Set image */
  if ((str = htsmsg_xml_get_cdata_str(tags, "image"))) {
    save |= epg_brand_set_image(brand, str, mod);
  } else if ((str = htsmsg_xml_get_cdata_str(tags, "thumb"))) {
    save |= epg_brand_set_image(brand, str, mod);
  }

  /* Set season count */
  if (htsmsg_xml_get_cdata_u32(tags, "series-count", &u32) == 0) {
    save |= epg_brand_set_season_count(brand, u32, mod);
  }

  if (save) stats->brands.modified++;

  return save;
}

static int _pyepg_parse_season 
  ( epggrab_module_t *mod, htsmsg_t *data, epggrab_stats_t *stats )
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
      save |= epg_season_set_brand(season, brand, mod);
    }
  }

  /* Set summary */
  if ((str = htsmsg_xml_get_cdata_str(tags, "summary"))) {
    save |= epg_season_set_summary(season, str, NULL, mod);
  }
  
  /* Set image */
  if ((str = htsmsg_xml_get_cdata_str(tags, "image"))) {
    save |= epg_season_set_image(season, str, mod);
  } else if ((str = htsmsg_xml_get_cdata_str(tags, "thumb"))) {
    save |= epg_season_set_image(season, str, mod);
  }

  /* Set season number */
  if (htsmsg_xml_get_cdata_u32(tags, "number", &u32) == 0) {
    save |= epg_season_set_number(season, u32, mod);
  }

  /* Set episode count */
  if (htsmsg_xml_get_cdata_u32(tags, "episode-count", &u32) == 0) {
    save |= epg_season_set_episode_count(season, u32, mod);
  }

  if(save) stats->seasons.modified++;

  return save;
}

static int _pyepg_parse_episode 
  ( epggrab_module_t *mod, htsmsg_t *data, epggrab_stats_t *stats )
{
  int save = 0;
  htsmsg_t *attr, *tags;
  epg_episode_t *episode;
  epg_season_t *season;
  epg_brand_t *brand;
  const char *str;
  uint32_t u32, pc, pn;
  epg_genre_list_t *egl;

  if ( data == NULL ) return 0;

  if ((attr = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str  = htsmsg_get_str(attr, "id")) == NULL) return 0;
  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  /* Find episode */
  if ((episode = epg_episode_find_by_uri(str, 1, &save)) == NULL) return 0;
  stats->episodes.total++;
  if (save) stats->episodes.created++;
  
  /* Set season */
  if ((str = htsmsg_get_str(attr, "series"))) {
    if ((season = epg_season_find_by_uri(str, 0, NULL))) {
      save |= epg_episode_set_season(episode, season, mod);
    }
  }

  /* Set brand */
  if ((str = htsmsg_get_str(attr, "brand"))) {
    if ((brand = epg_brand_find_by_uri(str, 0, NULL))) {
      save |= epg_episode_set_brand(episode, brand, mod);
    }
  }

  /* Set title/subtitle */
  if ((str = htsmsg_xml_get_cdata_str(tags, "title"))) {
    save |= epg_episode_set_title(episode, str, NULL, mod);
  } 
  if ((str = htsmsg_xml_get_cdata_str(tags, "subtitle"))) {
    save |= epg_episode_set_subtitle(episode, str, NULL, mod);
  } 

  /* Set summary */
  if ((str = htsmsg_xml_get_cdata_str(tags, "summary"))) {
    save |= epg_episode_set_summary(episode, str, NULL, mod);
  }

  /* Number */
  if (htsmsg_xml_get_cdata_u32(tags, "number", &u32) == 0) {
    save |= epg_episode_set_number(episode, u32, mod);
  }
  if (!htsmsg_xml_get_cdata_u32(tags, "part-number", &pn)) {
    pc = 0;
    htsmsg_xml_get_cdata_u32(tags, "part-count", &pc);
    save |= epg_episode_set_part(episode, pn, pc, mod);
  }

  /* Set image */
  if ((str = htsmsg_xml_get_cdata_str(tags, "image"))) {
    save |= epg_episode_set_image(episode, str, mod);
  } else if ((str = htsmsg_xml_get_cdata_str(tags, "thumb"))) {
    save |= epg_episode_set_image(episode, str, mod);
  }

  /* Genre */
  if ((egl = _pyepg_parse_genre(tags))) {
    save |= epg_episode_set_genre(episode, egl, mod);
    epg_genre_list_destroy(egl);
  }

  /* Content */
  if ((htsmsg_get_map(tags, "blackandwhite")))
    save |= epg_episode_set_is_bw(episode, 1, mod);

  if (save) stats->episodes.modified++;

  return save;
}

static int _pyepg_parse_broadcast 
  ( epggrab_module_t *mod, htsmsg_t *data, channel_t *channel,
    epggrab_stats_t *stats )
{
  int save = 0;
  htsmsg_t *attr, *tags;
  epg_episode_t *episode;
  epg_broadcast_t *broadcast;
  const char *id, *start, *stop;
  time_t tm_start, tm_stop;
  uint32_t u32;

  if ( data == NULL || channel == NULL ) return 0;

  if ((attr    = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((id      = htsmsg_get_str(attr, "episode")) == NULL) return 0;
  if ((start   = htsmsg_get_str(attr, "start")) == NULL ) return 0;
  if ((stop    = htsmsg_get_str(attr, "stop")) == NULL ) return 0;
  if ((tags    = htsmsg_get_map(data, "tags")) == NULL) return 0;

  /* Parse times */
  if (!_pyepg_parse_time(start, &tm_start)) return 0;
  if (!_pyepg_parse_time(stop, &tm_stop)) return 0;

  /* Find broadcast */
  broadcast 
    = epg_broadcast_find_by_time(channel, tm_start, tm_stop, 0, 1, &save);
  if ( broadcast == NULL ) return 0;
  stats->broadcasts.total++;
  if ( save ) stats->broadcasts.created++;

  /* Quality */
  u32 = htsmsg_get_map(tags, "hd") ? 1 : 0;
  save |= epg_broadcast_set_is_hd(broadcast, u32, mod);
  u32 = htsmsg_get_map(tags, "widescreen") ? 1 : 0;
  save |= epg_broadcast_set_is_widescreen(broadcast, u32, mod);
  // TODO: lines, aspect

  /* Accessibility */
  // Note: reuse XMLTV parse code as this is the same
  xmltv_parse_accessibility(mod, broadcast, tags);

  /* New/Repeat */
  u32 = htsmsg_get_map(tags, "new") || htsmsg_get_map(tags, "premiere");
  save |= epg_broadcast_set_is_new(broadcast, u32, mod);
  u32 = htsmsg_get_map(tags, "repeat") ? 1 : 0;
  save |= epg_broadcast_set_is_repeat(broadcast, u32, mod);

  /* Set episode */
  if ((episode = epg_episode_find_by_uri(id, 1, &save)) == NULL) return 0;
  save |= epg_broadcast_set_episode(broadcast, episode, mod);

  if (save) stats->broadcasts.modified++;  

  return save;
}

static int _pyepg_parse_schedule 
  ( epggrab_module_t *mod, htsmsg_t *data, epggrab_stats_t *stats )
{
  int save = 0;
  htsmsg_t *attr, *tags;
  htsmsg_field_t *f;
  epggrab_channel_t *ec;
  const char *str;
  epggrab_channel_link_t *ecl;

  if ( data == NULL ) return 0;

  if ((attr = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str  = htsmsg_get_str(attr, "channel")) == NULL) return 0;
  if ((ec   = _pyepg_channel_find(str, 0, NULL)) == NULL) return 0;
  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  HTSMSG_FOREACH(f, tags) {
    if (strcmp(f->hmf_name, "broadcast") == 0) {
      LIST_FOREACH(ecl, &ec->channels, ecl_epg_link)
        save |= _pyepg_parse_broadcast(mod, htsmsg_get_map_by_field(f),
                                       ecl->ecl_channel, stats);
    }
  }

  return save;
}

static int _pyepg_parse_epg 
  ( epggrab_module_t *mod, htsmsg_t *data, epggrab_stats_t *stats )
{
  int save = 0;
  htsmsg_t *tags;
  htsmsg_field_t *f;

  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  HTSMSG_FOREACH(f, tags) {
    if (strcmp(f->hmf_name, "channel") == 0 ) {
      save |= _pyepg_parse_channel(mod, htsmsg_get_map_by_field(f), stats);
    } else if (strcmp(f->hmf_name, "brand") == 0 ) {
      save |= _pyepg_parse_brand(mod, htsmsg_get_map_by_field(f), stats);
    } else if (strcmp(f->hmf_name, "series") == 0 ) {
      save |= _pyepg_parse_season(mod, htsmsg_get_map_by_field(f), stats);
    } else if (strcmp(f->hmf_name, "episode") == 0 ) {
      save |= _pyepg_parse_episode(mod, htsmsg_get_map_by_field(f), stats);
    } else if (strcmp(f->hmf_name, "schedule") == 0 ) {
      save |= _pyepg_parse_schedule(mod, htsmsg_get_map_by_field(f), stats);
    }
  }

  return save;
}

static int _pyepg_parse 
  ( void *mod, htsmsg_t *data, epggrab_stats_t *stats )
{
  htsmsg_t *tags, *epg;

  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  /* PyEPG format */
  if ((epg = htsmsg_get_map(tags, "epg")) != NULL)
    return _pyepg_parse_epg(mod, epg, stats);

  return 0;
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

void pyepg_init ( void )
{
  char buf[256];

  RB_INIT(&_pyepg_channels);

  /* Internal module */
  if (find_exec("pyepg", buf, sizeof(buf)-1))
    epggrab_module_int_create(NULL, NULL,
                              "pyepg-internal", "PyEPG", 4, buf,
                              NULL, _pyepg_parse, NULL, NULL);

  /* External module */
  _pyepg_module = (epggrab_module_t*)
    epggrab_module_ext_create(NULL, "pyepg", "PyEPG", 4, "pyepg",
                              _pyepg_parse, NULL,
                              &_pyepg_channels);
}

void pyepg_done ( void )
{
  epggrab_channel_flush(&_pyepg_channels, 0);
}

void pyepg_load ( void )
{
  epggrab_module_channels_load(epggrab_module_find_by_id("pyepg"));
}
