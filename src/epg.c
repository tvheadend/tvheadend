/*
 *  Electronic Program Guide - Common functions
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

#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <assert.h>

#include "tvheadend.h"
#include "queue.h"
#include "channels.h"
#include "settings.h"
#include "epg.h"
#include "dvr/dvr.h"
#include "htsp.h"
#include "htsmsg_binary.h"
#include "epggrab.h"

/* Broadcast hashing */
#define EPG_HASH_WIDTH 1024
#define EPG_HASH_MASK  (EPG_HASH_WIDTH - 1)

/* Element lists */
epg_object_list_t epg_objects[EPG_HASH_WIDTH];
epg_object_tree_t epg_brands;
epg_object_tree_t epg_seasons;
epg_object_tree_t epg_episodes;

/* Unreferenced */
epg_object_list_t epg_object_unref;

/* Global counter */
static uint64_t _epg_object_idx    = 0;

/* **************************************************************************
 * Comparators
 * *************************************************************************/

static int _uri_cmp ( const void *a, const void *b )
{
  return strcmp(((epg_object_t*)a)->uri, ((epg_object_t*)b)->uri);
}

static int _id_cmp ( const void *a, const void *b )
{
  return ((epg_object_t*)a)->id - ((epg_object_t*)b)->id;
}

static int _ebc_start_cmp ( const void *a, const void *b )
{
  return ((epg_broadcast_t*)a)->start - ((epg_broadcast_t*)b)->start;
}

/* **************************************************************************
 * Setup / Update
 * *************************************************************************/

static int _epg_write ( int fd, htsmsg_t *m )
{
  int ret = 1;
  size_t msglen;
  void *msgdata;
  if (m) {
    int r = htsmsg_binary_serialize(m, &msgdata, &msglen, 0x10000);
    htsmsg_destroy(m);
    if (!r) {
      ssize_t w = write(fd, msgdata, msglen);
      free(msgdata);
      if(w == msglen) ret = 0;
    }
  } else {
    ret = 0;
  }
  if(ret) {
    tvhlog(LOG_DEBUG, "epg", "failed to store epg to disk");
    close(fd);
    hts_settings_remove("epgdb");
  }
  return ret;
}

static int _epg_write_sect ( int fd, const char *sect )
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "__section__", sect);
  return _epg_write(fd, m);
}

void epg_save ( void )
{
  int fd;
  epg_object_t  *eo;
  channel_t *ch;
  epggrab_stats_t stats;
  
  fd = hts_settings_open_file(1, "epgdb");

  memset(&stats, 0, sizeof(stats));
  if ( _epg_write_sect(fd, "brands") ) return;
  RB_FOREACH(eo,  &epg_brands, glink) {
    if (_epg_write(fd, epg_brand_serialize((epg_brand_t*)eo))) return;
    stats.brands.total++;
  }
  if ( _epg_write_sect(fd, "seasons") ) return;
  RB_FOREACH(eo,  &epg_seasons, glink) {
    if (_epg_write(fd, epg_season_serialize((epg_season_t*)eo))) return;
    stats.seasons.total++;
  }
  if ( _epg_write_sect(fd, "episodes") ) return;
  RB_FOREACH(eo,  &epg_episodes, glink) {
    if (_epg_write(fd, epg_episode_serialize((epg_episode_t*)eo))) return;
    stats.episodes.total++;
  }
  if ( _epg_write_sect(fd, "broadcasts") ) return;
  RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
    RB_FOREACH(eo, &ch->ch_epg_schedule, glink) {
      if (_epg_write(fd, epg_broadcast_serialize((epg_broadcast_t*)eo))) return;
      stats.broadcasts.total++;
    }
  }

  /* Stats */
  tvhlog(LOG_DEBUG, "epg", "database saved");
  tvhlog(LOG_DEBUG, "epg", "  brands     %d", stats.brands.total);
  tvhlog(LOG_DEBUG, "epg", "  seasons    %d", stats.seasons.total);
  tvhlog(LOG_DEBUG, "epg", "  episodes   %d", stats.episodes.total);
  tvhlog(LOG_DEBUG, "epg", "  broadcasts %d", stats.broadcasts.total);
}

