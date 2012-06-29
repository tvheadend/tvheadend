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

#include <assert.h>
#include <string.h>

#include "tvheadend.h"
#include "settings.h"
#include "htsmsg.h"
#include "channels.h"
#include "service.h"
#include "epg.h"
#include "epggrab.h"
#include "epggrab/private.h"

#if 0
static int _ch_id_cmp ( void *a, void *b )
{
  return strcmp(((epggrab_channel_t*)a)->id,
                ((epggrab_channel_t*)b)->id);
}
#endif

int epggrab_channel_link ( epggrab_channel_t *ec, channel_t *ch )
{
  service_t *sv;
  int match = 0, i;

  if (!ec || !ch) return 0;
  if (ec->channel) return 0;

  if (ec->name && !strcmp(ec->name, ch->ch_name))
    match = 1;
  else {
    LIST_FOREACH(sv, &ch->ch_services, s_ch_link) {
      if (ec->sid) {
        i = 0;
        while (ec->sid[i]) {
          if (sv->s_dvb_service_id == ec->sid[i]) {
            match = 1;
            break;
          }
          i++;
        }
      }
      if (!match && ec->sname) {
        i = 0;
        while (ec->sname[i]) {
          if (!strcmp(ec->sname[i], sv->s_svcname)) {
            match = 1;
            break;
          }
          i++;
        }
      }
      if (match) break;
    }
  }

  if (match) {
    tvhlog(LOG_INFO, ec->mod->id, "linking %s to %s",
           ec->id, ch->ch_name);
    ec->channel = ch;
    if (ec->name && epggrab_channel_rename)
      channel_rename(ch, ec->name);
    if (ec->icon && epggrab_channel_reicon)
      channel_set_icon(ch, ec->icon);
    if (ec->number>0 && epggrab_channel_renumber)
      channel_set_number(ch, ec->number);
  }

  return match;
}

int epggrab_channel_set_name ( epggrab_channel_t *ec, const char *name )
{
  int save = 0;
  if (!ec || !name) return 0;
  if (!ec->name || strcmp(ec->name, name)) {
    if (ec->name) free(ec->name);
    ec->name = strdup(name);
    if (ec->channel) channel_rename(ec->channel, name);
    save = 1;
  }
  return save;
}

int epggrab_channel_set_sid 
  ( epggrab_channel_t *ec, const uint16_t *sid )
{
#if 0
  int save = 0, i = 0, num = 0;
  if ( !ec || !sid ) return 0;
  if (!ec->sid) save = 1;
  else {
    
    for (i = 0; i < num; i++ ) {
      if (sid[i] != ec->sid[i]) {
        save = 1;
        break;
      }
    }
  }
  if (save) {
    if (ec->sid) free(ec->sid);
    ec->sid = calloc(num, sizeof(uint16_t));
    for (i = 0; i < num; i++ ) {
      ec->sid[i] = sid[i];
    }
    ec->sid_cnt = num;
  }
  return save;
#endif
  return 0;
}

int epggrab_channel_set_sname ( epggrab_channel_t *ec, const char **sname )
{
  int save = 0, i = 0;
  if ( !ec || !sname ) return 0;
  if (!ec->sname) save = 1;
  else {
    while ( ec->sname[i] && sname[i] ) {
      if (strcmp(ec->sname[i], sname[i])) {
        save = 1;
        break;
      }
      i++;
    }
    if (!save && (ec->sname[i] || sname[i])) save = 1;
  }
  if (save) {
    if (ec->sname) {
      i = 0;
      while (ec->sname[i])
        free(ec->sname[i++]);
      free(ec->sname);
    }
    i = 0;
    while (sname[i++]);
    ec->sname = calloc(i+1, sizeof(char*));
    i = 0;
    while (sname[i]) {
      ec->sname[i] = strdup(sname[i]);
      i++;
    } 
  }
  return save;
}

int epggrab_channel_set_icon ( epggrab_channel_t *ec, const char *icon )
{
  int save = 0;
  if (!ec->icon || strcmp(ec->icon, icon) ) {
  if (!ec | !icon) return 0;
    if (ec->icon) free(ec->icon);
    ec->icon = strdup(icon);
    if (ec->channel) channel_set_icon(ec->channel, icon);
    save = 1;
  }
  return save;
}

int epggrab_channel_set_number ( epggrab_channel_t *ec, int number )
{
  int save = 0;
  if (!ec || (number <= 0)) return 0;
  if (ec->number != number) {
    ec->number = number;
    if (ec->channel) channel_set_number(ec->channel, number);
    save = 1;
  }
  return save;
}


void epggrab_channel_updated ( epggrab_channel_t *ec )
{
  //epggrab_channel_link(ec);
  epggrab_module_ch_save(ec->mod, ec);
}

#if 0
void epggrab_channel_link ( epggrab_channel_t *ec )
{ 
  channel_t *ch;

  if (!ec) return;

  /* Link */
  if (!ec->channel) {
    RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
      if (_ch_link(ec, ch)) break;
    }
  }
}
#endif

htsmsg_t *epggrab_channel_list ( void )
{
#if 0
  char name[100];
  epggrab_module_t *mod;
  epggrab_channel_t *ec;
  htsmsg_t *e, *m;
  m = htsmsg_create_list();
  LIST_FOREACH(mod, &epggrab_modules, link) {
    if (mod->enabled && mod->channels) {
      RB_FOREACH(ec, mod->channels, link) {
        e = htsmsg_create_map();
        htsmsg_add_str(e, "module", mod->id);
        htsmsg_add_str(e, "id",     ec->id);
        sprintf(name, "%s: %s", mod->name, ec->name);
        htsmsg_add_str(e, "name", name);
        htsmsg_add_msg(m, NULL, e);
      }
    }
  }
  return m;
#endif
  return NULL;
}

void epggrab_channel_add ( channel_t *ch )
{
#if 0
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->ch_add) m->ch_add(m, ch);
  }
#endif
}

void epggrab_channel_rem ( channel_t *ch )
{
#if 0
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->ch_rem) m->ch_rem(m, ch);
  }
#endif
}

void epggrab_channel_mod ( channel_t *ch )
{
#if 0
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->ch_mod) m->ch_mod(m, ch);
  }
#endif
}

epggrab_channel_t *epggrab_channel_find
  ( epggrab_channel_tree_t *tree, const char *id, int create, int *save )
{
  // TODO
  return NULL;
}

