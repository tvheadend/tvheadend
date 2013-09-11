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

static struct strtab filtcmptab[] = {
  { "gt", IC_GT },
  { "lt", IC_LT },
  { "eq", IC_EQ }
};

static void
api_idnode_grid_conf
  ( htsmsg_t *args, api_idnode_grid_conf_t *conf )
{
  htsmsg_field_t *f;
  htsmsg_t *filter, *e;
  const char *str;

  /* Start */
  if ((str = htsmsg_get_str(args, "start")))
    conf->start = atoi(str);
  else
    conf->start = 0;

  /* Limit */
  if ((str = htsmsg_get_str(args, "limit")))
    conf->limit = atoi(str);
  else
    conf->limit = 50;

  /* Filter */
  if ((filter = htsmsg_get_list(args, "filter"))) {
    HTSMSG_FOREACH(f, filter) {
      const char *k, *t, *v;
      if (!(e = htsmsg_get_map_by_field(f))) continue;
      if (!(k = htsmsg_get_str(e, "field"))) continue;
      if (!(t = htsmsg_get_str(e, "type")))  continue;
      if (!strcmp(t, "string")) {
        if ((v = htsmsg_get_str(e, "value")))
          idnode_filter_add_str(&conf->filter, k, v, IC_RE);
      } else if (!strcmp(t, "numeric")) {
        uint32_t v;
        if (!htsmsg_get_u32(e, "value", &v)) {
          int t = str2val(htsmsg_get_str(e, "comparison") ?: "",
                          filtcmptab);
          idnode_filter_add_num(&conf->filter, k, v, t == -1 ? IC_EQ : t);
        }
      } else if (!strcmp(t, "boolean")) {
        uint32_t v;
        if (!htsmsg_get_u32(e, "value", &v))
          idnode_filter_add_bool(&conf->filter, k, v, IC_EQ);
      }
    }
  }

  /* Sort */
  if ((str = htsmsg_get_str(args, "sort"))) {
    conf->sort.key = str;
    if ((str = htsmsg_get_str(args, "dir")) && !strcmp(str, "DESC"))
      conf->sort.dir = IS_DSC;
    else
      conf->sort.dir = IS_ASC;
  } else
    conf->sort.key = NULL;
}

int
api_idnode_grid
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int i;
  htsmsg_t *list, *e;
  api_idnode_grid_conf_t conf = { 0 };
  idnode_set_t ins = { 0 };
  api_idnode_grid_callback_t cb = opaque;

  /* Grid configuration */
  api_idnode_grid_conf(args, &conf);

  /* Create list */
  pthread_mutex_lock(&global_lock);
  cb(&ins, &conf);

  /* Sort */
  if (conf.sort.key)
    idnode_set_sort(&ins, &conf.sort);

  /* Paginate */
  list  = htsmsg_create_list();
  for (i = conf.start; i < ins.is_count && conf.limit != 0; i++) {
    e = htsmsg_create_map();
    htsmsg_add_str(e, "uuid", idnode_uuid_as_str(ins.is_array[i]));
    idnode_read0(ins.is_array[i], e, 0);
    htsmsg_add_msg(list, NULL, e);
    if (conf.limit > 0) conf.limit--;
  }

  pthread_mutex_unlock(&global_lock);

  /* Output */
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", list);
  htsmsg_add_u32(*resp, "total",   ins.is_count);

  /* Cleanup */
  free(ins.is_array);
  idnode_filter_clear(&conf.filter);

  return 0;
}

int
api_idnode_load_by_class
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int i, _enum;
  const idclass_t *idc;
  idnode_set_t    *is;
  idnode_t        *in;
  htsmsg_t        *l, *e;

  // TODO: this only works if pass as integer
  _enum = htsmsg_get_bool_or_default(args, "enum", 0);

  pthread_mutex_lock(&global_lock);

  /* Find class */
  idc = opaque;
  assert(idc);

  l = htsmsg_create_list();
  if ((is = idnode_find_all(idc))) {
    for (i = 0; i < is->is_count; i++) {
      in = is->is_array[i];

      /* Name/UUID only */
      if (_enum) {
        e = htsmsg_create_map();
        htsmsg_add_str(e, "key", idnode_uuid_as_str(in));
        htsmsg_add_str(e, "val", idnode_get_title(in));

      /* Full record */
      } else
        e = idnode_serialize(in);
        
      if (e)
        htsmsg_add_msg(l, NULL, e);
    }
  }
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);

  pthread_mutex_unlock(&global_lock);

  return 0;
}

