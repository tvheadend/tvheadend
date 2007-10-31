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
#include "tcp.h"

#include <ffmpeg/avformat.h>
#include <ffmpeg/rtspcodes.h>
#include <ffmpeg/random.h>

#define rcprintf(c, fmt...) tcp_printf(&(rc)->rc_tcp_session, fmt)

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

  void *rs_muxer;

} rtsp_session_t;





typedef struct rtsp_arg {
  LIST_ENTRY(rtsp_arg) link;
  char *key;
  char *val;
} rtsp_arg_t;



typedef struct rtsp_connection {
  tcp_session_t rc_tcp_session; /* Must be first */
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
    RTSP_CMD_TEARDOWN,
    RTSP_CMD_PLAY,
    RTSP_CMD_PAUSE,

  } rc_cmd;

  struct rtsp_session_head rc_sessions;
} rtsp_connection_t;


static struct strtab RTSP_cmdtab[] = {
  { "DESCRIBE",   RTSP_CMD_DESCRIBE },
  { "OPTIONS",    RTSP_CMD_OPTIONS },
  { "SETUP",      RTSP_CMD_SETUP },
  { "PLAY",       RTSP_CMD_PLAY },
  { "TEARDOWN",   RTSP_CMD_TEARDOWN },
  { "PAUSE",      RTSP_CMD_PAUSE },
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

  if(sscanf(c, "chid-%d", &chid) != 1)
    return NULL;
  return channel_by_index(chid);
}

/*
 * Called when a subscription gets/loses access to a transport
 */
static void
rtsp_subscription_callback(struct th_subscription *s,
			   subscription_event_t event, void *opaque)
{
  rtsp_session_t *rs = opaque;

  switch(event) {
  case TRANSPORT_AVAILABLE:
    assert(rs->rs_muxer == NULL);
    rs->rs_muxer = ts_muxer_init(s, rtp_output_ts, &rs->rs_rtp_streamer, 0);
    break;

  case TRANSPORT_UNAVAILABLE:
    assert(rs->rs_muxer != NULL);
    ts_muxer_deinit(rs->rs_muxer);
    rs->rs_muxer = NULL;
    break;
  }
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
    rs->rs_server_port[1] = rs->rs_server_port[0] + 1;
    
    sin.sin_port = htons(rs->rs_server_port[1]);

    rs->rs_fd[1] = socket(AF_INET, SOCK_DGRAM, 0);
    
    if(bind(rs->rs_fd[1], (struct sockaddr *)&sin, sizeof(sin)) == -1) {
      close(rs->rs_fd[0]);
      close(rs->rs_fd[1]);
      continue;
    }
    
    LIST_INSERT_HEAD(&rtsp_sessions, rs, rs_global_link);

    rs->rs_s = subscription_create(ch, 600, "RTSP",
				   rtsp_subscription_callback, rs);

    /* Initialize RTP */
    
    rtp_streamer_init(&rs->rs_rtp_streamer, rs->rs_fd[0], dst);
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
  subscription_unsubscribe(rs->rs_s); /* will call subscription_callback
					 with TRANSPORT_UNAVAILABLE if
					 we are hooked on a transport */
  close(rs->rs_fd[0]);
  close(rs->rs_fd[1]);
  LIST_REMOVE(rs, rs_global_link);
  LIST_REMOVE(rs, rs_con_link);

  free(rs);
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
#if 0  
  if(!strcasecmp(key, "User-Agent")) {
    free(rc->rc_logname);

    snprintf(buf, sizeof(buf), "%s:%d [%s]",
	     inet_ntoa(rc->rc_from.sin_addr), ntohs(rc->rc_from.sin_port),
	     val);
    rc->rc_logname = strdup(buf);
  }
#endif
}


/*
 * Split a string in components delimited by 'delimiter'
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
 * Return an error
 */
static void
rtsp_reply_error(rtsp_connection_t *rc, int error, const char *errstr)
{
  char *c;

  if(errstr == NULL)
    errstr = rtsp_err2str(error);

  syslog(LOG_INFO, "rtsp: %s: %s", tcp_logname(&rc->rc_tcp_session), errstr);

  rcprintf(rc, "RTSP/1.0 %d %s\r\n", error, errstr);
  if((c = rtsp_con_get_arg(rc, "cseq")) != NULL)
    rcprintf(rc, "CSeq: %s\r\n", c);
  rcprintf(rc, "\r\n");
}

/*
 * Find a session pointed do by the current connection
 */
static rtsp_session_t *
rtsp_get_session(rtsp_connection_t *rc)
{
  char *ses;
  int sesid;
  rtsp_session_t *rs;
  th_channel_t *ch;

  if((ch = rtsp_channel_by_url(rc->rc_url)) == NULL) {
    rtsp_reply_error(rc, RTSP_STATUS_SERVICE, "URL does not resolve");
    return NULL;
  }


  if((ses = rtsp_con_get_arg(rc, "session")) == NULL) {
    rtsp_reply_error(rc, RTSP_STATUS_SESSION, NULL);
    return NULL;
  }

  sesid = atoi(ses);
  LIST_FOREACH(rs, &rtsp_sessions, rs_global_link)
    if(rs->rs_id == sesid)
      break;
  
  if(rs == NULL)
    rtsp_reply_error(rc, RTSP_STATUS_SESSION, NULL);
  
  return rs;
}


