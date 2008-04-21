/*
 *  Multicasted IPTV Input
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <linux/netdevice.h>

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "iptv_input.h"
#include "channels.h"
#include "transports.h"
#include "dispatch.h"
#include "psi.h"
#include "tsdemux.h"

struct th_transport_list iptv_probing_transports;
struct th_transport_list iptv_stale_transports;
static dtimer_t iptv_probe_timer;

static void iptv_probe_transport(th_transport_t *t);
static void iptv_probe_callback(void *aux, int64_t now);
static void iptv_probe_done(th_transport_t *t, int timeout);

static void
iptv_fd_callback(int events, void *opaque, int fd)
{
  th_transport_t *t = opaque;
  uint8_t buf[2000];
  int r;
  uint8_t *tsb = buf;

  r = read(fd, buf, sizeof(buf));

  while(r >= 188) {
    ts_recv_packet1(t, tsb);
    r -= 188;
    tsb += 188;
  }
}

static int
iptv_start_feed(th_transport_t *t, unsigned int weight, int status)
{
  int fd;
  struct ip_mreqn m;
  struct sockaddr_in sin;

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if(fd == -1)
    return -1;

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(t->tht_iptv_port);
  sin.sin_addr.s_addr = t->tht_iptv_group_addr.s_addr;

  if(bind(fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
    syslog(LOG_ERR, "iptv: \"%s\" cannot bind %s:%d -- %s", 
	   t->tht_name, inet_ntoa(sin.sin_addr), t->tht_iptv_port,
	   strerror(errno));
    close(fd);
    return -1;
  }

  memset(&m, 0, sizeof(m));
  m.imr_multiaddr.s_addr = t->tht_iptv_group_addr.s_addr;
  m.imr_address.s_addr = t->tht_iptv_interface_addr.s_addr;
  m.imr_ifindex = t->tht_iptv_ifindex;

  if(setsockopt(fd, SOL_IP, IP_ADD_MEMBERSHIP, &m, 
		sizeof(struct ip_mreqn)) == -1) {
    syslog(LOG_ERR, "iptv: \"%s\" cannot join %s -- %s", 
	   t->tht_name, inet_ntoa(m.imr_multiaddr),
	   strerror(errno));
    close(fd);
    return -1;
  }

  t->tht_iptv_fd = fd;
  t->tht_status = status;

  syslog(LOG_ERR, "iptv: \"%s\" joined group", t->tht_name);

  t->tht_iptv_dispatch_handle = dispatch_addfd(fd, iptv_fd_callback, t,
					       DISPATCH_READ);
  return 0;
}

static void
iptv_stop_feed(th_transport_t *t)
{
  if(t->tht_status == TRANSPORT_IDLE)
    return;

  t->tht_status = TRANSPORT_IDLE;
  dispatch_delfd(t->tht_iptv_dispatch_handle);
  close(t->tht_iptv_fd);

  syslog(LOG_ERR, "iptv: \"%s\" left group", t->tht_name);
}


/*
 *
 */

static void
iptv_parse_pmt(struct th_transport *t, th_stream_t *st,
	       uint8_t *table, int table_len)
{
  if(table[0] != 2 || t->tht_status != TRANSPORT_PROBING)
    return;

  psi_parse_pmt(t, table + 3, table_len - 3, 0);

  iptv_probe_done(t, 0);
}


/*
 *
 */

static void
iptv_parse_pat(struct th_transport *t, th_stream_t *st,
	       uint8_t *table, int table_len)
{
  if(table[0] != 0 || t->tht_status != TRANSPORT_PROBING)
    return;

  psi_parse_pat(t, table + 3, table_len - 3, iptv_parse_pmt);
}

/*
 *
 */

int
iptv_configure_transport(th_transport_t *t, const char *iptv_type,
			 struct config_head *head, const char *channel_name)
{
  const char *s;
  int fd;
  char buf[100];
  char ifname[100];
  struct ifreq ifr;
  th_stream_t *st;
  
