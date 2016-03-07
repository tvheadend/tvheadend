/*
 *  tvheadend, CAPMT Server
 *  Copyright (C) 2009
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
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>

#include "tvheadend.h"

#include "input.h"
#include "caclient.h"
#include "service.h"
#include "tcp.h"
#include "tvhpoll.h"

#include "subscriptions.h"
#if ENABLE_LINUXDVB
#include "input/mpegts/linuxdvb/linuxdvb_private.h"
#endif

#if defined(PLATFORM_LINUX)
#include <linux/ioctl.h>
#elif defined(PLATFORM_FREEBSD)
#include <sys/ioccom.h>
#endif

/*
 * Linux compatible definitions
 */

#define DMX_FILTER_SIZE 16

typedef struct dmx_filter {
  uint8_t filter[DMX_FILTER_SIZE];
  uint8_t mask[DMX_FILTER_SIZE];
  uint8_t mode[DMX_FILTER_SIZE];
} dmx_filter_t;

#define DVBAPI_PROTOCOL_VERSION     2

#define CA_SET_DESCR       0x40106f86
#define CA_SET_DESCR_X     0x866f1040
#define CA_SET_DESCR_AES   0x40106f87
#define CA_SET_DESCR_AES_X 0x876f1040
#define CA_SET_PID         0x40086f87
#define CA_SET_PID_X       0x876f0840
#define DMX_STOP           0x00006f2a
#define DMX_STOP_X         0x2a6f0000
#define DMX_SET_FILTER     0x403c6f2b
#define DMX_SET_FILTER_X   0x2b6f3c40
#define DVBAPI_FILTER_DATA 0xFFFF0000
#define DVBAPI_CLIENT_INFO 0xFFFF0001
#define DVBAPI_SERVER_INFO 0xFFFF0002
#define DVBAPI_ECM_INFO    0xFFFF0003

#define PID_UNUSED  (0xffff)
#define PID_BLOCKED (0xfffe)

// ca_pmt_list_management values:
#define CAPMT_LIST_MORE    0x00    // append a 'MORE' CAPMT object the list and start receiving the next object
#define CAPMT_LIST_FIRST   0x01    // clear the list when a 'FIRST' CAPMT object is received, and start receiving the next object
#define CAPMT_LIST_LAST    0x02    // append a 'LAST' CAPMT object to the list and start working with the list
#define CAPMT_LIST_ONLY    0x03    // clear the list when an 'ONLY' CAPMT object is received, and start working with the object
#define CAPMT_LIST_ADD     0x04    // append an 'ADD' CAPMT object to the current list and start working with the updated list
#define CAPMT_LIST_UPDATE  0x05    // replace an entry in the list with an 'UPDATE' CAPMT object, and start working with the updated list

// ca_pmt_cmd_id values:
#define CAPMT_CMD_OK_DESCRAMBLING   0x01  // start descrambling the service in this CAPMT object as soon as the list of CAPMT objects is complete
#define CAPMT_CMD_OK_MMI            0x02  //
#define CAPMT_CMD_QUERY             0x03  //
#define CAPMT_CMD_NOT_SELECTED      0x04

// ca_pmt_descriptor types
#define CAPMT_DESC_PRIVATE 0x81
#define CAPMT_DESC_DEMUX   0x82
#define CAPMT_DESC_PID     0x84

// message type
#define CAPMT_MSG_FAST     0x01
#define CAPMT_MSG_CLEAR    0x02
#define CAPMT_MSG_NODUP    0x04
#define CAPMT_MSG_HELLO    0x08

// limits
#define MAX_CA       16
#define MAX_INDEX    64
#define MAX_FILTER   64
#define MAX_SOCKETS  16   // max sockets (simultaneous channels) per demux
#define MAX_PIDS     64   // max opened pids
#define MAX_INFO_LEN 255

typedef enum {
    CAPMT_OSCAM_SO_WRAPPER,
    CAPMT_OSCAM_OLD,
    CAPMT_OSCAM_MULTILIST,
    CAPMT_OSCAM_TCP,
    CAPMT_OSCAM_UNIX_SOCKET,
    CAPMT_OSCAM_NET_PROTO,
    CAPMT_OSCAM_UNIX_SOCKET_NP /* NET_PROTO through socket */
} capmt_oscam_mode_t;

/**
 *
 */
LIST_HEAD(capmt_service_list, capmt_service);
LIST_HEAD(capmt_caid_ecm_list, capmt_caid_ecm);

/*
 * ECM descriptor
 */
typedef struct ca_info {
  uint16_t pids[MAX_PIDS];	// elementary stream pids (was: sequence / service id number)
} ca_info_t;

/** 
 * capmt descriptor
 */
typedef struct capmt_descriptor {
  uint8_t cad_type;
  uint8_t cad_length;
  uint8_t cad_data[17];
} __attribute__((packed)) capmt_descriptor_t;

/**
 * capmt header structure 
 */
typedef struct capmt_header {
  uint8_t  capmt_indicator[6];
  uint8_t  capmt_list_management;
  uint16_t program_number;
  unsigned reserved1                : 2;
  unsigned version_number           : 5;
  unsigned current_next_indicator   : 1;
  unsigned reserved2                : 4;
  unsigned program_info_length      : 12;
  uint8_t  capmt_cmd_id;
} __attribute__((packed)) capmt_header_t;

/**
 * caid <-> ecm mapping 
 */
typedef struct capmt_caid_ecm {
  /** ca system id */
  uint16_t cce_caid;
  /** ecm pid */
  uint16_t cce_ecmpid;
  /** provider id */
  uint32_t cce_providerid;
  /** service */
  mpegts_service_t *cce_service;

  LIST_ENTRY(capmt_caid_ecm) cce_link;
} capmt_caid_ecm_t;

/**
 *
 */
typedef struct capmt_service {
  th_descrambler_t;

  struct capmt *ct_capmt;

  LIST_ENTRY(capmt_service) ct_link;

  /* capmt (linuxdvb) adapter number */
  int ct_adapter;

  /* list of used ca-systems with ids and last ecm */
  struct capmt_caid_ecm_list ct_caid_ecm;

  /* PIDs list */
  uint16_t ct_pids[MAX_PIDS];
} capmt_service_t;

/**
 **
 */
typedef struct capmt_dmx {
  dmx_filter_t filter;
  uint16_t pid;
  uint32_t flags;
} capmt_dmx_t;

typedef struct capmt_filters {
  int max;
  int adapter;
  capmt_dmx_t dmx[MAX_FILTER];
} capmt_filters_t;

typedef struct capmt_demuxes {
  int max;
  capmt_filters_t filters[MAX_INDEX];
} capmt_demuxes_t;

typedef struct capmt_message {
  TAILQ_ENTRY(capmt_message) cm_link;
  int cm_adapter;
  int cm_sid;
  int cm_flags;
  sbuf_t cm_sb;
} capmt_message_t;

/**
 *
 */
struct capmt;
typedef struct capmt_opaque {
  struct capmt    *capmt;
  uint16_t         adapter;
  uint16_t         pid;
  uint32_t         pid_refs;
  int              ecm;
} capmt_opaque_t;

typedef struct capmt_adapter {
  ca_info_t       ca_info[MAX_INDEX];
  int             ca_sock;
  mpegts_input_t *ca_tuner;
  capmt_opaque_t  ca_pids[MAX_PIDS];
} capmt_adapter_t;

/**
 *
 */
typedef struct capmt {
  caclient_t;

  tvh_cond_t capmt_cond;

  struct capmt_service_list capmt_services;

  pthread_t capmt_tid;

  /* from capmt configuration */
  char *capmt_sockfile;
  int   capmt_port;
  int   capmt_oscam;

  /* capmt sockets */
  int   sids[MAX_SOCKETS];
  int   adps[MAX_SOCKETS];
  int   capmt_sock[MAX_SOCKETS];
  int   capmt_sock_reconnect[MAX_SOCKETS];

  /* thread flags */
  int   capmt_connected;
  int   capmt_running;
  int   capmt_reconfigure;

  /* runtime status */
  tvhpoll_t      *capmt_poll;
  th_pipe_t       capmt_pipe;
  capmt_demuxes_t capmt_demuxes;
  capmt_adapter_t capmt_adapters[MAX_CA];
  TAILQ_HEAD(, capmt_message) capmt_writeq;
  pthread_mutex_t capmt_mutex;
} capmt_t;

static void capmt_enumerate_services(capmt_t *capmt, int force);
static void capmt_notify_server(capmt_t *capmt, capmt_service_t *ct, int force);
static void capmt_send_request(capmt_service_t *ct, int lm);
static void capmt_table_input(void *opaque, int pid,
                              const uint8_t *data, int len, int emm);
static void capmt_send_client_info(capmt_t *capmt);

/**
 *
 */
static inline const char *
capmt_name(capmt_t *capmt)
{
  return idnode_get_title(&capmt->cac_id, NULL);
}

static inline int
capmt_oscam_new(capmt_t *capmt)
{
  int oscam = capmt->capmt_oscam;
  return oscam != CAPMT_OSCAM_SO_WRAPPER &&
         oscam != CAPMT_OSCAM_OLD;
}

static inline int
capmt_oscam_netproto(capmt_t *capmt)
{
  int oscam = capmt->capmt_oscam;
  return oscam == CAPMT_OSCAM_NET_PROTO ||
         oscam == CAPMT_OSCAM_UNIX_SOCKET_NP;
}

static void
capmt_poll_add(capmt_t *capmt, int fd, uint32_t u32)
{
  tvhpoll_event_t ev;

  if (capmt->capmt_poll == NULL)
    return;
  memset(&ev, 0, sizeof(ev));
  ev.events   = TVHPOLL_IN;
  ev.fd       = fd;
  ev.data.u32 = u32;
  tvhpoll_add(capmt->capmt_poll, &ev, 1);
}

static void
capmt_poll_rem(capmt_t *capmt, int fd)
{
  tvhpoll_event_t ev;

  if (capmt->capmt_poll == NULL)
    return;
  memset(&ev, 0, sizeof(ev));
  ev.fd       = fd;
  tvhpoll_rem(capmt->capmt_poll, &ev, 1);
}

