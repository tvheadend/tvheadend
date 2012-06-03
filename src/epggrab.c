#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "htsmsg.h"
#include "settings.h"
#include "tvheadend.h"
#include "src/queue.h"
#include "epg.h"
#include "epggrab.h"
#include "epggrab/eit.h"
#include "epggrab/xmltv.h"
#include "epggrab/pyepg.h"
#include "channels.h"

/* Thread protection */
int                   epggrab_confver;
pthread_mutex_t       epggrab_mutex;
pthread_cond_t        epggrab_cond;

/* Config */
uint32_t              epggrab_advanced;
uint32_t              epggrab_eit;
uint32_t              epggrab_interval;
epggrab_module_t*     epggrab_module;
epggrab_sched_list_t  epggrab_schedule;
epggrab_module_list_t epggrab_modules;

/* **************************************************************************
 * Modules
 * *************************************************************************/

static int _ch_id_cmp ( void *a, void *b )
{
  return strcmp(((epggrab_channel_t*)a)->id,
                ((epggrab_channel_t*)b)->id);
}

static int _ch_match ( epggrab_channel_t *ec, channel_t *ch )
{
  return 0;
}

void epggrab_module_channels_load ( epggrab_module_t *mod )
{
  char path[100];
  uint32_t chid;
  htsmsg_t *m;
  htsmsg_field_t *f;
  epggrab_channel_t *ec;
  channel_t *ch;

  sprintf(path, "epggrab/%s/channels", mod->id);
  if ((m = hts_settings_load(path))) {
    HTSMSG_FOREACH(f, m) {
      if ( !htsmsg_get_u32(m, f->hmf_name, &chid) ) {
        ch = channel_find_by_identifier(chid);
        if (ch) {
          ec = calloc(1, sizeof(epggrab_channel_t));
          ec->id      = strdup(f->hmf_name);
          ec->channel = ch;
          RB_INSERT_SORTED(mod->channels, ec, link, _ch_id_cmp);
        }
      }
    }
  }
}

void epggrab_module_channels_save
  ( epggrab_module_t *mod, const char *path )
{
  epggrab_channel_t *c;
  htsmsg_t *m = htsmsg_create_map();

  RB_FOREACH(c, mod->channels, link) {
    if (c->channel) htsmsg_add_u32(m, c->id, c->channel->ch_id);
  }
  hts_settings_save(m, path);
}

int epggrab_module_channel_add ( epggrab_module_t *mod, channel_t *ch )
{
  int save = 0;
  epggrab_channel_t *egc;
  RB_FOREACH(egc, mod->channels, link) {
    if (_ch_match(egc, ch) ) {
      save = 1;
      egc->channel = ch;
      break;
    }
  }
  return save;
}

int epggrab_module_channel_rem ( epggrab_module_t *mod, channel_t *ch )
{
  int save = 0;
  epggrab_channel_t *egc;
  RB_FOREACH(egc, mod->channels, link) {
    if (egc->channel == ch) {
      save = 1;
      egc->channel = NULL;
      break;
    }
  }
  return save;
}

int epggrab_module_channel_mod ( epggrab_module_t *mod, channel_t *ch )
{
  return epggrab_module_channel_add(mod, ch);
}


epggrab_channel_t *epggrab_module_channel_create ( epggrab_module_t *mod )
{
  return NULL;
}

epggrab_channel_t *epggrab_module_channel_find
  ( epggrab_module_t *mod, const char *id )
{
  epggrab_channel_t skel; 
  skel.id = (char*)id;
  return RB_FIND(mod->channels, &skel, link, _ch_id_cmp);
}

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
  htsmsg_t *a = htsmsg_create_list();
  return a;
}

/* **************************************************************************
 * Configuration
 * *************************************************************************/

static int _update_str ( char **dst, const char *src )
{
  int save = 0;
  if ( !(*dst) && src ) {
    *dst = strdup(src);
    save = 1;
  } else if ( (*dst) && !src ) {
    free(*dst);
    *dst = NULL;
    save = 1;
  } else if ( strcmp(*dst, src) ) {
    free(*dst);
    *dst = strdup(src);
    save = 1;
  }
  return save;
} 

