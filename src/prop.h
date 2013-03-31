#pragma once
#include <stddef.h>

#include "htsmsg.h"

typedef enum {
  PT_BOOL,
  PT_INT,
  PT_STR,
} prop_type_t;

typedef struct property {
  const char *id;
  const char *name;
  prop_type_t type;
  size_t off;

  const char *(*str_get)(void *ptr);
  void (*str_set)(void *ptr, const char *str);

} property_t;



void prop_add_descriptors_to_msg(void *obj, const property_t *p, htsmsg_t *msg);

void prop_write_values(void *ptr, const property_t *pl, htsmsg_t *m);

void prop_read_values(void *obj, const property_t *p, htsmsg_t *m);

int prop_set(void *obj, const property_t *p, const char *key, const char *val);

int prop_update_all(void *obj, const property_t *p,
                     const char *(*getvalue)(void *opaque, const char *key),
                     void *opaque);
