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
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <assert.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <linux/netdevice.h>

#include "tvheadend.h"
#include "htsmsg.h"
#include "channels.h"
#include "iptv_input.h"
#include "tsdemux.h"
#include "psi.h"
#include "settings.h"

static int iptv_thread_running;
static int iptv_epollfd;
static pthread_mutex_t iptv_recvmutex;

struct service_list iptv_all_services; /* All IPTV services */
static struct service_list iptv_active_services; /* Currently enabled */

/**
 * PAT parser. We only parse a single program. CRC has already been verified
 */
static void
iptv_got_pat(const uint8_t *ptr, size_t len, void *aux)
{
  service_t *t = aux;
  uint16_t prognum, pmt;

  len -= 8;
  ptr += 8;

  while(len >= 4) {

    prognum =  ptr[0]         << 8 | ptr[1];
    pmt     = (ptr[2] & 0x1f) << 8 | ptr[3];

    if(prognum != 0) {
      t->s_pmt_pid = pmt;
      return;
    }
    ptr += 4;
    len -= 4;
  }
}


/**
 * PMT parser. CRC has already been verified
 */
static void
iptv_got_pmt(const uint8_t *ptr, size_t len, void *aux)
{
  service_t *t = aux;

  if(len < 3 || ptr[0] != 2)
    return;

  pthread_mutex_lock(&t->s_stream_mutex);
  psi_parse_pmt(t, ptr + 3, len - 3, 0, 1);
  pthread_mutex_unlock(&t->s_stream_mutex);
}


/**
 * Handle a single TS packet for the given IPTV service
 */
static void
iptv_ts_input(service_t *t, const uint8_t *tsb)
{
  uint16_t pid = ((tsb[1] & 0x1f) << 8) | tsb[2];

  if(pid == 0) {

    if(t->s_pat_section == NULL)
      t->s_pat_section = calloc(1, sizeof(psi_section_t));
    psi_section_reassemble(t->s_pat_section, tsb, 1, iptv_got_pat, t);

  } else if(pid == t->s_pmt_pid) {

    if(t->s_pmt_section == NULL)
      t->s_pmt_section = calloc(1, sizeof(psi_section_t));
    psi_section_reassemble(t->s_pmt_section, tsb, 1, iptv_got_pmt, t);

  } else {
    ts_recv_packet1(t, tsb, NULL);
  } 
}


/**
 * Main epoll() based input thread for IPTV
 */
static void *
iptv_thread(void *aux)
{
  int nfds, fd, r, j, hlen;
  uint8_t tsb[65536], *buf;
  struct epoll_event ev;
  service_t *t;

  while(1) {
    nfds = epoll_wait(iptv_epollfd, &ev, 1, -1);
    if(nfds == -1) {
      tvhlog(LOG_ERR, "IPTV", "epoll() error -- %s, sleeping 1 second",
	     strerror(errno));
      sleep(1);
      continue;
    }

    if(nfds < 1)
      continue;

    fd = ev.data.fd;
    r = read(fd, tsb, sizeof(tsb));

    if(r > 1 && tsb[0] == 0x47 && (r % 188) == 0) {
      /* Looks like raw TS in UDP */
      buf = tsb;
    } else {
      /* Check for valid RTP packets */
      if(r < 12)
	continue;

      if((tsb[0] & 0xc0) != 0x80)
	continue;

      if((tsb[1] & 0x7f) != 33)
	continue;
      
      hlen = (tsb[0] & 0xf) * 4 + 12;

      if(tsb[0] & 0x10) {
	// Extension (X bit) == true

	if(r < hlen + 4)
	  continue; // Packet size < hlen + extension header

	// Skip over extension header (last 2 bytes of header is length)
	hlen += ((tsb[hlen + 2] << 8) | tsb[hlen + 3]) * 4;
      }

      if(r < hlen || (r - hlen) % 188 != 0)
	continue;

      buf = tsb + hlen;
      r -= hlen;
    }

    pthread_mutex_lock(&iptv_recvmutex);
    
    LIST_FOREACH(t, &iptv_active_services, s_active_link) {
      if(t->s_iptv_fd != fd)
	continue;
      
      for(j = 0; j < r; j += 188)
	iptv_ts_input(t, buf + j);
    }
    pthread_mutex_unlock(&iptv_recvmutex);
  }
  return NULL;
}


