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
#include "tcp.h"
#include "http.h"
#include "access.h"
#include "rtsp.h"
#include "rtp.h"
#include "streaming.h"
#include "transports.h"

TAILQ_HEAD(rtsp_packet_queue, rtsp_packet);

/**
 *
 */
typedef struct rtsp_packet {
  TAILQ_ENTRY(rtsp_packet) rp_link;
  
  int rp_payloadsize;
  uint8_t rp_payload[0];

} rtsp_packet_t;


/**
 *
 */
typedef struct rtsp {
  int rtsp_refcount; // Should only be modified with global_lock held

  struct th_subscription *rtsp_sub;


  /* Startup */
  const char *rtsp_start_error;
  struct streaming_start *rtsp_ss;
  pthread_mutex_t rtsp_start_mutex;
  pthread_cond_t rtsp_start_cond;



  streaming_target_t rtsp_input;

  LIST_HEAD(, rtsp_stream) rtsp_streams;

  int rtsp_running;

  char rtsp_session_id[65];

  struct rtsp_packet_queue rtsp_pqueue;
  int rtsp_pqueue_size;

  pthread_t rtsp_thread;
  int rtsp_run_thread;
  int rtsp_tcp_socket;

  pthread_mutex_t rtsp_mutex;
  pthread_cond_t rtsp_cond;

} rtsp_t;


/**
 *
 */
typedef struct rtsp_stream {
  LIST_ENTRY(rtsp_stream) rs_link;
  rtsp_t *rs_rtsp;

  int rs_index;  // Source index
  int rs_output; // Output index (set by RTSP client)

  int rs_send_sock;
  int rs_recv_sock;

  int rs_type;
  rtp_stream_t rs_rtp;
  rtp_send_t *rs_sender;

  int rs_interleaved_stream;

} rtsp_stream_t;


#define rtsp_printf(hc, fmt...) htsbuf_qprintf(&(hc)->hc_reply, fmt)

#define RTSP_STATUS_OK           200
#define RTSP_STATUS_UNAUTHORIZED 401
#define RTSP_STATUS_METHOD       405
#define RTSP_STATUS_SESSION      454
#define RTSP_STATUS_TRANSPORT    461
#define RTSP_STATUS_INTERNAL     500
#define RTSP_STATUS_SERVICE      503



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
  case 403:                         return "Permission denied";
  case 459:                         return "Aggregate operation not allowed";
  default:
    return "Error";
  }
}

/*
 * Return an error
 */
static int
rtsp_error(http_connection_t *hc, int error, const char *errstr)
{
  char *c;

  if(errstr == NULL)
    errstr = rtsp_err2str(error);

  tvhlog(LOG_ERR, "RTSP", "%s: %s -- %s", 
	 inet_ntoa(hc->hc_peer->sin_addr), hc->hc_url_orig, errstr);

  htsbuf_qprintf(&hc->hc_reply, "RTSP/1.0 %d %s\r\n", error, errstr);
  if((c = http_arg_get(&hc->hc_args, "cseq")) != NULL)
    htsbuf_qprintf(&hc->hc_reply, "CSeq: %s\r\n", c);
  if(error == HTTP_STATUS_UNAUTHORIZED)
    htsbuf_qprintf(&hc->hc_reply, 
		   "WWW-Authenticate: Basic realm=\"tvheadend\"\r\n");
  htsbuf_qprintf(&hc->hc_reply, "\r\n");
  return 0;
}


/**
 * 
 */
