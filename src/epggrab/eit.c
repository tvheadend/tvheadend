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

static const char *
longest_string ( const char *a, const char *b )
{
  if (!a) return b;
  if (!b) return a;
  if (strlen(a) - strlen(b) >= 0) return a;
}

// called from dvb_tables.c
void eit_callback ( channel_t *ch, int id, time_t start, time_t stop,
                    const char *title, const char *desc,
                    const char *extitem, const char *extdesc,
                    const char *exttext,
                    const uint8_t *genre, int genre_cnt ) {
  int save = 0;
  epg_broadcast_t *ebc;
  epg_episode_t *ee;
  const char *summary     = NULL;
  const char *description = NULL;
  char *uri;

  /* Ignore */
  if (!ch || !ch->ch_name || !ch->ch_name[0]) return;
  if (!title) return;

  /* Disabled? */
  if (!epggrab_eitenabled) return;

  /* Find broadcast */
  ebc  = epg_broadcast_find_by_time(ch, start, stop, 1, &save);
  if (!ebc) return;

  /* Determine summary */
  description = summary = desc;
  description = longest_string(description, extitem);
  description = longest_string(description, extdesc);
  description = longest_string(description, exttext);
  if (summary == description) description = NULL;
  // TODO: is this correct?

  /* Create episode URI */
  uri = md5sum(longest_string(title, longest_string(description, summary)));

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
    if (genre_cnt)
      save |= epg_episode_set_genre(ee, genre, genre_cnt);

    /* Update */
    save |= epg_broadcast_set_episode(ebc, ee);
  }
  free(uri);
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static epggrab_module_t _eit_mod;

void eit_init ( epggrab_module_list_t *list )
{
  _eit_mod.id     = strdup("eit");
  _eit_mod.name   = strdup("EIT: On-Air Grabber");
  *((uint8_t*)&_eit_mod.flags) = EPGGRAB_MODULE_SPECIAL;
  LIST_INSERT_HEAD(list, &_eit_mod, link);
  // Note: this is mostly ignored anyway as EIT is treated as a special case!
}

void eit_load ( void )
{
}
