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

static int randfd = 0;

RB_HEAD(idnode_tree, idnode);

static struct idnode_tree idnodes;

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
static int
in_cmp(const idnode_t *a, const idnode_t *b)
{
  return memcmp(a->in_uuid, b->in_uuid, 16);
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


/**
 *
 */
void
idnode_unlink(idnode_t *in)
{
  RB_REMOVE(&idnodes, in, in_link);
}


/**
 * Recursive to get superclass nodes first
 */
static void
add_params(struct idnode *self, const idclass_t *ic, htsmsg_t *p)
{
  if(ic->ic_super != NULL)
    add_params(self, ic->ic_super, p);

  if(TAILQ_FIRST(&p->hm_fields) != NULL) {
    // Only add separator if not empty
    htsmsg_t *m = htsmsg_create_map();
    htsmsg_add_str(m, "caption", ic->ic_caption ?: ic->ic_class);
    htsmsg_add_str(m, "type", "separator");
    htsmsg_add_msg(p, NULL, m);
  }

  prop_add_params_to_msg(self, ic->ic_properties, p);
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
idnode_t **
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
  if(ic->ic_leaf)
    return 1;
  for(; ic != NULL; ic = ic->ic_super) {
    if(ic->ic_get_childs != NULL)
      return 0;
  }
  return 1;
}



/**
 *
 */
htsmsg_t *
idnode_serialize(struct idnode *self)
{
  const idclass_t *c = self->in_class;
  htsmsg_t *m;
  if(c->ic_serialize != NULL) {
    m = c->ic_serialize(self);
  } else {
    m = htsmsg_create_map();
    htsmsg_add_str(m, "id", idnode_uuid_as_str(self));
    htsmsg_add_str(m, "text", idnode_get_title(self));

    htsmsg_t *p  = htsmsg_create_list();
    add_params(self, c, p);

    htsmsg_add_msg(m, "params", p);

  }
  return m;
}

/**
 *
 */
static void
idnode_updated(idnode_t *in)
{
  const idclass_t *ic = in->in_class;

  for(; ic != NULL; ic = ic->ic_super) {
    if(ic->ic_save != NULL) {
      ic->ic_save(in);
      break;
    }
  }

  // Tell about updated parameters

  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "id", idnode_uuid_as_str(in));

  htsmsg_t *p  = htsmsg_create_list();
  add_params(in, in->in_class, p);
  htsmsg_add_msg(m, "params", p);

  notify_by_msg("idnodeParamsChanged", m);
}


/**
 *
 */
void
idnode_set_prop(idnode_t *in, const char *key, const char *value)
{
  const idclass_t *ic = in->in_class;
  int do_save = 0;
  for(;ic != NULL; ic = ic->ic_super) {
    int x = prop_set(in, ic->ic_properties, key, value);
    if(x == -1)
      continue;
    do_save |= x;
    break;
  }
  if(do_save)
    idnode_updated(in);
}


/**
 *
 */
void
idnode_update_all_props(idnode_t *in,
                        const char *(*getvalue)(void *opaque, const char *key),
                        void *opaque)
{
  const idclass_t *ic = in->in_class;
  int do_save = 0;
  for(;ic != NULL; ic = ic->ic_super)
    do_save |= prop_update_all(in, ic->ic_properties, getvalue, opaque);
  if(do_save)
    idnode_updated(in);
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

/*
 * Save
 */
void
idnode_save ( idnode_t *self, htsmsg_t *c )
{
  const idclass_t *idc = self->in_class;
  while (idc) {
    prop_read_values(self, idc->ic_properties, c);
    idc = idc->ic_super;
  }
}

/*
 * Load
 */
void
idnode_load ( idnode_t *self, htsmsg_t *c )
{
  const idclass_t *idc = self->in_class;
  while (idc) {
    prop_write_values(self, idc->ic_properties, c);
    idc = idc->ic_super;
  }
}

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

/* **************************************************************************
 * Set procsesing
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
