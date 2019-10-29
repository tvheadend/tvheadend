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

#include "tvheadend.h"
#include "tcp.h"
#include "http.h"
#include "access.h"
#include "notify.h"
#include "channels.h"
#include "config.h"
#include "htsmsg_json.h"
#include "compat.h"

#include "openssl/opensslv.h"

#if ENABLE_ANDROID
#include <sys/socket.h>
#endif

void *http_server;
static int http_server_running;

static tvh_mutex_t http_paths_mutex = TVH_THREAD_MUTEX_INITIALIZER;
static http_path_list_t http_paths;

static struct strtab HTTP_cmdtab[] = {
  { "NONE",          HTTP_CMD_NONE },
  { "GET",           HTTP_CMD_GET },
  { "HEAD",          HTTP_CMD_HEAD },
  { "POST",          HTTP_CMD_POST },
  { "DESCRIBE",      RTSP_CMD_DESCRIBE },
  { "OPTIONS",       RTSP_CMD_OPTIONS },
  { "SETUP",         RTSP_CMD_SETUP },
  { "PLAY",          RTSP_CMD_PLAY },
  { "TEARDOWN",      RTSP_CMD_TEARDOWN },
  { "PAUSE",         RTSP_CMD_PAUSE },
  { "GET_PARAMETER", RTSP_CMD_GET_PARAMETER },
};



static struct strtab HTTP_versiontab[] = {
  { "HTTP/1.0",        HTTP_VERSION_1_0 },
  { "HTTP/1.1",        HTTP_VERSION_1_1 },
  { "RTSP/1.0",        RTSP_VERSION_1_0 },
};

/**
 *
 */
const char *
http_cmd2str(int val)
{
  return val2str(val, HTTP_cmdtab);
}

int http_str2cmd(const char *str)
{
  return str2val(str, HTTP_cmdtab);
}

const char *
http_ver2str(int val)
{
  return val2str(val, HTTP_versiontab);
}

int http_str2ver(const char *str)
{
  return str2val(str, HTTP_versiontab);
}

/**
 *
 */
static int
http_resolve(http_connection_t *hc, http_path_t *_hp,
             char **remainp, char **argsp)
{
  http_path_t *hp;
  int n = 0, cut = 0;
  char *v, *path = tvh_strdupa(hc->hc_url);
  char *npath;

  /* Canocalize path (or at least remove excess slashes) */
  v  = path;
  while (*v && *v != '?') {
    if (*v == '/' && v[1] == '/') {
      int l = strlen(v+1);
      memmove(v, v+1, l);
      v[l] = 0;
      n++;
    } else
      v++;
  }

  while (1) {

    tvh_mutex_lock(hc->hc_paths_mutex);
    LIST_FOREACH(hp, hc->hc_paths, hp_link) {
      if(!strncmp(path, hp->hp_path, hp->hp_len)) {
        if(path[hp->hp_len] == 0 ||
           path[hp->hp_len] == '/' ||
           path[hp->hp_len] == '?')
          break;
      }
    }
    if (hp) {
      *_hp = *hp;
      hp = _hp;
    }
    tvh_mutex_unlock(hc->hc_paths_mutex);

    if(hp == NULL)
      return 0;

    cut += hp->hp_len;

    if(hp->hp_path_modify == NULL)
      break;

    npath = hp->hp_path_modify(hc, path, &cut);
    if(npath == NULL)
      break;

    path = tvh_strdupa(npath);
    free(npath);

  }

  v = hc->hc_url + n + cut;

  *remainp = NULL;
  *argsp = NULL;

  switch(*v) {
  case 0:
    break;

  case '/':
    if(v[1] == '?') {
      *argsp = v + 1;
      break;
    }

    *remainp = v + 1;
    v = strchr(v + 1, '?');
    if(v != NULL) {
      *v = 0;  /* terminate remaining url */
      *argsp = v + 1;
    }
    break;

  case '?':
    *v = 0; /* terminate remaining url */
    *argsp = v + 1;
    break;

  default:
    return 0;
  }

  return 1;
}



/*
 * HTTP status code to string
 */

static const char *
http_rc2str(int code)
{
  switch(code) {
  case HTTP_STATUS_PSWITCH:         /* 101 */ return "Switching Protocols";
  case HTTP_STATUS_OK:              /* 200 */ return "OK";
  case HTTP_STATUS_PARTIAL_CONTENT: /* 206 */ return "Partial Content";
  case HTTP_STATUS_FOUND:           /* 302 */ return "Found";
  case HTTP_STATUS_BAD_REQUEST:     /* 400 */ return "Bad Request";
  case HTTP_STATUS_UNAUTHORIZED:    /* 401 */ return "Unauthorized";
  case HTTP_STATUS_FORBIDDEN:       /* 403 */ return "Forbidden";
  case HTTP_STATUS_NOT_FOUND:       /* 404 */ return "Not Found";
  case HTTP_STATUS_NOT_ALLOWED:     /* 405 */ return "Method Not Allowed";
  case HTTP_STATUS_UNSUPPORTED:     /* 415 */ return "Unsupported Media Type";
  case HTTP_STATUS_BANDWIDTH:       /* 453 */ return "Not Enough Bandwidth";
  case HTTP_STATUS_BAD_SESSION:     /* 454 */ return "Session Not Found";
  case HTTP_STATUS_INTERNAL:        /* 500 */ return "Internal Server Error";
  case HTTP_STATUS_HTTP_VERSION:    /* 505 */ return "HTTP/RTSP Version Not Supported";
default:
    return "Unknown Code";
    break;
  }
}

static const char *cachedays[7] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char *cachemonths[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov",
  "Dec"
};

/**
 * Digest authentication helpers
 */
typedef struct http_nonce {
  RB_ENTRY(http_nonce) link;
  mtimer_t expire;
  char nonce[64];
} http_nonce_t;

static RB_HEAD(, http_nonce) http_nonces;

static int
http_nonce_cmp(const void *a, const void *b)
{
  return strcmp(((http_nonce_t *)a)->nonce, ((http_nonce_t *)b)->nonce);
}

static void
http_nonce_timeout(void *aux)
{
  struct http_nonce *n = aux;
  RB_REMOVE(&http_nonces, n, link);
  free(n);
}

static char *
http_get_digest_hash(int algo, const char *msg)
{
  switch (algo) {
  case HTTP_AUTH_ALGO_MD5:
    return md5sum(msg, 1);
  case HTTP_AUTH_ALGO_SHA256:
    return sha256sum(msg, 1);
  default:
    return sha512sum256(msg, 1);
  }
}

static int
http_get_nonce(http_connection_t *hc)
{
  struct http_nonce *n = calloc(1, sizeof(*n));
  char stamp[33], *m;
  int64_t mono;
  static int64_t xor;

  while (1) {
    mono = getmonoclock();
    mono ^= 0xa1687211885fcd30LL;
    xor ^= 0xf6e398624aa55013LL;
    snprintf(stamp, sizeof(stamp), "A!*Fz32%"PRId64"%"PRId64, mono, xor);
    m = sha256sum_base64(stamp);
    if (m == NULL) return -1;
    strlcpy(n->nonce, m, sizeof(n->nonce));
    tvh_mutex_lock(&global_lock);
    if (RB_INSERT_SORTED(&http_nonces, n, link, http_nonce_cmp)) {
      tvh_mutex_unlock(&global_lock);
      free(m);
      continue; /* get unique hash */
    }
    mtimer_arm_rel(&n->expire, http_nonce_timeout, n, sec2mono(30));
    tvh_mutex_unlock(&global_lock);
    break;
  }
  hc->hc_nonce = m;
  return 0;
}

static int
http_nonce_exists(const char *nonce)
{
  struct http_nonce *n, tmp;

  if (nonce == NULL)
    return 0;
  strlcpy(tmp.nonce, nonce, sizeof(tmp.nonce));
  tvh_mutex_lock(&global_lock);
  n = RB_FIND(&http_nonces, &tmp, link, http_nonce_cmp);
  if (n) {
    mtimer_arm_rel(&n->expire, http_nonce_timeout, n, sec2mono(2*60));
    tvh_mutex_unlock(&global_lock);
    return 1;
  }
  tvh_mutex_unlock(&global_lock);
  return 0;
}

