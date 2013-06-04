#pragma once
#include <stddef.h>

#include "htsmsg.h"

typedef enum {
  PT_BOOL,
  PT_STR,
  PT_INT,
  PT_U16,
  PT_U32,
} prop_type_t;

#define PO_NONE   0x0
#define PO_RDONLY 0x1 // Note: if this is changed, change PROPDEF2
#define PO_NOSAVE 0x2

typedef struct property {
  const char *id;
  const char *name;
  prop_type_t type;
  size_t off;
  int options;

  const char *(*str_get)(void *ptr);
  void (*str_set)(void *ptr, const char *str);

  void (*notify)(void *ptr);

} property_t;

const property_t *prop_find(const property_t *p, const char *name);

void prop_add_params_to_msg(void *obj, const property_t *p, htsmsg_t *msg);

void prop_write_values(void *ptr, const property_t *pl, htsmsg_t *m);

void prop_read_values(void *obj, const property_t *p, htsmsg_t *m);

int prop_set(void *obj, const property_t *p, const char *key, const char *val);

int prop_update_all(void *obj, const property_t *p,
                     const char *(*getvalue)(void *opaque, const char *key),
                     void *opaque);

#define PROPDEF0(_i, _n, _t, _o)\
  .id      = _i,\
  .name    = _n,\
  .type    = _t,\
  .options = _o

#define PROPDEF1(_i, _n, _t, _o, _v)\
  .id      = _i,\
  .name    = _n,\
  .type    = _t,\
  .off     = offsetof(_o, _v)

#define PROPDEF2(_i, _n, _t, _o, _v, _r)\
  .id      = _i,\
  .name    = _n,\
  .type    = _t,\
  .off     = offsetof(_o, _v),\
  .options = _r

#define PROPDEF3(_i, _n, _t, _o, _v, _c)\
  .id      = _i,\
  .name    = _n,\
  .type    = _t,\
  .off     = offsetof(_o, _v),\
  .options = _c

