/*
 *  API - channel related calls
 *
 *  Copyright (C) 2013 Adam Sutton
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

#ifndef __TVH_API_CHANNEL_H__
#define __TVH_API_CHANNEL_H__

#include "tvheadend.h"
#include "channels.h"
#include "access.h"
#include "api.h"
#include "string_list.h"

static int
api_channel_is_all(access_t *perm, htsmsg_t *args)
{
  return htsmsg_get_bool_or_default(args, "all", 0) &&
         !access_verify2(perm, ACCESS_ADMIN);
}

static int
api_channel_list
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  channel_t *ch, **chlist;
  htsmsg_t *l;
  const int cfg = api_channel_is_all(perm, args);
  const int numbers = htsmsg_get_s32_or_default(args, "numbers", 0);
  const int sources = htsmsg_get_s32_or_default(args, "sources", 0);
  const int flags = (numbers ? CHANNEL_ENAME_NUMBERS : 0) |
                    (sources ? CHANNEL_ENAME_SOURCES : 0);
  char buf[128], buf1[128], ubuf[UUID_HEX_SIZE];
  const char *name, *blank, *sort = htsmsg_get_str(args, "sort");
  int i, count;

  sort = htsmsg_get_str(args, "sort");
  if (numbers && !sort) sort = "numname";
  blank = tvh_gettext_lang(perm->aa_lang_ui, channel_blank_name);
  l = htsmsg_create_list();
  tvh_mutex_lock(&global_lock);
  chlist = channel_get_sorted_list(sort, cfg, &count);
  for (i = 0; i < count; i++) {
    ch = chlist[i];
    if (!cfg && !channel_access(ch, perm, 0)) continue;
    if (!ch->ch_enabled) {
      snprintf(buf, sizeof(buf), "{%s}",
               channel_get_ename(ch, buf1, sizeof(buf1), blank, flags));
      name = buf;
    } else {
      name = channel_get_ename(ch, buf1, sizeof(buf1), blank, flags);
    }
    htsmsg_add_msg(l, NULL, htsmsg_create_key_val(idnode_uuid_as_str(&ch->ch_id, ubuf), name));
  }
  tvh_mutex_unlock(&global_lock);
  free(chlist);
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  
  return 0;
}

static void
api_channel_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  channel_t *ch;
  int cfg = api_channel_is_all(perm, args);

  CHANNEL_FOREACH(ch)
    if (cfg || channel_access(ch, perm, 0))
      idnode_set_add(ins, (idnode_t*)ch, &conf->filter, perm->aa_lang_ui);
}

static int
api_channel_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf;
  channel_t *ch;

  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;

  tvh_mutex_lock(&global_lock);
  ch = channel_create(NULL, conf, NULL);
  if (ch)
    api_idnode_create(resp, &ch->ch_id);
  tvh_mutex_unlock(&global_lock);

  return 0;
}

static int
api_channel_rename
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  const char *from, *to;
  if (!(from = htsmsg_get_str(args, "from")))
    return EINVAL;
  if (!(to = htsmsg_get_str(args, "to")))
    return EINVAL;
  /* We need the lock since we are altering details */
  tvh_mutex_lock(&global_lock);
  const int num_match = channel_rename_and_save(from, to);
  tvh_mutex_unlock(&global_lock);

  return num_match > 0 ? 0 : ENOENT;
}

static int
api_channel_tag_list
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  channel_tag_t *ct;
  htsmsg_t *l;
  int cfg = api_channel_is_all(perm, args);
  char buf[128], ubuf[UUID_HEX_SIZE], *name;

  l = htsmsg_create_list();
  tvh_mutex_lock(&global_lock);
  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    if (cfg || channel_tag_access(ct, perm, 0)) {
      if (ct->ct_enabled) {
        name = ct->ct_name;
      } else {
        snprintf(buf, sizeof(buf), "{%s}", ct->ct_name);
        name = buf;
      }
      htsmsg_add_msg(l, NULL, htsmsg_create_key_val(idnode_uuid_as_str(&ct->ct_id, ubuf), name));
    }
  tvh_mutex_unlock(&global_lock);
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  return 0;
}

static void
api_channel_tag_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  channel_tag_t *ct;
  int cfg = api_channel_is_all(perm, args);

  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    if (cfg || channel_tag_access(ct, perm, 0))
      idnode_set_add(ins, (idnode_t*)ct, &conf->filter, perm->aa_lang_ui);
}

static int
api_channel_tag_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf;
  channel_tag_t *ct;

  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;

  tvh_mutex_lock(&global_lock);
  ct = channel_tag_create(NULL, conf);
  if (ct)
    api_idnode_create(resp, &ct->ct_id);
  tvh_mutex_unlock(&global_lock);

  return 0;
}

static int
api_channel_cat_list
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  channel_t *ch;
  int cfg = api_channel_is_all(perm, args);

  htsmsg_t *l = htsmsg_create_list();
  string_list_t *sl = string_list_create();
  const string_list_item_t *item;

  tvh_mutex_lock(&global_lock);
  /* Build string_list of all categories the user is allowed
   * to see.
   */
  CHANNEL_FOREACH(ch) {
    if (!cfg && !channel_access(ch, perm, 0)) continue;
    if (!ch->ch_enabled) continue;
    epg_broadcast_t *e;
    RB_FOREACH(e, &ch->ch_epg_schedule, sched_link) {
      if (e->category) {
        RB_FOREACH(item, e->category, h_link) {
          const char *id = item->id;
          /* Get rid of duplicates */
          string_list_insert(sl, id);
        }
      }
    }
  }
  tvh_mutex_unlock(&global_lock);

  /* Now we have the unique list, convert it for GUI. */
  RB_FOREACH(item, sl, h_link) {
    const char *id = item->id;
    htsmsg_add_msg(l, NULL, htsmsg_create_key_val(id, id));
  }

  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);

  string_list_destroy(sl);
  return 0;
}


void api_channel_init ( void )
{
  static api_hook_t ah[] = {
    { "channel/class",   ACCESS_ANONYMOUS, api_idnode_class, (void*)&channel_class },
    { "channel/grid",    ACCESS_ANONYMOUS, api_idnode_grid,  api_channel_grid },
    { "channel/list",    ACCESS_ANONYMOUS, api_channel_list, NULL },
    { "channel/create",  ACCESS_ADMIN,     api_channel_create, NULL },
    { "channel/rename",  ACCESS_ADMIN,     api_channel_rename, NULL }, /* User convenience function */

    { "channeltag/class",ACCESS_ANONYMOUS, api_idnode_class, (void*)&channel_tag_class },
    { "channeltag/grid", ACCESS_ANONYMOUS, api_idnode_grid,  api_channel_tag_grid },
    { "channeltag/list", ACCESS_ANONYMOUS, api_channel_tag_list, NULL },
    { "channeltag/create",  ACCESS_ADMIN,  api_channel_tag_create, NULL },

    { "channelcategory/list",  ACCESS_ANONYMOUS, api_channel_cat_list, NULL },
    { NULL },
  };

  api_register_all(ah);
}


#endif /* __TVH_API_CHANNEL_H__ */
