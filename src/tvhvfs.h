/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2015 Jaroslav Kysela
 *
 * File system management
 */

#ifndef __TVH_VFS_H__
#define __TVH_VFS_H__

#if ENABLE_ANDROID
#include <sys/vfs.h>
#define statvfs statfs
#define fstatvfs fstatfs
#define tvh_fsid(x) (((uint64_t)x.__val[0] << 32) | (x.__val[1]))
#else
#include <sys/statvfs.h>
#define tvh_fsid(x) ((uint64_t)x)
#endif

struct statvfs;

typedef struct {
  uint64_t fsid;
  char id[64];
} tvh_fsid_t;

int tvh_vfs_fsid_build(const char *path, struct statvfs *vfs, tvh_fsid_t *dst);

static inline int tvh_vfs_fsid_match(tvh_fsid_t *a, tvh_fsid_t *b)
{
  if (a->fsid != 0 && b->fsid != 0)
    return a->fsid == b->fsid;
  if (a->fsid == 0 && b->fsid == 0)
    return strcmp(a->id, b->id) == 0;
  return 0;
}

#endif /* __TVH_VFS_H__ */