static void
capmt_pid_add(capmt_t *capmt, int adapter, int pid, mpegts_service_t *s)
{
  capmt_adapter_t *ca = &capmt->capmt_adapters[adapter];
  capmt_opaque_t *o = NULL, *t;
  mpegts_mux_instance_t *mmi;
  mpegts_mux_t *mux;
  int i = 0, ecm = s != NULL;

  lock_assert(&capmt->capmt_mutex);

  for (i = 0; i < MAX_PIDS; i++) {
    t = &ca->ca_pids[i];
    if (t->pid == pid && t->ecm == ecm) {
      t->pid_refs++;
      tvhtrace("capmt", "%s: adding pid %d adapter %d - reusing (%d)",
               capmt_name(capmt), pid, adapter, t->pid_refs);
      return;
    }
    if (t->pid == PID_UNUSED)
      o = t;
  }
  if (o) {
    o->capmt    = capmt;
    o->adapter  = adapter;
    o->pid      = pid;
    o->pid_refs = 1;
    o->ecm      = ecm;
    mmi         = ca->ca_tuner ? LIST_FIRST(&ca->ca_tuner->mi_mux_active) : NULL;
    mux         = mmi ? mmi->mmi_mux : NULL;
    tvhtrace("capmt", "%s: adding pid %d adapter %d, tuner %p, mmi %p, mux %p", capmt_name(capmt), pid, adapter, ca->ca_tuner, mmi, mux);
    if (mux) {
      pthread_mutex_unlock(&capmt->capmt_mutex);
      descrambler_open_pid(mux, o,
                           s ? DESCRAMBLER_ECM_PID(pid) : pid,
                           capmt_table_input, (service_t *)s);
      pthread_mutex_lock(&capmt->capmt_mutex);
    }
  }
}

static void
capmt_pid_remove(capmt_t *capmt, int adapter, int pid, uint32_t flags)
{
  capmt_adapter_t *ca = &capmt->capmt_adapters[adapter];
  capmt_opaque_t *o;
  mpegts_mux_instance_t *mmi;
  mpegts_mux_t *mux;
  int i = 0, ecm = (flags & CAPMT_MSG_FAST) != 0;

  lock_assert(&capmt->capmt_mutex);

  if (pid <= 0)
    return;
  for (i = 0; i < MAX_PIDS; i++) {
    o = &ca->ca_pids[i];
    if (o->pid == pid && o->ecm == ecm) {
      if (--o->pid_refs == 0)
        break;
      return;
    }
  }
  if (i >= MAX_PIDS)
    return;
  mmi = ca->ca_tuner ? LIST_FIRST(&ca->ca_tuner->mi_mux_active) : NULL;
  mux = mmi ? mmi->mmi_mux : NULL;
  o->pid = PID_BLOCKED;
  o->pid_refs = 0;
  if (o->ecm)
    pid = DESCRAMBLER_ECM_PID(pid);
  o->ecm = -1;
  if (mux) {
    pthread_mutex_unlock(&capmt->capmt_mutex);
    descrambler_close_pid(mux, o, pid);
    pthread_mutex_lock(&capmt->capmt_mutex);
  }
  o->pid = PID_UNUSED;
}

static void
capmt_pid_flush_adapter(capmt_t *capmt, int adapter)
{
  capmt_adapter_t *ca;
  mpegts_mux_instance_t *mmi;
  mpegts_input_t *tuner;
  mpegts_mux_t *mux;
  capmt_opaque_t *o;
  int pid, i;

  tvhtrace("capmt", "%s: pid flush for adapter %d", capmt_name(capmt), adapter);
  ca = &capmt->capmt_adapters[adapter];
  tuner = capmt->capmt_adapters[adapter].ca_tuner;
  if (tuner == NULL) {
    /* clean all pids (to be sure) */
    for (i = 0; i < MAX_PIDS; i++) {
      o = &ca->ca_pids[i];
      o->pid = PID_UNUSED;
      o->pid_refs = 0;
    }
    return;
  }
  mmi = LIST_FIRST(&tuner->mi_mux_active);
  mux = mmi ? mmi->mmi_mux : NULL;
  for (i = 0; i < MAX_PIDS; i++) {
    o = &ca->ca_pids[i];
    if ((pid = o->pid) > 0) {
      o->pid = PID_BLOCKED;
      o->pid_refs = 0;
      if (mux) {
        pthread_mutex_unlock(&capmt->capmt_mutex);
        descrambler_close_pid(mux, &ca->ca_pids[i], pid);
        pthread_mutex_lock(&capmt->capmt_mutex);
      }
      o->pid = PID_UNUSED;
    }
  }
}

static void
capmt_pid_flush(capmt_t *capmt)
{
  int adapter;
  for (adapter = 0; adapter < MAX_CA; adapter++)
    capmt_pid_flush_adapter(capmt, adapter);
}

/*
 *
 */
static int
capmt_connect(capmt_t *capmt, int i)
{
  int fd;

  capmt->capmt_sock[i] = -1;

  if (!capmt->capmt_running)
    return -1;

  if (capmt->capmt_oscam == CAPMT_OSCAM_TCP ||
      capmt->capmt_oscam == CAPMT_OSCAM_NET_PROTO) {

    char errbuf[256];

    fd = tcp_connect(capmt->capmt_sockfile, capmt->capmt_port, NULL,
                     errbuf, sizeof(errbuf), 2);
    if (fd < 0 && tvheadend_running) {
      tvhlog(LOG_ERR, "capmt",
             "%s: Cannot connect to %s:%i (%s); Do you have OSCam running?",
             capmt_name(capmt), capmt->capmt_sockfile, capmt->capmt_port, errbuf);
      fd = -1;
    }

  } else {

    struct sockaddr_un serv_addr_un;

    memset(&serv_addr_un, 0, sizeof(serv_addr_un));
    serv_addr_un.sun_family = AF_LOCAL;
    snprintf(serv_addr_un.sun_path,
             sizeof(serv_addr_un.sun_path),
             "%s", capmt->capmt_sockfile);

    fd = tvh_socket(AF_LOCAL, SOCK_STREAM, 0);
    if (fd < 0 ||
        connect(fd, (const struct sockaddr*)&serv_addr_un,
                sizeof(serv_addr_un)) != 0) {
      if (tvheadend_running)
        tvhlog(LOG_ERR, "capmt",
               "%s: Cannot connect to %s (%s); Do you have OSCam running?",
               capmt_name(capmt), capmt->capmt_sockfile, strerror(errno));
      if (fd >= 0) {
        close(fd);
        fd = -1;
      }
    }

  }

  if (fd >= 0) {
    tvhlog(LOG_DEBUG, "capmt", "%s: Created socket %d", capmt_name(capmt), fd);
    capmt->capmt_sock[i] = fd;
    capmt->capmt_sock_reconnect[i]++;
    if (capmt_oscam_netproto(capmt))
      capmt_send_client_info(capmt);
    capmt_poll_add(capmt, fd, i + 1);
  }

  return fd;
}

/**
 *
 */
static void
capmt_socket_close(capmt_t *capmt, int sock_idx)
{
  int fd = capmt->capmt_sock[sock_idx];
  lock_assert(&capmt->capmt_mutex);
  tvhtrace("capmt", "%s: socket close %d", capmt_name(capmt), fd);
  if (fd < 0)
    return;
  capmt_poll_rem(capmt, fd);
  close(fd);
  capmt->capmt_sock[sock_idx] = -1;
  if (capmt_oscam_new(capmt))
    capmt_pid_flush(capmt);
  else if (capmt->capmt_oscam == CAPMT_OSCAM_OLD)
    capmt->sids[sock_idx] = capmt->adps[sock_idx] = -1;
}

static void
capmt_socket_close_lock(capmt_t *capmt, int sock_idx)
{
  pthread_mutex_lock(&capmt->capmt_mutex);
  capmt_socket_close(capmt, sock_idx);
  pthread_mutex_unlock(&capmt->capmt_mutex);
}

/**
 *
 */
static int
capmt_write_msg(capmt_t *capmt, int adapter, int sid, const uint8_t *buf, size_t len)
{
  int i = 0, found = 0, fd;
  ssize_t res;

  if (capmt->capmt_oscam != CAPMT_OSCAM_SO_WRAPPER) {
    if (capmt->capmt_oscam == CAPMT_OSCAM_OLD) {
      // dumping current SID table
      for (i = 0; i < MAX_SOCKETS; i++)
        tvhlog(LOG_DEBUG, "capmt", "%s: %s: SOCKETS TABLE DUMP [%d]: sid=%d socket=%d", capmt_name(capmt), __FUNCTION__, i, capmt->sids[i], capmt->capmt_sock[i]);
      if (sid == 0) {
        tvhlog(LOG_DEBUG, "capmt", "%s: %s: got empty SID - returning from function", capmt_name(capmt), __FUNCTION__);
        return -1;
      }

      // searching for the SID and socket
      for (i = 0; i < MAX_SOCKETS; i++) {
        if (capmt->sids[i] == sid && capmt->adps[i] == adapter) {
          found = 1;
          break;
        }
      }

      if (found)
        tvhlog(LOG_DEBUG, "capmt", "%s: %s: found sid, reusing socket, i=%d", capmt_name(capmt), __FUNCTION__, i);
      else {                       //not found - adding to first free in table
        for (i = 0; i < MAX_SOCKETS; i++) {
          if (capmt->sids[i] == 0) {
            capmt->sids[i] = sid;
            capmt->adps[i] = adapter;
            break;
          }
        }
      }
      if (i == MAX_SOCKETS) {
        tvhlog(LOG_DEBUG, "capmt", "%s: %s: no free space for new SID!!!", capmt_name(capmt), __FUNCTION__);
        return -1;
      } else {
        capmt->sids[i] = sid;
        tvhlog(LOG_DEBUG, "capmt", "%s: %s: added: i=%d", capmt_name(capmt), __FUNCTION__, i);
      }
    }

    // check if the socket is still alive by writing 0 bytes
    if (capmt->capmt_sock[i] >= 0) {
      if (send(capmt->capmt_sock[i], NULL, 0, MSG_DONTWAIT) < 0)
        capmt->capmt_sock[i] = -1;
      else if ((capmt->capmt_oscam == CAPMT_OSCAM_SO_WRAPPER ||
                capmt->capmt_oscam == CAPMT_OSCAM_OLD) && found)
        return 0;
    }

    // opening socket and sending
    if (capmt->capmt_sock[i] < 0) {
      fd = capmt_connect(capmt, i);
      caclient_set_status((caclient_t *)capmt,
                          fd >= 0 ? CACLIENT_STATUS_CONNECTED :
                                    CACLIENT_STATUS_READY);
      if (fd >= 0)
        capmt_notify_server(capmt, NULL, 1);
    }
  } else {  // standard old capmt mode
    i = 0;
  }

  fd = capmt->capmt_sock[i];

  if (fd <= 0) {
    tvhtrace("capmt", "%s: Unable to send message for sid %i", capmt_name(capmt), sid);
    return -1;
  }

  tvhtrace("capmt", "%s: Sending message to socket %i (sid %i)", capmt_name(capmt), fd, sid);
  tvhlog_hexdump("capmt", buf, len);

  res = send(fd, buf, len, MSG_DONTWAIT);
  if (res < len) {
#if ENABLE_ANDROID
    tvhlog(LOG_DEBUG, "capmt", "%s: Message send failed to socket %i (%li)", capmt_name(capmt), fd, (long int)res); // Android bug, ssize_t is long int
#else
    tvhlog(LOG_DEBUG, "capmt", "%s: Message send failed to socket %i (%zi)", capmt_name(capmt), fd, res);
#endif
    if (capmt->capmt_oscam != CAPMT_OSCAM_SO_WRAPPER) {
      capmt_socket_close_lock(capmt, i);
      return -1;
    }
  }

  return 0;
}

