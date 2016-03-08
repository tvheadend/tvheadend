/*
 *  Tvheadend - HTTP client functions
 *
 *  Copyright (C) 2014 Jaroslav Kysela
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
#include "http.h"
#include "tcp.h"

#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>

#if defined(PLATFORM_FREEBSD)
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include <openssl/err.h>
#include <openssl/ssl.h>

#ifndef SSL_OP_NO_COMPRESSION
#define SSL_OP_NO_COMPRESSION 0
#endif

#if ENABLE_ANDROID
#include <sys/socket.h>
#endif

struct http_client_ssl {
  int      connected;
  int      shutdown;
  int      notified;

  SSL_CTX *ctx;
  SSL     *ssl;

  BIO     *rbio;
  char    *rbio_buf;
  size_t   rbio_size;
  size_t   rbio_pos;
  
  BIO     *wbio;
  char    *wbio_buf;
  size_t   wbio_size;
  size_t   wbio_pos;
};


static int
http_client_redirected ( http_client_t *hc );
static int
http_client_ssl_write_update( http_client_t *hc );
static int
http_client_reconnect
  ( http_client_t *hc, http_ver_t ver, const char *scheme,
    const char *host, int port );
#if HTTPCLIENT_TESTSUITE
static void
http_client_testsuite_run( void );
#endif


/*
 * Global state
 */
static int                      http_running;
static tvhpoll_t               *http_poll;
static TAILQ_HEAD(,http_client) http_clients;
static pthread_mutex_t          http_lock;
static tvh_cond_t               http_cond;
static th_pipe_t                http_pipe;
static char                    *http_user_agent;

/*
 *
 */
static inline int
shortid( http_client_t *hc )
{
  return hc->hc_id;
}

/*
 *
 */
static int
http_port( http_client_t *hc, const char *scheme, int port )
{
  if (port <= 0 || port > 65535) {
    if (scheme && strcmp(scheme, "http") == 0)
      port = 80;
    else if (scheme && strcmp(scheme, "https") == 0)
      port = 443;
    else if (scheme && strcmp(scheme, "rtsp") == 0)
      port = 554;
    else {
      tvhlog(LOG_ERR, "httpc", "%04X: Unknown scheme '%s'", shortid(hc), scheme ? scheme : "");
      return -EINVAL;
    }
  }
  return port;
}

/*
 * Disable
 */
static void
http_client_shutdown ( http_client_t *hc, int force, int reconnect )
{
  struct http_client_ssl *ssl = hc->hc_ssl;

  hc->hc_shutdown = 1;
  if (ssl) {
    if (!ssl->shutdown) {
      SSL_shutdown(hc->hc_ssl->ssl);
      http_client_ssl_write_update(hc);
      ssl->shutdown = 1;
    }
    if (!force)
      return;
  }
  if (hc->hc_efd) {
    tvhpoll_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.fd       = hc->hc_fd;
    tvhpoll_rem(hc->hc_efd, &ev, 1);
    if (hc->hc_efd == http_poll && !reconnect) {
      pthread_mutex_lock(&http_lock);
      TAILQ_REMOVE(&http_clients, hc, hc_link);
      hc->hc_efd = NULL;
      pthread_mutex_unlock(&http_lock);
    } else {
      hc->hc_efd  = NULL;
    }
  }
  if (hc->hc_fd >= 0) {
    if (hc->hc_conn_closed) {
      pthread_mutex_unlock(&hc->hc_mutex);
      hc->hc_conn_closed(hc, -hc->hc_result);
      pthread_mutex_lock(&hc->hc_mutex);
    }
    if (hc->hc_fd >= 0)
      close(hc->hc_fd);
    hc->hc_fd = -1;
  }
}

/*
 * Poll I/O
 */
static void
http_client_poll_dir ( http_client_t *hc, int in, int out )
{
  int events = (in ? TVHPOLL_IN : 0) | (out ? TVHPOLL_OUT : 0);
  tvhpoll_event_t ev;
  if (hc->hc_efd) {
    if (events == 0 && hc->hc_pause) {
      if (hc->hc_pevents_pause == 0)
        hc->hc_pevents_pause = hc->hc_pevents;
      memset(&ev, 0, sizeof(ev));
      ev.fd       = hc->hc_fd;
      ev.data.ptr = hc;
      tvhpoll_rem(hc->hc_efd, &ev, 1);
    } else if (hc->hc_pevents != events) {
      memset(&ev, 0, sizeof(ev));
      ev.fd       = hc->hc_fd;
      ev.events   = events | TVHPOLL_IN;
      ev.data.ptr = hc;
      tvhpoll_add(hc->hc_efd, &ev, 1);
    }
  }
  hc->hc_pevents = events;
  /* make sure to se the correct errno for our SSL routines */
  errno = EAGAIN;
}

void
http_client_unpause( http_client_t *hc )
{
  if (hc->hc_pause) {
    http_client_poll_dir(hc, hc->hc_pevents_pause & TVHPOLL_IN,
                             hc->hc_pevents_pause & TVHPOLL_OUT);
    hc->hc_pause = 0;
    hc->hc_pevents_pause = 0;
  }
}

static void
http_client_direction ( http_client_t *hc, int sending )
{
  hc->hc_sending = sending;
  if (hc->hc_ssl == NULL)
    http_client_poll_dir(hc, 1, sending);
}

/*
 * Main I/O routines
 */

static void
http_client_cmd_destroy( http_client_t *hc, http_client_wcmd_t *cmd )
{
  TAILQ_REMOVE(&hc->hc_wqueue, cmd, link);
  free(cmd->wbuf);
  free(cmd);
}

static int
http_client_flush( http_client_t *hc, int result )
{
  hc->hc_result       = result;
  tvhtrace("httpc", "%04X: client flush %i", shortid(hc), result);
  if (result < 0)
    http_client_shutdown(hc, 0, 0);
  hc->hc_in_data      = 0;
  hc->hc_in_rtp_data  = 0;
  hc->hc_hsize        = 0;
  hc->hc_csize        = 0;
  hc->hc_rpos         = 0;
  hc->hc_chunked      = 0;
  free(hc->hc_chunk);
  hc->hc_chunk        = 0;
  hc->hc_chunk_pos    = 0;
  hc->hc_chunk_size   = 0;
  hc->hc_chunk_csize  = 0;
  hc->hc_chunk_alloc  = 0;
  hc->hc_chunk_trails = 0;
  http_arg_flush(&hc->hc_args);
  return result;
}

int
http_client_clear_state( http_client_t *hc )
{
  if (hc->hc_shutdown)
    return -EBADF;
  free(hc->hc_data);
  hc->hc_data = NULL;
  hc->hc_data_size = 0;
  return http_client_flush(hc, 0);
}

static int
http_client_einprogress( http_client_t *hc )
{
  struct pollfd fds;
  fds.fd      = hc->hc_fd;
  fds.events  = POLLOUT|POLLNVAL|POLLERR;
  fds.revents = 0;
  if (poll(&fds, 1, 0) == 0) {
    http_client_poll_dir(hc, 0, 1);
    return 1;
  }
  hc->hc_einprogress = 0;
  return 0;
}

static int
http_client_ssl_read_update( http_client_t *hc )
{
  struct http_client_ssl *ssl = hc->hc_ssl;
  char *rbuf = alloca(hc->hc_io_size);
  ssize_t r, r2;
  size_t len;

  if (ssl->rbio_pos > 0) {
    r = BIO_write(ssl->rbio, ssl->rbio_buf, ssl->rbio_pos);
    if (r >= 0) {
      memmove(ssl->rbio_buf, ssl->rbio_buf + r, ssl->rbio_pos - r);
      ssl->rbio_pos -= r;
    } else if (r < 0) {
      errno = EIO;
      return -1;
    }
  }
  r = recv(hc->hc_fd, rbuf, hc->hc_io_size, MSG_DONTWAIT);
  if (r == 0) {
    errno = EIO;
    return -1;
  }
  if (r < 0) {
    if (ERRNO_AGAIN(errno)) {
      http_client_poll_dir(hc, 1, 0);
      errno = EAGAIN;
      return r;
    }
    return r;
  }
  r2 = BIO_write(ssl->rbio, rbuf, r);
  len = r - (r2 < 0 ? 0 : r2);
  if (len) {
    if (ssl->rbio_pos + len > ssl->rbio_size) {
      ssl->rbio_buf = realloc(ssl->rbio_buf, ssl->rbio_pos + len);
      ssl->rbio_size += len;
    }
    memcpy(ssl->rbio_buf + ssl->rbio_pos, rbuf + (len - r), len);
    ssl->rbio_pos += len;
  }
  return 0;
}