/**
 *
 */
static int
iptv_service_start(service_t *t, unsigned int weight, int force_start)
{
  pthread_t tid;
  int fd;
  char straddr[INET6_ADDRSTRLEN];
  struct ip_mreqn m;
  struct ipv6_mreq m6;
  struct sockaddr_in sin;
  struct sockaddr_in6 sin6;
  struct ifreq ifr;
  struct epoll_event ev;

  assert(t->s_iptv_fd == -1);

  if(iptv_thread_running == 0) {
    iptv_thread_running = 1;
    iptv_epollfd = epoll_create(10);
    pthread_create(&tid, NULL, iptv_thread, NULL);
  }

  /* Now, open the real socket for UDP */
  if(t->s_iptv_group.s_addr!=0) {
    fd = tvh_socket(AF_INET, SOCK_DGRAM, 0);
  
  }
  else {
    fd = tvh_socket(AF_INET6, SOCK_DGRAM, 0);
  }
  if(fd == -1) {
    tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot open socket", t->s_identifier);
    return -1;
  }

  /* First, resolve interface name */
  memset(&ifr, 0, sizeof(ifr));
  snprintf(ifr.ifr_name, IFNAMSIZ, "%s", t->s_iptv_iface);
  ifr.ifr_name[IFNAMSIZ - 1] = 0;
  if(ioctl(fd, SIOCGIFINDEX, &ifr)) {
    tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot find interface %s", 
	   t->s_identifier, t->s_iptv_iface);
    close(fd);
    return -1;
  }

  /* Bind to IPv4 multicast group */
  if(t->s_iptv_group.s_addr!=0) {
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(t->s_iptv_port);
    sin.sin_addr.s_addr = t->s_iptv_group.s_addr;
    if(bind(fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
      tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot bind %s:%d -- %s",
           t->s_identifier, inet_ntoa(sin.sin_addr), t->s_iptv_port,
           strerror(errno));
      close(fd);
      return -1;
    }
    /* Join IPv4 group */
    memset(&m, 0, sizeof(m));
    m.imr_multiaddr.s_addr = t->s_iptv_group.s_addr;
    m.imr_address.s_addr = 0;
    m.imr_ifindex = ifr.ifr_ifindex;

      if(setsockopt(fd, SOL_IP, IP_ADD_MEMBERSHIP, &m,
                sizeof(struct ip_mreqn)) == -1) {
      tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot join %s -- %s",
           t->s_identifier, inet_ntoa(m.imr_multiaddr), strerror(errno));
      close(fd);
      return -1;
    }
  } else {
    /* Bind to IPv6 multicast group */
    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port = htons(t->s_iptv_port);
    sin6.sin6_addr = t->s_iptv_group6;
    if(bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) == -1) {
      inet_ntop(AF_INET6, &sin6.sin6_addr, straddr, sizeof(straddr));
      tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot bind %s:%d -- %s",
           t->s_identifier, straddr, t->s_iptv_port,
           strerror(errno));
      close(fd);
      return -1;
    }
    /* Join IPv6 group */
    memset(&m6, 0, sizeof(m6));
    m6.ipv6mr_multiaddr = t->s_iptv_group6;
    m6.ipv6mr_interface = ifr.ifr_ifindex;

    if(setsockopt(fd, SOL_IPV6, IPV6_ADD_MEMBERSHIP, &m6,
                sizeof(struct ipv6_mreq)) == -1) {
      inet_ntop(AF_INET6, m6.ipv6mr_multiaddr.s6_addr,
		straddr, sizeof(straddr));
      tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot join %s -- %s",
           t->s_identifier, straddr, strerror(errno));
      close(fd);
      return -1;
    }
  }


  int resize = 262142;
  if(setsockopt(fd,SOL_SOCKET,SO_RCVBUF, &resize, sizeof(resize)) == -1)
    tvhlog(LOG_WARNING, "IPTV",
	   "Can not icrease UDP receive buffer size to %d -- %s",
	   resize, strerror(errno));

  memset(&ev, 0, sizeof(ev));
  ev.events = EPOLLIN;
  ev.data.fd = fd;
  if(epoll_ctl(iptv_epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
    tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot add to epoll set -- %s", 
	   t->s_identifier, strerror(errno));
    close(fd);
    return -1;
  }

  t->s_iptv_fd = fd;

  pthread_mutex_lock(&iptv_recvmutex);
  LIST_INSERT_HEAD(&iptv_active_services, t, s_active_link);
  pthread_mutex_unlock(&iptv_recvmutex);
  return 0;
}


