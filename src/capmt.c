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
#include <linux/ioctl.h>
#include <linux/dvb/ca.h>
#include <fcntl.h>

#include "tvheadend.h"
#include "dvb/dvb.h"
#include "tcp.h"
#include "psi.h"
#include "tsdemux.h"
#include "capmt.h"
#include "notify.h"
#include "subscriptions.h"
#include "dtable.h"

#if ENABLE_DVBCSA
#include <dvbcsa/dvbcsa.h>
#else
#include "ffdecsa/FFdecsa.h"
#endif

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

#define CW_DUMP(buf, len, format, ...) \
  printf(format, __VA_ARGS__); int j; for (j = 0; j < len; ++j) printf("%02X ", buf[j]); printf("\n");

#pragma GCC diagnostic ignored "-Warray-bounds"
#define MAX_CA  4
#define MAX_INDEX 64
#define KEY_SIZE  8
#define INFO_SIZE (2+KEY_SIZE+KEY_SIZE)
#define EVEN_OFF  (2)
#define ODD_OFF   (2+KEY_SIZE)
#define MAX_SOCKETS 16   // max sockets (simultaneous channels) per demux
static unsigned char ca_info[MAX_CA][MAX_INDEX][INFO_SIZE];

/**
 *
 */
TAILQ_HEAD(capmt_queue, capmt);
LIST_HEAD(capmt_service_list, capmt_service);
LIST_HEAD(capmt_caid_ecm_list, capmt_caid_ecm);
static struct capmt_queue capmts;
static pthread_cond_t capmt_config_changed;

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
  /** last ecm size */
  uint32_t cce_ecmsize;
  /** last ecm buffer */
  uint8_t  cce_ecm[4096];
  
  LIST_ENTRY(capmt_caid_ecm) cce_link;
} capmt_caid_ecm_t;

/**
 *
 */
typedef struct capmt_service {
  th_descrambler_t ct_head;

  service_t *ct_service;

  struct capmt *ct_capmt;

  LIST_ENTRY(capmt_service) ct_link;

  /* list of used ca-systems with ids and last ecm */
  struct capmt_caid_ecm_list ct_caid_ecm;

  /**
   * Status of the key(s) in ct_keys
   */
  enum {
    CT_UNKNOWN,
    CT_RESOLVED,
    CT_FORBIDDEN
  } ct_keystate;

  /* buffers for keystructs */
#if ENABLE_DVBCSA
  struct dvbcsa_bs_key_s *ct_key_even;
  struct dvbcsa_bs_key_s *ct_key_odd;
#else
  void                   *ct_keys;
#endif

  /* CSA */
  int      ct_cluster_size;
  uint8_t *ct_tsbcluster;
  int      ct_fill;
#if ENABLE_DVBCSA
  struct dvbcsa_bs_batch_s *ct_tsbbatch_even;
  struct dvbcsa_bs_batch_s *ct_tsbbatch_odd;
  int      ct_fill_even;
  int      ct_fill_odd;
#endif

  /* current sequence number */
  uint16_t ct_seq;

  /* sending requests will be based on this caid */
  int      ct_caid_last;
} capmt_service_t;


/**
 *
 */
typedef struct capmt {
  pthread_cond_t capmt_cond;

  TAILQ_ENTRY(capmt) capmt_link; /* Linkage protected via global_lock */

  struct capmt_service_list capmt_services;

  /* from capmt configuration */
  char *capmt_sockfile;
  char *capmt_hostname;
  int   capmt_port;
  char *capmt_comment;
  char *capmt_id;
  int   capmt_oscam;

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
} capmt_t;

/**
 *
 */