static int
http_client_ssl_write_update( http_client_t *hc )
{
  struct http_client_ssl *ssl = hc->hc_ssl;
  char *rbuf = alloca(hc->hc_io_size);
  ssize_t r, r2;
  size_t len;

  if (ssl->wbio_pos) {
    r = send(hc->hc_fd, ssl->wbio_buf, ssl->wbio_pos, MSG_DONTWAIT);
    if (r > 0) {
      memmove(ssl->wbio_buf, ssl->wbio_buf + r, ssl->wbio_pos - r);
      ssl->wbio_pos -= r;
    } else if (r < 0) {
      if (ERRNO_AGAIN(errno)) {
        http_client_poll_dir(hc, 0, 1);
        errno = EAGAIN;
        return r;
      }
      return r;
    }
    if (ssl->wbio_pos)
      return 1;
  }
  r = BIO_read(ssl->wbio, rbuf, hc->hc_io_size);
  if (r > 0) {
    r2 = send(hc->hc_fd, rbuf, r, MSG_DONTWAIT);
    len = r - (r2 < 0 ? 0 : r2);
    if (len) {
      if (ssl->wbio_pos + len > ssl->wbio_size) {
        ssl->wbio_buf = realloc(ssl->wbio_buf, ssl->wbio_pos + len);
        ssl->wbio_size += len;
      }
      memcpy(ssl->wbio_buf + ssl->wbio_pos, rbuf + (len - r), len);
      ssl->wbio_pos += len;
    }
    if (r2 < 0) {
      if (ERRNO_AGAIN(errno)) {
        http_client_poll_dir(hc, 0, 1);
        errno = EAGAIN;
        return r2;
      }
      return r2;
    }
    return 1;
  }
  return 0;
}

static ssize_t
http_client_ssl_recv( http_client_t *hc, void *buf, size_t len )
{
  ssize_t r;
  int e;

  while (1) {
    r = SSL_read(hc->hc_ssl->ssl, buf, len);
    if (r > 0)
      return r;
    e = SSL_get_error(hc->hc_ssl->ssl, r);
    if (e == SSL_ERROR_WANT_READ) {
      r = http_client_ssl_write_update(hc);
      if (r < 0)
        return r;
      r = http_client_ssl_read_update(hc);
      if (r < 0)
        return r;
    } else if (e == SSL_ERROR_WANT_WRITE) {
      r = http_client_ssl_write_update(hc);
      if (r < 0)
        return r;
    } else if (e == SSL_ERROR_ZERO_RETURN) {
      errno = EIO;
      return -1;
    } else if (e == SSL_ERROR_WANT_CONNECT || e == SSL_ERROR_WANT_ACCEPT) {
      errno = EBADF;
      return -1;
    } else if (e == SSL_ERROR_SSL) {
      errno = EPERM;
      return -1;
    } else {
      errno = EIO;
      return -1;
    }
  }
  return 0;
}

static ssize_t
http_client_ssl_send( http_client_t *hc, const void *buf, size_t len )
{
  struct http_client_ssl *ssl = hc->hc_ssl;
  ssize_t r, r2;
  int e;

  if (hc->hc_verify_peer < 0)
    http_client_ssl_peer_verify(hc, 1); /* default method - verify */
  while (1) {
    if (!ssl->connected) {
      r = SSL_connect(ssl->ssl);
      if (r > 0) {
        ssl->connected = 1;
        if (hc->hc_verify_peer > 0) {
          if (SSL_get_peer_certificate(ssl->ssl) == NULL ||
              SSL_get_verify_result(ssl->ssl) != X509_V_OK) {
            tvhlog(LOG_ERR, "httpc", "%04X: SSL peer verification failed (%s:%i)%s %li",
                   shortid(hc), hc->hc_host, hc->hc_port,
                   SSL_get_peer_certificate(ssl->ssl) ? " X509" : "",
                   SSL_get_verify_result(ssl->ssl));
            errno = EPERM;
            return -1;
          }
        }
        goto write;
      }
    } else {
write:
      r = SSL_write(ssl->ssl, buf, len);
    }
    if (r > 0) {
      while (1) {
        r2 = http_client_ssl_write_update(hc);
        if (r2 < 0) {
          if (ERRNO_AGAIN(errno))
            break;
          return r2;
        }
        if (r2 == 0)
          break;
      }
      return r;
    }
    e = SSL_get_error(ssl->ssl, r);
    ERR_print_errors_fp(stdout);
    if (e == SSL_ERROR_WANT_READ) {
      r = http_client_ssl_write_update(hc);
      if (r < 0)
        return r;
      r = http_client_ssl_read_update(hc);
      if (r < 0)
        return r;
    } else if (e == SSL_ERROR_WANT_WRITE) {
      r = http_client_ssl_write_update(hc);
      if (r < 0)
        return r;
    } else if (e == SSL_ERROR_WANT_CONNECT || e == SSL_ERROR_WANT_ACCEPT) {
      errno = EBADF;
      return -1;
    } else if (e == SSL_ERROR_SSL) {
      errno = EPERM;
      return -1;
    } else {
      errno = EIO;
      return -1;
    }
  }
  return 0;
}

static ssize_t
http_client_ssl_shutdown( http_client_t *hc )
{
  ssize_t r;
  int e;

  while (1) {
    r = SSL_shutdown(hc->hc_ssl->ssl);
    if (r > 0) {
      /* everything done, bail-out completely */
      http_client_shutdown(hc, 1, 0);
      return r;
    }
    e = SSL_get_error(hc->hc_ssl->ssl, r);
    if (e == SSL_ERROR_WANT_READ) {
      r = http_client_ssl_write_update(hc);
      if (r < 0)
        return r;
      r = http_client_ssl_read_update(hc);
      if (r < 0)
        return r;
    } else if (e == SSL_ERROR_WANT_WRITE) {
      r = http_client_ssl_write_update(hc);
      if (r < 0)
        return r;
    } else if (e == SSL_ERROR_WANT_CONNECT || e == SSL_ERROR_WANT_ACCEPT) {
      errno = EBADF;
      return -1;
    } else if (e == SSL_ERROR_SSL) {
      errno = EPERM;
      return -1;
    } else {
      errno = EIO;
      return -1;
    }
  }
  return 0;
}

static int
http_client_send_partial( http_client_t *hc )
{
  http_client_wcmd_t *wcmd;
  ssize_t r;
  int res = HTTP_CON_IDLE;

  wcmd = TAILQ_FIRST(&hc->hc_wqueue);
  while (wcmd != NULL) {
    hc->hc_cmd   = wcmd->wcmd;
    hc->hc_rcseq = wcmd->wcseq;
    if (hc->hc_einprogress && http_client_einprogress(hc)) {
      errno = EINPROGRESS;
      r = -1;
      goto skip;
    }
    if (hc->hc_ssl)
      r = http_client_ssl_send(hc, wcmd->wbuf + wcmd->wpos,
                               wcmd->wsize - wcmd->wpos);
    else
      r = send(hc->hc_fd, wcmd->wbuf + wcmd->wpos,
               wcmd->wsize - wcmd->wpos, MSG_DONTWAIT);
skip:
    if (r < 0) {
      if (ERRNO_AGAIN(errno) || errno == EINPROGRESS) {
        http_client_direction(hc, 1);
        return HTTP_CON_SENDING;
      }
      return http_client_flush(hc, -errno);
    }
    wcmd->wpos += r;
    if (wcmd->wpos >= wcmd->wsize) {
      res = HTTP_CON_SENT;
      wcmd = NULL;
    }
    break;
  }
  if (wcmd == NULL) {
    http_client_direction(hc, 0);
    return res;
  } else {
    http_client_direction(hc, 1);
    return HTTP_CON_SENDING;
  }
}

