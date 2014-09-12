/*
 *  EPG Grabber - channel functions
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

#include "tvheadend.h"
#include "settings.h"
#include "htsmsg.h"
#include "channels.h"
#include "epg.h"
#include "epggrab.h"
#include "epggrab/private.h"

#include <assert.h>
#include <string.h>

SKEL_DECLARE(epggrab_channel_skel, epggrab_channel_t);

/* **************************************************************************
 * EPG Grab Channel functions
 * *************************************************************************/

/* Check if channels match */
int epggrab_channel_match ( epggrab_channel_t *ec, channel_t *ch )
{
  if (!ec || !ch) return 0;
  if (LIST_FIRST(&ec->channels)) return 0; // ignore already paired

  if (ec->name && !strcmp(ec->name, channel_get_name(ch))) return 1;
  return 0;
}

/* Destroy */
void
epggrab_channel_link_delete
  ( epggrab_channel_link_t *ecl )
{
  LIST_REMOVE(ecl, ecl_chn_link);
  LIST_REMOVE(ecl, ecl_epg_link);
  if (ecl->ecl_epggrab->mod->ch_save)
    ecl->ecl_epggrab->mod->ch_save(ecl->ecl_epggrab->mod, ecl->ecl_epggrab);
  free(ecl);
}

/* Link epggrab channel to real channel */
int
epggrab_channel_link ( epggrab_channel_t *ec, channel_t *ch )
{
  epggrab_channel_link_t *ecl;

  /* No change */
  if (!ch) return 0;

  /* Already linked */
  LIST_FOREACH(ecl, &ec->channels, ecl_epg_link) {
    if (ecl->ecl_channel == ch) {
      ecl->ecl_mark = 0;
      return 0;
    }
  }

  /* New link */
  tvhdebug(ec->mod->id, "linking %s to %s",
         ec->id, channel_get_name(ch));
  ecl = calloc(1, sizeof(epggrab_channel_link_t));
  ecl->ecl_channel = ch;
  ecl->ecl_epggrab = ec;
  LIST_INSERT_HEAD(&ec->channels, ecl, ecl_epg_link);
  LIST_INSERT_HEAD(&ch->ch_epggrab, ecl, ecl_chn_link);
#if TODO_CHAN_UPDATE
  if (ec->name && epggrab_channel_rename)
    channel_rename(ch, ec->name);
  if (ec->number>0 && epggrab_channel_renumber)
    channel_set_number(ch, ec->number);
  if (ec->icon && epggrab_channel_reicon)
    channel_set_icon(ch, ec->icon);
#endif

  /* Save */
  if (ec->mod->ch_save) ec->mod->ch_save(ec->mod, ec);
  return 1;
}

/* Match and link (basically combines two funcs above for ease) */
int epggrab_channel_match_and_link ( epggrab_channel_t *ec, channel_t *ch )
{
  int r = epggrab_channel_match(ec, ch);
  if (r)
    epggrab_channel_link(ec, ch);
  return r;
}

/* Set name */
int epggrab_channel_set_name ( epggrab_channel_t *ec, const char *name )
{
  int save = 0;
  if (!ec || !name) return 0;
  if (!ec->name || strcmp(ec->name, name)) {
    if (ec->name) free(ec->name);
    ec->name = strdup(name);
#if TODO_CHAN_UPDATE
    if (epggrab_channel_rename) {
      epggrab_channel_link_t *ecl;
      LIST_FOREACH(ecl, &ec->channels, link)
        channel_rename(ecl->channel, name);
    }
#endif
    save = 1;
  }
  return save;
}

/* Set icon */
int epggrab_channel_set_icon ( epggrab_channel_t *ec, const char *icon )
{
  int save = 0;
  if (!ec->icon || strcmp(ec->icon, icon) ) {
  if (!ec | !icon) return 0;
    if (ec->icon) free(ec->icon);
    ec->icon = strdup(icon);
#if TODO_CHAN_UPDATE
    if (epggrab_channel_reicon) {
      epggrab_channel_link_t *ecl;
      LIST_FOREACH(ecl, &ec->channels, link)
        channel_set_icon(ecl->channel, icon);
    }
#endif
    save = 1;
  }
  return save;
}

