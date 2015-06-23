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

#include "tvheadend.h"
#include "access.h"
#include "idnode.h"
#include "htsmsg.h"
#include "api.h"

static htsmsg_t *
api_idnode_flist_conf( htsmsg_t *args, const char *name )
{
  htsmsg_t *m = NULL;
  const char *s = htsmsg_get_str(args, name);
  char *r, *saveptr = NULL;
  int use = 1;
  if (s && s[0] == '-') {
    use = 0;
    s++;
  }
  if (s && s[0] != '\0') {
    s = r = strdup(s);
    r = strtok_r(r, ",;:", &saveptr);
    while (r) {
      while (*r != '\0' && *r <= ' ')
        r++;
      if (*r != '\0') {
        if (m == NULL)
          m = htsmsg_create_map();
        htsmsg_add_bool(m, r, use);
      }
      r = strtok_r(NULL, ",;:", &saveptr);
    }
    free((char *)s);
  }
  return m;
}

static struct strtab filtcmptab[] = {
  { "gt", IC_GT },
  { "lt", IC_LT },
  { "eq", IC_EQ }
};

static void
api_idnode_grid_conf
  ( access_t *perm, htsmsg_t *args, api_idnode_grid_conf_t *conf )
{
  htsmsg_field_t *f, *f2;
  htsmsg_t *filter, *e;
  const char *str;

  conf->start = htsmsg_get_u32_or_default(args, "start", 0);
  conf->limit = htsmsg_get_u32_or_default(args, "limit", 50);

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
        f2 = htsmsg_field_find(e, "value");
        if (f2) {
          int t = str2val(htsmsg_get_str(e, "comparison") ?: "", filtcmptab);
          if (f2->hmf_type == HMF_DBL) {
            double dbl;
            if (!htsmsg_field_get_dbl(f2, &dbl))
              idnode_filter_add_dbl(&conf->filter, k, dbl, t == -1 ? IC_EQ : t);
          } else {
            int64_t v;
            int64_t intsplit = 0;
            htsmsg_get_s64(e, "intsplit", &intsplit);
            if (!htsmsg_field_get_s64(f2, &v))
              idnode_filter_add_num(&conf->filter, k, v, t == -1 ? IC_EQ : t, intsplit);
          }
        }
      } else if (!strcmp(t, "boolean")) {
        uint32_t v;
        if (!htsmsg_get_u32(e, "value", &v))
          idnode_filter_add_bool(&conf->filter, k, v, IC_EQ);
      }
    }
  }

  /* Sort */
  conf->sort.lang = perm->aa_lang;
  if ((str = htsmsg_get_str(args, "sort"))) {
    conf->sort.key = str;
    if ((str = htsmsg_get_str(args, "dir")) && !strcasecmp(str, "DESC"))
      conf->sort.dir = IS_DSC;
    else
      conf->sort.dir = IS_ASC;
  } else
    conf->sort.key = NULL;
}

int
api_idnode_grid
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int i;
  htsmsg_t *list, *e;
  htsmsg_t *flist = api_idnode_flist_conf(args, "list");
  api_idnode_grid_conf_t conf = { 0 };
  idnode_t *in;
  idnode_set_t ins = { 0 };
  api_idnode_grid_callback_t cb = opaque;

  /* Grid configuration */
  api_idnode_grid_conf(perm, args, &conf);

  /* Create list */
  pthread_mutex_lock(&global_lock);
  cb(perm, &ins, &conf, args);

  /* Sort */
  if (conf.sort.key)
    idnode_set_sort(&ins, &conf.sort);

  /* Paginate */
  list  = htsmsg_create_list();
  for (i = conf.start; i < ins.is_count && conf.limit != 0; i++) {
    e = htsmsg_create_map();
    htsmsg_add_str(e, "uuid", idnode_uuid_as_str(ins.is_array[i]));
    in = ins.is_array[i];
    if (idnode_perm(in, perm, NULL))
      continue;
    idnode_read0(in, e, flist, 0, perm->aa_lang);
    idnode_perm_unset(in);
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
  htsmsg_destroy(flist);

  return 0;
}