/**
 *
 */
static void
capmt_queue_msg
  (capmt_t *capmt, int adapter, int sid,
   const uint8_t *buf, size_t len, int flags)
{
  capmt_message_t *msg, *msg2;

  if (flags & CAPMT_MSG_CLEAR) {
    while ((msg = TAILQ_FIRST(&capmt->capmt_writeq)) != NULL) {
      TAILQ_REMOVE(&capmt->capmt_writeq, msg, cm_link);
      sbuf_free(&msg->cm_sb);
      free(msg);
    }
  }
  if (flags & CAPMT_MSG_NODUP) {
    TAILQ_FOREACH(msg, &capmt->capmt_writeq, cm_link)
      if (msg->cm_sb.sb_ptr == len &&
          memcmp(msg->cm_sb.sb_data, buf, len) == 0)
        return;
  }
  msg = malloc(sizeof(*msg));
  sbuf_init_fixed(&msg->cm_sb, len);
  sbuf_append(&msg->cm_sb, buf, len);
  msg->cm_adapter = adapter;
  msg->cm_sid     = sid;
  msg->cm_flags   = flags;
  if (flags & CAPMT_MSG_FAST) {
    msg2 = TAILQ_FIRST(&capmt->capmt_writeq);
    TAILQ_INSERT_HEAD(&capmt->capmt_writeq, msg, cm_link);
    if (msg2 && (msg2->cm_flags & CAPMT_MSG_HELLO) != 0) {
      TAILQ_REMOVE(&capmt->capmt_writeq, msg2, cm_link);
      TAILQ_INSERT_HEAD(&capmt->capmt_writeq, msg2, cm_link);
    }
  } else
    TAILQ_INSERT_TAIL(&capmt->capmt_writeq, msg, cm_link);
  tvh_write(capmt->capmt_pipe.wr, "c", 1);
}

/**
 *
 */
static void
capmt_flush_queue(capmt_t *capmt, int del_only)
{
  capmt_message_t *msg;

  while (1) {
    pthread_mutex_lock(&capmt->capmt_mutex);
    msg = TAILQ_FIRST(&capmt->capmt_writeq);
    if (msg)
      TAILQ_REMOVE(&capmt->capmt_writeq, msg, cm_link);
    pthread_mutex_unlock(&capmt->capmt_mutex);
    if (msg == NULL)
      break;

    if (!del_only)
      capmt_write_msg(capmt, msg->cm_adapter, msg->cm_sid,
                      msg->cm_sb.sb_data, msg->cm_sb.sb_ptr);
    sbuf_free(&msg->cm_sb);
    free(msg);
  }
}

/**
 *
 */
static void 
capmt_send_stop(capmt_service_t *t)
{
  mpegts_service_t *s = (mpegts_service_t *)t->td_service;
  capmt_t *capmt = t->ct_capmt;
  int oscam = capmt->capmt_oscam;

  lock_assert(&capmt->capmt_mutex);

  if (oscam == CAPMT_OSCAM_OLD) {
    int i;
    // searching for socket to close
    for (i = 0; i < MAX_SOCKETS; i++)
      if (capmt->sids[i] == s->s_dvb_service_id)
        break;

    if (i == MAX_SOCKETS) {
      tvhlog(LOG_DEBUG, "capmt", "%s: %s: socket to close not found", capmt_name(capmt), __FUNCTION__);
      return;
    }

    // closing socket (oscam handle this as event and stop decrypting)
    tvhlog(LOG_DEBUG, "capmt", "%s: %s: closing socket i=%d, socket_fd=%d", capmt_name(capmt), __FUNCTION__, i, capmt->capmt_sock[i]);
    capmt->sids[i] = 0;
    capmt->adps[i] = 0;
    capmt_socket_close(capmt, i);
  } else if (oscam == CAPMT_OSCAM_SO_WRAPPER) {  // standard old capmt mode
    /* buffer for capmt */
    int pos = 0;
    uint8_t buf[4094];

    capmt_header_t head = {
      .capmt_indicator        = { 0x9F, 0x80, 0x32, 0x82, 0x00, 0x00 },
      .capmt_list_management  = CAPMT_LIST_ONLY,
      .program_number         = s->s_dvb_service_id,
      .version_number         = 0,
      .current_next_indicator = 0,
      .program_info_length    = 0,
      .capmt_cmd_id           = CAPMT_CMD_NOT_SELECTED,
    };
    memcpy(&buf[pos], &head, sizeof(head));
    pos    += sizeof(head);

    uint8_t end[] = {
      0x01, (t->ct_pids[0] >> 8) & 0xFF, t->ct_pids[0] & 0xFF, 0x00, 0x06 };
    memcpy(&buf[pos], end, sizeof(end));
    pos    += sizeof(end);
    buf[4]  = ((pos - 6) >> 8);
    buf[5]  = ((pos - 6) & 0xFF);
    buf[7]  = s->s_dvb_service_id >> 8;
    buf[8]  = s->s_dvb_service_id & 0xFF;
    buf[9]  = 1;
    buf[10] = ((pos - 5 - 12) & 0xF00) >> 8;
    buf[11] = ((pos - 5 - 12) & 0xFF);
  
    capmt_queue_msg(capmt, t->ct_adapter, s->s_dvb_service_id,
                    buf, pos, CAPMT_MSG_CLEAR);
  }
}

/**
 *
 */
static void
capmt_send_stop_descrambling(capmt_t *capmt)
{
  static uint8_t buf[8] = {
    0x9F,
    0x80,
    0x3F,
    0x04,
    0x83,
    0x02,
    0x00,
    0xFF, /* wildcard demux id */
  };
  capmt_write_msg(capmt, 0, 0, buf, 8);
}

/**
 * global_lock is held
 * s_stream_mutex is held
 */
static void 
capmt_service_destroy(th_descrambler_t *td)
{
  capmt_service_t *ct = (capmt_service_t *)td;
  mpegts_service_t *s = (mpegts_service_t *)ct->td_service;
  int oscam_new = capmt_oscam_new(ct->ct_capmt);
  capmt_caid_ecm_t *cce;
  capmt_t *capmt = ct->ct_capmt;

  tvhlog(LOG_INFO, "capmt",
         "%s: Removing CAPMT Server from service \"%s\" on adapter %d",
         capmt_name(capmt), s->s_dvb_svcname, ct->ct_adapter);

  pthread_mutex_lock(&capmt->capmt_mutex);

  /* send stop to client */
  if (!oscam_new)
    capmt_send_stop(ct);

  while (!LIST_EMPTY(&ct->ct_caid_ecm)) { 
    /* List Deletion. */
    cce = LIST_FIRST(&ct->ct_caid_ecm);
    LIST_REMOVE(cce, cce_link);
    free(cce);
  }

  LIST_REMOVE(td, td_service_link);

  LIST_REMOVE(ct, ct_link);

  if (oscam_new)
    capmt_enumerate_services(capmt, 1);

  if (LIST_EMPTY(&capmt->capmt_services)) {
    capmt_pid_flush_adapter(capmt, ct->ct_adapter);
    capmt->capmt_adapters[ct->ct_adapter].ca_tuner = NULL;
    memset(&capmt->capmt_demuxes, 0, sizeof(capmt->capmt_demuxes));
  }

  pthread_mutex_unlock(&capmt->capmt_mutex);

  free(ct->td_nicename);
  free(ct);
}

static void
capmt_send_client_info(capmt_t *capmt)
{
  char buf[MAX_INFO_LEN + 7];
  int len;

  *(uint32_t *)(buf + 0) = htonl(DVBAPI_CLIENT_INFO);
  *(uint16_t *)(buf + 4) = htons(DVBAPI_PROTOCOL_VERSION); //supported protocol version
  len = snprintf(buf + 7, sizeof(buf) - 7, "Tvheadend %s", tvheadend_version);
  if (len >= sizeof(buf) - 7)
    len = sizeof(buf) - 7 - 1;
  buf[6] = len;

  capmt_queue_msg(capmt, 0, 0, (uint8_t *)&buf, len + 7,
                  CAPMT_MSG_FAST|CAPMT_MSG_NODUP|CAPMT_MSG_HELLO);
}

static void
capmt_filter_data(capmt_t *capmt, uint8_t adapter, uint8_t demux_index,
                  uint8_t filter_index, const uint8_t *data, int len,
                  int flags)
{
  uint8_t *buf = alloca(len + 6);

  *(uint32_t *)(buf + 0) = htonl(DVBAPI_FILTER_DATA);
  buf[4] = demux_index;
  buf[5] = filter_index;
  memcpy(buf + 6, data, len);
  if (len - 3 == ((((uint16_t)buf[7] << 8) | buf[8]) & 0xfff))
    capmt_queue_msg(capmt, adapter, 0, buf, len + 6, flags);
}

static void
capmt_set_filter(capmt_t *capmt, int adapter, sbuf_t *sb, int offset)
{
  uint8_t demux_index  = sbuf_peek_u8 (sb, offset + 4);
  uint8_t filter_index = sbuf_peek_u8 (sb, offset + 5);
  uint16_t pid         = sbuf_peek_u16(sb, offset + 6);
  capmt_dmx_t *filter;
  capmt_filters_t *cf;
  capmt_service_t *ct;
  mpegts_service_t *t;
  capmt_caid_ecm_t *cce;
  int i, flags = 0, add = 0;

  tvhtrace("capmt", "%s: setting filter: adapter=%d, demux=%d, filter=%d, pid=%d",
           capmt_name(capmt), adapter, demux_index, filter_index, pid);
  if (adapter >= MAX_CA ||
      demux_index >= MAX_INDEX ||
      filter_index >= MAX_FILTER ||
      pid > 8191)
    return;
  cf = &capmt->capmt_demuxes.filters[demux_index];
  if (cf->max && cf->adapter != adapter)
    return;

  pthread_mutex_lock(&capmt->capmt_mutex);

  /* ECM messages have the higher priority */
  t = NULL;
  LIST_FOREACH(ct, &capmt->capmt_services, ct_link) {
    LIST_FOREACH(cce, &ct->ct_caid_ecm, cce_link)
      if (cce->cce_ecmpid == pid) {
        flags = CAPMT_MSG_FAST;
        t = cce->cce_service;
        break;
      }
    if (t) break;
  }
  if (t) {
    dmx_filter_t *pf = (dmx_filter_t *)sbuf_peek(sb, offset + 8);
    /* OK, probably ECM, but sometimes, it's shared */
    /* Inspect the filter */
    for (i = 1; i < DMX_FILTER_SIZE; i++) {
      if (pf->mode[i]) break;
      if (pf->filter[i]) break;
      if (pf->mask[i]) break;
    }
    if (i < DMX_FILTER_SIZE ||
        pf->mode[0] ||
        (pf->filter[0] & 0xf0) != 0x80 ||
        (pf->mask[0] & 0xf0) != 0xf0)
      t = NULL;
  }

  cf->adapter = adapter;
  filter = &cf->dmx[filter_index];
  if (!filter->pid) {
    add = 1;
  } else if (pid != filter->pid || flags != filter->flags) {
    capmt_pid_remove(capmt, adapter, filter->pid, filter->flags);
    add = 1;
  }
  filter->pid = pid;
  filter->flags = flags;
  memcpy(&filter->filter, sbuf_peek(sb, offset + 8), sizeof(filter->filter));
  tvhlog_hexdump("capmt", filter->filter.filter, DMX_FILTER_SIZE);
  tvhlog_hexdump("capmt", filter->filter.mask, DMX_FILTER_SIZE);
  tvhlog_hexdump("capmt", filter->filter.mode, DMX_FILTER_SIZE);
  for (i = 0; i < DMX_FILTER_SIZE; i++)
    filter->filter.filter[i] &= filter->filter.mask[i];
  if (add)
    capmt_pid_add(capmt, adapter, pid, t);
  /* Update the max values */
  if (capmt->capmt_demuxes.max <= demux_index)
    capmt->capmt_demuxes.max = demux_index + 1;
  if (cf->max <= filter_index)
    cf->max = filter_index + 1;
  pthread_mutex_unlock(&capmt->capmt_mutex);
}

