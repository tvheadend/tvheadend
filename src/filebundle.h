/*
 *  TV headend - File bundles
 *  Copyright (C) 2008 Andreas Öman, Adam Sutton
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

#ifndef __TVH_FILE_BUNDLE_H__
#define __TVH_FILE_BUNDLE_H__

#include <sys/types.h>
#include <stdint.h>
#include <dirent.h>
#include <stdio.h>

/* File bundle entry type */
enum filebundle_type
{
  FB_UNKNOWN,
  FB_FILE,
  FB_DIR
};

/* File bundle entry */
typedef struct filebundle_entry
{
  enum filebundle_type type;
  const char          *name;
  union {
    struct {
      struct filebundle_entry *child;
    } d;
    struct {
      const uint8_t *data;
      size_t  size;
      ssize_t orig;
    } f;
  };
  struct filebundle_entry *next;
} filebundle_entry_t;

/* File bundle directory entry */
typedef struct filebundle_dirent
{
  const char           *name;
  enum filebundle_type  type;
} fb_dirent;

/* Opaque types */
typedef struct filebundle_dir  fb_dir;
typedef struct filebundle_file fb_file;

/* Root of bundle */
extern filebundle_entry_t *filebundle_root;

/* Directory processing wrappers */
fb_dir    *fb_opendir  ( const char *path );
fb_dirent *fb_readdir  ( fb_dir *fb );
void       fb_closedir ( fb_dir *fb );

/* File processing wrappers */
// Note: all access is read-only
// Note: decompress is only for compressed filebundles,
//       not direct disk access
fb_file *fb_open2   ( const fb_dir *dir, const char *name, int decompress, int compress );
fb_file *fb_open    ( const char *path, int decompress, int compress );
void     fb_close   ( fb_file *fp );
size_t   fb_size    ( fb_file *fp );
int      fb_gzipped ( fb_file *fp );
int      fb_eof     ( fb_file *fp );
ssize_t  fb_read    ( fb_file *fp, void *buf, size_t count );
char    *fb_gets    ( fb_file *fp, void *buf, size_t count );

#endif /* __TVH_FILE_BUNDLE_H__ */
