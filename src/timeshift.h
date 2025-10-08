/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2012 Adam Sutton
 *
 * TV headend - Timeshift
 */

#ifndef __TVH_TIMESHIFT_H__
#define __TVH_TIMESHIFT_H__

#include "idnode.h"
#include "streaming.h"
#include "memoryinfo.h"

typedef struct timeshift_conf {
  idnode_t  idnode;
  int       enabled;
  int       ondemand;
  char     *path;
  int       unlimited_period;
  uint32_t  max_period;
  int       unlimited_size;
  uint64_t  max_size;
  uint64_t  total_size;
  uint64_t  ram_size;
  uint64_t  ram_segment_size;
  uint64_t  total_ram_size;
  int       ram_only;
  int       ram_fit;
  int       teletext;
} timeshift_conf_t;

extern struct timeshift_conf timeshift_conf;
extern const idclass_t timeshift_conf_class;

extern memoryinfo_t timeshift_memoryinfo;
extern memoryinfo_t timeshift_memoryinfo_ram;

void timeshift_init ( void );
void timeshift_term ( void );

streaming_target_t *timeshift_create
  (streaming_target_t *out, time_t max_period);

void timeshift_destroy(streaming_target_t *pad);

#endif /* __TVH_TIMESHIFT_H__ */
