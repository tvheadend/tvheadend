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
#include "settings.h"
#include "epg.h"
#include "epggrab.h"
#include "epggrab/private.h"
#include "input/mpegts.h"
#include "subscriptions.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

RB_HEAD(,epggrab_ota_mux)   epggrab_ota_all;
LIST_HEAD(,epggrab_ota_mux) epggrab_ota_pending;
LIST_HEAD(,epggrab_ota_mux) epggrab_ota_active;

gtimer_t                    epggrab_ota_pending_timer;
gtimer_t                    epggrab_ota_active_timer;

SKEL_DECLARE(epggrab_ota_mux_skel, epggrab_ota_mux_t);

static void epggrab_ota_active_timer_cb ( void *p );
static void epggrab_ota_pending_timer_cb ( void *p );

static void epggrab_ota_save ( epggrab_ota_mux_t *ota );

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
epggrab_ota_done ( epggrab_ota_mux_t *ota, int timeout )
{
  mpegts_mux_t *mm;

  LIST_REMOVE(ota, om_q_link);
  ota->om_active = 0;
  ota->om_when   = dispatch_clock + epggrab_ota_period(ota);
  LIST_INSERT_SORTED(&epggrab_ota_pending, ota, om_q_link, om_time_cmp);

  /* Remove subscriber */
  if ((mm = mpegts_mux_find(ota->om_mux_uuid)))
    mpegts_mux_unsubscribe_by_name(mm, "epggrab");

  /* Re-arm */
  if (LIST_FIRST(&epggrab_ota_pending) == ota)
    epggrab_ota_pending_timer_cb(NULL);
  
  /* Remove from active */
  if (!timeout) {
    epggrab_ota_active_timer_cb(NULL);
  }
}

static void
epggrab_ota_start ( epggrab_ota_mux_t *om )
{
  epggrab_ota_map_t *map;
  om->om_when   = dispatch_clock + epggrab_ota_timeout(om);
  om->om_active = 1;
  LIST_INSERT_SORTED(&epggrab_ota_active, om, om_q_link, om_time_cmp);
  if (LIST_FIRST(&epggrab_ota_active) == om)
    epggrab_ota_active_timer_cb(NULL);
  LIST_FOREACH(map, &om->om_modules, om_link) {
    map->om_complete = 0;
    tvhdebug(map->om_module->id, "grab started");
  }
}

/* **************************************************************************
 * MPEG-TS listener
 * *************************************************************************/

static void
epggrab_mux_start0 ( mpegts_mux_t *mm, int force )
{
  epggrab_module_t *m;
  epggrab_module_ota_t *om;
  epggrab_ota_mux_t *ota;

  /* Already started */
  if (!force) {
    LIST_FOREACH(ota, &epggrab_ota_active, om_q_link)
      if (!strcmp(ota->om_mux_uuid, idnode_uuid_as_str(&mm->mm_id)))
        return;
  }

  /* Check if already active */
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->type == EPGGRAB_OTA) {
      om = (epggrab_module_ota_t*)m;
      if (om->start) om->start(om, mm);
    }
  }
}

static void
epggrab_mux_start ( mpegts_mux_t *mm, void *p )
{
  epggrab_mux_start0(mm, 0);
}

static void
epggrab_mux_stop ( mpegts_mux_t *mm, void *p )
{
  epggrab_ota_mux_t *ota;
  while ((ota = LIST_FIRST(&epggrab_ota_active)))
    epggrab_ota_done(ota, 0);
}

/* **************************************************************************
 * Module methods
 * *************************************************************************/

