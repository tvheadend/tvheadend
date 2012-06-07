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
#include "src/queue.h"
#include "epg.h"
#include "epggrab.h"
#include "epggrab/eit.h"
#include "epggrab/xmltv.h"
#include "epggrab/pyepg.h"
#include "channels.h"
#include "spawn.h"
#include "htsmsg_xml.h"
#include "file.h"

/* Thread protection */
int                   epggrab_confver;
pthread_mutex_t       epggrab_mutex;
pthread_cond_t        epggrab_cond;

/* Config */
uint32_t              epggrab_eitenabled;
uint32_t              epggrab_interval;
epggrab_module_t*     epggrab_module;
epggrab_module_list_t epggrab_modules;

/* **************************************************************************
 * Helpers
 * *************************************************************************/

/*
 * Run the parse
 */
static void _epggrab_module_parse
  ( epggrab_module_t *mod, htsmsg_t *data )
{
  time_t tm1, tm2;
  int save = 0;
  epggrab_stats_t stats;

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
}

/*
 * Grab from module
 */
static void _epggrab_module_grab ( epggrab_module_t *mod )
{
  time_t tm1, tm2;
  htsmsg_t *data;

  /* Grab */
  time(&tm1);
  data = mod->trans(mod, mod->grab(mod));
  time(&tm2);

  /* Process */
  if ( data ) {
    tvhlog(LOG_DEBUG, mod->id, "grab took %d seconds", tm2 - tm1);
    _epggrab_module_parse(mod, data);
  } else {
    tvhlog(LOG_WARNING, mod->id, "grab returned no data");
  }
}

/*
 * Socket handler
 *
 * TODO: could make this threaded to allow multiple simultaneous inputs
 */
static void _epggrab_socket_handler ( epggrab_module_t *mod, int s )
{
  size_t outlen;
  char *outbuf;
  time_t tm1, tm2;
  htsmsg_t *data = NULL;

  /* Grab/Translate */
  time(&tm1);
  outlen = file_readall(s, &outbuf);
  if (outlen) data = mod->trans(mod, outbuf);
  time(&tm2);

  /* Process */
  if ( data ) {
    tvhlog(LOG_DEBUG, mod->id, "grab took %d seconds", tm2 - tm1);
    _epggrab_module_parse(mod, data);

  /* Failed */
  } else {
    tvhlog(LOG_ERR, mod->id, "failed to read data");
  }
}

/* **************************************************************************
 * Threads
 * *************************************************************************/

/*
 * Thread (for internal grabbing)
 */
static void* _epggrab_internal_thread ( void* p )
{
  int confver = -1; // force first run
  struct timespec ts;
  epggrab_module_t *mod;

  /* Setup timeout */
  ts.tv_nsec = 0; 
  ts.tv_sec  = 0;

  while ( 1 ) {

    /* Check for config change */
    pthread_mutex_lock(&epggrab_mutex);
    while ( confver == epggrab_confver ) {
      int err = pthread_cond_timedwait(&epggrab_cond, &epggrab_mutex, &ts);
      if ( err == ETIMEDOUT ) break;
    }
    confver    = epggrab_confver;
    mod        = NULL;//epggrab_module;
    ts.tv_sec += epggrab_interval;
    pthread_mutex_unlock(&epggrab_mutex);

    /* Run grabber */
    if (mod) _epggrab_module_grab(mod);
  }

  return NULL;
}

/*
 * External (socket) grab thread
 *
 * TODO: I could common all of this up and have a single thread
 *       servicing all the available sockets, but we're unlikely to
 *       have a massive number of modules enabled anyway!
 */
static void *_epggrab_socket_thread ( void *p )
{
  int s;
  epggrab_module_t *mod = (epggrab_module_t*)p;
  
  while ( mod->enabled && mod->sock ) {
    tvhlog(LOG_DEBUG, mod->id, "waiting for connection");
    s = accept(mod->sock, NULL, NULL);
    if (s <= 0) break; // assume closed
    tvhlog(LOG_DEBUG, mod->id, "got connection %d", s);
    _epggrab_socket_handler(mod, s);
  }
  tvhlog(LOG_DEBUG, mod->id, "terminated");
  return NULL;
}

/* **************************************************************************
 * Base Module functions
 * *************************************************************************/

static int _ch_id_cmp ( void *a, void *b )
{
  return strcmp(((epggrab_channel_t*)a)->id,
                ((epggrab_channel_t*)b)->id);
}

static int _ch_match ( epggrab_channel_t *ec, channel_t *ch )
{
  // TODO: this needs implementing
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
  // TODO: implement this
  return NULL;
}

epggrab_channel_t *epggrab_module_channel_find
  ( epggrab_module_t *mod, const char *id )
{
  epggrab_channel_t skel; 
  skel.id = (char*)id;
  return RB_FIND(mod->channels, &skel, link, _ch_id_cmp);
}

