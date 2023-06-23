/*
 *  Tvheadend - NetCeiver DVB input
 *
 *  Copyright (C) 2013-2018 Sven Wegener
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

#ifndef __TVH_NETCEIVER_PRIVATE_H__
#define __TVH_NETCEIVER_PRIVATE_H__

#include "tvheadend.h"
#include "input.h"
#include "tvhpoll.h"
#include "udp.h"
#include "htsmsg_xml.h"


#define NETCEIVER_MC_PREFIX 0xff18
#define NETCEIVER_MC_PORT 23000

#define NETCEIVER_TS_PACKET_SIZE 188
#define NETCEIVER_TS_PACKETS_PER_IP_PACKET 7
#define NETCEIVER_TS_IP_PACKET_SIZE (NETCEIVER_TS_PACKETS_PER_IP_PACKET * NETCEIVER_TS_PACKET_SIZE)
#define NETCEIVER_TS_IP_BUFFER_SIZE (256 * NETCEIVER_TS_IP_PACKET_SIZE)

#define RDF_NS "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define CCPP_NS "http://www.w3.org/2002/11/08-ccpp-schema#"
#define PRF_NS "http://www.w3.org/2000/01/rdf-schema#"

/*
 * Tuning
 */

typedef enum netceiver_group {
  NETCEIVER_GROUP_ANNOUNCE = 1,
  NETCEIVER_GROUP_STATUS = 2,
  NETCEIVER_GROUP_STREAM = 3,
  NETCEIVER_GROUP_MONITOR = 4,
  NETCEIVER_GROUP_LOG = 5,
} netceiver_group_t;

udp_connection_t *netceiver_tune(const char *name, const char *interface, netceiver_group_t group, int priority, dvb_mux_t *dm, int pid, int sid);

/*
 * Hardware
 */

void netceiver_hardware_init(void);

/*
 * Frontend
 */

extern const idclass_t netceiver_frontend_class;

typedef struct netceiver_frontend
{
  mpegts_input_t;

  dvb_fe_type_t     ncf_fe_type;

  const char       *ncf_interface;

  const char       *ncf_netceiver_tuner_uuid;

  pthread_t         ncf_input_thread;
  th_pipe_t         ncf_input_thread_pipe;
  tvhpoll_t        *ncf_poll;

  mpegts_apids_t    ncf_pids;
  udp_connection_t *ncf_udps[8192];

  udp_connection_t *ncf_monitor_udp;
} netceiver_frontend_t;

void netceiver_frontend_init(void);
netceiver_frontend_t *netceiver_frontend_create(const char *uuid, const char *interface, dvb_fe_type_t type);
udp_connection_t *netceiver_frontend_tune(netceiver_frontend_t *ncf, netceiver_group_t group, int priority, dvb_mux_t *dm, int pid, int sid);

/*
 * Monitor
 */

void netceiver_monitor_init(void);
void netceiver_start_monitor(netceiver_frontend_t *ncf, dvb_mux_t *dm);
void netceiver_stop_monitor(netceiver_frontend_t *ncf);

/*
 * Discovery
 */

void netceiver_discovery_init(void);
void netceiver_discovery_add_interface(const char *interface);
void netceiver_discovery_remove_interface(const char *interface);

#endif /* __TVH_NETCEIVER_PRIVATE_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
