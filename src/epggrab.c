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
#include "service.h"

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
    tvhlog(LOG_INFO, mod->id, "grab took %d seconds", tm2 - tm1);
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
    tvhlog(LOG_INFO, mod->id, "grab took %d seconds", tm2 - tm1);
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
  int err, confver = -1; // force first run
  struct timespec ts;
  epggrab_module_t *mod;

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
  tvhlog(LOG_INFO, mod->id, "external socket enabled");
  
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

// TODO: add other matches
static int _ch_link ( epggrab_channel_t *ec, channel_t *ch )
{
  service_t *sv;
  int match = 0, i;

  if (!ec || !ch) return 0;
  if (ec->channel) return 0;

  if (ec->name && !strcmp(ec->name, ch->ch_name))
    match = 1;
  else {
    LIST_FOREACH(sv, &ch->ch_services, s_ch_link) {
      if (ec->sid) {
        for (i = 0; i < ec->sid_cnt; i++ ) {
          if (sv->s_dvb_service_id == ec->sid[i]) {
            match = 1;
            break;
          }
        }
      }
      if (!match && ec->sname) {
        i = 0;
        while (ec->sname[i]) {
          if (!strcmp(ec->sname[i], sv->s_svcname)) {
            match = 1;
            break;
          }
          i++;
        }
      }
      if (match) break;
    }
  }

  if (match) {
    tvhlog(LOG_INFO, ec->mod->id, "linking %s to %s",
           ec->id, ch->ch_name);
    ec->channel = ch;
    if (ec->name)     channel_rename(ch, ec->name);
    if (ec->icon)     channel_set_icon(ch, ec->icon);
    if (ec->number>0) channel_set_number(ch, ec->number);
  }

  return match;
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

const char *epggrab_module_socket_path ( const char *id )
{
  char *ret = malloc(100);
  snprintf(ret, 100, "%s/epggrab/%s.sock",
           hts_settings_get_root(), id);
  return ret;
}

char *epggrab_module_grab ( epggrab_module_t *mod )
{ 
  int        outlen;
  char       *outbuf;

  /* Debug */
  tvhlog(LOG_INFO, mod->id, "grab %s", mod->path);

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

// TODO: add extra metadata
void epggrab_module_channel_save
  ( epggrab_module_t *mod, epggrab_channel_t *ch )
{
  int i;
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_t *a;

  if (ch->name)
    htsmsg_add_str(m, "name", ch->name);
  if (ch->icon)
    htsmsg_add_str(m, "icon", ch->icon);
  if (ch->channel)
    htsmsg_add_u32(m, "channel", ch->channel->ch_id);
  if (ch->sid) {
    a = htsmsg_create_list();
    for (i = 0; i < ch->sid_cnt; i++ ) {
      htsmsg_add_u32(a, NULL, ch->sid[i]);
    }
    htsmsg_add_msg(m, "sid", a);
  }
  if (ch->sname) {
    a = htsmsg_create_list();
    i = 0;
    while (ch->sname[i]) {
      htsmsg_add_str(a, NULL, ch->sname[i]);
      i++;
    }
    htsmsg_add_msg(m, "sname", a);
  }

  hts_settings_save(m, "epggrab/%s/channels/%s", mod->id, ch->id);
}

static void epggrab_module_channel_load 
  ( epggrab_module_t *mod, htsmsg_t *m, const char *id )
{
  int save = 0, i;
  const char *str;
  uint32_t u32;
  htsmsg_t *a;
  htsmsg_field_t *f;

  epggrab_channel_t *ch = epggrab_module_channel_find(mod, id, 1, &save);

  if ((str = htsmsg_get_str(m, "name")))
    ch->name = strdup(str);
  if ((str = htsmsg_get_str(m, "icon")))
    ch->icon = strdup(str);
  if ((a = htsmsg_get_list(m, "sid"))) {
    i = 0;
    HTSMSG_FOREACH(f, a) i++;
    ch->sid_cnt = i;
    ch->sid     = calloc(i, sizeof(uint16_t));
    i = 0;
    HTSMSG_FOREACH(f, a) {
      ch->sid[i] = (uint16_t)f->hmf_s64;
      i++;
    }
  }
  if ((a = htsmsg_get_list(m, "sname"))) {
    i = 0;
    HTSMSG_FOREACH(f, a) i++;
    ch->sname = calloc(i+1, sizeof(char*));
    i = 0;
    HTSMSG_FOREACH(f, a) {
      ch->sname[i] = strdup(f->hmf_str);
      i++;
    }
  }

  if (!htsmsg_get_u32(m, "channel", &u32))
    ch->channel = channel_find_by_identifier(u32);
}

void epggrab_module_channels_load ( epggrab_module_t *mod )
{
  htsmsg_t *m, *e;
  htsmsg_field_t *f;

  if ((m = hts_settings_load("epggrab/%s/channels", mod->id))) {
    HTSMSG_FOREACH(f, m) {
      if ((e = htsmsg_get_map_by_field(f)))
        epggrab_module_channel_load(mod, e, f->hmf_name);
    }
  }
}

void epggrab_module_channel_add ( epggrab_module_t *mod, channel_t *ch )
{
  epggrab_channel_t *egc;
  RB_FOREACH(egc, mod->channels, link) {
    if (_ch_link(egc, ch) ) {
      epggrab_module_channel_save(mod, egc);
      break;
    }
  }
}

void epggrab_module_channel_rem ( epggrab_module_t *mod, channel_t *ch )
{
  epggrab_channel_t *egc;
  RB_FOREACH(egc, mod->channels, link) {
    if (egc->channel == ch) {
      egc->channel = NULL;
      epggrab_module_channel_save(mod, egc);
      break;
    }
  }
}

void epggrab_module_channel_mod ( epggrab_module_t *mod, channel_t *ch )
{
  return epggrab_module_channel_add(mod, ch);
}


epggrab_channel_t *epggrab_module_channel_find
  ( epggrab_module_t *mod, const char *id, int create, int *save ) 
{
  epggrab_channel_t *ec;
  static epggrab_channel_t *skel = NULL;

  if (!mod || !mod->channels ) return NULL;

  if ( !skel ) skel = calloc(1, sizeof(epggrab_channel_t));
  skel->id  = (char*)id;
  skel->mod = mod;

  /* Find */
  if (!create) {
    ec = RB_FIND(mod->channels, skel, link, _ch_id_cmp);

  /* Create (if required) */
  } else {
    ec = RB_INSERT_SORTED(mod->channels, skel, link, _ch_id_cmp);
    if ( ec == NULL ) {
      skel->id = strdup(skel->id);
      ec       = skel;
      skel     = NULL;
      *save    = 1;
    }
  }

  return ec;
}

/* **************************************************************************
 * Channels
 * *************************************************************************/

int epggrab_channel_set_name ( epggrab_channel_t *ec, const char *name )
{
  int save = 0;
  if (!ec || !name) return 0;
  if (!ec->name || strcmp(ec->name, name)) {
    if (ec->name) free(ec->name);
    ec->name = strdup(name);
    if (ec->channel) channel_rename(ec->channel, name);
    save = 1;
  }
  return save;
}

// TODO: what a mess!
int epggrab_channel_set_sid 
  ( epggrab_channel_t *ec, const uint16_t *sid, int num )
{
  int save = 0, i = 0;
  if ( !ec || !sid ) return 0;
  if (!ec->sid) save = 1;
  else if (ec->sid_cnt != num) save = 1;
  else {
    for (i = 0; i < num; i++ ) {
      if (sid[i] != ec->sid[i]) {
        save = 1;
        break;
      }
    }
  }
  if (save) {
    if (ec->sid) free(ec->sid);
    ec->sid = calloc(num, sizeof(uint16_t));
    for (i = 0; i < num; i++ ) {
      ec->sid[i] = sid[i];
    }
    ec->sid_cnt = num;
  }
  return save;
}

// TODO: what a mess!
int epggrab_channel_set_sname ( epggrab_channel_t *ec, const char **sname )
{
  int save = 0, i = 0;
  if ( !ec || !sname ) return 0;
  if (!ec->sname) save = 1;
  else {
    while ( ec->sname[i] && sname[i] ) {
      if (strcmp(ec->sname[i], sname[i])) {
        save = 1;
        break;
      }
      i++;
    }
    if (!save && (ec->sname[i] || sname[i])) save = 1;
  }
  if (save) {
    if (ec->sname) {
      i = 0;
      while (ec->sname[i])
        free(ec->sname[i++]);
      free(ec->sname);
    }
    i = 0;
    while (sname[i++]);
    ec->sname = calloc(i+1, sizeof(char*));
    i = 0;
    while (sname[i]) {
      ec->sname[i] = strdup(sname[i]);
      i++;
    } 
  }
  return save;
}

int epggrab_channel_set_icon ( epggrab_channel_t *ec, const char *icon )
{
  int save = 0;
  if (!ec->icon || strcmp(ec->icon, icon) ) {
  if (!ec | !icon) return 0;
    if (ec->icon) free(ec->icon);
    ec->icon = strdup(icon);
    if (ec->channel) channel_set_icon(ec->channel, icon);
    save = 1;
  }
  return save;
}

int epggrab_channel_set_number ( epggrab_channel_t *ec, int number )
{
  int save = 0;
  if (!ec || (number <= 0)) return 0;
  if (ec->number != number) {
    ec->number = number;
    if (ec->channel) channel_set_number(ec->channel, number);
    save = 1;
  }
  return save;
}

// TODO: add other match critera
// TODO: add additional metadata updates
// TODO: add configurable updating
void epggrab_channel_link ( epggrab_channel_t *ec )
{ 
  channel_t *ch;

  if (!ec) return;

  /* Link */
  if (!ec->channel) {
    RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
      if (_ch_link(ec, ch)) break;
    }
  }
}

void epggrab_channel_updated ( epggrab_channel_t *ec )
{
  epggrab_channel_link(ec);
  epggrab_module_channel_save(ec->mod, ec);
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
    if (m->ch_add) m->ch_add(m, ch);
  }
}

void epggrab_channel_rem ( channel_t *ch )
{
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->ch_rem) m->ch_rem(m, ch);
  }
}

void epggrab_channel_mod ( channel_t *ch )
{
  epggrab_module_t *m;
  LIST_FOREACH(m, &epggrab_modules, link) {
    if (m->ch_mod) m->ch_mod(m, ch);
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
