/*
 *  tvheadend, CWC interface
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
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/des.h>

#include "tvheadend.h"
#include "tcp.h"
#include "caclient.h"
#include "descrambler/caid.h"
#include "descrambler/emm_reass.h"
#include "atomic.h"
#include "subscriptions.h"
#include "service.h"
#include "input.h"

/**
 *
 */
#define TVHEADEND_PROTOCOL_ID 0x6502
#define CWC_KEEPALIVE_INTERVAL 30
#define CWC_ES_PIDS           8
#define CWC_MAX_NOKS          3

#define CWS_NETMSGSIZE 1024
#define CWS_FIRSTCMDNO 0xe0

typedef enum {
  MSG_CLIENT_2_SERVER_LOGIN = CWS_FIRSTCMDNO,
  MSG_CLIENT_2_SERVER_LOGIN_ACK,
  MSG_CLIENT_2_SERVER_LOGIN_NAK,
  MSG_CARD_DATA_REQ,
  MSG_CARD_DATA,
  MSG_SERVER_2_CLIENT_NAME,
  MSG_SERVER_2_CLIENT_NAME_ACK,
  MSG_SERVER_2_CLIENT_NAME_NAK,
  MSG_SERVER_2_CLIENT_LOGIN,
  MSG_SERVER_2_CLIENT_LOGIN_ACK,
  MSG_SERVER_2_CLIENT_LOGIN_NAK,
  MSG_ADMIN,
  MSG_ADMIN_ACK,
  MSG_ADMIN_LOGIN,
  MSG_ADMIN_LOGIN_ACK,
  MSG_ADMIN_LOGIN_NAK,
  MSG_ADMIN_COMMAND,
  MSG_ADMIN_COMMAND_ACK,
  MSG_ADMIN_COMMAND_NAK,
  MSG_KEEPALIVE = CWS_FIRSTCMDNO + 0x1d
} net_msg_type_t;


/**
 *
 */
TAILQ_HEAD(cwc_queue, cwc);
LIST_HEAD(cwc_cards_list, cs_card_data);
LIST_HEAD(cwc_service_list, cwc_service);
TAILQ_HEAD(cwc_message_queue, cwc_message);
LIST_HEAD(ecm_section_list, ecm_section);
static char *crypt_md5(const char *pw, const char *salt);

/**
 *
 */
typedef struct ecm_section {
  LIST_ENTRY(ecm_section) es_link;

  enum {
    ES_UNKNOWN,
    ES_RESOLVED,
    ES_FORBIDDEN,
    ES_IDLE
  } es_keystate;

  int es_section;
  int es_channel;
  uint16_t es_caid;
  uint16_t es_provid;

  uint16_t es_seq;
  char es_nok;
  char es_pending;
  char es_resolved;
  int64_t es_time;  // time request was sent

} ecm_section_t;


/**
 *
 */
typedef struct ecm_pid {
  LIST_ENTRY(ecm_pid) ep_link;

  uint16_t ep_pid;

  int ep_last_section;
  LIST_HEAD(, ecm_section) ep_sections;
} ecm_pid_t;


/**
 *
 */
typedef struct cwc_service {
  th_descrambler_t;

  struct cwc *cs_cwc;

  LIST_ENTRY(cwc_service) cs_link;

  int cs_channel;
  int cs_epids[CWC_ES_PIDS];
  mpegts_mux_t *cs_mux;

  /**
   * ECM Status
   */
  enum {
    ECM_INIT,
    ECM_VALID,
    ECM_RESET
  } ecm_state;

  LIST_HEAD(, ecm_pid) cs_pids;

} cwc_service_t;


/**
 *
 */
typedef struct cwc_message {
  TAILQ_ENTRY(cwc_message) cm_link;
  int cm_len;
  uint8_t cm_data[CWS_NETMSGSIZE];
} cwc_message_t;

/**
 *
 */
typedef struct cwc_provider {
  uint32_t id;
  uint8_t sa[8];
} cwc_provider_t;

struct cwc;

typedef struct cs_card_data {
  
  LIST_ENTRY(cs_card_data) cs_card;

  emm_reass_t cs_ra;

  struct cwc *cwc;
  mpegts_mux_t *cwc_mux;
  int running;

} cs_card_data_t;

/**
 *
 */
typedef struct cwc {
  caclient_t;

  int cwc_fd;

  int cwc_keepalive_interval;
  int cwc_retry_delay;

  pthread_t cwc_tid;

  tvh_cond_t cwc_cond;

  pthread_mutex_t cwc_mutex;
  pthread_mutex_t cwc_writer_mutex; 
  tvh_cond_t cwc_writer_cond;
  int cwc_writer_running;
  struct cwc_message_queue cwc_writeq;

  struct cwc_service_list cwc_services;


  struct cwc_cards_list cwc_cards;
  int cwc_seq;

  DES_key_schedule cwc_k1, cwc_k2;

  uint8_t cwc_buf[256];
  int cwc_bufptr;

  /* Emm forwarding */
  int cwc_forward_emm;

  /* Emm exclusive update */
  int64_t cwc_emm_update_time;
  void *cwc_emm_mux;
  
  /* From configuration */

  uint8_t cwc_confedkey[14];
  char *cwc_username;
  char *cwc_password;
  char *cwc_password_salted;   /* salted version */
  char *cwc_hostname;
  int cwc_port;
  int cwc_emm;
  int cwc_emmex;

  const char *cwc_errtxt;

  int cwc_running;
  int cwc_reconfigure;
} cwc_t;


/**
 *
 */

static void cwc_service_pid_free(cwc_service_t *ct);


/**
 *
 */
static void 
des_key_parity_adjust(uint8_t *key, uint8_t len)
{
  uint8_t i, j, parity;
  
  for (i = 0; i < len; i++) {
    parity = 1;
    for (j = 1; j < 8; j++) if ((key[i] >> j) & 0x1) parity = ~parity & 0x01;
    key[i] |= parity;
  }
}


/**
 *
 */
static void
des_key_spread(uint8_t *normal, uint8_t *spread)
{
  spread[ 0] = normal[ 0] & 0xfe;
  spread[ 1] = ((normal[ 0] << 7) | (normal[ 1] >> 1)) & 0xfe;
  spread[ 2] = ((normal[ 1] << 6) | (normal[ 2] >> 2)) & 0xfe;
  spread[ 3] = ((normal[ 2] << 5) | (normal[ 3] >> 3)) & 0xfe;
  spread[ 4] = ((normal[ 3] << 4) | (normal[ 4] >> 4)) & 0xfe;
  spread[ 5] = ((normal[ 4] << 3) | (normal[ 5] >> 5)) & 0xfe;
  spread[ 6] = ((normal[ 5] << 2) | (normal[ 6] >> 6)) & 0xfe;
  spread[ 7] = normal[ 6] << 1;
  spread[ 8] = normal[ 7] & 0xfe;
  spread[ 9] = ((normal[ 7] << 7) | (normal[ 8] >> 1)) & 0xfe;
  spread[10] = ((normal[ 8] << 6) | (normal[ 9] >> 2)) & 0xfe;
  spread[11] = ((normal[ 9] << 5) | (normal[10] >> 3)) & 0xfe;
  spread[12] = ((normal[10] << 4) | (normal[11] >> 4)) & 0xfe;
  spread[13] = ((normal[11] << 3) | (normal[12] >> 5)) & 0xfe;
  spread[14] = ((normal[12] << 2) | (normal[13] >> 6)) & 0xfe;
  spread[15] = normal[13] << 1;

  des_key_parity_adjust(spread, 16);
}

/**
 *
 */
static int
des_encrypt(uint8_t *buffer, int len, cwc_t *cwc)
{
  uint8_t checksum = 0;
  uint8_t noPadBytes;
  uint8_t padBytes[7];
  DES_cblock ivec;
  uint16_t i;

  noPadBytes = (8 - ((len - 1) % 8)) % 8;
  if (len + noPadBytes + 1 >= CWS_NETMSGSIZE-8) return -1;
  uuid_random(padBytes, noPadBytes);
  for (i = 0; i < noPadBytes; i++) buffer[len++] = padBytes[i];
  for (i = 2; i < len; i++) checksum ^= buffer[i];
  buffer[len++] = checksum;
  uuid_random((uint8_t *)ivec, 8);
  memcpy(buffer+len, ivec, 8);
  for (i = 2; i < len; i += 8) {
    DES_ncbc_encrypt(buffer+i, buffer+i, 8,  &cwc->cwc_k1, &ivec, 1);

    DES_ecb_encrypt((DES_cblock *)(buffer+i), (DES_cblock *)(buffer+i),
                    &cwc->cwc_k2, 0);

    DES_ecb_encrypt((DES_cblock *)(buffer+i), (DES_cblock *)(buffer+i),
                    &cwc->cwc_k1, 1);
    memcpy(ivec, buffer+i, 8);
  }
  len += 8;
  return len;
}