  if(!strcasecmp(iptv_type, "rawudp"))
    t->tht_iptv_mode = IPTV_MODE_RAWUDP;
  else
    return -1;

  t->tht_type = TRANSPORT_IPTV;
  t->tht_start_feed = iptv_start_feed;
  t->tht_stop_feed  = iptv_stop_feed;

  if((s = config_get_str_sub(head, "group-address", NULL)) == NULL)
    return -1;
  t->tht_iptv_group_addr.s_addr = inet_addr(s);

  t->tht_iptv_ifindex = 0;

  if((s = config_get_str_sub(head, "interface-address", NULL)) != NULL)
    t->tht_iptv_interface_addr.s_addr = inet_addr(s);
  else
    t->tht_iptv_interface_addr.s_addr = INADDR_ANY; 
  
  snprintf(ifname, sizeof(ifname), "%s",
	   inet_ntoa(t->tht_iptv_interface_addr));

  if((s = config_get_str_sub(head, "interface", NULL)) != NULL) {

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, s, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;
    
    fd = socket(PF_INET,SOCK_STREAM,0);
    if(fd != -1) {
      if(ioctl(fd, SIOCGIFINDEX, &ifr) == 0) {
	t->tht_iptv_ifindex = ifr.ifr_ifindex;
	snprintf(ifname, sizeof(ifname), "%s", s);
      }
      close(fd);
    }
  }

  if((s = config_get_str_sub(head, "port", NULL)) == NULL)
    return -1;
  t->tht_iptv_port = atoi(s);

  snprintf(buf, sizeof(buf), "IPTV: %s (%s:%s:%d)", channel_name,
	   ifname, inet_ntoa(t->tht_iptv_group_addr), t->tht_iptv_port);
  t->tht_name = strdup(buf);

  st = transport_add_stream(t, 0, HTSTV_PAT);
  st->st_got_section = iptv_parse_pat;
  st->st_section_docrc = 1;

  s = config_get_str_sub(head, "provider", NULL);
  if(s != NULL)
    t->tht_provider = strdup(s);
  else
    t->tht_provider = strdup("IPTV");

  snprintf(buf, sizeof(buf), "iptv_%s_%d",
	   inet_ntoa(t->tht_iptv_group_addr), t->tht_iptv_port);
  t->tht_identifier = strdup(buf);

  t->tht_channelname = strdup(channel_name);

  LIST_INSERT_HEAD(&iptv_probing_transports, t, tht_active_link);
  startupcounter++;

  if(!dtimer_isarmed(&iptv_probe_timer)) {
    iptv_probe_transport(t);
    dtimer_arm(&iptv_probe_timer, iptv_probe_callback, t, 5);
  }

  return 0;
}

static void
iptv_probe_transport(th_transport_t *t)
{
  syslog(LOG_INFO, "iptv: Probing transport %s", t->tht_name);
  iptv_start_feed(t, 1, TRANSPORT_PROBING);
}


static void
iptv_probe_done(th_transport_t *t, int timeout)
{
  int pidcnt = 0;
  th_stream_t *st;

  startupcounter--;

  dtimer_disarm(&iptv_probe_timer);

  LIST_FOREACH(st, &t->tht_streams, st_link)
    pidcnt++;
  
  LIST_REMOVE(t, tht_active_link);

  syslog(LOG_INFO, "iptv: Transport %s probed, %d pids found%s", 
	 t->tht_name, pidcnt, timeout ? ", but probe timeouted" : "");

  iptv_stop_feed(t);

  if(!timeout)
    transport_set_channel(t, t->tht_channelname);
  else
    LIST_INSERT_HEAD(&iptv_stale_transports, t, tht_active_link);

  t = LIST_FIRST(&iptv_probing_transports);
  if(t == NULL)
    return;

  iptv_probe_transport(t);
  dtimer_arm(&iptv_probe_timer, iptv_probe_callback, t, 5);
}



static void
iptv_probe_callback(void *aux, int64_t now)
{
  th_transport_t *t = aux;
  iptv_probe_done(t, 1);
}
