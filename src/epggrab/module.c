/*
 *  EPG Grabber - module functions
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

#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "tvheadend.h"
#include "settings.h"
#include "htsmsg.h"
#include "htsmsg_xml.h"
#include "service.h"
#include "channels.h"
#include "spawn.h"
#include "file.h"
#include "epg.h"
#include "epggrab.h"
#include "epggrab/private.h"

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
    if (m->type == EPGGRAB_INT)
      htsmsg_add_str(e, "type", "internal");
    else if (m->type == EPGGRAB_EXT)
      htsmsg_add_str(e, "type", "external");
    else
      htsmsg_add_str(e, "type", "ota");
    htsmsg_add_u32(e, "enabled", m->enabled);
    if(m->name) 
      htsmsg_add_str(e, "name", m->name);
    if(m->type == EPGGRAB_EXT) {
      epggrab_module_ext_t *ext = (epggrab_module_ext_t*)m;
      if (ext->path)
        htsmsg_add_str(e, "path", ext->path);
    }
    htsmsg_add_msg(a, NULL, e);
  }
  return a;
}

/* **************************************************************************
 * Generic module routines
 * *************************************************************************/

epggrab_module_t *epggrab_module_create
  ( epggrab_module_t *skel, const char *id, const char *name, int priority,
    epggrab_channel_tree_t *channels )
{
  assert(skel);
  
  /* Setup */
  skel->id       = strdup(id);
  skel->name     = strdup(name);
  skel->priority = priority;
  skel->channels = channels;
  if (channels) {
    skel->ch_save = epggrab_module_ch_save;
    skel->ch_add  = epggrab_module_ch_add;
    skel->ch_mod  = epggrab_module_ch_mod;
    skel->ch_rem  = epggrab_module_ch_rem;
  }

  /* Insert */
  assert(!epggrab_module_find_by_id(id));
  LIST_INSERT_HEAD(&epggrab_modules, skel, link);
  tvhlog(LOG_INFO, "epggrab", "module %s created", id);

  return skel;
}

/*
 * Run the parse
 */
void epggrab_module_parse( void *m, htsmsg_t *data )
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
  tvhlog(LOG_INFO, mod->id, "parse took %"PRItime_t" seconds", tm2 - tm1);
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

/* **************************************************************************
 * Module channel routines
 * *************************************************************************/

void epggrab_module_ch_save ( void *_m, epggrab_channel_t *ch )
{
  htsmsg_t *a = NULL, *m = htsmsg_create_map();
  epggrab_module_t *mod = _m;
  epggrab_channel_link_t *ecl;

  if (ch->name)
    htsmsg_add_str(m, "name", ch->name);
  if (ch->icon)
    htsmsg_add_str(m, "icon", ch->icon);
  LIST_FOREACH(ecl, &ch->channels, ecl_epg_link) {
    if (!a) a = htsmsg_create_list();
    htsmsg_add_str(a, NULL, channel_get_uuid(ecl->ecl_channel));
  }
  if (a) htsmsg_add_msg(m, "channels", a);
  if (ch->major)
    htsmsg_add_u32(m, "major", ch->major);
  if (ch->minor)
    htsmsg_add_u32(m, "minor", ch->minor);

  hts_settings_save(m, "epggrab/%s/channels/%s", mod->id, ch->id);
  htsmsg_destroy(m);
}

void epggrab_module_ch_add ( void *m, channel_t *ch )
{
  epggrab_channel_t *egc;
  epggrab_module_int_t *mod = m;
  RB_FOREACH(egc, mod->channels, link) {
    if (epggrab_channel_match_and_link(egc, ch)) break;
  }
}

void epggrab_module_ch_rem ( void *m, channel_t *ch )
{
  epggrab_channel_link_t *ecl;
  while ((ecl = LIST_FIRST(&ch->ch_epggrab)))
    epggrab_channel_link_delete(ecl, 1);
}

void epggrab_module_ch_mod ( void *mod, channel_t *ch )
{
  return epggrab_module_ch_add(mod, ch);
}