int
http_client_send( http_client_t *hc, enum http_cmd cmd,
                  const char *path, const char *query,
                  http_arg_list_t *header, void *body, size_t body_size )
{
  http_client_wcmd_t *wcmd = calloc(1, sizeof(*wcmd));
  http_arg_t *h;
  htsbuf_queue_t q;
  const char *s;
  int empty;

  if (hc->hc_shutdown) {
    if (header)
      http_arg_flush(header);
    free(wcmd);
    return -EIO;
  }

  wcmd->wcmd = cmd;
  hc->hc_keepalive = 1;

  htsbuf_queue_init(&q, 0);
  s = http_cmd2str(cmd);
  if (s == NULL) {
error:
    if (header)
      http_arg_flush(header);
    free(wcmd);
    return -EINVAL;
  }
  htsbuf_append_str(&q, s);
  htsbuf_append(&q, " ", 1);
  if (path == NULL || path[0] == '\0')
    path = "/";
  htsbuf_append_str(&q, path);
  if (query && query[0] != '\0') {
    htsbuf_append(&q, "?", 1);
    htsbuf_append_str(&q, query);
  }
  htsbuf_append(&q, " ", 1);
  s = http_ver2str(hc->hc_version);
  if (s == NULL) {
    htsbuf_queue_flush(&q);
    goto error;
  }
  htsbuf_append_str(&q, s);
  htsbuf_append(&q, "\r\n", 2);

  if (header) {
    TAILQ_FOREACH(h, header, link) {
      htsbuf_append_str(&q, h->key);
      htsbuf_append(&q, ": ", 2);
      htsbuf_append_str(&q, h->val);
      htsbuf_append(&q, "\r\n", 2);
      if (strcasecmp(h->key, "Connection") == 0 &&
          strcasecmp(h->val, "close") == 0)
        hc->hc_keepalive = 0;
    }
    http_arg_flush(header);
  }

  if (hc->hc_version == HTTP_VERSION_1_0)
    hc->hc_keepalive = 0;
  if (hc->hc_version == RTSP_VERSION_1_0) {
    hc->hc_cseq = (hc->hc_cseq + 1) & 0x7fff;
    htsbuf_qprintf(&q, "CSeq: %i\r\n", hc->hc_cseq);
    wcmd->wcseq = hc->hc_cseq;
  }
  htsbuf_append(&q, "\r\n", 2);
  if (body && body_size)
    htsbuf_append(&q, body, body_size);

  body_size = q.hq_size;
  body = malloc(body_size);
  htsbuf_read(&q, body, body_size);

  if (tvhtrace_enabled()) {
    tvhtrace("httpc", "%04X: sending %s cmd", shortid(hc), http_ver2str(hc->hc_version));
    tvhlog_hexdump("httpc", body, body_size);
  }

  wcmd->wbuf  = body;
  wcmd->wsize = body_size;

  empty = TAILQ_EMPTY(&hc->hc_wqueue);
  TAILQ_INSERT_TAIL(&hc->hc_wqueue, wcmd, link);

  hc->hc_ping_time = mclk();

  if (empty)
    return http_client_send_partial(hc);
  return HTTP_CON_SENDING;
}

static int
http_client_finish( http_client_t *hc )
{
  http_client_wcmd_t *wcmd;
  int res;

  if (hc->hc_data && tvhtrace_enabled()) {
    tvhtrace("httpc", "%04X: received %s data", shortid(hc), http_ver2str(hc->hc_version));
    tvhlog_hexdump("httpc", hc->hc_data, hc->hc_csize);
  }
  if (hc->hc_in_rtp_data && hc->hc_rtp_data_complete) {
    pthread_mutex_unlock(&hc->hc_mutex);
    res = hc->hc_rtp_data_complete(hc);
    pthread_mutex_lock(&hc->hc_mutex);
    if (res < 0)
      return http_client_flush(hc, res);
  } else if (hc->hc_data_complete) {
    pthread_mutex_unlock(&hc->hc_mutex);
    res = hc->hc_data_complete(hc);
    pthread_mutex_lock(&hc->hc_mutex);
    if (res < 0)
      return http_client_flush(hc, res);
  }
  hc->hc_hsize = hc->hc_csize = 0;
  wcmd = TAILQ_FIRST(&hc->hc_wqueue);
  if (wcmd)
    http_client_cmd_destroy(hc, wcmd);
  if (hc->hc_version != RTSP_VERSION_1_0 &&
      hc->hc_handle_location &&
      (hc->hc_code == HTTP_STATUS_MOVED ||
       hc->hc_code == HTTP_STATUS_FOUND ||
       hc->hc_code == HTTP_STATUS_SEE_OTHER ||
       hc->hc_code == HTTP_STATUS_NOT_MODIFIED)) {
    const char *p = http_arg_get(&hc->hc_args, "Location");
    if (p) {
      hc->hc_location = strdup(p);
      res = http_client_redirected(hc);
      if (res < 0)
        return http_client_flush(hc, res);
      return HTTP_CON_RECEIVING;
    }
  }
  if (TAILQ_FIRST(&hc->hc_wqueue) && hc->hc_code == HTTP_STATUS_OK &&
      !hc->hc_in_rtp_data) {
    hc->hc_code = 0;
    return http_client_send_partial(hc);
  }
  if (!hc->hc_keepalive) {
    http_client_shutdown(hc, 0, 0);
    if (hc->hc_ssl) {
      /* finish the shutdown I/O sequence, notify owner later */
      errno = EAGAIN;
      return HTTP_CON_RECEIVING;
    }
  }
  return hc->hc_reconnected ? HTTP_CON_RECEIVING : HTTP_CON_DONE;
}

static int
http_client_parse_arg( http_arg_list_t *list, const char *p )
{
  char *d, *t;

  d = strchr(p, ':');
  if (d) {
    *d++ = '\0';
    while (*d && *d <= ' ')
      d++;
    t = d + strlen(d);
    while (--t != d && *t <= ' ')
      *t = '\0';
    http_arg_set(list, p, d);
    return 0;
  }
  return -EINVAL;
}

static int
http_client_data_copy( http_client_t *hc, char *buf, size_t len )
{
  int res;

  if (hc->hc_data_received) {
    pthread_mutex_unlock(&hc->hc_mutex);
    res = hc->hc_data_received(hc, buf, len);
    pthread_mutex_lock(&hc->hc_mutex);
    if (res < 0)
      return res;
  } else {
    hc->hc_data = realloc(hc->hc_data, hc->hc_data_size + len + 1);
    memcpy(hc->hc_data + hc->hc_data_size, buf, len);
    hc->hc_data_size += len;
    hc->hc_data[hc->hc_data_size] = '\0';
  }
  return 0;
}

static ssize_t
http_client_data_chunked( http_client_t *hc, char *buf, size_t len, int *end )
{
  size_t old = len, l, l2;
  char *d, *s;
  int res;

  while (len > 0) {
    if (hc->hc_chunk_size) {
      s = hc->hc_chunk;
      l = len;
      if (hc->hc_chunk_pos + l > hc->hc_chunk_size)
        l = hc->hc_chunk_size - hc->hc_chunk_pos;
      memcpy(s + hc->hc_chunk_pos, buf, l);
      hc->hc_chunk_pos += l;
      buf += l;
      len -= l;
      if (hc->hc_chunk_pos >= hc->hc_chunk_size) {
        if (s[hc->hc_chunk_size - 2] != '\r' &&
            s[hc->hc_chunk_size - 1] != '\n')
          return -EIO;
        res = http_client_data_copy(hc, hc->hc_chunk, hc->hc_chunk_size - 2);
        if (res < 0)
          return res;
        hc->hc_chunk_size = hc->hc_chunk_pos = 0;
      }
      continue;
    }
    l = 0;
    if (hc->hc_chunk_csize) {
      s = hc->hc_chunk;
      if (buf[0] == '\n' && s[hc->hc_chunk_csize-1] == '\r')
        l = 1;
      else if (len > 1 && buf[0] == '\r' && buf[1] == '\n')
        l = 2;
    } else {
      d = strstr(s = buf, "\r\n");
      if (d) {
        *d = '\0';
        l = (d + 2) - s;
      }
    }
    if (l) {
      hc->hc_chunk_csize = 0;
      if (hc->hc_chunk_trails) {
        buf += l;
        len -= l;
        if (s[0] == '\0') {
          *end = 1;
          return old - len;
        }
        res = http_client_parse_arg(&hc->hc_args, s);
        if (res < 0)
          return res;
        continue;
      }
      d = s + 1;
      while (*d == '0' && *d)
        d++;
      if (s[0] == '0' && *d == '\0')
        hc->hc_chunk_trails = 1;
      else {
        hc->hc_chunk_size = strtoll(s, NULL, 16);
        if (hc->hc_chunk_size == 0)
          return -EIO;
        if (hc->hc_chunk_size > 256*1024)
          return -EMSGSIZE;
        hc->hc_chunk_size += 2; /* CR-LF */
        if (hc->hc_chunk_alloc < hc->hc_chunk_size) {
          hc->hc_chunk = realloc(hc->hc_chunk, hc->hc_chunk_size + 1);
          hc->hc_chunk[hc->hc_chunk_size] = '\0';
          hc->hc_chunk_alloc = hc->hc_chunk_size;
        }
      }
      buf += l;
      len -= l;
    } else {
      l2 = hc->hc_chunk_csize + len;
      if (l2 > hc->hc_chunk_alloc) {
        hc->hc_chunk = realloc(hc->hc_chunk, l2 + 1);
        hc->hc_chunk[l2] = '\0';
        hc->hc_chunk_alloc = l2;
      }
      memcpy(hc->hc_chunk + hc->hc_chunk_csize, buf, len);
      hc->hc_chunk_csize += len;
      buf += len;
      len -= len;
    }
  }
  return old;
}