/**
 *
 */
static void
iptv_service_refresh(service_t *t)
{

}


/**
 *
 */
static void
iptv_service_stop(service_t *t)
{
  struct ifreq ifr;

  pthread_mutex_lock(&iptv_recvmutex);
  LIST_REMOVE(t, s_active_link);
  pthread_mutex_unlock(&iptv_recvmutex);

  assert(t->s_iptv_fd >= 0);

  /* First, resolve interface name */
  memset(&ifr, 0, sizeof(ifr));
  snprintf(ifr.ifr_name, IFNAMSIZ, "%s", t->s_iptv_iface);
  ifr.ifr_name[IFNAMSIZ - 1] = 0;
  if(ioctl(t->s_iptv_fd, SIOCGIFINDEX, &ifr)) {
    tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot find interface %s",
	   t->s_identifier, t->s_iptv_iface);
  }

  if(t->s_iptv_group.s_addr != 0) {

    struct ip_mreqn m;
    memset(&m, 0, sizeof(m));
    /* Leave multicast group */
    m.imr_multiaddr.s_addr = t->s_iptv_group.s_addr;
    m.imr_address.s_addr = 0;
    m.imr_ifindex = ifr.ifr_ifindex;
    
    if(setsockopt(t->s_iptv_fd, SOL_IP, IP_DROP_MEMBERSHIP, &m,
		  sizeof(struct ip_mreqn)) == -1) {
      tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot leave %s -- %s",
	     t->s_identifier, inet_ntoa(m.imr_multiaddr), strerror(errno));
    }
  } else {
    char straddr[INET6_ADDRSTRLEN];

    struct ipv6_mreq m6;
    memset(&m6, 0, sizeof(m6));

    m6.ipv6mr_multiaddr = t->s_iptv_group6;
    m6.ipv6mr_interface = ifr.ifr_ifindex;

    if(setsockopt(t->s_iptv_fd, SOL_IPV6, IPV6_DROP_MEMBERSHIP, &m6,
		  sizeof(struct ipv6_mreq)) == -1) {
      inet_ntop(AF_INET6, m6.ipv6mr_multiaddr.s6_addr,
		straddr, sizeof(straddr));

      tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot leave %s -- %s",
	     t->s_identifier, straddr, strerror(errno));
    }



  }
  close(t->s_iptv_fd); // Automatically removes fd from epoll set

  t->s_iptv_fd = -1;
}


/**
 *
 */
static void
iptv_service_save(service_t *t)
{
  htsmsg_t *m = htsmsg_create_map();
  char abuf[INET_ADDRSTRLEN];
  char abuf6[INET6_ADDRSTRLEN];

  lock_assert(&global_lock);

  htsmsg_add_u32(m, "pmt", t->s_pmt_pid);

  if(t->s_iptv_port)
    htsmsg_add_u32(m, "port", t->s_iptv_port);

  if(t->s_iptv_iface)
    htsmsg_add_str(m, "interface", t->s_iptv_iface);

  if(t->s_iptv_group.s_addr!= 0) {
    inet_ntop(AF_INET, &t->s_iptv_group, abuf, sizeof(abuf));
    htsmsg_add_str(m, "group", abuf);
  }
  if(IN6_IS_ADDR_MULTICAST(t->s_iptv_group6.s6_addr) ) {
    inet_ntop(AF_INET6, &t->s_iptv_group6, abuf6, sizeof(abuf6));
    htsmsg_add_str(m, "group", abuf6);
  }
  if(t->s_ch != NULL) {
    htsmsg_add_str(m, "channelname", t->s_ch->ch_name);
    htsmsg_add_u32(m, "mapped", 1);
  }
  
  pthread_mutex_lock(&t->s_stream_mutex);
  psi_save_service_settings(m, t);
  pthread_mutex_unlock(&t->s_stream_mutex);
  
  hts_settings_save(m, "iptvservices/%s",
		    t->s_identifier);

  htsmsg_destroy(m);
}


/**
 *
 */
