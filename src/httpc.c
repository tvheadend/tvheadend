/*
 *  Tvheadend - HTTP client functions (libcurl implementation)
 *
 *  Copyright (C) 2014 Jaroslav Kysela
 *  Copyright (C) 2024 Tvheadend Project
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
#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <curl/curl.h>

#if defined(PLATFORM_FREEBSD)
#include <sys/types.h>
#include <sys/socket.h>
#endif

#if ENABLE_ANDROID
#include <sys/socket.h>
#endif

/*
 * Configuration constants
 */
#define HTTPC_CONNECT_TIMEOUT   30      /* Connection timeout in seconds */
#define HTTPC_MAX_REDIRECTS     10      /* Maximum number of redirects to follow */
#define HTTPC_URL_BUFSIZE       2048    /* Buffer size for URL construction */
#define HTTPC_HDR_BUFSIZE       1024    /* Buffer size for header construction */

/*
 * Global state
 */
static int                      http_running;
static tvhpoll_t               *http_poll;
static TAILQ_HEAD(,http_client) http_clients;
static tvh_mutex_t              http_lock;
static tvh_cond_t               http_cond;
static th_pipe_t                http_pipe;
static CURLM                   *http_curl_multi;

/*
 * Helper functions
 */
static inline int
shortid( http_client_t *hc )
{
  return hc->hc_id;
}

static void
http_client_get( http_client_t *hc )
{
  hc->hc_refcnt++;
}

static void
http_client_put( http_client_t *hc )
{
  hc->hc_refcnt--;
}

static int
http_client_busy( http_client_t *hc )
{
  return !!hc->hc_refcnt;
}

static struct strtab HTTP_statetab[] = {
  { "WAIT_REQUEST",  HTTP_CON_WAIT_REQUEST },
  { "READ_HEADER",   HTTP_CON_READ_HEADER },
  { "END",           HTTP_CON_END },
  { "POST_DATA",     HTTP_CON_POST_DATA },
  { "SENDING",       HTTP_CON_SENDING },
  { "SENT",          HTTP_CON_SENT },
  { "RECEIVING",     HTTP_CON_RECEIVING },
  { "DONE",          HTTP_CON_DONE },
  { "IDLE",          HTTP_CON_IDLE },
  { "OK",            HTTP_CON_OK }
};

const char *
http_client_con2str(http_state_t val)
{
  return val2str(val, HTTP_statetab);
}

/*
 * HTTP port helper
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
      tvherror(LS_HTTPC, "%04X: Unknown scheme '%s'", shortid(hc), scheme ? scheme : "");
      return -EINVAL;
    }
  }
  return port;
}

/*
 * libcurl write callback - receives response body data
 */
static size_t
http_client_curl_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  http_client_t *hc = userdata;
  size_t realsize = size * nmemb;
  int res;

  if (hc->hc_shutdown)
    return 0;

  /* Check data limit */
  if (hc->hc_data_limit && hc->hc_data_size + realsize > hc->hc_data_limit) {
    tvhtrace(LS_HTTPC, "%04X: data overflow", shortid(hc));
    hc->hc_result = -EOVERFLOW;
    return 0;
  }

  /* Call data_received callback if set */
  if (hc->hc_data_received) {
    http_client_get(hc);
    tvh_mutex_unlock(&hc->hc_mutex);
    res = hc->hc_data_received(hc, ptr, realsize);
    tvh_mutex_lock(&hc->hc_mutex);
    http_client_put(hc);
    if (res < 0) {
      hc->hc_result = res;
      return 0;
    }
  } else {
    /* Store data in buffer */
    char *new_data = realloc(hc->hc_data, hc->hc_data_size + realsize + 1);
    if (new_data == NULL) {
      hc->hc_result = -ENOMEM;
      return 0;
    }
    hc->hc_data = new_data;
    memcpy(hc->hc_data + hc->hc_data_size, ptr, realsize);
    hc->hc_data_size += realsize;
    hc->hc_data[hc->hc_data_size] = '\0';
  }

  return realsize;
}

/*
 * libcurl header callback - receives response headers
 */
