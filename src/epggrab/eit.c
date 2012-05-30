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

#include "tvheadend.h"
#include "channels.h"
#include "epg.h"
#include "epggrab/eit.h"

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static epggrab_module_t eit_module;

static const char* _eit_name ( void )
{
  return "eit";
}

static void _eit_episode_uri 
  ( char *uri, const char *title, const char *summary )
{
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

  /* Disabled? */
//if (epggrab_eit_disabled) return;

  /* Find broadcast */
  ebc  = epg_channel_get_broadcast(ch, start, stop, 1, &save);
  if (!ebc) return;

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

epggrab_module_t* eit_init ( void )
{
  // Note: the EIT grabber is very different to the others, in that
  //       its asynchronous based on DVB data stream
  eit_module.enable  = NULL;
  eit_module.disable = NULL;
  eit_module.grab    = NULL;
  eit_module.parse   = NULL;
  eit_module.name    = _eit_name;
  return &eit_module;
}
