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

#include "htsbuf.h"
#include "url.h"
#include "tvhpoll.h"
  #include "access.h"

struct channel;
struct http_path;

typedef LIST_HEAD(, http_path) http_path_list_t;

typedef TAILQ_HEAD(http_arg_list, http_arg) http_arg_list_t;

typedef RB_HEAD(,http_arg) http_arg_tree_t;

typedef struct http_arg {
  RB_ENTRY(http_arg) rb_link;
  TAILQ_ENTRY(http_arg) link;
  char *key;
  char *val;
} http_arg_t;

#define HTTP_STATUS_CONTINUE        100
#define HTTP_STATUS_PSWITCH         101
#define HTTP_STATUS_OK              200
#define HTTP_STATUS_CREATED         201
#define HTTP_STATUS_ACCEPTED        202
#define HTTP_STATUS_NON_AUTH_INFO   203
#define HTTP_STATUS_NO_CONTENT      204
#define HTTP_STATUS_RESET_CONTENT   205
#define HTTP_STATUS_PARTIAL_CONTENT 206
#define HTTP_STATUS_MULTIPLE        300
#define HTTP_STATUS_MOVED           301
#define HTTP_STATUS_FOUND           302
#define HTTP_STATUS_SEE_OTHER       303
#define HTTP_STATUS_NOT_MODIFIED    304
#define HTTP_STATUS_USE_PROXY       305
#define HTTP_STATUS_TMP_REDIR       307
#define HTTP_STATUS_BAD_REQUEST     400
#define HTTP_STATUS_UNAUTHORIZED    401
#define HTTP_STATUS_PAYMENT         402
#define HTTP_STATUS_FORBIDDEN       403
#define HTTP_STATUS_NOT_FOUND       404
#define HTTP_STATUS_NOT_ALLOWED     405
#define HTTP_STATUS_NOT_ACCEPTABLE  406
#define HTTP_STATUS_PROXY_AUTH      407
#define HTTP_STATUS_TIMEOUT         408
#define HTTP_STATUS_CONFLICT        409
#define HTTP_STATUS_GONE            410
#define HTTP_STATUS_LENGTH          411
#define HTTP_STATUS_PRECONDITION    412
#define HTTP_STATUS_ENTITY_OVER     413
#define HTTP_STATUS_URI_TOO_LONG    414
#define HTTP_STATUS_UNSUPPORTED     415
#define HTTP_STATUS_BAD_RANGE       417
#define HTTP_STATUS_EXPECTATION     418
#define HTTP_STATUS_BANDWIDTH       453
#define HTTP_STATUS_BAD_SESSION     454
#define HTTP_STATUS_METHOD_INVALID  455
#define HTTP_STATUS_BAD_TRANSFER    456
#define HTTP_STATUS_INTERNAL        500
#define HTTP_STATUS_NOT_IMPLEMENTED 501
#define HTTP_STATUS_BAD_GATEWAY     502
#define HTTP_STATUS_SERVICE         503
#define HTTP_STATUS_GATEWAY_TIMEOUT 504
#define HTTP_STATUS_HTTP_VERSION    505
#define HTTP_STATUS_OP_NOT_SUPPRT   551

typedef enum http_state {
  HTTP_CON_WAIT_REQUEST,
  HTTP_CON_READ_HEADER,
  HTTP_CON_END,
  HTTP_CON_POST_DATA,
  HTTP_CON_SENDING,
  HTTP_CON_SENT,
  HTTP_CON_RECEIVING,
  HTTP_CON_DONE,
  HTTP_CON_IDLE,
  HTTP_CON_OK
} http_state_t;

typedef enum http_cmd {
  HTTP_CMD_NONE,
  HTTP_CMD_GET,
  HTTP_CMD_HEAD,
  HTTP_CMD_POST,
  RTSP_CMD_DESCRIBE,
  RTSP_CMD_OPTIONS,
  RTSP_CMD_SETUP,
  RTSP_CMD_TEARDOWN,
  RTSP_CMD_PLAY,
  RTSP_CMD_PAUSE,
} http_cmd_t;

typedef enum http_ver {
  HTTP_VERSION_1_0,
  HTTP_VERSION_1_1,
  RTSP_VERSION_1_0,
} http_ver_t;