/**
 *
 */
static int 
des_decrypt(uint8_t *buffer, int len, cwc_t *cwc)
{
  DES_cblock ivec;
  DES_cblock nextIvec;
  int i;
  uint8_t checksum = 0;

  if ((len-2) % 8 || (len-2) < 16) return -1;
  len -= 8;
  memcpy(nextIvec, buffer+len, 8);
  for (i = 2; i < len; i += 8)
    {
      memcpy(ivec, nextIvec, 8);
      memcpy(nextIvec, buffer+i, 8);

      DES_ecb_encrypt((DES_cblock *)(buffer+i), (DES_cblock *)(buffer+i),
                      &cwc->cwc_k1, 0);
      DES_ecb_encrypt((DES_cblock *)(buffer+i), (DES_cblock *)(buffer+i),
                      &cwc->cwc_k2, 1);

      DES_ncbc_encrypt(buffer+i, buffer+i, 8,  &cwc->cwc_k1, &ivec, 0);
    } 
  for (i = 2; i < len; i++) checksum ^= buffer[i];
  if (checksum) return -1;
  return len;
}

/**
 *
 */
static void
des_make_login_key(cwc_t *cwc, uint8_t *k)
{
  uint8_t des14[14], spread[16];
  int i;

  for (i = 0; i < 14; i++) 
    des14[i] = cwc->cwc_confedkey[i] ^ k[i];
  des_key_spread(des14, spread);

  DES_set_key_unchecked((DES_cblock *)spread,     &cwc->cwc_k1);
  DES_set_key_unchecked((DES_cblock *)(spread+8), &cwc->cwc_k2);
}

/**
 *
 */
static void
des_make_session_key(cwc_t *cwc)
{
  uint8_t des14[14], spread[16], *k2 = (uint8_t *)cwc->cwc_password_salted;
  int i, l = strlen(cwc->cwc_password_salted);

  memcpy(des14, cwc->cwc_confedkey, 14);

  for (i = 0; i < l; i++)
    des14[i % 14] ^= k2[i];

  des_key_spread(des14, spread);
  DES_set_key_unchecked((DES_cblock *)spread,     &cwc->cwc_k1);
  DES_set_key_unchecked((DES_cblock *)(spread+8), &cwc->cwc_k2);
}

/**
 * Note, this function is called from multiple threads so beware of
 * locking / race issues (Note how we use atomic_add() to generate
 * the ID)
 */
static int
cwc_send_msg(cwc_t *cwc, const uint8_t *msg, size_t len, int sid, int enq, uint16_t st_caid , uint32_t st_provider )
{
  cwc_message_t *cm = malloc(sizeof(cwc_message_t));
  uint8_t *buf = cm->cm_data;
  int seq, ret;

  if (len < 3 || len + 12 > CWS_NETMSGSIZE) {
    free(cm);
    return -1;
  }

  seq = atomic_add(&cwc->cwc_seq, 1);

  buf[0] = buf[1] = 0;
  buf[2] = (seq >> 8) & 0xff;
  buf[3] = seq & 0xff;
  buf[4] = (sid >> 8) & 0xff;
  buf[5] = sid & 0xff;
  buf[6] = (st_caid >> 8) & 0xff;
  buf[7] = st_caid & 0xff;
  buf[8] = (st_provider >> 16) & 0xff;
  buf[9] = (st_provider >> 8) & 0xff;
  buf[10] = st_provider & 0xff;
  buf[11] = (st_provider >> 24) & 0xff; // used for mgcamd

  memcpy(buf+12, msg, len);
  // adding packet header size
  len += 12;

  if((ret = des_encrypt(buf, len, cwc)) <= 0) {
    free(cm);
    return -1;
  }
  len = (size_t)ret;

  tvhtrace(LS_CWC, "sending message sid %d len %zu enq %d", sid, len, enq);
  tvhlog_hexdump(LS_CWC, buf, len);

  buf[0] = (len - 2) >> 8;
  buf[1] = (len - 2) & 0xff;

  if(enq) {
    cm->cm_len = len;
    pthread_mutex_lock(&cwc->cwc_writer_mutex);
    TAILQ_INSERT_TAIL(&cwc->cwc_writeq, cm, cm_link);
    tvh_cond_signal(&cwc->cwc_writer_cond, 0);
    pthread_mutex_unlock(&cwc->cwc_writer_mutex);
  } else {
    if (tvh_write(cwc->cwc_fd, buf, len))
      tvhinfo(LS_CWC, "write error %s", strerror(errno));

    free(cm);
  }
  return seq & 0xffff;
}



/**
 * Card data command
 */

static void
cwc_send_data_req(cwc_t *cwc)
{
  uint8_t buf[CWS_NETMSGSIZE];

  buf[0] = MSG_CARD_DATA_REQ;
  buf[1] = 0;
  buf[2] = 0;

  cwc_send_msg(cwc, buf, 3, 0, 0, 0, 1);
}


/**
 * Send keep alive
 */
static void
cwc_send_ka(cwc_t *cwc)
{
  uint8_t buf[CWS_NETMSGSIZE];

  buf[0] = MSG_KEEPALIVE;
  buf[1] = 0;
  buf[2] = 0;

  cwc_send_msg(cwc, buf, 3, 0, 0, 0, 0);
}

/**
 *
 */
static cs_card_data_t *
cwc_new_card(cwc_t *cwc, uint16_t caid, uint8_t *ua,
             int pcount, uint8_t **pid, uint8_t **psa)
{
  cs_card_data_t *pcard = NULL;
  emm_provider_t *ep;
  const char *n;
  const uint8_t *id, *sa;
  int i, allocated = 0;

  LIST_FOREACH(pcard, &cwc->cwc_cards, cs_card)
    if (pcard->cs_ra.caid == caid)
      break;

  if (pcard == NULL) {
    pcard = calloc(1, sizeof(cs_card_data_t));
    emm_reass_init(&pcard->cs_ra, caid);
    allocated = 1;
  }

  if (ua)
    memcpy(pcard->cs_ra.ua, ua, 8);
  else
    memset(pcard->cs_ra.ua, 0, 8);

  free(pcard->cs_ra.providers);
  pcard->cs_ra.providers_count = pcount;
  pcard->cs_ra.providers = calloc(pcount, sizeof(pcard->cs_ra.providers[0]));

  for (i = 0, ep = pcard->cs_ra.providers; i < pcount; i++, ep++) {
    id = pid[i];
    if (id)
      ep->id = (id[0] << 16) | (id[1] << 8) | id[2];
    if (psa)
      memcpy(ep->sa, psa[i], 8);
  }

  n = caid2name(caid & 0xff00) ?: "Unknown";

  if (ua) {
    tvhinfo(LS_CWC, "%s:%i: Connected as user %s "
            "to a %s-card [0x%04x : %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x] "
            "with %d provider%s",
            cwc->cwc_hostname, cwc->cwc_port,
            cwc->cwc_username, n, caid,
            ua[0], ua[1], ua[2], ua[3], ua[4], ua[5], ua[6], ua[7],
            pcount, pcount > 1 ? "s" : "");
  } else {
    tvhinfo(LS_CWC, "%s:%i: Connected as user %s "
            "to a %s-card [0x%04x] with %d provider%s",
            cwc->cwc_hostname, cwc->cwc_port,
            cwc->cwc_username, n, caid,
            pcount, pcount > 1 ? "s" : "");
  }

  for (i = 0, ep = pcard->cs_ra.providers; i < pcount; i++, ep++) {
    if (psa) {
      sa = ep->sa;
      tvhinfo(LS_CWC, "%s:%i: Provider ID #%d: 0x%04x:0x%06x %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x",
              cwc->cwc_hostname, cwc->cwc_port, i + 1, caid, ep->id,
              sa[0], sa[1], sa[2], sa[3], sa[4], sa[5], sa[6], sa[7]);
    } else {
      tvhinfo(LS_CWC, "%s:%i: Provider ID #%d: 0x%04x:0x%06x",
              cwc->cwc_hostname, cwc->cwc_port, i + 1, caid, ep->id);
    }
  }

  pcard->running = 1;
  if (allocated)
    LIST_INSERT_HEAD(&cwc->cwc_cards, pcard, cs_card);
  return pcard;
}

