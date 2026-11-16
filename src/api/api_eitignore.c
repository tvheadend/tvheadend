/*
 *  API - EIT ignore rules
 *
 *  Copyright (C) 2026 Michael Utech
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
#include "epggrab/eitignore.h"

static void
api_eitignore_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf )
{
  eitignore_t *ei;

  TAILQ_FOREACH(ei, &eitignores, ei_link)
    idnode_set_add(ins, (idnode_t*)ei, &conf->filter, perm->aa_lang_ui);
}

static int
api_eitignore_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf;
  eitignore_t *ei;

  if (!(conf = htsmsg_get_map(args, "conf")))
    return EINVAL;

  tvh_mutex_lock(&global_lock);
  ei = eitignore_create(NULL, conf);
  if (ei)
    api_idnode_create(resp, &ei->ei_id);
  tvh_mutex_unlock(&global_lock);

  return 0;
}

void api_eitignore_init ( void )
{
  static api_hook_t ah[] = {
    { "eitignore/class",   ACCESS_ADMIN, api_idnode_class, (void*)&eitignore_class },
    { "eitignore/grid",    ACCESS_ADMIN, api_idnode_grid,  api_eitignore_grid },
    { "eitignore/create",  ACCESS_ADMIN, api_eitignore_create, NULL },
    { NULL },
  };

  api_register_all(ah);
}
