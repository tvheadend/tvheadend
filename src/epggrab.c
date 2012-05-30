#include <errno.h>
#include <stdlib.h>
#include <string.h>
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
int                  epggrab_confver;
pthread_mutex_t      epggrab_mutex;
pthread_cond_t       epggrab_cond;

/* Config */
uint32_t             epggrab_advanced;
uint32_t             epggrab_eit;
uint32_t             epggrab_interval;
epggrab_module_t*    epggrab_module;
epggrab_sched_list_t epggrab_schedule;

/* Modules */
LIST_HEAD(epggrab_module_list, epggrab_module);
struct epggrab_module_list epggrab_modules;

/* Internal prototypes */
static void*  _epggrab_thread          ( void* );
static time_t _epggrab_thread_simple   ( void );
static time_t _epggrab_thread_advanced ( void );
static void   _epggrab_load            ( void );
static void   _epggrab_save            ( void );
static void   _epggrab_set_schedule    ( int, epggrab_sched_t* );

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
  LIST_INSERT_HEAD(&epggrab_modules, eit_init(), glink);
#if TODO_XMLTV_SUPPORT
  LIST_INSERT_HEAD(&epggrab_modules, xmltv_init(), glink);
#endif
  LIST_INSERT_HEAD(&epggrab_modules, pyepg_init(), glink);

  /* Start thread */
  pthread_t      tid;
  pthread_attr_t tattr;
  pthread_attr_init(&tattr);
  pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
  pthread_create(&tid, &tattr, _epggrab_thread, NULL);
}

/*
 * Grab from module
 */
static void _epggrab_module_run ( epggrab_module_t *mod, const char *opts )
{
  int save = 0;
  time_t tm1, tm2;
  htsmsg_t *data;
  epggrab_stats_t stats;
  memset(&stats, 0, sizeof(stats));

  /* Check */
  if ( !mod ) return;
  
  /* Grab */
  time(&tm1);
  data = mod->grab(opts);
  time(&tm2);

  /* Process */
  if ( data ) {
    tvhlog(LOG_DEBUG, mod->name(), "grab took %d seconds", tm2 - tm1);
    pthread_mutex_lock(&global_lock);
    time(&tm1);
    save |= mod->parse(data, &stats);
    time(&tm2);
    if (save) epg_updated();  
    pthread_mutex_unlock(&global_lock);
    htsmsg_destroy(data);

    /* Debug stats */
    tvhlog(LOG_DEBUG, mod->name(), "parse took %d seconds", tm2 - tm1);
    tvhlog(LOG_DEBUG, mod->name(), "  channels   tot=%5d new=%5d mod=%5d",
           stats.channels.total, stats.channels.created,
           stats.channels.modified);
    tvhlog(LOG_DEBUG, mod->name(), "  brands     tot=%5d new=%5d mod=%5d",
           stats.brands.total, stats.brands.created,
           stats.brands.modified);
    tvhlog(LOG_DEBUG, mod->name(), "  seasons    tot=%5d new=%5d mod=%5d",
           stats.seasons.total, stats.seasons.created,
           stats.seasons.modified);
    tvhlog(LOG_DEBUG, mod->name(), "  episodes   tot=%5d new=%5d mod=%5d",
           stats.episodes.total, stats.episodes.created,
           stats.episodes.modified);
    tvhlog(LOG_DEBUG, mod->name(), "  broadcasts tot=%5d new=%5d mod=%5d",
           stats.broadcasts.total, stats.broadcasts.created,
           stats.broadcasts.modified);

  } else {
    tvhlog(LOG_WARNING, mod->name(), "grab returned no data");
  }
}

/*
 * Simple run
 */
time_t _epggrab_thread_simple ( void )
{
  /* Copy config */
  time_t            ret = time(NULL) + epggrab_interval;
  epggrab_module_t* mod = epggrab_module;

  /* Unlock */
  pthread_mutex_unlock(&epggrab_mutex);

  /* Run the module */
  _epggrab_module_run(mod, NULL);

  /* Re-lock */
  pthread_mutex_lock(&epggrab_mutex);

  return ret;
}

/*
 * Advanced run
 *
 * TODO: might be nice to put each module run in another thread, in case
 *       they're scheduled at the same time?
 */