static void *
rtsp_tcp_thread(void *aux)
{
  rtsp_t *rtsp = aux;
  rtsp_packet_t *rp;

  pthread_mutex_lock(&rtsp->rtsp_mutex);

  while(1) {
    
    if(!rtsp->rtsp_run_thread)
      break;

    if((rp = TAILQ_FIRST(&rtsp->rtsp_pqueue)) == NULL) {
      pthread_cond_wait(&rtsp->rtsp_cond, &rtsp->rtsp_mutex);
      continue;
    }

    TAILQ_REMOVE(&rtsp->rtsp_pqueue, rp, rp_link);
    rtsp->rtsp_pqueue_size -= rp->rp_payloadsize;

    pthread_mutex_unlock(&rtsp->rtsp_mutex);

    if(write(rtsp->rtsp_tcp_socket, rp->rp_payload, rp->rp_payloadsize)) {}
    free(rp);

    pthread_mutex_lock(&rtsp->rtsp_mutex);
  }

  pthread_mutex_unlock(&rtsp->rtsp_mutex);
  return NULL;
}


/**
 *
 */
static void
start_tcp_writer(rtsp_t *rtsp, int fd)
{
  if(rtsp->rtsp_run_thread)
    return;
  
  rtsp->rtsp_tcp_socket = fd;
  rtsp->rtsp_run_thread = 1;
  pthread_create(&rtsp->rtsp_thread, NULL, rtsp_tcp_thread, rtsp);
}


/**
 *
 */
static void
rtsp_stream_teardown(rtsp_stream_t *rs)
{
  if(rs->rs_send_sock != -1)
    close(rs->rs_send_sock);

  if(rs->rs_recv_sock != -1)
    close(rs->rs_recv_sock);

  LIST_REMOVE(rs, rs_link);
  free(rs);
}


/**
 *
 */
static void
rtsp_destroy_unref(rtsp_t *rtsp)
{
  rtsp_stream_t *rs;
  rtsp_packet_t *rp;

  lock_assert(&global_lock);

  if(rtsp->rtsp_refcount > 1) {
    rtsp->rtsp_refcount--;
    return;
  }

  if(rtsp->rtsp_sub != NULL)
    subscription_unsubscribe(rtsp->rtsp_sub);

  if(rtsp->rtsp_ss != NULL)
    streaming_start_unref(rtsp->rtsp_ss);

  while((rs = LIST_FIRST(&rtsp->rtsp_streams)) != NULL)
    rtsp_stream_teardown(rs);

  if(rtsp->rtsp_run_thread) {
    rtsp->rtsp_run_thread = 0;
    pthread_cond_signal(&rtsp->rtsp_cond);

    pthread_join(rtsp->rtsp_thread, NULL);
  }

  while((rp = TAILQ_FIRST(&rtsp->rtsp_pqueue)) != NULL) {
    TAILQ_REMOVE(&rtsp->rtsp_pqueue, rp, rp_link);
    free(rp);
  }

  free(rtsp);
}



/**
 *
 */
static int
genrand32(uint8_t *r)
{
  int fd, n;

  if((fd = tvh_open("/dev/urandom", O_RDONLY, 0)) < 0)
    return -1;
  
  n = read(fd, r, 32);
  close(fd);
  return n != 32;
}


/**
 *
 */
static rtsp_t *
rtsp_get_session(http_connection_t *hc)
{
  rtsp_t *rtsp;
  uint8_t r[32];
  int i;

  if(hc->hc_rtsp_session != NULL)
    return hc->hc_rtsp_session;

  rtsp = hc->hc_rtsp_session = calloc(1, sizeof(rtsp_t));

  if(genrand32(r))
    return NULL;

  for(i = 0; i < 32; i++)
    sprintf(rtsp->rtsp_session_id + i * 2, "%02x", r[i]);

  TAILQ_INIT(&rtsp->rtsp_pqueue);
  pthread_cond_init(&rtsp->rtsp_cond, NULL);
  pthread_mutex_init(&rtsp->rtsp_mutex, NULL);

  pthread_cond_init(&rtsp->rtsp_start_cond, NULL);
  pthread_mutex_init(&rtsp->rtsp_start_mutex, NULL);
  return rtsp;
}

/**
 * Return 0 if OK, ~0 if fail
 */
static int
rtsp_check_session(http_connection_t *hc, rtsp_t *rtsp, int none_is_ok)
{
  char *ses = http_arg_get(&hc->hc_args, "session");

  if(none_is_ok && ses == NULL)
    return 0;

  return ses == NULL || strcmp(rtsp->rtsp_session_id, ses);
}

