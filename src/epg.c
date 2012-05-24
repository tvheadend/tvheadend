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

/* Element lists */
struct epg_object_tree epg_brands;
struct epg_object_tree epg_seasons;
struct epg_object_tree epg_episodes;
struct epg_object_tree epg_channels;
struct epg_object_tree epg_broadcasts;

/* Unlinked channels */
LIST_HEAD(epg_unlinked_channel_list1, epg_channel);
LIST_HEAD(epg_unlinked_channel_list2, channel);
struct epg_unlinked_channel_list1 epg_unlinked_channels1;
struct epg_unlinked_channel_list2 epg_unlinked_channels2;

/* Global counters */
static uint64_t _epg_object_idx    = 0;

/* **************************************************************************
 * Comparators
 * *************************************************************************/

static int _uri_cmp ( const void *a, const void *b )
{
  return strcmp(((epg_object_t*)a)->uri, ((epg_object_t*)b)->uri);
}

#if TODO_MIGHT_NEED_LATER
static int _id_cmp ( const void *a, const void *b )
{
  return ((epg_object_t*)a)->id - ((epg_object_t*)b)->id;
}
#endif

static int _ptr_cmp ( const void *a, const void *b )
{
  return a - b;
}

static int _ebc_win_cmp ( const void *a, const void *b )
{
  if ( ((epg_broadcast_t*)a)->start < ((epg_broadcast_t*)b)->start ) return -1;
  if ( ((epg_broadcast_t*)a)->start >= ((epg_broadcast_t*)b)->stop  ) return 1;
  return 0;
}

static int _epg_channel_match ( epg_channel_t *ec, channel_t *ch )
{
  int ret = 0;
  if ( ec->name && !strcmp(ec->name, ch->ch_name) ) ret = 1;
  return ret;
}

/* **************************************************************************
 * Testing/Debug
 * *************************************************************************/