static int
iptv_service_quality(service_t *t)
{
  if(t->s_iptv_iface == NULL || 
     (t->s_iptv_group.s_addr == 0 && t->s_iptv_group6.s6_addr == 0) ||
     t->s_iptv_port == 0)
    return 0;

  return 100;
}


/**
 * Generate a descriptive name for the source
 */
static void
iptv_service_setsourceinfo(service_t *t, struct source_info *si)
{
  char straddr[INET6_ADDRSTRLEN];
  memset(si, 0, sizeof(struct source_info));

  si->si_adapter = t->s_iptv_iface ? strdup(t->s_iptv_iface) : NULL;
  if(t->s_iptv_group.s_addr != 0) {
    si->si_mux = strdup(inet_ntoa(t->s_iptv_group));
  }
  else {
    inet_ntop(AF_INET6, &t->s_iptv_group6, straddr, sizeof(straddr));
    si->si_mux = strdup(straddr);
  }
}


/**
 *
 */
static int
iptv_grace_period(service_t *t)
{
  return 3;
}


/**
 *
 */
static void
iptv_service_dtor(service_t *t)
{
  hts_settings_remove("iptvservices/%s", t->s_identifier); 
}


/**
 *
 */
service_t *
iptv_service_find(const char *id, int create)
{
  static int tally;
  service_t *t;
  char buf[20];

  if(id != NULL) {

    if(strncmp(id, "iptv_", 5))
      return NULL;

    LIST_FOREACH(t, &iptv_all_services, s_group_link)
      if(!strcmp(t->s_identifier, id))
	return t;
  }

  if(create == 0)
    return NULL;
  
  if(id == NULL) {
    tally++;
    snprintf(buf, sizeof(buf), "iptv_%d", tally);
    id = buf;
  } else {
    tally = MAX(atoi(id + 5), tally);
  }

  t = service_create(id, SERVICE_TYPE_IPTV, S_MPEG_TS);

  t->s_start_feed    = iptv_service_start;
  t->s_refresh_feed  = iptv_service_refresh;
  t->s_stop_feed     = iptv_service_stop;
  t->s_config_save   = iptv_service_save;
  t->s_setsourceinfo = iptv_service_setsourceinfo;
  t->s_quality_index = iptv_service_quality;
  t->s_grace_period  = iptv_grace_period;
  t->s_dtor          = iptv_service_dtor;
  t->s_iptv_fd = -1;

  LIST_INSERT_HEAD(&iptv_all_services, t, s_group_link);

  pthread_mutex_lock(&t->s_stream_mutex); 
  service_make_nicename(t);
  pthread_mutex_unlock(&t->s_stream_mutex); 

  return t;
}


/**
 * Load config for the given mux
 */
static void
iptv_service_load(void)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;
  uint32_t pmt;
  const char *s;
  unsigned int u32;
  service_t *t;

  lock_assert(&global_lock);

  if((l = hts_settings_load("iptvservices")) == NULL)
    return;
  
  HTSMSG_FOREACH(f, l) {
    if((c = htsmsg_get_map_by_field(f)) == NULL)
      continue;

    if(htsmsg_get_u32(c, "pmt", &pmt))
      continue;
    
    t = iptv_service_find(f->hmf_name, 1);
    t->s_pmt_pid = pmt;

    tvh_str_update(&t->s_iptv_iface, htsmsg_get_str(c, "interface"));

    if((s = htsmsg_get_str(c, "group")) != NULL){
      if (!inet_pton(AF_INET, s, &t->s_iptv_group.s_addr)) {
         inet_pton(AF_INET6, s, &t->s_iptv_group6.s6_addr);
      }
    }
    
    if(!htsmsg_get_u32(c, "port", &u32))
      t->s_iptv_port = u32;

    pthread_mutex_lock(&t->s_stream_mutex);
    service_make_nicename(t);
    psi_load_service_settings(c, t);
    pthread_mutex_unlock(&t->s_stream_mutex);
    
    s = htsmsg_get_str(c, "channelname");
    if(htsmsg_get_u32(c, "mapped", &u32))
      u32 = 0;
    
    if(s && u32)
      service_map_channel(t, channel_find_by_name(s, 1, 0), 0);
  }
  htsmsg_destroy(l);
}


/**
 *
 */
void
iptv_input_init(void)
{
  pthread_mutex_init(&iptv_recvmutex, NULL);
  iptv_service_load();
}