static void
capmt_stop_filter(capmt_t *capmt, int adapter, sbuf_t *sb, int offset)
{
  uint8_t demux_index  = sbuf_peek_u8   (sb, offset + 4);
  uint8_t filter_index = sbuf_peek_u8   (sb, offset + 5);
  int16_t pid;
  uint32_t flags;
  capmt_dmx_t *filter;
  capmt_filters_t *cf;

  if (capmt_oscam_netproto(capmt))
    pid          = sbuf_peek_s16  (sb, offset + 6);
  else
    pid          = sbuf_peek_s16be(sb, offset + 6);

  tvhtrace("capmt", "%s: stopping filter: adapter=%d, demux=%d, filter=%d, pid=%d",
           capmt_name(capmt), adapter, demux_index, filter_index, pid);
  if (adapter >= MAX_CA ||
      demux_index >= MAX_INDEX ||
      filter_index >= MAX_FILTER)
    return;
  cf = &capmt->capmt_demuxes.filters[demux_index];
  filter = &cf->dmx[filter_index];
  if (filter->pid != pid)
    return;
  flags = filter->flags;
  pthread_mutex_lock(&capmt->capmt_mutex);
  memset(filter, 0, sizeof(*filter));
  capmt_pid_remove(capmt, adapter, pid, flags);
  /* short the max values */
  filter_index = cf->max - 1;
  while (filter_index != 255 && cf->dmx[filter_index].pid == 0)
    filter_index--;
  cf->max = filter_index == 255 ? 0 : filter_index + 1;
  demux_index = capmt->capmt_demuxes.max - 1;
  while (demux_index != 255 && capmt->capmt_demuxes.filters[demux_index].max == 0)
    demux_index--;
  capmt->capmt_demuxes.max = demux_index == 255 ? 0 : demux_index + 1;
  pthread_mutex_unlock(&capmt->capmt_mutex);
}

static void
capmt_notify_server(capmt_t *capmt, capmt_service_t *ct, int force)
{
  /* flush out the greeting */
  if (capmt_oscam_netproto(capmt))
    capmt_flush_queue(capmt, 0);

  pthread_mutex_lock(&capmt->capmt_mutex);
  if (capmt_oscam_new(capmt)) {
    if (!LIST_EMPTY(&capmt->capmt_services))
      capmt_enumerate_services(capmt, force);
  } else {
    if (ct)
      capmt_send_request(ct, CAPMT_LIST_ONLY);
    else
      LIST_FOREACH(ct, &capmt->capmt_services, ct_link)
        capmt_send_request(ct, CAPMT_LIST_ONLY);
  }
  pthread_mutex_unlock(&capmt->capmt_mutex);
}

#if CONFIG_LINUXDVB
static void
capmt_abort(capmt_t *capmt, int keystate)
{
  mpegts_service_t *t;
  capmt_service_t *ct;

  pthread_mutex_lock(&capmt->capmt_mutex);
  LIST_FOREACH(ct, &capmt->capmt_services, ct_link) {
    t = (mpegts_service_t *)ct->td_service;

    if (ct->td_keystate != keystate) {
      tvhlog(LOG_ERR, "capmt",
             "%s: Can not descramble service \"%s\", %s",
             capmt_name(capmt),
             t->s_dvb_svcname,
             keystate == DS_FORBIDDEN ?
               "access denied" : "connection close");
      ct->td_keystate = keystate;
    }
  }
  pthread_mutex_unlock(&capmt->capmt_mutex);
}
#endif

static int
capmt_ecm_reset(th_descrambler_t *th)
{
  capmt_service_t *ct = (capmt_service_t *)th;

  ct->td_keystate = DS_UNKNOWN;
  return 0;
}

static void
capmt_process_key(capmt_t *capmt, uint8_t adapter, uint32_t index,
                  int type, const uint8_t *even, const uint8_t *odd,
                  int ok)
{
  mpegts_service_t *t;
  capmt_service_t *ct;
  ca_info_t *cai;
  uint16_t *pids;
  int i, j;

  pthread_mutex_lock(&capmt->capmt_mutex);
  LIST_FOREACH(ct, &capmt->capmt_services, ct_link) {
    t = (mpegts_service_t *)ct->td_service;

    if (!ok) {
      if (ct->td_keystate != DS_FORBIDDEN) {
        tvhlog(LOG_ERR, "capmt",
               "%s: Can not descramble service \"%s\", access denied",
               capmt_name(capmt), t->s_dvb_svcname);
        ct->td_keystate = DS_FORBIDDEN;
      }
      continue;
    }

    if (adapter != ct->ct_adapter)
      continue;

    cai = &capmt->capmt_adapters[adapter].ca_info[index];
    pids = cai->pids;

    for (i = 0; i < MAX_PIDS; i++) {
      if (pids[i] == 0) continue;
      for (j = 0; j < MAX_PIDS; j++) {
        if (ct->ct_pids[j] == 0) break;
        if (ct->ct_pids[j] == pids[i])
          goto found;
      }
    }
    continue;

found:
    descrambler_keys((th_descrambler_t *)ct, type, even, odd);
  }
  pthread_mutex_unlock(&capmt->capmt_mutex);
}

static void
capmt_process_notify(capmt_t *capmt, uint8_t adapter,
                     uint16_t sid, uint16_t caid, uint32_t provid,
                     const char *cardsystem, uint16_t pid, uint32_t ecmtime,
                     uint16_t hops, const char *reader, const char *from,
                     const char *protocol )
{
  mpegts_service_t *t;
  capmt_service_t *ct;

  pthread_mutex_lock(&capmt->capmt_mutex);
  LIST_FOREACH(ct, &capmt->capmt_services, ct_link) {
    t = (mpegts_service_t *)ct->td_service;

    if (sid != t->s_dvb_service_id)
      continue;
    if (adapter != ct->ct_adapter)
      continue;

    descrambler_notify((th_descrambler_t *)ct, caid, provid,
                       cardsystem, pid, ecmtime, hops, reader, from,
                       protocol);
  }
  pthread_mutex_unlock(&capmt->capmt_mutex);
}                     

static int
capmt_msg_size(capmt_t *capmt, sbuf_t *sb, int offset)
{
  uint32_t cmd;
  uint8_t adapter_byte = 0;
  int oscam_new = capmt_oscam_new(capmt);

  if (sb->sb_ptr - offset < 4)
    return 0;
  cmd = sbuf_peek_u32(sb, offset);
  if (capmt_oscam_netproto(capmt))  {
    adapter_byte = 1; //we need to take into account the adapter index byte which is now after the cmd
  } else {
    if (!sb->sb_bswap && !sb->sb_err) {
      if (cmd == CA_SET_PID_X ||
          cmd == CA_SET_DESCR_X ||
          cmd == CA_SET_DESCR_AES_X ||
          cmd == DMX_SET_FILTER_X ||
          cmd == DMX_STOP_X) {
        sb->sb_bswap = 1;
        cmd = sbuf_peek_u32(sb, offset);
      }
    }
  }
  sb->sb_err = 1; /* "first seen" flag for the moment */
  if (cmd == CA_SET_PID)
    return 4 + 8 + adapter_byte;
  else if (cmd == CA_SET_DESCR)
    return 4 + 16 + adapter_byte;
  else if (cmd == CA_SET_DESCR_AES)
    return 4 + 32;
  else if (oscam_new && cmd == DMX_SET_FILTER)
    //when using network protocol the dmx_sct_filter_params fields are added seperately to avoid padding problems, so we substract 2 bytes:
    return 4 + 2 + 60 + adapter_byte + (capmt_oscam_netproto(capmt) ? -2 : 0);
  else if (oscam_new && cmd == DMX_STOP)
    return 4 + 4 + adapter_byte;
  else if (oscam_new && cmd == DVBAPI_SERVER_INFO)
    return sb->sb_ptr < 7 ? 0 : 4 + 2 + 1 + sbuf_peek_u8(sb, 6);
  else if (oscam_new && cmd == DVBAPI_ECM_INFO) {
    if (sb->sb_ptr < 15)
      return 0;
    int len = 4 + adapter_byte + 2 + 2 + 2 + 4 + 4;
    int i;
    for (i=0; i<4; i++) {
      if (len >= sb->sb_ptr)
        return 0;
      len += sbuf_peek_u8(sb, len) + 1;
    }
    len += 1;
    return len;
  }
  else {
    sb->sb_err = 0;
    return -1; /* fatal */
  }
}

static char *
capmt_peek_str(sbuf_t *sb, int *offset)
{
  uint8_t len = sbuf_peek_u8(sb, *offset);
  char *str = malloc(len + 1), *p;
  memcpy(str, sbuf_peek(sb, *offset + 1), len);
  str[len] = '\0';
  for (p = str; *p; p++)
    if (*p < ' ') *p = ' ';
  *offset += len + 1;
  return str;
}

