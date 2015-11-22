/*
 *  Electronic Program Guide - Common functions
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <assert.h>
#include <inttypes.h>
#include <time.h>

#include "tvheadend.h"
#include "queue.h"
#include "channels.h"
#include "settings.h"
#include "epg.h"
#include "dvr/dvr.h"
#include "htsp_server.h"
#include "epggrab.h"
#include "imagecache.h"
#include "notify.h"

/* Broadcast hashing */
#define EPG_HASH_WIDTH 1024
#define EPG_HASH_MASK  (EPG_HASH_WIDTH - 1)

/* Objects tree */
epg_object_tree_t epg_objects[EPG_HASH_WIDTH];

/* URI lists */
epg_object_tree_t epg_brands;
epg_object_tree_t epg_seasons;
epg_object_tree_t epg_episodes;
epg_object_tree_t epg_serieslinks;

/* Other special case lists */
epg_object_list_t epg_object_unref;
epg_object_list_t epg_object_updated;

int epg_in_load;

/* Global counter */
static uint32_t _epg_object_idx    = 0;

/*
 *
 */
static inline epg_object_tree_t *epg_id_tree( epg_object_t *eo )
{
  return &epg_objects[eo->id & EPG_HASH_MASK];
}

/* **************************************************************************
 * Comparators / Ordering
 * *************************************************************************/

static int _id_cmp ( const void *a, const void *b )
{
  return ((epg_object_t*)a)->id - ((epg_object_t*)b)->id;
}

static int _uri_cmp ( const void *a, const void *b )
{
  return strcmp(((epg_object_t*)a)->uri, ((epg_object_t*)b)->uri);
}

static int _ebc_start_cmp ( const void *a, const void *b )
{
  return ((epg_broadcast_t*)a)->start - ((epg_broadcast_t*)b)->start;
}

static int _season_order ( const void *_a, const void *_b )
{
  const epg_season_t *a = (const epg_season_t*)_a;
  const epg_season_t *b = (const epg_season_t*)_b;
  if ( !a || !a->number ) return 1;
  if ( !b || !b->number ) return -1;
  return a->number - b->number;
}

// Note: this will do nothing with text episode numbering
static int _episode_order ( const void *_a, const void *_b )
{
  int r, as, bs;
  const epg_episode_t *a = (const epg_episode_t*)_a;
  const epg_episode_t *b = (const epg_episode_t*)_b;
  if (!a) return -1;
  if (!b) return 1;
  if (a->season) as = a->season->number;
  else           as = a->epnum.s_num;
  if (b->season) bs = b->season->number;
  else           bs = b->epnum.s_num;
  r = as - bs;
  if (r) return r;
  r = a->epnum.e_num - b->epnum.e_num;
  if (r) return r;
  return a->epnum.p_num - b->epnum.p_num;
}

void epg_updated ( void )
{
  epg_object_t *eo;

  lock_assert(&global_lock);

  /* Remove unref'd */
  while ((eo = LIST_FIRST(&epg_object_unref))) {
    tvhtrace("epg",
             "unref'd object %u (%s) created during update", eo->id, eo->uri);
    LIST_REMOVE(eo, un_link);
    eo->destroy(eo);
  }
  // Note: we do things this way around since unref'd objects are not likely
  //       to be useful to DVR since they will relate to episode/seasons/brands
  //       with no valid broadcasts etc..

  /* Update updated */
  while ((eo = LIST_FIRST(&epg_object_updated))) {
    eo->update(eo);
    LIST_REMOVE(eo, up_link);
    eo->_updated = 0;
    eo->created  = dispatch_clock;
  }
}

/* **************************************************************************
 * Object (Generic routines)
 * *************************************************************************/

static void _epg_object_destroy 
  ( epg_object_t *eo, epg_object_tree_t *tree )
{
  assert(eo->refcount == 0);
  tvhtrace("epg", "eo [%p, %u, %d, %s] destroy",
           eo, eo->id, eo->type, eo->uri);
  if (eo->uri) free(eo->uri);
  if (tree) RB_REMOVE(tree, eo, uri_link);
  if (eo->_updated) LIST_REMOVE(eo, up_link);
  RB_REMOVE(epg_id_tree(eo), eo, id_link);
}

static void _epg_object_getref ( void *o )
{
  epg_object_t *eo = o;
  tvhtrace("epg", "eo [%p, %u, %d, %s] getref %d",
           eo, eo->id, eo->type, eo->uri, eo->refcount+1);
  if (eo->refcount == 0) LIST_REMOVE(eo, un_link);
  eo->refcount++;
}

static void _epg_object_putref ( void *o )
{
  epg_object_t *eo = o;
  tvhtrace("epg", "eo [%p, %u, %d, %s] putref %d",
           eo, eo->id, eo->type, eo->uri, eo->refcount-1);
  assert(eo->refcount>0);
  eo->refcount--;
  if (!eo->refcount) eo->destroy(eo);
}

static void _epg_object_set_updated ( void *o )
{
  epg_object_t *eo = o;
  if (!eo->_updated) {
    tvhtrace("epg", "eo [%p, %u, %d, %s] updated",
             eo, eo->id, eo->type, eo->uri);
    eo->_updated = 1;
    eo->updated  = dispatch_clock;
    LIST_INSERT_HEAD(&epg_object_updated, eo, up_link);
  }
}

static void _epg_object_create ( void *o )
{
  epg_object_t *eo = o;
  uint32_t id = eo->id;
  if (!id) eo->id = ++_epg_object_idx;
  if (!eo->id) eo->id = ++_epg_object_idx;
  if (!eo->getref) eo->getref = _epg_object_getref;
  if (!eo->putref) eo->putref = _epg_object_putref;
  tvhtrace("epg", "eo [%p, %u, %d, %s] created",
           eo, eo->id, eo->type, eo->uri);
  _epg_object_set_updated(eo);
  LIST_INSERT_HEAD(&epg_object_unref, eo, un_link);
  while (1) {
    if (!RB_INSERT_SORTED(epg_id_tree(eo), eo, id_link, _id_cmp))
      break;
    if (id) {
      tvherror("epg", "fatal error, duplicate EPG ID");
      abort();
    }
    eo->id = ++_epg_object_idx;
    if (!eo->id) eo->id = ++_epg_object_idx;
  }
}

static epg_object_t *_epg_object_find_by_uri
  ( const char *uri, int create, int *save,
    epg_object_tree_t *tree, epg_object_t **skel )
{
  epg_object_t *eo;

  assert(skel != NULL);
  lock_assert(&global_lock);

  (*skel)->uri = (char*)uri;

  /* Find only */
  if ( !create ) {
    eo = RB_FIND(tree, *skel, uri_link, _uri_cmp);
  
  /* Find/create */
  } else {
    eo = RB_INSERT_SORTED(tree, *skel, uri_link, _uri_cmp);
    if ( !eo ) {
      *save        = 1;
      eo           = *skel;
      *skel        = NULL;
      eo->uri      = strdup(uri);
      _epg_object_create(eo);
    }
  }
  return eo;
}

epg_object_t *epg_object_find_by_id ( uint32_t id, epg_object_type_t type )
{
  epg_object_t *eo, temp;
  temp.id = id;
  eo = RB_FIND(epg_id_tree(&temp), &temp, id_link, _id_cmp);
  if (eo && eo->type == type)
    return eo;
  return NULL;
}

static htsmsg_t * _epg_object_serialize ( void *o )
{
  epg_object_t *eo = o;
  tvhtrace("epg", "eo [%p, %u, %d, %s] serialize",
           eo, eo->id, eo->type, eo->uri);
  htsmsg_t *m;
  if ( !eo->id || !eo->type ) return NULL;
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", eo->id);
  htsmsg_add_u32(m, "type", eo->type);
  if (eo->uri)
    htsmsg_add_str(m, "uri", eo->uri);
  if (eo->grabber)
    htsmsg_add_str(m, "grabber", eo->grabber->id);
  htsmsg_add_s64(m, "updated", eo->updated);
  return m;
}

static epg_object_t *_epg_object_deserialize ( htsmsg_t *m, epg_object_t *eo )
{
  int64_t s64;
  uint32_t u32;
  const char *s;
  if (htsmsg_get_u32(m, "id",   &eo->id)) return NULL;
  if (htsmsg_get_u32(m, "type", &u32))    return NULL;
  if (u32 != eo->type)                    return NULL;
  eo->uri = (char*)htsmsg_get_str(m, "uri");
  if ((s = htsmsg_get_str(m, "grabber")))
    eo->grabber = epggrab_module_find_by_id(s);
  if (!htsmsg_get_s64(m, "updated", &s64)) {
    _epg_object_set_updated(eo);
    eo->updated = s64;
  }
  tvhtrace("epg", "eo [%p, %u, %d, %s] deserialize",
           eo, eo->id, eo->type, eo->uri);
  return eo;
}

static int _epg_object_set_grabber ( void *o, epggrab_module_t *grab  )
{
  epg_object_t *eo = o;
  if ( !grab ) return 1; // grab=NULL is override
  if ( !eo->grabber ||
       ((eo->grabber != grab) && (grab->priority > eo->grabber->priority)) ) {
    eo->grabber = grab;
  }
  return grab == eo->grabber;
}

static int _epg_object_set_str
  ( void *o, char **old, const char *new, epggrab_module_t *src )
{
  int save = 0;
  epg_object_t *eo = o;
  if ( !eo || !new ) return 0;
  if ( !_epg_object_set_grabber(eo, src) && *old ) return 0;
  if ( !*old || strcmp(*old, new) ) {
    if ( *old ) free(*old);
    *old = strdup(new);
    _epg_object_set_updated(eo);
    save = 1;
  }
  return save;
}

static int _epg_object_set_lang_str
  ( void *o, lang_str_t **old, const char *newstr, const char *newlang,
    epggrab_module_t *src )
{
  int update, save;
  epg_object_t *eo = o;
  if ( !eo || !newstr ) return 0;
  update = _epg_object_set_grabber(eo, src);
  if (!*old) *old = lang_str_create();
  save = lang_str_add(*old, newstr, newlang, update);
  if (save)
    _epg_object_set_updated(eo);
  return save;
}

static int _epg_object_set_lang_str2
  ( void *o, lang_str_t **old, const lang_str_t *str, epggrab_module_t *src )
{
  int save = 0;
  lang_str_ele_t *ls;
  RB_FOREACH(ls, str, link) {
    save |= _epg_object_set_lang_str(o, old, ls->str, ls->lang, src);
  }
  return save;
}

static int _epg_object_set_u8
  ( void *o, uint8_t *old, const uint8_t new, epggrab_module_t *src )
{
  int save = 0;
  if ( !_epg_object_set_grabber(o, src) && *old ) return 0;
  if ( *old != new ) {
    *old = new;
    _epg_object_set_updated(o);
    save = 1;
  }
  return save;
}

static int _epg_object_set_u16
  ( void *o, uint16_t *old, const uint16_t new, epggrab_module_t *src )
{
  int save = 0;
  if ( !_epg_object_set_grabber(o, src) && *old ) return 0;
  if ( *old != new ) {
    *old = new;
    _epg_object_set_updated(o);
    save = 1;
  }
  return save;
}

htsmsg_t *epg_object_serialize ( epg_object_t *eo )
{
  if (!eo) return NULL;
  switch (eo->type) {
    case EPG_BRAND:
      return epg_brand_serialize((epg_brand_t*)eo);
    case EPG_SEASON:
      return epg_season_serialize((epg_season_t*)eo);
    case EPG_EPISODE:
      return epg_episode_serialize((epg_episode_t*)eo);
    case EPG_BROADCAST:
      return epg_broadcast_serialize((epg_broadcast_t*)eo);
    case EPG_SERIESLINK:
      return epg_serieslink_serialize((epg_serieslink_t*)eo);
    default:
      return NULL;
  }
}

epg_object_t *epg_object_deserialize ( htsmsg_t *msg, int create, int *save )
{
  uint32_t type;
  if (!msg) return NULL;
  type = htsmsg_get_u32_or_default(msg, "type", 0);
  switch (type) {
    case EPG_BRAND:
      return (epg_object_t*)epg_brand_deserialize(msg, create, save);
    case EPG_SEASON:
      return (epg_object_t*)epg_season_deserialize(msg, create, save);
    case EPG_EPISODE:
      return (epg_object_t*)epg_episode_deserialize(msg, create, save);
    case EPG_BROADCAST:
      return (epg_object_t*)epg_broadcast_deserialize(msg, create, save);
    case EPG_SERIESLINK:
      return (epg_object_t*)epg_serieslink_deserialize(msg, create, save);
  }
  return NULL;
}