static size_t
http_client_curl_header_cb(char *buffer, size_t size, size_t nitems, void *userdata)
{
  http_client_t *hc = userdata;
  size_t realsize = size * nitems;
  char *line, *key, *val, *p;

  if (hc->hc_shutdown)
    return 0;

  /* Copy header line (removing trailing newlines) */
  line = alloca(realsize + 1);
  memcpy(line, buffer, realsize);
  line[realsize] = '\0';

  /* Remove trailing whitespace */
  p = line + realsize - 1;
  while (p >= line && (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t'))
    *p-- = '\0';

  /* Skip empty lines */
  if (line[0] == '\0')
    return realsize;

  /* Parse HTTP status line */
  if (strncasecmp(line, "HTTP/", 5) == 0 || strncasecmp(line, "RTSP/", 5) == 0) {
    /* Status line: HTTP/1.1 200 OK */
    p = strchr(line, ' ');
    if (p) {
      hc->hc_code = atoi(p + 1);
    }
    return realsize;
  }

  /* Parse header: Key: Value */
  key = line;
  val = strchr(line, ':');
  if (val) {
    *val++ = '\0';
    while (*val == ' ' || *val == '\t')
      val++;
    http_arg_set(&hc->hc_args, key, val);
  }

  return realsize;
}

/*
 * Clear client state
 */
int
http_client_clear_state( http_client_t *hc )
{
  if (hc->hc_shutdown)
    return -EBADF;
  free(hc->hc_data);
  hc->hc_data = NULL;
  hc->hc_data_size = 0;
  hc->hc_code = 0;
  hc->hc_result = 0;
  http_arg_flush(&hc->hc_args);
  return 0;
}

/*
 * Shutdown helper
 */
static void
http_client_shutdown( http_client_t *hc, int force, int reconnect )
{
  tvhtrace(LS_HTTPC, "%04X: shutdown", shortid(hc));
  hc->hc_shutdown = 1;

  if (hc->hc_curl) {
    if (http_curl_multi)
      curl_multi_remove_handle(http_curl_multi, hc->hc_curl);
    curl_easy_cleanup(hc->hc_curl);
    hc->hc_curl = NULL;
  }

  if (hc->hc_curl_headers) {
    curl_slist_free_all(hc->hc_curl_headers);
    hc->hc_curl_headers = NULL;
  }

  if (hc->hc_efd) {
    if (hc->hc_fd >= 0)
      tvhpoll_rem1(hc->hc_efd, hc->hc_fd);
    if (hc->hc_efd == http_poll && !reconnect) {
      tvh_mutex_lock(&http_lock);
      TAILQ_REMOVE(&http_clients, hc, hc_link);
      hc->hc_efd = NULL;
      tvh_mutex_unlock(&http_lock);
    } else {
      hc->hc_efd = NULL;
    }
  }

  if (hc->hc_fd >= 0) {
    if (hc->hc_conn_closed) {
      http_client_get(hc);
      tvh_mutex_unlock(&hc->hc_mutex);
      hc->hc_conn_closed(hc, -hc->hc_result);
      tvh_mutex_lock(&hc->hc_mutex);
      http_client_put(hc);
    }
    hc->hc_fd = -1;
  }
}

/*
 * Setup curl handle for a request
 */
static int
http_client_curl_setup( http_client_t *hc, const char *url, http_cmd_t cmd )
{
  CURL *curl;

  /* Clean up previous handle */
  if (hc->hc_curl) {
    curl_easy_cleanup(hc->hc_curl);
    hc->hc_curl = NULL;
  }
  if (hc->hc_curl_headers) {
    curl_slist_free_all(hc->hc_curl_headers);
    hc->hc_curl_headers = NULL;
  }

  curl = curl_easy_init();
  if (!curl) {
    tvherror(LS_HTTPC, "%04X: Failed to create curl handle", shortid(hc));
    return -ENOMEM;
  }

  hc->hc_curl = curl;

  /* Set URL */
  curl_easy_setopt(curl, CURLOPT_URL, url);

  /* Set callbacks */
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_client_curl_write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, hc);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, http_client_curl_header_cb);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, hc);

  /* Set private data for multi handle lookups */
  curl_easy_setopt(curl, CURLOPT_PRIVATE, hc);

  /* HTTP method */
  switch (cmd) {
    case HTTP_CMD_GET:
      curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
      break;
    case HTTP_CMD_HEAD:
      curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
      break;
    case HTTP_CMD_POST:
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      break;
    case RTSP_CMD_DESCRIBE:
      curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_DESCRIBE);
      break;
    case RTSP_CMD_OPTIONS:
      curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_OPTIONS);
      break;
    case RTSP_CMD_SETUP:
      curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_SETUP);
      break;
    case RTSP_CMD_TEARDOWN:
      curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_TEARDOWN);
      break;
    case RTSP_CMD_PLAY:
      curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_PLAY);
      break;
    case RTSP_CMD_PAUSE:
      curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_PAUSE);
      break;
    case RTSP_CMD_GET_PARAMETER:
      curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_GET_PARAMETER);
      break;
    default:
      break;
  }

  /* SSL peer verification */
  if (hc->hc_verify_peer > 0) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  } else {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }

  /* Follow redirects */
  if (hc->hc_handle_location) {
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, (long)HTTPC_MAX_REDIRECTS);
  }

  /* Keep-alive */
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, hc->hc_keepalive ? 1L : 0L);

  /* Bind address */
  if (hc->hc_bindaddr)
    curl_easy_setopt(curl, CURLOPT_INTERFACE, hc->hc_bindaddr);

  /* User agent */
  if (config.http_user_agent)
    curl_easy_setopt(curl, CURLOPT_USERAGENT, config.http_user_agent);

  /* Timeout */
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, (long)HTTPC_CONNECT_TIMEOUT);

  /* HTTP version */
  if (hc->hc_version == HTTP_VERSION_1_0)
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
  else if (hc->hc_version == HTTP_VERSION_1_1)
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

  return 0;
}

