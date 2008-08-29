/*
 *  tvheadend, HTTP interface
 *  Copyright (C) 2007 Andreas Öman
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

#ifndef HTTP_H_
#define HTTP_H_

#include <libhts/htsbuf.h>

TAILQ_HEAD(http_arg_list, http_arg);

typedef struct http_arg {
  TAILQ_ENTRY(http_arg) link;
  char *key;
  char *val;
} http_arg_t;

#define HTTP_STATUS_OK           200
#define HTTP_STATUS_FOUND        302
#define HTTP_STATUS_BAD_REQUEST  400
#define HTTP_STATUS_UNAUTHORIZED 401
#define HTTP_STATUS_NOT_FOUND    404


LIST_HEAD(rtsp_session_head, rtsp_session);

typedef struct http_connection {
  int hc_fd;
  struct sockaddr_in *hc_peer;
  char *hc_url;
  int hc_keep_alive;

  htsbuf_queue_t hc_reply;

  struct http_arg_list hc_args;

  struct http_arg_list hc_req_args; /* Argumets from GET or POST request */

  enum {
    HTTP_CON_WAIT_REQUEST,
    HTTP_CON_READ_HEADER,
    HTTP_CON_END,
    HTTP_CON_POST_DATA,
  } hc_state;

  enum {
    HTTP_CMD_GET,
    HTTP_CMD_POST,
    RTSP_CMD_DESCRIBE,
    RTSP_CMD_OPTIONS,
    RTSP_CMD_SETUP,
    RTSP_CMD_TEARDOWN,
    RTSP_CMD_PLAY,
    RTSP_CMD_PAUSE,
  } hc_cmd;

  enum {
    HTTP_VERSION_1_0,
    HTTP_VERSION_1_1,
    RTSP_VERSION_1_0,
  } hc_version;

  char *hc_username;
  char *hc_password;

  //  struct rtsp_session_head hc_rtsp_sessions;

  int hc_authenticated; /* Used by RTSP, it seems VLC does not 
			   send authentication headers for each 
			   command, so we just say that it's ok
			   if it has authenticated at least once */

  struct config_head *hc_user_config;

  /* Support for HTTP POST */
  
  char *hc_post_data;
  unsigned int hc_post_len;

} http_connection_t;


void http_arg_flush(struct http_arg_list *list);

char *http_arg_get(struct http_arg_list *list, const char *name);

void http_arg_set(struct http_arg_list *list, char *key, char *val);

int http_tokenize(char *buf, char **vec, int vecsize, int delimiter);

void http_error(http_connection_t *hc, int error);

void http_output_html(http_connection_t *hc);

void http_output_content(http_connection_t *hc, const char *content);

void http_redirect(http_connection_t *hc, const char *location);

typedef int (http_callback_t)(http_connection_t *hc, 
			      const char *remain, void *opaque);

typedef struct http_path {
  LIST_ENTRY(http_path) hp_link;
  const char *hp_path;
  void *hp_opaque;
  http_callback_t *hp_callback;
  int hp_len;
  uint32_t hp_accessmask;
} http_path_t;

http_path_t *http_path_add(const char *path, void *opaque,
			   http_callback_t *callback, uint32_t accessmask);



void http_server_init(void);

#endif /* HTTP_H_ */
