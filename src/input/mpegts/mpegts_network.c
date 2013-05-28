/*
 *  Tvheadend - MPEGTS input source
 *  Copyright (C) 2013 Adam Sutton
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

#include "input/mpegts.h"

#include <assert.h>

const idclass_t mpegts_network_class =
{
  .ic_class      = "mpegts_network",
  .ic_caption    = "MPEGTS Network",
  .ic_properties = (const property_t[]){
  }
};

static void
mpegts_network_config_save
  ( mpegts_network_t *mn )
{
}

static mpegts_mux_t *
mpegts_network_create_mux
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t tsid, dvb_mux_conf_t *aux )
{
  return NULL;
}

static void
mpegts_network_initial_scan(void *aux)
{
  mpegts_network_t *mn = aux;
  mpegts_mux_t     *mm;

  tvhtrace("mpegts", "setup initial scan for %p", mn);
  while((mm = TAILQ_FIRST(&mn->mn_initial_scan_pending_queue)) != NULL) {
    assert(mm->mm_initial_scan_status == MM_SCAN_PENDING);
    if (mm->mm_start(mm, "initial scan", 1))
      break;
    assert(mm->mm_initial_scan_status == MM_SCAN_CURRENT);
  }
  gtimer_arm(&mn->mn_initial_scan_timer, mpegts_network_initial_scan, mn, 10);
}

void
mpegts_network_schedule_initial_scan ( mpegts_network_t *mn )
{
  gtimer_arm(&mn->mn_initial_scan_timer, mpegts_network_initial_scan, mn, 0);
}

void
mpegts_network_add_input ( mpegts_network_t *mn, mpegts_input_t *mi )
{
  mi->mi_network = mn;
  LIST_INSERT_HEAD(&mn->mn_inputs, mi, mi_network_link);
}

mpegts_network_t *
mpegts_network_create0
  ( mpegts_network_t *mn, const idclass_t *idc, const char *uuid,
    const char *netname )
{
  idnode_insert(&mn->mn_id, uuid, idc);
  mn->mn_create_mux   = mpegts_network_create_mux;
  mn->mn_config_save  = mpegts_network_config_save;
  if (netname) mn->mn_network_name = strdup(netname);
  TAILQ_INIT(&mn->mn_initial_scan_pending_queue);
  TAILQ_INIT(&mn->mn_initial_scan_current_queue);
  return mn;
}

int
mpegts_network_set_nid
  ( mpegts_network_t *mn, uint16_t nid )
{
  if (mn->mn_nid == nid)
    return 0;
  mn->mn_nid = nid;
  return 1;
}

int
mpegts_network_set_network_name
  ( mpegts_network_t *mn, const char *name )
{
  if (!name || !strcmp(name, mn->mn_network_name))
    return 0;
  tvh_str_update(&mn->mn_network_name, name);
  return 1;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
