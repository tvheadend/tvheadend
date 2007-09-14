/*
 *  tvheadend, RTSP interface
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

#include "tvhead.h"
#include "channels.h"
#include "transports.h"
#include "pvr.h"
#include "epg.h"
#include "teletext.h"
#include "dispatch.h"
#include "dvb.h"
#include "strtab.h"
#include "rtp.h"

#include <ffmpeg/avformat.h>
#include <ffmpeg/rtspcodes.h>
#include <ffmpeg/random.h>

static AVRandomState rtsp_rnd;

#define RTSP_MAX_LINE_LEN 1000

LIST_HEAD(rtsp_session_head, rtsp_session);
static struct rtsp_session_head rtsp_sessions;


typedef struct rtsp_session {
  LIST_ENTRY(rtsp_session) rs_global_link;
  LIST_ENTRY(rtsp_session) rs_con_link;

  uint32_t rs_id;

  int rs_fd[2];
  int rs_server_port[2];

  th_subscription_t *rs_s;

  th_rtp_streamer_t rs_rtp_streamer;

} rtsp_session_t;





typedef struct rtsp_arg {
  LIST_ENTRY(rtsp_arg) link;
  char *key;
  char *val;
} rtsp_arg_t;



typedef struct rtsp_connection {
  int rc_fd;
  void *rc_dispatch_handle;
  char *rc_url;

  LIST_HEAD(, rtsp_arg) rc_args;

  enum {
    RTSP_CON_WAIT_REQUEST,
    RTSP_CON_READ_HEADER,
    RTSP_CON_END,
  } rc_state;

  enum {
    RTSP_CMD_NONE,
    RTSP_CMD_DESCRIBE,
    RTSP_CMD_OPTIONS,
    RTSP_CMD_SETUP,
    RTSP_CMD_PLAY,

  } rc_cmd;

  struct rtsp_session_head rc_sessions;

  int rc_input_buf_ptr;
  char rc_input_buf[RTSP_MAX_LINE_LEN];
  struct sockaddr_in rc_from;

} rtsp_connection_t;


static struct strtab RTSP_cmdtab[] = {
  { "DESCRIBE",   RTSP_CMD_DESCRIBE },
  { "OPTIONS",    RTSP_CMD_OPTIONS },
  { "SETUP",      RTSP_CMD_SETUP },
  { "PLAY",       RTSP_CMD_PLAY },
};

/**
 * Resolve an URL into a channel
 */

static th_channel_t *
rtsp_channel_by_url(char *url)
{
  char *c;
  int chid;

  c = strrchr(url, '/');
  if(c != NULL && c[1] == 0) {
    /* remove trailing slash */
    *c = 0;
    c = strrchr(url, '/');
  }

  if(c == NULL || c[1] == 0 || (url != c && c[-1] == '/'))
    return NULL;
  c++; 

  printf("URL RESOLVER: %s\n", c);
  

  if(sscanf(c, "chid-%d", &chid) != 1)
    return NULL;
  printf("\t\t\t == %d\n", chid);

  return channel_by_index(chid);
}

/**
 * Create an RTSP session
 */

static rtsp_session_t *
rtsp_session_create(th_channel_t *ch, struct sockaddr_in *dst)
{
  rtsp_session_t *rs;
  uint32_t id;
  struct sockaddr_in sin;
  socklen_t slen;
  int max_tries = 100;

  /* generate a random id (but make sure we do not collide) */
  
  do {
    id = av_random(&rtsp_rnd);
    id &= 0x7fffffffUL; /* dont want any signed issues */

    LIST_FOREACH(rs, &rtsp_sessions, rs_global_link)
      if(rs->rs_id == id)
	break;
  } while(rs != NULL);

  rs = calloc(1, sizeof(rtsp_session_t));
  rs->rs_id = id;

  while(--max_tries) {
    rs->rs_fd[0] = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    
    if(bind(rs->rs_fd[0], (struct sockaddr *)&sin, sizeof(sin)) == -1) {
      close(rs->rs_fd[0]);
      free(rs);
      return NULL;
    }

    slen = sizeof(struct sockaddr_in);
    getsockname(rs->rs_fd[0], (struct sockaddr *)&sin, &slen);

    rs->rs_server_port[0] = ntohs(sin.sin_port);
    printf("rtpserver: bound to port %d\n", rs->rs_server_port[0]);
    rs->rs_server_port[1] = rs->rs_server_port[0] + 1;
    
    sin.sin_port = htons(rs->rs_server_port[1]);

    rs->rs_fd[1] = socket(AF_INET, SOCK_DGRAM, 0);
    
    if(bind(rs->rs_fd[1], (struct sockaddr *)&sin, sizeof(sin)) == -1) {
      close(rs->rs_fd[0]);
      close(rs->rs_fd[1]);
      continue;
    }
    
    printf("bound server_port %d-%d\n", 
	   rs->rs_server_port[0], rs->rs_server_port[1]);
    LIST_INSERT_HEAD(&rtsp_sessions, rs, rs_global_link);

    rtp_streamer_init(&rs->rs_rtp_streamer, rs->rs_fd[0], dst);

    rs->rs_s = channel_subscribe(ch, &rs->rs_rtp_streamer, 
				 rtp_streamer, 600, "RTSP");
    return rs;
  }

  free(rs);
  return NULL;
}

