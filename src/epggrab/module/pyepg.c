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
  if (!(ch     = epggrab_channel_find(mod, str, 1, &save))) return 0;
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
  if (save)
    stats->channels.modified++;

  return save;
}

static int _pyepg_parse_brand 
  ( epggrab_module_t *mod, htsmsg_t *data, epggrab_stats_t *stats )
{
  int save = 0;
  htsmsg_t *attr, *tags;
  epg_brand_t *brand;
  const char *str;
  lang_str_t *ls;
  uint32_t u32, changes = 0;

  if ( data == NULL ) return 0;

  if ((attr = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str  = htsmsg_get_str(attr, "id")) == NULL) return 0;
  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;
  
  /* Find brand */
  if ((brand = epg_brand_find_by_uri(str, mod, 1, &save, &changes)) == NULL)
    return 0;
  stats->brands.total++;
  if (save) stats->brands.created++;

  /* Set title */
  if ((str = htsmsg_xml_get_cdata_str(tags, "title"))) {
    ls = lang_str_create2(str, NULL);
    save |= epg_brand_set_title(brand, ls, &changes);
    lang_str_destroy(ls);
  }

  /* Set summary */
  if ((str = htsmsg_xml_get_cdata_str(tags, "summary"))) {
    ls = lang_str_create2(str, NULL);
    save |= epg_brand_set_summary(brand, ls, &changes);
    lang_str_destroy(ls);
  }
  
  /* Set image */
  if ((str = htsmsg_xml_get_cdata_str(tags, "image"))) {
    ls = lang_str_create2(str, NULL);
    save |= epg_brand_set_image(brand, str, &changes);
    lang_str_destroy(ls);
  } else if ((str = htsmsg_xml_get_cdata_str(tags, "thumb"))) {
    ls = lang_str_create2(str, NULL);
    save |= epg_brand_set_image(brand, str, &changes);
    lang_str_destroy(ls);
  }

  /* Set season count */
  if (htsmsg_xml_get_cdata_u32(tags, "series-count", &u32) == 0) {
    save |= epg_brand_set_season_count(brand, u32, &changes);
  }

  save |= epg_brand_change_finish(brand, changes, 0);
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
  lang_str_t *ls;
  uint32_t u32, changes = 0;

  if ( data == NULL ) return 0;

  if ((attr = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str  = htsmsg_get_str(attr, "id")) == NULL) return 0;
  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  /* Find series */
  if ((season = epg_season_find_by_uri(str, mod, 1, &save, &changes)) == NULL)
    return 0;
  stats->seasons.total++;
  if (save) stats->seasons.created++;
  
  /* Set brand */
  if ((str = htsmsg_get_str(attr, "brand"))) {
    if ((brand = epg_brand_find_by_uri(str, mod, 0, NULL, NULL))) {
      save |= epg_season_set_brand(season, brand, &changes);
    }
  }

  /* Set summary */
  if ((str = htsmsg_xml_get_cdata_str(tags, "summary"))) {
    ls = lang_str_create2(str, NULL);
    save |= epg_season_set_summary(season, ls, &changes);
    lang_str_destroy(ls);
  }
  
  /* Set image */
  if ((str = htsmsg_xml_get_cdata_str(tags, "image")) ||
      (str = htsmsg_xml_get_cdata_str(tags, "thumb"))) {
    save |= epg_season_set_image(season, str, &changes);
  }

  /* Set season number */
  if (htsmsg_xml_get_cdata_u32(tags, "number", &u32) == 0) {
    save |= epg_season_set_number(season, u32, &changes);
  }

  /* Set episode count */
  if (htsmsg_xml_get_cdata_u32(tags, "episode-count", &u32) == 0) {
    save |= epg_season_set_episode_count(season, u32, &changes);
  }

  save |= epg_season_change_finish(season, changes, 0);
  if (save) stats->seasons.modified++;

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
  lang_str_t *ls;
  uint32_t u32, pc, pn, changes = 0;
  epg_genre_list_t *egl;

  if ( data == NULL ) return 0;

  if ((attr = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str  = htsmsg_get_str(attr, "id")) == NULL) return 0;
  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  /* Find episode */
  if ((episode = epg_episode_find_by_uri(str, mod, 1, &save, &changes)) == NULL) return 0;
  stats->episodes.total++;
  if (save) stats->episodes.created++;
  
  /* Set season */
  if ((str = htsmsg_get_str(attr, "series"))) {
    if ((season = epg_season_find_by_uri(str, mod, 0, NULL, NULL))) {
      save |= epg_episode_set_season(episode, season, &changes);
    }
  }

  /* Set brand */
  if ((str = htsmsg_get_str(attr, "brand"))) {
    if ((brand = epg_brand_find_by_uri(str, mod, 0, NULL, NULL))) {
      save |= epg_episode_set_brand(episode, brand, &changes);
    }
  }

  /* Set title/subtitle */
  if ((str = htsmsg_xml_get_cdata_str(tags, "title"))) {
    ls = lang_str_create2(str, NULL);
    save |= epg_episode_set_title(episode, ls, &changes);
    lang_str_destroy(ls);
  } 
  if ((str = htsmsg_xml_get_cdata_str(tags, "subtitle"))) {
    ls = lang_str_create2(str, NULL);
    save |= epg_episode_set_subtitle(episode, ls, &changes);
    lang_str_destroy(ls);
  } 

  /* Set summary */
  if ((str = htsmsg_xml_get_cdata_str(tags, "summary"))) {
    ls = lang_str_create2(str, NULL);
    save |= epg_episode_set_summary(episode, ls, &changes);
    lang_str_destroy(ls);
  }

  /* Number */
  if (htsmsg_xml_get_cdata_u32(tags, "number", &u32) == 0) {
    save |= epg_episode_set_number(episode, u32, &changes);
  }
  if (!htsmsg_xml_get_cdata_u32(tags, "part-number", &pn)) {
    pc = 0;
    htsmsg_xml_get_cdata_u32(tags, "part-count", &pc);
    save |= epg_episode_set_part(episode, pn, pc, &changes);
  }

  /* Set image */
  if ((str = htsmsg_xml_get_cdata_str(tags, "image")) ||
      (str = htsmsg_xml_get_cdata_str(tags, "thumb"))) {
    save |= epg_episode_set_image(episode, str, &changes);
  }

  /* Genre */
  if ((egl = _pyepg_parse_genre(tags))) {
    save |= epg_episode_set_genre(episode, egl, &changes);
    epg_genre_list_destroy(egl);
  }

  /* Content */
  if ((htsmsg_get_map(tags, "blackandwhite")))
    save |= epg_episode_set_is_bw(episode, 1, &changes);

  save |= epg_episode_change_finish(episode, changes, 0);
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
  uint32_t u32, changes = 0;

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
  broadcast = epg_broadcast_find_by_time(channel, mod, tm_start, tm_stop,
                                         1, &save, &changes);
  if ( broadcast == NULL ) return 0;
  stats->broadcasts.total++;
  if ( save ) stats->broadcasts.created++;

  /* Quality */
  u32 = htsmsg_get_map(tags, "hd") ? 1 : 0;
  save |= epg_broadcast_set_is_hd(broadcast, u32, &changes);
  u32 = htsmsg_get_map(tags, "widescreen") ? 1 : 0;
  save |= epg_broadcast_set_is_widescreen(broadcast, u32, &changes);
  // TODO: lines, aspect

  /* Accessibility */
  // Note: reuse XMLTV parse code as this is the same
  xmltv_parse_accessibility(broadcast, tags, &changes);

  /* New/Repeat */
  u32 = htsmsg_get_map(tags, "new") || htsmsg_get_map(tags, "premiere");
  save |= epg_broadcast_set_is_new(broadcast, u32, &changes);
  u32 = htsmsg_get_map(tags, "repeat") ? 1 : 0;
  save |= epg_broadcast_set_is_repeat(broadcast, u32, &changes);

  /* Set episode */
  if ((episode = epg_episode_find_by_uri(id, mod, 1, &save, &changes)) == NULL) return 0;
  save |= epg_broadcast_set_episode(broadcast, episode, &changes);

  save |= epg_broadcast_change_finish(broadcast, changes, 0);
  if (save) stats->broadcasts.modified++;  

  return save;
}

