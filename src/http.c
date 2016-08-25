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

#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <openssl/md5.h>

#include "tvheadend.h"
#include "tcp.h"
#include "http.h"
#include "access.h"
#include "notify.h"
#include "channels.h"
#include "config.h"

#if ENABLE_ANDROID
#include <sys/socket.h>
#endif

void *http_server;
static int http_server_running;

static pthread_mutex_t http_paths_mutex = PTHREAD_MUTEX_INITIALIZER;
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

    pthread_mutex_lock(hc->hc_paths_mutex);
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
    pthread_mutex_unlock(hc->hc_paths_mutex);

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
  char nonce[MD5_DIGEST_LENGTH*2+1];
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
http_get_nonce(void)
{
  struct http_nonce *n = calloc(1, sizeof(*n));
  char stamp[32], *m;
  int64_t mono;

  while (1) {
    mono = getmonoclock();
    mono ^= 0xa1687211885fcd30LL;
    snprintf(stamp, sizeof(stamp), "%"PRId64, mono);
    m = md5sum(stamp, 1);
    strcpy(n->nonce, m);
    pthread_mutex_lock(&global_lock);
    if (RB_INSERT_SORTED(&http_nonces, n, link, http_nonce_cmp)) {
      pthread_mutex_unlock(&global_lock);
      free(m);
      continue; /* get unique md5 */
    }
    mtimer_arm_rel(&n->expire, http_nonce_timeout, n, sec2mono(10));
    pthread_mutex_unlock(&global_lock);
    break;
  }
  return m;
}

static int
http_nonce_exists(const char *nonce)
{
  struct http_nonce *n, tmp;

  if (nonce == NULL)
    return 0;
  strncpy(tmp.nonce, nonce, sizeof(tmp.nonce)-1);
  tmp.nonce[sizeof(tmp.nonce)-1] = '\0';
  pthread_mutex_lock(&global_lock);
  n = RB_FIND(&http_nonces, &tmp, link, http_nonce_cmp);
  if (n) {
    mtimer_arm_rel(&n->expire, http_nonce_timeout, n, sec2mono(2*60));
    pthread_mutex_unlock(&global_lock);
    return 1;
  }
  pthread_mutex_unlock(&global_lock);
  return 0;
}

static char *
http_get_opaque(const char *realm, const char *nonce)
{
  char *a = alloca(strlen(realm) + strlen(nonce) + 1);
  strcpy(a, realm);
  strcat(a, nonce);
  return md5sum(a, 1);
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

  lock_assert(&hc->hc_fd_lock);

  htsbuf_queue_init(&hdrs, 0);

  htsbuf_qprintf(&hdrs, "%s %d %s\r\n", 
		 http_ver2str(hc->hc_version), rc, http_rc2str(rc));

  if (hc->hc_version != RTSP_VERSION_1_0){
    htsbuf_append_str(&hdrs, "Server: HTS/tvheadend\r\n");
    if (config.cors_origin && config.cors_origin[0]) {
      htsbuf_qprintf(&hdrs, "Access-Control-Allow-Origin: %s\r\n", config.cors_origin);
      htsbuf_append_str(&hdrs, "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n");
      htsbuf_append_str(&hdrs, "Access-Control-Allow-Headers: x-requested-with\r\n");
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
    const char *realm = config.realm;
    if (realm == NULL || realm[0] == '\0')
      realm = "tvheadend";
    if (config.digest) {
      if (hc->hc_nonce == NULL)
        hc->hc_nonce = http_get_nonce();
      char *opaque = http_get_opaque(realm, hc->hc_nonce);
      htsbuf_append_str(&hdrs, "WWW-Authenticate: Digest realm=\"");
      htsbuf_append_str(&hdrs, realm);
      htsbuf_append_str(&hdrs, "\", qop=\"auth\", nonce=\"");
      htsbuf_append_str(&hdrs, hc->hc_nonce);
      htsbuf_append_str(&hdrs, "\", opaque=\"");
      htsbuf_append_str(&hdrs, opaque);
      htsbuf_append_str(&hdrs, "\"\r\n");
      free(opaque);
    } else {
      htsbuf_append_str(&hdrs, "WWW-Authenticate: Basic realm=\"");
      htsbuf_append_str(&hdrs, realm);
      htsbuf_append_str(&hdrs, "\"\r\n");
    }
  }
  if (hc->hc_logout_cookie == 1) {
    htsbuf_append_str(&hdrs, "Set-Cookie: logout=1; Path=\"/logout\"\r\n");
  } else if (hc->hc_logout_cookie == 2) {
    htsbuf_append_str(&hdrs, "Set-Cookie: logout=0; Path=\"/logout'\"; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n");
  }

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
      htsbuf_qprintf(&hdrs, "%s: %s\r\n", ra->key, ra->val);
    }
  }
  if(hc->hc_session && !sess)
    htsbuf_qprintf(&hdrs, "Session: %s\r\n", hc->hc_session);

  htsbuf_append_str(&hdrs, "\r\n");

  tcp_write_queue(hc->hc_fd, &hdrs);
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
static char *
http_get_header_value(const char *hdr, const char *name)
{
  char *tokbuf, *tok, *saveptr = NULL, *q, *s;

  tokbuf = tvh_strdupa(hdr);
  tok = strtok_r(tokbuf, ",", &saveptr);
  while (tok) {
    while (*tok == ' ')
      tok++;
    if ((q = strchr(tok, '=')) == NULL)
      goto next;
    *q = '\0';
    if (strcasecmp(tok, name))
      goto next;
    s = q + 1;
    if (*s == '"') {
      q = strchr(++s, '"');
      if (q)
        *q = '\0';
    } else {
      q = strchr(s, ' ');
      if (q)
        *q = '\0';
    }
    return strdup(s);
next:
    tok = strtok_r(NULL, ",", &saveptr);
  }
  return NULL;
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

  pthread_mutex_lock(&hc->hc_fd_lock);
  http_send_header(hc, rc, content, size,
		   encoding, location, maxage, 0, NULL, NULL);
  
  if(!hc->hc_no_output) {
    if (data == NULL)
      tcp_write_queue(hc->hc_fd, &hc->hc_reply);
    else
      tvh_write(hc->hc_fd, data, size);
  }
  pthread_mutex_unlock(&hc->hc_fd_lock);

  free(data);
}


