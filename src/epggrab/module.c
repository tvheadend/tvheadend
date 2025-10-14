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

#include <signal.h>
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

extern gtimer_t epggrab_save_timer;

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

static void epggrab_mod_class_title
  (idnode_t *self, const char *lang, char *dst, size_t dstsize)
{
  epggrab_module_t *mod = (epggrab_module_t *)self;
  const char *s1 = tvh_gettext_lang(lang, epggrab_module_type(mod));
  const char *s2 = tvh_str_default(mod->name, mod->id);
  snprintf(dst, dstsize, "%s: %s", s1, s2);
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

static htsmsg_t *
epggrab_module_ota_scrapper_config_list ( void *o, const char *lang )
{
  htsmsg_t *m = htsmsg_create_list();
  htsmsg_t *e = htsmsg_create_map();
  htsmsg_add_str(e, "key", "");
  htsmsg_add_str(e, "val", tvh_gettext_lang(lang, N_("Use default configuration")));
  htsmsg_add_msg(m, NULL, e);
  htsmsg_t *config;
  /* We load all the config so we can get the names of ones that are
   * valid. This is a bit of overhead but we are rarely called since
   * this is for the configuration GUI drop-down.
   */
  if((config = hts_settings_load_r(1, "epggrab/eit/scrape")) != NULL) {
    htsmsg_field_t *f;
    HTSMSG_FOREACH(f, config) {
      e = htsmsg_create_map();
      htsmsg_add_str(e, "key", htsmsg_field_name(f));
      htsmsg_add_str(e, "val", htsmsg_field_name(f));
      htsmsg_add_msg(m, NULL, e);
    }
    htsmsg_destroy(config);
  }
  return m;
}



CLASS_DOC(epggrabber_modules)
PROP_DOC(epggrabber_priority)

const idclass_t epggrab_mod_class = {
  .ic_class      = "epggrab_mod",
  .ic_caption    = N_("Channels / EPG - EPG Grabber Modules"),
  .ic_doc        = tvh_doc_epggrabber_modules_class,
  .ic_event      = "epggrab_mod",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_get_title  = epggrab_mod_class_title,
  .ic_changed    = epggrab_mod_class_changed,
  .ic_groups     = (const property_group_t[]) {
     {
        .name   = N_("Grabber Settings"),
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
                   "EPG grabber's data get used first. Priority is "
                   "given to the grabber with the highest value set here. "
                   "See Help for more info."),
      .doc    = prop_doc_epggrabber_priority,
      .off    = offsetof(epggrab_module_t, priority),
      .opts   = PO_EXPERT,
      .group  = 1
    },
    {}
  }
};

