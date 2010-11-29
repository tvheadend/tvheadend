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
#include <rpc/des_crypt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>

#include "tvheadend.h"
#include "dvb/dvb.h"
#include "tcp.h"
#include "psi.h"
#include "tsdemux.h"
#include "ffdecsa/FFdecsa.h"
#include "transports.h"
#include "capmt.h"
#include "notify.h"

#include "dtable.h"

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

/**
 *
 */
TAILQ_HEAD(capmt_queue, capmt);
LIST_HEAD(capmt_transport_list, capmt_transport);
LIST_HEAD(capmt_caid_ecm_list, capmt_caid_ecm);
static struct capmt_queue capmts;
static pthread_cond_t capmt_config_changed;

/** 
 * capmt descriptor
 */
typedef struct capmt_descriptor {
  uint8_t cad_type;
  uint8_t cad_length;
  uint8_t cad_data[16];
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
  /** last ecm size */
  uint32_t cce_ecmsize;
  /** last ecm buffer */
  uint8_t  cce_ecm[4096];
  
  LIST_ENTRY(capmt_caid_ecm) cce_link;
} capmt_caid_ecm_t;

/**
 *
 */
typedef struct capmt_transport {
  th_descrambler_t ct_head;

  th_transport_t *ct_transport;

  struct capmt *ct_capmt;

  LIST_ENTRY(capmt_transport) ct_link;

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

  /* buffer for keystruct */
  void    *ct_keys;

  /* CSA */
  int      ct_cluster_size;
  uint8_t *ct_tsbcluster;
  int      ct_fill;

  /* current sequence number */
  uint16_t ct_seq;

  /* sending requests will be based on this caid */
  int      ct_caid_last;
} capmt_transport_t;


/**
 *
 */