static int
capmt_send_msg(capmt_t *capmt, int sid, const uint8_t *buf, size_t len)
{
  if (capmt->capmt_oscam) {
    int i, sent = 0;

    // dumping current SID table
    for (i = 0; i < MAX_SOCKETS; i++)
      tvhlog(LOG_DEBUG, "capmt", "%s: SOCKETS TABLE DUMP [%d]: sid=%d socket=%d", __FUNCTION__, i, capmt->sids[i], capmt->capmt_sock[i]);
    if (sid == 0) {
      tvhlog(LOG_DEBUG, "capmt", "%s: got empty SID - returning from function", __FUNCTION__);
      return -1;
    }

    // searching for the SID and socket
    int found = 0;
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

    // opening socket and sending
    if (capmt->capmt_sock[i] == 0) {
      capmt->capmt_sock[i] = tvh_socket(AF_LOCAL, SOCK_STREAM, 0);

      struct sockaddr_un serv_addr_un;
      memset(&serv_addr_un, 0, sizeof(serv_addr_un));
      serv_addr_un.sun_family = AF_LOCAL;
      snprintf(serv_addr_un.sun_path, sizeof(serv_addr_un.sun_path), "%s", capmt->capmt_sockfile);

      if (connect(capmt->capmt_sock[i], (const struct sockaddr*)&serv_addr_un, sizeof(serv_addr_un)) != 0) {
        tvhlog(LOG_ERR, "capmt", "Cannot connect to %s, Do you have OSCam running?", capmt->capmt_sockfile);
        capmt->capmt_sock[i] = 0;
      } else
        tvhlog(LOG_DEBUG, "capmt", "created socket with socket_fd=%d", capmt->capmt_sock[i]);
    }
    if (capmt->capmt_sock[i] > 0) {
      sent = write(capmt->capmt_sock[i], buf, len);
      tvhlog(LOG_DEBUG, "capmt", "socket_fd=%d len=%d sent=%d", capmt->capmt_sock[i], (int)len, sent);
      if (sent != len) {
        tvhlog(LOG_ERR, "capmt", "%s: len != sent", __FUNCTION__);
        close(capmt->capmt_sock[i]);
        capmt->capmt_sock[i] = 0;
      }
    }
    return sent;
  }
  else  // standard old capmt mode
    return write(capmt->capmt_sock[0], buf, len);
}

static void 
capmt_send_stop(capmt_service_t *t)
{
  if (t->ct_capmt->capmt_oscam) {
    int i;
    // searching for socket to close
    for (i = 0; i < MAX_SOCKETS; i++)
      if (t->ct_capmt->sids[i] == t->ct_service->s_dvb_service_id)
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
      .program_number         = t->ct_service->s_dvb_service_id,
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
    buf[7]  = t->ct_service->s_dvb_service_id >> 8;
    buf[8]  = t->ct_service->s_dvb_service_id & 0xFF;
    buf[9]  = 1;
    buf[10] = ((pos - 5 - 12) & 0xF00) >> 8;
    buf[11] = ((pos - 5 - 12) & 0xFF);
  
    capmt_send_msg(t->ct_capmt, t->ct_service->s_dvb_service_id, buf, pos);
  }
}

/**
 * global_lock is held
 * s_stream_mutex is held
 */
static void 
capmt_service_destroy(th_descrambler_t *td)
{
  tvhlog(LOG_INFO, "capmt", "Removing CAPMT Server from service");

  capmt_service_t *ct = (capmt_service_t *)td;

  /* send stop to client */
  capmt_send_stop(ct);

  capmt_caid_ecm_t *cce;
  while (!LIST_EMPTY(&ct->ct_caid_ecm)) 
  { 
    /* List Deletion. */
    cce = LIST_FIRST(&ct->ct_caid_ecm);
    LIST_REMOVE(cce, cce_link);
    free(cce);
  }

  LIST_REMOVE(td, td_service_link);

  LIST_REMOVE(ct, ct_link);

#if ENABLE_DVBCSA
  dvbcsa_bs_key_free(ct->ct_key_odd);
  dvbcsa_bs_key_free(ct->ct_key_even);
  free(ct->ct_tsbbatch_odd);
  free(ct->ct_tsbbatch_even);
#else
  free_key_struct(ct->ct_keys);
#endif
  free(ct->ct_tsbcluster);
  free(ct);
}