/* **************************************************************************
 * Brand
 * *************************************************************************/

static void _epg_brand_destroy ( void *eo )
{
  epg_brand_t *eb = (epg_brand_t*)eo;
  if (LIST_FIRST(&eb->seasons)) {
    tvhlog(LOG_CRIT, "epg", "attempt to destroy brand with seasons");
    assert(0);
  }
  if (LIST_FIRST(&eb->episodes)) {
    tvhlog(LOG_CRIT, "epg", "attempt to destroy brand with episodes");
    assert(0);
  }
  if (eb->title)   lang_str_destroy(eb->title);
  if (eb->summary) lang_str_destroy(eb->summary);
  if (eb->image)   free(eb->image);
  _epg_object_destroy(eo, &epg_brands);
  free(eb);
}

static void _epg_brand_updated ( void *o )
{
  dvr_autorec_check_brand((epg_brand_t*)o);
}

static epg_object_t **_epg_brand_skel ( void )
{
  static epg_object_t *skel = NULL;
  if ( !skel ) {
    skel = calloc(1, sizeof(epg_brand_t));
    skel->type    = EPG_BRAND;
    skel->destroy = _epg_brand_destroy;
    skel->update  = _epg_brand_updated;
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

epg_brand_t *epg_brand_find_by_id ( uint32_t id )
{
  return (epg_brand_t*)epg_object_find_by_id(id, EPG_BRAND);
}

int epg_brand_set_title
  ( epg_brand_t *brand, const char *title, const char *lang,
    epggrab_module_t *src )
{
  if (!brand || !title || !*title) return 0;
  return _epg_object_set_lang_str(brand, &brand->title, title, lang, src);
}

int epg_brand_set_summary
  ( epg_brand_t *brand, const char *summary, const char *lang,
    epggrab_module_t *src )
{
  if (!brand || !summary || !*summary) return 0;
  return _epg_object_set_lang_str(brand, &brand->summary, summary, lang, src);
}

int epg_brand_set_image
  ( epg_brand_t *brand, const char *image, epggrab_module_t *src )
{
  int save;
  if (!brand || !image) return 0;
  save = _epg_object_set_str(brand, &brand->image, image, src);
  if (save)
    imagecache_get_id(image);
  return save;
}

int epg_brand_set_season_count
  ( epg_brand_t *brand, uint16_t count, epggrab_module_t *src )
{
  if (!brand || !count) return 0;
  return _epg_object_set_u16(brand, &brand->season_count, count, src);
}

static void _epg_brand_add_season 
  ( epg_brand_t *brand, epg_season_t *season )
{
  _epg_object_getref(brand);
  _epg_object_set_updated(brand);
  LIST_INSERT_SORTED(&brand->seasons, season, blink, _season_order);
}

static void _epg_brand_rem_season
  ( epg_brand_t *brand, epg_season_t *season )
{
  LIST_REMOVE(season, blink);
  _epg_object_set_updated(brand);
  _epg_object_putref(brand);
}

static void _epg_brand_add_episode
  ( epg_brand_t *brand, epg_episode_t *episode )
{
  _epg_object_getref(brand);
  _epg_object_set_updated(brand);
  LIST_INSERT_SORTED(&brand->episodes, episode, blink, _episode_order);
}

static void _epg_brand_rem_episode
  ( epg_brand_t *brand, epg_episode_t *episode )
{
  LIST_REMOVE(episode, blink);
  _epg_object_set_updated(brand);
  _epg_object_putref(brand);
}

htsmsg_t *epg_brand_serialize ( epg_brand_t *brand )
{
  htsmsg_t *m;
  if ( !brand || !brand->uri ) return NULL;
  if ( !(m = _epg_object_serialize(brand)) ) return NULL;
  if (brand->title)
    lang_str_serialize(brand->title, m, "title");
  if (brand->summary)
    lang_str_serialize(brand->summary, m, "summary");
  if (brand->season_count)
    htsmsg_add_u32(m, "season-count", brand->season_count);
  if (brand->image)
    htsmsg_add_str(m, "image", brand->image);
  return m;
}

epg_brand_t *epg_brand_deserialize ( htsmsg_t *m, int create, int *save )
{
  epg_object_t **skel = _epg_brand_skel();
  epg_brand_t *eb;
  uint32_t u32;
  const char *str;
  lang_str_t *ls;
  lang_str_ele_t *e;

  if ( !_epg_object_deserialize(m, *skel) ) return NULL;
  if ( !(eb = epg_brand_find_by_uri((*skel)->uri, create, save)) ) return NULL;
  
  if ((ls = lang_str_deserialize(m, "title"))) {
    RB_FOREACH(e, ls, link)
      *save |= epg_brand_set_title(eb, e->str, e->lang, NULL);
    lang_str_destroy(ls);
  }
  if ((ls = lang_str_deserialize(m, "summary"))) {
    RB_FOREACH(e, ls, link)
      *save |= epg_brand_set_summary(eb, e->str, e->lang, NULL);
    lang_str_destroy(ls);
  }
  if ( !htsmsg_get_u32(m, "season-count", &u32) )
    *save |= epg_brand_set_season_count(eb, u32, NULL);

  if ( (str = htsmsg_get_str(m, "image")) )
    *save |= epg_brand_set_image(eb, str, NULL);

  return eb;
}

htsmsg_t *epg_brand_list ( void )
{
  epg_object_t *eo;
  htsmsg_t *a, *e;
  a = htsmsg_create_list();
  RB_FOREACH(eo, &epg_brands, uri_link) {
    assert(eo->type == EPG_BRAND);
    e = epg_brand_serialize((epg_brand_t*)eo);
    htsmsg_add_msg(a, NULL, e);
  }
  return a;
}

const char *epg_brand_get_title ( const epg_brand_t *b, const char *lang )
{
  if (!b || !b->title) return NULL;
  return lang_str_get(b->title, lang);
}

const char *epg_brand_get_summary ( const epg_brand_t *b, const char *lang )
{
  if (!b || !b->summary) return NULL;
  return lang_str_get(b->summary, lang);
}

/* **************************************************************************
 * Season
 * *************************************************************************/

static void _epg_season_destroy ( void *eo )
{
  epg_season_t *es = (epg_season_t*)eo;
  if (LIST_FIRST(&es->episodes)) {
    tvhlog(LOG_CRIT, "epg", "attempt to destory season with episodes");
    assert(0);
  }
  if (es->brand)   _epg_brand_rem_season(es->brand, es);
  if (es->summary) lang_str_destroy(es->summary);
  if (es->image)   free(es->image);
  _epg_object_destroy(eo, &epg_seasons);
  free(es);
}

static void _epg_season_updated ( void *eo )
{
  dvr_autorec_check_season((epg_season_t*)eo);
}

static epg_object_t **_epg_season_skel ( void )
{
  static epg_object_t *skel = NULL;
  if ( !skel ) {
    skel = calloc(1, sizeof(epg_season_t));
    skel->type    = EPG_SEASON;
    skel->destroy = _epg_season_destroy;
    skel->update  = _epg_season_updated;
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

epg_season_t *epg_season_find_by_id ( uint32_t id )
{
  return (epg_season_t*)epg_object_find_by_id(id, EPG_SEASON);
}

int epg_season_set_summary
  ( epg_season_t *season, const char *summary, const char *lang,
    epggrab_module_t *src )
{
  if (!season || !summary || !*summary) return 0;
  return _epg_object_set_lang_str(season, &season->summary, summary, lang, src);
}

int epg_season_set_image
  ( epg_season_t *season, const char *image, epggrab_module_t *src )
{
  int save;
  if (!season || !image) return 0;
  save = _epg_object_set_str(season, &season->image, image, src);
  if (save)
    imagecache_get_id(image);
  return save;
}

int epg_season_set_episode_count
  ( epg_season_t *season, uint16_t count, epggrab_module_t *src )
{
  if (!season || !count) return 0;
  return _epg_object_set_u16(season, &season->episode_count, count, src);
}

int epg_season_set_number
  ( epg_season_t *season, uint16_t number, epggrab_module_t *src )
{
  if (!season || !number) return 0;
  return _epg_object_set_u16(season, &season->number, number, src);
}

int epg_season_set_brand
  ( epg_season_t *season, epg_brand_t *brand, epggrab_module_t *src )
{
  int save = 0;
  if ( !season || !brand ) return 0;
  if ( !_epg_object_set_grabber(season, src) && season->brand ) return 0;
  if ( season->brand != brand ) {
    if ( season->brand ) _epg_brand_rem_season(season->brand, season);
    season->brand = brand;
    _epg_brand_add_season(brand, season);
    _epg_object_set_updated(season);
    save = 1;
  }
  return save;
}

static void _epg_season_add_episode
  ( epg_season_t *season, epg_episode_t *episode )
{
  _epg_object_getref(season);
  _epg_object_set_updated(season);
  LIST_INSERT_SORTED(&season->episodes, episode, slink, _episode_order);
}

static void _epg_season_rem_episode
  ( epg_season_t *season, epg_episode_t *episode )
{
  LIST_REMOVE(episode, slink);
  _epg_object_set_updated(season);
  _epg_object_putref(season);
}

htsmsg_t *epg_season_serialize ( epg_season_t *season )
{
  htsmsg_t *m;
  if (!season || !season->uri) return NULL;
  if (!(m = _epg_object_serialize((epg_object_t*)season))) return NULL;
  if (season->summary)
    lang_str_serialize(season->summary, m, "summary");
  if (season->number)
    htsmsg_add_u32(m, "number", season->number);
  if (season->episode_count)
    htsmsg_add_u32(m, "episode-count", season->episode_count);
  if (season->brand)
    htsmsg_add_str(m, "brand", season->brand->uri);
  if (season->image)
    htsmsg_add_str(m, "image", season->image);
  return m;
}

epg_season_t *epg_season_deserialize ( htsmsg_t *m, int create, int *save )
{
  epg_object_t **skel = _epg_season_skel();
  epg_season_t *es;
  epg_brand_t *eb;
  uint32_t u32;
  const char *str;
  lang_str_t *ls;
  lang_str_ele_t *e;

  if ( !_epg_object_deserialize(m, *skel) ) return NULL;
  if ( !(es = epg_season_find_by_uri((*skel)->uri, create, save)) ) return NULL;
  
  if ((ls = lang_str_deserialize(m, "summary"))) {
    RB_FOREACH(e, ls, link) {
      *save |= epg_season_set_summary(es, e->str, e->lang, NULL);
    }
    lang_str_destroy(ls);
  }
  if ( !htsmsg_get_u32(m, "number", &u32) )
    *save |= epg_season_set_number(es, u32, NULL);
  if ( !htsmsg_get_u32(m, "episode-count", &u32) )
    *save |= epg_season_set_episode_count(es, u32, NULL);
  
  if ( (str = htsmsg_get_str(m, "brand")) )
    if ( (eb = epg_brand_find_by_uri(str, 0, NULL)) )
      *save |= epg_season_set_brand(es, eb, NULL);

  if ( (str = htsmsg_get_str(m, "image")) )
    *save |= epg_season_set_image(es, str, NULL);

  return es;
}

const char *epg_season_get_summary
  ( const epg_season_t *s, const char *lang )
{
  if (!s || !s->summary) return NULL;
  return lang_str_get(s->summary, lang);
}

/* **************************************************************************
 * Episode
 * *************************************************************************/

static htsmsg_t *epg_episode_num_serialize ( epg_episode_num_t *num )
{
  htsmsg_t *m;
  if (!num) return NULL;
  m = htsmsg_create_map();
  if (num->e_num)
    htsmsg_add_u32(m, "e_num", num->e_num);
  if (num->e_cnt)
    htsmsg_add_u32(m, "e_cnt", num->e_cnt);
  if (num->s_num)
    htsmsg_add_u32(m, "s_num", num->s_num);
  if (num->s_cnt)
    htsmsg_add_u32(m, "s_cnt", num->s_cnt);
  if (num->p_num)
    htsmsg_add_u32(m, "p_num", num->p_num);
  if (num->p_cnt)
    htsmsg_add_u32(m, "p_cnt", num->p_cnt);
  if (num->text)
    htsmsg_add_str(m, "text", num->text);
  return m;
}

static void epg_episode_num_deserialize 
  ( htsmsg_t *m, epg_episode_num_t *num )
{
  const char *str;
  uint32_t u32;
  assert(m && num);

  memset(num, 0, sizeof(epg_episode_num_t));

  if (!htsmsg_get_u32(m, "e_num", &u32))
    num->e_num = u32;
  if (!htsmsg_get_u32(m, "e_cnt", &u32))
    num->e_cnt = u32;
  if (!htsmsg_get_u32(m, "s_num", &u32))
    num->s_num = u32;
  if (!htsmsg_get_u32(m, "s_cnt", &u32))
    num->s_cnt = u32;
  if (!htsmsg_get_u32(m, "p_num", &u32))
    num->p_num = u32;
  if (!htsmsg_get_u32(m, "p_cnt", &u32))
    num->p_cnt = u32;
  if ((str = htsmsg_get_str(m, "text")))
    num->text = strdup(str);
}

static void _epg_episode_destroy ( void *eo )
{
  epg_genre_t *g;
  epg_episode_t *ee = eo;
  if (LIST_FIRST(&ee->broadcasts)) {
    tvhlog(LOG_CRIT, "epg", "attempt to destroy episode with broadcasts");
    assert(0);
  }
  if (ee->brand)       _epg_brand_rem_episode(ee->brand, ee);
  if (ee->season)      _epg_season_rem_episode(ee->season, ee);
  if (ee->title)       lang_str_destroy(ee->title);
  if (ee->subtitle)    lang_str_destroy(ee->subtitle);
  if (ee->summary)     lang_str_destroy(ee->summary);
  if (ee->description) lang_str_destroy(ee->description);
  while ((g = LIST_FIRST(&ee->genre))) {
    LIST_REMOVE(g, link);
    free(g);
  }
  if (ee->image)       free(ee->image);
  if (ee->epnum.text)  free(ee->epnum.text);
  _epg_object_destroy(eo, &epg_episodes);
  free(ee);
}

static void _epg_episode_updated ( void *eo )
{
}

static epg_object_t **_epg_episode_skel ( void )
{
  static epg_object_t *skel = NULL;
  if ( !skel ) {
    skel = calloc(1, sizeof(epg_episode_t));
    skel->type    = EPG_EPISODE;
    skel->destroy = _epg_episode_destroy;
    skel->update  = _epg_episode_updated;
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

epg_episode_t *epg_episode_find_by_id ( uint32_t id )
{
  return (epg_episode_t*)epg_object_find_by_id(id, EPG_EPISODE);
}

int epg_episode_set_title
  ( epg_episode_t *episode, const char *title, const char *lang,
    epggrab_module_t *src )
{
  if (!episode) return 0;
  return _epg_object_set_lang_str(episode, &episode->title, title, lang, src);
}

int epg_episode_set_title2
  ( epg_episode_t *episode, const lang_str_t *str, epggrab_module_t *src )
{
  if (!episode || !str) return 0;
  return _epg_object_set_lang_str2(episode, &episode->title, str, src);
}

int epg_episode_set_subtitle
  ( epg_episode_t *episode, const char *subtitle, const char *lang,
    epggrab_module_t *src )
{
  if (!episode || !subtitle || !*subtitle) return 0;
  return _epg_object_set_lang_str(episode, &episode->subtitle,
                                  subtitle, lang, src);
}

int epg_episode_set_subtitle2
  ( epg_episode_t *episode, const lang_str_t *str, epggrab_module_t *src )
{
  if (!episode || !str) return 0;
  return _epg_object_set_lang_str2(episode, &episode->subtitle, str, src);
}

int epg_episode_set_summary
  ( epg_episode_t *episode, const char *summary, const char *lang,
    epggrab_module_t *src )
{
  if (!episode || !summary || !*summary) return 0;
  return _epg_object_set_lang_str(episode, &episode->summary,
                                  summary, lang, src);
}

int epg_episode_set_description
  ( epg_episode_t *episode, const char *desc, const char *lang,
    epggrab_module_t *src )
{
  if (!episode || !desc || !*desc) return 0;
  return _epg_object_set_lang_str(episode, &episode->description,
                                  desc, lang, src);
}

int epg_episode_set_image
  ( epg_episode_t *episode, const char *image, epggrab_module_t *src )
{
  int save;
  if (!episode || !image) return 0;
  save = _epg_object_set_str(episode, &episode->image, image, src);
  if (save)
    imagecache_get_id(image);
  return save;
}

int epg_episode_set_number
  ( epg_episode_t *episode, uint16_t number, epggrab_module_t *src )
{
  if (!episode || !number) return 0;
  return _epg_object_set_u16(episode, &episode->epnum.e_num, number, src);
}

int epg_episode_set_part
  ( epg_episode_t *episode, uint16_t part, uint16_t count,
    epggrab_module_t *src )
{
  int save = 0;
  if (!episode || !part) return 0;
  save |= _epg_object_set_u16(episode, &episode->epnum.p_num, part, src);
  save |= _epg_object_set_u16(episode, &episode->epnum.p_cnt, count, src);
  return save;
}

int epg_episode_set_epnum
  ( epg_episode_t *episode, epg_episode_num_t *num, epggrab_module_t *src )
{
  int save = 0;
  if (!episode || !num || (!num->e_num && !num->text)) return 0;
  if (num->s_num)
    save |= _epg_object_set_u16(episode, &episode->epnum.s_num,
                                num->s_num, src);
  if (num->s_cnt)
    save |= _epg_object_set_u16(episode, &episode->epnum.s_cnt,
                                num->s_cnt, src);
  if (num->e_num)
    save |= _epg_object_set_u16(episode, &episode->epnum.e_num,
                                num->e_num, src);
  if (num->e_cnt)
    save |= _epg_object_set_u16(episode, &episode->epnum.e_cnt,
                                num->e_cnt, src);
  if (num->p_num)
    save |= _epg_object_set_u16(episode, &episode->epnum.p_num,
                                num->p_num, src);
  if (num->p_cnt)
    save |= _epg_object_set_u16(episode, &episode->epnum.p_cnt,
                                num->p_cnt, src);
  if (num->text)
    save |= _epg_object_set_str(episode, &episode->epnum.text,
                                num->text, src);
  return save;
}

int epg_episode_set_brand
  ( epg_episode_t *episode, epg_brand_t *brand, epggrab_module_t *src )
{
  int save = 0;
  if ( !episode || !brand ) return 0;
  if ( !_epg_object_set_grabber(episode, src) && episode->brand ) return 0;
  if ( episode->brand != brand ) {
    if ( episode->brand ) _epg_brand_rem_episode(episode->brand, episode);
    episode->brand = brand;
    _epg_brand_add_episode(brand, episode);
    _epg_object_set_updated(episode);
    save = 1;
  }
  return save;
}

int epg_episode_set_season 
  ( epg_episode_t *episode, epg_season_t *season, epggrab_module_t *src )
{
  int save = 0;
  if ( !episode || !season ) return 0;
  if ( !_epg_object_set_grabber(episode, src) && episode->season ) return 0;
  if ( episode->season != season ) {
    if ( episode->season ) _epg_season_rem_episode(episode->season, episode);
    episode->season = season;
    _epg_season_add_episode(season, episode);
    if ( season->brand )
      save |= epg_episode_set_brand(episode, season->brand, src);
    _epg_object_set_updated(episode);
    save = 1;
  }
  return save;
}

int epg_episode_set_genre
  ( epg_episode_t *ee, epg_genre_list_t *genre, epggrab_module_t *src )
{
  int save = 0;
  epg_genre_t *g1, *g2;

  g1 = LIST_FIRST(&ee->genre);
  if (!_epg_object_set_grabber(ee, src) && g1) return 0;

  /* Remove old */
  while (g1) {
    g2 = LIST_NEXT(g1, link);
    if (!epg_genre_list_contains(genre, g1, 0)) {
      LIST_REMOVE(g1, link);
      free(g1);
      save = 1;
    }
    g1 = g2;
  }
  
  /* Insert all entries */
  LIST_FOREACH(g1, genre, link) {
    save |= epg_genre_list_add(&ee->genre, g1);
  }

  return save;
}

int epg_episode_set_is_bw
  ( epg_episode_t *episode, uint8_t bw, epggrab_module_t *src )
{
  if (!episode) return 0;
  return _epg_object_set_u8(episode, &episode->is_bw, bw, src);
}

int epg_episode_set_star_rating
  ( epg_episode_t *episode, uint8_t stars, epggrab_module_t *src )
{
  if (!episode) return 0;
  return _epg_object_set_u8(episode, &episode->star_rating, stars, src);
}

int epg_episode_set_age_rating
  ( epg_episode_t *episode, uint8_t age, epggrab_module_t *src )
{
  if (!episode) return 0;
  return _epg_object_set_u8(episode, &episode->age_rating, age, src);
}

int epg_episode_set_first_aired
  ( epg_episode_t *episode, time_t aired, epggrab_module_t *src )
{
  int save = 0;
  if (!episode) return 0;
  if ( !_epg_object_set_grabber(episode, src) && episode->first_aired ) 
    return 0;
  if ( episode->first_aired != aired ) {
    episode->first_aired = aired;
    _epg_object_set_updated(episode);
    save = 1;
  }
  return save;
}

static void _epg_episode_add_broadcast 
  ( epg_episode_t *episode, epg_broadcast_t *broadcast )
{
  _epg_object_getref(episode);
  _epg_object_set_updated(episode);
  LIST_INSERT_SORTED(&episode->broadcasts, broadcast, ep_link, _ebc_start_cmp);
}

static void _epg_episode_rem_broadcast
  ( epg_episode_t *episode, epg_broadcast_t *broadcast )
{
  LIST_REMOVE(broadcast, ep_link);
  _epg_object_set_updated(episode);
  _epg_object_putref(episode);
}

size_t epg_episode_number_format 
  ( epg_episode_t *episode, char *buf, size_t len,
    const char *pre,  const char *sfmt,
    const char *sep,  const char *efmt,
    const char *cfmt )
{
  size_t i = 0;
  if (!episode || !buf || !len) return 0;
  epg_episode_num_t num;
  epg_episode_get_epnum(episode, &num);
  buf[0] = '\0';
  if ( num.e_num ) {
    if (pre) tvh_strlcatf(buf, len, i, "%s", pre);
    if ( sfmt && num.s_num ) {
      tvh_strlcatf(buf, len, i, sfmt, num.s_num);
      if ( cfmt && num.s_cnt )
        tvh_strlcatf(buf, len, i, cfmt, num.s_cnt);
      if (sep) tvh_strlcatf(buf, len, i, "%s", sep);
    }
    tvh_strlcatf(buf, len, i, efmt, num.e_num);
    if ( cfmt && num.e_cnt )
      tvh_strlcatf(buf, len, i, cfmt, num.e_cnt);
  } else if ( num.text ) {
    if (pre) tvh_strlcatf(buf, len, i, "%s", pre);
    tvh_strlcatf(buf, len, i, "%s", num.text);
  }
  return i;
}

void epg_episode_get_epnum ( epg_episode_t *ee, epg_episode_num_t *num )
{
  if (!ee || !num) return;
  *num = ee->epnum;
  if (ee->season) {
    num->e_cnt = ee->season->episode_count;
    num->s_num = ee->season->number;
  }
  if (ee->brand) {
    num->s_cnt = ee->brand->season_count;
  }
}

int epg_episode_number_cmp ( epg_episode_num_t *a, epg_episode_num_t *b )
{
  if (a->s_num != b->s_num) {
    return a->s_num - b->s_num;
  } else if (a->e_num != b->e_num) {
    return a->e_num - b->e_num;
  } else {
    return a->p_num - b->p_num;
  }
}

// WIBNI: this could do with soem proper matching, maybe some form of
//        fuzzy string match. I did try a few things, but none of them
//        were very reliable.
#if TODO_FUZZY_MATCH
int epg_episode_fuzzy_match
  ( epg_episode_t *episode, const char *uri, const char *title,
    const char *summary, const char *description )
{
  if ( !episode ) return 0;
  if ( uri && episode->uri && !strcmp(episode->uri, uri) ) return 1;
  if ( title && episode->title && (strstr(title, episode->title) || strstr(episode->title, title)) ) return 1;
  return 0;
}
#endif

htsmsg_t *epg_episode_serialize ( epg_episode_t *episode )
{
  epg_genre_t *eg;
  htsmsg_t *m, *a = NULL;
  if (!episode || !episode->uri) return NULL;
  if (!(m = _epg_object_serialize((epg_object_t*)episode))) return NULL;
  if (episode->title)
    lang_str_serialize(episode->title, m, "title");
  if (episode->subtitle)
    lang_str_serialize(episode->subtitle, m, "subtitle");
  if (episode->summary)
    lang_str_serialize(episode->summary, m, "summary");
  if (episode->description)
    lang_str_serialize(episode->description, m, "description");
  htsmsg_add_msg(m, "epnum", epg_episode_num_serialize(&episode->epnum));
  LIST_FOREACH(eg, &episode->genre, link) {
    if (!a) a = htsmsg_create_list();
    htsmsg_add_u32(a, NULL, eg->code);
  }
  if (a) htsmsg_add_msg(m, "genre", a);
  if (episode->brand)
    htsmsg_add_str(m, "brand", episode->brand->uri);
  if (episode->season)
    htsmsg_add_str(m, "season", episode->season->uri);
  if (episode->is_bw)
    htsmsg_add_u32(m, "is_bw", 1);
  if (episode->star_rating)
    htsmsg_add_u32(m, "star_rating", episode->star_rating);
  if (episode->age_rating)
    htsmsg_add_u32(m, "age_rating", episode->age_rating);
  if (episode->first_aired)
    htsmsg_add_s64(m, "first_aired", episode->first_aired);
  if (episode->image)
    htsmsg_add_str(m, "image", episode->image);

  return m;
}

epg_episode_t *epg_episode_deserialize ( htsmsg_t *m, int create, int *save )
{
  epg_object_t **skel = _epg_episode_skel();
  epg_episode_t *ee;
  epg_season_t *es;
  epg_brand_t *eb;
  const char *str;
  epg_episode_num_t num;
  htsmsg_t *sub;
  htsmsg_field_t *f;
  uint32_t u32;
  int64_t s64;
  lang_str_t *ls;
  lang_str_ele_t *e;
  
  if ( !_epg_object_deserialize(m, *skel) ) return NULL;
  if ( !(ee = epg_episode_find_by_uri((*skel)->uri, create, save)) )
    return NULL;
  
  if ((ls = lang_str_deserialize(m, "title"))) {
    RB_FOREACH(e, ls, link)
      *save |= epg_episode_set_title(ee, e->str, e->lang, NULL);
    lang_str_destroy(ls);
  }
  if ((ls = lang_str_deserialize(m, "subtitle"))) {
    RB_FOREACH(e, ls, link)
      *save |= epg_episode_set_subtitle(ee, e->str, e->lang, NULL);
    lang_str_destroy(ls);
  }
  if ((ls = lang_str_deserialize(m, "summary"))) {
    RB_FOREACH(e, ls, link)
      *save |= epg_episode_set_summary(ee, e->str, e->lang, NULL);
    lang_str_destroy(ls);
  }
  if ((ls = lang_str_deserialize(m, "description"))) {
    RB_FOREACH(e, ls, link)
      *save |= epg_episode_set_description(ee, e->str, e->lang, NULL);
    lang_str_destroy(ls);
  }
  if ( (sub = htsmsg_get_map(m, "epnum")) ) {
    epg_episode_num_deserialize(sub, &num);
    *save |= epg_episode_set_epnum(ee, &num, NULL);
    if (num.text) free(num.text);
  }
  if ( (sub = htsmsg_get_list(m, "genre")) ) {
    epg_genre_list_t *egl = calloc(1, sizeof(epg_genre_list_t));
    HTSMSG_FOREACH(f, sub) {
      epg_genre_t genre;
      genre.code = (uint8_t)f->hmf_s64;
      epg_genre_list_add(egl, &genre);
    }
    *save |= epg_episode_set_genre(ee, egl, NULL);
    epg_genre_list_destroy(egl);
  }
  
  if ( (str = htsmsg_get_str(m, "season")) )
    if ( (es = epg_season_find_by_uri(str, 0, NULL)) )
      *save |= epg_episode_set_season(ee, es, NULL);
  if ( (str = htsmsg_get_str(m, "brand")) )
    if ( (eb = epg_brand_find_by_uri(str, 0, NULL)) )
      *save |= epg_episode_set_brand(ee, eb, NULL);
  
  if (!htsmsg_get_u32(m, "is_bw", &u32))
    *save |= epg_episode_set_is_bw(ee, u32, NULL);

  if (!htsmsg_get_u32(m, "star_rating", &u32))
    *save |= epg_episode_set_star_rating(ee, u32, NULL);

  if (!htsmsg_get_u32(m, "age_rating", &u32))
    *save |= epg_episode_set_age_rating(ee, u32, NULL);

  if (!htsmsg_get_s64(m, "first_aired", &s64))
    *save |= epg_episode_set_first_aired(ee, (time_t)s64, NULL);

  if ( (str = htsmsg_get_str(m, "image")) )
    *save |= epg_episode_set_image(ee, str, NULL);

  return ee;
}

const char *epg_episode_get_title 
  ( const epg_episode_t *e, const char *lang )
{
  if (!e || !e->title) return NULL;
  return lang_str_get(e->title, lang);
}

const char *epg_episode_get_subtitle 
  ( const epg_episode_t *e, const char *lang )
{
  if (!e || !e->subtitle) return NULL;
  return lang_str_get(e->subtitle, lang);
}

const char *epg_episode_get_summary
  ( const epg_episode_t *e, const char *lang )
{
  if (!e || !e->summary) return NULL;
  return lang_str_get(e->summary, lang);
}

const char *epg_episode_get_description 
  ( const epg_episode_t *e, const char *lang )
{
  if (!e || !e->description) return NULL;
  return lang_str_get(e->description, lang);
}

/* **************************************************************************
 * Series link
 * *************************************************************************/

static void _epg_serieslink_destroy ( void *eo )
{
  epg_serieslink_t *es = (epg_serieslink_t*)eo;
  if (LIST_FIRST(&es->broadcasts)) {
    tvhlog(LOG_CRIT, "epg", "attempt to destory series link with broadcasts");
    assert(0);
  }
  _epg_object_destroy(eo, &epg_serieslinks);
  free(es);
}

static void _epg_serieslink_updated ( void *eo )
{
  dvr_autorec_check_serieslink((epg_serieslink_t*)eo);
}

static epg_object_t **_epg_serieslink_skel ( void )
{
  static epg_object_t *skel = NULL;
  if ( !skel ) {
    skel = calloc(1, sizeof(epg_serieslink_t));
    skel->type    = EPG_SERIESLINK;
    skel->destroy = _epg_serieslink_destroy;
    skel->update  = _epg_serieslink_updated;
  }
  return &skel;
}

epg_serieslink_t* epg_serieslink_find_by_uri
  ( const char *uri, int create, int *save )
{
  return (epg_serieslink_t*)
    _epg_object_find_by_uri(uri, create, save,
                            &epg_serieslinks,
                            _epg_serieslink_skel());
}

epg_serieslink_t *epg_serieslink_find_by_id ( uint32_t id )
{
  return (epg_serieslink_t*)epg_object_find_by_id(id, EPG_SERIESLINK);
}

static void _epg_serieslink_add_broadcast
  ( epg_serieslink_t *esl, epg_broadcast_t *ebc )
{
  _epg_object_getref(esl);
  _epg_object_set_updated(esl);
  LIST_INSERT_HEAD(&esl->broadcasts, ebc, sl_link);
}

static void _epg_serieslink_rem_broadcast
  ( epg_serieslink_t *esl, epg_broadcast_t *ebc )
{
  LIST_REMOVE(ebc, sl_link);
  _epg_object_set_updated(esl);
  _epg_object_putref(esl);
}

htsmsg_t *epg_serieslink_serialize ( epg_serieslink_t *esl )
{
  htsmsg_t *m;
  if (!esl || !esl->uri) return NULL;
  if (!(m = _epg_object_serialize((epg_object_t*)esl))) return NULL;
  return m;
}

epg_serieslink_t *epg_serieslink_deserialize 
  ( htsmsg_t *m, int create, int *save )
{
  epg_object_t **skel = _epg_serieslink_skel();
  epg_serieslink_t *esl;

  if ( !_epg_object_deserialize(m, *skel) ) return NULL;
  if ( !(esl = epg_serieslink_find_by_uri((*skel)->uri, create, save)) ) 
    return NULL;
  
  return esl;
}

/* **************************************************************************
 * Channel
 * *************************************************************************/

static void _epg_channel_rem_broadcast 
  ( channel_t *ch, epg_broadcast_t *ebc, epg_broadcast_t *ebc_new )
{
  RB_REMOVE(&ch->ch_epg_schedule, ebc, sched_link);
  if (ch->ch_epg_now  == ebc) ch->ch_epg_now  = NULL;
  if (ch->ch_epg_next == ebc) ch->ch_epg_next = NULL;
  if (ebc_new) {
    dvr_event_replaced(ebc, ebc_new);
  } else {
    dvr_event_removed(ebc);
  }
  _epg_object_putref(ebc);
}

static void _epg_channel_timer_callback ( void *p )
{
  time_t next = 0;
  epg_broadcast_t *ebc, *cur, *nxt;
  channel_t *ch = (channel_t*)p;

  /* Clear now/next */
  if ((cur = ch->ch_epg_now)) {
    if (cur->running) {
      /* running? don't do anything */
      gtimer_arm(&ch->ch_epg_timer, _epg_channel_timer_callback, ch, 2);
      return;
    }
    cur->getref(cur);
  }
  if ((nxt = ch->ch_epg_next))
    nxt->getref(nxt);
  ch->ch_epg_now = ch->ch_epg_next = NULL;

  /* Check events */
  while ( (ebc = RB_FIRST(&ch->ch_epg_schedule)) ) {

    /* Expire */
    if ( ebc->stop <= dispatch_clock ) {
      tvhlog(LOG_DEBUG, "epg", "expire event %u (%s) from %s",
             ebc->id, epg_broadcast_get_title(ebc, NULL),
             channel_get_name(ch));
      _epg_channel_rem_broadcast(ch, ebc, NULL);
      continue; // skip to next

    /* No now */
    } else if ( ebc->start > dispatch_clock ) {
      ch->ch_epg_next = ebc;
      next            = ebc->start;

    /* Now/Next */
    } else {
      ch->ch_epg_now  = ebc;
      ch->ch_epg_next = RB_NEXT(ebc, sched_link);
      next            = ebc->stop;
    }
    break;
  }
  
  /* Change (update HTSP) */
  if (cur != ch->ch_epg_now || nxt != ch->ch_epg_next) {
    tvhlog(LOG_DEBUG, "epg", "now/next %u/%u set on %s",
           ch->ch_epg_now  ? ch->ch_epg_now->id : 0,
           ch->ch_epg_next ? ch->ch_epg_next->id : 0,
           channel_get_name(ch));
    tvhlog(LOG_DEBUG, "epg", "inform HTSP of now event change on %s",
           channel_get_name(ch));
    htsp_channel_update_nownext(ch);
  }

  /* re-arm */
  if ( next ) {
    tvhlog(LOG_DEBUG, "epg", "arm channel timer @ %"PRItime_t" for %s",
           next, channel_get_name(ch));
    gtimer_arm_abs(&ch->ch_epg_timer, _epg_channel_timer_callback, ch, next);
  }

  /* Remove refs */
  if (cur) cur->putref(cur);
  if (nxt) nxt->putref(nxt);
}

static epg_broadcast_t *_epg_channel_add_broadcast 
  ( channel_t *ch, epg_broadcast_t **bcast, int create, int *save )
{
  int timer = 0;
  epg_broadcast_t *ebc, *ret;

  /* Set channel */
  (*bcast)->channel = ch;

  /* Find (only) */
  if ( !create ) {
    return RB_FIND(&ch->ch_epg_schedule, *bcast, sched_link, _ebc_start_cmp);

  /* Find/Create */
  } else {
    ret = RB_INSERT_SORTED(&ch->ch_epg_schedule, *bcast, sched_link, _ebc_start_cmp);

    /* New */
    if (!ret) {
      *save  = 1;
      ret    = *bcast;
      *bcast = NULL;
      _epg_object_create(ret);
      // Note: sets updated
      _epg_object_getref(ret);
      tvhtrace("epg", "added event %u (%s) on %s @ %"PRItime_t " to %"PRItime_t,
               ret->id, epg_broadcast_get_title(ret, NULL),
               channel_get_name(ch), ret->start, ret->stop);

    /* Existing */
    } else {
      *save |= _epg_object_set_u16(ret, &ret->dvb_eid, (*bcast)->dvb_eid, NULL);

      /* No time change */
      if ( ret->stop == (*bcast)->stop ) {
        return ret;

      /* Extend in time */
      } else {
        ret->stop = (*bcast)->stop;
        _epg_object_set_updated(ret);
        tvhtrace("epg", "updated event %u (%s) on %s @ %"PRItime_t " to %"PRItime_t,
                 ret->id, epg_broadcast_get_title(ret, NULL),
                 channel_get_name(ch), ret->start, ret->stop);
      }
    }
  }
  
  /* Changed */
  *save |= 1;

  /* Remove overlapping (before) */
  while ( (ebc = RB_PREV(ret, sched_link)) != NULL ) {
    if ( ebc->stop <= ret->start ) break;
    tvhtrace("epg", "remove overlap (b) event %u (%s) on %s @ %"PRItime_t " to %"PRItime_t,
             ebc->id, epg_broadcast_get_title(ebc, NULL),
             channel_get_name(ch), ebc->start, ebc->stop);
    _epg_channel_rem_broadcast(ch, ebc, ret);
  }

  /* Remove overlapping (after) */
  while ( (ebc = RB_NEXT(ret, sched_link)) != NULL ) {
    if ( ebc->start >= ret->stop ) break;
    tvhtrace("epg", "remove overlap (a) event %u (%s) on %s @ %"PRItime_t " to %"PRItime_t,
             ebc->id, epg_broadcast_get_title(ebc, NULL),
             channel_get_name(ch), ebc->start, ebc->stop);
    _epg_channel_rem_broadcast(ch, ebc, ret);
  }

  /* Check now/next change */
  if ( RB_FIRST(&ch->ch_epg_schedule) == ret ) {
    timer = 1;
  } else if ( ch->ch_epg_now &&
              RB_NEXT(ch->ch_epg_now, sched_link) == ret ) {
    timer = 1;
  }

  /* Reset timer */
  if (timer) _epg_channel_timer_callback(ch);
  return ret;
}

void epg_channel_unlink ( channel_t *ch )
{
  epg_broadcast_t *ebc;
  while ( (ebc = RB_FIRST(&ch->ch_epg_schedule)) ) {
    _epg_channel_rem_broadcast(ch, ebc, NULL);
  }
  gtimer_disarm(&ch->ch_epg_timer);
}

/* **************************************************************************
 * Broadcast
 * *************************************************************************/

static void _epg_broadcast_destroy ( void *eo )
{
  epg_broadcast_t *ebc = eo;
  char id[16];

  if (ebc->created) {
    htsp_event_delete(ebc);
    snprintf(id, sizeof(id), "%u", ebc->id);
    notify_delayed(id, "epg", "delete");
  }
  if (ebc->episode)     _epg_episode_rem_broadcast(ebc->episode, ebc);
  if (ebc->serieslink)  _epg_serieslink_rem_broadcast(ebc->serieslink, ebc);
  if (ebc->summary)     lang_str_destroy(ebc->summary);
  if (ebc->description) lang_str_destroy(ebc->description);
  _epg_object_destroy(eo, NULL);
  free(ebc);
}

static void _epg_broadcast_updated ( void *eo )
{
  epg_broadcast_t *ebc = eo;
  char id[16];

  if (!epg_in_load)
    snprintf(id, sizeof(id), "%u", ebc->id);
  else
    id[0] = '\0';

  if (ebc->created) {
    htsp_event_update(eo);
    notify_delayed(id, "epg", "update");
  } else {
    htsp_event_add(eo);
    notify_delayed(id, "epg", "create");
  }
  if (ebc->channel) {
    dvr_event_updated(eo);
    dvr_autorec_check_event(eo);
    channel_event_updated(eo);
  }
}

static epg_broadcast_t **_epg_broadcast_skel ( void )
{
  static epg_broadcast_t *skel = NULL;
  if ( !skel ) {
    skel = calloc(1, sizeof(epg_broadcast_t));
    skel->type    = EPG_BROADCAST;
    skel->destroy = _epg_broadcast_destroy;
    skel->update  = _epg_broadcast_updated;
  }
  return &skel;
}

epg_broadcast_t* epg_broadcast_find_by_time 
  ( channel_t *channel, time_t start, time_t stop, uint16_t eid, 
    int create, int *save )
{
  epg_broadcast_t **ebc;
  if ( !channel || !start || !stop ) return NULL;
  if ( stop <= start ) return NULL;
  if ( stop <= dispatch_clock ) return NULL;

  ebc = _epg_broadcast_skel();
  (*ebc)->start   = start;
  (*ebc)->stop    = stop;
  (*ebc)->dvb_eid = eid;

  return _epg_channel_add_broadcast(channel, ebc, create, save);
}

epg_broadcast_t *epg_broadcast_clone
  ( channel_t *channel, epg_broadcast_t *src, int *save )
{
  epg_broadcast_t *ebc;

  if ( !src ) return NULL;
  ebc = epg_broadcast_find_by_time(channel, src->start, src->stop,
                                   src->dvb_eid, 1, save);
  if (ebc) {
    /* Copy metadata */
    *save |= epg_broadcast_set_is_widescreen(ebc, src->is_widescreen, NULL);
    *save |= epg_broadcast_set_is_hd(ebc, src->is_hd, NULL);
    *save |= epg_broadcast_set_lines(ebc, src->lines, NULL);
    *save |= epg_broadcast_set_aspect(ebc, src->aspect, NULL);
    *save |= epg_broadcast_set_is_deafsigned(ebc, src->is_deafsigned, NULL);
    *save |= epg_broadcast_set_is_subtitled(ebc, src->is_subtitled, NULL);
    *save |= epg_broadcast_set_is_audio_desc(ebc, src->is_audio_desc, NULL);
    *save |= epg_broadcast_set_is_new(ebc, src->is_new, NULL);
    *save |= epg_broadcast_set_is_repeat(ebc, src->is_repeat, NULL);
    *save |= epg_broadcast_set_summary2(ebc, src->summary, NULL);
    *save |= epg_broadcast_set_description2(ebc, src->description, NULL);
    *save |= epg_broadcast_set_serieslink(ebc, src->serieslink, NULL);
    *save |= epg_broadcast_set_episode(ebc, src->episode, NULL);
  }
  return ebc;
}

epg_broadcast_t *epg_broadcast_find_by_id ( uint32_t id )
{
  return (epg_broadcast_t*)epg_object_find_by_id(id, EPG_BROADCAST);
}

epg_broadcast_t *epg_broadcast_find_by_eid ( channel_t *ch, uint16_t eid )
{
  epg_broadcast_t *e;
  RB_FOREACH(e, &ch->ch_epg_schedule, sched_link) {
    if (e->dvb_eid == eid) return e;
  }
  return NULL;
}

void epg_broadcast_notify_running
  ( epg_broadcast_t *broadcast, epg_source_t esrc, int running )
{
  channel_t *ch;
  epg_broadcast_t *now;
  int orunning = broadcast->running;

  broadcast->running = !!running;
  ch = broadcast->channel;
  now = ch ? ch->ch_epg_now : NULL;
  if (!running) {
    if (now == broadcast && orunning == broadcast->running)
      broadcast->stop = dispatch_clock - 1;
  } else {
    if (broadcast != now && now) {
      now->running = 0;
      dvr_event_running(now, esrc, 0);
    }
  }
  dvr_event_running(broadcast, esrc, running);
}

int epg_broadcast_set_episode 
  ( epg_broadcast_t *broadcast, epg_episode_t *episode, epggrab_module_t *src )
{
  int save = 0;
  if ( !broadcast || !episode ) return 0;
  if ( !_epg_object_set_grabber(broadcast, src) && broadcast->episode )
    return 0;
  if ( broadcast->episode != episode ) {
    if ( broadcast->episode )
      _epg_episode_rem_broadcast(broadcast->episode, broadcast);
    broadcast->episode = episode;
    _epg_episode_add_broadcast(episode, broadcast);
    _epg_object_set_updated(broadcast);
    save = 1;
  }
  return save;
}

int epg_broadcast_set_serieslink
  ( epg_broadcast_t *ebc, epg_serieslink_t *esl, epggrab_module_t *src )
{
  int save = 0;
  if ( !ebc || !esl ) return 0;
  if ( !_epg_object_set_grabber(ebc, src) && ebc->serieslink ) return 0;
  if ( ebc->serieslink != esl ) {
    if ( ebc->serieslink ) _epg_serieslink_rem_broadcast(ebc->serieslink, ebc);
    ebc->serieslink = esl;
    _epg_serieslink_add_broadcast(esl, ebc);
    save = 1;
  }
  return save;
}

int epg_broadcast_set_is_widescreen
  ( epg_broadcast_t *b, uint8_t ws, epggrab_module_t *src )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->is_widescreen, ws, src);
}

int epg_broadcast_set_is_hd
  ( epg_broadcast_t *b, uint8_t hd, epggrab_module_t *src )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->is_hd, hd, src);
}

int epg_broadcast_set_lines
  ( epg_broadcast_t *b, uint16_t lines, epggrab_module_t *src )
{
  if (!b) return 0;
  return _epg_object_set_u16(b, &b->lines, lines, src);
}

int epg_broadcast_set_aspect
  ( epg_broadcast_t *b, uint16_t aspect, epggrab_module_t *src )
{
  if (!b) return 0;
  return _epg_object_set_u16(b, &b->aspect, aspect, src);
}

int epg_broadcast_set_is_deafsigned
  ( epg_broadcast_t *b, uint8_t ds, epggrab_module_t *src )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->is_deafsigned, ds, src);
}

