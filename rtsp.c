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
#include "rtp.h"
#include "tsmux.h"
#include "tcp.h"
#include "http.h"
#include "access.h"
#include "rtsp.h"

#include <libavutil/random.h>

#include <libhts/htscfg.h>

#define RTSP_STATUS_OK           200
#define RTSP_STATUS_UNAUTHORIZED 401
#define RTSP_STATUS_METHOD       405
#define RTSP_STATUS_SESSION      454
#define RTSP_STATUS_TRANSPORT    461
#define RTSP_STATUS_INTERNAL     500
#define RTSP_STATUS_SERVICE      503



#define rcprintf(c, fmt...) tcp_printf(&(rc)->rc_tcp_session, fmt)

static AVRandomState rtsp_rnd;

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




/**
 * Resolve an URL into a channel
 */

static channel_t *
rtsp_channel_by_url(char *url)
{
  channel_t *ch;
  char *c;

  c = strrchr(url, '/');
  if(c != NULL && c[1] == 0) {
    /* remove trailing slash */
    *c = 0;
    c = strrchr(url, '/');
  }

  if(c == NULL || c[1] == 0 || (url != c && c[-1] == '/'))
    return NULL;
  c++; 

  LIST_FOREACH(ch, &channels, ch_global_link)
    if(!strcasecmp(ch->ch_sname, c))
      return ch;

  return NULL;
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
    rs->rs_muxer = ts_muxer_init(s, rtp_output_ts, &rs->rs_rtp_streamer, 
				 TS_SEEK, 0);
    break;

  case TRANSPORT_UNAVAILABLE:
    assert(rs->rs_muxer != NULL);
    ts_muxer_deinit(rs->rs_muxer, s);
    rs->rs_muxer = NULL;
    break;
  }
}

/**
 * Create an RTSP session
 */
static rtsp_session_t *
rtsp_session_create(channel_t *ch, struct sockaddr_in *dst)
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
				   rtsp_subscription_callback, rs, 0);

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
 * RTSP return code to string
 */
static const char *
rtsp_err2str(int err)
{
  switch(err) {
  case RTSP_STATUS_OK:              return "OK";
  case RTSP_STATUS_UNAUTHORIZED:    return "Unauthorized";
  case RTSP_STATUS_METHOD:          return "Method Not Allowed";
  case RTSP_STATUS_SESSION:         return "Session Not Found";
  case RTSP_STATUS_TRANSPORT:       return "Unsupported transport";
  case RTSP_STATUS_INTERNAL:        return "Internal Server Error";
  case RTSP_STATUS_SERVICE:         return "Service Unavailable";
  default:
    return "Unknown Error";
    break;
  }
}

/*
 * Return an error
 */
static void
rtsp_reply_error(http_connection_t *hc, int error, const char *errstr)
{
  char *c;

  if(errstr == NULL)
    errstr = rtsp_err2str(error);

  tvhlog(LOG_INFO, "rtsp",
	 "%s: %s", tcp_logname(&hc->hc_tcp_session), errstr);

  http_printf(hc, "RTSP/1.0 %d %s\r\n", error, errstr);
  if((c = http_arg_get(&hc->hc_args, "cseq")) != NULL)
    http_printf(hc, "CSeq: %s\r\n", c);
  if(error == HTTP_STATUS_UNAUTHORIZED)
    http_printf(hc, "WWW-Authenticate: Basic realm=\"tvheadend\"\r\n");
  http_printf(hc, "\r\n");
}


/*
 * Find a session pointed do by the current connection
 */
static rtsp_session_t *
rtsp_get_session(http_connection_t *hc)
{
  char *ses;
  int sesid;
  rtsp_session_t *rs;
  channel_t *ch;

  if((ch = rtsp_channel_by_url(hc->hc_url)) == NULL) {
    rtsp_reply_error(hc, RTSP_STATUS_SERVICE, "URL does not resolve");
    return NULL;
  }


  if((ses = http_arg_get(&hc->hc_args, "session")) == NULL) {
    rtsp_reply_error(hc, RTSP_STATUS_SESSION, NULL);
    return NULL;
  }

  sesid = atoi(ses);
  LIST_FOREACH(rs, &rtsp_sessions, rs_global_link)
    if(rs->rs_id == sesid)
      break;
  
  if(rs == NULL)
    rtsp_reply_error(hc, RTSP_STATUS_SESSION, NULL);
  
  return rs;
}


/*
 * RTSP PLAY
 */

