/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2014 Jaroslav Kysela
 *
 * tvheadend, UPnP interface
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
               int delay_ms, int from_multicast);

void upnp_server_init(const char *bindaddr);
void upnp_server_done(void);

#endif /* UPNP_H_ */