/**
 * Destroy an RTSP session
 */

static void
rtsp_session_destroy(rtsp_session_t *rs)
{
  subscription_unsubscribe(rs->rs_s);
  close(rs->rs_fd[0]);
  close(rs->rs_fd[1]);
  LIST_REMOVE(rs, rs_global_link);
  LIST_REMOVE(rs, rs_con_link);
  rtp_streamer_deinit(&rs->rs_rtp_streamer);

  free(rs);
}

/*
 *  Prints data on rtsp connection
 */

static void
rcprintf(rtsp_connection_t *rc, const char *fmt, ...)
{
  va_list ap;
  char buf[5000];

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  write(rc->rc_fd, buf, strlen(buf));
}


/*
 * Delete all arguments associated with an RTSP connection
 */

static void
rtsp_con_flush_args(rtsp_connection_t *rc)
{
  rtsp_arg_t *ra;
  while((ra = LIST_FIRST(&rc->rc_args)) != NULL) {
    LIST_REMOVE(ra, link);
    free(ra->key);
    free(ra->val);
    free(ra);
  }
}


/**
 * Find an argument associated with an RTSP connection
 */

static char *
rtsp_con_get_arg(rtsp_connection_t *rc, char *name)
{
  rtsp_arg_t *ra;
  LIST_FOREACH(ra, &rc->rc_args, link)
    if(!strcasecmp(ra->key, name))
      return ra->val;
  return NULL;
}


/**
 * Set an argument associated with an RTSP connection
 */

static void
rtsp_con_set_arg(rtsp_connection_t *rc, char *key, char *val)
{
  rtsp_arg_t *ra;
  LIST_FOREACH(ra, &rc->rc_args, link)
    if(!strcasecmp(ra->key, key))
      break;

  if(ra == NULL) {
    ra = malloc(sizeof(rtsp_arg_t));
    LIST_INSERT_HEAD(&rc->rc_args, ra, link);
    ra->key = strdup(key);
  } else {
    free(ra->val);
  }
  ra->val = strdup(val);
}


/*
 *
 */

static int
tokenize(char *buf, char **vec, int vecsize, int delimiter)
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

/*
 * RTSP return code to string
 */

static const char *
rtsp_err2str(int err)
{
  switch(err) {
  case RTSP_STATUS_OK:              return "OK";
  case RTSP_STATUS_METHOD:          return "Method Not Allowed";
  case RTSP_STATUS_BANDWIDTH:       return "Not Enough Bandwidth";
  case RTSP_STATUS_SESSION:         return "Session Not Found";
  case RTSP_STATUS_STATE:           return "Method Not Valid in This State";
  case RTSP_STATUS_AGGREGATE:       return "Aggregate operation not allowed";
  case RTSP_STATUS_ONLY_AGGREGATE:  return "Only aggregate operation allowed";
  case RTSP_STATUS_TRANSPORT:       return "Unsupported transport";
  case RTSP_STATUS_INTERNAL:        return "Internal Server Error";
  case RTSP_STATUS_SERVICE:         return "Service Unavailable";
  case RTSP_STATUS_VERSION:         return "RTSP Version not supported";
  default:
    return "Unknown Error";
    break;
  }
}

/*
 *
 */
static int
rtsp_reply_error(rtsp_connection_t *rc, int error)
{
  char *c;

  syslog(LOG_NOTICE, "RTSP error %d %s", error, rtsp_err2str(error));

  rcprintf(rc, "RTSP/1.0 %d %s\r\n", error, rtsp_err2str(error));
  if((c = rtsp_con_get_arg(rc, "cseq")) != NULL)
    rcprintf(rc, "CSeq: %s\r\n", c);
  rcprintf(rc, "\r\n");
  return ECONNRESET;
}