static int _epggrab_schedule_deserialize ( htsmsg_t *m )
{ 
  int save = 0;
  htsmsg_t *a, *e;
  htsmsg_field_t *f;
  epggrab_sched_t *es;
  epggrab_module_t *mod;
  const char *str;

  es = LIST_FIRST(&epggrab_schedule);
  if ( (a = htsmsg_get_list(m, "schedule")) ) {
    HTSMSG_FOREACH(f, a) {
      if ((e = htsmsg_get_map_by_field(f))) {
        if (es) {
          es = LIST_NEXT(es, link);
        } else {
          es   = calloc(1, sizeof(epggrab_sched_t));
          save = 1;
          LIST_INSERT_HEAD(&epggrab_schedule, es, link);
        }

        mod = NULL;
        if ((str = htsmsg_get_str(e, "module")))
          mod = epggrab_module_find_by_id(str);
        if (es->mod != mod) {
          es->mod = mod;
          save    = 1;
        }

        save |= _update_str(&es->cmd,  htsmsg_get_str(e, "command"));
        save |= _update_str(&es->opts, htsmsg_get_str(e, "options"));
        str   = htsmsg_get_str(e, "cron");
        if (es->cron) {
          if (!str) {
            free(es->cron);
            es->cron = NULL;
            save     = 1;
          } else {
            save |= cron_set_string(es->cron, str);
          }
        } else if (str) {
          es->cron = cron_create(str);
          save     = 1;
        }
      }
    }
  }
  
  if (es)
    while ( LIST_NEXT(es, link) ) {
      LIST_REMOVE(es, link);
      save = 1;
    }

  return save;
}

static htsmsg_t *_epggrab_schedule_serialize ( void )
{
  epggrab_sched_t *es;
  htsmsg_t *e, *a;
  a = htsmsg_create_list();
  LIST_FOREACH(es, &epggrab_schedule, link) {
    e = htsmsg_create_map();
    if ( es->mod  ) htsmsg_add_str(e, "module",  es->mod->id);
    if ( es->cmd  ) htsmsg_add_str(e, "command", es->cmd);
    if ( es->opts ) htsmsg_add_str(e, "options", es->opts);
    if ( es->cron ) cron_serialize(es->cron, e);
    htsmsg_add_msg(a, NULL, e);
  }
  return a;
}

static void _epggrab_load ( void )
{
  htsmsg_t *m, *a;
  const char *str;
  
  /* No config */
  if ((m = hts_settings_load("epggrab/config")) == NULL)
    return;

  /* Load settings */
  htsmsg_get_u32(m, "advanced", &epggrab_advanced);
  htsmsg_get_u32(m, "eit",      &epggrab_eit);
  htsmsg_get_u32(m, "interval", &epggrab_interval);
  if ( (str = htsmsg_get_str(m, "module")) )
    epggrab_module = epggrab_module_find_by_id(str);
  if ( (a = htsmsg_get_list(m, "schedule")) )
    _epggrab_schedule_deserialize(a);
  htsmsg_destroy(m);
}

void epggrab_save ( void )
{
  htsmsg_t *m;

  /* Register */
  epggrab_confver++;
  pthread_cond_signal(&epggrab_cond);

  /* Save */
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "advanced",   epggrab_advanced);
  htsmsg_add_u32(m, "eitenabled", epggrab_eitenabled);
  htsmsg_add_u32(m, "interval",   epggrab_interval);
  if ( epggrab_module )
    htsmsg_add_str(m, "module", epggrab_module->id);
  htsmsg_add_msg(m, "schedule", _epggrab_schedule_serialize());
  hts_settings_save(m, "epggrab/config");
  htsmsg_destroy(m);
}

int epggrab_set_advanced ( uint32_t advanced )
{ 
  int save = 0;
  if ( epggrab_advanced != advanced ) {
    save = 1;
    epggrab_advanced = advanced;
  }
  return save;
}