void epg_init ( void )
{
  int save, fd;
  struct stat st;
  size_t remain;
  uint8_t *mem, *rp;
  char *sect = NULL;
  const char *s;
  epggrab_stats_t stats;
  
  /* Map file to memory */
  fd = hts_settings_open_file(0, "epgdb");
  if ( fd < 0 ) {
    tvhlog(LOG_DEBUG, "epg", "database does not exist");
    return;
  }
  if ( fstat(fd, &st) != 0 ) {
    tvhlog(LOG_ERR, "epg", "failed to detect database size");
    return;
  }
  if ( !st.st_size ) {
    tvhlog(LOG_DEBUG, "epg", "database is empty");
    return;
  }
  remain   = st.st_size;
  rp = mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if ( mem == MAP_FAILED ) {
    tvhlog(LOG_ERR, "epg", "failed to mmap database");
    return;
  }

  /* Process */
  memset(&stats, 0, sizeof(stats));
  while ( remain > 4 ) {

    // TODO: would be nice if htsmsg_binary handled this for us!

    /* Get message length */
    int msglen = (rp[0] << 24) | (rp[1] << 16) | (rp[2] << 8) | rp[3];
    remain    -= 4;
    rp        += 4;

    /* Extract message */
    htsmsg_t *m = htsmsg_binary_deserialize(rp, msglen, NULL);

    /* Process */
    if(m) {

      /* New section */
      s = htsmsg_get_str(m, "__section__");
      if (s) {
        if (sect) free(sect);
        sect = strdup(s);

      /* Brand */
      } else if ( !strcmp(sect, "brands") ) {
        if (epg_brand_deserialize(m, 1, &save)) stats.brands.total++;
        
      /* Season */
      } else if ( !strcmp(sect, "seasons") ) {
        if (epg_season_deserialize(m, 1, &save)) stats.seasons.total++;

      /* Episode */
      } else if ( !strcmp(sect, "episodes") ) {
        if (epg_episode_deserialize(m, 1, &save)) stats.episodes.total++;
  
      /* Broadcasts */
      } else if ( !strcmp(sect, "broadcasts") ) {
        if (epg_broadcast_deserialize(m, 1, &save)) stats.broadcasts.total++;

      /* Unknown */
      } else {
        tvhlog(LOG_DEBUG, "epg", "malformed database section [%s]", sect);
        //htsmsg_print(m);
      }

      /* Cleanup */
      htsmsg_destroy(m);
    }

    /* Next */
    rp     += msglen;
    remain -= msglen;
  }
  if (sect) free(sect);

  /* Stats */
  tvhlog(LOG_DEBUG, "epg", "database loaded");
  tvhlog(LOG_DEBUG, "epg", "  channels   %d", stats.channels.total);
  tvhlog(LOG_DEBUG, "epg", "  brands     %d", stats.brands.total);
  tvhlog(LOG_DEBUG, "epg", "  seasons    %d", stats.seasons.total);
  tvhlog(LOG_DEBUG, "epg", "  episodes   %d", stats.episodes.total);
  tvhlog(LOG_DEBUG, "epg", "  broadcasts %d", stats.broadcasts.total);
  tvhlog(LOG_DEBUG, "epg", "next object id %lu", _epg_object_idx+1);

  /* Close file */
  munmap(mem, st.st_size);
  close(fd);
}

void epg_updated ( void )
{
  epg_object_t *eo;
  while ((eo = LIST_FIRST(&epg_object_unref))) {
    tvhlog(LOG_DEBUG, "epg",
           "unref'd object %lu (%s) created during update", eo->id, eo->uri);
    LIST_REMOVE(eo, ulink);
    eo->destroy(eo);
  }
}

/* **************************************************************************
 * Object
 * *************************************************************************/

static void _epg_object_destroy ( epg_object_t *eo, epg_object_tree_t *tree )
{
  assert(eo->refcount == 0);
  if (eo->uri) free(eo->uri);
  if (tree) RB_REMOVE(tree, eo, glink);
  LIST_REMOVE(eo, hlink);
}

static void _epg_object_getref ( epg_object_t *eo )
{
  if (eo->refcount == 0) LIST_REMOVE(eo, ulink);
  eo->refcount++;
}

static void _epg_object_putref ( epg_object_t *eo )
{
  assert(eo->refcount>0);
  eo->refcount--;
  if (!eo->refcount) eo->destroy(eo);
}

static void _epg_object_create ( epg_object_t *eo )
{
  if (!eo->id) eo->id = ++_epg_object_idx;
  else if (eo->id > _epg_object_idx) _epg_object_idx = eo->id;
  if (!eo->getref) eo->getref = _epg_object_getref;
  if (!eo->putref) eo->putref = _epg_object_putref;
  if (eo->uri) eo->uri = strdup(eo->uri);
  LIST_INSERT_HEAD(&epg_object_unref, eo, ulink);
  LIST_INSERT_HEAD(&epg_objects[eo->id & EPG_HASH_MASK], eo, hlink);
}

static epg_object_t *_epg_object_find
  ( int create, int *save, epg_object_tree_t *tree,
    epg_object_t **skel, int (*cmp) (const void*,const void*))
{
  epg_object_t *eo;

  /* Find */
  if ( !create ) {
    eo = RB_FIND(tree, *skel, glink, cmp);
  
  /* Create */
  } else {
    eo = RB_INSERT_SORTED(tree, *skel, glink, cmp);
    if ( eo == NULL ) {
      *save     |= 1;
      eo         = *skel;
      *skel      = NULL;
      _epg_object_create(eo);
    }
  }

  return eo;
}

static epg_object_t *_epg_object_find_by_uri 
  ( const char *uri, int create, int *save,
    epg_object_tree_t *tree, epg_object_t **skel )
{
  int save2 = 0;
  epg_object_t *eo;
  
  assert(skel != NULL);
  lock_assert(&global_lock); // pointless!

  (*skel)->uri = (char*)uri;

  eo = _epg_object_find(create, &save2, tree, skel, _uri_cmp);
  if (save2) {
    *save  |= 1;
  }
  return eo;
}

static epg_object_t *_epg_object_find_by_id2 ( uint64_t id )
{
  epg_object_t *eo;
  LIST_FOREACH(eo, &epg_objects[id & EPG_HASH_MASK], hlink) {
    if (eo->id == id) return eo;
  }
  return NULL;
}

static epg_object_t *_epg_object_find_by_id
  ( uint64_t id, epg_object_tree_t *tree )
{
  epg_object_t *eo;
  if (!tree) return _epg_object_find_by_id2(id);
  RB_FOREACH(eo, tree, glink) {
    if ( eo->id == id ) return eo;
  }
  return NULL;
}

