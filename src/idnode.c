/*
 *  Tvheadend - idnode (class) system
 *
 *  Copyright (C) 2013 Andreas Öman
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

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "idnode.h"
#include "notify.h"
#include "settings.h"
#include "uuid.h"

static const idnodes_rb_t * idnode_domain ( const idclass_t *idc );
static void idclass_root_register ( idnode_t *in );

typedef struct idclass_link
{
  const idclass_t       *idc;
  idnodes_rb_t           nodes;
  RB_ENTRY(idclass_link) link;
} idclass_link_t;

static idnodes_rb_t           idnodes;
static RB_HEAD(,idclass_link) idclasses;
static RB_HEAD(,idclass_link) idrootclasses;
static pthread_cond_t         idnode_cond;
static pthread_mutex_t        idnode_mutex;
static htsmsg_t              *idnode_queue;
static void*                  idnode_thread(void* p);

SKEL_DECLARE(idclasses_skel, idclass_link_t);

/* **************************************************************************
 * Utilities
 * *************************************************************************/

/**
 *
 */
static int
in_cmp(const idnode_t *a, const idnode_t *b)
{
  return memcmp(a->in_uuid, b->in_uuid, sizeof(a->in_uuid));
}

static int
ic_cmp ( const idclass_link_t *a, const idclass_link_t *b )
{
  assert(a->idc->ic_class);
  assert(b->idc->ic_class);
  return strcmp(a->idc->ic_class, b->idc->ic_class);
}

/* **************************************************************************
 * Registration
 * *************************************************************************/

/**
 *
 */
pthread_t idnode_tid;

void
idnode_init(void)
{
  idnode_queue = NULL;
  RB_INIT(&idnodes);
  RB_INIT(&idclasses);
  RB_INIT(&idrootclasses);
  pthread_mutex_init(&idnode_mutex, NULL);
  pthread_cond_init(&idnode_cond, NULL);
  tvhthread_create(&idnode_tid, NULL, idnode_thread, NULL);
}

void
idnode_done(void)
{
  idclass_link_t *il;

  pthread_cond_signal(&idnode_cond);
  pthread_join(idnode_tid, NULL);
  pthread_mutex_lock(&idnode_mutex);
  htsmsg_destroy(idnode_queue);
  idnode_queue = NULL;
  pthread_mutex_unlock(&idnode_mutex);  
  while ((il = RB_FIRST(&idclasses)) != NULL) {
    RB_REMOVE(&idclasses, il, link);
    free(il);
  }
  while ((il = RB_FIRST(&idrootclasses)) != NULL) {
    RB_REMOVE(&idrootclasses, il, link);
    free(il);
  }
  SKEL_FREE(idclasses_skel);
}

static const idclass_t *
idnode_root_class(const idclass_t *idc)
{
  while (idc && idc->ic_super)
    idc = idc->ic_super;
  return idc;
}

/**
 *
 */
int
idnode_insert(idnode_t *in, const char *uuid, const idclass_t *class, int flags)
{
  idnode_t *c;
  tvh_uuid_t u;
  int retries = 5;
  uint32_t u32;
  const idclass_t *idc;;

  lock_assert(&global_lock);

  in->in_class = class;
  do {

    if (uuid_init_bin(&u, uuid)) {
      in->in_class = NULL;
      return -1;
    }
    memcpy(in->in_uuid, u.bin, sizeof(in->in_uuid));

    c = NULL;
    if (flags & IDNODE_SHORT_UUID) {
      u32 = idnode_get_short_uuid(in);
      idc = idnode_root_class(in->in_class);
      RB_FOREACH(c, &idnodes, in_link) {
        if (idc != idnode_root_class(c->in_class))
          continue;
        if (idnode_get_short_uuid(c) == u32)
          break;
      }
    }

    if (c == NULL)
      c = RB_INSERT_SORTED(&idnodes, in, in_link, in_cmp);

  } while (c != NULL && --retries > 0);

  if(c != NULL) {
    fprintf(stderr, "Id node collision%s\n",
            (flags & IDNODE_SHORT_UUID) ? " (short)" : "");
    abort();
  }
  tvhtrace("idnode", "insert node %s", idnode_uuid_as_str(in));

  /* Register the class */
  idclass_register(class); // Note: we never actually unregister
  idclass_root_register(in);
  assert(in->in_domain);
  c = RB_INSERT_SORTED(in->in_domain, in, in_domain_link, in_cmp);
  assert(c == NULL);

  /* Fire event */
  idnode_notify_simple(in);

  return 0;
}

/**
 *
 */