epggrab_ota_mux_t *
epggrab_ota_register
  ( epggrab_module_ota_t *mod, mpegts_mux_t *mm,
    int interval, int timeout )
{
  int save = 0;
  epggrab_ota_map_t *map;
  epggrab_ota_mux_t *ota;

  /* Find mux entry */
  const char *uuid = idnode_uuid_as_str(&mm->mm_id);
  SKEL_ALLOC(epggrab_ota_mux_skel);
  epggrab_ota_mux_skel->om_mux_uuid = (char*)uuid;

  ota = RB_INSERT_SORTED(&epggrab_ota_all, epggrab_ota_mux_skel, om_global_link, om_id_cmp);
  if (!ota) {
    char buf[256];
    mm->mm_display_name(mm, buf, sizeof(buf));
    tvhinfo(mod->id, "registering mux %s", buf);
    ota  = epggrab_ota_mux_skel;
    SKEL_USED(epggrab_ota_mux_skel);
    ota->om_mux_uuid = strdup(uuid);
    ota->om_when     = dispatch_clock + epggrab_ota_timeout(ota);
    ota->om_active   = 1;
    LIST_INSERT_SORTED(&epggrab_ota_active, ota, om_q_link, om_time_cmp);
    if (LIST_FIRST(&epggrab_ota_active) == ota)
      epggrab_ota_active_timer_cb(NULL);
    save = 1;
  }
  
  /* Find module entry */
  LIST_FOREACH(map, &ota->om_modules, om_link)
    if (map->om_module == mod)
      break;
  if (!map) {
    map = calloc(1, sizeof(epggrab_ota_map_t));
    map->om_module   = mod;
    map->om_timeout  = timeout;
    map->om_interval = interval;
    LIST_INSERT_HEAD(&ota->om_modules, map, om_link);
    save = 1;
  }

  /* Save config */
  if (save) epggrab_ota_save(ota);

  return ota;
}

void
epggrab_ota_complete
  ( epggrab_module_ota_t *mod, epggrab_ota_mux_t *ota )
{
  int done = 1;
  epggrab_ota_map_t *map;
  tvhdebug(mod->id, "grab complete");

  /* Test for completion */
  LIST_FOREACH(map, &ota->om_modules, om_link) {
    if (map->om_module == mod) {
      map->om_complete = 1;
    } else if (!map->om_complete) {
      done = 0;
    }
  }
  if (!done) return;

  /* Done */
  epggrab_ota_done(ota, 0);
}

/* **************************************************************************
 * Timer callback
 * *************************************************************************/