static void _epg_dump ( void )
{
  epg_object_t  *eo;
  epg_brand_t   *eb;
  epg_season_t  *es;
  epg_episode_t *ee;
  epg_channel_t *ec;
  epg_broadcast_t *ebc;
  printf("dump epg\n");

  /* Go down the brand/season/episode */
  RB_FOREACH(eo, &epg_brands, glink) {
    eb = (epg_brand_t*)eo;
    printf("BRAND: %p %s\n", eb, eb->_.uri);
    RB_FOREACH(es, &eb->seasons, blink) {
      printf("  SEASON: %p %s %d\n", es, es->_.uri, es->number);
      RB_FOREACH(ee, &es->episodes, slink) {
        printf("    EPISODE: %p %s %d\n", ee, ee->_.uri, ee->number);
      }
    }
    RB_FOREACH(ee, &eb->episodes, blink) {
      if ( !ee->season ) printf("  EPISODE: %p %s %d\n", ee, ee->_.uri, ee->number);
    }
  }
  RB_FOREACH(eo, &epg_seasons, glink) {
    es = (epg_season_t*)eo;
    if ( !es->brand ) {
      printf("SEASON: %p %s %d\n", es, es->_.uri, es->number);
      RB_FOREACH(ee, &es->episodes, slink) {
        printf("  EPISODE: %p %s %d\n", ee, ee->_.uri, ee->number);
      }
    }
  }
  RB_FOREACH(eo, &epg_episodes, glink) {
    ee = (epg_episode_t*)eo;
    if ( !ee->brand && !ee->season ) {
      printf("EPISODE: %p %s %d\n", ee, ee->_.uri, ee->number);
    }
  }
  RB_FOREACH(eo, &epg_channels, glink) {
    ec = (epg_channel_t*)eo;
    printf("CHANNEL: %s\n", ec->_.uri);
    RB_FOREACH(eo, &ec->schedule, glink) {
      ebc = (epg_broadcast_t*)eo;
      if ( ebc->episode ) {
        printf("  BROADCAST: %s @ %ld to %ld\n", ebc->episode->_.uri,
               ebc->start, ebc->stop);
      }
    }
  }
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
  epg_channel_t *ec;
  
  // TODO: requires markers in the file or some other means of
  //       determining where the various object types are?
  fd = hts_settings_open_file(1, "epgdb");

  /* Channels */
  if ( _epg_write_sect(fd, "channels") ) return;
  RB_FOREACH(eo,  &epg_channels, glink) {
    if (_epg_write(fd, epg_channel_serialize((epg_channel_t*)eo))) return;
  }
  if ( _epg_write_sect(fd, "brands") ) return;
  RB_FOREACH(eo,  &epg_brands, glink) {
    if (_epg_write(fd, epg_brand_serialize((epg_brand_t*)eo))) return;
  }
  if ( _epg_write_sect(fd, "seasons") ) return;
  RB_FOREACH(eo,  &epg_seasons, glink) {
    if (_epg_write(fd, epg_season_serialize((epg_season_t*)eo))) return;
  }
  if ( _epg_write_sect(fd, "episodes") ) return;
  RB_FOREACH(eo,  &epg_episodes, glink) {
    if (_epg_write(fd, epg_episode_serialize((epg_episode_t*)eo))) return;
  }
  if ( _epg_write_sect(fd, "broadcasts") ) return;
  RB_FOREACH(eo, &epg_channels, glink) {
    ec = (epg_channel_t*)eo;
    RB_FOREACH(eo, &ec->schedule, glink) {
      if (_epg_write(fd, epg_broadcast_serialize((epg_broadcast_t*)eo))) return;
    }
  }
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

      /* Channel */
      } else if ( !strcmp(sect, "channels") ) {
        if (epg_channel_deserialize(m, 1, &save)) stats.channels.total++;

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

  /* Stats */
  tvhlog(LOG_DEBUG, "epg", "database loaded");
  tvhlog(LOG_DEBUG, "epg", "channels   %d", stats.channels.total);
  tvhlog(LOG_DEBUG, "epg", "brands     %d", stats.brands.total);
  tvhlog(LOG_DEBUG, "epg", "seasons    %d", stats.seasons.total);
  tvhlog(LOG_DEBUG, "epg", "episodes   %d", stats.episodes.total);
  tvhlog(LOG_DEBUG, "epg", "broadcasts %d", stats.broadcasts.total);

  /* Close file */
  munmap(mem, st.st_size);
  close(fd);
}

void epg_updated ( void )
{
  if (0)_epg_dump();
}

void epg_add_channel ( channel_t *ch )
{
#if TODO_REDO_CHANNEL_LINKING
  epg_channel_t *ec;
  LIST_INSERT_HEAD(&epg_unlinked_channels2, ch, ch_eulink);
  LIST_FOREACH(ec, &epg_unlinked_channels1, ulink) {
    if ( _epg_channel_match(ec, ch) ) {
      epg_channel_set_channel(ec, ch);
      break;
    }
  }
#endif
}

void epg_rem_channel ( channel_t *ch )
{
#if TODO_REDO_CHANNEL_LINKING
  if ( ch->ch_epg_channel ) {
    ch->ch_epg_channel->channel = NULL;
    // TODO: free the channel?
  } else {
    LIST_REMOVE(ch, ch_eulink);
  }
#endif
}

void epg_mod_channel ( channel_t *ch )
{
  if ( !ch->ch_epg_channel ) epg_add_channel(ch);
}


/* **************************************************************************
 * Object
 * *************************************************************************/

static epg_object_t *_epg_object_find
  ( int create, int *save, epg_object_tree_t *tree,
    epg_object_t *skel, int (*cmp) (const void*,const void*))
{
  epg_object_t *eo;

  /* Find */
  if ( !create ) {
    eo = RB_FIND(tree, skel, glink, cmp);
  
  /* Create */
  } else {
    eo = RB_INSERT_SORTED(tree, skel, glink, cmp);
    if ( eo == NULL ) {
      *save     |= 1;
      eo         = skel;
      skel       = NULL;
      _epg_object_idx++;
    }
  }

  return eo;
}