static void 
handle_ca0(capmt_t* capmt) {
  capmt_service_t *ct;
  service_t *t;
  int ret, bufsize;
  int *request;
  ca_descr_t *ca;
  ca_pid_t *cpd;
  int process_key, process_next, cai;
  int i, j;

  if (capmt->capmt_oscam)
    bufsize = sizeof(int) + sizeof(ca_descr_t);
  else
    bufsize = 18;

  uint8_t invalid[8], buffer[bufsize], *even, *odd;
  uint16_t seq;
  memset(invalid, 0, 8);

  tvhlog(LOG_INFO, "capmt", "got connection from client ...");

  i = 0;
  while (capmt->capmt_running) {
    process_key = 0;

    // receiving data from UDP socket
    if (!capmt->capmt_oscam) {
      ret = recv(capmt->capmt_sock_ca0[0], buffer, bufsize, MSG_WAITALL);

      if (ret < 0)
        tvhlog(LOG_ERR, "capmt", "error receiving over socket");
      else if (ret == 0) {
        // normal socket shutdown
        tvhlog(LOG_INFO, "capmt", "normal socket shutdown");
        break;
      }
    } else {
      process_next = 0;
      if (capmt->capmt_sock_ca0[i] > 0) {
        ret = recv(capmt->capmt_sock_ca0[i], buffer, bufsize, MSG_DONTWAIT);
        if (ret < 0)
          process_next = 1;
        else if (ret == 0) {
          // normal socket shutdown
          tvhlog(LOG_INFO, "capmt", "normal socket shutdown");
          close(capmt->capmt_sock_ca0[i]);
          capmt->capmt_sock_ca0[i] = -1;

          int still_left = 0;
          for (j = 0; j < MAX_CA; j++) {
            if (capmt->capmt_sock_ca0[j] > 0) {
              still_left = 1;
              break;
            }
          }
          if (still_left) //this socket is closed but there are others active
            process_next = 1;
          else            //all sockets closed
            break;
        } 
      } else
        process_next = 1;

      if (process_next) {
        i++;
        if (i >= MAX_CA)
          i = 0;
        usleep(10 * 1000);
        continue;
      }
    }

    // parsing data
    if (capmt->capmt_oscam) {
      cai = i;
      request = (int *) &buffer;
      if (*request == CA_SET_PID) {
        cpd = (ca_pid_t *)&buffer[sizeof(int)];
        tvhlog(LOG_DEBUG, "capmt", "CA_SET_PID cai %d req %d (%d %04x)", cai, *request, cpd->index, cpd->pid);

        if (cpd->index >=0 && cpd->index < MAX_INDEX) {
          ca_info[cai][cpd->index][0] = (cpd->pid >> 0) & 0xff;
          ca_info[cai][cpd->index][1] = (cpd->pid >> 8) & 0xff;
        } else if (cpd->index == -1) {
          memset(&ca_info[cai], 0, sizeof(ca_info[cai]));
        } else
          tvhlog(LOG_ERR, "capmt", "Invalid index %d in CA_SET_PID (%d) for ca id %d", cpd->index, MAX_INDEX, cai);
      } else if (*request == CA_SET_DESCR) {
        ca = (ca_descr_t *)&buffer[sizeof(int)];
        tvhlog(LOG_DEBUG, "capmt", "CA_SET_DESCR cai %d req %d par %d idx %d %02x%02x%02x%02x%02x%02x%02x%02x", cai, *request, ca->parity, ca->index, ca->cw[0], ca->cw[1], ca->cw[2], ca->cw[3], ca->cw[4], ca->cw[5], ca->cw[6], ca->cw[7]);
        if (ca->index == -1)   // skipping removal request
          continue;

        if(ca->parity==0) {
          memcpy(&ca_info[cai][ca->index][EVEN_OFF],ca->cw,KEY_SIZE); // even key
          process_key = 1;
        } else if(ca->parity==1) {
          memcpy(&ca_info[cai][ca->index][ODD_OFF],ca->cw,KEY_SIZE); // odd key
          process_key = 1;
        } else
          tvhlog(LOG_ERR, "capmt", "Invalid parity %d in CA_SET_DESCR for ca id %d", ca->parity, cai);

        seq  = ca_info[cai][ca->index][0] | ((uint16_t)ca_info[cai][ca->index][1] << 8);
        even = &ca_info[cai][ca->index][EVEN_OFF];
        odd  = &ca_info[cai][ca->index][ODD_OFF];
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
    LIST_FOREACH(ct, &capmt->capmt_services, ct_link) {
      t = ct->ct_service;

      if(ret < bufsize) {
        if(ct->ct_keystate != CT_FORBIDDEN) {
          tvhlog(LOG_ERR, "capmt", "Can not descramble service \"%s\", access denied", t->s_svcname);

          ct->ct_keystate = CT_FORBIDDEN;
        }

        continue;
      }

      if(seq != ct->ct_seq)
        continue;

      if (memcmp(even, invalid, 8))
#if ENABLE_DVBCSA
        dvbcsa_bs_key_set(even, ct->ct_key_even);
#else
        set_even_control_word(ct->ct_keys, even);
#endif
      if (memcmp(odd, invalid, 8))
#if ENABLE_DVBCSA
        dvbcsa_bs_key_set(odd, ct->ct_key_odd);
#else
        set_odd_control_word(ct->ct_keys, odd);
#endif

      if(ct->ct_keystate != CT_RESOLVED)
        tvhlog(LOG_INFO, "capmt", "Obtained key for service \"%s\"",t->s_svcname);

      ct->ct_keystate = CT_RESOLVED;
    }
   }
  }

  tvhlog(LOG_INFO, "capmt", "connection from client closed ...");
}

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
    perror("[CapmtServer] ERROR binding to ca0");
    return 0;
  }
  else
    return 1;
}