static void
rtsp_cmd_play(http_connection_t *hc)
{
  char *c;
  int64_t start;
  rtsp_session_t *rs;
 
  rs = rtsp_get_session(hc);
  if(rs == NULL)
    return;

  if((c = http_arg_get(&hc->hc_args, "range")) != NULL) {
    start = AV_NOPTS_VALUE;
  } else {
    start = AV_NOPTS_VALUE;
  }

  if(rs->rs_muxer == NULL) {
    rtsp_reply_error(hc, RTSP_STATUS_SERVICE, 
		     "No muxer attached (missing SETUP ?)");
    return;
  }

  ts_muxer_play(rs->rs_muxer, start);

  if(rs->rs_s->ths_channel != NULL)
    tvhlog(LOG_INFO, "rtsp",
	   "%s: Starting playback of %s",
	   tcp_logname(&hc->hc_tcp_session), rs->rs_s->ths_channel->ch_name);

  http_printf(hc,
	      "RTSP/1.0 200 OK\r\n"
	      "Session: %u\r\n",
	      rs->rs_id);

  if((c = http_arg_get(&hc->hc_args, "cseq")) != NULL)
    http_printf(hc, "CSeq: %s\r\n", c);
  
  http_printf(hc, "\r\n");
}

/*
 * RTSP PAUSE
 */

static void
rtsp_cmd_pause(http_connection_t *hc)
{
  char *c;
  rtsp_session_t *rs;
  
  rs = rtsp_get_session(hc);
  if(rs == NULL)
    return;

  if(rs->rs_muxer == NULL) {
    rtsp_reply_error(hc, RTSP_STATUS_SERVICE,
		     "No muxer attached (missing SETUP ?)");
    return;
  }

  ts_muxer_pause(rs->rs_muxer);

  if(rs->rs_s->ths_channel != NULL)
    tvhlog(LOG_INFO, "rtsp", 
	   "%s: Pausing playback of %s",
	   tcp_logname(&hc->hc_tcp_session), rs->rs_s->ths_channel->ch_name);

  http_printf(hc,
	      "RTSP/1.0 200 OK\r\n"
	      "Session: %u\r\n",
	      rs->rs_id);

  if((c = http_arg_get(&hc->hc_args, "cseq")) != NULL)
    http_printf(hc, "CSeq: %s\r\n", c);
  
  http_printf(hc, "\r\n");
}

/*
 * RTSP SETUP
 */

static void
rtsp_cmd_setup(http_connection_t *hc)
{
  char *transports[10];
  char *params[10];
  char *avp[2];
  char *ports[2];
  char *t, *c;
  int nt, i, np, j, navp, nports, ismulticast;
  int client_ports[2];
  rtsp_session_t *rs;
  channel_t *ch;
  struct sockaddr_in dst;
  
  if((ch = rtsp_channel_by_url(hc->hc_url)) == NULL) {
    rtsp_reply_error(hc, RTSP_STATUS_SERVICE, "URL does not resolve");
    return;
  }

  client_ports[0] = 0;
  client_ports[1] = 0;

  if((t = http_arg_get(&hc->hc_args, "transport")) == NULL) {
    rtsp_reply_error(hc, RTSP_STATUS_TRANSPORT, NULL);
    return;
  }

  nt = http_tokenize(t, transports, 10, ',');
  
  /* Select a transport we can accept */

  for(i = 0; i < nt; i++) {
    np = http_tokenize(transports[i], params, 10, ';');

    if(np == 0)
      continue;
    if(strcasecmp(params[0], "RTP/AVP/UDP") &&
       strcasecmp(params[0], "RTP/AVP"))
      continue;

    ismulticast = 1; /* multicast is default according to RFC */
    client_ports[0] = 0;
    client_ports[1] = 0;


    for(j = 1; j < np; j++) {
      if((navp = http_tokenize(params[j], avp, 2, '=')) == 0)
	continue;

      if(navp == 1 && !strcmp(avp[0], "unicast")) {
	ismulticast = 0;
      } else if(navp == 2 && !strcmp(avp[0], "client_port")) {
	nports = http_tokenize(avp[1], ports, 2, '-');
	if(nports > 0) client_ports[0] = atoi(ports[0]);
	if(nports > 1) client_ports[1] = atoi(ports[1]);
      }
    }
    if(!ismulticast && client_ports[0] && client_ports[1])
      break;
  }

  if(i == nt) {
    /* couldnt find a suitable transport */
    rtsp_reply_error(hc, RTSP_STATUS_TRANSPORT, NULL);
    return;
  }

  memcpy(&dst, &hc->hc_tcp_session.tcp_peer_addr, sizeof(struct sockaddr_in));
  dst.sin_port = htons(client_ports[0]);

  if((rs = rtsp_session_create(ch, &dst)) == NULL) {
    rtsp_reply_error(hc, RTSP_STATUS_INTERNAL, NULL);
    return;
  }

  LIST_INSERT_HEAD(&hc->hc_rtsp_sessions, rs, rs_con_link);

  http_printf(hc,
	      "RTSP/1.0 200 OK\r\n"
	      "Session: %u\r\n"
	      "Transport: RTP/AVP/UDP;unicast;client_port=%d-%d;"
	      "server_port=%d-%d\r\n",
	      rs->rs_id,
	      client_ports[0],
	      client_ports[1],
	      rs->rs_server_port[0],
	      rs->rs_server_port[1]);

  if((c = http_arg_get(&hc->hc_args, "cseq")) != NULL)
    http_printf(hc, "CSeq: %s\r\n", c);
  
  http_printf(hc, "\r\n");
}



