/*
 *  tvheadend, (simple) client interface
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

#include <assert.h>
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
#include "subscriptions.h"
#include "pvr.h"
#include "epg.h"
#include "teletext.h"
#include "dispatch.h"
#include "dvb.h"
#include "buffer.h"
#include "tsmux.h"
#include "tcp.h"

LIST_HEAD(client_list, client);

struct client_list all_clients;


/*
 * Client
 */

typedef struct client {
  tcp_session_t c_tcp_session;

  LIST_ENTRY(client) c_global_link;
  int c_streamfd;
  pthread_t c_ptid;
  
  LIST_HEAD(, th_subscription) c_subscriptions;

  struct in_addr c_ipaddr;
  int c_port;

  struct ref_update_queue c_refq;

  dtimer_t c_status_timer;

  void *c_muxer;

} client_t;



static void client_status_update(void *aux, int64_t now);

#define cprintf(c, fmt...) tcp_printf(&(c)->c_tcp_session, fmt)


void
client_output_ts(void *opaque, th_subscription_t *s, 
		 uint8_t *pkt, int blocks, int64_t pcr)
{
  struct msghdr msg;
  struct iovec vec[2];
  int r;
  client_t *c = opaque;
  struct sockaddr_in sin;
  char hdr[2];


  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(c->c_port);
  sin.sin_addr = c->c_ipaddr;

  hdr[0] = HTSTV_TRANSPORT_STREAM;
  hdr[1] = s->ths_channel->ch_index;

  vec[0].iov_base = hdr;
  vec[0].iov_len  = 2;
  vec[1].iov_base = pkt;
  vec[1].iov_len  = blocks * 188;

  memset(&msg, 0, sizeof(msg));
  msg.msg_name    = &sin;
  msg.msg_namelen = sizeof(struct sockaddr_in);
  msg.msg_iov     = vec;
  msg.msg_iovlen  = 2;

  r = sendmsg(c->c_streamfd, &msg, 0);
  if(r < 0)
    perror("sendmsg");
}



/*
 *
 *
 */

void
clients_send_ref(int ref)
{
  client_t  *c;
  char buf[10];
  uint32_t v = htonl(ref);
  struct sockaddr_in sin;
   
  buf[0] = HTSTV_REFTAG;
  memcpy(buf + 1, &v, sizeof(uint32_t));

  LIST_FOREACH(c, &all_clients, c_global_link) {
    if(c->c_streamfd == -1)
      continue;
	 
    sin.sin_family = AF_INET;
    sin.sin_port = htons(c->c_port);
    sin.sin_addr = c->c_ipaddr;

    sendto(c->c_streamfd, buf, 5, 0,
	   (struct sockaddr *)&sin, sizeof(sin));
  }
}

/*
 *
 */
static void
print_tdmi(client_t *c, th_dvb_mux_instance_t *tdmi)
{
  switch(tdmi->tdmi_state) {
  case TDMI_CONFIGURED:
    cprintf(c, "Configured, awaiting scan slot\n");
    return;

  case TDMI_INITIAL_SCAN:
    cprintf(c, "Initial scan\n");
    return;

  case TDMI_IDLE:
    cprintf(c, "   Idle since %s", ctime(&tdmi->tdmi_lost_adapter));
    cprintf(c, "\tLast known status: ");
    break;

  case TDMI_RUNNING:
    cprintf(c, "Running since %s", ctime(&tdmi->tdmi_got_adapter));
    cprintf(c, "\t   Current status: ");
    break;

  case TDMI_IDLESCAN:
    cprintf(c, "Idle but scanning\n");
    cprintf(c, "\t   Current status: ");
    break;
  }

  if(tdmi->tdmi_status != NULL) {
    cprintf(c, "%s\n", tdmi->tdmi_status);
    return;
  }

  cprintf(c, "locked, %d errors / second\n", tdmi->tdmi_fec_err_per_sec);
}

/*
 *
 */
