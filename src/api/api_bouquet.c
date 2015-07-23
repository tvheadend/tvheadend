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

  l = htsmsg_create_list();
  pthread_mutex_lock(&global_lock);
  RB_FOREACH(bq, &bouquets, bq_link) {
    e = htsmsg_create_map();
    htsmsg_add_str(e, "key", idnode_uuid_as_str(&bq->bq_id));
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
    idnode_set_add(ins, (idnode_t*)bq, &conf->filter, perm->aa_lang);
}

static int
api_bouquet_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf;
  bouquet_t *bq;

  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;

  pthread_mutex_lock(&global_lock);
  bq = bouquet_create(NULL, conf, NULL, NULL);
  if (bq)
    bouquet_save(bq, 0);
  pthread_mutex_unlock(&global_lock);

  return 0;
}

void api_bouquet_init ( void )
{
  static api_hook_t ah[] = {
    { "bouquet/list",    ACCESS_ADMIN, api_bouquet_list, NULL },
    { "bouquet/class",   ACCESS_ADMIN, api_idnode_class, (void*)&bouquet_class },
    { "bouquet/grid",    ACCESS_ADMIN, api_idnode_grid,  api_bouquet_grid },
    { "bouquet/create",  ACCESS_ADMIN, api_bouquet_create, NULL },

    { NULL },
  };

  api_register_all(ah);
}

#endif /* __TVH_API_BOUQUET_H__ */
