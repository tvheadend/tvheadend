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
#include "input.h"
#include "subscriptions.h"
#include "cron.h"
#include "dbus.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define EPGGRAB_OTA_MIN_TIMEOUT     30
#define EPGGRAB_OTA_MAX_TIMEOUT   7200

#define EPGGRAB_OTA_DONE_COMPLETE    0
#define EPGGRAB_OTA_DONE_TIMEOUT     1
#define EPGGRAB_OTA_DONE_NO_DATA     2
#define EPGGRAB_OTA_DONE_STOLEN      3

typedef TAILQ_HEAD(epggrab_ota_head,epggrab_ota_mux) epggrab_ota_head_t;

cron_multi_t                *epggrab_ota_cron_multi;

RB_HEAD(,epggrab_ota_mux)    epggrab_ota_all;
epggrab_ota_head_t           epggrab_ota_pending;
epggrab_ota_head_t           epggrab_ota_active;

gtimer_t                     epggrab_ota_kick_timer;
gtimer_t                     epggrab_ota_start_timer;

int                          epggrab_ota_running;
int                          epggrab_ota_pending_flag;

pthread_mutex_t              epggrab_ota_mutex;

SKEL_DECLARE(epggrab_ota_mux_skel, epggrab_ota_mux_t);
SKEL_DECLARE(epggrab_svc_link_skel, epggrab_ota_svc_link_t);

static void epggrab_ota_kick ( int delay );

static void epggrab_ota_timeout_cb ( void *p );
static void epggrab_ota_data_timeout_cb ( void *p );
static void epggrab_ota_kick_cb ( void *p );

static void epggrab_mux_start ( mpegts_mux_t *mm, void *p );

static void epggrab_ota_save ( epggrab_ota_mux_t *ota );

static void epggrab_ota_free ( epggrab_ota_head_t *head, epggrab_ota_mux_t *ota );

/* **************************************************************************
 * Utilities
 * *************************************************************************/

static int
om_id_cmp   ( epggrab_ota_mux_t *a, epggrab_ota_mux_t *b )
{
  return strcmp(a->om_mux_uuid, b->om_mux_uuid);
}

static int
om_svcl_cmp ( epggrab_ota_svc_link_t *a, epggrab_ota_svc_link_t *b )
{
  return strcmp(a->uuid, b->uuid);
}

static int
epggrab_ota_timeout_get ( void )
{
  int timeout = epggrab_conf.ota_timeout;

  if (timeout < EPGGRAB_OTA_MIN_TIMEOUT)
    timeout = EPGGRAB_OTA_MIN_TIMEOUT;
  if (timeout > EPGGRAB_OTA_MAX_TIMEOUT)
    timeout = EPGGRAB_OTA_MAX_TIMEOUT;

  return timeout;
}

static int
epggrab_ota_queue_one( epggrab_ota_mux_t *om )
{
  om->om_done = 0;
  om->om_requeue = 1;
  if (om->om_q_type != EPGGRAB_OTA_MUX_IDLE)
    return 0;
  TAILQ_INSERT_TAIL(&epggrab_ota_pending, om, om_q_link);
  om->om_q_type = EPGGRAB_OTA_MUX_PENDING;
  return 1;
}

void
epggrab_ota_queue_mux( mpegts_mux_t *mm )
{
  const char *id;
  epggrab_ota_mux_t *om;
  int epg_flag;

  if (!mm)
    return;

  lock_assert(&global_lock);

  id = idnode_uuid_as_sstr(&mm->mm_id);
  epg_flag = mm->mm_is_epg(mm);
  if (epg_flag < 0 || epg_flag == MM_EPG_DISABLE)
    return;
  RB_FOREACH(om, &epggrab_ota_all, om_global_link)
    if (!strcmp(om->om_mux_uuid, id)) {
      if (epggrab_ota_queue_one(om))
        epggrab_ota_kick(4);
      break;
    }
}

static void
epggrab_ota_requeue ( void )
{
  epggrab_ota_mux_t *om;

  /*
   * enqueue all muxes, but ommit the delayed ones (active+pending)
   */
  RB_FOREACH(om, &epggrab_ota_all, om_global_link)
    epggrab_ota_queue_one(om);
}