static int
cr_show(client_t *c, char **argv, int argc)
{
  const char *subcmd;
  th_subscription_t *s;
  th_transport_t *t;
  th_channel_t *ch;
  th_dvb_adapter_t *tda;
  th_dvb_mux_instance_t *tdmi;
  event_t *e;
  char *tmp;
  char *txt;
  int v, remain;

  if(argc != 1)
    return 1;
  
  subcmd = argv[0];

  if(!strcasecmp(subcmd, "subscriptions")) {
    cprintf(c, "prio %-18s %-20s %s\n", "output", "channel", "source");
    cprintf(c, "-----------------------------------------------------------------------------\n");

    LIST_FOREACH(s, &subscriptions, ths_global_link) {

      cprintf(c, "%4d %-18s %-20s %s\n", 
	      s->ths_weight,
	      s->ths_title, s->ths_channel->ch_name,
	      s->ths_transport ? s->ths_transport->tht_name : "no transport");
    }
    return 0;
  }

  if(!strcasecmp(subcmd, "channel")) {
    
    LIST_FOREACH(ch, &channels, ch_global_link) {

      tmp = utf8toprintable(ch->ch_name);
      cprintf(c, "%3d: \"%s\"\n", ch->ch_index, tmp);
      free(tmp);

      epg_lock();

      e = epg_event_get_current(ch);

      if(e != NULL) {

	tmp = utf8toprintable(e->e_title ?: "<no current event>");

	remain = e->e_start + e->e_duration - time(NULL);
	remain /= 60;

	switch(e->e_source) {
	case EVENT_SRC_XMLTV:
	  txt = "xmltv";
	  break;
	case EVENT_SRC_DVB:
	  txt = "dvb";
	  break;
	default:
	  txt = "???";
	  break;
	}

	cprintf(c, "\tNow: %-40s %2d:%02d [%s] tag: %d\n", 
		tmp, remain / 60, remain % 60, txt, e->e_tag);
	free(tmp);
      }

      epg_unlock();

      LIST_FOREACH(t, &ch->ch_transports, tht_channel_link) {

	cprintf(c, "\t%-47s", t->tht_name);

	switch(t->tht_status) {
	case TRANSPORT_IDLE:
	  cprintf(c, "idle\n");
	  break;

	case TRANSPORT_RUNNING:
	  v = avgstat_read_and_expire(&t->tht_rate, dispatch_clock);
	  cprintf(c, "running (%d kb/s)\n",
		  v * 8 / 1000 / 10);
	  break;

	default:
	  continue;
	}

	v = avgstat_read(&t->tht_cc_errors, 60, dispatch_clock);

	if(v)
	  cprintf(c, "\t\t%d error%s last minute   %f / second\n",
		  v, v == 1 ? "" : "s", v / 60.);

	v = avgstat_read_and_expire(&t->tht_cc_errors, dispatch_clock);
      
	if(v)
	  cprintf(c, "\t\t%d error%s last hour     %f / second\n",
		  v, v == 1 ? "" : "s", v / 3600.);

	LIST_FOREACH(s, &t->tht_subscriptions, ths_transport_link) {
	  cprintf(c, "\t\t%s @ prio %d, since %s",
		  s->ths_title, s->ths_weight, ctime(&s->ths_start));
	  if(s->ths_total_err) {
	    cprintf(c,"\t\t\t%d error%s seen\n",
		    s->ths_total_err, s->ths_total_err == 1 ? "" : "s");
	  }
	}
      }
      cprintf(c, "\n");
 

    }
    return 0;
  }

  if(!strcasecmp(subcmd, "dvbadapters")) {
    LIST_FOREACH(tda, &dvb_adapters_running, tda_link) {

      cprintf(c, "%20s:   ", tda->tda_rootpath);

      tdmi = tda->tda_mux_current;

      if(tdmi == NULL) {
	cprintf(c, "inactive\n");
	continue;
      }

      cprintf(c, "Tuned to \"%s\"\n", tdmi->tdmi_shortname);
      cprintf(c, "\t\t     ");

      print_tdmi(c, tda->tda_mux_current);
      cprintf(c, "\n");
    }
    return 0;
  }

  if(!strcasecmp(subcmd, "storage")) {
    cprintf(c, "In-memory storage %lld / %lld\n", 
	    store_mem_size, store_mem_size_max);

    cprintf(c, "  On-disk storage %lld / %lld\n", 
	    store_disk_size, store_disk_size_max);

    cprintf(c, "  %d packets in memory\n",
	    store_packets);
    return 0;
  }

  return 1;
}


