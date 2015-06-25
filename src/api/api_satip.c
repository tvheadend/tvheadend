/*
 *  API - SAT>IP Server related calls
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
#include "satip/server.h"

#if ENABLE_SATIP_SERVER

static int
api_satip_server_load
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *l;
  pthread_mutex_lock(&global_lock);
  *resp = htsmsg_create_map();
  l     = htsmsg_create_list();
  htsmsg_add_msg(l, NULL, satip_server_get_config());
  htsmsg_add_msg(*resp, "entries", l);
  pthread_mutex_unlock(&global_lock);
  return 0;
}

static int
api_satip_server_save
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  pthread_mutex_lock(&global_lock);
  if (satip_server_set_config(args))
    satip_server_save();
  pthread_mutex_unlock(&global_lock);
  *resp = htsmsg_create_map();
  htsmsg_add_u32(*resp, "success", 1);
  return 0;
}

void
api_satip_server_init ( void )
{
  static api_hook_t ah[] = {
    { "satips/config/load", ACCESS_ADMIN, api_satip_server_load, NULL },
    { "satips/config/save", ACCESS_ADMIN, api_satip_server_save, NULL },
    { NULL },
  };

  api_register_all(ah);
}

#else /* ENABLE_SATIP_SERVER */

void 
api_satip_server_init ( void )
{
}

#endif