static epg_object_t *_epg_object_find_by_uri 
  ( const char *uri, int create, int *save,
    epg_object_tree_t *tree, size_t size )
{
  int save2 = 0;
  epg_object_t *eo;
  static epg_object_t* skel = NULL;
  
  lock_assert(&global_lock); // pointless!

  if ( skel == NULL ) skel = calloc(1, size);
  skel->uri = (char*)uri;
  skel->id  = _epg_object_idx;
  // TODO: need to add function pointers

  eo = _epg_object_find(create, &save2, tree, skel, _uri_cmp);
  if (save2) eo->uri = strdup(uri);
  *save |= save2;
  return eo;
}

static epg_object_t *_epg_object_find_by_id 
  ( uint64_t id, epg_object_tree_t *tree )
{
  epg_object_t *eo;
  if (!tree) return NULL; // TODO: use a global list?
  RB_FOREACH(eo, tree, glink) {
    if ( eo->id == id ) return eo;
  }
  return NULL;
}

/* **************************************************************************
 * Brand
 * *************************************************************************/

epg_brand_t* epg_brand_find_by_uri 
  ( const char *uri, int create, int *save )
{
  return (epg_brand_t*)
    _epg_object_find_by_uri(uri, create, save,
                            &epg_brands, sizeof(epg_brand_t));
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
  // TODO: could set only if less?
  int save = 0;
  if ( !brand || !count ) return 0;
  if ( brand->season_count != count ) {
    brand->season_count = count;
    save = 1;
  }
  return save;
}

int epg_brand_add_season ( epg_brand_t *brand, epg_season_t *season, int u )
{
  int save = 0;
  epg_season_t *es;
  if ( !brand || !season ) return 0;
  es = RB_INSERT_SORTED(&brand->seasons, season, blink, _uri_cmp);
  if ( es == NULL ) {
    if ( u ) save |= epg_season_set_brand(season, brand, 0);
    save = 1;
  }
  return save;
}

int epg_brand_rem_season ( epg_brand_t *brand, epg_season_t *season, int u )
{
  int save = 0;
  epg_season_t *es;
  if ( !brand || !season ) return 0;
  es = RB_FIND(&brand->seasons, season, blink, _uri_cmp);
  if ( es != NULL ) {
    if ( u ) save |= epg_season_set_brand(season, NULL, 0); // TODO: this will do nothing
    RB_REMOVE(&brand->seasons, season, blink);
    save = 1;
  }
  return save;
}

int epg_brand_add_episode ( epg_brand_t *brand, epg_episode_t *episode, int u )
{
  int save = 0;
  epg_episode_t *ee;
  if ( !brand || !episode ) return 0;
  ee = RB_INSERT_SORTED(&brand->episodes, episode, blink, _uri_cmp);
  if ( ee == NULL ) {
    if ( u ) save |= epg_episode_set_brand(episode, brand, 0);
    save = 1;
  }
  return save;
}

int epg_brand_rem_episode ( epg_brand_t *brand, epg_episode_t *episode, int u )
{
  int save = 0;
  epg_episode_t *ee;
  if ( !brand || !episode ) return 0;
  ee = RB_FIND(&brand->episodes, episode, blink, _uri_cmp);
  if ( ee != NULL ) {
    if ( u ) save |= epg_episode_set_brand(episode, NULL, 0); // TODO: will do nothing
    RB_REMOVE(&brand->episodes, episode, blink);
    save = 1;
  }
  return save;
}

