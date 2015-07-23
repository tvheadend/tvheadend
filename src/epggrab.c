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
char                 *epggrab_cron;
epggrab_module_int_t* epggrab_module;
epggrab_module_list_t epggrab_modules;
uint32_t              epggrab_channel_rename;
uint32_t              epggrab_channel_renumber;
uint32_t              epggrab_channel_reicon;
uint32_t              epggrab_epgdb_periodicsave;

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
  time_t tm1, tm2;
  htsmsg_t *data;

  /* Grab */
  time(&tm1);
  data = mod->trans(mod, mod->grab(mod));
  time(&tm2);

  /* Process */
  if ( data ) {
    tvhlog(LOG_INFO, mod->id, "grab took %"PRItime_t" seconds", tm2 - tm1);
    epggrab_module_parse(mod, data);
  } else {
    tvhlog(LOG_WARNING, mod->id, "grab returned no data");
  }
}

/*
 * Thread (for internal grabbing)
 */
static void* _epggrab_internal_thread ( void* p )
{
  int err, confver = -1; // force first run
  struct timespec ts;
  epggrab_module_int_t *mod;
  time_t t;

  /* Setup timeout */
  ts.tv_nsec = 0; 
  time(&ts.tv_sec);

  while ( 1 ) {

    /* Check for config change */
    pthread_mutex_lock(&epggrab_mutex);
    while ( epggrab_running && confver == epggrab_confver ) {
      if (epggrab_module) {
        err = pthread_cond_timedwait(&epggrab_cond, &epggrab_mutex, &ts);
      } else {
        err = pthread_cond_wait(&epggrab_cond, &epggrab_mutex);
      }
      if ( err == ETIMEDOUT ) break;
    }
    confver    = epggrab_confver;
    mod        = epggrab_module;
    if (!cron_multi_next(epggrab_cron_multi, time(NULL), &t))
      ts.tv_sec = t;
    else
      ts.tv_sec += 60;
    pthread_mutex_unlock(&epggrab_mutex);

    if ( !epggrab_running)
      break;

    /* Run grabber */
    if (mod) _epggrab_module_grab(mod);
  }

  return NULL;
}

/* **************************************************************************
 * Configuration
 * *************************************************************************/

static void _epggrab_load ( void )
{
  epggrab_module_t *mod;
  htsmsg_field_t *f;
  htsmsg_t *m, *a;
  uint32_t enabled = 1;
  const char *str;

  /* Load settings */
  m = hts_settings_load("epggrab/config");

  /* Process */
  if (m) {
    htsmsg_get_u32(m, "channel_rename",   &epggrab_channel_rename);
    htsmsg_get_u32(m, "channel_renumber", &epggrab_channel_renumber);
    htsmsg_get_u32(m, "channel_reicon",   &epggrab_channel_reicon);
    htsmsg_get_u32(m, "epgdb_periodicsave", &epggrab_epgdb_periodicsave);
    if (epggrab_epgdb_periodicsave) {
      epggrab_epgdb_periodicsave = MAX(epggrab_epgdb_periodicsave, 3600);
      gtimer_arm(&epggrab_save_timer, epg_save_callback, NULL,
                 epggrab_epgdb_periodicsave);
    }
    if ((str = htsmsg_get_str(m, "cron")) != NULL)
      epggrab_set_cron(str);
    htsmsg_get_u32(m, "grab-enabled", &enabled);
    if (enabled) {
      if ( (str = htsmsg_get_str(m, "module")) ) {
        mod = epggrab_module_find_by_id(str);
        if (mod && mod->type == EPGGRAB_INT) {
          epggrab_module = (epggrab_module_int_t*)mod;
        }
      }
      if ( (a = htsmsg_get_map(m, "mod_enabled")) ) {
        LIST_FOREACH(mod, &epggrab_modules, link) {
          if (htsmsg_get_u32_or_default(a, mod->id, 0)) {
            epggrab_enable_module(mod, 1);
          }
        }
      }
      if ( (a = htsmsg_get_list(m, "mod_priority")) ) {
        int prio = 1;
        LIST_FOREACH(mod, &epggrab_modules, link)
          mod->priority = 0;
        HTSMSG_FOREACH(f, a) {
          mod = epggrab_module_find_by_id(f->hmf_str);
          if (mod) mod->priority = prio++;
        }
      }
    }
    htsmsg_get_u32(m, "ota_timeout", &epggrab_ota_timeout);
    htsmsg_get_u32(m, "ota_initial", &epggrab_ota_initial);
    if ((str = htsmsg_get_str(m, "ota_cron")) != NULL)
      epggrab_ota_set_cron(str, 0);
    htsmsg_destroy(m);

  /* Defaults */
  } else {
    free(epggrab_cron);
    epggrab_cron       = strdup("# Default config (00:04 and 12:04 everyday)\n4 */12 * * *");
    epggrab_module     = NULL;              // disabled
    LIST_FOREACH(mod, &epggrab_modules, link) // enable all OTA by default
      if (mod->type == EPGGRAB_OTA)
        epggrab_enable_module(mod, 1);
  }
 
  /* Load module config (channels) */
#if 0 //ENABLE_MPEGTS
  eit_load();
  opentv_load();
#endif
  pyepg_load();
  xmltv_load();
}

