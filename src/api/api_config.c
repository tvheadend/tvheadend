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
#include "api.h"
#include "config.h"

static int
api_config_capabilities(access_t *perm, void *opaque, const char *op,
                        htsmsg_t *args, htsmsg_t **resp)
{
    *resp = tvheadend_capabilities_list(0);
    return 0;
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
    { NULL },
  };

  api_register_all(ah);
}
