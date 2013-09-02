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

#include <string.h>

#include "tvheadend.h"
#include "queue.h"
#include "epg.h"
#include "epggrab.h"
#include "epggrab/private.h"
#include "input/mpegts.h"
#include "subscriptions.h"

RB_HEAD(,epggrab_ota_mux)   epggrab_ota_all;
LIST_HEAD(,epggrab_ota_mux) epggrab_ota_pending;
LIST_HEAD(,epggrab_ota_mux) epggrab_ota_active;

gtimer_t                    epggrab_ota_pending_timer;
gtimer_t                    epggrab_ota_active_timer;

static void epggrab_ota_active_timer_cb ( void *p );
static void epggrab_ota_pending_timer_cb ( void *p );

/* **************************************************************************
 * Utilities
 * *************************************************************************/

static int
om_time_cmp ( epggrab_ota_mux_t *a, epggrab_ota_mux_t *b )
{
  return (a->om_when - b->om_when);
}

static int
om_id_cmp   ( epggrab_ota_mux_t *a, epggrab_ota_mux_t *b )
{
  return strcmp(a->om_mux_uuid, b->om_mux_uuid);
}

#define EPGGRAB_OTA_MIN_PERIOD  600
#define EPGGRAB_OTA_MIN_TIMEOUT  30

static int
epggrab_ota_period ( epggrab_ota_mux_t *ota )
{
  int period = 0;
  epggrab_ota_map_t *map;

  if (ota->om_interval)
    period = ota->om_interval;
  else {
    LIST_FOREACH(map, &ota->om_modules, om_link)

      if (!period || map->om_interval < period)
        period = map->om_interval;
  }

  if (period < EPGGRAB_OTA_MIN_PERIOD)
    period = EPGGRAB_OTA_MIN_PERIOD;
  
  return period;
}

static int
epggrab_ota_timeout ( epggrab_ota_mux_t *ota )
{
  int timeout = 0;
  epggrab_ota_map_t *map;

  if (ota->om_timeout)
    timeout = ota->om_timeout;
  else {
    LIST_FOREACH(map, &ota->om_modules, om_link)
      if (map->om_timeout > timeout)
        timeout = map->om_timeout;
  }

  if (timeout < EPGGRAB_OTA_MIN_TIMEOUT)
    timeout = EPGGRAB_OTA_MIN_TIMEOUT;

  return timeout;
}

static void
epggrab_ota_done ( epggrab_ota_mux_t *ota, int cancel, int timeout )
{
  LIST_REMOVE(ota, om_q_link);
  ota->om_when   = dispatch_clock + 10;//epggrab_ota_period(ota);
  ota->om_active = 0;
  LIST_INSERT_SORTED(&epggrab_ota_pending, ota, om_q_link, om_time_cmp);

  /* Re-arm */
  if (LIST_FIRST(&epggrab_ota_pending) == ota)
    epggrab_ota_pending_timer_cb(NULL);
  
  /* Remove from active */
  if (!timeout) {
    gtimer_disarm(&epggrab_ota_active_timer);
    epggrab_ota_active_timer_cb(NULL);
  }

  /* Remove subscription */
  if (ota->om_sub) {
    subscription_unsubscribe(ota->om_sub);
    free(ota->om_sub);
    ota->om_sub = NULL;
  }
}

/* **************************************************************************
 * MPEG-TS listener
 * *************************************************************************/

static void
epggrab_mux_start ( mpegts_mux_t *mm, void *p )
{
  epggrab_module_t *m;
  epggrab_module_ota_t *om;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->type == EPGGRAB_OTA) {
      om = (epggrab_module_ota_t*)m;
      if (om->start) om->start(om, mm);
    }
  }
}

static void
epggrab_mux_stop ( mpegts_mux_t *mm, void *p )
{
  epggrab_ota_mux_t *ota;
  RB_FOREACH(ota, &epggrab_ota_all, om_global_link) {
    const char *uuid = idnode_uuid_as_str(&mm->mm_id);
    if (!strcmp(ota->om_mux_uuid, uuid))
      break;
  }
  if (ota && ota->om_active)
    epggrab_ota_done(ota, 1, 0);
}

void
epggrab_ota_init ( void )
{
  static mpegts_listener_t ml = {
    .ml_mux_start = epggrab_mux_start,
    .ml_mux_stop  = epggrab_mux_stop,
  };
  mpegts_add_listener(&ml);
}

/* **************************************************************************
 * Completion handling
 * *************************************************************************/

/* **************************************************************************
 * Module methods
 * *************************************************************************/