static htsmsg_t * _epg_object_serialize ( epg_object_t *eo )
{
  htsmsg_t *m;
  if ( !eo->id || !eo->type ) return NULL;
  m = htsmsg_create_map();
  htsmsg_add_u64(m, "id", eo->id);
  htsmsg_add_u32(m, "type", eo->type);
  if (eo->uri)
    htsmsg_add_str(m, "uri", eo->uri);
  return m;
}

static epg_object_t *_epg_object_deserialize ( htsmsg_t *m, epg_object_t *eo )
{
  if (htsmsg_get_u64(m, "id",   &eo->id))   return NULL;
  if (htsmsg_get_u32(m, "type", &eo->type)) return NULL;
  eo->uri = (char*)htsmsg_get_str(m, "uri");
  return eo;
}

/* **************************************************************************
 * Brand
 * *************************************************************************/

static void _epg_brand_destroy ( epg_object_t *eo )
{
  epg_brand_t *eb = (epg_brand_t*)eo;
  if (RB_FIRST(&eb->seasons)) {
    tvhlog(LOG_CRIT, "epg", "attempt to destroy brand with seasons");
    assert(0);
  }
  if (RB_FIRST(&eb->episodes)) {
    tvhlog(LOG_CRIT, "epg", "attempt to destroy brand with episodes");
    assert(0);
  }
  _epg_object_destroy(eo, &epg_brands);
  if (eb->title)   free(eb->title);
  if (eb->summary) free(eb->summary);
  free(eb);
}

static epg_object_t **_epg_brand_skel ( void )
{
  static epg_object_t *skel = NULL;
  if ( !skel ) {
    skel = calloc(1, sizeof(epg_brand_t));
    skel->type    = EPG_BRAND;
    skel->destroy = _epg_brand_destroy;
  }
  return &skel;
}

epg_brand_t* epg_brand_find_by_uri 
  ( const char *uri, int create, int *save )
{
  return (epg_brand_t*)
    _epg_object_find_by_uri(uri, create, save,
                            &epg_brands,
                            _epg_brand_skel());
}

epg_brand_t *epg_brand_find_by_id ( uint64_t id )
{
  return (epg_brand_t*)_epg_object_find_by_id(id, &epg_brands);
}

int epg_brand_set_title ( epg_brand_t *brand, const char *title )
{
  int save = 0;
  if ( !brand || !title ) return 0;
  if ( !brand->title || strcmp(brand->title, title) ) {
    if ( brand->title ) free(brand->title);
    brand->title = strdup(title);
    save = 1;
  }
  return save;
}

int epg_brand_set_summary ( epg_brand_t *brand, const char *summary )
{
  int save = 0;
  if ( !brand || !summary ) return 0;
  if ( !brand->summary || strcmp(brand->summary, summary) ) {
    if ( brand->summary ) free(brand->summary);
    brand->summary = strdup(summary);
    save = 1;
  }
  return save;
}

int epg_brand_set_season_count ( epg_brand_t *brand, uint16_t count )
{
  int save = 0;
  if ( !brand || !count ) return 0;
  if ( brand->season_count != count ) {
    brand->season_count = count;
    save = 1;
  }
  return save;
}

static void _epg_brand_add_season 
  ( epg_brand_t *brand, epg_season_t *season )
{
  RB_INSERT_SORTED(&brand->seasons, season, blink, _uri_cmp);
}

static void _epg_brand_rem_season
  ( epg_brand_t *brand, epg_season_t *season )
{
  RB_REMOVE(&brand->seasons, season, blink);
}

static void _epg_brand_add_episode
  ( epg_brand_t *brand, epg_episode_t *episode )
{
  RB_INSERT_SORTED(&brand->episodes, episode, blink, _uri_cmp);
}

static void _epg_brand_rem_episode
  ( epg_brand_t *brand, epg_episode_t *episode )
{
  RB_REMOVE(&brand->episodes, episode, blink);
}

htsmsg_t *epg_brand_serialize ( epg_brand_t *brand )
{
  htsmsg_t *m;
  if ( !brand || !brand->_.uri ) return NULL;
  if ( !(m = _epg_object_serialize((epg_object_t*)brand)) ) return NULL;
  if (brand->title)
    htsmsg_add_str(m, "title",   brand->title);
  if (brand->summary)
    htsmsg_add_str(m, "summary", brand->summary);
  if (brand->season_count)
    htsmsg_add_u32(m, "season-count", brand->season_count);
  return m;
}

epg_brand_t *epg_brand_deserialize ( htsmsg_t *m, int create, int *save )
{
  epg_object_t **skel = _epg_brand_skel();
  epg_brand_t *eb;
  uint32_t u32;
  const char *str;

  if ( !_epg_object_deserialize(m, *skel) ) return NULL;
  if ( !(eb = epg_brand_find_by_uri((*skel)->uri, create, save)) ) return NULL;
  
  if ( (str = htsmsg_get_str(m, "title")) )
    *save |= epg_brand_set_title(eb, str);
  if ( (str = htsmsg_get_str(m, "summary")) )
    *save |= epg_brand_set_summary(eb, str);
  if ( !htsmsg_get_u32(m, "season-count", &u32) )
    *save |= epg_brand_set_season_count(eb, u32);

  return eb;
}

/* **************************************************************************
 * Season
 * *************************************************************************/

