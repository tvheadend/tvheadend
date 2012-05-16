#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "htsmsg.h"
#include "settings.h"
#include "tvheadend.h"
#include "epggrab.h"

/* Thread protection */
int               epggrab_confver;
pthread_mutex_t   epggrab_mutex;
pthread_cond_t    epggrab_cond;

/* Config */
uint32_t             epggrab_advanced;
uint32_t             epggrab_eit;
uint32_t             epggrab_interval;
epggrab_module_t*    epggrab_module;
epggrab_sched_list_t epggrab_schedule;

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
  epggrab_eit       = 1;    // on air grab enabled
  epggrab_interval  = 12;   // hours
  epggrab_module    = NULL; // disabled

  // TODO: HACK : testing
#if 0
  epggrab_advanced = 1;
  epggrab_sched_t es;
  memset(&es, 0, sizeof(epggrab_sched_t));
  es.opts      = strdup("-d 1");
  es.mod       = NULL;
  es.cron.hour = 0x1001;
  _epggrab_set_schedule(1, &es);
  _epggrab_save();
#endif

  /* Start thread */
  pthread_t      tid;
  pthread_attr_t tattr;
  pthread_attr_init(&tattr);
  pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
  pthread_create(&tid, &tattr, _epggrab_thread, NULL);
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
  if ( mod ) mod->run(NULL);

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
    if ( s->mod && cron_is_time(&s->cron) ) {
      s->mod->run(s->opts);
    }
  }

  // TODO: make this driven off next time
  //       get cron to tell us when next call will run
  return time(NULL) + 30;
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
  return NULL;
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
  printf("advanced = %d\n", epggrab_advanced);
  htsmsg_get_u32(m, "eit",      &epggrab_eit);
  printf("eit      = %d\n", epggrab_eit);
  if ( !epggrab_advanced ) {
    htsmsg_get_u32(m, "interval", &epggrab_interval);
    printf("interval = %d\n", epggrab_interval);
    str = htsmsg_get_str(m, "module");
    if (str) epggrab_module = epggrab_module_find_by_name(str);
    printf("module   = %p\n", epggrab_module);
  } else {
    if ((s = htsmsg_get_list(m, "schedule")) != NULL) {
      HTSMSG_FOREACH(f, s) {
        if ((e = htsmsg_get_map_by_field(f)) != NULL) {
          es = calloc(1, sizeof(epggrab_sched_t));
          str = htsmsg_get_str(e, "module");
          if (str) es->mod  = epggrab_module_find_by_name(str);
          str = htsmsg_get_str(e, "opts");
          if (str) es->opts = strdup(str);
          c = htsmsg_get_map(e, "cron");
          if (f) cron_unpack(&es->cron, c);
          printf("scheduleN: mod %p opts %s cron 0x%04X\n",
                 es->mod, es->opts, es->cron.hour);
        }
      }
    }
  }
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
      if ( es->mod  ) htsmsg_add_str(e, "module", es->mod->name());
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

  /* Remove existing */
#if 0
  for ( i = 0; i < epggrab_schedcnt; i++ ) {
    if ( epggrab_schedule[i].opts ) free(epggrab_schedule[i].opts);
  }
  free(epggrab_schedule);
#endif

  /* Create new */
  for ( i = 0; i < count; i++ ) {
    epggrab_sched_t *es = calloc(1, sizeof(epggrab_sched_t));
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
