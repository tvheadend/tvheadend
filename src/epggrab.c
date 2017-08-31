/*
 *  Electronic Program Guide - Common functions
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include "htsmsg.h"
#include "settings.h"
#include "tvheadend.h"
#include "queue.h"
#include "epg.h"
#include "epggrab.h"
#include "epggrab/private.h"
#include "channels.h"
#include "spawn.h"
#include "htsmsg_xml.h"
#include "file.h"
#include "service.h"
#include "cron.h"

/* Thread protection */
static int            epggrab_confver;
pthread_mutex_t       epggrab_mutex;
static pthread_cond_t epggrab_cond;
int                   epggrab_running;

/* Config */
epggrab_module_list_t epggrab_modules;

gtimer_t              epggrab_save_timer;

static cron_multi_t  *epggrab_cron_multi;

/* **************************************************************************
 * Internal Grab Thread
 * *************************************************************************/

/*
 * Grab from module
 */
static void _epggrab_module_grab ( epggrab_module_int_t *mod )
{
  int64_t tm1, tm2;
  htsmsg_t *data;

  if (!mod->enabled)
    return;

  /* Grab */
  tm1 = getfastmonoclock();
  data = mod->trans(mod, mod->grab(mod));
  tm2 = getfastmonoclock() + (MONOCLOCK_RESOLUTION / 2);

  /* Process */
  if ( data ) {
    tvhinfo(mod->subsys, "%s: grab took %"PRId64" seconds", mod->id, mono2sec(tm2 - tm1));
    epggrab_module_parse(mod, data);
  } else {
    tvhwarn(mod->subsys, "%s: grab returned no data", mod->id);
  }
}

/*
 * Thread (for internal grabbing)
 */
static void* _epggrab_internal_thread ( void* p )
{
  epggrab_module_t *mod;
  int err, confver = -1; // force first run
  struct timespec ts;
  time_t t;

  /* Setup timeout */
  ts.tv_nsec = 0; 
  ts.tv_sec  = time(NULL) + 120;

  /* Time for other jobs */
  while (atomic_get(&epggrab_running)) {
    pthread_mutex_lock(&epggrab_mutex);
    err = ETIMEDOUT;
    while (atomic_get(&epggrab_running)) {
      err = pthread_cond_timedwait(&epggrab_cond, &epggrab_mutex, &ts);
      if (err == ETIMEDOUT) break;
    }
    pthread_mutex_unlock(&epggrab_mutex);
    if (err == ETIMEDOUT) break;
  }

  time(&ts.tv_sec);

  while (atomic_get(&epggrab_running)) {

    /* Check for config change */
    pthread_mutex_lock(&epggrab_mutex);
    while (atomic_get(&epggrab_running) && confver == epggrab_confver) {
      err = pthread_cond_timedwait(&epggrab_cond, &epggrab_mutex, &ts);
      if (err == ETIMEDOUT) break;
    }
    confver    = epggrab_confver;
    if (!cron_multi_next(epggrab_cron_multi, time(NULL), &t))
      ts.tv_sec = t;
    else
      ts.tv_sec += 60;
    pthread_mutex_unlock(&epggrab_mutex);

    /* Run grabber(s) */
    /* Note: this loop is not protected, assuming static boot allocation */
    LIST_FOREACH(mod, &epggrab_modules, link) {
      if (!atomic_get(&epggrab_running))
        break;
      if (mod->type == EPGGRAB_INT)
         _epggrab_module_grab((epggrab_module_int_t *)mod);
    }
  }

  return NULL;
}

void
epggrab_rerun_internal(void)
{
  epggrab_confver++;
  pthread_cond_signal(&epggrab_cond);
}

/* **************************************************************************
 * Configuration
 * *************************************************************************/

static void _epggrab_load ( void )
{
  htsmsg_t *m, *a, *map;
  htsmsg_field_t *f;
  epggrab_module_t *mod;

  /* Load settings */
  m = hts_settings_load("epggrab/config");
  if (m) {
    idnode_load(&epggrab_conf.idnode, m);
    if ((a = htsmsg_get_map(m, "modules"))) {
      HTSMSG_FOREACH(f, a) {
        mod = epggrab_module_find_by_id(f->hmf_name);
        map = htsmsg_field_get_map(f);
        if (mod && map) {
          idnode_load(&mod->idnode, map);
          epggrab_activate_module(mod, mod->enabled);
        }
      }
    }
    htsmsg_destroy(m);
  /* Defaults */
  } else {
    free(epggrab_conf.cron);
    epggrab_conf.cron = strdup("# Default config (00:04 and 12:04 everyday)\n4 */12 * * *");
    LIST_FOREACH(mod, &epggrab_modules, link) // enable only OTA EIT and OTA PSIP by default
      if (mod->type == EPGGRAB_OTA &&
          ((mod->subsys == LS_TBL_EIT && strcmp(mod->id, "eit") == 0) ||
           mod->subsys == LS_PSIP)) {
        mod->enabled = 1;
        epggrab_activate_module(mod, 1);
      }
  }

  if (epggrab_conf.epgdb_periodicsave)
    gtimer_arm_rel(&epggrab_save_timer, epg_save_callback, NULL,
                   epggrab_conf.epgdb_periodicsave * 3600);

  idnode_notify_changed(&epggrab_conf.idnode);
 
  /* Load module config (channels) */
  eit_load();
  opentv_load();
  pyepg_load();
  xmltv_load();
}