/**
 * Handle reply to card data request
 */
static int
cwc_decode_card_data_reply(cwc_t *cwc, uint8_t *msg, int len)
{
  int plen, i, emm_allowed;
  unsigned int nprov;
  uint8_t **pid, **psa, *ua, *msg2;
  cs_card_data_t *pcard;

  msg += 12;
  len -= 12;

  if(len < 3) {
    tvhinfo(LS_CWC, "Invalid card data reply");
    return -1;
  }

  plen = (msg[1] & 0xf) << 8 | msg[2];

  if(plen < 14) {
    tvhinfo(LS_CWC, "Invalid card data reply (message)");
    return -1;
  }

  nprov = msg[14];

  msg2  = msg + 15;
  plen -= 12;

  if(plen < nprov * 11) {
    tvhinfo(LS_CWC, "Invalid card data reply (provider list)");
    return -1;
  }

  pid = nprov ? alloca(nprov * sizeof(uint8_t *)) : NULL;
  psa = nprov ? alloca(nprov * sizeof(uint8_t *)) : NULL;

  for (i = 0; i < nprov; i++) {
    pid[i] = msg2;
    psa[i] = msg2 + 3;
    msg2 += 11;
  }

  caclient_set_status((caclient_t *)cwc, CACLIENT_STATUS_CONNECTED);
  pcard = cwc_new_card(cwc, (msg[4] << 8) | msg[5], msg + 6, nprov, pid, psa);

  cwc->cwc_forward_emm = 0;
  if (cwc->cwc_emm) {
    ua = pcard->cs_ra.ua;
    emm_allowed = ua[0] || ua[1] || ua[2] || ua[3] ||
                  ua[4] || ua[5] || ua[6] || ua[7];

    if (!emm_allowed) {
      tvhinfo(LS_CWC, 
              "%s:%i: Will not forward EMMs (not allowed by server)",
              cwc->cwc_hostname, cwc->cwc_port);
    } else if (pcard->cs_ra.type != CARD_UNKNOWN) {
      tvhinfo(LS_CWC, "%s:%i: Will forward EMMs",
              cwc->cwc_hostname, cwc->cwc_port);
      cwc->cwc_forward_emm = 1;
    } else {
      tvhinfo(LS_CWC, 
             "%s:%i: Will not forward EMMs (unsupported CA system)",
              cwc->cwc_hostname, cwc->cwc_port);
    }
  }

  return 0;
}

/**
 * Login command
 */
static int
cwc_send_login(cwc_t *cwc)
{
  uint8_t buf[CWS_NETMSGSIZE];
  size_t ul, pl;

  if (cwc->cwc_username == NULL ||
      cwc->cwc_password_salted == NULL)
    return 1;

  ul = strlen(cwc->cwc_username) + 1;
  pl = strlen(cwc->cwc_password_salted) + 1;

  if (ul + pl > 255)
    return 1;

  buf[0] = MSG_CLIENT_2_SERVER_LOGIN;
  buf[1] = 0;
  buf[2] = ul + pl;
  memcpy(buf + 3,      cwc->cwc_username, ul);
  memcpy(buf + 3 + ul, cwc->cwc_password_salted, pl);

  cwc_send_msg(cwc, buf, ul + pl + 3, TVHEADEND_PROTOCOL_ID, 0, 0, 0);

  return 0;
}

static int
cwc_ecm_reset(th_descrambler_t *th)
{
  cwc_service_t *ct = (cwc_service_t *)th;
  cwc_t *cwc = ct->cs_cwc;
  ecm_pid_t *ep;
  ecm_section_t *es;

  pthread_mutex_lock(&cwc->cwc_mutex);
  descrambler_change_keystate(th, DS_READY, 1);
  LIST_FOREACH(ep, &ct->cs_pids, ep_link)
    LIST_FOREACH(es, &ep->ep_sections, es_link)
      es->es_keystate = ES_UNKNOWN;
  ct->ecm_state = ECM_RESET;
  pthread_mutex_unlock(&cwc->cwc_mutex);
  return 0;
}

static void
cwc_ecm_idle(th_descrambler_t *th)
{
  cwc_service_t *ct = (cwc_service_t *)th;
  cwc_t *cwc = ct->cs_cwc;
  ecm_pid_t *ep;
  ecm_section_t *es;

  pthread_mutex_lock(&cwc->cwc_mutex);
  LIST_FOREACH(ep, &ct->cs_pids, ep_link)
    LIST_FOREACH(es, &ep->ep_sections, es_link)
      es->es_keystate = ES_IDLE;
  ct->ecm_state = ECM_RESET;
  pthread_mutex_unlock(&cwc->cwc_mutex);
}

static void
handle_ecm_reply(cwc_service_t *ct, ecm_section_t *es, uint8_t *msg,
                 int len, int seq)
{
  mpegts_service_t *t = (mpegts_service_t *)ct->td_service;
  cwc_t *cwc = ct->cs_cwc;
  ecm_pid_t *ep;
  ecm_section_t *es2, es3;
  char chaninfo[128];
  int i;
  uint32_t off;
  int64_t delay = (getfastmonoclock() - es->es_time) / 1000LL; // in ms

  es->es_pending = 0;

  snprintf(chaninfo, sizeof(chaninfo), " (PID %d)", es->es_channel);

  if(len < 19) {

    /* ERROR */

    if (es->es_nok < CWC_MAX_NOKS)
      es->es_nok++;

    if(es->es_keystate == ES_FORBIDDEN)
      return; // We already know it's bad

    if (es->es_nok >= CWC_MAX_NOKS) {
      tvhdebug(LS_CWC,
              "Too many NOKs[%i] for service \"%s\"%s from %s",
              es->es_section, t->s_dvb_svcname, chaninfo, ct->td_nicename);
      es->es_keystate = ES_FORBIDDEN;
      goto forbid;
    }

    if (descrambler_resolved((service_t *)t, (th_descrambler_t *)ct)) {
      tvhdebug(LS_CWC,
              "NOK[%i] from %s: Already has a key for service \"%s\"",
               es->es_section, ct->td_nicename, t->s_dvb_svcname);
      es->es_nok = CWC_MAX_NOKS; /* do not send more ECM requests */
      es->es_keystate = ES_IDLE;
      if (ct->td_keystate == DS_READY)
        descrambler_change_keystate((th_descrambler_t *)ct, DS_IDLE, 1);
    }

    tvhdebug(LS_CWC,
             "Received NOK[%i] for service \"%s\"%s "
             "(seqno: %d Req delay: %"PRId64" ms)",
             es->es_section, t->s_dvb_svcname, chaninfo, seq, delay);

forbid:
    i = 0;
    LIST_FOREACH(ep, &ct->cs_pids, ep_link)
      LIST_FOREACH(es2, &ep->ep_sections, es_link)
        if(es2 && es2 != es && es2->es_nok == 0) {
          if (es2->es_pending)
            return;
          i++;
        }
    if (i && es->es_nok < CWC_MAX_NOKS)
      return;
    
    es->es_keystate = ES_FORBIDDEN;
    LIST_FOREACH(ep, &ct->cs_pids, ep_link) {
      LIST_FOREACH(es2, &ep->ep_sections, es_link)
        if (es2->es_keystate == ES_UNKNOWN ||
            es2->es_keystate == ES_RESOLVED)
          break;
      if (es2)
        break;
    }

    if (ep == NULL) { /* !UNKNOWN && !RESOLVED */
      tvherror(LS_CWC,
               "Can not descramble service \"%s\", access denied (seqno: %d "
               "Req delay: %"PRId64" ms) from %s",
               t->s_dvb_svcname, seq, delay, ct->td_nicename);
      descrambler_change_keystate((th_descrambler_t *)ct, DS_FORBIDDEN, 1);
      ct->ecm_state = ECM_RESET;
      /* this pid is not valid, force full scan */
      if (t->s_dvb_prefcapid == ct->cs_channel && t->s_dvb_prefcapid_lock == PREFCAPID_OFF)
        t->s_dvb_prefcapid = 0;
    }
    return;

  } else {

    es->es_nok = 0;
    ct->cs_channel = es->es_channel;
    ct->ecm_state = ECM_VALID;

    if(t->s_dvb_prefcapid == 0 ||
       (t->s_dvb_prefcapid != ct->cs_channel &&
        t->s_dvb_prefcapid_lock == PREFCAPID_OFF)) {
      t->s_dvb_prefcapid = ct->cs_channel;
      tvhdebug(LS_CWC, "Saving prefered PID %d for %s",
                               t->s_dvb_prefcapid, ct->td_nicename);
      service_request_save((service_t*)t, 0);
    }

    if(len < 35) {
      tvhdebug(LS_CWC,
               "Received ECM reply%s for service \"%s\" [%d] "
               "even: %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x"
               " odd: %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x (seqno: %d "
               "Req delay: %"PRId64" ms)",
               chaninfo,
               t->s_dvb_svcname, es->es_section,
               msg[3 + 0], msg[3 + 1], msg[3 + 2], msg[3 + 3], msg[3 + 4],
               msg[3 + 5], msg[3 + 6], msg[3 + 7], msg[3 + 8], msg[3 + 9],
               msg[3 + 10],msg[3 + 11],msg[3 + 12],msg[3 + 13],msg[3 + 14],
               msg[3 + 15], seq, delay);

      if(es->es_keystate != ES_RESOLVED)
        tvhdebug(LS_CWC,
                 "Obtained DES keys for service \"%s\" in %"PRId64" ms, from %s",
                 t->s_dvb_svcname, delay, ct->td_nicename);
      es->es_keystate = ES_RESOLVED;
      es->es_resolved = 1;
      off = 8;
    } else {
      tvhdebug(LS_CWC,
           "Received ECM reply%s for service \"%s\" "
           "even: %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x"
           " odd: %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x"
           "(seqno: %d Req delay: %"PRId64" ms)",
           chaninfo,
           t->s_dvb_svcname,
           msg[3 + 0], msg[3 + 1], msg[3 + 2], msg[3 + 3], msg[3 + 4],
           msg[3 + 5], msg[3 + 6], msg[3 + 7], msg[3 + 8], msg[3 + 9],
           msg[3 + 10],msg[3 + 11],msg[3 + 12],msg[3 + 13],msg[3 + 14],
           msg[3 + 15],msg[3 + 16],msg[3 + 17],msg[3 + 18],msg[3 + 19],
           msg[3 + 20],msg[3 + 21],msg[3 + 22],msg[3 + 23],msg[3 + 24],
           msg[3 + 25],msg[3 + 26],msg[3 + 27],msg[3 + 28],msg[3 + 29],
           msg[3 + 30],msg[3 + 31], seq, delay);

      if(es->es_keystate != ES_RESOLVED)
        tvhdebug(LS_CWC,
                 "Obtained AES keys for service \"%s\" in %"PRId64" ms, from %s",
                 t->s_dvb_svcname, delay, ct->td_nicename);
      es->es_keystate = ES_RESOLVED;
      es->es_resolved = 1;
      off = 16;
    }

    es3 = *es;
    pthread_mutex_unlock(&cwc->cwc_mutex);
    descrambler_keys((th_descrambler_t *)ct,
                     off == 16 ? DESCRAMBLER_AES128_ECB : DESCRAMBLER_CSA_CBC,
                     0, msg + 3, msg + 3 + off);
    snprintf(chaninfo, sizeof(chaninfo), "%s:%i", cwc->cwc_hostname, cwc->cwc_port);
    descrambler_notify((th_descrambler_t *)ct,
                       es3.es_caid, es3.es_provid,
                       caid2name(es3.es_caid),
                       es3.es_channel, delay,
                       1, "", chaninfo, "newcamd");
    pthread_mutex_lock(&cwc->cwc_mutex);
  }
}