void epggrab_save ( void )
{
  epggrab_module_t *mod;
  htsmsg_t *m, *a;

  /* Register */
  epggrab_confver++;
  pthread_cond_signal(&epggrab_cond);

  /* Save */
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "channel_rename", epggrab_channel_rename);
  htsmsg_add_u32(m, "channel_renumber", epggrab_channel_renumber);
  htsmsg_add_u32(m, "channel_reicon", epggrab_channel_reicon);
  htsmsg_add_u32(m, "epgdb_periodicsave", epggrab_epgdb_periodicsave);
  htsmsg_add_str(m, "cron", epggrab_cron);
  htsmsg_add_str(m, "ota_cron", epggrab_ota_cron);
  htsmsg_add_u32(m, "ota_timeout", epggrab_ota_timeout);
  htsmsg_add_u32(m, "ota_initial", epggrab_ota_initial);
  if ( epggrab_module )
    htsmsg_add_str(m, "module", epggrab_module->id);
  a = NULL;
  LIST_FOREACH(mod, &epggrab_modules, link) {
    if (mod->enabled) {
      if (!a) a = htsmsg_create_map();
      htsmsg_add_u32(a, mod->id, 1);
    } 
  }
  if (a) htsmsg_add_msg(m, "mod_enabled", a);
  hts_settings_save(m, "epggrab/config");
  htsmsg_destroy(m);
}

int epggrab_set_cron ( const char *cron )
{
  int save = 0;
  if ( epggrab_cron == NULL || strcmp(epggrab_cron, cron) ) {
    save = 1;
    free(epggrab_cron);
    epggrab_cron       = strdup(cron);
    free(epggrab_cron_multi);
    epggrab_cron_multi = cron_multi_set(cron);
  }
  return save;
}

int epggrab_set_module ( epggrab_module_t *m )
{
  int save = 0;
  epggrab_module_int_t *mod;
  if (m && m->type != EPGGRAB_INT) return 0;
  mod = (epggrab_module_int_t*)m;
  if ( epggrab_module != mod ) {
    epggrab_enable_module((epggrab_module_t*)epggrab_module, 0);
    epggrab_module = (epggrab_module_int_t*)mod;
    epggrab_enable_module((epggrab_module_t*)epggrab_module, 1);
    save           = 1;
  }
  return save;
}

int epggrab_set_module_by_id ( const char *id )
{
  return epggrab_set_module(epggrab_module_find_by_id(id));
}

int epggrab_set_channel_rename ( uint32_t e )
{
  int save = 0;
  if ( e != epggrab_channel_rename ) {
    epggrab_channel_rename = e;
    save = 1;
  }
  return save;
}

int epggrab_set_channel_renumber ( uint32_t e )
{
  int save = 0;
  if ( e != epggrab_channel_renumber ) {
    epggrab_channel_renumber = e;
    save = 1;
  }
  return save;
}

/*
 * Config from the webui for period save of db to disk
 */
int epggrab_set_periodicsave ( uint32_t e )
{
  int save = 0;
  pthread_mutex_lock(&global_lock);
  if ( e != epggrab_epgdb_periodicsave ) {
    epggrab_epgdb_periodicsave = e ? MAX(e, 3600) : 0;
    if (!e)
      gtimer_disarm(&epggrab_save_timer);
    else
      epg_save(); // will arm the timer
    save = 1;
  }
  pthread_mutex_unlock(&global_lock);
  return save;
}

int epggrab_set_channel_reicon ( uint32_t e )
{
  int save = 0;
  if ( e != epggrab_channel_reicon ) {
    epggrab_channel_reicon = e;
    save = 1;
  }
  return save;
}

int epggrab_enable_module ( epggrab_module_t *mod, uint8_t e )
{
  int save = 0;
  if (!mod) return 0;
  if (mod->enable) {
    save         = mod->enable(mod, e);
  } else if ( e != mod->enabled ) {
    mod->enabled = e;
    save         = 1;
  }
  return save;
}

int epggrab_enable_module_by_id ( const char *id, uint8_t e )
{
  return epggrab_enable_module(epggrab_module_find_by_id(id), e);
}

/* **************************************************************************
 * Initialisation
 * *************************************************************************/

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
  epggrab_cron               = NULL;
  epggrab_module             = NULL;
  epggrab_channel_rename     = 0;
  epggrab_channel_renumber   = 0;
  epggrab_channel_reicon     = 0;
  epggrab_epgdb_periodicsave = 0;

  epggrab_cron_multi         = NULL;

  pthread_mutex_init(&epggrab_mutex, NULL);
  pthread_cond_init(&epggrab_cond, NULL);

  /* Initialise modules */
#if ENABLE_MPEGTS
  eit_init();
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
  epggrab_running = 1;
  tvhthread_create(&epggrab_tid, NULL, _epggrab_internal_thread, NULL);
}

/*
 * Cleanup
 */
void epggrab_done ( void )
{
  epggrab_module_t *mod;

  epggrab_running = 0;
  pthread_cond_signal(&epggrab_cond);
  pthread_join(epggrab_tid, NULL);

  pthread_mutex_lock(&global_lock);
  while ((mod = LIST_FIRST(&epggrab_modules)) != NULL) {
    LIST_REMOVE(mod, link);
    pthread_mutex_unlock(&global_lock);
    if (mod->done)
      mod->done(mod);
    free((void *)mod->id);
    free((void *)mod->name);
    free(mod);
    pthread_mutex_lock(&global_lock);
  }
  pthread_mutex_unlock(&global_lock);
  epggrab_ota_shutdown();
  eit_done();
  opentv_done();
  pyepg_done();
  xmltv_done();
  free(epggrab_cron);
  epggrab_cron = NULL;
  free(epggrab_cron_multi);
  epggrab_cron_multi = NULL;
  epggrab_channel_done();
}