/*
 * RTSP PLAY
 */

static void
rtsp_cmd_play(rtsp_connection_t *rc)
{
  char *c;
  int64_t start;
  rtsp_session_t *rs;
 
  rs = rtsp_get_session(rc);
  if(rs == NULL)
    return;

  if((c = rtsp_con_get_arg(rc, "range")) != NULL) {
    start = AV_NOPTS_VALUE;
  } else {
    start = AV_NOPTS_VALUE;
  }

  if(rs->rs_muxer == NULL) {
    rtsp_reply_error(rc, RTSP_STATUS_SERVICE, 
		     "No muxer attached (missing SETUP ?)");
    return;
  }

  ts_muxer_play(rs->rs_muxer, start);

  syslog(LOG_INFO, "rtsp: %s: Starting playback of %s",
	 tcp_logname(&rc->rc_tcp_session), rs->rs_s->ths_channel->ch_name);

  rcprintf(rc,
	   "RTSP/1.0 200 OK\r\n"
	   "Session: %u\r\n",
	   rs->rs_id);

  if((c = rtsp_con_get_arg(rc, "cseq")) != NULL)
    rcprintf(rc, "CSeq: %s\r\n", c);
  
  rcprintf(rc, "\r\n");
}

/*
 * RTSP PAUSE
 */

static void
rtsp_cmd_pause(rtsp_connection_t *rc)
{
  char *c;
  rtsp_session_t *rs;
  
  rs = rtsp_get_session(rc);
  if(rs == NULL)
    return;

  if(rs->rs_muxer == NULL) {
    rtsp_reply_error(rc, RTSP_STATUS_SERVICE,
		     "No muxer attached (missing SETUP ?)");
    return;
  }

  ts_muxer_pause(rs->rs_muxer);

  syslog(LOG_INFO, "rtsp: %s: Pausing playback of %s",
	 tcp_logname(&rc->rc_tcp_session), rs->rs_s->ths_channel->ch_name);

  rcprintf(rc,
	   "RTSP/1.0 200 OK\r\n"
	   "Session: %u\r\n",
	   rs->rs_id);

  if((c = rtsp_con_get_arg(rc, "cseq")) != NULL)
    rcprintf(rc, "CSeq: %s\r\n", c);
  
  rcprintf(rc, "\r\n");
}

/*
 * RTSP SETUP
 */

static void
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

  if((ch = rtsp_channel_by_url(rc->rc_url)) == NULL) {
    rtsp_reply_error(rc, RTSP_STATUS_SERVICE, "URL does not resolve");
    return;
  }

  client_ports[0] = 0;
  client_ports[1] = 0;

  if((t = rtsp_con_get_arg(rc, "transport")) == NULL) {
    rtsp_reply_error(rc, RTSP_STATUS_TRANSPORT, NULL);
    return;
  }

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

      if(navp == 1 && !strcmp(avp[0], "unicast")) {
	ismulticast = 0;
      } else if(navp == 2 && !strcmp(avp[0], "client_port")) {
	nports = tokenize(avp[1], ports, 2, '-');
	if(nports > 0) client_ports[0] = atoi(ports[0]);
	if(nports > 1) client_ports[1] = atoi(ports[1]);
      }
    }
    if(!ismulticast && client_ports[0] && client_ports[1])
      break;
  }

  if(i == nt) {
    /* couldnt find a suitable transport */
    rtsp_reply_error(rc, RTSP_STATUS_TRANSPORT, NULL);
    return;
  }

  memcpy(&dst, &rc->rc_tcp_session.tcp_peer_addr, sizeof(struct sockaddr_in));
  dst.sin_port = htons(client_ports[0]);

  if((rs = rtsp_session_create(ch, &dst)) == NULL) {
    rtsp_reply_error(rc, RTSP_STATUS_INTERNAL, NULL);
    return;
  }

  LIST_INSERT_HEAD(&rc->rc_sessions, rs, rs_con_link);

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
}



/*
 * RTSP DESCRIBE
 */

static void
rtsp_cmd_describe(rtsp_connection_t *rc)
{
  char sdpreply[1000];
  th_channel_t *ch;
  char *c;

  if((ch = rtsp_channel_by_url(rc->rc_url)) == NULL) {
    rtsp_reply_error(rc, RTSP_STATUS_SERVICE, "URL does not resolve");
    return;
  }

  snprintf(sdpreply, sizeof(sdpreply),
	   "v=0\r\n"
	   "o=- 0 0 IN IPV4 127.0.0.1\r\n"
	   "s=%s\r\n"
	   "a=tool:hts tvheadend\r\n"
	   "m=video 0 RTP/AVP 33\r\n",
	   ch->ch_name);


  rcprintf(rc,
	   "RTSP/1.0 200 OK\r\n"
	   "Content-Type: application/sdp\r\n"
	   "Content-Length: %d\r\n",
	   strlen(sdpreply));

  if((c = rtsp_con_get_arg(rc, "cseq")) != NULL)
    rcprintf(rc, "CSeq: %s\r\n", c);
  
  rcprintf(rc, "\r\n%s", sdpreply);
}


