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

void epggrab_mux_start ( dvb_mux_t *dm )
{
  epggrab_module_t *m;
  epggrab_module_ota_t *om;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->type == EPGGRAB_OTA) {
      om = (epggrab_module_ota_t*)m;
      if (om->start) om->start(om, dm);
    }
  }
}

void epggrab_mux_stop ( dvb_mux_t *dm, int timeout )
{
  // Note: the slightly akward list iteration here is because
  // _ota_cancel/delete can remove the object and free() it
  epggrab_ota_mux_t *a, *b;
  a = TAILQ_FIRST(&dm->dm_epg_grab);
  while (a) {
    b = TAILQ_NEXT(a, dm_link);
    if (a->dm == dm) {
      if (timeout)
        epggrab_ota_timeout(a);
      else
        epggrab_ota_cancel(a);
    }
    a = b;
  }
}

void epggrab_mux_delete ( dvb_mux_t *dm )
{
  epggrab_ota_destroy_by_dm(dm);
}

int epggrab_mux_period ( dvb_mux_t *dm )
{
  int period = 0;
  epggrab_ota_mux_t *ota;
  TAILQ_FOREACH(ota, &dm->dm_epg_grab, dm_link) {
    if (!ota->is_reg) continue;
    if (ota->timeout > period)
      period = ota->timeout;
  }
  return period;
}

dvb_mux_t *epggrab_mux_next ( dvb_network_t *dn )
{
  time_t now;
  epggrab_ota_mux_t *ota;
  time(&now);
  TAILQ_FOREACH(ota, &ota_mux_all, glob_link) {
    if (ota->interval + ota->completed > now) return NULL;
    if (!ota->is_reg) return NULL;
    if (ota->dm->dm_dn == dn) break;
  }
  return ota ? ota->dm : NULL;
}

/* **************************************************************************
 * Config
 * *************************************************************************/

static void _epggrab_ota_load_one ( epggrab_module_ota_t *mod, htsmsg_t *m )
{
  htsmsg_t *e;
  htsmsg_field_t *f;
  int onid, tsid, period, interval;
  const char *netname;

  HTSMSG_FOREACH(f, m) {
    if ((e = htsmsg_get_map_by_field(f))) {
      onid     = htsmsg_get_u32_or_default(m, "onid",     0);
      tsid     = htsmsg_get_u32_or_default(m, "tsid",     0);
      period   = htsmsg_get_u32_or_default(m, "period",   0);
      interval = htsmsg_get_u32_or_default(m, "interval", 0);
      netname  = htsmsg_get_str(m, "networkname");

      if (tsid)
        epggrab_ota_create_and_register_by_id(mod, onid, tsid,
                                              period, interval,
                                              netname);
    }
  }
}

void epggrab_ota_load ( void )
{
  htsmsg_t *m, *l;
  htsmsg_field_t *f;
  epggrab_module_t *mod;

  if ((m = hts_settings_load("epggrab/otamux"))) {
    HTSMSG_FOREACH(f, m) {
      mod = epggrab_module_find_by_id(f->hmf_name);
      if (!mod || mod->type != EPGGRAB_OTA) continue;
      if ((l = htsmsg_get_list_by_field(f)))
        _epggrab_ota_load_one((epggrab_module_ota_t*)mod, l);
    }
    htsmsg_destroy(m);
  }
}

static void _epggrab_ota_save_one ( htsmsg_t *m, epggrab_module_ota_t *mod )
{
  epggrab_ota_mux_t *ota;
  htsmsg_t *e, *l = NULL;
  TAILQ_FOREACH(ota, &mod->muxes, grab_link) {
    if (!l) l = htsmsg_create_list();
    e = htsmsg_create_map();

    const dvb_mux_t *dm = ota->dm;

    htsmsg_add_u32(e, "onid",     dm->dm_network_id);
    htsmsg_add_u32(e, "tsid",     dm->dm_transport_stream_id);
    htsmsg_add_u32(e, "period",   ota->timeout);
    htsmsg_add_u32(e, "interval", ota->interval);
    if (dm->dm_network_name)
      htsmsg_add_str(e, "networkname", dm->dm_network_name);
    htsmsg_add_msg(l, NULL, e);
  }
  if (l) htsmsg_add_msg(m, mod->id, l);
}

