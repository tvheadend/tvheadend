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

extern int http_port;

#include "tcp.h"

#define HTTP_STATUS_OK           200
#define HTTP_STATUS_FOUND        302
#define HTTP_STATUS_BAD_REQUEST  400
#define HTTP_STATUS_UNAUTHORIZED 401
#define HTTP_STATUS_NOT_FOUND    404


LIST_HEAD(rtsp_session_head, rtsp_session);

#define http_printf(x, fmt...) tcp_printf(&(x)->hc_tcp_session, fmt)

TAILQ_HEAD(http_arg_list, http_arg);

typedef struct http_arg {
  TAILQ_ENTRY(http_arg) link;
  char *key;
  char *val;
} http_arg_t;


TAILQ_HEAD(http_reply_queue, http_reply);

typedef struct http_reply {
  TAILQ_ENTRY(http_reply) hr_link;

  void *hr_opaque;

  void (*hr_destroy)(struct http_reply *hr, void *opaque);

  struct http_connection *hr_connection;
  int hr_version; /* HTTP version */
  int hr_keep_alive;
  
  char *hr_location;

  int hr_rc;      /* Return code */
  int hr_maxage;
  const char *hr_encoding;
  const char *hr_content;

  tcp_queue_t hr_tq;

} http_reply_t;


typedef struct http_connection {
  tcp_session_t hc_tcp_session; /* Must be first */
  char *hc_url;
  int hc_keep_alive;

  struct http_reply_queue hc_replies;

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
    HTTP_VERSION_0_9,
    HTTP_VERSION_1_0,
    HTTP_VERSION_1_1,
    RTSP_VERSION_1_0,
  } hc_version;

  char *hc_username;
  char *hc_password;

  struct rtsp_session_head hc_rtsp_sessions;

  int hc_authenticated; /* Used by RTSP, it seems VLC does not 
			   send authentication headers for each 
			   command, so we just say that it's ok
			   if it has authenticated at least once */

  struct config_head *hc_user_config;

  /* Support for HTTP POST */
  
  char *hc_post_data;
  unsigned int hc_post_len;
  unsigned int hc_post_ptr;

} http_connection_t;



void http_start(int port);

void http_arg_flush(struct http_arg_list *list);

char *http_arg_get(struct http_arg_list *list, const char *name);

void http_arg_set(struct http_arg_list *list, char *key, char *val);

int http_tokenize(char *buf, char **vec, int vecsize, int delimiter);

void http_error(http_connection_t *hc, http_reply_t *hr, int error);

void http_output(http_connection_t *hc, http_reply_t *hr,
		 const char *content, const char *encoding, int maxage);

void http_output_html(http_connection_t *hc, http_reply_t *hr);

void http_continue(http_connection_t *hc);

void http_redirect(http_connection_t *hc, http_reply_t *hr, 
		   const char *location);

typedef int (http_callback_t)(http_connection_t *hc, http_reply_t *hr, 
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

void http_resource_add(const char *path, const void *ptr, size_t len, 
		       const char *content, const char *encoding);

#endif /* HTTP_H_ */