/**
 * Handle running reply
 * cwc_mutex is held
 */
static int
cwc_running_reply(cwc_t *cwc, uint8_t msgtype, uint8_t *msg, int len)
{
  cwc_service_t *ct;
  ecm_pid_t *ep;
  ecm_section_t *es;
  uint16_t seq = (msg[2] << 8) | msg[3];
  int plen;
  short caid;
  uint8_t **u8;

  switch(msgtype) {
    case 0x80:
    case 0x81:
      len -= 12;
      msg += 12;
      LIST_FOREACH(ct, &cwc->cwc_services, cs_link)
        LIST_FOREACH(ep, &ct->cs_pids, ep_link)
          LIST_FOREACH(es, &ep->ep_sections, es_link)
            if(es->es_seq == seq) {
              if (es->es_resolved) {
                mpegts_service_t *t = (mpegts_service_t *)ct->td_service;
                tvhdebug(LS_CWC,
                         "Ignore %sECM (PID %d) for service \"%s\" from %s (seq %i)",
                         es->es_pending ? "duplicate " : "",
                         ep->ep_pid, t->s_dvb_svcname, ct->td_nicename, es->es_seq);
                return 0;
              }
              if (es->es_pending) {
                handle_ecm_reply(ct, es, msg, len, seq);
                return 0;
              }
            }
      tvhwarn(LS_CWC, "Got unexpected ECM reply (seqno: %d)", seq);
      break;

    case 0xD3:
      caid = (msg[6] << 8) | msg[7];

      if (caid){
        if(len < 3) {
          tvhinfo(LS_CWC, "Invalid card data reply");
          return -1;
        }

        plen = (msg[1] & 0xf) << 8 | msg[2];

        if(plen < 14) {
          tvhinfo(LS_CWC, "Invalid card data reply (message)");
          return -1;
        }

        caclient_set_status((caclient_t *)cwc, CACLIENT_STATUS_CONNECTED);
        u8 = alloca(sizeof(uint8_t *));
        u8[0] = &msg[8];
        cwc_new_card(cwc, caid, NULL, 1, u8, NULL);
      }
  }
  return 0;
}

/**
 *
 */
static int
cwc_must_break(cwc_t *cwc)
{
  return !cwc->cwc_running || !cwc->cac_enabled || cwc->cwc_reconfigure;
}

/**
 *
 */
static int
cwc_read(cwc_t *cwc, void *buf, size_t len, int timeout)
{
  int r;

  pthread_mutex_unlock(&cwc->cwc_mutex);
  r = tcp_read_timeout(cwc->cwc_fd, buf, len, timeout);
  pthread_mutex_lock(&cwc->cwc_mutex);

  if (r && tvheadend_is_running())
    tvhwarn(LS_CWC, "read error %d (%s)", r, strerror(r));

  if(cwc_must_break(cwc))
    return ECONNABORTED;

  return r;
}


/**
 *
 */
static int
cwc_read_message(cwc_t *cwc, const char *state, int timeout)
{
  char buf[2];
  int msglen, r;

  if((r = cwc_read(cwc, buf, 2, timeout))) {
    if (tvheadend_is_running())
      tvhinfo(LS_CWC, "%s:%i: %s: Read error (header): %s",
              cwc->cwc_hostname, cwc->cwc_port, state, strerror(r));
    return -1;
  }

  msglen = (buf[0] << 8) | buf[1];
  if(msglen >= CWS_NETMSGSIZE) {
    if (tvheadend_is_running())
      tvhinfo(LS_CWC, "%s:%i: %s: Invalid message size: %d",
              cwc->cwc_hostname, cwc->cwc_port, state, msglen);
    return -1;
  }

  /* We expect the rest of the message to arrive fairly quick,
     so just wait 1 second here */

  if((r = cwc_read(cwc, cwc->cwc_buf + 2, msglen, 1000))) {
    if (tvheadend_is_running())
      tvhinfo(LS_CWC, "%s:%i: %s: Read error: %s",
              cwc->cwc_hostname, cwc->cwc_port, state, strerror(r));
    return -1;
  }

  if((msglen = des_decrypt(cwc->cwc_buf, msglen + 2, cwc)) < 15) {
    tvhinfo(LS_CWC, "%s:%i: %s: Decrypt failed",
            cwc->cwc_hostname, cwc->cwc_port, state);
    return -1;
  }
  return msglen;
}

/**
 *
 */
static void
cwc_invalidate_cards(cwc_t *cwc)
{
  cs_card_data_t *cd;

  LIST_FOREACH(cd, &cwc->cwc_cards, cs_card)
    cd->running = 0;
}

/**
 *
 */
static void
cwc_free_cards(cwc_t *cwc)
{
  cs_card_data_t *cd;

  while((cd = LIST_FIRST(&cwc->cwc_cards)) != NULL) {
    LIST_REMOVE(cd, cs_card);
    descrambler_close_emm(cd->cwc_mux, cd, cd->cs_ra.caid);
    emm_reass_done(&cd->cs_ra);
    free(cd);
  }
}

/**
 *
 */