static void _epg_season_destroy ( epg_object_t *eo )
{
  epg_season_t *es = (epg_season_t*)eo;
  if (RB_FIRST(&es->episodes)) {
    tvhlog(LOG_CRIT, "epg", "attempt to destory season with episodes");
    assert(0);
  }
  _epg_object_destroy(eo, &epg_seasons);
  if (es->brand) {
    _epg_brand_rem_season(es->brand, es);
    es->brand->_.putref((epg_object_t*)es->brand);
  }
  if (es->summary) free(es->summary);
  free(es);
}

static epg_object_t **_epg_season_skel ( void )
{
  static epg_object_t *skel = NULL;
  if ( !skel ) {
    skel = calloc(1, sizeof(epg_season_t));
    skel->type    = EPG_SEASON;
    skel->destroy = _epg_season_destroy;
  }
  return &skel;
}

epg_season_t* epg_season_find_by_uri 
  ( const char *uri, int create, int *save )
{
  return (epg_season_t*)
    _epg_object_find_by_uri(uri, create, save,
                            &epg_seasons,
                            _epg_season_skel());
}

epg_season_t *epg_season_find_by_id ( uint64_t id )
{
  return (epg_season_t*)_epg_object_find_by_id(id, &epg_seasons);
}

int epg_season_set_summary ( epg_season_t *season, const char *summary )
{
  int save = 0;
  if ( !season || !summary ) return 0;
  if ( !season->summary || strcmp(season->summary, summary) ) {
    if ( season->summary ) free(season->summary);
    season->summary = strdup(summary);
    save = 1;
  }
  return save;
}

int epg_season_set_episode_count ( epg_season_t *season, uint16_t count )
{
  int save = 0;
  if ( !season || !count ) return 0;
  if ( season->episode_count != count ) {
    season->episode_count = count;
    save = 1;
  }
  return save;
}

int epg_season_set_number ( epg_season_t *season, uint16_t number )
{
  int save = 0;
  if ( !season || !number ) return 0;
  if ( season->number != number ) {
    season->number = number;
    save = 1;
  }
  return save;
}

int epg_season_set_brand ( epg_season_t *season, epg_brand_t *brand, int u )
{
  int save = 0;
  if ( !season || !brand ) return 0;
  if ( season->brand != brand ) {
    if ( season->brand ) {
      _epg_brand_rem_season(season->brand, season);
      season->brand->_.putref((epg_object_t*)season->brand);
    }
    season->brand = brand;
    _epg_brand_add_season(brand, season);
    brand->_.getref((epg_object_t*)brand);
    save = 1;
  }
  return save;
}

static void _epg_season_add_episode
  ( epg_season_t *season, epg_episode_t *episode )
{
  RB_INSERT_SORTED(&season->episodes, episode, slink, _uri_cmp);
}

static void _epg_season_rem_episode
  ( epg_season_t *season, epg_episode_t *episode )
{
  RB_REMOVE(&season->episodes, episode, slink);
}

htsmsg_t *epg_season_serialize ( epg_season_t *season )
{
  htsmsg_t *m;
  if (!season || !season->_.uri) return NULL;
  if (!(m = _epg_object_serialize((epg_object_t*)season))) return NULL;
  if (season->summary)
    htsmsg_add_str(m, "summary", season->summary);
  if (season->number)
    htsmsg_add_u32(m, "number", season->number);
  if (season->episode_count)
    htsmsg_add_u32(m, "episode-count", season->episode_count);
  if (season->brand)
    htsmsg_add_str(m, "brand", season->brand->_.uri);
  return m;
}

epg_season_t *epg_season_deserialize ( htsmsg_t *m, int create, int *save )
{
  epg_object_t **skel = _epg_brand_skel();
  epg_season_t *es;
  epg_brand_t *eb;
  uint32_t u32;
  const char *str;

  if ( !_epg_object_deserialize(m, *skel) ) return NULL;
  if ( !(es = epg_season_find_by_uri((*skel)->uri, create, save)) ) return NULL;
  
  if ( (str = htsmsg_get_str(m, "summary")) )
    *save |= epg_season_set_summary(es, str);
  if ( !htsmsg_get_u32(m, "number", &u32) )
    *save |= epg_season_set_number(es, u32);
  if ( !htsmsg_get_u32(m, "episode-count", &u32) )
    *save |= epg_season_set_episode_count(es, u32);
  
  if ( (str = htsmsg_get_str(m, "brand")) )
    if ( (eb = epg_brand_find_by_uri(str, 0, NULL)) )
      *save |= epg_season_set_brand(es, eb, 1);

  return es;
}

/* **************************************************************************
 * Episode
 * *************************************************************************/

static void _epg_episode_destroy ( epg_object_t *eo )
{
  epg_episode_t *ee = (epg_episode_t*)eo;
  if (RB_FIRST(&ee->broadcasts)) {
    tvhlog(LOG_CRIT, "epg", "attempt to destroy episode with broadcasts");
    assert(0);
  }
  _epg_object_destroy(eo, &epg_episodes);
  if (ee->brand) {
    _epg_brand_rem_episode(ee->brand, ee);
    ee->brand->_.putref((epg_object_t*)ee->brand);
  }
  if (ee->season) {
    _epg_season_rem_episode(ee->season, ee);
    ee->season->_.putref((epg_object_t*)ee->season);
  }
  if (ee->title)       free(ee->title);
  if (ee->subtitle)    free(ee->subtitle);
  if (ee->summary)     free(ee->summary);
  if (ee->description) free(ee->description);
  free(ee);
}

