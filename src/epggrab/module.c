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
  LIST_FOREACH(m, &epggrab_modules, link)
    if (!strcmp(m->id, id))
      return m;
  return NULL;
}

const char *
epggrab_module_type(epggrab_module_t *mod)
{
  switch (mod->type) {
  case EPGGRAB_OTA: return N_("Over-the-air");
  case EPGGRAB_INT: return N_("Internal");
  case EPGGRAB_EXT: return N_("External");
  default:          return N_("Unknown");
  }
}

const char *
epggrab_module_get_status(epggrab_module_t *mod)
{
  if (mod->enabled)
    return "epggrabmodEnabled";
  return "epggrabmodNone";
}

/*
 * Class
 */

static const char *epggrab_mod_class_title(idnode_t *self, const char *lang)
{
  epggrab_module_t *mod = (epggrab_module_t *)self;
  const char *s1 = tvh_gettext_lang(lang, epggrab_module_type(mod));
  const char *s2 = mod->name;
  if (s2 == NULL || s2[0] == '\0')
    s2 = mod->id;
  snprintf(prop_sbuf, PROP_SBUF_LEN, "%s: %s", s1, s2);
  return prop_sbuf;
}

static void
epggrab_mod_class_changed(idnode_t *self)
{
  epggrab_module_t *mod = (epggrab_module_t *)self;
  epggrab_activate_module(mod, mod->enabled);
  idnode_changed(&epggrab_conf.idnode);
}

static const void *epggrab_mod_class_type_get(void *o)
{
  static const char *s;
  s = epggrab_module_type((epggrab_module_t *)o);
  return &s;
}

static int epggrab_mod_class_type_set(void *o, const void *v)
{
  return 0;
}

const idclass_t epggrab_mod_class = {
  .ic_class      = "epggrab_mod",
  .ic_caption    = N_("EPG grabber"),
  .ic_event      = "epggrab_mod",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_get_title  = epggrab_mod_class_title,
  .ic_changed    = epggrab_mod_class_changed,
  .ic_groups     = (const property_group_t[]) {
     {
        .name   = N_("Settings"),
        .number = 1,
     },
     {}
  },
  .ic_properties = (const property_t[]){
    {
      .type   = PT_STR,
      .id     = "name",
      .name   = N_("Name"),
      .desc   = N_("The EPG grabber name."),
      .off    = offsetof(epggrab_module_t, name),
      .opts   = PO_RDONLY,
      .group  = 1,
    },
    {
      .type   = PT_STR,
      .id     = "type",
      .name   = N_("Type"),
      .desc   = N_("The EPG grabber type."),
      .get    = epggrab_mod_class_type_get,
      .set    = epggrab_mod_class_type_set,
      .opts   = PO_RDONLY | PO_LOCALE,
      .group  = 1,
    },
    {
      .type   = PT_BOOL,
      .id     = "enabled",
      .name   = N_("Enabled"),
      .desc   = N_("Enable/disable the grabber."),
      .off    = offsetof(epggrab_module_t, enabled),
      .group  = 1,
    },
    {
      .type   = PT_INT,
      .id     = "priority",
      .name   = N_("Priority"),
      .desc   = N_("Grabber priority. This option lets you pick which "
                   "EPG grabber's data gets used first if more than one "
                   "grabber is enabled. Priority is given to the grabber "
                   "with the highest value set here."),
      .off    = offsetof(epggrab_module_t, priority),
      .opts   = PO_ADVANCED,
      .group  = 1
    },
    {}
  }
};

const idclass_t epggrab_class_mod_int = {
  .ic_super      = &epggrab_mod_class,
  .ic_class      = "epggrab_mod_int",
  .ic_caption    = N_("Internal EPG grabber"),
  .ic_properties = (const property_t[]){
    {
      .type   = PT_STR,
      .id     = "path",
      .name   = N_("Path"),
      .desc   = N_("Path to the grabber executable."),
      .off    = offsetof(epggrab_module_int_t, path),
      .opts   = PO_RDONLY | PO_NOSAVE,
      .group  = 1
    },
    {
      .type   = PT_STR,
      .id     = "args",
      .name   = N_("Extra arguments"),
      .desc   = N_("Additional arguments to pass to the grabber."),
      .off    = offsetof(epggrab_module_int_t, args),
      .opts   = PO_ADVANCED,
      .group  = 1
    },
    {}
  }
};

