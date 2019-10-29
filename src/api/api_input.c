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

#ifndef __TVH_API_INPUT_H__
#define __TVH_API_INPUT_H__

#include "tvheadend.h"
#include "idnode.h"
#include "input.h"
#include "access.h"
#include "api.h"

static idnode_set_t *
api_input_hw_tree ( void )
{
  tvh_hardware_t *th;
  idnode_set_t *is = idnode_set_create(0);
  TVH_HARDWARE_FOREACH(th)
    idnode_set_add(is, &th->th_id, NULL, NULL);
  return is;
}

#if ENABLE_SATIP_CLIENT
static int
api_input_satip_discover
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err = 0;

  if (op == NULL || strcmp(op, "all"))
    return EINVAL;

  tvhinfo(LS_SATIP, "Triggered new server discovery");

  tvh_mutex_lock(&global_lock);
  satip_device_discovery_start();
  tvh_mutex_unlock(&global_lock);

  return err;
}
#endif

void api_input_init ( void )
{
  static api_hook_t ah[] = {
    { "hardware/tree", ACCESS_ADMIN,     api_idnode_tree, api_input_hw_tree }, 
#if ENABLE_SATIP_CLIENT
    { "hardware/satip/discover", ACCESS_ADMIN, api_input_satip_discover, NULL },
#endif
    { NULL },
  };

  api_register_all(ah);
}


#endif /* __TVH_API_INPUT_H__ */