static epg_object_t **_epg_episode_skel ( void )
{
  static epg_object_t *skel = NULL;
  if ( !skel ) {
    skel = calloc(1, sizeof(epg_episode_t));
    skel->type    = EPG_EPISODE;
    skel->destroy = _epg_episode_destroy;
  }
  return &skel;
}

epg_episode_t* epg_episode_find_by_uri
  ( const char *uri, int create, int *save )
{
  return (epg_episode_t*)
    _epg_object_find_by_uri(uri, create, save,
                            &epg_episodes,
                            _epg_episode_skel());
}

epg_episode_t *epg_episode_find_by_id ( uint64_t id )
{
  return (epg_episode_t*)_epg_object_find_by_id(id, &epg_episodes);
}

int epg_episode_set_title ( epg_episode_t *episode, const char *title )
{
  int save = 0;
  if ( !episode || !title ) return 0;
  if ( !episode->title || strcmp(episode->title, title) ) {
    if ( episode->title ) free(episode->title);
    episode->title = strdup(title);
    save = 1;
  }
  return save;
}

int epg_episode_set_subtitle ( epg_episode_t *episode, const char *subtitle )
{
  int save = 0;
  if ( !episode || !subtitle ) return 0;
  if ( !episode->subtitle || strcmp(episode->subtitle, subtitle) ) {
    if ( episode->subtitle ) free(episode->subtitle);
    episode->subtitle = strdup(subtitle);
    save = 1;
  }
  return save;
}

int epg_episode_set_summary ( epg_episode_t *episode, const char *summary )
{
  int save = 0;
  if ( !episode || !summary ) return 0;
  if ( !episode->summary || strcmp(episode->summary, summary) ) {
    if ( episode->summary ) free(episode->summary);
    episode->summary = strdup(summary);
    save = 1;
  }
  return save;
}

int epg_episode_set_description ( epg_episode_t *episode, const char *desc )
{
  int save = 0;
  if ( !episode || !desc ) return 0;
  if ( !episode->description || strcmp(episode->description, desc) ) {
    if ( episode->description ) free(episode->description);
    episode->description = strdup(desc);
    save = 1;
  }
  return save;
}

int epg_episode_set_number ( epg_episode_t *episode, uint16_t number )
{
  int save = 0;
  if ( !episode || !number ) return 0;
  if ( episode->number != number ) {
    episode->number = number;
    save = 1;
  }
  return save;
}

int epg_episode_set_part ( epg_episode_t *episode, uint16_t part, uint16_t count )
{
  int save = 0;
  if ( !episode || !part ) return 0;
  if ( episode->part_number != part ) {
    episode->part_number = part;
    save |= 1;
  }
  if ( count && episode->part_count != count ) {
    episode->part_count = count;
    save |= 1;
  }
  return save;
}

int epg_episode_set_brand ( epg_episode_t *episode, epg_brand_t *brand )
{
  int save = 0;
  if ( !episode || !brand ) return 0;
  if ( episode->brand != brand ) {
    if ( episode->brand ) {
      _epg_brand_rem_episode(episode->brand, episode);
      episode->brand->_.putref((epg_object_t*)episode->brand);
    }
    episode->brand = brand;
    _epg_brand_add_episode(brand, episode);
    brand->_.getref((epg_object_t*)brand);
    save = 1;
  }
  return save;
}

int epg_episode_set_season ( epg_episode_t *episode, epg_season_t *season )
{
  int save = 0;
  if ( !episode || !season ) return 0;
  if ( episode->season != season ) {
    if ( episode->season ) {
      _epg_season_rem_episode(episode->season, episode);
      episode->season->_.putref((epg_object_t*)episode->season);
    }
    episode->season = season;
    _epg_season_add_episode(season, episode);
    season->_.getref((epg_object_t*)season);
    if ( season->brand ) save |= epg_episode_set_brand(episode, season->brand);
    save = 1;
  }
  return save;
}

static void _epg_episode_add_broadcast 
  ( epg_episode_t *episode, epg_broadcast_t *broadcast )
{
  RB_INSERT_SORTED(&episode->broadcasts, broadcast, elink, _id_cmp);
}

static void _epg_episode_rem_broadcast
  ( epg_episode_t *episode, epg_broadcast_t *broadcast )
{
  RB_REMOVE(&episode->broadcasts, broadcast, elink);
}


size_t epg_episode_number_format 
  ( epg_episode_t *episode, char *buf, size_t len,
    const char *pre,  const char *sfmt,
    const char *sep,  const char *efmt,
    const char *cfmt )
{
  size_t i = 0;
  if ( episode->number ) {
    if (pre) i += snprintf(&buf[i], len-i, "%s", pre);
    if ( sfmt && episode->season && episode->season->number ) {
      i += snprintf(&buf[i], len-i, sfmt, episode->season->number);
      if ( cfmt && episode->brand && episode->brand->season_count )
        i += snprintf(&buf[i], len-i, cfmt,
                      episode->brand->season_count);
      if (sep) i += snprintf(&buf[i], len-i, "%s", sep);
    }
    i += snprintf(&buf[i], len-i, efmt, episode->number);
    if ( cfmt && episode->season && episode->season->episode_count)
      i+= snprintf(&buf[i], len-i, cfmt, episode->season->episode_count);
  }
  return i;
}

int epg_episode_fuzzy_match
  ( epg_episode_t *episode, const char *uri, const char *title,
    const char *summary, const char *description )
{
  if ( !episode ) return 0;
  if ( uri && episode->_.uri && !strcmp(episode->_.uri, uri) ) return 1;
  // TODO: how to match title/summary/description
  return 0;
}

