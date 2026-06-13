/*
 *  tvheadend, UPnP interface
 *  Copyright (C) 2014 Jaroslav Kysela
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

#ifndef UPNP_H_
#define UPNP_H_

#include "http.h"
#include "udp.h"

extern int upnp_running;

typedef struct upnp_service upnp_service_t;

struct upnp_service {
  TAILQ_ENTRY(upnp_service) us_link;
  void (*us_received)(uint8_t *data, size_t len,
                      udp_connection_t *conn,
                      struct sockaddr_storage *storage);
  void (*us_destroy)(upnp_service_t *us);
};

upnp_service_t *upnp_service_create0(upnp_service_t *us);
#define upnp_service_create(us) \
  upnp_service_create0(calloc(1, sizeof(struct us)))
void upnp_service_destroy(upnp_service_t *service);

void upnp_send(htsbuf_queue_t *q, struct sockaddr_storage *storage,
               int delay_ms, int from_multicast, int fill_source);

void upnp_server_init(const char *bindaddr);
void upnp_server_done(void);

#endif /* UPNP_H_ */
