/*
 *  API - ratinglabel calls
 *
 *  Copyright (C) 2014 Jaroslav Kysela (Original Bouquets)
 *  Copyright (C) 2023 DeltaMikeCharlie (Updated for Rating Labels)
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

#ifndef __TVH_API_RATINGLABEL_H__
#define __TVH_API_RATINGLABEL_H__

#include "tvheadend.h"
#include "ratinglabels.h"
#include "access.h"
#include "api.h"

static int
api_ratinglabel_list
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  ratinglabel_t *rl;
  htsmsg_t *l;
  char ubuf[UUID_HEX_SIZE];

  l = htsmsg_create_list();
  tvh_mutex_lock(&global_lock);
  RB_FOREACH(rl, &ratinglabels, rl_link)
    {
      htsmsg_add_msg(l, NULL, htsmsg_create_key_val(idnode_uuid_as_str(&rl->rl_id, ubuf), rl->rl_country ?: ""));
    }
  tvh_mutex_unlock(&global_lock);
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);

  return 0;
}

static void
api_ratinglabel_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf )
{
  ratinglabel_t *rl;

  RB_FOREACH(rl, &ratinglabels, rl_link)
    idnode_set_add(ins, (idnode_t*)rl, &conf->filter, perm->aa_lang_ui);
}

static int
api_ratinglabel_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf;
  ratinglabel_t *rl;

  if (!(conf = htsmsg_get_map(args, "conf")))
    return EINVAL;

  tvh_mutex_lock(&global_lock);
  rl = ratinglabel_create(NULL, conf, NULL, NULL);
  if (rl)
    api_idnode_create(resp, &rl->rl_id);
  tvh_mutex_unlock(&global_lock);

  return 0;
}

void api_ratinglabel_init ( void )
{
  static api_hook_t ah[] = {
    { "ratinglabel/list",    ACCESS_ADMIN, api_ratinglabel_list, NULL },
    { "ratinglabel/class",   ACCESS_ADMIN, api_idnode_class, (void*)&ratinglabel_class },
    { "ratinglabel/grid",    ACCESS_ADMIN, api_idnode_grid,  api_ratinglabel_grid },
    { "ratinglabel/create",  ACCESS_ADMIN, api_ratinglabel_create, NULL },
    { NULL },
  };

  api_register_all(ah);
}

#endif /* __TVH_API_RATINGLABEL_H__ */
