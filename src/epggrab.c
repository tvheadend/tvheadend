#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <wordexp.h>
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
int                   epggrab_confver;
pthread_mutex_t       epggrab_mutex;
pthread_cond_t        epggrab_cond;

/* Config */
uint32_t              epggrab_interval;
epggrab_module_int_t* epggrab_module;
epggrab_module_list_t epggrab_modules;
uint32_t              epggrab_channel_rename;
uint32_t              epggrab_channel_renumber;
uint32_t              epggrab_channel_reicon;

/* **************************************************************************
 * Helpers
 * *************************************************************************/

void epggrab_resched ( void )
{
}

/*
 * Run the parse
 */
void epggrab_module_parse
  ( void *m, htsmsg_t *data )
{
  time_t tm1, tm2;
  int save = 0;
  epggrab_stats_t stats;
  epggrab_module_int_t *mod = m;

  /* Parse */
  memset(&stats, 0, sizeof(stats));
  pthread_mutex_lock(&global_lock);
  time(&tm1);
  save |= mod->parse(mod, data, &stats);
  time(&tm2);
  if (save) epg_updated();  
  pthread_mutex_unlock(&global_lock);
  htsmsg_destroy(data);

  /* Debug stats */
  tvhlog(LOG_INFO, mod->id, "parse took %d seconds", tm2 - tm1);
  tvhlog(LOG_INFO, mod->id, "  channels   tot=%5d new=%5d mod=%5d",
         stats.channels.total, stats.channels.created,
         stats.channels.modified);
  tvhlog(LOG_INFO, mod->id, "  brands     tot=%5d new=%5d mod=%5d",
         stats.brands.total, stats.brands.created,
         stats.brands.modified);
  tvhlog(LOG_INFO, mod->id, "  seasons    tot=%5d new=%5d mod=%5d",
         stats.seasons.total, stats.seasons.created,
         stats.seasons.modified);
  tvhlog(LOG_INFO, mod->id, "  episodes   tot=%5d new=%5d mod=%5d",
         stats.episodes.total, stats.episodes.created,
         stats.episodes.modified);
  tvhlog(LOG_INFO, mod->id, "  broadcasts tot=%5d new=%5d mod=%5d",
         stats.broadcasts.total, stats.broadcasts.created,
         stats.broadcasts.modified);
}

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
    tvhlog(LOG_INFO, mod->id, "grab took %d seconds", tm2 - tm1);
    epggrab_module_parse(mod, data);
  } else {
    tvhlog(LOG_WARNING, mod->id, "grab returned no data");
  }
}

/* **************************************************************************
 * Internal Grab Thread
 * *************************************************************************/

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
    if (!htsmsg_get_u32(m, old ? "grab-interval" : "interval", &epggrab_interval))
      if (old) epggrab_interval *= 3600;
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
            if (mod->enable) mod->enable(mod, 1);
          }
        }
      }
    }
    htsmsg_destroy(m);
  
    /* Finish up migration */
    if (old) {
      htsmsg_field_t *f;
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
    epggrab_module_t *m;
    epggrab_interval   = 12 * 3600;         // hours
    epggrab_module     = NULL;              // disabled
    LIST_FOREACH(m, &epggrab_modules, link) // enable all OTA by default
      if (m->type == EPGGRAB_OTA)
        epggrab_enable_module(m, 1);
  }
 
  /* Load module config (channels) */
  eit_load();
  xmltv_load();
  pyepg_load();
  opentv_load();
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
    if (epggrab_module && epggrab_module->enable)
      epggrab_module->enable(epggrab_module, 0);
    epggrab_module = (epggrab_module_int_t*)mod;
    if (epggrab_module && epggrab_module->enable)
      epggrab_module->enable(epggrab_module, 1);
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
 * Module Access
 * *************************************************************************/

epggrab_module_t* epggrab_module_find_by_id ( const char *id )
{
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if ( !strcmp(m->id, id) ) return m;
  }
  return NULL;
}

htsmsg_t *epggrab_module_list ( void )
{
  epggrab_module_t *m;
  htsmsg_t *e, *a = htsmsg_create_list();
  LIST_FOREACH(m, &epggrab_modules, link) {
    e = htsmsg_create_map();
    htsmsg_add_str(e, "id", m->id);
    htsmsg_add_u32(e, "type", m->type);
    htsmsg_add_u32(e, "enabled", m->enabled);
    if(m->name) 
      htsmsg_add_str(e, "name", m->name);
    if(m->type == EPGGRAB_EXT) {
      epggrab_module_ext_t *ext = (epggrab_module_ext_t*)m;
      htsmsg_add_str(e, "path", ext->path);
    }
    htsmsg_add_msg(a, NULL, e);
  }
  return a;
}

/* **************************************************************************
 * Initialisation
 * *************************************************************************/

/*
 * Initialise
 */
void epggrab_init ( void )
{
  /* Initialise modules */
  eit_init();
  xmltv_init();
  pyepg_init();
  opentv_init();

  /* Load config */
  _epggrab_load();

  /* Start internal grab thread */
  pthread_t      tid;
  pthread_attr_t tattr;
  pthread_attr_init(&tattr);
  pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
  pthread_create(&tid, &tattr, _epggrab_internal_thread, NULL);
}

