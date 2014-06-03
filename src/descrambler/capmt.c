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

#if ENABLE_CAPMT

#include "input.h"
#include "service.h"
#include "tcp.h"
#include "capmt.h"

#include "notify.h"
#include "subscriptions.h"
#include "dtable.h"
#include "tvhcsa.h"
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

typedef struct ca_slot_info {
  int num;               /* slot number */

  int type;              /* CA interface this slot supports */
#define CA_CI            1     /* CI high level interface */
#define CA_CI_LINK       2     /* CI link layer level interface */
#define CA_CI_PHYS       4     /* CI physical layer level interface */
#define CA_DESCR         8     /* built-in descrambler */
#define CA_SC          128     /* simple smart card interface */

  unsigned int flags;
#define CA_CI_MODULE_PRESENT 1 /* module (or card) inserted */
#define CA_CI_MODULE_READY   2
} ca_slot_info_t;


/* descrambler types and info */

typedef struct ca_descr_info {
  unsigned int num;          /* number of available descramblers (keys) */
  unsigned int type;         /* type of supported scrambling system */
#define CA_ECD           1
#define CA_NDS           2
#define CA_DSS           4
} ca_descr_info_t;

typedef struct ca_caps {
  unsigned int slot_num;     /* total number of CA card and module slots */
  unsigned int slot_type;    /* OR of all supported types */
  unsigned int descr_num;    /* total number of descrambler slots (keys) */
  unsigned int descr_type;   /* OR of all supported types */
} ca_caps_t;

/* a message to/from a CI-CAM */
typedef struct ca_msg {
  unsigned int index;
  unsigned int type;
  unsigned int length;
  unsigned char msg[256];
} ca_msg_t;

typedef struct ca_descr {
  unsigned int index;
  unsigned int parity;	/* 0 == even, 1 == odd */
  unsigned char cw[8];
} ca_descr_t;

typedef struct ca_pid {
  unsigned int pid;
  int index;		/* -1 == disable*/
} ca_pid_t;

#define CA_RESET          _IO('o', 128)
#define CA_GET_CAP        _IOR('o', 129, ca_caps_t)
#define CA_GET_SLOT_INFO  _IOR('o', 130, ca_slot_info_t)
#define CA_GET_DESCR_INFO _IOR('o', 131, ca_descr_info_t)
#define CA_GET_MSG        _IOR('o', 132, ca_msg_t)
#define CA_SEND_MSG       _IOW('o', 133, ca_msg_t)
#define CA_SET_DESCR      _IOW('o', 134, ca_descr_t)
#define CA_SET_PID        _IOW('o', 135, ca_pid_t)

#define DMX_FILTER_SIZE 16

typedef struct dmx_filter
{
  uint8_t filter[DMX_FILTER_SIZE];
  uint8_t mask[DMX_FILTER_SIZE];
  uint8_t mode[DMX_FILTER_SIZE];
} dmx_filter_t;

typedef struct dmx_sct_filter_params
{
  uint16_t       pid;
  dmx_filter_t   filter;
  uint32_t       timeout;
  uint32_t       flags;
#define DMX_CHECK_CRC       1
#define DMX_ONESHOT         2
#define DMX_IMMEDIATE_START 4
#define DMX_KERNEL_CLIENT   0x8000
} dmx_filter_params_t;

#define DMX_STOP          _IO('o', 42)
#define DMX_SET_FILTER    _IOW('o', 43, struct dmx_sct_filter_params)

// ca_pmt_list_management values:
#define CAPMT_LIST_MORE   0x00    // append a 'MORE' CAPMT object the list and start receiving the next object
#define CAPMT_LIST_FIRST  0x01    // clear the list when a 'FIRST' CAPMT object is received, and start receiving the next object
#define CAPMT_LIST_LAST   0x02    // append a 'LAST' CAPMT object to the list and start working with the list
#define CAPMT_LIST_ONLY   0x03    // clear the list when an 'ONLY' CAPMT object is received, and start working with the object
#define CAPMT_LIST_ADD    0x04    // append an 'ADD' CAPMT object to the current list and start working with the updated list
#define CAPMT_LIST_UPDATE 0x05    // replace an entry in the list with an 'UPDATE' CAPMT object, and start working with the updated list

// ca_pmt_cmd_id values:
#define CAPMT_CMD_OK_DESCRAMBLING   0x01  // start descrambling the service in this CAPMT object as soon as the list of CAPMT objects is complete
#define CAPMT_CMD_OK_MMI            0x02  //
#define CAPMT_CMD_QUERY             0x03  //
#define CAPMT_CMD_NOT_SELECTED      0x04

// ca_pmt_descriptor types
#define CAPMT_DESC_PRIVATE 0x81
#define CAPMT_DESC_DEMUX   0x82
#define CAPMT_DESC_PID     0x84

// limits
#define MAX_CA      16
#define MAX_INDEX   64
#define MAX_FILTER  64
#define MAX_SOCKETS 16   // max sockets (simultaneous channels) per demux

/**
 *
 */
TAILQ_HEAD(capmt_queue, capmt);
LIST_HEAD(capmt_service_list, capmt_service);
LIST_HEAD(capmt_caid_ecm_list, capmt_caid_ecm);
static struct capmt_queue capmts;
static pthread_cond_t capmt_config_changed;

/*
 * ECM descriptor
 */
typedef struct ca_info {
  uint16_t pid;
  uint8_t  even[8];
  uint8_t  odd[8];
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

  tvhcsa_t ct_csa;

  /* current sequence number */
  uint16_t ct_seq;
} capmt_service_t;

/**
 **
 */
typedef struct capmt_filters {
  int max;
  dmx_filter_params_t dmx[MAX_FILTER];
} capmt_filters_t;

typedef struct capmt_demuxes {
  int max;
  capmt_filters_t filters[MAX_INDEX];
} capmt_demuxes_t;