typedef struct capmt {
  pthread_cond_t capmt_cond;

  TAILQ_ENTRY(capmt) capmt_link; /* Linkage protected via global_lock */

  struct capmt_transport_list capmt_transports;

  /* from capmt configuration */
  char *capmt_sockfile;
  char *capmt_hostname;
  int   capmt_port;
  char *capmt_comment;
  char *capmt_id;

  /* capmt sockets */
  int   capmt_sock;
  int   capmt_sock_ca0;

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
capmt_send_msg(capmt_t *capmt, const uint8_t *buf, size_t len)
{
  return write(capmt->capmt_sock, buf, len);
}

static void 
capmt_send_stop(capmt_transport_t *t)
{
  /* buffer for capmt */
  int pos = 0;
  uint8_t buf[4094];

  capmt_header_t head = {
    .capmt_indicator        = { 0x9F, 0x80, 0x32, 0x82, 0x00, 0x00 },
    .capmt_list_management  = CAPMT_LIST_ONLY,
    .program_number         = t->ct_transport->tht_dvb_service_id,
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
  buf[7]  = t->ct_transport->tht_dvb_service_id >> 8;
  buf[8]  = t->ct_transport->tht_dvb_service_id & 0xFF;
  buf[9]  = 1;
  buf[10] = ((pos - 5 - 12) & 0xF00) >> 8;
  buf[11] = ((pos - 5 - 12) & 0xFF);
  
  capmt_send_msg(t->ct_capmt, buf, pos);
}

/**
 * global_lock is held
 * tht_stream_mutex is held
 */
static void 
capmt_transport_destroy(th_descrambler_t *td)
{
  tvhlog(LOG_INFO, "capmt", "Removing CAPMT Server from service");

  capmt_transport_t *ct = (capmt_transport_t *)td;

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

  LIST_REMOVE(td, td_transport_link);

  LIST_REMOVE(ct, ct_link);

  free_key_struct(ct->ct_keys);
  free(ct->ct_tsbcluster);
  free(ct);
}

static void 
handle_ca0(capmt_t* capmt) {
  capmt_transport_t *ct;
  th_transport_t *t;
  int ret;

  uint8_t invalid[8], buffer[20], *even, *odd;
  uint16_t seq;
  memset(invalid, 0, 8);

  tvhlog(LOG_INFO, "capmt", "got connection from client ...");

  while (capmt->capmt_running) {

    ret = recv(capmt->capmt_sock_ca0, buffer, 18, MSG_WAITALL);

    if (ret < 0)
      tvhlog(LOG_ERR, "capmt", "error receiving over socket");
    else if (ret == 0) {
      // normal socket shutdown
      tvhlog(LOG_INFO, "capmt", "normal socket shutdown");
      break;
    } 
      
    /* get control words */
    seq  = buffer[0] | ((uint16_t)buffer[1] << 8);
    even = &buffer[2];
    odd  = &buffer[10];

    LIST_FOREACH(ct, &capmt->capmt_transports, ct_link) {
      t = ct->ct_transport;

      if(ret < 18) {
        if(ct->ct_keystate != CT_FORBIDDEN) {
          tvhlog(LOG_ERR, "capmt", "Can not descramble service \"%s\", access denied", t->tht_svcname);

          ct->ct_keystate = CT_FORBIDDEN;
        }

        continue;
      }

      if(seq != ct->ct_seq)
        continue;

      if (memcmp(even, invalid, 8))
        set_even_control_word(ct->ct_keys, even);
      if (memcmp(odd, invalid, 8))
        set_odd_control_word(ct->ct_keys, odd);

      if(ct->ct_keystate != CT_RESOLVED)
        tvhlog(LOG_INFO, "capmt", "Obtained key for service \"%s\"",t->tht_svcname);

      ct->ct_keystate = CT_RESOLVED;
    }
  }

  tvhlog(LOG_INFO, "capmt", "connection from client closed ...");
}

/**
 *
 */
static void *
capmt_thread(void *aux) 
{
  capmt_t *capmt = aux;
  struct timespec ts;
  int d;

  while (capmt->capmt_running) {
    capmt->capmt_sock = -1;
    capmt->capmt_sock_ca0 = -1;
    capmt->capmt_connected = 0;
    
    pthread_mutex_lock(&global_lock);

    while(capmt->capmt_running && capmt->capmt_enabled == 0)
      pthread_cond_wait(&capmt->capmt_cond, &global_lock);

    pthread_mutex_unlock(&global_lock);

    /* open connection to camd.socket */
    capmt->capmt_sock = tvh_socket(AF_LOCAL, SOCK_STREAM, 0);

    struct sockaddr_un serv_addr_un;
    memset(&serv_addr_un, 0, sizeof(serv_addr_un));
    serv_addr_un.sun_family = AF_LOCAL;
    snprintf(serv_addr_un.sun_path, sizeof(serv_addr_un.sun_path), "%s", capmt->capmt_sockfile);

    if (connect(capmt->capmt_sock, (const struct sockaddr*)&serv_addr_un, sizeof(serv_addr_un)) == 0) {
      capmt->capmt_connected = 1;

      /* open connection to emulated ca0 device */
      capmt->capmt_sock_ca0 = tvh_socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

      struct sockaddr_in serv_addr;
      serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
      serv_addr.sin_port = htons( (unsigned short int)capmt->capmt_port);
      serv_addr.sin_family = AF_INET;

      if (bind(capmt->capmt_sock_ca0, (const struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) 
        perror("[CapmtServer] ERROR binding to ca0");
      else 
        handle_ca0(capmt);
    } else 
      tvhlog(LOG_ERR, "capmt", "Error connecting to %s: %s", capmt->capmt_sockfile, strerror(errno));
  
    capmt->capmt_connected = 0;

    /* close opened sockets */
    if (capmt->capmt_sock > 0)
      close(capmt->capmt_sock);
    if (capmt->capmt_sock_ca0 > 0)
      close(capmt->capmt_sock_ca0);

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
capmt_table_input(struct th_descrambler *td, struct th_transport *t,
    struct th_stream *st, const uint8_t *data, int len)
{
  capmt_transport_t *ct = (capmt_transport_t *)td;
  capmt_t *capmt = ct->ct_capmt;
  int adapter_num = t->tht_dvb_mux_instance->tdmi_adapter->tda_adapter_num;

  caid_t *c;

  c = LIST_FIRST(&st->st_caids);
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
            "New caid 0x%04X for service \"%s\"", c->caid, t->tht_svcname);

          /* ecmpid not already seen, add it to list */
          cce             = calloc(1, sizeof(capmt_caid_ecm_t));
          cce->cce_caid   = c->caid;
          cce->cce_ecmpid = st->st_pid;
          LIST_INSERT_HEAD(&ct->ct_caid_ecm, cce, cce_link);
        }

        if ((cce->cce_ecmsize == len) && !memcmp(cce->cce_ecm, data, len))
          break; /* key already sent */

        if(capmt->capmt_sock == -1) {
          /* New key, but we are not connected (anymore), can not descramble */
          ct->ct_keystate = CT_UNKNOWN;
          break;
        }

        uint16_t sid = t->tht_dvb_service_id;
        uint16_t ecmpid = st->st_pid;
        uint16_t transponder = 0;

        /* don't do too much requests */
        if (caid != ct->ct_caid_last)
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

        capmt_descriptor_t prd = { 
          .cad_type = CAPMT_DESC_PRIVATE, 
          .cad_length = 0x08,
          .cad_data = { 0x00, 0x00, 0x00, 0x00, 
            sid >> 8, sid & 0xFF,
            transponder >> 8, transponder & 0xFF
          }};
        memcpy(&buf[pos], &prd, prd.cad_length + 2);
        pos += prd.cad_length + 2;

        capmt_descriptor_t dmd = { 
          .cad_type = CAPMT_DESC_DEMUX, 
          .cad_length = 0x02,
          .cad_data = { 
            1 << adapter_num, adapter_num }};
        memcpy(&buf[pos], &dmd, dmd.cad_length + 2);
        pos += dmd.cad_length + 2;

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
          memcpy(&buf[pos], &cad, cad.cad_length + 2);
          pos += cad.cad_length + 2;
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
            "Trying to obtain key for service \"%s\"",t->tht_svcname);

        buf[9] = pmtversion;
        pmtversion = (pmtversion + 1) & 0x1F;

        capmt_send_msg(capmt, buf, pos);
        break;
      }
    default:
      /* EMM */
      break;
  }
}


/**
 *
 */
static int
capmt_descramble(th_descrambler_t *td, th_transport_t *t, struct th_stream *st,
     const uint8_t *tsb)
{
  capmt_transport_t *ct = (capmt_transport_t *)td;
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

/**
 * Check if our CAID's matches, and if so, link
 *
 * global_lock is held
 */
void
capmt_transport_start(th_transport_t *t)
{
  capmt_t *capmt;
  capmt_transport_t *ct;
  capmt_caid_ecm_t *cce;
  th_descrambler_t *td;
  
  lock_assert(&global_lock);

  TAILQ_FOREACH(capmt, &capmts, capmt_link) {
    tvhlog(LOG_INFO, "capmt",
      "Starting capmt server for service \"%s\" on tuner %d", 
      t->tht_svcname,
      t->tht_dvb_mux_instance->tdmi_adapter->tda_adapter_num);

    th_stream_t *st;

    /* create new capmt transport */
    ct                  = calloc(1, sizeof(capmt_transport_t));
    ct->ct_cluster_size = get_suggested_cluster_size();
    ct->ct_tsbcluster   = malloc(ct->ct_cluster_size * 188);
    ct->ct_seq          = capmt->capmt_seq++;

    TAILQ_FOREACH(st, &t->tht_components, st_link) {
      caid_t *c = LIST_FIRST(&st->st_caids);
      if(c == NULL)
	continue;

      tvhlog(LOG_DEBUG, "capmt",
        "New caid 0x%04X for service \"%s\"", c->caid, t->tht_svcname);

      /* add it to list */
      cce             = calloc(1, sizeof(capmt_caid_ecm_t));
      cce->cce_caid   = c->caid;
      cce->cce_ecmpid = st->st_pid;
      LIST_INSERT_HEAD(&ct->ct_caid_ecm, cce, cce_link);

      /* sending request will be based on first seen caid */
      ct->ct_caid_last = -1;
    }

    ct->ct_keys       = get_key_struct();
    ct->ct_capmt      = capmt;
    ct->ct_transport  = t;

    td = &ct->ct_head;
    td->td_stop       = capmt_transport_destroy;
    td->td_table      = capmt_table_input;
    td->td_descramble = capmt_descramble;
    LIST_INSERT_HEAD(&t->tht_descramblers, td, td_transport_link);

    LIST_INSERT_HEAD(&capmt->capmt_transports, ct, ct_link);
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