static void
epggrab_ota_kick ( int delay )
{
  /* next round is pending? queue rest of ota muxes */
  if (epggrab_ota_pending_flag) {
    epggrab_ota_pending_flag = 0;
    epggrab_ota_requeue();
  }

  if (TAILQ_EMPTY(&epggrab_ota_pending))
    return;

  gtimer_arm(&epggrab_ota_kick_timer, epggrab_ota_kick_cb, NULL, delay);
}

static void
epggrab_ota_done ( epggrab_ota_mux_t *om, int reason )
{
  static const char *reasons[] = {
    [EPGGRAB_OTA_DONE_COMPLETE]    = "complete",
    [EPGGRAB_OTA_DONE_TIMEOUT]     = "timeout",
    [EPGGRAB_OTA_DONE_NO_DATA]     = "no data",
    [EPGGRAB_OTA_DONE_STOLEN]      = "stolen"
  };
  char name[256];
  mpegts_mux_t *mm;
  epggrab_ota_map_t *map;

  if (om->om_save)
    epggrab_ota_save(om);

  mm = mpegts_mux_find(om->om_mux_uuid);
  mpegts_mux_nice_name(mm, name, sizeof(name));
  tvhdebug("epggrab", "grab done for %s (%s)", name, reasons[reason]);

  gtimer_disarm(&om->om_timer);
  gtimer_disarm(&om->om_data_timer);

  assert(om->om_q_type == EPGGRAB_OTA_MUX_ACTIVE);
  TAILQ_REMOVE(&epggrab_ota_active, om, om_q_link);
  om->om_q_type = EPGGRAB_OTA_MUX_IDLE;
  if (reason == EPGGRAB_OTA_DONE_STOLEN) {
    /* Do not requeue completed muxes */
    if (!om->om_done && om->om_requeue) {
      TAILQ_INSERT_HEAD(&epggrab_ota_pending, om, om_q_link);
      om->om_q_type = EPGGRAB_OTA_MUX_PENDING;
    } else {
      om->om_requeue = 0;
    }
  } else if (reason == EPGGRAB_OTA_DONE_TIMEOUT) {
    om->om_requeue = 0;
    LIST_FOREACH(map, &om->om_modules, om_link)
      if (!map->om_complete)
        tvhlog(LOG_WARNING, "epggrab", "%s - data completion timeout for %s", map->om_module->name, name);
  } else {
    om->om_requeue = 0;
  }

  /* Remove subscriber */
  if (mm)
    mpegts_mux_unsubscribe_by_name(mm, "epggrab");

  /* Kick - try start waiting muxes */
  epggrab_ota_kick(1);
}

static void
epggrab_ota_complete_mark ( epggrab_ota_mux_t *om, int done )
{
  om->om_done = 1;
  if (!om->om_complete) {
    om->om_complete = 1;
    epggrab_ota_save(om);
  }
}

static void
epggrab_ota_start ( epggrab_ota_mux_t *om, mpegts_mux_t *mm )
{
  epggrab_module_t  *m;
  epggrab_ota_map_t *map;
  char *modname = om->om_force_modname;
  mpegts_mux_instance_t *mmi = mm->mm_active;
  int grace;

  /* In pending queue? Remove.. */
  if (om->om_q_type == EPGGRAB_OTA_MUX_PENDING)
    TAILQ_REMOVE(&epggrab_ota_pending, om, om_q_link);
  else
    assert(om->om_q_type == EPGGRAB_OTA_MUX_IDLE);

  TAILQ_INSERT_TAIL(&epggrab_ota_active, om, om_q_link);
  om->om_q_type = EPGGRAB_OTA_MUX_ACTIVE;
  grace = mpegts_input_grace(mmi->mmi_input, mm);
  gtimer_arm(&om->om_timer, epggrab_ota_timeout_cb, om,
             epggrab_ota_timeout_get() + grace);
  gtimer_arm(&om->om_data_timer, epggrab_ota_data_timeout_cb, om,
             30 + grace); /* 30 seconds to receive any EPG info */
  if (modname) {
    LIST_FOREACH(m, &epggrab_modules, link)
      if (!strcmp(m->id, modname)) {
        epggrab_ota_register((epggrab_module_ota_t *)m, om, mm);
        break;
      }
  }
  LIST_FOREACH(map, &om->om_modules, om_link) {
    map->om_first    = 1;
    map->om_forced   = 0;
    if (modname && !strcmp(modname, map->om_module->id))
      map->om_forced = 1;
    map->om_complete = 0;
    if (map->om_module->start(map, mm) < 0) {
      map->om_complete = 1;
    } else
      tvhdebug(map->om_module->id, "grab started");
  }
}

