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

static void
api_bouquet_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf )
{
  bouquet_t *bq;

  RB_FOREACH(bq, &bouquets, bq_link)
    idnode_set_add(ins, (idnode_t*)bq, &conf->filter);
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
    { "bouquet/class",   ACCESS_ADMIN, api_idnode_class, (void*)&bouquet_class },
    { "bouquet/grid",    ACCESS_ADMIN, api_idnode_grid,  api_bouquet_grid },
    { "bouquet/create",  ACCESS_ADMIN, api_bouquet_create, NULL },

    { NULL },
  };

  api_register_all(ah);
}

#endif /* __TVH_API_BOUQUET_H__ */