int epg_broadcast_set_is_subtitled
  ( epg_broadcast_t *b, uint8_t st, epggrab_module_t *src )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->is_subtitled, st, src);
}

int epg_broadcast_set_is_audio_desc
  ( epg_broadcast_t *b, uint8_t ad, epggrab_module_t *src )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->is_audio_desc, ad, src);
}

int epg_broadcast_set_is_new
  ( epg_broadcast_t *b, uint8_t n, epggrab_module_t *src )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->is_new, n, src);
}

int epg_broadcast_set_is_repeat
  ( epg_broadcast_t *b, uint8_t r, epggrab_module_t *src )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->is_repeat, r, src);
}

int epg_broadcast_set_summary
  ( epg_broadcast_t *b, const char *str, const char *lang,
    epggrab_module_t *src )
{
  if (!b) return 0;
  return _epg_object_set_lang_str(b, &b->summary, str, lang, src);
}

int epg_broadcast_set_description
  ( epg_broadcast_t *b, const char *str, const char *lang,
    epggrab_module_t *src )
{
  if (!b) return 0;
  return _epg_object_set_lang_str(b, &b->description, str, lang, src);
}

int epg_broadcast_set_summary2
  ( epg_broadcast_t *b, const lang_str_t *str, epggrab_module_t *src )
{
  if (!b || !str) return 0;
  return _epg_object_set_lang_str2(b, &b->summary, str, src);
}

