/*
 *  Tvheadend - SAT-IP DVB server - private data
 *
 *  Copyright (C) 2015 Jaroslav Kysela
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

#ifndef __TVH_SATIP_SERVER__H__
#define __TVH_SATIP_SERVER_H__

#include "build.h"

#if ENABLE_SATIP_SERVER

#include "input.h"
#include "htsbuf.h"
#include "udp.h"
#include "http.h"

void satip_server_rtsp_init(const char *bindaddr, int port);
void satip_server_rtsp_register(void);
void satip_server_rtsp_done(void);

int satip_server_http_page(http_connection_t *hc,
                           const char *remain, void *opaque);

int satip_server_match_uuid(const char *uuid);

void satip_server_config_changed(void);

void satip_server_init(int rtsp_port);
void satip_server_register(void);
void satip_server_done(void);

#else

static inline int satip_server_match_uuid(const char *uuid) { return 0; }

static inline void satip_server_config_changed(void) { };

static inline void satip_server_init(int rtsp_port) { };
static inline void satip_server_register(void) { };
static inline void satip_server_done(void) { };

#endif

#endif /* __TVH_SATIP_SERVER_H__ */
