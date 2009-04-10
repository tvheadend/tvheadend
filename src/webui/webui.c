/*
 *  tvheadend, WEBUI / HTML user interface
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

#define _GNU_SOURCE /* for splice() */
#include <fcntl.h>

#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/sendfile.h>

#include "tvhead.h"
#include "access.h"
#include "http.h"
#include "webui.h"
#include "dvr/dvr.h"
#include "filebundle.h"

struct filebundle *filebundles;

/**
 *
 */
static int
is_client_simple(http_connection_t *hc)
{
  char *c;

  if((c = http_arg_get(&hc->hc_args, "UA-OS")) != NULL) {
    if(strstr(c, "Windows CE") || strstr(c, "Pocket PC"))
      return 1;
  }

  if((c = http_arg_get(&hc->hc_args, "x-wap-profile")) != NULL) {
    return 1;
  }
  return 0;
}



/**
 * Root page, we direct the client to different pages depending
 * on if it is a full blown browser or just some mobile app
 */
static int
page_root(http_connection_t *hc, const char *remain, void *opaque)
{
  if(is_client_simple(hc)) {
    http_redirect(hc, "/simple.html");
  } else {
    http_redirect(hc, "/extjs.html");
  }
  return 0;
}

/**
 * Static download of a file from the filesystem
 */
static int
page_static_file(http_connection_t *hc, const char *remain, void *opaque)
{
  const char *base = opaque;

  int fd;
  char path[500];
  struct stat st;
  const char *content = NULL, *postfix;

  if(strstr(remain, ".."))
    return HTTP_STATUS_BAD_REQUEST;

  postfix = strrchr(remain, '.');
  if(postfix != NULL) {
    postfix++;
    if(!strcmp(postfix, "js"))
      content = "text/javascript; charset=UTF-8";
  }

  snprintf(path, sizeof(path), "%s/%s", base, remain);

  if((fd = open(path, O_RDONLY)) < 0)
    return 404;

  if(fstat(fd, &st) < 0) {
    close(fd);
    return 404;
  }

  http_send_header(hc, 200, content, st.st_size, NULL, NULL, 10);
  sendfile(hc->hc_fd, fd, NULL, st.st_size);
  close(fd);
  return 0;
}


/**
 * Static download of a file from an embedded filebundle
 */
static int
page_static_bundle(http_connection_t *hc, const char *remain, void *opaque)
{
  const struct filebundle *fb = opaque;
  const struct filebundle_entry *fbe;
  const char *content = NULL, *postfix;
  int n;

  postfix = strrchr(remain, '.');
  if(postfix != NULL) {
    postfix++;
    if(!strcmp(postfix, "js"))
      content = "text/javascript; charset=UTF-8";
  }

  for(fbe = fb->entries; fbe->filename != NULL; fbe++) {
    if(!strcmp(fbe->filename, remain)) {

      http_send_header(hc, 200, content, fbe->size, 
		       fbe->original_size == -1 ? NULL : "gzip", NULL, 10);
      /* ignore return value */
      n = write(hc->hc_fd, fbe->data, fbe->size);
      return 0;
    }
  }
  return 404;
}


/**
 * Download a recorded file
 */
static int
page_dvrfile(http_connection_t *hc, const char *remain, void *opaque)
{
  int fd;
  struct stat st;
  const char *content = NULL, *postfix, *range;
  dvr_entry_t *de;
  char *fname;

  if(remain == NULL)
    return 404;

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_RECORDER)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  de = dvr_entry_find_by_id(atoi(remain));
  if(de == NULL || de->de_filename == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  fname = strdup(de->de_filename);
  pthread_mutex_unlock(&global_lock);

  postfix = strrchr(remain, '.');
  if(postfix != NULL) {
    postfix++;
    if(!strcmp(postfix, "mkv"))
      content = "video/x-matroska";
  }

  fd = open(fname, O_RDONLY);
  free(fname);
  if(fd < 0)
    return 404;

  if(fstat(fd, &st) < 0) {
    close(fd);
    return 404;
  }

  
  range = http_arg_get(&hc->hc_args, "Range");
#if 0
  if(range != NULL) {
    printf("Range req: %s\n", range);
  }
#endif

  http_send_header(hc, 200, content, st.st_size, NULL, NULL, 10);
  sendfile(hc->hc_fd, fd, NULL, st.st_size);
  close(fd);
  return 0;
}



/**
 *
 */
static void
webui_static_content(const char *content_path, const char *http_path,
		     const char *source)
{
  char path[256];
  struct stat st;
  struct filebundle *fb;
 
  if(content_path != NULL) {
    snprintf(path, sizeof(path), "%s/%s", content_path, source);
    
    if(!stat(path, &st) && S_ISDIR(st.st_mode)) {

      http_path_add(http_path, strdup(source), 
		    page_static_file, ACCESS_WEB_INTERFACE);
      return;
    }
  }

  for(fb = filebundles; fb != NULL; fb = fb->next) {
    if(!strcmp(source, fb->prefix)) {
      http_path_add(http_path, fb, 
		    page_static_bundle, ACCESS_WEB_INTERFACE);
      return;
    }
  }

  tvhlog(LOG_ERR, "webui", 
	 "No source path providing HTTP content: %s", http_path);
}

/**
 * WEB user interface
 */
void
webui_init(const char *contentpath)
{
  http_path_add("/", NULL, page_root, ACCESS_WEB_INTERFACE);

  http_path_add("/dvrfile", NULL, page_dvrfile, ACCESS_WEB_INTERFACE);

  webui_static_content(contentpath, "/static",        "src/webui/static");
  webui_static_content(contentpath, "/docs",          "docs/html");
  webui_static_content(contentpath, "/docresources",  "docs/docresources");

   //  simpleui_start();
  extjs_start();
  comet_init();

}