int epg_broadcast_set_description2
  ( epg_broadcast_t *b, const lang_str_t *str, epggrab_module_t *src )
{
  if (!b || !str) return 0;
  return _epg_object_set_lang_str2(b, &b->description, str, src);
}

epg_broadcast_t *epg_broadcast_get_next ( epg_broadcast_t *broadcast )
{
  if ( !broadcast ) return NULL;
  return RB_NEXT(broadcast, sched_link);
}

epg_episode_t *epg_broadcast_get_episode
  ( epg_broadcast_t *ebc, int create, int *save )
{
  char uri[256];
  epg_episode_t *ee;
  if (!ebc) return NULL;
  if (ebc->episode) return ebc->episode;
  if (!create) return NULL;
  snprintf(uri, sizeof(uri)-1, "tvh://channel-%s/bcast-%u/episode",
           idnode_uuid_as_sstr(&ebc->channel->ch_id), ebc->id);
  if ((ee = epg_episode_find_by_uri(uri, 1, save)))
    *save |= epg_broadcast_set_episode(ebc, ee, ebc->grabber);
  return ee;
}

const char *epg_broadcast_get_title ( epg_broadcast_t *b, const char *lang )
{
  if (!b || !b->episode) return NULL;
  return epg_episode_get_title(b->episode, lang);
}

