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
#include "htsmsg.h"
#include "url.h"
#include "tvhpoll.h"
#include "access.h"
#include "atomic.h"

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

#define HTTP_AUTH_PLAIN             0
#define HTTP_AUTH_DIGEST            1
#define HTTP_AUTH_PLAIN_DIGEST      2

#define HTTP_AUTH_ALGO_MD5          0
#define HTTP_AUTH_ALGO_SHA256       1
#define HTTP_AUTH_ALGO_SHA512_256   2

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
  RTSP_CMD_GET_PARAMETER,
} http_cmd_t;

#define HTTP_CMD_OPTIONS RTSP_CMD_OPTIONS

typedef enum http_ver {
  HTTP_VERSION_1_0,
  HTTP_VERSION_1_1,
  RTSP_VERSION_1_0,
} http_ver_t;

typedef enum http_wsop {
  HTTP_WSOP_TEXT = 0,
  HTTP_WSOP_BINARY = 1,
  HTTP_WSOP_PING = 2,
  HTTP_WSOP_PONG = 3
} http_wsop_t;

typedef struct http_connection {
  int hc_subsys;
  int hc_fd;
  struct sockaddr_storage *hc_peer;
  char *hc_peer_ipstr;
  struct sockaddr_storage *hc_self;
  char *hc_representative;
  struct sockaddr_storage *hc_proxy_ip;
  struct sockaddr_storage *hc_local_ip;

  tvh_mutex_t *hc_paths_mutex;
  http_path_list_t *hc_paths;
  int (*hc_process)(struct http_connection *hc, htsbuf_queue_t *spill);

  char *hc_url;
  char *hc_url_orig;

  htsbuf_queue_t  hc_reply;

  int             hc_extra_insend;
  tvh_mutex_t     hc_extra_lock;
  int             hc_extra_chunks;
  htsbuf_queue_t  hc_extra;

  http_arg_list_t hc_args;

  http_arg_list_t hc_req_args; /* Argumets from GET or POST request */

  http_state_t    hc_state;
  http_cmd_t      hc_cmd;
  http_ver_t      hc_version;

  char *hc_username;
  char *hc_password;
  char *hc_authhdr;
  char *hc_nonce;
  access_t *hc_access;
  enum {
    HC_AUTH_NONE,
    HC_AUTH_ADDR,
    HC_AUTH_PLAIN,
    HC_AUTH_DIGEST,
    HC_AUTH_TICKET,
    HC_AUTH_PERM
  } hc_auth_type;

  /* RTSP */
  uint64_t hc_cseq;
  char *hc_session;

  /* Flags */
  uint8_t hc_keep_alive;
  uint8_t hc_no_output;
  uint8_t hc_shutdown;
  uint8_t hc_is_local_ip;   /*< a connection from the local network */

  /* Support for HTTP POST */
  
  char *hc_post_data;
  unsigned int hc_post_len;

} http_connection_t;

extern void *http_server;

const char *http_cmd2str(int val);
int http_str2cmd(const char *str);
const char *http_ver2str(int val);
int http_str2ver(const char *str);

static inline void http_arg_init(http_arg_list_t *list)
{
  TAILQ_INIT(list);
}

void http_arg_remove(http_arg_list_t *list, struct http_arg *arg);
void http_arg_flush(http_arg_list_t *list);

char *http_arg_get(http_arg_list_t *list, const char *name);
char *http_arg_get_remove(http_arg_list_t *list, const char *name);

void http_arg_set(http_arg_list_t *list, const char *key, const char *val);

char *http_arg_get_query(http_arg_list_t *list);

static inline int http_args_empty(const http_arg_list_t *list) { return TAILQ_EMPTY(list); }

int http_tokenize(char *buf, char **vec, int vecsize, int delimiter);

const char * http_username(http_connection_t *hc);

int http_noaccess_code(http_connection_t *hc);

void http_alive(http_connection_t *hc);

void http_error(http_connection_t *hc, int error);

int http_encoding_valid(http_connection_t *hc, const char *encoding);

int http_header_match(http_connection_t *hc, const char *name, const char *value);

void http_output_html(http_connection_t *hc);

void http_output_content(http_connection_t *hc, const char *content);

void http_redirect(http_connection_t *hc, const char *location,
                   struct http_arg_list *req_args, int external);

