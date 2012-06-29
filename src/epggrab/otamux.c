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
 * TODO: this current implementation is a bit naff, possibly not as
 * efficient as it could be (due to use of the a single list). But generally
 * the length of the list will be short and it shouldn't significantly
 * impact the overall performance
 *
 * TODO: currently all muxes are treated independently, this might result
 *       in the same mux being scanned on multiple adapters, which is
 *       a bit pointless.
 *
 *       I think that at least _mux_next() and _ota_complete() need
 *       updating to handle this
 */

#include "tvheadend.h"
#include "queue.h"
#include "epg.h"
#include "dvb/dvb.h"
#include "epggrab.h"
#include "epggrab/private.h"

LIST_HEAD(,epggrab_ota_mux)   ota_mux_all;
TAILQ_HEAD(, epggrab_ota_mux) ota_mux_reg;

/* **************************************************************************
 * Global functions (called from DVB code)
 * *************************************************************************/

void epggrab_mux_start ( th_dvb_mux_instance_t *tdmi )
{
  epggrab_module_t *m;
  epggrab_module_ota_t *mod;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->type == EPGGRAB_OTA) {
      mod = (epggrab_module_ota_t*)m;
      mod->start(mod, tdmi);
    }
  }
}

void epggrab_mux_stop ( th_dvb_mux_instance_t *tdmi, int timeout )
{
  epggrab_ota_mux_t *a, *b;
  a = LIST_FIRST(&ota_mux_all);
  while (a) {
    b = LIST_NEXT(a, glob_link);
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
  epggrab_ota_mux_t *a, *b;
  a = LIST_FIRST(&ota_mux_all);
  while (a) {
    b = LIST_NEXT(a, glob_link);
    if (a->tdmi == tdmi)
      epggrab_ota_destroy(a);
    a = b;
  }
}

int epggrab_mux_period ( th_dvb_mux_instance_t *tdmi )
{
  int period = 0;
  epggrab_ota_mux_t *ota;
  TAILQ_FOREACH(ota, &ota_mux_reg, reg_link) {
    if (ota->timeout > period)
      period = ota->timeout;
  }
  return period;
}

th_dvb_mux_instance_t *epggrab_mux_next ( th_dvb_adapter_t *tda )
{
  epggrab_ota_mux_t *ota;
  TAILQ_FOREACH(ota, &ota_mux_reg, reg_link) {
    if (ota->tdmi->tdmi_adapter == tda) break;
  }
  if (!ota) return NULL;
  return ota->tdmi;
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
  time_t now, wa, wb;
  time(&now);
  epggrab_ota_mux_t *a = _a;
  epggrab_ota_mux_t *b = _b;
  wa = a->completed + a->interval;
  wb = b->completed + b->interval;
  if (wa < now && wb < now)
    return a->started - b->started;
  else
    return wa - wb;
}

/*
 * Create (temporary) or Find (existing) link
 */
epggrab_ota_mux_t *epggrab_ota_create
  ( epggrab_module_ota_t *mod, th_dvb_mux_instance_t *tdmi )
{
  /* Search for existing */
  epggrab_ota_mux_t *ota;
  LIST_FOREACH(ota, &ota_mux_all, glob_link) {
    if (ota->grab == mod && ota->tdmi == tdmi) break;
  }

  /* Create new */
  if (!ota) {
    ota = calloc(1, sizeof(epggrab_ota_mux_t));
    ota->grab  = mod;
    ota->tdmi  = tdmi;
    LIST_INSERT_HEAD(&ota_mux_all, ota, glob_link);

  } else {
    time_t now;
    time(&now);
    if (ota) ota->state = EPGGRAB_OTA_MUX_IDLE;

    /* Blocked */
    if (epggrab_ota_is_blocked(ota)) ota = NULL;
  }
  return ota;
}

/*
 * Destrory link (either because it was temporary OR mux deleted)
 */
void epggrab_ota_destroy ( epggrab_ota_mux_t *ota )
{
  LIST_REMOVE(ota, glob_link);
  if (ota->is_reg) TAILQ_REMOVE(&ota_mux_reg, ota, reg_link);
  if (ota->destroy) ota->destroy(ota);
  else {
    if (ota->status) free(ota->status);
    free(ota);
  }
}

/*
 * Register interest (called when useful data exists on the MUX
 * thus inserting it into the EPG scanning queue
 */
void epggrab_ota_register ( epggrab_ota_mux_t *ota, int timeout, int interval )
{
  // TODO: handle changes to the interval/timeout?
  if (!ota->is_reg) {
    ota->timeout  = timeout;
    ota->interval = interval;
    ota->is_reg   = 1;
    TAILQ_INSERT_SORTED(&ota_mux_reg, ota, reg_link, _ota_time_cmp);
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
    TAILQ_REMOVE(&ota_mux_reg, ota, reg_link);
    TAILQ_INSERT_SORTED(&ota_mux_reg, ota, reg_link, _ota_time_cmp);
  }
}

int epggrab_ota_begin     ( epggrab_ota_mux_t *ota )
{
  if (ota->state == EPGGRAB_OTA_MUX_IDLE) {
    ota->state = EPGGRAB_OTA_MUX_RUNNING;
    time(&ota->started);
    return 1;
  }
  return 0;
}

void epggrab_ota_complete  ( epggrab_ota_mux_t *ota )
{
  if (ota->state != EPGGRAB_OTA_MUX_COMPLETE) {
    ota->state = EPGGRAB_OTA_MUX_COMPLETE;
    time(&ota->completed);
    _epggrab_ota_finished(ota);

    // TODO: need to inform tdmi
  }
}

/* Reset */
void epggrab_ota_cancel    ( epggrab_ota_mux_t *ota )
{
  ota->state = EPGGRAB_OTA_MUX_IDLE;
  _epggrab_ota_finished(ota);
}

/* Same as complete */
void epggrab_ota_timeout ( epggrab_ota_mux_t *ota )
{
  epggrab_ota_complete(ota);
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