typedef struct http_connection {
  int hc_fd;
  struct sockaddr_storage *hc_peer;
  char *hc_peer_ipstr;
  struct sockaddr_storage *hc_self;
  char *hc_representative;

  http_path_list_t *hc_paths;
  int (*hc_process)(struct http_connection *hc, htsbuf_queue_t *spill);

  char *hc_url;
  char *hc_url_orig;
  int hc_keep_alive;

  htsbuf_queue_t  hc_reply;

  http_arg_list_t hc_args;

  http_arg_list_t hc_req_args; /* Argumets from GET or POST request */

  http_state_t    hc_state;
  http_cmd_t      hc_cmd;
  http_ver_t      hc_version;

  char *hc_username;
  char *hc_password;
  access_t *hc_access;

  struct config_head *hc_user_config;

  int hc_no_output;
  int hc_logout_cookie;
  int hc_shutdown;
  uint64_t hc_cseq;
  char *hc_session;

  /* Support for HTTP POST */
  
  char *hc_post_data;
  unsigned int hc_post_len;

} http_connection_t;

extern void *http_server;

const char *http_cmd2str(int val);
int http_str2cmd(const char *str);
const char *http_ver2str(int val);
int http_str2ver(const char *str);

static inline void http_arg_init(struct http_arg_list *list)
{
  TAILQ_INIT(list);
}

void http_arg_flush(struct http_arg_list *list);

char *http_arg_get(struct http_arg_list *list, const char *name);
char *http_arg_get_remove(struct http_arg_list *list, const char *name);

void http_arg_set(struct http_arg_list *list, const char *key, const char *val);

int http_tokenize(char *buf, char **vec, int vecsize, int delimiter);

void http_error(http_connection_t *hc, int error);

int http_encoding_valid(http_connection_t *hc, const char *encoding);

void http_output_html(http_connection_t *hc);

void http_output_content(http_connection_t *hc, const char *content);

void http_redirect(http_connection_t *hc, const char *location,
                   struct http_arg_list *req_args);

void http_send_header(http_connection_t *hc, int rc, const char *content, 
		      int64_t contentlen, const char *encoding,
		      const char *location, int maxage, const char *range,
		      const char *disposition, http_arg_list_t *args);

void http_serve_requests(http_connection_t *hc);

void http_cancel(void *opaque);

typedef int (http_callback_t)(http_connection_t *hc, 
			      const char *remain, void *opaque);

typedef char * (http_path_modify_t)(http_connection_t *hc,
                                    const char * path, int *cut);
                                 

typedef struct http_path {
  LIST_ENTRY(http_path) hp_link;
  const char *hp_path;
  void *hp_opaque;
  http_callback_t *hp_callback;
  int hp_len;
  uint32_t hp_accessmask;
  http_path_modify_t *hp_path_modify;
} http_path_t;

http_path_t *http_path_add_modify(const char *path, void *opaque,
                                  http_callback_t *callback,
                                  uint32_t accessmask,
                                  http_path_modify_t path_modify);
http_path_t *http_path_add(const char *path, void *opaque,
			   http_callback_t *callback, uint32_t accessmask);

void http_server_init(const char *bindaddr);
void http_server_register(void);
void http_server_done(void);

int http_access_verify(http_connection_t *hc, int mask);
int http_access_verify_channel(http_connection_t *hc, int mask,
                               struct channel *ch, int ticket);

void http_deescape(char *s);

void http_parse_get_args(http_connection_t *hc, char *args);

/*
 * HTTP/RTSP Client
 */

typedef struct http_client http_client_t;

typedef struct http_client_wcmd {

  TAILQ_ENTRY(http_client_wcmd) link;

  enum http_cmd wcmd;
  int           wcseq;

  void         *wbuf;
  size_t        wpos;
  size_t        wsize;
} http_client_wcmd_t;

struct http_client {

  TAILQ_ENTRY(http_client) hc_link;

  int          hc_id;
  int          hc_fd;
  char        *hc_scheme;
  char        *hc_host;
  int          hc_port;
  char        *hc_bindaddr;
  tvhpoll_t   *hc_efd;
  int          hc_pevents;

  int          hc_code;
  http_ver_t   hc_version;
  http_cmd_t   hc_cmd;

  struct http_arg_list hc_args; /* header */

  void        *hc_aux;
  size_t       hc_data_limit;
  size_t       hc_io_size;
  char        *hc_data;         /* data body */
  size_t       hc_data_size;    /* data body size - result for caller */