/*
 *
 */

static int
cr_channel_info(client_t *c, char **argv, int argc)
{
  th_channel_t *ch;

  if(argc < 1)
    return 1;
  
  if((ch = channel_by_index(atoi(argv[0]))) == NULL)
    return 1;

  cprintf(c,
	  "displayname = %s\n"
	  "icon = %s\n"
	  "tag = %d\n",
	  ch->ch_name,
	  ch->ch_icon ? refstr_get(ch->ch_icon) : "",
	  ch->ch_tag);

  return 0;
}


/*
 *
 */

int
cr_channel_unsubscribe(client_t *c, char **argv, int argc)
{
  th_subscription_t *s;
  int chindex;

  if(argc < 1)
    return 1;

  chindex = atoi(argv[0]);

  LIST_FOREACH(s, &c->c_subscriptions, ths_subscriber_link) {
    if(s->ths_channel->ch_index == chindex)
      break;
  }

  if(s == NULL)
    return 1;

  LIST_REMOVE(s, ths_subscriber_link);
  subscription_unsubscribe(s);
  return 0;
}


/*
 * Called when a subscription gets/loses access to a transport
 */
static void
client_subscription_callback(struct th_subscription *s,
			     subscription_event_t event, void *opaque)
{
  client_t *c = opaque;

  switch(event) {
  case TRANSPORT_AVAILABLE:
    assert(c->c_muxer == NULL);
    c->c_muxer = ts_muxer_init(s, client_output_ts, c,
			       TS_HTSCLIENT | TS_SEEK, 0);
    ts_muxer_play(c->c_muxer, 0);
    break;

  case TRANSPORT_UNAVAILABLE:
    assert(c->c_muxer != NULL);
    ts_muxer_deinit(c->c_muxer, s);
    c->c_muxer = NULL;
    break;
  }
}

/*
 *
 */
int
cr_channel_subscribe(client_t *c, char **argv, int argc)
{
  th_channel_t *ch;
  th_subscription_t *s;
  unsigned int chindex, weight;
  char tmp[100];
  struct sockaddr_in *si;

  if(argc < 1)
    return 1;

  chindex = atoi(argv[0]);

  weight = argc > 1 ? atoi(argv[1]) : 100;

  LIST_FOREACH(s, &c->c_subscriptions, ths_subscriber_link) {
    if(s->ths_channel->ch_index == chindex) {
      subscription_set_weight(s, weight);
      return 0;
    }
  }

  if((ch = channel_by_index(chindex)) == NULL)
    return 1;

  si = (struct sockaddr_in *)&c->c_tcp_session.tcp_peer_addr;

  snprintf(tmp, sizeof(tmp), "HTS Client @ %s",
	   inet_ntoa(si->sin_addr));

  s = subscription_create(ch, weight, tmp, client_subscription_callback, c);
  if(s == NULL)
    return 1;

  LIST_INSERT_HEAD(&c->c_subscriptions, s, ths_subscriber_link);
  return 0;
}


/*
 *
 */

int
cr_channels_list(client_t *c, char **argv, int argc)
{
  th_channel_t *ch;

  LIST_FOREACH(ch, &channels, ch_global_link) {
    if(ch->ch_group == NULL)
      continue;
    cprintf(c, "channel = %d\n", ch->ch_index);
  }
  return 0;
}


/*
 *
 */

int
cr_streamport(client_t *c, char **argv, int argc)
{
  if(argc < 2)
    return 1;

  if(c->c_streamfd == -1)
    c->c_streamfd = socket(AF_INET, SOCK_DGRAM, 0);

  c->c_ipaddr.s_addr = inet_addr(argv[0]);
  c->c_port = atoi(argv[1]);

  syslog(LOG_INFO, "%s registers UDP stream target %s:%d",
	 tcp_logname(&c->c_tcp_session), inet_ntoa(c->c_ipaddr), c->c_port);

  return 0;
}

/*
 *
 */