/*
 * Send HTTP request
 */
int
http_client_send( http_client_t *hc, enum http_cmd cmd,
                  const char *path, const char *query,
                  http_arg_list_t *header, void *body, size_t body_size )
{
  http_arg_t *h;
  char *url = NULL;
  size_t url_len;
  int r;
  struct curl_slist *headers = NULL;

  if (hc->hc_shutdown) {
    if (header)
      http_arg_flush(header);
    return -EIO;
  }

  hc->hc_cmd = cmd;
  hc->hc_keepalive = 1;

  /* Build URL */
  if (tvh_str_default(path, NULL) == NULL)
    path = "/";

  /* Calculate required URL buffer size */
  url_len = (hc->hc_scheme ? strlen(hc->hc_scheme) : 0) + 3 /* :// */ +
            (hc->hc_host ? strlen(hc->hc_host) : 0) + 6 /* :65535 */ +
            (path ? strlen(path) : 0) + 1 /* ? */ +
            (query ? strlen(query) : 0) + 1 /* \0 */;

  if (url_len <= HTTPC_URL_BUFSIZE) {
    url = alloca(HTTPC_URL_BUFSIZE);
    url_len = HTTPC_URL_BUFSIZE;
  } else {
    url = alloca(url_len);
  }

  if (hc->hc_version == RTSP_VERSION_1_0) {
    /* RTSP URLs need the full URL */
    if (path[0] == '/') {
      snprintf(url, url_len, "%s://%s:%d%s%s%s",
               hc->hc_scheme, hc->hc_host, hc->hc_port,
               path,
               query && query[0] ? "?" : "",
               query ? query : "");
    } else {
      snprintf(url, url_len, "%s%s%s",
               path,
               query && query[0] ? "?" : "",
               query ? query : "");
    }
  } else {
    snprintf(url, url_len, "%s://%s:%d%s%s%s",
             hc->hc_scheme, hc->hc_host, hc->hc_port,
             path,
             query && query[0] ? "?" : "",
             query ? query : "");
  }

  /* Setup curl handle */
  r = http_client_curl_setup(hc, url, cmd);
  if (r < 0) {
    if (header)
      http_arg_flush(header);
    return r;
  }

  /* Add headers */
  if (header) {
    TAILQ_FOREACH(h, header, link) {
      const char *val = h->val ? h->val : "";
      size_t key_len = h->key ? strlen(h->key) : 0;
      size_t val_len = strlen(val);
      size_t hdr_len = key_len + 2 + val_len + 1; /* "key: val\0" */
      char *hdr;
      
      if (hdr_len <= HTTPC_HDR_BUFSIZE) {
        hdr = alloca(HTTPC_HDR_BUFSIZE);
      } else {
        hdr = alloca(hdr_len);
      }
      snprintf(hdr, hdr_len, "%s: %s", h->key, val);
      headers = curl_slist_append(headers, hdr);
      if (strcasecmp(h->key, "Connection") == 0 &&
          strcasecmp(val, "close") == 0)
        hc->hc_keepalive = 0;
    }
    http_arg_flush(header);
  }

  if (headers) {
    curl_easy_setopt(hc->hc_curl, CURLOPT_HTTPHEADER, headers);
    hc->hc_curl_headers = headers;
  }

  /* Request body */
  if (body && body_size > 0) {
    curl_easy_setopt(hc->hc_curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(hc->hc_curl, CURLOPT_POSTFIELDSIZE, body_size);
  }

  /* RTSP CSeq */
  if (hc->hc_version == RTSP_VERSION_1_0) {
    hc->hc_cseq = (hc->hc_cseq + 1) & 0x7fff;
    curl_easy_setopt(hc->hc_curl, CURLOPT_RTSP_CLIENT_CSEQ, (long)hc->hc_cseq);
  }

  hc->hc_ping_time = mclk();
  hc->hc_sending = 1;

  tvhtrace(LS_HTTPC, "%04X: sending %s request to %s", shortid(hc),
           http_cmd2str(cmd), url);

  return HTTP_CON_SENDING;
}

/*
 * Run the HTTP client (process events)
 */
int
http_client_run( http_client_t *hc )
{
  CURLcode res;
  long response_code;
  int ret;

  if (hc == NULL)
    return 0;

  tvh_mutex_lock(&hc->hc_mutex);

  if (hc->hc_shutdown) {
    ret = hc->hc_result ? hc->hc_result : HTTP_CON_DONE;
    goto done;
  }

  if (!hc->hc_curl) {
    ret = HTTP_CON_IDLE;
    goto done;
  }

  /* Perform the request synchronously in the calling thread */
  res = curl_easy_perform(hc->hc_curl);

  if (res != CURLE_OK) {
    tvherror(LS_HTTPC, "%04X: curl error: %s", shortid(hc), curl_easy_strerror(res));
    hc->hc_result = -EIO;
    http_client_shutdown(hc, 0, 0);
    ret = hc->hc_result;
    goto done;
  }

  /* Get response code */
  curl_easy_getinfo(hc->hc_curl, CURLINFO_RESPONSE_CODE, &response_code);
  hc->hc_code = (int)response_code;

  hc->hc_sending = 0;

  /* Call header received callback */
  if (hc->hc_hdr_received) {
    http_client_get(hc);
    tvh_mutex_unlock(&hc->hc_mutex);
    ret = hc->hc_hdr_received(hc);
    tvh_mutex_lock(&hc->hc_mutex);
    http_client_put(hc);
    if (ret < 0) {
      hc->hc_result = ret;
      goto done;
    }
  }

  /* Call data complete callback */
  if (hc->hc_data_complete) {
    http_client_get(hc);
    tvh_mutex_unlock(&hc->hc_mutex);
    ret = hc->hc_data_complete(hc);
    tvh_mutex_lock(&hc->hc_mutex);
    http_client_put(hc);
    if (ret < 0) {
      hc->hc_result = ret;
      goto done;
    }
  }

  /* Clean up curl handle */
  if (hc->hc_curl) {
    curl_easy_cleanup(hc->hc_curl);
    hc->hc_curl = NULL;
  }
  if (hc->hc_curl_headers) {
    curl_slist_free_all(hc->hc_curl_headers);
    hc->hc_curl_headers = NULL;
  }

  ret = HTTP_CON_DONE;

done:
  tvh_mutex_unlock(&hc->hc_mutex);
  return ret;
}

/*
 * Basic auth helper
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
 * Basic args helper
 */
void
http_client_basic_args ( http_client_t *hc, http_arg_list_t *h, const url_t *url, int keepalive )
{
  char buf[256];

  http_arg_flush(h);
  if (url->port == 0) {
    http_arg_set(h, "Host", url->host);
  } else {
    snprintf(buf, sizeof(buf), "%s:%u", url->host,
                                        http_port(hc, url->scheme, url->port));
    http_arg_set(h, "Host", buf);
  }
  if (config.http_user_agent)
    http_arg_set(h, "User-Agent", config.http_user_agent);
  if (!keepalive)
    http_arg_set(h, "Connection", "close");
  http_client_basic_auth(hc, h, url->user, url->pass);
}

/*
 * Add args helper
 */
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
  p = tvh_strdupa(args);
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

/*
 * Simple reconnect
 */
int
http_client_simple_reconnect ( http_client_t *hc, const url_t *u,
                               http_ver_t ver )
{
  http_arg_list_t h;
  int r;

  lock_assert(&hc->hc_mutex);

  if (tvh_str_default(u->scheme, NULL) == NULL ||
      tvh_str_default(u->host, NULL) == NULL ||
      u->port < 0) {
    tvherror(LS_HTTPC, "Invalid url '%s'", u->raw);
    return -EINVAL;
  }

  /* Update connection info if needed */
  if (strcmp(u->scheme, hc->hc_scheme) ||
      strcmp(u->host, hc->hc_host) ||
      http_port(hc, u->scheme, u->port) != hc->hc_port ||
      !hc->hc_keepalive) {
    free(hc->hc_scheme);
    free(hc->hc_host);
    hc->hc_scheme = strdup(u->scheme);
    hc->hc_host = strdup(u->host);
    hc->hc_port = http_port(hc, u->scheme, u->port);
  }

  hc->hc_reconnected = 1;
  hc->hc_shutdown = 0;
  hc->hc_version = ver;

  if (ver != RTSP_VERSION_1_0) {
    http_arg_init(&h);
    hc->hc_hdr_create(hc, &h, u, 0);
    r = http_client_send(hc, hc->hc_cmd, u->path, u->query, &h, NULL, 0);
  } else
    r = http_client_send(hc, hc->hc_cmd, u->raw, u->query, NULL, NULL, 0);
  if (r < 0)
    return r;

  hc->hc_reconnected = 1;
  free(hc->hc_url);
  hc->hc_url = u->raw ? strdup(u->raw) : NULL;
  return HTTP_CON_RECEIVING;
}

/*
 * Simple request
 */
int
http_client_simple( http_client_t *hc, const url_t *url )
{
  http_arg_list_t h;
  int r;

  tvh_mutex_lock(&hc->hc_mutex);
  http_arg_init(&h);
  hc->hc_hdr_create(hc, &h, url, 0);
  free(hc->hc_url);
  hc->hc_url = url->raw ? strdup(url->raw) : NULL;
  r = http_client_send(hc, HTTP_CMD_GET, url->path, url->query,
                       &h, NULL, 0);
  tvh_mutex_unlock(&hc->hc_mutex);
  return r;
}

/*
 * SSL peer verification
 */
void
http_client_ssl_peer_verify( http_client_t *hc, int verify )
{
  if (hc->hc_verify_peer < 0) {
    hc->hc_verify_peer = verify ? 1 : 0;
  } else {
    tvherror(LS_HTTPC, "%04X: SSL peer verification method must be set only once", shortid(hc));
  }
}

/*
 * Unpause
 */
void
http_client_unpause( http_client_t *hc )
{
  if (hc->hc_pause) {
    tvhtrace(LS_HTTPC, "%04X: resuming input", shortid(hc));
    hc->hc_pause = 0;
  }
}

/*
 * Background thread for handling HTTP clients
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
        tvherror(LS_HTTPC, "tvhpoll_wait() error");
    } else if (n > 0) {
      if (&http_pipe == ev.ptr) {
        if (read(http_pipe.rd, &c, 1) == 1) {
          /* end-of-task */
          break;
        }
        continue;
      }
      tvh_mutex_lock(&http_lock);
      TAILQ_FOREACH(hc, &http_clients, hc_link)
        if (hc == ev.ptr)
          break;
      if (hc == NULL) {
        tvh_mutex_unlock(&http_lock);
        continue;
      }
      if (hc->hc_shutdown_wait) {
        tvh_cond_signal(&http_cond, 1);
        tvh_mutex_unlock(&http_lock);
        continue;
      }
      hc->hc_running = 1;
      tvh_mutex_unlock(&http_lock);
      http_client_run(hc);
      tvh_mutex_lock(&http_lock);
      hc->hc_running = 0;
      if (hc->hc_shutdown_wait)
        tvh_cond_signal(&http_cond, 1);
      tvh_mutex_unlock(&http_lock);
    }
  }

  return NULL;
}

