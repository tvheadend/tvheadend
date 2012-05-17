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

// TODO:

#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <assert.h>

#include "tvheadend.h"
#include "channels.h"
#include "settings.h"
#include "epg.h"
#include "dvr/dvr.h"
#include "htsp.h"
#include "htsmsg_binary.h"

struct epg_brand_tree   epg_brands;
struct epg_season_tree  epg_seasons;
struct epg_episode_tree epg_episodes;
struct epg_channel_tree epg_channels;

/* **************************************************************************
 * Comparators
 * *************************************************************************/

static int eb_uri_cmp ( const epg_brand_t *a, const epg_brand_t *b )
{
  return strcmp(a->eb_uri, b->eb_uri);
}

static int es_uri_cmp ( const epg_season_t *a, const epg_season_t *b )
{
  return strcmp(a->es_uri, b->es_uri);
}

static int ee_uri_cmp ( const epg_episode_t *a, const epg_episode_t *b )
{
  return strcmp(a->ee_uri, b->ee_uri);
}

static int ec_uri_cmp ( const epg_channel_t *a, const epg_channel_t *b )
{
  return strcmp(a->ec_uri, b->ec_uri);
}

static int ebc_win_cmp ( const epg_broadcast_t *a, const epg_broadcast_t *b )
{
  printf("check a %ld against b %ld to %ld\n", a->eb_start, b->eb_start, b->eb_stop);
// TODO: some le-way?
  if ( a->eb_start < b->eb_start ) return -1;
  if ( a->eb_start >= b->eb_stop  ) return 1;
  return 0;
}

/* **************************************************************************
 * Testing/Debug
 * *************************************************************************/

static void _epg_dump ( void )
{
  epg_brand_t   *eb;
  epg_season_t  *es;
  epg_episode_t *ee;
  epg_channel_t *ec;
  epg_broadcast_t *ebc;
  printf("dump epg\n");

  /* Go down the brand/season/episode */
  RB_FOREACH(eb, &epg_brands, eb_link) {
    printf("BRAND: %p %s\n", eb, eb->eb_uri);
    RB_FOREACH(es, &eb->eb_seasons, es_blink) {
      printf("  SEASON: %p %s\n", es, es->es_uri);
      RB_FOREACH(ee, &es->es_episodes, ee_slink) {
        printf("    EPISODE: %p %s\n", ee, ee->ee_uri);
      }
    }
    RB_FOREACH(ee, &eb->eb_episodes, ee_blink) {
      if ( !ee->ee_season ) printf("  EPISODE: %p %s\n", ee, ee->ee_uri);
    }
  }
  RB_FOREACH(es, &epg_seasons, es_link) {
    if ( !es->es_brand ) {
      printf("SEASON: %p %s\n", es, es->es_uri);
      RB_FOREACH(ee, &es->es_episodes, ee_slink) {
        printf("  EPISODE: %p %s\n", ee, ee->ee_uri);
      }
    }
  }
  RB_FOREACH(ee, &epg_episodes, ee_link) {
    if ( !ee->ee_brand && !ee->ee_season ) {
      printf("EPISODE: %p %s\n", ee, ee->ee_uri);
    }
  }
  RB_FOREACH(ec, &epg_channels, ec_link) {
    printf("CHANNEL: %s\n", ec->ec_uri);
    RB_FOREACH(ebc, &ec->ec_schedule, eb_slink) {
      if ( ebc->eb_episode ) {
        printf("  BROADCAST: %s @ %ld to %ld\n", ebc->eb_episode->ee_uri,
               ebc->eb_start, ebc->eb_stop);
      }
    }
  }
}

/* **************************************************************************
 * Setup / Update
 * *************************************************************************/

void epg_init ( void )
{
}

void epg_save ( void )
{
}

void epg_updated ( void )
{
  _epg_dump();
}


/* **************************************************************************
 * Brand
 * *************************************************************************/

epg_brand_t* epg_brand_find_by_uri ( const char *id, int create )
{
  epg_brand_t *eb;
  static epg_brand_t* skel = NULL;
  
  lock_assert(&global_lock); // pointless!

  if ( skel == NULL ) skel = calloc(1, sizeof(epg_brand_t));
  skel->eb_uri = (char*)id;

  /* Find */
  if ( !create ) {
    eb = RB_FIND(&epg_brands, skel, eb_link, eb_uri_cmp);
  
  /* Create */
  } else {
    eb = RB_INSERT_SORTED(&epg_brands, skel, eb_link, eb_uri_cmp);
    if ( eb == NULL ) {
      eb   = skel;
      skel = NULL;
      eb->eb_uri = strdup(id);
    }
  }

  return eb;
}

