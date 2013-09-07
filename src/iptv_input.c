/*
 *  Multicasted IPTV Input
 *  Copyright (C) 2007 Andreas �man
 *                2012 Adrien CLERC
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

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <assert.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "tvheadend.h"
#include "htsmsg.h"
#include "channels.h"
#include "iptv_input.h"
#include "iptv_input_rtsp.h"
#include "rtcp.h"
#include "tsdemux.h"
#include "psi.h"
#include "settings.h"
#include "tvhpoll.h"

#if defined(PLATFORM_LINUX)
#include <linux/netdevice.h>
#elif defined(PLATFORM_FREEBSD)
#  include <netdb.h>
#  include <net/if.h>
#  ifndef IPV6_ADD_MEMBERSHIP
#    define IPV6_ADD_MEMBERSHIP	IPV6_JOIN_GROUP
#    define IPV6_DROP_MEMBERSHIP	IPV6_LEAVE_GROUP
#  endif
#endif

static int             iptv_thread_running;
static tvhpoll_t      *iptv_poll;
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
 * Determine whether service is an RTSP service
 */
static int
is_rtsp(service_t *service)
{
  return service->s_iptv_rtsp_info != NULL
        || strncmp(service->s_iptv_iface, "rtsp://", 7) == 0;
}

/**
 * Determine whether service is an IPv4 based multicast service
 */
static int
is_multicast_ipv4(service_t *service)
{
  return service->s_iptv_group.s_addr != 0;
}

/**
 * Determine whether service is an IPv6 based multicast service
 */
static int
is_multicast_ipv6(service_t *service)
{
#ifdef SOL_IPV6
  return IN6_IS_ADDR_MULTICAST(service->s_iptv_group6.s6_addr);
#else
  return 0;
#endif
}

/**
 * Determine whether service is of a given subtype
 */
int
iptv_is_service_subtype(service_t *service, iptv_subtype_t subtype)
{
  switch(subtype) {
  case SERVICE_TYPE_IPTV_MCAST:
    return !iptv_is_service_subtype(service, SERVICE_TYPE_IPTV_RTSP);
  case SERVICE_TYPE_IPTV_RTSP:
    return is_rtsp(service);
  default:
    return 0;
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
  tvhpoll_event_t ev;
  service_t *t;

  while(1) {
    nfds = tvhpoll_wait(iptv_poll, &ev, 1, -1);
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
	// Add the extension header itself (EHL does not inc header)
	hlen += 4;
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
      
      // RTP check OK, send this to RTCP if needed
      if(is_rtsp(t))
      {
        rtcp_receiver_update(t, tsb);
      }
      
      for(j = 0; j < r; j += 188)
	iptv_ts_input(t, buf + j);
    }
    pthread_mutex_unlock(&iptv_recvmutex);
  }
  return NULL;
}

static int
iptv_find_interface(service_t *service, struct ifreq *ifr, int fd)
{
  memset(ifr, 0, sizeof(*ifr));
  snprintf(ifr->ifr_name, IFNAMSIZ, "%s", service->s_iptv_iface);
  ifr->ifr_name[IFNAMSIZ - 1] = 0;
  if(ioctl(fd, SIOCGIFINDEX, ifr)) {
    tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot find interface %s", 
	   service->s_identifier, service->s_iptv_iface);
    return -1;
  }
  
  return 0;
}

static int
iptv_multicast_ipv4_start(service_t *service, int fd)
{
  struct sockaddr_in sin;
  struct ip_mreqn m;
  struct ifreq ifr;
  int solip;
  
  /* First, resolve interface name */
  if(iptv_find_interface(service, &ifr, fd) != 0)
  {
    return -1;
  }
  
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(service->s_iptv_port);
  sin.sin_addr.s_addr = service->s_iptv_group.s_addr;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &m, sizeof(struct ip_mreqn));
  if(bind(fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
    tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot bind %s:%d -- %s",
         service->s_identifier, inet_ntoa(sin.sin_addr), service->s_iptv_port,
         strerror(errno));
    return -1;
  }

  /* Join IPv4 group */
  memset(&m, 0, sizeof(m));
  m.imr_multiaddr.s_addr = service->s_iptv_group.s_addr;
  m.imr_address.s_addr = 0;
