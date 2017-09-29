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

static inline int
is_paired( epggrab_channel_t *ec )
{
  return ec->only_one && LIST_FIRST(&ec->channels);
}

static inline int
epggrab_channel_check ( epggrab_channel_t *ec, channel_t *ch )
{
  if (!ec || !ch || !ch->ch_epgauto || !ch->ch_enabled)
    return 0;
  if (ch->ch_epg_parent || !ec->enabled)
    return 0;
  if (is_paired(ec))
    return 0; // ignore already paired
  return 1;
}

/* Check if channels match by epgid */
int epggrab_channel_match_epgid ( epggrab_channel_t *ec, channel_t *ch )
{
  const char *chid;

  if (ec->id == NULL)
    return 0;
  if (!epggrab_channel_check(ec, ch))
    return 0;
  chid = channel_get_epgid(ch);
  if (chid && !strcmp(ec->id, chid))
    return 1;
  return 0;
}

/* Check if channels match by name */
int epggrab_channel_match_name ( epggrab_channel_t *ec, channel_t *ch )
{
  const char *name, *s;
  htsmsg_field_t *f;

  if (!epggrab_channel_check(ec, ch))
    return 0;

  name = channel_get_name(ch, NULL);
  if (name == NULL)
    return 0;

  if (ec->name && !strcasecmp(ec->name, name))
    return 1;

  if (ec->names)
    HTSMSG_FOREACH(f, ec->names)
      if ((s = htsmsg_field_get_str(f)) != NULL)
        if (!strcasecmp(s, name))
          return 1;

  return 0;
}

/* Check if channels match by number */
int epggrab_channel_match_number ( epggrab_channel_t *ec, channel_t *ch )
{
  if (ec->lcn == 0)
    return 0;

  if (!epggrab_channel_check(ec, ch))
    return 0;

  if (ec->lcn && ec->lcn == channel_get_number(ch)) return 1;
  return 0;
}

/* Delete ilm */
static void
_epgggrab_channel_link_delete(idnode_list_mapping_t *ilm, int delconf)
{
  epggrab_channel_t *ec = (epggrab_channel_t *)ilm->ilm_in1;
  channel_t *ch = (channel_t *)ilm->ilm_in2;
  tvhdebug(ec->mod->subsys, "%s: unlinking %s from %s",
           ec->mod->id, ec->id, channel_get_name(ch, channel_blank_name));
  idnode_list_unlink(ilm, delconf ? ec : NULL);
}

/* Destroy */
void
epggrab_channel_link_delete
  ( epggrab_channel_t *ec, channel_t *ch, int delconf )
{
  idnode_list_mapping_t *ilm;
  LIST_FOREACH(ilm, &ec->channels, ilm_in1_link)
    if (ilm->ilm_in1 == &ec->idnode && ilm->ilm_in2 == &ch->ch_id) {
      _epgggrab_channel_link_delete(ilm, delconf);
      return;
    }
}

/* Destroy all links */
static void epggrab_channel_links_delete( epggrab_channel_t *ec, int delconf )
{
  idnode_list_mapping_t *ilm;
  while ((ilm = LIST_FIRST(&ec->channels)))
    _epgggrab_channel_link_delete(ilm, delconf);
}

/* Do update */
static void
epggrab_channel_sync( epggrab_channel_t *ec, channel_t *ch )
{
  int save = 0;

  if (ec->update_chname && ec->name && epggrab_conf.channel_rename)
    save |= channel_set_name(ch, ec->name);
  if (ec->update_chnum && ec->lcn > 0 && epggrab_conf.channel_renumber)
    save |= channel_set_number(ch, ec->lcn / CHANNEL_SPLIT, ec->lcn % CHANNEL_SPLIT);
  if (ec->update_chicon && ec->icon && epggrab_conf.channel_reicon)
    save |= channel_set_icon(ch, ec->icon);
  if (save)
    idnode_changed(&ch->ch_id);
}

/* Link epggrab channel to real channel */
int
epggrab_channel_link ( epggrab_channel_t *ec, channel_t *ch, void *origin )
{
  idnode_list_mapping_t *ilm;

  /* No change */
  if (!ch || !ch->ch_enabled) return 0;

  /* Already linked */
  LIST_FOREACH(ilm, &ec->channels, ilm_in1_link)
    if (ilm->ilm_in2 == &ch->ch_id) {
      ilm->ilm_mark = 0;
      epggrab_channel_sync(ec, ch);
      return 0;
    }

  /* New link */
  tvhdebug(ec->mod->subsys, "%s: linking %s to %s",
           ec->mod->id, ec->id, channel_get_name(ch, channel_blank_name));

  ilm = idnode_list_link(&ec->idnode, &ec->channels,
                         &ch->ch_id, &ch->ch_epggrab,
                         origin, 1);
  if (ilm == NULL)
    return 0;

  epggrab_channel_sync(ec, ch);

  if (origin == NULL)
    idnode_changed(&ec->idnode);
  return 1;
}