static int
http_client_data_received( http_client_t *hc, char *buf, ssize_t len, int hdr )
{
  ssize_t l, l2, csize;
  int res, end = 0;

  buf[len] = '\0';

  if (len == 0) {
    if (hc->hc_csize == -1)
      return 1;
    if (!hdr && hc->hc_rpos >= hc->hc_csize)
      return 1;
    return 0;
  }  

  csize = hc->hc_csize == (size_t) -1 ? 0 : hc->hc_csize;
  l = len;
  if (hc->hc_csize && hc->hc_csize != (size_t) -1 && hc->hc_rpos > csize) {
    l2 = hc->hc_rpos - csize;
    if (l2 < l)
      l = l2;
  }
  if (l) {
    if (hc->hc_chunked) {
      l = http_client_data_chunked(hc, buf, l, &end);
      if (l < 0)
        return l;
    } else {
      res = http_client_data_copy(hc, buf, l);
      if (res < 0)
        return res;
    }
  }
  hc->hc_rpos += l;
  end |= hc->hc_csize && hc->hc_rpos >= hc->hc_csize;
  if (l < len) {
    l2 = len - l;
    if (l2 > hc->hc_rsize)
      hc->hc_rbuf = realloc(hc->hc_rbuf, hc->hc_rsize = l2 + 1);
    memcpy(hc->hc_rbuf, buf + l, l2);
    hc->hc_rbuf[l2] = '\0';
    hc->hc_rpos = l2;
  }
  return end ? 1 : 0;
}

static int
http_client_run0( http_client_t *hc )
{
  char *buf, *saveptr, *argv[3], *d, *p;
  int ver, res, delimsize = 4;
  ssize_t r;
  size_t len;

  if (hc->hc_shutdown) {
    if (hc->hc_ssl && hc->hc_ssl->shutdown) {
      r = http_client_ssl_shutdown(hc);
      if (r < 0) {
        if (errno != EIO) {
          if (ERRNO_AGAIN(errno))
            return HTTP_CON_SENDING;
          return r;
        }
      }
      if (r == 0)
        return HTTP_CON_SENDING;
    }
    return hc->hc_result ? hc->hc_result : HTTP_CON_DONE;
  }

  if (hc->hc_sending) {
    res = http_client_send_partial(hc);
    if (res < 0 || res == HTTP_CON_SENDING)
      return res;
  }

  buf = alloca(hc->hc_io_size);
  if (!hc->hc_in_data && !hc->hc_in_rtp_data && hc->hc_rpos > 3) {
    if (hc->hc_version == RTSP_VERSION_1_0 && hc->hc_rbuf[0] == '$')
      goto rtsp_data;
    else if ((d = strstr(hc->hc_rbuf, "\r\n\r\n")) != NULL)
      goto header;
    if ((d = strstr(hc->hc_rbuf, "\n\n")) != NULL) {
      delimsize = 2;
      goto header;
    }
  }

retry:
  if (hc->hc_pause) {
    http_client_poll_dir(hc, 0, 0);
    return HTTP_CON_RECEIVING;
  }
  if (hc->hc_ssl)
    r = http_client_ssl_recv(hc, buf, hc->hc_io_size);
  else
    r = recv(hc->hc_fd, buf, hc->hc_io_size, MSG_DONTWAIT);
  if (r == 0) {
    if (hc->hc_in_data && !hc->hc_keepalive)
      return http_client_finish(hc);
    return http_client_flush(hc, -EIO);
  }
  if (r < 0) {
    if (errno == EIO && hc->hc_in_data && !hc->hc_keepalive)
      return http_client_finish(hc);
    if (ERRNO_AGAIN(errno))
      return HTTP_CON_RECEIVING;
    return http_client_flush(hc, -errno);
  }
  if (r > 0 && tvhtrace_enabled()) {
    tvhtrace("httpc", "%04X: received %s answer (len = %zd)", shortid(hc), http_ver2str(hc->hc_version), r);
    tvhlog_hexdump("httpc", buf, MIN(64, r));
  }

  if (hc->hc_in_data && !hc->hc_in_rtp_data) {
    res = http_client_data_received(hc, buf, r, 0);
    if (res < 0)
      return http_client_flush(hc, res);
    if (res > 0)
      return http_client_finish(hc);
    if (hc->hc_data_limit && r + hc->hc_rsize >= hc->hc_data_limit)
      return http_client_flush(hc, -EOVERFLOW);
    goto retry;
  }

  if (hc->hc_rsize < r + hc->hc_rpos) {
    if (hc->hc_rsize + r > hc->hc_io_size + 16*1024)
      return http_client_flush(hc, -EMSGSIZE);
    hc->hc_rsize += r;
    hc->hc_rbuf = realloc(hc->hc_rbuf, hc->hc_rsize + 1);
  }
  memcpy(hc->hc_rbuf + hc->hc_rpos, buf, r);
  hc->hc_rpos += r;
  hc->hc_rbuf[hc->hc_rpos] = '\0';

next_header:
  if (hc->hc_rpos < 3)
    return HTTP_CON_RECEIVING;
  if (hc->hc_version == RTSP_VERSION_1_0 && hc->hc_rbuf[0] == '$')
    goto rtsp_data;
  if ((d = strstr(hc->hc_rbuf, "\r\n\r\n")) == NULL) {
    delimsize = 2;
    if ((d = strstr(hc->hc_rbuf, "\n\n")) == NULL)
      return HTTP_CON_RECEIVING;
  }

header:
  *d = '\0';
  len = hc->hc_rpos;
  hc->hc_reconnected = 0;
  http_client_clear_state(hc);
  hc->hc_rpos  = len;
  hc->hc_hsize = d - hc->hc_rbuf + delimsize;
  p = strtok_r(hc->hc_rbuf, "\r\n", &saveptr);
  if (p == NULL)
    return http_client_flush(hc, -EINVAL);
  tvhtrace("httpc", "%04X: %s answer '%s' (rcseq: %d)",
           shortid(hc), http_ver2str(hc->hc_version), p, hc->hc_rcseq);
  tvhlog_hexdump("httpc", hc->hc_rbuf, hc->hc_hsize);
  if (http_tokenize(p, argv, 3, -1) != 3)
    return http_client_flush(hc, -EINVAL);
  if ((ver = http_str2ver(argv[0])) < 0)
    return http_client_flush(hc, -EINVAL);
  if (ver != hc->hc_version) {
    /* 1.1 -> 1.0 transition allowed */
    if (hc->hc_version == HTTP_VERSION_1_1 && ver == HTTP_VERSION_1_0)
      hc->hc_version = ver;
    else
      return http_client_flush(hc, -EINVAL);
  }
  if ((hc->hc_code = atoi(argv[1])) < 200)
    return http_client_flush(hc, -EINVAL);
  while ((p = strtok_r(NULL, "\r\n", &saveptr)) != NULL) {
    res = http_client_parse_arg(&hc->hc_args, p);
    if (res < 0)
      return http_client_flush(hc, -EINVAL);
  }
  p = http_arg_get(&hc->hc_args, "Content-Length");
  if (p) {
    hc->hc_csize = atoll(p);
    if (hc->hc_csize == 0)
      hc->hc_csize = -1;
  }
  p = http_arg_get(&hc->hc_args, "Connection");
  if (p && ver != RTSP_VERSION_1_0) {
    if (strcasecmp(p, "close") == 0)
      hc->hc_keepalive = 0;
    else if (strcasecmp(p, "keep-alive")) /* no change for keep-alive */
      return http_client_flush(hc, -EINVAL);
  }
  if (ver == RTSP_VERSION_1_0) {
    p = http_arg_get(&hc->hc_args, "CSeq");
    if (p == NULL || hc->hc_rcseq != atoi(p))
      return http_client_flush(hc, -EINVAL);
  }
  p = http_arg_get(&hc->hc_args, "Transfer-Encoding");
  if (p)
    hc->hc_chunked = strcasecmp(p, "chunked") == 0;
  if (hc->hc_hdr_received) {
    pthread_mutex_unlock(&hc->hc_mutex);
    res = hc->hc_hdr_received(hc);
    pthread_mutex_lock(&hc->hc_mutex);
    if (res < 0)
      return http_client_flush(hc, res);
  }
  len = hc->hc_rpos - hc->hc_hsize;
  hc->hc_rpos = 0;
  if (hc->hc_code == HTTP_STATUS_CONTINUE) {
    memmove(hc->hc_rbuf, hc->hc_rbuf + hc->hc_hsize, len);
    hc->hc_rpos = len;
    goto next_header;
  }
  if (hc->hc_version == RTSP_VERSION_1_0 &&
     (hc->hc_csize == -1 || !hc->hc_csize)) {
    hc->hc_csize = -1;
    hc->hc_in_data = 0;
    memmove(hc->hc_rbuf, hc->hc_rbuf + hc->hc_hsize, len);
    hc->hc_rpos = len;
    return http_client_finish(hc);
  } else {
    hc->hc_in_data = 1;
  }
  res = http_client_data_received(hc, hc->hc_rbuf + hc->hc_hsize, len, 1);
  if (res < 0)
    return http_client_flush(hc, res);
  if (res > 0)
    return http_client_finish(hc);
  goto retry;

rtsp_data:
  /* RTSP embedded data */
  r = 0;
  res = HTTP_CON_RECEIVING;
  hc->hc_in_data = 0;
  hc->hc_in_rtp_data = 0;
  while (hc->hc_rpos > r + 3) {
    hc->hc_csize = 4 + ((hc->hc_rbuf[r+2] << 8) | hc->hc_rbuf[r+3]);
    hc->hc_chunked = 0;
    if (r + hc->hc_csize > hc->hc_rpos) {
      memmove(hc->hc_rbuf, hc->hc_rbuf + r, hc->hc_rpos - r);
      hc->hc_rpos -= r;
      hc->hc_in_rtp_data = 1;
      if (r == 0)
        goto retry;
      return HTTP_CON_RECEIVING;
    }
    if (hc->hc_rtp_data_received) {
      pthread_mutex_unlock(&hc->hc_mutex);
      res = hc->hc_rtp_data_received(hc, hc->hc_rbuf + r, hc->hc_csize);
      pthread_mutex_lock(&hc->hc_mutex);
      if (res < 0)
        return res;
    } else {
      res = 0;
    }
    r += hc->hc_csize;
    hc->hc_in_rtp_data = 1;
    hc->hc_code = 0;
    res = http_client_finish(hc);
    hc->hc_in_rtp_data = 0;
    if (res < 0)
      return http_client_flush(hc, res);
    res = HTTP_CON_RECEIVING;
    if (hc->hc_rpos < r + 4 || hc->hc_rbuf[r] != '$') {
      memmove(hc->hc_rbuf, hc->hc_rbuf + r, hc->hc_rpos - r);
      hc->hc_rpos -= r;
      goto next_header;
    }
  }
  return res;
}

