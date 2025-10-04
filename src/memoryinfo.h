/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2016 Jaroslav Kysela
 *
 * Tvheadend - memory info support
 */

#ifndef TVHEADEND_MEMORYINFO_H
#define TVHEADEND_MEMORYINFO_H

#include "idnode.h"

struct memoryinfo;

typedef void (*memoryinfo_cb_t)(struct memoryinfo *my);

typedef struct memoryinfo {
  idnode_t               my_idnode;
  LIST_ENTRY(memoryinfo) my_link;
  const char            *my_name;
  void                  *my_opaque;
  memoryinfo_cb_t        my_update;
  int64_t                my_size;
  int64_t                my_peak_size;
  int64_t                my_count;
  int64_t                my_peak_count;
} memoryinfo_t;

LIST_HEAD(memoryinfo_list, memoryinfo);

extern struct memoryinfo_list memoryinfo_entries;
extern const idclass_t memoryinfo_class;

static inline void memoryinfo_register(memoryinfo_t *my)
{
  LIST_INSERT_HEAD(&memoryinfo_entries, my, my_link);
  idnode_insert(&my->my_idnode, NULL, &memoryinfo_class, 0);
}

static inline void memoryinfo_unregister(memoryinfo_t *my)
{
  LIST_REMOVE(my, my_link);
  idnode_unlink(&my->my_idnode);
}

static inline void memoryinfo_update(memoryinfo_t *my, int64_t size, int64_t count)
{
  atomic_set_s64_peak(&my->my_size, size, &my->my_peak_size);
  atomic_set_s64_peak(&my->my_count, count, &my->my_peak_count);
}

static inline void memoryinfo_alloc(memoryinfo_t *my, int64_t size)
{
  atomic_pre_add_s64_peak(&my->my_size, size, &my->my_peak_size);
  atomic_pre_add_s64_peak(&my->my_count, 1, &my->my_peak_count);
}

static inline void memoryinfo_append(memoryinfo_t *my, int64_t size)
{
  atomic_pre_add_s64_peak(&my->my_size, size, &my->my_peak_size);
}

static inline void memoryinfo_free(memoryinfo_t *my, int64_t size)
{
  atomic_dec_s64(&my->my_size, size);
  atomic_dec_s64(&my->my_count, 1);
}

static inline void memoryinfo_remove(memoryinfo_t *my, int64_t size)
{
  atomic_dec_s64(&my->my_size, size);
}

#endif /* TVHEADEND_MEMORYINFO_H */
