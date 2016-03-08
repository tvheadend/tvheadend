/*
 *  TV headend - File bundles
 *  Copyright (C) 2012 Adam Sutton
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
#include "filebundle.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

/* **************************************************************************
 * Opaque data types
 * *************************************************************************/

/* File bundle dir handle */
struct filebundle_dir
{
  fb_type   type;
  fb_dirent dirent;
  union {
    struct {
      char *root;
      DIR  *cur;
    } d;
    struct {
      const filebundle_entry_t *root;
      const filebundle_entry_t *cur;
    } b;
  };
};

/* File bundle file handle */
struct filebundle_file
{
  fb_type type;
  size_t  size;
  int     gzip;
  uint8_t *buf;
  size_t  pos;
  union {
    struct {
      FILE  *cur;
    } d;
    struct {
      const  filebundle_entry_t *root;
    } b;
  };
};

/* **************************************************************************
 * Miscellanous
 * *************************************************************************/

/* Get path stats */
// TODO: this could do with being more efficient
int fb_stat ( const char *path, struct filebundle_stat *st )
{
  int ret = 1;
  fb_dir *dir;
  fb_file *fp;

  if (*path == '/') {
    struct stat _st;
    if (!lstat(path, &_st)) {
      st->type   = FB_DIRECT;
      st->is_dir = S_ISDIR(_st.st_mode) ? 1 : 0;
      st->size   = _st.st_size;
      ret        = 0;
    }
  } else if ((dir = fb_opendir(path))) {
    st->type   = dir->type;
    st->is_dir = 1;
    st->size   = 0;
    ret = 0;
    fb_closedir(dir);
  } else if ((fp = fb_open(path, 0, 0))) {
    st->type   = fp->type;
    st->is_dir = 0;
    st->size   = fp->size;
    ret = 0;
    fb_close(fp);
  }
  return ret;
}

/* **************************************************************************
 * Directory processing
 * *************************************************************************/

/* Open directory (directly) */
static fb_dir *_fb_opendir ( const char *root, const char *path )
{
  DIR *dir;
  char buf[512];
  fb_dir *ret = NULL;

  /* Pre-pend root */
  if (root) {
    snprintf(buf, sizeof(buf), "%s/%s", root, path);
    path = buf;
  }

  /* Open */
  if ((dir = opendir(path))) {
    ret         = calloc(1, sizeof(fb_dir));
    ret->type   = FB_DIRECT;
    ret->d.root = strdup(path);
    ret->d.cur  = dir;
  }
  return ret;
}

/* Open directory */
fb_dir *fb_opendir ( const char *path )
{
  fb_dir *ret = NULL;

  /* In-direct (search) */
  if (*path != '/') {

    /* Bundle */
#if ENABLE_BUNDLE
    char *tmp1, *tmp2, *tmp3 = NULL;
    tmp1 = strdup(path);
    tmp2 = strtok_r(tmp1, "/", &tmp3);
    const filebundle_entry_t *fb = filebundle_root;
    while (fb && tmp2) {
      if (fb->type == FB_DIR && !strcmp(fb->name, tmp2)) {
        tmp2 = strtok_r(NULL, "/", &tmp3);
        if (tmp2) fb = fb->d.child;
      } else {
        fb = fb->next;
      }
    }
    free(tmp1);

    /* Found */
    if (fb) {
      ret = calloc(1, sizeof(fb_dir));
      ret->type   = FB_BUNDLE;
      ret->b.root = fb;
      ret->b.cur  = fb->d.child;
    }
#endif

    /* Local */
    if (!ret) ret = _fb_opendir(tvheadend_cwd, path);

    /* System */
    if (!ret) ret = _fb_opendir(TVHEADEND_DATADIR, path);

  /* Direct */
  } else {
    ret = _fb_opendir(NULL, path);
  }

  return ret;
}

/* Close directory */
void fb_closedir ( fb_dir *dir )
{
  if (dir->type == FB_DIRECT) {
    closedir(dir->d.cur);
    free(dir->d.root);
  }
  free(dir);
}