/**
 *
 */
typedef struct capmt {
  pthread_cond_t capmt_cond;

  TAILQ_ENTRY(capmt) capmt_link; /* Linkage protected via global_lock */

  struct capmt_service_list capmt_services;

  pthread_t capmt_tid;

  /* from capmt configuration */
  char *capmt_sockfile;
  char *capmt_hostname;
  int   capmt_port;
  char *capmt_comment;
  char *capmt_id;
  enum {
    CAPMT_OSCAM_SO_WRAPPER,
    CAPMT_OSCAM_OLD,
    CAPMT_OSCAM_MULTILIST,
    CAPMT_OSCAM_TCP,
    CAPMT_OSCAM_UNIX_SOCKET
  } capmt_oscam;

  /* capmt sockets */
  int   sids[MAX_SOCKETS];
  int   capmt_sock[MAX_SOCKETS];
  int   capmt_sock_ca0[MAX_CA];

  /* thread flags */
  int   capmt_connected;
  int   capmt_enabled;
  int   capmt_running;
  int   capmt_reconfigure;

  /* next sequence number */
  uint16_t capmt_seq;

  /* runtime status */
  mpegts_input_t *capmt_tuners[MAX_CA];
  capmt_demuxes_t capmt_demuxes;
  ca_info_t capmt_ca_info[MAX_CA][MAX_INDEX];
} capmt_t;

static void capmt_enumerate_services(capmt_t *capmt, int force);
static void capmt_send_request(capmt_service_t *ct, int lm);

/**
 *
 */
static htsmsg_t *
capmt_record_build(capmt_t *capmt)
{
  htsmsg_t *e = htsmsg_create_map();

  htsmsg_add_str(e, "id", capmt->capmt_id);
  htsmsg_add_u32(e, "enabled",  !!capmt->capmt_enabled);
  htsmsg_add_u32(e, "connected", capmt->capmt_connected);

  htsmsg_add_str(e, "camdfilename", capmt->capmt_sockfile ?: "");
  htsmsg_add_u32(e, "port", capmt->capmt_port);
  htsmsg_add_u32(e, "oscam", capmt->capmt_oscam);
  htsmsg_add_str(e, "comment", capmt->capmt_comment ?: "");
  
  return e;
}

/**
 *
 */
static inline int
capmt_oscam_new(capmt_t *capmt)
{
  int oscam = capmt->capmt_oscam;
  return oscam != CAPMT_OSCAM_SO_WRAPPER &&
         oscam != CAPMT_OSCAM_OLD;
}

/**
 *
 */
static void
capmt_set_connected(capmt_t *capmt, int c)
{
  if (c == capmt->capmt_connected)
    return;
  capmt->capmt_connected = c;
  notify_by_msg("capmt", capmt_record_build(capmt));
}

/*
 *
 */
static int
capmt_connect(capmt_t *capmt)
{
  int fd;

  if (capmt->capmt_oscam == CAPMT_OSCAM_TCP) {

    char errbuf[256];

    fd = tcp_connect(capmt->capmt_sockfile, capmt->capmt_port, NULL,
                     errbuf, sizeof(errbuf), 2);
    if (fd < 0) {
      tvhlog(LOG_ERR, "capmt",
             "Cannot connect to %s:%i (%s); Do you have OSCam running?",
             capmt->capmt_sockfile, capmt->capmt_port, errbuf);
      fd = 0;
    }

  } else {

    struct sockaddr_un serv_addr_un;

    memset(&serv_addr_un, 0, sizeof(serv_addr_un));
    serv_addr_un.sun_family = AF_LOCAL;
    snprintf(serv_addr_un.sun_path,
             sizeof(serv_addr_un.sun_path),
             "%s", capmt->capmt_sockfile);

    fd = tvh_socket(AF_LOCAL, SOCK_STREAM, 0);
    if (connect(fd, (const struct sockaddr*)&serv_addr_un,
                sizeof(serv_addr_un)) != 0) {
      tvhlog(LOG_ERR, "capmt",
             "Cannot connect to %s (%s); Do you have OSCam running?",
             capmt->capmt_sockfile, strerror(errno));
      close(fd);
      fd = 0;
    }

  }

  if (fd)
    tvhlog(LOG_DEBUG, "capmt", "created socket with socket_fd=%d", fd);

  return fd;
}

/**
 *
 */