htsmsg_t *epg_episode_serialize ( epg_episode_t *episode )
{
  htsmsg_t *m;
  if (!episode || !episode->_.uri) return NULL;
  if (!(m = _epg_object_serialize((epg_object_t*)episode))) return NULL;
  htsmsg_add_str(m, "uri", episode->_.uri);
  if (episode->title)
    htsmsg_add_str(m, "title", episode->title);
  if (episode->subtitle)
    htsmsg_add_str(m, "subtitle", episode->subtitle);
  if (episode->summary)
    htsmsg_add_str(m, "summary", episode->summary);
  if (episode->description)
    htsmsg_add_str(m, "description", episode->description);
  if (episode->number)
    htsmsg_add_u32(m, "number", episode->number);
  if (episode->part_count && episode->part_count) {
    htsmsg_add_u32(m, "part-number", episode->part_number);
    htsmsg_add_u32(m, "part-count", episode->part_count);
  }
  if (episode->brand)
    htsmsg_add_str(m, "brand", episode->brand->_.uri);
  if (episode->season)
    htsmsg_add_str(m, "season", episode->season->_.uri);
  return m;
}

epg_episode_t *epg_episode_deserialize ( htsmsg_t *m, int create, int *save )
{
  epg_object_t **skel = _epg_brand_skel();
  epg_episode_t *ee;
  epg_season_t *es;
  epg_brand_t *eb;
  uint32_t u32, u32a;
  const char *str;
  
  if ( !_epg_object_deserialize(m, *skel) ) return NULL;
  if ( !(ee = epg_episode_find_by_uri((*skel)->uri, create, save)) ) return NULL;
  
  if ( (str = htsmsg_get_str(m, "title")) )
    *save |= epg_episode_set_title(ee, str);
  if ( (str = htsmsg_get_str(m, "subtitle")) )
    *save |= epg_episode_set_subtitle(ee, str);
  if ( (str = htsmsg_get_str(m, "summary")) )
    *save |= epg_episode_set_summary(ee, str);
  if ( (str = htsmsg_get_str(m, "description")) )
    *save |= epg_episode_set_description(ee, str);
  if ( !htsmsg_get_u32(m, "number", &u32) )
    *save |= epg_episode_set_number(ee, u32);
  if ( !htsmsg_get_u32(m, "part-number", &u32) &&
       !htsmsg_get_u32(m, "part-count", &u32a) )
    *save |= epg_episode_set_part(ee, u32, u32a);
  
  if ( (str = htsmsg_get_str(m, "brand")) )
    if ( (eb = epg_brand_find_by_uri(str, 0, NULL)) )
      *save |= epg_episode_set_brand(ee, eb);
  if ( (str = htsmsg_get_str(m, "season")) )
    if ( (es = epg_season_find_by_uri(str, 0, NULL)) )
      *save |= epg_episode_set_season(ee, es);
  
  return ee;
}

/* **************************************************************************
 * Channel
 * *************************************************************************/

static void _epg_channel_timer_callback ( void *p )
{
  time_t next = 0;
  epg_object_t *eo;
  epg_broadcast_t *ebc, *cur;
  channel_t *ch = (channel_t*)p;

  /* Clear now/next */
  cur = ch->ch_epg_now;
  ch->ch_epg_now = ch->ch_epg_next = NULL;

  /* Check events */
  while ( (eo = RB_FIRST(&ch->ch_epg_schedule)) ) {
    ebc = (epg_broadcast_t*)eo;

    /* Expire */
    if ( ebc->stop <= dispatch_clock ) {
      RB_REMOVE(&ch->ch_epg_schedule, eo, glink);
      tvhlog(LOG_DEBUG, "epg", "expire event %lu from %s",
             eo->id, ch->ch_name);
      eo->putref(eo);
      continue; // skip to next

    /* No now */
    } else if ( ebc->start > dispatch_clock ) {
      ch->ch_epg_next = ebc;
      next            = ebc->start;

    /* Now/Next */
    } else {
      ch->ch_epg_now  = ebc;
      ch->ch_epg_next = (epg_broadcast_t*)RB_NEXT(eo, glink);
      next            = ebc->stop;
    }
    break;
  }
  tvhlog(LOG_DEBUG, "epg", "now/next %lu/%lu set on %s",
         ch->ch_epg_now  ? ch->ch_epg_now->_.id : 0,
         ch->ch_epg_next ? ch->ch_epg_next->_.id : 0,
         ch->ch_name);

  /* re-arm */
  if ( next ) {
    tvhlog(LOG_DEBUG, "epg", "arm channel timer @ %lu for %s",
           next, ch->ch_name);
    gtimer_arm_abs(&ch->ch_epg_timer, _epg_channel_timer_callback, ch, next);
  }

  /* Update HTSP */
  if ( cur != ch->ch_epg_now ) {
    tvhlog(LOG_DEBUG, "epg", "inform HTSP of now event change on %s",
           ch->ch_name);
    htsp_channel_update_current(ch);
  }
}

static void _epg_channel_rem_broadcast 
  ( channel_t *ch, epg_broadcast_t *ebc, epg_broadcast_t *new )
{
  if (new) dvr_event_replaced(ebc, new);
  RB_REMOVE(&ch->ch_epg_schedule, (epg_object_t*)ebc, glink);
  ebc->_.putref((epg_object_t*)ebc);
}