const char *epg_broadcast_get_subtitle ( epg_broadcast_t *b, const char *lang )
{
  if (!b || !b->episode) return NULL;
  return epg_episode_get_subtitle(b->episode, lang);
}

const char *epg_broadcast_get_summary ( epg_broadcast_t *b, const char *lang )
{
  if (!b || !b->summary) return NULL;
  return lang_str_get(b->summary, lang);
}

const char *epg_broadcast_get_description ( epg_broadcast_t *b, const char *lang )
{
  if (!b || !b->description) return NULL;
  return lang_str_get(b->description, lang);
}

htsmsg_t *epg_broadcast_serialize ( epg_broadcast_t *broadcast )
{
  htsmsg_t *m;
  if (!broadcast) return NULL;
  if (!broadcast->episode || !broadcast->episode->uri) return NULL;
  if (!(m = _epg_object_serialize((epg_object_t*)broadcast))) return NULL;
  htsmsg_add_s64(m, "start", broadcast->start);
  htsmsg_add_s64(m, "stop", broadcast->stop);
  htsmsg_add_str(m, "episode", broadcast->episode->uri);
  if (broadcast->channel)
    htsmsg_add_str(m, "channel", channel_get_suuid(broadcast->channel));
  if (broadcast->dvb_eid)
    htsmsg_add_u32(m, "dvb_eid", broadcast->dvb_eid);
  if (broadcast->is_widescreen)
    htsmsg_add_u32(m, "is_widescreen", 1);
  if (broadcast->is_hd)
    htsmsg_add_u32(m, "is_hd", 1);
  if (broadcast->lines)
    htsmsg_add_u32(m, "lines", broadcast->lines);
  if (broadcast->aspect)
    htsmsg_add_u32(m, "aspect", broadcast->aspect);
  if (broadcast->is_deafsigned)
    htsmsg_add_u32(m, "is_deafsigned", 1);
  if (broadcast->is_subtitled)
    htsmsg_add_u32(m, "is_subtitled", 1);
  if (broadcast->is_audio_desc)
    htsmsg_add_u32(m, "is_audio_desc", 1);
  if (broadcast->is_new)
    htsmsg_add_u32(m, "is_new", 1);
  if (broadcast->is_repeat)
    htsmsg_add_u32(m, "is_repeat", 1);
  if (broadcast->summary)
    lang_str_serialize(broadcast->summary, m, "summary");
  if (broadcast->description)
    lang_str_serialize(broadcast->description, m, "description");
  if (broadcast->serieslink)
    htsmsg_add_str(m, "serieslink", broadcast->serieslink->uri);
  
  return m;
}