/**
 *
 */
static void
rtsp_send_tcp(void *opaque, void *buf, size_t len)
{
  rtsp_stream_t *rs = opaque;
  rtsp_t *rtsp = rs->rs_rtsp;
  rtsp_packet_t *rp = malloc(sizeof(rtsp_packet_t) + len + 4);

  rp->rp_payload[0] = '$';
  rp->rp_payload[1] = rs->rs_interleaved_stream;
  rp->rp_payload[2] = len >> 8;
  rp->rp_payload[3] = len;

  memcpy(rp->rp_payload + 4, buf, len);
  rp->rp_payloadsize = len + 4;

  pthread_mutex_lock(&rtsp->rtsp_mutex);
  TAILQ_INSERT_TAIL(&rtsp->rtsp_pqueue, rp, rp_link);
  rtsp->rtsp_pqueue_size += rp->rp_payloadsize;
  pthread_cond_signal(&rtsp->rtsp_cond);
  pthread_mutex_unlock(&rtsp->rtsp_mutex);
}


/**
 *
 */
static void
rtsp_send_udp(void *opaque, void *buf, size_t len)
{
  rtsp_stream_t *rs = opaque;
  if(write(rs->rs_send_sock, buf, len)) {}
}




/**
 *
 */
static void
rtsp_streaming_send(rtsp_t *rtsp, th_pkt_t *pkt)
{
  rtsp_stream_t *rs;

  LIST_FOREACH(rs, &rtsp->rtsp_streams, rs_link)
    if(rs->rs_index == pkt->pkt_componentindex)
      break;

  if(rs == NULL)
    return;

  switch(rs->rs_type) {
  case SCT_MPEG2VIDEO:
    rtp_send_mpv(rs->rs_sender, rs, &rs->rs_rtp, pkt->pkt_payload,
		 pkt->pkt_payloadlen, pkt->pkt_pts);
    break;
  case SCT_MPEG2AUDIO:
    rtp_send_mpa(rs->rs_sender, rs, &rs->rs_rtp, pkt->pkt_payload,
		 pkt->pkt_payloadlen, pkt->pkt_pts);
    break;
  }
}

    


/**
 *
 */
static void
rtsp_streaming_input(void *opaque, streaming_message_t *sm)
{
  rtsp_t *rtsp = opaque;

  switch(sm->sm_type) {
  case SMT_START:

    pthread_mutex_lock(&rtsp->rtsp_start_mutex);

    assert(rtsp->rtsp_ss == NULL);
    rtsp->rtsp_ss = sm->sm_data;

    pthread_cond_signal(&rtsp->rtsp_start_cond);
    pthread_mutex_unlock(&rtsp->rtsp_start_mutex);

    sm->sm_data = NULL; // steal reference
    break;

  case SMT_STOP:
    pthread_mutex_lock(&rtsp->rtsp_start_mutex);

    assert(rtsp->rtsp_ss != NULL);
    streaming_start_unref(rtsp->rtsp_ss);
    rtsp->rtsp_ss = NULL;

    pthread_cond_signal(&rtsp->rtsp_start_cond);
    pthread_mutex_unlock(&rtsp->rtsp_start_mutex);
    break;

  case SMT_PACKET:
    if(rtsp->rtsp_running)
      rtsp_streaming_send(rtsp, sm->sm_data);
    break;

  case SMT_TRANSPORT_STATUS:
     if(sm->sm_code & TSS_PACKETS) {
	
     } else if(sm->sm_code & (TSS_GRACEPERIOD | TSS_ERRORS)) {

       pthread_mutex_lock(&rtsp->rtsp_start_mutex);
       rtsp->rtsp_start_error = transport_tss2text(sm->sm_code);
       pthread_cond_signal(&rtsp->rtsp_start_cond);
       pthread_mutex_unlock(&rtsp->rtsp_start_mutex);
     }
    break;

  case SMT_NOSOURCE:
    pthread_mutex_lock(&rtsp->rtsp_start_mutex);
    rtsp->rtsp_start_error = transport_nostart2txt(sm->sm_code);
    pthread_cond_signal(&rtsp->rtsp_start_cond);
    pthread_mutex_unlock(&rtsp->rtsp_start_mutex);
    break;

  case SMT_MPEGTS:
    break;

  default:
    abort();
  }
  streaming_msg_free(sm);
}