static epg_broadcast_t *_epg_channel_add_broadcast 
  ( channel_t *ch, epg_broadcast_t **bcast, int create, int *save )
{
  int save2 = 0, timer = 0;
  epg_object_t *eo;
  epg_broadcast_t *ebc, *ret;

  /* Set channel */
  (*bcast)->channel = ch;

  /* Find/Create */
  eo = _epg_object_find(create, &save2, &ch->ch_epg_schedule,
                        (epg_object_t**)bcast, _ebc_start_cmp);
  if (!eo) return NULL;
  ret = (epg_broadcast_t*)eo;

  /* No changes */
  if (!save2 && (ret->start == (*bcast)->start) &&
                (ret->stop  == (*bcast)->stop)) return ret;

  // Note: scheduling changes are relatively rare and therefore
  //       the rest of this code will happen infrequently (hopefully)

  /* Grab ref */
  ret->_.getref((epg_object_t*)ret);
  *save |= 1;

  /* Remove overlapping (before) */
  while ( (ebc = (epg_broadcast_t*)RB_PREV(eo, glink)) != NULL ) {
    if ( ebc->stop <= ret->start ) break;
    if ( ch->ch_epg_now == ebc ) ch->ch_epg_now = NULL;
    _epg_channel_rem_broadcast(ch, ebc, ret);
  }

  /* Remove overlapping (after) */
  while ( (ebc = (epg_broadcast_t*)RB_NEXT(eo, glink)) != NULL ) {
    if ( ebc->start >= ret->stop ) break;
    _epg_channel_rem_broadcast(ch, ebc, ret);
  }

  /* Check now/next change */
  if ( RB_FIRST(&ch->ch_epg_schedule) == eo ) {
    timer = 1;
  } else if ( ch->ch_epg_now &&
            RB_NEXT((epg_object_t*)ch->ch_epg_now, glink) == eo ) {
    timer = 1;
  }

  /* Reset timer */
  if (timer) _epg_channel_timer_callback(ch);
  return ret;
}

void epg_channel_unlink ( channel_t *ch )
{
  epg_object_t *eo;
  while ( (eo = RB_FIRST(&ch->ch_epg_schedule)) ) {
    _epg_channel_rem_broadcast(ch, (epg_broadcast_t*)eo, NULL);
  }
  gtimer_disarm(&ch->ch_epg_timer);
}

/* **************************************************************************
 * Broadcast
 * *************************************************************************/

static void _epg_broadcast_destroy ( epg_object_t *eo )
{
  epg_broadcast_t *ebc = (epg_broadcast_t*)eo;
  _epg_object_destroy(eo, NULL);
  if (ebc->episode) {
    _epg_episode_rem_broadcast(ebc->episode, ebc);
    ebc->episode->_.putref((epg_object_t*)ebc->episode);
  }
  free(ebc);
}

static epg_broadcast_t **_epg_broadcast_skel ( void )
{
  static epg_broadcast_t *skel = NULL;
  if ( !skel ) {
    skel = calloc(1, sizeof(epg_broadcast_t));
    skel->_.type    = EPG_BROADCAST;
    skel->_.destroy = _epg_broadcast_destroy;
  }
  return &skel;
}

epg_broadcast_t* epg_broadcast_find_by_time 
  ( channel_t *channel, time_t start, time_t stop, int create, int *save )
{
  epg_broadcast_t **ebc;
  if ( !channel || !start || !stop ) return NULL;
  if ( stop <= start ) return NULL;
  if ( stop < dispatch_clock ) return NULL;

  ebc = _epg_broadcast_skel();
  (*ebc)->start   = start;
  (*ebc)->stop    = stop;

  return _epg_channel_add_broadcast(channel, ebc, create, save);
}

epg_broadcast_t *epg_broadcast_find_by_id ( uint64_t id, channel_t *ch )
{
  epg_object_t *eo = NULL;
  if ( ch ) {
    eo = _epg_object_find_by_id(id, &ch->ch_epg_schedule);
  } else {
    eo = _epg_object_find_by_id2(id);
  }
  return (epg_broadcast_t*)eo;
}

int epg_broadcast_set_episode 
  ( epg_broadcast_t *broadcast, epg_episode_t *episode )
{
  int save = 0;
  if ( !broadcast || !episode ) return 0;
  if ( broadcast->episode != episode ) {
    if ( broadcast->episode ) {
      _epg_episode_rem_broadcast(broadcast->episode, broadcast);
      broadcast->episode->_.putref((epg_object_t*)broadcast->episode);
    }
    _epg_episode_add_broadcast(episode, broadcast);
    broadcast->episode = episode;
    episode->_.getref((epg_object_t*)episode);
    save = 1;
  }
  return save;
}

epg_broadcast_t *epg_broadcast_get_next ( epg_broadcast_t *broadcast )
{
  if ( !broadcast ) return NULL;
  return (epg_broadcast_t*)RB_NEXT((epg_object_t*)broadcast, glink);
}

htsmsg_t *epg_broadcast_serialize ( epg_broadcast_t *broadcast )
{
  htsmsg_t *m;
  if (!broadcast) return NULL;
  if (!broadcast->episode || !broadcast->episode->_.uri) return NULL;
  if (!(m = _epg_object_serialize((epg_object_t*)broadcast))) return NULL;
  htsmsg_add_u32(m, "start", broadcast->start);
  htsmsg_add_u32(m, "stop", broadcast->stop);
  htsmsg_add_str(m, "episode", broadcast->episode->_.uri);
  if (broadcast->channel)
    htsmsg_add_u32(m, "channel", broadcast->channel->ch_id);
  
  return m;
}

