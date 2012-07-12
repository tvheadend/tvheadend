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

/*
 * TODO: currently I don't try to block multiple scans of the same
 *       tdmi on different adapters
 */

#include <string.h>

#include "tvheadend.h"
#include "queue.h"
#include "epg.h"
#include "dvb/dvb.h"
#include "epggrab.h"
#include "epggrab/private.h"

TAILQ_HEAD(, epggrab_ota_mux) ota_mux_all;

/* **************************************************************************
 * Global functions (called from DVB code)
 * *************************************************************************/

void epggrab_mux_start ( th_dvb_mux_instance_t *tdmi )
{
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->type == EPGGRAB_OTA) {
      ((epggrab_module_ota_t*)m)->start((epggrab_module_ota_t*)m, tdmi);
    }
  }
}

void epggrab_mux_stop ( th_dvb_mux_instance_t *tdmi, int timeout )
{
  // Note: the slightly akward list iteration here is because
  // _ota_cancel/delete can remove the object and free() it
  epggrab_ota_mux_t *a, *b;
  a = TAILQ_FIRST(&tdmi->tdmi_epg_grab);
  while (a) {
    b = TAILQ_NEXT(a, tdmi_link);
    if (a->tdmi == tdmi) {
      if (timeout)
        epggrab_ota_timeout(a);
      else
        epggrab_ota_cancel(a);
    }
    a = b;
  }
}

void epggrab_mux_delete ( th_dvb_mux_instance_t *tdmi )
{
  epggrab_ota_destroy_by_tdmi(tdmi);
}

int epggrab_mux_period ( th_dvb_mux_instance_t *tdmi )
{
  int period = 0;
  epggrab_ota_mux_t *ota;
  TAILQ_FOREACH(ota, &tdmi->tdmi_epg_grab, tdmi_link) {
    if (!ota->is_reg) continue;
    if (ota->timeout > period)
      period = ota->timeout;
  }
  return period;
}

th_dvb_mux_instance_t *epggrab_mux_next ( th_dvb_adapter_t *tda )
{
  time_t now;
  epggrab_ota_mux_t *ota;
  time(&now);
  TAILQ_FOREACH(ota, &ota_mux_all, glob_link) {
    if (ota->interval + ota->completed > now) return NULL;
    if (!ota->is_reg) return NULL;
    if (ota->tdmi->tdmi_adapter == tda) break;
  }
  return ota ? ota->tdmi : NULL;
}

/* **************************************************************************
 * OTA Mux link functions
 * *************************************************************************/

/*
 * Comprison of ota_mux instances based on when they can next run
 *
 * Note: ordering isn't garaunteed to be perfect as we use the current time
 */
static int _ota_time_cmp ( void *_a, void *_b )
{
  int r;
  time_t now, wa, wb;
  time(&now);
  epggrab_ota_mux_t *a = _a;
  epggrab_ota_mux_t *b = _b;
  
  /* Unreg'd always at the end */
  r = a->is_reg - b->is_reg;
  if (r) return r;

  /* Check when */
  wa = a->completed + a->interval;
  wb = b->completed + b->interval;
  if (wa < now && wb < now)
    return a->started - b->started;
  else
    return wa - wb;
}

/*
 * Find existing link
 */
epggrab_ota_mux_t *epggrab_ota_find
  ( epggrab_module_ota_t *mod, th_dvb_mux_instance_t *tdmi )
{
  epggrab_ota_mux_t *ota;
  TAILQ_FOREACH(ota, &mod->muxes, grab_link) {
    if (ota->tdmi == tdmi) break;
  }
  return ota;
}

/*
 * Create (temporary) or Find (existing) link
 */
epggrab_ota_mux_t *epggrab_ota_create
  ( epggrab_module_ota_t *mod, th_dvb_mux_instance_t *tdmi )
{
  /* Search for existing */
  epggrab_ota_mux_t *ota = epggrab_ota_find(mod, tdmi);

  /* Create new */
  if (!ota) {
    ota = calloc(1, sizeof(epggrab_ota_mux_t));
    ota->grab  = mod;
    ota->tdmi  = tdmi;
    TAILQ_INSERT_TAIL(&ota_mux_all, ota, glob_link);
    TAILQ_INSERT_TAIL(&tdmi->tdmi_epg_grab, ota, tdmi_link);
    TAILQ_INSERT_TAIL(&mod->muxes, ota, grab_link);

  } else {
    time_t now;
    time(&now);
    ota->state = EPGGRAB_OTA_MUX_IDLE;

    /* Blocked */
    if (epggrab_ota_is_blocked(ota)) ota = NULL;
  }
  return ota;
}

/*
 * Create and register using mux ID
 */
