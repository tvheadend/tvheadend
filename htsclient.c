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

struct client_list all_clients;


static void client_status_update(void *aux);

static void
cprintf(client_t *c, const char *fmt, ...)
{
  va_list ap;
  char buf[5000];

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  write(c->c_fd, buf, strlen(buf));
}


static void 
client_ip_streamer(struct th_subscription *s, uint8_t *pkt, th_pid_t *pi,
		   int64_t pcr)
{
  client_t *c = s->ths_opaque;
  char stoppkt[4];
  struct sockaddr_in sin;


  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(c->c_port);
  sin.sin_addr = c->c_ipaddr;
 
  if(pkt == NULL) {
    stoppkt[0] = HTSTV_EOS;
    stoppkt[1] = s->ths_channel->ch_index;
    free(s->ths_pkt);
    s->ths_pkt = NULL;

    sendto(c->c_streamfd, stoppkt, 2, 0, 
	   (struct sockaddr *)&sin, sizeof(sin));
    return;
  }

  if(s->ths_pkt == NULL) {
    s->ths_pkt = malloc(c->c_pkt_maxsiz + 2);
    s->ths_pkt_ptr = 2;
  }

  memcpy(s->ths_pkt + s->ths_pkt_ptr, pkt, 188);
  s->ths_pkt[s->ths_pkt_ptr] = pi->tp_type;
  s->ths_pkt_ptr += 188;
  
  if(s->ths_pkt_ptr == c->c_pkt_maxsiz + 2) {
    
    s->ths_pkt[0] = HTSTV_TRANSPORT_STREAM;
    s->ths_pkt[1] = s->ths_channel->ch_index;
    
    sendto(c->c_streamfd, s->ths_pkt, s->ths_pkt_ptr, 0, 
	   (struct sockaddr *)&sin, sizeof(sin));
    s->ths_pkt_ptr = 2;
  }
}

/*
 *
 *
 */

void
clients_enq_ref(int ref)
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
  th_dvb_mux_t *tdm;
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
    
    
    TAILQ_FOREACH(ch, &channels, ch_global_link) {

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

  if(!strcasecmp(subcmd, "dvbmuxes")) {
    LIST_FOREACH(tdm, &dvb_muxes, tdm_global_link) {

      cprintf(c, "\"%s\"\n", tdm->tdm_title);
      
      LIST_FOREACH(tdmi, &tdm->tdm_instances, tdmi_mux_link) {
	cprintf(c, "%20s:   ", tdmi->tdmi_adapter->tda_path);

	print_tdmi(c, tdmi);


      }
    }
    return 0;
  }

  if(!strcasecmp(subcmd, "dvbadapters")) {
    LIST_FOREACH(tda, &dvb_adapters_running, tda_link) {

      cprintf(c, "%20s:   ", tda->tda_path);

      tdmi = tda->tda_mux_current;

      if(tdmi == NULL) {
	cprintf(c, "inactive\n");
	continue;
      }

      cprintf(c, "Tuned to \"%s\"\n", tdmi->tdmi_mux->tdm_title);
      cprintf(c, "\t\t     ");

      print_tdmi(c, tda->tda_mux_current);
      cprintf(c, "\n");
    }
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
 *
 */

int
cr_channel_subscribe(client_t *c, char **argv, int argc)
{
  th_channel_t *ch;
  th_subscription_t *s;
  unsigned int chindex, weight;

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

  s = channel_subscribe(ch, c, client_ip_streamer, weight, "client");
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

  TAILQ_FOREACH(ch, &channels, ch_global_link)
    cprintf(c, "channel = %d\n", ch->ch_index);

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
	 c->c_title, inet_ntoa(c->c_ipaddr), c->c_port);

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
	  pvr_prog_status(e));

  epg_unlock();
  return 0;
}

/*
 *
 */

static struct strtab recoptab[] = {
  { "once",   RECOP_ONCE },
  { "daily",  RECOP_DAILY },
  { "weekly", RECOP_WEEKLY },
  { "cancel", RECOP_CANCEL },
  { "toggle", RECOP_TOGGLE }
};