/**
 * Attach the url to an internal resource (channel or transport)
 */
static int
rtsp_subscribe(http_connection_t *hc, rtsp_t *rtsp,
	       char *title, size_t titlelen,
	       char *baseurl, size_t baseurllen)
{
  char *components[5];
  int nc;
  char *url = hc->hc_url;
  int pri = 150;
  int subflags = 0;
  th_subscription_t *s;
  char urlprefix[128];
  char buf[INET_ADDRSTRLEN + 1];

  inet_ntop(AF_INET, &hc->hc_self->sin_addr, buf, sizeof(buf));
  snprintf(urlprefix, sizeof(urlprefix),
	   "rtsp://%s:%d", buf, ntohs(hc->hc_self->sin_port));

  if(rtsp->rtsp_sub != NULL) {
    rtsp_error(hc, 400, "Please teardown first");
    return -1;
  }

  if(!strncasecmp(url, "rtsp://", strlen("rtsp://"))) {
    url += strlen("rtsp://");
    url = strchr(url, '/');
    if(url == NULL) {
      rtsp_error(hc, RTSP_STATUS_SERVICE, "Invalid URL");
      return -1;
    }
    url++;
  }
  
  nc = http_tokenize(url, components, 5, '/');
  
  if(nc < 2 || nc > 3)
    return -1;

  http_deescape(components[1]);

  rtsp->rtsp_start_error = NULL;
  rtsp->rtsp_ss = NULL;
  streaming_target_init(&rtsp->rtsp_input, rtsp_streaming_input, rtsp, 0);


  if(!strcmp(components[0], "channel")) {
    channel_t *ch;
    scopedgloballock();
  
    if((ch = channel_find_by_name(components[1], 0)) == NULL) {
      rtsp_error(hc, RTSP_STATUS_SERVICE, "Channel name not found");
      return -1;
    }

    s = subscription_create_from_channel(ch, pri, "RTSP", &rtsp->rtsp_input,
					 subflags);

    snprintf(baseurl, baseurllen, "%s/channelid/%d", urlprefix, ch->ch_id);
    snprintf(title, titlelen, "%s", ch->ch_name);

  } else if(!strcmp(components[0], "channelid")) {
    channel_t *ch;
    scopedgloballock();
  
    if((ch = channel_find_by_identifier(atoi(components[1]))) == NULL) {
      rtsp_error(hc, RTSP_STATUS_SERVICE, "Channel ID not found");
      return -1;
    }

    s = subscription_create_from_channel(ch, pri, "RTSP", &rtsp->rtsp_input,
					 subflags);

    snprintf(baseurl, baseurllen, "%s/channelid/%d", urlprefix, ch->ch_id);
    snprintf(title, titlelen, "%s", ch->ch_name);

  } else if(!strcmp(components[0], "service")) {
    th_transport_t *t;
    scopedgloballock();

    if((t = transport_find_by_identifier(components[1])) == NULL) {
      rtsp_error(hc, RTSP_STATUS_SERVICE, "Transport ID not found");
      return -1;
    }
    s = subscription_create_from_transport(t, "RTSP", &rtsp->rtsp_input,
					   subflags);

    snprintf(baseurl, baseurllen, "%s/service/%s", 
	     urlprefix, t->tht_identifier);
    snprintf(title, titlelen, "%s", t->tht_identifier);

  } else {
    rtsp_error(hc, RTSP_STATUS_SERVICE, "Invalid URL");
    return -1;
  }

  pthread_mutex_lock(&rtsp->rtsp_start_mutex);
  while(rtsp->rtsp_start_error == NULL && rtsp->rtsp_ss == NULL)
    pthread_cond_wait(&rtsp->rtsp_start_cond, &rtsp->rtsp_start_mutex);

  if(rtsp->rtsp_start_error != NULL) {
    pthread_mutex_unlock(&rtsp->rtsp_start_mutex);


    pthread_mutex_lock(&global_lock);
    subscription_unsubscribe(s);
    pthread_mutex_unlock(&global_lock);

    rtsp_error(hc, RTSP_STATUS_SERVICE, rtsp->rtsp_start_error);
    return -1;
  }

  pthread_mutex_unlock(&rtsp->rtsp_start_mutex);

  rtsp->rtsp_sub = s;
  return 0;
}


