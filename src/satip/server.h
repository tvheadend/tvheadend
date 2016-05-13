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

#ifndef __TVH_SATIP_SERVER_H__
#define __TVH_SATIP_SERVER_H__

#include "build.h"

#if ENABLE_SATIP_SERVER

#include "input.h"
#include "htsbuf.h"
#include "udp.h"
#include "http.h"

#define MUXCNF_AUTO   0
#define MUXCNF_KEEP   1
#define MUXCNF_REJECT 2

#define RTSP_TCP_DATA 1000000

struct satip_server_conf {
  idnode_t idnode;
  int satip_deviceid;
  char *satip_uuid;
  int satip_rtsp;
  int satip_weight;
  int satip_allow_remote_weight;
  int satip_descramble;
  int satip_rewrite_pmt;
  int satip_muxcnf;
  int satip_dvbs;
  int satip_dvbs2;
  int satip_dvbt;
  int satip_dvbt2;
  int satip_dvbc;
  int satip_dvbc2;
  int satip_atsc_t;
  int satip_atsc_c;
  char *satip_nat_ip;
};

extern struct satip_server_conf satip_server_conf;

extern const idclass_t satip_server_class;

void satip_rtp_queue(void *id, th_subscription_t *subs,
                     streaming_queue_t *sq,
                     pthread_mutex_t *tcp_lock,
                     struct sockaddr_storage *peer, int port,
                     int fd_rtp, int fd_rtcp,
                     int frontend, int source,
                     dvb_mux_conf_t *dmc,
                     mpegts_apids_t *pids, int perm_lock);
void satip_rtp_update(void *id, th_subscription_t *subs,
                      streaming_queue_t *sq,
                      int frontend, int source,
                      dvb_mux_conf_t *dmc,
                      mpegts_apids_t *pids,
                      mpegts_apids_t *pmt_pids);
void satip_rtp_update_pids(void *id, mpegts_apids_t *pids);
void satip_rtp_update_pmt_pids(void *id, mpegts_apids_t *pmt_pids);
int satip_rtp_status(void *id, char *buf, int len);
void satip_rtp_close(void *id);

void satip_rtp_init(int boot);
void satip_rtp_done(void);

int satip_rtsp_delsys(int fe, int *findex, const char **ftype);

void satip_server_rtsp_init(const char *bindaddr, int port,
                            int descramble, int rewrite_pmt, int muxcnf,
                            const char *nat_ip);
void satip_server_rtsp_register(void);
void satip_server_rtsp_done(void);

int satip_server_http_page(http_connection_t *hc,
                           const char *remain, void *opaque);

int satip_server_match_uuid(const char *uuid);

void satip_server_init(const char *bindaddr, int rtsp_port);
void satip_server_register(void);
void satip_server_done(void);

#else

static inline int satip_server_match_uuid(const char *uuid) { return 0; }

static inline void satip_server_config_changed(void) { };

static inline void satip_server_init(const char *bindaddr, int rtsp_port) { };
static inline void satip_server_register(void) { };
static inline void satip_server_done(void) { };

#endif

#endif /* __TVH_SATIP_SERVER_H__ */