const idclass_t epggrab_mod_int_class = {
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

const idclass_t epggrab_mod_ext_class = {
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

const idclass_t epggrab_mod_ota_class = {
  .ic_super      = &epggrab_mod_class,
  .ic_class      = "epggrab_mod_ota",
  .ic_caption    = N_("EPG - Over-the-air EPG Grabber"),
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t epggrab_mod_ota_scraper_class = {
  .ic_super      = &epggrab_mod_ota_class,
  .ic_class      = "epggrab_mod_ota_scraper",
  .ic_caption    = N_("Over-the-air EPG grabber with scraping"),
  .ic_groups     = (const property_group_t[]) {
    {
      .name      = N_("EPG behaviour"),
      .number    = 1,
    },
    {
      .name      = N_("Scrape behaviour"),
      .number    = 2,
    },
    {}
  },
  .ic_properties = (const property_t[]){
    {
      /* The "eit" grabber is used by a number of countries so
       * we can't ship a config file named "eit" since regex use
       * in the UK won't be the same as in Italy.
       *
       * So, this option allows the user to specify the configuration
       * file to use from the ones that we do ship without them having
       * to mess around in the filesystem copying files.
       *
       * For example they can simply specify "uk" to use its
       * configuration file.
       */
      .type   = PT_STR,
      .id     = "scrape_config",
      .name   = N_("Scraper configuration to use"),
      .desc   = N_("Configuration containing regular expressions to use for "
                   "scraping additional information from the broadcast guide."
                   "This option does not access or retrieve details from the "
                   "Internet."
                   "This can be left blank to use the default or "
                   "set to one of the Tvheadend configurations from the "
                   "epggrab/eit/scrape directory such as "
                   "\"uk\" (without the quotes)."
                  ),
      .off    = offsetof(epggrab_module_ota_scraper_t, scrape_config),
      .list   = epggrab_module_ota_scrapper_config_list,
      .group  = 2,
    },
    {
      .type   = PT_BOOL,
      .id     = "scrape_episode",
      .name   = N_("Scrape Episode"),
      .desc   = N_("Enable/disable scraping episode details using the grabber."),
      .off    = offsetof(epggrab_module_ota_scraper_t, scrape_episode),
      .group  = 2,
    },
    {
      .type   = PT_BOOL,
      .id     = "scrape_title",
      .name   = N_("Scrape Title"),
      .desc   = N_("Enable/disable scraping title from the programme title and description. "
                   "Some broadcasters can split the title over the separate title, "
                   "and summary fields. This allows scraping of common split title formats "
                   "from within the broadcast title and summary field if supported by the "
                   "configuration file."
                   ),
      .off    = offsetof(epggrab_module_ota_scraper_t, scrape_title),
      .group  = 2,
    },
    {
      .type   = PT_BOOL,
      .id     = "scrape_subtitle",
      .name   = N_("Scrape Subtitle"),
      .desc   = N_("Enable/disable scraping subtitle from the programme description. "
                   "Some broadcasters do not send separate title, subtitle, description, "
                   "and summary fields. This allows scraping of common subtitle formats "
                   "from within the broadcast summary field if supported by the "
                   "configuration file."
                   ),
      .off    = offsetof(epggrab_module_ota_scraper_t, scrape_subtitle),
      .group  = 2,
    },
    {
      .type   = PT_BOOL,
      .id     = "scrape_summary",
      .name   = N_("Scrape Summary"),
      .desc   = N_("Enable/disable scraping summary from the programme description. "
                   "Some broadcasters do not send separate title, subtitle, description, "
                   "and summary fields. This allows scraping of a modified summary "
                   "from within the broadcast summary field if supported by the "
                   "configuration file."
                   ),
      .off    = offsetof(epggrab_module_ota_scraper_t, scrape_summary),
      .group  = 2,
    },
    {}
  }
};

/* **************************************************************************
 * Generic module routines
 * *************************************************************************/

epggrab_module_t *epggrab_module_create
  ( epggrab_module_t *skel, const idclass_t *cls,
    const char *id, int subsys, const char *saveid,
    const char *name, int priority )
{
  assert(skel);

  /* Setup */
  skel->id       = strdup(id);
  skel->subsys   = subsys;
  skel->saveid   = strdup(saveid ?: id);
  skel->name     = strdup(name);
  skel->priority = priority;
  RB_INIT(&skel->channels);
  TAILQ_INIT(&skel->data_queue);

  /* Insert */
  assert(!epggrab_module_find_by_id(id));
  LIST_INSERT_HEAD(&epggrab_modules, skel, link);
  tvhinfo(LS_EPGGRAB, "module %s created", id);

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
  tvhinfo(mod->subsys, "%s: parse took %"PRId64" seconds", mod->id, mono2sec(tm2 - tm1));
  tvhinfo(mod->subsys, "%s:  channels   tot=%5d new=%5d mod=%5d",
          mod->id, stats.channels.total, stats.channels.created,
          stats.channels.modified);
  tvhinfo(mod->subsys, "%s:  brands     tot=%5d new=%5d mod=%5d",
          mod->id, stats.brands.total, stats.brands.created,
          stats.brands.modified);
  tvhinfo(mod->subsys, "%s:  seasons    tot=%5d new=%5d mod=%5d",
          mod->id, stats.seasons.total, stats.seasons.created,
          stats.seasons.modified);
  tvhinfo(mod->subsys, "%s:  episodes   tot=%5d new=%5d mod=%5d",
          mod->id, stats.episodes.total, stats.episodes.created,
          stats.episodes.modified);
  tvhinfo(mod->subsys, "%s:  broadcasts tot=%5d new=%5d mod=%5d",
          mod->id, stats.broadcasts.total, stats.broadcasts.created,
          stats.broadcasts.modified);

  /* Now we've parsed, do we need to save? */
  if (save && epggrab_conf.epgdb_saveafterimport) {
    tvhinfo(mod->subsys, "%s: scheduling save epg timer", mod->id);
    tvh_mutex_lock(&global_lock);
    /* Disarm any existing timer first (from a periodic save). */
    gtimer_disarm(&epggrab_save_timer);
    /* Reschedule for a few minutes away so if the user is
     * refreshing from multiple xmltv sources we will give time for
     * them to all complete before persisting, rather than persisting
     * immediately after a parse.
     *
     * If periodic saving is enabled then the callback will then
     * rearm the timer for x hours after the previous save.
     */
    gtimer_arm_rel(&epggrab_save_timer, epg_save_callback, NULL,
                   60 * 2);
    tvh_mutex_unlock(&global_lock);
  }
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
        if (mod == NULL || strcmp(mod->id, id))
          mod = epggrab_module_find_by_id(id);
        if (mod)
          epggrab_channel_create(mod, e, htsmsg_field_name(f));
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
    const char *id, int subsys, const char *saveid,
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
                        cls ?: &epggrab_mod_int_class,
                        id, subsys, saveid, name, priority);

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
  tvhinfo(mod->subsys, "%s: grab %s", mod->id, mod->path);

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
    tvherror(mod->subsys, "%s: unable to parse arguments", mod->id);
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
  tvherror(mod->subsys, "%s: no output detected", mod->id);
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
    tvherror(mod->subsys, "%s: htsmsg_xml_deserialize error %s", mod->id, errbuf);
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
    tvhinfo(mod->subsys, "%s: grab took %"PRItime_t" seconds", mod->id, tm2 - tm1);
    epggrab_module_parse(mod, data);

  /* Failed */
  } else {
    tvherror(mod->subsys, "%s: failed to read data", mod->id);
  }
}


/*
 * External (socket) grab thread
 */
static void *_epggrab_socket_thread ( void *p )
{
  int s, s1;
  epggrab_module_ext_t *mod = (epggrab_module_ext_t*)p;
  tvhinfo(mod->subsys, "%s: external socket enabled", mod->id);

  while (mod->enabled && (s1 = atomic_get(&mod->sock)) >= 0) {
    tvhdebug(mod->subsys, "%s: waiting for connection", mod->id);
    s = accept(s1, NULL, NULL);
    if (s < 0) continue;
    tvhdebug(mod->subsys, "%s: got connection %d", mod->id, s);
    _epggrab_socket_handler(mod, s);
    close(s);
  }
  tvhdebug(mod->subsys, "%s: terminated", mod->id);
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
  sock = atomic_exchange(&mod->sock, -1);
  shutdown(sock, SHUT_RDWR);
  close(sock);
  if (mod->tid) {
    tvh_thread_kill(mod->tid, SIGQUIT);
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
  int sock;

  /* Ignore */
  if (mod->active == a) return 0;

  /* Disable */
  if (!a) {
    path = strdup(mod->path);
    epggrab_module_done_socket(m);
    mod->path = path;
  /* Enable */
  } else {
    unlink(mod->path); // just in case!
    hts_settings_makedirs(mod->path);

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    assert(sock >= 0);

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strlcpy(addr.sun_path, mod->path, sizeof(addr.sun_path));
    if (bind(sock, (struct sockaddr*)&addr,
             sizeof(struct sockaddr_un)) != 0) {
      tvherror(mod->subsys, "%s: failed to bind socket: %s", mod->id, strerror(errno));
      close(sock);
      return 0;
    }

    if (listen(sock, 5) != 0) {
      tvherror(mod->subsys, "%s: failed to listen on socket: %s", mod->id, strerror(errno));
      close(sock);
      return 0;
    }

    tvhdebug(mod->subsys, "%s: starting socket thread", mod->id);
    pthread_attr_init(&tattr);
    mod->active = 1;
    atomic_set(&mod->sock, sock);
    tvh_thread_create(&mod->tid, &tattr, _epggrab_socket_thread, mod, "epggrabso");
  }
  return 1;
}

/*
 * Create a module
 */
epggrab_module_ext_t *epggrab_module_ext_create
  ( epggrab_module_ext_t *skel, const idclass_t *cls,
    const char *id, int subsys, const char *saveid,
    const char *name, int priority, const char *sockid,
    int (*parse) (void *m, htsmsg_t *data, epggrab_stats_t *sta),
    htsmsg_t* (*trans) (void *mod, char *data) )
{
  char path[512];

  /* Allocate data */
  if (!skel) skel = calloc(1, sizeof(epggrab_module_ext_t));
  atomic_set(&skel->sock, -1);

  /* Pass through */
  if (hts_settings_buildpath(path, sizeof(path), "epggrab/%s.sock", sockid))
    path[0] = '\0';
  epggrab_module_int_create((epggrab_module_int_t*)skel,
                            cls ?: &epggrab_mod_ext_class,
                            id, subsys, saveid, name, priority, path,
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
    const char *id, int subsys, const char *saveid,
    const char *name, int priority, const idclass_t *idclass,
    const epggrab_ota_module_ops_t *ops )
{
  if (!skel) skel = calloc(1, sizeof(epggrab_module_ota_t));

  /* Pass through */
  epggrab_module_create((epggrab_module_t*)skel,
                        idclass ?: &epggrab_mod_ota_class,
                        id, subsys, saveid, name, priority);

  /* Setup */
  skel->type         = EPGGRAB_OTA;
  skel->activate     = ops->activate;
  skel->start        = ops->start;
  skel->stop         = ops->stop;
  skel->handlers     = ops->handlers;
  skel->done         = ops->done;
  skel->tune         = ops->tune;
  skel->process_data = ops->process_data;
  skel->opaque       = ops->opaque;

  return skel;
}