const idclass_t epggrab_class_mod_ext = {
  .ic_super      = &epggrab_mod_class,
  .ic_class      = "epggrab_mod_ext",
  .ic_caption    = N_("External EPG grabber"),
  .ic_properties = (const property_t[]){
    {
      .type   = PT_STR,
      .id     = "path",
      .name   = N_("Path"),
      .desc   = N_("Path to the socket Tvheadend will read data from."),
      .off    = offsetof(epggrab_module_ext_t, path),
      .opts   = PO_RDONLY | PO_NOSAVE,
      .group  = 1
    },
    {}
  }
};

const idclass_t epggrab_class_mod_ota = {
  .ic_super      = &epggrab_mod_class,
  .ic_class      = "epggrab_mod_ota",
  .ic_caption    = N_("Over-the-air EPG grabber"),
  .ic_properties = (const property_t[]){
    {}
  }
};

/* **************************************************************************
 * Generic module routines
 * *************************************************************************/

epggrab_module_t *epggrab_module_create
  ( epggrab_module_t *skel, const idclass_t *cls,
    const char *id, const char *saveid,
    const char *name, int priority )
{
  assert(skel);

  /* Setup */
  skel->id       = strdup(id);
  skel->saveid   = strdup(saveid ?: id);
  skel->name     = strdup(name);
  skel->priority = priority;
  RB_INIT(&skel->channels);

  /* Insert */
  assert(!epggrab_module_find_by_id(id));
  LIST_INSERT_HEAD(&epggrab_modules, skel, link);
  tvhlog(LOG_INFO, "epggrab", "module %s created", id);

  idnode_insert(&skel->idnode, NULL, cls, 0);

  return skel;
}

/*
 * Run the parse
 */