static int
api_idnode_load
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err = 0;
  idnode_t *in;
  htsmsg_t *uuids, *l = NULL;
  htsmsg_field_t *f;
  const char *uuid, *class;

  /* Class based */
  if ((class = htsmsg_get_str(args, "class"))) {
    const idclass_t *idc;
    pthread_mutex_lock(&global_lock);
    idc = idclass_find(class);
    pthread_mutex_unlock(&global_lock);
    if (!idc)
      return EINVAL;
    // TODO: bit naff that 2 locks are required here
    return api_idnode_load_by_class((void*)idc, NULL, args, resp);
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
    l = htsmsg_create_list();
    HTSMSG_FOREACH(f, uuids) {
      if (!(uuid = htsmsg_field_get_str(f))) continue;
      if (!(in   = idnode_find(uuid, NULL))) continue;
      htsmsg_add_msg(l, NULL, idnode_serialize(in));
    }

  /* Single */
  } else {
    if (!(in = idnode_find(uuid, NULL)))
      err = ENOENT;
    else {
      l     = htsmsg_create_list();
      htsmsg_add_msg(l, NULL, idnode_serialize(in));
    }
  }

  if (l) {
    *resp = htsmsg_create_map();
    htsmsg_add_msg(*resp, "entries", l);
  }

  pthread_mutex_unlock(&global_lock);

  return err;
}

static int
api_idnode_save
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err = EINVAL;
  idnode_t *in;
  htsmsg_t *msg, *conf;
  htsmsg_field_t *f;
  const char *uuid;

  if (!(f = htsmsg_field_find(args, "node")))
    return EINVAL;
  if (!(msg = htsmsg_field_get_list(f)))
    if (!(msg = htsmsg_field_get_map(f)))
      return EINVAL;

  pthread_mutex_lock(&global_lock);

  /* Single */
  if (!msg->hm_islist) {
    if (!(uuid = htsmsg_get_str(msg, "uuid")))
      goto exit;
    if (!(in = idnode_find(uuid, NULL)))
      goto exit;
    idnode_update(in, msg);
    err = 0;

  /* Multiple */
  } else {
    HTSMSG_FOREACH(f, msg) {
      if (!(conf = htsmsg_field_get_map(f)))
        continue;
      if (!(uuid = htsmsg_get_str(conf, "uuid")))
        continue;
      if (!(in = idnode_find(uuid, NULL)))
        continue;
      idnode_update(in, conf);
    }
    err = 0;
  }

  // TODO: return updated UUIDs?

exit:
  pthread_mutex_unlock(&global_lock);

  return err;
}

int
api_idnode_tree
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  const char *uuid;
  const char *root = NULL;
  int      isroot;
  idnode_t *node = NULL;
  api_idnode_tree_callback_t rootfn = opaque;

  /* UUID */
  if (!(uuid = htsmsg_get_str(args, "uuid")))
    return EINVAL;

  /* Root UUID */
  if (!rootfn)
    root = htsmsg_get_str(args, "root");

  /* Is root? */
  isroot = (strcmp("root", uuid) == 0);
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
      idnode_set_sort_by_title(v);
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

int
api_idnode_class
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err = EINVAL;
  const char      *name;
  const idclass_t *idc;

  pthread_mutex_lock(&global_lock);

  /* Lookup */
  if (!opaque) {
    if (!(name = htsmsg_get_str(args, "name")))
      goto exit;
    if (!(idc  = idclass_find(name)))
      goto exit;
  
  } else {
    idc = opaque;
  }

  err   = 0;
  *resp = idclass_serialize(idc);

exit:
  pthread_mutex_unlock(&global_lock);

  return err;
}

static int
api_idnode_delete
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err = 0;
  idnode_t *in;
  htsmsg_t *uuids;
  htsmsg_field_t *f;
  const char *uuid;

  /* ID based */
  if (!(f = htsmsg_field_find(args, "uuid")))
    return EINVAL;
  if (!(uuids = htsmsg_field_get_list(f)))
    if (!(uuid = htsmsg_field_get_str(f)))
      return EINVAL;

  pthread_mutex_lock(&global_lock);

  /* Multiple */
  if (uuids) {
    HTSMSG_FOREACH(f, uuids) {
      if (!(uuid = htsmsg_field_get_string(f))) continue;
      if (!(in   = idnode_find(uuid, NULL))) continue;
      idnode_delete(in);
    }
  
  /* Single */
  } else {
    uuid = htsmsg_field_get_string(f);
    if (!(in   = idnode_find(uuid, NULL)))
      err = ENOENT;
    else
      idnode_delete(in);
  }

  pthread_mutex_unlock(&global_lock);

  return err;
}

void api_idnode_init ( void )
{
  static api_hook_t ah[] = {
    { "idnode/load",   ACCESS_ANONYMOUS, api_idnode_load,   NULL },
    { "idnode/save",   ACCESS_ADMIN,     api_idnode_save,   NULL },
    { "idnode/tree",   ACCESS_ANONYMOUS, api_idnode_tree,   NULL },
    { "idnode/class",  ACCESS_ANONYMOUS, api_idnode_class,  NULL },
    { "idnode/delete", ACCESS_ADMIN,     api_idnode_delete, NULL },
    { NULL },
  };

  api_register_all(ah);
}


#endif /* __TVH_API_IDNODE_H__ */