static char *
http_get_opaque(http_connection_t *hc, const char *realm)
{
  char *a = alloca(strlen(realm) + strlen(hc->hc_nonce) + 1);
  strcpy(a, realm);
  strcat(a, hc->hc_nonce);
  return sha256sum_base64(a);
}

/**
 *
 */
void
http_alive(http_connection_t *hc)
{
  if (hc->hc_nonce)
    http_nonce_exists(hc->hc_nonce); /* update timer */
}

/**
 *
 */
static void
http_auth_header
  (htsbuf_queue_t *hdrs, const char *realm, const char *algo,
   const char *nonce, const char *opaque)
{
  htsbuf_qprintf(hdrs, "WWW-Authenticate: Digest realm=\"%s\", qop=auth", realm);
  if (algo)
    htsbuf_qprintf(hdrs, ", algorithm=%s", algo);
  htsbuf_qprintf(hdrs, ", nonce=\"%s\"", nonce);
  htsbuf_qprintf(hdrs, ", opaque=\"%s\"\r\n", opaque);
}

/**
 * Transmit a HTTP reply
 */
void
http_send_header(http_connection_t *hc, int rc, const char *content,
		 int64_t contentlen,
		 const char *encoding, const char *location, 
		 int maxage, const char *range,
		 const char *disposition,
		 http_arg_list_t *args)
{
  struct tm tm0, *tm;
  htsbuf_queue_t hdrs;
  http_arg_t *ra;
  time_t t;
  int sess = 0;

  assert(atomic_get(&hc->hc_extra_insend) > 0);

  htsbuf_queue_init(&hdrs, 0);

  htsbuf_qprintf(&hdrs, "%s %d %s\r\n", 
		 http_ver2str(hc->hc_version), rc, http_rc2str(rc));

  if (hc->hc_version != RTSP_VERSION_1_0){
    htsbuf_qprintf(&hdrs, "Server: %s\r\n", config_get_http_server_name());
    if (config.cors_origin && config.cors_origin[0]) {
      htsbuf_qprintf(&hdrs, "Access-Control-Allow-Origin: %s\r\n%s%s%s", config.cors_origin,
                            "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n",
                            "Access-Control-Allow-Headers: x-requested-with,authorization\r\n",
                            "Access-Control-Allow-Credentials: true\r\n");
    }
  }
  
  if(maxage == 0) {
    if (hc->hc_version != RTSP_VERSION_1_0)
      htsbuf_append_str(&hdrs, "Cache-Control: no-cache\r\n");
  } else if (maxage > 0) {
    time(&t);

    tm = gmtime_r(&t, &tm0);
    htsbuf_qprintf(&hdrs, 
                "Last-Modified: %s, %d %s %02d %02d:%02d:%02d GMT\r\n",
                cachedays[tm->tm_wday], tm->tm_mday, 
                cachemonths[tm->tm_mon], tm->tm_year + 1900,
                tm->tm_hour, tm->tm_min, tm->tm_sec);

    t += maxage;

    tm = gmtime_r(&t, &tm0);
    htsbuf_qprintf(&hdrs, 
		"Expires: %s, %d %s %02d %02d:%02d:%02d GMT\r\n",
		cachedays[tm->tm_wday],	tm->tm_mday,
                cachemonths[tm->tm_mon], tm->tm_year + 1900,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
      
    htsbuf_qprintf(&hdrs, "Cache-Control: max-age=%d\r\n", maxage);
  }

  if(rc == HTTP_STATUS_UNAUTHORIZED) {
    const char *realm = tvh_str_default(config.realm, "tvheadend");
    if (config.http_auth == HTTP_AUTH_DIGEST ||
        config.http_auth == HTTP_AUTH_PLAIN_DIGEST) {
      char *opaque;
      if (hc->hc_nonce == NULL && http_get_nonce(hc)) goto __noauth;
      opaque = http_get_opaque(hc, realm);
      if (opaque == NULL) goto __noauth;
      if (config.http_auth_algo != HTTP_AUTH_ALGO_MD5)
        http_auth_header(&hdrs, realm,
                         config.http_auth_algo == HTTP_AUTH_ALGO_SHA256 ?
                           "SHA-256" :
#if OPENSSL_VERSION_NUMBER >= 0x1010101fL
                             "SHA-512-256",
#else
                             "SHA-256",
#endif
                           hc->hc_nonce, opaque);
      http_auth_header(&hdrs, realm, NULL, hc->hc_nonce, opaque);
      free(opaque);
    } else {
      htsbuf_qprintf(&hdrs, "WWW-Authenticate: Basic realm=\"%s\"\r\n", realm);
    }
  }
__noauth:

  if (hc->hc_version != RTSP_VERSION_1_0)
    htsbuf_qprintf(&hdrs, "Connection: %s\r\n",
	           hc->hc_keep_alive ? "Keep-Alive" : "Close");

  if(encoding != NULL)
    htsbuf_qprintf(&hdrs, "Content-Encoding: %s\r\n", encoding);

  if(location != NULL)
    htsbuf_qprintf(&hdrs, "Location: %s\r\n", location);

  if(content != NULL)
    htsbuf_qprintf(&hdrs, "Content-Type: %s\r\n", content);

  if(contentlen > 0)
    htsbuf_qprintf(&hdrs, "Content-Length: %"PRId64"\r\n", contentlen);
  else if(contentlen == INT64_MIN)
    htsbuf_append_str(&hdrs, "Content-Length: 0\r\n");

  if(range) {
    htsbuf_qprintf(&hdrs, "Accept-Ranges: %s\r\n", "bytes");
    htsbuf_qprintf(&hdrs, "Content-Range: %s\r\n", range);
  }

  if(disposition != NULL)
    htsbuf_qprintf(&hdrs, "Content-Disposition: %s\r\n", disposition);
  
  if(hc->hc_cseq) {
    htsbuf_qprintf(&hdrs, "CSeq: %"PRIu64"\r\n", hc->hc_cseq);
    if (++hc->hc_cseq == 0)
      hc->hc_cseq = 1;
  }

  if (args) {
    TAILQ_FOREACH(ra, args, link) {
      if (strcmp(ra->key, "Session") == 0)
        sess = 1;
      htsbuf_qprintf(&hdrs, "%s: %s\r\n", ra->key, ra->val ?: "");
    }
  }
  if(hc->hc_session && !sess)
    htsbuf_qprintf(&hdrs, "Session: %s\r\n", hc->hc_session);

  htsbuf_append_str(&hdrs, "\r\n");

  tcp_write_queue(hc->hc_fd, &hdrs);
}

/*
 * Transmit a websocket upgrade reply
 */
int
http_send_header_websocket(http_connection_t *hc, const char *protocol)
{
  htsbuf_queue_t hdrs;
  const int rc = HTTP_STATUS_PSWITCH;
  uint8_t hash[20];
  const char *s;
  const char *guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  char encoded[512];

  s = http_arg_get(&hc->hc_args, "sec-websocket-key");
  if (s == NULL || strlen(s) < 10) {
    http_error(hc, HTTP_STATUS_UNSUPPORTED);
    return -1;
  }

  htsbuf_queue_init(&hdrs, 0);

  htsbuf_qprintf(&hdrs, "%s %d %s\r\n",
		 http_ver2str(hc->hc_version), rc, http_rc2str(rc));

  sha1_calc(hash, (uint8_t *)s, strlen(s), (uint8_t *)guid, strlen(guid));
  base64_encode(encoded, sizeof(encoded), hash, sizeof(hash));

  htsbuf_qprintf(&hdrs, "Upgrade: websocket\r\n"
                        "Connection: Upgrade\r\n"
                        "Sec-WebSocket-Accept: %s\r\n"
                        "Sec-WebSocket-Protocol: %s\r\n"
                        "\r\n",
                        encoded, protocol);

  http_send_begin(hc);
  tcp_write_queue(hc->hc_fd, &hdrs);
  http_send_end(hc);
  return 0;
}

/*
 *
 */
int
http_encoding_valid(http_connection_t *hc, const char *encoding)
{
  const char *accept;
  char *tokbuf, *tok, *saveptr = NULL, *q, *s;

  accept = http_arg_get(&hc->hc_args, "accept-encoding");
  if (!accept)
    return 0;

  tokbuf = tvh_strdupa(accept);
  tok = strtok_r(tokbuf, ",", &saveptr);
  while (tok) {
    while (*tok == ' ')
      tok++;
    // check for semicolon
    if ((q = strchr(tok, ';')) != NULL) {
      *q = '\0';
      q++;
      while (*q == ' ')
        q++;
    }
    if ((s = strchr(tok, ' ')) != NULL)
      *s = '\0';
    // check for matching encoding with q > 0
    if ((!strcasecmp(tok, encoding) || !strcmp(tok, "*")) && (q == NULL || strncmp(q, "q=0.000", strlen(q))))
      return 1;
    tok = strtok_r(NULL, ",", &saveptr);
  }
  return 0;
}

/**
 *
 */
int
http_header_match(http_connection_t *hc, const char *name, const char *value)
{
  char *tokbuf, *tok, *saveptr = NULL, *q, *s;

  s = http_arg_get(&hc->hc_args, name);
  if (s == NULL)
    return 0;
  tokbuf = tvh_strdupa(s);
  tok = strtok_r(tokbuf, ",", &saveptr);
  while (tok) {
    while (*tok == ' ')
      tok++;
    s = tok;
    if (*s == '"') {
      q = strchr(++s, '"');
      if (q)
        *q = '\0';
    } else {
      q = strchr(s, ' ');
      if (q)
        *q = '\0';
    }
    if (strcasecmp(s, value) == 0)
      return 1;
    tok = strtok_r(NULL, ",", &saveptr);
  }
  return 0;
}

/**
 *
 */
static char *
http_get_header_value(const char *hdr, const char *name)
{
  char *s, *start, *val;
  s = tvh_strdupa(hdr);
  while (*s) {
    while (*s && *s <= ' ') s++;
    start = s;
    while (*s && *s != '=') s++;
    if (*s == '=') {
      *s = '\0';
      s++;
    }
    while (*s && *s <= ' ') s++;
    if (*s == '"') {
      val = ++s;
      while (*s && *s != '"') s++;
      if (*s == '"') {
        *s = '\0';
        s++;
      }
      while (*s && (*s <= ' ' || *s == ',')) s++;
    } else {
      val = s;
      while (*s && *s != ',') s++;
      *s = '\0';
      s++;
    }
    if (*start && strcmp(name, start) == 0)
      return strdup(val);
  }
  return NULL;
}

/**
 *
 */
int
http_check_local_ip( http_connection_t *hc )
{
  if (hc->hc_local_ip == NULL) {
    hc->hc_local_ip = malloc(sizeof(*hc->hc_local_ip));
    *hc->hc_local_ip = *hc->hc_self;
    hc->hc_is_local_ip = ip_check_is_local_address(hc->hc_peer, hc->hc_self, hc->hc_local_ip) > 0;
  }
  return hc->hc_is_local_ip;
}

/**
 * Transmit a HTTP reply
 */
static void
http_send_reply(http_connection_t *hc, int rc, const char *content, 
		const char *encoding, const char *location, int maxage)
{
  size_t size = hc->hc_reply.hq_size;
  uint8_t *data = NULL;

#if ENABLE_ZLIB
  if (http_encoding_valid(hc, "gzip") && encoding == NULL && size > 256) {
    uint8_t *data2 = (uint8_t *)htsbuf_to_string(&hc->hc_reply);
    data = tvh_gzip_deflate(data2, size, &size);
    free(data2);
    encoding = "gzip";
  }
#endif

  http_send_begin(hc);
  http_send_header(hc, rc, content, size,
		   encoding, location, maxage, 0, NULL, NULL);
  
  if(!hc->hc_no_output) {
    if (data == NULL)
      tcp_write_queue(hc->hc_fd, &hc->hc_reply);
    else
      tvh_write(hc->hc_fd, data, size);
  }
  http_send_end(hc);

  free(data);
}


/**
 * Send HTTP error back
 */
void
http_error(http_connection_t *hc, int error)
{
  const char *errtxt = http_rc2str(error);
  const char *lang;
  int level;

  if (!atomic_get(&http_server_running)) return;

  if (error != HTTP_STATUS_FOUND && error != HTTP_STATUS_MOVED) {
    level = LOG_INFO;
    if (error == HTTP_STATUS_UNAUTHORIZED)
      level = LOG_DEBUG;
    else if (error == HTTP_STATUS_BAD_REQUEST || error > HTTP_STATUS_UNAUTHORIZED)
      level = LOG_ERR;
    tvhlog(level, hc->hc_subsys, "%s: %s %s (%d) %s -- %d",
	   hc->hc_peer_ipstr, http_ver2str(hc->hc_version),
           http_cmd2str(hc->hc_cmd), hc->hc_cmd, hc->hc_url, error);
  }

  if (hc->hc_version != RTSP_VERSION_1_0) {
    htsbuf_queue_flush(&hc->hc_reply);

    htsbuf_qprintf(&hc->hc_reply,
                   "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
                   "<HTML><HEAD>\r\n"
                   "<TITLE>%d %s</TITLE>\r\n"
                   "</HEAD><BODY>\r\n"
                   "<H1>%d %s</H1>\r\n",
                   error, errtxt, error, errtxt);

    if (error == HTTP_STATUS_UNAUTHORIZED) {
      lang = hc->hc_access ? hc->hc_access->aa_lang_ui : NULL;
      htsbuf_qprintf(&hc->hc_reply, "<P STYLE=\"text-align: center; margin: 2em\"><A HREF=\"%s/\" STYLE=\"border: 1px solid; border-radius: 4px; padding: .6em\">%s</A></P>",
                     tvheadend_webroot ? tvheadend_webroot : "",
                     tvh_gettext_lang(lang, N_("Default login")));
      htsbuf_qprintf(&hc->hc_reply, "<P STYLE=\"text-align: center; margin: 2em\"><A HREF=\"%s/login\" STYLE=\"border: 1px solid; border-radius: 4px; padding: .6em\">%s</A></P>",
                     tvheadend_webroot ? tvheadend_webroot : "",
                     tvh_gettext_lang(lang, N_("New login")));
    }

    htsbuf_append_str(&hc->hc_reply, "</BODY></HTML>\r\n");

    http_send_reply(hc, error, "text/html", NULL, NULL, 0);
  } else {
    http_send_reply(hc, error, NULL, NULL, NULL, 0);
  }
}


/**
 * Send an HTTP OK, simple version for text/html
 */
void
http_output_html(http_connection_t *hc)
{
  return http_send_reply(hc, HTTP_STATUS_OK, "text/html; charset=UTF-8",
			 NULL, NULL, 0);
}

/**
 * Send an HTTP OK, simple version for text/html
 */
void
http_output_content(http_connection_t *hc, const char *content)
{
  return http_send_reply(hc, HTTP_STATUS_OK, content, NULL, NULL, 0);
}



/**
 * Send an HTTP REDIRECT
 */
void
http_redirect(http_connection_t *hc, const char *location,
              http_arg_list_t *req_args, int external)
{
  const char *loc = location;
  htsbuf_queue_flush(&hc->hc_reply);

  if (req_args) {
    http_arg_t *ra;
    htsbuf_queue_t hq;
    int first = 1;
    htsbuf_queue_init(&hq, 0);
    if (!external && tvheadend_webroot && location[0] == '/')
      htsbuf_append_str(&hq, tvheadend_webroot);
    htsbuf_append_str(&hq, location);
    TAILQ_FOREACH(ra, req_args, link) {
      if (!first)
        htsbuf_append(&hq, "&", 1);
      else
        htsbuf_append(&hq, "?", 1);
      first = 0;
      htsbuf_append_and_escape_url(&hq, ra->key);
      if (ra->val) {
        htsbuf_append(&hq, "=", 1);
        htsbuf_append_and_escape_url(&hq, ra->val);
      }
    }
    loc = htsbuf_to_string(&hq);
    htsbuf_queue_flush(&hq);
  } else if (!external && tvheadend_webroot && location[0] == '/') {
    loc = malloc(strlen(location) + strlen(tvheadend_webroot) + 1);
    strcpy((char *)loc, tvheadend_webroot);
    strcat((char *)loc, location);
  }

  htsbuf_qprintf(&hc->hc_reply,
		 "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
		 "<HTML><HEAD>\r\n"
		 "<TITLE>Redirect</TITLE>\r\n"
		 "</HEAD><BODY>\r\n"
		 "Please follow <a href=\"%s\">%s</a>\r\n"
		 "</BODY></HTML>\r\n",
		 loc, loc);

  http_send_reply(hc, HTTP_STATUS_FOUND, "text/html", NULL, loc, 0);

  if (loc != location)
    free((char *)loc);
}


/**
 * Send an CSS @import
 */
void
http_css_import(http_connection_t *hc, const char *location)
{
  const char *loc = location;

  htsbuf_queue_flush(&hc->hc_reply);

  if (tvheadend_webroot && location[0] == '/') {
    loc = alloca(strlen(location) + strlen(tvheadend_webroot) + 1);
    strcpy((char *)loc, tvheadend_webroot);
    strcat((char *)loc, location);
  }

  htsbuf_qprintf(&hc->hc_reply, "@import url('%s');\r\n", loc);

  http_send_reply(hc, HTTP_STATUS_OK, "text/css", NULL, loc, 0);
}

/**
 *
 */
void
http_extra_destroy(http_connection_t *hc)
{
  htsbuf_data_t *hd, *hd_next;

  tvh_mutex_lock(&hc->hc_extra_lock);
  for (hd = TAILQ_FIRST(&hc->hc_extra.hq_q); hd; hd = hd_next) {
    hd_next = TAILQ_NEXT(hd, hd_link);
    if (hd->hd_data_off <= 0) {
      htsbuf_data_free(&hc->hc_extra, hd);
      atomic_dec(&hc->hc_extra_chunks, 1);
    }
  }
  tvh_mutex_unlock(&hc->hc_extra_lock);
}

/**
 *
 */
int
http_extra_flush(http_connection_t *hc)
{
  htsbuf_data_t *hd;
  int r, serr = 0;

  if (atomic_add(&hc->hc_extra_insend, 1) != 0)
    goto fin;

  while (1) {
    r = -1;
    serr = 0;
    tvh_mutex_lock(&hc->hc_extra_lock);
    if (atomic_add(&hc->hc_extra_insend, 0) != 1)
      goto unlock;
    hd = TAILQ_FIRST(&hc->hc_extra.hq_q);
    if (hd == NULL)
      goto unlock;
    do {
      r = send(hc->hc_fd, hd->hd_data + hd->hd_data_off,
               hd->hd_data_size - hd->hd_data_off,
               MSG_DONTWAIT | (TAILQ_NEXT(hd, hd_link) ? MSG_MORE : 0));
      serr = errno;
    } while (r < 0 && serr == EINTR);
    if (r > 0 && r + hd->hd_data_off >= hd->hd_data_size) {
      atomic_dec(&hc->hc_extra_chunks, 1);
      htsbuf_data_free(&hc->hc_extra, hd);
    } else if (r > 0) {
      hd->hd_data_off += r;
      hc->hc_extra.hq_size -= r;
    }
unlock:
    tvh_mutex_unlock(&hc->hc_extra_lock);

    if (r < 0) {
      if (ERRNO_AGAIN(serr))
        serr = 0;
      break;
    }
  }

fin:
  atomic_dec(&hc->hc_extra_insend, 1);
  return serr;
}

/**
 *
 */
int
http_extra_flush_partial(http_connection_t *hc)
{
  htsbuf_data_t *hd;
  int r = 0;
  unsigned int off, size;
  void *data = NULL;

  atomic_add(&hc->hc_extra_insend, 1);

  tvh_mutex_lock(&hc->hc_extra_lock);
  hd = TAILQ_FIRST(&hc->hc_extra.hq_q);
  if (hd && hd->hd_data_off > 0) {
    data = hd->hd_data;
    hd->hd_data = NULL;
    off = hd->hd_data_off;
    size = hd->hd_data_size;
    atomic_dec(&hc->hc_extra_chunks, 1);
    htsbuf_data_free(&hc->hc_extra, hd);
  }
  tvh_mutex_unlock(&hc->hc_extra_lock);
  if (data) {
    r = tvh_write(hc->hc_fd, data + off, size - off);
    free(data);
  }
  atomic_dec(&hc->hc_extra_insend, 1);
  return r;
}

/**
 *
 */
int
http_extra_send(http_connection_t *hc, const void *data,
                size_t data_len, int may_discard)
{
  uint8_t *b = malloc(data_len);
  memcpy(b, data, data_len);
  return http_extra_send_prealloc(hc, b, data_len, may_discard);
}

/**
 *
 */
int
http_extra_send_prealloc(http_connection_t *hc, const void *data,
                         size_t data_len, int may_discard)
{
  if (data == NULL) return 0;
  tvh_mutex_lock(&hc->hc_extra_lock);
  if (!may_discard || hc->hc_extra.hq_size <= 1024*1024) {
    atomic_add(&hc->hc_extra_chunks, 1);
    htsbuf_append_prealloc(&hc->hc_extra, data, data_len);
  } else {
    free((void *)data);
  }
  tvh_mutex_unlock(&hc->hc_extra_lock);
  return http_extra_flush(hc);
}

/**
 *
 */
char *
http_get_hostpath(http_connection_t *hc, char *buf, size_t buflen)
{
  const char *host, *proto;

  host  = http_arg_get(&hc->hc_args, "Host") ?:
          http_arg_get(&hc->hc_args, "X-Forwarded-Host");
  proto = http_arg_get(&hc->hc_args, "X-Forwarded-Proto");

  snprintf(buf, buflen, "%s://%s%s",
           proto ?: "http", host ?: "localhost", tvheadend_webroot ?: "");

  return buf;
}

/**
 *
 */
static void
http_access_verify_ticket(http_connection_t *hc)
{
  const char *ticket_id;

  ticket_id = http_arg_get(&hc->hc_req_args, "ticket");
  hc->hc_access = access_ticket_verify2(ticket_id, hc->hc_url);
  if (hc->hc_access == NULL)
    return;
  hc->hc_auth_type = HC_AUTH_TICKET;
  tvhinfo(hc->hc_subsys, "%s: using ticket %s for %s",
	  hc->hc_peer_ipstr, ticket_id, hc->hc_url);
}

/**
 *
 */
static void
http_access_verify_auth(http_connection_t *hc)
{
  const char *auth_id;

  auth_id = http_arg_get(&hc->hc_req_args, "auth");
  hc->hc_access = access_get_by_auth(hc->hc_peer, auth_id);
  if (hc->hc_access == NULL)
    return;
  hc->hc_auth_type = HC_AUTH_PERM;
  tvhinfo(hc->hc_subsys, "%s: using auth %s for %s",
	  hc->hc_peer_ipstr, auth_id, hc->hc_url);
}

/**
 *
 */
struct http_verify_structure {
  char *password;
  char *d_ha1;
  char *d_all;
  char *d_response;
  int algo;
  http_connection_t *hc;
};

static int
http_verify_callback(void *aux, const char *passwd)
{
   struct http_verify_structure *v = aux;
   char ha1[512];
   char all[1024];
   char *m;
   int res;

   if (v->d_ha1) {
     snprintf(ha1, sizeof(ha1), "%s:%s", v->d_ha1, passwd);
     m = http_get_digest_hash(v->algo, ha1);
     snprintf(all, sizeof(all), "%s:%s", m, v->d_all);
     free(m);
     m = http_get_digest_hash(v->algo, all);
     res = strcmp(m, v->d_response) == 0;
     free(m);
     return res;
   }
   if (v->password == NULL) return 0;
   return strcmp(v->password, passwd) == 0;
}

/**
 *
 */
static int
http_verify_prepare(http_connection_t *hc, struct http_verify_structure *v)
{
  memset(v, 0, sizeof(*v));
  if (hc->hc_authhdr) {

    if (hc->hc_nonce == NULL)
      return -1;

    const char *method = http_cmd2str(hc->hc_cmd);
    char *response = http_get_header_value(hc->hc_authhdr, "response");
    char *qop = http_get_header_value(hc->hc_authhdr, "qop");
    char *uri = http_get_header_value(hc->hc_authhdr, "uri");
    char *algo1 = http_get_header_value(hc->hc_authhdr, "algorithm");
    char *realm = NULL, *nonce_count = NULL, *cnonce = NULL, *m = NULL;
    char all[1024];
    int res = -1;

    if (algo1 == NULL || strcasecmp(algo1, "MD5") == 0) {
      v->algo = HTTP_AUTH_ALGO_MD5;
    } else if (strcasecmp(algo1, "SHA-256") == 0) {
      v->algo = HTTP_AUTH_ALGO_SHA256;
    } else if (strcasecmp(algo1, "SHA-512-256") == 0) {
      v->algo = HTTP_AUTH_ALGO_SHA512_256;
    } else {
      goto end;
    }

    if (qop == NULL || uri == NULL)
      goto end;
     
    if (strcasecmp(qop, "auth-int") == 0) {
      m = http_get_digest_hash(v->algo, hc->hc_post_data ?: "");
      snprintf(all, sizeof(all), "%s:%s:%s", method, uri, m);
      free(m);
      m = NULL;
    } else if (strcasecmp(qop, "auth") == 0) {
      snprintf(all, sizeof(all), "%s:%s", method, uri);
    } else {
      goto end;
    }

    m = http_get_digest_hash(v->algo, all);
    if (tvh_str_default(qop, NULL) == NULL) {
      snprintf(all, sizeof(all), "%s:%s", hc->hc_nonce, m);
      goto set;
    } else {
      realm = http_get_header_value(hc->hc_authhdr, "realm");
      nonce_count = http_get_header_value(hc->hc_authhdr, "nc");
      cnonce = http_get_header_value(hc->hc_authhdr, "cnonce");
      if (realm == NULL || strcmp(realm, config.realm) ||
          nonce_count == NULL || cnonce == NULL) {
        goto end;
      } else {
        snprintf(all, sizeof(all), "%s:%s:%s:%s:%s",
                 hc->hc_nonce, nonce_count, cnonce, qop, m);
set:
        v->d_all = strdup(all);
        snprintf(all, sizeof(all), "%s:%s", hc->hc_username, realm);
        v->d_ha1 = strdup(all);
        v->d_response = response;
        v->hc = hc;
        response = NULL;
      }
    }
    res = 0;
end:
    free(response);
    free(qop);
    free(uri);
    free(realm);
    free(nonce_count);
    free(cnonce);
    free(m);
    return res;
  } else {
    v->password = hc->hc_password;
  }
  return 0;
}

/*
 *
 */
static void
http_verify_free(struct http_verify_structure *v)
{
  free(v->d_ha1);
  free(v->d_all);
  free(v->d_response);
}

/**
 * Return non-zero if no access
 */
int
http_access_verify(http_connection_t *hc, int mask)
{
  struct http_verify_structure v;
  int r;

  /* quick path */
  if (hc->hc_access)
    return access_verify2(hc->hc_access, mask);

  /* tickets */
  http_access_verify_ticket(hc);
  if (hc->hc_access && !access_verify2(hc->hc_access, mask))
    return 0;

  http_access_verify_auth(hc);
  if (hc->hc_access && !access_verify2(hc->hc_access, mask))
    return 0;

  access_destroy(hc->hc_access);
  if (http_verify_prepare(hc, &v)) {
    hc->hc_access = NULL;
    return -1;
  }
  hc->hc_access = access_get(hc->hc_peer, hc->hc_username,
                             http_verify_callback, &v);
  http_verify_free(&v);
  if (hc->hc_access) {
    r = access_verify2(hc->hc_access, mask);
    if (r == 0) {
      if (!strempty(hc->hc_username))
        hc->hc_auth_type = hc->hc_authhdr ? HC_AUTH_DIGEST : HC_AUTH_PLAIN;
      else
        hc->hc_auth_type = HC_AUTH_ADDR;
    }
    return r;
  }

  return -1;
}

/**
 * Return non-zero if no access
 */
int
http_access_verify_channel(http_connection_t *hc, int mask,
                           struct channel *ch)
{
  if (http_access_verify(hc, mask))
    return -1;

  return channel_access(ch, hc->hc_access, 0) ? 0 : -1;
}

/**
 *
 */
const char *
http_username(http_connection_t *hc)
{
  if (strempty(hc->hc_username) && hc->hc_access)
    return hc->hc_access->aa_username;
  return hc->hc_username;
}

/**
 *
 */
int
http_noaccess_code(http_connection_t *hc)
{
  return strempty(hc->hc_username) ?
              HTTP_STATUS_UNAUTHORIZED : HTTP_STATUS_FORBIDDEN;
}

/**
 *
 */
static int
http_websocket_valid(http_connection_t *hc)
{
  if (!http_header_match(hc, "connection", "upgrade") ||
      http_arg_get(&hc->hc_args, "sec-websocket-key") == NULL)
    return HTTP_STATUS_UNSUPPORTED;
  return 0;
}

/**
 * Execute url callback
 *
 * Returns 1 if we should disconnect
 * 
 */
static int
http_exec(http_connection_t *hc, http_path_t *hp, char *remain)
{
  int err;

  /* this is a special case when client probably requires authentication */
  if ((hp->hp_accessmask & ACCESS_NO_EMPTY_ARGS) != 0 && TAILQ_EMPTY(&hc->hc_req_args)) {
    err = http_noaccess_code(hc);
    goto destroy;
  }
  if (http_access_verify(hc, hp->hp_accessmask & ~ACCESS_INTERNAL)) {
    if ((hp->hp_flags & HTTP_PATH_NO_VERIFICATION) == 0) {
      err = http_noaccess_code(hc);
      goto destroy;
    }
  }
  if (hp->hp_flags & HTTP_PATH_WEBSOCKET) {
    err = http_websocket_valid(hc);
    if (err)
      goto destroy;
  }
  err = hp->hp_callback(hc, remain, hp->hp_opaque);
destroy:
  access_destroy(hc->hc_access);
  hc->hc_access = NULL;

  if(err == -1)
     return 1;

  if(err)
    http_error(hc, err);
  return 0;
}

/*
 * Dump request
 */
static void
dump_request(http_connection_t *hc)
{
  char buf[2048] = "";
  http_arg_t *ra;
  int first, ptr = 0;

  first = 1;
  TAILQ_FOREACH(ra, &hc->hc_req_args, link) {
    tvh_strlcatf(buf, sizeof(buf), ptr, first ? "?%s=%s" : "&%s=%s", ra->key, ra->val);
    first = 0;
  }

  first = 1;
  TAILQ_FOREACH(ra, &hc->hc_args, link) {
    tvh_strlcatf(buf, sizeof(buf), ptr, first ? "{{%s=%s" : ",%s=%s", ra->key, ra->val);
    first = 0;
  }
  if (!first)
    tvh_strlcatf(buf, sizeof(buf), ptr, "}}");

  tvhtrace(hc->hc_subsys, "%s %s %s%s", http_ver2str(hc->hc_version),
           http_cmd2str(hc->hc_cmd), hc->hc_url, buf);
}

/**
 * HTTP GET
 */
static int
http_cmd_options(http_connection_t *hc)
{
  http_send_begin(hc);
  http_send_header(hc, HTTP_STATUS_OK, NULL, INT64_MIN,
		   NULL, NULL, -1, 0, NULL, NULL);
  http_send_end(hc);
  return 0;
}

/**
 * HTTP GET
 */
static int
http_cmd_get(http_connection_t *hc)
{
  http_path_t hp;
  char *remain;
  char *args;

  if (tvhtrace_enabled())
    dump_request(hc);

  if (!http_resolve(hc, &hp, &remain, &args)) {
    http_error(hc, HTTP_STATUS_NOT_FOUND);
    return 0;
  }

  if(args != NULL)
    http_parse_args(&hc->hc_req_args, args);

  return http_exec(hc, &hp, remain);
}





/**
 * Initial processing of HTTP POST
 *
 * Return non-zero if we should disconnect
 */
static int
http_cmd_post(http_connection_t *hc, htsbuf_queue_t *spill)
{
  http_path_t hp;
  char *remain, *args, *v;

  /* Set keep-alive status */
  v = http_arg_get(&hc->hc_args, "Content-Length");
  if(v == NULL) {
    /* No content length in POST, make us disconnect */
    return -1;
  }

  hc->hc_post_len = atoi(v);
  if(hc->hc_post_len > 16 * 1024 * 1024) {
    /* Bail out if POST data > 16 Mb */
    hc->hc_keep_alive = 0;
    return -1;
  }

  /* Allocate space for data, we add a terminating null char to ease
     string processing on the content */

  hc->hc_post_data = malloc(hc->hc_post_len + 1);
  hc->hc_post_data[hc->hc_post_len] = 0;

  if(tcp_read_data(hc->hc_fd, hc->hc_post_data, hc->hc_post_len, spill) < 0)
    return -1;

  /* Parse content-type */
  v = http_arg_get(&hc->hc_args, "Content-Type");
  if(v != NULL) {
    char  *argv[2];
    int n = http_tokenize(v, argv, 2, ';');
    if(n == 0) {
      http_error(hc, HTTP_STATUS_BAD_REQUEST);
      return 0;
    }

    if(!strcmp(argv[0], "application/x-www-form-urlencoded"))
      http_parse_args(&hc->hc_req_args, hc->hc_post_data);
  }

  if (tvhtrace_enabled())
    dump_request(hc);

  if (!http_resolve(hc, &hp, &remain, &args)) {
    http_error(hc, HTTP_STATUS_NOT_FOUND);
    return 0;
  }
  return http_exec(hc, &hp, remain);
}


/**
 * Process a HTTP request
 */
static int
http_process_request(http_connection_t *hc, htsbuf_queue_t *spill)
{
  switch(hc->hc_cmd) {
  default:
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    return 0;
  case HTTP_CMD_OPTIONS:
    return http_cmd_options(hc);
  case HTTP_CMD_GET:
    return http_cmd_get(hc);
  case HTTP_CMD_HEAD:
    hc->hc_no_output = 1;
    return http_cmd_get(hc);
  case HTTP_CMD_POST:
    return http_cmd_post(hc, spill);
  }
}

/**
 * Process a request, extract info from headers, dispatch command and
 * clean up
 */
static int
process_request(http_connection_t *hc, htsbuf_queue_t *spill)
{
  char *v, *argv[2];
  int n, rval = -1;
  char authbuf[150];

  hc->hc_url_orig = tvh_strdupa(hc->hc_url);

  v = (config.proxy) ? http_arg_get(&hc->hc_args, "X-Forwarded-For") : NULL;
  if (v) {
    if (hc->hc_proxy_ip == NULL)
      hc->hc_proxy_ip = malloc(sizeof(*hc->hc_proxy_ip));
    *hc->hc_proxy_ip = *hc->hc_peer;
    if (tcp_get_ip_from_str(v, hc->hc_peer) == NULL) {
      http_error(hc, HTTP_STATUS_BAD_REQUEST);
      free(hc->hc_proxy_ip);
      hc->hc_proxy_ip = NULL;
      return -1;
    }
  }

  tcp_get_str_from_ip(hc->hc_peer, authbuf, sizeof(authbuf));

  hc->hc_peer_ipstr = tvh_strdupa(authbuf);
  hc->hc_representative = hc->hc_peer_ipstr;
  hc->hc_username = NULL;
  hc->hc_password = NULL;
  hc->hc_authhdr  = NULL;
  hc->hc_session  = NULL;

  /* Set keep-alive status */
  v = http_arg_get(&hc->hc_args, "connection");

  switch(hc->hc_version) {
  case RTSP_VERSION_1_0:
    hc->hc_keep_alive = 1;
    /* Extract CSeq */
    if((v = http_arg_get(&hc->hc_args, "CSeq")) != NULL) {
      hc->hc_cseq = strtoll(v, NULL, 10);
    } else {
      hc->hc_cseq = 0;
    }
    free(hc->hc_session);
    if ((v = http_arg_get(&hc->hc_args, "Session")) != NULL)
      hc->hc_session = tvh_strdupa(v);
    else
      hc->hc_session = NULL;
    break;

  case HTTP_VERSION_1_0:
    /* Keep-alive is default off, but can be enabled */
    hc->hc_keep_alive = v != NULL && !strcasecmp(v, "keep-alive");
    break;
    
  case HTTP_VERSION_1_1:
    /* Keep-alive is default on, but can be disabled */
    hc->hc_keep_alive = !(v != NULL && !strcasecmp(v, "close"));
    break;
  }

  /* Extract authorization */
  if((v = http_arg_get(&hc->hc_args, "Authorization")) != NULL) {
    if((n = http_tokenize(v, argv, 2, -1)) == 2) {
      if (strcasecmp(argv[0], "basic") == 0) {
        if (config.http_auth == HTTP_AUTH_PLAIN ||
            config.http_auth == HTTP_AUTH_PLAIN_DIGEST) {
          n = base64_decode((uint8_t *)authbuf, argv[1], sizeof(authbuf) - 1);
          if (n < 0)
            n = 0;
          authbuf[n] = 0;
          if((n = http_tokenize(authbuf, argv, 2, ':')) == 2) {
            hc->hc_username = tvh_strdupa(argv[0]);
            hc->hc_password = tvh_strdupa(argv[1]);
            http_deescape(hc->hc_username);
            http_deescape(hc->hc_password);
            // No way to actually track this
          } else {
            http_error(hc, HTTP_STATUS_UNAUTHORIZED);
            return -1;
          }
        } else {
          http_error(hc, HTTP_STATUS_UNAUTHORIZED);
          return -1;
        }
      } else if (strcasecmp(argv[0], "digest") == 0) {
        if (config.http_auth == HTTP_AUTH_DIGEST ||
            config.http_auth == HTTP_AUTH_PLAIN_DIGEST) {
          v = http_get_header_value(argv[1], "nonce");
          if (v == NULL || !http_nonce_exists(v)) {
            free(v);
            http_error(hc, HTTP_STATUS_UNAUTHORIZED);
            return -1;
          }
          free(hc->hc_nonce);
          hc->hc_nonce = v;
          v = http_get_header_value(argv[1], "username");
          hc->hc_authhdr  = tvh_strdupa(argv[1]);
          hc->hc_username = tvh_strdupa(v);
          http_deescape(hc->hc_username);
          free(v);
        } else {
          http_error(hc, HTTP_STATUS_UNAUTHORIZED);
          return -1;
        }
      } else {
        http_error(hc, HTTP_STATUS_BAD_REQUEST);
        return -1;
      }
    }
  }

  if (hc->hc_username)
    hc->hc_representative = hc->hc_username;

  switch(hc->hc_version) {
  case RTSP_VERSION_1_0:
    if (tvhtrace_enabled())
      dump_request(hc);
    rval = hc->hc_process(hc, spill);
    break;

  case HTTP_VERSION_1_0:
  case HTTP_VERSION_1_1:
    if (!hc->hc_cseq)
      rval = hc->hc_process(hc, spill);
    else
      http_error(hc, HTTP_STATUS_HTTP_VERSION);
    break;
  }
  return rval;
}



/*
 * Delete one argument
 */
void
http_arg_remove(struct http_arg_list *list, struct http_arg *arg)
{
  TAILQ_REMOVE(list, arg, link);
  free(arg->key);
  free(arg->val);
  free(arg);
}


/*
 * Delete all arguments associated with a connection
 */
void
http_arg_flush(http_arg_list_t *list)
{
  http_arg_t *ra;
  while((ra = TAILQ_FIRST(list)) != NULL)
    http_arg_remove(list, ra);
}


/**
 * Find an argument associated with a connection
 */
char *
http_arg_get(http_arg_list_t *list, const char *name)
{
  http_arg_t *ra;
  TAILQ_FOREACH(ra, list, link)
    if(!strcasecmp(ra->key, name))
      return ra->val;
  return NULL;
}

/**
 * Find an argument associated with a connection and remove it
 */
char *
http_arg_get_remove(struct http_arg_list *list, const char *name)
{
  static __thread char buf[128];
  int empty;
  http_arg_t *ra;
  TAILQ_FOREACH(ra, list, link)
    if(!strcasecmp(ra->key, name)) {
      TAILQ_REMOVE(list, ra, link);
      empty = ra->val == NULL;
      if (!empty)
        strlcpy(buf, ra->val, sizeof(buf));
      free(ra->key);
      free(ra->val);
      free(ra);
      return !empty ? buf : NULL;
    }
  buf[0] = '\0';
  return buf;
}


/**
 * Set an argument associated with a connection
 */
void
http_arg_set(struct http_arg_list *list, const char *key, const char *val)
{
  http_arg_t *ra;

  ra = malloc(sizeof(http_arg_t));
  TAILQ_INSERT_TAIL(list, ra, link);
  ra->key = strdup(key);
  ra->val = val ? strdup(val) : NULL;
}

/*
 *
 */
char *
http_arg_get_query(http_arg_list_t *args)
{
  htsbuf_queue_t q;
  http_arg_t *ra;
  char *r;

  if (http_args_empty(args))
    return NULL;
  htsbuf_queue_init(&q, 0);
  htsbuf_queue_init(&q, 0);
  TAILQ_FOREACH(ra, args, link) {
    if (!htsbuf_empty(&q))
      htsbuf_append(&q, "&", 1);
    htsbuf_append_and_escape_url(&q, ra->key);
    if (ra->val) {
      htsbuf_append(&q, "=", 1);
      htsbuf_append_and_escape_url(&q, ra->val);
    }
  }
  r = htsbuf_to_string(&q);
  htsbuf_queue_flush(&q);
  return r;
}

/*
 * Split a string in components delimited by 'delimiter'
 */
int
http_tokenize(char *buf, char **vec, int vecsize, int delimiter)
{
  int n = 0;

  while(1) {
    while((*buf > 0 && *buf < 33) || *buf == delimiter)
      buf++;
    if(*buf == 0)
      break;
    vec[n++] = buf;
    if(n == vecsize)
      break;
    while(*buf > 32 && *buf != delimiter)
      buf++;
    if(*buf == 0)
      break;
    *buf = 0;
    buf++;
  }
  return n;
}


/**
 * Add a callback for a given "virtual path" on our HTTP server
 */
http_path_t *
http_path_add_modify(const char *path, void *opaque, http_callback_t *callback,
                     uint32_t accessmask, http_path_modify_t *path_modify)
{
  http_path_t *hp = malloc(sizeof(http_path_t));
  char *tmp;

  if (tvheadend_webroot) {
    size_t len = strlen(tvheadend_webroot) + strlen(path) + 1;
    hp->hp_path     = tmp = malloc(len);
    sprintf(tmp, "%s%s", tvheadend_webroot, path);
  } else
    hp->hp_path     = strdup(path);
  hp->hp_len      = strlen(hp->hp_path);
  hp->hp_opaque   = opaque;
  hp->hp_callback = callback;
  hp->hp_accessmask = accessmask;
  hp->hp_path_modify = path_modify;
  hp->hp_flags    = 0;
  tvh_mutex_lock(&http_paths_mutex);
  LIST_INSERT_HEAD(&http_paths, hp, hp_link);
  tvh_mutex_unlock(&http_paths_mutex);
  return hp;
}

/**
 * Add a callback for a given "virtual path" on our HTTP server
 */
http_path_t *
http_path_add(const char *path, void *opaque, http_callback_t *callback,
              uint32_t accessmask)
{
  return http_path_add_modify(path, opaque, callback, accessmask, NULL);
}

/**
 *
 */
int
http_websocket_send(http_connection_t *hc, uint8_t *buf, uint64_t buflen,
                    int opcode)
{
  int op, r = 0;
  uint8_t b[10];
  uint64_t bsize;

  switch (opcode) {
  case HTTP_WSOP_TEXT:   op = 0x01; break;
  case HTTP_WSOP_BINARY: op = 0x02; break;
  case HTTP_WSOP_PING:   op = 0x09; break;
  case HTTP_WSOP_PONG:   op = 0x0a; break;
  default: return -1;
  }

  b[0] = 0x80 | op; /* FIN + opcode */
  if (buflen <= 125) {
    b[1] = buflen;
    bsize = 2;
  } else if (buflen <= 65535) {
    b[1] = 126;
    b[2] = (buflen >> 8) & 0xff;
    b[3] = buflen & 0xff;
    bsize = 4;
  } else {
    b[1] = 127;
    b[2] = (buflen >> 56) & 0xff;
    b[3] = (buflen >> 48) & 0xff;
    b[4] = (buflen >> 40) & 0xff;
    b[5] = (buflen >> 32) & 0xff;
    b[6] = (buflen >> 24) & 0xff;
    b[7] = (buflen >> 16) & 0xff;
    b[8] = (buflen >>  8) & 0xff;
    b[9] = (buflen >>  0) & 0xff;
    bsize = 10;
  }
  http_send_begin(hc);
  if (tvh_write(hc->hc_fd, b, bsize))
    r = -1;
  if (r == 0 && tvh_write(hc->hc_fd, buf, buflen))
    r = -1;
  http_send_end(hc);
  return r;
}

/**
 *
 */
int
http_websocket_send_json(http_connection_t *hc, htsmsg_t *msg)
{
  htsbuf_queue_t q;
  char *s;
  int r;

  htsbuf_queue_init(&q, 0);
  htsmsg_json_serialize(msg, &q, 0);
  s = htsbuf_to_string(&q);
  htsbuf_queue_flush(&q);
  r = http_websocket_send(hc, (uint8_t *)s, strlen(s), HTTP_WSOP_TEXT);
  free(s);
  return r;
}

/**
 *
 */
int
http_websocket_read(http_connection_t *hc, htsmsg_t **_res, int timeout)
{
  htsmsg_t *msg, *msg1;
  uint8_t b[2], bl[8], *p;
  int op, r;
  uint64_t plen, pi;

  *_res = NULL;
again:
  r = tcp_read_timeout(hc->hc_fd, b, 2, timeout);
  if (r == ETIMEDOUT)
    return 0;
  if (r)                  /* closed connection */
    return -1;
  if ((b[1] & 0x80) == 0) /* accept only masked messages */
    return -1;
  switch (b[0] & 0x0f) {  /* opcode */
  case 0x0:               /* continuation */
    goto again;
  case 0x1:               /* text */
    op = HTTP_WSOP_TEXT;
    break;
  case 0x2:               /* binary */
    op = HTTP_WSOP_BINARY;
    break;
  case 0x8:               /* close connection */
    return -1;
  case 0x9:               /* ping */
    op = HTTP_WSOP_PING;
    break;
  case 0xa:               /* pong */
    op = HTTP_WSOP_PONG;
    break;
  default:
    return -1;
  }
  plen = b[1] & 0x7f;
  if (plen == 126) {
    if (tcp_read(hc->hc_fd, bl, 2))
      return -1;
    plen = (bl[0] << 8) | bl[1];
  } else if (plen == 127) {
    if (tcp_read(hc->hc_fd, bl, 8))
      return -1;
    plen = ((uint64_t)bl[0] << 56) | ((uint64_t)bl[1] << 48) |
           ((uint64_t)bl[2] << 40) | ((uint64_t)bl[3] << 32) |
           ((uint64_t)bl[4] << 24) | ((uint64_t)bl[5] << 16) |
           ((uint64_t)bl[6] << 8 ) | ((uint64_t)bl[7] << 0);
  }
  if (plen > 5*1024*1024)
    return -1;
  p = malloc(plen + 1);
  if (p == NULL)
    return -1;
  if (tcp_read(hc->hc_fd, bl, 4))
    goto err1;
  if (tcp_read(hc->hc_fd, p, plen))
    goto err1;
  /* apply mask descrambling */
  for (pi = 0; pi < plen; pi++)
    p[pi] ^= bl[pi & 3];
  if (op == HTTP_WSOP_PING) {
    http_websocket_send(hc, p, plen, HTTP_WSOP_PONG);
    free(p);
    goto again;
  }
  msg = htsmsg_create_map();
  htsmsg_add_s32(msg, "op", op);
  if (op == HTTP_WSOP_TEXT) {
    p[plen] = '\0';
    msg1 = p[0] == '{' || p[0] == '[' ?
             htsmsg_json_deserialize((char *)p) : NULL;
    if (msg1) {
      htsmsg_add_msg(msg, "json", msg1);
      free(p);
    } else {
      htsmsg_add_str_alloc(msg, "text", (char *)p);
    }
  } else {
    htsmsg_add_bin_alloc(msg, "bin", p, plen);
  }
  *_res = msg;
  return 0;
err1:
  free(p);
  return -1;
}

/**
 * Parse arguments of a HTTP GET url, not perfect, but works for us
 */
void
http_parse_args(http_arg_list_t *list, char *args)
{
  char *k, *v;

  if (args && *args == '&')
    args++;
  while(args) {
    k = args;
    if((args = strchr(args, '=')) != NULL) {
      *args++ = 0;
    } else {
      args = k;
    }

    v = args;
    args = strchr(args, '&');
    if(args != NULL)
      *args++ = 0;
    else if(v == k) {
      if (*k == '\0')
        break;
      v = NULL;
    }

    http_deescape(k);
    if (v)
      http_deescape(v);
    //    printf("%s = %s\n", k, v);
    http_arg_set(list, k, v);
  }
}

/**
 *
 */
void
http_serve_requests(http_connection_t *hc)
{
  htsbuf_queue_t spill;
  char *argv[3], *c, *s, *cmdline = NULL, *hdrline = NULL;
  int n, r, delim;

  tvh_mutex_init(&hc->hc_extra_lock, NULL);
  http_arg_init(&hc->hc_args);
  http_arg_init(&hc->hc_req_args);
  htsbuf_queue_init(&spill, 0);
  htsbuf_queue_init(&hc->hc_reply, 0);
  htsbuf_queue_init(&hc->hc_extra, 0);
  atomic_set(&hc->hc_extra_insend, 0);
  atomic_set(&hc->hc_extra_chunks, 0);

  do {
    hc->hc_no_output  = 0;

    if (cmdline) free(cmdline);

    if ((cmdline = tcp_read_line(hc->hc_fd, &spill)) == NULL)
      goto error;

    /* PROXY Protocol v1 support
     * Format: 'PROXY TCP4 192.168.0.1 192.168.0.11 56324 9981\r\n'
     *                     SRC-ADDRESS DST-ADDRESS  SPORT DPORT */
    if (config.proxy && strncmp(cmdline, "PROXY ", 6) == 0) {
      tvhtrace(hc->hc_subsys, "[PROXY] PROXY protocol detected! cmdline='%s'", cmdline);

      argv[0] = cmdline;
      s = cmdline + 6;

      if ((cmdline = tcp_read_line(hc->hc_fd, &spill)) == NULL)
        goto error;  /* No more data after the PROXY protocol */
        
      delim = '.';
      if (strncmp(s, "TCP6 ", 5) == 0) {
        delim = ':';
      } else if (strncmp(s, "TCP4 ", 5) != 0) {
        goto error;
      }

      s += 5;

      /* Check the SRC-ADDRESS */
      for (c = s; *c != ' '; c++) {
        if (*c == '\0') goto error;  /* Incomplete PROXY format */
        if (*c != delim && (*c < '0' || *c > '9')) {
          if (delim == ':') {
            if (*c >= 'a' && *c <= 'f') continue;
            if (*c >= 'A' && *c <= 'F') continue;
          }
          goto error;  /* Not valid IP address */
        }
      }
      if (*c != ' ') goto error;
      /* Check length */
      if ((c-s) < 7) goto error;
      if ((c-s) > (delim == ':' ? 45 : 15)) goto error;
      
      /* Add null terminator */
      *c = '\0';

      /* Don't care about DST-ADDRESS, SRC-PORT & DST-PORT
         All it's OK, push the original client IP */
      tvhtrace(hc->hc_subsys, "[PROXY] Original source='%s'", s);
      http_arg_set(&hc->hc_args, "X-Forwarded-For", s);
      free(argv[0]);
    }

    if((n = http_tokenize(cmdline, argv, 3, -1)) != 3)
      goto error;
    
    if((hc->hc_cmd = str2val(argv[0], HTTP_cmdtab)) == -1)
      goto error;

    hc->hc_url = argv[1];
    if((hc->hc_version = str2val(argv[2], HTTP_versiontab)) == -1)
      goto error;

    /* parse header */
    while(1) {
      if (hdrline) free(hdrline);

      if ((hdrline = tcp_read_line(hc->hc_fd, &spill)) == NULL)
        goto error;

      if(!*hdrline)
        break; /* header complete */

      if((n = http_tokenize(hdrline, argv, 2, -1)) < 2) {
        if ((c = strchr(hdrline, ':')) != NULL) {
          *c = '\0';
          argv[0] = hdrline;
          argv[1] = c + 1;
        } else {
          continue;
        }
      } else if((c = strrchr(argv[0], ':')) == NULL)
        goto error;

      *c = 0;
      http_arg_set(&hc->hc_args, argv[0], argv[1]);
    }

    r = process_request(hc, &spill);

    free(hc->hc_post_data);
    hc->hc_post_data = NULL;

    http_arg_flush(&hc->hc_args);
    http_arg_flush(&hc->hc_req_args);

    htsbuf_queue_flush(&hc->hc_reply);

    if (r)
      break;

  } while(hc->hc_keep_alive && atomic_get(&http_server_running));

error:
  free(hdrline);
  free(cmdline);
  htsbuf_queue_flush(&spill);
  htsbuf_queue_flush(&hc->hc_extra);

  free(hc->hc_nonce);
  hc->hc_nonce = NULL;

  free(hc->hc_proxy_ip);
  free(hc->hc_local_ip);
}


/**
 *
 */
static void
http_serve(int fd, void **opaque, struct sockaddr_storage *peer, 
	   struct sockaddr_storage *self)
{
  http_connection_t hc;

  /* Note: global_lock held on entry */
  tvh_mutex_unlock(&global_lock);
  memset(&hc, 0, sizeof(http_connection_t));
  *opaque = &hc;

  hc.hc_subsys  = LS_HTTP;
  hc.hc_fd      = fd;
  hc.hc_peer    = peer;
  hc.hc_self    = self;
  hc.hc_paths   = &http_paths;
  hc.hc_paths_mutex = &http_paths_mutex;
  hc.hc_process = http_process_request;

  http_serve_requests(&hc);

  close(fd);

  // Note: leave global_lock held for parent
  tvh_mutex_lock(&global_lock);
  *opaque = NULL;
}

void
http_cancel( void *opaque )
{
  http_connection_t *hc = opaque;

  if (hc) {
    shutdown(hc->hc_fd, SHUT_RDWR);
    hc->hc_shutdown = 1;
  }
}

/**
 *  Fire up HTTP server
 */
void
http_server_init(const char *bindaddr)
{
  static tcp_server_ops_t ops = {
    .start  = http_serve,
    .stop   = NULL,
    .cancel = http_cancel
  };
  RB_INIT(&http_nonces);
  if (tvheadend_webui_port > 0) {
    http_server = tcp_server_create(LS_HTTP, "HTTP", bindaddr, tvheadend_webui_port, &ops, NULL);
    atomic_set(&http_server_running, 1);
  }
}

void
http_server_register(void)
{
  tcp_server_register(http_server);
}

void
http_server_done(void)
{
  http_path_t *hp;
  http_nonce_t *n;

  tvh_mutex_lock(&global_lock);
  atomic_set(&http_server_running, 0);
  if (http_server)
    tcp_server_delete(http_server);
  http_server = NULL;
  tvh_mutex_lock(&http_paths_mutex);
  while ((hp = LIST_FIRST(&http_paths)) != NULL) {
    LIST_REMOVE(hp, hp_link);
    free((void *)hp->hp_path);
    free(hp);
  }
  tvh_mutex_unlock(&http_paths_mutex);
  while ((n = RB_FIRST(&http_nonces)) != NULL) {
    mtimer_disarm(&n->expire);
    RB_REMOVE(&http_nonces, n, link);
    free(n);
  }
  tvh_mutex_unlock(&global_lock);
}