static int
cr_event_info(client_t *c, char **argv, int argc)
{
  event_t *e = NULL, *x;
  uint32_t tag, prev, next;
  th_channel_t *ch;
  pvr_rec_t *pvrr;

  if(argc < 2)
    return 1;

  epg_lock();

  if(!strcasecmp(argv[0], "tag")) 
    e = epg_event_find_by_tag(atoi(argv[1]));
  if(!strcasecmp(argv[0], "now")) 
    if((ch = channel_by_index(atoi(argv[1]))) != NULL)
      e = epg_event_get_current(ch);
  if(!strcasecmp(argv[0], "at") && argc == 3) 
    if((ch = channel_by_index(atoi(argv[1]))) != NULL)
      e = epg_event_find_by_time(ch, atoi(argv[2]));

  if(e == NULL) {
    epg_unlock();
    return 1;
  }

  tag = e->e_tag;
  x = TAILQ_PREV(e, event_queue, e_link);
  prev = x != NULL ? x->e_tag : 0;

  x = TAILQ_NEXT(e, e_link);
  next = x != NULL ? x->e_tag : 0;

  pvrr = pvr_get_by_entry(e);

  cprintf(c,
	  "start = %ld\n"
	  "stop = %ld\n"
	  "title = %s\n"
	  "desc = %s\n"
	  "tag = %u\n"
	  "prev = %u\n"
	  "next = %u\n"
	  "pvrstatus = %d\n",

	  e->e_start,
	  e->e_start + e->e_duration,
	  e->e_title ?: "",
	  e->e_desc  ?: "",
	  tag,
	  prev,
	  next,
	  pvrr != NULL ? pvrr->pvrr_status : HTSTV_PVR_STATUS_NONE);

  epg_unlock();
  return 0;
}

/*
 *
 */

static int
cr_event_record(client_t *c, char **argv, int argc)
{
  event_t *e;

  if(argc < 1)
    return 1;

  epg_lock();

  e = epg_event_find_by_tag(atoi(argv[0]));
  if(e == NULL) {
    epg_unlock();
    return 1;
  }

  pvr_schedule_by_event(e, "htsclient");

  epg_unlock();
  return 0;
}




/*
 *
 */
static int
cr_channel_record(client_t *c, char **argv, int argc)
{
  th_channel_t *ch;
  int duration;

  if(argc < 2)
    return 1;

  if((ch = channel_by_index(atoi(argv[0]))) == NULL)
    return 1;

  duration = atoi(argv[1]);
  
  pvr_schedule_by_channel_and_time(ch, duration, "htsclient");
  return 0;
}

/*
 *
 */
static int
cr_pvr_entry(client_t *c, pvr_rec_t *pvrr)
{
  event_t *e;

  if(pvrr == NULL)
    return 1;

  cprintf(c,
	  "title = %s\n"
	  "start = %ld\n"
	  "stop = %ld\n"
	  "desc = %s\n"
	  "pvr_tag = %d\n"
	  "pvrstatus = %d\n"
	  "filename = %s\n"
	  "channel = %d\n",
	  pvrr->pvrr_title ?: "",
	  pvrr->pvrr_start,
	  pvrr->pvrr_stop,
	  pvrr->pvrr_desc ?: "",
	  pvrr->pvrr_ref,
	  pvrr->pvrr_status,
	  pvrr->pvrr_filename,
	  pvrr->pvrr_channel->ch_index);


  e = epg_event_find_by_time(pvrr->pvrr_channel, pvrr->pvrr_start);
  if(e != NULL)
    cprintf(c, "event_tag = %d\n", e->e_tag);

  return 0;
}

/*
 *
 */

static int
cr_pvr_getlog(client_t *c, char **argv, int argc)
{
  pvr_rec_t *pvrr;

  if(argc < 1)
    return 1;

  pvrr = pvr_get_log_entry(atoi(argv[0]));
  return cr_pvr_entry(c, pvrr);
}


/*
 *
 */

static int
cr_pvr_gettag(client_t *c, char **argv, int argc)
{
  pvr_rec_t *pvrr;

  if(argc < 1)
    return 1;

  pvrr = pvr_get_tag_entry(atoi(argv[0]));
  return cr_pvr_entry(c, pvrr);
}


/*
 *
 */