/* Iterate through entries */
fb_dirent *fb_readdir ( fb_dir *dir )
{
  fb_dirent *ret = NULL;
  if (dir->type == FB_BUNDLE) {
    if (dir->b.cur) {
      strncpy(dir->dirent.name, dir->b.cur->name, sizeof(dir->dirent.name)-1);
      dir->dirent.name[sizeof(dir->dirent.name)-1] = '\0';
      dir->dirent.type = dir->b.cur->type;
      dir->b.cur       = dir->b.cur->next;
      ret              = &dir->dirent;
    }
    
  } else {
    struct dirent *de = readdir(dir->d.cur);
    if (de) {
      struct stat st;
      char buf[512];
      snprintf(buf, sizeof(buf), "%s/%s", dir->d.root, de->d_name);
      strcpy(dir->dirent.name, de->d_name);
      dir->dirent.type = FB_UNKNOWN;
      if (!lstat(buf, &st))
        dir->dirent.type = S_ISDIR(st.st_mode) ? FB_DIR : FB_FILE;
      ret              = &dir->dirent;
    }
  }
  return ret;
}

/* Get entry list */
int fb_scandir ( const char *path, fb_dirent ***list )
{
  int i, ret = -1;
  struct dirent **de;
  fb_dir *dir;

  if (!(dir = fb_opendir(path))) return -1;

  /* Direct */
  if (dir->type == FB_DIRECT) {
    if ((ret = scandir(dir->d.root, &de, NULL, NULL)) > 0) {
      *list = malloc(sizeof(fb_dirent*)*ret);
      for (i = 0; i < ret; i++) {
        (*list)[i] = calloc(1, sizeof(fb_dirent));
        strcpy((*list)[i]->name, de[i]->d_name);
        switch(de[i]->d_type) {
          case DT_DIR: 
            (*list)[i]->type = FB_DIR;
            break;
          case DT_REG:
            (*list)[i]->type = FB_FILE;
            break;
          default: {
            struct stat st;
            char buf[512];
            snprintf(buf, sizeof(buf), "%s/%s", dir->d.root, de[i]->d_name);
            if (!lstat(buf, &st))
              (*list)[i]->type = S_ISDIR(st.st_mode) ? FB_DIR : FB_FILE;
            break;
          }
        }
        free(de[i]);
      }
      free(de);
    }

  /* Bundle */
  } else {
    const filebundle_entry_t *fb;
    ret = dir->b.root->d.count;
    fb  = dir->b.root->d.child;
    *list = malloc(ret * sizeof(fb_dirent*));
    i = 0;
    while (fb) {
      (*list)[i] = calloc(1, sizeof(fb_dirent));
      strncpy((*list)[i]->name, fb->name, sizeof((*list)[i]->name));
      (*list)[i]->name[sizeof((*list)[i]->name)-1] = '\0';
      fb = fb->next;
      i++;
    }
  }

  /* Close */
  fb_closedir(dir);

  return ret;
}

/* **************************************************************************
 * Directory processing
 * *************************************************************************/

/* Open file (with dir and name) */
// Note: decompress is only used on bundled (not direct) files that were
//       compressed at the time the bundle was generated, 1 can safely
//       be passed in though and will be ignored if this is not the case
// Note: compress will work on EITHER type (but will be ignored for already
//       compressed bundles)
fb_file *fb_open2 
  ( const fb_dir *dir, const char *name, int decompress, int compress )
{
  assert(!decompress || !compress);
  fb_file *ret = NULL;

  /* Bundle file */
  if (dir->type == FB_BUNDLE) {
    const filebundle_entry_t *fb = dir->b.root->d.child;
    while (fb) {
      if (!strcmp(name, fb->name)) break;
      fb = fb->next;
    }
    if (fb) {
      ret               = calloc(1, sizeof(fb_file));
      ret->type         = FB_BUNDLE;
      ret->size         = fb->f.size;
      ret->gzip         = fb->f.orig != -1;
      ret->b.root       = fb;

      /* Inflate the file */
      if (fb->f.orig != -1 && decompress) {
#if (ENABLE_ZLIB && ENABLE_BUNDLE)
        ret->gzip = 0;
        ret->size = fb->f.orig;
        ret->buf  = tvh_gzip_inflate(fb->f.data, fb->f.size, fb->f.orig);
        if (!ret->buf) {
          free(ret);
          ret = NULL;
        }
#else
        free(ret);
        ret = NULL;
#endif
      }
    }

  /* Direct file */
  } else {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir->d.root, name);
    FILE *fp = tvh_fopen(path, "rb");
    if (fp) {
      struct stat st;
      stat(path, &st);
      ret         = calloc(1, sizeof(fb_file));
      ret->type   = FB_DIRECT;
      ret->size   = st.st_size;
      ret->gzip   = 0;
      ret->d.cur  = fp;
    }
  }

  /* Compress */
