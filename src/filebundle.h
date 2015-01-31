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

/* Bundle or Direct */
typedef enum filebundle_handle_type
{
  FB_BUNDLE,
  FB_DIRECT
} fb_type;

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
  enum filebundle_type     type;
  const char              *name;
  const struct filebundle_entry *next;
  union {
    struct {
      size_t count;
      const struct filebundle_entry *child;
    } d;
    struct {
      const uint8_t *data;
      size_t  size;
      ssize_t orig;
    } f;
  };
} filebundle_entry_t;

/* File bundle directory entry */
typedef struct filebundle_dirent
{
  char                 name[256];
  enum filebundle_type type;
} fb_dirent;

/* File bundle stat */
struct filebundle_stat
{
  fb_type  type;
  uint8_t  is_dir;
  size_t   size;
};

/* Opaque types */
typedef struct filebundle_dir  fb_dir;
typedef struct filebundle_file fb_file;

/* Root of bundle */
extern const filebundle_entry_t * const filebundle_root;

/* Miscellaneous */
int fb_stat ( const char *path, struct filebundle_stat *st );

/* Directory processing wrappers */
fb_dir    *fb_opendir  ( const char *path );
fb_dirent *fb_readdir  ( fb_dir *fb );
void       fb_closedir ( fb_dir *fb );
int        fb_scandir  ( const char *path, fb_dirent ***list );

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