/*
 * RTSP OPTIONS
 */
static void
rtsp_cmd_options(rtsp_connection_t *rc)
{
  char *c;

  if(strcmp(rc->rc_url, "*") && rtsp_channel_by_url(rc->rc_url) == NULL) {
    rtsp_reply_error(rc, RTSP_STATUS_SERVICE, "URL does not resolve");
    return;
  }

  rcprintf(rc,
	   "RTSP/1.0 200 OK\r\n"
	   "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n");

  if((c = rtsp_con_get_arg(rc, "cseq")) != NULL)
    rcprintf(rc, "CSeq: %s\r\n", c);
  rcprintf(rc, "\r\n");
}

/*
 * RTSP TEARDOWN
 */
static void
rtsp_cmd_teardown(rtsp_connection_t *rc)
{
  rtsp_session_t *rs;
  char *c;

  rs = rtsp_get_session(rc);
  if(rs == NULL)
    return;

  rcprintf(rc,
	   "RTSP/1.0 200 OK\r\n"
	   "Session: %u\r\n",
	   rs->rs_id);

  if((c = rtsp_con_get_arg(rc, "cseq")) != NULL)
    rcprintf(rc, "CSeq: %s\r\n", c);
  
  rcprintf(rc, "\r\n");

  rtsp_session_destroy(rs);
}

/*
 * RTSP connection state machine & parser
 */
static int
rtsp_con_parse(void *aux, char *buf)
{
  rtsp_connection_t *rc = aux;
  int n;
  char *argv[3], *c;

  switch(rc->rc_state) {
  case RTSP_CON_WAIT_REQUEST:
    rtsp_con_flush_args(rc);
    if(rc->rc_url != NULL) {
      free(rc->rc_url);
      rc->rc_url = NULL;
    }

    n = tokenize(buf, argv, 3, -1);
    
    if(n < 3)
      return EBADRQC;

    if(strcmp(argv[2], "RTSP/1.0")) {
      rtsp_reply_error(rc, RTSP_STATUS_VERSION, NULL);
      return ECONNRESET;
    }
    rc->rc_cmd = str2val(argv[0], RTSP_cmdtab);
    rc->rc_url = strdup(argv[1]);
    rc->rc_state = RTSP_CON_READ_HEADER;
    break;

  case RTSP_CON_READ_HEADER:
    if(*buf == 0) {
      rc->rc_state = RTSP_CON_WAIT_REQUEST;
      switch(rc->rc_cmd) {
      default:
	rtsp_reply_error(rc, RTSP_STATUS_METHOD, NULL);
	break;
      case RTSP_CMD_DESCRIBE:  	rtsp_cmd_describe(rc);  break;
      case RTSP_CMD_SETUP: 	rtsp_cmd_setup(rc);     break;
      case RTSP_CMD_PLAY:       rtsp_cmd_play(rc);      break;
      case RTSP_CMD_PAUSE:      rtsp_cmd_pause(rc);     break;
      case RTSP_CMD_OPTIONS:    rtsp_cmd_options(rc);   break;
      case RTSP_CMD_TEARDOWN:   rtsp_cmd_teardown(rc);  break;

      }
      break;
    }

    n = tokenize(buf, argv, 2, -1);
    if(n < 2)
      break;

    c = strrchr(argv[0], ':');
    if(c == NULL)
      break;
    *c = 0;
    rtsp_con_set_arg(rc, argv[0], argv[1]);
    break;

  case RTSP_CON_END:
    break;
  }
  return 0;
}


/*
 * disconnect
 */
static void
rtsp_disconnect(rtsp_connection_t *rc)
{
  rtsp_session_t *rs;

  rtsp_con_flush_args(rc);

  while((rs = LIST_FIRST(&rc->rc_sessions)) != NULL)
    rtsp_session_destroy(rs);

  free(rc->rc_url);
}


/*
 *
 */
static void
rtsp_tcp_callback(tcpevent_t event, void *tcpsession)
{
  rtsp_connection_t *rc = tcpsession;

  switch(event) {
  case TCP_CONNECT:
    break;

  case TCP_DISCONNECT:
    rtsp_disconnect(rc);
    break;

  case TCP_INPUT:
    tcp_line_read(&rc->rc_tcp_session, rtsp_con_parse);
    break;
  }
}


/*
 *  Fire up RTSP server
 */

void
rtsp_start(void)
{
  av_init_random(time(NULL), &rtsp_rnd);
  tcp_create_server(9908, sizeof(rtsp_connection_t), "rtsp",
		    rtsp_tcp_callback);
}
