/*
 *  API - General configuration related calls
 *
 *  Copyright (C) 2015 Jaroslav Kysela
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; withm even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tvheadend.h"
#include "channels.h"
#include "access.h"
#include "memoryinfo.h"
#include "api.h"
#include "config.h"

static int
api_config_capabilities(access_t *perm, void *opaque, const char *op,
                        htsmsg_t *args, htsmsg_t **resp)
{
    *resp = tvheadend_capabilities_list(0);
    return 0;
}

static void
api_memoryinfo_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  memoryinfo_t *my;

  LIST_FOREACH(my, &memoryinfo_entries, my_link) {
    if (my->my_update)
      my->my_update(my);
    idnode_set_add(ins, (idnode_t*)my, &conf->filter, perm->aa_lang_ui);
  }
}

void
api_config_init ( void )
{
  static api_hook_t ah[] = {
    { "config/capabilities", ACCESS_OR|ACCESS_WEB_INTERFACE|ACCESS_HTSP_INTERFACE, api_config_capabilities, NULL },
    { "config/load",         ACCESS_ADMIN, api_idnode_load_simple, &config },
    { "config/save",         ACCESS_ADMIN, api_idnode_save_simple, &config },
    { "tvhlog/config/load",  ACCESS_ADMIN, api_idnode_load_simple, &tvhlog_conf },
    { "tvhlog/config/save",  ACCESS_ADMIN, api_idnode_save_simple, &tvhlog_conf },
    { "memoryinfo/class",    ACCESS_ADMIN, api_idnode_class, (void *)&memoryinfo_class },
    { "memoryinfo/grid",     ACCESS_ADMIN, api_idnode_grid, api_memoryinfo_grid },
    { NULL },
  };

  api_register_all(ah);
}
