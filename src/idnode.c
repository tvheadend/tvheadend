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

typedef struct idclass_link
{
  const idclass_t        *idc;
  RB_ENTRY(idclass_link) link;
} idclass_link_t;

static int                    randfd = 0;
static RB_HEAD(,idnode)       idnodes;
static RB_HEAD(,idclass_link) idclasses;
static pthread_cond_t         idnode_cond;
static pthread_mutex_t        idnode_mutex;
static htsmsg_t              *idnode_queue;
static void*                  idnode_thread(void* p);

static void
idclass_register(const idclass_t *idc);

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
  return memcmp(a->in_uuid, b->in_uuid, sizeof(a->in_uuid));
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
  pthread_t tid;

  randfd = open("/dev/urandom", O_RDONLY);
  if(randfd == -1)
    exit(1);
  
  idnode_queue = NULL;
  pthread_mutex_init(&idnode_mutex, NULL);
  pthread_cond_init(&idnode_cond, NULL);
  tvhthread_create(&tid, NULL, idnode_thread, NULL, 1);
}

/**
 *
 */
int
idnode_insert(idnode_t *in, const char *uuid, const idclass_t *class)
{
  idnode_t *c;
  lock_assert(&global_lock);
  if(uuid == NULL) {
    if(read(randfd, in->in_uuid, sizeof(in->in_uuid)) != sizeof(in->in_uuid)) {
      perror("read(random for uuid)");
      exit(1);
    }
  } else {
    if(hex2bin(in->in_uuid, sizeof(in->in_uuid), uuid))
      return -1;
  }

  in->in_class = class;

  c = RB_INSERT_SORTED(&idnodes, in, in_link, in_cmp);
  if(c != NULL) {
    fprintf(stderr, "Id node collision\n");
    abort();
  }
  tvhtrace("idnode", "insert node %s", idnode_uuid_as_str(in));

  /* Register the class */
  idclass_register(class); // Note: we never actually unregister

  /* Fire event */
  idnode_notify(in, NULL, 0, 1);

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
  tvhtrace("idnode", "unlink node %s", idnode_uuid_as_str(in));
  idnode_notify(in, NULL, 0, 1);
}

/**
 *
 */