void
idnode_unlink(idnode_t *in)
{
  lock_assert(&global_lock);
  RB_REMOVE(&idnodes, in, in_link);
  RB_REMOVE(in->in_domain, in, in_domain_link);
  tvhtrace("idnode", "unlink node %s", idnode_uuid_as_str(in));
  idnode_notify_simple(in);
}

/**
 *
 */
static void
idnode_handler(size_t off, idnode_t *in)
{
  void (**fcn)(idnode_t *);
  lock_assert(&global_lock);
  const idclass_t *idc = in->in_class;
  while (idc) {
    fcn = (void *)idc + off;
    if (*fcn) {
      (*fcn)(in);
      break;
    }
    idc = idc->ic_super;
  }
}

void
idnode_delete(idnode_t *in)
{
  return idnode_handler(offsetof(idclass_t, ic_delete), in);
}

void
idnode_moveup(idnode_t *in)
{
  return idnode_handler(offsetof(idclass_t, ic_moveup), in);
}

void
idnode_movedown(idnode_t *in)
{
  return idnode_handler(offsetof(idclass_t, ic_movedown), in);
}

/* **************************************************************************
 * Info
 * *************************************************************************/

uint32_t
idnode_get_short_uuid (const idnode_t *in)
{
  uint32_t u32;
  memcpy(&u32, in->in_uuid, sizeof(u32));
  return u32 & 0x7FFFFFFF; // compat needs to be +ve signed
}

/**
 *
 */
const char *
idnode_uuid_as_str(const idnode_t *in)
{
  static tvh_uuid_t __thread ret[16];
  static uint8_t p = 0;
  bin2hex(ret[p].hex, sizeof(ret[p].hex), in->in_uuid, sizeof(in->in_uuid));
  const char *s = ret[p].hex;
  p = (p + 1) % 16;
  return s;
}

/**
 *
 */
const char *
idnode_get_title(idnode_t *in)
{
  const idclass_t *ic = in->in_class;
  for(; ic != NULL; ic = ic->ic_super) {
    if(ic->ic_get_title != NULL)
      return ic->ic_get_title(in);
  }
  return idnode_uuid_as_str(in);
}


/**
 *
 */
idnode_set_t *
idnode_get_childs(idnode_t *in)
{
  if(in == NULL)
    return NULL;

  const idclass_t *ic = in->in_class;
  for(; ic != NULL; ic = ic->ic_super) {
    if(ic->ic_get_childs != NULL)
      return ic->ic_get_childs(in);
  }
  return NULL;
}

/**
 *
 */
int
idnode_is_leaf(idnode_t *in)
{
  const idclass_t *ic = in->in_class;
  for(; ic != NULL; ic = ic->ic_super) {
    if(ic->ic_get_childs != NULL)
      return 0;
  }
  return 1;
}

int
idnode_is_instance(idnode_t *in, const idclass_t *idc)
{
  const idclass_t *ic = in->in_class;
  for(; ic != NULL; ic = ic->ic_super) {
    if (ic == idc) return 1;
  }
  return 0;
}

/* **************************************************************************
 * Properties
 * *************************************************************************/

static const property_t *
idnode_find_prop
  ( idnode_t *self, const char *key )
{
  const idclass_t *idc = self->in_class;
  const property_t *p;
  while (idc) {
    if ((p = prop_find(idc->ic_properties, key))) return p;
    idc = idc->ic_super;
  }
  return NULL;
}

/*
 * Get display value
 */
static char *
idnode_get_display
  ( idnode_t *self, const property_t *p )
{
  if (p) {
    if (p->rend)
      return p->rend(self);
    if (p->islist) {
      htsmsg_t *l = (htsmsg_t*)p->get(self);
      if (l)
        return htsmsg_list_2_csv(l);
    }
  }
  return NULL;
}

/*
 * Get field as string
 */
const char *
idnode_get_str
  ( idnode_t *self, const char *key )
{
  const property_t *p = idnode_find_prop(self, key);
  if (p && p->type == PT_STR) {
    const void *ptr;
    if (p->get)
      ptr = p->get(self);
    else
      ptr = ((void*)self) + p->off;
    return *(const char**)ptr;
  }

  return NULL;
}

/*
 * Get field as unsigned int
 */
int
idnode_get_u32
  ( idnode_t *self, const char *key, uint32_t *u32 )
{
  const property_t *p = idnode_find_prop(self, key);
  if (p) {
    const void *ptr;
    if (p->islist)
      return 1;
    else if (p->get)
      ptr = p->get(self);
    else
      ptr = ((void*)self) + p->off;
    switch (p->type) {
      case PT_INT:
      case PT_BOOL:
        *u32 = *(int*)ptr;
        return 0;
      case PT_U16:
        *u32 = *(uint16_t*)ptr;
        return 0;
      case PT_U32:
        *u32 = *(uint32_t*)ptr;
        return 0;
      default:
        break;
    }
  }
  return 1;
}

