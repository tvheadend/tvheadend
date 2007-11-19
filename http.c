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

#include <ffmpeg/base64.h>

#include "tvhead.h"
#include "channels.h"
#include "subscriptions.h"
#include "epg.h"
#include "teletext.h"
#include "dispatch.h"
#include "dvb.h"
#include "strtab.h"
#include "rtp.h"
#include "tsmux.h"
#include "http.h"
#include "rtsp.h"

static LIST_HEAD(, http_path) http_paths;

static struct strtab HTTP_cmdtab[] = {
  { "GET",        HTTP_CMD_GET },
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
  { "RTSP/1.0",        RTSP_VERSION_1_0 },
};


/**
 *
 */
static http_path_t *
http_resolve(http_connection_t *hc, char **remainp)
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

  if(*v != 0 && *v != '/')
    return NULL;

  if(*v == '/')
    v++;

  *remainp = v;
  return hp;
}



/*
 * HTTP status code to string
 */

#define HTTP_STATUS_OK           200
#define HTTP_STATUS_BAD_REQUEST  400
#define HTTP_STATUS_UNAUTHORIZED 401
#define HTTP_STATUS_NOT_FOUND    404

static const char *
http_rc2str(int code)
{
  switch(code) {
  case HTTP_STATUS_OK:              return "OK";
  case HTTP_STATUS_NOT_FOUND:       return "Not found";
  case HTTP_STATUS_UNAUTHORIZED:    return "Unauthorized";
  case HTTP_STATUS_BAD_REQUEST:     return "Bad request";
  default:
    return "Unknown returncode";
    break;
  }
}

/**
 * If current version mandates it, send a HTTP reply header back
 */
void
http_output_reply_header(http_connection_t *hc, int rc)
{
  if(hc->hc_version < HTTP_VERSION_1_0)
    return;
  
  http_printf(hc, "%s %d %s\r\n", val2str(hc->hc_version, HTTP_versiontab),
	      rc, http_rc2str(rc));
  http_printf(hc, "Server: HTS/tvheadend\r\n");
  http_printf(hc, "Connection: %s\r\n", 
	      hc->hc_keep_alive ? "Keep-Alive" : "Close");
}


/**
 * Send HTTP error back
 */
void
http_error(http_connection_t *hc, int error)
{
  char ret[300];
  const char *errtxt = http_rc2str(error);

  http_output_reply_header(hc, error);

  snprintf(ret, sizeof(ret),
	   "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
	   "<HTML><HEAD>\r\n"
	   "<TITLE>%d %s</TITLE>\r\n"
	   "</HEAD><BODY>\r\n"
	   "<H1>%d %s</H1>\r\n"
	   "</BODY></HTML>\r\n",
	   error, errtxt,
	   error, errtxt);

  if(hc->hc_version >= HTTP_VERSION_1_0) {
    if(error == HTTP_STATUS_UNAUTHORIZED)
      http_printf(hc, "WWW-Authenticate: Basic realm=\"tvheadend\"\r\n");

    http_printf(hc, "Content-Type: text/html\r\n");
    http_printf(hc, "Content-Length: %d\r\n", strlen(ret));
    http_printf(hc, "\r\n");
  }
  http_printf(hc, "%s", ret);
}

/**
 * Send an HTTP OK and post data from a tcp queue
 */
void
http_output_queue(http_connection_t *hc, tcp_queue_t *tq, const char *content)
{
  http_output_reply_header(hc, 200);

  if(hc->hc_version >= HTTP_VERSION_1_0) {
    http_printf(hc, 
		"Content-Type: %s\r\n"
		"Content-Length: %d\r\n"
		"\r\n",
		content, tq->tq_depth);
  }

  tcp_output_queue(&hc->hc_tcp_session, NULL, tq);
}

/**
 * HTTP GET
 */
static void
http_cmd_get(http_connection_t *hc)
{
  http_path_t *hp;
  char *remain;
  int err;

  hp = http_resolve(hc, &remain);
  if(hp == NULL) {
    http_error(hc, HTTP_STATUS_NOT_FOUND);
    return;
  }

  err = hp->hp_callback(hc, remain, hp->hp_opaque);
  if(err)
    http_error(hc, err);
}


/**
 * Process a HTTP request
 */
static void
http_process_request(http_connection_t *hc)
{
  switch(hc->hc_cmd) {
  default:
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    break;
  case HTTP_CMD_GET:
    http_cmd_get(hc); 
    break;
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
    break;

  case HTTP_VERSION_0_9:
  case HTTP_VERSION_1_0:
  case HTTP_VERSION_1_1:
    http_process_request(hc);
    break;
  }


  /* Clean up */

  free(hc->hc_username);
  hc->hc_username = NULL;

  free(hc->hc_password);
  hc->hc_password = NULL;

  if(hc->hc_keep_alive == 0)
    return -1;
  return 0;
}



/*
 * HTTP connection state machine & parser
 */
static int
http_con_parse(void *aux, char *buf)
{
  http_connection_t *hc = aux;
  int n, v;
  char *argv[3], *c;

  printf("HTTP INPUT: %s\n", buf);

  switch(hc->hc_state) {
  case HTTP_CON_WAIT_REQUEST:
    http_arg_flush(&hc->hc_args);
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
  rtsp_disconncet(hc);
  http_arg_flush(&hc->hc_args);
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
    break;

  case TCP_DISCONNECT:
    http_disconnect(hc);
    break;

  case TCP_INPUT:
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
  while((ra = LIST_FIRST(list)) != NULL) {
    LIST_REMOVE(ra, link);
    free(ra->key);
    free(ra->val);
    free(ra);
  }
}


/**
 * Find an argument associated with a connection
 */
char *
http_arg_get(struct http_arg_list *list, char *name)
{
  http_arg_t *ra;
  LIST_FOREACH(ra, list, link)
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

  LIST_FOREACH(ra, list, link)
    if(!strcasecmp(ra->key, key))
      break;

  if(ra == NULL) {
    ra = malloc(sizeof(http_arg_t));
    LIST_INSERT_HEAD(list, ra, link);
    ra->key = strdup(key);
  } else {
    free(ra->val);
  }
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
http_path_add(const char *path, void *opaque, http_callback_t *callback)
{
  http_path_t *hp = malloc(sizeof(http_path_t));

  hp->hp_len      = strlen(path);
  hp->hp_path     = strdup(path);
  hp->hp_opaque   = opaque;
  hp->hp_callback = callback;
  LIST_INSERT_HEAD(&http_paths, hp, hp_link);
  return hp;
}

