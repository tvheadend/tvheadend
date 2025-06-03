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
#include "memoryinfo.h"

/* Thread protection */
static int             epggrab_confver;
tvh_mutex_t            epggrab_mutex;
static tvh_cond_t      epggrab_cond;
static tvh_mutex_t     epggrab_data_mutex;
static tvh_cond_t      epggrab_data_cond;
int                    epggrab_running;

static TAILQ_HEAD(, epggrab_module) epggrab_data_modules;

static memoryinfo_t epggrab_data_memoryinfo = { .my_name = "EPG grabber data queue" };

/* Config */
epggrab_module_list_t  epggrab_modules;

gtimer_t               epggrab_save_timer;

static cron_multi_t   *epggrab_cron_multi;

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
static void *_epggrab_internal_thread( void *aux )
{
  epggrab_module_t *mod;
  int err, confver;
  struct timespec cron_next, current_time;
  time_t t;

  confver   = epggrab_conf.int_initial ? -1 /* force first run */ : epggrab_confver;

  /* Setup timeout */
  clock_gettime(CLOCK_REALTIME, &cron_next);
  cron_next.tv_nsec = 0;
  cron_next.tv_sec  += 120;

  /* Time for other jobs */
  while (atomic_get(&epggrab_running)) {
    tvh_mutex_lock(&epggrab_mutex);
    err = ETIMEDOUT;
    while (atomic_get(&epggrab_running)) {
      err = tvh_cond_timedwait_ts(&epggrab_cond, &epggrab_mutex, &cron_next);
      if (err == ETIMEDOUT) break;
    }
    tvh_mutex_unlock(&epggrab_mutex);
    if (err == ETIMEDOUT) break;
  }

  while (atomic_get(&epggrab_running)) {

    tvh_mutex_lock(&epggrab_mutex);

    clock_gettime(CLOCK_REALTIME, &current_time);
    if (!cron_multi_next(epggrab_cron_multi, current_time.tv_sec, &t))
        cron_next.tv_sec = t;
    else
        cron_next.tv_sec += 60;

    /* Check for config change */
    while (atomic_get(&epggrab_running) && confver == epggrab_confver) {
      err = tvh_cond_timedwait_ts(&epggrab_cond, &epggrab_mutex, &cron_next);
      if (err == ETIMEDOUT) break;
    }
    confver    = epggrab_confver;

    tvh_mutex_unlock(&epggrab_mutex);

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
  tvh_cond_signal(&epggrab_cond, 0);
}

/*
 * Thread (for data queue processing)
 */
static void *_epggrab_data_thread( void *aux )
{
  epggrab_module_t *mod;
  epggrab_queued_data_t *eq;

  while (atomic_get(&epggrab_running)) {
    tvh_mutex_lock(&epggrab_data_mutex);
    do {
      eq = NULL;
      mod = TAILQ_FIRST(&epggrab_data_modules);
      if (mod) {
        eq = TAILQ_FIRST(&mod->data_queue);
        if (eq) {
          TAILQ_REMOVE(&mod->data_queue, eq, eq_link);
          if (TAILQ_EMPTY(&mod->data_queue))
            TAILQ_REMOVE(&epggrab_data_modules, mod, qlink);
        }
      }
      if (eq == NULL)
        tvh_cond_wait(&epggrab_data_cond, &epggrab_data_mutex);
    } while (eq == NULL && atomic_get(&epggrab_running));
    tvh_mutex_unlock(&epggrab_data_mutex);
    if (eq) {
      mod->process_data(mod, eq->eq_data, eq->eq_len);
      memoryinfo_free(&epggrab_data_memoryinfo, sizeof(*eq) + eq->eq_len);
      free(eq);
    }
  }
  tvh_mutex_lock(&epggrab_data_mutex);
  while ((mod = TAILQ_FIRST(&epggrab_data_modules)) != NULL) {
    while ((eq = TAILQ_FIRST(&mod->data_queue)) != NULL) {
      TAILQ_REMOVE(&mod->data_queue, eq, eq_link);
      memoryinfo_free(&epggrab_data_memoryinfo, sizeof(*eq) + eq->eq_len);
      free(eq);
    }
    TAILQ_REMOVE(&epggrab_data_modules, mod, qlink);
  }
  tvh_mutex_unlock(&epggrab_data_mutex);
  return NULL;
}

void epggrab_queue_data(epggrab_module_t *mod,
                        const void *data1, uint32_t len1,
                        const void *data2, uint32_t len2)
{
  epggrab_queued_data_t *eq;
  size_t len;

  if (!atomic_get(&epggrab_running))
    return;
  len = sizeof(*eq) + len1 + len2;
  eq = malloc(len);
  if (eq == NULL)
    return;
  eq->eq_len = len1 + len2;
  if (len1)
    memcpy(eq->eq_data, data1, len1);
  if (len2)
    memcpy(eq->eq_data + len1, data2, len2);
  memoryinfo_alloc(&epggrab_data_memoryinfo, len);
  tvh_mutex_lock(&epggrab_data_mutex);
  if (TAILQ_EMPTY(&mod->data_queue)) {
    tvh_cond_signal(&epggrab_data_cond, 0);
    TAILQ_INSERT_TAIL(&epggrab_data_modules, mod, qlink);
  }
  TAILQ_INSERT_TAIL(&mod->data_queue, eq, eq_link);
  tvh_mutex_unlock(&epggrab_data_mutex);
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
        mod = epggrab_module_find_by_id(htsmsg_field_name(f));
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
  xmltv_load();
}

/* **************************************************************************
 * Class
 * *************************************************************************/

static void
epggrab_class_changed(idnode_t *self)
{
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
  tvh_mutex_lock(&epggrab_mutex);
  free(epggrab_cron_multi);
  epggrab_cron_multi = cron_multi_set(epggrab_conf.cron);
  tvh_mutex_unlock(&epggrab_mutex);
}

static void
epggrab_class_ota_cron_notify(void *self, const char *lang)
{
  epggrab_ota_set_cron();
}

static void
epggrab_class_ota_genre_translation_notify(void *self, const char *lang)
{
  epggrab_ota_set_genre_translation();
}

CLASS_DOC(epgconf)
PROP_DOC(cron)
PROP_DOC(ota_genre_translation)

const idclass_t epggrab_class = {
  .ic_snode      = &epggrab_conf.idnode,
  .ic_class      = "epggrab",
  .ic_caption    = N_("Channels / EPG - EPG Grabber Configuration"),
  .ic_doc        = tvh_doc_epgconf_class,
  .ic_event      = "epggrab",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_changed    = epggrab_class_changed,
  .ic_save       = epggrab_class_save,
  .ic_groups     = (const property_group_t[]) {
      {
         .name   = N_("General Settings"),
         .number = 1,
      },
      {
         .name   = N_("Internal Grabber Settings"),
         .number = 2,
      },
      {
         .name   = N_("OTA (Over-the-air) Grabber Settings"),
         .number = 3,
      },
      {
         .name   = N_("OTA (Over-the-air) Genre Translation"),
         .number = 4,
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
      .type   = PT_BOOL,
      .id     = "epgdb_saveafterimport",
      .name   = N_("Save EPG to disk after xmltv import"),
      .desc   = N_("Writes the current in-memory EPG database to disk "
                   "shortly after an xmltv import has completed, so should a crash/unexpected "
                   "shutdown occur EPG data is saved "
                   "(re-read on next startup)."),
      .off    = offsetof(epggrab_conf_t, epgdb_saveafterimport),
      .group  = 1,
    },
    {
      .type   = PT_BOOL,
      .id     = "epgdb_processparentallabels",
      .name   = N_("Process Parental Rating Labels"),
      .desc   = N_("Convert broadcast ratings codes into "
                   "human-readable labels like 'PG' or 'FSK 16'."),
      .off    = offsetof(epggrab_conf_t, epgdb_processparentallabels),
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
      .id     = "int_initial",
      .name   = N_("Force initial EPG grab at start-up (internal grabbers)"),
      .desc   = N_("Force an initial EPG grab at start-up (internal grabbers)."),
      .off    = offsetof(epggrab_conf_t, int_initial),
      .opts   = PO_ADVANCED,
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
    {
      .type   = PT_STR,
      .id     = "ota_genre_translation",
      .name   = N_("Over-the-air Genre Translation"),
      .desc   = N_("Translate the genre codes received from the broadcaster to another genre code."
                   "<br>Use the form xxx=yyy, where xxx and yyy are "
                   "'ETSI EN 300 468' content descriptor values expressed in decimal (0-255). "
                   "<br>Genre code xxx will be converted to genre code yyy."
                   "<br>Use a separate line for each genre code to be converted."),
      .doc    = prop_doc_ota_genre_translation,
      .off    = offsetof(epggrab_conf_t, ota_genre_translation),
      .notify = epggrab_class_ota_genre_translation_notify,
      .opts   = PO_MULTILINE | PO_EXPERT,
      .group  = 4,
    },
    {}
  }
};

/* **************************************************************************
 * Get the time for the next scheduled internal grabber
 * *************************************************************************/
time_t epggrab_get_next_int(void)
{
  time_t ret_time;
  struct timespec current_time;

  clock_gettime(CLOCK_REALTIME, &current_time);

  tvh_mutex_lock(&epggrab_mutex);

  if(cron_multi_next(epggrab_cron_multi, current_time.tv_sec, &ret_time))  //Zero means success
  {
    ret_time = 0;   //Reset to zero in case it was set to garbage during failure.
  }
  
  tvh_mutex_unlock(&epggrab_mutex);

  return ret_time;

}//END function

/* **************************************************************************
 * Count the number of EPG grabbers of a specified type
 * *************************************************************************/
int epggrab_count_type(int grabberType)
{
  epggrab_module_t *mod;
  int temp_count = 0;

  LIST_FOREACH(mod, &epggrab_modules, link) {
    if(mod->enabled && mod->type == grabberType)
    {
      temp_count++;
    }
  }

  return temp_count;

}

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
 * Initialise
 */
pthread_t      epggrab_tid;
pthread_t      epggrab_data_tid;

void epggrab_init ( void )
{
  memoryinfo_register(&epggrab_data_memoryinfo);

  /* Defaults */
  epggrab_conf.cron               = NULL;
  epggrab_conf.int_initial        = 1;
  epggrab_conf.channel_rename     = 0;
  epggrab_conf.channel_renumber   = 0;
  epggrab_conf.channel_reicon     = 0;
  epggrab_conf.epgdb_periodicsave = 0;
  epggrab_conf.epgdb_saveafterimport = 0;
  epggrab_conf.epgdb_processparentallabels = 0;

  epggrab_cron_multi              = NULL;

  tvh_mutex_init(&epggrab_mutex, NULL);
  tvh_mutex_init(&epggrab_data_mutex, NULL);
  tvh_cond_init(&epggrab_cond, 0);
  tvh_cond_init(&epggrab_data_cond, 0);

  TAILQ_INIT(&epggrab_data_modules);

  idclass_register(&epggrab_class);
  idclass_register(&epggrab_mod_class);
  idclass_register(&epggrab_mod_int_class);
  idclass_register(&epggrab_mod_int_xmltv_class);
  idclass_register(&epggrab_mod_ext_class);
  idclass_register(&epggrab_mod_ext_xmltv_class);
  idclass_register(&epggrab_mod_ota_class);

  epggrab_channel_init();

  /* Initialise modules */
#if ENABLE_MPEGTS
  eit_init();
  psip_init();
  opentv_init();
#endif
  xmltv_init();

  /* Initialise the OTA subsystem */
  epggrab_ota_init();

  /* Load config */
  _epggrab_load();

  /* Post-init for OTA subsystem */
  epggrab_ota_post();

  /* Start internal and data queue grab thread */
  atomic_set(&epggrab_running, 1);
  tvh_thread_create(&epggrab_tid, NULL, _epggrab_internal_thread, NULL, "epggrabi");
  tvh_thread_create(&epggrab_data_tid, NULL, _epggrab_data_thread, NULL, "epgdata");
}

/*
 * Cleanup
 */
void epggrab_done ( void )
{
  epggrab_module_t *mod;

  atomic_set(&epggrab_running, 0);
  tvh_cond_signal(&epggrab_cond, 0);
  tvh_cond_signal(&epggrab_data_cond, 0);
  pthread_join(epggrab_tid, NULL);
  pthread_join(epggrab_data_tid, NULL);

  tvh_mutex_lock(&global_lock);
  while ((mod = LIST_FIRST(&epggrab_modules)) != NULL) {
    idnode_save_check(&mod->idnode, 1);
    idnode_unlink(&mod->idnode);
    LIST_REMOVE(mod, link);
    tvh_mutex_unlock(&global_lock);
    if (mod->done)
      mod->done(mod);
    tvh_mutex_lock(&global_lock);
    epggrab_channel_flush(mod, 0);
    free((void *)mod->id);
    free((void *)mod->saveid);
    free((void *)mod->name);
    free(mod);
  }
  epggrab_ota_shutdown();
  eit_done();
  opentv_done();
  xmltv_done();
  free(epggrab_conf.cron);
  epggrab_conf.cron = NULL;
  free(epggrab_cron_multi);
  epggrab_cron_multi = NULL;
  free(epggrab_conf.ota_cron);
  epggrab_conf.ota_cron = NULL;
  epggrab_channel_done();
  memoryinfo_unregister(&epggrab_data_memoryinfo);
  tvh_mutex_unlock(&global_lock);
}