int
http_client_run( http_client_t *hc )
{
  int r;

  if (hc == NULL)
    return 0;
  pthread_mutex_lock(&hc->hc_mutex);
  r = http_client_run0(hc);
  pthread_mutex_unlock(&hc->hc_mutex);
  return r;
}

/*
 *
 */
void
http_client_basic_auth( http_client_t *hc, http_arg_list_t *h,
                        const char *user, const char *pass )
{
  if (user && user[0] && pass && pass[0]) {
#define BASIC "Basic "
    size_t plen = strlen(pass);
    size_t ulen = strlen(user);
    size_t len = BASE64_SIZE(plen + ulen + 1) + 1;
    char *buf = alloca(ulen + 1 + plen + 1);
    char *cbuf = alloca(len + sizeof(BASIC) + 1);
    strcpy(buf, user);
    strcat(buf, ":");
    strcat(buf, pass);
    strcpy(cbuf, BASIC);
    base64_encode(cbuf + sizeof(BASIC) - 1, len,
                  (uint8_t *)buf, ulen + 1 + plen);
    http_arg_set(h, "Authorization", cbuf);
#undef BASIC
  }
}

/*
 * Redirected
 */
void
http_client_basic_args ( http_client_t *hc, http_arg_list_t *h, const url_t *url, int keepalive )
{
  char buf[256];

  http_arg_flush(h);
  if (url->port == 0) { /* default port */
    http_arg_set(h, "Host", url->host);
  } else {
    snprintf(buf, sizeof(buf), "%s:%u", url->host,
                                        http_port(hc, url->scheme, url->port));
    http_arg_set(h, "Host", buf);
  }
  if (http_user_agent) {
    http_arg_set(h, "User-Agent", http_user_agent);
  } else {
    snprintf(buf, sizeof(buf), "TVHeadend/%s", tvheadend_version);
    http_arg_set(h, "User-Agent", buf);
  }
  if (!keepalive)
    http_arg_set(h, "Connection", "close");
  http_client_basic_auth(hc, h, url->user, url->pass);
}

static char *
strstrip(char *s)
{
  size_t l;
  if (s != NULL) {
    while (*s && *s <= ' ') s++;
    l = *s ? 0 : strlen(s) - 1;
    while (*s && s[l] <= ' ') s[l--] = '\0';
  }
  return s;
}

void
http_client_add_args ( http_client_t *hc, http_arg_list_t *h, const char *args )
{
  char *p, *k, *v;

  if (args == NULL)
    return;
  p = strdupa(args);
  while (*p) {
    while (*p && *p <= ' ') p++;
    if (*p == '\0') break;
    k = p;
    while (*p && *p != '\r' && *p != '\n' && *p != ':' && *p != '=') p++;
    v = NULL;
    if (*p == '=' || *p == ':') { *p = '\0'; p++; v = p; }
    while (*p && *p != '\r' && *p != '\n') p++;
    if (*p) { *p = '\0'; p++; }
    k = strstrip(k);
    v = strstrip(v);
    if (v && *v && *k &&
        strcasecmp(k, "Connection") != 0 &&
        strcasecmp(k, "Host") != 0) {
      http_arg_get_remove(h, k);
      http_arg_set(h, k, v);
    }
  }
}

int
http_client_simple_reconnect ( http_client_t *hc, const url_t *u,
                               http_ver_t ver )
{
  http_arg_list_t h;
  tvhpoll_t *efd;
  int r;

  if (u->scheme == NULL || u->scheme[0] == '\0' ||
      u->host == NULL || u->host[0] == '\0' ||
      u->port < 0) {
    tvherror("httpc", "Invalid url '%s'", u->raw);
    return -EINVAL;
  }
  if (strcmp(u->scheme, hc->hc_scheme) ||
      strcmp(u->host, hc->hc_host) ||
      http_port(hc, u->scheme, u->port) != hc->hc_port ||
      !hc->hc_keepalive) {
    efd = hc->hc_efd;
    http_client_shutdown(hc, 1, 1);
    r = http_client_reconnect(hc, hc->hc_version,
                              u->scheme, u->host, u->port);
    if (r < 0)
      return r;
    r = hc->hc_verify_peer;
    hc->hc_verify_peer = -1;
    http_client_ssl_peer_verify(hc, r);
    hc->hc_efd = efd;
  }

  http_client_flush(hc, 0);

  http_arg_init(&h);
  hc->hc_hdr_create(hc, &h, u, 0);
  hc->hc_reconnected = 1;
  hc->hc_shutdown    = 0;
  hc->hc_pevents     = 0;
  hc->hc_version     = ver;

  r = http_client_send(hc, hc->hc_cmd, u->path, u->query, &h, NULL, 0);
  if (r < 0)
    return r;

  hc->hc_reconnected = 1;
  free(hc->hc_url);
  hc->hc_url = u->raw ? strdup(u->raw) : NULL;
  return HTTP_CON_RECEIVING;
}

static int
http_client_redirected ( http_client_t *hc )
{
  char *location, *location2;
  url_t u;
  int r;

  if (++hc->hc_redirects > 10)
    return -ELOOP;

  location  = hc->hc_location;
  location2 = hc->hc_location = NULL;

  if (location[0] == '\0' || location[0] == '/') {
    size_t size2 = strlen(hc->hc_scheme) + 3 + strlen(hc->hc_host) +
                   12 + strlen(location) + 1;
    location2 = alloca(size2);
    snprintf(location2, size2, "%s://%s:%i%s",
        hc->hc_scheme, hc->hc_host, hc->hc_port, location);
  }

  urlinit(&u);
  if (urlparse(location2 ? location2 : location, &u)) {
    tvherror("httpc", "%04X: redirection - cannot parse url '%s'",
             shortid(hc), location2 ? location2 : location);
    free(location);
    return -EIO;
  }
  free(hc->hc_url);
  hc->hc_url = u.raw ? strdup(u.raw) : NULL;
  free(location);

  r = http_client_simple_reconnect(hc, &u, hc->hc_redirv);

  urlreset(&u);
  return r;
}

int
http_client_simple( http_client_t *hc, const url_t *url )
{
  http_arg_list_t h;
  int r;

  pthread_mutex_lock(&hc->hc_mutex);
  http_arg_init(&h);
  hc->hc_hdr_create(hc, &h, url, 0);
  free(hc->hc_url);
  hc->hc_url = url->raw ? strdup(url->raw) : NULL;
  r = http_client_send(hc, HTTP_CMD_GET, url->path, url->query,
                       &h, NULL, 0);
  pthread_mutex_unlock(&hc->hc_mutex);
  return r;
}

void
http_client_ssl_peer_verify( http_client_t *hc, int verify )
{
  struct http_client_ssl *ssl;

  if (hc->hc_verify_peer < 0) {
    hc->hc_verify_peer = verify ? 1 : 0;
    if ((ssl = hc->hc_ssl) != NULL) {
      if (!SSL_CTX_set_default_verify_paths(ssl->ctx))
        tvherror("httpc", "%04X: SSL - unable to load CA certificates for verification", shortid(hc));
      SSL_CTX_set_verify_depth(ssl->ctx, 1);
      SSL_CTX_set_verify(ssl->ctx,
                         hc->hc_verify_peer ? SSL_VERIFY_PEER : SSL_VERIFY_NONE,
                         NULL);
    }
  } else {
    tvherror("httpc", "%04X: SSL peer verification method must be set only once", shortid(hc));
  }
}

/*
 * Data thread
 */
