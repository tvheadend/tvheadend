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

static void
api_channel_key_val(htsmsg_t *dst, const char *key, const char *val)
{
  htsmsg_t *e = htsmsg_create_map();
  htsmsg_add_str(e, "key", key);
  htsmsg_add_str(e, "val", val ?: "");
  htsmsg_add_msg(dst, NULL, e);
}

// TODO: this will need converting to an idnode system
static int
api_channel_list
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  channel_t *ch;
  htsmsg_t *l;

  l = htsmsg_create_list();
  pthread_mutex_lock(&global_lock);
  CHANNEL_FOREACH(ch) {
    if (!channel_access(ch, perm, 0)) continue;
    api_channel_key_val(l, idnode_uuid_as_str(&ch->ch_id), channel_get_name(ch));
  }
  pthread_mutex_unlock(&global_lock);
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  
  return 0;
}

static void
api_channel_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf )
{
  channel_t *ch;

  CHANNEL_FOREACH(ch)
    if (channel_access(ch, perm, !access_verify2(perm, ACCESS_ADMIN)))
      idnode_set_add(ins, (idnode_t*)ch, &conf->filter);
}

static int
api_channel_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf;
  channel_t *ch;

  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;

  pthread_mutex_lock(&global_lock);
  ch = channel_create(NULL, conf, NULL);
  if (ch)
    channel_save(ch);
  pthread_mutex_unlock(&global_lock);

  return 0;
}

static int
api_channel_tag_list
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  channel_tag_t *ct;
  htsmsg_t *l;
  
  l = htsmsg_create_list();
  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    if (channel_tag_access(ct, perm, 0))
      api_channel_key_val(l, idnode_uuid_as_str(&ct->ct_id), ct->ct_name);
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  return 0;
}

static void
api_channel_tag_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf )
{
  channel_tag_t *ct;

  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    if (channel_tag_access(ct, perm, !access_verify2(perm, ACCESS_ADMIN)))
      idnode_set_add(ins, (idnode_t*)ct, &conf->filter);
}

static int
api_channel_tag_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf;
  channel_tag_t *ct;

  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;

  pthread_mutex_lock(&global_lock);
  ct = channel_tag_create(NULL, conf);
  if (ct)
    channel_tag_save(ct);
  pthread_mutex_unlock(&global_lock);

  return 0;
}

void api_channel_init ( void )
{
  static api_hook_t ah[] = {
    { "channel/class",   ACCESS_ANONYMOUS, api_idnode_class, (void*)&channel_class },
    { "channel/grid",    ACCESS_ANONYMOUS, api_idnode_grid,  api_channel_grid },
    { "channel/list",    ACCESS_ANONYMOUS, api_channel_list, NULL },
    { "channel/create",  ACCESS_ADMIN,     api_channel_create, NULL },

    { "channeltag/class",ACCESS_ANONYMOUS, api_idnode_class, (void*)&channel_tag_class },
    { "channeltag/grid", ACCESS_ANONYMOUS, api_idnode_grid,  api_channel_tag_grid },
    { "channeltag/list", ACCESS_ANONYMOUS, api_channel_tag_list, NULL },
    { "channeltag/create",  ACCESS_ADMIN,  api_channel_tag_create, NULL },

    { NULL },
  };

  api_register_all(ah);
}


#endif /* __TVH_API_CHANNEL_H__ */
