/*
 *  API - bouquet calls
 *
 *  Copyright (C) 2014 Jaroslav Kysela
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

#ifndef __TVH_API_BOUQUET_H__
#define __TVH_API_BOUQUET_H__

#include "tvheadend.h"
#include "bouquet.h"
#include "access.h"
#include "api.h"

static int
api_bouquet_list
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  bouquet_t *bq;
  htsmsg_t *l, *e;
  char ubuf[UUID_HEX_SIZE];

  l = htsmsg_create_list();
  pthread_mutex_lock(&global_lock);
  RB_FOREACH(bq, &bouquets, bq_link) {
    e = htsmsg_create_map();
    htsmsg_add_str(e, "key", idnode_uuid_as_str(&bq->bq_id, ubuf));
    htsmsg_add_str(e, "val", bq->bq_name ?: "");
    htsmsg_add_msg(l, NULL, e);
  }
  pthread_mutex_unlock(&global_lock);
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);

  return 0;
}

static void
api_bouquet_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf )
{
  bouquet_t *bq;

  RB_FOREACH(bq, &bouquets, bq_link)
    idnode_set_add(ins, (idnode_t*)bq, &conf->filter, perm->aa_lang_ui);
}

static int
api_bouquet_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf;
  bouquet_t *bq;
  const char *s;

  if (!(conf = htsmsg_get_map(args, "conf")))
    return EINVAL;
  s = htsmsg_get_str(conf, "ext_url");
  if (s == NULL || *s == '\0')
    return EINVAL;

  pthread_mutex_lock(&global_lock);
  bq = bouquet_create(NULL, conf, NULL, NULL);
  if (bq)
    idnode_changed(&bq->bq_id);
  pthread_mutex_unlock(&global_lock);

  return 0;
}

static int
api_bouquet_scan
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_field_t *f;
  htsmsg_t *uuids;
  bouquet_t *bq;
  const char *uuid;

  if (!(f = htsmsg_field_find(args, "uuid")))
    return -EINVAL;
  if ((uuids = htsmsg_field_get_list(f))) {
    HTSMSG_FOREACH(f, uuids) {
      if (!(uuid = htsmsg_field_get_str(f))) continue;
      pthread_mutex_lock(&global_lock);
      bq = bouquet_find_by_uuid(uuid);
      if (bq)
        bouquet_scan(bq);
      pthread_mutex_unlock(&global_lock);
    }
  } else if ((uuid = htsmsg_field_get_str(f))) {
    pthread_mutex_lock(&global_lock);
    bq = bouquet_find_by_uuid(uuid);
    if (bq)
      bouquet_scan(bq);
    pthread_mutex_unlock(&global_lock);
    if (!bq)
      return -ENOENT;
  } else {
    return -EINVAL;
  }

  return 0;
}

void api_bouquet_init ( void )
{
  static api_hook_t ah[] = {
    { "bouquet/list",    ACCESS_ADMIN, api_bouquet_list, NULL },
    { "bouquet/class",   ACCESS_ADMIN, api_idnode_class, (void*)&bouquet_class },
    { "bouquet/grid",    ACCESS_ADMIN, api_idnode_grid,  api_bouquet_grid },
    { "bouquet/create",  ACCESS_ADMIN, api_bouquet_create, NULL },
    { "bouquet/scan",    ACCESS_ADMIN, api_bouquet_scan, NULL },

    { NULL },
  };

  api_register_all(ah);
}

#endif /* __TVH_API_BOUQUET_H__ */
