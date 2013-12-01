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
#include "service.h"
#include "service_mapper.h"
#include "access.h"
#include "api.h"
#include "notify.h"

static int
api_mapper_start
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  service_mapper_conf_t conf = { 0 };
  htsmsg_t *uuids;
#define get_u32(x)\
  conf.x = htsmsg_get_bool_or_default(args, #x, 0)
  
  /* Get config */
  uuids = htsmsg_get_list(args, "uuids");
  get_u32(check_availability);
  get_u32(encrypted);
  get_u32(merge_same_name);
  get_u32(provider_tags);
  
  pthread_mutex_lock(&global_lock);
  service_mapper_start(&conf, uuids);
  pthread_mutex_unlock(&global_lock);

  return 0;
}

static int
api_mapper_stop
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  pthread_mutex_lock(&global_lock);
  service_mapper_stop();
  pthread_mutex_unlock(&global_lock);

  return 0;
}

static htsmsg_t *
api_mapper_status_msg ( void )
{
  htsmsg_t *m;
  service_mapper_status_t stat = service_mapper_status();
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "total",  stat.total);
  htsmsg_add_u32(m, "ok",     stat.ok);
  htsmsg_add_u32(m, "fail",   stat.fail);
  htsmsg_add_u32(m, "ignore", stat.ignore);
  if (stat.active)
    htsmsg_add_str(m, "active", idnode_uuid_as_str(&stat.active->s_id));
  return m;
}

static int
api_mapper_status
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  pthread_mutex_lock(&global_lock);
  *resp = api_mapper_status_msg();
  pthread_mutex_unlock(&global_lock);
  return 0;
}

void
api_service_mapper_notify ( void )
{
  notify_by_msg("servicemapper", api_mapper_status_msg());
}

void api_service_init ( void )
{
  extern const idclass_t service_class;
  static api_hook_t ah[] = {
    { "service/mapper/start",   ACCESS_ADMIN, api_mapper_start,  NULL },
    { "service/mapper/stop",    ACCESS_ADMIN, api_mapper_stop,   NULL },
    { "service/mapper/status",  ACCESS_ADMIN, api_mapper_status, NULL },
    { "service/list",           ACCESS_ANONYMOUS, api_idnode_load_by_class, 
      (void*)&service_class },
    { NULL },
  };

  api_register_all(ah);
}


#endif /* __TVH_API_IDNODE_H__ */