static void *
http_client_thread ( void *p )
{
  int n;
  tvhpoll_event_t ev;
  http_client_t *hc;
  char c;

  while (atomic_get(&http_running)) {
    n = tvhpoll_wait(http_poll, &ev, 1, -1);
    if (n < 0) {
      if (atomic_get(&http_running) && !ERRNO_AGAIN(errno))
        tvherror("httpc", "tvhpoll_wait() error");
    } else if (n > 0) {
      if (&http_pipe == ev.data.ptr) {
        if (read(http_pipe.rd, &c, 1) == 1) {
          /* end-of-task */
          break;
        }
        continue;
      }
      pthread_mutex_lock(&http_lock);
      TAILQ_FOREACH(hc, &http_clients, hc_link)
        if (hc == ev.data.ptr)
          break;
      if (hc == NULL) {
        pthread_mutex_unlock(&http_lock);
        continue;
      }
      if (hc->hc_shutdown_wait) {
        tvh_cond_signal(&http_cond, 1);
        /* Disable the poll looping for this moment */
        http_client_poll_dir(hc, 0, 0);
        pthread_mutex_unlock(&http_lock);
        continue;
      }
      hc->hc_running = 1;
      pthread_mutex_unlock(&http_lock);
      http_client_run(hc);
      pthread_mutex_lock(&http_lock);
      hc->hc_running = 0;
      if (hc->hc_shutdown_wait)
        tvh_cond_signal(&http_cond, 1);
      pthread_mutex_unlock(&http_lock);
    }
  }

  return NULL;
}

static void
http_client_ssl_free( http_client_t *hc )
{
  struct http_client_ssl *ssl;

  if ((ssl = hc->hc_ssl) != NULL) {
    free(ssl->rbio_buf);
    free(ssl->wbio_buf);
    SSL_free(ssl->ssl);
    SSL_CTX_free(ssl->ctx);
    free(ssl);
    hc->hc_ssl = NULL;
  }
}

/*
 * Setup a connection (async)
 */
static int
http_client_reconnect
  ( http_client_t *hc, http_ver_t ver, const char *scheme,
    const char *host, int port )
{
  struct http_client_ssl *ssl;
  char errbuf[256];

  pthread_mutex_lock(&hc->hc_mutex);
  free(hc->hc_scheme);
  free(hc->hc_host);

  if (scheme == NULL || host == NULL)
    goto errnval;

  port           = http_port(hc, scheme, port);
  hc->hc_pevents = 0;
  hc->hc_version = ver;
  hc->hc_redirv  = ver;
  hc->hc_scheme  = strdup(scheme);
  hc->hc_host    = strdup(host);
  hc->hc_port    = port;
  hc->hc_fd      = tcp_connect(host, port, hc->hc_bindaddr, errbuf, sizeof(errbuf), -1);
  if (hc->hc_fd < 0) {
    tvhlog(LOG_ERR, "httpc", "%04X: Unable to connect to %s:%i - %s", shortid(hc), host, port, errbuf);
    goto errnval;
  }
  hc->hc_einprogress = 1;
  tvhtrace("httpc", "%04X: Connected to %s:%i", shortid(hc), host, port);
  http_client_ssl_free(hc);
  if (strcasecmp(scheme, "https") == 0 || strcasecmp(scheme, "rtsps") == 0) {
    ssl = calloc(1, sizeof(*ssl));
    hc->hc_ssl = ssl;
    ssl->ctx   = SSL_CTX_new(SSLv23_client_method());
    if (ssl->ctx == NULL) {
      tvhlog(LOG_ERR, "httpc", "%04X: Unable to get SSL_CTX", shortid(hc));
      goto err1;
    }
    /* do not use SSLv2 */
    SSL_CTX_set_options(ssl->ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_COMPRESSION);
    /* adjust cipher list */
    if (SSL_CTX_set_cipher_list(ssl->ctx, "HIGH:MEDIUM") != 1) {
      tvhlog(LOG_ERR, "httpc", "%04X: Unable to adjust SSL cipher list", shortid(hc));
      goto err2;
    }
    ssl->rbio  = BIO_new(BIO_s_mem());
    ssl->wbio  = BIO_new(BIO_s_mem());
    ssl->ssl   = SSL_new(ssl->ctx);
    if (ssl->ssl == NULL || ssl->rbio == NULL || ssl->wbio == NULL) {
      tvhlog(LOG_ERR, "httpc", "%04X: Unable to get SSL handle", shortid(hc));
      goto err3;
    }
    SSL_set_bio(ssl->ssl, ssl->rbio, ssl->wbio);
    if (!SSL_set_tlsext_host_name(ssl->ssl, host)) {
      tvhlog(LOG_ERR, "httpc", "%04X: Unable to set SSL hostname", shortid(hc));
      goto err4;
    }
  }

  pthread_mutex_unlock(&hc->hc_mutex);
  return 0;

err4:
  SSL_free(ssl->ssl);
  ssl->ssl = NULL;
err3:
  BIO_free(ssl->rbio);
  BIO_free(ssl->wbio);
  ssl->rbio = ssl->wbio = NULL;
err2:
  SSL_CTX_free(ssl->ctx);
  ssl->ctx = NULL;
err1:
  close(hc->hc_fd);
  hc->hc_fd = -1;
  free(ssl);
errnval:
  pthread_mutex_unlock(&hc->hc_mutex);
  return -EINVAL;
}

http_client_t *
http_client_connect 
  ( void *aux, http_ver_t ver, const char *scheme,
    const char *host, int port, const char *bindaddr )
{
  http_client_t *hc;
  static int tally;

  hc             = calloc(1, sizeof(http_client_t));
  pthread_mutex_init(&hc->hc_mutex, NULL);
  hc->hc_id      = ++tally;
  hc->hc_aux     = aux;
  hc->hc_io_size = 1024;
  hc->hc_rtsp_stream_id = -1;
  hc->hc_verify_peer = -1;
  hc->hc_bindaddr = bindaddr ? strdup(bindaddr) : NULL;

  TAILQ_INIT(&hc->hc_args);
  TAILQ_INIT(&hc->hc_wqueue);

  hc->hc_hdr_create = http_client_basic_args;

  if (http_client_reconnect(hc, ver, scheme, host, port) < 0) {
    free(hc);
    return NULL;
  }

  return hc;
}

/*
 * Register to the another thread
 */
void
http_client_register( http_client_t *hc )
{
  assert(hc->hc_data_received || hc->hc_conn_closed || hc->hc_data_complete);
  assert(hc->hc_efd == NULL);
  
  pthread_mutex_lock(&http_lock);

  TAILQ_INSERT_TAIL(&http_clients, hc, hc_link);

  hc->hc_efd  = http_poll;

  pthread_mutex_unlock(&http_lock);
}

/*
 * Cancel
 *
 * This function is not allowed to be called inside the callbacks for
 * registered clients to the http_client_thread .
 */
void
http_client_close ( http_client_t *hc )
{
  http_client_wcmd_t *wcmd;
  tvhpoll_event_t ev;

  if (hc == NULL)
    return;

  if (hc->hc_efd == http_poll) { /* http_client_thread */
    pthread_mutex_lock(&http_lock);
    hc->hc_shutdown_wait = 1;
    while (hc->hc_running)
      tvh_cond_wait(&http_cond, &http_lock);
    if (hc->hc_efd) {
      memset(&ev, 0, sizeof(ev));
      ev.fd = hc->hc_fd;
      tvhpoll_rem(hc->hc_efd, &ev, 1);
      TAILQ_REMOVE(&http_clients, hc, hc_link);
      hc->hc_efd = NULL;
    }
    pthread_mutex_unlock(&http_lock);
  }
  pthread_mutex_lock(&hc->hc_mutex);
  http_client_shutdown(hc, 1, 0);
  http_client_flush(hc, 0);
  tvhtrace("httpc", "%04X: Closed", shortid(hc));
  while ((wcmd = TAILQ_FIRST(&hc->hc_wqueue)) != NULL)
    http_client_cmd_destroy(hc, wcmd);
  http_client_ssl_free(hc);
  rtsp_clear_session(hc);
  pthread_mutex_unlock(&hc->hc_mutex);
  pthread_mutex_destroy(&hc->hc_mutex);
  free(hc->hc_url);
  free(hc->hc_location);
  free(hc->hc_rbuf);
  free(hc->hc_data);
  free(hc->hc_host);
  free(hc->hc_scheme);
  free(hc->hc_bindaddr);
  free(hc->hc_rtsp_user);
  free(hc->hc_rtsp_pass);
  free(hc);
}

/*
 * Initialise subsystem
 */
pthread_t http_client_tid;