int
epggrab_channel_map ( idnode_t *ec, idnode_t *ch, void *origin )
{
  return epggrab_channel_link((epggrab_channel_t *)ec, (channel_t *)ch, origin);
}

/* Set name */
int epggrab_channel_set_name ( epggrab_channel_t *ec, const char *name )
{
  idnode_list_mapping_t *ilm;
  channel_t *ch;
  int save = 0;
  if (!ec || !name) return 0;
  if (!ec->newnames && (!ec->name || strcmp(ec->name, name))) {
    if (ec->name) free(ec->name);
    ec->name = strdup(name);
    if (epggrab_conf.channel_rename) {
      LIST_FOREACH(ilm, &ec->channels, ilm_in1_link) {
        ch = (channel_t *)ilm->ilm_in2;
        if (channel_set_name(ch, name))
          idnode_changed(&ch->ch_id);
      }
    }
    save = 1;
  }
  if (ec->newnames == NULL)
    ec->newnames = htsmsg_create_list();
  htsmsg_add_str(ec->newnames, NULL, name);
  ec->updated |= save;
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
      LIST_FOREACH(ilm, &ec->channels, ilm_in1_link) {
        ch = (channel_t *)ilm->ilm_in2;
        if (channel_set_icon(ch, icon))
          idnode_changed(&ch->ch_id);
      }
    }
    save = 1;
  }
  ec->updated |= save;
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
      LIST_FOREACH(ilm, &ec->channels, ilm_in1_link) {
        ch = (channel_t *)ilm->ilm_in2;
        if (channel_set_number(ch,
                               lcn / CHANNEL_SPLIT,
                               lcn % CHANNEL_SPLIT))
          idnode_changed(&ch->ch_id);
      }
    }
    save = 1;
  }
  ec->updated |= save;
  return save;
}

/* Autolink EPG channel to channel */
static int
epggrab_channel_autolink_one( epggrab_channel_t *ec, channel_t *ch )
{
  if (epggrab_channel_match_epgid(ec, ch))
    return epggrab_channel_link(ec, ch, NULL);
  else if (epggrab_channel_match_name(ec, ch))
    return epggrab_channel_link(ec, ch, NULL);
  else if (epggrab_channel_match_number(ec, ch))
    return epggrab_channel_link(ec, ch, NULL);
  return 0;
}

/* Autolink EPG channel to channel */
static int
epggrab_channel_autolink( epggrab_channel_t *ec )
{
  channel_t *ch;

  CHANNEL_FOREACH(ch)
    if (epggrab_channel_match_epgid(ec, ch))
      if (epggrab_channel_link(ec, ch, NULL))
        return 1;
  CHANNEL_FOREACH(ch)
    if (epggrab_channel_match_name(ec, ch))
      if (epggrab_channel_link(ec, ch, NULL))
        return 1;
  CHANNEL_FOREACH(ch)
    if (epggrab_channel_match_number(ec, ch))
      if (epggrab_channel_link(ec, ch, NULL))
        return 1;
  return 0;
}

/* Channel settings updated */
void epggrab_channel_updated ( epggrab_channel_t *ec )
{
  if (!ec) return;

  /* Find a link */
  if (!is_paired(ec))
    epggrab_channel_autolink(ec);

  /* Save */
  idnode_changed(&ec->idnode);
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

  if (htsmsg_get_str(conf, "id") == NULL)
    return NULL;

  ec = calloc(1, sizeof(*ec));
  if (idnode_insert(&ec->idnode, uuid, &epggrab_channel_class, 0)) {
    if (uuid)
      tvherror(LS_EPGGRAB, "invalid uuid '%s'", uuid);
    free(ec);
    return NULL;
  }

  ec->mod = owner;
  ec->enabled = 1;
  ec->update_chicon = 1;
  ec->update_chnum = 1;
  ec->update_chname = 1;

  if (conf)
    idnode_load(&ec->idnode, conf);

  TAILQ_INSERT_TAIL(&epggrab_channel_entries, ec, all_link);
  if (RB_INSERT_SORTED(&owner->channels, ec, link, _ch_id_cmp)) {
    tvherror(LS_EPGGRAB, "removing duplicate channel id '%s' (uuid '%s')", ec->id, uuid);
    epggrab_channel_destroy(ec, 1, 0);
    return NULL;
  }

  return ec;
}