htsmsg_t *epg_brand_serialize ( epg_brand_t *brand )
{
  htsmsg_t *m;
  if ( !brand || !brand->_.uri ) return NULL;
  m = htsmsg_create_map();
  // TODO: generic serialize?
  htsmsg_add_str(m, "uri", brand->_.uri);
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
  epg_brand_t *eb;
  uint32_t u32;
  const char *str;
  
  // TODO: generic deserialize?
  if ( !(str = htsmsg_get_str(m, "uri"))                ) return NULL;
  if ( !(eb = epg_brand_find_by_uri(str, create, save)) ) return NULL;
  
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

epg_season_t* epg_season_find_by_uri 
  ( const char *uri, int create, int *save )
{
  return (epg_season_t*)
    _epg_object_find_by_uri(uri, create, save,
                            &epg_seasons, sizeof(epg_season_t));
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
  // TODO: should we only update if number is larger
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
    if ( u ) save |= epg_brand_rem_season(season->brand, season, 0);
    season->brand = brand;
    if ( u ) save |= epg_brand_add_season(season->brand, season, 0);
    save = 1;
  }
  return save;
}

int epg_season_add_episode 
  ( epg_season_t *season, epg_episode_t *episode, int u )
{
  int save = 0;
  epg_episode_t *ee;
  if ( !season || !episode ) return 0;
  ee = RB_INSERT_SORTED(&season->episodes, episode, slink, _uri_cmp);
  if ( ee == NULL ) {
    if ( u ) save |= epg_episode_set_season(episode, season, 0);
    save = 1;
  }
  // TODO?: else assert(ee == episode)
  return save;
}

int epg_season_rem_episode 
  ( epg_season_t *season, epg_episode_t *episode, int u )
{
  int save = 0;
  epg_episode_t *ee;
  if ( !season || !episode ) return 0;
  ee = RB_FIND(&season->episodes, episode, slink, _uri_cmp);
  if ( ee != NULL ) {
    // TODO?: assert(ee == episode)
    if ( u ) save |= epg_episode_set_season(episode, NULL, 0); // does nothing
    RB_REMOVE(&season->episodes, episode, slink);
    save = 1;
  }
  return save;
}

