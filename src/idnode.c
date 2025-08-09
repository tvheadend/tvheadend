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
#include "access.h"

static const idnodes_rb_t * idnode_domain ( const idclass_t *idc );
static idnodes_rb_t * idclass_find_domain ( const idclass_t *idc );

typedef struct idclass_link
{
  const idclass_t       *idc;
  idnodes_rb_t           nodes;
  RB_ENTRY(idclass_link) link;
} idclass_link_t;

tvh_mutex_t                     idnode_mutex;
static idnodes_rb_t             idnodes;
static RB_HEAD(,idclass_link)   idclasses;
static RB_HEAD(,idclass_link)   idrootclasses;
static TAILQ_HEAD(,idnode_save) idnodes_save;

static tvh_cond_t save_cond;
static pthread_t  save_tid;
static int        save_running;
static mtimer_t   save_timer;

static tvh_mutex_t idnode_lnotify_mutex = TVH_THREAD_MUTEX_INITIALIZER;
static tvh_uuid_set_t  idnode_lnotify_set;
static tvh_uuid_set_t  idnode_lnotify_title_set;

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
  return uuid_cmp(&a->in_uuid, &b->in_uuid);
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
  const idclass_t *idc;
  char ubuf[UUID_HEX_SIZE];

  lock_assert(&global_lock);

  idnode_lock();

  in->in_class = class;
  do {

    if (uuid_set(&u, uuid)) {
      in->in_class = NULL;
      return -1;
    }
    uuid_duplicate(&in->in_uuid, &u);

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
    tvherror(LS_IDNODE, "Id node collission (%s) %s",
             uuid, (flags & IDNODE_SHORT_UUID) ? " (short)" : "");
    fprintf(stderr, "Id node collision (%s) %s\n",
            uuid, (flags & IDNODE_SHORT_UUID) ? " (short)" : "");
    abort();
  }
  tvhtrace(LS_IDNODE, "insert node %s", idnode_uuid_as_str(in, ubuf));

  /* Register the class */
  in->in_domain = idclass_find_domain(class);
  if (in->in_domain == NULL) {
    tvherror(LS_IDNODE, "class '%s' is not registered", class->ic_class);
    abort();
  }
  c = RB_INSERT_SORTED(in->in_domain, in, in_domain_link, in_cmp);
  assert(c == NULL);

  idnode_unlock();

  /* Fire event */
  idnode_notify(in, "create");

  return 0;
}

/**
 *
 */
void
idnode_unlink(idnode_t *in)
{
  char ubuf[UUID_HEX_SIZE];

  lock_assert(&global_lock);
  idnode_lock();
  RB_REMOVE(&idnodes, in, in_link);
  RB_REMOVE(in->in_domain, in, in_domain_link);
  idnode_unlock();
  tvhtrace(LS_IDNODE, "unlink node %s", idnode_uuid_as_str(in, ubuf));
  idnode_notify(in, "delete");
  assert(in->in_save == NULL || in->in_save == SAVEPTR_OUTOFSERVICE);
}

/**
 *
 */
static void
idnode_handler(size_t off, idnode_t *in, const char *action)
{
  void (**fcn)(idnode_t *);
  lock_assert(&global_lock);
  const idclass_t *idc = in->in_class;
  while (idc) {
    fcn = (void *)idc + off;
    if (*fcn) {
      if (action)
        idnode_notify(in, action);
      (*fcn)(in);
      break;
    }
    idc = idc->ic_super;
  }
}

void
idnode_delete(idnode_t *in)
{
  idnode_handler(offsetof(idclass_t, ic_delete), in, NULL);
}

void
idnode_moveup(idnode_t *in)
{
  return idnode_handler(offsetof(idclass_t, ic_moveup), in, "moveup");
}

void
idnode_movedown(idnode_t *in)
{
  return idnode_handler(offsetof(idclass_t, ic_movedown), in, "movedown");
}

/* **************************************************************************
 * Info
 * *************************************************************************/

uint32_t
idnode_get_short_uuid (const idnode_t *in)
{
  uint32_t u32;
  memcpy(&u32, in->in_uuid.bin, sizeof(u32));
  return u32 & 0x7FFFFFFF; // compat needs to be +ve signed
}

/**
 *
 */
const char *
idnode_uuid_as_str(const idnode_t *in, char *uuid)
{
  return bin2hex(uuid, UUID_HEX_SIZE, in->in_uuid.bin, sizeof(in->in_uuid.bin));
}

/**
 *
 */