static void
epggrab_ota_active_timer_cb ( void *p )
{
  epggrab_ota_mux_t *om = LIST_FIRST(&epggrab_ota_active);
  gtimer_disarm(&epggrab_ota_active_timer);

  lock_assert(&global_lock);
  if (!om)
    return;

  /* Double check */
  if (om->om_when > dispatch_clock)
    goto done;

  /* Re-queue */
  epggrab_ota_done(om, 1);

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
  gtimer_disarm(&epggrab_ota_pending_timer);

  lock_assert(&global_lock);
  if (!om)
    return;
  
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

  /* Check we have modules attached and enabled */
  LIST_FOREACH(map, &om->om_modules, om_link) {
    if (map->om_module->enabled)
      break;
  }
  if (!map) {
    char name[256];
    mm->mm_display_name(mm, name, sizeof(name));
    tvhdebug("epggrab", "no modules attached to %s, check again later", name);
    om->om_when = dispatch_clock + epggrab_ota_period(om) / 2;
    LIST_INSERT_SORTED(&epggrab_ota_pending, om, om_q_link, om_time_cmp);
    goto done;
  }

  /* Insert into active (assume success) */
  // Note: if we don't do this the subscribe below can result in a mux
  //       start call which means we call it a second time below
  epggrab_ota_start(om);

  /* Subscribe to the mux */
  if (mpegts_mux_subscribe(mm, "epggrab", SUBSCRIPTION_PRIO_EPG)) {
    LIST_REMOVE(om, om_q_link);
    om->om_active = 0;
    om->om_when   = dispatch_clock + epggrab_ota_period(om) / 2;
    LIST_INSERT_SORTED(&epggrab_ota_pending, om, om_q_link, om_time_cmp);
  } else {
    epggrab_mux_start0(mm, 1);
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

static void
epggrab_ota_save ( epggrab_ota_mux_t *ota )
{
  epggrab_ota_map_t *map;
  htsmsg_t *e, *l, *c = htsmsg_create_map();

  htsmsg_add_u32(c, "timeout",  ota->om_timeout);
  htsmsg_add_u32(c, "interval", ota->om_interval);
  l = htsmsg_create_list();
  LIST_FOREACH(map, &ota->om_modules, om_link) {
    e = htsmsg_create_map();
    htsmsg_add_str(e, "id", map->om_module->id);
    htsmsg_add_u32(e, "timeout", map->om_timeout);
    htsmsg_add_u32(e, "interval", map->om_interval);
    htsmsg_add_msg(l, NULL, e);
  }
  htsmsg_add_msg(c, "modules", l);
  hts_settings_save(c, "epggrab/otamux/%s", ota->om_mux_uuid);
  htsmsg_destroy(c);
}

static void
epggrab_ota_load_one
  ( const char *uuid, htsmsg_t *c )
{
  htsmsg_t *l, *e;
  htsmsg_field_t *f;
  mpegts_mux_t *mm;
  epggrab_module_ota_t *mod;
  epggrab_ota_mux_t *ota;
  epggrab_ota_map_t *map;
  const char *id;
  
  mm = mpegts_mux_find(uuid);
  if (!mm) {
    hts_settings_remove("epggrab/otamux/%s", uuid);
    return;
  }

  ota = calloc(1, sizeof(epggrab_ota_mux_t));
  ota->om_mux_uuid = strdup(uuid);
  ota->om_timeout  = htsmsg_get_u32_or_default(c, "timeout", 0);
  ota->om_interval = htsmsg_get_u32_or_default(c, "interval", 0);
  if (RB_INSERT_SORTED(&epggrab_ota_all, ota, om_global_link, om_id_cmp)) {
    free(ota->om_mux_uuid);
    free(ota);
    return;
  }
  LIST_INSERT_SORTED(&epggrab_ota_pending, ota, om_q_link, om_time_cmp);
  
  if (!(l = htsmsg_get_list(c, "modules"))) return;
  HTSMSG_FOREACH(f, l) {
    if (!(e   = htsmsg_field_get_map(f))) continue;
    if (!(id  = htsmsg_get_str(e, "id"))) continue;
    if (!(mod = (epggrab_module_ota_t*)epggrab_module_find_by_id(id)))
      continue;
    
    map = calloc(1, sizeof(epggrab_ota_map_t));
    map->om_module   = mod;
    map->om_timeout  = htsmsg_get_u32_or_default(e, "timeout", 0);
    map->om_interval = htsmsg_get_u32_or_default(e, "interval", 0);
    LIST_INSERT_HEAD(&ota->om_modules, map, om_link);
  }
}

void
epggrab_ota_init ( void )
{
  htsmsg_t *c, *m;
  htsmsg_field_t *f;
  char path[1024];
  struct stat st;

  /* Add listener */
  static mpegts_listener_t ml = {
    .ml_mux_start = epggrab_mux_start,
    .ml_mux_stop  = epggrab_mux_stop,
  };
  mpegts_add_listener(&ml);

  /* Delete old config */
  hts_settings_buildpath(path, sizeof(path), "epggrab/otamux");
  if (!lstat(path, &st))
    if (!S_ISDIR(st.st_mode))
      hts_settings_remove("epggrab/otamux");
  
  /* Load config */
  if ((c = hts_settings_load_r(1, "epggrab/otamux"))) {
    HTSMSG_FOREACH(f, c) {
      if (!(m  = htsmsg_field_get_map(f))) continue;
      epggrab_ota_load_one(f->hmf_name, m); 
    }
    htsmsg_destroy(c);
  }
  
  /* Init timer (immediate call after full init) */
  if (LIST_FIRST(&epggrab_ota_pending))
    gtimer_arm_abs(&epggrab_ota_pending_timer, epggrab_ota_pending_timer_cb,
                   NULL, 0);
}

static void
epggrab_ota_free ( epggrab_ota_mux_t *ota )
{
  epggrab_ota_map_t *map;

  LIST_REMOVE(ota, om_q_link);
  while ((map = LIST_FIRST(&ota->om_modules)) != NULL) {
    LIST_REMOVE(map, om_link);
    free(map);
  }
  free(ota->om_mux_uuid);
  free(ota);
}

void
epggrab_ota_done_ ( void )
{
  epggrab_ota_mux_t *ota;

  pthread_mutex_lock(&global_lock);
  while ((ota = LIST_FIRST(&epggrab_ota_active)) != NULL)
    epggrab_ota_free(ota);
  while ((ota = LIST_FIRST(&epggrab_ota_pending)) != NULL)
    epggrab_ota_free(ota);
  pthread_mutex_unlock(&global_lock);
  SKEL_FREE(epggrab_ota_mux_skel);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