htsmsg_t *epg_season_serialize ( epg_season_t *season )
{
  htsmsg_t *m;
  if (!season || !season->_.uri) return NULL;
  m = htsmsg_create_map();
  // TODO: generic
  htsmsg_add_str(m, "uri", season->_.uri);
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
  epg_season_t *es;
  epg_brand_t *eb;
  uint32_t u32;
  const char *str;

  // TODO: generic
  if ( !(str = htsmsg_get_str(m, "uri")) )                  return NULL;
  if ( !(es  = epg_season_find_by_uri(str, create, save)) ) return NULL;
  
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

epg_episode_t* epg_episode_find_by_uri
  ( const char *uri, int create, int *save )
{
  return (epg_episode_t*)
    _epg_object_find_by_uri(uri, create, save,
                            &epg_episodes, sizeof(epg_episode_t));
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
  // TODO: could treat part/count independently
  if ( !episode || !part || !count ) return 0;
  if ( episode->part_number != part ) {
    episode->part_number = part;
    save |= 1;
  }
  if ( episode->part_count != count ) {
    episode->part_count = count;
    save |= 1;
  }
  return save;
}

int epg_episode_set_brand ( epg_episode_t *episode, epg_brand_t *brand, int u )
{
  int save = 0;
  if ( !episode || !brand ) return 0;
  if ( episode->brand != brand ) {
    if ( u ) save |= epg_brand_rem_episode(episode->brand, episode, 0); /// does nothing
    episode->brand = brand;
    if ( u ) save |= epg_brand_add_episode(episode->brand, episode, 0);
    save |= 1;
  }
  return save;
}

int epg_episode_set_season ( epg_episode_t *episode, epg_season_t *season, int u )
{
  int save = 0;
  if ( !episode || !season ) return 0;
  if ( episode->season != season ) {
    if ( u ) save |= epg_season_rem_episode(episode->season, episode, 0);
    episode->season = season;
    if ( u && episode->season ) {
      save |= epg_season_add_episode(episode->season, episode, 0);
      save |= epg_brand_add_episode(episode->season->brand, episode, 0);
      // TODO: is this the right place?
    }
    save = 1;
  }
  return save;
}

int epg_episode_add_broadcast 
  ( epg_episode_t *episode, epg_broadcast_t *broadcast, int u )
{
  int save = 0;
  epg_broadcast_t *eb;
  if ( !episode || !broadcast ) return 0;
  eb = RB_INSERT_SORTED(&episode->broadcasts, broadcast, elink, _ptr_cmp);
  if ( eb == NULL ) {
    if ( u ) save |= epg_broadcast_set_episode(broadcast, episode, 0);
    save = 1;
  }
  return save;
}

int epg_episode_rem_broadcast
  ( epg_episode_t *episode, epg_broadcast_t *broadcast, int u )
{
  int save = 0;
  epg_broadcast_t *eb;
  if ( !episode || !broadcast ) return 0;
  eb = RB_FIND(&episode->broadcasts, broadcast, elink, _ptr_cmp);
  if ( eb != NULL ) {
    if ( u ) save |= epg_broadcast_set_episode(broadcast, episode, 0);
    RB_REMOVE(&episode->broadcasts, broadcast, elink);
    save = 1;
  }
  return save;
}

int epg_episode_get_number_onscreen
  ( epg_episode_t *episode, char *buf, int len )
{
  int i = 0;
  if ( episode->number ) {
    // TODO: add counts
    if ( episode->season && episode->season->number ) {
      i += snprintf(&buf[i], len-i, "Season %d ",
                    episode->season->number);
    }
    i += snprintf(&buf[i], len-i, "Episode %d", episode->number);
  }
  return i;
}

htsmsg_t *epg_episode_serialize ( epg_episode_t *episode )
{
  htsmsg_t *m;
  if (!episode || !episode->_.uri) return NULL;
  m = htsmsg_create_map();
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
  epg_episode_t *ee;
  epg_season_t *es;
  epg_brand_t *eb;
  uint32_t u32, u32a;
  const char *str;
  
  if ( !(str = htsmsg_get_str(m, "uri")) )                   return NULL;
  if ( !(ee  = epg_episode_find_by_uri(str, create, save)) ) return NULL;

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
      *save |= epg_episode_set_brand(ee, eb, 1);
  if ( (str = htsmsg_get_str(m, "season")) )
    if ( (es = epg_season_find_by_uri(str, 0, NULL)) )
      *save |= epg_episode_set_season(ee, es, 1);
  
  return ee;
}

/* **************************************************************************
 * Broadcast
 * *************************************************************************/

epg_broadcast_t* epg_broadcast_find_by_time 
  ( epg_channel_t *channel, time_t start, time_t stop, int create, int *save )
{
  static epg_broadcast_t *skel;
  if ( !channel || !start || !stop ) return 0;
  if ( stop < start ) return 0;

  if ( skel == NULL ) skel = calloc(1, sizeof(epg_broadcast_t));
  skel->channel = channel;
  skel->start   = start;
  skel->stop    = stop;
  skel->_.id    = _epg_object_idx; // TODO: prefer if this wasn't here!

  return (epg_broadcast_t*)
    _epg_object_find(create, save, &channel->schedule,
                     (epg_object_t*)skel, _ebc_win_cmp);
}

// TODO: optional channel?
epg_broadcast_t *epg_broadcast_find_by_id ( uint64_t id )
{
  epg_object_t *eo, *ec;
  RB_FOREACH(ec, &epg_channels, glink) {
    eo = _epg_object_find_by_id(id, &((epg_channel_t*)ec)->schedule);
    if (eo) return (epg_broadcast_t*)eo;
  }
  return NULL;
}

int epg_broadcast_set_episode 
  ( epg_broadcast_t *broadcast, epg_episode_t *episode, int u )
{
  int save = 0;
  if ( !broadcast || !episode ) return 0;
  if ( broadcast->episode != episode ) {
    broadcast->episode = episode;
    if ( u ) save |= epg_episode_add_broadcast(episode, broadcast, 0);
    save = 1;
  }
  return save;
}

epg_broadcast_t *epg_broadcast_get_next ( epg_broadcast_t *broadcast )
{
  if ( !broadcast ) return NULL;
  return RB_NEXT(broadcast, slink);
}

htsmsg_t *epg_broadcast_serialize ( epg_broadcast_t *broadcast )
{
  htsmsg_t *m;
  if (!broadcast) return NULL;
  if (!broadcast->channel || !broadcast->channel->_.uri) return NULL;
  if (!broadcast->episode || !broadcast->episode->_.uri) return NULL;
  m = htsmsg_create_map();

  htsmsg_add_u32(m, "id", broadcast->_.id);
  htsmsg_add_u32(m, "start", broadcast->start);
  htsmsg_add_u32(m, "stop", broadcast->stop);
  // TODO: should these be optional?
  htsmsg_add_str(m, "channel", broadcast->channel->_.uri);
  htsmsg_add_str(m, "episode", broadcast->episode->_.uri);
  
  if (broadcast->dvb_id)
    htsmsg_add_u32(m, "dvb-id", broadcast->dvb_id);
  // TODO: add other metadata fields
  return m;
}

epg_broadcast_t *epg_broadcast_deserialize
  ( htsmsg_t *m, int create, int *save )
{
  epg_broadcast_t *ebc;
  epg_channel_t *ec;
  epg_episode_t *ee;
  const char *str;
  uint32_t start, stop;
  uint64_t id;

  if ( htsmsg_get_u64(m, "id", &id)                   ) return NULL;
  if ( htsmsg_get_u32(m, "start", &start)             ) return NULL;
  if ( htsmsg_get_u32(m, "stop", &stop)               ) return NULL;
  // TODO: should these be optional?
  if ( !(str = htsmsg_get_str(m, "channel"))          ) return NULL;
  if ( !(ec  = epg_channel_find_by_uri(str, 0, NULL)) ) return NULL;
  if ( !(str = htsmsg_get_str(m, "episode"))          ) return NULL;
  if ( !(ee  = epg_episode_find_by_uri(str, 0, NULL)) ) return NULL;

  ebc = epg_broadcast_find_by_time(ec, start, stop, create, save);
  if ( !ebc ) return NULL;

  *save |= epg_broadcast_set_episode(ebc, ee, 1);

  /* Bodge the ID - keep them the same */
  ebc->_.id = id;
  if ( id >= _epg_object_idx ) _epg_object_idx = id + 1;

#if TODO_BROADCAST_METADATA
  if ( !htsmsg_get_u32(m, "dvb-id", &u32) )
    save |= epg_broadcast_set_dvb_id(ebc, u32);
  // TODO: more metadata
#endif

  return ebc;
}

/* **************************************************************************
 * Channel
 * *************************************************************************/

epg_channel_t* epg_channel_find_by_uri
  ( const char *uri, int create, int *save )
{
  return (epg_channel_t*)
    _epg_object_find_by_uri(uri, create, save,
                            &epg_channels, sizeof(epg_channel_t));
}

epg_channel_t *epg_channel_find_by_id ( uint64_t id )
{
  return (epg_channel_t*)_epg_object_find_by_id(id, &epg_channels);
}

int epg_channel_set_name ( epg_channel_t *channel, const char *name )
{
  int save = 0;
  channel_t *ch;
  if ( !channel || !name ) return 0;
  if ( !channel->name || strcmp(channel->name, name) ) {
    channel->name = strdup(name);
    if ( !channel->channel ) {
      LIST_FOREACH(ch, &epg_unlinked_channels2, ch_eulink) {
        if ( _epg_channel_match(channel, ch) ) {
          epg_channel_set_channel(channel, ch);
          break;
        }
      }
    }
    // TODO: should be try to override?
    save = 1;
  }
  return save;
}

int epg_channel_set_channel ( epg_channel_t *ec, channel_t *ch )
{
  int save = 0;
#if TODO_REDO_CHANNEL_LINKING
  if ( !ec || !ch ) return 0;
  if ( ec->channel != ch ) {
    if (!ec->channel)     LIST_REMOVE(ec, ulink);
    if (!ch->ch_epg_channel) LIST_REMOVE(ch, ch_eulink);
    // TODO: should it be possible to "change" it
    //if(ec->channel)
    //   channel_set_epg_source(ec->channel, NULL)
    //   LIST_INSERT_HEAD(&epg_unlinked_channels2, ec->channel, ch_eulink);
    ec->channel = ch;
    channel_set_epg_source(ch, ec);
  }
#endif
  return save;
}

epg_broadcast_t *epg_channel_get_current_broadcast ( epg_channel_t *channel )
{
  // TODO: its not really the head!
  if ( !channel ) return NULL;
  return (epg_broadcast_t*)RB_FIRST(&channel->schedule);
}

htsmsg_t *epg_channel_serialize ( epg_channel_t *channel )
{
  htsmsg_t *m;
  if (!channel || !channel->_.uri) return NULL;
  m = htsmsg_create_map();
  htsmsg_add_str(m, "uri", channel->_.uri);
  if (channel->name)
    htsmsg_add_str(m, "name", channel->name);
  if (channel->channel)
    htsmsg_add_u32(m, "channel", channel->channel->ch_id);
  // TODO: other data
  return m;
}

epg_channel_t *epg_channel_deserialize ( htsmsg_t *m, int create, int *save )
{
  epg_channel_t *ec;
  channel_t *ch;
  uint32_t u32;
  const char *str;
  
  if ( !(str = htsmsg_get_str(m, "uri"))                   ) return NULL;
  if ( !(ec  = epg_channel_find_by_uri(str, create, save)) ) return NULL;

  if ( (str = htsmsg_get_str(m, "name")) )
    *save |= epg_channel_set_name(ec, str);

  if ( !htsmsg_get_u32(m, "channel", &u32) )
    if ( (ch = channel_find_by_identifier(u32)) )
      epg_channel_set_channel(ec, ch);
  // TODO: this call needs updating
      
  return ec;
}

/* **************************************************************************
 * Querying
 * *************************************************************************/

static void _eqr_add ( epg_query_result_t *eqr, epg_broadcast_t *e )
{
  /* More space */
  if ( eqr->eqr_entries == eqr->eqr_alloced ) {
    eqr->eqr_alloced = MAX(100, eqr->eqr_alloced * 2);
    eqr->eqr_array   = realloc(eqr->eqr_array, 
                               eqr->eqr_alloced * sizeof(epg_broadcast_t));
  }
  
  /* Store */
  eqr->eqr_array[eqr->eqr_entries++] = e;
  // TODO: ref counting
}

static void _eqr_add_channel 
  ( epg_query_result_t *eqr, epg_channel_t *ec )
{
  epg_object_t *eo;
  epg_broadcast_t *ebc;
  RB_FOREACH(eo, &ec->schedule, glink) {
    ebc = (epg_broadcast_t*)eo;
    if ( ebc->episode && ebc->channel ) _eqr_add(eqr, ebc);
  }
}

void epg_query0
  ( epg_query_result_t *eqr, channel_t *channel, channel_tag_t *tag,
    uint8_t contentgroup, const char *title )
{
  epg_object_t *ec;

  /* Clear (just incase) */
  memset(eqr, 0, sizeof(epg_query_result_t));

  /* All channels */
  if (!channel) {
    RB_FOREACH(ec, &epg_channels, glink) {
      _eqr_add_channel(eqr, (epg_channel_t*)ec);
    }

  /* Single channel */
  } else if ( channel->ch_epg_channel ) {
    _eqr_add_channel(eqr, channel->ch_epg_channel);
  }

  return;
}

void epg_query(epg_query_result_t *eqr, const char *channel, const char *tag,
	       const char *contentgroup, const char *title)
{
  channel_t *ch = NULL;
  if (channel) ch = channel_find_by_name(channel, 0, 0);
  epg_query0(eqr, ch, NULL, 0, title);
}

void epg_query_free(epg_query_result_t *eqr)
{
  free(eqr->eqr_array);
  // TODO: reference counting
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