#if defined(PLATFORM_LINUX)
  m.imr_ifindex = ifr.ifr_ifindex;
#elif defined(PLATFORM_FREEBSD)
  m.imr_ifindex = ifr.ifr_index;
#endif

#ifdef SOL_IP
  solip = SOL_IP;
#else
  {
    struct protoent *pent;
    pent = getprotobyname("ip");
    solip = (pent != NULL) ? pent->p_proto : 0;
  }
#endif

  if(setsockopt(fd, solip, IP_ADD_MEMBERSHIP, &m,
              sizeof(struct ip_mreqn)) == -1) {
    tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot join %s -- %s",
         service->s_identifier, inet_ntoa(m.imr_multiaddr), strerror(errno));
    return -1;
  }
  return 0;
}

static void
iptv_multicast_ipv4_stop(service_t *service)
{
  struct ifreq ifr;
  struct ip_mreqn m;
  int solip;

  iptv_find_interface(service, &ifr, service->s_iptv_fd);

  memset(&m, 0, sizeof(m));
  /* Leave multicast group */
  m.imr_multiaddr.s_addr = service->s_iptv_group.s_addr;
  m.imr_address.s_addr = 0;
#if defined(PLATFORM_LINUX)
  m.imr_ifindex = ifr.ifr_ifindex;
#elif defined(PLATFORM_FREEBSD)
  m.imr_ifindex = ifr.ifr_index;
#endif

#ifdef SOL_IP
  solip = SOL_IP;
#else
  {
    struct protoent *pent;
    pent = getprotobyname("ip");
    solip = (pent != NULL) ? pent->p_proto : 0;
  }
#endif

  if(setsockopt(service->s_iptv_fd, solip, IP_DROP_MEMBERSHIP, &m,
		  sizeof(struct ip_mreqn)) == -1) {
    tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot leave %s -- %s",
	     service->s_identifier, inet_ntoa(m.imr_multiaddr), strerror(errno));
  }
}

static void
iptv_multicast_ipv6_stop(service_t *service)
{
  struct ifreq ifr;
  struct ipv6_mreq m6;
  memset(&m6, 0, sizeof(m6));

  iptv_find_interface(service, &ifr, service->s_iptv_fd);

  m6.ipv6mr_multiaddr = service->s_iptv_group6;
#if defined(PLATFORM_LINUX)
  m6.ipv6mr_interface = ifr.ifr_ifindex;
#elif defined(PLATFORM_FREEBSD)
  m6.ipv6mr_interface = ifr.ifr_index;
#endif

#ifdef SOL_IPV6
  if(setsockopt(service->s_iptv_fd, SOL_IPV6, IPV6_DROP_MEMBERSHIP, &m6,
		  sizeof(struct ipv6_mreq)) == -1) {
    char straddr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, m6.ipv6mr_multiaddr.s6_addr,
		straddr, sizeof(straddr));

    tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot leave %s -- %s",
	     service->s_identifier, straddr, strerror(errno));
  }
#else
  tvhlog(LOG_ERR, "IPTV", "IPv6 multicast not supported on your platform");
#endif
}