/*
 * Connect to a server
 */
http_client_t *
http_client_connect 
  ( void *aux, http_ver_t ver, const char *scheme,
    const char *host, int port, const char *bindaddr )
{
  http_client_t *hc;
  static int tally;

  if (scheme == NULL || host == NULL)
    return NULL;

  hc = calloc(1, sizeof(http_client_t));
  tvh_mutex_init(&hc->hc_mutex, NULL);
  hc->hc_id = atomic_add(&tally, 1);
  hc->hc_aux = aux;
  hc->hc_io_size = 2048;
  hc->hc_rtsp_stream_id = -1;
  hc->hc_verify_peer = -1;
  hc->hc_bindaddr = bindaddr ? strdup(bindaddr) : NULL;
  hc->hc_fd = -1;

  TAILQ_INIT(&hc->hc_args);
  TAILQ_INIT(&hc->hc_wqueue);

  hc->hc_hdr_create = http_client_basic_args;

  port = http_port(hc, scheme, port);
  if (port < 0) {
    tvh_mutex_destroy(&hc->hc_mutex);
    free(hc);
    return NULL;
  }

  hc->hc_version = ver;
  hc->hc_redirv = ver;
  hc->hc_port = port;
  hc->hc_scheme = strdup(scheme);
  hc->hc_host = strdup(host);

  tvhtrace(LS_HTTPC, "%04X: Connected to %s:%i", shortid(hc), host, port);

  return hc;
}