int
api_idnode_load_by_class
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
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
  if ((is = idnode_find_all(idc, NULL))) {
    for (i = 0; i < is->is_count; i++) {
      in = is->is_array[i];

      if (idnode_perm(in, perm, NULL))
        continue;

      /* Name/UUID only */
      if (_enum) {
        e = htsmsg_create_map();
        htsmsg_add_str(e, "key", idnode_uuid_as_str(in));
        htsmsg_add_str(e, "val", idnode_get_title(in, perm->aa_lang));

      /* Full record */
      } else {
        htsmsg_t *flist = api_idnode_flist_conf(args, "list");
        e = idnode_serialize0(in, flist, 0, perm->aa_lang);
        htsmsg_destroy(flist);
      }

      if (e)
        htsmsg_add_msg(l, NULL, e);

      idnode_perm_unset(in);
    }
    free(is->is_array);
    free(is);
  }
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);

  pthread_mutex_unlock(&global_lock);

  return 0;
}

static int
api_idnode_load
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err = 0, meta, count = 0;
  idnode_t *in;
  htsmsg_t *uuids, *l = NULL, *m;
  htsmsg_t *flist;
  htsmsg_field_t *f;
  const char *uuid = NULL, *class;

  /* Class based */
  if ((class = htsmsg_get_str(args, "class"))) {
    const idclass_t *idc;
    pthread_mutex_lock(&global_lock);
    idc = idclass_find(class);
    pthread_mutex_unlock(&global_lock);
    if (!idc)
      return EINVAL;
    // TODO: bit naff that 2 locks are required here
    return api_idnode_load_by_class(perm, (void*)idc, NULL, args, resp);
  }
  
  /* UUIDs */
  if (!(f = htsmsg_field_find(args, "uuid")))
    return EINVAL;
  if (!(uuids = htsmsg_field_get_list(f)))
    if (!(uuid = htsmsg_field_get_str(f)))
      return EINVAL;
  meta = htsmsg_get_s32_or_default(args, "meta", 0);

  flist = api_idnode_flist_conf(args, "list");

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
      m = idnode_serialize0(in, flist, 0, perm->aa_lang);
      if (meta > 0)
        htsmsg_add_msg(m, "meta", idclass_serialize0(in->in_class, flist, 0, perm->aa_lang));
      htsmsg_add_msg(l, NULL, m);
      count++;
      idnode_perm_unset(in);
    }

    if (count)
      err = 0;

  /* Single */
  } else {
    if (!(in = idnode_find(uuid, NULL, NULL)))
      err = ENOENT;
    else {
      if (idnode_perm(in, perm, NULL)) {
        err = EPERM;
      } else {
        l = htsmsg_create_list();
        m = idnode_serialize0(in, flist, 0, perm->aa_lang);
        if (meta > 0)
          htsmsg_add_msg(m, "meta", idclass_serialize0(in->in_class, flist, 0, perm->aa_lang));
        htsmsg_add_msg(l, NULL, m);
        idnode_perm_unset(in);
      }
    }
  }

  if (l) {
    *resp = htsmsg_create_map();
    htsmsg_add_msg(*resp, "entries", l);
  }

  pthread_mutex_unlock(&global_lock);

  htsmsg_destroy(flist);

  return err;
}

