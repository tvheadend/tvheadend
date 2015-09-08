/*
 *  tvheadend - API access to Conditional Access Clients
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
#include "htsmsg.h"
#include "api.h"
#include "descrambler/caclient.h"

/*
 *
 */
static int
api_caclient_list
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  caclient_t *cac;
  htsmsg_t *l, *e;

  l = htsmsg_create_list();
  pthread_mutex_lock(&global_lock);
  TAILQ_FOREACH(cac, &caclients, cac_link) {
    e = htsmsg_create_map();
    htsmsg_add_str(e, "uuid", idnode_uuid_as_sstr(&cac->cac_id));
    htsmsg_add_str(e, "title", idnode_get_title(&cac->cac_id, perm->aa_lang));
    htsmsg_add_str(e, "status", caclient_get_status(cac));
    htsmsg_add_msg(l, NULL, e);
  }
  pthread_mutex_unlock(&global_lock);
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  return 0;
}

static int
api_caclient_builders
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  const idclass_t **r;
  htsmsg_t *l, *e;

  /* List of available builder classes */
  l = htsmsg_create_list();
  for (r = caclient_classes; *r; r++)
    if ((e = idclass_serialize(*r, perm->aa_lang)))
      htsmsg_add_msg(l, NULL, e);

  /* Output */
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);

  return 0;
}

static int
api_caclient_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err = 0;
  const char *clazz;
  htsmsg_t *conf;

  if (!(clazz = htsmsg_get_str(args, "class")))
    return EINVAL;
  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;
  htsmsg_set_str(conf, "class", clazz);

  pthread_mutex_lock(&global_lock);
  if (caclient_create(NULL, conf, 1) == NULL)
    err = -EINVAL;
  pthread_mutex_unlock(&global_lock);

  return err;
}

/*
 * Init
 */
void
api_caclient_init ( void )
{
  static api_hook_t ah[] = {
    { "caclient/list",       ACCESS_ADMIN, api_caclient_list,     NULL },
    { "caclient/class",      ACCESS_ADMIN, api_idnode_class, (void*)&caclient_class },
    { "caclient/builders",   ACCESS_ADMIN, api_caclient_builders, NULL },
    { "caclient/create",     ACCESS_ADMIN, api_caclient_create,   NULL },
    { NULL },
  };

  api_register_all(ah);
}