/*
 * Register client with background thread
 */
void
http_client_register( http_client_t *hc )
{
  assert(hc->hc_data_received || hc->hc_conn_closed || hc->hc_data_complete);
  assert(hc->hc_efd == NULL);
  
  tvh_mutex_lock(&http_lock);
  TAILQ_INSERT_TAIL(&http_clients, hc, hc_link);
  hc->hc_efd = http_poll;
  tvh_mutex_unlock(&http_lock);
}

/*
 * Close and cleanup
 */
void
http_client_close ( http_client_t *hc )
{
  http_client_wcmd_t *wcmd;

  if (hc == NULL)
    return;

  if (hc->hc_efd == http_poll) {
    tvh_mutex_lock(&http_lock);
    hc->hc_shutdown_wait = 1;
    while (hc->hc_running)
      tvh_cond_wait(&http_cond, &http_lock);
    if (hc->hc_efd) {
      if (hc->hc_fd >= 0)
        tvhpoll_rem1(hc->hc_efd, hc->hc_fd);
      TAILQ_REMOVE(&http_clients, hc, hc_link);
      hc->hc_efd = NULL;
    }
    tvh_mutex_unlock(&http_lock);
  }

  tvh_mutex_lock(&hc->hc_mutex);
  while (http_client_busy(hc)) {
    tvh_mutex_unlock(&hc->hc_mutex);
    tvh_safe_usleep(10000);
    tvh_mutex_lock(&hc->hc_mutex);
  }
  http_client_shutdown(hc, 1, 0);
  http_client_clear_state(hc);
  tvhtrace(LS_HTTPC, "%04X: Closed", shortid(hc));
  while ((wcmd = TAILQ_FIRST(&hc->hc_wqueue)) != NULL) {
    TAILQ_REMOVE(&hc->hc_wqueue, wcmd, link);
    free(wcmd->wbuf);
    free(wcmd);
  }
  rtsp_clear_session(hc);
  tvh_mutex_unlock(&hc->hc_mutex);
  tvh_mutex_destroy(&hc->hc_mutex);
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
 * Initialize subsystem
 */
pthread_t http_client_tid;

void
http_client_init ( void )
{
  /* Initialize libcurl */
  curl_global_init(CURL_GLOBAL_ALL);
  http_curl_multi = curl_multi_init();

  /* Setup list */
  tvh_mutex_init(&http_lock, NULL);
  tvh_cond_init(&http_cond, 1);
  TAILQ_INIT(&http_clients);

  /* Setup pipe */
  tvh_pipe(O_NONBLOCK, &http_pipe);

  /* Setup poll */
  http_poll = tvhpoll_create(10);
  tvhpoll_add1(http_poll, http_pipe.rd, TVHPOLL_IN, &http_pipe);

  /* Setup thread */
  atomic_set(&http_running, 1);
  tvh_thread_create(&http_client_tid, NULL, http_client_thread, NULL, "httpc");
}

void
http_client_done ( void )
{
  http_client_t *hc;

  atomic_set(&http_running, 0);
  tvh_write(http_pipe.wr, "", 1);
  pthread_join(http_client_tid, NULL);
  tvh_pipe_close(&http_pipe);
  tvh_mutex_lock(&http_lock);
  TAILQ_FOREACH(hc, &http_clients, hc_link)
    if (hc->hc_efd == http_poll)
      hc->hc_efd = NULL;
  tvhpoll_destroy(http_poll);
  http_poll = NULL;
  tvh_mutex_unlock(&http_lock);

  /* Cleanup libcurl */
  if (http_curl_multi) {
    curl_multi_cleanup(http_curl_multi);
    http_curl_multi = NULL;
  }
  curl_global_cleanup();
}