/*
 * Get field as signed 64-bit int
 */
int
idnode_get_s64
  ( idnode_t *self, const char *key, int64_t *s64 )
{
  const property_t *p = idnode_find_prop(self, key);
  if (p) {
    const void *ptr;
    if (p->islist)
      return 1;
    else if (p->get)
      ptr = p->get(self);
    else
      ptr = ((void*)self) + p->off;
    switch (p->type) {
      case PT_INT:
      case PT_BOOL:
        *s64 = *(int*)ptr;
        return 0;
      case PT_U16:
        *s64 = *(uint16_t*)ptr;
        return 0;
      case PT_U32:
        *s64 = *(uint32_t*)ptr;
        return 0;
      case PT_S64:
        *s64 = *(int64_t*)ptr;
        return 0;
      case PT_DBL:
        *s64 = *(double*)ptr;
        return 0;
      case PT_TIME:
        *s64 = *(time_t*)ptr;
        return 0;
      default:
        break;
    }
  }
  return 1;
}

/*
 * Get field as double
 */
int
idnode_get_dbl
  ( idnode_t *self, const char *key, double *dbl )
{
  const property_t *p = idnode_find_prop(self, key);
  if (p) {
    const void *ptr;
    if (p->islist)
      return 1;
    else if (p->get)
      ptr = p->get(self);
    else
      ptr = ((void*)self) + p->off;
    switch (p->type) {
      case PT_INT:
      case PT_BOOL:
        *dbl = *(int*)ptr;
        return 0;
      case PT_U16:
        *dbl = *(uint16_t*)ptr;
        return 0;
      case PT_U32:
        *dbl = *(uint32_t*)ptr;
        return 0;
      case PT_S64:
        *dbl = *(int64_t*)ptr;
        return 0;
      case PT_DBL:
        *dbl = *(double *)ptr;
        return 0;
      case PT_TIME:
        *dbl = *(time_t*)ptr;
        return 0;
      default:
        break;
    }
  }
  return 1;
}

/*
 * Get field as BOOL
 */
int
idnode_get_bool
  ( idnode_t *self, const char *key, int *b )
{
  const property_t *p = idnode_find_prop(self, key);
 if (p) {
    const void *ptr;
    if (p->islist)
      return 1;
    else if (p->get)
      ptr = p->get(self);
    else
      ptr = ((void*)self) + p->off;
    switch (p->type) {
      case PT_BOOL:
        *b = *(int*)ptr;
        return 0;
      default:
        break;
    }
  }
  return 1; 
}

/*
 * Get field as time
 */
int
idnode_get_time
  ( idnode_t *self, const char *key, time_t *tm )
{
  const property_t *p = idnode_find_prop(self, key);
  if (p) {
    const void *ptr;
    if (p->islist)
      return 1;
    if (p->get)
      ptr = p->get(self);
    else
      ptr = ((void*)self) + p->off;
    switch (p->type) {
      case PT_TIME:
        *tm = *(time_t*)ptr;
        return 0;
      default:
        break;
    }
  }
  return 1;
}

/* **************************************************************************
 * Lookup
 * *************************************************************************/

/**
 *
 */
static const idnodes_rb_t *
idnode_domain(const idclass_t *idc)
{
  if (idc) {
    idclass_link_t lskel, *l;
    const idclass_t *root = idnode_root_class(idc);
    lskel.idc = root;
    l = RB_FIND(&idrootclasses, &lskel, link, ic_cmp);
    if (l == NULL)
      return NULL;
    return &l->nodes;
  } else {
    return NULL;
  }
}

void *
idnode_find ( const char *uuid, const idclass_t *idc, const idnodes_rb_t *domain )
{
  idnode_t skel, *r;

  tvhtrace("idnode", "find node %s class %s", uuid, idc ? idc->ic_class : NULL);
  if(uuid == NULL || strlen(uuid) != UUID_HEX_SIZE - 1)
    return NULL;
  if(hex2bin(skel.in_uuid, sizeof(skel.in_uuid), uuid))
    return NULL;
  if (domain == NULL)
    domain = idnode_domain(idc);
  if (domain == NULL)
    r = RB_FIND(&idnodes, &skel, in_link, in_cmp);
  else
    r = RB_FIND(domain, &skel, in_domain_link, in_cmp);
  if(r != NULL && idc != NULL) {
    const idclass_t *c = r->in_class;
    for(;c != NULL; c = c->ic_super) {
      if(idc == c)
        return r;
    }
    return NULL;
  }
  return r;
}