/*
 * RTSP PLAY
 */

static int
rtsp_cmd_play(rtsp_connection_t *rc)
{
  char *ses, *c;
  int sesid;
  rtsp_session_t *rs;
  th_channel_t *ch;

  if((ch = rtsp_channel_by_url(rc->rc_url)) == NULL)
    return rtsp_reply_error(rc, RTSP_STATUS_SERVICE);

  printf(">>> PLAY\n");

  if((ses = rtsp_con_get_arg(rc, "session:")) == NULL) 
    return rtsp_reply_error(rc, RTSP_STATUS_SESSION);

  sesid = atoi(ses);
  printf("\t\tsesid = %u\n", sesid);
  LIST_FOREACH(rs, &rtsp_sessions, rs_global_link)
    if(rs->rs_id == sesid)
      break;
  
  if(rs == NULL)
    return rtsp_reply_error(rc, RTSP_STATUS_SESSION);

  rcprintf(rc,
	   "RTSP/1.0 200 OK\r\n"
	   "Session: %u\r\n",
	   rs->rs_id);

  if((c = rtsp_con_get_arg(rc, "cseq")) != NULL)
    rcprintf(rc, "CSeq: %s\r\n", c);
  
  rcprintf(rc, "\r\n");

  return 0;
}

/*
 * RTSP SETUP
 */

static int
rtsp_cmd_setup(rtsp_connection_t *rc)
{
  char *transports[10];
  char *params[10];
  char *avp[2];
  char *ports[2];
  char *t, *c;
  int nt, i, np, j, navp, nports, ismulticast;
  int client_ports[2];
  rtsp_session_t *rs;
  th_channel_t *ch;
  struct sockaddr_in dst;

  if((ch = rtsp_channel_by_url(rc->rc_url)) == NULL)
    return rtsp_reply_error(rc, RTSP_STATUS_SERVICE);

  client_ports[0] = 0;
  client_ports[1] = 0;

  printf(">>> SETUP\n");
  if((t = rtsp_con_get_arg(rc, "transport:")) == NULL) 
    return rtsp_reply_error(rc, RTSP_STATUS_TRANSPORT);

  nt = tokenize(t, transports, 10, ',');
  
  /* Select a transport we can accept */

  for(i = 0; i < nt; i++) {
    np = tokenize(transports[i], params, 10, ';');

    if(np == 0)
      continue;
    if(strcasecmp(params[0], "RTP/AVP/UDP") &&
       strcasecmp(params[0], "RTP/AVP"))
      continue;

    ismulticast = 1; /* multicast is default according to RFC */
    client_ports[0] = 0;
    client_ports[1] = 0;


    for(j = 1; j < np; j++) {
      if((navp = tokenize(params[j], avp, 2, '=')) == 0)
	continue;

      printf("%s = %s\n", avp[0], navp == 2 ? avp[1] : "");

      if(navp == 1 && !strcmp(avp[0], "unicast")) {
	ismulticast = 0;
      } else if(navp == 2 && !strcmp(avp[0], "client_port")) {
	nports = tokenize(avp[1], ports, 2, '-');
	if(nports > 0) client_ports[0] = atoi(ports[0]);
	if(nports > 1) client_ports[1] = atoi(ports[1]);
      }
    }
    printf("\t%d %d %d\n",
	   ismulticast, client_ports[0], client_ports[1]);

    if(!ismulticast && client_ports[0] && client_ports[1])
      break;
  }

  if(i == nt) /* couldnt find a suitable transport */ {
    printf(">>> SETUP; no transport\n");
    return rtsp_reply_error(rc, RTSP_STATUS_TRANSPORT);
  }

  dst = rc->rc_from;
  dst.sin_port = htons(client_ports[0]);

  if((rs = rtsp_session_create(ch, &dst)) == NULL)
    return rtsp_reply_error(rc, RTSP_STATUS_INTERNAL);

  LIST_INSERT_HEAD(&rc->rc_sessions, rs, rs_con_link);

  printf(">>> SETUP, rending reply\n");

  rcprintf(rc,
	   "RTSP/1.0 200 OK\r\n"
	   "Session: %u\r\n"
	   "Transport: RTP/AVP/UDP;unicast;client_port=%d-%d;"
	   "server_port=%d-%d\r\n",
	   rs->rs_id,
	   client_ports[0],
	   client_ports[1],
	   rs->rs_server_port[0],
	   rs->rs_server_port[1]);

  if((c = rtsp_con_get_arg(rc, "cseq")) != NULL)
    rcprintf(rc, "CSeq: %s\r\n", c);
  
  rcprintf(rc, "\r\n");
  return 0;
}