/**
 * RTSP OPTIONS
 */
static int
rtsp_cmd_options(http_connection_t *hc)
{
  char *c;

  rtsp_printf(hc,
	      "RTSP/1.0 200 OK\r\n"
	      "Public: DESCRIBE, SETUP, TEARDOWN, PLAY\r\n");

  if((c = http_arg_get(&hc->hc_args, "cseq")) != NULL)
    rtsp_printf(hc, "CSeq: %s\r\n", c);
  rtsp_printf(hc, "\r\n");
  return 0;
}


/**
 * Get stream by index
 */
static const streaming_start_component_t *
get_ssc_by_index(rtsp_t *rtsp, int wanted_idx)
{
  const streaming_start_t *ss = rtsp->rtsp_ss;
  int i;
  for(i = 0; i < ss->ss_num_components; i++)
    if(ss->ss_components[i].ssc_index == wanted_idx)
      return &ss->ss_components[i];
  return NULL;
}






/**
 * RTSP DESCRIBE
 */
static int
rtsp_cmd_describe(http_connection_t *hc, rtsp_t *rtsp)
{
  char sdp[1000];
  char baseurl[128];
  char *c;
  extern const char *htsversion;
  const streaming_start_t *ss;
  int i;
  char title[128];

  if(rtsp_subscribe(hc, rtsp, title, sizeof(title), baseurl, sizeof(baseurl)))
    return 0;

  snprintf(sdp, sizeof(sdp),
	   "v=0\r\n"
	   "o=- 0 0 IN IPV4 127.0.0.1\r\n"
	   "s=%s\r\n"
	   "a=tool:HTS Tvheadend %s\r\n",
	   title, htsversion);

  ss = rtsp->rtsp_ss;

  for(i = 0; i < ss->ss_num_components; i++) {
    const streaming_start_component_t *ssc = &ss->ss_components[i];
    
    char controlurl[256];
    snprintf(controlurl, sizeof(controlurl), "%s/streamid=%d",
	     baseurl, ssc->ssc_index);

    switch(ssc->ssc_type) {
    case SCT_MPEG2VIDEO:
      tvh_strlcatf(sdp, sizeof(sdp), 
		   "m=video 0 RTP/AVP 32\r\n"
		   "a=control:%s\r\n", controlurl);
      break;
    case SCT_MPEG2AUDIO:
      tvh_strlcatf(sdp, sizeof(sdp), 
		   "m=audio 0 RTP/AVP 14\r\n"
		   "a=control:%s\r\n", controlurl);
      break;
    }
  }

  rtsp_printf(hc,
	      "RTSP/1.0 200 OK\r\n"
	      "Content-Type: application/sdp\r\n"
	      "Content-Length: %d\r\n",
	      strlen(sdp));

  if((c = http_arg_get(&hc->hc_args, "cseq")) != NULL)
    rtsp_printf(hc, "CSeq: %s\r\n", c);
  
  rtsp_printf(hc, "\r\n%s", sdp);
  return 0;
}

/**
 * Create rs stream from streaming_start_componen
 */