/**
 * Send HTTP error back
 */
void
http_error(http_connection_t *hc, int error)
{
  const char *errtxt = http_rc2str(error);
  int level;

  if (!atomic_get(&http_server_running)) return;

  if (error != HTTP_STATUS_FOUND && error != HTTP_STATUS_MOVED) {
    level = LOG_INFO;
    if (error == HTTP_STATUS_UNAUTHORIZED)
      level = LOG_DEBUG;
    else if (error == HTTP_STATUS_BAD_REQUEST || error > HTTP_STATUS_UNAUTHORIZED)
      level = LOG_ERR;
    tvhlog(level, LS_HTTP, "%s: %s %s %s -- %d",
	   hc->hc_peer_ipstr, http_ver2str(hc->hc_version),
           http_cmd2str(hc->hc_cmd), hc->hc_url, error);
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

    if (error == HTTP_STATUS_UNAUTHORIZED)
      htsbuf_qprintf(&hc->hc_reply, "<P><A HREF=\"%s/\">Default Login</A></P>",
                     tvheadend_webroot ? tvheadend_webroot : "");

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
      htsbuf_append(&hq, "=", 1);
      htsbuf_append_and_escape_url(&hq, ra->val);
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
char *
http_get_hostpath(http_connection_t *hc)
{
  char buf[256];
  const char *host, *proto;

  host  = http_arg_get(&hc->hc_args, "Host") ?:
          http_arg_get(&hc->hc_args, "X-Forwarded-Host");
  proto = http_arg_get(&hc->hc_args, "X-Forwarded-Proto");

  snprintf(buf, sizeof(buf), "%s://%s%s",
           proto ?: "http", host, tvheadend_webroot ?: "");

  return strdup(buf);
}

/**
 *
 */
static void
http_access_verify_ticket(http_connection_t *hc)
{
  const char *ticket_id;

  if (hc->hc_access)
    return;
  ticket_id = http_arg_get(&hc->hc_req_args, "ticket");
  hc->hc_access = access_ticket_verify2(ticket_id, hc->hc_url);
  if (hc->hc_access == NULL)
    return;
  tvhinfo(LS_HTTP, "%s: using ticket %s for %s",
	  hc->hc_peer_ipstr, ticket_id, hc->hc_url);
}

/**
 *
 */
struct http_verify_structure {
  char *password;
  char *d_ha1;
  char *d_all;
  char *d_response;
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
     m = md5sum(ha1, 1);
     snprintf(all, sizeof(all), "%s:%s", m, v->d_all);
     free(m);
     m = md5sum(all, 1);
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
    char *realm = NULL, *nonce_count = NULL, *cnonce = NULL, *m = NULL;
    char all[1024];
    int res = -1;

    if (qop == NULL || uri == NULL)
      goto end;
     
    if (strcasecmp(qop, "auth-int") == 0) {
      m = md5sum(hc->hc_post_data ?: "", 1);
      snprintf(all, sizeof(all), "%s:%s:%s", method, uri, m);
      free(m);
      m = NULL;
    } else if (strcasecmp(qop, "auth") == 0) {
      snprintf(all, sizeof(all), "%s:%s", method, uri);
    } else {
      goto end;
    }

    m = md5sum(all, 1);
    if (qop == NULL || *qop == '\0') {
      snprintf(all, sizeof(all), "%s:%s", hc->hc_nonce, m);
      goto set;
    } else {
      realm = http_get_header_value(hc->hc_authhdr, "realm");
      nonce_count = http_get_header_value(hc->hc_authhdr, "nc");
      cnonce = http_get_header_value(hc->hc_authhdr, "cnonce");
      if (realm == NULL || nonce_count == NULL || cnonce == NULL) {
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
  int res = -1;

  http_access_verify_ticket(hc);
  if (hc->hc_access)
    res = access_verify2(hc->hc_access, mask);

  if (res) {
    access_destroy(hc->hc_access);
    if (http_verify_prepare(hc, &v)) {
      hc->hc_access = NULL;
      return -1;
    }
    hc->hc_access = access_get((struct sockaddr *)hc->hc_peer, hc->hc_username,
                               http_verify_callback, &v);
    http_verify_free(&v);
    if (hc->hc_access)
      res = access_verify2(hc->hc_access, mask);
  }

  return res;
}

/**
 * Return non-zero if no access
 */
int
http_access_verify_channel(http_connection_t *hc, int mask,
                           struct channel *ch)
{
  struct http_verify_structure v;
  int res = -1;

  assert(ch);

  http_access_verify_ticket(hc);
  if (hc->hc_access) {
    res = access_verify2(hc->hc_access, mask);

    if (!res && !channel_access(ch, hc->hc_access, 0))
      res = -1;
  }

  if (res) {
    access_destroy(hc->hc_access);
    if (http_verify_prepare(hc, &v))
      return -1;
    hc->hc_access = access_get((struct sockaddr *)hc->hc_peer, hc->hc_username,
                               http_verify_callback, &v);
    http_verify_free(&v);
    if (hc->hc_access) {
      res = access_verify2(hc->hc_access, mask);

      if (!res && !channel_access(ch, hc->hc_access, 0))
        res = -1;
    }
  }


  return res;
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

  if(http_access_verify(hc, hp->hp_accessmask))
    err = HTTP_STATUS_UNAUTHORIZED;
  else
    err = hp->hp_callback(hc, remain, hp->hp_opaque);
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

  tvhtrace(LS_HTTP, "%s %s %s%s", http_ver2str(hc->hc_version),
           http_cmd2str(hc->hc_cmd), hc->hc_url, buf);
}

/**
 * HTTP GET
 */
static int
http_cmd_options(http_connection_t *hc)
{
  pthread_mutex_lock(&hc->hc_fd_lock);
  http_send_header(hc, HTTP_STATUS_OK, NULL, INT64_MIN,
		   NULL, NULL, -1, 0, NULL, NULL);
  pthread_mutex_unlock(&hc->hc_fd_lock);
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
  tcp_get_str_from_ip((struct sockaddr*)hc->hc_peer, authbuf, sizeof(authbuf));
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
    if((v = http_arg_get(&hc->hc_args, "CSeq")) != NULL)
      hc->hc_cseq = strtoll(v, NULL, 10);
    else
      hc->hc_cseq = 0;
    free(hc->hc_session);
    if ((v = http_arg_get(&hc->hc_args, "Session")) != NULL)
      hc->hc_session = tvh_strdupa(v);
    else
      hc->hc_session = NULL;
    if(hc->hc_cseq == 0) {
      http_error(hc, HTTP_STATUS_BAD_REQUEST);
      return -1;
    }
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
        n = base64_decode((uint8_t *)authbuf, argv[1], sizeof(authbuf) - 1);
        if (n < 0)
          n = 0;
        authbuf[n] = 0;
        if((n = http_tokenize(authbuf, argv, 2, ':')) == 2) {
          hc->hc_username = tvh_strdupa(argv[0]);
          hc->hc_password = tvh_strdupa(argv[1]);
          // No way to actually track this
        }
      } else if (strcasecmp(argv[0], "digest") == 0) {
        v = http_get_header_value(argv[1], "nonce");
        if (v == NULL || !http_nonce_exists(v)) {
          http_error(hc, HTTP_STATUS_UNAUTHORIZED);
          free(v);
          return -1;
        }
        free(hc->hc_nonce);
        hc->hc_nonce = v;
        v = http_get_header_value(argv[1], "username");
        hc->hc_authhdr  = tvh_strdupa(argv[1]);
        hc->hc_username = tvh_strdupa(v);
        free(v);
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
    if (hc->hc_cseq)
      rval = hc->hc_process(hc, spill);
    else
      http_error(hc, HTTP_STATUS_HTTP_VERSION);
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
http_arg_flush(struct http_arg_list *list)
{
  http_arg_t *ra;
  while((ra = TAILQ_FIRST(list)) != NULL)
    http_arg_remove(list, ra);
}


/**
 * Find an argument associated with a connection
 */
char *
http_arg_get(struct http_arg_list *list, const char *name)
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
  static char __thread buf[128];
  http_arg_t *ra;
  TAILQ_FOREACH(ra, list, link)
    if(!strcasecmp(ra->key, name)) {
      TAILQ_REMOVE(list, ra, link);
      strncpy(buf, ra->val, sizeof(buf)-1);
      buf[sizeof(buf)-1] = '\0';
      free(ra->key);
      free(ra->val);
      free(ra);
      return buf;
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
  ra->val = strdup(val);
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
  pthread_mutex_lock(&http_paths_mutex);
  LIST_INSERT_HEAD(&http_paths, hp, hp_link);
  pthread_mutex_unlock(&http_paths_mutex);
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
 * De-escape HTTP URL
 */
void
http_deescape(char *s)
{
  char v, *d = s;

  while(*s) {
    if(*s == '+') {
      *d++ = ' ';
      s++;
    } else if(*s == '%') {
      s++;
      switch(*s) {
      case '0' ... '9':
	v = (*s - '0') << 4;
	break;
      case 'a' ... 'f':
	v = (*s - 'a' + 10) << 4;
	break;
      case 'A' ... 'F':
	v = (*s - 'A' + 10) << 4;
	break;
      default:
	*d = 0;
	return;
      }
      s++;
      switch(*s) {
      case '0' ... '9':
	v |= (*s - '0');
	break;
      case 'a' ... 'f':
	v |= (*s - 'a' + 10);
	break;
      case 'A' ... 'F':
	v |= (*s - 'A' + 10);
	break;
      default:
	*d = 0;
	return;
      }
      s++;

      *d++ = v;
    } else {
      *d++ = *s++;
    }
  }
  *d = 0;
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
    if((args = strchr(args, '=')) == NULL)
      break;
    *args++ = 0;
    v = args;
    args = strchr(args, '&');

    if(args != NULL)
      *args++ = 0;

    http_deescape(k);
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
  char *argv[3], *c, *cmdline = NULL, *hdrline = NULL;
  int n, r;

  pthread_mutex_init(&hc->hc_fd_lock, NULL);
  http_arg_init(&hc->hc_args);
  http_arg_init(&hc->hc_req_args);
  htsbuf_queue_init(&spill, 0);
  htsbuf_queue_init(&hc->hc_reply, 0);

  do {
    hc->hc_no_output  = 0;

    if (cmdline) free(cmdline);

    if ((cmdline = tcp_read_line(hc->hc_fd, &spill)) == NULL)
      goto error;

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

    hc->hc_logout_cookie = 0;

  } while(hc->hc_keep_alive && atomic_get(&http_server_running));

error:
  free(hdrline);
  free(cmdline);
  htsbuf_queue_flush(&spill);

  free(hc->hc_nonce);
  hc->hc_nonce = NULL;

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
  pthread_mutex_unlock(&global_lock);
  memset(&hc, 0, sizeof(http_connection_t));
  *opaque = &hc;

  hc.hc_fd      = fd;
  hc.hc_peer    = peer;
  hc.hc_self    = self;
  hc.hc_paths   = &http_paths;
  hc.hc_paths_mutex = &http_paths_mutex;
  hc.hc_process = http_process_request;

  http_serve_requests(&hc);

  close(fd);

  // Note: leave global_lock held for parent
  pthread_mutex_lock(&global_lock);
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
  http_server = tcp_server_create(LS_HTTP, "HTTP", bindaddr, tvheadend_webui_port, &ops, NULL);
  atomic_set(&http_server_running, 1);
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

  pthread_mutex_lock(&global_lock);
  atomic_set(&http_server_running, 0);
  if (http_server)
    tcp_server_delete(http_server);
  http_server = NULL;
  pthread_mutex_lock(&http_paths_mutex);
  while ((hp = LIST_FIRST(&http_paths)) != NULL) {
    LIST_REMOVE(hp, hp_link);
    free((void *)hp->hp_path);
    free(hp);
  }
  pthread_mutex_unlock(&http_paths_mutex);
  while ((n = RB_FIRST(&http_nonces)) != NULL) {
    mtimer_disarm(&n->expire);
    RB_REMOVE(&http_nonces, n, link);
    free(n);
  }
  pthread_mutex_unlock(&global_lock);
}