static int
cr_event_record(client_t *c, char **argv, int argc)
{
  event_t *e;
  recop_t op;

  if(argc < 2)
    return 1;

  op = str2val(argv[1], recoptab);
  if(op == -1)
    return 1;

  epg_lock();

  e = epg_event_find_by_tag(atoi(argv[0]));
  if(e == NULL) {
    epg_unlock();
    return 1;
  }

  pvr_event_record_op(e->e_ch, e, op);

  epg_unlock();
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
  { "event.info", cr_event_info },
  { "event.record", cr_event_record },
  { "pvr.getlog", cr_pvr_getlog },
  { "pvr.gettag", cr_pvr_gettag },
};


static void
client_req(client_t *c, char *buf)
{
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

      return;
    }
  }
  cprintf(c, "eom nocommand\n");
}

/*
 * client error
 */
static void
client_teardown(client_t *c, int err)
{
  th_subscription_t *s;

  syslog(LOG_INFO, "%s disconnected -- %s", c->c_title, strerror(err));

  stimer_del(c->c_status_timer);

  dispatch_delfd(c->c_dispatch_handle);

  close(c->c_fd);

  if(c->c_streamfd != -1)
    close(c->c_streamfd);

  LIST_REMOVE(c, c_global_link);

  while((s = LIST_FIRST(&c->c_subscriptions)) != NULL) {
    LIST_REMOVE(s, ths_subscriber_link);
    subscription_unsubscribe(s);
  }
  free(c->c_title);
  free(c);
}

/*
 * data available on socket
 */
static void
client_data_read(client_t *c)
{
  int space = sizeof(c->c_input_buf) - c->c_input_buf_ptr - 1;
  int r, cr = 0, i;
  char buf[100];

  if(space < 1) {
    client_teardown(c, EBADMSG);
    return;
  }

  r = read(c->c_fd, c->c_input_buf + c->c_input_buf_ptr, space);
  if(r < 0) {
    client_teardown(c, errno);
    return;
  }

  if(r == 0) {
    client_teardown(c, ECONNRESET);
    return;
  }

  c->c_input_buf_ptr += r;
  c->c_input_buf[c->c_input_buf_ptr] = 0;

  while(1) {
    cr = 0;

    for(i = 0; i < c->c_input_buf_ptr; i++)
      if(c->c_input_buf[i] == 0xa)
	break;

    if(i == c->c_input_buf_ptr)
      break;

    memcpy(buf, c->c_input_buf, i);
    buf[i] = 0;
    i++;
    memmove(c->c_input_buf, c->c_input_buf + i, sizeof(c->c_input_buf) - i);
    c->c_input_buf_ptr -= i;

    i = strlen(buf);
    while(i > 0 && buf[i-1] < 32)
      buf[--i] = 0;
    //    printf("buf = |%s|\n", buf);
    client_req(c, buf);
  }
}


/*
 * dispatcher callback
 */
static void
client_socket_callback(int events, void *opaque, int fd)
{
  client_t *c = opaque;

  if(events & DISPATCH_ERR) {
    client_teardown(c, ECONNRESET);
    return;
  }

  if(events & DISPATCH_READ)
    client_data_read(c);
}



/*
 *
 */
static void
client_connect_callback(int events, void *opaque, int fd)
{
  struct sockaddr_in from;
  socklen_t socklen = sizeof(struct sockaddr_in);
  int newfd;
  int val;
  client_t *c;
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

  c = calloc(1, sizeof(client_t));
  c->c_fd = newfd;
  c->c_pkt_maxsiz = 188 * 7;
  TAILQ_INIT(&c->c_refq);
  LIST_INSERT_HEAD(&all_clients, c, c_global_link);
  c->c_streamfd = -1; //socket(AF_INET, SOCK_DGRAM, 0);

  snprintf(txt, sizeof(txt), "%s:%d",
	   inet_ntoa(from.sin_addr), ntohs(from.sin_port));

  c->c_title = strdup(txt);

  syslog(LOG_INFO, "Got TCP connection from %s", c->c_title);

  c->c_dispatch_handle = dispatch_addfd(newfd, client_socket_callback, c, 
					DISPATCH_READ);

  c->c_status_timer = stimer_add(client_status_update, c, 1);
}