static rtsp_stream_t *
rs_by_ssc(rtsp_t *rtsp, const streaming_start_component_t *ssc)
{
  rtsp_stream_t *rs = calloc(1, sizeof(rtsp_stream_t));
  rs->rs_rtsp = rtsp;
  rs->rs_index = ssc->ssc_index;
  rs->rs_type = ssc->ssc_type;
  return rs;
}


/**
 * Setup a TCP stream
 */
static int
rtsp_setup_tcp(http_connection_t *hc, rtsp_t *rtsp, 
	       const streaming_start_component_t *ssc,
	       char *params[], int np)
{
  rtsp_stream_t *rs;
  int navp;
  char *avp[2];
  int nvalues;
  char *values[2];
  int interleaved[2];
  int i;
  const char *c;

  interleaved[0] = -1;
  interleaved[1] = -1;
  
  for(i = 1; i < np; i++) {
    if((navp = http_tokenize(params[i], avp, 2, '=')) == 0)
      continue;

    if(navp == 2 && !strcmp(avp[0], "interleaved")) {
      nvalues = http_tokenize(avp[1], values, 2, '-');
      if(nvalues > 0) interleaved[0] = atoi(values[0]);
      if(nvalues > 1) interleaved[1] = atoi(values[1]);
    }
  }

 if(interleaved[0] == -1 || interleaved[1] == -1)
    return rtsp_error(hc, RTSP_STATUS_TRANSPORT, 
		      "No interleaved values selected by client");
   
 rs = rs_by_ssc(rtsp, ssc);

 rs->rs_interleaved_stream = interleaved[0];
 rs->rs_sender = rtsp_send_tcp;

 rs->rs_send_sock = -1;
 rs->rs_recv_sock = -1;

 LIST_INSERT_HEAD(&rtsp->rtsp_streams, rs, rs_link);

 start_tcp_writer(rtsp, hc->hc_fd);

 rtsp_printf(hc,
	     "RTSP/1.0 200 OK\r\n"
	     "Session: %s\r\n"
	     "Transport: RTP/AVP/TCP;interleaved=%d-%d\r\n",
	     rtsp->rtsp_session_id,
	     interleaved[0], interleaved[1]);

  if((c = http_arg_get(&hc->hc_args, "cseq")) != NULL)
    rtsp_printf(hc, "CSeq: %s\r\n", c);

  rtsp_printf(hc, "\r\n");  
  return 0;
}


/**
 * Setup an UDP stream
 */