static void _epggrab_module_channel_load 
  ( epggrab_module_t *mod, htsmsg_t *m, const char *id )
{
  int save = 0;
  const char *str;
  uint32_t u32;
  htsmsg_t *a;
  htsmsg_field_t *f;
  channel_t *ch;
  epggrab_channel_t *egc;
  
  egc  = epggrab_channel_find(mod->channels, id, 1, &save, mod);

  if ((str = htsmsg_get_str(m, "name")))
    egc->name = strdup(str);
  if ((str = htsmsg_get_str(m, "icon")))
    egc->icon = strdup(str);
  if(!htsmsg_get_u32(m, "major", &u32))
    egc->major = u32;
  if(!htsmsg_get_u32(m, "minor", &u32))
    egc->minor = u32;
  if ((a = htsmsg_get_list(m, "channels"))) {
    HTSMSG_FOREACH(f, a) {
      if ((str = htsmsg_field_get_str(f))) {
        if ((ch = channel_find_by_uuid(str)))
          epggrab_channel_link(egc, ch);
      }
    }

  /* Compat with older 3.1 code */
  } else if (!htsmsg_get_u32(m, "channel", &u32)) {
    if ((ch = channel_find_by_id(u32)))
      epggrab_channel_link(egc, ch);
  }
}

void epggrab_module_channels_load ( epggrab_module_t *mod )
{
  htsmsg_t *m, *e;
  htsmsg_field_t *f;
  if (!mod || !mod->channels) return;
  if ((m = hts_settings_load("epggrab/%s/channels", mod->id))) {
    HTSMSG_FOREACH(f, m) {
      if ((e = htsmsg_get_map_by_field(f)))
        _epggrab_module_channel_load(mod, e, f->hmf_name);
    }
    htsmsg_destroy(m);
  }
}

/* **************************************************************************
 * Internal module routines
 * *************************************************************************/

static void
epggrab_module_int_done( void *m )
{
  epggrab_module_int_t *mod = m;
  free((char *)mod->path);
  mod->path = NULL;
}

epggrab_module_int_t *epggrab_module_int_create
  ( epggrab_module_int_t *skel,
    const char *id, const char *name, int priority,
    const char *path,
    char* (*grab) (void*m),
    int (*parse) (void *m, htsmsg_t *data, epggrab_stats_t *sta),
    htsmsg_t* (*trans) (void *mod, char *data),
    epggrab_channel_tree_t *channels )
{
  /* Allocate data */
  if (!skel) skel = calloc(1, sizeof(epggrab_module_int_t));
  
  /* Pass through */
  epggrab_module_create((epggrab_module_t*)skel, id, name, priority, channels);

  /* Int data */
  skel->type     = EPGGRAB_INT;
  skel->path     = strdup(path);
  skel->channels = channels;
  skel->grab     = grab  ?: epggrab_module_grab_spawn;
  skel->trans    = trans ?: epggrab_module_trans_xml;
  skel->parse    = parse;
  skel->done     = epggrab_module_int_done;

  return skel;
}

char *epggrab_module_grab_spawn ( void *m )
{ 
  int        rd = -1, outlen;
  char       *outbuf;
  epggrab_module_int_t *mod = m;
  char **argv = NULL;

  /* Debug */
  tvhlog(LOG_INFO, mod->id, "grab %s", mod->path);

  /* Arguments */
  if (spawn_parse_args(&argv, 64, mod->path, NULL)) {
    tvhlog(LOG_ERR, mod->id, "unable to parse arguments");
    return NULL;
  }

  /* Grab */
  outlen = spawn_and_give_stdout(argv[0], (char **)argv, NULL, &rd, NULL, 1);

  spawn_free_args(argv);

  if (outlen < 0)
    goto error;

  outlen = file_readall(rd, &outbuf);
  if (outlen < 1)
    goto error;

  close(rd);

  return outbuf;

error:
  spawn_free_args(argv);
  if (rd >= 0)
    close(rd);
  tvhlog(LOG_ERR, mod->id, "no output detected");
  return NULL;
}



htsmsg_t *epggrab_module_trans_xml ( void *m,  char *c )
{
  htsmsg_t *ret;
  char     errbuf[100];
  epggrab_module_t *mod = m;

  if (!c) return NULL;

  /* Extract */
  ret = htsmsg_xml_deserialize(c, errbuf, sizeof(errbuf));
  if (!ret)
    tvhlog(LOG_ERR, mod->id, "htsmsg_xml_deserialize error %s", errbuf);
  return ret;
}

/* **************************************************************************
 * External module routines
 * *************************************************************************/

/*
 * Socket handler
 */