idnode_set_t *
idnode_find_all ( const idclass_t *idc, const idnodes_rb_t *domain )
{
  idnode_t *in;
  const idclass_t *ic;
  tvhtrace("idnode", "find class %s", idc->ic_class);
  idnode_set_t *is = calloc(1, sizeof(idnode_set_t));
  if (domain == NULL)
    domain = idnode_domain(idc);
  if (domain == NULL) {
    RB_FOREACH(in, &idnodes, in_link) {
      ic = in->in_class;
      while (ic) {
        if (ic == idc) {
          tvhtrace("idnode", "  add node %s", idnode_uuid_as_str(in));
          idnode_set_add(is, in, NULL);
          break;
        }
        ic = ic->ic_super;
      }
    }
  } else {
    RB_FOREACH(in, domain, in_domain_link) {
      ic = in->in_class;
      while (ic) {
        if (ic == idc) {
          tvhtrace("idnode", "  add node %s", idnode_uuid_as_str(in));
          idnode_set_add(is, in, NULL);
          break;
        }
        ic = ic->ic_super;
      }
    }
  }
  return is;
}

/* **************************************************************************
 * Set processing
 * *************************************************************************/

static int
idnode_cmp_title
  ( const void *a, const void *b )
{
  idnode_t      *ina  = *(idnode_t**)a;
  idnode_t      *inb  = *(idnode_t**)b;
  const char *sa = idnode_get_title(ina);
  const char *sb = idnode_get_title(inb);
  return strcmp(sa ?: "", sb ?: "");
}

#define safecmp(a, b) ((a) > (b) ? 1 : ((a) < (b) ? -1 : 0))

static int
idnode_cmp_sort
  ( const void *a, const void *b, void *s )
{
  idnode_t      *ina  = *(idnode_t**)a;
  idnode_t      *inb  = *(idnode_t**)b;
  idnode_sort_t *sort = s;
  const property_t *p = idnode_find_prop(ina, sort->key);
  if (!p) return 0;

  /* Get display string */
  if (p->islist || (p->list && !(p->opts & PO_SORTKEY))) {
    int r;
    char *stra = idnode_get_display(ina, p);
    char *strb = idnode_get_display(inb, p);
    if (sort->dir == IS_ASC)
      r = strcmp(stra ?: "", strb ?: "");
    else
      r = strcmp(strb ?: "", stra ?: "");
    free(stra);
    free(strb);
    return r;
  }

  switch (p->type) {
    case PT_STR:
      {
        int r;
        const char *stra = tvh_strdupa(idnode_get_str(ina, sort->key) ?: "");
        const char *strb = idnode_get_str(inb, sort->key) ?: "";
        if (sort->dir == IS_ASC)
          r = strcmp(stra, strb);
        else
          r = strcmp(strb, stra);
        return r;
      }
      break;
    case PT_INT:
    case PT_U16:
    case PT_BOOL:
    case PT_PERM:
      {
        int32_t i32a = 0, i32b = 0;
        idnode_get_u32(ina, sort->key, (uint32_t *)&i32a);
        idnode_get_u32(inb, sort->key, (uint32_t *)&i32b);
        if (sort->dir == IS_ASC)
          return safecmp(i32a, i32b);
        else
          return safecmp(i32b, i32a);
      }
      break;
    case PT_U32:
      {
        uint32_t u32a = 0, u32b = 0;
        idnode_get_u32(ina, sort->key, &u32a);
        idnode_get_u32(inb, sort->key, &u32b);
        if (sort->dir == IS_ASC)
          return safecmp(u32a, u32b);
        else
          return safecmp(u32b, u32a);
      }
      break;
    case PT_S64:
      {
        int64_t s64a = 0, s64b = 0;
        idnode_get_s64(ina, sort->key, &s64a);
        idnode_get_s64(inb, sort->key, &s64b);
        if (sort->dir == IS_ASC)
          return safecmp(s64a, s64b);
        else
          return safecmp(s64b, s64a);
      }
      break;
    case PT_DBL:
      {
        double dbla = 0, dblb = 0;
        idnode_get_dbl(ina, sort->key, &dbla);
        idnode_get_dbl(inb, sort->key, &dblb);
        if (sort->dir == IS_ASC)
          return safecmp(dbla, dblb);
        else
          return safecmp(dblb, dbla);
      }
      break;
    case PT_TIME:
      {
        time_t ta = 0, tb = 0;
        idnode_get_time(ina, sort->key, &ta);
        idnode_get_time(inb, sort->key, &tb);
        if (sort->dir == IS_ASC)
          return safecmp(ta, tb);
        else
          return safecmp(tb, ta);
      }
      break;
    case PT_LANGSTR:
      // TODO?
    case PT_NONE:
      break;
  }
  return 0;
}

