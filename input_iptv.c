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

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "input_iptv.h"
#include "channels.h"
#include "transports.h"
#include "dispatch.h"

static void
iptv_fd_callback(int events, void *opaque, int fd)
{
  th_transport_t *t = opaque;
  uint8_t buf[2000];
  int r, pid;
  uint8_t *tsb = buf;

  r = read(fd, buf, sizeof(buf));

  while(r >= 188) {
    pid = (tsb[1] & 0x1f) << 8 | tsb[2];
    transport_recv_tsb(t, pid, tsb);
    r -= 188;
    tsb += 188;
  }
}

int
iptv_start_feed(th_transport_t *t, unsigned int weight)
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
    syslog(LOG_ERR, "\"%s\" cannot bind %s:%d -- %s", 
	   t->tht_name, inet_ntoa(sin.sin_addr), t->tht_iptv_port,
	   strerror(errno));
    close(fd);
    return -1;
  }

  memset(&m, 0, sizeof(m));
  m.imr_multiaddr.s_addr = t->tht_iptv_group_addr.s_addr;
  m.imr_address.s_addr = t->tht_iptv_interface_addr.s_addr;

  if(setsockopt(fd, SOL_IP, IP_ADD_MEMBERSHIP, &m, 
		sizeof(struct ip_mreqn)) == -1) {
    syslog(LOG_ERR, "\"%s\" cannot join %s -- %s", 
	   t->tht_name, inet_ntoa(m.imr_multiaddr),
	   strerror(errno));
    close(fd);
    return -1;
  }

  t->tht_iptv_fd = fd;
  t->tht_status = TRANSPORT_RUNNING;

  syslog(LOG_ERR, "\"%s\" joined group", t->tht_name);

  t->tht_iptv_dispatch_handle = dispatch_addfd(fd, iptv_fd_callback, t,
					       DISPATCH_READ);
  return 0;
}

int
iptv_stop_feed(th_transport_t *t)
{
  if(t->tht_status == TRANSPORT_IDLE)
    return 0;

  t->tht_status = TRANSPORT_IDLE;
  dispatch_delfd(t->tht_iptv_dispatch_handle);
  close(t->tht_iptv_fd);

  syslog(LOG_ERR, "\"%s\" left group", t->tht_name);
  return 0;
}


/*
 *
 */

int
iptv_configure_transport(th_transport_t *t, const char *muxname)
{
  config_entry_t *ce;
  const char *s;
  char buf[100];

  if((ce = find_mux_config("iptvmux", muxname)) == NULL)
    return -1;

  t->tht_type = TRANSPORT_IPTV;
  
  if((s = config_get_str_sub(&ce->ce_sub, "group-address", NULL)) == NULL)
    return -1;
  t->tht_iptv_group_addr.s_addr = inet_addr(s);

  if((s = config_get_str_sub(&ce->ce_sub, "interface-address", NULL)) == NULL)
    return -1;
  t->tht_iptv_interface_addr.s_addr = inet_addr(s);

  if((s = config_get_str_sub(&ce->ce_sub, "port", NULL)) == NULL)
    return -1;
  t->tht_iptv_port = atoi(s);

  snprintf(buf, sizeof(buf), "IPTV: %s (%s:%d)", muxname, 
	   inet_ntoa(t->tht_iptv_group_addr), t->tht_iptv_port);
  t->tht_name = strdup(buf);

  return 0;
}
