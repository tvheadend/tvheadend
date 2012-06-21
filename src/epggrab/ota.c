/*
 *  Electronic Program Guide - EPG grabber OTA functions
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
#include "queue.h"
#include "epg.h"
#include "dvb/dvb.h"
#include "epggrab.h"
#include "epggrab/ota.h"

LIST_HEAD(,epggrab_ota_mux) ota_muxes;

epggrab_ota_mux_t *epggrab_ota_register
  ( epggrab_module_t *mod, th_dvb_mux_instance_t *tdmi,
    int timeout, int interval )
{
  epggrab_ota_mux_t *ota;

  /* Check for existing */
  LIST_FOREACH(ota, &ota_muxes, glob_link) {
    if (ota->grab == mod && ota->tdmi == tdmi) return ota;
  }
  
  /* Install new */
  ota = calloc(1, sizeof(epggrab_ota_mux_t));
  ota->grab     = mod;
  ota->tdmi     = tdmi;
  ota->timeout  = timeout;
  ota->interval = interval;
  LIST_INSERT_HEAD(&ota_muxes, ota, glob_link);
  LIST_INSERT_HEAD(&tdmi->tdmi_epg_grabbers, ota, tdmi_link);
  LIST_INSERT_HEAD(&mod->muxes, ota, grab_link);
  return ota;
}

/*
 * Unregister
 */
void epggrab_ota_unregister ( epggrab_ota_mux_t *ota )
{
  LIST_REMOVE(ota, glob_link);
  LIST_REMOVE(ota, tdmi_link);
  LIST_REMOVE(ota, grab_link);
  free(ota);
}

void epggrab_ota_unregister_one
  ( epggrab_module_t *mod, th_dvb_mux_instance_t *tdmi )
{
  epggrab_ota_mux_t *ota;
  LIST_FOREACH(ota, &ota_muxes, glob_link) {
    if (ota->grab == mod && (!tdmi || ota->tdmi == tdmi)) {
      epggrab_ota_unregister(ota);
      if (tdmi) break;
    }
  }
}

void epggrab_ota_unregister_all
  ( epggrab_module_t *mod )
{
  epggrab_ota_unregister_one(mod, NULL);
}

/*
 * Status
 */
int epggrab_ota_begin     ( epggrab_ota_mux_t *ota )
{
  if (ota->status != EPGGRAB_OTA_STARTED) {
    ota->status = EPGGRAB_OTA_STARTED;
    time(&ota->started);
    return 1;
  }
  return 0;
}

void epggrab_ota_complete  ( epggrab_ota_mux_t *ota )
{
  if (ota->status != EPGGRAB_OTA_COMPLETED)
    time(&ota->completed);
  ota->status = EPGGRAB_OTA_COMPLETED;
}

void epggrab_ota_cancel    ( epggrab_ota_mux_t *ota )
{
  ota->status = EPGGRAB_OTA_IDLE;
}

void epggrab_ota_timeout ( epggrab_ota_mux_t *ota )
{
  epggrab_ota_complete(ota); 
  // TODO: for now just treat as completed, stops it being allowed
  //       to immediately re-enter the queue
}