void epggrab_ota_create_and_register_by_id
  ( epggrab_module_ota_t *mod, int nid, int tsid, int period, int interval, const char *networkname )
{
  th_dvb_adapter_t *tda;
  th_dvb_mux_instance_t *tdmi;
  epggrab_ota_mux_t *ota;
  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link) {
    //TODO: if (tda->nitoid != nid) continue;
    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
      if (tdmi->tdmi_transport_stream_id != tsid) continue;
      if (networkname && (!tdmi->tdmi_network || strcmp(networkname, tdmi->tdmi_network))) continue;
      ota = epggrab_ota_create(mod, tdmi);
      epggrab_ota_register(ota, period, interval);
    }
  }
}

/*
 * Destrory link (either because it was temporary OR mux deleted)
 */
void epggrab_ota_destroy ( epggrab_ota_mux_t *ota )
{
  TAILQ_REMOVE(&ota_mux_all, ota, glob_link);
  TAILQ_REMOVE(&ota->tdmi->tdmi_epg_grab, ota, tdmi_link);
  TAILQ_REMOVE(&ota->grab->muxes, ota, grab_link);
  if (ota->destroy) ota->destroy(ota);
  else {
    if (ota->status) free(ota->status);
    free(ota);
  }
}

/*
 * Destroy by tdmi
 */
void epggrab_ota_destroy_by_tdmi ( th_dvb_mux_instance_t *tdmi )
{
  epggrab_ota_mux_t *ota;
  while ((ota = TAILQ_FIRST(&tdmi->tdmi_epg_grab)))
    epggrab_ota_destroy(ota);
}

/*
 * Destroy by module
 */
void epggrab_ota_destroy_by_module ( epggrab_module_ota_t *mod )
{
  epggrab_ota_mux_t *ota;
  while ((ota = TAILQ_FIRST(&mod->muxes)))
    epggrab_ota_destroy(ota);
}

/*
 * Register interest (called when useful data exists on the MUX
 * thus inserting it into the EPG scanning queue
 */
void epggrab_ota_register ( epggrab_ota_mux_t *ota, int timeout, int interval )
{
  int up = 0;
  ota->is_reg   = 1;
  if (timeout > ota->timeout) {
    up = 1;
    ota->timeout = timeout;
  }
  if (interval > ota->interval) {
    up = 1;
    ota->interval = interval;
  }

  if (up) {
    TAILQ_REMOVE(&ota_mux_all, ota, glob_link);
    TAILQ_INSERT_SORTED(&ota_mux_all, ota, glob_link, _ota_time_cmp);
  }
}

/*
 * State changes
 */

static void _epggrab_ota_finished ( epggrab_ota_mux_t *ota )
{
  /* Temporary link - delete it */
  if (!ota->is_reg)
    epggrab_ota_destroy(ota);

  /* Reinsert into reg queue */
  else {
    TAILQ_REMOVE(&ota_mux_all, ota, glob_link);
    TAILQ_INSERT_SORTED(&ota_mux_all, ota, glob_link, _ota_time_cmp);
  }
}

int epggrab_ota_begin     ( epggrab_ota_mux_t *ota )
{
  if (ota->state == EPGGRAB_OTA_MUX_IDLE) {
    tvhlog(LOG_INFO, ota->grab->id, "begin processing");
    ota->state = EPGGRAB_OTA_MUX_RUNNING;
    time(&ota->started);
    return 1;
  }
  return 0;
}

void epggrab_ota_complete  ( epggrab_ota_mux_t *ota )
{
  th_dvb_mux_instance_t *tdmi = ota->tdmi;

  if (ota->state != EPGGRAB_OTA_MUX_COMPLETE) {
    tvhlog(LOG_INFO, ota->grab->id, "processing complete");
    ota->state = EPGGRAB_OTA_MUX_COMPLETE;
    time(&ota->completed);

    /* Check others */
    TAILQ_FOREACH(ota, &tdmi->tdmi_epg_grab, tdmi_link) {
      if (ota->is_reg && ota->state == EPGGRAB_OTA_MUX_RUNNING) break;
    }

    /* All complete (bring timer forward) */
    if (!ota) {
      gtimer_arm(&tdmi->tdmi_adapter->tda_mux_scanner_timer,
                 dvb_adapter_mux_scanner, tdmi->tdmi_adapter, 20);
    }
  }
}

/* Reset */
void epggrab_ota_cancel    ( epggrab_ota_mux_t *ota )
{
  tvhlog(LOG_INFO, ota->grab->id, "processing cancelled");
  ota->state = EPGGRAB_OTA_MUX_IDLE;
  _epggrab_ota_finished(ota);
}

/* Same as complete */
void epggrab_ota_timeout ( epggrab_ota_mux_t *ota )
{
  epggrab_ota_complete(ota);
  _epggrab_ota_finished(ota);
}

/*
 * Check status
 */

int epggrab_ota_is_complete ( epggrab_ota_mux_t *ota )
{
  return ota->state == EPGGRAB_OTA_MUX_COMPLETE ||
         ota->state == EPGGRAB_OTA_MUX_TIMEDOUT;
}

int epggrab_ota_is_blocked ( epggrab_ota_mux_t *ota )
{
  time_t t;
  time(&t);
  return t < (ota->completed + ota->interval);
}