void http_css_import(http_connection_t *hc, const char *location);

void http_extra_destroy(http_connection_t *hc);

int http_extra_flush(http_connection_t *hc);

int http_extra_flush_partial(http_connection_t *hc);

int http_extra_send(http_connection_t *hc, const void *data,
                    size_t data_len, int may_discard);

int http_extra_send_prealloc(http_connection_t *hc, const void *data,
                             size_t data_len, int may_discard);

static inline void http_send_begin(http_connection_t *hc)
{
  atomic_add(&hc->hc_extra_insend, 1);
  if (atomic_get(&hc->hc_extra_chunks) > 0)
    http_extra_flush_partial(hc);
}

static inline void http_send_end(http_connection_t *hc)
{
  atomic_dec(&hc->hc_extra_insend, 1);
  if (atomic_get(&hc->hc_extra_chunks) > 0)
    http_extra_flush(hc);
}

void http_send_header(http_connection_t *hc, int rc, const char *content, 
		      int64_t contentlen, const char *encoding,
		      const char *location, int maxage, const char *range,
		      const char *disposition, http_arg_list_t *args);

int http_send_header_websocket(http_connection_t *hc, const char *protocol);

int http_websocket_send(http_connection_t *hc, uint8_t *buf, uint64_t buflen, int opcode);

int http_websocket_send_json(http_connection_t *hc, htsmsg_t *msg);

int http_websocket_read(http_connection_t *hc, htsmsg_t **_res, int timeout);

void http_serve_requests(http_connection_t *hc);

void http_cancel(void *opaque);

int http_check_local_ip(http_connection_t *hc);

typedef int (http_callback_t)(http_connection_t *hc, 
			      const char *remain, void *opaque);

typedef char * (http_path_modify_t)(http_connection_t *hc,
                                    const char * path, int *cut);
                                 
#define HTTP_PATH_NO_VERIFICATION    (1 << 0)
#define HTTP_PATH_WEBSOCKET          (1 << 1)

typedef struct http_path {
  LIST_ENTRY(http_path) hp_link;
  const char *hp_path;
  void *hp_opaque;
  http_callback_t *hp_callback;
  int hp_len;
  uint32_t hp_flags;
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
                               struct channel *ch);

void http_parse_args(http_arg_list_t *list, char *args);

char *http_get_hostpath(http_connection_t *hc, char *buf, size_t buflen);

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

  tvh_mutex_t  hc_mutex;

  int          hc_id;
  int          hc_fd;
  char        *hc_scheme;
  char        *hc_host;
  int          hc_port;
  char        *hc_bindaddr;
  tvhpoll_t   *hc_efd;
  int          hc_pevents;
  int          hc_pevents_pause;

  int          hc_code;
  http_ver_t   hc_version;
  http_ver_t   hc_redirv;
  http_cmd_t   hc_cmd;

  struct http_arg_list hc_args; /* header */

  char        *hc_url;

  void        *hc_aux;
  size_t       hc_data_limit;
  size_t       hc_io_size;
  char        *hc_data;         /* data body */
  size_t       hc_data_size;    /* data body size - result for caller */

  int64_t      hc_ping_time;    /* last issued command */

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
  uint8_t      hc_running;	/* outside hc_mutex */
  uint8_t      hc_shutdown_wait;/* outside hc_mutex */
  int          hc_refcnt;       /* callback protection - outside hc_mutex */
  int          hc_redirects;
  int          hc_result;
  unsigned int hc_shutdown:1;
  unsigned int hc_sending:1;
  unsigned int hc_einprogress:1;
  unsigned int hc_reconnected:1;
  unsigned int hc_keepalive:1;
  unsigned int hc_in_data:1;
  unsigned int hc_in_rtp_data:1;
  unsigned int hc_chunked:1;
  unsigned int hc_chunk_trails:1;
  unsigned int hc_handle_location:1; /* handle the redirection (location) requests */
  unsigned int hc_pause:1;

  http_client_wcmd_t            *hc_wcmd;
  TAILQ_HEAD(,http_client_wcmd)  hc_wqueue;

  int          hc_verify_peer;  /* SSL - verify peer */

  int          hc_cseq;         /* RTSP */
  int          hc_rcseq;        /* RTSP - expected cseq */
  char        *hc_rtsp_session;
  char        *hc_rtp_dest;
  int          hc_rtp_port;
  int          hc_rtcp_port;
  int          hc_rtp_tcp;
  int          hc_rtcp_tcp;
  int          hc_rtcp_server_port;
  unsigned int hc_rtp_multicast:1;
  unsigned int hc_rtp_avpf:1;
  long         hc_rtsp_stream_id;
  int          hc_rtp_timeout;
  char        *hc_rtsp_user;
  char        *hc_rtsp_pass;
  char         hc_rtsp_keep_alive_cmd;
  time_t       hc_rtsp_stream_start;
  time_t       hc_rtsp_range_start;
  time_t       hc_rtsp_range_end;
  float        hc_rtsp_scale;

  struct http_client_ssl *hc_ssl; /* ssl internals */

  mtimer_t     hc_close_timer;

  /* callbacks */
  void    (*hc_hdr_create)   (http_client_t *hc, http_arg_list_t *h,
                              const url_t *url, int keepalive);
  int     (*hc_hdr_received) (http_client_t *hc);
  int     (*hc_data_received)(http_client_t *hc, void *buf, size_t len);
  int     (*hc_data_complete)(http_client_t *hc);
  int     (*hc_rtp_data_received)(http_client_t *hc, void *buf, size_t len);
  int     (*hc_rtp_data_complete)(http_client_t *hc);
  void    (*hc_conn_closed)  (http_client_t *hc, int err);
};