/* Find/Create channel in the list */
epggrab_channel_t *epggrab_channel_find
  ( epggrab_module_t *mod, const char *id, int create, int *save )
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
    ec = RB_FIND(&mod->channels, epggrab_channel_skel, link, _ch_id_cmp);

  /* Find/Create */
  } else {
    ec = RB_INSERT_SORTED(&mod->channels, epggrab_channel_skel, link, _ch_id_cmp);
    if (!ec) {
      ec       = epggrab_channel_skel;
      SKEL_USED(epggrab_channel_skel);
      ec->enabled = 1;
      ec->id   = strdup(ec->id);
      ec->mod  = mod;
      TAILQ_INSERT_TAIL(&epggrab_channel_entries, ec, all_link);

      if (idnode_insert(&ec->idnode, NULL, &epggrab_channel_class, 0))
        abort();
      *save    = 1;
      return ec;
    }
  }
  return ec;
}

void epggrab_channel_destroy( epggrab_channel_t *ec, int delconf, int rb_remove )
{
  char ubuf[UUID_HEX_SIZE];

  if (ec == NULL) return;

  idnode_save_check(&ec->idnode, delconf);

  /* Already linked */
  epggrab_channel_links_delete(ec, 1);
  if (rb_remove)
    RB_REMOVE(&ec->mod->channels, ec, link);
  TAILQ_REMOVE(&epggrab_channel_entries, ec, all_link);
  idnode_unlink(&ec->idnode);

  if (delconf)
    hts_settings_remove("epggrab/%s/channels/%s",
                        ec->mod->saveid, idnode_uuid_as_str(&ec->idnode, ubuf));

  htsmsg_destroy(ec->newnames);
  htsmsg_destroy(ec->names);
  free(ec->comment);
  free(ec->name);
  free(ec->icon);
  free(ec->id);
  free(ec);
}

void epggrab_channel_flush
  ( epggrab_module_t *mod, int delconf )
{
  epggrab_channel_t *ec;
  while ((ec = RB_FIRST(&mod->channels)) != NULL)
    epggrab_channel_destroy(ec, delconf, 1);
}

void epggrab_channel_begin_scan ( epggrab_module_t *mod )
{
  epggrab_channel_t *ec;
  lock_assert(&global_lock);
  RB_FOREACH(ec, &mod->channels, link) {
    ec->updated = 0;
    if (ec->newnames) {
      htsmsg_destroy(ec->newnames);
      ec->newnames = NULL;
    }
  }
}

void epggrab_channel_end_scan ( epggrab_module_t *mod )
{
  epggrab_channel_t *ec;
  lock_assert(&global_lock);
  RB_FOREACH(ec, &mod->channels, link) {
    if (ec->newnames) {
      if (htsmsg_cmp(ec->names, ec->newnames)) {
        htsmsg_destroy(ec->names);
        ec->names = ec->newnames;
        ec->updated = 1;
      } else {
        htsmsg_destroy(ec->newnames);
      }
      ec->newnames = NULL;
    }
    if (ec->updated) {
      epggrab_channel_updated(ec);
      ec->updated = 0;
    }
  }
}

/* **************************************************************************
 * Global routines
 * *************************************************************************/

void epggrab_channel_add ( channel_t *ch )
{
  epggrab_module_t *mod;
  epggrab_channel_t *ec;

  LIST_FOREACH(mod, &epggrab_modules, link)
    RB_FOREACH(ec, &mod->channels, link) {
      if (!is_paired(ec))
        epggrab_channel_autolink_one(ec, ch);
    }
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
    if ((mod = epggrab_module_find_by_id(mid)) != NULL)
      return epggrab_channel_find(mod, cid, 0, NULL);
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
           ec->name ?: ec->id, ec->id, ec->mod->name);
  return prop_sbuf;
}

static htsmsg_t *
epggrab_channel_class_save(idnode_t *self, char *filename, size_t fsize)
{
  epggrab_channel_t *ec = (epggrab_channel_t *)self;
  htsmsg_t *m = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];
  idnode_save(&ec->idnode, m);
  if (filename)
    snprintf(filename, fsize, "epggrab/%s/channels/%s",
             ec->mod->saveid, idnode_uuid_as_str(&ec->idnode, ubuf));
  return m;
}