static void
idnode_filter_init
  ( idnode_t *in, idnode_filter_t *filter )
{
  idnode_filter_ele_t *f;
  const property_t *p;

  LIST_FOREACH(f, filter, link) {
    if (f->type == IF_NUM) {
      p = idnode_find_prop(in, f->key);
      if (p) {
        if (p->type == PT_U32 || p->type == PT_S64 ||
            p->type == PT_TIME) {
          int64_t v = f->u.n.n;
          if (p->intsplit != f->u.n.intsplit) {
            v = (v / MIN(1, f->u.n.intsplit)) * p->intsplit;
            f->u.n.n = v;
          }
        }
      }
    }
    f->checked = 1;
  }
}

int
idnode_filter
  ( idnode_t *in, idnode_filter_t *filter )
{
  idnode_filter_ele_t *f;
  
  LIST_FOREACH(f, filter, link) {
    if (!f->checked)
      idnode_filter_init(in, filter);
    if (f->type == IF_STR) {
      const char *str;
      char *strdisp;
      int r = 1;
      str = strdisp = idnode_get_display(in, idnode_find_prop(in, f->key));
      if (!str)
        if (!(str = idnode_get_str(in, f->key)))
          return 1;
      switch(f->comp) {
        case IC_IN: r = strstr(str, f->u.s) == NULL; break;
        case IC_EQ: r = strcmp(str, f->u.s) != 0; break;
        case IC_LT: r = strcmp(str, f->u.s) > 0; break;
        case IC_GT: r = strcmp(str, f->u.s) < 0; break;
        case IC_RE: r = !!regexec(&f->u.re, str, 0, NULL, 0); break;
      }
      if (strdisp)
        free(strdisp);
      return r;
    } else if (f->type == IF_NUM || f->type == IF_BOOL) {
      int64_t a, b;
      if (idnode_get_s64(in, f->key, &a))
        return 1;
      b = (f->type == IF_NUM) ? f->u.n.n : f->u.b;
      switch (f->comp) {
        case IC_IN:
        case IC_RE:
          break; // Note: invalid
        case IC_EQ:
          if (a != b)
            return 1;
          break;
        case IC_LT:
          if (a > b)
            return 1;
          break;
        case IC_GT:
          if (a < b)
            return 1;
          break;
      }
    } else if (f->type == IF_DBL) {
      double a, b;
      if (idnode_get_dbl(in, f->key, &a))
        return 1;
      b = f->u.dbl;
      switch (f->comp) {
        case IC_IN:
        case IC_RE:
          break; // Note: invalid
        case IC_EQ:
          if (a != b)
            return 1;
          break;
        case IC_LT:
          if (a > b)
            return 1;
          break;
        case IC_GT:
          if (a < b)
            return 1;
          break;
      }
    }
  }

  return 0;
}

void
idnode_filter_add_str
  ( idnode_filter_t *filt, const char *key, const char *val, int comp )
{
  idnode_filter_ele_t *ele = calloc(1, sizeof(idnode_filter_ele_t));
  ele->key  = strdup(key);
  ele->type = IF_STR;
  ele->comp = comp;
  if (comp == IC_RE) {
    if (regcomp(&ele->u.re, val, REG_ICASE | REG_EXTENDED | REG_NOSUB)) {
      free(ele);
      return;
    }
  } else
    ele->u.s  = strdup(val);
  LIST_INSERT_HEAD(filt, ele, link);
}

void
idnode_filter_add_num
  ( idnode_filter_t *filt, const char *key, int64_t val, int comp, int64_t intsplit )
{
  idnode_filter_ele_t *ele = calloc(1, sizeof(idnode_filter_ele_t));
  ele->key  = strdup(key);
  ele->type = IF_NUM;
  ele->comp = comp;
  ele->u.n.n = val;
  ele->u.n.intsplit = intsplit;
  LIST_INSERT_HEAD(filt, ele, link);
}

void
idnode_filter_add_dbl
  ( idnode_filter_t *filt, const char *key, double dbl, int comp )
{
  idnode_filter_ele_t *ele = calloc(1, sizeof(idnode_filter_ele_t));
  ele->key   = strdup(key);
  ele->type  = IF_DBL;
  ele->comp  = comp;
  ele->u.dbl = dbl;
  LIST_INSERT_HEAD(filt, ele, link);
}