static int
capmt_send_msg(capmt_t *capmt, int sid, const uint8_t *buf, size_t len)
{
  if (capmt->capmt_oscam != CAPMT_OSCAM_SO_WRAPPER) {
    int i = 0;
    int found = 0;
    if (capmt->capmt_oscam == CAPMT_OSCAM_OLD) {
      // dumping current SID table
      for (i = 0; i < MAX_SOCKETS; i++)
        tvhlog(LOG_DEBUG, "capmt", "%s: SOCKETS TABLE DUMP [%d]: sid=%d socket=%d", __FUNCTION__, i, capmt->sids[i], capmt->capmt_sock[i]);
      if (sid == 0) {
        tvhlog(LOG_DEBUG, "capmt", "%s: got empty SID - returning from function", __FUNCTION__);
        return -1;
      }

      // searching for the SID and socket
      for (i = 0; i < MAX_SOCKETS; i++) {
        if (capmt->sids[i] == sid) {
          found = 1;
          break;
        }
      }

      if (found)
        tvhlog(LOG_DEBUG, "capmt", "%s: found sid, reusing socket, i=%d", __FUNCTION__, i);
      else {                       //not found - adding to first free in table
        for (i = 0; i < MAX_SOCKETS; i++) {
          if (capmt->sids[i] == 0) {
            capmt->sids[i] = sid;
            break;
          }
        }
      }
      if (i == MAX_SOCKETS) {
        tvhlog(LOG_DEBUG, "capmt", "%s: no free space for new SID!!!", __FUNCTION__);
        return -1;
      } else {
        capmt->sids[i] = sid;
        tvhlog(LOG_DEBUG, "capmt", "%s: added: i=%d", __FUNCTION__, i);
      }
    }

    // check if the socket is still alive by writing 0 bytes
    if (capmt->capmt_sock[i] > 0) {
      if (write(capmt->capmt_sock[i], NULL, 0) < 0)
        capmt->capmt_sock[i] = 0;
      else if ((capmt->capmt_oscam == CAPMT_OSCAM_SO_WRAPPER ||
                capmt->capmt_oscam == CAPMT_OSCAM_OLD) && found)
        return 0;
    }

    // opening socket and sending
    if (capmt->capmt_sock[i] == 0) {
      capmt->capmt_sock[i] = capmt_connect(capmt);
      capmt_set_connected(capmt, capmt->capmt_sock[i] ? 2 : 0);
    }

    if (capmt->capmt_sock[i] > 0) {
      if (tvh_write(capmt->capmt_sock[i], buf, len)) {
        tvhlog(LOG_DEBUG, "capmt", "socket_fd=%d send failed", capmt->capmt_sock[i]);
        close(capmt->capmt_sock[i]);
        capmt->capmt_sock[i] = 0;
        return -1;
      }
    }
  }
  else  // standard old capmt mode
    tvh_write(capmt->capmt_sock[0], buf, len);
  return 0;
}

static void 
capmt_send_stop(capmt_service_t *t)
{
  mpegts_service_t *s = (mpegts_service_t *)t->td_service;

  if (t->ct_capmt->capmt_oscam != CAPMT_OSCAM_SO_WRAPPER) {
    int i;
    // searching for socket to close
    for (i = 0; i < MAX_SOCKETS; i++)
      if (t->ct_capmt->sids[i] == s->s_dvb_service_id)
        break;

    if (i == MAX_SOCKETS) {
      tvhlog(LOG_DEBUG, "capmt", "%s: socket to close not found", __FUNCTION__);
      return;
    }

    // closing socket (oscam handle this as event and stop decrypting)
    tvhlog(LOG_DEBUG, "capmt", "%s: closing socket i=%d, socket_fd=%d", __FUNCTION__, i, t->ct_capmt->capmt_sock[i]);
    t->ct_capmt->sids[i] = 0;
    if (t->ct_capmt->capmt_sock[i] > 0)
      close(t->ct_capmt->capmt_sock[i]);
    t->ct_capmt->capmt_sock[i] = 0;
  } else {  // standard old capmt mode
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
      0x01, (t->ct_seq >> 8) & 0xFF, t->ct_seq & 0xFF, 0x00, 0x06 };
    memcpy(&buf[pos], end, sizeof(end));
    pos    += sizeof(end);
    buf[4]  = ((pos - 6) >> 8);
    buf[5]  = ((pos - 6) & 0xFF);
    buf[7]  = s->s_dvb_service_id >> 8;
    buf[8]  = s->s_dvb_service_id & 0xFF;
    buf[9]  = 1;
    buf[10] = ((pos - 5 - 12) & 0xF00) >> 8;
    buf[11] = ((pos - 5 - 12) & 0xFF);
  
    capmt_send_msg(t->ct_capmt, s->s_dvb_service_id, buf, pos);
  }
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

  tvhlog(LOG_INFO, "capmt",
         "Removing CAPMT Server from service \"%s\" on adapter %d",
         s->s_dvb_svcname, ct->ct_adapter);

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
    capmt_enumerate_services(ct->ct_capmt, 1);

  if (LIST_EMPTY(&ct->ct_capmt->capmt_services)) {
    ct->ct_capmt->capmt_tuners[ct->ct_adapter] = NULL;
    memset(&ct->ct_capmt->capmt_demuxes, 0, sizeof(ct->ct_capmt->capmt_demuxes));
  }

  tvhcsa_destroy(&ct->ct_csa);
  free(ct);
}

static void
capmt_filter_data(capmt_t *capmt, int sid, uint8_t adapter, uint8_t demux_index,
                  uint8_t filter_index, const uint8_t *data, int len)
{
  uint8_t buf[4096 + 8];

  buf[0] = buf[1] = buf[3] = 0xff;
  buf[4] = demux_index;
  buf[5] = filter_index;
  memcpy(buf + 6, data, len);
  if (len - 3 == ((((uint16_t)buf[7] << 8) | buf[8]) & 0xfff))
    capmt_send_msg(capmt, sid, buf, len + 6);
}

static void
capmt_set_filter(capmt_t *capmt, uint8_t adapter, uint8_t *buf)
{
  uint8_t demux_index = buf[4];
  uint8_t filter_index = buf[5];
  dmx_filter_params_t *filter, *params = (dmx_filter_params_t *)(buf + 6);
  capmt_filters_t *cf;

  if (adapter >= MAX_CA ||
      demux_index >= MAX_INDEX ||
      filter_index >= MAX_FILTER)
    return;
  cf = &capmt->capmt_demuxes.filters[demux_index];
  filter = &cf->dmx[filter_index];
  *filter = *params;
  if (capmt->capmt_demuxes.max <= demux_index)
    capmt->capmt_demuxes.max = demux_index + 1;
  if (cf->max <= filter_index)
    cf->max = filter_index + 1;
  /* Position correction */
  memmove(filter->filter.mask + 3, filter->filter.mask + 1, DMX_FILTER_SIZE - 3);
  memmove(filter->filter.filter + 3, filter->filter.filter + 1, DMX_FILTER_SIZE - 3);
  memmove(filter->filter.mode + 3, filter->filter.mode + 1, DMX_FILTER_SIZE - 3);
  filter->filter.mask[1] = filter->filter.mask[2] = 0;
}