static void
cwc_flush_services(cwc_t *cwc)
{
  cwc_service_t *ct;

  LIST_FOREACH(ct, &cwc->cwc_services, cs_link)
    descrambler_flush_table_data(ct->td_service);
}

/**
 *
 */
static void *
cwc_writer_thread(void *aux)
{
  cwc_t *cwc = aux;
  cwc_message_t *cm;
  int64_t mono;
  int r;

  pthread_mutex_lock(&cwc->cwc_writer_mutex);

  while(cwc->cwc_writer_running) {

    if((cm = TAILQ_FIRST(&cwc->cwc_writeq)) != NULL) {
      TAILQ_REMOVE(&cwc->cwc_writeq, cm, cm_link);
      pthread_mutex_unlock(&cwc->cwc_writer_mutex);
      //      int64_t ts = getfastmonoclock();
      if (tvh_write(cwc->cwc_fd, cm->cm_data, cm->cm_len))
        tvhinfo(LS_CWC, "write error %s", strerror(errno));
      //      printf("Write took %lld usec\n", getfastmonoclock() - ts);
      free(cm);
      pthread_mutex_lock(&cwc->cwc_writer_mutex);
      continue;
    }


    /* If nothing is to be sent in keepalive interval seconds we
       need to send a keepalive */
    mono = mclk() + sec2mono(cwc->cwc_keepalive_interval);
    do {
      r = tvh_cond_timedwait(&cwc->cwc_writer_cond, &cwc->cwc_writer_mutex, mono);
      if(r == ETIMEDOUT) {
        cwc_send_ka(cwc);
        break;
      }
    } while (ERRNO_AGAIN(r));
  }

  pthread_mutex_unlock(&cwc->cwc_writer_mutex);
  return NULL;
}



/**
 *
 */
static void
cwc_session(cwc_t *cwc)
{
  int r;
  pthread_t writer_thread_id;

  /**
   * Get login key
   */
  if((r = cwc_read(cwc, cwc->cwc_buf, 14, 5000))) {
    tvhinfo(LS_CWC, "%s:%i: No login key received: %s",
            cwc->cwc_hostname, cwc->cwc_port, strerror(r));
    return;
  }

  des_make_login_key(cwc, cwc->cwc_buf);

  /**
   * Login
   */
  if (cwc_send_login(cwc))
    return;
  
  if(cwc_read_message(cwc, "Wait login response", 5000) < 0)
    return;

  if(cwc->cwc_buf[12] != MSG_CLIENT_2_SERVER_LOGIN_ACK) {
    tvhinfo(LS_CWC, "%s:%i: Login failed",
            cwc->cwc_hostname, cwc->cwc_port);
    return;
  }

  des_make_session_key(cwc);

  /**
   * Request card data
   */
  cwc_send_data_req(cwc);
  if((r = cwc_read_message(cwc, "Request card data", 5000)) < 0)
    return;

  if(cwc->cwc_buf[12] != MSG_CARD_DATA) {
    tvhinfo(LS_CWC, "%s:%i: Card data request failed",
            cwc->cwc_hostname, cwc->cwc_port);
    return;
  }

  if(cwc_decode_card_data_reply(cwc, cwc->cwc_buf, r) < 0)
    return;

  /**
   * Ok, connection good, reset retry delay to zero
   */
  cwc->cwc_retry_delay = 0;

  cwc_flush_services(cwc);

  /**
   * We do all requests from now on in a separate thread
   */
  cwc->cwc_writer_running = 1;
  tvh_cond_init(&cwc->cwc_writer_cond);
  pthread_mutex_init(&cwc->cwc_writer_mutex, NULL);
  TAILQ_INIT(&cwc->cwc_writeq);
  tvhthread_create(&writer_thread_id, NULL, cwc_writer_thread, cwc, "cwc-writer");

  /**
   * Mainloop
   */
  while(!cwc_must_break(cwc)) {

    if((r = cwc_read_message(cwc, "Decoderloop", 
                             cwc->cwc_keepalive_interval * 2 * 1000)) < 0)
      break;
    cwc_running_reply(cwc, cwc->cwc_buf[12], cwc->cwc_buf, r);
  }
  tvhdebug(LS_CWC, "session thread exiting");

  /**
   * Collect the writer thread
   */
  shutdown(cwc->cwc_fd, SHUT_RDWR);
  cwc->cwc_writer_running = 0;
  tvh_cond_signal(&cwc->cwc_writer_cond, 0);
  pthread_join(writer_thread_id, NULL);
  tvhdebug(LS_CWC, "Write thread joined");
}

/**
 *
 */
static void *
cwc_thread(void *aux)
{
  cwc_t *cwc = aux;
  int fd, d, r;
  char errbuf[100];
  char hostname[256];
  int port;
  int attempts = 0;
  int64_t mono;

  pthread_mutex_lock(&cwc->cwc_mutex);

  while(cwc->cwc_running) {

    cwc_invalidate_cards(cwc);
    caclient_set_status((caclient_t *)cwc, CACLIENT_STATUS_READY);
    
    snprintf(hostname, sizeof(hostname), "%s", cwc->cwc_hostname);
    port = cwc->cwc_port;

    tvhinfo(LS_CWC, "Attemping to connect to %s:%d", hostname, port);

    pthread_mutex_unlock(&cwc->cwc_mutex);

    fd = tcp_connect(hostname, port, NULL, errbuf, sizeof(errbuf), 10);

    pthread_mutex_lock(&cwc->cwc_mutex);

    if(fd == -1) {
      attempts++;
      tvhinfo(LS_CWC, 
              "Connection attempt to %s:%d failed: %s",
              hostname, port, errbuf);
    } else {

      if(cwc->cwc_running == 0) {
        close(fd);
        break;
      }

      tvhinfo(LS_CWC, "Connected to %s:%d", hostname, port);
      attempts = 0;

      cwc->cwc_fd = fd;
      cwc->cwc_reconfigure = 0;

      cwc_session(cwc);

      cwc->cwc_fd = -1;
      close(fd);
      tvhinfo(LS_CWC, "Disconnected from %s:%i", 
              cwc->cwc_hostname, cwc->cwc_port);
    }

    if(cwc->cwc_running == 0) continue;
    if(attempts == 1 || cwc->cwc_reconfigure) {
      cwc->cwc_reconfigure = 0;
      continue; // Retry immediately
    }

    caclient_set_status((caclient_t *)cwc, CACLIENT_STATUS_DISCONNECTED);

    d = 3;

    tvhinfo(LS_CWC, 
            "%s:%i: Automatic connection attempt in %d seconds",
            cwc->cwc_hostname, cwc->cwc_port, d-1);

    mono = mclk() + sec2mono(d);
    do {
      r = tvh_cond_timedwait(&cwc->cwc_cond, &cwc->cwc_mutex, mono);
      if (r == ETIMEDOUT)
        break;
    } while (ERRNO_AGAIN(r));
  }

  tvhinfo(LS_CWC, "%s:%i inactive",
          cwc->cwc_hostname, cwc->cwc_port);
  cwc_free_cards(cwc);
  pthread_mutex_unlock(&cwc->cwc_mutex);
  return NULL;
}


/**
 *
 */
static int
verify_provider(cs_card_data_t *pcard, uint32_t providerid)
{
  int i;

  if(providerid == 0)
    return 1;
  
  for(i = 0; i < pcard->cs_ra.providers_count; i++)
    if(providerid == pcard->cs_ra.providers[i].id)
      return 1;
  return 0;
}

/**
 *
 */
static void
cwc_emm_send(void *aux, const uint8_t *radata, int ralen, void *mux)
{
  cs_card_data_t *pcard = aux;
  cwc_t *cwc = pcard->cwc;

  tvhtrace(LS_CWC, "sending EMM for %04x mux %p", pcard->cs_ra.caid, mux);
  tvhlog_hexdump(LS_CWC, radata, ralen);
  cwc_send_msg(cwc, radata, ralen, 0, 1, 0, 0);
}

/**
 *
 */
static void
cwc_emm(void *opaque, int pid, const uint8_t *data, int len, int emm)
{
  cs_card_data_t *pcard = opaque;
  cwc_t *cwc;
  void *mux;

  if (data == NULL) {  /* end-of-data */
    pcard->cwc_mux = NULL;
    return;
  }
  if (pcard->cwc_mux == NULL)
    return;
  cwc = pcard->cwc;
  pthread_mutex_lock(&cwc->cwc_mutex);
  mux = pcard->cwc_mux;
  if (pcard->running && cwc->cwc_forward_emm && cwc->cwc_writer_running) {
    if (cwc->cwc_emmex) {
      if (cwc->cwc_emm_mux && cwc->cwc_emm_mux != mux) {
        if (cwc->cwc_emm_update_time + sec2mono(25) > mclk())
          goto end_of_job;
      }
      cwc->cwc_emm_update_time = mclk();
    }
    cwc->cwc_emm_mux = mux;
    emm_filter(&pcard->cs_ra, data, len, mux, cwc_emm_send, pcard);
  }
end_of_job:
  pthread_mutex_unlock(&cwc->cwc_mutex);
}