epggrab_ota_mux_t *
epggrab_ota_register
  ( epggrab_module_ota_t *mod, mpegts_mux_t *mm,
    int interval, int timeout )
{
  static epggrab_ota_mux_t *skel = NULL;
  epggrab_ota_map_t *map;
  epggrab_ota_mux_t *ota;

  /* Find mux entry */
  const char *uuid = idnode_uuid_as_str(&mm->mm_id);
  if (!skel)
    skel = calloc(1, sizeof(epggrab_ota_mux_t));
  skel->om_mux_uuid = (char*)uuid;

  ota = RB_INSERT_SORTED(&epggrab_ota_all, skel, om_global_link, om_id_cmp);
  if (!ota) {
    ota  = skel;
    skel = NULL;
    ota->om_mux_uuid = strdup(uuid);
    ota->om_active   = 1;
    // idnode_link(&ota->om_id, NULL);
    LIST_INSERT_SORTED(&epggrab_ota_pending, ota, om_q_link, om_time_cmp);
    // TODO: save config
    // TODO: generic creation routine (grid?)
  }
  
  /* Find module entry */
  LIST_FOREACH(map, &ota->om_modules, om_link)
    if (map->om_module == mod)
      break;
  if (!mod) {
    map = calloc(1, sizeof(epggrab_ota_map_t));
    map->om_module   = mod;
    map->om_timeout  = timeout;
    map->om_interval = interval;
    LIST_INSERT_HEAD(&ota->om_modules, map, om_link);
  }

  return ota;
}

void
epggrab_ota_complete
  ( epggrab_module_ota_t *mod, epggrab_ota_mux_t *ota )
{
  int done = 1;
  epggrab_ota_map_t *map;

  /* Just for completion */
  LIST_FOREACH(map, &ota->om_modules, om_link) {
    if (map->om_module == mod)
      map->om_complete = 1;
    else if (!map->om_complete)
      done = 0;
  }
  if (!done) return;

  /* Done */
  epggrab_ota_done(ota, 0, 0);
}

/* **************************************************************************
 * Timer callback
 * *************************************************************************/

static void
epggrab_ota_active_timer_cb ( void *p )
{
  epggrab_ota_mux_t *om = LIST_FIRST(&epggrab_ota_active);

  lock_assert(&global_lock);
  if (!om)
    return;

  /* Double check */
  if (om->om_when > dispatch_clock)
    goto done;

  /* Re-queue */
  epggrab_ota_done(om, 0, 1);

done:
  om = LIST_FIRST(&epggrab_ota_active);
  if (om)
    gtimer_arm_abs(&epggrab_ota_active_timer, epggrab_ota_active_timer_cb,
                   NULL, om->om_when);
}

static void
epggrab_ota_pending_timer_cb ( void *p )
{
  epggrab_ota_map_t *map;
  epggrab_ota_mux_t *om = LIST_FIRST(&epggrab_ota_pending);
  mpegts_mux_t *mm;
  th_subscription_t *s;

  lock_assert(&global_lock);
  if (!om)
    return;
  assert(om->om_sub == NULL);
  
  /* Double check */
  if (om->om_when > dispatch_clock)
    goto done;
  LIST_REMOVE(om, om_q_link);

  /* Find the mux */
  extern const idclass_t mpegts_mux_class;
  mm = mpegts_mux_find(om->om_mux_uuid);
  if (!mm) {
    RB_REMOVE(&epggrab_ota_all, om, om_global_link);
    while ((map = LIST_FIRST(&om->om_modules))) {
      LIST_REMOVE(map, om_link);
      free(map);
    }
    free(om->om_mux_uuid);
    free(om);
    goto done;
  }

  /* Subscribe to the mux */
  // TODO: remove hardcoded weight
  s = subscription_create_from_mux(mm, 2, "epggrab", NULL,
                                   SUBSCRIPTION_NONE, NULL, NULL, NULL);
  if (s) {
    om->om_sub  = s;
    om->om_when = dispatch_clock + epggrab_ota_period(om) / 2;
    LIST_INSERT_SORTED(&epggrab_ota_pending, om, om_q_link, om_time_cmp);
  } else {
    om->om_when = dispatch_clock + epggrab_ota_timeout(om);
    LIST_INSERT_SORTED(&epggrab_ota_active, om, om_q_link, om_time_cmp);
    if (LIST_FIRST(&epggrab_ota_active) == om)
      epggrab_ota_active_timer_cb(NULL);
    LIST_FOREACH(map, &om->om_modules, om_link)
      map->om_complete = 0;
  }

done:
  om = LIST_FIRST(&epggrab_ota_pending);
  if (om)
    gtimer_arm_abs(&epggrab_ota_pending_timer, epggrab_ota_pending_timer_cb,
                   NULL, om->om_when);
}

/* **************************************************************************
 * Config
 * *************************************************************************/