static void
capmt_stop_filter(capmt_t *capmt, uint8_t adapter, uint8_t *buf)
{
  uint8_t demux_index = buf[4];
  uint8_t filter_index = buf[5];
  int16_t pid = ((int16_t)buf[6] << 8) | buf[7];
  dmx_filter_params_t *filter;
  capmt_filters_t *cf;

  if (adapter >= MAX_CA ||
      demux_index >= MAX_INDEX ||
      filter_index >= MAX_FILTER)
    return;
  cf = &capmt->capmt_demuxes.filters[demux_index];
  filter = &cf->dmx[filter_index];
  if (filter->pid != pid)
    return;
  memset(filter, 0, sizeof(*filter));
  /* short the max values */
  filter_index = cf->max - 1;
  while (filter_index != 255 && cf->dmx[filter_index].pid == 0)
    filter_index--;
  cf->max = filter_index == 255 ? 0 : filter_index + 1;
  demux_index = capmt->capmt_demuxes.max - 1;
  while (demux_index != 255 && capmt->capmt_demuxes.filters[demux_index].max == 0)
    demux_index--;
  capmt->capmt_demuxes.max = demux_index == 255 ? 0 : demux_index + 1;
}

static void
capmt_notify_server(capmt_t *capmt, capmt_service_t *ct)
{
  if (capmt_oscam_new(capmt)) {
    if (!LIST_EMPTY(&capmt->capmt_services))
      capmt_enumerate_services(capmt, 0);
  } else {
    if (ct)
      capmt_send_request(ct, CAPMT_LIST_ONLY);
    else
      LIST_FOREACH(ct, &capmt->capmt_services, ct_link)
        capmt_send_request(ct, CAPMT_LIST_ONLY);
  }
}

