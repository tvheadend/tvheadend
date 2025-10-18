/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2017 Jaroslav Kysela <perex@perex.cz>
 */

#include "tvheadend.h"
#include "tvhvfs.h"
#if !defined(PLATFORM_DARWIN) && !defined(PLATFORM_FREEBSD)
#include <sys/sysmacros.h>
#endif
#include <sys/stat.h>

int tvh_vfs_fsid_build(const char *path, struct statvfs *vfs, tvh_fsid_t *dst)
{
  struct statvfs _vfs;
  struct stat st;

  if (vfs == NULL)
    vfs = &_vfs;

  if (statvfs(path, vfs) == -1)
    return -1;

  dst->fsid = tvh_fsid(vfs->f_fsid);
  dst->id[0] = '\0';
  if (dst->fsid == 0) {
    if (stat(path, &st) == -1)
      return -1;
    if (major(st.st_dev) == 0 && minor(st.st_dev) == 0)
      return -1;
    snprintf(dst->id, sizeof(dst->id), "dev=%u:%u", major(st.st_dev), minor(st.st_dev));
  }

  return 0;
}
