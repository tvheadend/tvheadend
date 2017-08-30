/*
 *  API - idnode raw load/save related API calls
 *
 *  Copyright (C) 2017 Jaroslav Kysela
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
#include "idnode.h"
#include "htsmsg.h"
#include "api.h"

static int
api_idnode_raw_export_by_class0
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int i;
  const idclass_t *idc;
  idnode_set_t    *is;
  idnode_t        *in;
  htsmsg_t        *l, *e;
  char filename[PATH_MAX];

  /* Find class */
  idc = opaque;
  assert(idc);

  l = htsmsg_create_list();
  if ((is = idnode_find_all(idc, NULL))) {
    for (i = 0; i < is->is_count; i++) {
      in = is->is_array[i];

      if (idnode_perm(in, perm, NULL))
        continue;

      e = idnode_savefn(in, filename, sizeof(filename));

      if (e)
        htsmsg_add_msg(l, NULL, e);

      idnode_perm_unset(in);
    }
    free(is->is_array);
    free(is);
  }

  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);

  return 0;
}

static int
api_idnode_raw_export
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err = 0, count = 0;
  idnode_t *in;
  htsmsg_t *uuids, *l = NULL, *m;
  htsmsg_field_t *f;
  const char *uuid = NULL, *class;
  char filename[PATH_MAX];

  /* Class based */
  if ((class = htsmsg_get_str(args, "class"))) {
    const idclass_t *idc;
    pthread_mutex_lock(&global_lock);
    idc = idclass_find(class);
    if (idc)
      err = api_idnode_raw_export_by_class0(perm, (void*)idc, NULL, args, resp);
    else
      err = EINVAL;
    pthread_mutex_unlock(&global_lock);
    return err;
  }
  
  /* UUIDs */
  if (!(f = htsmsg_field_find(args, "uuid")))
    return EINVAL;
  if (!(uuids = htsmsg_field_get_list(f)))
    if (!(uuid = htsmsg_field_get_str(f)))
      return EINVAL;

  pthread_mutex_lock(&global_lock);

  /* Multiple */
  if (uuids) {
    const idnodes_rb_t *domain = NULL;
    l = htsmsg_create_list();
    HTSMSG_FOREACH(f, uuids) {
      if (!(uuid = htsmsg_field_get_str(f))) continue;
      if (!(in   = idnode_find(uuid, NULL, domain))) continue;
      domain = in->in_domain;
      if (idnode_perm(in, perm, NULL)) {
        err = EPERM;
        continue;
      }
      m = idnode_savefn(in, filename, sizeof(filename));
      if (m)
        htsmsg_add_msg(l, NULL, m);
      count++;
      idnode_perm_unset(in);
    }

    if (count)
      err = 0;

  /* Single */
  } else {
    l = htsmsg_create_list();
    if ((in = idnode_find(uuid, NULL, NULL)) != NULL) {
      if (idnode_perm(in, perm, NULL)) {
        err = EPERM;
      } else {
        m = idnode_savefn(in, filename, sizeof(filename));
        if (m)
          htsmsg_add_msg(l, NULL, m);
        idnode_perm_unset(in);
      }
    }
  }

  if (l && err == 0) {
    *resp = htsmsg_create_map();
    htsmsg_add_msg(*resp, "entries", l);
  } else {
    htsmsg_destroy(l);
  }

  pthread_mutex_unlock(&global_lock);

  return err;
}

static int
api_idnode_raw_import
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err = EINVAL;
  idnode_t *in;
  htsmsg_t *msg, *conf;
  htsmsg_field_t *f;
  const char *uuid;
  int count = 0;
  const idnodes_rb_t *domain = NULL;

  if (!(f = htsmsg_field_find(args, "node")))
    return EINVAL;
  if (!(msg = htsmsg_field_get_list(f)))
    if (!(msg = htsmsg_field_get_map(f)))
      return EINVAL;

  pthread_mutex_lock(&global_lock);

  /* Single or Foreach */
  if (!msg->hm_islist) {

    if (!(uuid = htsmsg_get_str(msg, "uuid"))) {

      /* Foreach */
      f = htsmsg_field_find(msg, "uuid");
      if (!f || !(conf = htsmsg_field_get_list(f)))
        goto exit;
      HTSMSG_FOREACH(f, conf) {
        if (!(uuid = htsmsg_field_get_str(f)))
          continue;
        if (!(in = idnode_find(uuid, NULL, domain)))
          continue;
        domain = in->in_domain;
        if (idnode_perm(in, perm, msg)) {
          err = EPERM;
          continue;
        }
        count++;
        idnode_update(in, msg);
        idnode_perm_unset(in);
      }
      if (count)
        err = 0;

    } else {

      if (!(in = idnode_find(uuid, NULL, NULL)))
        goto exit;
      if (idnode_perm(in, perm, msg)) {
        err = EPERM;
        goto exit;
      }
      idnode_update(in, msg);
      idnode_perm_unset(in);
      err = 0;

    }

  /* Multiple */
  } else {

    HTSMSG_FOREACH(f, msg) {
      if (!(conf = htsmsg_field_get_map(f)))
        continue;
      if (!(uuid = htsmsg_get_str(conf, "uuid")))
        continue;
      if (!(in = idnode_find(uuid, NULL, domain)))
        continue;
      domain = in->in_domain;
      if (idnode_perm(in, perm, conf)) {
        err = EPERM;
        continue;
      }
      count++;
      idnode_update(in, conf);
      idnode_perm_unset(in);
    }
    if (count)
      err = 0;

  }

  // TODO: return updated UUIDs?

exit:
  pthread_mutex_unlock(&global_lock);

  return err;
}

void api_idnode_raw_init ( void )
{
  /*
   * note: permissions are verified using idnode_perm() calls
   */
  static api_hook_t ah[] = {
    { "raw/export",     ACCESS_ANONYMOUS, api_idnode_raw_export,     NULL },
    { "raw/import",     ACCESS_ANONYMOUS, api_idnode_raw_import,     NULL },
    { NULL },
  };

  api_register_all(ah);
}