/*
 *  Fire up client handling
 */

void
client_start(void)
{
  struct sockaddr_in sin;
  int s;
  int one = 1;
  s = socket(AF_INET, SOCK_STREAM, 0);
  memset(&sin, 0, sizeof(sin));

  sin.sin_family = AF_INET;
  sin.sin_port = htons(9909);

  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

  fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);

  syslog(LOG_INFO, "Listening for TCP connections on %s:%d",
	 inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

  if(bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    syslog(LOG_ERR, "Unable to bind socket for incomming TCP connections"
	   "%s:%d -- %s",
	   inet_ntoa(sin.sin_addr), ntohs(sin.sin_port),
	   strerror(errno));
    return;
  }

  listen(s, 1);
  dispatch_addfd(s, client_connect_callback, NULL, DISPATCH_READ);
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
client_status_update(void *aux)
{
  client_t *c = aux;
  th_channel_t *ch;
  th_dvb_adapter_t *tda;
  th_v4l_adapter_t *v4l;
  th_dvb_mux_instance_t *tdmi;
  th_subscription_t *s;
  th_transport_t *t;
  int ccerr, rate;

  c->c_status_timer = stimer_add(client_status_update, c, 1);

  LIST_FOREACH(s, &c->c_subscriptions, ths_subscriber_link) {

    ch = s->ths_channel;
    t = s->ths_transport;

    if(t == NULL) {
      csprintf(c, ch, 
	       "status = 0\n"
	       "info = No transport available");
      continue;
    }

    ccerr = avgstat_read(&t->tht_cc_errors, 60, dispatch_clock);
    rate = avgstat_read_and_expire(&t->tht_rate, dispatch_clock);
    rate = rate * 8 / 1000 / 10; /* convert to kbit / s */

    switch(t->tht_type) {
    case TRANSPORT_DVB:
      if((tda = t->tht_dvb_adapter) == NULL) {
	csprintf(c, ch, 
		 "status = 0\n"
		 "info = No adapter available"
		 "transport = %s\n",
		 t->tht_name);
	break;
      }
      if((tdmi = tda->tda_mux_current) == NULL) {
	csprintf(c, ch,
		 "status = 0\n"
		 "info = No mux available"
		 "transport = %s\n",
		 t->tht_name);
	break;
      }

      if(tdmi->tdmi_status == NULL) {
	csprintf(c, ch, 
		 "status = 1\n"
		 "info = Signal ok\n"
		 "adapter = %s\n"
		 "transport = %s\n"
		 "uncorrected-blocks = %d\n"
		 "rate = %d\n"
		 "cc-errors = %d\n",
		 tda->tda_name,
		 t->tht_name,
		 tdmi->tdmi_uncorrected_blocks,
		 rate, ccerr);
		   
		   
      } else {
	csprintf(c, ch, 
		 "status = 0"
		 "info = %s"
		 "adapter = %s\n"
		 "transport = %s\n",
		 tdmi->tdmi_status,
		 tda->tda_name,
		 t->tht_name);
      }
      break;

    case TRANSPORT_IPTV:
      csprintf(c, ch, 
	       "status = 1\n"
	       "info = Signal ok\n"
	       "transport = %s\n"
	       "rate = %d\n"
	       "cc-errors = %d\n",
	       t->tht_name,
	       rate, ccerr);
      break;

    case TRANSPORT_V4L:
      v4l = t->tht_v4l_adapter;
      if(v4l == NULL) {
	csprintf(c, ch, 
		 "status = 0\n"
		 "info = No adapter available"
		 "transport = %s\n",
		 t->tht_name);
	continue;
      }

      csprintf(c, ch, 
	       "status = 1\n"
	       "info = Signal ok\n"
	       "adapter = %s\n"
	       "transport = %s\n"
	       "rate = %d\n"
	       "cc-errors = %d\n",
	       v4l->tva_name,
	       t->tht_name,
	       rate, ccerr);
      break;

    }
  }
}