static int
api_idnode_save
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err = EINVAL;
  idnode_t *in;
  htsmsg_t *msg, *conf;
  htsmsg_field_t *f;
  const char *uuid;
  int count = 0;

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
    if (!(in = idnode_find(uuid, NULL, NULL)))
      goto exit;
    if (idnode_perm(in, perm, msg)) {
      err = EPERM;
      goto exit;
    }
    idnode_update(in, msg);
    idnode_perm_unset(in);
    err = 0;

  /* Multiple */
  } else {
    const idnodes_rb_t *domain = NULL;
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

int
api_idnode_tree
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
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
    if (!(node = idnode_find(isroot ? root : uuid, NULL, NULL))) {
      pthread_mutex_unlock(&global_lock);
      return EINVAL;
    }
  }

  *resp = htsmsg_create_list();

  /* Root node */
  if (isroot && node) {
    htsmsg_t *m;
    if (idnode_perm(node, perm, NULL)) {
      pthread_mutex_unlock(&global_lock);
      return EINVAL;
    }
    m = idnode_serialize(node, perm->aa_lang);
    idnode_perm_unset(node);
    htsmsg_add_u32(m, "leaf", idnode_is_leaf(node));
    htsmsg_add_msg(*resp, NULL, m);

  /* Children */
  } else {
    idnode_set_t *v = node ? idnode_get_childs(node) : rootfn(perm);
    if (v) {
      int i;
      idnode_set_sort_by_title(v, perm->aa_lang);
      for(i = 0; i < v->is_count; i++) {
        idnode_t *in = v->is_array[i];
        htsmsg_t *m;
        if (idnode_perm(in, perm, NULL))
          continue;
        m = idnode_serialize(v->is_array[i], perm->aa_lang);
        idnode_perm_unset(in);
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
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err = EINVAL;
  const char      *name;
  const idclass_t *idc;
  htsmsg_t *flist = api_idnode_flist_conf(args, "list");

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
  *resp = idclass_serialize0(idc, flist, 0, perm->aa_lang);

exit:
  pthread_mutex_unlock(&global_lock);

  htsmsg_destroy(flist);

  return err;
}

int
api_idnode_handler
  ( access_t *perm, htsmsg_t *args, htsmsg_t **resp,
    void (*handler)(access_t *perm, idnode_t *in),
    const char *op )
{
  int err = 0;
  idnode_t *in;
  htsmsg_t *uuids, *msg;
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
    const idnodes_rb_t *domain = NULL;
    HTSMSG_FOREACH(f, uuids) {
      if (!(uuid = htsmsg_field_get_string(f))) continue;
      if (!(in   = idnode_find(uuid, NULL, domain))) continue;
      domain = in->in_domain;
      handler(perm, in);
    }
  
  /* Single */
  } else {
    uuid = htsmsg_field_get_string(f);
    if (!(in   = idnode_find(uuid, NULL, NULL))) {
      err = ENOENT;
    } else {
      msg = htsmsg_create_map();
      htsmsg_add_str(msg, "__op__", op);
      if (idnode_perm(in, perm, msg)) {
        err = EPERM;
      } else {
        handler(perm, in);
        idnode_perm_unset(in);
      }
      htsmsg_destroy(msg);
    }
  }

  pthread_mutex_unlock(&global_lock);

  return err;
}

static void
api_idnode_delete_ (access_t *perm, idnode_t *in)
{
  return idnode_delete(in);
}

static int
api_idnode_delete
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return api_idnode_handler(perm, args, resp, api_idnode_delete_, "delete");
}

static void
api_idnode_moveup_ (access_t *perm, idnode_t *in)
{
  return idnode_moveup(in);
}

static int
api_idnode_moveup
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return api_idnode_handler(perm, args, resp, api_idnode_moveup_, "moveup");
}

static void
api_idnode_movedown_ (access_t *perm, idnode_t *in)
{
  return idnode_movedown(in);
}

static int
api_idnode_movedown
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return api_idnode_handler(perm, args, resp, api_idnode_movedown_, "movedown");
}

void api_idnode_init ( void )
{
  /*
   * note: permissions are verified using idnode_perm() calls
   */
  static api_hook_t ah[] = {
    { "idnode/load",     ACCESS_ANONYMOUS, api_idnode_load,     NULL },
    { "idnode/save",     ACCESS_ANONYMOUS, api_idnode_save,     NULL },
    { "idnode/tree",     ACCESS_ANONYMOUS, api_idnode_tree,     NULL },
    { "idnode/class",    ACCESS_ANONYMOUS, api_idnode_class,    NULL },
    { "idnode/delete",   ACCESS_ANONYMOUS, api_idnode_delete,   NULL },
    { "idnode/moveup",   ACCESS_ANONYMOUS, api_idnode_moveup,   NULL },
    { "idnode/movedown", ACCESS_ANONYMOUS, api_idnode_movedown, NULL },
    { NULL },
  };

  api_register_all(ah);
}
