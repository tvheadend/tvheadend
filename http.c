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

#include <libavutil/base64.h>

#include "tvhead.h"
#include "channels.h"
#include "subscriptions.h"
#include "epg.h"
#include "teletext.h"
#include "dispatch.h"
#include "dvb.h"
#include "rtp.h"
#include "tsmux.h"
#include "http.h"
#include "rtsp.h"
#include "access.h"

int http_port;

static LIST_HEAD(, http_path) http_paths;

static struct strtab HTTP_cmdtab[] = {
  { "GET",        HTTP_CMD_GET },
  { "POST",       HTTP_CMD_POST },
  { "DESCRIBE",   RTSP_CMD_DESCRIBE },
  { "OPTIONS",    RTSP_CMD_OPTIONS },
  { "SETUP",      RTSP_CMD_SETUP },
  { "PLAY",       RTSP_CMD_PLAY },
  { "TEARDOWN",   RTSP_CMD_TEARDOWN },
  { "PAUSE",      RTSP_CMD_PAUSE },
};



static struct strtab HTTP_versiontab[] = {
  { "HTTP/0.9",        HTTP_VERSION_0_9 },
  { "HTTP/1.0",        HTTP_VERSION_1_0 },
  { "HTTP/1.1",        HTTP_VERSION_1_1 },
  { "RTSP/1.0",        RTSP_VERSION_1_0 }, /* not enabled yet */
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
    if(!strncmp(hc->hc_url, hp->hp_path, hp->hp_len))
      break;
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
  case HTTP_STATUS_OK:              return "Ok";
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
 *
 */
static void
http_destroy_reply(http_connection_t *hc, http_reply_t *hr)
{
  if(hr->hr_destroy != NULL)
    hr->hr_destroy(hr, hr->hr_opaque);

  TAILQ_REMOVE(&hc->hc_replies, hr, hr_link);
  free(hr->hr_location);
  tcp_flush_queue(&hr->hr_tq);
  free(hr);
}

/**
 * Transmit a HTTP reply
 *
 * Return non-zero if we should disconnect (no more keep-alive)
 */
static int
http_send_reply(http_connection_t *hc, http_reply_t *hr)
{
  struct tm tm0, *tm;
  time_t t;
  tcp_queue_t *tq = &hr->hr_tq;
  int r;

  if(hr->hr_version >= HTTP_VERSION_1_0) {
    http_printf(hc, "%s %d %s\r\n", val2str(hr->hr_version, HTTP_versiontab),
		hr->hr_rc, http_rc2str(hr->hr_rc));

    http_printf(hc, "Server: HTS/tvheadend\r\n");

    if(hr->hr_maxage == 0) {
      http_printf(hc, "Cache-Control: no-cache\r\n");
    } else {
      t = dispatch_clock;
      
      tm = gmtime_r(&t, &tm0);
      http_printf(hc, 
		  "Last-Modified: %s, %02d %s %d %02d:%02d:%02d GMT\r\n",
		  cachedays[tm->tm_wday],	tm->tm_year + 1900,
		  cachemonths[tm->tm_mon], tm->tm_mday,
		  tm->tm_hour, tm->tm_min, tm->tm_sec);

      t += hr->hr_maxage;

      tm = gmtime_r(&t, &tm0);
      http_printf(hc, 
		  "Expires: %s, %02d %s %d %02d:%02d:%02d GMT\r\n",
		  cachedays[tm->tm_wday],	tm->tm_year + 1900,
		  cachemonths[tm->tm_mon], tm->tm_mday,
		  tm->tm_hour, tm->tm_min, tm->tm_sec);
      
      http_printf(hc, "Cache-Control: max-age=%d\r\n", hr->hr_maxage);
    }

    http_printf(hc, "Connection: %s\r\n", 
		hr->hr_keep_alive ? "Keep-Alive" : "Close");

    if(hr->hr_rc == HTTP_STATUS_UNAUTHORIZED)
      http_printf(hc, "WWW-Authenticate: Basic realm=\"tvheadend\"\r\n");

    if(hr->hr_encoding != NULL)
      http_printf(hc, "Content-Encoding: %s\r\n", hr->hr_encoding);

    if(hr->hr_location != NULL)
      http_printf(hc, "Location: %s\r\n", hr->hr_location);

    http_printf(hc, 
		"Content-Type: %s\r\n"
		"Content-Length: %d\r\n"
		"\r\n",
		hr->hr_content, tq->tq_depth);
  }
  tcp_output_queue(&hc->hc_tcp_session, NULL, tq);

  r = !hr->hr_keep_alive;

  http_destroy_reply(hc, hr);
  return r;
}


/**
 * Send HTTP replies
 *
 * Return non-zero if we should disconnect
 */
static int
http_xmit_queue(http_connection_t *hc)
{
  http_reply_t *hr;

  while((hr = TAILQ_FIRST(&hc->hc_replies)) != NULL) {
    if(hr->hr_destroy != NULL)
      break; /* Pending reply, cannot send this yet */
    if(http_send_reply(hc, hr))
      return 1;
  }
  return 0;
}


/**
 * Continue HTTP session, called by deferred replies
 */
void
http_continue(http_connection_t *hc)
{
  if(http_xmit_queue(hc))
    tcp_disconnect(&hc->hc_tcp_session, 0);
}

/**
 * Send HTTP error back
 */
void
http_error(http_connection_t *hc, http_reply_t *hr, int error)
{
  const char *errtxt = http_rc2str(error);
  tcp_queue_t *tq = &hr->hr_tq;

  tcp_flush_queue(tq);

  tcp_qprintf(tq,
	      "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
	      "<HTML><HEAD>\r\n"
	      "<TITLE>%d %s</TITLE>\r\n"
	      "</HEAD><BODY>\r\n"
	      "<H1>%d %s</H1>\r\n"
	      "</BODY></HTML>\r\n",
	      error, errtxt,  error, errtxt);

  hr->hr_destroy = NULL;
  hr->hr_rc = error;
  hr->hr_content = "text/html";
}

/**
 * Send an HTTP OK
 */
void
http_output(http_connection_t *hc, http_reply_t *hr, const char *content,
	    const char *encoding, int maxage)
{
  hr->hr_rc = HTTP_STATUS_OK;
  hr->hr_destroy = NULL;
  hr->hr_encoding = encoding;
  hr->hr_content = content;
  hr->hr_maxage = maxage;
}

/**
 * Send an HTTP OK, simple version for text/html
 */
void
http_output_html(http_connection_t *hc, http_reply_t *hr)
{
  hr->hr_rc = HTTP_STATUS_OK;
  hr->hr_destroy = NULL;
  hr->hr_content = "text/html; charset=UTF-8";
}



/**
 * Send an HTTP REDIRECT
 */
void
http_redirect(http_connection_t *hc, http_reply_t *hr, const char *location)
{
  tcp_queue_t *tq = &hr->hr_tq;

  tcp_qprintf(tq,
	      "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
	      "<HTML><HEAD>\r\n"
	      "<TITLE>Redirect</TITLE>\r\n"
	      "</HEAD><BODY>\r\n"
	      "Please follow <a href=\"%s\">%s</a>\r\n"
	      "</BODY></HTML>\r\n",
	      location, location);

  hr->hr_location = strdup(location);
  hr->hr_rc = HTTP_STATUS_FOUND;
  hr->hr_content = "text/html";
}


/**
 * Execute url callback
 *
 * Returns 1 if we should disconnect
 * 
 */
static int
http_exec(http_connection_t *hc, http_path_t *hp, char *remain, int err)
{
  http_reply_t *hr = calloc(1, sizeof(http_reply_t));

  /* Insert reply in order */
  TAILQ_INSERT_TAIL(&hc->hc_replies, hr, hr_link);
  
  tcp_init_queue(&hr->hr_tq, -1);
  hr->hr_connection = hc;
  hr->hr_version    = hc->hc_version;
  hr->hr_keep_alive = hc->hc_keep_alive;

  if(!err &&
     access_verify(hc->hc_username, hc->hc_password,
		   (struct sockaddr *)&hc->hc_tcp_session.tcp_peer_addr,
		   hp->hp_accessmask))
    err = HTTP_STATUS_UNAUTHORIZED;

  if(!err)
    err = hp->hp_callback(hc, hr, remain, hp->hp_opaque);

  if(err)
    http_error(hc, hr, err);

  if(hr->hr_destroy != NULL)
    return 0; /* New entry is delayed, do not transmit anything */

  return http_xmit_queue(hc);
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
  if(hp == NULL)
    return http_exec(hc, NULL, NULL, HTTP_STATUS_NOT_FOUND);

  if(args != NULL)
    http_parse_get_args(hc, args);

  return http_exec(hc, hp, remain, 0);
}


/**
 * Check if a HTTP POST is fully received, and if so, continue processing
 *
 * Return non-zero if we should disconnect
 */
static int
http_post_check(http_connection_t *hc)
{
  http_path_t *hp;
  char *remain, *args, *v, *argv[2];
  int n;

  if(hc->hc_post_ptr != hc->hc_post_len)
    return 0;

  hc->hc_state = HTTP_CON_WAIT_REQUEST;

  /* Parse content-type */
  v = http_arg_get(&hc->hc_args, "Content-Type");
  if(v == NULL)
    return http_exec(hc, NULL, NULL, HTTP_STATUS_BAD_REQUEST);

  n = http_tokenize(v, argv, 2, ';');
  if(n == 0)
    return http_exec(hc, NULL, NULL, HTTP_STATUS_BAD_REQUEST);

  if(!strcmp(argv[0], "application/x-www-form-urlencoded"))
    http_parse_get_args(hc, hc->hc_post_data);

  hp = http_resolve(hc, &remain, &args);
  if(hp == NULL)
    return http_exec(hc, NULL, NULL, HTTP_STATUS_NOT_FOUND);

  return http_exec(hc, hp, remain, 0);
}





/**
 * Initial processing of HTTP POST
 *
 * Return non-zero if we should disconnect
 */
static int
http_cmd_post(http_connection_t *hc)
{
  char *v;

  /* Set keep-alive status */
  v = http_arg_get(&hc->hc_args, "Content-Length");
  if(v == NULL)
    return 1; /* No content length in POST, make us disconnect */
  
  hc->hc_post_len = atoi(v);
  if(hc->hc_post_len > 16 * 1024 * 1024)
    return 1; /* Bail out if POST data > 16 Mb */

  hc->hc_state = HTTP_CON_POST_DATA;

  /* Allocate space for data, we add a terminating null char to ease
     string processing on the content */

  hc->hc_post_data = malloc(hc->hc_post_len + 1);
  hc->hc_post_data[hc->hc_post_len] = 0;

  hc->hc_post_ptr  = 0;

  /* We need to drain the line parser of any excess data */

  hc->hc_post_ptr = tcp_line_drain(&hc->hc_tcp_session, hc->hc_post_data,
				   hc->hc_post_len);
  return http_post_check(hc);
}


/**
 * Read POST data directly from socket
 */
static void
http_consume_post_data(http_connection_t *hc)
{
  tcp_session_t *tcp = &hc->hc_tcp_session;
  int togo = hc->hc_post_len - hc->hc_post_ptr;
  int r;

  r = read(tcp->tcp_fd, hc->hc_post_data + hc->hc_post_ptr, togo);
  if(r < 1) {
     tcp_disconnect(tcp, r == 0 ? ECONNRESET : errno);
     return;
  }

  hc->hc_post_ptr += r;
  if(http_post_check(hc))
    tcp_disconnect(tcp, 0);
}

/**
 * Process a HTTP request
 */
static int
http_process_request(http_connection_t *hc)
{
  switch(hc->hc_cmd) {
  default:
    return http_exec(hc, NULL, NULL, HTTP_STATUS_BAD_REQUEST);
  case HTTP_CMD_GET:
    return http_cmd_get(hc); 
  case HTTP_CMD_POST:
    return http_cmd_post(hc); 
  }
}

/**
 * Process a request, extract info from headers, dispatch command and
 * clean up
 */
static int
process_request(http_connection_t *hc)
{
  char *v, *argv[2];
  int n;
  uint8_t authbuf[150];
  
  /* Set keep-alive status */
  v = http_arg_get(&hc->hc_args, "connection");

  switch(hc->hc_version) {
  case RTSP_VERSION_1_0:
    hc->hc_keep_alive = 1;
    break;

  case HTTP_VERSION_0_9:
    hc->hc_keep_alive = 0;
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

  free(hc->hc_username);
  hc->hc_username = NULL;

  free(hc->hc_password);
  hc->hc_password = NULL;

  /* Extract authorization */
  if((v = http_arg_get(&hc->hc_args, "Authorization")) != NULL) {
    if((n = http_tokenize(v, argv, 2, -1)) == 2) {
      n = av_base64_decode(authbuf, argv[1], sizeof(authbuf) - 1);
      authbuf[n] = 0;
      if((n = http_tokenize((char *)authbuf, argv, 2, ':')) == 2) {
	hc->hc_username = strdup(argv[0]);
	hc->hc_password = strdup(argv[1]);
      }
    }
  }

  

  switch(hc->hc_version) {
  case RTSP_VERSION_1_0:
    rtsp_process_request(hc);
    return 0;

  case HTTP_VERSION_0_9:
  case HTTP_VERSION_1_0:
  case HTTP_VERSION_1_1:
    return http_process_request(hc) ? -1 : 0;
  }
  return -1;
}



/*
 * HTTP connection state machine & parser
 *
 * If we return non zero we will disconnect
 */
static int
http_con_parse(void *aux, char *buf)
{
  http_connection_t *hc = aux;
  int n, v;
  char *argv[3], *c;

  //printf("HTTP INPUT: %s\n", buf);

  switch(hc->hc_state) {
  case HTTP_CON_WAIT_REQUEST:
    if(hc->hc_post_data != NULL) {
      free(hc->hc_post_data);
      hc->hc_post_data = NULL;
    }

    http_arg_flush(&hc->hc_args);
    http_arg_flush(&hc->hc_req_args);
    if(hc->hc_url != NULL) {
      free(hc->hc_url);
      hc->hc_url = NULL;
    }

    n = http_tokenize(buf, argv, 3, -1);
    
    if(n < 2)
      return EBADRQC;

    hc->hc_cmd = str2val(argv[0], HTTP_cmdtab);
    hc->hc_url = strdup(argv[1]);


    if(n == 3) {
      v = str2val(argv[2], HTTP_versiontab);
      if(v == -1) 
	return EBADRQC;

      hc->hc_version = v;
      hc->hc_state = HTTP_CON_READ_HEADER;
    } else {
      hc->hc_version = HTTP_VERSION_0_9;
      return process_request(hc);
    }

    break;

  case HTTP_CON_READ_HEADER:
    if(*buf == 0) { 
      /* Empty crlf line, end of header lines */
      hc->hc_state = HTTP_CON_WAIT_REQUEST;
      return process_request(hc);
    }

    n = http_tokenize(buf, argv, 2, -1);
    if(n < 2)
      break;

    c = strrchr(argv[0], ':');
    if(c == NULL)
      break;
    *c = 0;
    http_arg_set(&hc->hc_args, argv[0], argv[1]);
    break;

  case HTTP_CON_POST_DATA:
    abort();

  case HTTP_CON_END:
    break;
  }
  return 0;
}


/*
 * disconnect
 */
static void
http_disconnect(http_connection_t *hc)
{
  http_reply_t *hr;

  while((hr = TAILQ_FIRST(&hc->hc_replies)) != NULL)
    http_destroy_reply(hc, hr);

  free(hc->hc_post_data);
  free(hc->hc_username);
  free(hc->hc_password);

  rtsp_disconncet(hc);
  http_arg_flush(&hc->hc_args);
  http_arg_flush(&hc->hc_req_args);
  free(hc->hc_url);
}


/*
 *
 */
static void
http_tcp_callback(tcpevent_t event, void *tcpsession)
{
  http_connection_t *hc = tcpsession;

  switch(event) {
  case TCP_CONNECT:
    TAILQ_INIT(&hc->hc_replies);
    TAILQ_INIT(&hc->hc_args);
    TAILQ_INIT(&hc->hc_req_args);
    break;

  case TCP_DISCONNECT:
    http_disconnect(hc);
    break;

  case TCP_INPUT:

    if(hc->hc_state == HTTP_CON_POST_DATA)
      http_consume_post_data(hc);
    else
      tcp_line_read(&hc->hc_tcp_session, http_con_parse);
    break;
  }
}


/*
 *  Fire up HTTP server
 */

void
http_start(int port)
{
  rtsp_init();

  http_port = port;
  tcp_create_server(port, sizeof(http_connection_t), "http",
		    http_tcp_callback);
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
static void
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
 * HTTP embedded resource
 */
typedef struct http_resource {
  const void *data;
  size_t len;
  const char *content;
  const char *encoding;
} http_resource_t;

static int
deliver_resource(http_connection_t *hc, http_reply_t *hr, 
		 const char *remain, void *opaque)
{
  http_resource_t *hres = opaque;

  tcp_qput(&hr->hr_tq, hres->data, hres->len);
  http_output(hc, hr, hres->content, hres->encoding, 15);
  return 0;
}

void
http_resource_add(const char *path, const void *ptr, size_t len, 
		  const char *content, const char *encoding)
{
  http_resource_t *hres = malloc(sizeof(http_resource_t));

  hres->data     = ptr;
  hres->len      = len;
  hres->content  = content;
  hres->encoding = encoding;

  http_path_add(path, hres, deliver_resource, ACCESS_WEB_INTERFACE);
}