time_t _epggrab_thread_advanced ( void )
{
  epggrab_sched_t *s;

  /* Determine which to run */
  LIST_FOREACH(s, &epggrab_schedule, es_link) {
    if ( cron_is_time(&s->cron) ) {
      _epggrab_module_run(s->mod, s->opts);
    }
  }

  // TODO: make this driven off next time
  //       get cron to tell us when next call will run
  return time(NULL) + 60;
}

/*
 * Thread
 */
void* _epggrab_thread ( void* p )
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
      if ( pthread_cond_timedwait(&epggrab_cond, &epggrab_mutex, &ts) == ETIMEDOUT ) break;
    }

    /* Run grabber */
    ts.tv_sec = epggrab_advanced
              ? _epggrab_thread_advanced()
              : _epggrab_thread_simple();
  }

  return NULL;
}

/* **************************************************************************
 * Module access
 * *************************************************************************/

epggrab_module_t* epggrab_module_find_by_name ( const char *name )
{
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, glink) {
    if ( !strcmp(m->name(), name) ) return m;
  }
  return NULL;
}

/* **************************************************************************
 * Channel handling
 *
 * TODO: this is all a bit messy (too much indirection?) but it works!
 *
 * TODO: need to save mappings to disk
 *
 * TODO: probably want rules on what can be updated etc...
 * *************************************************************************/

static int _ch_id_cmp ( void *a, void *b )
{
  return strcmp(((epggrab_channel_t*)a)->id,
                ((epggrab_channel_t*)b)->id);
}

static channel_t *_channel_find ( epggrab_channel_t *ch )
{
  channel_t *ret = NULL;
  if (ch->name) ret = channel_find_by_name(ch->name, 0, 0);
  return ret;
}

static int _channel_match ( epggrab_channel_t *egc, channel_t *ch )
{
  /* Already linked */
  if ( egc->channel ) return 0;
  
  /* Name match */
  if ( !strcmp(ch->ch_name, egc->name) ) return 1;

  return 0;
}

void epggrab_channel_add ( channel_t *ch )
{
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, glink) {
    if (m->ch_add) m->ch_add(ch);
  }
}

void epggrab_channel_rem ( channel_t *ch )
{
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, glink) {
    if (m->ch_rem) m->ch_rem(ch);
  }
}

void epggrab_channel_mod ( channel_t *ch )
{
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, glink) {
    if (m->ch_mod) m->ch_mod(ch);
  }
}

epggrab_channel_t *epggrab_module_channel_create 
  ( epggrab_channel_tree_t *tree, epggrab_channel_t *iskel )
{
  epggrab_channel_t *egc;
  static epggrab_channel_t *skel = NULL;
  if (!skel) skel = calloc(1, sizeof(epggrab_channel_t));
  skel->id   = iskel->id;
  skel->name = iskel->name;
  egc = RB_INSERT_SORTED(tree, skel, glink, _ch_id_cmp);
  if ( egc == NULL ) {
    skel->id      = strdup(skel->id);
    skel->name    = strdup(skel->name);
    skel->channel = _channel_find(skel);
    egc  = skel;
    skel = NULL;
  }
  return egc;
}

epggrab_channel_t *epggrab_module_channel_find 
  ( epggrab_channel_tree_t *tree, const char *id )
{
  epggrab_channel_t skel;
  skel.id = (char*)id;
  return RB_FIND(tree, &skel, glink, _ch_id_cmp); 
}

void epggrab_module_channel_add ( epggrab_channel_tree_t *tree, channel_t *ch )
{
  epggrab_channel_t *egc;
  RB_FOREACH(egc, tree, glink) {
    if (_channel_match(egc, ch) ) {
      egc->channel = ch;
      break;
    }
  }
}

void epggrab_module_channel_rem ( epggrab_channel_tree_t *tree, channel_t *ch )
{
  epggrab_channel_t *egc;
  RB_FOREACH(egc, tree, glink) {
    if (egc->channel == ch) {
      egc->channel = NULL;
      break;
    }
  }
}

void epggrab_module_channel_mod ( epggrab_channel_tree_t *tree, channel_t *ch )
{
  epggrab_module_channel_add(tree, ch);
}

/* **************************************************************************
 * Configuration handling
 * *************************************************************************/

