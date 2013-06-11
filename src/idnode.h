#pragma once

#include "tvheadend.h"
#include "prop.h"

#include <regex.h>

struct htsmsg;
struct idnode;

typedef struct idclass {
  const struct idclass *ic_super;
  const char *ic_class;
  const char *ic_caption;
  int ic_leaf;
  struct htsmsg *(*ic_serialize)(struct idnode *self);
  struct idnode **(*ic_get_childs)(struct idnode *self);
  const char *(*ic_get_title)(struct idnode *self);
  void (*ic_save)(struct idnode *self);
  const property_t *ic_properties;
} idclass_t;


typedef struct idnode {
  uint8_t in_uuid[16];
  RB_ENTRY(idnode) in_link;
  const idclass_t *in_class;
} idnode_t;

typedef struct idnode_sort {
  const char *key;
  enum {
    IS_ASC,
    IS_DSC
  }           dir;
} idnode_sort_t;

typedef struct idnode_filter_ele
{
  LIST_ENTRY(idnode_filter_ele) link;
  char *key;
  enum {
    IF_STR,
    IF_NUM,
    IF_BOOL
  } type;
  union {
    int      b;
    char    *s;
    int64_t  n;
    regex_t  re;
  } u;
  enum {
    IC_EQ, // Equals
    IC_LT, // LT
    IC_GT, // GT
    IC_IN, // contains (STR only)
    IC_RE, // regexp (STR only)
  } comp;
} idnode_filter_ele_t;

typedef LIST_HEAD(,idnode_filter_ele) idnode_filter_t;

typedef struct idnode_set
{
  idnode_t **is_array;
  size_t     is_alloc;
  size_t     is_count;
} idnode_set_t;

void idnode_init(void);

int idnode_insert(idnode_t *in, const char *uuid, const idclass_t *class);

const char *idnode_uuid_as_str(const idnode_t *in);

void *idnode_find(const char *uuid, const idclass_t *class);

idnode_set_t *idnode_find_all(const idclass_t *class);

idnode_t **idnode_get_childs(idnode_t *in);

int idnode_is_leaf(idnode_t *in);

void idnode_unlink(idnode_t *in);

htsmsg_t *idnode_serialize(struct idnode *self);

void idnode_set_prop(idnode_t *in, const char *key, const char *value);

const property_t* idnode_get_prop(idnode_t *in, const char *key);

void idnode_update_all_props(idnode_t *in,
                             const char *(*getvalue)(void *opaque,
                                                     const char *key),
                             void *opaque);

void idnode_notify_title_changed(void *obj);

void idnode_save ( idnode_t *self, htsmsg_t *m );
void idnode_load ( idnode_t *self, htsmsg_t *m, int dosave );

const char *idnode_get_str ( idnode_t *self, const char *key );
int idnode_get_u32(idnode_t *self, const char *key, uint32_t *u32);
int idnode_get_bool(idnode_t *self, const char *key, int *b);

/*
 * Set processing
 */
void idnode_filter_add_str
  (idnode_filter_t *f, const char *k, const char *v, int t);
void idnode_filter_add_num
  (idnode_filter_t *f, const char *k, int64_t s64, int t);
void idnode_filter_add_bool
  (idnode_filter_t *f, const char *k, int b, int t);
void idnode_filter_clear
  (idnode_filter_t *f);
int  idnode_filter
  ( idnode_t *in, idnode_filter_t *filt );
void idnode_set_add
  ( idnode_set_t *is, idnode_t *in, idnode_filter_t *filt );
void idnode_set_sort    ( idnode_set_t *is, idnode_sort_t *s );
void idnode_set_free    ( idnode_set_t *is );