static int
iptv_multicast_ipv6_start(service_t *service, int fd)
{
  char straddr[INET6_ADDRSTRLEN];
  struct sockaddr_in6 sin6;
  struct ipv6_mreq m6;
  struct ifreq ifr;
  
  /* First, resolve interface name */
  if(iptv_find_interface(service, &ifr, fd) != 0)
  {
    return -1;
  }
  
  /* Bind to IPv6 multicast group */
  memset(&sin6, 0, sizeof(sin6));
  sin6.sin6_family = AF_INET6;
  sin6.sin6_port = htons(service->s_iptv_port);
  sin6.sin6_addr = service->s_iptv_group6;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &m6, sizeof(struct ipv6_mreq));
  if(bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) == -1) {
    inet_ntop(AF_INET6, &sin6.sin6_addr, straddr, sizeof(straddr));
    tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot bind %s:%d -- %s",
         service->s_identifier, straddr, service->s_iptv_port,
         strerror(errno));
    return -1;
  }
  /* Join IPv6 group */
  memset(&m6, 0, sizeof(m6));
  m6.ipv6mr_multiaddr = service->s_iptv_group6;
#if defined(PLATFORM_LINUX)
  m6.ipv6mr_interface = ifr.ifr_ifindex;
#elif defined(PLATFORM_FREEBSD)
  m6.ipv6mr_interface = ifr.ifr_index;
#endif

#ifdef SOL_IPV6
  if(setsockopt(fd, SOL_IPV6, IPV6_ADD_MEMBERSHIP, &m6,
              sizeof(struct ipv6_mreq)) == -1) {
    inet_ntop(AF_INET6, m6.ipv6mr_multiaddr.s6_addr,
		straddr, sizeof(straddr));
    tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot join %s -- %s",
         service->s_identifier, straddr, strerror(errno));
    return -1;
  }
  
  return 0;
}

/**
 *
 */
