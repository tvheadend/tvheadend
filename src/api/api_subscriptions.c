/*
 *  API - service related calls
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
#include "subscriptions.h"
#include "access.h"
#include "api.h"

static int
api_subscription_list
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int c;
  htsmsg_t *l, *e;
  th_subscription_t *ths;

  l = htsmsg_create_list();
  c = 0;
  LIST_FOREACH(ths, &subscriptions, ths_global_link) {
    e = subscription_create_msg(ths);
    htsmsg_add_msg(l, NULL, e);
    c++;
  }

  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  htsmsg_add_msg(*resp, "totalCount", c);

  return 0;
}

void api_service_init ( void )
{
  extern const idclass_t service_class;
  static api_hook_t ah[] = {
    { "subscription/list",   ACCESS_ANONYMOUS, api_subscribtion_list, NULL },
    { NULL },
  };

  api_register_all(ah);
}


#endif /* __TVH_API_IDNODE_H__ */
