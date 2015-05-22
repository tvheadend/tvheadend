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

#include "tvheadend.h"
#include "tcp.h"
#include "http.h"
#include "filebundle.h"
#include "access.h"
#include "notify.h"
#include "channels.h"

void *http_server;

static http_path_list_t http_paths;

static struct strtab HTTP_cmdtab[] = {
  { "NONE",       HTTP_CMD_NONE },
  { "GET",        HTTP_CMD_GET },
  { "HEAD",       HTTP_CMD_HEAD },
  { "POST",       HTTP_CMD_POST },
  { "DESCRIBE",   RTSP_CMD_DESCRIBE },
  { "OPTIONS",    RTSP_CMD_OPTIONS },
  { "SETUP",      RTSP_CMD_SETUP },
  { "PLAY",       RTSP_CMD_PLAY },
  { "TEARDOWN",   RTSP_CMD_TEARDOWN },
  { "PAUSE",      RTSP_CMD_PAUSE },
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
static http_path_t *
http_resolve(http_connection_t *hc, char **remainp, char **argsp)
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

    LIST_FOREACH(hp, hc->hc_paths, hp_link) {
      if(!strncmp(path, hp->hp_path, hp->hp_len)) {
        if(path[hp->hp_len] == 0 ||
           path[hp->hp_len] == '/' ||
           path[hp->hp_len] == '?')
          break;
      }
    }

    if(hp == NULL)
      return NULL;

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
    return NULL;
  }

  return hp;
}



/*
 * HTTP status code to string
 */