/* **************************************************************************
 * Class
 * *************************************************************************/

static void
epggrab_class_changed(idnode_t *self)
{
  /* Register */
  epggrab_rerun_internal();
}

static htsmsg_t *
epggrab_class_save(idnode_t *self, char *filename, size_t fsize)
{
  epggrab_module_t *mod;
  htsmsg_t *m, *a;

  /* Save */
  m = htsmsg_create_map();
  idnode_save(&epggrab_conf.idnode, m);
  a = htsmsg_create_map();
  LIST_FOREACH(mod, &epggrab_modules, link) {
    htsmsg_t *m = htsmsg_create_map();
    htsmsg_add_str(m, "class", mod->idnode.in_class->ic_class);
    idnode_save(&mod->idnode, m);
    htsmsg_add_msg(a, mod->id, m);
  }
  htsmsg_add_msg(m, "modules", a);
  if (filename)
    snprintf(filename, fsize, "epggrab/config");
  return m;
}

epggrab_conf_t epggrab_conf = {
  .idnode.in_class = &epggrab_class
};

static void
epggrab_class_cron_notify(void *self, const char *lang)
{
  pthread_mutex_lock(&epggrab_mutex);
  free(epggrab_cron_multi);
  epggrab_cron_multi = cron_multi_set(epggrab_conf.cron);
  pthread_mutex_unlock(&epggrab_mutex);
}

static void
epggrab_class_ota_cron_notify(void *self, const char *lang)
{
  epggrab_ota_set_cron();
}

CLASS_DOC(epgconf)
PROP_DOC(cron)

const idclass_t epggrab_class = {
  .ic_snode      = &epggrab_conf.idnode,
  .ic_class      = "epggrab",
  .ic_caption    = N_("EPG Grabber Configuration"),
  .ic_doc        = tvh_doc_epgconf_class,
  .ic_event      = "epggrab",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_changed    = epggrab_class_changed,
  .ic_save       = epggrab_class_save,
  .ic_groups     = (const property_group_t[]) {
      {
         .name   = N_("General configuration"),
         .number = 1,
      },
      {
         .name   = N_("Internal grabber"),
         .number = 2,
      },
      {
         .name   = N_("Over-the-air grabbers"),
         .number = 3,
      },
      {}
  },
  .ic_properties = (const property_t[]){
    {
      .type   = PT_BOOL,
      .id     = "channel_rename",
      .name   = N_("Update channel name"),
      .desc   = N_("Automatically update channel names using "
                   "information provided by the enabled EPG providers. "
                   "Note, this may cause unwanted changes to "
                   "already defined channel names."),
      .off    = offsetof(epggrab_conf_t, channel_rename),
      .opts   = PO_ADVANCED,
      .group  = 1,
    },
    {
      .type   = PT_BOOL,
      .id     = "channel_renumber",
      .name   = N_("Update channel number"),
      .desc   = N_("Automatically update channel numbers using "
                   "information provided by the enabled EPG providers. "
                   "Note, this may cause unwanted changes to "
                   "already defined channel numbers."),
      .off    = offsetof(epggrab_conf_t, channel_renumber),
      .opts   = PO_ADVANCED,
      .group  = 1,
    },
    {
      .type   = PT_BOOL,
      .id     = "channel_reicon",
      .name   = N_("Update channel icon"),
      .desc   = N_("Automatically update channel icons using "
                   "information provided by the enabled EPG providers. "
                   "Note, this may cause unwanted changes to "
                   "already defined channel icons."),
      .off    = offsetof(epggrab_conf_t, channel_reicon),
      .opts   = PO_ADVANCED,
      .group  = 1,
    },
    {
      .type   = PT_INT,
      .id     = "epgdb_periodicsave",
      .name   = N_("Periodically save EPG to disk (hours)"),
      .desc   = N_("Writes the current in-memory EPG database to disk "
                   "every x hours, so should a crash/unexpected "
                   "shutdown occur EPG data is saved "
                   "periodically to the database (re-read on next "
                   "startup). Set to 0 to disable."),
      .off    = offsetof(epggrab_conf_t, epgdb_periodicsave),
      .group  = 1,
    },
    {
      .type   = PT_STR,
      .id     = "cron",
      .name   = N_("Cron multi-line"),
      .desc   = N_("Multiple lines of the cron time specification. "
                   "The default cron triggers the internal grabbers "
                   "daily at 12:04 and 00:04. See Help on how to define "
                   "your own."),
      .doc    = prop_doc_cron,
      .off    = offsetof(epggrab_conf_t, cron),
      .notify = epggrab_class_cron_notify,
      .opts   = PO_MULTILINE | PO_ADVANCED,
      .group  = 2,
    },
    {
      .type   = PT_BOOL,
      .id     = "ota_initial",
      .name   = N_("Force initial EPG grab at start-up"),
      .desc   = N_("Force an initial EPG grab at start-up."),
      .off    = offsetof(epggrab_conf_t, ota_initial),
      .opts   = PO_ADVANCED,
      .group  = 3,
    },
    {
      .type   = PT_STR,
      .id     = "ota_cron",
      .name   = N_("Over-the-air Cron multi-line"),
      .desc   = N_("Multiple lines of the cron time specification. "
                   "The default cron triggers the over-the-air "
                   "grabber daily at 02:04 and 14:04. See Help on how "
                   "to define your own."),
      .doc    = prop_doc_cron,
      .off    = offsetof(epggrab_conf_t, ota_cron),
      .notify = epggrab_class_ota_cron_notify,
      .opts   = PO_MULTILINE | PO_ADVANCED,
      .group  = 3,
    },
    {
      .type   = PT_U32,
      .id     = "ota_timeout",
      .name   = N_("EPG scan time-out in seconds (30-7200)"),
      .desc   = N_("The maximum amount of time a grabber is allowed "
                   "scan a mux for data (in seconds)."),
      .off    = offsetof(epggrab_conf_t, ota_timeout),
      .opts   = PO_EXPERT,
      .group  = 3,
    },
    {}
  }
};