static int
iptv_service_start(service_t *t, unsigned int weight, int force_start)
{
  pthread_t tid;
  int fd;
  tvhpoll_event_t ev;

  assert(t->s_iptv_fd == -1);

  if(iptv_thread_running == 0) {
    iptv_thread_running = 1;
    iptv_poll = tvhpoll_create(10);
    pthread_create(&tid, NULL, iptv_thread, NULL);
  }

  if(is_rtsp(t))
  {
    tvhlog(LOG_WARNING, "IPTV",
	   "Starting RTSP Streaming with %s", t->s_iptv_iface);
    // The socket will be opened by the function
    t->s_iptv_rtsp_info = iptv_rtsp_start(t->s_iptv_iface, &fd);
    if(!t->s_iptv_rtsp_info)
    {
      close(fd);
      return -1;
    }
  } else if(is_multicast_ipv4(t)) {
    fd = tvh_socket(AF_INET, SOCK_DGRAM, 0);
    if(fd == -1) {
      tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot open socket", t->s_identifier);
      return -1;
    }
    /* Bind to IPv4 multicast group */
    if(iptv_multicast_ipv4_start(t, fd) != 0)
    {
      close(fd);
      return -1;
    }
  } else if(is_multicast_ipv6(t)) {
    fd = tvh_socket(AF_INET6, SOCK_DGRAM, 0);
    if(fd == -1) {
      tvhlog(LOG_ERR, "IPTV", "\"%s\" cannot open socket", t->s_identifier);
      return -1;
    }
    /* Bind to IPv6 multicast group */
    if(iptv_multicast_ipv6_start(t, fd) != 0)
    {
      close(fd);
      return -1;
    }
#else
    tvhlog(LOG_ERR, "IPTV", "IPv6 multicast not supported on your platform");

    close(fd);
    return -1;
#endif
  }


  int resize = 262142;
  if(setsockopt(fd,SOL_SOCKET,SO_RCVBUF, &resize, sizeof(resize)) == -1)
    tvhlog(LOG_WARNING, "IPTV",
	   "Can not icrease UDP receive buffer size to %d -- %s",
	   resize, strerror(errno));

  memset(&ev, 0, sizeof(ev));
  ev.events  = TVHPOLL_IN;
  ev.fd      = fd;
  ev.data.fd = fd;
  if(tvhpoll_add(iptv_poll, &ev, 1) == -1) {
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
  pthread_mutex_lock(&iptv_recvmutex);
  LIST_REMOVE(t, s_active_link);
  pthread_mutex_unlock(&iptv_recvmutex);

  assert(t->s_iptv_fd >= 0);
  
  if(is_rtsp(t))
  {
    iptv_rtsp_stop(t->s_iptv_rtsp_info);
  } else if (is_multicast_ipv4(t)) {
    iptv_multicast_ipv4_stop(t);
  } else if (is_multicast_ipv6(t)) {
    iptv_multicast_ipv6_stop(t);
  }
  close(t->s_iptv_fd); // Automatically removes fd from epoll set
  // TODO: this is an issue

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
#ifdef SOL_IPV6
  char abuf6[INET6_ADDRSTRLEN];
#endif

  lock_assert(&global_lock);

  htsmsg_add_u32(m, "pmt", t->s_pmt_pid);

  if(t->s_servicetype)
    htsmsg_add_u32(m, "stype", t->s_servicetype);

  if(t->s_iptv_port)
    htsmsg_add_u32(m, "port", t->s_iptv_port);

  if(t->s_iptv_iface)
    htsmsg_add_str(m, "interface", t->s_iptv_iface);

  if(t->s_iptv_group.s_addr!= 0) {
    inet_ntop(AF_INET, &t->s_iptv_group, abuf, sizeof(abuf));
    htsmsg_add_str(m, "group", abuf);
  }
#ifdef SOL_IPV6
  if(IN6_IS_ADDR_MULTICAST(t->s_iptv_group6.s6_addr) ) {
    inet_ntop(AF_INET6, &t->s_iptv_group6, abuf6, sizeof(abuf6));
    htsmsg_add_str(m, "group", abuf6);
  }
#endif
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
  if(!is_rtsp(t) && !is_multicast_ipv4(t) && !is_multicast_ipv6(t))
    return 0;

  return 100;
}

/**
 *
 */
static int
iptv_service_is_enabled(service_t *t)
{
  return t->s_enabled;
}

/**
 * Generate a descriptive name for the source
 */
static void
iptv_service_setsourceinfo(service_t *t, struct source_info *si)
{
  char straddr[INET6_ADDRSTRLEN];
  memset(si, 0, sizeof(struct source_info));

  si->si_type = S_MPEG_TS;
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

  t->s_servicetype   = ST_SDTV;
  t->s_start_feed    = iptv_service_start;
  t->s_refresh_feed  = iptv_service_refresh;
  t->s_stop_feed     = iptv_service_stop;
  t->s_config_save   = iptv_service_save;
  t->s_setsourceinfo = iptv_service_setsourceinfo;
  t->s_quality_index = iptv_service_quality;
  t->s_is_enabled    = iptv_service_is_enabled;
  t->s_grace_period  = iptv_grace_period;
  t->s_dtor          = iptv_service_dtor;
  t->s_iptv_fd = -1;
  t->s_iptv_rtsp_info = NULL;

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
  int old = 0;

  lock_assert(&global_lock);

  if((l = hts_settings_load("iptvservices")) == NULL) {
    if ((l = hts_settings_load("iptvtransports")) == NULL)
      return;
    else
      old = 1;
  }
  
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

    if(!htsmsg_get_u32(c, "stype", &u32))
      t->s_servicetype = u32;
    else if (!htsmsg_get_u32(c, "radio", &u32) && u32)
      t->s_servicetype = ST_RADIO;
    else
      t->s_servicetype = ST_SDTV;
    // Note: for compat with old PR #52 I load "radio" flag

    pthread_mutex_lock(&t->s_stream_mutex);
    service_make_nicename(t);
    psi_load_service_settings(c, t);
    pthread_mutex_unlock(&t->s_stream_mutex);
    
    s = htsmsg_get_str(c, "channelname");
    if(htsmsg_get_u32(c, "mapped", &u32))
      u32 = 0;
    
    if(s && u32)
      service_map_channel(t, channel_find_by_name(s, 1, 0), 0);

    /* Migrate to new */
    if(old)
      iptv_service_save(t);
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
  iptv_init_rtsp();
}