/**
 *
 */
static void *
capmt_thread(void *aux) 
{
  capmt_t *capmt = aux;
  struct timespec ts;
  int d, i, bind_ok = 0;
  th_dvb_adapter_t *tda;

  while (capmt->capmt_running) {
    for (i = 0; i < MAX_CA; i++)
      capmt->capmt_sock_ca0[i] = -1;
    for (i = 0; i < MAX_SOCKETS; i++) {
      capmt->sids[i] = 0;
      capmt->capmt_sock[i] = 0;
    }
    capmt->capmt_connected = 0;
    
    pthread_mutex_lock(&global_lock);

    while(capmt->capmt_running && capmt->capmt_enabled == 0)
      pthread_cond_wait(&capmt->capmt_cond, &global_lock);

    pthread_mutex_unlock(&global_lock);

    /* open connection to camd.socket */
    capmt->capmt_sock[0] = tvh_socket(AF_LOCAL, SOCK_STREAM, 0);

    struct sockaddr_un serv_addr_un;
    memset(&serv_addr_un, 0, sizeof(serv_addr_un));
    serv_addr_un.sun_family = AF_LOCAL;
    snprintf(serv_addr_un.sun_path, sizeof(serv_addr_un.sun_path), "%s", capmt->capmt_sockfile);

    if (connect(capmt->capmt_sock[0], (const struct sockaddr*)&serv_addr_un, sizeof(serv_addr_un)) == 0) {
      capmt->capmt_connected = 1;

      /* open connection to emulated ca0 device */
      if (!capmt->capmt_oscam) {
        bind_ok = capmt_create_udp_socket(&capmt->capmt_sock_ca0[0], capmt->capmt_port);
      } else {
        TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link) {
          if (tda->tda_rootpath) {          //if rootpath is NULL then can't rely on tda_adapter_num because it is always 0
            if (tda->tda_adapter_num > MAX_CA) {
              tvhlog(LOG_ERR, "capmt", "adapter number > MAX_CA");
              continue;
            }
            tvhlog(LOG_INFO, "capmt", "Creating capmt UDP socket for adapter %d", tda->tda_adapter_num);
            bind_ok = capmt_create_udp_socket(&capmt->capmt_sock_ca0[tda->tda_adapter_num], 9000 + tda->tda_adapter_num);
          }
        }
      }
      if (bind_ok)
        handle_ca0(capmt);
    } else 
      tvhlog(LOG_ERR, "capmt", "Error connecting to %s: %s", capmt->capmt_sockfile, strerror(errno));
  
    capmt->capmt_connected = 0;

    /* close opened sockets */
    for (i = 0; i < MAX_SOCKETS; i++)
      if (capmt->capmt_sock[i] > 0)
        close(capmt->capmt_sock[i]);
    for (i = 0; i < MAX_CA; i++)
      if (capmt->capmt_sock_ca0[i] > 0)
        close(capmt->capmt_sock_ca0[i]);

    /* schedule reconnection */
    if(subscriptions_active()) {
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

  return NULL;
}

/**
 *
 */
static void
capmt_table_input(struct th_descrambler *td, struct service *t,
    struct elementary_stream *st, const uint8_t *data, int len)
{
  capmt_service_t *ct = (capmt_service_t *)td;
  capmt_t *capmt = ct->ct_capmt;
  int adapter_num = t->s_dvb_mux_instance->tdmi_adapter->tda_adapter_num;
  int total_caids = 0, current_caid = 0;
  int i;

  caid_t *c;

  LIST_FOREACH(c, &st->es_caids, link) {
    total_caids++;
  }

  LIST_FOREACH(c, &st->es_caids, link) {
    current_caid++;

    if(c == NULL)
      return;

    if(len > 4096)
      return;

    switch(data[0]) {
      case 0x80:
      case 0x81:
        {
          /* ECM */
          if (ct->ct_caid_last == -1)
            ct->ct_caid_last = c->caid;

          uint16_t caid = c->caid;
          /* search ecmpid in list */
          capmt_caid_ecm_t *cce, *cce2;
          LIST_FOREACH(cce, &ct->ct_caid_ecm, cce_link)
            if (cce->cce_caid == caid)
              break;

          if (!cce) 
          {
            tvhlog(LOG_DEBUG, "capmt",
              "New caid 0x%04X for service \"%s\"", c->caid, t->s_svcname);

            /* ecmpid not already seen, add it to list */
            cce             = calloc(1, sizeof(capmt_caid_ecm_t));
            cce->cce_caid   = c->caid;
            cce->cce_ecmpid = st->es_pid;
            cce->cce_providerid = c->providerid;
            LIST_INSERT_HEAD(&ct->ct_caid_ecm, cce, cce_link);
          }

          if ((cce->cce_ecmsize == len) && !memcmp(cce->cce_ecm, data, len))
            break; /* key already sent */

          if(!capmt->capmt_oscam && capmt->capmt_sock[0] == 0) {
            /* New key, but we are not connected (anymore), can not descramble */
            ct->ct_keystate = CT_UNKNOWN;
            break;
          }

          uint16_t sid = t->s_dvb_service_id;
          uint16_t ecmpid = st->es_pid;
          uint16_t transponder = 0;

          /* don't do too much requests */
          if (current_caid == total_caids && caid != ct->ct_caid_last)
            return;

          static uint8_t pmtversion = 1;

          /* buffer for capmt */
          int pos = 0;
          uint8_t buf[4094];

          capmt_header_t head = {
            .capmt_indicator        = { 0x9F, 0x80, 0x32, 0x82, 0x00, 0x00 },
            .capmt_list_management  = CAPMT_LIST_ONLY,
            .program_number         = sid,
            .version_number         = 0, 
            .current_next_indicator = 0,
            .program_info_length    = 0,
            .capmt_cmd_id           = CAPMT_CMD_OK_DESCRAMBLING,
          };
          memcpy(&buf[pos], &head, sizeof(head));
          pos += sizeof(head);

          if (capmt->capmt_oscam)
          {
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
            .cad_data = { 0x00, 0x00, 0x00, 0x00, 
              sid >> 8, sid & 0xFF,
              transponder >> 8, transponder & 0xFF
            }};
          memcpy(&buf[pos], &prd, prd.cad_length + 2);
          pos += prd.cad_length + 2;

          if (!capmt->capmt_oscam)
          {
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
              ecmpid >> 8, ecmpid & 0xFF }};
          memcpy(&buf[pos], &ecd, ecd.cad_length + 2);
          pos += ecd.cad_length + 2;

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

          memcpy(cce->cce_ecm, data, len);
          cce->cce_ecmsize = len;

          if(ct->ct_keystate != CT_RESOLVED)
            tvhlog(LOG_INFO, "capmt",
              "Trying to obtain key for service \"%s\"",t->s_svcname);

          buf[9] = pmtversion;
          pmtversion = (pmtversion + 1) & 0x1F;

          int found = 0;
          if (capmt->capmt_oscam) {
            for (i = 0; i < MAX_SOCKETS; i++) {
              if (capmt->sids[i] == sid) {
                found = 1;
                break;
              }
            }
          }
          if ((capmt->capmt_oscam && !found) || !capmt->capmt_oscam)
            capmt_send_msg(capmt, sid, buf, pos);
          break;
        }
      default:
        /* EMM */
        break;
    }
  }
}