static int _pyepg_parse_schedule 
  ( epggrab_module_t *mod, htsmsg_t *data, epggrab_stats_t *stats )
{
  int save = 0;
  htsmsg_t *attr, *tags;
  htsmsg_field_t *f;
  channel_t *ch;
  epggrab_channel_t *ec;
  const char *str;
  idnode_list_mapping_t *ilm;

  if ( data == NULL ) return 0;

  if ((attr = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str  = htsmsg_get_str(attr, "channel")) == NULL) return 0;
  if ((ec   = epggrab_channel_find(mod, str, 0, NULL)) == NULL) return 0;
  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  HTSMSG_FOREACH(f, tags) {
    if (strcmp(f->hmf_name, "broadcast") == 0) {
      ec->laststamp = gclk();
      LIST_FOREACH(ilm, &ec->channels, ilm_in1_link) {
        ch = (channel_t *)ilm->ilm_in2;
        if (!ch->ch_enabled || ch->ch_epg_parent) continue;
        save |= _pyepg_parse_broadcast(mod, htsmsg_get_map_by_field(f),
                                       ch, stats);
      }
    }
  }

  return save;
}

static int _pyepg_parse_epg 
  ( epggrab_module_t *mod, htsmsg_t *data, epggrab_stats_t *stats )
{
  int gsave = 0, save;
  htsmsg_t *tags;
  htsmsg_field_t *f;

  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  pthread_mutex_lock(&global_lock);
  epggrab_channel_begin_scan(mod);
  pthread_mutex_unlock(&global_lock);

  HTSMSG_FOREACH(f, tags) {
    save = 0;
    if (strcmp(f->hmf_name, "channel") == 0 ) {
      pthread_mutex_lock(&global_lock);
      save = _pyepg_parse_channel(mod, htsmsg_get_map_by_field(f), stats);
      pthread_mutex_unlock(&global_lock);
    } else if (strcmp(f->hmf_name, "brand") == 0 ) {
      pthread_mutex_lock(&global_lock);
      save = _pyepg_parse_brand(mod, htsmsg_get_map_by_field(f), stats);
      if (save) epg_updated();
      pthread_mutex_unlock(&global_lock);
    } else if (strcmp(f->hmf_name, "series") == 0 ) {
      pthread_mutex_lock(&global_lock);
      save = _pyepg_parse_season(mod, htsmsg_get_map_by_field(f), stats);
      if (save) epg_updated();
      pthread_mutex_unlock(&global_lock);
    } else if (strcmp(f->hmf_name, "episode") == 0 ) {
      pthread_mutex_lock(&global_lock);
      save = _pyepg_parse_episode(mod, htsmsg_get_map_by_field(f), stats);
      if (save) epg_updated();
      pthread_mutex_unlock(&global_lock);
    } else if (strcmp(f->hmf_name, "schedule") == 0 ) {
      pthread_mutex_lock(&global_lock);
      save = _pyepg_parse_schedule(mod, htsmsg_get_map_by_field(f), stats);
      if (save) epg_updated();
      pthread_mutex_unlock(&global_lock);
    }
    gsave |= save;
  }

  pthread_mutex_lock(&global_lock);
  epggrab_channel_end_scan(mod);
  pthread_mutex_unlock(&global_lock);

  return gsave;
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

const idclass_t epggrab_mod_int_pyepg_class = {
  .ic_super      = &epggrab_mod_int_class,
  .ic_class      = "epggrab_mod_int_pyepg",
  .ic_caption    = N_("Internal PyEPG Grabber"),
};


const idclass_t epggrab_mod_ext_pyepg_class = {
  .ic_super      = &epggrab_mod_ext_class,
  .ic_class      = "epggrab_mod_ext_pyepg",
  .ic_caption    = N_("External PyEPG Grabber"),
};


void pyepg_init ( void )
{
  char buf[256];

  /* Internal module */
  if (find_exec("pyepg", buf, sizeof(buf)-1))
    epggrab_module_int_create(NULL, &epggrab_mod_int_pyepg_class,
                              "pyepg-internal", "pyepg", "PyEPG", 4, buf,
                              NULL, _pyepg_parse, NULL);

  /* External module */
  epggrab_module_ext_create(NULL, &epggrab_mod_ext_pyepg_class,
                            "pyepg", "pyepg", "PyEPG", 4, "pyepg",
                            _pyepg_parse, NULL);
}

void pyepg_done ( void )
{
}

void pyepg_load ( void )
{
  epggrab_module_channels_load("pyepg");
}