static void 
handle_ca0(capmt_t* capmt) {
  capmt_service_t *ct;
  mpegts_service_t *t;
  int ret, bufsize;
  unsigned int *request = NULL;
  ca_descr_t *ca;
  ca_pid_t *cpd;
  int process_key, process_next, cai = 0;
  int i, j;
  int recvsock = 0;

  if (capmt->capmt_oscam != CAPMT_OSCAM_SO_WRAPPER)
    bufsize = sizeof(int) + sizeof(ca_descr_t);
  else
    bufsize = 18;

  uint8_t buffer[bufsize], *even, *odd;
  uint16_t seq;

  tvhlog(LOG_INFO, "capmt", "got connection from client ...");

  pthread_mutex_lock(&global_lock);
  capmt_notify_server(capmt, NULL);
  pthread_mutex_unlock(&global_lock);

  i = 0;
  while (capmt->capmt_running) {
    process_key = 0;

    // receiving data from UDP socket
    if (capmt->capmt_oscam == CAPMT_OSCAM_SO_WRAPPER) {
      ret = recv(capmt->capmt_sock_ca0[0], buffer, bufsize, MSG_WAITALL);

      if (ret < 0)
        tvhlog(LOG_ERR, "capmt", "error receiving over socket");
      else if (ret == 0) {
        // normal socket shutdown
        tvhlog(LOG_INFO, "capmt", "normal socket shutdown");
        break;
      }
    } else {
      process_next = 1;
      if (capmt_oscam_new(capmt))
        recvsock = capmt->capmt_sock[0];
      else
        recvsock = capmt->capmt_sock_ca0[i];
      if (recvsock > 0) {
        if (capmt_oscam_new(capmt))
        {
          // adapter index is in first byte
          uint8_t adapter_index;
          ret = recv(recvsock, &adapter_index, 1, MSG_DONTWAIT);
          if (ret < 0)
          {
            usleep(10 * 1000);
            continue;
          }
          cai = adapter_index;
        }
        request = NULL;
        ret = recv(recvsock, buffer, capmt_oscam_new(capmt) ? sizeof(int) : bufsize, MSG_DONTWAIT);
        if (ret > 0) {
          request = (unsigned int *) &buffer;
          if (!capmt_oscam_new(capmt))
            process_next = 0;
          else {
            int ret = 0;
            if (*request == CA_SET_PID) {            //receive CA_SET_PID
              ret = recv(recvsock, buffer+sizeof(int), sizeof(ca_pid_t), MSG_DONTWAIT);
              if (ret != sizeof(ca_pid_t))
                *request = 0;
            } else if (*request == CA_SET_DESCR) {   //receive CA_SET_DESCR
              ret = recv(recvsock, buffer+sizeof(int), sizeof(ca_descr_t), MSG_DONTWAIT);
              if (ret != sizeof(ca_descr_t))
                *request = 0;
            } else if (*request == DMX_SET_FILTER) { //receive DMX_SET_FILTER
              ret = recv(recvsock, buffer+sizeof(int), 2 + sizeof(dmx_filter_params_t), MSG_DONTWAIT);
              if (ret != 2 + sizeof(dmx_filter_params_t))
                *request = 0;
            } else if (*request == DMX_STOP) {       //receive DMX_STOP
              ret = recv(recvsock, buffer+sizeof(int), 4, MSG_DONTWAIT);
              if (ret != 4)
                *request = 0;
            }
            if (ret > 0)
              process_next = 0;
          }
        }
        else if (ret == 0) {
          // normal socket shutdown
          tvhlog(LOG_INFO, "capmt", "normal socket shutdown");

          // we are not connected any more - set services as unavailable
          LIST_FOREACH(ct, &capmt->capmt_services, ct_link) {
            if (ct->td_keystate != DS_FORBIDDEN) {
              ct->td_keystate = DS_FORBIDDEN;
            }
          }

          int still_left = 0;
          if (!capmt_oscam_new(capmt)) {
           close(capmt->capmt_sock_ca0[i]);
           capmt->capmt_sock_ca0[i] = -1;

            for (j = 0; j < MAX_CA; j++) {
              if (capmt->capmt_sock_ca0[j] > 0) {
                still_left = 1;
                break;
              }
            }
          }
          if (!still_left) //all sockets closed
            break;
        }
      } 

      if (process_next) {
        if (!capmt_oscam_new(capmt)) {
          i++;
          if (i >= MAX_CA)
            i = 0;
        }
        usleep(10 * 1000);
        continue;
      }
    }

    // parsing data
    if (capmt->capmt_oscam) {
      if (!request)
        continue;
      if (capmt->capmt_oscam != 2) //in mode 2 we read it directly from socket
        cai = i;
      if (*request == CA_SET_PID) {
        cpd = (ca_pid_t *)&buffer[sizeof(int)];
        tvhlog(LOG_DEBUG, "capmt", "CA_SET_PID cai %d req %d (%d %04x)", cai, *request, cpd->index, cpd->pid);

        if (cai < MAX_CA && cpd->index >=0 && cpd->index < MAX_INDEX) {
          capmt->capmt_ca_info[cai][cpd->index].pid = cpd->pid;
        } else if (cpd->index == -1) {
          memset(&capmt->capmt_ca_info[cai], 0, sizeof(capmt->capmt_ca_info[cai]));
        } else
          tvhlog(LOG_ERR, "capmt", "Invalid index %d in CA_SET_PID (%d) for ca id %d", cpd->index, MAX_INDEX, cai);
      } else if (*request == CA_SET_DESCR) {
        ca = (ca_descr_t *)&buffer[sizeof(int)];
        tvhlog(LOG_DEBUG, "capmt", "CA_SET_DESCR cai %d req %d par %d idx %d %02x%02x%02x%02x%02x%02x%02x%02x", cai, *request, ca->parity, ca->index, ca->cw[0], ca->cw[1], ca->cw[2], ca->cw[3], ca->cw[4], ca->cw[5], ca->cw[6], ca->cw[7]);
        if (ca->index == -1)   // skipping removal request
          continue;
        if (cai >= MAX_CA || ca->index >= MAX_INDEX)
          continue;
        if(ca->parity==0) {
          memcpy(capmt->capmt_ca_info[cai][ca->index].even,ca->cw,8); // even key
          process_key = 1;
        } else if(ca->parity==1) {
          memcpy(capmt->capmt_ca_info[cai][ca->index].odd,ca->cw,8);  // odd key
          process_key = 1;
        } else
          tvhlog(LOG_ERR, "capmt", "Invalid parity %d in CA_SET_DESCR for ca id %d", ca->parity, cai);

        seq  = capmt->capmt_ca_info[cai][ca->index].pid;
        even = capmt->capmt_ca_info[cai][ca->index].even;
        odd  = capmt->capmt_ca_info[cai][ca->index].odd;
      } else if (*request == DMX_SET_FILTER) {
        capmt_set_filter(capmt, cai, buffer);
      } else if (*request == DMX_STOP) {
        capmt_stop_filter(capmt, cai, buffer);
      }
    } else {
      /* get control words */
      seq  = buffer[0] | ((uint16_t)buffer[1] << 8);
      even = &buffer[2];
      odd  = &buffer[10];
      process_key = 1;
    }

    // processing key
    if (process_key) {
      pthread_mutex_lock(&global_lock);
      LIST_FOREACH(ct, &capmt->capmt_services, ct_link) {
        t = (mpegts_service_t *)ct->td_service;

        if (capmt->capmt_oscam == CAPMT_OSCAM_SO_WRAPPER && ret < bufsize) {
          if(ct->td_keystate != DS_FORBIDDEN) {
            tvhlog(LOG_ERR, "capmt", "Can not descramble service \"%s\", access denied", t->s_dvb_svcname);

            ct->td_keystate = DS_FORBIDDEN;
          }

          continue;
        }

        if(seq != ct->ct_seq)
          continue;

        for (i = 0; i < 8; i++)
          if (even[i]) {
            tvhcsa_set_key_even(&ct->ct_csa, even);
            break;
          }
        for (i = 0; i < 8; i++)
          if (odd[i]) {
            tvhcsa_set_key_odd(&ct->ct_csa, odd);
            break;
          }

        if(ct->td_keystate != DS_RESOLVED)
          tvhlog(LOG_DEBUG, "capmt", "Obtained key for service \"%s\"",t->s_dvb_svcname);

        ct->td_keystate = DS_RESOLVED;
      }
      pthread_mutex_unlock(&global_lock);
    }
  }

  tvhlog(LOG_INFO, "capmt", "connection from client closed ...");
}

