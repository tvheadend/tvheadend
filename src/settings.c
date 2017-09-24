/*
 *  Functions for storing program settings
 *  Copyright (C) 2008 Andreas Öman
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include "htsmsg.h"
#include "htsmsg_binary.h"
#include "htsmsg_json.h"
#include "settings.h"
#include "tvheadend.h"
#include "filebundle.h"

static char *settingspath = NULL;

/**
 *
 */
const char *
hts_settings_get_root(void)
{
  return settingspath;
}

/**
 *
 */
void
hts_settings_init(const char *confpath)
{
  if (confpath)
    settingspath = realpath(confpath, NULL);
}

/**
 *
 */
void
hts_settings_done(void)
{
  free(settingspath);
}

/**
 *
 */
int
hts_settings_makedirs ( const char *inpath )
{
  size_t x = strlen(inpath) - 1;
  char *path = alloca(x + 2);

  if (path == NULL) return -1;
  strcpy(path, inpath);

  while (x) {
    if (path[x] == '/') {
      path[x] = 0;
      break;
    }
    x--;
  }
  return makedirs(LS_SETTINGS, path, 0700, 1, -1, -1);
}

/**
 *
 */
static void
_hts_settings_buildpath
  (char *dst, size_t dstsize, const char *fmt, va_list ap, const char *prefix)
{
  char tmp[PATH_MAX];
  char *n = dst;

  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  if (*tmp != '/' && prefix)
    snprintf(dst, dstsize, "%s/%s", prefix, tmp);
  else
    strncpy(dst, tmp, dstsize);

  while(*n) {
    if(*n == ':' || *n == '?' || *n == '*' || *n > 127 || *n < 32)
      *n = '_';
    n++;
  }
}

int
hts_settings_buildpath
  (char *dst, size_t dstsize, const char *fmt, ...)
{
  va_list va;
  if (!settingspath)
    return 1;
  va_start(va, fmt);
  _hts_settings_buildpath(dst, dstsize, fmt, va, settingspath);
  va_end(va);
  return 0;
}

/**
 *
 */
void
hts_settings_save(htsmsg_t *record, const char *pathfmt, ...)
{
  char path[PATH_MAX];
  char tmppath[PATH_MAX];
  int fd;
  va_list ap;
  htsbuf_queue_t hq;
  htsbuf_data_t *hd;
  int ok, r, pack;

  if(settingspath == NULL)
    return;

  /* Clean the path */
  va_start(ap, pathfmt);
  _hts_settings_buildpath(path, sizeof(path), pathfmt, ap, settingspath);
  va_end(ap);

  /* Create directories */
  if (hts_settings_makedirs(path)) return;

  tvhdebug(LS_SETTINGS, "saving to %s", path);

  /* Create tmp file */
  snprintf(tmppath, sizeof(tmppath), "%s.tmp", path);
  if((fd = tvh_open(tmppath, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR)) < 0) {
    tvhalert(LS_SETTINGS, "Unable to create \"%s\" - %s",
	     tmppath, strerror(errno));
    return;
  }

  /* Store data */
#if ENABLE_ZLIB
  pack = strstr(path, "/muxes/") != NULL && /* ugly, redesign API */
         strstr(path, "/networks/") != NULL &&
         strstr(path, "/input/") != NULL;
#else
  pack = 0;
#endif
  ok = 1;

  if (!pack) {
    htsbuf_queue_init(&hq, 0);
    htsmsg_json_serialize(record, &hq, 1);
    TAILQ_FOREACH(hd, &hq.hq_q, hd_link)
      if(tvh_write(fd, hd->hd_data + hd->hd_data_off, hd->hd_data_len)) {
        tvhalert(LS_SETTINGS, "Failed to write file \"%s\" - %s",
                 tmppath, strerror(errno));
        ok = 0;
        break;
      }
    htsbuf_queue_flush(&hq);
  } else {
#if ENABLE_ZLIB
    void *msgdata = NULL;
    size_t msglen;
    r = htsmsg_binary_serialize(record, &msgdata, &msglen, 2*1024*1024);
    if (!r && msglen >= 4) {
      r = tvh_gzip_deflate_fd_header(fd, msgdata + 4, msglen - 4, NULL, 3);
      if (r)
        ok = 0;
    } else {
      tvhalert(LS_SETTINGS, "Unable to pack the configuration data \"%s\"", path);
    }
    free(msgdata);
#endif
  }
  close(fd);

  /* Move */
  if(ok) {
    r = rename(tmppath, path);
    if (r && errno == EISDIR) {
      rmtree(path);
      r = rename(tmppath, path);
    }
    if (r)
      tvhalert(LS_SETTINGS, "Unable to rename file \"%s\" to \"%s\" - %s",
	       tmppath, path, strerror(errno));
  
  /* Delete tmp */
  } else
    unlink(tmppath);
}

/**
 *
 */
