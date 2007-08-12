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

struct client_list all_clients;


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
client_ip_streamer(struct th_subscription *s, uint8_t *pkt, pidinfo_t *pi,
		   int streamindex)
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

static int
cr_show(client_t *c, char **argv, int argc)
{
  const char *subcmd;
  th_subscription_t *s;
  th_transport_t *t;
  th_channel_t *ch;
  programme_t *p;
  int chid = 0;
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
    
    while((ch = channel_by_id(chid++)) != NULL) {

      pthread_mutex_lock(&ch->ch_epg_mutex);
      
      p = epg_get_cur_prog(ch);

      txt = p && p->pr_title ? p->pr_title: "<no current programme>";
      remain = p ? p->pr_stop - time(NULL) : 0;
      remain /= 60;

      cprintf(c, "%3d: \"%s\":   \"%s\"  %d:%02d remaining\n", 
	      ch->ch_index, ch->ch_name, txt, remain / 60, remain % 60);

      pthread_mutex_unlock(&ch->ch_epg_mutex);

      LIST_FOREACH(t, &ch->ch_transports, tht_channel_link) {

	cprintf(c, "\tTransport: %-35s", t->tht_name);

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

  return 1;
}




/*
 *
 */

static int
cr_channels_total(client_t *c, char **argv, int argc)
{
  cprintf(c, "%d\n", channel_get_channels());
  return 0;
}
 
/*
 *
 */

static int
cr_channel_reftag(client_t *c, char **argv, int argc)
{
  th_channel_t *ch;

  if(argc != 1 || (ch = channel_by_id(atoi(argv[0]))) == NULL)
    return 1;

  cprintf(c, "%d\n", ch->ch_ref);
  return 0;
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
  
  if((ch = channel_by_id(atoi(argv[0]))) == NULL)
    return 1;

  cprintf(c,
	  "displayname = %s\n"
	  "icon = %s\n"
	  "reftag = %d\n",
	  ch->ch_name,
	  ch->ch_icon ?: "",
	  ch->ch_ref);

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

  if((ch = channel_by_id(chindex)) == NULL)
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
cr_programme_info(client_t *c, char **argv, int argc)
{
  th_channel_t *ch;
  programme_t *pr;

  if(argc < 1)
    return 1;

  if((ch = channel_by_id(atoi(argv[0]))) == NULL)
    return 1;

  pthread_mutex_lock(&ch->ch_epg_mutex);

  pr = argc == 2 ? epg_get_prog_by_id(ch, atoi(argv[1])) : 
    epg_get_cur_prog(ch);

  if(pr == NULL) {
    pthread_mutex_unlock(&ch->ch_epg_mutex);
    return 1;
  }

  cprintf(c,
	  "index = %d\n"
	  "title = %s\n"
	  "start = %ld\n"
	  "stop = %ld\n"
	  "desc = %s\n"
	  "reftag = %d\n"
	  "pvrstatus = %c\n",
	  pr->pr_index,
	  pr->pr_title ?: "",
	  pr->pr_start,
	  pr->pr_stop,
	  pr->pr_desc ?: "",
	  pr->pr_ref,
	  pvr_prog_status(pr));

  pthread_mutex_unlock(&ch->ch_epg_mutex);
  return 0;
}

/*
 *
 */

static int
cr_programme_bytime(client_t *c, char **argv, int argc)
{
  th_channel_t *ch;
  programme_t *pr;
  time_t t;

  if(argc < 2)
    return 1;

  if((ch = channel_by_id(atoi(argv[0]))) == NULL)
    return 1;

  t = atoi(argv[1]);

  pthread_mutex_lock(&ch->ch_epg_mutex);

  pr = epg_find_programme_by_time(ch, t);
  if(pr == NULL) {
    pthread_mutex_unlock(&ch->ch_epg_mutex);
    return 1;
  }

  cprintf(c, "%d\n", pr->pr_index);
  pthread_mutex_unlock(&ch->ch_epg_mutex);
  return 0;
}

/*
 *
 */

static int
cr_programme_record(client_t *c, char **argv, int argc)
{
  th_channel_t *ch;
  programme_t *pr;

  if(argc < 1)
    return 1;

  if((ch = channel_by_id(atoi(argv[0]))) == NULL)
    return 1;

  pthread_mutex_lock(&ch->ch_epg_mutex);

  pr = argc == 2 ? epg_get_prog_by_id(ch, atoi(argv[1])) : 
    epg_get_cur_prog(ch);


  if(pr == NULL) {
    pthread_mutex_unlock(&ch->ch_epg_mutex);
    return 1;
  }

  pvr_add_recording_by_pr(ch, pr);
  pthread_mutex_unlock(&ch->ch_epg_mutex);
  return 0;
}

/*
 *
 */
static int
cr_pvr_entry(client_t *c, pvr_rec_t *pvrr)
{
  programme_t *pr;

  if(pvrr == NULL)
    return 1;

  cprintf(c,
	  "title = %s\n"
	  "start = %ld\n"
	  "stop = %ld\n"
	  "desc = %s\n"
	  "reftag = %d\n"
	  "pvrstatus = %c\n"
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


  pr = epg_find_programme(pvrr->pvrr_channel, pvrr->pvrr_start,
			  pvrr->pvrr_stop);
  if(pr != NULL)
    cprintf(c, "index = %d\n", pr->pr_index);

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
  { "channels.total", cr_channels_total },
  { "channel.reftag", cr_channel_reftag },
  { "channel.info", cr_channel_info },
  { "channel.subscribe", cr_channel_subscribe },
  { "channel.unsubscribe", cr_channel_unsubscribe },
  { "programme.info", cr_programme_info },
  { "programme.bytime", cr_programme_bytime },
  { "programme.record", cr_programme_record },
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
    memmove(c->c_input_buf, c->c_input_buf + i, sizeof(c->c_input_buf) - 1);
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
  size_t socklen = sizeof(struct sockaddr_in);
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


void
client_status_update(void)
{
  client_t *c;
  th_channel_t *ch;
  th_dvb_adapter_t *dvb;
  th_v4l_adapter_t *v4l;
  const char *info;
  th_subscription_t *s;
  th_transport_t *t;
  int ccerr;

  LIST_FOREACH(c, &all_clients, c_global_link) {
    if(c->c_streamfd == -1)
      continue;

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

      switch(t->tht_type) {
      case TRANSPORT_DVB:
	dvb = t->tht_dvb_adapter;
	if(dvb == NULL) {
	  csprintf(c, ch, 
		  "status = 0\n"
		  "info = No adapter available"
		  "transport = %s\n",
		  t->tht_name);
	  continue;
	}


	if(dvb->tda_fe_status & FE_HAS_LOCK) {
	  csprintf(c, ch, 
		  "status = 1\n"
		  "info = Signal ok\n"
		  "adapter = %s\n"
		  "transport = %s\n"
		  "signal-strength = 0x%04x\n"
		  "snr = %d\n"
		  "ber = %d\n"
		  "uncorrected-blocks = %d\n"
		  "cc-errors = %d\n",
		  dvb->tda_name,
		  t->tht_name,
		  dvb->tda_signal,
		  (dvb->tda_snr & 0xff) * 100 / 256,
		  dvb->tda_ber,
		  dvb->tda_uncorrected_blocks,
		  ccerr);
	  continue;
	} else if(dvb->tda_fe_status & FE_HAS_SYNC)
	  info = "No lock, but sync ok";
	else if(dvb->tda_fe_status & FE_HAS_VITERBI)
	  info = "No lock, but FEC stable";
	else if(dvb->tda_fe_status & FE_HAS_CARRIER)
	  info = "No lock, but carrier seen";
	else if(dvb->tda_fe_status & FE_HAS_SIGNAL)
	  info = "No lock, but faint signal";
	else 
	  info = "No signal";
    
	csprintf(c, ch, 
		"status = 0"
		"info = %s"
		"adapter = %s\n"
		"transport = %s\n",
		info,
		dvb->tda_name,
		t->tht_name);
	break;


      case TRANSPORT_IPTV:
	csprintf(c, ch, 
		"status = 1\n"
		"info = Signal ok\n"
		"transport = %s\n"
		"cc-errors = %d\n",
		t->tht_name,
		ccerr);
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
		"cc-errors = %d\n",
		v4l->tva_name,
		t->tht_name,
		ccerr);
	break;

      }
    }
  }
}
