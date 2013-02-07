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
} property_t;



void prop_read_values(void *ptr, const property_t p[], htsmsg_t *m);
htsmsg_t *prop_get_values(void *ptr, const property_t p[]);
void prop_read_names(const property_t p[], htsmsg_t *m);
htsmsg_t *prop_get_names(const property_t p[]);