static void _epggrab_socket_handler ( epggrab_module_ext_t *mod, int s )
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
    tvhlog(LOG_INFO, mod->id, "grab took %"PRItime_t" seconds", tm2 - tm1);
    epggrab_module_parse(mod, data);

  /* Failed */
  } else {
    tvhlog(LOG_ERR, mod->id, "failed to read data");
  }
}


/*
 * External (socket) grab thread
 */
static void *_epggrab_socket_thread ( void *p )
{
  int s;
  epggrab_module_ext_t *mod = (epggrab_module_ext_t*)p;
  tvhlog(LOG_INFO, mod->id, "external socket enabled");
  
  while ( mod->enabled && mod->sock ) {
    tvhlog(LOG_DEBUG, mod->id, "waiting for connection");
    s = accept(mod->sock, NULL, NULL);
    if (s <= 0) continue;
    tvhlog(LOG_DEBUG, mod->id, "got connection %d", s);
    _epggrab_socket_handler(mod, s);
  }
  tvhlog(LOG_DEBUG, mod->id, "terminated");
  return NULL;
}

/*
 * Shutdown socket module
 */
static void
epggrab_module_done_socket( void *m )
{
  epggrab_module_ext_t *mod = (epggrab_module_ext_t*)m;
  int sock;

  assert(mod->type == EPGGRAB_EXT);
  mod->enabled = 0;
  sock = mod->sock;
  mod->sock = 0;
  shutdown(sock, SHUT_RDWR);
  close(sock);
  if (mod->tid) {
    pthread_kill(mod->tid, SIGQUIT);
    pthread_join(mod->tid, NULL);
  }
  mod->tid = 0;
  if (mod->path)
    unlink(mod->path);
  epggrab_module_int_done(mod);
}

/*
 * Enable socket module
 */
static int
epggrab_module_enable_socket ( void *m, uint8_t e )
{
  pthread_attr_t tattr;
  struct sockaddr_un addr;
  epggrab_module_ext_t *mod = (epggrab_module_ext_t*)m;
  assert(mod->type == EPGGRAB_EXT);
  
  /* Ignore */
  if ( mod->enabled == e ) return 0;

  /* Disable */
  if (!e) {
    epggrab_module_done_socket(m);
  
  /* Enable */
  } else {
    unlink(mod->path); // just in case!
    hts_settings_makedirs(mod->path);

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
    mod->enabled = 1;
    tvhthread_create(&mod->tid, &tattr, _epggrab_socket_thread, mod);
  }
  return 1;
}

/*
 * Create a module
 */
epggrab_module_ext_t *epggrab_module_ext_create
  ( epggrab_module_ext_t *skel,
    const char *id, const char *name, int priority, const char *sockid,
    int (*parse) (void *m, htsmsg_t *data, epggrab_stats_t *sta),
    htsmsg_t* (*trans) (void *mod, char *data),
    epggrab_channel_tree_t *channels )
{
  char path[512];

  /* Allocate data */
  if (!skel) skel = calloc(1, sizeof(epggrab_module_ext_t));
  
  /* Pass through */
  hts_settings_buildpath(path, sizeof(path), "epggrab/%s.sock", sockid);
  epggrab_module_int_create((epggrab_module_int_t*)skel,
                            id, name, priority, path,
                            NULL, parse, trans,
                            channels);

  /* Local */
  skel->type   = EPGGRAB_EXT;
  skel->enable = epggrab_module_enable_socket;
  skel->done   = epggrab_module_done_socket;

  return skel;
}

/* **************************************************************************
 * OTA module functions
 * *************************************************************************/

epggrab_module_ota_t *epggrab_module_ota_create
  ( epggrab_module_ota_t *skel,
    const char *id, const char *name, int priority,
    epggrab_ota_module_ops_t *ops,
    epggrab_channel_tree_t *channels )
{
  if (!skel) skel = calloc(1, sizeof(epggrab_module_ota_t));
  
  /* Pass through */
  epggrab_module_create((epggrab_module_t*)skel, id, name, priority, channels);

  /* Setup */
  skel->type   = EPGGRAB_OTA;
  skel->enable = ops->enable;
  skel->start  = ops->start;
  skel->done   = ops->done;
  skel->tune   = ops->tune;
  //TAILQ_INIT(&skel->muxes);

  return skel;
}