static void
capmt_analyze_cmd(capmt_t *capmt, int adapter, sbuf_t *sb, int offset)
{
  static uint8_t empty[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  uint32_t cmd;

  cmd = sbuf_peek_u32(sb, offset);

  if (capmt_oscam_netproto(capmt) && cmd != DVBAPI_SERVER_INFO) {
    adapter = sbuf_peek_u8(sb, 4);
    offset = 1;
  }

  if (cmd == CA_SET_PID) {

    uint32_t pid   = sbuf_peek_u32(sb, offset + 4);
    int32_t  index = sbuf_peek_s32(sb, offset + 8);
    int i, j;
    ca_info_t *cai;

    tvhlog(LOG_DEBUG, "capmt", "%s: CA_SET_PID adapter %d index %d pid %d (0x%04x)", capmt_name(capmt), adapter, index, pid, pid);
    if (adapter < MAX_CA && index >= 0 && index < MAX_INDEX) {
      cai = &capmt->capmt_adapters[adapter].ca_info[index];
      for (i = 0, j = -1; i < MAX_PIDS; i++) {
        if (cai->pids[i] == pid)
          break;
        if (j < 0 && cai->pids[i] == 0)
          j = i;
      }
      if (i >= MAX_PIDS && j >= 0)
        cai->pids[j] = pid;
    } else if (index < 0) {
      for (index = 0; index < MAX_INDEX; index++) {
        cai = &capmt->capmt_adapters[adapter].ca_info[index];
        for (i = 0; i < MAX_PIDS; i++)
          if (cai->pids[i] == pid)
            cai->pids[i] = 0;
      }
    } else {
      tvhlog(LOG_ERR, "capmt", "%s: Invalid index %d in CA_SET_PID (%d) for adapter %d", capmt_name(capmt), index, MAX_INDEX, adapter);
    }

  } else if (cmd == CA_SET_DESCR) {

    int32_t index  = sbuf_peek_s32(sb, offset + 4);
    int32_t parity = sbuf_peek_s32(sb, offset + 8);
    uint8_t *cw    = sbuf_peek    (sb, offset + 12);

    tvhlog(LOG_DEBUG, "capmt", "%s, CA_SET_DESCR adapter %d par %d idx %d %02x%02x%02x%02x%02x%02x%02x%02x",
                      capmt_name(capmt), adapter, parity, index,
                      cw[0], cw[1], cw[2], cw[3], cw[4], cw[5], cw[6], cw[7]);
    if (index == -1)   // skipping removal request
      return;
    if (adapter >= MAX_CA || index >= MAX_INDEX)
      return;
    if (parity == 0) {
      capmt_process_key(capmt, adapter, index, DESCRAMBLER_DES, cw, empty, 1);
    } else if (parity == 1) {
      capmt_process_key(capmt, adapter, index, DESCRAMBLER_DES, empty, cw, 1);
    } else
      tvhlog(LOG_ERR, "capmt", "%s: Invalid parity %d in CA_SET_DESCR for adapter%d", capmt_name(capmt), parity, adapter);

  } else if (cmd == CA_SET_DESCR_AES) {

    int32_t index  = sbuf_peek_s32(sb, offset + 4);
    int32_t parity = sbuf_peek_s32(sb, offset + 8);
    uint8_t *cw    = sbuf_peek    (sb, offset + 12);

    tvhlog(LOG_DEBUG, "capmt", "%s: CA_SET_DESCR_AES adapter %d par %d idx %d "
                      "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                      capmt_name(capmt), adapter, parity, index,
                      cw[0], cw[1], cw[2], cw[3], cw[4], cw[5], cw[6], cw[7],
                      cw[8], cw[9], cw[10], cw[11], cw[12], cw[13], cw[14], cw[15]);
    if (index == -1)   // skipping removal request
      return;
    if (adapter >= MAX_CA || index >= MAX_INDEX)
      return;
    if (parity == 0) {
      capmt_process_key(capmt, adapter, index, DESCRAMBLER_AES, cw, empty, 1);
    } else if (parity == 1) {
      capmt_process_key(capmt, adapter, index, DESCRAMBLER_AES, empty, cw, 1);
    } else
      tvhlog(LOG_ERR, "capmt", "%s: Invalid parity %d in CA_SET_DESCR_AES for adapter%d", capmt_name(capmt), parity, adapter);

  } else if (cmd == DMX_SET_FILTER) {

    capmt_set_filter(capmt, adapter, sb, offset);

  } else if (cmd == DMX_STOP) {

    capmt_stop_filter(capmt, adapter, sb, offset);

  } else if (cmd == DVBAPI_ECM_INFO) {

    uint16_t sid     = sbuf_peek_u16(sb, offset + 4);
    uint16_t caid    = sbuf_peek_u16(sb, offset + 6);
    uint16_t pid     = sbuf_peek_u16(sb, offset + 8);
    uint32_t provid  = sbuf_peek_u32(sb, offset + 10);
    uint32_t ecmtime = sbuf_peek_u32(sb, offset + 14);
    int offset2      = offset + 18;
    char *cardsystem = capmt_peek_str(sb, &offset2);
    char *reader     = capmt_peek_str(sb, &offset2);
    char *from       = capmt_peek_str(sb, &offset2);
    char *protocol   = capmt_peek_str(sb, &offset2);
    uint8_t hops     = sbuf_peek_u8(sb, offset2);

    capmt_process_notify(capmt, adapter, sid, caid, provid,
                         cardsystem, pid, ecmtime, hops, reader,
                         from, protocol);
                         
    tvhlog(LOG_DEBUG, "capmt", "%s: ECM_INFO: adapter=%d sid=%d caid=%04X(%s) pid=%04X provid=%06X ecmtime=%d hops=%d reader=%s from=%s protocol=%s",
                      capmt_name(capmt), adapter, sid, caid, cardsystem, pid, provid, ecmtime, hops, reader, from, protocol);

    free(protocol);
    free(from);
    free(reader);
    free(cardsystem);

  } else if (cmd == DVBAPI_SERVER_INFO) {

    uint16_t protover = sbuf_peek_u16(sb, offset + 4);
    int offset2       = offset + 6;
    char *info        = capmt_peek_str(sb, &offset2);

    tvhlog(LOG_INFO, "capmt", "%s: Connected to server '%s' (protocol version %d)", capmt_name(capmt), info, protover);

    free(info);

  } else {
    tvhlog(LOG_ERR, "capmt", "%s: unknown command %08X", capmt_name(capmt), cmd);
  }
}

static void
show_connection(capmt_t *capmt, const char *what)
{
  if (capmt->capmt_oscam == CAPMT_OSCAM_TCP || capmt->capmt_oscam == CAPMT_OSCAM_NET_PROTO) {
    tvhlog(LOG_INFO, "capmt",
           "%s: mode %i connected to %s:%i (%s)",
           capmt_name(capmt),
           capmt->capmt_oscam,
           capmt->capmt_sockfile, capmt->capmt_port,
           what);
  } else if (capmt->capmt_oscam == CAPMT_OSCAM_UNIX_SOCKET ||
             capmt->capmt_oscam == CAPMT_OSCAM_UNIX_SOCKET_NP) {
    tvhlog(LOG_INFO, "capmt",
           "%s: mode %i sockfile %s got connection from client (%s)",
           capmt_name(capmt),
           capmt->capmt_oscam,
           capmt->capmt_sockfile,
           what);
  } else {
    tvhlog(LOG_INFO, "capmt",
           "%s: mode %i sockfile %s port %i got connection from client (%s)",
           capmt_name(capmt),
           capmt->capmt_oscam,
           capmt->capmt_sockfile, capmt->capmt_port,
           what);
  }
}

#if CONFIG_LINUXDVB
static void 
handle_ca0(capmt_t *capmt) {
  int i, ret, recvsock, adapter, nfds, cmd_size;
  uint8_t buf[256];
  sbuf_t buffer[MAX_CA];
  sbuf_t *pbuf;
  tvhpoll_event_t ev[MAX_CA + 1];

  show_connection(capmt, "ca0");

  for (i = 0; i < MAX_CA; i++)
    sbuf_init(&buffer[i]);

  capmt_notify_server(capmt, NULL, 1);

  capmt->capmt_poll = tvhpoll_create(MAX_CA + 1);
  capmt_poll_add(capmt, capmt->capmt_pipe.rd, 0);
  for (i = 0; i < MAX_CA; i++)
    if (capmt->capmt_adapters[i].ca_sock)
      capmt_poll_add(capmt, capmt->capmt_adapters[i].ca_sock, i + 1);

  i = 0;
  adapter = -1;

  while (capmt->capmt_running) {

    nfds = tvhpoll_wait(capmt->capmt_poll, ev, MAX_CA + 1, 500);

    if (nfds <= 0)
      continue;

    for (i = 0; i < nfds; i++) {

      if (ev[i].data.u32 == 0) {
        ret = read(capmt->capmt_pipe.rd, buf, 1);
        if (ret == 1 && buf[0] == 'c') {
          capmt_flush_queue(capmt, 0);
          continue;
        }

        tvhtrace("capmt", "%s: thread received shutdown", capmt_name(capmt));
        capmt->capmt_running = 0;
        continue;
      }

      adapter = ev[i].data.u32 - 1;

      if (adapter < 0 || adapter >= MAX_CA)
        continue;

      recvsock = capmt->capmt_adapters[adapter].ca_sock;

      if (recvsock <= 0)
        continue;

      ret = recv(recvsock, buf, sizeof(buf), MSG_DONTWAIT);

      if (ret == 0) {
        tvhlog(LOG_INFO, "capmt", "%s: normal socket shutdown", capmt_name(capmt));

        close(recvsock);
        capmt_poll_rem(capmt, recvsock);
        capmt->capmt_adapters[adapter].ca_sock = -1;
        continue;
      }
      
      if (ret < 0)
        continue;

      tvhtrace("capmt", "%s: Received message from socket %i", capmt_name(capmt), recvsock);
      tvhlog_hexdump("capmt", buf, ret);

      pbuf = &buffer[adapter];
      sbuf_append(pbuf, buf, ret);

      while (pbuf->sb_ptr > 0) {
        cmd_size = 0;
        while (pbuf->sb_ptr) {
          cmd_size = capmt_msg_size(capmt, pbuf, 0);
          if (cmd_size < 0)
            sbuf_cut(pbuf, 1);
        }
        if (cmd_size <= pbuf->sb_ptr) {
          capmt_analyze_cmd(capmt, adapter, pbuf, 0);
          sbuf_cut(pbuf, cmd_size);
        } else {
          break;
        }
      }

    }
  }

  for (i = 0; i < MAX_CA; i++)
    sbuf_free(&buffer[i]);
  tvhpoll_destroy(capmt->capmt_poll);
  capmt->capmt_poll = NULL;
}
#endif

static void
handle_single(capmt_t *capmt)
{
  int ret, recvsock, adapter, nfds, cmd_size, reconnect, offset;
  uint8_t buf[256];
  sbuf_t buffer;
  tvhpoll_event_t ev;

  show_connection(capmt, "single");

  reconnect = capmt->capmt_sock_reconnect[0];
  sbuf_init(&buffer);

  capmt_notify_server(capmt, NULL, 1);

  capmt->capmt_poll = tvhpoll_create(2);
  capmt_poll_add(capmt, capmt->capmt_pipe.rd, 0);
  capmt_poll_add(capmt, capmt->capmt_sock[0], 1);

  while (capmt->capmt_running) {

    nfds = tvhpoll_wait(capmt->capmt_poll, &ev, 1, 500);

    if (nfds <= 0)
      continue;

    if (ev.data.u32 == 0) {
      ret = read(capmt->capmt_pipe.rd, buf, 1);
      if (ret == 1 && buf[0] == 'c') {
        capmt_flush_queue(capmt, 0);
        continue;
      }
      
      tvhtrace("capmt", "%s: thread received shutdown", capmt_name(capmt));
      capmt->capmt_running = 0;
      continue;
    }
    
    if (reconnect != capmt->capmt_sock_reconnect[0]) {
      buffer.sb_bswap = 0;
      sbuf_reset(&buffer, 1024);
      capmt_flush_queue(capmt, 1);
      reconnect = capmt->capmt_sock_reconnect[0];
    }

    recvsock = capmt->capmt_sock[0];
    ret = recv(recvsock, buf, sizeof(buf), MSG_DONTWAIT);

    if (ret == 0) {
      tvhlog(LOG_INFO, "capmt", "%s: normal socket shutdown", capmt_name(capmt));
      capmt_poll_rem(capmt, recvsock);
      break;
    }
    
    if (ret < 0)
      continue;

    tvhtrace("capmt", "%s: Received message from socket %i", capmt_name(capmt), recvsock);
    tvhlog_hexdump("capmt", buf, ret);

    sbuf_append(&buffer, buf, ret);

    while (buffer.sb_ptr > 0) {
      cmd_size = 0;
      adapter = -1;
      offset = 0;
      while (buffer.sb_ptr > 0) {
        if (capmt_oscam_netproto(capmt)) {
          buffer.sb_bswap = 1;
        } else {
          adapter = buffer.sb_data[0];
          offset = 1;
        }
        if (adapter < MAX_CA) {
          cmd_size = capmt_msg_size(capmt, &buffer, offset);
          if (cmd_size >= 0)
            break;
        }
        sbuf_cut(&buffer, 1);
      }
      if (cmd_size > 0 && cmd_size + offset <= buffer.sb_ptr) {
        capmt_analyze_cmd(capmt, adapter, &buffer, offset);
        sbuf_cut(&buffer, cmd_size + offset);
      } else {
        break;
      }
    }
  }

  sbuf_free(&buffer);
  tvhpoll_destroy(capmt->capmt_poll);
  capmt->capmt_poll = NULL;
}

#if CONFIG_LINUXDVB
static void 
handle_ca0_wrapper(capmt_t *capmt)
{
  uint8_t buffer[18];
  int ret;

  show_connection(capmt, ".so wrapper");

  capmt_notify_server(capmt, NULL, 1);

  while (capmt->capmt_running) {

    if (capmt->capmt_sock[0] < 0)
      break;

    /* receiving data from UDP socket */
    ret = recv(capmt->capmt_adapters[0].ca_sock, buffer, 18, MSG_WAITALL);

    if (ret < 0) {
      tvhlog(LOG_ERR, "capmt", "%s: error receiving over socket", capmt_name(capmt));
      break;
    } else if (ret == 0) {
      /* normal socket shutdown */
      tvhlog(LOG_INFO, "capmt", "%s: normal socket shutdown", capmt_name(capmt));
      break;
    } else {

      tvhtrace("capmt", "%s: Received message from socket %i", capmt_name(capmt), capmt->capmt_adapters[0].ca_sock);
      tvhlog_hexdump("capmt", buffer, ret);

      capmt_process_key(capmt, 0,
                        buffer[0] | ((uint16_t)buffer[1] << 8),
                        DESCRAMBLER_DES,
                        buffer + 2, buffer + 10,
                        ret == 18);
    }
  }

  capmt_abort(capmt, DS_UNKNOWN);
  tvhlog(LOG_INFO, "capmt", "%s: connection from client closed ...", capmt_name(capmt));
}
#endif

#if ENABLE_LINUXDVB
static int
capmt_create_udp_socket(capmt_t *capmt, int *socket, int port)
{
  *socket = tvh_socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (*socket < 0) {
    tvherror("capmt", "%s: failed to create UDP socket", capmt_name(capmt));
    return 0;
  }

  struct sockaddr_in serv_addr;
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  serv_addr.sin_port = htons( (unsigned short int)port);
  serv_addr.sin_family = AF_INET;

  if (bind(*socket, (const struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0)
  {
    tvherror("capmt", "%s: failed to bind to ca0 (port %d)", capmt_name(capmt), port);
    return 0;
  }
  else
    return 1;
}
#endif

/**
 *
 */
static void *
capmt_thread(void *aux) 
{
  capmt_t *capmt = aux;
  capmt_adapter_t *ca;
  capmt_opaque_t *t;
  int d, i, j, fatal;
  int64_t mono;

  tvhlog(LOG_INFO, "capmt", "%s active", capmt_name(capmt));

  while (capmt->capmt_running) {
    fatal = 0;
    for (i = 0; i < MAX_CA; i++) {
      ca = &capmt->capmt_adapters[i];
      ca->ca_sock = -1;
      memset(&ca->ca_info, 0, sizeof(ca->ca_info));
      for (j = 0; j < MAX_PIDS; j++) {
        t = &ca->ca_pids[j];
        t->pid = PID_UNUSED;
        t->pid_refs = 0;
      }
    }
    for (i = 0; i < MAX_SOCKETS; i++) {
      capmt->sids[i] = 0;
      capmt->adps[i] = -1;
      capmt->capmt_sock[i] = -1;
      capmt->capmt_sock_reconnect[i] = 0;
    }
    memset(&capmt->capmt_demuxes, 0, sizeof(capmt->capmt_demuxes));

    /* Accessible */
    if (capmt->capmt_sockfile && capmt->capmt_oscam != CAPMT_OSCAM_TCP &&
        capmt->capmt_oscam != CAPMT_OSCAM_NET_PROTO &&
        !access(capmt->capmt_sockfile, R_OK | W_OK))
      caclient_set_status((caclient_t *)capmt, CACLIENT_STATUS_NONE);
    else
      caclient_set_status((caclient_t *)capmt, CACLIENT_STATUS_READY);
    
    pthread_mutex_lock(&capmt->capmt_mutex);

    while(capmt->capmt_running && capmt->cac_enabled == 0)
      tvh_cond_wait(&capmt->capmt_cond, &capmt->capmt_mutex);

    pthread_mutex_unlock(&capmt->capmt_mutex);

    if (!capmt->capmt_running) continue;

    /* open connection to camd.socket */
    capmt_connect(capmt, 0);

    if (capmt->capmt_sock[0] >= 0) {
      caclient_set_status((caclient_t *)capmt, CACLIENT_STATUS_CONNECTED);
#if CONFIG_LINUXDVB
      if (capmt_oscam_new(capmt)) {
        handle_single(capmt);
      } else {
        int bind_ok = 0;
        /* open connection to emulated ca0 device */
        if (capmt->capmt_oscam == CAPMT_OSCAM_SO_WRAPPER) {
          bind_ok = capmt_create_udp_socket(capmt,
                                            &capmt->capmt_adapters[0].ca_sock,
                                            capmt->capmt_port);
          if (bind_ok)
            handle_ca0_wrapper(capmt);
        } else {
          int i, n;
          extern const idclass_t linuxdvb_adapter_class;
          linuxdvb_adapter_t *la;
          idnode_set_t *is = idnode_find_all(&linuxdvb_adapter_class, NULL);
          for (i = 0; i < is->is_count; i++) {
            la = (linuxdvb_adapter_t*)is->is_array[i];
            if (!la || !la->la_is_enabled(la)) continue;
            n = la->la_dvb_number;
            if (n < 0 || n > MAX_CA) {
              tvhlog(LOG_ERR, "capmt", "%s: adapter number > MAX_CA", capmt_name(capmt));
              continue;
            }
            tvhlog(LOG_INFO, "capmt", "%s: created UDP socket %d", capmt_name(capmt), n);
            bind_ok = capmt_create_udp_socket(capmt,
                                              &capmt->capmt_adapters[n].ca_sock,
                                              capmt->capmt_port + n);
          }
          if (bind_ok)
            handle_ca0(capmt);
        }
        if (!bind_ok)
          fatal = 1;
      }
#else
     if (capmt->capmt_oscam == CAPMT_OSCAM_TCP ||
         capmt->capmt_oscam == CAPMT_OSCAM_NET_PROTO ||
         capmt->capmt_oscam == CAPMT_OSCAM_UNIX_SOCKET ||
         capmt->capmt_oscam == CAPMT_OSCAM_UNIX_SOCKET_NP) {
       handle_single(capmt);
     } else {
       tvhlog(LOG_ERR, "capmt", "%s: Only modes 3 and 4 are supported for non-linuxdvb devices", capmt_name(capmt));
       fatal = 1;
     }
#endif
    }

    pthread_mutex_lock(&capmt->capmt_mutex);

    caclient_set_status((caclient_t *)capmt, CACLIENT_STATUS_DISCONNECTED);

    /* close opened sockets */
    for (i = 0; i < MAX_SOCKETS; i++)
      capmt_socket_close(capmt, i);

    /* close all tuners */
    for (i = 0; i < MAX_CA; i++) {
      if (capmt->capmt_adapters[i].ca_sock >= 0)
        close(capmt->capmt_adapters[i].ca_sock);
    }

    if (capmt->capmt_reconfigure) {
      capmt->capmt_reconfigure = 0;
      capmt->capmt_running = 1;
      pthread_mutex_unlock(&capmt->capmt_mutex);
      continue;
    }

    if (!capmt->capmt_running) {
      pthread_mutex_unlock(&capmt->capmt_mutex);
      continue;
    }

    /* schedule reconnection */
    if(subscriptions_active() && !fatal) {
      d = 3;
    } else {
      d = 60;
    }

    tvhlog(LOG_INFO, "capmt", "%s: Automatic reconnection attempt in in %d seconds", idnode_get_title(&capmt->cac_id, NULL), d);

    mono = mclk() + sec2mono(d);
    do {
      i = tvh_cond_timedwait(&capmt->capmt_cond, &capmt->capmt_mutex, mono);
      if (i == ETIMEDOUT)
        break;
    } while (ERRNO_AGAIN(i));

    pthread_mutex_unlock(&capmt->capmt_mutex);
  }

  tvhlog(LOG_INFO, "capmt", "%s inactive", capmt_name(capmt));
  return NULL;
}

/**
 *
 */
static void
capmt_table_input(void *opaque, int pid, const uint8_t *data, int len, int emm)
{
  capmt_opaque_t *o = opaque;
  capmt_t *capmt = o->capmt;
  int i, demux_index, filter_index;
  capmt_dmx_t *df;
  capmt_filters_t *cf;
  dmx_filter_t *f;
  int flags = emm ? 0 : CAPMT_MSG_FAST;

  /* Validate */
  if (data == NULL || len > 4096) return;

  pthread_mutex_lock(&capmt->capmt_mutex);

  for (demux_index = 0; demux_index < capmt->capmt_demuxes.max; demux_index++) {
    cf = &capmt->capmt_demuxes.filters[demux_index];
    if (cf->adapter != o->adapter)
      continue;
    for (filter_index = 0; filter_index < cf->max; filter_index++) {
      df = &cf->dmx[filter_index];
      if (df->pid == pid && (df->flags & CAPMT_MSG_FAST) == flags) {
        f = &cf->dmx[filter_index].filter;
        if (f->mode[0] != 0)
          continue;
        if ((data[0] & f->mask[0]) != f->filter[0])
          continue;
        /* note that the data offset changes here (+2) !!! */
        for (i = 1; i < DMX_FILTER_SIZE && i + 2 < len; i++) {
          if (f->mode[i] != 0)
            break;
          if ((data[i + 2] & f->mask[i]) != f->filter[i])
            break;
        }
        if (i >= DMX_FILTER_SIZE && i + 2 <= len) {
          tvhtrace("capmt", "filter match pid %d len %d emm %d", pid, len, emm);
          capmt_filter_data(capmt,
                            o->adapter, demux_index,
                            filter_index, data, len,
                            cf->dmx[filter_index].flags);
        }
      }
    }
  }

  pthread_mutex_unlock(&capmt->capmt_mutex);
}

static void
capmt_caid_add(capmt_service_t *ct, mpegts_service_t *t, int pid, caid_t *c)
{
  capmt_caid_ecm_t *cce;

  tvhlog(LOG_DEBUG, "capmt",
         "%s: New caid 0x%04X:0x%06X (pid 0x%04X) for service \"%s\"",
          capmt_name(ct->ct_capmt), c->caid, c->providerid, pid, t->s_dvb_svcname);

  cce = calloc(1, sizeof(capmt_caid_ecm_t));
  cce->cce_caid = c->caid;
  cce->cce_ecmpid = pid;
  cce->cce_providerid = c->providerid;
  cce->cce_service = t;
  LIST_INSERT_HEAD(&ct->ct_caid_ecm, cce, cce_link);
}

static void
capmt_caid_change(th_descrambler_t *td)
{
  capmt_service_t *ct = (capmt_service_t *)td;
  capmt_t *capmt = ct->ct_capmt;
  mpegts_service_t *t;
  elementary_stream_t *st;
  capmt_caid_ecm_t *cce;
  caid_t *c;
  int change = 0;

  pthread_mutex_lock(&capmt->capmt_mutex);

  t = (mpegts_service_t*)td->td_service;

  lock_assert(&t->s_stream_mutex);

  TAILQ_FOREACH(st, &t->s_filt_components, es_filt_link) {
    if (t->s_dvb_prefcapid_lock == PREFCAPID_FORCE &&
        t->s_dvb_prefcapid != st->es_pid)
      continue;
    LIST_FOREACH(c, &st->es_caids, link) {
      /* search ecmpid in list */
      LIST_FOREACH(cce, &ct->ct_caid_ecm, cce_link)
        if (c->use && cce->cce_caid == c->caid && cce->cce_providerid == c->providerid)
          if (!t->s_dvb_forcecaid || t->s_dvb_forcecaid == cce->cce_caid)
            break;
      if (cce)
        continue;
      capmt_caid_add(ct, t, st->es_pid, c);
      change = 1;
    }
  }

  pthread_mutex_unlock(&capmt->capmt_mutex);

  if (change)
    capmt_notify_server(capmt, ct, 0);
}

static void
capmt_send_request(capmt_service_t *ct, int lm)
{
  capmt_t *capmt = ct->ct_capmt;
  mpegts_service_t *t = (mpegts_service_t *)ct->td_service;
  uint16_t sid = t->s_dvb_service_id;
  uint16_t pmtpid = t->s_pmt_pid;
  uint16_t transponder = t->s_dvb_mux->mm_tsid;
  uint16_t onid = t->s_dvb_mux->mm_onid;
  static uint8_t pmtversion = 1;
  int adapter_num = ct->ct_adapter;

  /* buffer for capmt */
  int pos = 0;
  uint8_t buf[4094];

  capmt_header_t head = {
    .capmt_indicator        = { 0x9F, 0x80, 0x32, 0x82, 0x00, 0x00 },
    .capmt_list_management  = lm,
    .program_number         = sid,
    .version_number         = 0, 
    .current_next_indicator = 0,
    .program_info_length    = 0,
    .capmt_cmd_id           = CAPMT_CMD_OK_DESCRAMBLING,
  };
  memcpy(&buf[pos], &head, sizeof(head));
  pos += sizeof(head);

  if (capmt->capmt_oscam != CAPMT_OSCAM_SO_WRAPPER) {
    capmt_descriptor_t dmd = { 
      .cad_type = CAPMT_DESC_DEMUX, 
      .cad_length = 0x02,
      .cad_data = { 
        0, adapter_num }};
    memcpy(&buf[pos], &dmd, dmd.cad_length + 2);
    pos += dmd.cad_length + 2;
  }

  capmt_descriptor_t prd = { 
    .cad_type = CAPMT_DESC_PRIVATE, 
    .cad_length = 0x08,
    .cad_data = { 0x00, 0x00, 0x00, 0x00, // enigma namespace goes here              
      transponder >> 8, transponder & 0xFF,
      onid >> 8, onid & 0xFF }};
  memcpy(&buf[pos], &prd, prd.cad_length + 2);
  pos += prd.cad_length + 2;

  if (capmt->capmt_oscam == CAPMT_OSCAM_SO_WRAPPER) {
    capmt_descriptor_t dmd = { 
      .cad_type = CAPMT_DESC_DEMUX, 
      .cad_length = 0x02,
      .cad_data = { 
        1 << adapter_num, adapter_num }};
    memcpy(&buf[pos], &dmd, dmd.cad_length + 2);
    pos += dmd.cad_length + 2;
  }

  capmt_descriptor_t ecd = { 
    .cad_type = CAPMT_DESC_PID, 
    .cad_length = 0x02,
    .cad_data = { 
      pmtpid >> 8, pmtpid & 0xFF }};
  memcpy(&buf[pos], &ecd, ecd.cad_length + 2);
  pos += ecd.cad_length + 2;

  capmt_caid_ecm_t *cce2;
  LIST_FOREACH(cce2, &ct->ct_caid_ecm, cce_link) {
    capmt_descriptor_t cad = { 
      .cad_type = 0x09, 
      .cad_length = 0x04,
      .cad_data = { 
        cce2->cce_caid   >> 8,        cce2->cce_caid   & 0xFF, 
        cce2->cce_ecmpid >> 8 | 0xE0, cce2->cce_ecmpid & 0xFF}};
    if (cce2->cce_providerid) { //we need to add provider ID to the data
      if (cce2->cce_caid >> 8 == 0x01) {
        cad.cad_length = 0x11;
        cad.cad_data[4] = cce2->cce_providerid >> 8;
        cad.cad_data[5] = cce2->cce_providerid & 0xff;
      } else if (cce2->cce_caid >> 8 == 0x05) {
        cad.cad_length = 0x0f;
        cad.cad_data[10] = 0x14;
        cad.cad_data[11] = cce2->cce_providerid >> 24;
        cad.cad_data[12] = cce2->cce_providerid >> 16;
        cad.cad_data[13] = cce2->cce_providerid >> 8;
        cad.cad_data[14] = cce2->cce_providerid & 0xff;
      } else if (cce2->cce_caid >> 8 == 0x18) {
        cad.cad_length = 0x07;
        cad.cad_data[5] = cce2->cce_providerid >> 8;
        cad.cad_data[6] = cce2->cce_providerid & 0xff;
      } else if (cce2->cce_caid >> 8 == 0x4a && cce2->cce_caid != 0x4ad2) {
          cad.cad_length = 0x05;
          cad.cad_data[4] = cce2->cce_providerid & 0xff;
      } else if (cce2->cce_caid == 0x4ad2) {
          cad.cad_length = 0x04;
          cad.cad_data[3] = cce2->cce_providerid & 0xffffff;
      }else
        tvhlog(LOG_WARNING, "capmt", "%s: Unknown CAID type, don't know where to put provider ID", capmt_name(capmt));
    }
    memcpy(&buf[pos], &cad, cad.cad_length + 2);
    pos += cad.cad_length + 2;
    tvhlog(LOG_DEBUG, "capmt", "%s: adding ECMPID=0x%X (%d), "
                      "CAID=0x%X (%d) PROVID=0x%X (%d), SID=%d, ADAPTER=%d",
      capmt_name(capmt),
      cce2->cce_ecmpid, cce2->cce_ecmpid,
      cce2->cce_caid, cce2->cce_caid,
      cce2->cce_providerid, cce2->cce_providerid,
      sid, adapter_num);
  }

  uint8_t end[] = { 
    0x01, (ct->ct_pids[0] >> 8) & 0xFF, ct->ct_pids[0] & 0xFF, 0x00, 0x06 };
  memcpy(&buf[pos], end, sizeof(end));
  pos += sizeof(end);
  buf[10] = ((pos - 5 - 12) & 0xF00) >> 8;
  buf[11] = ((pos - 5 - 12) & 0xFF);
  buf[4]  = ((pos - 6) >> 8);
  buf[5]  = ((pos - 6) & 0xFF);

  buf[7]  = sid >> 8;
  buf[8]  = sid & 0xFF;


  if(ct->td_keystate != DS_RESOLVED)
    tvhlog(LOG_DEBUG, "capmt",
      "%s: Trying to obtain key for service \"%s\"", capmt_name(capmt), t->s_dvb_svcname);

  buf[9] = pmtversion;
  pmtversion = (pmtversion + 1) & 0x1F;

  capmt_queue_msg(capmt, adapter_num, sid, buf, pos, 0);
}

static void
capmt_enumerate_services(capmt_t *capmt, int force)
{
  int lm = CAPMT_LIST_FIRST;	//list management
  int all_srv_count = 0;	//all services
  int res_srv_count = 0;	//services with resolved state
  int i = 0;
  capmt_service_t *ct;

  lock_assert(&capmt->capmt_mutex);

  LIST_FOREACH(ct, &capmt->capmt_services, ct_link) {
    all_srv_count++;
    if (ct->td_keystate == DS_RESOLVED)
      res_srv_count++;
  }

  if (!all_srv_count) {
    // closing socket (oscam handle this as event and stop decrypting)
    tvhlog(LOG_DEBUG, "capmt", "%s: %s: no subscribed services, closing socket, fd=%d", capmt_name(capmt), __FUNCTION__, capmt->capmt_sock[0]);
    if (capmt->capmt_sock[0] >= 0)
      caclient_set_status((caclient_t *)capmt, CACLIENT_STATUS_READY);
    if (capmt_oscam_netproto(capmt)) {
      capmt_send_stop_descrambling(capmt);
      capmt_pid_flush(capmt);
    }
    else
      capmt_socket_close(capmt, 0);
  }
  else if (force || (res_srv_count != all_srv_count)) {
    LIST_FOREACH(ct, &capmt->capmt_services, ct_link) {
      if (all_srv_count == i + 1)
        lm |= CAPMT_LIST_LAST;
      capmt_send_request(ct, lm);
      lm = CAPMT_LIST_MORE;
      i++;
    }
  }
}

/**
 * Check if our CAID's matches, and if so, link
 *
 * global_lock is held
 */
static void
capmt_service_start(caclient_t *cac, service_t *s)
{
  capmt_t *capmt = (capmt_t *)cac;
  capmt_service_t *ct;
  th_descrambler_t *td;
  mpegts_service_t *t = (mpegts_service_t*)s;
  elementary_stream_t *st;
  int tuner = -1, i, change = 0;
  char buf[512];
  caid_t *c, sca;
  
  lock_assert(&global_lock);

  /* Validate */
  if (!idnode_is_instance(&s->s_id, &mpegts_service_class))
    return;
  if (!t->s_dvb_active_input) return;

#if ENABLE_LINUXDVB
  extern const idclass_t linuxdvb_frontend_class;
  linuxdvb_frontend_t *lfe;
  lfe = (linuxdvb_frontend_t*)t->s_dvb_active_input;
  if (idnode_is_instance(&lfe->ti_id, &linuxdvb_frontend_class))
    tuner = lfe->lfe_adapter->la_dvb_number;
#endif

  pthread_mutex_lock(&t->s_stream_mutex);
  pthread_mutex_lock(&capmt->capmt_mutex);

  LIST_FOREACH(ct, &capmt->capmt_services, ct_link)
    /* skip, if we already have this service */
    if (ct->td_service == (service_t *)t) {
      /* update PIDs only */
      i = 0;
      TAILQ_FOREACH(st, &t->s_filt_components, es_filt_link)
        if (i < MAX_PIDS && SCT_ISAV(st->es_type))
          ct->ct_pids[i++] = st->es_pid;
      for ( ; i < MAX_PIDS; i++)
        ct->ct_pids[i] = 0;
      goto fin;
    }

  if (tuner < 0 && capmt->capmt_oscam != CAPMT_OSCAM_TCP &&
                   capmt->capmt_oscam != CAPMT_OSCAM_NET_PROTO &&
                   capmt->capmt_oscam != CAPMT_OSCAM_UNIX_SOCKET &&
                   capmt->capmt_oscam != CAPMT_OSCAM_UNIX_SOCKET_NP) {
    tvhlog(LOG_WARNING, "capmt",
           "%s: Virtual adapters are supported only in modes 3, 4, 5 and 6 (service \"%s\")",
           capmt_name(capmt), t->s_dvb_svcname);
    goto fin;
  }

  if (tuner < 0) {
    for (i = 0; i < MAX_CA; i++) {
      if (capmt->capmt_adapters[i].ca_tuner == NULL && tuner < 0)
        tuner = i;
      if (capmt->capmt_adapters[i].ca_tuner == t->s_dvb_active_input) {
        tuner = i;
        break;
      }
    }
    if (tuner < 0 || tuner >= MAX_CA) {
      tvhlog(LOG_ERR, "capmt",
             "%s: No free adapter slot available for service \"%s\"",
             capmt_name(capmt), t->s_dvb_svcname);
      pthread_mutex_unlock(&capmt->capmt_mutex);
      return;
    }
  }

  tvhlog(LOG_INFO, "capmt",
         "%s: Starting CAPMT server for service \"%s\" on adapter %d",
         capmt_name(capmt), t->s_dvb_svcname, tuner);

  capmt->capmt_adapters[tuner].ca_tuner = t->s_dvb_active_input;

  /* create new capmt service */
  ct              = calloc(1, sizeof(capmt_service_t));
  ct->ct_capmt    = capmt;
  ct->ct_adapter  = tuner;

  i = 0;
  TAILQ_FOREACH(st, &t->s_filt_components, es_filt_link) {
    if (i < MAX_PIDS && SCT_ISAV(st->es_type))
      ct->ct_pids[i++] = st->es_pid;
    if (t->s_dvb_prefcapid_lock == PREFCAPID_FORCE &&
        t->s_dvb_prefcapid != st->es_pid)
      continue;
    LIST_FOREACH(c, &st->es_caids, link) {
      if(c == NULL || c->use == 0)
        continue;
      if (t->s_dvb_forcecaid && t->s_dvb_forcecaid != c->caid)
        continue;
      capmt_caid_add(ct, t, st->es_pid, c);
      change = 1;
    }
  }

  if (!change && t->s_dvb_forcecaid) {
    memset(&sca, 0, sizeof(sca));
    sca.caid = t->s_dvb_forcecaid;
    capmt_caid_add(ct, t, 8191, &sca);
    change = 1;
  }

  td = (th_descrambler_t *)ct;
  if (capmt->capmt_oscam == CAPMT_OSCAM_TCP ||
      capmt->capmt_oscam == CAPMT_OSCAM_NET_PROTO) {
    snprintf(buf, sizeof(buf), "capmt-%s-%i",
                               capmt->capmt_sockfile,
                               capmt->capmt_port);
  } else {
    snprintf(buf, sizeof(buf), "capmt-%s", capmt->capmt_sockfile);
  }
  td->td_nicename    = strdup(buf);
  td->td_service     = s;
  td->td_stop        = capmt_service_destroy;
  td->td_caid_change = capmt_caid_change;
  td->td_ecm_reset   = capmt_ecm_reset;

  LIST_INSERT_HEAD(&t->s_descramblers, td, td_service_link);
  LIST_INSERT_HEAD(&capmt->capmt_services, ct, ct_link);

  /* wake-up idle thread */
  tvh_cond_signal(&capmt->capmt_cond, 0);

fin:
  pthread_mutex_unlock(&capmt->capmt_mutex);
  pthread_mutex_unlock(&t->s_stream_mutex);

  if (change)
    capmt_notify_server(capmt, NULL, 0);
}


/**
 *
 */
static void
capmt_free(caclient_t *cac)
{
  capmt_t *capmt = (capmt_t *)cac;
  tvhlog(LOG_INFO, "capmt", "%s: mode %i %s %s port %i destroyed",
         capmt_name(capmt),
         capmt->capmt_oscam,
         capmt->capmt_oscam == CAPMT_OSCAM_TCP ||
           capmt->capmt_oscam == CAPMT_OSCAM_NET_PROTO ? "IP address" : "sockfile",
         capmt->capmt_sockfile, capmt->capmt_port);
  capmt_flush_queue(capmt, 1);
  free(capmt->capmt_sockfile);
}

/**
 *
 */
static void
capmt_conf_changed(caclient_t *cac)
{
  capmt_t *capmt = (capmt_t *)cac;
  pthread_t tid;

  if (capmt->cac_enabled) {
    if (capmt->capmt_sockfile == NULL || capmt->capmt_sockfile[0] == '\0') {
      caclient_set_status(cac, CACLIENT_STATUS_NONE);
      return;
    }
    if (!capmt->capmt_running) {
      capmt->capmt_running = 1;
      capmt->capmt_reconfigure = 0;
      tvhthread_create(&capmt->capmt_tid, NULL, capmt_thread, capmt, "capmt");
      return;
    }
    pthread_mutex_lock(&capmt->capmt_mutex);
    capmt->capmt_reconfigure = 1;
    tvh_cond_signal(&capmt->capmt_cond, 0);
    pthread_mutex_unlock(&capmt->capmt_mutex);
    tvh_write(capmt->capmt_pipe.wr, "", 1);
  } else {
    if (!capmt->capmt_running)
      return;
    pthread_mutex_lock(&capmt->capmt_mutex);
    capmt->capmt_running = 0;
    capmt->capmt_reconfigure = 0;
    tvh_cond_signal(&capmt->capmt_cond, 0);
    tid = capmt->capmt_tid;
    pthread_mutex_unlock(&capmt->capmt_mutex);
    tvh_write(capmt->capmt_pipe.wr, "", 1);
    pthread_join(tid, NULL);
    caclient_set_status(cac, CACLIENT_STATUS_NONE);
  }

}

static htsmsg_t *
caclient_capmt_class_oscam_mode_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("OSCam new pc-nodmx (rev >= 10389)"), CAPMT_OSCAM_UNIX_SOCKET_NP },
    { N_("OSCam net protocol (rev >= 10389)"), CAPMT_OSCAM_NET_PROTO },
    { N_("OSCam pc-nodmx (rev >= 9756)"),      CAPMT_OSCAM_UNIX_SOCKET },
    { N_("OSCam TCP (rev >= 9574)"),           CAPMT_OSCAM_TCP },
    { N_("OSCam (rev >= 9095)"),               CAPMT_OSCAM_MULTILIST },
    { N_("Older OSCam"),                       CAPMT_OSCAM_OLD },
    { N_("Wrapper (capmt_ca.so)"),             CAPMT_OSCAM_SO_WRAPPER },
  };
  return strtab2htsmsg(tab, 1, lang);
}