/* Set channel number */
int epggrab_channel_set_number ( epggrab_channel_t *ec, int number )
{
  int save = 0;
  if (!ec || (number <= 0)) return 0;
  if (ec->number != number) {
    ec->number = number;
#if TODO_CHAN_UPDATE
    if (epggrab_channel_renumber) {
      epggrab_channel_link_t *ecl;
      LIST_FOREACH(ecl, &ec->channels, link)
        channel_set_number(ecl->channel, number);
    }
#endif
    save = 1;
  }
  return save;
}

/* Channel settings updated */
void epggrab_channel_updated ( epggrab_channel_t *ec )
{
  channel_t *ch;
  if (!ec) return;

  /* Find a link */
  if (!LIST_FIRST(&ec->channels))
    CHANNEL_FOREACH(ch)
      if (epggrab_channel_match_and_link(ec, ch)) break;

  /* Save */
  if (ec->mod->ch_save) ec->mod->ch_save(ec->mod, ec);
}

/* ID comparison */
static int _ch_id_cmp ( void *a, void *b )
{
  return strcmp(((epggrab_channel_t*)a)->id,
                ((epggrab_channel_t*)b)->id);
}

/* Find/Create channel in the list */
epggrab_channel_t *epggrab_channel_find
  ( epggrab_channel_tree_t *tree, const char *id, int create, int *save,
    epggrab_module_t *owner )
{
  char *s;
  epggrab_channel_t *ec;

  SKEL_ALLOC(epggrab_channel_skel);
  s = epggrab_channel_skel->id = tvh_strdupa(id);

  /* Replace / with # */
  // Note: this is a bit of a nasty fix for #1774, but will do for now
  while (*s) {
    if (*s == '/') *s = '#';
    s++;
  }

  /* Find */
  if (!create) {
    ec = RB_FIND(tree, epggrab_channel_skel, link, _ch_id_cmp);

  /* Find/Create */
  } else {
    ec = RB_INSERT_SORTED(tree, epggrab_channel_skel, link, _ch_id_cmp);
    if (!ec) {
      assert(owner);
      ec      = epggrab_channel_skel;
      SKEL_USED(epggrab_channel_skel);
      ec->id  = strdup(ec->id);
      ec->mod = owner;
      *save   = 1;
      return ec;
    }
  }
  return ec;
}

/* **************************************************************************
 * Global routines
 * *************************************************************************/

htsmsg_t *epggrab_channel_list ( int ota )
{
  char name[500];
  epggrab_module_t *mod;
  epggrab_channel_t *ec;
  htsmsg_t *e, *m;
  m = htsmsg_create_list();
  LIST_FOREACH(mod, &epggrab_modules, link) {
    if (!ota && (mod->type == EPGGRAB_OTA)) continue;
    if (mod->channels) {
      RB_FOREACH(ec, mod->channels, link) {
        e = htsmsg_create_map();
        snprintf(name, sizeof(name), "%s|%s", mod->id, ec->id);
        htsmsg_add_str(e, "key", name);
        snprintf(name, sizeof(name), "%s: %s (%s)",
                 mod->name, ec->name, ec->id);
        htsmsg_add_str(e, "val", name);
        htsmsg_add_msg(m, NULL, e);
      }
    }
  }
  return m;
}

void epggrab_channel_add ( channel_t *ch )
{
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->ch_add) m->ch_add(m, ch);
  }
}

void epggrab_channel_rem ( channel_t *ch )
{
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->ch_rem) m->ch_rem(m, ch);
  }
}

void epggrab_channel_mod ( channel_t *ch )
{
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->ch_mod) m->ch_mod(m, ch);
  }
}

const char *
epggrab_channel_get_id ( epggrab_channel_t *ec )
{
  static char buf[1024];
  epggrab_module_t *m = ec->mod;
  snprintf(buf, sizeof(buf), "%s|%s", m->id, ec->id);
  return buf;
}

epggrab_channel_t *
epggrab_channel_find_by_id ( const char *id )
{
  char buf[1024];
  char *mid, *cid;
  epggrab_module_t *mod;
  strncpy(buf, id, sizeof(buf));
  if ((mid = strtok_r(buf, "|", &cid)) && cid)
    if ((mod = epggrab_module_find_by_id(mid)) && mod->channels)
      return epggrab_channel_find(mod->channels, cid, 0, NULL, NULL);
  return NULL;
}

int
epggrab_channel_is_ota ( epggrab_channel_t *ec )
{
  return ec->mod->type == EPGGRAB_OTA;
}

void
epggrab_channel_done( void )
{
  SKEL_FREE(epggrab_channel_skel);
}