/* **************************************************************************
 * MPEG-TS listener
 * *************************************************************************/

static void
epggrab_mux_start ( mpegts_mux_t *mm, void *p )
{
  epggrab_module_t  *m;
  epggrab_ota_mux_t *ota;
  const char *uuid = idnode_uuid_as_sstr(&mm->mm_id);

  /* Already started */
  TAILQ_FOREACH(ota, &epggrab_ota_active, om_q_link)
    if (!strcmp(ota->om_mux_uuid, uuid))
      return;

  /* Register all modules */
  ota = NULL;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->type == EPGGRAB_OTA && m->enabled)
      ota = epggrab_ota_register((epggrab_module_ota_t *)m, ota, mm);
  }

  if (ota)
    epggrab_ota_start(ota, mm);
}

static void
epggrab_mux_stop ( mpegts_mux_t *mm, void *p, int reason )
{
  epggrab_ota_mux_t *ota;
  const char *uuid = idnode_uuid_as_sstr(&mm->mm_id);
  int done = EPGGRAB_OTA_DONE_STOLEN;

  if (reason == SM_CODE_NO_INPUT)
    done = EPGGRAB_OTA_DONE_NO_DATA;
  tvhtrace("epggrab", "mux %p (%s) stop", mm, uuid);
  TAILQ_FOREACH(ota, &epggrab_ota_active, om_q_link)
    if (!strcmp(ota->om_mux_uuid, uuid)) {
      epggrab_ota_done(ota, done);
      break;
    }
}

/* **************************************************************************
 * Module methods
 * *************************************************************************/