const idclass_t caclient_capmt_class =
{
  .ic_super      = &caclient_class,
  .ic_class      = "caclient_capmt",
  .ic_caption    = N_("CAPMT (Linux DVBAPI)"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_INT,
      .id       = "mode",
      .name     = N_("Mode"),
      .off      = offsetof(capmt_t, capmt_oscam),
      .list     = caclient_capmt_class_oscam_mode_list,
      .def.i    = CAPMT_OSCAM_MULTILIST,
    },
    {
      .type     = PT_STR,
      .id       = "camdfilename",
      .name     = N_("Camd.socket filename / IP Address (TCP mode)"),
      .off      = offsetof(capmt_t, capmt_sockfile),
      .def.s    = "/tmp/camd.socket",
    },
    {
      .type     = PT_INT,
      .id       = "port",
      .name     = N_("Listen / Connect port"),
      .off      = offsetof(capmt_t, capmt_port),
      .def.i    = 9000
    },
    { }
  }
};

/*
 *
 */
caclient_t *capmt_create(void)
{
  capmt_t *capmt = calloc(1, sizeof(*capmt));

  pthread_mutex_init(&capmt->capmt_mutex, NULL);
  tvh_cond_init(&capmt->capmt_cond);
  TAILQ_INIT(&capmt->capmt_writeq);
  tvh_pipe(O_NONBLOCK, &capmt->capmt_pipe);

  capmt->cac_free         = capmt_free;
  capmt->cac_start        = capmt_service_start;
  capmt->cac_conf_changed = capmt_conf_changed;

  return (caclient_t *)capmt;
}
