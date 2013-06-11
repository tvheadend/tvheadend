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

static int              randfd = 0;
static RB_HEAD(,idnode) idnodes;

static void idnode_updated(idnode_t *in, int optmask);

/* **************************************************************************
 * Utilities
 * *************************************************************************/

/**
 *
 */
static int
hexnibble(char c)
{
  switch(c) {
  case '0' ... '9':    return c - '0';
  case 'a' ... 'f':    return c - 'a' + 10;
  case 'A' ... 'F':    return c - 'A' + 10;
  default:
    return -1;
  }
}


/**
 *
 */
static int
hex2bin(uint8_t *buf, size_t buflen, const char *str)
{
  int hi, lo;

  while(*str) {
    if(buflen == 0)
      return -1;
    if((hi = hexnibble(*str++)) == -1)
      return -1;
    if((lo = hexnibble(*str++)) == -1)
      return -1;

    *buf++ = hi << 4 | lo;
    buflen--;
  }
  return 0;
}


/**
 *
 */
static void
bin2hex(char *dst, size_t dstlen, const uint8_t *src, size_t srclen)
{
  while(dstlen > 2 && srclen > 0) {
    *dst++ = "0123456789abcdef"[*src >> 4];
    *dst++ = "0123456789abcdef"[*src & 0xf];
    src++;
    srclen--;
    dstlen -= 2;
  }
  *dst = 0;
}

/**
 *
 */
static int
in_cmp(const idnode_t *a, const idnode_t *b)
{
  return memcmp(a->in_uuid, b->in_uuid, 16);
}

/* **************************************************************************
 * Registration
 * *************************************************************************/

/**
 *
 */
void
idnode_init(void)
{
  randfd = open("/dev/urandom", O_RDONLY);
  if(randfd == -1)
    exit(1);
}

/**
 *
 */
int
idnode_insert(idnode_t *in, const char *uuid, const idclass_t *class)
{
  idnode_t *c;
  if(uuid == NULL) {
    if(read(randfd, in->in_uuid, 16) != 16) {
      perror("read(random for uuid)");
      exit(1);
    }
  } else {
    if(hex2bin(in->in_uuid, 16, uuid))
      return -1;
  }

  in->in_class = class;

  c = RB_INSERT_SORTED(&idnodes, in, in_link, in_cmp);
  if(c != NULL) {
    fprintf(stderr, "Id node collision\n");
    abort();
  }
  return 0;
}

/**
 *
 */
void
idnode_unlink(idnode_t *in)
{
  RB_REMOVE(&idnodes, in, in_link);
}

/* **************************************************************************
 * Info
 * *************************************************************************/

/**
 *
 */
const char *
idnode_uuid_as_str(const idnode_t *in)
{
  static char ret[16][33];
  static int p;
  char *b = ret[p];
  bin2hex(b, 33, in->in_uuid, 16);
  p = (p + 1) & 15;
  return b;
}

/**
 *
 */
static const char *
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
 * Get field as string
 */
