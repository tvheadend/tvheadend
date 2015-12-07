/*
 *  tvheadend - API access to Stream Profile
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
#include "profile.h"

/*
 *
 */
static int
api_profile_list
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  profile_t *pro;
  htsmsg_t *l, *e;
  char ubuf[UUID_HEX_SIZE];

  l = htsmsg_create_list();
  pthread_mutex_lock(&global_lock);
  TAILQ_FOREACH(pro, &profiles, pro_link) {
    e = htsmsg_create_map();
    htsmsg_add_str(e, "key", idnode_uuid_as_str(&pro->pro_id, ubuf));
    htsmsg_add_str(e, "val", profile_get_name(pro));
    htsmsg_add_msg(l, NULL, e);
  }
  pthread_mutex_unlock(&global_lock);
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  return 0;
}

static int
api_profile_builders
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  profile_build_t *pb;
  htsmsg_t *l, *e;

  l = htsmsg_create_list();

  pthread_mutex_lock(&global_lock);
  /* List of available builder classes */
  LIST_FOREACH(pb, &profile_builders, link)
    if ((e = idclass_serialize(pb->clazz, perm->aa_lang_ui)))
      htsmsg_add_msg(l, NULL, e);
  pthread_mutex_unlock(&global_lock);

  /* Output */
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);

  return 0;
}

static int
api_profile_create
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
  if (profile_create(NULL, conf, 1) == NULL)
    err = -EINVAL;
  pthread_mutex_unlock(&global_lock);

  return err;
}

/*
 * Init
 */
void
api_profile_init ( void )
{
  static api_hook_t ah[] = {
    { "profile/list",       ACCESS_ANONYMOUS, api_profile_list,     NULL },
    { "profile/class",      ACCESS_ADMIN,     api_idnode_class, (void*)&profile_class },
    { "profile/builders",   ACCESS_ADMIN,     api_profile_builders, NULL },
    { "profile/create",     ACCESS_ADMIN,     api_profile_create,   NULL },
    { NULL },
  };

  api_register_all(ah);
}