// TODO: could use TCP socket to allow remote access
int epggrab_module_enable_socket ( epggrab_module_t *mod, uint8_t e )
{
  pthread_t      tid;
  pthread_attr_t tattr;
  struct sockaddr_un addr;
  assert(mod->path);
  assert(mod->flags & EPGGRAB_MODULE_EXTERNAL);
  
  /* Ignore */
  if ( mod->enabled == e ) return 0;

  /* Disable */
  if (!e) {
    shutdown(mod->sock, SHUT_RDWR);
    close(mod->sock);
    unlink(mod->path);
    mod->sock = 0;
  
  /* Enable */
  } else {
    unlink(mod->path); // just in case!

    mod->sock = socket(AF_UNIX, SOCK_STREAM, 0);
    assert(mod->sock);

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, mod->path, 100);
    if ( bind(mod->sock, (struct sockaddr*)&addr,
             sizeof(struct sockaddr_un)) != 0 ) {
      tvhlog(LOG_ERR, mod->id, "failed to bind socket");
      close(mod->sock);
      mod->sock = 0;
      return 0;
    }

    if ( listen(mod->sock, 5) != 0 ) {
      tvhlog(LOG_ERR, mod->id, "failed to listen on socket");
      close(mod->sock);
      mod->sock = 0;
      return 0;
    }

    tvhlog(LOG_DEBUG, mod->id, "starting socket thread");
    pthread_attr_init(&tattr);
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &tattr, _epggrab_socket_thread, mod);
  }
  mod->enabled = e;
  return 1;
}

char *epggrab_module_grab ( epggrab_module_t *mod )
{ 
  int        outlen;
  char       *outbuf;

  /* Debug */
  tvhlog(LOG_DEBUG, mod->id, "grab %s", mod->path);

  /* Grab */
  outlen = spawn_and_store_stdout(mod->path, NULL, &outbuf);
  if ( outlen < 1 ) {
    tvhlog(LOG_ERR, "pyepg", "no output detected");
    return NULL;
  }

  return outbuf;
}

htsmsg_t *epggrab_module_trans_xml
  ( epggrab_module_t *mod, char *c )
{
  htsmsg_t *ret;
  char     errbuf[100];

  if (!c) return NULL;

  /* Extract */
  ret = htsmsg_xml_deserialize(c, errbuf, sizeof(errbuf));
  if (!ret)
    tvhlog(LOG_ERR, "pyepg", "htsmsg_xml_deserialize error %s", errbuf);
  return ret;
}

const char *epggrab_module_socket_path ( epggrab_module_t *mod )
{
  char *ret = malloc(100);
  snprintf(ret, 100, "%s/epggrab/%s.sock",
           hts_settings_get_root(), mod->id);
  return ret;
}

/* **************************************************************************
 * Configuration
 * *************************************************************************/

static void _epggrab_load ( void )
{
  epggrab_module_t *mod;
  htsmsg_t *m, *a;
  const char *str;
  
  /* No config */
  if ((m = hts_settings_load("epggrab/config")) == NULL)
    return;

  /* Load settings */
  htsmsg_get_u32(m, "eit",      &epggrab_eitenabled);
  htsmsg_get_u32(m, "interval", &epggrab_interval);
  if ( (str = htsmsg_get_str(m, "module")) )
    epggrab_module = epggrab_module_find_by_id(str);
  if ( (a = htsmsg_get_map(m, "mod_enabled")) ) {
    LIST_FOREACH(mod, &epggrab_modules, link) {
      if (htsmsg_get_u32_or_default(a, mod->id, 0)) {
        if (mod->enable) mod->enable(mod, 1);
      }
    }
  }
  htsmsg_destroy(m);
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
  htsmsg_add_u32(m, "eitenabled", epggrab_eitenabled);
  htsmsg_add_u32(m, "interval",   epggrab_interval);
  if ( epggrab_module )
    htsmsg_add_str(m, "module", epggrab_module->id);
  a = htsmsg_create_map();
  LIST_FOREACH(mod, &epggrab_modules, link) {
    if (mod->enabled) htsmsg_add_u32(a, mod->id, 1);
  }
  htsmsg_add_msg(m, "mod_enabled", a);
  hts_settings_save(m, "epggrab/config");
  htsmsg_destroy(m);
}

int epggrab_set_eitenabled ( uint32_t eitenabled )
{
  // TODO: could use module variable
  int save = 0;
  if ( epggrab_eitenabled != eitenabled ) {
    save = 1;
    epggrab_eitenabled = eitenabled;
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
  if ( epggrab_module != mod ) {
    if (mod) {
      assert(mod->grab);
      assert(mod->trans);
      assert(mod->parse);
    }
    epggrab_module = mod;
    save           = 1;
  }
  return save;
}

int epggrab_set_module_by_id ( const char *id )
{
  return epggrab_set_module(epggrab_module_find_by_id(id));
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
 * Global Functions
 * *************************************************************************/

/*
 * Initialise
 */
void epggrab_init ( void )
{
  /* Defaults */
  epggrab_eitenabled = 1;         // on air grab enabled
  epggrab_interval   = 12 * 3600; // hours
  epggrab_module     = NULL;      // disabled

  /* Initialise modules */
  eit_init(&epggrab_modules);
  xmltv_init(&epggrab_modules);
  pyepg_init(&epggrab_modules);

  /* Load config */
  _epggrab_load();

  /* Start internal grab thread */
  pthread_t      tid;
  pthread_attr_t tattr;
  pthread_attr_init(&tattr);
  pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
  pthread_create(&tid, &tattr, _epggrab_internal_thread, NULL);
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
    if(m->name) htsmsg_add_str(e, "name", m->name);
    if(m->path) htsmsg_add_str(e, "path", m->path);
    htsmsg_add_u32(e, "flags", m->flags);
    htsmsg_add_u32(e, "enabled", m->enabled);
    htsmsg_add_msg(a, NULL, e);
  }
  return a;
}