void
idnode_delete(idnode_t *in)
{
  lock_assert(&global_lock);
  const idclass_t *idc = in->in_class;
  while (idc) {
    if (idc->ic_delete) {
      idc->ic_delete(in);
      break;
    }
    idc = idc->ic_super;
  }
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
idnode_uuid_as_str0(const idnode_t *in, char *b)
{
  bin2hex(b, UUID_STR_LEN, in->in_uuid, sizeof(in->in_uuid));
  return b;
}
const char *
idnode_uuid_as_str(const idnode_t *in)
{
  static char ret[16][UUID_STR_LEN];
  static int p = 0;
  char *b = ret[p];
  p = (p + 1) % 16;
  return idnode_uuid_as_str0(in, b);
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
  if (p->rend)
    return p->rend(self);
  if (p->islist) {
    htsmsg_t *l = (htsmsg_t*)p->get(self);
    if (l)
      return htsmsg_list_2_csv(l);
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
  if (p->islist) return 1;
  if (p) {
    const void *ptr;
    if (p->get)
      ptr = p->get(self);
    else
      ptr = ((void*)self) + p->off;
    switch (p->type) {
      case PT_INT:
      case PT_BOOL:
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
  if (p->islist) return 1;
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

  tvhtrace("idnode", "find node %s class %s", uuid, idc ? idc->ic_class : NULL);
  if(hex2bin(skel.in_uuid, sizeof(skel.in_uuid), uuid))
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
  tvhtrace("idnode", "find class %s", idc->ic_class);
  idnode_set_t *is = calloc(1, sizeof(idnode_set_t));
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
  if (p->islist || p->list) {
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
        const char *strb = idnode_get_str(inb, sort->key);
        if (sort->dir == IS_ASC)
          r = strcmp(stra ?: "", strb ?: "");
        else
          r = strcmp(strb ?: "", stra ?: "");
        return r;
      }
      break;
    case PT_INT:
    case PT_U16:
    case PT_U32:
    case PT_BOOL:
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
    case PT_DBL:
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
      str = idnode_get_display(in, idnode_find_prop(in, f->key));
      if (!str)
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
  qsort_r(is->is_array, is->is_count, sizeof(idnode_t*), idnode_cmp_sort, (void*)sort);
}

void
idnode_set_sort_by_title
  ( idnode_set_t *is )
{
  qsort(is->is_array, is->is_count, sizeof(idnode_t*), idnode_cmp_title);
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
    idnode_notify(self, NULL, 0, 0);
  // Note: always output event if "dosave", reason is that UI updates on
  //       these, but there are some subtle cases where it will expect
  //       an update and not get one. This include fields being set for
  //       which there is user-configurable value and auto fallback so
  //       the UI state might not atually reflect the user config
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
    prop_read_values(self, idc->ic_properties, c, optmask, NULL);
}

/**
 * Recursive to get superclass nodes first
 */
static void
add_params
  (struct idnode *self, const idclass_t *ic, htsmsg_t *p, int optmask, htsmsg_t *inc)
{
  /* Parent first */
  if(ic->ic_super != NULL)
    add_params(self, ic->ic_super, p, optmask, inc);

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
  prop_serialize(self, ic->ic_properties, p, optmask, inc);
}

static htsmsg_t *
idnode_params (const idclass_t *idc, idnode_t *self, int optmask)
{
  htsmsg_t *p  = htsmsg_create_list();
  add_params(self, idc, p, optmask, NULL);
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

static int
ic_cmp ( const idclass_link_t *a, const idclass_link_t *b )
{
  assert(a->idc->ic_class);
  assert(b->idc->ic_class);
  return strcmp(a->idc->ic_class, b->idc->ic_class);
}

static void
idclass_register(const idclass_t *idc)
{
  static idclass_link_t *skel = NULL;
  while (idc) {
    if (!skel)
      skel = calloc(1, sizeof(idclass_link_t));
    skel->idc = idc;
    if (RB_INSERT_SORTED(&idclasses, skel, link, ic_cmp))
      break;
    tvhtrace("idnode", "register class %s", idc->ic_class);
    skel = NULL;
    idc = idc->ic_super;
  }
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
idclass_serialize0(const idclass_t *idc, int optmask)
{
  const char *s;
  htsmsg_t *p, *m = htsmsg_create_map();

  /* Caption and name */
  if ((s = idclass_get_caption(idc)))
    htsmsg_add_str(m, "caption", s);
  if ((s = idclass_get_class(idc)))
    htsmsg_add_str(m, "class", s);

  /* Props */
  if ((p = idnode_params(idc, NULL, optmask)))
    htsmsg_add_msg(m, "props", p);
  
  return m;
}

/**
 *
 */
htsmsg_t *
idnode_serialize0(idnode_t *self, int optmask)
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

  htsmsg_add_msg(m, "params", idnode_params(idc, self, optmask));

  return m;
}

/* **************************************************************************
 * Notifcation
 * *************************************************************************/

/**
 * Update internal event pipes
 */
static void
idnode_notify_event ( idnode_t *in )
{
  const idclass_t *ic = in->in_class;
  const char *uuid = idnode_uuid_as_str(in);
  while (ic) {
    if (ic->ic_event) {
      htsmsg_t *m = htsmsg_create_map();
      htsmsg_add_str(m, "uuid", uuid);
      notify_by_msg(ic->ic_event, m);
    }
    ic = ic->ic_super;
  }
}

/**
 * Notify on a given channel
 */
void
idnode_notify
  (idnode_t *in, const char *chn, int force, int event)
{
  const char *uuid = idnode_uuid_as_str(in);

  /* Forced */
  if (chn || force) {
    htsmsg_t *m = htsmsg_create_map();
    htsmsg_add_str(m, "uuid", uuid);
    notify_by_msg(chn ?: "idnodeUpdated", m);
  
  /* Rate-limited */
  } else {
    pthread_mutex_lock(&idnode_mutex);
    if (!idnode_queue)
      idnode_queue = htsmsg_create_map();
    htsmsg_set_u32(idnode_queue, uuid, 1);
    pthread_cond_signal(&idnode_cond);
    pthread_mutex_unlock(&idnode_mutex);
  }

  /* Send event */
  if (event)
    idnode_notify_event(in);
}

void
idnode_notify_simple (void *in)
{
  idnode_notify(in, NULL, 0, 0);
}

void
idnode_notify_title_changed (void *in)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "uuid", idnode_uuid_as_str(in));
  htsmsg_add_str(m, "text", idnode_get_title(in));
  notify_by_msg("idnodeUpdated", m);
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

  pthread_mutex_lock(&idnode_mutex);

  while (1) {

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
      node = idnode_find(f->hmf_name, NULL);
      m    = htsmsg_create_map();
      htsmsg_add_str(m, "uuid", f->hmf_name);
      if (node)
        notify_by_msg("idnodeUpdated", m);
      else
        notify_by_msg("idnodeDeleted", m);      
    }
    
    /* Finished */
    pthread_mutex_unlock(&global_lock);
    htsmsg_destroy(q);
    q = NULL;

    /* Wait */
    usleep(500000);
    pthread_mutex_lock(&idnode_mutex);
  }
  if (q) htsmsg_destroy(q);
  
  return NULL;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
