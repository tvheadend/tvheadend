/*
 *  Electronic Program Guide - eit grabber
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

#include <string.h>
#include "tvheadend.h"
#include "channels.h"
#include "epg.h"
#include "epggrab/eit.h"

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static void _eit_episode_uri 
  ( char *uri, const char *title, const char *summary )
{
  // TODO: do something better
  snprintf(uri, 1023, "%s::%s", title, summary);
}

// called from dvb_tables.c
void eit_callback ( channel_t *ch, int id, time_t start, time_t stop,
                    const char *title, const char *desc,
                    const char *extitem, const char *extdesc,
                    const char *exttext ) {
  int save = 0;
  epg_broadcast_t *ebc;
  epg_episode_t *ee;
  const char *summary     = NULL;
  const char *description = NULL;
  char uri[1024];

  /* Ignore */
  if (!ch || !ch->ch_name || !ch->ch_name[0]) return;

  /* Disabled? */
#if TODO_REENABLE_THIS
  if (epggrab_eit_disabled) return;
#endif

  /* Find broadcast */
  ebc  = epg_broadcast_find_by_time(ch, start, stop, 1, &save);
  if (!ebc) return;

  /* TODO: Determine summary */
  summary = desc;

  /* Create episode URI */
  _eit_episode_uri(uri, title, summary);

  /* Create/Replace episode */
  if ( !ebc->episode ||
       !epg_episode_fuzzy_match(ebc->episode, uri, title,
                                summary, description) ) {
    
    /* Create episode */
    ee  = epg_episode_find_by_uri(uri, 1, &save);

    /* Set fields */
    if (title)
      save |= epg_episode_set_title(ee, title);
    if (summary)
      save |= epg_episode_set_summary(ee, summary);
    if (description)
      save |= epg_episode_set_description(ee, description);

    /* Update */
    save |= epg_broadcast_set_episode(ebc, ee);
  }
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static epggrab_module_t _eit_mod;
static uint8_t          _eit_enabled;

static void _eit_enable ( epggrab_module_t *mod, uint8_t e )
{
  _eit_enabled = e;
}

void eit_init ( epggrab_module_list_t *list )
{
  _eit_mod.id     = strdup("eit");
  _eit_mod.name   = strdup("EIT: On-Air Grabber");
  *((uint8_t*)&_eit_mod.flags) = EPGGRAB_MODULE_ASYNC;
  _eit_mod.enable = _eit_enable; 
  LIST_INSERT_HEAD(list, &_eit_mod, link);
}