static int
rtsp_setup_udp(http_connection_t *hc, rtsp_t *rtsp, 
	       const streaming_start_component_t *ssc,
	       char *params[], int np)
{
  const char *c;
  int i;
  int ismulticast = 1; /* multicast is default according to RFC */
  int navp;
  char *avp[2];
  int nports;
  char *ports[2];
  int client_ports[2];
  int server_ports[2];
  struct sockaddr_in dst;
  int attempt = 0;
  rtsp_stream_t *rs;
  socklen_t slen;
  struct sockaddr_in sin;
  int fd[2];

  client_ports[0] = 0;
  client_ports[1] = 0;

  for(i = 1; i < np; i++) {
    if((navp = http_tokenize(params[i], avp, 2, '=')) == 0)
      continue;

    if(navp == 1 && !strcmp(avp[0], "unicast")) {
      ismulticast = 0;

    } else if(navp == 2 && !strcmp(avp[0], "client_port")) {
      nports = http_tokenize(avp[1], ports, 2, '-');
      if(nports > 0) client_ports[0] = atoi(ports[0]);
      if(nports > 1) client_ports[1] = atoi(ports[1]);
    }
  }

  if(ismulticast)
    return rtsp_error(hc, RTSP_STATUS_TRANSPORT, "Multicast is not supported");
    
  if(!client_ports[0] || !client_ports[1])
    return rtsp_error(hc, RTSP_STATUS_TRANSPORT, 
		      "No UDP ports selected by client");
 

 retry:
  fd[0] = tvh_socket(AF_INET, SOCK_DGRAM, 0);

  memset(&sin, 0, sizeof(struct sockaddr_in));
  sin.sin_family = AF_INET;
    
  if(bind(fd[0], (struct sockaddr *)&sin, sizeof(sin)) == -1) {
    close(fd[0]);
    // XXX log
    return rtsp_error(hc, RTSP_STATUS_TRANSPORT, NULL);
  }

  slen = sizeof(struct sockaddr_in);
  getsockname(fd[0], (struct sockaddr *)&sin, &slen);

  server_ports[0] = ntohs(sin.sin_port);
  server_ports[1] = server_ports[0] + 1;
    
  sin.sin_port = htons(server_ports[1]);

  fd[1] = tvh_socket(AF_INET, SOCK_DGRAM, 0);
    
  if(bind(fd[1], (struct sockaddr *)&sin, sizeof(sin)) == -1) {
    close(fd[0]);
    close(fd[1]);
      
    attempt++;
    if(attempt == 100) {
      // XXX log
      return rtsp_error(hc, RTSP_STATUS_TRANSPORT, NULL);
    }
    goto retry;
  }

  memcpy(&dst, hc->hc_peer, sizeof(struct sockaddr_in));
  dst.sin_port = htons(client_ports[0]);

  if(connect(fd[0], (struct sockaddr *)&dst, sizeof(dst))) {
    close(fd[0]);
    close(fd[1]);
    // XXX log
    return rtsp_error(hc, RTSP_STATUS_TRANSPORT, NULL);
  }

  rs = rs_by_ssc(rtsp, ssc);
  rs->rs_send_sock = fd[0];
  rs->rs_recv_sock = fd[1];
  rs->rs_sender = rtsp_send_udp;

  LIST_INSERT_HEAD(&rtsp->rtsp_streams, rs, rs_link);

  tvhlog(LOG_DEBUG, "RTSP", "%s: %s -- Stream %s to UDP port %d", 
	 inet_ntoa(hc->hc_peer->sin_addr), hc->hc_url_orig,
	 streaming_component_type2txt(ssc->ssc_type),
	 client_ports[0]);

  rtsp_printf(hc,
	      "RTSP/1.0 200 OK\r\n"
	      "Session: %s\r\n"
	      "Transport: RTP/AVP/UDP;unicast;client_port=%d-%d;"
	      "server_port=%d-%d\r\n",
	      rtsp->rtsp_session_id,
	      client_ports[0],
	      client_ports[1],
	      server_ports[0],
	      server_ports[1]);

  if((c = http_arg_get(&hc->hc_args, "cseq")) != NULL)
    rtsp_printf(hc, "CSeq: %s\r\n", c);
  rtsp_printf(hc, "\r\n");  
  return 0;
}


/**
 * RTSP SETUP
 */
static int
rtsp_cmd_setup(http_connection_t *hc, rtsp_t *rtsp)
{
  char *transports[10];
  char *params[10];
  char *t;
  int nt, i, np;
  int streamid;
  char *remain;
  const streaming_start_component_t *ssc;

  remain = strstr(hc->hc_url, "streamid=");
  if(remain == NULL) {
    rtsp_error(hc, RTSP_STATUS_SERVICE, "SETUP: URL does not resolve");
    return 0;
  }

  if(rtsp->rtsp_ss == NULL) {
    rtsp_error(hc, RTSP_STATUS_SERVICE, "Not described");
    return 0;
  }

  streamid = atoi(remain + strlen("streamid="));
  if((ssc = get_ssc_by_index(rtsp, streamid)) == NULL) {
    return rtsp_error(hc, RTSP_STATUS_SERVICE, "Stream not found");
  }

  if((t = http_arg_get(&hc->hc_args, "transport")) == NULL) {
    rtsp_error(hc, RTSP_STATUS_TRANSPORT, NULL);
    return 0;
  }

  nt = http_tokenize(t, transports, 10, ',');
  
  /* Select a transport we can accept */

  for(i = 0; i < nt; i++) {
    np = http_tokenize(transports[i], params, 10, ';');
    if(np == 0)
      continue;

    if(!strcasecmp(params[0], "RTP/AVP/UDP") ||
       !strcasecmp(params[0], "RTP/AVP"))
      return rtsp_setup_udp(hc, rtsp, ssc, params, np);

    if(!strcasecmp(params[0], "RTP/AVP/TCP"))
      return rtsp_setup_tcp(hc, rtsp, ssc, params, np);
  }

  rtsp_error(hc, RTSP_STATUS_TRANSPORT, NULL);
  return 0;
}


