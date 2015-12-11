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
#define tvh_fsid_t uint64_t
#define tvh_fsid(x) (((uint64_t)x.__val[0] << 32) | (x.__val[1]))
#else
#include <sys/statvfs.h>
#define tvh_fsid_t unsigned long
#define tvh_fsid(x) (x)
#endif

#endif /* __TVH_VFS_H__ */
