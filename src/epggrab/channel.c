/*
 *  EPG Grabber - channel functions
 *  Copyright (C) 2012 Adam Sutton
 *  Copyright (C) 2015 Jaroslav Kysela
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

#include "tvheadend.h"
#include "settings.h"
#include "htsmsg.h"
#include "channels.h"
#include "epg.h"
#include "epggrab.h"
#include "epggrab/private.h"

#include <assert.h>
#include <string.h>

struct epggrab_channel_queue epggrab_channel_entries;

SKEL_DECLARE(epggrab_channel_skel, epggrab_channel_t);

/* **************************************************************************
 * EPG Grab Channel functions
 * *************************************************************************/

/* Check if channels match */
int epggrab_channel_match ( epggrab_channel_t *ec, channel_t *ch )
{
  if (!ec || !ch || !ch->ch_epgauto || !ch->ch_enabled || !ec->enabled) return 0;
  if (LIST_FIRST(&ec->channels)) return 0; // ignore already paired

  if (ec->name && !strcmp(ec->name, channel_get_epgid(ch))) return 1;
  if (ec->lcn && ec->lcn == channel_get_number(ch)) return 1;
  return 0;
}

/* Destroy */
void
epggrab_channel_link_delete
  ( epggrab_channel_t *ec, channel_t *ch, int delconf )
{
  idnode_list_mapping_t *ilm;
  LIST_FOREACH(ilm, &ec->channels, ilm_in2_link)
    if (ilm->ilm_in1 == &ec->idnode && ilm->ilm_in2 == &ch->ch_id)
      idnode_list_unlink(ilm, NULL);
}

/* Destroy all links */
static void epggrab_channel_links_delete( epggrab_channel_t *ec, int delconf )
{
  idnode_list_mapping_t *ilm;
  while ((ilm = LIST_FIRST(&ec->channels)))
    idnode_list_unlink(ilm, delconf ? ec : NULL);
}

/* Link epggrab channel to real channel */
int
epggrab_channel_link ( epggrab_channel_t *ec, channel_t *ch, void *origin )
{
  int save = 0;
  idnode_list_mapping_t *ilm;

  /* No change */
  if (!ch || !ch->ch_enabled) return 0;

  /* Already linked */
  LIST_FOREACH(ilm, &ec->channels, ilm_in2_link)
    if (ilm->ilm_in2 == &ch->ch_id)
      return 0;

  /* New link */
  tvhdebug(ec->mod->id, "linking %s to %s",
         ec->id, channel_get_name(ch));

  ilm = idnode_list_link(&ec->idnode, &ec->channels,
                         &ch->ch_id, &ch->ch_epggrab,
                         origin, 1);
  if (ilm == NULL)
    return 0;

  if (ec->name && epggrab_conf.channel_rename)
    save |= channel_set_name(ch, ec->name);
  if (ec->lcn > 0 && epggrab_conf.channel_renumber)
    save |= channel_set_number(ch, ec->lcn / CHANNEL_SPLIT, ec->lcn % CHANNEL_SPLIT);
  if (ec->icon && epggrab_conf.channel_reicon)
    save |= channel_set_icon(ch, ec->icon);
  if (save)
    channel_save(ch);

  if (origin == NULL)
    epggrab_channel_save(ec);
  return 1;
}

int
epggrab_channel_map ( idnode_t *ec, idnode_t *ch, void *origin )
{
  return epggrab_channel_link((epggrab_channel_t *)ec, (channel_t *)ch, origin);
}

/* Match and link (basically combines two funcs above for ease) */
int epggrab_channel_match_and_link ( epggrab_channel_t *ec, channel_t *ch )
{
  int r = epggrab_channel_match(ec, ch);
  if (r)
    epggrab_channel_link(ec, ch, NULL);
  return r;
}

/* Set name */
int epggrab_channel_set_name ( epggrab_channel_t *ec, const char *name )
{
  idnode_list_mapping_t *ilm;
  channel_t *ch;
  int save = 0;
  if (!ec || !name) return 0;
  if (!ec->name || strcmp(ec->name, name)) {
    if (ec->name) free(ec->name);
    ec->name = strdup(name);
    if (epggrab_conf.channel_rename) {
      LIST_FOREACH(ilm, &ec->channels, ilm_in2_link) {
        ch = (channel_t *)ilm->ilm_in2;
        if (channel_set_name(ch, name))
          channel_save(ch);
      }
    }
    save = 1;
  }
  return save;
}

/* Set icon */
int epggrab_channel_set_icon ( epggrab_channel_t *ec, const char *icon )
{
  idnode_list_mapping_t *ilm;
  channel_t *ch;
  int save = 0;
  if (!ec || !icon) return 0;
  if (!ec->icon || strcmp(ec->icon, icon) ) {
    if (ec->icon) free(ec->icon);
    ec->icon = strdup(icon);
    if (epggrab_conf.channel_reicon) {
      LIST_FOREACH(ilm, &ec->channels, ilm_in2_link) {
        ch = (channel_t *)ilm->ilm_in2;
        if (channel_set_icon(ch, icon))
          channel_save(ch);
      }
    }
    save = 1;
  }
  return save;
}

