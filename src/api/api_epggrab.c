/*
 *  API - epggrab related calls
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

#include "tvheadend.h"
#include "access.h"
#include "api.h"
#include "epggrab.h"

static void
api_epggrab_channel_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  epggrab_channel_t *ec;

  TAILQ_FOREACH(ec, &epggrab_channel_entries, all_link)
    idnode_set_add(ins, (idnode_t*)ec, &conf->filter, perm->aa_lang_ui);
}

static int
api_epggrab_module_list
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *l = htsmsg_create_list(), *m;
  epggrab_module_t *mod;
  char ubuf[UUID_HEX_SIZE];
  pthread_mutex_lock(&global_lock);
  LIST_FOREACH(mod, &epggrab_modules, link) {
    m = htsmsg_create_map();
    htsmsg_add_str(m, "uuid", idnode_uuid_as_str(&mod->idnode, ubuf));
    htsmsg_add_str(m, "status", epggrab_module_get_status(mod));
    htsmsg_add_str(m, "title", idnode_get_title(&mod->idnode, perm->aa_lang_ui));
    htsmsg_add_msg(l, NULL, m);
  }
  pthread_mutex_unlock(&global_lock);
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  return 0;
}

static int
api_epggrab_ota_trigger
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int32_t s32;
  if (htsmsg_get_s32(args, "trigger", &s32))
    return EINVAL;
  if (s32 > 0) {
    pthread_mutex_lock(&global_lock);
    epggrab_ota_trigger(s32);
    pthread_mutex_unlock(&global_lock);
  }
  return 0;
}

void api_epggrab_init ( void )
{
  static api_hook_t ah[] = {
    { "epggrab/channel/list", ACCESS_ANONYMOUS, api_idnode_load_by_class, (void*)&epggrab_channel_class },
    { "epggrab/channel/class", ACCESS_ADMIN, api_idnode_class, (void*)&epggrab_channel_class },
    { "epggrab/channel/grid", ACCESS_ADMIN, api_idnode_grid, api_epggrab_channel_grid },

    { "epggrab/module/list",  ACCESS_ADMIN, api_epggrab_module_list, NULL },
    { "epggrab/config/load",  ACCESS_ADMIN, api_idnode_load_simple, &epggrab_conf.idnode },
    { "epggrab/config/save",  ACCESS_ADMIN, api_idnode_save_simple, &epggrab_conf.idnode },
    { "epggrab/ota/trigger",  ACCESS_ADMIN, api_epggrab_ota_trigger, NULL },
    { NULL },
  };

  api_register_all(ah);
}