epg_broadcast_t *epg_broadcast_deserialize
  ( htsmsg_t *m, int create, int *save )
{
  channel_t *ch = NULL;
  epg_broadcast_t *ebc, **skel = _epg_broadcast_skel();
  epg_episode_t *ee;
  epg_serieslink_t *esl;
  lang_str_t *ls;
  const char *str;
  uint32_t eid, u32;
  int64_t start, stop;

  if ( htsmsg_get_s64(m, "start", &start) ) return NULL;
  if ( htsmsg_get_s64(m, "stop", &stop)   ) return NULL;
  if ( !start || !stop ) return NULL;
  if ( stop <= start ) return NULL;
  if ( stop <= dispatch_clock ) return NULL;
  if ( !(str = htsmsg_get_str(m, "episode")) ) return NULL;
  if ( !(ee  = epg_episode_find_by_uri(str, 0, NULL)) ) return NULL;

  /* Set properties */
  _epg_object_deserialize(m, (epg_object_t*)*skel);
  (*skel)->start   = start;
  (*skel)->stop    = stop;

  /* Get DVB id */
  if ( !htsmsg_get_u32(m, "dvb_eid", &eid) ) {
    (*skel)->dvb_eid = eid;
  }

  /* Get channel */
  if ((str = htsmsg_get_str(m, "channel")))
    ch = channel_find(str);
  if (!ch) return NULL;

  /* Create */
  ebc = _epg_channel_add_broadcast(ch, skel, create, save);
  if (!ebc) return NULL;

  /* Get metadata */
  if (!htsmsg_get_u32(m, "is_widescreen", &u32))
    *save |= epg_broadcast_set_is_widescreen(ebc, u32, NULL);
  if (!htsmsg_get_u32(m, "is_hd", &u32))
    *save |= epg_broadcast_set_is_hd(ebc, u32, NULL);
  if (!htsmsg_get_u32(m, "lines", &u32))
    *save |= epg_broadcast_set_lines(ebc, u32, NULL);
  if (!htsmsg_get_u32(m, "aspect", &u32))
    *save |= epg_broadcast_set_aspect(ebc, u32, NULL);
  if (!htsmsg_get_u32(m, "is_deafsigned", &u32))
    *save |= epg_broadcast_set_is_deafsigned(ebc, u32, NULL);
  if (!htsmsg_get_u32(m, "is_subtitled", &u32))
    *save |= epg_broadcast_set_is_subtitled(ebc, u32, NULL);
  if (!htsmsg_get_u32(m, "is_audio_desc", &u32))
    *save |= epg_broadcast_set_is_audio_desc(ebc, u32, NULL);
  if (!htsmsg_get_u32(m, "is_new", &u32))
    *save |= epg_broadcast_set_is_new(ebc, u32, NULL);
  if (!htsmsg_get_u32(m, "is_repeat", &u32))
    *save |= epg_broadcast_set_is_repeat(ebc, u32, NULL);

  if ((ls = lang_str_deserialize(m, "summary"))) {
    *save |= epg_broadcast_set_summary2(ebc, ls, NULL);
    lang_str_destroy(ls);
  }

  if ((ls = lang_str_deserialize(m, "description"))) {
    *save |= epg_broadcast_set_description2(ebc, ls, NULL);
    lang_str_destroy(ls);
  }

  /* Series link */
  if ((str = htsmsg_get_str(m, "serieslink")))
    if ((esl = epg_serieslink_find_by_uri(str, 1, save)))
      *save |= epg_broadcast_set_serieslink(ebc, esl, NULL);

  /* Set the episode */
  *save |= epg_broadcast_set_episode(ebc, ee, NULL);

  return ebc;
}

/* **************************************************************************
 * Genre
 * *************************************************************************/

// TODO: make this configurable
// FULL(ish) list from EN 300 468, I've excluded the last category
// that relates more to broadcast content than what I call a "genre"
// these will be handled elsewhere as broadcast metadata
#define C_ (const char *[])
static const char **_epg_genre_names[16][16] = {
  { /* 00 */
    C_{ "", NULL  }
  },
  { /* 01 */
    C_{ N_("Movie"), N_("Drama"), NULL },
    C_{ N_("Detective"), N_("Thriller"), NULL },
    C_{ N_("Adventure"), N_("Western"), N_("War"), NULL },
    C_{ N_("Science fiction"), N_("Fantasy"), N_("Horror"), NULL },
    C_{ N_("Comedy"), NULL },
    C_{ N_("Soap"), N_("Melodrama"), N_("Folkloric"), NULL },
    C_{ N_("Romance"), NULL },
    C_{ N_("Serious"), N_("Classical"), N_("Religious"), N_("Historical movie"), N_("Drama"), NULL },
    C_{ N_("Adult movie"), N_("Drama"), NULL },
    C_{ N_("Adult movie"), N_("Drama"), NULL },
    C_{ N_("Adult movie"), N_("Drama"), NULL },
    C_{ N_("Adult movie"), N_("Drama"), NULL },
    C_{ N_("Adult movie"), N_("Drama"), NULL },
    C_{ N_("Adult movie"), N_("Drama"), NULL },
  },
  { /* 02 */
    C_{ N_("News"), N_("Current affairs"), NULL },
    C_{ N_("News"), N_("Weather report"), NULL },
    C_{ N_("News magazine"), NULL },
    C_{ N_("Documentary"), NULL },
    C_{ N_("Discussion"), N_("Interview"), N_("Debate"), NULL },
    C_{ N_("Discussion"), N_("Interview"), N_("Debate"), NULL },
    C_{ N_("Discussion"), N_("Interview"), N_("Debate"), NULL },
    C_{ N_("Discussion"), N_("Interview"), N_("Debate"), NULL },
    C_{ N_("Discussion"), N_("Interview"), N_("Debate"), NULL },
    C_{ N_("Discussion"), N_("Interview"), N_("Debate"), NULL },
    C_{ N_("Discussion"), N_("Interview"), N_("Debate"), NULL },
    C_{ N_("Discussion"), N_("Interview"), N_("Debate"), NULL },
    C_{ N_("Discussion"), N_("Interview"), N_("Debate"), NULL },
    C_{ N_("Discussion"), N_("Interview"), N_("Debate"), NULL },
  },
  { /* 03 */
    C_{ N_("Show"), N_("Game show"), NULL },
    C_{ N_("Game show"), N_("Quiz"), N_("Contest"), NULL },
    C_{ N_("Variety show"), NULL },
    C_{ N_("Talk show"), NULL },
    C_{ N_("Talk show"), NULL },
    C_{ N_("Talk show"), NULL },
    C_{ N_("Talk show"), NULL },
    C_{ N_("Talk show"), NULL },
    C_{ N_("Talk show"), NULL },
    C_{ N_("Talk show"), NULL },
    C_{ N_("Talk show"), NULL },
    C_{ N_("Talk show"), NULL },
    C_{ N_("Talk show"), NULL },
    C_{ N_("Talk show"), NULL },
  },
  { /* 04 */
    C_{ N_("Sports"), NULL },
    C_{ N_("Special events (Olympic Games, World Cup, etc.)"), NULL },
    C_{ N_("Sports magazines"), NULL },
    C_{ N_("Football"), N_("Soccer"), NULL },
    C_{ N_("Tennis"), N_("Squash"), NULL },
    C_{ N_("Team sports (excluding football)"), NULL },
    C_{ N_("Athletics"), NULL },
    C_{ N_("Motor sport"), NULL },
    C_{ N_("Water sport"), NULL },
  },
  { /* 05 */
    C_{ N_("Children's / Youth programs"), NULL },
    C_{ N_("Pre-school children's programs"), NULL },
    C_{ N_("Entertainment programs for 6 to 14"), NULL },
    C_{ N_("Entertainment programs for 10 to 16"), NULL },
    C_{ N_("Informational"), N_("Educational"), N_("School programs"), NULL },
    C_{ N_("Cartoons"), N_("Puppets"), NULL },
    C_{ N_("Cartoons"), N_("Puppets"), NULL },
    C_{ N_("Cartoons"), N_("Puppets"), NULL },
    C_{ N_("Cartoons"), N_("Puppets"), NULL },
    C_{ N_("Cartoons"), N_("Puppets"), NULL },
    C_{ N_("Cartoons"), N_("Puppets"), NULL },
    C_{ N_("Cartoons"), N_("Puppets"), NULL },
    C_{ N_("Cartoons"), N_("Puppets"), NULL },
    C_{ N_("Cartoons"), N_("Puppets"), NULL },
  },
  { /* 06 */
    C_{ N_("Music"), N_("Ballet"), N_("Dance"), NULL },
    C_{ N_("Rock"), N_("Pop"), NULL },
    C_{ N_("Serious music"), N_("Classical music"), NULL },
    C_{ N_("Folk"), N_("Traditional music"), NULL },
    C_{ N_("Jazz"), NULL },
    C_{ N_("Musical"), N_("Opera"), NULL },
    C_{ N_("Musical"), N_("Opera"), NULL },
    C_{ N_("Musical"), N_("Opera"), NULL },
    C_{ N_("Musical"), N_("Opera"), NULL },
    C_{ N_("Musical"), N_("Opera"), NULL },
    C_{ N_("Musical"), N_("Opera"), NULL },
    C_{ N_("Musical"), N_("Opera"), NULL },
    C_{ N_("Musical"), N_("Opera"), NULL },
  },
  { /* 07 */
    C_{ N_("Arts"), N_("Culture (without music)"), NULL },
    C_{ N_("Performing arts"), NULL },
    C_{ N_("Fine arts"), NULL },
    C_{ N_("Religion"), NULL },
    C_{ N_("Popular culture"), N_("Traditional arts"), NULL },
    C_{ N_("Literature"), NULL },
    C_{ N_("Film"), N_("Cinema"), NULL },
    C_{ N_("Experimental film"), N_("Video"), NULL },
    C_{ N_("Broadcasting"), N_("Press"), NULL },
  },
  { /* 08 */
    C_{ N_("Social"), N_("Political issues"), N_("Economics"), NULL },
    C_{ N_("Magazines"), N_("Reports"), N_("Documentary"), NULL },
    C_{ N_("Economics"), N_("Social advisory"), NULL },
    C_{ N_("Remarkable people"), NULL },
    C_{ N_("Remarkable people"), NULL },
    C_{ N_("Remarkable people"), NULL },
    C_{ N_("Remarkable people"), NULL },
    C_{ N_("Remarkable people"), NULL },
    C_{ N_("Remarkable people"), NULL },
    C_{ N_("Remarkable people"), NULL },
    C_{ N_("Remarkable people"), NULL },
    C_{ N_("Remarkable people"), NULL },
    C_{ N_("Remarkable people"), NULL },
    C_{ N_("Remarkable people"), NULL },
  },
  { /* 09 */
    C_{ N_("Education"), N_("Science"), N_("Factual topics"), NULL },
    C_{ N_("Nature"), N_("Animals"), N_("Environment"), NULL },
    C_{ N_("Technology"), N_("Natural sciences"), NULL },
    C_{ N_("Medicine"), N_("Physiology"), N_("Psychology"), NULL },
    C_{ N_("Foreign countries"), N_("Expeditions"), NULL },
    C_{ N_("Social"), N_("Spiritual sciences"), NULL },
    C_{ N_("Further education"), NULL },
    C_{ N_("Languages"), NULL },
    C_{ N_("Languages"), NULL },
    C_{ N_("Languages"), NULL },
    C_{ N_("Languages"), NULL },
    C_{ N_("Languages"), NULL },
    C_{ N_("Languages"), NULL },
    C_{ N_("Languages"), NULL },
  },
  { /* 10 */
    C_{ N_("Leisure hobbies"), NULL },
    C_{ N_("Tourism / Travel"), NULL },
    C_{ N_("Handicraft"), NULL },
    C_{ N_("Motoring"), NULL },
    C_{ N_("Fitness and health"), NULL },
    C_{ N_("Cooking"), NULL },
    C_{ N_("Advertisement / Shopping"), NULL },
    C_{ N_("Gardening"), NULL },
    C_{ N_("Gardening"), NULL },
    C_{ N_("Gardening"), NULL },
    C_{ N_("Gardening"), NULL },
    C_{ N_("Gardening"), NULL },
    C_{ N_("Gardening"), NULL },
    C_{ N_("Gardening"), NULL },
  }
};