/**
 *
 */
static void
cwc_table_input(void *opaque, int pid, const uint8_t *data, int len, int emm)
{
  cwc_service_t *ct = opaque;
  elementary_stream_t *st;
  mpegts_service_t *t = (mpegts_service_t*)ct->td_service;
  uint16_t sid = t->s_dvb_service_id;
  cwc_t *cwc = ct->cs_cwc;
  int channel;
  int section;
  ecm_pid_t *ep;
  ecm_section_t *es;
  char chaninfo[32];
  cs_card_data_t *pcard = NULL;
  caid_t *c;
  uint16_t caid;
  uint32_t provid;

  if (data == NULL)
    return;

  if(len > 4096)
    return;

  if((data[0] & 0xf0) != 0x80)
    return;

  pthread_mutex_lock(&cwc->cwc_mutex);
  pthread_mutex_lock(&t->s_stream_mutex);

  if (ct->td_keystate == DS_IDLE)
    goto end;

  if (ct->ecm_state == ECM_RESET) {
    /* clean all */
    cwc_service_pid_free(ct);
    /* move to init state */
    ct->ecm_state = ECM_INIT;
    ct->cs_channel = -1;
    t->s_dvb_prefcapid = 0;
    tvhdebug(LS_CWC, "Reset after unexpected or no reply for service \"%s\"", t->s_dvb_svcname);
  }

  LIST_FOREACH(ep, &ct->cs_pids, ep_link)
    if(ep->ep_pid == pid) break;

  if(ep == NULL) {
    if (ct->ecm_state == ECM_INIT) {
      // Validate prefered ECM PID
      tvhdebug(LS_CWC, "ECM state INIT");

      if(t->s_dvb_prefcapid_lock != PREFCAPID_OFF) {
        st = service_stream_find((service_t*)t, t->s_dvb_prefcapid);
        if (st && st->es_type == SCT_CA)
          LIST_FOREACH(c, &st->es_caids, link)
            LIST_FOREACH(pcard, &cwc->cwc_cards, cs_card)
              if(pcard->running &&
                 pcard->cs_ra.caid == c->caid &&
                 verify_provider(pcard, c->providerid))
                goto prefcapid_ok;
        tvhdebug(LS_CWC, "Invalid prefered ECM (PID %d) found for service \"%s\"", t->s_dvb_prefcapid, t->s_dvb_svcname);
        t->s_dvb_prefcapid = 0;
      }

prefcapid_ok:
      if(t->s_dvb_prefcapid == pid || t->s_dvb_prefcapid == 0 ||
         t->s_dvb_prefcapid_lock == PREFCAPID_OFF) {
        ep = calloc(1, sizeof(ecm_pid_t));
        ep->ep_pid = pid;
        LIST_INSERT_HEAD(&ct->cs_pids, ep, ep_link);
        tvhdebug(LS_CWC, "Insert %s ECM (PID %d) for service \"%s\"",
                 t->s_dvb_prefcapid ? "preferred" : "new", pid, t->s_dvb_svcname);
      }
    }
    if(ep == NULL)
      goto end;
  }

  st = service_stream_find((service_t *)t, pid);
  if (st) {
    LIST_FOREACH(c, &st->es_caids, link)
      LIST_FOREACH(pcard, &cwc->cwc_cards, cs_card)
        if(pcard->running &&
           pcard->cs_ra.caid == c->caid &&
           verify_provider(pcard, c->providerid))
          goto found;
  }

  goto end;

found:
  caid = c->caid;
  provid = c->providerid;

  switch(data[0]) {
    case 0x80:
    case 0x81:
      /* ECM */
      
      if((pcard->cs_ra.caid >> 8) == 6) {
        ep->ep_last_section = data[5];
        section = data[4];
        
      } else {
        ep->ep_last_section = 0;
        section = 0;
      }

      channel = pid;
      snprintf(chaninfo, sizeof(chaninfo), " (PID %d)", channel);

      LIST_FOREACH(es, &ep->ep_sections, es_link)
        if (es->es_section == section)
          break;
      if (es == NULL) {
        es = calloc(1, sizeof(ecm_section_t));
        es->es_section = section;
        LIST_INSERT_HEAD(&ep->ep_sections, es, es_link);
      }

      if(cwc->cwc_fd == -1) {
        // New key, but we are not connected (anymore), can not descramble
        descrambler_change_keystate((th_descrambler_t *)ct, DS_READY, 0);
        break;
      }

      if (es->es_keystate == ES_FORBIDDEN || es->es_keystate == ES_IDLE)
        break;

      es->es_caid = caid;
      es->es_provid = provid;
      es->es_channel = channel;
      es->es_pending = 1;
      es->es_resolved = 0;

      if(ct->cs_channel >= 0 && channel != -1 &&
         ct->cs_channel != channel) {
        tvhdebug(LS_CWC, "Filtering ECM (PID %d)", channel);
        goto end;
      }

      es->es_seq = cwc_send_msg(cwc, data, len, sid, 1, caid, provid);
      
      tvhdebug(LS_CWC,
               "Sending ECM%s section=%d/%d, for service \"%s\" (seqno: %d)",
               chaninfo, section, ep->ep_last_section, t->s_dvb_svcname, es->es_seq);
      es->es_time = getfastmonoclock();
      break;

    default:
      /* EMM */
      if (cwc->cwc_forward_emm)
        cwc_send_msg(cwc, data, len, sid, 1, 0, 0);
      break;
  }

end:
  pthread_mutex_unlock(&t->s_stream_mutex);
  pthread_mutex_unlock(&cwc->cwc_mutex);
}

/**
 * cwc_mutex is held
 */
static void 
cwc_service_pid_free(cwc_service_t *ct)
{
  ecm_pid_t *ep;
  ecm_section_t *es;

  while((ep = LIST_FIRST(&ct->cs_pids)) != NULL) {
    while ((es = LIST_FIRST(&ep->ep_sections)) != NULL) {
      LIST_REMOVE(es, es_link);
      free(es);
    }
    LIST_REMOVE(ep, ep_link);
    free(ep);
  }
}

/**
 * cwc_mutex is held
 */
static void
cwc_service_destroy0(th_descrambler_t *td)
{
  cwc_service_t *ct = (cwc_service_t *)td;
  int i;

  for (i = 0; i < CWC_ES_PIDS; i++)
    if (ct->cs_epids[i])
      descrambler_close_pid(ct->cs_mux, ct,
                            DESCRAMBLER_ECM_PID(ct->cs_epids[i]));

  cwc_service_pid_free(ct);

  LIST_REMOVE(td, td_service_link);

  LIST_REMOVE(ct, cs_link);

  free(ct->td_nicename);
  free(ct);
}

/**
 * cwc_mutex is held
 */
static void
cwc_service_destroy(th_descrambler_t *td)
{
  cwc_service_t *ct = (cwc_service_t *)td;
  cwc_t *cwc = ct->cs_cwc;

  pthread_mutex_lock(&cwc->cwc_mutex);
  cwc_service_destroy0(td);
  pthread_mutex_unlock(&cwc->cwc_mutex);
}

/**
 * Check if our CAID's matches, and if so, link
 *
 * global_lock is held. Not that we care about that, but either way, it is.
 */