/**
 * RTSP PLAY
 */
static int
rtsp_cmd_play(http_connection_t *hc, rtsp_t *rtsp)
{
  char *c;

  if(rtsp_check_session(hc, rtsp, 0))
    return rtsp_error(hc, RTSP_STATUS_SERVICE, "Invalid session ID");

  rtsp->rtsp_running = 1;

  rtsp_printf(hc,
	      "RTSP/1.0 200 OK\r\n"
	      "Session: %s\r\n",
	      rtsp->rtsp_session_id);

  if((c = http_arg_get(&hc->hc_args, "cseq")) != NULL)
    rtsp_printf(hc, "CSeq: %s\r\n", c);
  
  rtsp_printf(hc, "\r\n");
  return 0;
}
  
/**
 * RTSP TEARDOWN
 */
static int
rtsp_cmd_teardown(http_connection_t *hc, rtsp_t *rtsp)
{
  char *c;

  if(rtsp_check_session(hc, rtsp, 0))
    return rtsp_error(hc, RTSP_STATUS_SERVICE, "Invalid session ID");

  assert(hc->hc_rtsp_session == rtsp);

  rtsp_printf(hc,
	      "RTSP/1.0 200 OK\r\n");

  if((c = http_arg_get(&hc->hc_args, "cseq")) != NULL)
    rtsp_printf(hc, "CSeq: %s\r\n", c);
  
  rtsp_printf(hc, "\r\n");

  pthread_mutex_lock(&global_lock);
  rtsp_destroy_unref(hc->hc_rtsp_session);
  hc->hc_rtsp_session = NULL;
  pthread_mutex_unlock(&global_lock);
  return 0;
}
  


/*
 * RTSP connection state machine & parser
 */
int 
rtsp_process_request(http_connection_t *hc)
{
  int r;
  if(http_access_verify(hc, ACCESS_STREAMING)) {
    rtsp_error(hc, RTSP_STATUS_UNAUTHORIZED, NULL);
    r = 0;
  } else {
    rtsp_t *rtsp = rtsp_get_session(hc);

    switch(hc->hc_cmd) {
    case RTSP_CMD_OPTIONS:   
      r = rtsp_cmd_options(hc);
      break;
    case RTSP_CMD_DESCRIBE:
      rtsp_cmd_describe(hc, rtsp);
      break;
     case RTSP_CMD_SETUP:
      rtsp_cmd_setup(hc, rtsp);
      break;
    case RTSP_CMD_PLAY:
      rtsp_cmd_play(hc, rtsp);
      break;
    case RTSP_CMD_TEARDOWN: 
      rtsp_cmd_teardown(hc, rtsp);
      break;
    case RTSP_CMD_PAUSE:
      rtsp_error(hc, RTSP_STATUS_METHOD, "Pause is not supported");
      break;
    default:
      rtsp_error(hc, RTSP_STATUS_METHOD, NULL);
      break;
    }
  }

  if(!r)
    tcp_write_queue(hc->hc_fd, &hc->hc_reply);

  return 0;
}

/*
 *
 */
void
rtsp_disconncet(http_connection_t *hc)
{
  pthread_mutex_lock(&global_lock);
  if(hc->hc_rtsp_session != NULL)
    rtsp_destroy_unref(hc->hc_rtsp_session);
  pthread_mutex_unlock(&global_lock);
}