#if ENABLE_LINUXDVB
static int
capmt_create_udp_socket(int *socket, int port)
{
  *socket = tvh_socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

  struct sockaddr_in serv_addr;
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  serv_addr.sin_port = htons( (unsigned short int)port);
  serv_addr.sin_family = AF_INET;

  if (bind(*socket, (const struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0)
  {
    tvherror("capmt", "failed to bind to ca0 (port %d)", port);
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
  struct timespec ts;
  int d, i, fatal;

  while (capmt->capmt_running) {
    fatal = 0;
    for (i = 0; i < MAX_CA; i++)
      capmt->capmt_sock_ca0[i] = -1;
    for (i = 0; i < MAX_SOCKETS; i++) {
      capmt->sids[i] = 0;
      capmt->capmt_sock[i] = 0;
    }
    memset(&capmt->capmt_demuxes, 0, sizeof(capmt->capmt_demuxes));

    /* Accessible */
    if (!access(capmt->capmt_sockfile, R_OK | W_OK))
      capmt_set_connected(capmt, 1);
    else
      capmt_set_connected(capmt, 0);
    
    pthread_mutex_lock(&global_lock);

    while(capmt->capmt_running && capmt->capmt_enabled == 0)
      pthread_cond_wait(&capmt->capmt_cond, &global_lock);

    pthread_mutex_unlock(&global_lock);

    /* open connection to camd.socket */
    capmt->capmt_sock[0] = capmt_connect(capmt);

    if (capmt->capmt_sock[0]) {
      capmt_set_connected(capmt, 2);
#if CONFIG_LINUXDVB
      if (capmt_oscam_new(capmt)) {
        handle_ca0(capmt);
      } else {
        int bind_ok = 0;
        /* open connection to emulated ca0 device */
        if (capmt->capmt_oscam == CAPMT_OSCAM_SO_WRAPPER) {
          bind_ok = capmt_create_udp_socket(&capmt->capmt_sock_ca0[0], capmt->capmt_port);
        } else {
          int i, n;
          extern const idclass_t linuxdvb_adapter_class;
          linuxdvb_adapter_t *la;
          idnode_set_t *is = idnode_find_all(&linuxdvb_adapter_class);
          for (i = 0; i < is->is_count; i++) {
            la = (linuxdvb_adapter_t*)is->is_array[i];
            if (!la || !la->la_is_enabled(la)) continue;
            n = la->la_dvb_number;
            if (n < 0 || n > MAX_CA) {
              tvhlog(LOG_ERR, "capmt", "adapter number > MAX_CA");
              continue;
            }
            tvhlog(LOG_INFO, "capmt", "created UDP socket %d", n);
            bind_ok = capmt_create_udp_socket(&capmt->capmt_sock_ca0[n], capmt->capmt_port + n);
          }
        }
        if (bind_ok)
          handle_ca0(capmt);
        else
          fatal = 1;
      }
#else
     if (capmt->capmt_oscam == CAPMT_OSCAM_TCP ||
         capmt->capmt_oscam == CAPMT_OSCAM_UNIX_SOCKET) {
       handle_ca0(capmt);
     } else {
       tvhlog(LOG_ERR, "capmt", "Only modes 3 and 4 are supported for non-linuxdvb devices");
       fatal = 1;
     }
#endif
    }

    capmt_set_connected(capmt, 0);

    /* close opened sockets */
    for (i = 0; i < MAX_SOCKETS; i++)
      if (capmt->capmt_sock[i] > 0)
        close(capmt->capmt_sock[i]);
    for (i = 0; i < MAX_CA; i++)
      if (capmt->capmt_sock_ca0[i] > 0)
        close(capmt->capmt_sock_ca0[i]);

    /* schedule reconnection */
    if(subscriptions_active() && !fatal) {
      d = 3;
    } else {
      d = 60;
    }

    ts.tv_sec = time(NULL) + d;
    ts.tv_nsec = 0;

    tvhlog(LOG_INFO, "capmt", "Automatic reconnection attempt in in %d seconds", d);

    pthread_mutex_lock(&global_lock);
    pthread_cond_timedwait(&capmt_config_changed, &global_lock, &ts);
    pthread_mutex_unlock(&global_lock);
  }

  free(capmt->capmt_id);
  free(capmt);

  return NULL;
}

/**
 *
 */
static void
capmt_table_input(struct th_descrambler *td,
    struct elementary_stream *st, const uint8_t *data, int len)
{
  capmt_service_t *ct = (capmt_service_t *)td;
  capmt_t *capmt = ct->ct_capmt;
  mpegts_service_t *t = (mpegts_service_t*)td->td_service;
  capmt_caid_ecm_t *cce;
  int i, change, demux_index, filter_index;
  capmt_filters_t *cf;
  dmx_filter_t *f;
  caid_t *c;

  /* Validate */
  if (!idnode_is_instance(&td->td_service->s_id, &mpegts_service_class))
    return;
  if (!t->s_dvb_active_input) return;
  if (len > 4096) return;

  for (demux_index = 0; demux_index < capmt->capmt_demuxes.max; demux_index++) {
    cf = &capmt->capmt_demuxes.filters[demux_index];
    for (filter_index = 0; filter_index < cf->max; filter_index++)
      if (cf->dmx[filter_index].pid == st->es_pid) {
        f = &cf->dmx[filter_index].filter;
        for (i = 0; i < DMX_FILTER_SIZE && i < len; i++) {
          if (f->mode[i] != 0)
            break;
          if ((data[i] & f->mask[i]) != f->filter[i])
            break;
        }
        if (i >= DMX_FILTER_SIZE && i <= len)
          capmt_filter_data(capmt, t->s_dvb_service_id,
                            ct->ct_adapter, demux_index,
                            filter_index, data, len);
      }
  }

  /* Add new caids, no ecm management */
  if (data[0] != 0x80 && data[0] != 0x81) return; /* ignore EMM */
  change = 0;
  LIST_FOREACH(c, &st->es_caids, link) {
    /* search ecmpid in list */
    LIST_FOREACH(cce, &ct->ct_caid_ecm, cce_link)
      if (cce->cce_caid == c->caid)
        break;
    if (cce)
      continue;

    tvhlog(LOG_DEBUG, "capmt",
           "New caid 0x%04X for service \"%s\"", c->caid, t->s_dvb_svcname);

    /* ecmpid not already seen, add it to list */
    cce             = calloc(1, sizeof(capmt_caid_ecm_t));
    cce->cce_caid   = c->caid;
    cce->cce_ecmpid = st->es_pid;
    cce->cce_providerid = c->providerid;
    LIST_INSERT_HEAD(&ct->ct_caid_ecm, cce, cce_link);
    change = 1;
  }

  if (capmt->capmt_oscam == CAPMT_OSCAM_SO_WRAPPER && capmt->capmt_sock[0] == 0) {
    /* New key, but we are not connected (anymore), can not descramble */
    ct->td_keystate = DS_UNKNOWN;
    return;
  }

  if (change)
    capmt_notify_server(capmt, ct);
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
        cad.cad_data[5] = cce2->cce_providerid & 0xffffff;
      } else if (cce2->cce_caid >> 8 == 0x05) {
        cad.cad_length = 0x0f;
        cad.cad_data[10] = 0x14;
        cad.cad_data[11] = cce2->cce_providerid >> 24;
        cad.cad_data[12] = cce2->cce_providerid >> 16;
        cad.cad_data[13] = cce2->cce_providerid >> 8;
        cad.cad_data[14] = cce2->cce_providerid & 0xffffff;
      } else if (cce2->cce_caid >> 8 == 0x18) {
        cad.cad_length = 0x07;
        cad.cad_data[5] = cce2->cce_providerid >> 8;
        cad.cad_data[6] = cce2->cce_providerid & 0xffffff;
      } else if (cce2->cce_caid >> 8 == 0x4a) {
        cad.cad_length = 0x05;
        cad.cad_data[4] = cce2->cce_providerid & 0xffffff;
      } else
        tvhlog(LOG_WARNING, "capmt", "Unknown CAID type, don't know where to put provider ID");
    }
    memcpy(&buf[pos], &cad, cad.cad_length + 2);
    pos += cad.cad_length + 2;
    tvhlog(LOG_DEBUG, "capmt", "adding ECMPID=0x%X (%d), CAID=0x%X (%d) PROVID=0x%X (%d)",
      cce2->cce_ecmpid, cce2->cce_ecmpid,
      cce2->cce_caid, cce2->cce_caid,
      cce2->cce_providerid, cce2->cce_providerid);
  }

  uint8_t end[] = { 
    0x01, (ct->ct_seq >> 8) & 0xFF, ct->ct_seq & 0xFF, 0x00, 0x06 };
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
      "Trying to obtain key for service \"%s\"",t->s_dvb_svcname);

  buf[9] = pmtversion;
  pmtversion = (pmtversion + 1) & 0x1F;

  capmt_send_msg(capmt, sid, buf, pos);
}

