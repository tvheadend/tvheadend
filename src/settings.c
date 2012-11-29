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
#include "htsmsg_json.h"
#include "settings.h"
#include "tvheadend.h"
#include "filebundle.h"

static char *settingspath;

/**
 *
 */
const char *
hts_settings_get_root(void)
{
  return settingspath ?: "No settings dir";
}

/**
 *
 */
void
hts_settings_init(const char *confpath)
{
  char buf[256];
  const char *homedir = getenv("HOME");
  struct stat st;

  if(confpath != NULL) {
    settingspath = strdup(confpath);
  } else if(homedir != NULL) {
    snprintf(buf, sizeof(buf), "%s/.hts", homedir);
    if(stat(buf, &st) == 0 || mkdir(buf, 0700) == 0) {
	    snprintf(buf, sizeof(buf), "%s/.hts/tvheadend", homedir);
	    if(stat(buf, &st) == 0 || mkdir(buf, 0700) == 0)
	      settingspath = strdup(buf);
    }
  }
  if(settingspath == NULL) {
    tvhlog(LOG_ALERT, "START", 
	   "No configuration path set, "
	   "settings and configuration will not be saved");
  } else if(access(settingspath, R_OK | W_OK)) {
    tvhlog(LOG_ALERT, "START", 
	   "Configuration path %s is not read/write:able "
	   "by user (UID:%d, GID:%d) -- %s",
	   settingspath, getuid(), getgid(), strerror(errno));
    settingspath = NULL;
  }
}

/**
 *
 */
int
hts_settings_makedirs ( const char *inpath )
{
  size_t x = strlen(inpath) - 1;
  char path[512];
  strcpy(path, inpath);

  while (x) {
    if (path[x] == '/') {
      path[x] = 0;
      break;
    }
    x--;
  }
  return makedirs(path, 0700);
}

/**
 *
 */
static void
hts_settings_buildpath
  (char *dst, size_t dstsize, const char *fmt, va_list ap, const char *prefix)
{
  char tmp[256];
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

/**
 *
 */
void
hts_settings_save(htsmsg_t *record, const char *pathfmt, ...)
{
  char path[256];
  char tmppath[256];
  int fd;
  va_list ap;
  htsbuf_queue_t hq;
  htsbuf_data_t *hd;
  int ok;

  if(settingspath == NULL)
    return;

  /* Clean the path */
  va_start(ap, pathfmt);
  hts_settings_buildpath(path, sizeof(path), pathfmt, ap, settingspath);
  va_end(ap);

  /* Create directories */
  if (hts_settings_makedirs(path)) return;

  /* Create tmp file */
  snprintf(tmppath, sizeof(tmppath), "%s.tmp", path);
  if((fd = tvh_open(tmppath, O_CREAT | O_TRUNC | O_RDWR, 0700)) < 0) {
    tvhlog(LOG_ALERT, "settings", "Unable to create \"%s\" - %s",
	    tmppath, strerror(errno));
    return;
  }

  /* Store data */
  ok = 1;
  htsbuf_queue_init(&hq, 0);
  htsmsg_json_serialize(record, &hq, 1);
  TAILQ_FOREACH(hd, &hq.hq_q, hd_link)
    if(write(fd, hd->hd_data + hd->hd_data_off, hd->hd_data_len) != 
       hd->hd_data_len) {
      tvhlog(LOG_ALERT, "settings", "Failed to write file \"%s\" - %s",
	      tmppath, strerror(errno));
      ok = 0;
      break;
    }
  close(fd);
  htsbuf_queue_flush(&hq);

  /* Move */
  if(ok) {
    rename(tmppath, path);
  
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
  ssize_t n;
  char *mem;
  fb_file *fp;
  htsmsg_t *r = NULL;

  /* Open */
  if (!(fp = fb_open(filename, 1, 0))) return NULL;
  
  /* Load data */
  mem    = malloc(fb_size(fp)+1);
  n      = fb_read(fp, mem, fb_size(fp));
  if (n >= 0) mem[n] = 0;

  /* Decode */
  if(n == fb_size(fp))
    r = htsmsg_json_deserialize(mem);

  /* Close */
  fb_close(fp);
  free(mem);

  return r;
}

/**
 *
 */
static htsmsg_t *
_hts_settings_load(const char *fullpath)
{
  char child[256];
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
      if(d->name[0] != '.') {
	      snprintf(child, sizeof(child), "%s/%s", fullpath, d->name);
 	      if ((c = hts_settings_load_one(child)))
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
htsmsg_t *
hts_settings_load(const char *pathfmt, ...)
{
  htsmsg_t *ret = NULL;
  char fullpath[256];
  va_list ap;

  /* Try normal path */
  va_start(ap, pathfmt);
  hts_settings_buildpath(fullpath, sizeof(fullpath), 
                         pathfmt, ap, settingspath);
  va_end(ap);
  ret = _hts_settings_load(fullpath);

  /* Try bundle path */
  if (!ret && *pathfmt != '/') {
    va_start(ap, pathfmt);
    hts_settings_buildpath(fullpath, sizeof(fullpath),
                           pathfmt, ap, "data/conf");
    va_end(ap);
    ret = _hts_settings_load(fullpath);
  }
  
  return ret;
}

/**
 *
 */
void
hts_settings_remove(const char *pathfmt, ...)
{
  char fullpath[256];
  va_list ap;
  struct stat st;

  va_start(ap, pathfmt);
   hts_settings_buildpath(fullpath, sizeof(fullpath),
                          pathfmt, ap, settingspath);
  va_end(ap);
  if (stat(fullpath, &st) == 0) {
    if (S_ISDIR(st.st_mode))
      rmdir(fullpath);
    else
      unlink(fullpath);
  }
}

/**
 *
 */
int
hts_settings_open_file(int for_write, const char *pathfmt, ...)
{
  char path[256];
  va_list ap;

  /* Build path */
  va_start(ap, pathfmt);
  hts_settings_buildpath(path, sizeof(path), pathfmt, ap, settingspath);
  va_end(ap);

  /* Create directories */
  if (for_write)
    if (hts_settings_makedirs(path)) return -1;

  /* Open file */
  int flags = for_write ? O_CREAT | O_TRUNC | O_WRONLY : O_RDONLY;

  return tvh_open(path, flags, 0700);
}