/**
 *
 */
#if ENABLE_DVBCSA
static int
capmt_descramble(th_descrambler_t *td, service_t *t, struct elementary_stream *st,
     const uint8_t *tsb)
{
  capmt_service_t *ct = (capmt_service_t *)td;
  uint8_t *pkt;
  int xc0;
  int ev_od;
  int len;
  int offset;
  int n;
  // FIXME: //int residue;
  int i;
  uint8_t *t0;

  if(ct->ct_keystate == CT_FORBIDDEN)
    return 1;

  if(ct->ct_keystate != CT_RESOLVED)
    return -1;

  pkt = ct->ct_tsbcluster + ct->ct_fill * 188;
  memcpy(pkt, tsb, 188);
  ct->ct_fill++;

  do { // handle this packet
    xc0 = pkt[3] & 0xc0;
    if(xc0 == 0x00) { // clear
      break;
    }
    if(xc0 == 0x40) { // reserved
      break;
    }
    if(xc0 == 0x80 || xc0 == 0xc0) { // encrypted
      ev_od = (xc0 & 0x40) >> 6; // 0 even, 1 odd
      pkt[3] &= 0x3f;  // consider it decrypted now
      if(pkt[3] & 0x20) { // incomplete packet
        offset = 4 + pkt[4] + 1;
        len = 188 - offset;
        n = len >> 3;
        // FIXME: //residue = len - (n << 3);
        if(n == 0) { // decrypted==encrypted!
          break; // this doesn't need more processing
        }
      } else {
        len = 184;
        offset = 4;
        // FIXME: //n = 23;
        // FIXME: //residue = 0;
      }
      if(ev_od == 0) {
        ct->ct_tsbbatch_even[ct->ct_fill_even].data = pkt + offset;
        ct->ct_tsbbatch_even[ct->ct_fill_even].len = len;
        ct->ct_fill_even++;
      } else {
        ct->ct_tsbbatch_odd[ct->ct_fill_odd].data = pkt + offset;
        ct->ct_tsbbatch_odd[ct->ct_fill_odd].len = len;
        ct->ct_fill_odd++;
      }
    }
  } while(0);

  if(ct->ct_fill != ct->ct_cluster_size)
    return 0;

  if(ct->ct_fill_even) {
    ct->ct_tsbbatch_even[ct->ct_fill_even].data = NULL;
    dvbcsa_bs_decrypt(ct->ct_key_even, ct->ct_tsbbatch_even, 184);
    ct->ct_fill_even = 0;
  }
  if(ct->ct_fill_odd) {
    ct->ct_tsbbatch_odd[ct->ct_fill_odd].data = NULL;
    dvbcsa_bs_decrypt(ct->ct_key_odd, ct->ct_tsbbatch_odd, 184);
    ct->ct_fill_odd = 0;
  }

    t0 = ct->ct_tsbcluster;
    for(i = 0; i < ct->ct_fill; i++) {
      ts_recv_packet2(t, t0);
      t0 += 188;
    }
  ct->ct_fill = 0;
  return 0;
}
#else
static int
capmt_descramble(th_descrambler_t *td, service_t *t, struct elementary_stream *st,
     const uint8_t *tsb)
{
  capmt_service_t *ct = (capmt_service_t *)td;
  int r, i;
  unsigned char *vec[3];
  uint8_t *t0;

  if(ct->ct_keystate == CT_FORBIDDEN)
    return 1;

  if(ct->ct_keystate != CT_RESOLVED)
    return -1;

  memcpy(ct->ct_tsbcluster + ct->ct_fill * 188, tsb, 188);
  ct->ct_fill++;

  if(ct->ct_fill != ct->ct_cluster_size)
    return 0;

  ct->ct_fill = 0;

  vec[0] = ct->ct_tsbcluster;
  vec[1] = ct->ct_tsbcluster + ct->ct_cluster_size * 188;
  vec[2] = NULL;

  while(1) {
    t0 = vec[0];
    r = decrypt_packets(ct->ct_keys, vec);
    if(r == 0)
      break;
    for(i = 0; i < r; i++) {
      ts_recv_packet2(t, t0);
      t0 += 188;
    }
  }
  return 0;
}
#endif

