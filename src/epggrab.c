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

/* Thread protection */
static int                   epggrab_confver;
pthread_mutex_t       epggrab_mutex;
static pthread_cond_t        epggrab_cond;

/* Config */
uint32_t              epggrab_interval;
epggrab_module_int_t* epggrab_module;
epggrab_module_list_t epggrab_modules;
uint32_t              epggrab_channel_rename;
uint32_t              epggrab_channel_renumber;
uint32_t              epggrab_channel_reicon;

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

  /* Setup timeout */
  ts.tv_nsec = 0; 
  time(&ts.tv_sec);

  while ( 1 ) {

    /* Check for config change */
    pthread_mutex_lock(&epggrab_mutex);
    while ( confver == epggrab_confver ) {
      if (epggrab_module) {
        err = pthread_cond_timedwait(&epggrab_cond, &epggrab_mutex, &ts);
      } else {
        err = pthread_cond_wait(&epggrab_cond, &epggrab_mutex);
      }
      if ( err == ETIMEDOUT ) break;
    }
    confver    = epggrab_confver;
    mod        = epggrab_module;
    ts.tv_sec += epggrab_interval;
    pthread_mutex_unlock(&epggrab_mutex);

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
  int old = 0;

  /* Load settings */
  if (!(m = hts_settings_load("epggrab/config"))) {
    if ((m = hts_settings_load("xmltv/config")))
      old = 1;
  }
  if (old) tvhlog(LOG_INFO, "epggrab", "migrating old configuration");

  /* Process */
  if (m) {
    htsmsg_get_u32(m, "channel_rename",   &epggrab_channel_rename);
    htsmsg_get_u32(m, "channel_renumber", &epggrab_channel_renumber);
    htsmsg_get_u32(m, "channel_reicon",   &epggrab_channel_reicon);
    if (!htsmsg_get_u32(m, old ? "grab-interval" : "interval",
                        &epggrab_interval)) {
      if (old) epggrab_interval *= 3600;
    }
    htsmsg_get_u32(m, "grab-enabled", &enabled);
    if (enabled) {
      if ( (str = htsmsg_get_str(m, old ? "current-grabber" : "module")) ) {
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
    htsmsg_destroy(m);

    /* Finish up migration */
    if (old) {

      /* Enable OTA modules */
      LIST_FOREACH(mod, &epggrab_modules, link)
        if (mod->type == EPGGRAB_OTA)
          epggrab_enable_module(mod, 1);

      /* Migrate XMLTV channels */
      htsmsg_t *xc, *ch;
      htsmsg_t *xchs = hts_settings_load("xmltv/channels");
      htsmsg_t *chs  = hts_settings_load("channels");
      if (xchs) {
        HTSMSG_FOREACH(f, chs) {
          if ((ch = htsmsg_get_map_by_field(f))) {
            if ((str = htsmsg_get_str(ch, "xmltv-channel"))) {
              if ((xc = htsmsg_get_map(xchs, str))) {
                htsmsg_add_u32(xc, "channel", atoi(f->hmf_name));
              }
            }
          }
        }
        HTSMSG_FOREACH(f, xchs) {
          if ((xc = htsmsg_get_map_by_field(f))) {
            hts_settings_save(xc, "epggrab/xmltv/channels/%s", f->hmf_name);
          }
        }
      }

      /* Save epggrab config */
      epggrab_save();
    }

  /* Defaults */
  } else {
    epggrab_interval   = 12 * 3600;         // hours
    epggrab_module     = NULL;              // disabled
    LIST_FOREACH(mod, &epggrab_modules, link) // enable all OTA by default
      if (mod->type == EPGGRAB_OTA)
        epggrab_enable_module(mod, 1);
  }
 
  /* Load module config (channels) */
#if ENABLE_LINUXDVB
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
  htsmsg_add_u32(m, "interval",   epggrab_interval);
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

int epggrab_set_interval ( uint32_t interval )
{
  int save = 0;
  if ( epggrab_interval != interval ) {
    save = 1;
    epggrab_interval = interval;
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
void epggrab_init ( void )
{
  /* Lists */
#if ENABLE_LINUXDVB
  extern TAILQ_HEAD(, epggrab_ota_mux) ota_mux_all;
  TAILQ_INIT(&ota_mux_all);
#endif

  pthread_mutex_init(&epggrab_mutex, NULL);
  pthread_cond_init(&epggrab_cond, NULL);
  
  /* Initialise modules */
#if ENABLE_LINUXDVB
  eit_init();
  opentv_init();
#endif
  pyepg_init();
  xmltv_init();

  /* Load config */
  _epggrab_load();
#if ENABLE_LINUXDVB
  epggrab_ota_load();
#endif

  /* Start internal grab thread */
  pthread_t      tid;
  pthread_attr_t tattr;
  pthread_attr_init(&tattr);
  pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
  pthread_create(&tid, &tattr, _epggrab_internal_thread, NULL);
  pthread_attr_destroy(&tattr);
}