void epggrab_ota_save ( void )
{
  htsmsg_t *m = htsmsg_create_map();
  epggrab_module_t *mod;
    
  // TODO: there is redundancy in this saving, because each MUX can
  // be represented N times, due to one copy per adapter. But load
  // only requires one instance to find them all.
  LIST_FOREACH(mod, &epggrab_modules, link) {
    if (mod->type != EPGGRAB_OTA) continue;
    _epggrab_ota_save_one(m, (epggrab_module_ota_t*)mod);
  }

  hts_settings_save(m, "epggrab/otamux");
  htsmsg_destroy(m);
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
  ( epggrab_module_ota_t *mod, dvb_mux_t *dm )
{
  epggrab_ota_mux_t *ota;
  TAILQ_FOREACH(ota, &mod->muxes, grab_link) {
    if (ota->dm == dm) break;
  }
  return ota;
}

/*
 * Create (temporary) or Find (existing) link
 */
epggrab_ota_mux_t *epggrab_ota_create
  ( epggrab_module_ota_t *mod, dvb_mux_t *dm)
{
  /* Search for existing */
  epggrab_ota_mux_t *ota = epggrab_ota_find(mod, dm);

  /* Create new */
  if (!ota) {
    ota = calloc(1, sizeof(epggrab_ota_mux_t));
    ota->grab  = mod;
    ota->dm    = dm;
    TAILQ_INSERT_TAIL(&ota_mux_all, ota, glob_link);
    TAILQ_INSERT_TAIL(&dm->dm_epg_grab, ota, dm_link);
    TAILQ_INSERT_TAIL(&mod->muxes, ota, grab_link);

  } else {
    time_t now;
    time(&now);
    ota->state = EPGGRAB_OTA_MUX_IDLE;
  }
  return ota;
}

/*
 * Create and register using mux ID
 */
void epggrab_ota_create_and_register_by_id
  ( epggrab_module_ota_t *mod, int onid, int tsid, int period, int interval, 
    const char *networkname )
{
  dvb_network_t *dn;
  dvb_mux_t *dm;
  epggrab_ota_mux_t *ota;
  LIST_FOREACH(dn, &dvb_networks, dn_global_link) {
    LIST_FOREACH(dm, &dn->dn_muxes, dm_network_link) {
      if (dm->dm_transport_stream_id != tsid) continue;
      if (onid && dm->dm_network_id != onid) continue;
      if (networkname && (!dm->dm_network_name ||
                          strcmp(networkname, dm->dm_network_name))) continue;
      ota = epggrab_ota_create(mod, dm);
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
  TAILQ_REMOVE(&ota->dm->dm_epg_grab, ota, dm_link);
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
void epggrab_ota_destroy_by_dm ( dvb_mux_t *dm )
{
  epggrab_ota_mux_t *ota;
  while ((ota = TAILQ_FIRST(&dm->dm_epg_grab)))
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
    epggrab_ota_save();
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
    tvhlog(LOG_DEBUG, ota->grab->id, "begin processing");
    ota->state = EPGGRAB_OTA_MUX_RUNNING;
    time(&ota->started);
    return 1;
  }
  return 0;
}

void epggrab_ota_complete  ( epggrab_ota_mux_t *ota )
{
  dvb_mux_t *dm = ota->dm;

  if (ota->state != EPGGRAB_OTA_MUX_COMPLETE) {
    tvhlog(LOG_DEBUG, ota->grab->id, "processing complete");
    ota->state = EPGGRAB_OTA_MUX_COMPLETE;
    time(&ota->completed);

    /* Check others */
    TAILQ_FOREACH(ota, &dm->dm_epg_grab, dm_link) {
      if (ota->is_reg && ota->state == EPGGRAB_OTA_MUX_RUNNING) break;
    }
#if 0  // XXX(dvbreorg)
    /* All complete (bring timer forward) */
    if (!ota) {
      dvb_network_t *dn = dm->dm_dn;
      gtimer_arm(&dn->dn_mux_scanner_timer,
                 dvb_network_mux_scanner, dn, 20);
    }
#endif
  }
}

/* Reset */
void epggrab_ota_cancel    ( epggrab_ota_mux_t *ota )
{
  if (ota->state == EPGGRAB_OTA_MUX_RUNNING) {
    tvhlog(LOG_DEBUG, ota->grab->id, "processing cancelled");
    ota->state = EPGGRAB_OTA_MUX_IDLE;
  }
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
  if (ota->state == EPGGRAB_OTA_MUX_COMPLETE ||
      ota->state == EPGGRAB_OTA_MUX_TIMEDOUT) {
    if (epggrab_ota_is_blocked(ota))
      return 1;
    ota->state = EPGGRAB_OTA_MUX_IDLE;
  }
  return 0;
}

int epggrab_ota_is_blocked ( epggrab_ota_mux_t *ota )
{
  time_t t;
  time(&t);
  return t < (ota->completed + ota->interval);
}
