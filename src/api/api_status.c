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
#include "tcp.h"
#include "input.h"

static int
api_status_inputs
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int c = 0;
  htsmsg_t *l, *e;
  tvh_input_t *ti;
  tvh_input_stream_t *st;
  tvh_input_stream_list_t stl = { 0 };
  
  TVH_INPUT_FOREACH(ti)
    ti->ti_get_streams(ti, &stl);

  l = htsmsg_create_list();
  while ((st = LIST_FIRST(&stl))) {
    e = tvh_input_stream_create_msg(st);
    htsmsg_add_msg(l, NULL, e);
    tvh_input_stream_destroy(st);
    LIST_REMOVE(st, link);
    free(st);
    c++;
  }
    
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  htsmsg_add_u32(*resp, "totalCount", c);

  return 0;
}

static int
api_status_subscriptions
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
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
  htsmsg_add_u32(*resp, "totalCount", c);

  return 0;
}

static int
api_status_connections
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  pthread_mutex_lock(&global_lock);
  *resp = tcp_server_connections();
  pthread_mutex_unlock(&global_lock);
  return 0;
}

static int
api_connections_cancel
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_field_t *f;
  htsmsg_t *ids;
  uint32_t id;

  if (!(f = htsmsg_field_find(args, "id")))
    return EINVAL;
  if (!(ids = htsmsg_field_get_list(f)))
    if (htsmsg_field_get_u32(f, &id))
      return EINVAL;

  if (ids) {
    HTSMSG_FOREACH(f, ids) {
      if (htsmsg_field_get_u32(f, &id)) continue;
      if (!id) continue;
      pthread_mutex_lock(&global_lock);
      tcp_connection_cancel(id);
      pthread_mutex_unlock(&global_lock);
    }
  } else {
    pthread_mutex_lock(&global_lock);
    tcp_connection_cancel(id);
    pthread_mutex_unlock(&global_lock);
  }
  return 0;
}

void api_status_init ( void )
{
  static api_hook_t ah[] = {
    { "status/connections",   ACCESS_ADMIN, api_status_connections, NULL },
    { "status/subscriptions", ACCESS_ADMIN, api_status_subscriptions, NULL },
    { "status/inputs",        ACCESS_ADMIN, api_status_inputs, NULL },
    { "connections/cancel",   ACCESS_ADMIN, api_connections_cancel, NULL },
    { NULL },
  };

  api_register_all(ah);
}


#endif /* __TVH_API_IDNODE_H__ */
