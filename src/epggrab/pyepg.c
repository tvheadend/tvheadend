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

static epggrab_channel_tree_t _pyepg_channels;
static epggrab_module_t       *_pyepg_module;

static epggrab_channel_t *_pyepg_channel_find
  ( const char *id, int create, int *save )
{
  return epggrab_module_channel_find(_pyepg_module, id, create, save);
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

static const uint8_t *_pyepg_parse_genre ( htsmsg_t *tags, int *cnt )
{
  // TODO: implement this
  return NULL;
}

static int _pyepg_parse_channel ( htsmsg_t *data, epggrab_stats_t *stats )
{
  int save = 0;
  epggrab_channel_t *ch;
  htsmsg_t *attr, *tags, *e;
  htsmsg_field_t *f;
  const char *str;
  uint32_t u32;
  const char *sname[11];
  uint16_t sid[10];
  int sid_idx = 0, sname_idx = 0;
  
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
    save |= epggrab_channel_set_number(ch, u32);
  
  HTSMSG_FOREACH(f, tags) {
    if (!strcmp(f->hmf_name, "sid")) {
      if (sid_idx < 10) {
        e = htsmsg_get_map_by_field(f);
        if (!htsmsg_get_u32(e, "cdata", &u32)) {
          sid[sid_idx] = (uint16_t)u32;
          sid_idx++;
        }
      }
    } else if (!strcmp(f->hmf_name, "sname")) {
      if (sname_idx < 10) {
        e = htsmsg_get_map_by_field(f);
        if ((str = htsmsg_get_str(e, "cdata"))) {
          sname[sname_idx] = str;
          sname_idx++;
        }
      }
    }
  }
  if (sid_idx)   save |= epggrab_channel_set_sid(ch, sid, sid_idx);
  if (sname_idx) {
    sname[sname_idx] = NULL;
    save |= epggrab_channel_set_sname(ch, sname);
  }

  /* Update */
  if (save) {
    epggrab_channel_updated(ch);
    stats->channels.modified++;
  }

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
  
  /* Set image */
  if ((str = htsmsg_xml_get_cdata_str(tags, "image"))) {
    save |= epg_brand_set_image(brand, str);
  } else if ((str = htsmsg_xml_get_cdata_str(tags, "thumb"))) {
    save |= epg_brand_set_image(brand, str);
  }

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

  /* Set summary */
  if ((str = htsmsg_xml_get_cdata_str(tags, "summary"))) {
    save |= epg_season_set_summary(season, str);
  }
  
  /* Set image */
  if ((str = htsmsg_xml_get_cdata_str(tags, "image"))) {
    save |= epg_season_set_image(season, str);
  } else if ((str = htsmsg_xml_get_cdata_str(tags, "thumb"))) {
    save |= epg_season_set_image(season, str);
  }

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
  int genre_cnt, save = 0;
  htsmsg_t *attr, *tags;
  epg_episode_t *episode;
  epg_season_t *season;
  epg_brand_t *brand;
  const char *str;
  uint32_t u32, pc, pn;
  const uint8_t *genre;

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
      save |= epg_episode_set_season(episode, season);
    }
  }

  /* Set brand */
  if ((str = htsmsg_get_str(attr, "brand"))) {
    if ((brand = epg_brand_find_by_uri(str, 0, NULL))) {
      save |= epg_episode_set_brand(episode, brand);
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
  if (!htsmsg_xml_get_cdata_u32(tags, "part-number", &pn)) {
    pc = 0;
    htsmsg_xml_get_cdata_u32(tags, "part-count", &pc);
    save |= epg_episode_set_part(episode, pn, pc);
  }

  /* Set image */
  if ((str = htsmsg_xml_get_cdata_str(tags, "image"))) {
    save |= epg_episode_set_image(episode, str);
  } else if ((str = htsmsg_xml_get_cdata_str(tags, "thumb"))) {
    save |= epg_episode_set_image(episode, str);
  }

  /* Genre */
  if ((genre = _pyepg_parse_genre(tags, &genre_cnt))) {
    save |= epg_episode_set_genre(episode, genre, genre_cnt);
  }

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
  epggrab_channel_t *ec;
  const char *str;

  if ( data == NULL ) return 0;

  if ((attr = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str  = htsmsg_get_str(attr, "channel")) == NULL) return 0;
  if ((ec   = _pyepg_channel_find(str, 0, NULL)) == NULL) return 0;
  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;
  if (!ec->channel) return 0;

  HTSMSG_FOREACH(f, tags) {
    if (strcmp(f->hmf_name, "broadcast") == 0) {
      save |= _pyepg_parse_broadcast(htsmsg_get_map_by_field(f),
                                     ec->channel, stats);
    }
  }

  return save;
}

static int _pyepg_parse_epg ( htsmsg_t *data, epggrab_stats_t *stats )
{
  int save = 0;
  htsmsg_t *tags;
  htsmsg_field_t *f;

  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  HTSMSG_FOREACH(f, tags) {
    if (strcmp(f->hmf_name, "channel") == 0 ) {
      save |= _pyepg_parse_channel(htsmsg_get_map_by_field(f), stats);
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

  return save;
}


/* ************************************************************************
 * Module Setup
 * ***********************************************************************/


static int _pyepg_parse 
  ( epggrab_module_t *mod, htsmsg_t *data, epggrab_stats_t *stats )
{
  htsmsg_t *tags, *epg;

  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  /* PyEPG format */
  if ((epg = htsmsg_get_map(tags, "epg")) != NULL)
    return _pyepg_parse_epg(epg, stats);

  return 0;
}

void pyepg_init ( epggrab_module_list_t *list )
{
  epggrab_module_t *mod;

  /* Standard module */
  mod                      = calloc(1, sizeof(epggrab_module_t));
  mod->id                  = strdup("pyepg");
  mod->path                = strdup("/usr/bin/pyepg");
  mod->name                = strdup("PyEPG");
  mod->grab                = epggrab_module_grab;
  mod->trans               = epggrab_module_trans_xml;
  mod->parse               = _pyepg_parse;
  mod->channels            = &_pyepg_channels;
  mod->ch_add              = epggrab_module_channel_add;
  mod->ch_rem              = epggrab_module_channel_rem;
  mod->ch_mod              = epggrab_module_channel_mod;
  *((uint8_t*)&mod->flags) = EPGGRAB_MODULE_INTERNAL;
  LIST_INSERT_HEAD(list, mod, link);
  _pyepg_module = mod;

  /* External module */
  mod                      = calloc(1, sizeof(epggrab_module_t));
  mod->id                  = strdup("pyepg_ext");
  mod->path                = epggrab_module_socket_path("pyepg");
  mod->name                = strdup("PyEPG");
  mod->channels            = &_pyepg_channels;
  mod->enable              = epggrab_module_enable_socket;
  mod->trans               = epggrab_module_trans_xml;
  mod->parse               = _pyepg_parse;
  *((uint8_t*)&mod->flags) = EPGGRAB_MODULE_EXTERNAL;
  LIST_INSERT_HEAD(list, mod, link);
}

void pyepg_load ( void )
{
  epggrab_module_channels_load(_pyepg_module);
}