static void
capmt_enumerate_services(capmt_t *capmt, int force)
{
  int lm = CAPMT_LIST_FIRST;	//list management
  int all_srv_count = 0;	//all services
  int res_srv_count = 0;	//services with resolved state
  int i = 0;

  capmt_service_t *ct;
  LIST_FOREACH(ct, &capmt->capmt_services, ct_link) {
    all_srv_count++;
    if (ct->td_keystate == DS_RESOLVED)
      res_srv_count++;
  }

  if (!all_srv_count && !res_srv_count) {
    // closing socket (oscam handle this as event and stop decrypting)
    tvhlog(LOG_DEBUG, "capmt", "%s: no subscribed services, closing socket, fd=%d", __FUNCTION__, capmt->capmt_sock[0]);
    if (capmt->capmt_sock[0] > 0) {
      close(capmt->capmt_sock[0]);
      capmt_set_connected(capmt, 1);
    }
    capmt->capmt_sock[0] = 0;
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
void
capmt_service_start(service_t *s)
{
  capmt_t *capmt;
  capmt_service_t *ct;
  capmt_caid_ecm_t *cce;
  th_descrambler_t *td;
  mpegts_service_t *t = (mpegts_service_t*)s;
  elementary_stream_t *st;
  int tuner = -1, i, change;
  
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

  TAILQ_FOREACH(capmt, &capmts, capmt_link) {
    LIST_FOREACH(ct, &capmt->capmt_services, ct_link) {
      /* skip, if we already have this service */
      if (ct->td_service == (service_t *)t)
        return;
    }
  }

  TAILQ_FOREACH(capmt, &capmts, capmt_link) {
    /* skip, if we're not active */
    if (!capmt->capmt_enabled)
      continue;

    if (tuner < 0 && !capmt_oscam_new(capmt)) {
      tvhlog(LOG_WARNING, "capmt",
             "Virtual adapters are supported only in mode 2 (service \"%s\")",
             t->s_dvb_svcname);
      continue;
    }

    if (tuner < 0) {
      for (i = 0; i < MAX_CA; i++) {
        if (capmt->capmt_tuners[i] == NULL && tuner < 0)
          tuner = i;
        if (capmt->capmt_tuners[i] == t->s_dvb_active_input) {
          tuner = i;
          break;
        }
      }
      if (tuner < 0 || tuner >= MAX_CA) {
        tvhlog(LOG_ERR, "capmt",
               "No free adapter slot available for service \"%s\"",
               t->s_dvb_svcname);
        continue;
      }
    }

    tvhlog(LOG_INFO, "capmt",
           "Starting CAPMT server for service \"%s\" on adapter %d",
           t->s_dvb_svcname, tuner);

    capmt->capmt_tuners[tuner] = t->s_dvb_active_input;

    /* create new capmt service */
    ct              = calloc(1, sizeof(capmt_service_t));
    ct->ct_capmt    = capmt;
    ct->ct_seq      = capmt->capmt_seq++;
    ct->ct_adapter  = tuner;

    change = 0;
    TAILQ_FOREACH(st, &t->s_components, es_link) {
      caid_t *c;
      LIST_FOREACH(c, &st->es_caids, link) {
        if(c == NULL)
          continue;

        tvhlog(LOG_DEBUG, "capmt",
          "New caid 0x%04X for service \"%s\"", c->caid, t->s_dvb_svcname);

        mpegts_table_register_caid(((mpegts_service_t *)s)->s_dvb_mux, c->caid);

        /* add it to list */
        cce             = calloc(1, sizeof(capmt_caid_ecm_t));
        cce->cce_caid   = c->caid;
        cce->cce_ecmpid = st->es_pid;
        cce->cce_providerid = c->providerid;
        LIST_INSERT_HEAD(&ct->ct_caid_ecm, cce, cce_link);
        change = 1;
      }
    }

    td = (th_descrambler_t *)ct;
    tvhcsa_init(td->td_csa = &ct->ct_csa);
    td->td_service    = s;
    td->td_stop       = capmt_service_destroy;
    td->td_table      = capmt_table_input;
    LIST_INSERT_HEAD(&t->s_descramblers, td, td_service_link);

    LIST_INSERT_HEAD(&capmt->capmt_services, ct, ct_link);

    /* wake-up idle thread */
    pthread_cond_signal(&capmt->capmt_cond);

    if (change)
      capmt_notify_server(capmt, NULL);
  }
}


/**
 *
 */
static void
capmt_destroy(capmt_t *capmt)
{
  lock_assert(&global_lock);
  TAILQ_REMOVE(&capmts, capmt, capmt_link);  
  capmt->capmt_running = 0;
  pthread_cond_signal(&capmt->capmt_cond);
}

/**
 *
 */
static capmt_t *
capmt_entry_find(const char *id, int create)
{
  char buf[20];
  capmt_t *capmt;
  static int tally;

  if(id != NULL) {
    TAILQ_FOREACH(capmt, &capmts, capmt_link)
      if(!strcmp(capmt->capmt_id, id))
  return capmt;
  }
  if(create == 0)
    return NULL;

  if(id == NULL) {
    tally++;
    snprintf(buf, sizeof(buf), "%d", tally);
    id = buf;
  } else {
    tally = MAX(atoi(id), tally);
  }

  capmt = calloc(1, sizeof(capmt_t));
  pthread_cond_init(&capmt->capmt_cond, NULL);
  capmt->capmt_id      = strdup(id); 
  capmt->capmt_running = 1; 
  capmt->capmt_seq     = 0;

  TAILQ_INSERT_TAIL(&capmts, capmt, capmt_link);  

  tvhthread_create(&capmt->capmt_tid, NULL, capmt_thread, capmt, 1);

  return capmt;
}


/**
 *
 */
static htsmsg_t *
capmt_entry_update(void *opaque, const char *id, htsmsg_t *values, int maycreate)
{
  capmt_t *capmt;
  const char *s;
  uint32_t u32;

  if((capmt = capmt_entry_find(id, maycreate)) == NULL)
    return NULL;

  lock_assert(&global_lock);
  
  if((s = htsmsg_get_str(values, "camdfilename")) != NULL) {
    free(capmt->capmt_sockfile);
    capmt->capmt_sockfile = strdup(s);
  }
  
  if(!htsmsg_get_u32(values, "port", &u32))
    capmt->capmt_port = u32;

  if(!htsmsg_get_u32(values, "oscam", &u32)) 
    capmt->capmt_oscam = u32;

  if((s = htsmsg_get_str(values, "comment")) != NULL) {
    free(capmt->capmt_comment);
    capmt->capmt_comment = strdup(s);
  }

  if(!htsmsg_get_u32(values, "enabled", &u32)) 
    capmt->capmt_enabled = u32;


  capmt->capmt_reconfigure = 1;

  pthread_cond_signal(&capmt->capmt_cond);

  pthread_cond_broadcast(&capmt_config_changed);

  return capmt_record_build(capmt);
}
  


/**
 *
 */
static int
capmt_entry_delete(void *opaque, const char *id)
{
  capmt_t *capmt;

  pthread_cond_broadcast(&capmt_config_changed);

  if((capmt = capmt_entry_find(id, 0)) == NULL)
    return -1;
  capmt_destroy(capmt);
  return 0;
}


/**
 *
 */
static htsmsg_t *
capmt_entry_get_all(void *opaque)
{
  htsmsg_t *r = htsmsg_create_list();
  capmt_t *capmt;

  TAILQ_FOREACH(capmt, &capmts, capmt_link)
    htsmsg_add_msg(r, NULL, capmt_record_build(capmt));

  return r;
}

/**
 *
 */
static htsmsg_t *
capmt_entry_get(void *opaque, const char *id)
{
  capmt_t *capmt;


  if((capmt = capmt_entry_find(id, 0)) == NULL)
    return NULL;
  return capmt_record_build(capmt);
}

/**
 *
 */
static htsmsg_t *
capmt_entry_create(void *opaque)
{
  pthread_cond_broadcast(&capmt_config_changed);
  return capmt_record_build(capmt_entry_find(NULL, 1));
}

/**
 *
 */
static const dtable_class_t capmt_dtc = {
  .dtc_record_get     = capmt_entry_get,
  .dtc_record_get_all = capmt_entry_get_all,
  .dtc_record_create  = capmt_entry_create,
  .dtc_record_update  = capmt_entry_update,
  .dtc_record_delete  = capmt_entry_delete,
  .dtc_read_access = ACCESS_ADMIN,
  .dtc_write_access = ACCESS_ADMIN,
  .dtc_mutex = &global_lock,
};

/**
 *
 */
void
capmt_init(void)
{
  dtable_t *dt;

  TAILQ_INIT(&capmts);

  pthread_cond_init(&capmt_config_changed, NULL);

  dt = dtable_create(&capmt_dtc, "capmt", NULL);
  dtable_load(dt);
}

void
capmt_done(void)
{
  capmt_t *capmt, *n;
  pthread_t tid;

  for (capmt = TAILQ_FIRST(&capmts); capmt != NULL; capmt = n) {
    n = TAILQ_NEXT(capmt, capmt_link);
    pthread_mutex_lock(&global_lock);
    tid = capmt->capmt_tid;
    capmt_destroy(capmt);
    pthread_mutex_unlock(&global_lock);
    pthread_join(tid, NULL);
  }
  dtable_delete("capmt");
}

#else /* ENABLE_CAPMT */

void capmt_init ( void ) {}
void capmt_done ( void ) {}
void capmt_service_start(service_t *s) {}

#endif