const char *
idnode_get_str
  ( idnode_t *self, const char *key )
{
  const property_t *p = idnode_find_prop(self, key);
  if (p && p->type == PT_STR) {
    const char *s;
    if (p->str_get)
      s = p->str_get(self);
    else {
      void *ptr = self;
      ptr += p->off;
      s = *(const char**)ptr;
    }
    return s;
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
    void *ptr = self;
    ptr += p->off;
    switch (p->type) {
      case PT_INT:
        *u32 = *(int*)ptr;
        return 0;
      case PT_U16:
        *u32 = *(uint32_t*)ptr;
        return 0;
      case PT_U32:
        *u32 = *(uint16_t*)ptr;
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
    void *ptr = self;
    ptr += p->off;
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

/* **************************************************************************
 * Lookup
 * *************************************************************************/

/**
 *
 */
void *
idnode_find(const char *uuid, const idclass_t *idc)
{
  idnode_t skel, *r;

  if(hex2bin(skel.in_uuid, 16, uuid))
    return NULL;
  r = RB_FIND(&idnodes, &skel, in_link, in_cmp);
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
idnode_find_all ( const idclass_t *idc )
{
  idnode_t *in;
  const idclass_t *ic;
  idnode_set_t *is = calloc(1, sizeof(idnode_set_t));
  RB_FOREACH(in, &idnodes, in_link) {
    ic = in->in_class;
    while (ic) {
      if (ic == idc) {
        idnode_set_add(is, in, NULL);
        break;
      }
      ic = ic->ic_super;
    }
  }
  return is;
}

/* **************************************************************************
 * Set processing
 * *************************************************************************/

static int
idnode_cmp_sort
  ( const void *a, const void *b, void *s )
{
  idnode_t      *ina  = *(idnode_t**)a;
  idnode_t      *inb  = *(idnode_t**)b;
  idnode_sort_t *sort = s;
  const property_t *p = idnode_find_prop(ina, sort->key);
  switch (p->type) {
    case PT_STR:
      {
        const char *stra = idnode_get_str(ina, sort->key);
        const char *strb = idnode_get_str(inb, sort->key);
        if (sort->dir == IS_ASC)
          return strcmp(stra ?: "", strb ?: "");
        else
          return strcmp(strb ?: "", stra ?: "");
      }
      break;
    case PT_INT:
    case PT_U16:
    case PT_U32:
      {
        uint32_t u32a = 0, u32b = 0;
        idnode_get_u32(ina, sort->key, &u32a);
        idnode_get_u32(inb, sort->key, &u32b);
        if (sort->dir == IS_ASC)
          return u32a - u32b;
        else
          return u32b - u32a;
      }
      break;
    case PT_BOOL:
      // TODO
      break;
  }
  return 0;
}

int
idnode_filter
  ( idnode_t *in, idnode_filter_t *filter )
{
  idnode_filter_ele_t *f;
  
  LIST_FOREACH(f, filter, link) {
    if (f->type == IF_STR) {
      const char *str;
      if (!(str = idnode_get_str(in, f->key)))
        return 1;
      switch(f->comp) {
        case IC_IN:
          if (strstr(str, f->u.s) == NULL)
            return 1;
          break;
        case IC_EQ:
          if (strcmp(str, f->u.s) != 0)
            return 1;
        case IC_LT:
          if (strcmp(str, f->u.s) > 0)
            return 1;
          break;
        case IC_GT:
          if (strcmp(str, f->u.s) < 0)
            return 1;
          break;
        case IC_RE:
          if (regexec(&f->u.re, str, 0, NULL, 0))
            return 1;
          break;
      }
    } else if (f->type == IF_NUM || f->type == IF_BOOL) {
      uint32_t u32;
      int64_t a, b;
      if (idnode_get_u32(in, f->key, &u32))
        return 1;
      a = u32;
      b = (f->type == IF_NUM) ? f->u.n : f->u.b;
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
  ( idnode_filter_t *filt, const char *key, int64_t val, int comp )
{
  idnode_filter_ele_t *ele = calloc(1, sizeof(idnode_filter_ele_t));
  ele->key  = strdup(key);
  ele->type = IF_NUM;
  ele->comp = comp;
  ele->u.n  = val;
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

void
idnode_set_sort
  ( idnode_set_t *is, idnode_sort_t *sort )
{
  qsort_r(is->is_array, is->is_count, sizeof(idnode_t*), idnode_cmp_sort, sort);
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

int
idnode_write0 ( idnode_t *self, htsmsg_t *c, int optmask, int dosave )
{
  int save = 0;
  const idclass_t *idc = self->in_class;
  for (; idc; idc = idc->ic_super)
    save |= prop_write_values(self, idc->ic_properties, c, optmask);
  if (save && dosave)
    idnode_updated(self, optmask);
  return save;
}

/* **************************************************************************
 * Read
 * *************************************************************************/

/*
 * Save
 */
void
idnode_read0 ( idnode_t *self, htsmsg_t *c, int optmask )
{
  const idclass_t *idc = self->in_class;
  for (; idc; idc = idc->ic_super)
    prop_read_values(self, idc->ic_properties, c, optmask);
}

/**
 * Recursive to get superclass nodes first
 */
static void
add_params(struct idnode *self, const idclass_t *ic, htsmsg_t *p, int optmask)
{
  /* Parent first */
  if(ic->ic_super != NULL)
    add_params(self, ic->ic_super, p, optmask);

  /* Seperator (if not empty) */
  if(TAILQ_FIRST(&p->hm_fields) != NULL) {
    htsmsg_t *m = htsmsg_create_map();
    htsmsg_add_str(m, "caption",  ic->ic_caption ?: ic->ic_class);
    htsmsg_add_str(m, "type",     "separator");
    htsmsg_add_msg(p, NULL, m);
  }

  /* Properties */
  prop_serialize(self, ic->ic_properties, p, optmask);
}

static htsmsg_t *
idnode_params (const idclass_t *idc, idnode_t *self, int optmask)
{
  htsmsg_t *p  = htsmsg_create_list();
  add_params(self, idc, p, optmask);
  return p;
}

/*
 * Just get the class definition
 */
htsmsg_t *
idclass_serialize0(const idclass_t *idc, int optmask)
{
  return idnode_params(idc, NULL, optmask);
}

/**
 *
 */
htsmsg_t *
idnode_serialize0(idnode_t *self, int optmask)
{
  const idclass_t *idc = self->in_class;

  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "uuid", idnode_uuid_as_str(self));
  htsmsg_add_str(m, "text", idnode_get_title(self));

  htsmsg_add_msg(m, "params", idnode_params(idc, self, optmask));

  return m;
}

/* **************************************************************************
 * Notifcation
 * *************************************************************************/

/**
 *
 */
static void
idnode_updated(idnode_t *in, int optmask)
{
  const idclass_t *ic = in->in_class;

  /* Save */
  for(; ic != NULL; ic = ic->ic_super) {
    if(ic->ic_save != NULL) {
      ic->ic_save(in);
      break;
    }
  }

  /* Notification */

  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "id", idnode_uuid_as_str(in));

  htsmsg_t *p  = htsmsg_create_list();
  add_params(in, in->in_class, p, optmask);
  htsmsg_add_msg(m, "params", p);

  notify_by_msg("idnodeParamsChanged", m);
}

/**
 *
 */
void
idnode_notify_title_changed(void *obj)
{
  idnode_t *in = obj;
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "id", idnode_uuid_as_str(in));
  htsmsg_add_str(m, "text", idnode_get_title(in));
  notify_by_msg("idnodeNameChanged", m);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
