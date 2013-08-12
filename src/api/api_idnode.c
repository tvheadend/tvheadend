/*
 *  API - idnode related API calls
 *
 *  Copyright (C) 2013 Adam Sutton
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

#ifndef __TVH_API_IDNODE_H__
#define __TVH_API_IDNODE_H__

#include "tvheadend.h"
#include "access.h"
#include "idnode.h"
#include "htsmsg.h"
#include "api.h"

int
api_idnode_tree0
  ( const char *uuid, const char *root, 
    idnode_set_t *(*rootfn)(void), htsmsg_t **resp )
{
  int      isroot;
  idnode_t *node = NULL;

  /* Validate */
  if (!uuid)
    return EINVAL;
  isroot = !strcmp("root", uuid);
  if (isroot && !(root || rootfn))
    return EINVAL;

  pthread_mutex_lock(&global_lock);

  if (!isroot || root) {
    if (!(node = idnode_find(isroot ? root : uuid, NULL))) {
      pthread_mutex_unlock(&global_lock);
      return EINVAL;
    }
  }

  *resp = htsmsg_create_list();

  /* Root node */
  if (isroot && node) {
    htsmsg_t *m = idnode_serialize(node);
    htsmsg_add_u32(m, "leaf", idnode_is_leaf(node));
    htsmsg_add_msg(*resp, NULL, m);

  /* Children */
  } else {
    idnode_set_t *v = node ? idnode_get_childs(node) : rootfn();
    if (v) {
      int i;
      for(i = 0; i < v->is_count; i++) {
        htsmsg_t *m = idnode_serialize(v->is_array[i]);
        htsmsg_add_u32(m, "leaf", idnode_is_leaf(v->is_array[i]));
        htsmsg_add_msg(*resp, NULL, m);
      }
      idnode_set_free(v);
    }
  }
  pthread_mutex_unlock(&global_lock);

  return 0;
}

static int
api_idnode_class
  ( const char *class, htsmsg_t *args, htsmsg_t **resp )
{
  int i, brief;
  const idclass_t *idc;
  idnode_set_t    *is;
  idnode_t        *in;
  htsmsg_t        *e;

  // TODO: this only works if pass as integer
  brief = htsmsg_get_bool_or_default(args, "brief", 0);

  pthread_mutex_lock(&global_lock);

  /* Find class */
  if (!(idc = idclass_find(class))) {
    pthread_mutex_unlock(&global_lock);
    return EINVAL;
  }

  *resp = htsmsg_create_list();
  if ((is = idnode_find_all(idc))) {
    for (i = 0; i < is->is_count; i++) {
      in = is->is_array[i];

      /* Name/UUID only */
      if (brief) {
        e = htsmsg_create_map();
        htsmsg_add_str(e, "key", idnode_uuid_as_str(in));
        htsmsg_add_str(e, "val", idnode_get_title(in));

      /* Full record */
      } else
        e = idnode_serialize(in);
        
      if (e)
        htsmsg_add_msg(*resp, NULL, e);
    }
  }

  pthread_mutex_unlock(&global_lock);

  return 0;
}

static int
api_idnode_load
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err = 0;
  idnode_t *in;
  htsmsg_t *l;
  htsmsg_field_t *f;
  const char *uuid, *class;

  /* Class based */
  if ((class = htsmsg_get_str(args, "class")))
    return api_idnode_class(class, args, resp);
  
  /* ID based */
  if (!(f = htsmsg_field_find(args, "uuid")))
    return EINVAL;

  pthread_mutex_lock(&global_lock);

  /* Single */
  if (f->hmf_type == HMF_STR) {
    uuid = htsmsg_field_get_string(f);
    in   = idnode_find(uuid, NULL);
    if (in)
      *resp = idnode_serialize(in);
    else
      err   = ENOENT;

  /* Multiple */
  } else if (f->hmf_type == HMF_LIST) {
    l     = htsmsg_get_list_by_field(f);
    *resp = htsmsg_create_list();
    HTSMSG_FOREACH(f, l) {
      if (!(uuid = htsmsg_field_get_string(f))) continue;
      if (!(in   = idnode_find(uuid, NULL))) continue;
      htsmsg_add_msg(*resp, NULL, idnode_serialize(in));
    }

  /* Invalid */
  } else {
    err = EINVAL;
  }

  pthread_mutex_unlock(&global_lock);

  return err;
}

static int
api_idnode_save
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return 0;
}

static int
api_idnode_tree
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  const char *uuid = htsmsg_get_str(args, "uuid");
  const char *root = htsmsg_get_str(args, "root");

  return api_idnode_tree0(uuid, root, NULL, resp);
}

void api_idnode_init ( void )
{
  static api_hook_t ah[] = {
    { "idnode/load", ACCESS_ANONYMOUS, api_idnode_load, NULL },
    { "idnode/save", ACCESS_ADMIN,     api_idnode_save, NULL },
    { "idnode/tree", ACCESS_ANONYMOUS, api_idnode_tree, NULL },
    { NULL },
  };

  api_register_all(ah);
}


#endif /* __TVH_API_IDNODE_H__ */