const struct {
  const char *name;
  int (*func)(client_t *c, char *argv[], int argc);
} cr_cmds[] = {
  { "show", cr_show },
  { "streamport", cr_streamport },
  { "channels.list", cr_channels_list },
  { "channel.info", cr_channel_info },
  { "channel.subscribe", cr_channel_subscribe },
  { "channel.unsubscribe", cr_channel_unsubscribe },
  { "channel.record", cr_channel_record },
  { "event.info", cr_event_info },
  { "event.record", cr_event_record },
  { "pvr.getlog", cr_pvr_getlog },
  { "pvr.gettag", cr_pvr_gettag },
};


static int
client_req(void *aux, char *buf)
{
  client_t *c = aux;
  int i, l, x;
  const char *n;
  char *argv[40];
  int argc = 0;

  for(i = 0; i < sizeof(cr_cmds) / sizeof(cr_cmds[0]); i++) {
    n = cr_cmds[i].name;
    l = strlen(n);
    if(!strncasecmp(buf, n, l) && (buf[l] == ' ' || buf[l] == 0)) {
      buf += l;

      while(*buf) {
	if(*buf < 33) {
	  buf++;
	  continue;
	}
	argv[argc++] = buf;
	while(*buf > 32)
	  buf++;
	if(*buf == 0)
	  break;
	*buf++ = 0;
      }
      x = cr_cmds[i].func(c, argv, argc);

      if(x >= 0)
	cprintf(c, "eom %s\n", x ? "error" : "ok");

      return 0;
    }
  }
  cprintf(c, "eom nocommand\n");
  return 0;
}

/*
 * client disconnect
 */
static void
client_disconnect(client_t *c)
{
  th_subscription_t *s;

  dtimer_disarm(&c->c_status_timer);

  if(c->c_streamfd != -1)
    close(c->c_streamfd);

  LIST_REMOVE(c, c_global_link);

  while((s = LIST_FIRST(&c->c_subscriptions)) != NULL) {
    LIST_REMOVE(s, ths_subscriber_link);
    subscription_unsubscribe(s);
  }
}


/*
 *
 */
static void
htsclient_tcp_callback(tcpevent_t event, void *tcpsession)
{
  client_t *c = tcpsession;

  switch(event) {
  case TCP_CONNECT:
    TAILQ_INIT(&c->c_refq);
    LIST_INSERT_HEAD(&all_clients, c, c_global_link);
    c->c_streamfd = -1;
    dtimer_arm(&c->c_status_timer, client_status_update, c, 1);
    break;

  case TCP_DISCONNECT:
    client_disconnect(c);
    break;

  case TCP_INPUT:
    tcp_line_read(&c->c_tcp_session, client_req);
    break;
  }
}


/*
 *  Fire up client handling
 */

void
client_start(void)
{
  tcp_create_server(9909, sizeof(client_t), "htsclient",
		    htsclient_tcp_callback);
}










/*
 *  Periodically send status updates to client (on stream 2)
 */
static void
csprintf(client_t *c, th_channel_t *ch, const char *fmt, ...)
{
  va_list ap;
  char buf[1000];
  struct sockaddr_in sin;

  memset(&sin, 0, sizeof(sin));

  buf[0] = HTSTV_STATUS;
  buf[1] = ch->ch_index;

  va_start(ap, fmt);
  vsnprintf(buf + 2, sizeof(buf) - 2, fmt, ap);
  va_end(ap);

  sin.sin_family = AF_INET;
  sin.sin_port = htons(c->c_port);
  sin.sin_addr = c->c_ipaddr;

  sendto(c->c_streamfd, buf, strlen(buf + 2) + 2, 0,
	 (struct sockaddr *)&sin, sizeof(sin));
}


static void
client_status_update(void *aux, int64_t now)
{
  client_t *c = aux;
  th_channel_t *ch;
  th_subscription_t *s;
  th_transport_t *t;

  dtimer_arm(&c->c_status_timer, client_status_update, c, 1);

  LIST_FOREACH(s, &c->c_subscriptions, ths_subscriber_link) {

    ch = s->ths_channel;
    t = s->ths_transport;

    if(t == NULL) {
      csprintf(c, ch, 
	       "status = 0\n"
	       "info = No transport available");
      continue;
    }

    csprintf(c, ch,
	     "status = 0\n"
	     "info = Running");
  }
}