/**
 * Check if our CAID's matches, and if so, link
 *
 * global_lock is held
 */
void
capmt_service_start(service_t *t)
{
  capmt_t *capmt;
  capmt_service_t *ct;
  capmt_caid_ecm_t *cce;
  th_descrambler_t *td;
  
  lock_assert(&global_lock);

  TAILQ_FOREACH(capmt, &capmts, capmt_link) {
    tvhlog(LOG_INFO, "capmt",
      "Starting capmt server for service \"%s\" on tuner %d", 
      t->s_svcname,
      t->s_dvb_mux_instance->tdmi_adapter->tda_adapter_num);

    elementary_stream_t *st;

    /* create new capmt service */
    ct                   = calloc(1, sizeof(capmt_service_t));
#if ENABLE_DVBCSA
    ct->ct_cluster_size  = dvbcsa_bs_batch_size();
#else
    ct->ct_cluster_size  = get_suggested_cluster_size();
#endif
    ct->ct_tsbcluster    = malloc(ct->ct_cluster_size * 188);
    ct->ct_seq           = capmt->capmt_seq++;
#if ENABLE_DVBCSA
    ct->ct_tsbbatch_even = malloc((ct->ct_cluster_size + 1) *
                             sizeof(struct dvbcsa_bs_batch_s));
    ct->ct_tsbbatch_odd  = malloc((ct->ct_cluster_size + 1) *
                             sizeof(struct dvbcsa_bs_batch_s));
#endif

    TAILQ_FOREACH(st, &t->s_components, es_link) {
      caid_t *c;
      LIST_FOREACH(c, &st->es_caids, link) {
        if(c == NULL)
          continue;

        tvhlog(LOG_DEBUG, "capmt",
          "New caid 0x%04X for service \"%s\"", c->caid, t->s_svcname);

        /* add it to list */
        cce             = calloc(1, sizeof(capmt_caid_ecm_t));
        cce->cce_caid   = c->caid;
        cce->cce_ecmpid = st->es_pid;
        cce->cce_providerid = c->providerid;
        LIST_INSERT_HEAD(&ct->ct_caid_ecm, cce, cce_link);

        /* sending request will be based on first seen caid */
        ct->ct_caid_last = -1;
      }
    }

#if ENABLE_DVBCSA
    ct->ct_key_even   = dvbcsa_bs_key_alloc();
    ct->ct_key_odd    = dvbcsa_bs_key_alloc();
#else
    ct->ct_keys       = get_key_struct();
#endif
    ct->ct_capmt      = capmt;
    ct->ct_service  = t;

    td = &ct->ct_head;
    td->td_stop       = capmt_service_destroy;
    td->td_table      = capmt_table_input;
    td->td_descramble = capmt_descramble;
    LIST_INSERT_HEAD(&t->s_descramblers, td, td_service_link);

    LIST_INSERT_HEAD(&capmt->capmt_services, ct, ct_link);
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
  pthread_attr_t attr;
  pthread_t ptid;
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

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&ptid, &attr, capmt_thread, capmt);
  pthread_attr_destroy(&attr);

  return capmt;
}


/**
 *
 */
static htsmsg_t *
capmt_record_build(capmt_t *capmt)
{
  htsmsg_t *e = htsmsg_create_map();

  htsmsg_add_str(e, "id", capmt->capmt_id);
  htsmsg_add_u32(e, "enabled",  !!capmt->capmt_enabled);
  htsmsg_add_u32(e, "connected", !!capmt->capmt_connected);

  htsmsg_add_str(e, "camdfilename", capmt->capmt_sockfile ?: "");
  htsmsg_add_u32(e, "port", capmt->capmt_port);
  htsmsg_add_u32(e, "oscam", !!capmt->capmt_oscam);
  htsmsg_add_str(e, "comment", capmt->capmt_comment ?: "");
  
  return e;
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