static htsmsg_t *
hts_settings_load_one(const char *filename)
{
  ssize_t n, size;
  char *mem;
  fb_file *fp;
  htsmsg_t *r = NULL;

  /* Open */
  if (!(fp = fb_open(filename, 1, 0))) return NULL;
  size = fb_size(fp);

  /* Load data */
  mem    = malloc(size+1);
  n      = fb_read(fp, mem, size);
  if (n >= 0) mem[n] = 0;

  /* Decode */
  if(n == size) {
    if (size > 12 && memcmp(mem, "\xff\xffGZIP00", 8) == 0) {
#if ENABLE_ZLIB
      uint32_t orig = (mem[8] << 24) | (mem[9] << 16) | (mem[10] << 8) | mem[11];
      if (orig > 10*1024*1024U) {
        tvhalert(LS_SETTINGS, "too big gzip for %s", filename);
        r = NULL;
      } else if (orig > 0) {
        uint8_t *unpacked = tvh_gzip_inflate((uint8_t *)mem + 12, size - 12, orig);
        if (unpacked) {
          r = htsmsg_binary_deserialize(unpacked, orig, NULL);
          free(unpacked);
        }
      }
#endif
    } else {
      r = htsmsg_json_deserialize(mem);
    }
  }

  /* Close */
  fb_close(fp);
  free(mem);

  return r;
}

/**
 *
 */
static htsmsg_t *
hts_settings_load_path(const char *fullpath, int depth)
{
  char child[PATH_MAX];
  const char *name;
  struct filebundle_stat st;
  fb_dirent **namelist, *d;
  htsmsg_t *r, *c;
  int n, i;

  /* Invalid */
  if (fb_stat(fullpath, &st)) return NULL;

  /* Directory */
  if (st.is_dir) {

    /* Get file list */
    if((n = fb_scandir(fullpath, &namelist)) < 0)
      return NULL;

    /* Read files */
    r = htsmsg_create_map();
    for(i = 0; i < n; i++) {
      d = namelist[i];
      name = d->name;
      if(name[0] != '.' && name[0] && name[strlen(name)-1] != '~') {

        snprintf(child, sizeof(child), "%s/%s", fullpath, d->name);
        if(d->type == FB_DIR && depth > 0) {
          c = hts_settings_load_path(child, depth - 1);
        } else {
          c = hts_settings_load_one(child);
        }
        if(c != NULL)
          htsmsg_add_msg(r, d->name, c);

      }
      free(d);
    }
    free(namelist);

  /* File */
  } else {
    r = hts_settings_load_one(fullpath);
  }

  return r;
}

/**
 *
 */
static htsmsg_t *
hts_settings_vload(const char *pathfmt, va_list ap, int depth)
{
  htsmsg_t *ret = NULL;
  char fullpath[PATH_MAX];
  va_list ap2;
  va_copy(ap2, ap);

  /* Try normal path */
  _hts_settings_buildpath(fullpath, sizeof(fullpath), 
                          pathfmt, ap, settingspath);
  ret = hts_settings_load_path(fullpath, depth);

  /* Try bundle path */
  if (!ret && *pathfmt != '/') {
    _hts_settings_buildpath(fullpath, sizeof(fullpath),
                            pathfmt, ap2, "data/conf");
    ret = hts_settings_load_path(fullpath, depth);
  }

  va_end(ap2);

  return ret;
}


/**
 *
 */
htsmsg_t *
hts_settings_load(const char *pathfmt, ...)
{
  va_list ap;
  va_start(ap, pathfmt);
  htsmsg_t *r = hts_settings_vload(pathfmt, ap, 0);
  va_end(ap);
  return r;
}


/**
 *
 */
htsmsg_t *
hts_settings_load_r(int depth, const char *pathfmt, ...)
{
  va_list ap;
  va_start(ap, pathfmt);
  htsmsg_t *r = hts_settings_vload(pathfmt, ap, depth);
  va_end(ap);
  return r;
}

/**
 *
 */
void
hts_settings_remove(const char *pathfmt, ...)
{
  char fullpath[PATH_MAX];
  va_list ap;
  struct stat st;

  va_start(ap, pathfmt);
  _hts_settings_buildpath(fullpath, sizeof(fullpath),
                          pathfmt, ap, settingspath);
  va_end(ap);
  if (stat(fullpath, &st) == 0) {
    if (S_ISDIR(st.st_mode))
      rmtree(fullpath);
    else {
      unlink(fullpath);
      while (rmdir(dirname(fullpath)) == 0);
    }
  }
}

/**
 *
 */
int
hts_settings_open_file(int for_write, const char *pathfmt, ...)
{
  char path[PATH_MAX];
  int flags;
  va_list ap;

  /* Build path */
  va_start(ap, pathfmt);
  _hts_settings_buildpath(path, sizeof(path), pathfmt, ap, settingspath);
  va_end(ap);

  /* Create directories */
  if (for_write)
    if (hts_settings_makedirs(path)) return -1;

  /* Open file */
  flags = for_write ? O_CREAT | O_TRUNC | O_WRONLY : O_RDONLY;

  return tvh_open(path, flags, S_IRUSR | S_IWUSR);
}

/*
 * Check if a path exists
 */
int
hts_settings_exists ( const char *pathfmt, ... )
{
  va_list ap;
  char path[PATH_MAX];
  struct stat st;

  /* Build path */
  va_start(ap, pathfmt);
  _hts_settings_buildpath(path, sizeof(path), pathfmt, ap, settingspath);
  va_end(ap);

  return (stat(path, &st) == 0);
}