static const char *_genre_get_name(int a, int b, const char *lang)
{
  static char __thread name[64];
  size_t l = 0;
  const char **p = _epg_genre_names[a][b];
  name[0] = '\0';
  if (p == NULL)
    return NULL;
  for ( ; *p; p++)
    tvh_strlcatf(name, sizeof(name), l, "%s%s", l ? " / " : "",
                 lang ? tvh_gettext_lang(lang, *p) : *p);
  return name;
}

// match strings, ignoring case and whitespace
// Note: | 0x20 is a cheats (fast) way of lowering case
static int _genre_str_match ( const char *a, const char *b )
{
  int i = 0, j = 0;
  if (!a || !b) return 0;
  while (a[i] != '\0' || b[j] != '\0') {
    while (a[i] == ' ') i++;
    while (b[j] == ' ') j++;
    if ((a[i] | 0x20) != (b[j] | 0x20)) return 0;
    i++; j++;
  }
  return (a[i] == '\0' && b[j] == '\0'); // end of string(both)
}

static uint8_t _epg_genre_find_by_name ( const char *name, const char *lang )
{
  uint8_t a, b;
  const char *s;
  for ( a = 1; a < 11; a++ ) {
    for ( b = 0; b < 16; b++ ) {
      s = _genre_get_name(a, b, lang);
      if (_genre_str_match(name, s))
        return (a << 4) | b;
    }
  }
  return 0; // undefined
}

uint8_t epg_genre_get_eit ( const epg_genre_t *genre )
{
  if (!genre) return 0;
  return genre->code;
}

size_t epg_genre_get_str ( const epg_genre_t *genre, int major_only,
                           int major_prefix, char *buf, size_t len,
                           const char *lang )
{
  const char *s;
  int maj, min;
  size_t ret = 0;
  if (!genre || !buf) return 0;
  maj = (genre->code >> 4) & 0xf;
  s = _genre_get_name(maj, 0, lang);
  if (s[0] == '\0') return 0;
  min = major_only ? 0 : (genre->code & 0xf);
  if (!min || major_prefix ) {
    tvh_strlcatf(buf, len, ret, "%s", s);
  }
  if (min) {
    s = _genre_get_name(maj, min, lang);
    if (s[0]) {
      tvh_strlcatf(buf, len, ret, " : ");
      tvh_strlcatf(buf, len, ret, "%s", s);
    }
  }
  return ret;
}

int epg_genre_list_add ( epg_genre_list_t *list, epg_genre_t *genre )
{
  epg_genre_t *g1, *g2;
  if (!list || !genre || !genre->code) return 0;
  g1 = LIST_FIRST(list);
  if (!g1) {
    g2 = calloc(1, sizeof(epg_genre_t));
    g2->code = genre->code;
    LIST_INSERT_HEAD(list, g2, link);
  } else {
    while (g1) {
    
      /* Already exists */
      if (g1->code == genre->code) return 0;

      /* Update a major only entry */
      if (g1->code == (genre->code & 0xF0)) {
        g1->code = genre->code;
        break;
      }

      /* Insert before */
      if (g1->code > genre->code) {
        g2 = calloc(1, sizeof(epg_genre_t));
        g2->code = genre->code;
        LIST_INSERT_BEFORE(g1, g2, link);
        break;
      }

      /* Insert after (end) */
      if (!(g2 = LIST_NEXT(g1, link))) {
        g2 = calloc(1, sizeof(epg_genre_t));
        g2->code = genre->code;
        LIST_INSERT_AFTER(g1, g2, link);
        break;
      }

      /* Next */
      g1 = g2;
    }
  }
  return 1;
}

int epg_genre_list_add_by_eit ( epg_genre_list_t *list, uint8_t eit )
{
  epg_genre_t g;
  g.code = eit;
  return epg_genre_list_add(list, &g);
}

int epg_genre_list_add_by_str ( epg_genre_list_t *list, const char *str, const char *lang )
{
  epg_genre_t g;
  g.code = _epg_genre_find_by_name(str, lang);
  return epg_genre_list_add(list, &g);
}

// Note: if partial=1 and genre is a major only category then all minor
// entries will also match
int epg_genre_list_contains 
  ( epg_genre_list_t *list, epg_genre_t *genre, int partial )
{
  uint8_t mask = 0xFF;
  epg_genre_t *g;
  if (!list || !genre) return 0;
  if (partial && !(genre->code & 0x0F)) mask = 0xF0;
  LIST_FOREACH(g, list, link) {
    if ((g->code & mask) == genre->code) break;
  }
  return g ? 1 : 0;
}

void epg_genre_list_destroy ( epg_genre_list_t *list )
{
  epg_genre_t *g;
  while ((g = LIST_FIRST(list))) {
    LIST_REMOVE(g, link);
    free(g);
  }
  free(list);
}

htsmsg_t *epg_genres_list_all ( int major_only, int major_prefix, const char *lang )
{
  int i, j;
  htsmsg_t *e, *m;
  const char *s;
  m = htsmsg_create_list();
  for (i = 0; i < 16; i++ ) {
    for (j = 0; j < (major_only ? 1 : 16); j++) {
      if (_epg_genre_names[i][j]) {
        e = htsmsg_create_map();
        htsmsg_add_u32(e, "key", (i << 4) | (major_only ? 0 : j));
        s = _genre_get_name(i, j, lang);
        htsmsg_add_str(e, "val", s);
        // TODO: use major_prefix
        htsmsg_add_msg(m, NULL, e);
      }
    }
  }
  return m;
}

/* **************************************************************************
 * Querying
 * *************************************************************************/

static inline int
_eq_comp_num ( epg_filter_num_t *f, int64_t val )
{
  switch (f->comp) {
    case EC_EQ: return val != f->val1;
    case EC_LT: return val > f->val1;
    case EC_GT: return val < f->val1;
    case EC_RG: return val < f->val1 || val > f->val2;
    default: return 0;
  }
}

static inline int
_eq_comp_str ( epg_filter_str_t *f, const char *str )
{
  switch (f->comp) {
    case EC_EQ: return strcmp(str, f->str);
    case EC_LT: return strcmp(str, f->str) > 0;
    case EC_GT: return strcmp(str, f->str) < 0;
    case EC_IN: return strstr(str, f->str) != NULL;
    case EC_RE: return regexec(&f->re, str, 0, NULL, 0) != 0;
    default: return 0;
  }
}

static void
_eq_add ( epg_query_t *eq, epg_broadcast_t *e )
{
  const char *s, *lang = eq->lang;
  epg_episode_t *ep;
  int fulltext = eq->stitle && eq->fulltext;

  /* Filtering */
  if (e == NULL) return;
  if (e->stop < dispatch_clock) return;
  if (_eq_comp_num(&eq->start, e->start)) return;
  if (_eq_comp_num(&eq->stop, e->stop)) return;
  if (eq->duration.comp != EC_NO) {
    int64_t duration = (int64_t)e->stop - (int64_t)e->start;
    if (_eq_comp_num(&eq->duration, duration)) return;
  }
  ep = e->episode;
  if (eq->stars.comp != EC_NO)
    if (_eq_comp_num(&eq->stars, ep->star_rating)) return;
  if (eq->age.comp != EC_NO)
    if (_eq_comp_num(&eq->age, ep->age_rating)) return;
  if (eq->channel_num.comp != EC_NO)
    if (_eq_comp_num(&eq->channel_num, channel_get_number(e->channel))) return;
  if (eq->channel_name.comp != EC_NO)
    if (_eq_comp_str(&eq->channel_name, channel_get_name(e->channel))) return;
  if (eq->genre_count) {
    epg_genre_t genre;
    uint32_t i, r = 0;
    for (i = 0; i < eq->genre_count; i++) {
      genre.code = eq->genre[i];
      if (genre.code == 0) continue;
      if (epg_genre_list_contains(&e->episode->genre, &genre, 1)) r++;
    }
    if (!r) return;
  }
  if (fulltext) {
    if ((s = epg_episode_get_title(ep, lang)) == NULL ||
        regexec(&eq->stitle_re, s, 0, NULL, 0)) {
      if ((s = epg_episode_get_subtitle(ep, lang)) == NULL ||
          regexec(&eq->stitle_re, s, 0, NULL, 0)) {
        if ((s = epg_broadcast_get_summary(e, lang)) == NULL ||
            regexec(&eq->stitle_re, s, 0, NULL, 0)) {
          if ((s = epg_broadcast_get_description(e, lang)) == NULL ||
              regexec(&eq->stitle_re, s, 0, NULL, 0)) {
            return;
          }
        }
      }
    }
  }
  if (eq->title.comp != EC_NO || (eq->stitle && !fulltext)) {
    if ((s = epg_episode_get_title(ep, lang)) == NULL) return;
    if (eq->stitle && !fulltext && regexec(&eq->stitle_re, s, 0, NULL, 0)) return;
    if (eq->title.comp != EC_NO && _eq_comp_str(&eq->title, s)) return;
  }
  if (eq->subtitle.comp != EC_NO) {
    if ((s = epg_episode_get_subtitle(ep, lang)) == NULL) return;
    if (_eq_comp_str(&eq->subtitle, s)) return;
  }
  if (eq->summary.comp != EC_NO) {
    if ((s = epg_broadcast_get_summary(e, lang)) == NULL) return;
    if (_eq_comp_str(&eq->summary, s)) return;
  }
  if (eq->description.comp != EC_NO) {
    if ((s = epg_broadcast_get_description(e, lang)) == NULL) return;
    if (_eq_comp_str(&eq->description, s)) return;
  }

  /* More space */
  if (eq->entries == eq->allocated) {
    eq->allocated = MAX(100, eq->allocated + 100);
    eq->result    = realloc(eq->result, eq->allocated * sizeof(epg_broadcast_t *));
  }

  /* Store */
  eq->result[eq->entries++] = e;
}