static void
epggrab_channel_class_delete(idnode_t *self)
{
  epggrab_channel_destroy((epggrab_channel_t *)self, 1, 1);
}

static const void *
epggrab_channel_class_modid_get ( void *obj )
{
  epggrab_channel_t *ec = obj;
  snprintf(prop_sbuf, PROP_SBUF_LEN, "%s", ec->mod->id ?: "");
  return &prop_sbuf_ptr;
}

static int
epggrab_channel_class_modid_set ( void *obj, const void *p )
{
  return 0;
}

static const void *
epggrab_channel_class_module_get ( void *obj )
{
  epggrab_channel_t *ec = obj;
  snprintf(prop_sbuf, PROP_SBUF_LEN, "%s", ec->mod->name ?: "");
  return &prop_sbuf_ptr;
}

static const void *
epggrab_channel_class_path_get ( void *obj )
{
  epggrab_channel_t *ec = obj;
  if (ec->mod->type == EPGGRAB_INT || ec->mod->type == EPGGRAB_EXT)
    snprintf(prop_sbuf, PROP_SBUF_LEN, "%s", ((epggrab_module_int_t *)ec->mod)->path ?: "");
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
}

static const void *
epggrab_channel_class_names_get ( void *obj )
{
  epggrab_channel_t *ec = obj;
  char *s = ec->names ? htsmsg_list_2_csv(ec->names, ',', 0) : NULL;
  snprintf(prop_sbuf, PROP_SBUF_LEN, "%s", s ?: "");
  free(s);
  return &prop_sbuf_ptr;
}

static int
epggrab_channel_class_names_set ( void *obj, const void *p )
{
  htsmsg_t *m = htsmsg_csv_2_list(p, ',');
  epggrab_channel_t *ec = obj;
  if (htsmsg_cmp(ec->names, m)) {
    htsmsg_destroy(ec->names);
    ec->names = m;
  } else {
    htsmsg_destroy(m);
  }
  return 0;
}

