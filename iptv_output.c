/*
 *  Output functions for fixed multicast streaming
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
#include "iptv_output.h"
#include "dispatch.h"
#include "channels.h"
#include "transports.h"

typedef struct output_multicast {
  int fd;
  int port;
  struct in_addr group;
} output_multicast_t;

#define MULTICAST_PKT_SIZ (188 * 7)


static void 
om_ip_streamer(struct th_subscription *s, uint8_t *pkt, th_pid_t *pi,
	       int64_t pcr)
{
  output_multicast_t *om = s->ths_opaque;
  struct sockaddr_in sin;

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(om->port);
  sin.sin_addr = om->group;
 
  if(pkt == NULL)
    return;

  if(s->ths_pkt == NULL) {
    s->ths_pkt = malloc(MULTICAST_PKT_SIZ);
    s->ths_pkt_ptr = 0;
  }

  memcpy(s->ths_pkt + s->ths_pkt_ptr, pkt, 188);
  s->ths_pkt[s->ths_pkt_ptr] = 0x47;

  s->ths_pkt_ptr += 188;
  
  if(s->ths_pkt_ptr == MULTICAST_PKT_SIZ) {
    sendto(om->fd, s->ths_pkt, s->ths_pkt_ptr, 0,
	   (struct sockaddr *)&sin, sizeof(sin));
    s->ths_pkt_ptr = 0;
  }
}





static void
output_multicast_load(struct config_head *head)
{
  const char *name, *s, *b;
  th_channel_t *ch;
  output_multicast_t *om;
  int ttl = 32;
  struct sockaddr_in sin;
  char title[30];

  if((name = config_get_str_sub(head, "channel", NULL)) == NULL)
    return;

  ch = channel_find(name, 1);

  om = calloc(1, sizeof(output_multicast_t));
  
  if((b = config_get_str_sub(head, "interface-address", NULL)) == NULL) {
    fprintf(stderr, "no interface address configured\n");
    goto err;
  }
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = 0;
  sin.sin_addr.s_addr = inet_addr(b);

  if((s = config_get_str_sub(head, "group-address", NULL)) == NULL) {
    fprintf(stderr, "no group address configured\n");
    goto err;
  }
  om->group.s_addr = inet_addr(s);

  if((s = config_get_str_sub(head, "port", NULL)) == NULL) {
    fprintf(stderr, "no port configured\n");
    goto err;
  }
  om->port = atoi(s);

  om->fd = socket(AF_INET, SOCK_DGRAM, 0);

  if(bind(om->fd, (struct sockaddr *)&sin, sizeof(sin))==-1) {
    fprintf(stderr, "cannot bind to %s\n", b);
    close(om->fd);
    goto err;
  }

  if((s = config_get_str_sub(head, "ttl", NULL)) != NULL)
    ttl = atoi(s);

  setsockopt(om->fd, SOL_IP, IP_MULTICAST_TTL, &ttl, sizeof(int));

  snprintf(title, sizeof(title), "%s:%d", inet_ntoa(om->group), om->port);

  syslog(LOG_INFO, "Static multicast output: \"%s\" to %s, source %s ",
	 ch->ch_name, title, inet_ntoa(sin.sin_addr));

  subscription_create(ch, om, om_ip_streamer, 900, title);

  return;

 err:
  free(om);
}



void 
output_multicast_setup(void)
{
  config_entry_t *ce;
  
  TAILQ_FOREACH(ce, &config_list, ce_link) {
    if(ce->ce_type == CFG_SUB && !strcasecmp("multicast-output", ce->ce_key)) {
      output_multicast_load(&ce->ce_sub);
    }
  }
}
