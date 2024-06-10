/*
 * Copyright (c) 2017 Jaroslav Kysela <perex@perex.cz>
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

#include "tvheadend.h"
#include "tvhvfs.h"
#if !defined(PLATFORM_DARWIN) && !defined(PLATFORM_FREEBSD)
#include <sys/sysmacros.h>
#endif
#include <sys/stat.h>

int tvh_vfs_fsid_build(const char* path, struct statvfs* vfs, tvh_fsid_t* dst) {
  struct statvfs _vfs;
  struct stat    st;

  if (vfs == NULL)
    vfs = &_vfs;

  if (statvfs(path, vfs) == -1)
    return -1;

  dst->fsid  = tvh_fsid(vfs->f_fsid);
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