void
idnode_filter_add_bool
  ( idnode_filter_t *filt, const char *key, int val, int comp )
{
  idnode_filter_ele_t *ele = calloc(1, sizeof(idnode_filter_ele_t));
  ele->key  = strdup(key);
  ele->type = IF_BOOL;
  ele->comp = comp;
  ele->u.b  = val;
  LIST_INSERT_HEAD(filt, ele, link);
}

void
idnode_filter_clear
  ( idnode_filter_t *filt )
{
  idnode_filter_ele_t *ele;
  while ((ele = LIST_FIRST(filt))) {
    LIST_REMOVE(ele, link);
    if (ele->type == IF_STR) {
      if (ele->comp == IC_RE)
        regfree(&ele->u.re);
      else
        free(ele->u.s);
    }
    free(ele->key);
    free(ele);
  }
}

void
idnode_set_add
  ( idnode_set_t *is, idnode_t *in, idnode_filter_t *filt )
{
  if (filt && idnode_filter(in, filt))
    return;
  
  /* Allocate more space */
  if (is->is_alloc == is->is_count) {
    is->is_alloc = MAX(100, is->is_alloc * 2);
    is->is_array = realloc(is->is_array, is->is_alloc * sizeof(idnode_t*));
  }
  is->is_array[is->is_count++] = in;
}

int
idnode_set_exists
  ( idnode_set_t *is, idnode_t * in )
{
  int i;
  for (i = 0; i < is->is_count; i++)
    if (memcmp(is->is_array[i]->in_uuid, in->in_uuid, sizeof(in->in_uuid)) == 0)
      return 1;
  return 0;
}

void
idnode_set_sort
  ( idnode_set_t *is, idnode_sort_t *sort )
{
  tvh_qsort_r(is->is_array, is->is_count, sizeof(idnode_t*), idnode_cmp_sort, (void*)sort);
}

void
idnode_set_sort_by_title
  ( idnode_set_t *is )
{
  qsort(is->is_array, is->is_count, sizeof(idnode_t*), idnode_cmp_title);
}

htsmsg_t *
idnode_set_as_htsmsg
  ( idnode_set_t *is )
{
  htsmsg_t *l = htsmsg_create_list();
  int i;
  for (i = 0; i < is->is_count; i++)
    htsmsg_add_str(l, NULL, idnode_uuid_as_str(is->is_array[i]));
  return l;
}

void
idnode_set_free ( idnode_set_t *is )
{
  free(is->is_array);
  free(is);
}

/* **************************************************************************
 * Write
 * *************************************************************************/

static int
idnode_class_write_values
  ( idnode_t *self, const idclass_t *idc, htsmsg_t *c, int optmask )
{
  int save = 0;
  if (idc->ic_super)
    save |= idnode_class_write_values(self, idc->ic_super, c, optmask);
  save |= prop_write_values(self, idc->ic_properties, c, optmask, NULL);
  return save;
}

static void
idnode_savefn ( idnode_t *self )
{
  const idclass_t *idc = self->in_class;
  while (idc) {
    if (idc->ic_save) {
      idc->ic_save(self);
      break;
    }
    idc = idc->ic_super;
  }
}

int
idnode_write0 ( idnode_t *self, htsmsg_t *c, int optmask, int dosave )
{
  int save = 0;
  const idclass_t *idc = self->in_class;
  save = idnode_class_write_values(self, idc, c, optmask);
  if (save && dosave)
    idnode_savefn(self);
  if (dosave)
    idnode_notify_simple(self);
  // Note: always output event if "dosave", reason is that UI updates on
  //       these, but there are some subtle cases where it will expect
  //       an update and not get one. This include fields being set for
  //       which there is user-configurable value and auto fallback so
  //       the UI state might not atually reflect the user config
  return save;
}

void
idnode_changed( idnode_t *self )
{
  idnode_notify_simple(self);
  idnode_savefn(self);
}

/* **************************************************************************
 * Read
 * *************************************************************************/

void
idnode_read0 ( idnode_t *self, htsmsg_t *c, htsmsg_t *list, int optmask )
{
  const idclass_t *idc = self->in_class;
  for (; idc; idc = idc->ic_super)
    prop_read_values(self, idc->ic_properties, c, list, optmask);
}

/**
 * Recursive to get superclass nodes first
 */
