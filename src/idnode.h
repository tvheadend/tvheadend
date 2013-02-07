#pragma once

#include "tvheadend.h"
#include "prop.h"

struct htsmsg;
struct idnode;

typedef struct idclass {
  const char *ic_class;
  struct htsmsg *(*ic_serialize)(struct idnode *self);
  struct idnode **(*ic_get_childs)(struct idnode *self);
  const char *(*ic_get_title)(struct idnode *self);
  const property_t ic_properties[];
} idclass_t;


typedef struct idnode {
  uint8_t in_uuid[16];
  RB_ENTRY(idnode) in_link;
  const idclass_t *in_class;
} idnode_t;

void idnode_init(void);

int idnode_insert(idnode_t *in, const char *uuid, const idclass_t *class);

const char *idnode_uuid_as_str(const idnode_t *in);

idnode_t *idnode_find(const char *uuid);

void idnode_unlink(idnode_t *in);

htsmsg_t *idnode_serialize(struct idnode *self);
