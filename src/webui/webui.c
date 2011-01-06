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

#include "tvheadend.h"
#include "access.h"
#include "http.h"
#include "webui.h"
#include "dvr/dvr.h"
#include "filebundle.h"
#include "psi.h"

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

  if(remain == NULL)
    return 404;

  if(strstr(remain, ".."))
    return HTTP_STATUS_BAD_REQUEST;

  postfix = strrchr(remain, '.');
  if(postfix != NULL) {
    postfix++;
    if(!strcmp(postfix, "js"))
      content = "text/javascript; charset=UTF-8";
  }

  snprintf(path, sizeof(path), "%s/%s", base, remain);

  if((fd = tvh_open(path, O_RDONLY, 0)) < 0) {
    tvhlog(LOG_ERR, "webui", 
	   "Unable to open file %s -- %s", path, strerror(errno));
    return 404;
  }
  if(fstat(fd, &st) < 0) {
    tvhlog(LOG_ERR, "webui", 
	   "Unable to stat file %s -- %s", path, strerror(errno));
    close(fd);
    return 404;
  }

  http_send_header(hc, 200, content, st.st_size, NULL, NULL, 10, 0);
  sendfile(hc->hc_fd, fd, NULL, st.st_size);
  close(fd);
  return 0;
}

/**
 * HTTP stream loop
 */
static void
http_stream_run(http_connection_t *hc, streaming_queue_t *sq)
{
  streaming_message_t *sm;
  int run = 1;
  int start = 1;
  int timeouts = 0;
  pthread_mutex_lock(&sq->sq_mutex);

  while(run) {
    sm = TAILQ_FIRST(&sq->sq_queue);
    if(sm == NULL) {      
      struct timespec ts;
      struct timeval  tp;
      
      gettimeofday(&tp, NULL);
      ts.tv_sec  = tp.tv_sec + 1;
      ts.tv_nsec = tp.tv_usec * 1000;

      if(pthread_cond_timedwait(&sq->sq_cond, &sq->sq_mutex, &ts) == ETIMEDOUT) {
          int err = 0;
          socklen_t errlen = sizeof(err);  

          timeouts++;

          //Check socket status
          getsockopt(hc->hc_fd, SOL_SOCKET, SO_ERROR, (char *)&err, &errlen);  
          
          //Abort upon socket error, or after 5 seconds of silence
          if(err || timeouts > 4){
            run = 0;            
          }
      }
      continue;
    }

    timeouts = 0; //Reset timeout counter
    TAILQ_REMOVE(&sq->sq_queue, sm, sm_link);

    pthread_mutex_unlock(&sq->sq_mutex);

    switch(sm->sm_type) {
    case SMT_PACKET:
      //printf("SMT_PACKET\n");
      break;

    case SMT_START:
      if (start) {
        struct streaming_start *ss = sm->sm_data;
        uint8_t pat_ts[188];
        uint8_t pmt_ts[188];  
        int pcrpid = ss->ss_pcr_pid;
        int pmtpid = 0x0fff;

        http_output_content(hc, "video/mp2t");
        
        //Send PAT
        memset(pat_ts, 0xff, 188);
        psi_build_pat(NULL, pat_ts+5, 183, pmtpid);
        pat_ts[0] = 0x47;
        pat_ts[1] = 0x40;
        pat_ts[2] = 0x00;
        pat_ts[3] = 0x10;
        pat_ts[4] = 0x00;
        run = (write(hc->hc_fd, pat_ts, 188) == 188);
        
        if(!run) {
          break;
        }

        //Send PMT
        memset(pmt_ts, 0xff, 188);
        psi_build_pmt(ss, pmt_ts+5, 183, pcrpid);
        pmt_ts[0] = 0x47;
        pmt_ts[1] = 0x40 | (pmtpid >> 8);
        pmt_ts[2] = pmtpid;
        pmt_ts[3] = 0x10;
        pmt_ts[4] = 0x00;
        run = (write(hc->hc_fd, pmt_ts, 188) == 188);
        
        start = 0;
      }
      break;

    case SMT_STOP:
      run = 0;
      break;

    case SMT_SERVICE_STATUS:
      //printf("SMT_TRANSPORT_STATUS\n");
      break;

    case SMT_NOSTART:
      run = 0;
      break;

    case SMT_MPEGTS:
      run = (write(hc->hc_fd, sm->sm_data, 188) == 188);
      break;

    case SMT_EXIT:
      run = 0;
      break;
    }

    streaming_msg_free(sm);
    pthread_mutex_lock(&sq->sq_mutex);
  }

  pthread_mutex_unlock(&sq->sq_mutex);
}