static void
add_params
  (struct idnode *self, const idclass_t *ic, htsmsg_t *p, htsmsg_t *list, int optmask)
{
  /* Parent first */
  if(ic->ic_super != NULL)
    add_params(self, ic->ic_super, p, list, optmask);

  /* Seperator (if not empty) */
#if 0
  if(TAILQ_FIRST(&p->hm_fields) != NULL) {
    htsmsg_t *m = htsmsg_create_map();
    htsmsg_add_str(m, "caption",  ic->ic_caption ?: ic->ic_class);
    htsmsg_add_str(m, "type",     "separator");
    htsmsg_add_msg(p, NULL, m);
  }
#endif

  /* Properties */
  prop_serialize(self, ic->ic_properties, p, list, optmask);
}

static htsmsg_t *
idnode_params (const idclass_t *idc, idnode_t *self, htsmsg_t *list, int optmask)
{
  htsmsg_t *p  = htsmsg_create_list();
  add_params(self, idc, p, list, optmask);
  return p;
}

static const char *
idclass_get_caption (const idclass_t *idc )
{
  while (idc) {
    if (idc->ic_caption)
      return idc->ic_caption;
    idc = idc->ic_super;
  }
  return NULL;
}

static const char *
idclass_get_class (const idclass_t *idc)
{
  while (idc) {
    if (idc->ic_class)
      return idc->ic_class;
    idc = idc->ic_super;
  }
  return NULL;
}

static const char *
idclass_get_event (const idclass_t *idc)
{
  while (idc) {
    if (idc->ic_event)
      return idc->ic_event;
    idc = idc->ic_super;
  }
  return NULL;
}

static const char *
idclass_get_order (const idclass_t *idc)
{
  while (idc) {
    if (idc->ic_order)
      return idc->ic_order;
    idc = idc->ic_super;
  }
  return NULL;
}

static htsmsg_t *
idclass_get_property_groups (const idclass_t *idc)
{
  const property_group_t *g;
  htsmsg_t *e, *m;
  int count;
  while (idc) {
    if (idc->ic_groups) {
      m = htsmsg_create_list();
      count = 0;
      for (g = idc->ic_groups; g->number && g->name; g++) {
        e = htsmsg_create_map();
        htsmsg_add_u32(e, "number", g->number);
        htsmsg_add_str(e, "name",   g->name);
        if (g->parent)
          htsmsg_add_u32(e, "parent", g->parent);
        if (g->column)
          htsmsg_add_u32(e, "column", g->column);
        htsmsg_add_msg(m, NULL, e);
        count++;
      }
      if (count)
        return m;
      htsmsg_destroy(m);
      break;
    }
    idc = idc->ic_super;
  }
  return NULL;
}

void
idclass_register(const idclass_t *idc)
{
  while (idc) {
    SKEL_ALLOC(idclasses_skel);
    idclasses_skel->idc = idc;
    if (RB_INSERT_SORTED(&idclasses, idclasses_skel, link, ic_cmp))
      break;
    RB_INIT(&idclasses_skel->nodes); /* not used, but for sure */
    SKEL_USED(idclasses_skel);
    tvhtrace("idnode", "register class %s", idc->ic_class);
    idc = idc->ic_super;
  }
}

static void
idclass_root_register(idnode_t *in)
{
  const idclass_t *idc = in->in_class;
  idclass_link_t *r;
  idc = idnode_root_class(idc);
  SKEL_ALLOC(idclasses_skel);
  idclasses_skel->idc = idc;
  r = RB_INSERT_SORTED(&idrootclasses, idclasses_skel, link, ic_cmp);
  if (r) {
    in->in_domain = &r->nodes;
    return;
  }
  RB_INIT(&idclasses_skel->nodes);
  r = idclasses_skel;
  SKEL_USED(idclasses_skel);
  tvhtrace("idnode", "register root class %s", idc->ic_class);
  in->in_domain = &r->nodes;
}

const idclass_t *
idclass_find ( const char *class )
{
  idclass_link_t *t, skel;
  idclass_t idc;
  skel.idc = &idc;
  idc.ic_class = class;
  tvhtrace("idnode", "find class %s", class);
  t = RB_FIND(&idclasses, &skel, link, ic_cmp);
  return t ? t->idc : NULL;
}

/*
 * Just get the class definition
 */