int epg_brand_set_title ( epg_brand_t *brand, const char *title )
{
  int save = 0;
  if ( !brand || !title ) return 0;
  if ( !brand->eb_title || strcmp(brand->eb_title, title) ) {
    if ( brand->eb_title ) free(brand->eb_title);
    brand->eb_title = strdup(title);
    save = 1;
  }
  return save;
}

int epg_brand_set_summary ( epg_brand_t *brand, const char *summary )
{
  int save = 0;
  if ( !brand || !summary ) return 0;
  if ( !brand->eb_summary || strcmp(brand->eb_summary, summary) ) {
    if ( brand->eb_summary ) free(brand->eb_summary);
    brand->eb_summary = strdup(summary);
    save = 1;
  }
  return save;
}

int epg_brand_set_season_count ( epg_brand_t *brand, uint16_t count )
{
  // TODO: could set only if less?
  int save = 0;
  if ( !brand || !count ) return 0;
  if ( brand->eb_season_count != count ) {
    brand->eb_season_count = count;
    save = 1;
  }
  return save;
}

int epg_brand_add_season ( epg_brand_t *brand, epg_season_t *season, int u )
{
  int save = 0;
  epg_season_t *es;
  if ( !brand || !season ) return 0;
  es = RB_INSERT_SORTED(&brand->eb_seasons, season, es_blink, es_uri_cmp);
  if ( es == NULL ) {
    if ( u ) save |= epg_season_set_brand(season, brand, 0);
    save = 1;
  }
  // TODO?: else assert(es == season)
  return save;
}

int epg_brand_rem_season ( epg_brand_t *brand, epg_season_t *season, int u )
{
  int save = 0;
  epg_season_t *es;
  if ( !brand || !season ) return 0;
  es = RB_FIND(&brand->eb_seasons, season, es_blink, es_uri_cmp);
  if ( es != NULL ) {
    // TODO: assert(es == season)
    if ( u ) save |= epg_season_set_brand(season, NULL, 0); // TODO: this will do nothing
    RB_REMOVE(&brand->eb_seasons, season, es_blink);
    save = 1;
  }
  return save;
}

int epg_brand_add_episode ( epg_brand_t *brand, epg_episode_t *episode, int u )
{
  int save = 0;
  epg_episode_t *ee;
  if ( !brand || !episode ) return 0;
  ee = RB_INSERT_SORTED(&brand->eb_episodes, episode, ee_blink, ee_uri_cmp);
  if ( ee == NULL ) {
    if ( u ) save |= epg_episode_set_brand(episode, brand, 0);
    save = 1;
  }
  // TODO?: else assert(ee == episode)
  return save;
}

int epg_brand_rem_episode ( epg_brand_t *brand, epg_episode_t *episode, int u )
{
  int save = 0;
  epg_episode_t *ee;
  if ( !brand || !episode ) return 0;
  ee = RB_FIND(&brand->eb_episodes, episode, ee_blink, ee_uri_cmp);
  if ( ee != NULL ) {
    // TODO?: assert(ee == episode)
    if ( u ) save |= epg_episode_set_brand(episode, NULL, 0); // TODO: will do nothing
    RB_REMOVE(&brand->eb_episodes, episode, ee_blink);
    save = 1;
  }
  return save;
}

/* **************************************************************************
 * Season
 * *************************************************************************/

epg_season_t* epg_season_find_by_uri ( const char *id, int create )
{
  epg_season_t *es;
  static epg_season_t* skel = NULL;
  
  lock_assert(&global_lock); // pointless!

  if ( skel == NULL ) skel = calloc(1, sizeof(epg_season_t));
  skel->es_uri = (char*)id;

  /* Find */
  if ( !create ) {
    es = RB_FIND(&epg_seasons, skel, es_link, es_uri_cmp);
  
  /* Create */
  } else {
    es = RB_INSERT_SORTED(&epg_seasons, skel, es_link, es_uri_cmp);
    if ( es == NULL ) {
      es   = skel;
      skel = NULL;
      es->es_uri = strdup(id);
    }
  }

  return es;
}