static void
cwc_service_start(caclient_t *cac, service_t *t)
{
  cwc_t *cwc = (cwc_t *)cac;
  cwc_service_t *ct;
  th_descrambler_t *td;
  elementary_stream_t *st;
  caid_t *c;
  cs_card_data_t *pcard;
  char buf[512];
  int i, reuse = 0, prefpid, prefpid_lock, forcecaid;

  extern const idclass_t mpegts_service_class;
  if (!idnode_is_instance(&t->s_id, &mpegts_service_class))
    return;

  pthread_mutex_lock(&cwc->cwc_mutex);
  pthread_mutex_lock(&t->s_stream_mutex);
  LIST_FOREACH(ct, &cwc->cwc_services, cs_link) {
    if (ct->td_service == t && ct->cs_cwc == cwc)
      break;
  }
  prefpid      = ((mpegts_service_t *)t)->s_dvb_prefcapid;
  prefpid_lock = ((mpegts_service_t *)t)->s_dvb_prefcapid_lock;
  forcecaid    = ((mpegts_service_t *)t)->s_dvb_forcecaid;
  LIST_FOREACH(pcard, &cwc->cwc_cards, cs_card) {
    if (!pcard->running) continue;
    if (pcard->cs_ra.caid == 0) continue;
    TAILQ_FOREACH(st, &t->s_filt_components, es_filt_link) {
      if (prefpid_lock == PREFCAPID_FORCE && prefpid != st->es_pid)
        continue;
      LIST_FOREACH(c, &st->es_caids, link) {
        if (c->use && c->caid == pcard->cs_ra.caid)
          if (!forcecaid || forcecaid == c->caid)
            break;
      }
      if (c) break;
    }
    if (st) break;
  }
  if (!pcard) {
    if (ct) cwc_service_destroy0((th_descrambler_t*)ct);
    goto end;
  }
  if (ct) {
    reuse = 1;
    for (i = 0; i < CWC_ES_PIDS; i++) {
      if (!ct->cs_epids[i]) continue;
      TAILQ_FOREACH(st, &t->s_filt_components, es_filt_link) {
        if (st->es_pid != ct->cs_epids[i]) continue;
        LIST_FOREACH(c, &st->es_caids, link)
          if (c->use && c->caid == pcard->cs_ra.caid)
            break;
        if (c) break;
      }
      if (st == NULL) {
        descrambler_close_pid(ct->cs_mux, ct,
                              DESCRAMBLER_ECM_PID(ct->cs_epids[i]));
        reuse |= 2;
      }
    }
    goto add;
  }

  ct                   = calloc(1, sizeof(cwc_service_t));
  ct->cs_cwc           = cwc;
  ct->cs_channel       = -1;
  ct->cs_mux           = ((mpegts_service_t *)t)->s_dvb_mux;
  ct->ecm_state        = ECM_INIT;

  td                   = (th_descrambler_t *)ct;
  snprintf(buf, sizeof(buf), "cwc-%s-%i-%04X", cwc->cwc_hostname, cwc->cwc_port, pcard->cs_ra.caid);
  td->td_nicename      = strdup(buf);
  td->td_service       = t;
  td->td_stop          = cwc_service_destroy;
  td->td_ecm_reset     = cwc_ecm_reset;
  td->td_ecm_idle      = cwc_ecm_idle;
  LIST_INSERT_HEAD(&t->s_descramblers, td, td_service_link);

  LIST_INSERT_HEAD(&cwc->cwc_services, ct, cs_link);

  descrambler_change_keystate((th_descrambler_t *)td, DS_READY, 0);

add:
  i = 0;
  TAILQ_FOREACH(st, &t->s_filt_components, es_filt_link) {
    LIST_FOREACH(c, &st->es_caids, link)
      if (c->use && c->caid == pcard->cs_ra.caid) {
        if (reuse && ct->cs_epids[i] != st->es_pid) reuse |= 2;
        ct->cs_epids[i++] = st->es_pid;
        break;
      }
    if (i == CWC_ES_PIDS) break;
  }

  for (i = 0; i < CWC_ES_PIDS; i++)
    if (ct->cs_epids[i])
      descrambler_open_pid(ct->cs_mux, ct,
                           DESCRAMBLER_ECM_PID(ct->cs_epids[i]),
                           cwc_table_input, t);

  if (reuse & 2) {
    ct->cs_channel = -1;
    ct->ecm_state = ECM_INIT;
  }

  if (reuse != 1)
    tvhdebug(LS_CWC, "%s %susing CWC %s:%d",
             service_nicename(t), reuse ? "re" : "", cwc->cwc_hostname, cwc->cwc_port);

end:
  pthread_mutex_unlock(&t->s_stream_mutex);
  pthread_mutex_unlock(&cwc->cwc_mutex);
}

/**
 *
 */
static void
cwc_free(caclient_t *cac)
{
  cwc_t *cwc = (cwc_t *)cac;
  cwc_service_t *ct;

  while((ct = LIST_FIRST(&cwc->cwc_services)) != NULL)
    cwc_service_destroy((th_descrambler_t *)ct);

  cwc_free_cards(cwc);
  free((void *)cwc->cwc_password);
  free((void *)cwc->cwc_password_salted);
  free((void *)cwc->cwc_username);
  free((void *)cwc->cwc_hostname);
}

/**
 *
 */
static void
cwc_caid_update(caclient_t *cac, mpegts_mux_t *mux, uint16_t caid, uint16_t pid, int valid)
{
  cwc_t *cwc = (cwc_t *)cac;;
  cs_card_data_t *pcard;

  tvhtrace(LS_CWC,
           "caid update event - client %s mux %p caid %04x (%i) pid %04x (%i) valid %i",
           cac->cac_name, mux, caid, caid, pid, pid, valid);
  pthread_mutex_lock(&cwc->cwc_mutex);
  if (valid < 0 || cwc->cwc_running) {
    LIST_FOREACH(pcard, &cwc->cwc_cards, cs_card) {
      if (valid < 0 || pcard->cs_ra.caid == caid) {
        if (pcard->cwc_mux && pcard->cwc_mux != mux) continue;
        if (valid > 0) {
          pcard->cwc       = cwc;
          pcard->cwc_mux   = mux;
          descrambler_open_emm(mux, pcard, caid, cwc_emm);
        } else {
          pcard->cwc_mux   = NULL;
          descrambler_close_emm(mux, pcard, caid);
        }
      }
    }
  }
  pthread_mutex_unlock(&cwc->cwc_mutex);
}

/**
 *
 */
static int
nibble(char c)
{
  switch(c) {
  case '0' ... '9':
    return c - '0';
  case 'a' ... 'f':
    return c - 'a' + 10;
  case 'A' ... 'F':
    return c - 'A' + 10;
  default:
    return 0;
  }
}

/**
 *
 */
static void
cwc_conf_changed(caclient_t *cac)
{
  cwc_t *cwc = (cwc_t *)cac;
  pthread_t tid;

  free(cwc->cwc_password_salted);
  cwc->cwc_password_salted =
    cwc->cwc_password ? crypt_md5(cwc->cwc_password, "$1$abcdefgh$") : NULL;

  if (cac->cac_enabled) {
    if (cwc->cwc_hostname == NULL || cwc->cwc_hostname[0] == '\0') {
      caclient_set_status(cac, CACLIENT_STATUS_NONE);
      return;
    }
    if (!cwc->cwc_running) {
      cwc->cwc_running = 1;
      tvhthread_create(&cwc->cwc_tid, NULL, cwc_thread, cwc, "cwc");
      return;
    }
    pthread_mutex_lock(&cwc->cwc_mutex);
    cwc->cwc_reconfigure = 1;
    if(cwc->cwc_fd >= 0)
      shutdown(cwc->cwc_fd, SHUT_RDWR);
    tvh_cond_signal(&cwc->cwc_cond, 0);
    pthread_mutex_unlock(&cwc->cwc_mutex);
  } else {
    if (!cwc->cwc_running)
      return;
    pthread_mutex_lock(&cwc->cwc_mutex);
    cwc->cwc_running = 0;
    tvh_cond_signal(&cwc->cwc_cond, 0);
    tid = cwc->cwc_tid;
    if (cwc->cwc_fd >= 0)
      shutdown(cwc->cwc_fd, SHUT_RDWR);
    pthread_mutex_unlock(&cwc->cwc_mutex);
    pthread_kill(tid, SIGHUP);
    pthread_join(tid, NULL);
    caclient_set_status(cac, CACLIENT_STATUS_NONE);
  }

}

/**
 *
 */
static int
caclient_cwc_class_deskey_set(void *o, const void *v)
{
  cwc_t *cwc = o;
  const char *s = v ?: "";
  uint8_t key[14];
  int i, u, l;

  for(i = 0; i < ARRAY_SIZE(key); i++) {
    while(*s != 0 && !isxdigit(*s)) s++;
    u = *s ? nibble(*s++) : 0;
    while(*s != 0 && !isxdigit(*s)) s++;
    l = *s ? nibble(*s++) : 0;
    key[i] = (u << 4) | l;
  }
  i = memcmp(cwc->cwc_confedkey, key, ARRAY_SIZE(key)) != 0;
  memcpy(cwc->cwc_confedkey, key, ARRAY_SIZE(key));
  return i;
}