void
http_client_init ( const char *user_agent )
{
  tvhpoll_event_t ev;

  http_user_agent = user_agent ? strdup(user_agent) : NULL;

  /* Setup list */
  pthread_mutex_init(&http_lock, NULL);
  tvh_cond_init(&http_cond);
  TAILQ_INIT(&http_clients);

  /* Setup pipe */
  tvh_pipe(O_NONBLOCK, &http_pipe);

  /* Setup poll */
  http_poll   = tvhpoll_create(10);
  memset(&ev, 0, sizeof(ev));
  ev.fd       = http_pipe.rd;
  ev.events   = TVHPOLL_IN;
  ev.data.ptr = &http_pipe;
  tvhpoll_add(http_poll, &ev, 1);

  /* Setup thread */
  atomic_set(&http_running, 1);
  tvhthread_create(&http_client_tid, NULL, http_client_thread, NULL, "httpc");
#if HTTPCLIENT_TESTSUITE
  http_client_testsuite_run();
#endif
}

void
http_client_done ( void )
{
  http_client_t *hc;

  atomic_set(&http_running, 0);
  tvh_write(http_pipe.wr, "", 1);
  pthread_join(http_client_tid, NULL);
  tvh_pipe_close(&http_pipe);
  pthread_mutex_lock(&http_lock);
  TAILQ_FOREACH(hc, &http_clients, hc_link)
    if (hc->hc_efd == http_poll)
      hc->hc_efd = NULL;
  tvhpoll_destroy(http_poll);
  http_poll = NULL;
  pthread_mutex_unlock(&http_lock);
  free(http_user_agent);
}

/*
 *
 * TESTSUITE
 *
 */

#if HTTPCLIENT_TESTSUITE

static int
http_client_testsuite_hdr_received( http_client_t *hc )
{
  http_arg_t *ra;

  fprintf(stderr, "HTTPCTS: Received header from %s:%i\n", hc->hc_host, hc->hc_port);
  TAILQ_FOREACH(ra, &hc->hc_args, link)
    fprintf(stderr, "  %s: %s\n", ra->key, ra->val);
  return 0;
}

static void
http_client_testsuite_conn_closed( http_client_t *hc, int result )
{
  fprintf(stderr, "HTTPCTS: Closed (result=%i - %s)\n", result, strerror(result));
}

static int
http_client_testsuite_data_complete( http_client_t *hc )
{
  fprintf(stderr, "HTTPCTS: Data Complete (code=%i, data=%p, data_size=%zi)\n",
          hc->hc_code, hc->hc_data, hc->hc_data_size);
  return 0;
}

static int
http_client_testsuite_data_received( http_client_t *hc, void *data, size_t len )
{
  fprintf(stderr, "HTTPCTS: Data received (len=%zi)\n", len);
  /* check, if the data memory area is OK */
  memset(data, 0xa5, len);
  return 0;
}

static struct strtab HTTP_contab[] = {
  { "WAIT_REQUEST", HTTP_CON_WAIT_REQUEST },
  { "READ_HEADER",  HTTP_CON_READ_HEADER },
  { "END",          HTTP_CON_END },
  { "POST_DATA",    HTTP_CON_POST_DATA },
  { "SENDING",      HTTP_CON_SENDING },
  { "SENT",         HTTP_CON_SENT },
  { "RECEIVING",    HTTP_CON_RECEIVING },
  { "DONE",         HTTP_CON_DONE },
};

static struct strtab ERRNO_tab[] = {
  { "EPERM",           EPERM },
  { "ENOENT",          ENOENT },
  { "ESRCH",           ESRCH },
  { "EINTR",           EINTR },
  { "EIO",             EIO },
  { "ENXIO",           ENXIO },
  { "E2BIG",           E2BIG },
  { "ENOEXEC",         ENOEXEC },
  { "EBADF",           EBADF },
  { "ECHILD",          ECHILD },
  { "EAGAIN",          EAGAIN },
  { "ENOMEM",          ENOMEM },
  { "EACCES",          EACCES },
  { "EFAULT",          EFAULT },
  { "ENOTBLK",         ENOTBLK },
  { "EBUSY",           EBUSY },
  { "EEXIST",          EEXIST },
  { "EXDEV",           EXDEV },
  { "ENODEV",          ENODEV },
  { "ENOTDIR",         ENOTDIR },
  { "EISDIR",          EISDIR },
  { "EINVAL",          EINVAL },
  { "ENFILE",          ENFILE },
  { "EMFILE",          EMFILE },
  { "ENOTTY",          ENOTTY },
  { "ETXTBSY",         ETXTBSY },
  { "EFBIG",           EFBIG },
  { "ENOSPC",          ENOSPC },
  { "ESPIPE",          ESPIPE },
  { "EROFS",           EROFS },
  { "EMLINK",          EMLINK },
  { "EPIPE",           EPIPE },
  { "EDOM",            EDOM },
  { "ERANGE",          ERANGE },
#ifdef __linux__
  { "EDEADLK",         EDEADLK },
  { "ENAMETOOLONG",    ENAMETOOLONG },
  { "ENOLCK",          ENOLCK },
  { "ENOSYS",          ENOSYS },
  { "ENOTEMPTY",       ENOTEMPTY },
  { "ELOOP",           ELOOP },
  { "EWOULDBLOCK",     EWOULDBLOCK },
  { "ENOMSG",          ENOMSG },
  { "EIDRM",           EIDRM },
  { "ECHRNG",          ECHRNG },
  { "EL2NSYNC",        EL2NSYNC },
  { "EL3HLT",          EL3HLT },
  { "EL3RST",          EL3RST },
  { "ELNRNG",          ELNRNG },
  { "EUNATCH",         EUNATCH },
  { "ENOCSI",          ENOCSI },
  { "EL2HLT",          EL2HLT },
  { "EBADE",           EBADE },
  { "EBADR",           EBADR },
  { "EXFULL",          EXFULL },
  { "ENOANO",          ENOANO },
  { "EBADRQC",         EBADRQC },
  { "EBADSLT",         EBADSLT },
  { "EDEADLOCK",       EDEADLOCK },
  { "EBFONT",          EBFONT },
  { "ENOSTR",          ENOSTR },
  { "ENODATA",         ENODATA },
  { "ETIME",           ETIME },
  { "ENOSR",           ENOSR },
  { "ENONET",          ENONET },
  { "ENOPKG",          ENOPKG },
  { "EREMOTE",         EREMOTE },
  { "ENOLINK",         ENOLINK },
  { "EADV",            EADV },
  { "ESRMNT",          ESRMNT },
  { "ECOMM",           ECOMM },
  { "EPROTO",          EPROTO },
  { "EMULTIHOP",       EMULTIHOP },
  { "EDOTDOT",         EDOTDOT },
  { "EBADMSG",         EBADMSG },
  { "EOVERFLOW",       EOVERFLOW },
  { "ENOTUNIQ",        ENOTUNIQ },
  { "EBADFD",          EBADFD },
  { "EREMCHG",         EREMCHG },
  { "ELIBACC",         ELIBACC },
  { "ELIBBAD",         ELIBBAD },
  { "ELIBSCN",         ELIBSCN },
  { "ELIBMAX",         ELIBMAX },
  { "ELIBEXEC",        ELIBEXEC },
  { "EILSEQ",          EILSEQ },
  { "ERESTART",        ERESTART },
  { "ESTRPIPE",        ESTRPIPE },
  { "EUSERS",          EUSERS },
  { "ENOTSOCK",        ENOTSOCK },
  { "EDESTADDRREQ",    EDESTADDRREQ },
  { "EMSGSIZE",        EMSGSIZE },
  { "EPROTOTYPE",      EPROTOTYPE },
  { "ENOPROTOOPT",     ENOPROTOOPT },
  { "EPROTONOSUPPORT", EPROTONOSUPPORT },
  { "ESOCKTNOSUPPORT", ESOCKTNOSUPPORT },
  { "EOPNOTSUPP",      EOPNOTSUPP },
  { "EPFNOSUPPORT",    EPFNOSUPPORT },
  { "EAFNOSUPPORT",    EAFNOSUPPORT },
  { "EADDRINUSE",      EADDRINUSE },
  { "EADDRNOTAVAIL",   EADDRNOTAVAIL },
  { "ENETDOWN",        ENETDOWN },
  { "ENETUNREACH",     ENETUNREACH },
  { "ENETRESET",       ENETRESET },
  { "ECONNABORTED",    ECONNABORTED },
  { "ECONNRESET",      ECONNRESET },
  { "ENOBUFS",         ENOBUFS },
  { "EISCONN",         EISCONN },
  { "ENOTCONN",        ENOTCONN },
  { "ESHUTDOWN",       ESHUTDOWN },
  { "ETOOMANYREFS",    ETOOMANYREFS },
  { "ETIMEDOUT",       ETIMEDOUT },
  { "ECONNREFUSED",    ECONNREFUSED },
  { "EHOSTDOWN",       EHOSTDOWN },
  { "EHOSTUNREACH",    EHOSTUNREACH },
  { "EALREADY",        EALREADY },
  { "EINPROGRESS",     EINPROGRESS },
  { "ESTALE",          ESTALE },
  { "EUCLEAN",         EUCLEAN },
  { "ENOTNAM",         ENOTNAM },
  { "ENAVAIL",         ENAVAIL },
  { "EISNAM",          EISNAM },
  { "EREMOTEIO",       EREMOTEIO },
  { "EDQUOT",          EDQUOT },
  { "ENOMEDIUM",       ENOMEDIUM },
  { "EMEDIUMTYPE",     EMEDIUMTYPE },
  { "ECANCELED",       ECANCELED },
  { "ENOKEY",          ENOKEY },
  { "EKEYEXPIRED",     EKEYEXPIRED },
  { "EKEYREVOKED",     EKEYREVOKED },
  { "EKEYREJECTED",    EKEYREJECTED },
  { "EOWNERDEAD",      EOWNERDEAD },
  { "ENOTRECOVERABLE", ENOTRECOVERABLE },
#ifdef ERFKILL
  { "ERFKILL",         ERFKILL },
#endif
#ifdef EHWPOISON
  { "EHWPOISON",       EHWPOISON },
#endif
#endif /* __linux__ */
};

