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

static int
api_epggrab_channel_list
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *m;
  pthread_mutex_lock(&global_lock);
  m = epggrab_channel_list(0);
  pthread_mutex_unlock(&global_lock);
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", m);
  return 0;
}

void api_epggrab_init ( void )
{
  static api_hook_t ah[] = {
    { "epggrab/channel/list", ACCESS_ANONYMOUS,
      api_epggrab_channel_list, NULL },
    { NULL },
  };

  api_register_all(ah);
}