static void
_eq_add_channel ( epg_query_t *eq, channel_t *ch )
{
  epg_broadcast_t *ebc;
  RB_FOREACH(ebc, &ch->ch_epg_schedule, sched_link) {
    if (ebc->episode)
      _eq_add(eq, ebc);
  }
}

static int
_eq_init_str( epg_filter_str_t *f )
{
  if (f->comp != EC_RE) return 0;
  return regcomp(&f->re, f->str, REG_ICASE | REG_EXTENDED | REG_NOSUB);
}

static void
_eq_done_str( epg_filter_str_t *f )
{
  if (f->comp == EC_RE)
    regfree(&f->re);
  free(f->str);
  f->str = NULL;
}

static int _epg_sort_start_ascending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)a)->start - (*(epg_broadcast_t**)b)->start;
}

static int _epg_sort_start_descending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)b)->start - (*(epg_broadcast_t**)a)->start;
}

static int _epg_sort_stop_ascending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)a)->stop - (*(epg_broadcast_t**)b)->stop;
}

static int _epg_sort_stop_descending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)b)->stop - (*(epg_broadcast_t**)a)->stop;
}

static inline int64_t _epg_sort_duration( const epg_broadcast_t *b )
{
  return b->stop - b->start;
}

static int _epg_sort_duration_ascending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_duration(*(epg_broadcast_t**)a) - _epg_sort_duration(*(epg_broadcast_t**)b);
}

static int _epg_sort_duration_descending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_duration(*(epg_broadcast_t**)b) - _epg_sort_duration(*(epg_broadcast_t**)a);
}

static int _epg_sort_title_ascending ( const void *a, const void *b, void *eq )
{
  const char *s1 = epg_broadcast_get_title(*(epg_broadcast_t**)a, ((epg_query_t *)eq)->lang);
  const char *s2 = epg_broadcast_get_title(*(epg_broadcast_t**)b, ((epg_query_t *)eq)->lang);
  if (s1 == NULL && s2 == NULL) return 0;
  if (s1 == NULL && s2) return 1;
  if (s1 && s2 == NULL) return -1;
  return strcmp(s1, s2);
}

static int _epg_sort_title_descending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_title_ascending(a, b, eq) * -1;
}

static int _epg_sort_subtitle_ascending ( const void *a, const void *b, void *eq )
{
  const char *s1 = epg_broadcast_get_subtitle(*(epg_broadcast_t**)a, ((epg_query_t *)eq)->lang);
  const char *s2 = epg_broadcast_get_subtitle(*(epg_broadcast_t**)b, ((epg_query_t *)eq)->lang);
  if (s1 == NULL && s2 == NULL) return 0;
  if (s1 == NULL && s2) return 1;
  if (s1 && s2 == NULL) return -1;
  return strcmp(s1, s2);
}

static int _epg_sort_subtitle_descending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_subtitle_ascending(a, b, eq) * -1;
}

static int _epg_sort_summary_ascending ( const void *a, const void *b, void *eq )
{
  const char *s1 = epg_broadcast_get_summary(*(epg_broadcast_t**)a, ((epg_query_t *)eq)->lang);
  const char *s2 = epg_broadcast_get_summary(*(epg_broadcast_t**)b, ((epg_query_t *)eq)->lang);
  if (s1 == NULL && s2 == NULL) return 0;
  if (s1 == NULL && s2) return 1;
  if (s1 && s2 == NULL) return -1;
  return strcmp(s1, s2);
}

static int _epg_sort_summary_descending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_summary_ascending(a, b, eq) * -1;
}

static int _epg_sort_description_ascending ( const void *a, const void *b, void *eq )
{
  const char *s1 = epg_broadcast_get_description(*(epg_broadcast_t**)a, ((epg_query_t *)eq)->lang);
  const char *s2 = epg_broadcast_get_description(*(epg_broadcast_t**)b, ((epg_query_t *)eq)->lang);
  if (s1 == NULL && s2 == NULL) return 0;
  if (s1 == NULL && s2) return 1;
  if (s1 && s2 == NULL) return -1;
  return strcmp(s1, s2);
}

static int _epg_sort_description_descending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_description_ascending(a, b, eq) * -1;
}

static int _epg_sort_channel_ascending ( const void *a, const void *b, void *eq )
{
  char *s1 = strdup(channel_get_name((*(epg_broadcast_t**)a)->channel));
  char *s2 = strdup(channel_get_name((*(epg_broadcast_t**)b)->channel));
  int r = strcmp(s1, s2);
  free(s2);
  free(s1);
  return r;
}

static int _epg_sort_channel_descending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_description_ascending(a, b, eq) * -1;
}

static int _epg_sort_channel_num_ascending ( const void *a, const void *b, void *eq )
{
  int64_t v1 = channel_get_number((*(epg_broadcast_t**)a)->channel);
  int64_t v2 = channel_get_number((*(epg_broadcast_t**)b)->channel);
  return v1 - v2;
}

static int _epg_sort_channel_num_descending ( const void *a, const void *b, void *eq )
{
  int64_t v1 = channel_get_number((*(epg_broadcast_t**)a)->channel);
  int64_t v2 = channel_get_number((*(epg_broadcast_t**)b)->channel);
  return v2 - v1;
}

static int _epg_sort_stars_ascending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)a)->episode->star_rating - (*(epg_broadcast_t**)b)->episode->star_rating;
}

static int _epg_sort_stars_descending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)b)->episode->star_rating - (*(epg_broadcast_t**)a)->episode->star_rating;
}

static int _epg_sort_age_ascending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)a)->episode->age_rating - (*(epg_broadcast_t**)b)->episode->age_rating;
}

static int _epg_sort_age_descending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)b)->episode->age_rating - (*(epg_broadcast_t**)a)->episode->age_rating;
}

static uint64_t _epg_sort_genre_hash( epg_episode_t *ep )
{
  uint64_t h = 0, t;
  epg_genre_t *g;

  LIST_FOREACH(g, &ep->genre, link) {
    t = h >> 28;
    h <<= 8;
    h += (uint64_t)g->code + t;
  }
  return h;
}

static int _epg_sort_genre_ascending ( const void *a, const void *b, void *eq )
{
  uint64_t v1 = _epg_sort_genre_hash((*(epg_broadcast_t**)a)->episode);
  uint64_t v2 = _epg_sort_genre_hash((*(epg_broadcast_t**)b)->episode);
  return v1 - v2;
}

static int _epg_sort_genre_descending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_genre_ascending(a, b, eq) * -1;
}

epg_broadcast_t **
epg_query ( epg_query_t *eq, access_t *perm )
{
  channel_t *channel;
  channel_tag_t *tag;
  int (*fcn)(const void *, const void *, void *) = NULL;

  /* Setup exp */
  if (_eq_init_str(&eq->title)) goto fin;
  if (_eq_init_str(&eq->subtitle)) goto fin;
  if (_eq_init_str(&eq->summary)) goto fin;
  if (_eq_init_str(&eq->description)) goto fin;
  if (_eq_init_str(&eq->channel_name)) goto fin;

  if (eq->stitle)
    if (regcomp(&eq->stitle_re, eq->stitle, REG_ICASE | REG_EXTENDED | REG_NOSUB))
      goto fin;

  channel = channel_find_by_uuid(eq->channel) ?:
            channel_find_by_name(eq->channel);

  tag = channel_tag_find_by_uuid(eq->channel_tag) ?:
        channel_tag_find_by_name(eq->channel_tag, 0);

  /* Single channel */
  if (channel && tag == NULL) {
    if (channel_access(channel, perm, 0))
      _eq_add_channel(eq, channel);
  
  /* Tag based */
  } else if (tag) {
    idnode_list_mapping_t *ilm;
    channel_t *ch2;
    LIST_FOREACH(ilm, &tag->ct_ctms, ilm_in1_link) {
      ch2 = (channel_t *)ilm->ilm_in2;
      if(ch2 == channel || channel == NULL)
        if (channel_access(ch2, perm, 0))
          _eq_add_channel(eq, ch2);
    }

  /* All channels */
  } else {
    CHANNEL_FOREACH(channel)
      if (channel_access(channel, perm, 0))
        _eq_add_channel(eq, channel);
  }

  switch (eq->sort_dir) {
  case ES_ASC:
    switch (eq->sort_key) {
    case ESK_START:       fcn = _epg_sort_start_ascending;        break;
    case ESK_STOP:        fcn = _epg_sort_stop_ascending;         break;
    case ESK_DURATION:    fcn = _epg_sort_duration_ascending;     break;
    case ESK_TITLE:       fcn = _epg_sort_title_ascending;        break;
    case ESK_SUBTITLE:    fcn = _epg_sort_subtitle_ascending;     break;
    case ESK_SUMMARY:     fcn = _epg_sort_summary_ascending;      break;
    case ESK_DESCRIPTION: fcn = _epg_sort_description_ascending;  break;
    case ESK_CHANNEL:     fcn = _epg_sort_channel_ascending;      break;
    case ESK_CHANNEL_NUM: fcn = _epg_sort_channel_num_ascending;  break;
    case ESK_STARS:       fcn = _epg_sort_stars_ascending;        break;
    case ESK_AGE:         fcn = _epg_sort_age_ascending;          break;
    case ESK_GENRE:       fcn = _epg_sort_genre_ascending;        break;
    }
    break;
  case ES_DSC:
    switch (eq->sort_key) {
    case ESK_START:       fcn = _epg_sort_start_descending;       break;
    case ESK_STOP:        fcn = _epg_sort_stop_descending;        break;
    case ESK_DURATION:    fcn = _epg_sort_duration_descending;    break;
    case ESK_TITLE:       fcn = _epg_sort_title_descending;       break;
    case ESK_SUBTITLE:    fcn = _epg_sort_subtitle_descending;    break;
    case ESK_SUMMARY:     fcn = _epg_sort_summary_descending;     break;
    case ESK_DESCRIPTION: fcn = _epg_sort_description_descending; break;
    case ESK_CHANNEL:     fcn = _epg_sort_channel_descending;     break;
    case ESK_CHANNEL_NUM: fcn = _epg_sort_channel_num_descending; break;
    case ESK_STARS:       fcn = _epg_sort_stars_descending;       break;
    case ESK_AGE:         fcn = _epg_sort_age_descending;         break;
    case ESK_GENRE:       fcn = _epg_sort_genre_descending;       break;
    }
    break;
  }

  tvh_qsort_r(eq->result, eq->entries, sizeof(epg_broadcast_t *), fcn, eq);

fin:
  _eq_done_str(&eq->title);
  _eq_done_str(&eq->subtitle);
  _eq_done_str(&eq->summary);
  _eq_done_str(&eq->description);
  _eq_done_str(&eq->channel_name);

  if (eq->stitle)
    regfree(&eq->stitle_re);

  free(eq->lang); eq->lang = NULL;
  free(eq->channel); eq->channel = NULL;
  free(eq->channel_tag); eq->channel_tag = NULL;
  free(eq->stitle); eq->stitle = NULL;
  if (eq->genre != eq->genre_static)
    free(eq->genre);
  eq->genre = NULL;

  return eq->result;
}

void epg_query_free(epg_query_t *eq)
{
  free(eq->result); eq->result = NULL;
}


/* **************************************************************************
 * Miscellaneous
 * *************************************************************************/

htsmsg_t *epg_config_serialize( void )
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "last_id", _epg_object_idx);
  return m;
}

int epg_config_deserialize( htsmsg_t *m )
{
  if (htsmsg_get_u32(m, "last_id", &_epg_object_idx))
    return 0;
  return 1; /* ok */
}

void epg_skel_done(void)
{
  epg_object_t **skel;
  epg_broadcast_t **broad;

  skel = _epg_brand_skel();
  free(*skel); *skel = NULL;
  skel = _epg_season_skel();
  free(*skel); *skel = NULL;
  skel = _epg_episode_skel();
  free(*skel); *skel = NULL;
  skel = _epg_serieslink_skel();
  free(*skel); *skel = NULL;
  broad = _epg_broadcast_skel();
  free(*broad); *broad = NULL;
}