void
http_client_testsuite_run( void )
{
  const char *path, *cs, *cs2;
  char line[1024], *s;
  http_arg_list_t args;
  http_client_t *hc = NULL;
  http_cmd_t cmd;
  http_ver_t ver = HTTP_VERSION_1_1;
  int data_transfer = 0, port = 0;
  size_t data_limit = 0;
  tvhpoll_event_t ev;
  tvhpoll_t *efd;
  url_t u1, u2;
  FILE *fp;
  int r, expected = HTTP_CON_DONE, expected_code = 200;
  int handle_location = 0;
  int peer_verify = 1;

  path = getenv("TVHEADEND_HTTPC_TEST");
  if (path == NULL)
    path = TVHEADEND_DATADIR "/support/httpc-test.txt";
  fp = tvh_fopen(path, "r");
  if (fp == NULL) {
    tvhlog(LOG_NOTICE, "httpc", "Test: unable to open '%s': %s", path, strerror(errno));
    return;
  }
  urlinit(&u1);
  urlinit(&u2);
  http_arg_init(&args);
  efd = tvhpoll_create(1);
  while (fgets(line, sizeof(line), fp) != NULL && tvheadend_is_running()) {
    if (line[0] == '\0')
      continue;
    s = line + strlen(line) - 1;
    while (*s < ' ' && s != line)
      s--;
    if (*s < ' ')
      *s = '\0';
    else
      s[1] = '\0';
    s = line;
    while (*s && *s < ' ')
      s++;
    if (*s == '\0' || *s == '#')
      continue;
    if (strcmp(s, "Reset=1") == 0) {
      ver = HTTP_VERSION_1_1;
      urlreset(&u1);
      urlreset(&u2);
      http_client_close(hc);
      hc = NULL;
      data_transfer = 0;
      data_limit = 0;
      port = 0;
      expected = HTTP_CON_DONE;
      expected_code = 200;
      handle_location = 0;
      peer_verify = 1;
    } else if (strcmp(s, "DataTransfer=all") == 0) {
      data_transfer = 0;
    } else if (strcmp(s, "DataTransfer=cont") == 0) {
      data_transfer = 1;
    } else if (strcmp(s, "HandleLocation=0") == 0) {
      handle_location = 0;
    } else if (strcmp(s, "HandleLocation=1") == 0) {
      handle_location = 1;
    } else if (strcmp(s, "SSLPeerVerify=0") == 0) {
      peer_verify = 0;
    } else if (strcmp(s, "SSLPeerVerify=1") == 0) {
      peer_verify = 1;
    } else if (strncmp(s, "DataLimit=", 10) == 0) {
      data_limit = atoll(s + 10);
    } else if (strncmp(s, "Port=", 5) == 0) {
      port = atoi(s + 5);
    } else if (strncmp(s, "ExpectedError=", 14) == 0) {
      r = str2val(s + 14, HTTP_contab);
      if (r < 0) {
        r = str2val(s + 14, ERRNO_tab);
        if (r < 0) {
          fprintf(stderr, "HTTPCTS: Unknown error code '%s'\n", s + 14);
          goto fatal;
        } else {
          r = -r;
        }
      }
      expected = r;
    } else if (strncmp(s, "ExpectedCode=", 13) == 0) {
      expected_code = atoi(s + 13);
    } else if (strncmp(s, "Header=", 7) == 0) {
      r = http_client_parse_arg(&args, s + 7);
      if (r < 0)
        goto fatal;
    } else if (strncmp(s, "Version=", 8) == 0) {
      ver = http_str2ver(s + 8);
      if (ver < 0)
        goto fatal;
    } else if (strncmp(s, "URL=", 4) == 0) {
      urlreset(&u1);
      if (urlparse(s + 4, &u1) < 0) {
        fprintf(stderr, "HTTPCTS: Parse URL error for '%s'\n", s + 4);
        goto fatal;
      }
    } else if (strncmp(s, "Command=", 8) == 0) {
      if (strcmp(s + 8, "EXIT") == 0)
        break;
      if (u1.host == NULL || u1.host[0] == '\0') {
        fprintf(stderr, "HTTPCTS: Define URL\n");
        goto fatal;
      }
      cmd = http_str2cmd(s + 8);
      if (cmd < 0)
        goto fatal;
      http_client_basic_args(hc, &args, &u1, 1);
      if (u2.host == NULL || u1.host == NULL || strcmp(u1.host, u2.host) ||
          u2.port != u1.port || !hc->hc_keepalive) {
        http_client_close(hc);
        if (port)
          u1.port = port;
        hc = http_client_connect(NULL, ver, u1.scheme, u1.host, u1.port, NULL);
        if (hc == NULL) {
          fprintf(stderr, "HTTPCTS: Unable to connect to %s:%i (%s)\n", u1.host, u1.port, u1.scheme);
          goto fatal;
        } else {
          fprintf(stderr, "HTTPCTS: Connected to %s:%i\n", hc->hc_host, hc->hc_port);
        }
        http_client_ssl_peer_verify(hc, peer_verify);
      }
      fprintf(stderr, "HTTPCTS Send: Cmd=%s Ver=%s Host=%s Path=%s\n",
              http_cmd2str(cmd), http_ver2str(ver), http_arg_get(&args, "Host"), u1.path);
      hc->hc_efd = efd;
      hc->hc_handle_location = handle_location;
      hc->hc_data_limit = data_limit;
      hc->hc_hdr_received = http_client_testsuite_hdr_received;
      hc->hc_data_complete = http_client_testsuite_data_complete;
      hc->hc_conn_closed = http_client_testsuite_conn_closed;
      if (data_transfer) {
        hc->hc_data_received = http_client_testsuite_data_received;
      } else {
        hc->hc_data_received = NULL;
      }
      r = http_client_send(hc, cmd, u1.path, u1.query, &args, NULL, 0);
      if (r < 0) {
        fprintf(stderr, "HTTPCTS Send Failed %s\n", strerror(-r));
        goto fatal;
      }
      while (tvheadend_is_running()) {
        fprintf(stderr, "HTTPCTS: Enter Poll\n");
        r = tvhpoll_wait(efd, &ev, 1, -1);
        fprintf(stderr, "HTTPCTS: Leave Poll: %i (%s)\n", r, r < 0 ? val2str(-r, ERRNO_tab) : "OK");
        if (r < 0 && ERRNO_AGAIN(errno))
          continue;
        if (r < 0) {
          fprintf(stderr, "HTTPCTS: Poll result: %s\n", strerror(-r));
          goto fatal;
        }
        if (r != 1)
          continue;
        if (ev.data.ptr != hc) {
          fprintf(stderr, "HTTPCTS: Poll returned a wrong value\n");
          goto fatal;
        }
        r = http_client_run(hc);
        cs = val2str(r, HTTP_contab);
        if (cs == NULL)
          cs = val2str(-r, ERRNO_tab);
        cs2 = val2str(expected, HTTP_contab);
        if (cs2 == NULL)
          cs2 = val2str(-expected, ERRNO_tab);
        fprintf(stderr, "HTTPCTS: Run Done, Result = %i (%s), Expected = %i (%s)\n", r, cs, expected, cs2);
        if (r == expected) {
          if (hc->hc_code != expected_code) {
            fprintf(stderr, "HTTPCTS: HTTP Code Fail: Expected = %i Got = %i\n", expected_code, hc->hc_code);
            goto fatal;
          }
          break;
        }
        if (r < 0)
          goto fatal;
        if (r == HTTP_CON_DONE)
          goto fatal;
      }
      urlreset(&u2);
      urlcopy(&u2, &u1);
      urlreset(&u1);
      http_client_clear_state(hc);
    } else {
      fprintf(stderr, "HTTPCTS: Wrong line '%s'\n", s);
    }
  }
  urlreset(&u2);
  urlreset(&u1);
  http_client_close(hc);
  tvhpoll_destroy(efd);
  http_arg_flush(&args);
  fclose(fp);
  fprintf(stderr, "HTTPCTS Return To Main\n");
  return;
fatal:
  fprintf(stderr, "HTTPCTS Fatal Error\n");
  exit(1);
}

#endif
