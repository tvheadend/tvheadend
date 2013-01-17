#pragma once

#include "tvheadend.h"

struct htsmsg;
struct idnode;

typedef struct idclass {
  const char *ic_class;
  struct htsmsg *(*ic_serialize)(struct idnode *self, int full);
  struct idnode **(*ic_get_childs)(struct idnode *self);
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