static void
epggrab_channel_class_enabled_notify ( void *obj, const char *lang )
{
  epggrab_channel_t *ec = obj;
  if (!ec->enabled) {
    epggrab_channel_links_delete(ec, 1);
  } else {
    epggrab_channel_updated(ec);
  }
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

static idnode_slist_t epggrab_channel_class_update_slist[] = {
  {
    .id   = "update_icon",
    .name = N_("Icon"),
    .off  = offsetof(epggrab_channel_t, update_chicon),
  },
  {
    .id   = "update_chnum",
    .name = N_("Number"),
    .off  = offsetof(epggrab_channel_t, update_chnum),
  },
  {
    .id   = "update_chname",
    .name = N_("Name"),
    .off  = offsetof(epggrab_channel_t, update_chname),
  },
  {}
};

static htsmsg_t *
epggrab_channel_class_update_enum ( void *obj, const char *lang )
{
  return idnode_slist_enum(obj, epggrab_channel_class_update_slist, lang);
}

static const void *
epggrab_channel_class_update_get ( void *obj )
{
  return idnode_slist_get(obj, epggrab_channel_class_update_slist);
}

static char *
epggrab_channel_class_update_rend ( void *obj, const char *lang )
{
  return idnode_slist_rend(obj, epggrab_channel_class_update_slist, lang);
}

static int
epggrab_channel_class_update_set ( void *obj, const void *p )
{
  return idnode_slist_set(obj, epggrab_channel_class_update_slist, p);
}

static void
epggrab_channel_class_update_notify ( void *obj, const char *lang )
{
  epggrab_channel_t *ec = obj;
  channel_t *ch;
  idnode_list_mapping_t *ilm;

  if (!ec->update_chicon && !ec->update_chnum && !ec->update_chname)
    return;
  LIST_FOREACH(ilm, &ec->channels, ilm_in1_link) {
    ch = (channel_t *)ilm->ilm_in2;
    epggrab_channel_sync(ec, ch);
  }
}

static void
epggrab_channel_class_only_one_notify ( void *obj, const char *lang )
{
  epggrab_channel_t *ec = obj;
  channel_t *ch, *first = NULL;
  idnode_list_mapping_t *ilm1, *ilm2;
  if (ec->only_one) {
    for(ilm1 = LIST_FIRST(&ec->channels); ilm1; ilm1 = ilm2) {
      ilm2 = LIST_NEXT(ilm1, ilm_in1_link);
      ch = (channel_t *)ilm1->ilm_in2;
      if (!first)
        first = ch;
      else if (ch->ch_epgauto && first)
        idnode_list_unlink(ilm1, ec);
    }
  } else {
    epggrab_channel_updated(ec);
  }
}

CLASS_DOC(epggrabber_channel)

const idclass_t epggrab_channel_class = {
  .ic_class      = "epggrab_channel",
  .ic_caption    = N_("EPG Grabber Channel"),
  .ic_doc        = tvh_doc_epggrabber_channel_class,
  .ic_event      = "epggrab_channel",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = epggrab_channel_class_save,
  .ic_get_title  = epggrab_channel_class_get_title,
  .ic_delete     = epggrab_channel_class_delete,
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = N_("Configuration"),
      .number = 1,
    },
    {}
  },
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/disable EPG data for the entry."),
      .off      = offsetof(epggrab_channel_t, enabled),
      .notify   = epggrab_channel_class_enabled_notify,
      .group    = 1
    },
    {
      .type     = PT_STR,
      .id       = "modid",
      .name     = N_("Module ID"),
      .desc     = N_("Module ID used to grab EPG data."),
      .get      = epggrab_channel_class_modid_get,
      .set      = epggrab_channel_class_modid_set,
      .opts     = PO_RDONLY | PO_HIDDEN,
      .group    = 1
    },
    {
      .type     = PT_STR,
      .id       = "module",
      .name     = N_("Module"),
      .desc     = N_("Name of the module used to grab EPG data."),
      .get      = epggrab_channel_class_module_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1
    },
    {
      .type     = PT_STR,
      .id       = "path",
      .name     = N_("Path"),
      .desc     = N_("Data path (if applicable)."),
      .get      = epggrab_channel_class_path_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1
    },
    {
      .type     = PT_TIME,
      .id       = "updated",
      .name     = N_("Updated"),
      .desc     = N_("Date the EPG data was last updated (not set for OTA "
                     "grabbers)."),
      .off      = offsetof(epggrab_channel_t, laststamp),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1
    },
    {
      .type     = PT_STR,
      .id       = "id",
      .name     = N_("ID"),
      .desc     = N_("EPG data ID."),
      .off      = offsetof(epggrab_channel_t, id),
      .group    = 1
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = N_("Name"),
      .desc     = N_("Service name found in EPG data."),
      .off      = offsetof(epggrab_channel_t, name),
      .group    = 1
    },
    {
      .type     = PT_STR,
      .id       = "names",
      .name     = N_("Names"),
      .desc     = N_("Additional service names found in EPG data."),
      .get      = epggrab_channel_class_names_get,
      .set      = epggrab_channel_class_names_set,
      .group    = 1
    },
    {
      .type     = PT_S64,
      .intextra = CHANNEL_SPLIT,
      .id       = "number",
      .name     = N_("Number"),
      .desc     = N_("Channel number as defined in EPG data."),
      .off      = offsetof(epggrab_channel_t, lcn),
      .group    = 1
    },
    {
      .type     = PT_STR,
      .id       = "icon",
      .name     = N_("Icon"),
      .desc     = N_("Channel icon as defined in EPG data."),
      .off      = offsetof(epggrab_channel_t, icon),
      .group    = 1
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "channels",
      .name     = N_("Channels"),
      .desc     = N_("Channels EPG data is used by."),
      .set      = epggrab_channel_class_channels_set,
      .get      = epggrab_channel_class_channels_get,
      .list     = channel_class_get_list,
      .rend     = epggrab_channel_class_channels_rend,
      .group    = 1
    },
    {
      .type     = PT_BOOL,
      .id       = "only_one",
      .name     = N_("Once per auto channel"),
      .desc     = N_("Only use this EPG data once when automatically "
                     "determining what EPG data to set for a channel."),
      .off      = offsetof(epggrab_channel_t, only_one),
      .notify   = epggrab_channel_class_only_one_notify,
      .group    = 1
    },
    {
      .type     = PT_INT,
      .islist   = 1,
      .id       = "update",
      .name     = N_("Channel update options"),
      .desc     = N_("Options used when updating channels."),
      .notify   = epggrab_channel_class_update_notify,
      .list     = epggrab_channel_class_update_enum,
      .get      = epggrab_channel_class_update_get,
      .set      = epggrab_channel_class_update_set,
      .rend     = epggrab_channel_class_update_rend,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-form text field, enter whatever you like."),
      .off      = offsetof(epggrab_channel_t, comment),
      .group    = 1
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
  idclass_register(&epggrab_channel_class);
}

void
epggrab_channel_done( void )
{
  assert(TAILQ_FIRST(&epggrab_channel_entries) == NULL);
  SKEL_FREE(epggrab_channel_skel);
}