void http_client_init ( void );
void http_client_done ( void );

const char * http_client_con2str(http_state_t state);

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
void http_client_basic_args ( http_client_t *hc, http_arg_list_t *h,
                              const url_t *url, int keepalive );
void http_client_add_args ( http_client_t *hc, http_arg_list_t *h,
                            const char *args );
int http_client_simple_reconnect ( http_client_t *hc, const url_t *u, http_ver_t ver );
int http_client_simple( http_client_t *hc, const url_t *url);
int http_client_clear_state( http_client_t *hc );
int http_client_run( http_client_t *hc );
void http_client_ssl_peer_verify( http_client_t *hc, int verify );
void http_client_unpause( http_client_t *hc );

/*
 * RTSP helpers
 */

int rtsp_send_ext( http_client_t *hc, http_cmd_t cmd, const char *path,
               const char *query, http_arg_list_t *hdr, const char *body, size_t size );

static inline int
rtsp_send( http_client_t *hc, http_cmd_t cmd, const char *path,
               const char *query, http_arg_list_t *hdr ) {
  return rtsp_send_ext( hc, cmd, path, query, hdr, NULL, 0 );
}
                      
void rtsp_clear_session( http_client_t *hc );

static inline int rtsp_options( http_client_t *hc ) {
  return rtsp_send(hc, RTSP_CMD_OPTIONS, NULL, NULL, NULL);
}

static inline int
rtsp_describe( http_client_t *hc, const char *path, const char *query ) {
  return rtsp_send(hc, RTSP_CMD_DESCRIBE, path, query, NULL);
}

int rtsp_setup( http_client_t *hc, const char *path, const char *query,
                const char *multicast_addr, int rtp_port, int rtcp_port );

static inline int
rtsp_play( http_client_t *hc, const char *path, const char *query ) {
  return rtsp_send(hc, RTSP_CMD_PLAY, path, query, NULL);
}

static inline int
rtsp_pause( http_client_t *hc, const char *path, const char *query ) {
  return rtsp_send(hc, RTSP_CMD_PAUSE, path, query, NULL);
}

static inline int
rtsp_teardown( http_client_t *hc, const char *path, const char *query ) {
  return rtsp_send(hc, RTSP_CMD_TEARDOWN, path, query, NULL);
}

int rtsp_get_parameter( http_client_t *hc, const char *parameter );

int rtsp_set_speed( http_client_t *hc, double speed );

int rtsp_set_position( http_client_t *hc, time_t position );

int rtsp_describe_decode( http_client_t *hc, const char *buf, size_t len );

int rtsp_setup_decode( http_client_t *hc, int satip );

int rtsp_options_decode( http_client_t *hc );

int rtsp_play_decode( http_client_t *hc );
#endif /* HTTP_H_ */