#if ENABLE_ZLIB
  if (ret && !ret->gzip && compress) {
    ret->gzip = 1;
      
    /* Get data */
    if (ret->type == FB_BUNDLE) {
      const uint8_t *data;
      data     = ret->b.root->f.data;
      ret->buf = tvh_gzip_deflate(data, ret->size, &ret->size);
    } else {
      uint8_t *data = malloc(ret->size);
      ssize_t c = fread(data, 1, ret->size, ret->d.cur);
      if (c == ret->size)
        ret->buf = tvh_gzip_deflate(data, ret->size, &ret->size);
      fclose(ret->d.cur);
      ret->d.cur = NULL;
      free(data);
    }
  
    /* Cleanup */
    if (!ret->buf) {
      free(ret);
      ret = NULL; 
    }
  }
#endif

  return ret;
}

/* Open file */
fb_file *fb_open ( const char *path, int decompress, int compress )
{
  fb_file *ret = NULL;
  fb_dir *dir = NULL;
  char *tmp = strdup(path);
  char *pos = strrchr(tmp, '/');
  if (!pos) {
    free(tmp);
    return NULL;
  }
    
  /* Find directory */
  *pos = '\0';
  dir  = fb_opendir(tmp);
  if (!dir) {
    free(tmp);
    return NULL;
  }

  /* Open */
  ret = fb_open2(dir, pos+1, decompress, compress);
  fb_closedir(dir);
  free(tmp);
  return ret;
}

/* Close file */
void fb_close ( fb_file *fp )
{
  if (fp->type == FB_DIRECT && fp->d.cur) {
    fclose(fp->d.cur);
  }
  if (fp->buf)
    free(fp->buf);
  free(fp);
}

/* Get the files size */
size_t fb_size ( fb_file *fp )
{
  return fp->size;
}

/* Check if compressed */
int fb_gzipped ( fb_file *fp )
{
  return fp->gzip;
}

/* Check for EOF */
int fb_eof ( fb_file *fp )
{
  return fp->pos >= fp->size;
}

/* Read some data */
ssize_t fb_read ( fb_file *fp, void *buf, size_t count )
{
  if (fb_eof(fp)) {
    return -1;
  } else if (fp->buf) {
    count = MIN(count, fp->size - fp->pos);
    memcpy(buf, fp->buf + fp->pos, count);
    fp->pos += count;
  } else if (fp->type == FB_DIRECT) {
    count = fread(buf, 1, count, fp->d.cur);
    if (count <= 0)
      return -1;
    fp->pos += count;
  } else {
    count = MIN(count, fp->b.root->f.size - fp->pos);
    memcpy(buf, fp->b.root->f.data + fp->pos, count);
    fp->pos += count;
  }
  return count;
}

/* Read a line */
char *fb_gets ( fb_file *fp, void *buf, size_t count )
{
  ssize_t c = 0, err;
  while ((err = fb_read(fp, buf+c, 1)) > 0 && c < (count-1)) {
    char b = ((char*)buf)[c];
    c++;
    if (b == '\n' || b == '\0') break;
  }
  if (err < 0) return NULL;
  ((char*)buf)[c] = '\0';
  return buf;
}
