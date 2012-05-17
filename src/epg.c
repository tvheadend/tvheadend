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

void epg_init ( void )
{
}

void epg_save ( void )
{
}

void epg_updated ( void )
{
}

int epg_brand_set_title ( epg_brand_t *brand, const char *title )
{
  return 0;
}

int epg_brand_set_summary ( epg_brand_t *brand, const char *summary )
{
  return 0;
}

int epg_brand_set_season_count ( epg_brand_t *brand, uint16_t count )
{
  return 0;
}

int epg_season_set_brand ( epg_season_t *season, epg_brand_t *brand )
{
  return 0;
}

int epg_season_set_summary ( epg_season_t *season, const char *summary )
{
  return 0;
}

int epg_season_set_episode_count ( epg_season_t *season, uint16_t count )
{
  return 0;
}

int epg_season_set_number ( epg_season_t *season, uint16_t number )
{
  return 0;
}

int epg_episode_set_title ( epg_episode_t *episode, const char *title )
{
  return 0;
}
int epg_episode_set_subtitle ( epg_episode_t *episode, const char *title )
{
  return 0;
}
int epg_episode_set_summary ( epg_episode_t *episode, const char *summary )
{
  return 0;
}
int epg_episode_set_description ( epg_episode_t *episode, const char *desc )
{
  return 0;
}
int epg_episode_set_brand ( epg_episode_t *episode, epg_brand_t *brand )
{
  return 0;
}
int epg_episode_set_season ( epg_episode_t *episode, epg_season_t *season )
{
  return 0;
}
int epg_episode_set_number ( epg_episode_t *episode, uint16_t number )
{
  return 0;
}
int epg_episode_set_part ( epg_episode_t *episode, uint16_t part, uint16_t count )
{
  return 0;
}

static int eb_uri_cmp ( const epg_brand_t *a, const epg_brand_t *b )
{
  return strcmp(a->eb_uri, b->eb_uri);
}

epg_brand_t* epg_brand_find_by_uri ( const char *id, int create )
{
  epg_brand_t *eb;
  static epg_brand_t* skel = NULL;
  
  lock_assert(&global_lock); // pointless!

  if ( skel == NULL ) skel = calloc(1, sizeof(epg_brand_t));

  /* Find */
  skel->eb_uri = (char*)id;
  eb = RB_FIND(&epg_brands, skel, eb_link, eb_uri_cmp);
  if ( eb ) printf("found brand %s @ %p\n", id, eb);
  
  /* Create */
  if ( !eb && create ) {
    eb = RB_INSERT_SORTED(&epg_brands, skel, eb_link, eb_uri_cmp);
    if ( eb == NULL ) {
      eb   = skel;
      skel = NULL;
      eb->eb_uri = strdup(id);
      printf("create brand %s @ %p\n", id, eb);
    }
  }

  return eb;
}

static int es_uri_cmp ( const epg_season_t *a, const epg_season_t *b )
{
  return strcmp(a->es_uri, b->es_uri);
}

epg_season_t* epg_season_find_by_uri ( const char *id, int create )
{
  epg_season_t *es;
  static epg_season_t* skel = NULL;
  
  lock_assert(&global_lock); // pointless!

  if ( skel == NULL ) skel = calloc(1, sizeof(epg_season_t));

  /* Find */
  skel->es_uri = (char*)id;
  es = RB_FIND(&epg_seasons, skel, es_link, es_uri_cmp);
  if ( es ) printf("found season %s @ %p\n", id, es);
  
  /* Create */
  if ( !es && create ) {
    es = RB_INSERT_SORTED(&epg_seasons, skel, es_link, es_uri_cmp);
    if ( es == NULL ) {
      es   = skel;
      skel = NULL;
      es->es_uri = strdup(id);
      printf("create season %s @ %p\n", id, es);
    }
  }

  return es;
}

static int ee_uri_cmp ( const epg_episode_t *a, const epg_episode_t *b )
{
  return strcmp(a->ee_uri, b->ee_uri);
}

epg_episode_t* epg_episode_find_by_uri ( const char *id, int create )
{
  epg_episode_t *ee;
  static epg_episode_t* skel = NULL;
  
  lock_assert(&global_lock); // pointless!

  if ( skel == NULL ) skel = calloc(1, sizeof(epg_episode_t));

  /* Find */
  skel->ee_uri = (char*)id;
  ee = RB_FIND(&epg_episodes, skel, ee_link, ee_uri_cmp);
  if ( ee ) printf("found episode %s @ %p\n", id, ee);
  
  /* Create */
  if ( !ee && create ) {
    ee = RB_INSERT_SORTED(&epg_episodes, skel, ee_link, ee_uri_cmp);
    if ( ee == NULL ) {
      ee   = skel;
      skel = NULL;
      ee->ee_uri = strdup(id);
      printf("create epsiode %s @ %p\n", id, ee);
    }
  }

  return ee;
}

static int ec_uri_cmp ( const epg_channel_t *a, const epg_channel_t *b )
{
  return strcmp(a->ec_uri, b->ec_uri);
}

epg_channel_t* epg_channel_find_by_uri ( const char *id, int create )
{
  epg_channel_t *ec;
  static epg_channel_t *skel = NULL;
  // TODO: is it really that beneficial to save a few bytes on the stack?
  // TODO: does this really need to be the global lock?
  
  lock_assert(&global_lock); // pointless!

  if ( skel == NULL ) skel = calloc(1, sizeof(epg_channel_t));

  /* Find */
  skel->ec_uri = (char*)id;
  ec = RB_FIND(&epg_channels, skel, ec_link, ec_uri_cmp);
  if ( ec ) printf("found channel %s @ %p\n", id, ec);
  
  /* Create */
  if ( !ec && create ) {
    ec = RB_INSERT_SORTED(&epg_channels, skel, ec_link, ec_uri_cmp);
    if ( ec == NULL ) {
      ec   = skel;
      skel = NULL;
      ec->ec_uri = strdup(id);
      printf("create channel %s @ %p\n", id, ec);
    }
  }

  return ec;
}

epg_channel_t* epg_channel_find ( const char *id, const char *name, const char **sname, const int **sid )
{
  epg_channel_t* channel;

  /* Find or create */
  if ((channel = epg_channel_find_by_uri(id, 1)) == NULL) return NULL;

  /* Update fields? */

  return channel;
}