void epggrab_module_parse( void *m, htsmsg_t *data )
{
  int64_t tm1, tm2;
  int save = 0;
  epggrab_stats_t stats;
  epggrab_module_int_t *mod = m;

  /* Parse */
  memset(&stats, 0, sizeof(stats));
  tm1 = getfastmonoclock();
  save |= mod->parse(mod, data, &stats);
  tm2 = getfastmonoclock();
  htsmsg_destroy(data);

  /* Debug stats */
  tvhlog(LOG_INFO, mod->id, "parse took %"PRId64" seconds", mono2sec(tm2 - tm1));
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

void epggrab_module_channels_load ( const char *modid )
{
  epggrab_module_t *mod = NULL;
  htsmsg_t *m, *e;
  htsmsg_field_t *f;
  const char *id;
  if (!modid) return;
  if ((m = hts_settings_load_r(1, "epggrab/%s/channels", modid))) {
    HTSMSG_FOREACH(f, m) {
      if ((e = htsmsg_get_map_by_field(f))) {
        id = htsmsg_get_str(e, "modid") ?: modid;
        if (mod == NULL ||  strcmp(mod->id, id))
          mod = epggrab_module_find_by_id(id);
        if (mod)
          epggrab_channel_create(mod, e, f->hmf_name);
      }
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
  mod->active = 0;
  free((char *)mod->path);
  mod->path = NULL;
  free((char *)mod->args);
  mod->args = NULL;
}

epggrab_module_int_t *epggrab_module_int_create
  ( epggrab_module_int_t *skel, const idclass_t *cls,
    const char *id, const char *saveid,
    const char *name, int priority,
    const char *path,
    char* (*grab) (void*m),
    int (*parse) (void *m, htsmsg_t *data, epggrab_stats_t *sta),
    htsmsg_t* (*trans) (void *mod, char *data) )
{
  /* Allocate data */
  if (!skel) skel = calloc(1, sizeof(epggrab_module_int_t));

  /* Pass through */
  epggrab_module_create((epggrab_module_t*)skel,
                        cls ?: &epggrab_class_mod_int,
                        id, saveid, name, priority);

  /* Int data */
  skel->type     = EPGGRAB_INT;
  skel->path     = strdup(path);
  skel->args     = NULL;
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
  char      **argv = NULL;
  char       *path;

  /* Debug */
  tvhlog(LOG_INFO, mod->id, "grab %s", mod->path);

  /* Extra arguments */
  if (mod->args && mod->args[0]) {
    path = alloca(strlen(mod->path) + strlen(mod->args) + 2);
    strcpy(path, mod->path);
    strcat(path, " ");
    strcat(path, mod->args);
  } else {
    path = (char *)mod->path;
  }

  /* Arguments */
  if (spawn_parse_args(&argv, 64, path, NULL)) {
    tvhlog(LOG_ERR, mod->id, "unable to parse arguments");
    return NULL;
  }

  /* Grab */
  outlen = spawn_and_give_stdout(argv[0], argv, NULL, &rd, NULL, 1);

  spawn_free_args(argv);

  if (outlen < 0)
    goto error;

  outlen = file_readall(rd, &outbuf);
  if (outlen < 1)
    goto error;

  close(rd);

  return outbuf;

error:
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
  mod->active = 0;
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
 * Activate socket module
 */
static int
epggrab_module_activate_socket ( void *m, int a )
{
  pthread_attr_t tattr;
  struct sockaddr_un addr;
  epggrab_module_ext_t *mod = (epggrab_module_ext_t*)m;
  const char *path;
  assert(mod->type == EPGGRAB_EXT);

  /* Ignore */
  if ( mod->active == a ) return 0;

  /* Disable */
  if (!a) {
    path = strdup(mod->path);
    epggrab_module_done_socket(m);
    mod->path = path;
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
    mod->active = 1;
    tvhthread_create(&mod->tid, &tattr, _epggrab_socket_thread, mod, "epggrabso");
  }
  return 1;
}

/*
 * Create a module
 */
epggrab_module_ext_t *epggrab_module_ext_create
  ( epggrab_module_ext_t *skel,
    const char *id, const char *saveid,
    const char *name, int priority, const char *sockid,
    int (*parse) (void *m, htsmsg_t *data, epggrab_stats_t *sta),
    htsmsg_t* (*trans) (void *mod, char *data) )
{
  char path[512];

  /* Allocate data */
  if (!skel) skel = calloc(1, sizeof(epggrab_module_ext_t));

  /* Pass through */
  hts_settings_buildpath(path, sizeof(path), "epggrab/%s.sock", sockid);
  epggrab_module_int_create((epggrab_module_int_t*)skel,
                            &epggrab_class_mod_ext,
                            id, saveid, name, priority, path,
                            NULL, parse, trans);

  /* Local */
  skel->type     = EPGGRAB_EXT;
  skel->activate = epggrab_module_activate_socket;
  skel->done     = epggrab_module_done_socket;

  return skel;
}

/* **************************************************************************
 * OTA module functions
 * *************************************************************************/

epggrab_module_ota_t *epggrab_module_ota_create
  ( epggrab_module_ota_t *skel,
    const char *id, const char *saveid,
    const char *name, int priority,
    epggrab_ota_module_ops_t *ops )
{
  if (!skel) skel = calloc(1, sizeof(epggrab_module_ota_t));

  /* Pass through */
  epggrab_module_create((epggrab_module_t*)skel,
                        &epggrab_class_mod_ota,
                        id, saveid, name, priority);

  /* Setup */
  skel->type     = EPGGRAB_OTA;
  skel->activate = ops->activate;
  skel->start    = ops->start;
  skel->done     = ops->done;
  skel->tune     = ops->tune;

  return skel;
}