epg_broadcast_t *epg_broadcast_deserialize
  ( htsmsg_t *m, int create, int *save )
{
  channel_t *ch = NULL;
  epg_broadcast_t *ret, **ebc = _epg_broadcast_skel();
  epg_episode_t *ee;
  const char *str;
  uint32_t chid, start, stop;

  if ( htsmsg_get_u32(m, "start", &start) ) return NULL;
  if ( htsmsg_get_u32(m, "stop", &stop)   ) return NULL;
  if ( !start || !stop ) return NULL;
  if ( stop <= start ) return NULL;
  if ( stop < dispatch_clock ) return NULL;
  if ( !(str = htsmsg_get_str(m, "episode")) ) return NULL;
  if ( !(ee  = epg_episode_find_by_uri(str, 0, NULL)) ) return NULL;

  /* Set properties */
  _epg_object_deserialize(m, (epg_object_t*)*ebc);
  (*ebc)->start   = start;
  (*ebc)->stop    = stop;

  /* Get channel */
  if ( !htsmsg_get_u32(m, "channel", &chid) ) {
    ch  = channel_find_by_identifier(chid);
  }

  /* Add to channel */
  if ( ch ) {
    ret = _epg_channel_add_broadcast(ch, ebc, create, save);

  /* Create dangling (possibly in use by DVR?) */
  } else {
    ret = *ebc;
    _epg_object_create((epg_object_t*)*ebc);
    *ebc = NULL;
  }

  /* Set the episode */
  *save |= epg_broadcast_set_episode(ret, ee);

  return ret;
}

/* **************************************************************************
 * Querying
 * *************************************************************************/

static void _eqr_add 
  ( epg_query_result_t *eqr, epg_broadcast_t *e,
    uint8_t genre, regex_t *preg, time_t start )
{
  /* Ignore */
  if ( e->stop < start ) return;
#if TODO_GENRE_SUPPORT
  if ( genre && e->genre != genre ) return;
#endif
  if ( !e->episode->title ) return;
  if ( preg && regexec(preg, e->episode->title, 0, NULL, 0) ) return;

  /* More space */
  if ( eqr->eqr_entries == eqr->eqr_alloced ) {
    eqr->eqr_alloced = MAX(100, eqr->eqr_alloced * 2);
    eqr->eqr_array   = realloc(eqr->eqr_array, 
                               eqr->eqr_alloced * sizeof(epg_broadcast_t));
  }
  
  /* Store */
  eqr->eqr_array[eqr->eqr_entries++] = e;
}

static void _eqr_add_channel 
  ( epg_query_result_t *eqr, channel_t *ch, uint8_t genre,
    regex_t *preg, time_t start )
{
  epg_object_t *eo;
  epg_broadcast_t *ebc;
  RB_FOREACH(eo, &ch->ch_epg_schedule, glink) {
    ebc = (epg_broadcast_t*)eo;
    if ( ebc->episode ) _eqr_add(eqr, ebc, genre, preg, start);
  }
}

void epg_query0
  ( epg_query_result_t *eqr, channel_t *channel, channel_tag_t *tag,
    uint8_t genre, const char *title )
{
  time_t now;
  channel_tag_mapping_t *ctm;
  regex_t preg0, *preg;
  time(&now);

  /* Clear (just incase) */
  memset(eqr, 0, sizeof(epg_query_result_t));

  /* Setup exp */
  if ( title ) {
    if (regcomp(&preg0, title, REG_ICASE | REG_EXTENDED | REG_NOSUB) )
      return;
    preg = &preg0;
  } else {
    preg = NULL;
  }
  
  /* Single channel */
  if (channel && !tag) {
    _eqr_add_channel(eqr, channel, genre, preg, now);
  
  /* Tag based */
  } else if ( tag ) {
    LIST_FOREACH(ctm, &tag->ct_ctms, ctm_tag_link) {
      if(channel == NULL || ctm->ctm_channel == channel)
        _eqr_add_channel(eqr, ctm->ctm_channel, genre, preg, now);
    }

  /* All channels */
  } else {
    RB_FOREACH(channel, &channel_name_tree, ch_name_link) {
      _eqr_add_channel(eqr, channel, genre, preg, now);
    }
  }

  return;
}

void epg_query(epg_query_result_t *eqr, const char *channel, const char *tag,
	       const char *genre, const char *title)
{
  channel_t     *ch = channel ? channel_find_by_name(channel, 0, 0) : NULL;
  channel_tag_t *ct = tag     ? channel_tag_find_by_name(tag, 0)    : NULL;
#if TODO_GENRE_SUPPORT
  uint8_t        ge = genre   ? epg_content_group_find_by_name(genre) : 0;
#endif
  epg_query0(eqr, ch, ct, 0, title);
}

void epg_query_free(epg_query_result_t *eqr)
{
  free(eqr->eqr_array);
}

static int _epg_sort_start_ascending ( const void *a, const void *b )
{
  return (*(epg_broadcast_t**)a)->start - (*(epg_broadcast_t**)b)->start;
}

void epg_query_sort(epg_query_result_t *eqr)
{
  qsort(eqr->eqr_array, eqr->eqr_entries, sizeof(epg_broadcast_t*),
        _epg_sort_start_ascending);
}