static const char *
http_rc2str(int code)
{
  switch(code) {
  case HTTP_STATUS_OK:              return "OK";
  case HTTP_STATUS_PARTIAL_CONTENT: return "Partial Content";
  case HTTP_STATUS_FOUND:           return "Found";
  case HTTP_STATUS_BAD_REQUEST:     return "Bad Request";
  case HTTP_STATUS_UNAUTHORIZED:    return "Unauthorized";
  case HTTP_STATUS_NOT_FOUND:       return "Not Found";
  case HTTP_STATUS_UNSUPPORTED:     return "Unsupported Media Type";
  case HTTP_STATUS_BANDWIDTH:       return "Not Enough Bandwidth";
  case HTTP_STATUS_BAD_SESSION:     return "Session Not Found";
  case HTTP_STATUS_HTTP_VERSION:    return "HTTP/RTSP Version Not Supported";
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

  htsbuf_queue_init(&hdrs, 0);

  htsbuf_qprintf(&hdrs, "%s %d %s\r\n", 
		 http_ver2str(hc->hc_version), rc, http_rc2str(rc));

  if (hc->hc_version != RTSP_VERSION_1_0)
    htsbuf_qprintf(&hdrs, "Server: HTS/tvheadend\r\n");

  if(maxage == 0) {
    if (hc->hc_version != RTSP_VERSION_1_0)
      htsbuf_qprintf(&hdrs, "Cache-Control: no-cache\r\n");
  } else {
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

  if(rc == HTTP_STATUS_UNAUTHORIZED)
    htsbuf_qprintf(&hdrs, "WWW-Authenticate: Basic realm=\"tvheadend\"\r\n");
  if (hc->hc_logout_cookie == 1) {
    htsbuf_qprintf(&hdrs, "Set-Cookie: logout=1; Path=\"/logout\"\r\n");
  } else if (hc->hc_logout_cookie == 2) {
    htsbuf_qprintf(&hdrs, "Set-Cookie: logout=0; Path=\"/logout'\"; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n");
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

  htsbuf_qprintf(&hdrs, "\r\n");

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
    data = gzip_deflate(data2, size, &size);
    free(data2);
    encoding = "gzip";
  }
#endif

  http_send_header(hc, rc, content, size,
		   encoding, location, maxage, 0, NULL, NULL);
  
  if(!hc->hc_no_output) {
    if (data == NULL)
      tcp_write_queue(hc->hc_fd, &hc->hc_reply);
    else
      tvh_write(hc->hc_fd, data, size);
  }

  free(data);
}


/**
 * Send HTTP error back
 */
void
http_error(http_connection_t *hc, int error)
{
  const char *errtxt = http_rc2str(error);

  if (!http_server) return;

  if (error != HTTP_STATUS_FOUND && error != HTTP_STATUS_MOVED)
    tvhlog(error < 400 ? LOG_INFO : LOG_ERR, "http", "%s: %s %s %s -- %d",
	   hc->hc_peer_ipstr, http_ver2str(hc->hc_version),
           http_cmd2str(hc->hc_cmd), hc->hc_url, error);

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
      htsbuf_qprintf(&hc->hc_reply, "<P><A HREF=\"/\">Default Login</A></P>");

    htsbuf_qprintf(&hc->hc_reply, "</BODY></HTML>\r\n");

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
              http_arg_list_t *req_args)
{
  const char *loc = location;
  htsbuf_queue_flush(&hc->hc_reply);

  if (req_args) {
    http_arg_t *ra;
    htsbuf_queue_t hq;
    int first = 1;
    htsbuf_queue_init(&hq, 0);
    htsbuf_append(&hq, location, strlen(location));
    htsbuf_append(&hq, "?", 1);
    TAILQ_FOREACH(ra, req_args, link) {
      if (!first)
        htsbuf_append(&hq, "&", 1);
      first = 0;
      htsbuf_append_and_escape_url(&hq, ra->key);
      htsbuf_append(&hq, "=", 1);
      htsbuf_append_and_escape_url(&hq, ra->val);
    }
    loc = htsbuf_to_string(&hq);
    htsbuf_queue_flush(&hq);
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
    free((void *)loc);
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
  tvhlog(LOG_INFO, "http", "%s: using ticket %s for %s",
	 hc->hc_peer_ipstr, ticket_id, hc->hc_url);
}

/**
 * Return non-zero if no access
 */
int
http_access_verify(http_connection_t *hc, int mask)
{
  http_access_verify_ticket(hc);

  if (hc->hc_access == NULL) {
    hc->hc_access = access_get(hc->hc_username, hc->hc_password,
                               (struct sockaddr *)hc->hc_peer);
    if (hc->hc_access == NULL)
      return -1;
  }

  return access_verify2(hc->hc_access, mask);
}

/**
 * Return non-zero if no access
 */
int
http_access_verify_channel(http_connection_t *hc, int mask,
                           struct channel *ch, int ticket)
{
  int res = -1;

  assert(ch);

  if (ticket)
    http_access_verify_ticket(hc);

  if (hc->hc_access == NULL) {
    hc->hc_access = access_get(hc->hc_username, hc->hc_password,
                               (struct sockaddr *)hc->hc_peer);
    if (hc->hc_access == NULL)
      return -1;
  }

  if (access_verify2(hc->hc_access, mask))
    return -1;

  if (channel_access(ch, hc->hc_access, 0))
    res = 0;
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
#if ENABLE_TRACE
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

  tvhtrace("http", "%s %s %s%s", http_ver2str(hc->hc_version),
           http_cmd2str(hc->hc_cmd), hc->hc_url, buf);
}
#else
static inline void
dump_request(http_connection_t *hc)
{
}
#endif

/**
 * HTTP GET
 */
static int
http_cmd_get(http_connection_t *hc)
{
  http_path_t *hp;
  char *remain;
  char *args;

  dump_request(hc);

  hp = http_resolve(hc, &remain, &args);
  if(hp == NULL) {
    http_error(hc, HTTP_STATUS_NOT_FOUND);
    return 0;
  }

  if(args != NULL)
    http_parse_get_args(hc, args);

  return http_exec(hc, hp, remain);
}





/**
 * Initial processing of HTTP POST
 *
 * Return non-zero if we should disconnect
 */
static int
http_cmd_post(http_connection_t *hc, htsbuf_queue_t *spill)
{
  http_path_t *hp;
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
      http_parse_get_args(hc, hc->hc_post_data);
  }

  dump_request(hc);

  hp = http_resolve(hc, &remain, &args);
  if(hp == NULL) {
    http_error(hc, HTTP_STATUS_NOT_FOUND);
    return 0;
  }
  return http_exec(hc, hp, remain);
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
  tcp_get_ip_str((struct sockaddr*)hc->hc_peer, authbuf, sizeof(authbuf));
  hc->hc_peer_ipstr = tvh_strdupa(authbuf);
  hc->hc_representative = hc->hc_peer_ipstr;
  hc->hc_username = NULL;
  hc->hc_password = NULL;
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
      n = base64_decode((uint8_t *)authbuf, argv[1], sizeof(authbuf) - 1);
      if (n < 0)
        n = 0;
      authbuf[n] = 0;
      if((n = http_tokenize(authbuf, argv, 2, ':')) == 2) {
        hc->hc_username = tvh_strdupa(argv[0]);
        hc->hc_password = tvh_strdupa(argv[1]);
        // No way to actually track this
      }
    }
  }

  if (hc->hc_username)
    hc->hc_representative = hc->hc_username;

  switch(hc->hc_version) {
  case RTSP_VERSION_1_0:
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
 * Delete all arguments associated with a connection
 */
void
http_arg_flush(struct http_arg_list *list)
{
  http_arg_t *ra;
  while((ra = TAILQ_FIRST(list)) != NULL) {
    TAILQ_REMOVE(list, ra, link);
    free(ra->key);
    free(ra->val);
    free(ra);
  }
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
  LIST_INSERT_HEAD(&http_paths, hp, hp_link);
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
http_parse_get_args(http_connection_t *hc, char *args)
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
    http_arg_set(&hc->hc_req_args, k, v);
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

  } while(hc->hc_keep_alive && http_server);

error:
  free(hdrline);
  free(cmdline);
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
  http_server = tcp_server_create(bindaddr, tvheadend_webui_port, &ops, NULL);
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

  pthread_mutex_lock(&global_lock);
  if (http_server)
    tcp_server_delete(http_server);
  http_server = NULL;
  while ((hp = LIST_FIRST(&http_paths)) != NULL) {
    LIST_REMOVE(hp, hp_link);
    free((void *)hp->hp_path);
    free(hp);
  }
  pthread_mutex_unlock(&global_lock);
}