  time_t       hc_ping_time;    /* last issued command */

  char        *hc_rbuf;         /* read buffer */
  size_t       hc_rsize;        /* read buffer size */
  size_t       hc_rpos;         /* read buffer position */
  size_t       hc_hsize;        /* header size in bytes */
  size_t       hc_csize;        /* contents size in bytes */
  char        *hc_chunk;
  size_t       hc_chunk_size;
  size_t       hc_chunk_csize;
  size_t       hc_chunk_alloc;
  size_t       hc_chunk_pos;
  char        *hc_location;
  int          hc_redirects;
  int          hc_result;
  int          hc_shutdown:1;
  int          hc_sending:1;
  int          hc_einprogress:1;
  int          hc_reconnected:1;
  int          hc_keepalive:1;
  int          hc_in_data:1;
  int          hc_chunked:1;
  int          hc_chunk_trails:1;
  int          hc_running:1;
  int          hc_shutdown_wait:1;
  int          hc_handle_location:1; /* handle the redirection (location) requests */

  http_client_wcmd_t            *hc_wcmd;
  TAILQ_HEAD(,http_client_wcmd)  hc_wqueue;

  int          hc_verify_peer;  /* SSL - verify peer */

  int          hc_cseq;         /* RTSP */
  int          hc_rcseq;        /* RTSP - expected cseq */
  char        *hc_rtsp_session;
  char        *hc_rtp_dest;
  int          hc_rtp_port;
  int          hc_rtpc_port;
  int          hc_rtcp_server_port;
  int          hc_rtp_multicast:1;
  long         hc_rtsp_stream_id;
  int          hc_rtp_timeout;
  char        *hc_rtsp_user;
  char        *hc_rtsp_pass;

  struct http_client_ssl *hc_ssl; /* ssl internals */

  gtimer_t     hc_close_timer;

  /* callbacks */
  int     (*hc_hdr_received) (http_client_t *hc);
  int     (*hc_data_received)(http_client_t *hc, void *buf, size_t len);
  int     (*hc_data_complete)(http_client_t *hc);
  void    (*hc_conn_closed)  (http_client_t *hc, int err);
};

void http_client_init ( const char *user_agent );
void http_client_done ( void );

http_client_t*
http_client_connect ( void *aux, http_ver_t ver, const char *scheme,
                      const char *host, int port, const char *bindaddr );
void http_client_register ( http_client_t *hc );
void http_client_close ( http_client_t *hc );

int http_client_send( http_client_t *hc, http_cmd_t cmd,
                      const char *path, const char *query,
                      http_arg_list_t *header, void *body, size_t body_size );
void http_client_basic_auth( http_client_t *hc, http_arg_list_t *h,
                             const char *user, const char *pass );
int http_client_simple( http_client_t *hc, const url_t *url);
int http_client_clear_state( http_client_t *hc );
int http_client_run( http_client_t *hc );
void http_client_ssl_peer_verify( http_client_t *hc, int verify );

/*
 * RTSP helpers
 */

int rtsp_send( http_client_t *hc, http_cmd_t cmd, const char *path,
               const char *query, http_arg_list_t *hdr );
                      
void rtsp_clear_session( http_client_t *hc );

int rtsp_options_decode( http_client_t *hc );
static inline int rtsp_options( http_client_t *hc ) {
  return rtsp_send(hc, RTSP_CMD_OPTIONS, NULL, NULL, NULL);
}

int rtsp_setup_decode( http_client_t *hc, int satip );
int rtsp_setup( http_client_t *hc, const char *path, const char *query,
                const char *multicast_addr, int rtp_port, int rtpc_port );

static inline int
rtsp_play( http_client_t *hc, const char *path, const char *query ) {
  return rtsp_send(hc, RTSP_CMD_PLAY, path, query, NULL);
}

static inline int
rtsp_teardown( http_client_t *hc, const char *path, const char *query ) {
  return rtsp_send(hc, RTSP_CMD_TEARDOWN, path, query, NULL);
}

int rtsp_describe_decode( http_client_t *hc );
static inline int
rtsp_describe( http_client_t *hc, const char *path, const char *query ) {
  return rtsp_send(hc, RTSP_CMD_DESCRIBE, path, query, NULL);
}

#endif /* HTTP_H_ */