/* Set channel number */
int epggrab_channel_set_number ( epggrab_channel_t *ec, int major, int minor )
{
  idnode_list_mapping_t *ilm;
  channel_t *ch;
  int64_t lcn;
  int save = 0;
  if (!ec || (major <= 0 && minor <= 0)) return 0;
  lcn = (major * CHANNEL_SPLIT) + minor;
  if (ec->lcn != lcn) {
    ec->lcn = lcn;
    if (epggrab_conf.channel_renumber) {
      LIST_FOREACH(ilm, &ec->channels, ilm_in2_link) {
        ch = (channel_t *)ilm->ilm_in2;
        if (channel_set_number(ch,
                               lcn / CHANNEL_SPLIT,
                               lcn % CHANNEL_SPLIT))
          channel_save(ch);
      }
    }
    save = 1;
  }
  return save;
}

/* Channel settings updated */
void epggrab_channel_updated ( epggrab_channel_t *ec )
{
  channel_t *ch;
  if (!ec) return;

  /* Find a link */
  if (!LIST_FIRST(&ec->channels))
    CHANNEL_FOREACH(ch)
      if (epggrab_channel_match_and_link(ec, ch)) break;

  /* Save */
  epggrab_channel_save(ec);
}

/* ID comparison */
static int _ch_id_cmp ( void *a, void *b )
{
  return strcmp(((epggrab_channel_t*)a)->id,
                ((epggrab_channel_t*)b)->id);
}

/* Create new entry */
epggrab_channel_t *epggrab_channel_create
  ( epggrab_module_t *owner, htsmsg_t *conf, const char *uuid )
{
  epggrab_channel_t *ec;

  assert(owner->channels);

  if (htsmsg_get_str(conf, "id") == NULL)
    return NULL;

  ec = calloc(1, sizeof(*ec));
  if (idnode_insert(&ec->idnode, uuid, &epggrab_channel_class, 0)) {
    if (uuid)
      tvherror("epggrab", "invalid uuid '%s'", uuid);
    free(ec);
    return NULL;
  }

  ec->mod = owner;
  ec->enabled = 1;
  ec->tree = owner->channels;

  if (conf)
    idnode_load(&ec->idnode, conf);

  TAILQ_INSERT_TAIL(&epggrab_channel_entries, ec, all_link);
  if (RB_INSERT_SORTED(owner->channels, ec, link, _ch_id_cmp)) abort();

  return ec;
}

/* Find/Create channel in the list */
epggrab_channel_t *epggrab_channel_find
  ( epggrab_channel_tree_t *tree, const char *id, int create, int *save,
    epggrab_module_t *owner )
{
  char *s;
  epggrab_channel_t *ec;

  SKEL_ALLOC(epggrab_channel_skel);
  s = epggrab_channel_skel->id = tvh_strdupa(id);

  /* Replace / with # */
  // Note: this is a bit of a nasty fix for #1774, but will do for now
  while (*s) {
    if (*s == '/') *s = '#';
    s++;
  }

  /* Find */
  if (!create) {
    ec = RB_FIND(tree, epggrab_channel_skel, link, _ch_id_cmp);

  /* Find/Create */
  } else {
    ec = RB_INSERT_SORTED(tree, epggrab_channel_skel, link, _ch_id_cmp);
    if (!ec) {
      assert(owner);
      ec       = epggrab_channel_skel;
      SKEL_USED(epggrab_channel_skel);
      ec->enabled = 1;
      ec->tree = tree;
      ec->id   = strdup(ec->id);
      ec->mod  = owner;
      TAILQ_INSERT_TAIL(&epggrab_channel_entries, ec, all_link);

      if (idnode_insert(&ec->idnode, NULL, &epggrab_channel_class, 0))
        abort();
      *save    = 1;
      return ec;
    }
  }
  return ec;
}

void epggrab_channel_save( epggrab_channel_t *ec )
{
  htsmsg_t *m = htsmsg_create_map();
  idnode_save(&ec->idnode, m);
  hts_settings_save(m, "epggrab/%s/channels/%s",
                    ec->mod->id, idnode_uuid_as_sstr(&ec->idnode));
  htsmsg_destroy(m);
}

void epggrab_channel_destroy( epggrab_channel_t *ec, int delconf )
{
  if (ec == NULL) return;

  /* Already linked */
  epggrab_channel_links_delete(ec, 0);
  RB_REMOVE(ec->tree, ec, link);
  TAILQ_REMOVE(&epggrab_channel_entries, ec, all_link);
  idnode_unlink(&ec->idnode);

  if (delconf)
    hts_settings_remove("epggrab/%s/channels/%s",
                        ec->mod->id, idnode_uuid_as_sstr(&ec->idnode));

  free(ec->comment);
  free(ec->name);
  free(ec->icon);
  free(ec->id);
  free(ec);
}

void epggrab_channel_flush
  ( epggrab_channel_tree_t *tree, int delconf )
{
  epggrab_channel_t *ec;
  if (tree == NULL)
    return;
  while ((ec = RB_FIRST(tree)) != NULL) {
    assert(tree == ec->tree);
    epggrab_channel_destroy(ec, delconf);
  }
}