/*
 * RTSP DESCRIBE
 */

static int
rtsp_cmd_describe(rtsp_connection_t *rc)
{
  char sdpreply[1000];
  th_channel_t *ch;
  char *c;

  if((ch = rtsp_channel_by_url(rc->rc_url)) == NULL)
    return rtsp_reply_error(rc, RTSP_STATUS_SERVICE);

  snprintf(sdpreply, sizeof(sdpreply),
	   "v=0\r\n"
	   "o=- 0 0 IN IPV4 127.0.0.1\r\n"
	   "s=%s\r\n"
	   "a=tool:hts tvheadend\r\n"
	   "m=video 0 RTP/AVP 33\r\n",
	   ch->ch_name);

  printf("sdpreply = %s\n", sdpreply);

  rcprintf(rc,
	   "RTSP/1.0 200 OK\r\n"
	   "Content-Type: application/sdp\r\n"
	   "Content-Length: %d\r\n",
	   strlen(sdpreply));

  if((c = rtsp_con_get_arg(rc, "cseq")) != NULL)
    rcprintf(rc, "CSeq: %s\r\n", c);
  
  rcprintf(rc, "\r\n%s", sdpreply);
  return 0;
}


/*
 * RTSP OPTIONS
 */

static int
rtsp_cmd_options(rtsp_connection_t *rc)
{
  char *c;

  if(strcmp(rc->rc_url, "*") && rtsp_channel_by_url(rc->rc_url) == NULL)
    return rtsp_reply_error(rc, RTSP_STATUS_SERVICE);

  rcprintf(rc,
	   "RTSP/1.0 200 OK\r\n"
	   "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n");

  if((c = rtsp_con_get_arg(rc, "cseq")) != NULL)
    rcprintf(rc, "CSeq: %s\r\n", c);
  rcprintf(rc, "\r\n");
  return 0;
}


/*
 * RTSP connection state machine & parser
 */
static int
rtsp_con_parse(rtsp_connection_t *rc, char *buf)
{
  int n;
  char *argv[3];

  switch(rc->rc_state) {
  case RTSP_CON_WAIT_REQUEST:
    rtsp_con_flush_args(rc);
    if(rc->rc_url != NULL) {
      free(rc->rc_url);
      rc->rc_url = NULL;
    }

    printf(">>>> %s\n", buf);
    n = tokenize(buf, argv, 3, -1);
    
    if(n < 3)
      return EBADRQC;

    if(strcmp(argv[2], "RTSP/1.0")) {
      rtsp_reply_error(rc, RTSP_STATUS_VERSION);
      return ECONNRESET;
    }
    rc->rc_cmd = str2val(argv[0], RTSP_cmdtab);
    rc->rc_url = strdup(argv[1]);
    rc->rc_state = RTSP_CON_READ_HEADER;
    break;

  case RTSP_CON_READ_HEADER:
    if(*buf == 0) {
      rc->rc_state = RTSP_CON_WAIT_REQUEST;
      printf("cmd = %d\n", rc->rc_cmd);
      switch(rc->rc_cmd) {
      default:
	return rtsp_reply_error(rc, RTSP_STATUS_METHOD);
      case RTSP_CMD_DESCRIBE:
	return rtsp_cmd_describe(rc);
      case RTSP_CMD_SETUP:
	return rtsp_cmd_setup(rc);
      case RTSP_CMD_PLAY:
	return rtsp_cmd_play(rc);
      case RTSP_CMD_OPTIONS:
	return rtsp_cmd_options(rc);

      }
      break;
    }

    n = tokenize(buf, argv, 2, -1);
    if(n < 2)
      break;

    printf("'%s' '%s'\n", argv[0], argv[1]);

    rtsp_con_set_arg(rc, argv[0], argv[1]);
    break;

  case RTSP_CON_END:
    break;
  }
  return 0;
}


/*
 * client error, teardown connection
 */
static void
rtsp_con_teardown(rtsp_connection_t *rc, int err)
{
  rtsp_session_t *rs;
  syslog(LOG_INFO, "RTSP disconnected -- %s", strerror(err));

  while((rs = LIST_FIRST(&rc->rc_sessions)) != NULL)
    rtsp_session_destroy(rs);

  close(dispatch_delfd(rc->rc_dispatch_handle));

  free(rc->rc_url);
  rtsp_con_flush_args(rc);
  free(rc);
}

/*
 * data available on socket
 */
