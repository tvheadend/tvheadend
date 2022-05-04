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
#include "subscriptions.h"
#include "api.h"
#include "profile.h"

/*
 *
 */
static int
api_profile_is_all(access_t *perm, htsmsg_t *args)
{
  return htsmsg_get_bool_or_default(args, "all", 0) &&
         !access_verify2(perm, ACCESS_ADMIN);
}

static int
api_profile_find(access_t *perm, const char *uuid)
{
  htsmsg_field_t *f;
  const char *uuid2;

  if (perm->aa_profiles == NULL)
    return 1;
  HTSMSG_FOREACH(f, perm->aa_profiles) {
    uuid2 = htsmsg_field_get_str(f) ?: "";
    if (strcmp(uuid, uuid2) == 0)
      return 1;
  }
  return 0;
}

/*
 *
 */
static int
api_profile_list
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  profile_t *pro;
  htsmsg_t *l;
  int cfg = api_profile_is_all(perm, args);
  int sflags = htsmsg_get_bool_or_default(args, "htsp", 0) ? SUBSCRIPTION_HTSP : 0;
  char ubuf[UUID_HEX_SIZE];

  sflags |= SUBSCRIPTION_PACKET|SUBSCRIPTION_MPEGTS;
  l = htsmsg_create_list();
  tvh_mutex_lock(&global_lock);
  TAILQ_FOREACH(pro, &profiles, pro_link) {
    idnode_uuid_as_str(&pro->pro_id, ubuf);
    if (!cfg && (!profile_verify(pro, sflags) || !api_profile_find(perm, ubuf)))
      continue;
    htsmsg_add_msg(l, NULL, htsmsg_create_key_val(ubuf, profile_get_name(pro)));
  }
  tvh_mutex_unlock(&global_lock);
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

  tvh_mutex_lock(&global_lock);
  /* List of available builder classes */
  LIST_FOREACH(pb, &profile_builders, link)
    if ((e = idclass_serialize(pb->clazz, perm->aa_lang_ui)))
      htsmsg_add_msg(l, NULL, e);
  tvh_mutex_unlock(&global_lock);

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
  profile_t *pro;

  if (!(clazz = htsmsg_get_str(args, "class")))
    return EINVAL;
  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;
  htsmsg_set_str(conf, "class", clazz);

  tvh_mutex_lock(&global_lock);
  pro = profile_create(NULL, conf, 1);
  if (pro)
    api_idnode_create(resp, &pro->pro_id);
  else
    err = EINVAL;
  tvh_mutex_unlock(&global_lock);

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