/* **************************************************************************
 * Global routines
 * *************************************************************************/

void epggrab_channel_add ( channel_t *ch )
{
  epggrab_module_t *mod;
  epggrab_channel_t *egc;

  LIST_FOREACH(mod, &epggrab_modules, link)
    if (mod->channels)
      RB_FOREACH(egc, mod->channels, link)
        if (epggrab_channel_match_and_link(egc, ch))
          break;
}

void epggrab_channel_rem ( channel_t *ch )
{
  idnode_list_mapping_t *ilm;

  while ((ilm = LIST_FIRST(&ch->ch_epggrab)) != NULL)
    idnode_list_unlink(ilm, ch);
}

void epggrab_channel_mod ( channel_t *ch )
{
  return epggrab_channel_add(ch);
}

const char *
epggrab_channel_get_id ( epggrab_channel_t *ec )
{
  static char buf[1024];
  epggrab_module_t *m = ec->mod;
  snprintf(buf, sizeof(buf), "%s|%s", m->id, ec->id);
  return buf;
}

epggrab_channel_t *
epggrab_channel_find_by_id ( const char *id )
{
  char buf[1024];
  char *mid, *cid;
  epggrab_module_t *mod;
  strncpy(buf, id, sizeof(buf));
  buf[sizeof(buf)-1] = '\0';
  if ((mid = strtok_r(buf, "|", &cid)) && cid)
    if ((mod = epggrab_module_find_by_id(mid)) && mod->channels)
      return epggrab_channel_find(mod->channels, cid, 0, NULL, NULL);
  return NULL;
}

int
epggrab_channel_is_ota ( epggrab_channel_t *ec )
{
  return ec->mod->type == EPGGRAB_OTA;
}

/*
 * Class
 */

static const char *
epggrab_channel_class_get_title(idnode_t *self, const char *lang)
{
  epggrab_channel_t *ec = (epggrab_channel_t*)self;

  snprintf(prop_sbuf, PROP_SBUF_LEN, "%s: %s (%s)",
           ec->mod->name, ec->name ?: ec->id, ec->id);
  return prop_sbuf;
}

static void
epggrab_channel_class_save(idnode_t *self)
{
  epggrab_channel_save((epggrab_channel_t *)self);
}

static void
epggrab_channel_class_delete(idnode_t *self)
{
  epggrab_channel_destroy((epggrab_channel_t *)self, 1);
}

static const void *
epggrab_channel_class_module_get ( void *obj )
{
  epggrab_channel_t *ec = obj;
  snprintf(prop_sbuf, PROP_SBUF_LEN, "%s", ec->mod->name ?: "");
  return &prop_sbuf_ptr;
}

static const void *
epggrab_channel_class_channels_get ( void *obj )
{
  epggrab_channel_t *ec = obj;
  return idnode_list_get1(&ec->channels);
}

static int
epggrab_channel_class_channels_set ( void *obj, const void *p )
{
  epggrab_channel_t *ec = obj;
  return idnode_list_set1(&ec->idnode, &ec->channels,
                          &channel_class, (htsmsg_t *)p,
                          epggrab_channel_map);
}

static char *
epggrab_channel_class_channels_rend ( void *obj, const char *lang )
{
  epggrab_channel_t *ec = obj;
  return idnode_list_get_csv1(&ec->channels, lang);
}


const idclass_t epggrab_channel_class = {
  .ic_class      = "epggrab_channel",
  .ic_caption    = N_("EPG grabber channel"),
  .ic_event      = "epggrab_channel",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = epggrab_channel_class_save,
  .ic_get_title  = epggrab_channel_class_get_title,
  .ic_delete     = epggrab_channel_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .off      = offsetof(epggrab_channel_t, enabled),
    },
    {
      .type     = PT_STR,
      .id       = "module",
      .name     = N_("Module"),
      .get      = epggrab_channel_class_module_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "id",
      .name     = N_("ID"),
      .off      = offsetof(epggrab_channel_t, id),
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = N_("Name"),
      .off      = offsetof(epggrab_channel_t, name),
    },
    {
      .type     = PT_S64,
      .intsplit = CHANNEL_SPLIT,
      .id       = "number",
      .name     = N_("Number"),
      .off      = offsetof(epggrab_channel_t, lcn),
    },
    {
      .type     = PT_STR,
      .id       = "icon",
      .name     = N_("Icon"),
      .off      = offsetof(epggrab_channel_t, icon),
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "channels",
      .name     = N_("Channels"),
      .set      = epggrab_channel_class_channels_set,
      .get      = epggrab_channel_class_channels_get,
      .list     = channel_class_get_list,
      .rend     = epggrab_channel_class_channels_rend,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .off      = offsetof(epggrab_channel_t, comment)
    },
    {}
  }
};

/*
 *
 */
void
epggrab_channel_init( void )
{
  TAILQ_INIT(&epggrab_channel_entries);
}

void
epggrab_channel_done( void )
{
  assert(TAILQ_FIRST(&epggrab_channel_entries) == NULL);
  SKEL_FREE(epggrab_channel_skel);
}