/* **************************************************************************
 * Initialisation
 * *************************************************************************/

int epggrab_activate_module ( epggrab_module_t *mod, int a )
{
  int save = 0;
  if (!mod) return 0;
  if (mod->activate) {
    save         = mod->activate(mod, a);
  } else if (a != mod->active) {
    mod->active  = a;
    save         = 1;
  }
  return save;
}

/*
 * TODO: implement this
 */
void epggrab_resched ( void )
{
}

/*
 * Initialise
 */
pthread_t      epggrab_tid;

void epggrab_init ( void )
{
  /* Defaults */
  epggrab_conf.cron               = NULL;
  epggrab_conf.channel_rename     = 0;
  epggrab_conf.channel_renumber   = 0;
  epggrab_conf.channel_reicon     = 0;
  epggrab_conf.epgdb_periodicsave = 0;

  epggrab_cron_multi              = NULL;

  pthread_mutex_init(&epggrab_mutex, NULL);
  pthread_cond_init(&epggrab_cond, NULL);

  idclass_register(&epggrab_class);
  idclass_register(&epggrab_mod_class);
  idclass_register(&epggrab_mod_int_class);
  idclass_register(&epggrab_mod_int_pyepg_class);
  idclass_register(&epggrab_mod_int_xmltv_class);
  idclass_register(&epggrab_mod_ext_class);
  idclass_register(&epggrab_mod_ext_pyepg_class);
  idclass_register(&epggrab_mod_ext_xmltv_class);
  idclass_register(&epggrab_mod_ota_class);

  epggrab_channel_init();

  /* Initialise modules */
#if ENABLE_MPEGTS
  eit_init();
  psip_init();
  opentv_init();
#endif
  pyepg_init();
  xmltv_init();

  /* Initialise the OTA subsystem */
  epggrab_ota_init();
  
  /* Load config */
  _epggrab_load();

  /* Post-init for OTA subsystem */
  epggrab_ota_post();

  /* Start internal grab thread */
  atomic_set(&epggrab_running, 1);
  tvhthread_create(&epggrab_tid, NULL, _epggrab_internal_thread, NULL, "epggrabi");
}

/*
 * Cleanup
 */
void epggrab_done ( void )
{
  epggrab_module_t *mod;

  atomic_set(&epggrab_running, 0);
  pthread_cond_signal(&epggrab_cond);
  pthread_join(epggrab_tid, NULL);

  pthread_mutex_lock(&global_lock);
  while ((mod = LIST_FIRST(&epggrab_modules)) != NULL) {
    idnode_save_check(&mod->idnode, 1);
    idnode_unlink(&mod->idnode);
    LIST_REMOVE(mod, link);
    pthread_mutex_unlock(&global_lock);
    if (mod->done)
      mod->done(mod);
    pthread_mutex_lock(&global_lock);
    epggrab_channel_flush(mod, 0);
    free((void *)mod->id);
    free((void *)mod->saveid);
    free((void *)mod->name);
    free(mod);
  }
  epggrab_ota_shutdown();
  eit_done();
  opentv_done();
  pyepg_done();
  xmltv_done();
  free(epggrab_conf.cron);
  epggrab_conf.cron = NULL;
  free(epggrab_cron_multi);
  epggrab_cron_multi = NULL;
  free(epggrab_conf.ota_cron);
  epggrab_conf.ota_cron = NULL;
  epggrab_channel_done();
  pthread_mutex_unlock(&global_lock);
}