const char *
idnode_get_title(idnode_t *in, const char *lang, char *dst, size_t dstsize)
{
  static char ubuf[UUID_HEX_SIZE];
  const idclass_t *ic = in->in_class;
  for(; ic != NULL; ic = ic->ic_super) {
    if(ic->ic_get_title != NULL) {
      ic->ic_get_title(in, lang, dst, dstsize);
      return dst;
    }
  }
  strlcpy(dst, idnode_uuid_as_str(in, ubuf), dstsize);
  return dst;
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
  ( idnode_t *self, const property_t *p, const char *lang )
{
  char *r = NULL;
  if (p) {
    if (p->rend)
      r = p->rend(self, lang);
    else if (p->islist) {
      htsmsg_t *l = (htsmsg_t*)p->get(self);
      if (l) {
        r = htsmsg_list_2_csv(l, ',', 1);
        htsmsg_destroy(l);
      }
    } else if (p->list) {
      htsmsg_t *l = p->list(self, lang), *m;
      htsmsg_field_t *f;
      int32_t k, v;
      const char *s;
      if (l && !idnode_get_u32(self, p->id, (uint32_t *)&v))
        HTSMSG_FOREACH(f, l) {
          m = htsmsg_field_get_map(f);
          if (!htsmsg_get_s32(m, "key", &k) &&
              (s = htsmsg_get_str(m, "val")) != NULL &&
              v == k) {
            r = strdup(s);
            break;
          }
        }
      htsmsg_destroy(l);
    }
  }
  return r;
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
      case PT_DYN_INT:
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
      case PT_DYN_INT:
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
 * Get field as signed 64-bit int
 */
int
idnode_get_s64_atomic
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
      case PT_S64_ATOMIC:
        *s64 = atomic_get_s64((int64_t*)ptr);
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
      case PT_DYN_INT:
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

int
idnode_perm(idnode_t *self, struct access *a, htsmsg_t *msg_to_write)
{
  const idclass_t *ic = self->in_class;
  int r;

  while (ic) {
    if (ic->ic_perm)
      r = self->in_class->ic_perm(self, a, msg_to_write);
    else if (ic->ic_perm_def)
      r = access_verify2(a, ic->ic_perm_def);
    else {
      ic = ic->ic_super;
      continue;
    }
    if (!r) {
      self->in_access = a;
      return 0;
    }
    return r;
  }
  return 0;
}

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

static idnode_t *
idnode_find_ ( idnode_t *skel, const idclass_t *idc, const idnodes_rb_t *domain )
{
  idnode_t *r;

  if (domain == NULL)
    domain = idnode_domain(idc);
  if (domain == NULL)
    r = RB_FIND(&idnodes, skel, in_link, in_cmp);
  else
    r = RB_FIND(domain, skel, in_domain_link, in_cmp);
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

void *
idnode_find0 ( tvh_uuid_t *uuid, const idclass_t *idc, const idnodes_rb_t *domain )
{
  char buf[UUID_HEX_SIZE];
  idnode_t skel;
  skel.in_uuid = *uuid;
  tvhtrace(LS_IDNODE, "find node %s class %s",
           uuid_get_hex(uuid, buf), idc ? idc->ic_class : NULL);
  return idnode_find_(&skel, idc, domain);
}

void *
idnode_find ( const char *uuid, const idclass_t *idc, const idnodes_rb_t *domain )
{
  idnode_t skel;
  tvhtrace(LS_IDNODE, "find node %s class %s", uuid, idc ? idc->ic_class : NULL);
  if(uuid == NULL || strlen(uuid) != UUID_HEX_SIZE - 1)
    return NULL;
  if(hex2bin(skel.in_uuid.bin, sizeof(skel.in_uuid.bin), uuid))
    return NULL;
  return idnode_find_(&skel, idc, domain);
}

idnode_set_t *
idnode_find_all ( const idclass_t *idc, const idnodes_rb_t *domain )
{
  idnode_t *in;
  const idclass_t *ic;
  char ubuf[UUID_HEX_SIZE];
  tvhtrace(LS_IDNODE, "find class %s", idc->ic_class);
  idnode_set_t *is = calloc(1, sizeof(idnode_set_t));
  if (domain == NULL)
    domain = idnode_domain(idc);
  if (domain == NULL) {
    RB_FOREACH(in, &idnodes, in_link) {
      ic = in->in_class;
      while (ic) {
        if (ic == idc) {
          tvhtrace(LS_IDNODE, "  add node %s", idnode_uuid_as_str(in, ubuf));
          idnode_set_add(is, in, NULL, NULL);
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
          tvhtrace(LS_IDNODE, "  add node %s", idnode_uuid_as_str(in, ubuf));
          idnode_set_add(is, in, NULL, NULL);
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
  ( const void *a, const void *b, void *lang )
{
  idnode_t      *ina  = *(idnode_t**)a;
  idnode_t      *inb  = *(idnode_t**)b;
  char bufa[384];
  char bufb[384];
  idnode_get_title(ina, (const char *)lang, bufa, sizeof(bufa));
  idnode_get_title(inb, (const char *)lang, bufb, sizeof(bufb));
  return strcmp(bufa, bufb);
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
    char *stra = idnode_get_display(ina, p, sort->lang);
    char *strb = idnode_get_display(inb, p, sort->lang);
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
    case PT_DYN_INT:
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
    case PT_S64_ATOMIC:
      {
        int64_t s64a = 0, s64b = 0;
        idnode_get_s64_atomic(ina, sort->key, &s64a);
        idnode_get_s64_atomic(inb, sort->key, &s64b);
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
          if (INTEXTRA_IS_SPLIT(p->intextra) && p->intextra != f->u.n.intsplit) {
            v = (v / MAX(1, f->u.n.intsplit)) * p->intextra;
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
  ( idnode_t *in, idnode_filter_t *filter, const char *lang )
{
  idnode_filter_ele_t *f;
  
  LIST_FOREACH(f, filter, link) {
    if (!f->checked)
      idnode_filter_init(in, filter);
    if (f->type == IF_STR) {
      const char *str;
      char *strdisp;
      int r = 1;
      str = strdisp = idnode_get_display(in, idnode_find_prop(in, f->key), lang);
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
      if (r)
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
      free(ele->key);
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
idnode_set_alloc
  ( idnode_set_t *is, size_t alloc )
{
  if (is->is_alloc < alloc) {
    is->is_alloc = alloc;
    is->is_array = realloc(is->is_array, alloc * sizeof(idnode_t*));
  }
}

void
idnode_set_add
  ( idnode_set_t *is, idnode_t *in, idnode_filter_t *filt, const char *lang )
{
  if (filt && idnode_filter(in, filt, lang))
    return;
  
  /* Allocate more space */
  if (is->is_alloc == is->is_count)
    idnode_set_alloc(is, MAX(100, is->is_alloc * 2));
  if (is->is_sorted) {
    size_t i;
    idnode_t **a = is->is_array;
    for (i = is->is_count++; i > 0 && a[i - 1] > in; i--)
      a[i] = a[i - 1];
    a[i] = in;
  } else {
    is->is_array[is->is_count++] = in;
  }
}

ssize_t
idnode_set_find_index
  ( idnode_set_t *is, idnode_t *in )
{
  ssize_t i;

  if (is->is_sorted) {
    idnode_t **a = is->is_array;
    ssize_t first = 0, last = is->is_count - 1;
    i = last / 2;
    while (first <= last) {
      if (a[i] < in)
        first = i + 1;
      else if (a[i] == in)
        return i;
      else
        last = i - 1;
      i = (first + last) / 2;
    }
  } else {
    for (i = 0; i < is->is_count; i++)
      if (is->is_array[i] == in)
        return 1;
  }
  return -1;
}

int
idnode_set_remove
  ( idnode_set_t *is, idnode_t *in )
{
  ssize_t i = idnode_set_find_index(is, in);
  if (i >= 0) {
    if (is->is_count > 1)
      memmove(&is->is_array[i], &is->is_array[i+1],
              (is->is_count - i - 1) * sizeof(idnode_t *));
    is->is_count--;
    return 1;
  }
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
  ( idnode_set_t *is, const char *lang )
{
  tvh_qsort_r(is->is_array, is->is_count, sizeof(idnode_t*), idnode_cmp_title, (void*)lang);
}

htsmsg_t *
idnode_set_as_htsmsg
  ( idnode_set_t *is )
{
  htsmsg_t *l = htsmsg_create_list();
  int i;
  for (i = 0; i < is->is_count; i++)
    htsmsg_add_uuid(l, NULL, &is->is_array[i]->in_uuid);
  return l;
}

void
idnode_set_clear ( idnode_set_t *is )
{
  free(is->is_array);
  is->is_array = NULL;
  is->is_count = is->is_alloc = 0;
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
idnode_changedfn ( idnode_t *self )
{
  const idclass_t *idc = self->in_class;
  while (idc) {
    if (idc->ic_changed) {
      idc->ic_changed(self);
      break;
    }
    idc = idc->ic_super;
  }
}

htsmsg_t *
idnode_savefn ( idnode_t *self, char *filename, size_t fsize )
{
  const idclass_t *idc = self->in_class;
  while (idc) {
    if (idc->ic_save)
      return idc->ic_save(self, filename, fsize);
    idc = idc->ic_super;
  }
  return NULL;
}

void
idnode_loadfn ( idnode_t *self, htsmsg_t *conf )
{
  const idclass_t *idc = self->in_class;
  while (idc) {
    if (idc->ic_load) {
      idc->ic_load(self, conf);
      return;
    }
    idc = idc->ic_super;
  }
  idnode_load(self, conf);
}

static void
idnode_save_trigger_thread_cb( void *aux )
{
  tvh_cond_signal(&save_cond, 0);
}

static void
idnode_save_queue ( idnode_t *self )
{
  idnode_save_t *ise;

  if (self->in_save)
    return;
  ise = malloc(sizeof(*ise));
  ise->ise_node = self;
  ise->ise_reqtime = mclk();
  if (TAILQ_EMPTY(&idnodes_save) && atomic_get(&save_running))
    mtimer_arm_rel(&save_timer, idnode_save_trigger_thread_cb, NULL, IDNODE_SAVE_DELAY);
  TAILQ_INSERT_TAIL(&idnodes_save, ise, ise_link);
  self->in_save = ise;
}

void
idnode_save_check ( idnode_t *self, int weak )
{
  char filename[PATH_MAX];
  htsmsg_t *m;

  if (self->in_save == NULL || self->in_save == SAVEPTR_OUTOFSERVICE)
    return;

  TAILQ_REMOVE(&idnodes_save, self->in_save, ise_link);
  free(self->in_save);
  self->in_save = SAVEPTR_OUTOFSERVICE;

  if (weak)
    return;

  m = idnode_savefn(self, filename, sizeof(filename));
  if (m) {
    hts_settings_save(m, "%s", filename);
    htsmsg_destroy(m);
  }
}

int
idnode_write0 ( idnode_t *self, htsmsg_t *c, int optmask, int dosave )
{
  int save = 0;
  const idclass_t *idc = self->in_class;
  save = idnode_class_write_values(self, idc, c, optmask);
  if ((idc->ic_flags & IDCLASS_ALWAYS_SAVE) != 0 || (save && dosave)) {
    idnode_changedfn(self);
    idnode_save_queue(self);
  }
  if (dosave)
    idnode_notify_changed(self);
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
  idnode_notify_changed(self);
  idnode_changedfn(self);
  idnode_save_queue(self);
}

/* **************************************************************************
 * Read
 * *************************************************************************/

void
idnode_read0 ( idnode_t *self, htsmsg_t *c, htsmsg_t *list,
               int optmask, const char *lang )
{
  const idclass_t *idc = self->in_class;
  for (; idc; idc = idc->ic_super)
    prop_read_values(self, idc->ic_properties, c, list, optmask, lang);
}

/**
 * Recursive to get superclass nodes first
 */
static void
add_params
  (struct idnode *self, const idclass_t *ic, htsmsg_t *p, htsmsg_t *list,
   int optmask, const char *lang)
{
  /* Parent first */
  if(ic->ic_super != NULL)
    add_params(self, ic->ic_super, p, list, optmask, lang);

  /* Seperator (if not empty) */
#if 0
  if(TAILQ_FIRST(&p->hm_fields) != NULL) {
    htsmsg_t *m = htsmsg_create_map();
    htsmsg_add_str(m, "caption",  tvh_gettext_lang(lang, ic->ic_caption) ?: ic->ic_class);
    htsmsg_add_str(m, "type",     "separator");
    htsmsg_add_msg(p, NULL, m);
  }
#endif

  /* Properties */
  prop_serialize(self, ic->ic_properties, p, list, optmask, lang);
}

static htsmsg_t *
idnode_params
  (const idclass_t *idc, idnode_t *self, htsmsg_t *list,
   int optmask, const char *lang)
{
  htsmsg_t *p  = htsmsg_create_list();
  add_params(self, idc, p, list, optmask, lang);
  return p;
}

const char *
idclass_get_caption (const idclass_t *idc, const char *lang)
{
  while (idc) {
    if (idc->ic_caption)
      return tvh_gettext_lang(lang, idc->ic_caption);
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

const char **
idclass_get_doc (const idclass_t *idc)
{
  while (idc) {
    if (idc->ic_doc)
      return idc->ic_doc;
    idc = idc->ic_super;
  }
  return NULL;
}

static htsmsg_t *
idclass_get_property_groups (const idclass_t *idc, const char *lang)
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
        htsmsg_add_str(e, "name",   tvh_gettext_lang(lang, g->name));
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

static idnodes_rb_t *
idclass_find_domain(const idclass_t *idc)
{
  idclass_link_t *r;
  idc = idnode_root_class(idc);
  SKEL_ALLOC(idclasses_skel);
  idclasses_skel->idc = idc;
  r = RB_FIND(&idrootclasses, idclasses_skel, link, ic_cmp);
  if (r)
    return &r->nodes;
  return NULL;
}

static void
idclass_root_register(const idclass_t *idc)
{
  idclass_link_t *r;
  SKEL_ALLOC(idclasses_skel);
  idclasses_skel->idc = idc;
  r = RB_INSERT_SORTED(&idrootclasses, idclasses_skel, link, ic_cmp);
  if (r) return;
  RB_INIT(&idclasses_skel->nodes);
  SKEL_USED(idclasses_skel);
  tvhtrace(LS_IDNODE, "register root class %s", idc->ic_class);
}

void
idclass_register(const idclass_t *idc)
{
  const idclass_t *prev = NULL;
  while (idc) {
    SKEL_ALLOC(idclasses_skel);
    idclasses_skel->idc = idc;
    if (RB_INSERT_SORTED(&idclasses, idclasses_skel, link, ic_cmp)) {
      prev = NULL;
      break;
    }
    RB_INIT(&idclasses_skel->nodes); /* not used, but for sure */
    SKEL_USED(idclasses_skel);
    tvhtrace(LS_IDNODE, "register class %s", idc->ic_class);
    prev = idc;
    idc = idc->ic_super;
  }
  if (prev)
    idclass_root_register(prev);
}

const idclass_t *
idclass_find ( const char *class )
{
  idclass_link_t *t, skel;
  idclass_t idc;
  skel.idc = &idc;
  idc.ic_class = class;
  tvhtrace(LS_IDNODE, "find class %s", class);
  t = RB_FIND(&idclasses, &skel, link, ic_cmp);
  return t ? t->idc : NULL;
}

idclass_t const **
idclass_find_all(void)
{
  idclass_link_t *l;
  idclass_t const **ret;
  int i = 0, count = 0;
  RB_FOREACH(l, &idclasses, link)
    count++;
  if (count == 0)
    return NULL;
  ret = calloc(count + 1, sizeof(idclass_t *));
  RB_FOREACH(l, &idclasses, link)
    ret[i++] = l->idc;
  ret[i] = NULL;
  return ret;
}

idclass_t const **
idclass_find_children(const char *clazz)
{
  const idclass_t *ic = idclass_find(clazz), *root;
  idclass_link_t *l;
  idclass_t const **ret;
  int i, count;

  if (ic == NULL)
    return NULL;
  root = idnode_root_class(ic);
  if (root == NULL)
    return NULL;
  ret = NULL;
  count = i = 0;
  RB_FOREACH(l, &idclasses, link) {
    if (root == idnode_root_class(l->idc)) {
      if (i <= count) {
        count += 50;
        ret = realloc(ret, count * sizeof(const idclass_t *));
      }
      ret[i++] = ic;
    }
  }
  if (i <= count)
    ret = realloc(ret, (count + 1) * sizeof(const idclass_t *));
  ret[i] = NULL;
  return ret;
}

/*
 * Just get the class definition
 */
htsmsg_t *
idclass_serialize0(const idclass_t *idc, htsmsg_t *list, int optmask, const char *lang)
{
  const char *s;
  htsmsg_t *p, *m = htsmsg_create_map();

  /* Caption and name */
  if ((s = idclass_get_caption(idc, lang)))
    htsmsg_add_str(m, "caption", s);
  if ((s = idclass_get_class(idc)))
    htsmsg_add_str(m, "class", s);
  if ((s = idclass_get_event(idc)))
    htsmsg_add_str(m, "event", s);
  if ((s = idclass_get_order(idc)))
    htsmsg_add_str(m, "order", s);
  if ((p = idclass_get_property_groups(idc, lang)))
    htsmsg_add_msg(m, "groups", p);

  /* Props */
  if ((p = idnode_params(idc, NULL, list, optmask, lang)))
    htsmsg_add_msg(m, "props", p);
  
  return m;
}

/**
 *
 */
htsmsg_t *
idnode_serialize0(idnode_t *self, htsmsg_t *list, int optmask, const char *lang)
{
  const idclass_t *idc = self->in_class;
  const char *s;
  char buf[384];

  htsmsg_t *m = htsmsg_create_map();
  if (!idc->ic_snode) {
    htsmsg_add_uuid(m, "uuid", &self->in_uuid);
    htsmsg_add_uuid(m, "id",   &self->in_uuid);
  }
  htsmsg_add_str(m, "text", idnode_get_title(self, lang, buf, sizeof(buf)) ?: "");
  if ((s = idclass_get_caption(idc, lang)))
    htsmsg_add_str(m, "caption", s);
  if ((s = idclass_get_class(idc)))
    htsmsg_add_str(m, "class", s);
  if ((s = idclass_get_event(idc)))
    htsmsg_add_str(m, "event", s);

  htsmsg_add_msg(m, "params", idnode_params(idc, self, list, optmask, lang));

  return m;
}

/* **************************************************************************
 * Simple list helpers
 * *************************************************************************/

htsmsg_t *
idnode_slist_enum ( idnode_t *in, idnode_slist_t *options, const char *lang )
{
  htsmsg_t *l = htsmsg_create_list(), *m;

  for (; options->id; options++) {
    m = htsmsg_create_key_val(options->id, tvh_gettext_lang(lang, options->name));
    htsmsg_add_msg(l, NULL, m);
  }
  return l;
}

htsmsg_t *
idnode_slist_get ( idnode_t *in, idnode_slist_t *options )
{
  htsmsg_t *l = htsmsg_create_list();
  int val;

  for (; options->id; options++) {
    if (options->get)
      val = options->get(in, options);
    else if (options->off)
      val = *(int *)((void *)in + options->off);
    else
      val = 0;
    if (val)
      htsmsg_add_str(l, NULL, options->id);
  }
  return l;
}

int
idnode_slist_set ( idnode_t *in, idnode_slist_t *options, const htsmsg_t *vals )
{
  idnode_slist_t *o;
  htsmsg_field_t *f;
  int *ip, changed = 0;
  const char *s;

  for (o = options; o->id; o++) {
    if (o->off) {
      ip = (void *)in + o->off;
    } else {
      ip = NULL;
    }
    HTSMSG_FOREACH(f, vals) {
      if ((s = htsmsg_field_get_str(f)) == NULL)
        continue;
      if (strcmp(s, o->id))
        continue;
      if (o->set) {
        changed |= o->set(in, o, 1);
      } else if (*ip == 0) {
        *ip = changed = 1;
      }
      break;
    }
    if (f == NULL) {
      if (o->set) {
        changed |= o->set(in, o, 0);
      } else if (*ip) {
        *ip = 0;
        changed = 1;
      }
    }
  }
  return changed;
}

char *
idnode_slist_rend ( idnode_t *in, idnode_slist_t *options, const char *lang )
{
  int *ip;
  size_t l = 0;

  prop_sbuf[0] = '\0';
  for (; options->id; options++) {
    ip = (void *)in + options->off;
    if (*ip)
     tvh_strlcatf(prop_sbuf, PROP_SBUF_LEN, l, "%s%s", prop_sbuf[0] ? "," : "",
                   tvh_gettext_lang(lang, options->name));
  }
  return strdup(prop_sbuf);
}

/* **************************************************************************
 * List helpers
 * *************************************************************************/

static void
idnode_list_notify ( idnode_list_mapping_t *ilm, void *origin )
{
  if (origin == NULL)
    return;
  if (origin == ilm->ilm_in1) {
    idnode_notify_changed(ilm->ilm_in2);
    if (ilm->ilm_in2_save)
      idnode_save_queue(ilm->ilm_in2);
  }
  if (origin == ilm->ilm_in2) {
    idnode_notify_changed(ilm->ilm_in1);
    if (ilm->ilm_in1_save)
      idnode_save_queue(ilm->ilm_in1);
  }
}

/*
 * Link class1 and class2
 */
idnode_list_mapping_t *
idnode_list_link ( idnode_t *in1, idnode_list_head_t *in1_list,
                   idnode_t *in2, idnode_list_head_t *in2_list,
                   void *origin, uint32_t savemask )
{
  idnode_list_mapping_t *ilm;

  /* Already linked */
  LIST_FOREACH(ilm, in1_list, ilm_in1_link)
    if (ilm->ilm_in2 == in2) {
      ilm->ilm_mark = 0;
      return NULL;
    }
  LIST_FOREACH(ilm, in2_list, ilm_in2_link)
    if (ilm->ilm_in1 == in1) {
      ilm->ilm_mark = 0;
      return NULL;
    }

  /* Link */
  ilm = calloc(1, sizeof(idnode_list_mapping_t));
  ilm->ilm_in1 = in1;
  ilm->ilm_in2 = in2;
  LIST_INSERT_HEAD(in1_list, ilm, ilm_in1_link);
  LIST_INSERT_HEAD(in2_list, ilm, ilm_in2_link);
  ilm->ilm_in1_save = savemask & 1;
  ilm->ilm_in2_save = (savemask >> 1) & 1;
  idnode_list_notify(ilm, origin);
  return ilm;
}

void
idnode_list_unlink ( idnode_list_mapping_t *ilm, void *origin )
{
  LIST_REMOVE(ilm, ilm_in1_link);
  LIST_REMOVE(ilm, ilm_in2_link);
  idnode_list_notify(ilm, origin);
  free(ilm);
}

void
idnode_list_destroy(idnode_list_head_t *ilh, void *origin)
{
  idnode_list_mapping_t *ilm;

  while ((ilm = LIST_FIRST(ilh)) != NULL)
    idnode_list_unlink(ilm, origin);
}

static int
idnode_list_clean1
  ( idnode_t *in1, idnode_list_head_t *in1_list )
{
  int save = 0;
  idnode_list_mapping_t *ilm, *n;

  for (ilm = LIST_FIRST(in1_list); ilm != NULL; ilm = n) {
    n = LIST_NEXT(ilm, ilm_in1_link);
    if (ilm->ilm_mark) {
      idnode_list_unlink(ilm, in1);
      save = 1;
    }
  }
  return save;
}

static int
idnode_list_clean2
  ( idnode_t *in2, idnode_list_head_t *in2_list )
{
  int save = 0;
  idnode_list_mapping_t *ilm, *n;

  for (ilm = LIST_FIRST(in2_list); ilm != NULL; ilm = n) {
    n = LIST_NEXT(ilm, ilm_in2_link);
    if (ilm->ilm_mark) {
      idnode_list_unlink(ilm, in2);
      save = 1;
    }
  }
  return save;
}

htsmsg_t *
idnode_list_get1
  ( idnode_list_head_t *in1_list )
{
  idnode_list_mapping_t *ilm;
  htsmsg_t *l = htsmsg_create_list();

  LIST_FOREACH(ilm, in1_list, ilm_in1_link)
    htsmsg_add_uuid(l, NULL, &ilm->ilm_in2->in_uuid);
  return l;
}

htsmsg_t *
idnode_list_get2
  ( idnode_list_head_t *in2_list )
{
  idnode_list_mapping_t *ilm;
  htsmsg_t *l = htsmsg_create_list();

  LIST_FOREACH(ilm, in2_list, ilm_in2_link)
    htsmsg_add_uuid(l, NULL, &ilm->ilm_in1->in_uuid);
  return l;
}

char *
idnode_list_get_csv1
  ( idnode_list_head_t *in1_list, const char *lang )
{
  char *str;
  idnode_list_mapping_t *ilm;
  htsmsg_t *l = htsmsg_create_list();
  char buf[384];

  LIST_FOREACH(ilm, in1_list, ilm_in1_link)
    htsmsg_add_str(l, NULL, idnode_get_title(ilm->ilm_in2, lang, buf, sizeof(buf)));

  str = htsmsg_list_2_csv(l, ',', 1);
  htsmsg_destroy(l);
  return str;
}

char *
idnode_list_get_csv2
  ( idnode_list_head_t *in2_list, const char *lang )
{
  char *str;
  idnode_list_mapping_t *ilm;
  htsmsg_t *l = htsmsg_create_list();
  char buf[384];

  LIST_FOREACH(ilm, in2_list, ilm_in2_link)
    htsmsg_add_str(l, NULL, idnode_get_title(ilm->ilm_in1, lang, buf, sizeof(buf)));

  str = htsmsg_list_2_csv(l, ',', 1);
  htsmsg_destroy(l);
  return str;
}

int
idnode_list_set1
  ( idnode_t *in1, idnode_list_head_t *in1_list,
    const idclass_t *in2_class, htsmsg_t *in2_list,
    int (*in2_create)(idnode_t *in1, idnode_t *in2, void *origin) )
{
  const char *str;
  htsmsg_field_t *f;
  idnode_t *in2;
  idnode_list_mapping_t *ilm;
  int save = 0;

  /* Mark all for deletion */
  LIST_FOREACH(ilm, in1_list, ilm_in1_link)
    ilm->ilm_mark = 1;

  /* Make new links */
  HTSMSG_FOREACH(f, in2_list)
    if ((str = htsmsg_field_get_str(f)))
      if ((in2 = idnode_find(str, in2_class, NULL)) != NULL)
        if (in2_create(in1, in2, in1))
          save = 1;

  /* Delete unlinked */
  if (idnode_list_clean1(in1, in1_list))
    save = 1;

  /* Change notification */
  if (save)
    idnode_notify_changed(in1);

  /* Save only on demand */
  ilm = LIST_FIRST(in1_list);
  if (ilm && !ilm->ilm_in1_save)
    save = 0;

  return save;
}

int
idnode_list_set2
  ( idnode_t *in2, idnode_list_head_t *in2_list,
    const idclass_t *in1_class, htsmsg_t *in1_list,
    int (*in1_create)(idnode_t *in1, idnode_t *in2, void *origin) )
{
  const char *str;
  htsmsg_field_t *f;
  idnode_t *in1;
  idnode_list_mapping_t *ilm;
  int save = 0;

  /* Mark all for deletion */
  LIST_FOREACH(ilm, in2_list, ilm_in2_link)
    ilm->ilm_mark = 1;

  /* Make new links */
  HTSMSG_FOREACH(f, in1_list)
    if ((str = htsmsg_field_get_str(f)))
      if ((in1 = idnode_find(str, in1_class, NULL)) != NULL)
        if (in1_create(in1, in2, in2))
          save = 1;

  /* Delete unlinked */
  if (idnode_list_clean2(in2, in2_list))
    save = 1;

  /* Change notification */
  if (save)
    idnode_notify_changed(in2);

  /* Save only on demand */
  ilm = LIST_FIRST(in2_list);
  if (ilm && !ilm->ilm_in2_save)
    save = 0;

  return save;
}


/* **************************************************************************
 * Notification
 * *************************************************************************/

/**
 * Notify about a change
 */
void
idnode_notify ( idnode_t *in, const char *action )
{
  const idclass_t *ic = in->in_class;
  char ubuf[UUID_HEX_SIZE];
  const char *uuid = idnode_uuid_as_str(in, ubuf);

  if (!tvheadend_is_running())
    return;

  while (ic) {
    if (ic->ic_event) {
      if (!ic->ic_snode)
        notify_delayed(uuid, ic->ic_event, action);
      else
        notify_reload(ic->ic_event);
    }
    ic = ic->ic_super;
  }
}

void
idnode_notify_changed (void *in)
{
  idnode_notify(in, "change");
}

void
idnode_notify_title_changed (void *in)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_uuid(m, "uuid", &((idnode_t *)in)->in_uuid);
  notify_by_msg("title", m, 0, NOTIFY_REWRITE_TITLE);
  idnode_notify_changed(in);
}

void
idnode_notify_title_changed_lang (void *in, const char *lang)
{
  return idnode_notify_title_changed(in);
}

/* **************************************************************************
 * Save thread
 * *************************************************************************/

static void *
save_thread ( void *aux )
{
  idnode_save_t *ise;
  idnode_t *in;
  htsmsg_t *m;
  uint32_t u32;
  tvh_uuid_t *uuid;
  char filename[PATH_MAX];
  tvh_uuid_set_t set, tset;
  int lnotify;

  uuid_set_init(&set, 10);
  uuid_set_init(&tset, 10);

  tvh_thread_renice(15);

  tvh_mutex_lock(&global_lock);

  while (atomic_get(&save_running)) {
    if ((ise = TAILQ_FIRST(&idnodes_save)) == NULL ||
        ise->ise_reqtime + IDNODE_SAVE_DELAY > mclk()) {
      tvh_mutex_lock(&idnode_lnotify_mutex);
      lnotify = !uuid_set_empty(&idnode_lnotify_set) ||
                !uuid_set_empty(&idnode_lnotify_title_set);
      tvh_mutex_unlock(&idnode_lnotify_mutex);
      if (lnotify)
        goto lnotifygo;
      if (ise)
        mtimer_arm_abs(&save_timer, idnode_save_trigger_thread_cb, NULL,
                       ise->ise_reqtime + IDNODE_SAVE_DELAY);
      tvh_cond_wait(&save_cond, &global_lock);
      continue;
    }
    if (ise) {
      m = idnode_savefn(ise->ise_node, filename, sizeof(filename));
      ise->ise_node->in_save = NULL;
      TAILQ_REMOVE(&idnodes_save, ise, ise_link);
      tvh_mutex_unlock(&global_lock);
      free(ise);
      if (m) {
        hts_settings_save(m, "%s", filename);
        htsmsg_destroy(m);
      }
      tvh_mutex_lock(&global_lock);
    }
lnotifygo:
    tvh_mutex_lock(&idnode_lnotify_mutex);
    if (!uuid_set_empty(&idnode_lnotify_set)) {
      set = idnode_lnotify_set;
      uuid_set_init(&idnode_lnotify_set, 10);
    }
    if (!uuid_set_empty(&idnode_lnotify_title_set)) {
      tset = idnode_lnotify_title_set;
      uuid_set_init(&idnode_lnotify_title_set, 10);
    }
    tvh_mutex_unlock(&idnode_lnotify_mutex);
    if (!uuid_set_empty(&set)) {
      UUID_SET_FOREACH(uuid, &set, u32) {
        in = idnode_find0(uuid, NULL, NULL);
        if (in)
         idnode_notify_changed(in);
      }
      uuid_set_free(&set);
    }
    if (!uuid_set_empty(&tset)) {
      UUID_SET_FOREACH(uuid, &tset, u32) {
        in = idnode_find0(uuid, NULL, NULL);
        if (in)
          idnode_notify_title_changed(in);
      }
      uuid_set_free(&tset);
    }
  }

  mtimer_disarm(&save_timer);

  while ((ise = TAILQ_FIRST(&idnodes_save)) != NULL) {
    m = idnode_savefn(ise->ise_node, filename, sizeof(filename));
    ise->ise_node->in_save = NULL;
    TAILQ_REMOVE(&idnodes_save, ise, ise_link);
    tvh_mutex_unlock(&global_lock);
    free(ise);
    if (m) {
      hts_settings_save(m, "%s", filename);
      htsmsg_destroy(m);
    }
    tvh_mutex_lock(&global_lock);
  }

  tvh_mutex_unlock(&global_lock);
  return NULL;
}

/* **************************************************************************
 * Light update - outside global lock
 * *************************************************************************/

void idnode_lnotify_changed( void *in )
{
  tvh_mutex_lock(&idnode_lnotify_mutex);
  uuid_set_add(&idnode_lnotify_set, &((idnode_t *)in)->in_uuid);
  tvh_mutex_unlock(&idnode_lnotify_mutex);
  tvh_cond_signal(&save_cond, 0);
}

void idnode_lnotify_title_changed( void *in )
{
  tvh_mutex_lock(&idnode_lnotify_mutex);
  uuid_set_add(&idnode_lnotify_title_set, &((idnode_t *)in)->in_uuid);
  tvh_mutex_unlock(&idnode_lnotify_mutex);
  tvh_cond_signal(&save_cond, 0);
}

/* **************************************************************************
 * Initialization
 * *************************************************************************/

void
idnode_boot(void)
{
  tvh_mutex_init(&idnode_mutex, NULL);
  RB_INIT(&idnodes);
  RB_INIT(&idclasses);
  RB_INIT(&idrootclasses);
  TAILQ_INIT(&idnodes_save);
  tvh_cond_init(&save_cond, 1);
  uuid_set_init(&idnode_lnotify_set, 10);
  uuid_set_init(&idnode_lnotify_title_set, 10);
}

void
idnode_init(void)
{
  atomic_set(&save_running, 1);
  tvh_thread_create(&save_tid, NULL, save_thread, NULL, "save");
}

void
idnode_done(void)
{
  idclass_link_t *il;

  tvh_mutex_lock(&global_lock);
  atomic_set(&save_running, 0);
  tvh_cond_signal(&save_cond, 0);
  tvh_mutex_unlock(&global_lock);
  pthread_join(save_tid, NULL);

  tvh_mutex_lock(&global_lock);

  mtimer_disarm(&save_timer);

  while ((il = RB_FIRST(&idclasses)) != NULL) {
    RB_REMOVE(&idclasses, il, link);
    free(il);
  }
  while ((il = RB_FIRST(&idrootclasses)) != NULL) {
    RB_REMOVE(&idrootclasses, il, link);
    free(il);
  }
  SKEL_FREE(idclasses_skel);

  uuid_set_free(&idnode_lnotify_set);
  uuid_set_free(&idnode_lnotify_title_set);

  tvh_mutex_unlock(&global_lock);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