/**
 * Output playlist with http streams (.m3u format)
 */
static void
http_stream_playlist(http_connection_t *hc, channel_t *channel)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  channel_t *ch = NULL;
  const char *host = http_arg_get(&hc->hc_args, "Host");

  pthread_mutex_lock(&global_lock);

  htsbuf_qprintf(hq, "#EXTM3U\n");
  RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
    if (channel == NULL || ch == channel) {
      htsbuf_qprintf(hq, "#EXTINF:-1,%s\n", ch->ch_name);
      htsbuf_qprintf(hq, "http://%s/stream/channelid/%d\n", host, ch->ch_id);
    }
  }

  http_output_content(hc, "application/x-mpegURL");

  pthread_mutex_unlock(&global_lock);
}

/**
 * Handle requests for playlists
 */
static int
page_http_playlist(http_connection_t *hc, const char *remain, void *opaque)
{
  char *components[2];
  channel_t *ch = NULL;

  if(http_access_verify(hc, ACCESS_STREAMING)) {
    http_error(hc, HTTP_STATUS_UNAUTHORIZED);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  hc->hc_keep_alive = 0;

  if(remain == NULL) {
    http_stream_playlist(hc, NULL);
    return 0;
  }

  if(http_tokenize((char *)remain, components, 2, '/') != 2) {
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    return HTTP_STATUS_BAD_REQUEST;
  }

  http_deescape(components[1]);

  pthread_mutex_lock(&global_lock);

  if(!strcmp(components[0], "channelid")) {
    ch = channel_find_by_identifier(atoi(components[1]));
  } else if(!strcmp(components[0], "channel")) {
    ch = channel_find_by_name(components[1], 0, 0);
  }

  pthread_mutex_unlock(&global_lock);

  if(ch == NULL) {
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    return HTTP_STATUS_BAD_REQUEST;
  }

  http_stream_playlist(hc, ch);
  return 0;
}

/**
 * Subscribes to a service and starts the streaming loop
 */
static int
http_stream_service(http_connection_t *hc, service_t *service)
{
  streaming_queue_t sq;
  th_subscription_t *s;

  pthread_mutex_lock(&global_lock);

  streaming_queue_init(&sq, ~SMT_TO_MASK(SUBSCRIPTION_RAW_MPEGTS));

  s = subscription_create_from_service(service,
                                       "HTTP", &sq.sq_st,
                                       SUBSCRIPTION_RAW_MPEGTS);


  pthread_mutex_unlock(&global_lock);

  //We won't get a START command, send http-header here.
  http_output_content(hc, "video/mp2t");

  http_stream_run(hc, &sq);

  pthread_mutex_lock(&global_lock);
  subscription_unsubscribe(s);
  pthread_mutex_unlock(&global_lock);
  streaming_queue_deinit(&sq);

  return 0;
}

/**
 * Subscribes to a channel and starts the streaming loop
 */
static int
http_stream_channel(http_connection_t *hc, channel_t *ch)
{
  streaming_queue_t sq;
  th_subscription_t *s;
  int priority = 150; //Default value, Compute this somehow

  pthread_mutex_lock(&global_lock);

  streaming_queue_init(&sq, ~SMT_TO_MASK(SUBSCRIPTION_RAW_MPEGTS));

  s = subscription_create_from_channel(ch, priority, 
                                       "HTTP", &sq.sq_st,
                                       SUBSCRIPTION_RAW_MPEGTS);


  pthread_mutex_unlock(&global_lock);

  http_stream_run(hc, &sq);

  pthread_mutex_lock(&global_lock);
  subscription_unsubscribe(s);
  pthread_mutex_unlock(&global_lock);
  streaming_queue_deinit(&sq);

  return 0;
}


/**
 * Handle the http request. http://tvheadend/stream/channelid/<chid>
 *                          http://tvheadend/stream/channel/<chname>
 *                          http://tvheadend/stream/service/<servicename>
 */
static int
http_stream(http_connection_t *hc, const char *remain, void *opaque)
{
  char *components[2];
  channel_t *ch = NULL;
  service_t *service = NULL;

  if(http_access_verify(hc, ACCESS_STREAMING)) {
    http_error(hc, HTTP_STATUS_UNAUTHORIZED);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  hc->hc_keep_alive = 0;

  if(remain == NULL) {
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    return HTTP_STATUS_BAD_REQUEST;
  }

  if(http_tokenize((char *)remain, components, 2, '/') != 2) {
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    return HTTP_STATUS_BAD_REQUEST;
  }

  http_deescape(components[1]);

  pthread_mutex_lock(&global_lock);

  if(!strcmp(components[0], "channelid")) {
    ch = channel_find_by_identifier(atoi(components[1]));
  } else if(!strcmp(components[0], "channel")) {
    ch = channel_find_by_name(components[1], 0, 0);
  } else if(!strcmp(components[0], "service")) {
    service = service_find_by_identifier(components[1]);
  }

  pthread_mutex_unlock(&global_lock);

  if(ch != NULL) {
    return http_stream_channel(hc, ch);
  } else if(service != NULL) {
    return http_stream_service(hc, service);
  } else {
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    return HTTP_STATUS_BAD_REQUEST;
  }
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

  if(remain == NULL)
    return 404;

  postfix = strrchr(remain, '.');
  if(postfix != NULL) {
    postfix++;
    if(!strcmp(postfix, "js"))
      content = "text/javascript; charset=UTF-8";
  }

  for(fbe = fb->entries; fbe->filename != NULL; fbe++) {
    if(!strcmp(fbe->filename, remain)) {

      http_send_header(hc, 200, content, fbe->size, 
		       fbe->original_size == -1 ? NULL : "gzip", NULL, 10, 0);
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
  char range_buf[255];
  off_t content_len, file_start, file_end;
  
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

  fd = tvh_open(fname, O_RDONLY, 0);
  free(fname);
  if(fd < 0)
    return 404;

  if(fstat(fd, &st) < 0) {
    close(fd);
    return 404;
  }

  file_start = 0;
  file_end = st.st_size-1;
  
  range = http_arg_get(&hc->hc_args, "Range");
  if(range != NULL)
    sscanf(range, "bytes=%"PRId64"-%"PRId64"", &file_start, &file_end);

  //Sanity checks
  if(file_start < 0 || file_start >= st.st_size)
    return 200;

  if(file_end < 0 || file_end >= st.st_size)
    return 200;

  if(file_start > file_end)
    return 200;

  content_len = file_end - file_start+1;
  
  sprintf(range_buf, "bytes %"PRId64"-%"PRId64"/%"PRId64"", file_start, file_end, st.st_size);

  if(file_start > 0)
    lseek(fd, file_start, SEEK_SET);

  http_send_header(hc, 200, content, content_len, NULL, NULL, 10, range_buf);
  sendfile(hc->hc_fd, fd, NULL, content_len);
  close(fd);

  if(range)
    return 206;
  else
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

      http_path_add(http_path, strdup(path), 
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
	 "No source path providing HTTP content: \"%s\"."
	 "Checked in \"%s\" and in the binary's embedded file system. "
	 "If you need to move or install the binary, "
	 "reconfigure with --release", http_path, content_path);

}


/**
 *
 */
static int
favicon(http_connection_t *hc, const char *remain, void *opaque)
{
  http_redirect(hc, "static/htslogo.png");
  return 0;
}

int page_statedump(http_connection_t *hc, const char *remain, void *opaque);

/**
 * WEB user interface
 */
void
webui_init(const char *contentpath)
{
  http_path_add("/", NULL, page_root, ACCESS_WEB_INTERFACE);

  http_path_add("/dvrfile", NULL, page_dvrfile, ACCESS_WEB_INTERFACE);
  http_path_add("/favicon.ico", NULL, favicon, ACCESS_WEB_INTERFACE);
  http_path_add("/playlist", NULL, page_http_playlist, ACCESS_WEB_INTERFACE);

  http_path_add("/state", NULL, page_statedump, ACCESS_ADMIN);

  http_path_add("/stream",  NULL, http_stream,  ACCESS_STREAMING);

  webui_static_content(contentpath, "/static",        "src/webui/static");
  webui_static_content(contentpath, "/docs",          "docs/html");
  webui_static_content(contentpath, "/docresources",  "docs/docresources");

  simpleui_start();
  extjs_start();
  comet_init();

}