void _epggrab_load ( void )
{
  htsmsg_t *m, *s, *e, *c;
  htsmsg_field_t *f;
  const char *str;
  epggrab_sched_t *es;
  
  /* No config */
  if ((m = hts_settings_load("epggrab/config")) == NULL)
    return;

  /* Load settings */
  htsmsg_get_u32(m, "advanced", &epggrab_advanced);
  htsmsg_get_u32(m, "eit",      &epggrab_eit);
  if ( !epggrab_advanced ) {
    htsmsg_get_u32(m, "interval", &epggrab_interval);
    str = htsmsg_get_str(m, "module");
    if (str) epggrab_module = epggrab_module_find_by_name(str);
  } else {
    if ((s = htsmsg_get_list(m, "schedule")) != NULL) {
      HTSMSG_FOREACH(f, s) {
        if ((e = htsmsg_get_map_by_field(f)) != NULL) {
          es = calloc(1, sizeof(epggrab_sched_t));
          str = htsmsg_get_str(e, "mod");
          if (str) es->mod  = epggrab_module_find_by_name(str);
          str = htsmsg_get_str(e, "opts");
          if (str) es->opts = strdup(str);
          c = htsmsg_get_map(e, "cron");
          if (f) cron_unpack(&es->cron, c);
          LIST_INSERT_HEAD(&epggrab_schedule, es, es_link);
        }
      }
    }
  }
  htsmsg_destroy(m);
}

void _epggrab_save ( void )
{
  htsmsg_t *m, *s, *e, *c;
  epggrab_sched_t *es;

  /* Enable EIT */

  /* Register */
  epggrab_confver++;
  pthread_cond_signal(&epggrab_cond);

  /* Save */
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "advanced",  epggrab_advanced);
  htsmsg_add_u32(m, "eit",       epggrab_eit);
  if ( !epggrab_advanced ) {
    htsmsg_add_u32(m, "interval",  epggrab_interval);
    if ( epggrab_module )
      htsmsg_add_str(m, "module", epggrab_module->name());
  } else {
    s = htsmsg_create_list();
    LIST_FOREACH(es, &epggrab_schedule, es_link) {
      e = htsmsg_create_map();
      if ( es->mod  ) htsmsg_add_str(e, "mod", es->mod->name());
      if ( es->opts ) htsmsg_add_str(e, "opts",   es->opts);
      c = htsmsg_create_map();
      cron_pack(&es->cron, c);
      htsmsg_add_msg(e, "cron", c);
      htsmsg_add_msg(s, NULL, e);
    }
    htsmsg_add_msg(m, "schedule", s);
  }
  hts_settings_save(m, "epggrab/config");
  htsmsg_destroy(m);
}

void _epggrab_set_schedule ( int count, epggrab_sched_t *sched )
{
  int i;
  epggrab_sched_t *es;

  /* Remove existing */
  while ( !LIST_EMPTY(&epggrab_schedule) ) {
    es = LIST_FIRST(&epggrab_schedule);
    LIST_REMOVE(es, es_link);
    if ( es->opts ) free(es->opts);
    free(es);
  }

  /* Create new */
  for ( i = 0; i < count; i++ ) {
    es = calloc(1, sizeof(epggrab_sched_t));
    es->mod  = sched[i].mod;
    es->cron = sched[i].cron;
    if ( sched[i].opts ) es->opts = strdup(sched[i].opts);
    LIST_INSERT_HEAD(&epggrab_schedule, es, es_link);
  }
}

void epggrab_set_simple ( uint32_t interval, epggrab_module_t *mod )
{
  /* Set config */
  lock_assert(&epggrab_mutex);
  epggrab_advanced = 0;
  epggrab_interval = interval;
  epggrab_module   = mod;

  /* Save */
  _epggrab_save();
}

void epggrab_set_advanced ( uint32_t count, epggrab_sched_t *sched )
{
  /* Set config */
  lock_assert(&epggrab_mutex);
  epggrab_advanced = 1;
  _epggrab_set_schedule(count, sched);

  /* Save */
  _epggrab_save();
}

void epggrab_set_eit ( uint32_t eit )
{
  /* Set config */
  lock_assert(&epggrab_mutex);
  epggrab_eit = eit;
  
  /* Save */
  _epggrab_save();
}