int epg_season_set_summary ( epg_season_t *season, const char *summary )
{
  int save = 0;
  if ( !season || !summary ) return 0;
  if ( !season->es_summary || strcmp(season->es_summary, summary) ) {
    if ( season->es_summary ) free(season->es_summary);
    season->es_summary = strdup(summary);
    save = 1;
  }
  return save;
}

int epg_season_set_episode_count ( epg_season_t *season, uint16_t count )
{
  int save = 0;
  if ( !season || !count ) return 0;
  // TODO: should we only update if number is larger
  if ( season->es_episode_count != count ) {
    season->es_episode_count = count;
    save = 1;
  }
  return save;
}

int epg_season_set_number ( epg_season_t *season, uint16_t number )
{
  int save = 0;
  if ( !season || !number ) return 0;
  if ( season->es_number != number ) {
    season->es_number = number;
    save = 1;
  }
  return save;
}

int epg_season_set_brand ( epg_season_t *season, epg_brand_t *brand, int u )
{
  int save = 0;
  if ( !season || !brand ) return 0;
  if ( season->es_brand != brand ) {
    if ( u ) save |= epg_brand_rem_season(season->es_brand, season, 0);
    season->es_brand = brand;
    if ( u ) save |= epg_brand_add_season(season->es_brand, season, 0);
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
  ee = RB_INSERT_SORTED(&season->es_episodes, episode, ee_slink, ee_uri_cmp);
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
  ee = RB_FIND(&season->es_episodes, episode, ee_slink, ee_uri_cmp);
  if ( ee != NULL ) {
    // TODO?: assert(ee == episode)
    if ( u ) save |= epg_episode_set_season(episode, NULL, 0); // does nothing
    RB_REMOVE(&season->es_episodes, episode, ee_slink);
    save = 1;
  }
  return save;
}

/* **************************************************************************
 * Episode
 * *************************************************************************/

epg_episode_t* epg_episode_find_by_uri ( const char *id, int create )
{
  epg_episode_t *ee;
  static epg_episode_t* skel = NULL;
  
  lock_assert(&global_lock); // pointless!

  if ( skel == NULL ) skel = calloc(1, sizeof(epg_episode_t));
  skel->ee_uri = (char*)id;

  /* Find */
  if ( !create ) {
    ee = RB_FIND(&epg_episodes, skel, ee_link, ee_uri_cmp);
  
  /* Create */
  } else {
    ee = RB_INSERT_SORTED(&epg_episodes, skel, ee_link, ee_uri_cmp);
    if ( ee == NULL ) {
      ee   = skel;
      skel = NULL;
      ee->ee_uri = strdup(id);
    }
  }

  return ee;
}

int epg_episode_set_title ( epg_episode_t *episode, const char *title )
{
  int save = 0;
  if ( !episode || !title ) return 0;
  if ( !episode->ee_title || strcmp(episode->ee_title, title) ) {
    if ( episode->ee_title ) free(episode->ee_title);
    episode->ee_title = strdup(title);
    save = 1;
  }
  return save;
}

int epg_episode_set_subtitle ( epg_episode_t *episode, const char *subtitle )
{
  int save = 0;
  if ( !episode || !subtitle ) return 0;
  if ( !episode->ee_subtitle || strcmp(episode->ee_subtitle, subtitle) ) {
    if ( episode->ee_subtitle ) free(episode->ee_subtitle);
    episode->ee_subtitle = strdup(subtitle);
    save = 1;
  }
  return save;
}

int epg_episode_set_summary ( epg_episode_t *episode, const char *summary )
{
  int save = 0;
  if ( !episode || !summary ) return 0;
  if ( !episode->ee_summary || strcmp(episode->ee_summary, summary) ) {
    if ( episode->ee_summary ) free(episode->ee_summary);
    episode->ee_summary = strdup(summary);
    save = 1;
  }
  return save;
}

int epg_episode_set_description ( epg_episode_t *episode, const char *desc )
{
  int save = 0;
  if ( !episode || !desc ) return 0;
  if ( !episode->ee_description || strcmp(episode->ee_description, desc) ) {
    if ( episode->ee_description ) free(episode->ee_description);
    episode->ee_description = strdup(desc);
    save = 1;
  }
  return save;
}

int epg_episode_set_number ( epg_episode_t *episode, uint16_t number )
{
  int save = 0;
  if ( !episode || !number ) return 0;
  if ( episode->ee_number != number ) {
    episode->ee_number = number;
    save = 1;
  }
  return save;
}

int epg_episode_set_part ( epg_episode_t *episode, uint16_t part, uint16_t count )
{
  int save = 0;
  // TODO: could treat part/count independently
  if ( !episode || !part || !count ) return 0;
  if ( episode->ee_part_number != part ) {
    episode->ee_part_number = part;
    save |= 1;
  }
  if ( episode->ee_part_count != count ) {
    episode->ee_part_count = count;
    save |= 1;
  }
  return save;
}

int epg_episode_set_brand ( epg_episode_t *episode, epg_brand_t *brand, int u )
{
  int save = 0;
  if ( !episode || !brand ) return 0;
  if ( episode->ee_brand != brand ) {
    if ( u ) save |= epg_brand_rem_episode(episode->ee_brand, episode, 0); /// does nothing
    episode->ee_brand = brand;
    if ( u ) save |= epg_brand_add_episode(episode->ee_brand, episode, 0);
    save |= 1;
  }
  return save;
}

int epg_episode_set_season ( epg_episode_t *episode, epg_season_t *season, int u )
{
  int save = 0;
  if ( !episode || !season ) return 0;
  if ( episode->ee_season != season ) {
    if ( u ) save |= epg_season_rem_episode(episode->ee_season, episode, 0);
    episode->ee_season = season;
    if ( u && episode->ee_season ) {
      save |= epg_season_add_episode(episode->ee_season, episode, 0);
      save |= epg_brand_add_episode(episode->ee_season->es_brand, episode, 0);
      // TODO: is this the right place?
    }
    save = 1;
  }
  return save;
}

/* **************************************************************************
 * Broadcast
 * *************************************************************************/

// Note: will find broadcast playing at this time (not necessarily
//       one that starts at this time)
//
// Note: do we need to pass in stop?
epg_broadcast_t* epg_broadcast_find_by_time 
  ( epg_channel_t *channel, time_t start, time_t stop, int create )
{
  epg_broadcast_t *eb;
  static epg_broadcast_t *skel = NULL;

  if ( !channel || !start || !stop ) return 0;
  if ( stop < start ) return 0;
  
  lock_assert(&global_lock); // pointless!

  if ( skel == NULL ) skel = calloc(1, sizeof(epg_broadcast_t));
  skel->eb_start = start;
  skel->eb_stop  = stop;
  
  /* Find */
  if ( !create ) {
    eb = RB_FIND(&channel->ec_schedule, skel, eb_slink, ebc_win_cmp);

  /* Create */
  } else {
    eb = RB_INSERT_SORTED(&channel->ec_schedule, skel, eb_slink, ebc_win_cmp);
    if ( eb == NULL ) {
      eb   = skel;
      skel = NULL;
    }
  }

  return eb;
}

int epg_broadcast_set_episode 
  ( epg_broadcast_t *broadcast, epg_episode_t *episode, int u )
{
  int save = 0;
  if ( !broadcast || !episode ) return 0;
  if ( broadcast->eb_episode != episode ) {
    broadcast->eb_episode = episode;
    //if ( u ) epg_episode_add_broadcast(episode, broadcast, 0);
    save = 1;
  }
  return save;
}

/* **************************************************************************
 * Channel
 * *************************************************************************/

epg_channel_t* epg_channel_find_by_uri ( const char *id, int create )
{
  epg_channel_t *ec;
  static epg_channel_t *skel = NULL;
  
  lock_assert(&global_lock); // pointless!

  if ( skel == NULL ) skel = calloc(1, sizeof(epg_channel_t));
  skel->ec_uri = (char*)id;

  /* Find */
  if ( !create ) {
    ec = RB_FIND(&epg_channels, skel, ec_link, ec_uri_cmp);
  
  /* Create */
  } else {
    ec = RB_INSERT_SORTED(&epg_channels, skel, ec_link, ec_uri_cmp);
    if ( ec == NULL ) {
      ec   = skel;
      skel = NULL;
      ec->ec_uri = strdup(id);
    }
  }

  return ec;
}

epg_channel_t* epg_channel_find 
  ( const char *id, const char *name, const char **sname, const int **sid )
{
  epg_channel_t* channel;

  /* Find or create */
  if ((channel = epg_channel_find_by_uri(id, 1)) == NULL) return NULL;

  /* Update fields? */

  return channel;
}