int epggrab_set_eitenabled ( uint32_t eitenabled )
{
  int save = 0;
  if ( epggrab_eit != eitenabled ) {
    save = 1;
    eitenabled = eitenabled;
  }
  return save;
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

int epggrab_set_module ( epggrab_module_t *mod )
{
  int save = 0;
  if ( mod && epggrab_module != mod ) {
    save = 1;
    epggrab_module = mod;
  }
  return save;
}

int epggrab_set_module_by_id ( const char *id )
{
  return epggrab_set_module(epggrab_module_find_by_id(id));
}

int epggrab_set_schedule ( htsmsg_t *sched )
{
  return _epggrab_schedule_deserialize(sched);
}

htsmsg_t *epggrab_get_schedule ( void )
{
  return _epggrab_schedule_serialize();
}

/* **************************************************************************
 * Module Execution
 * *************************************************************************/

/*
 * Grab from module
 */
static void _epggrab_module_run 
  ( epggrab_module_t *mod, const char *icmd, const char *iopts )
{
  int save = 0;
  time_t tm1, tm2;
  htsmsg_t *data;
  epggrab_stats_t stats;
  char *cmd = NULL, *opts = NULL;

  /* Check */
  if ( !mod || !mod->grab || !mod->parse ) return;
  
  /* Dup before unlock */
  if ( !icmd  ) cmd  = strdup(icmd);
  if ( !iopts ) opts = strdup(iopts);
  pthread_mutex_unlock(&epggrab_mutex);
  
  /* Grab */
  time(&tm1);
  data = mod->grab(mod, cmd, opts);
  time(&tm2);

  /* Process */
  if ( data ) {

    /* Parse */
    memset(&stats, 0, sizeof(stats));
    tvhlog(LOG_DEBUG, mod->id, "grab took %d seconds", tm2 - tm1);
    pthread_mutex_lock(&global_lock);
    time(&tm1);
    save |= mod->parse(mod, data, &stats);
    time(&tm2);
    if (save) epg_updated();  
    pthread_mutex_unlock(&global_lock);
    htsmsg_destroy(data);

    /* Debug stats */
    tvhlog(LOG_DEBUG, mod->id, "parse took %d seconds", tm2 - tm1);
    tvhlog(LOG_DEBUG, mod->id, "  channels   tot=%5d new=%5d mod=%5d",
           stats.channels.total, stats.channels.created,
           stats.channels.modified);
    tvhlog(LOG_DEBUG, mod->id, "  brands     tot=%5d new=%5d mod=%5d",
           stats.brands.total, stats.brands.created,
           stats.brands.modified);
    tvhlog(LOG_DEBUG, mod->id, "  seasons    tot=%5d new=%5d mod=%5d",
           stats.seasons.total, stats.seasons.created,
           stats.seasons.modified);
    tvhlog(LOG_DEBUG, mod->id, "  episodes   tot=%5d new=%5d mod=%5d",
           stats.episodes.total, stats.episodes.created,
           stats.episodes.modified);
    tvhlog(LOG_DEBUG, mod->id, "  broadcasts tot=%5d new=%5d mod=%5d",
           stats.broadcasts.total, stats.broadcasts.created,
           stats.broadcasts.modified);

  } else {
    tvhlog(LOG_WARNING, mod->id, "grab returned no data");
  }

  /* Free a re-lock */
  if (cmd)  free(cmd);
  if (opts) free(cmd);
  pthread_mutex_lock(&epggrab_mutex);
}

/*
 * Simple run
 */
static time_t _epggrab_thread_simple ( void )
{
  /* Copy config */
  time_t            ret = time(NULL) + epggrab_interval;
  epggrab_module_t* mod = epggrab_module;

  /* Run the module */
  _epggrab_module_run(mod, NULL, NULL);

  return ret;
}

/*
 * Advanced run
 */
static time_t _epggrab_thread_advanced ( void )
{
  time_t ret;
  epggrab_sched_t *s;

  /* Determine which to run */
  time(&ret);
  LIST_FOREACH(s, &epggrab_schedule, link) {
    if ( cron_is_time(s->cron) ) {
      _epggrab_module_run(s->mod, s->cmd, s->opts);
      break; // TODO: can only run once else config may have changed
    }
  }

  return ret + 60;
}

/*
 * Thread
 */
static void* _epggrab_thread ( void* p )
{
  int confver = 0;
  struct timespec ts;
  pthread_mutex_lock(&epggrab_mutex);

  /* Load */
  _epggrab_load();

  /* Setup timeout */
  ts.tv_nsec = 0; 
  ts.tv_sec  = 0;

  while ( 1 ) {

    /* Check for config change */
    while ( confver == epggrab_confver ) {
      int err = pthread_cond_timedwait(&epggrab_cond, &epggrab_mutex, &ts);
      if ( err == ETIMEDOUT ) break;
    }
    confver = epggrab_confver;

    /* Run grabber */
    ts.tv_sec = epggrab_advanced
              ? _epggrab_thread_advanced()
              : _epggrab_thread_simple();
  }

  return NULL;
}

/* **************************************************************************
 * Global Functions
 * *************************************************************************/

/*
 * Initialise
 */
void epggrab_init ( void )
{
  /* Defaults */
  epggrab_advanced  = 0;
  epggrab_eit       = 1;         // on air grab enabled
  epggrab_interval  = 12 * 3600; // hours
  epggrab_module    = NULL;      // disabled

  /* Initialise modules */
  eit_init(&epggrab_modules);
  xmltv_init(&epggrab_modules);
  pyepg_init(&epggrab_modules);

  /* Start thread */
  pthread_t      tid;
  pthread_attr_t tattr;
  pthread_attr_init(&tattr);
  pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
  pthread_create(&tid, &tattr, _epggrab_thread, NULL);
}

void epggrab_channel_add ( channel_t *ch )
{
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->ch_add) 
      if (m->ch_add(m, ch) && m->ch_save)
        m->ch_save(m);
  }
}

void epggrab_channel_rem ( channel_t *ch )
{
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->ch_rem) 
      if (m->ch_rem(m, ch) && m->ch_save)
        m->ch_save(m);
  }
}

void epggrab_channel_mod ( channel_t *ch )
{
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->ch_mod) 
      if (m->ch_mod(m, ch) && m->ch_save)
        m->ch_save(m);
  }
}