/*
 * RTSP DESCRIBE
 */

static void
rtsp_cmd_describe(http_connection_t *hc)
{
  char sdpreply[1000];
  channel_t *ch;
  char *c;

  if((ch = rtsp_channel_by_url(hc->hc_url)) == NULL) {
    rtsp_reply_error(hc, RTSP_STATUS_SERVICE, "URL does not resolve");
    return;
  }

  snprintf(sdpreply, sizeof(sdpreply),
	   "v=0\r\n"
	   "o=- 0 0 IN IPV4 127.0.0.1\r\n"
	   "s=%s\r\n"
	   "a=tool:hts tvheadend\r\n"
	   "m=video 0 RTP/AVP 33\r\n",
	   ch->ch_name);


  http_printf(hc,
	      "RTSP/1.0 200 OK\r\n"
	      "Content-Type: application/sdp\r\n"
	      "Content-Length: %d\r\n",
	      strlen(sdpreply));

  if((c = http_arg_get(&hc->hc_args, "cseq")) != NULL)
    http_printf(hc, "CSeq: %s\r\n", c);
  
  http_printf(hc, "\r\n%s", sdpreply);
}


/*
 * RTSP OPTIONS
 */
static void
rtsp_cmd_options(http_connection_t *hc)
{
  char *c;

  if(strcmp(hc->hc_url, "*") && rtsp_channel_by_url(hc->hc_url) == NULL) {
    rtsp_reply_error(hc, RTSP_STATUS_SERVICE, "URL does not resolve");
    return;
  }

  http_printf(hc,
	      "RTSP/1.0 200 OK\r\n"
	      "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n");

  if((c = http_arg_get(&hc->hc_args, "cseq")) != NULL)
    http_printf(hc, "CSeq: %s\r\n", c);
  http_printf(hc, "\r\n");
}

/*
 * RTSP TEARDOWN
 */
static void
rtsp_cmd_teardown(http_connection_t *hc)
{
  rtsp_session_t *rs;
  char *c;

  if((rs = rtsp_get_session(hc)) == NULL)
    return;

  http_printf(hc,
	      "RTSP/1.0 200 OK\r\n"
	      "Session: %u\r\n",
	      rs->rs_id);

  if((c = http_arg_get(&hc->hc_args, "cseq")) != NULL)
    http_printf(hc, "CSeq: %s\r\n", c);
  
  http_printf(hc, "\r\n");

  rtsp_session_destroy(rs);
}

/**
 *
 */
static int
rtsp_access_list(http_connection_t *hc)
{
  int x;

  if(hc->hc_authenticated)
    return 0;

  x = access_verify(hc->hc_username, hc->hc_password,
		    (struct sockaddr *)&hc->hc_tcp_session.tcp_peer_addr,
		    ACCESS_STREAMING);
  if(x == 0)
    hc->hc_authenticated = 1;

  return x;
}



/*
 * RTSP connection state machine & parser
 */
int 
rtsp_process_request(http_connection_t *hc)
{
  if(rtsp_access_list(hc)) {
    rtsp_reply_error(hc, RTSP_STATUS_UNAUTHORIZED, NULL);
    return 0;
  }

  switch(hc->hc_cmd) {
  default:
    rtsp_reply_error(hc, RTSP_STATUS_METHOD, NULL);
    break;
  case RTSP_CMD_DESCRIBE:
    rtsp_cmd_describe(hc);
    break;
  case RTSP_CMD_SETUP:
    rtsp_cmd_setup(hc);
    break;
  case RTSP_CMD_PLAY:
    rtsp_cmd_play(hc);
    break;
  case RTSP_CMD_PAUSE:
    rtsp_cmd_pause(hc); 
    break;
  case RTSP_CMD_OPTIONS:   
    rtsp_cmd_options(hc);
    break;
  case RTSP_CMD_TEARDOWN: 
    rtsp_cmd_teardown(hc);
    break;
  }
  return 0;
}

/*
 *
 */
void
rtsp_disconncet(http_connection_t *hc)
{
  rtsp_session_t *rs;

  while((rs = LIST_FIRST(&hc->hc_rtsp_sessions)) != NULL)
    rtsp_session_destroy(rs);
}

/*
 *
 */
void
rtsp_init(void)
{
  av_init_random(time(NULL), &rtsp_rnd);
}
