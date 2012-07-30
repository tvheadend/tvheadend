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
  } else {

    if(homedir != NULL) {
      snprintf(buf, sizeof(buf), "%s/.hts", homedir);
      if(stat(buf, &st) == 0 || mkdir(buf, 0700) == 0) {
      
	snprintf(buf, sizeof(buf), "%s/.hts/tvheadend", homedir);
      
	if(stat(buf, &st) == 0 || mkdir(buf, 0700) == 0)
	  settingspath = strdup(buf);
      }
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
void
hts_settings_save(htsmsg_t *record, const char *pathfmt, ...)
{
  char path[256];
  char fullpath[256];
  char fullpath2[256];
  int x, l, fd;
  va_list ap;
  struct stat st;
  htsbuf_queue_t hq;
  htsbuf_data_t *hd;
  char *n;
  int ok;

  if(settingspath == NULL)
    return;

  va_start(ap, pathfmt);
  vsnprintf(path, sizeof(path), pathfmt, ap);
  va_end(ap);

  n = path;

  while(*n) {
    if(*n == ':' || *n == '?' || *n == '*' || *n > 127 || *n < 32)
      *n = '_';
    n++;
  }

  l = strlen(path);

  for(x = 0; x < l; x++) {
    if(path[x] == '/') {
      /* It's a directory here */

      path[x] = 0;
      snprintf(fullpath, sizeof(fullpath), "%s/%s", settingspath, path);

      if(stat(fullpath, &st) && mkdir(fullpath, 0700)) {
	tvhlog(LOG_ALERT, "settings", "Unable to create dir \"%s\": %s",
	       fullpath, strerror(errno));
	return;
      }
      path[x] = '/';
    }
  }

  snprintf(fullpath, sizeof(fullpath), "%s/%s.tmp", settingspath, path);

  if((fd = tvh_open(fullpath, O_CREAT | O_TRUNC | O_RDWR, 0700)) < 0) {
    tvhlog(LOG_ALERT, "settings", "Unable to create \"%s\" - %s",
	    fullpath, strerror(errno));
    return;
  }

  ok = 1;

  htsbuf_queue_init(&hq, 0);
  htsmsg_json_serialize(record, &hq, 1);
 
  TAILQ_FOREACH(hd, &hq.hq_q, hd_link)
    if(write(fd, hd->hd_data + hd->hd_data_off, hd->hd_data_len) != 
       hd->hd_data_len) {
      tvhlog(LOG_ALERT, "settings", "Failed to write file \"%s\" - %s",
	      fullpath, strerror(errno));
      ok = 0;
      break;
    }

  close(fd);

  snprintf(fullpath2, sizeof(fullpath2), "%s/%s", settingspath, path);

  if(ok)
    rename(fullpath, fullpath2);
  else
    unlink(fullpath);
	   
  htsbuf_queue_flush(&hq);
}

/**
 *
 */
static htsmsg_t *
hts_settings_load_one(const char *filename)
{
  struct stat st;
  int fd;
  char *mem;
  htsmsg_t *r;
  int n;

  if(stat(filename, &st) < 0)
    return NULL;

  if((fd = tvh_open(filename, O_RDONLY, 0)) < 0)
    return NULL;

  mem = malloc(st.st_size + 1);
  mem[st.st_size] = 0;

  n = read(fd, mem, st.st_size);
  close(fd);
  if(n == st.st_size)
    r = htsmsg_json_deserialize(mem);
  else
    r = NULL;

  free(mem);

  return r;
}

/**
 *
 */
static int
hts_settings_buildpath(char *dst, size_t dstsize, const char *fmt, va_list ap)
{
  char tmp[256];
  char *n = dst;

  if(settingspath == NULL)
     return -1;
  
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  if (*tmp != '/')
    snprintf(dst, dstsize, "%s/%s", settingspath, tmp);
  else
    strncpy(dst, tmp, dstsize);

  while(*n) {
    if(*n == ':' || *n == '?' || *n == '*' || *n > 127 || *n < 32)
      *n = '_';
    n++;
  }
  return 0;
}

/**
 *
 */
htsmsg_t *
hts_settings_load(const char *pathfmt, ...)
{
  char fullpath[256];
  char child[256];
  va_list ap;
  struct stat st;
  struct dirent **namelist, *d;
  htsmsg_t *r, *c;
  int n, i;

  va_start(ap, pathfmt);
  if(hts_settings_buildpath(fullpath, sizeof(fullpath), pathfmt, ap) < 0)
    return NULL;

  if(stat(fullpath, &st) != 0)
    return NULL;

  if(S_ISDIR(st.st_mode)) {

    if((n = scandir(fullpath, &namelist, NULL, NULL)) < 0)
      return NULL;
     
    r = htsmsg_create_map();

    for(i = 0; i < n; i++) {
      d = namelist[i];
      if(d->d_name[0] != '.') {
	snprintf(child, sizeof(child), "%s/%s", fullpath, d->d_name);
	c = hts_settings_load_one(child);
	if(c != NULL)
	  htsmsg_add_msg(r, d->d_name, c);
      }
      free(d);
    }
    free(namelist);

  } else {
    r = hts_settings_load_one(fullpath);
  }

  return r;
}

/**
 *
 */
void
hts_settings_remove(const char *pathfmt, ...)
{
  char fullpath[256];
  va_list ap;

  va_start(ap, pathfmt);
  if(hts_settings_buildpath(fullpath, sizeof(fullpath), pathfmt, ap) < 0)
    return;
  unlink(fullpath);
}


/**
 *
 */
int
hts_settings_open_file(int for_write, const char *pathfmt, ...)
{
  char path[256];
  char fullpath[256];
  int x, l;
  va_list ap;
  struct stat st;
  char *n;

  if(settingspath == NULL)
    return -1;

  va_start(ap, pathfmt);
  vsnprintf(path, sizeof(path), pathfmt, ap);
  va_end(ap);

  n = path;

  while(*n) {
    if(*n == ':' || *n == '?' || *n == '*' || *n > 127 || *n < 32)
      *n = '_';
    n++;
  }

  l = strlen(path);

  for(x = 0; x < l; x++) {
    if(path[x] == '/') {
      /* It's a directory here */

      path[x] = 0;
      snprintf(fullpath, sizeof(fullpath), "%s/%s", settingspath, path);

      if(stat(fullpath, &st) && mkdir(fullpath, 0700)) {
	tvhlog(LOG_ALERT, "settings", "Unable to create dir \"%s\": %s",
	       fullpath, strerror(errno));
	return -1;
      }
      path[x] = '/';
    }
  }

  snprintf(fullpath, sizeof(fullpath), "%s/%s", settingspath, path);

  int flags = for_write ? O_CREAT | O_TRUNC | O_WRONLY : O_RDONLY;

  return tvh_open(fullpath, flags, 0700);
}