static const void *
caclient_cwc_class_deskey_get(void *o)
{
  cwc_t *cwc = o;
  snprintf(prop_sbuf, PROP_SBUF_LEN,
           "%02x:%02x:%02x:%02x:%02x:%02x:%02x:"
           "%02x:%02x:%02x:%02x:%02x:%02x:%02x",
           cwc->cwc_confedkey[0x0],
           cwc->cwc_confedkey[0x1],
           cwc->cwc_confedkey[0x2],
           cwc->cwc_confedkey[0x3],
           cwc->cwc_confedkey[0x4],
           cwc->cwc_confedkey[0x5],
           cwc->cwc_confedkey[0x6],
           cwc->cwc_confedkey[0x7],
           cwc->cwc_confedkey[0x8],
           cwc->cwc_confedkey[0x9],
           cwc->cwc_confedkey[0xa],
           cwc->cwc_confedkey[0xb],
           cwc->cwc_confedkey[0xc],
           cwc->cwc_confedkey[0xd]);
  return &prop_sbuf_ptr;
}

const idclass_t caclient_cwc_class =
{
  .ic_super      = &caclient_class,
  .ic_class      = "caclient_cwc",
  .ic_caption    = N_("Code Word Client (newcamd)"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "username",
      .name     = N_("Username"),
      .desc     = N_("Login username."),
      .off      = offsetof(cwc_t, cwc_username),
      .opts     = PO_TRIM,
    },
    {
      .type     = PT_STR,
      .id       = "password",
      .name     = N_("Password"),
      .desc     = N_("Login password."),
      .off      = offsetof(cwc_t, cwc_password),
      .opts     = PO_PASSWORD
    },
    {
      .type     = PT_STR,
      .id       = "hostname",
      .name     = N_("Hostname/IP"),
      .desc     = N_("Hostname (or IP) of the server."),
      .off      = offsetof(cwc_t, cwc_hostname),
      .def.s    = "localhost",
      .opts     = PO_TRIM,
    },
    {
      .type     = PT_INT,
      .id       = "port",
      .name     = N_("Port"),
      .desc     = N_("Port to connect to."),
      .off      = offsetof(cwc_t, cwc_port),
    },
    {
      .type     = PT_STR,
      .id       = "deskey",
      .name     = N_("DES key"),
      .desc     = N_("DES Key."),
      .set      = caclient_cwc_class_deskey_set,
      .get      = caclient_cwc_class_deskey_get,
      .opts     = PO_PASSWORD,
      .def.s    = "00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d",
    },
    {
      .type     = PT_BOOL,
      .id       = "emm",
      .name     = N_("Update card (EMM)"),
      .desc     = N_("Enable/disable offering of Entitlement Management Message updates."),
      .off      = offsetof(cwc_t, cwc_emm),
      .def.i    = 1
    },
    {
      .type     = PT_BOOL,
      .id       = "emmex",
      .name     = N_("Updates from one mux (EMM)"),
      .desc     = N_("Update Entitlement Management Messages from one mux only."),
      .off      = offsetof(cwc_t, cwc_emmex),
      .def.i    = 1
    },
    {
      .type     = PT_INT,
      .id       = "keepalive_interval",
      .name     = N_("Keepalive interval"),
      .desc     = N_("Keepalive interval in seconds"),
      .off      = offsetof(cwc_t, cwc_keepalive_interval),
      .def.i    = CWC_KEEPALIVE_INTERVAL,
    },
    { }
  }
};

/*
 *
 */
caclient_t *cwc_create(void)
{
  cwc_t *cwc = calloc(1, sizeof(*cwc));

  pthread_mutex_init(&cwc->cwc_mutex, NULL);
  tvh_cond_init(&cwc->cwc_cond);
  cwc->cac_free         = cwc_free;
  cwc->cac_start        = cwc_service_start;
  cwc->cac_conf_changed = cwc_conf_changed;
  cwc->cac_caid_update  = cwc_caid_update;
  cwc->cwc_keepalive_interval = CWC_KEEPALIVE_INTERVAL;
  return (caclient_t *)cwc;
}

/*
 *
 */

#include <openssl/md5.h>

/*
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 */

static unsigned char itoa64[] = /* 0 ... 63 => ascii - 64 */
        "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

/* to64 BUFFER VALUE NUM
 * Write NUM base64 characters of VALUE into BUFFER. */
static void to64(char *s, unsigned long v, int n)
{
    while (--n >= 0) {
        *s++ = itoa64[v&0x3f];
        v >>= 6;
    }
}

/* crypt_md5 PASSWORD SALT
 * Poul-Henning Kamp's crypt(3)-alike using MD5. */
static char *
crypt_md5(const char *pw, const char *salt)
{
    const char *magic = "$1$";
    /* This string is magic for this algorithm.  Having
     * it this way, we can get get better later on */
    char *p;
    const char *sp,*ep;
    unsigned char   final[16];
    int sl,pl,i,j;
    MD5_CTX ctx,ctx1;
    unsigned long l;

    /* Refine the Salt first */
    sp = salt;

    /* If it starts with the magic string, then skip that */
    if(!strncmp(sp,magic,strlen(magic)))
        sp += strlen(magic);

    /* It stops at the first '$', max 8 chars */
    for(ep=sp;*ep && *ep != '$' && ep < (sp+8);ep++)
        continue;

    /* get the length of the true salt */
    sl = ep - sp;

    MD5_Init(&ctx);

    /* The password first, since that is what is most unknown */
    MD5_Update(&ctx,(unsigned char *)pw,strlen(pw));

    /* Then our magic string */
    MD5_Update(&ctx,(unsigned char *)magic,strlen(magic));

    /* Then the raw salt */
    MD5_Update(&ctx,(unsigned char *)sp,sl);

    /* Then just as many characters of the MD5_(pw,salt,pw) */
    MD5_Init(&ctx1);
    MD5_Update(&ctx1,(unsigned char *)pw,strlen(pw));
    MD5_Update(&ctx1,(unsigned char *)sp,sl);
    MD5_Update(&ctx1,(unsigned char *)pw,strlen(pw));
    MD5_Final(final,&ctx1);
    for(pl = strlen(pw); pl > 0; pl -= 16)
        MD5_Update(&ctx,(unsigned char *)final,pl>16 ? 16 : pl);

    /* Don't leave anything around in vm they could use. */
    memset(final,0,sizeof final);

    /* Then something really weird... */
    for (j=0,i = strlen(pw); i ; i >>= 1)
        if(i&1)
            MD5_Update(&ctx, (unsigned char *)final+j, 1);
        else
            MD5_Update(&ctx, (unsigned char *)pw+j, 1);

    /* Now make the output string */
    char *passwd = malloc(120);

    strcpy(passwd,magic);
    strncat(passwd,sp,sl);
    strcat(passwd,"$");

    MD5_Final(final,&ctx);

    /*
     * and now, just to make sure things don't run too fast
     * On a 60 Mhz Pentium this takes 34 msec, so you would
     * need 30 seconds to build a 1000 entry dictionary...
     */
    for(i=0;i<1000;i++) {
        MD5_Init(&ctx1);
        if(i & 1)
            MD5_Update(&ctx1,(unsigned char *)pw,strlen(pw));
        else
            MD5_Update(&ctx1,(unsigned char *)final,16);

        if(i % 3)
            MD5_Update(&ctx1,(unsigned char *)sp,sl);

        if(i % 7)
            MD5_Update(&ctx1,(unsigned char *)pw,strlen(pw));

        if(i & 1)
            MD5_Update(&ctx1,(unsigned char *)final,16);
        else
            MD5_Update(&ctx1,(unsigned char *)pw,strlen(pw));
        MD5_Final(final,&ctx1);
    }

    p = passwd + strlen(passwd);

    l = (final[ 0]<<16) | (final[ 6]<<8) | final[12]; to64(p,l,4); p += 4;
    l = (final[ 1]<<16) | (final[ 7]<<8) | final[13]; to64(p,l,4); p += 4;
    l = (final[ 2]<<16) | (final[ 8]<<8) | final[14]; to64(p,l,4); p += 4;
    l = (final[ 3]<<16) | (final[ 9]<<8) | final[15]; to64(p,l,4); p += 4;
    l = (final[ 4]<<16) | (final[10]<<8) | final[ 5]; to64(p,l,4); p += 4;
    l =                    final[11]                ; to64(p,l,2); p += 2;
    *p = '\0';

    /* Don't leave anything around in vm they could use. */
    memset(final,0,sizeof final);

    return passwd;
}
