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

#ifndef __TVH_API_SERVICE_H__
#define __TVH_API_SERVICE_H__

#include "tvheadend.h"
#include "channels.h"
#include "access.h"
#include "api.h"

// TODO: this will need converting to an idnode system
static int
api_channel_list
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  channel_t *ch;
  htsmsg_t *l, *e;

  l = htsmsg_create_list();
  pthread_mutex_lock(&global_lock);
  CHANNEL_FOREACH(ch) {
    e = htsmsg_create_map();
    htsmsg_add_str(e, "key", idnode_uuid_as_str(&ch->ch_id));
    htsmsg_add_str(e, "val", channel_get_name(ch));
    htsmsg_add_msg(l, NULL, e);
  }
  pthread_mutex_unlock(&global_lock);
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  
  return 0;
}

static void
api_channel_grid
  ( idnode_set_t *ins, api_idnode_grid_conf_t *conf )
{
  channel_t *ch;

  CHANNEL_FOREACH(ch)
    idnode_set_add(ins, (idnode_t*)ch, &conf->filter);
}

static int
api_channel_create
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
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
api_channeltag_enum
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  channel_tag_t *ct;
  htsmsg_t *l, *e;
  
  int _enum = htsmsg_get_bool_or_default(args, "enum", 0);

  if (_enum) {
    l = htsmsg_create_list();
    TAILQ_FOREACH(ct, &channel_tags, ct_link) {
      e = htsmsg_create_map();
      htsmsg_add_u32(e, "key", ct->ct_identifier);
      htsmsg_add_str(e, "val", ct->ct_name);
      htsmsg_add_msg(l, NULL, e);
    }
    *resp = htsmsg_create_map();
    htsmsg_add_msg(*resp, "entries", l);
  } else {
    // TODO: support full listing v enum
  }
  return 0;
}

void api_channel_init ( void )
{
  static api_hook_t ah[] = {
    { "channel/class",   ACCESS_ANONYMOUS, api_idnode_class, (void*)&channel_class },
    { "channel/grid",    ACCESS_ANONYMOUS, api_idnode_grid,  api_channel_grid },
    { "channel/list",    ACCESS_ANONYMOUS, api_channel_list, NULL },
    { "channel/create",  ACCESS_ADMIN,     api_channel_create, NULL },

    { "channeltag/list", ACCESS_ANONYMOUS, api_channeltag_enum, NULL },

    { NULL },
  };

  api_register_all(ah);
}


#endif /* __TVH_API_IDNODE_H__ */