static void
rtsp_con_data_read(rtsp_connection_t *rc)
{
  int space = sizeof(rc->rc_input_buf) - rc->rc_input_buf_ptr - 1;
  int r, cr = 0, i, err;
  char buf[RTSP_MAX_LINE_LEN];

  if(space < 1) {
    rtsp_con_teardown(rc, EBADMSG);
    return;
  }

  r = read(rc->rc_fd, rc->rc_input_buf + rc->rc_input_buf_ptr, space);
  if(r < 0) {
    rtsp_con_teardown(rc, errno);
    return;
  }

  if(r == 0) {
    rtsp_con_teardown(rc, ECONNRESET);
    return;
  }

  rc->rc_input_buf_ptr += r;
  rc->rc_input_buf[rc->rc_input_buf_ptr] = 0;

  while(1) {
    cr = 0;

    for(i = 0; i < rc->rc_input_buf_ptr; i++)
      if(rc->rc_input_buf[i] == 0xa)
	break;

    if(i == rc->rc_input_buf_ptr)
      break;

    memcpy(buf, rc->rc_input_buf, i);
    buf[i] = 0;
    i++;
    memmove(rc->rc_input_buf, rc->rc_input_buf + i, 
	    sizeof(rc->rc_input_buf) - i);
    rc->rc_input_buf_ptr -= i;

    i = strlen(buf);
    while(i > 0 && buf[i-1] < 32)
      buf[--i] = 0;

    if((err = rtsp_con_parse(rc, buf)) != 0) {
      rtsp_con_teardown(rc, err);
      break;
    }
  }
}


/*
 * dispatcher callback
 */
static void
rtsp_con_socket_callback(int events, void *opaque, int fd)
{
  rtsp_connection_t *rc = opaque;

  if(events & DISPATCH_ERR) {
    rtsp_con_teardown(rc, ECONNRESET);
    return;
  }

  if(events & DISPATCH_READ)
    rtsp_con_data_read(rc);
}



/*
 *
 */
static void
rtsp_connect_callback(int events, void *opaque, int fd)
{
  struct sockaddr_in from;
  socklen_t socklen = sizeof(struct sockaddr_in);
  int newfd;
  int val;
  rtsp_connection_t *rc;
  char txt[30];

  if(!(events & DISPATCH_READ))
    return;

  newfd = accept(fd, (struct sockaddr *)&from, &socklen);
  if(newfd == -1)
    return;
  
  fcntl(newfd, F_SETFL, fcntl(newfd, F_GETFL) | O_NONBLOCK);

  val = 1;
  setsockopt(newfd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val));
  
  val = 30;
  setsockopt(newfd, SOL_TCP, TCP_KEEPIDLE, &val, sizeof(val));

  val = 15;
  setsockopt(newfd, SOL_TCP, TCP_KEEPINTVL, &val, sizeof(val));

  val = 5;
  setsockopt(newfd, SOL_TCP, TCP_KEEPCNT, &val, sizeof(val));

  val = 1;
  setsockopt(newfd, SOL_TCP, TCP_NODELAY, &val, sizeof(val));

  rc = calloc(1, sizeof(rtsp_connection_t));
  rc->rc_fd = newfd;

  snprintf(txt, sizeof(txt), "%s:%d",
	   inet_ntoa(from.sin_addr), ntohs(from.sin_port));

  syslog(LOG_INFO, "Got RTSP/TCP connection from %s", txt);

  rc->rc_from = from;

  rc->rc_dispatch_handle = dispatch_addfd(newfd, rtsp_con_socket_callback, rc,
					  DISPATCH_READ);
}



/*
 *  Fire up RTSP server
 */

void
rtsp_start(void)
{
  struct sockaddr_in sin;
  int s;
  int one = 1;
  s = socket(AF_INET, SOCK_STREAM, 0);
  memset(&sin, 0, sizeof(sin));

  sin.sin_family = AF_INET;
  sin.sin_port = htons(9908);

  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

  fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);

  syslog(LOG_INFO, "Listening for RTSP/TCP connections on %s:%d",
	 inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

  if(bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    syslog(LOG_ERR, "Unable to bind socket for incomming RTSP/TCP connections"
	   "%s:%d -- %s",
	   inet_ntoa(sin.sin_addr), ntohs(sin.sin_port),
	   strerror(errno));
    return;
  }

  av_init_random(time(NULL), &rtsp_rnd);

  listen(s, 1);
  dispatch_addfd(s, rtsp_connect_callback, NULL, DISPATCH_READ);
}
