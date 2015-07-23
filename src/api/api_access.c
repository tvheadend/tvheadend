/*
 *  API - access control
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

#include "tvheadend.h"
#include "access.h"
#include "api.h"

/*
 *
 */

static void
api_passwd_entry_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  passwd_entry_t *pw;

  TAILQ_FOREACH(pw, &passwd_entries, pw_link)
    idnode_set_add(ins, (idnode_t*)pw, &conf->filter, perm->aa_lang);
}

static int
api_passwd_entry_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf;
  passwd_entry_t *pw;

  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;

  pthread_mutex_lock(&global_lock);
  if ((pw = passwd_entry_create(NULL, conf)) != NULL)
    passwd_entry_save(pw);
  pthread_mutex_unlock(&global_lock);

  return 0;
}

/*
 *
 */

static void
api_access_entry_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  access_entry_t *ae;

  TAILQ_FOREACH(ae, &access_entries, ae_link)
    idnode_set_add(ins, (idnode_t*)ae, &conf->filter, perm->aa_lang);
}

static int
api_access_entry_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf;
  access_entry_t *ae;

  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;

  pthread_mutex_lock(&global_lock);
  if ((ae = access_entry_create(NULL, conf)) != NULL)
    access_entry_save(ae);
  pthread_mutex_unlock(&global_lock);

  return 0;
}

void api_access_init ( void )
{
  static api_hook_t ah[] = {
    { "passwd/entry/class",  ACCESS_ADMIN, api_idnode_class, (void*)&passwd_entry_class },
    { "passwd/entry/grid",   ACCESS_ADMIN, api_idnode_grid,  api_passwd_entry_grid },
    { "passwd/entry/create", ACCESS_ADMIN, api_passwd_entry_create, NULL },

    { "access/entry/class",  ACCESS_ADMIN, api_idnode_class, (void*)&access_entry_class },
    { "access/entry/grid",   ACCESS_ADMIN, api_idnode_grid,  api_access_entry_grid },
    { "access/entry/create", ACCESS_ADMIN, api_access_entry_create, NULL },

    { NULL },
  };

  api_register_all(ah);
}
