/*
 *  File system management
 *  Copyright (C) 2015 Jaroslav Kysela
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