epggrab_ota_mux_t *
epggrab_ota_register
  ( epggrab_module_ota_t *mod, epggrab_ota_mux_t *ota, mpegts_mux_t *mm )
{
  int save = 0;
  epggrab_ota_map_t *map;

  if (!epggrab_ota_running)
    return NULL;

  if (ota == NULL) {
    /* Find mux entry */
    char ubuf[UUID_HEX_SIZE];
    const char *uuid = idnode_uuid_as_str(&mm->mm_id, ubuf);
    SKEL_ALLOC(epggrab_ota_mux_skel);
    epggrab_ota_mux_skel->om_mux_uuid = (char*)uuid;

    ota = RB_INSERT_SORTED(&epggrab_ota_all, epggrab_ota_mux_skel, om_global_link, om_id_cmp);
    if (!ota) {
      char buf[256];
      mpegts_mux_nice_name(mm, buf, sizeof(buf));
      tvhinfo(mod->id, "registering mux %s", buf);
      ota  = epggrab_ota_mux_skel;
      SKEL_USED(epggrab_ota_mux_skel);
      ota->om_mux_uuid = strdup(uuid);
      TAILQ_INSERT_TAIL(&epggrab_ota_pending, ota, om_q_link);
      ota->om_q_type = EPGGRAB_OTA_MUX_PENDING;
      if (TAILQ_FIRST(&epggrab_ota_pending) == ota)
        epggrab_ota_kick(1);
      save = 1;
    }
  }
  
  /* Find module entry */
  LIST_FOREACH(map, &ota->om_modules, om_link)
    if (map->om_module == mod)
      break;
  if (!map) {
    map = calloc(1, sizeof(epggrab_ota_map_t));
    RB_INIT(&map->om_svcs);
    map->om_module   = mod;
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
  lock_assert(&global_lock);
  tvhdebug(mod->id, "grab complete");

  /* Test for completion */
  LIST_FOREACH(map, &ota->om_modules, om_link) {
    if (map->om_module == mod) {
      map->om_complete = 1;
    } else if (!map->om_complete && !map->om_first) {
      done = 0;
    }
    tvhtrace("epggrab", "%s complete %i first %i",
                        map->om_module->id, map->om_complete, map->om_first);
  }

  epggrab_ota_complete_mark(ota, done);

  if (!done) return;

  /* Done */
  if (ota->om_q_type == EPGGRAB_OTA_MUX_ACTIVE)
    epggrab_ota_done(ota, EPGGRAB_OTA_DONE_COMPLETE);
  else if (ota->om_q_type == EPGGRAB_OTA_MUX_PENDING) {
    TAILQ_REMOVE(&epggrab_ota_pending, ota, om_q_link);
    ota->om_q_type = EPGGRAB_OTA_MUX_IDLE;
  }
}

/* **************************************************************************
 * Timer callbacks
 * *************************************************************************/

static void
epggrab_ota_timeout_cb ( void *p )
{
  epggrab_ota_mux_t *om = p;

  lock_assert(&global_lock);

  if (!om)
    return;

  /* Abort */
  epggrab_ota_done(om, EPGGRAB_OTA_DONE_TIMEOUT);
  /* Not completed, but no further data for a long period */
  /* wait for a manual mux tuning */
  epggrab_ota_complete_mark(om, 1);
}

static void
epggrab_ota_data_timeout_cb ( void *p )
{
  epggrab_ota_mux_t *om = p;
  epggrab_ota_map_t *map;

  lock_assert(&global_lock);

  if (!om)
    return;

  /* Test for any valid data reception */
  LIST_FOREACH(map, &om->om_modules, om_link) {
    if (!map->om_first)
      break;
  }

  if (map == NULL) {
    /* Abort */
    epggrab_ota_done(om, EPGGRAB_OTA_DONE_NO_DATA);
    /* Not completed, but no data - wait for a manual mux tuning */
    epggrab_ota_complete_mark(om, 1);
  } else {
    tvhtrace("epggrab", "data timeout check succeed");
  }
}

static void
epggrab_ota_kick_cb ( void *p )
{
  extern const idclass_t mpegts_mux_class;
  epggrab_ota_map_t *map;
  epggrab_ota_mux_t *om = TAILQ_FIRST(&epggrab_ota_pending);
  epggrab_ota_mux_t *first = NULL;
  mpegts_mux_t *mm;
  struct {
    mpegts_network_t *net;
    int failed;
  } networks[64], *net;	/* more than 64 networks? - you're a king */
  int i, r, networks_count = 0, epg_flag, kick = 1;
  const char *modname;
  static const char *modnames[] = {
    [MM_EPG_DISABLE]                 = NULL,
    [MM_EPG_ENABLE]                  = NULL,
    [MM_EPG_FORCE]                   = NULL,
    [MM_EPG_ONLY_EIT]                = "eit",
    [MM_EPG_ONLY_UK_FREESAT]         = "uk_freesat",
    [MM_EPG_ONLY_UK_FREEVIEW]        = "uk_freeview",
    [MM_EPG_ONLY_VIASAT_BALTIC]      = "viasat_baltic",
    [MM_EPG_ONLY_BULSATCOM_39E]      = "Bulsatcom_39E",
    [MM_EPG_ONLY_OPENTV_SKY_UK]      = "opentv-skyuk",
    [MM_EPG_ONLY_OPENTV_SKY_ITALIA]  = "opentv-skyit",
    [MM_EPG_ONLY_OPENTV_SKY_AUSAT]   = "opentv-ausat",
  };

  lock_assert(&global_lock);

  if (!om)
    return;

  tvhtrace("epggrab", "ota - kick callback");

next_one:
  /* Find the mux */
  mm = mpegts_mux_find(om->om_mux_uuid);
  if (!mm) {
    epggrab_ota_free(&epggrab_ota_pending, om);
    goto done;
  }

  assert(om->om_q_type == EPGGRAB_OTA_MUX_PENDING);
  TAILQ_REMOVE(&epggrab_ota_pending, om, om_q_link);
  om->om_q_type = EPGGRAB_OTA_MUX_IDLE;

  /* Check if this network failed before */
  for (i = 0, net = NULL; i < networks_count; i++) {
    net = &networks[i];
    if (net->net == mm->mm_network) {
      if (net->failed) {
        TAILQ_INSERT_TAIL(&epggrab_ota_pending, om, om_q_link);
        om->om_q_type = EPGGRAB_OTA_MUX_PENDING;
        goto done;
      }
      break;
    }
  }
  if (i >= networks_count) {
    net = &networks[networks_count++];
    net->net = mm->mm_network;
    net->failed = 0;
  }

  epg_flag = MM_EPG_DISABLE;
  if (mm->mm_is_enabled(mm)) {
    epg_flag = mm->mm_is_epg(mm);
    if (epg_flag > MM_EPG_LAST)
      epg_flag = MM_EPG_ENABLE;
    modname  = epg_flag >= 0 ? modnames[epg_flag] : NULL;
  }

  if (epg_flag < 0 || epg_flag == MM_EPG_DISABLE) {
    if (tvhtrace_enabled()) {
      char name[256];
      mpegts_mux_nice_name(mm, name, sizeof(name));
      tvhtrace("epggrab", "epg mux %s is disabled, skipping", name);
    }
    goto done;
  }

  /* Check we have modules attached and enabled */
  i = r = 0;
  LIST_FOREACH(map, &om->om_modules, om_link) {
    if (map->om_module->tune(map, om, mm)) {
      i++;
      if (modname && !strcmp(modname, map->om_module->id))
        r = 1;
    }
  }
  if ((i == 0 || (r == 0 && modname)) && epg_flag != MM_EPG_FORCE) {
    char name[256];
    mpegts_mux_nice_name(mm, name, sizeof(name));
    tvhdebug("epggrab", "no OTA modules active for %s, check again next time", name);
    goto done;
  }

  /* Some init stuff */
  free(om->om_force_modname);
  om->om_force_modname = modname ? strdup(modname) : NULL;

  /* Subscribe to the mux */
  om->om_requeue = 1;
  if ((r = mpegts_mux_subscribe(mm, NULL, "epggrab",
                                SUBSCRIPTION_PRIO_EPG,
                                SUBSCRIPTION_EPG |
                                SUBSCRIPTION_ONESHOT |
                                SUBSCRIPTION_TABLES))) {
    TAILQ_INSERT_TAIL(&epggrab_ota_pending, om, om_q_link);
    om->om_q_type = EPGGRAB_OTA_MUX_PENDING;
    if (r == SM_CODE_NO_FREE_ADAPTER)
      net->failed = 1;
    if (first == NULL)
      first = om;
  } else {
    tvhtrace("epggrab", "mux %p started", mm);
    kick = 0;
    /* note: it is possible that the mux_start listener is not called */
    /* for reshared mux subscriptions, so call it (maybe second time) here.. */
    epggrab_mux_start(mm, NULL);
  }

done:
  om = TAILQ_FIRST(&epggrab_ota_pending);
  if (networks_count < ARRAY_SIZE(networks) && om && first != om)
    goto next_one;
  if (kick)
    epggrab_ota_kick(64); /* a random number? */

  if (tvhtrace_enabled()) {
    i = r = 0;
    RB_FOREACH(om, &epggrab_ota_all, om_global_link)
      i++;
    TAILQ_FOREACH(om, &epggrab_ota_pending, om_q_link)
      r++;
    tvhtrace("epggrab", "mux stats - all %i pending %i", i, r);
  }
}

/*
 * Start times management
 */

static void
epggrab_ota_start_cb ( void *p );

static void
epggrab_ota_next_arm( time_t next )
{
  tvhtrace("epggrab", "next ota start event in %li seconds", next - time(NULL));
  gtimer_arm_abs(&epggrab_ota_start_timer, epggrab_ota_start_cb, NULL, next);
  dbus_emit_signal_s64("/epggrab/ota", "next", next);
}

static void
epggrab_ota_start_cb ( void *p )
{
  time_t next;

  tvhtrace("epggrab", "ota start callback");

  epggrab_ota_pending_flag = 1;

  epggrab_ota_kick(1);

  pthread_mutex_lock(&epggrab_ota_mutex);
  if (!cron_multi_next(epggrab_ota_cron_multi, dispatch_clock, &next))
    epggrab_ota_next_arm(next);
  else
    tvhwarn("epggrab", "ota cron config invalid or unset");
  pthread_mutex_unlock(&epggrab_ota_mutex);
}

static void
epggrab_ota_arm ( time_t last )
{
  time_t next;

  pthread_mutex_lock(&epggrab_ota_mutex);

  if (!cron_multi_next(epggrab_ota_cron_multi, time(NULL), &next)) {
    /* do not trigger the next EPG scan for 1/2 hour */
    if (last != (time_t)-1 && last + 1800 > next)
      next = last + 1800;
    epggrab_ota_next_arm(next);
  } else {
    tvhwarn("epggrab", "ota cron config invalid or unset");
  }

  pthread_mutex_unlock(&epggrab_ota_mutex);
}

/*
 * Service management
 */

static void
epggrab_ota_service_trace ( epggrab_ota_mux_t *ota,
                            epggrab_ota_svc_link_t *svcl,
                            const char *op )
{
  char buf[256];
  mpegts_mux_t *mm;
  mpegts_service_t *svc;

  if (!tvhtrace_enabled())
    return;

  mm = mpegts_mux_find(ota->om_mux_uuid);
  svc = mpegts_service_find_by_uuid(svcl->uuid);
  if (mm && svc) {
    mpegts_mux_nice_name(mm, buf, sizeof(buf));
    tvhtrace("epggrab", "ota %s %s service %s", buf, op, svc->s_nicename);
  } else if (tvheadend_running)
    tvhtrace("epggrab", "ota %s, problem? (%p %p)", op, mm, svc);
}

void
epggrab_ota_service_add ( epggrab_ota_map_t *map, epggrab_ota_mux_t *ota,
                          const char *uuid, int save )
{
  epggrab_ota_svc_link_t *svcl;

  if (uuid == NULL || !epggrab_ota_running)
    return;
  SKEL_ALLOC(epggrab_svc_link_skel);
  epggrab_svc_link_skel->uuid = (char *)uuid;
  svcl = RB_INSERT_SORTED(&map->om_svcs, epggrab_svc_link_skel, link, om_svcl_cmp);
  if (svcl == NULL) {
    svcl = epggrab_svc_link_skel;
    SKEL_USED(epggrab_svc_link_skel);
    svcl->uuid = strdup(uuid);
    if (save)
      ota->om_save = 1;
    epggrab_ota_service_trace(ota, svcl, "add new");
  }
  svcl->last_tune_count = map->om_tune_count;
}

void
epggrab_ota_service_del ( epggrab_ota_map_t *map, epggrab_ota_mux_t *ota,
                          epggrab_ota_svc_link_t *svcl, int save )
{
  if (svcl == NULL || (!epggrab_ota_running && save))
    return;
  epggrab_ota_service_trace(ota, svcl, "delete");
  RB_REMOVE(&map->om_svcs, svcl, link);
  free(svcl->uuid);
  free(svcl);
  if (save)
    ota->om_save = 1;
}

/* **************************************************************************
 * Config
 * *************************************************************************/

static void
epggrab_ota_save ( epggrab_ota_mux_t *ota )
{
  epggrab_ota_map_t *map;
  epggrab_ota_svc_link_t *svcl;
  htsmsg_t *e, *l, *l2, *c = htsmsg_create_map();

  ota->om_save = 0;
  htsmsg_add_u32(c, "complete", ota->om_complete);
  l = htsmsg_create_list();
  LIST_FOREACH(map, &ota->om_modules, om_link) {
    e = htsmsg_create_map();
    htsmsg_add_str(e, "id", map->om_module->id);
    if (RB_FIRST(&map->om_svcs)) {
      l2 = htsmsg_create_list();
      RB_FOREACH(svcl, &map->om_svcs, link)
        if (svcl->uuid)
          htsmsg_add_str(l2, NULL, svcl->uuid);
      htsmsg_add_msg(e, "services", l2);
    }
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
  htsmsg_t *l, *l2, *e;
  htsmsg_field_t *f, *f2;
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
  if (tvhtrace_enabled()) {
    char name[256];
    mpegts_mux_nice_name(mm, name, sizeof(name));
    tvhtrace("epggrab", "loading config for %s", name);
  }

  ota = calloc(1, sizeof(epggrab_ota_mux_t));
  ota->om_mux_uuid = strdup(uuid);
  if (RB_INSERT_SORTED(&epggrab_ota_all, ota, om_global_link, om_id_cmp)) {
    free(ota->om_mux_uuid);
    free(ota);
    return;
  }
  ota->om_complete = htsmsg_get_u32_or_default(c, "complete", 0) != 0;
  
  if (!(l = htsmsg_get_list(c, "modules"))) return;
  HTSMSG_FOREACH(f, l) {
    if (!(e   = htsmsg_field_get_map(f))) continue;
    if (!(id  = htsmsg_get_str(e, "id"))) continue;
    if (!(mod = (epggrab_module_ota_t*)epggrab_module_find_by_id(id)))
      continue;
    
    map = calloc(1, sizeof(epggrab_ota_map_t));
    RB_INIT(&map->om_svcs);
    map->om_module   = mod;
    if ((l2 = htsmsg_get_list(e, "services")) != NULL) {
      HTSMSG_FOREACH(f2, l2)
        epggrab_ota_service_add(map, ota, htsmsg_field_get_str(f2), 0);
    }
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

  epggrab_conf.ota_initial = 1;
  epggrab_conf.ota_timeout = 600;
  epggrab_conf.ota_cron    = strdup("# Default config (02:04 and 14:04 everyday)\n4 2 * * *\n4 14 * * *");
  epggrab_ota_cron_multi   = cron_multi_set(epggrab_conf.ota_cron);
  epggrab_ota_pending_flag = 0;

  RB_INIT(&epggrab_ota_all);
  TAILQ_INIT(&epggrab_ota_pending);
  TAILQ_INIT(&epggrab_ota_active);

  pthread_mutex_init(&epggrab_ota_mutex, NULL);

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

  epggrab_ota_running = 1;
  
  /* Load config */
  if ((c = hts_settings_load_r(1, "epggrab/otamux"))) {
    HTSMSG_FOREACH(f, c) {
      if (!(m  = htsmsg_field_get_map(f))) continue;
      epggrab_ota_load_one(f->hmf_name, m); 
    }
    htsmsg_destroy(c);
  }
}

void
epggrab_ota_trigger ( int secs )
{
  lock_assert(&global_lock);

  /* notify another system layers, that we will do EPG OTA */
  secs = MIN(1, MAX(secs, 7*24*3600));
  dbus_emit_signal_s64("/epggrab/ota", "next", time(NULL) + secs);
  epggrab_ota_pending_flag = 1;
  epggrab_ota_kick(secs);
}

void
epggrab_ota_post ( void )
{
  time_t t = (time_t)-1;

  /* Init timer (call after full init - wait for network tuners) */
  if (epggrab_conf.ota_initial) {
    epggrab_ota_trigger(15);
    t = time(NULL);
  }

  /* arm the first scheduled time */
  epggrab_ota_arm(t);
}

static void
epggrab_ota_free ( epggrab_ota_head_t *head, epggrab_ota_mux_t *ota  )
{
  epggrab_ota_map_t *map;
  epggrab_ota_svc_link_t *svcl;

  gtimer_disarm(&ota->om_timer);
  gtimer_disarm(&ota->om_data_timer);
  if (head != NULL)
    TAILQ_REMOVE(head, ota, om_q_link);
  RB_REMOVE(&epggrab_ota_all, ota, om_global_link);
  while ((map = LIST_FIRST(&ota->om_modules)) != NULL) {
    LIST_REMOVE(map, om_link);
    while ((svcl = RB_FIRST(&map->om_svcs)) != NULL)
      epggrab_ota_service_del(map, ota, svcl, 0);
    free(map);
  }
  free(ota->om_mux_uuid);
  free(ota->om_force_modname);
  free(ota);
}

void
epggrab_ota_shutdown ( void )
{
  epggrab_ota_mux_t *ota;

  pthread_mutex_lock(&global_lock);
  epggrab_ota_running = 0;
  while ((ota = TAILQ_FIRST(&epggrab_ota_active)) != NULL)
    epggrab_ota_free(&epggrab_ota_active, ota);
  while ((ota = TAILQ_FIRST(&epggrab_ota_pending)) != NULL)
    epggrab_ota_free(&epggrab_ota_pending, ota);
  while ((ota = RB_FIRST(&epggrab_ota_all)) != NULL)
    epggrab_ota_free(NULL, ota);
  pthread_mutex_unlock(&global_lock);
  SKEL_FREE(epggrab_ota_mux_skel);
  SKEL_FREE(epggrab_svc_link_skel);
  free(epggrab_ota_cron_multi);
  epggrab_ota_cron_multi = NULL;
}

/*
 *  Global configuration handlers
 */

void
epggrab_ota_set_cron ( void )
{
  lock_assert(&global_lock);

  pthread_mutex_lock(&epggrab_ota_mutex);
  free(epggrab_ota_cron_multi);
  epggrab_ota_cron_multi = cron_multi_set(epggrab_conf.ota_cron);
  pthread_mutex_unlock(&epggrab_ota_mutex);
  epggrab_ota_arm((time_t)-1);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