htsmsg_t *
idclass_serialize0(const idclass_t *idc, htsmsg_t *list, int optmask)
{
  const char *s;
  htsmsg_t *p, *m = htsmsg_create_map();

  /* Caption and name */
  if ((s = idclass_get_caption(idc)))
    htsmsg_add_str(m, "caption", s);
  if ((s = idclass_get_class(idc)))
    htsmsg_add_str(m, "class", s);
  if ((s = idclass_get_event(idc)))
    htsmsg_add_str(m, "event", s);
  if ((s = idclass_get_order(idc)))
    htsmsg_add_str(m, "order", s);
  if ((p = idclass_get_property_groups(idc)))
    htsmsg_add_msg(m, "groups", p);

  /* Props */
  if ((p = idnode_params(idc, NULL, list, optmask)))
    htsmsg_add_msg(m, "props", p);
  
  return m;
}

/**
 *
 */
htsmsg_t *
idnode_serialize0(idnode_t *self, htsmsg_t *list, int optmask)
{
  const idclass_t *idc = self->in_class;
  const char *uuid, *s;

  htsmsg_t *m = htsmsg_create_map();
  uuid = idnode_uuid_as_str(self);
  htsmsg_add_str(m, "uuid", uuid);
  htsmsg_add_str(m, "id",   uuid);
  htsmsg_add_str(m, "text", idnode_get_title(self) ?: "");
  if ((s = idclass_get_caption(idc)))
    htsmsg_add_str(m, "caption", s);
  if ((s = idclass_get_class(idc)))
    htsmsg_add_str(m, "class", s);
  if ((s = idclass_get_event(idc)))
    htsmsg_add_str(m, "event", s);

  htsmsg_add_msg(m, "params", idnode_params(idc, self, list, optmask));

  return m;
}

/* **************************************************************************
 * Notification
 * *************************************************************************/

/**
 * Delayed notification
 */
static void
idnode_notify_delayed ( idnode_t *in, const char *uuid, const char *event )
{
  pthread_mutex_lock(&idnode_mutex);
  if (!idnode_queue)
    idnode_queue = htsmsg_create_map();
  htsmsg_set_str(idnode_queue, uuid, event);
  pthread_cond_signal(&idnode_cond);
  pthread_mutex_unlock(&idnode_mutex);
}

/**
 * Update internal event pipes
 */
static void
idnode_notify_event ( idnode_t *in )
{
  const idclass_t *ic = in->in_class;
  const char *uuid = idnode_uuid_as_str(in);
  while (ic) {
    if (ic->ic_event)
      idnode_notify_delayed(in, uuid, ic->ic_event);
    ic = ic->ic_super;
  }
}

/**
 * Notify on a given channel
 */
void
idnode_notify
  (idnode_t *in, int event)
{
  const char *uuid = idnode_uuid_as_str(in);

  if (!tvheadend_running)
    return;

  /* Immediate */
  if (!event) {

    const idclass_t *ic = in->in_class;

    while (ic) {
      if (ic->ic_event) {
        htsmsg_t *m = htsmsg_create_map();
        htsmsg_add_str(m, "uuid", uuid);
        notify_by_msg(ic->ic_event, m);
      }
      ic = ic->ic_super;
    }
  
  /* Rate-limited */
  } else {
    idnode_notify_event(in);
  }
}

void
idnode_notify_simple (void *in)
{
  idnode_notify(in, 1);
}

void
idnode_notify_title_changed (void *in)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "uuid", idnode_uuid_as_str(in));
  htsmsg_add_str(m, "text", idnode_get_title(in));
  notify_by_msg("title", m);
  idnode_notify_event(in);
}

/*
 * Thread for handling notifications
 */
void*
idnode_thread ( void *p )
{
  idnode_t *node;
  htsmsg_t *m, *q = NULL;
  htsmsg_field_t *f;
  const char *event;

  pthread_mutex_lock(&idnode_mutex);

  while (tvheadend_running) {

    /* Get queue */
    if (!idnode_queue) {
      pthread_cond_wait(&idnode_cond, &idnode_mutex);
      continue;
    }
    q            = idnode_queue;
    idnode_queue = NULL;
    pthread_mutex_unlock(&idnode_mutex);

    /* Process */
    pthread_mutex_lock(&global_lock);

    HTSMSG_FOREACH(f, q) {
      node  = idnode_find(f->hmf_name, NULL, NULL);
      event = htsmsg_field_get_str(f);
      m     = htsmsg_create_map();
      htsmsg_add_str(m, "uuid", f->hmf_name);
      if (!node)
        htsmsg_add_u32(m, "removed", 1);
      notify_by_msg(event, m);
    }
    
    /* Finished */
    pthread_mutex_unlock(&global_lock);
    htsmsg_destroy(q);

    /* Wait */
    usleep(500000);
    pthread_mutex_lock(&idnode_mutex);
  }
  pthread_mutex_unlock(&idnode_mutex);
  
  return NULL;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
