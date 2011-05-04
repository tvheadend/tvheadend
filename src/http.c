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
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "tvheadend.h"
#include "tcp.h"
#include "http.h"
#include "access.h"

static void *http_server;

static LIST_HEAD(, http_path) http_paths;

static struct strtab HTTP_cmdtab[] = {
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

static void http_parse_get_args(http_connection_t *hc, char *args);

/**
 *
 */
static http_path_t *
http_resolve(http_connection_t *hc, char **remainp, char **argsp)
{
  http_path_t *hp;
  char *v;
  LIST_FOREACH(hp, &http_paths, hp_link) {
    if(!strncmp(hc->hc_url, hp->hp_path, hp->hp_len)) {
      if(hc->hc_url[hp->hp_len] == 0 || hc->hc_url[hp->hp_len] == '/' ||
	 hc->hc_url[hp->hp_len] == '?')
	break;
    }
  }

  if(hp == NULL)
    return NULL;

  v = hc->hc_url + hp->hp_len;

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
  case HTTP_STATUS_NOT_FOUND:       return "Not found";
  case HTTP_STATUS_UNAUTHORIZED:    return "Unauthorized";
  case HTTP_STATUS_BAD_REQUEST:     return "Bad request";
  case HTTP_STATUS_FOUND:           return "Found";
  default:
    return "Unknown returncode";
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
		 const char *disposition)
{
  struct tm tm0, *tm;
  htsbuf_queue_t hdrs;
  time_t t;

  htsbuf_queue_init(&hdrs, 0);

  htsbuf_qprintf(&hdrs, "%s %d %s\r\n", 
		 val2str(hc->hc_version, HTTP_versiontab),
		 rc, http_rc2str(rc));

  htsbuf_qprintf(&hdrs, "Server: HTS/tvheadend\r\n");

  if(maxage == 0) {
    htsbuf_qprintf(&hdrs, "Cache-Control: no-cache\r\n");
  } else {
    time(&t);

    tm = gmtime_r(&t, &tm0);
    htsbuf_qprintf(&hdrs, 
		"Last-Modified: %s, %02d %s %d %02d:%02d:%02d GMT\r\n",
		cachedays[tm->tm_wday],	tm->tm_year + 1900,
		cachemonths[tm->tm_mon], tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

    t += maxage;

    tm = gmtime_r(&t, &tm0);
    htsbuf_qprintf(&hdrs, 
		"Expires: %s, %02d %s %d %02d:%02d:%02d GMT\r\n",
		cachedays[tm->tm_wday],	tm->tm_year + 1900,
		cachemonths[tm->tm_mon], tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
      
    htsbuf_qprintf(&hdrs, "Cache-Control: max-age=%d\r\n", maxage);
  }

  if(rc == HTTP_STATUS_UNAUTHORIZED)
    htsbuf_qprintf(&hdrs, "WWW-Authenticate: Basic realm=\"tvheadend\"\r\n");

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
  
  htsbuf_qprintf(&hdrs, "\r\n");

  tcp_write_queue(hc->hc_fd, &hdrs);
}



/**
 * Transmit a HTTP reply
 */
static void
http_send_reply(http_connection_t *hc, int rc, const char *content, 
		const char *encoding, const char *location, int maxage)
{
  http_send_header(hc, rc, content, hc->hc_reply.hq_size,
		   encoding, location, maxage, 0, NULL);
  
  if(hc->hc_no_output)
    return;

  tcp_write_queue(hc->hc_fd, &hc->hc_reply);
}


/**
 * Send HTTP error back
 */
void
http_error(http_connection_t *hc, int error)
{
  const char *errtxt = http_rc2str(error);

  tvhlog(LOG_ERR, "HTTP", "%s: %s -- %d", 
	 inet_ntoa(hc->hc_peer->sin_addr), hc->hc_url, error);

  htsbuf_queue_flush(&hc->hc_reply);

  htsbuf_qprintf(&hc->hc_reply, 
		 "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
		 "<HTML><HEAD>\r\n"
		 "<TITLE>%d %s</TITLE>\r\n"
		 "</HEAD><BODY>\r\n"
		 "<H1>%d %s</H1>\r\n"
		 "</BODY></HTML>\r\n",
		 error, errtxt,  error, errtxt);

  http_send_reply(hc, error, "text/html", NULL, NULL, 0);
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
http_redirect(http_connection_t *hc, const char *location)
{
  htsbuf_queue_flush(&hc->hc_reply);

  htsbuf_qprintf(&hc->hc_reply,
		 "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
		 "<HTML><HEAD>\r\n"
		 "<TITLE>Redirect</TITLE>\r\n"
		 "</HEAD><BODY>\r\n"
		 "Please follow <a href=\"%s\">%s</a>\r\n"
		 "</BODY></HTML>\r\n",
		 location, location);

  http_send_reply(hc, HTTP_STATUS_FOUND, "text/html", NULL, location, 0);
}

/**
 * Return non-zero if no access
 */
int
http_access_verify(http_connection_t *hc, int mask)
{
  const char *ticket_id = http_arg_get(&hc->hc_req_args, "ticket");

  if(!access_ticket_verify(ticket_id, hc->hc_url)) {
    tvhlog(LOG_INFO, "HTTP", "%s: using ticket %s for %s", 
	   inet_ntoa(hc->hc_peer->sin_addr), ticket_id,
	   hc->hc_url);
    return 0;
  }

  return access_verify(hc->hc_username, hc->hc_password,
		       (struct sockaddr *)hc->hc_peer, mask);
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

  if(err == -1)
     return 1;

  if(err)
    http_error(hc, err);
  return 0;
}



/**
 * HTTP GET
 */
static int
http_cmd_get(http_connection_t *hc)
{
  http_path_t *hp;
  char *remain;
  char *args;

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
  char *remain, *args, *v, *argv[2];
  int n;

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
  if(v == NULL) {
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    return 0;
  }
  n = http_tokenize(v, argv, 2, ';');
  if(n == 0) {
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    return 0;
  }

  if(!strcmp(argv[0], "application/x-www-form-urlencoded"))
    http_parse_get_args(hc, hc->hc_post_data);

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
  uint8_t authbuf[150];
  
  hc->hc_url_orig = tvh_strdupa(hc->hc_url);

  /* Set keep-alive status */
  v = http_arg_get(&hc->hc_args, "connection");

  switch(hc->hc_version) {
  case RTSP_VERSION_1_0:
    hc->hc_keep_alive = 1;
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
      n = base64_decode(authbuf, argv[1], sizeof(authbuf) - 1);
      authbuf[n] = 0;
      if((n = http_tokenize((char *)authbuf, argv, 2, ':')) == 2) {
	hc->hc_username = strdup(argv[0]);
	hc->hc_password = strdup(argv[1]);
      }
    }
  }

  if(hc->hc_username != NULL) {
    hc->hc_representative = strdup(hc->hc_username);
  } else {
    hc->hc_representative = malloc(30);
    /* Not threadsafe ? */
    snprintf(hc->hc_representative, 30,
	     "%s", inet_ntoa(hc->hc_peer->sin_addr));

  }

  switch(hc->hc_version) {
  case RTSP_VERSION_1_0:
    break;

  case HTTP_VERSION_1_0:
  case HTTP_VERSION_1_1:
    rval = http_process_request(hc, spill);
    break;
  }
  free(hc->hc_representative);
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
 * Set an argument associated with a connection
 */
void
http_arg_set(struct http_arg_list *list, char *key, char *val)
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
http_path_add(const char *path, void *opaque, http_callback_t *callback,
	      uint32_t accessmask)
{
  http_path_t *hp = malloc(sizeof(http_path_t));

  hp->hp_len      = strlen(path);
  hp->hp_path     = strdup(path);
  hp->hp_opaque   = opaque;
  hp->hp_callback = callback;
  hp->hp_accessmask = accessmask;
  LIST_INSERT_HEAD(&http_paths, hp, hp_link);
  return hp;
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
static void
http_parse_get_args(http_connection_t *hc, char *args)
{
  char *k, *v;

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
static void
http_serve_requests(http_connection_t *hc, htsbuf_queue_t *spill)
{
  char cmdline[1024];
  char hdrline[1024];
  char *argv[3], *c;
  int n;

  htsbuf_queue_init(&hc->hc_reply, 0);

  do {
    hc->hc_no_output  = 0;

    if(tcp_read_line(hc->hc_fd, cmdline, sizeof(cmdline), spill) < 0)
      return;

    if((n = http_tokenize(cmdline, argv, 3, -1)) != 3)
      return;
    
    if((hc->hc_cmd = str2val(argv[0], HTTP_cmdtab)) == -1)
      return;
    hc->hc_url = argv[1];
    if((hc->hc_version = str2val(argv[2], HTTP_versiontab)) == -1)
      return;

    /* parse header */
    while(1) {
      if(tcp_read_line(hc->hc_fd, hdrline, sizeof(hdrline), spill) < 0)
	return;

      if(hdrline[0] == 0)
	break; /* header complete */

      if((n = http_tokenize(hdrline, argv, 2, -1)) < 2)
	continue;

      if((c = strrchr(argv[0], ':')) == NULL)
	return;

      *c = 0;
      http_arg_set(&hc->hc_args, argv[0], argv[1]);
    }

    if(process_request(hc, spill))
      break;

    free(hc->hc_post_data);
    hc->hc_post_data = NULL;

    http_arg_flush(&hc->hc_args);
    http_arg_flush(&hc->hc_req_args);

    htsbuf_queue_flush(&hc->hc_reply);

    free(hc->hc_username);
    hc->hc_username = NULL;

    free(hc->hc_password);
    hc->hc_password = NULL;

  } while(hc->hc_keep_alive);
  
}


/**
 *
 */
static void
http_serve(int fd, void *opaque, struct sockaddr_in *peer, 
	   struct sockaddr_in *self)
{
  htsbuf_queue_t spill;
  http_connection_t hc;
  
  memset(&hc, 0, sizeof(http_connection_t));

  TAILQ_INIT(&hc.hc_args);
  TAILQ_INIT(&hc.hc_req_args);

  hc.hc_fd = fd;
  hc.hc_peer = peer;
  hc.hc_self = self;

  htsbuf_queue_init(&spill, 0);

  http_serve_requests(&hc, &spill);

  free(hc.hc_post_data);
  free(hc.hc_username);
  free(hc.hc_password);

  http_arg_flush(&hc.hc_args);
  http_arg_flush(&hc.hc_req_args);

  htsbuf_queue_flush(&spill);
  close(fd);
}


/**
 *  Fire up HTTP server
 */
void
http_server_init(void)
{
  http_server = tcp_server_create(9981, http_serve, NULL);
}
