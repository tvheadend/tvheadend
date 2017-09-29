  /*
 *  tvheadend, CCCAM interface
 *  Copyright (C) 2007 Andreas Öman
 *  Copyright (C) 2017 Luis Alves
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
#include <openssl/sha.h>
#include <endian.h>
#include <semaphore.h>

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
#define CCCAM_KEEPALIVE_INTERVAL  0
#define CCCAM_ES_PIDS             8
#define CCCAM_MAX_NOKS            3
#define CCCAM_NETMSGSIZE          0x400


typedef enum {
  MSG_CLI_DATA,         // client -> server
  MSG_ECM_REQUEST,      // client -> server
  MSG_EMM_REQUEST,      // client -> server
  MSG_CARD_REMOVED = 4, // server -> client
  MSG_CMD_05,
  MSG_KEEPALIVE,        // client -> server
  MSG_NEW_CARD,         // server -> client
  MSG_SRV_DATA,         // server -> client
  MSG_CMD_0A = 0x0a,
  MSG_CMD_0B = 0x0b,
  MSG_CMD_0C = 0x0c,    // CCcam 2.2.x fake client checks
  MSG_CMD_0D = 0x0d,    // "
  MSG_CMD_0E = 0x0e,    // "
  MSG_NEW_CARD_SIDINFO = 0x0f,
  MSG_SLEEPSEND = 0x80, // Sleepsend support
  MSG_ECM_NOK1 = 0xfe,  // server -> client ecm queue full, card not found
  MSG_ECM_NOK2 = 0xff,  // server -> client
  MSG_NO_HEADER = 0xffff
} cccam_msg_type_t;

typedef enum {
  CCCAM_VERSION_2_0_11 = 0,
  CCCAM_VERSION_2_1_1,
  CCCAM_VERSION_2_1_2,
  CCCAM_VERSION_2_1_3,
  CCCAM_VERSION_2_1_4,
  CCCAM_VERSION_2_2_0,
  CCCAM_VERSION_2_2_1,
  CCCAM_VERSION_2_3_0,
  CCCAM_VERSION_COUNT,
} cccam_version_t;

static const char *cccam_version_str[CCCAM_VERSION_COUNT] = {
  "2.0.11", "2.1.1", "2.1.2", "2.1.3",
  "2.1.4", "2.2.0", "2.2.1", "2.3.0"
};

/**
 *
 */
//TAILQ_HEAD(cccam_queue, cccam);
LIST_HEAD(cccam_cards_list, cs_card_data);
LIST_HEAD(cccam_service_list, cccam_service);
TAILQ_HEAD(cccam_message_queue, cccam_message);
//LIST_HEAD(ecm_section_list, ecm_section);

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

  uint8_t es_seq;
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
typedef struct cccam_service {
  th_descrambler_t;

  struct cccam *cs_cccam;

  LIST_ENTRY(cccam_service) cs_link;

  int cs_channel;
  int cs_epids[CCCAM_ES_PIDS];
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

} cccam_service_t;


/**
 *
 */
typedef struct cccam_message {
  TAILQ_ENTRY(cccam_message) cm_link;
  int cm_len;
  int seq;
  int card_id;
  uint8_t cm_data[CCCAM_NETMSGSIZE];
} cccam_message_t;


struct cccam;

typedef struct cs_card_data {

  LIST_ENTRY(cs_card_data) cs_card;

  emm_reass_t cs_ra;

  struct cccam *cccam;
  mpegts_mux_t *cccam_mux;
  int running;

  uint32_t id;
  uint32_t remote_id;
  uint8_t hop;
  uint8_t reshare;

} cs_card_data_t;

/**
 *
 */
struct cccam_crypt_block {
  uint8_t keytable[256];
  uint8_t state;
  uint8_t counter;
  uint8_t sum;
};


/**
 *
 */
typedef struct cccam {
  caclient_t;

  int cccam_fd;

  pthread_t cccam_tid;
  tvh_cond_t cccam_cond;

  pthread_mutex_t cccam_mutex;
  pthread_mutex_t cccam_writer_mutex;
  tvh_cond_t cccam_writer_cond;
  int cccam_writer_running;
  struct cccam_message_queue cccam_writeq;

  struct cccam_service_list cccam_services;

  struct cccam_cards_list cccam_cards;
  int cccam_seq;

#if 0
  /* Emm forwarding */
  int cccam_forward_emm;
#endif

  /* Emm exclusive update */
  int64_t cccam_emm_update_time;
  void *cccam_emm_mux;


  /* From configuration */
  char *cccam_username;
  char *cccam_password;
  char *cccam_hostname;
  uint8_t cccam_nodeid[8];

  int cccam_port;
  int cccam_emm;
  int cccam_emmex;
  int cccam_version;
  int cccam_keepalive_interval;

  int cccam_running;
  int cccam_reconfigure;

  struct cccam_crypt_block sendblock;
  struct cccam_crypt_block recvblock;

  sem_t ecm_mutex;

  int seq;
  uint32_t card_id;

} cccam_t;


const uint8_t cccam_str[] = "CCcam";


/**
 *
 */
static inline void
uint8_swap(uint8_t *p1, uint8_t *p2)
{
  uint8_t tmp = *p1; *p1 = *p2; *p2 = tmp;
}

/**
 *
 */
static void
cccam_crypt_xor(uint8_t *buf)
{
  uint8_t i;

  for (i = 0; i < 8; i++) {
    buf[i + 8] = i * buf[i];
    if (i <= 5) {
      buf[i] ^= cccam_str[i];
    }
  }
}

/**
 *
 */
static void
cccam_crypt_init(struct cccam_crypt_block *block, uint8_t *key, int32_t len)
{
  uint32_t i = 0;
  uint8_t j = 0;

  for (i = 0; i < 256; i++) {
    block->keytable[i] = i;
  }
  for (i = 0; i < 256; i++) {
    j += key[i % len] + block->keytable[i];
    uint8_swap(&block->keytable[i], &block->keytable[j]);
  }
  block->state = *key;
  block->counter = 0;
  block->sum = 0;
}

/**
 *
 */
static void
cccam_decrypt(struct cccam_crypt_block *block, uint8_t *data, int32_t len)
{
  int32_t i;
  uint8_t z;

  for (i = 0; i < len; i++) {
    block->counter++;
    block->sum += block->keytable[block->counter];
    uint8_swap(&block->keytable[block->counter], &block->keytable[block->sum]);
    z = data[i];
    data[i] = z ^ block->keytable[(block->keytable[block->counter] +
              block->keytable[block->sum]) & 0xff] ^ block->state;
    z = data[i];
    block->state ^= z;
  }
}

/**
 *
 */
static void
cccam_encrypt(struct cccam_crypt_block *block, uint8_t *data, int32_t len)
{
  int32_t i;
  uint8_t z;
  for (i = 0; i < len; i++) {
    block->counter++;
    block->sum += block->keytable[block->counter];
    uint8_swap(&block->keytable[block->counter], &block->keytable[block->sum]);
    z = data[i];
    data[i] = z ^ block->keytable[(block->keytable[block->counter] +
              block->keytable[block->sum]) & 0xff] ^ block->state;
    block->state ^= z;
  }
}

static void
cccam_decrypt_cw(uint8_t *nodeid, uint32_t card_id, uint8_t *cws)
{
  uint8_t tmp, i;
  uint64_t node_id = be64toh(*((uint64_t *) nodeid));

  for (i = 0; i < 16; i++) {
    tmp = cws[i] ^ (node_id >> (4 * i));
    if (i & 1)
      tmp = ~tmp;
    cws[i] = (card_id >> (2 * i)) ^ tmp;
  }
}


static void cccam_service_pid_free(cccam_service_t *ct);


/**
 *
 */
static cs_card_data_t *
cccam_new_card(cccam_t *cccam, uint8_t *buf, uint8_t *ua,
             int pcount, uint8_t **pid, uint8_t **psa)
{
  cs_card_data_t *pcard = NULL;
  emm_provider_t *ep;
  const char *n;
  const uint8_t *id, *sa;
  int i, allocated = 0;

  uint16_t caid = (buf[12] << 8) | buf[13];

  LIST_FOREACH(pcard, &cccam->cccam_cards, cs_card)
    if (pcard->cs_ra.caid == caid)
      break;

  if (pcard == NULL) {
    pcard = calloc(1, sizeof(cs_card_data_t));
    emm_reass_init(&pcard->cs_ra, caid);
    allocated = 1;
  }

  pcard->id = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
  pcard->remote_id = (buf[8] << 24) | (buf[9] << 16) | (buf[10] << 8) | buf[11];
  pcard->hop = buf[14];
  pcard->reshare = buf[15];

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
    tvhinfo(LS_CCCAM, "%s:%i: Connected as user %s "
            "to a %s-card [0x%04x : %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x] "
            "with %d provider%s",
            cccam->cccam_hostname, cccam->cccam_port,
            cccam->cccam_username, n, caid,
            ua[0], ua[1], ua[2], ua[3], ua[4], ua[5], ua[6], ua[7],
            pcount, pcount > 1 ? "s" : "");
  } else {
    tvhinfo(LS_CCCAM, "%s:%i: Connected as user %s "
            "to a %s-card [0x%04x] id=%08x remoteid=%08x hop=%i reshare=%i with %d provider%s",
            cccam->cccam_hostname, cccam->cccam_port,
            cccam->cccam_username, n, caid,
            pcard->id, pcard->remote_id, pcard->hop, pcard->reshare,
            pcount, pcount > 1 ? "s" : "");
  }

  for (i = 0, ep = pcard->cs_ra.providers; i < pcount; i++, ep++) {
    if (psa) {
      sa = ep->sa;
      tvhinfo(LS_CCCAM, "%s:%i: Provider ID #%d: 0x%04x:0x%06x %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x",
              cccam->cccam_hostname, cccam->cccam_port, i + 1, caid, ep->id,
              sa[0], sa[1], sa[2], sa[3], sa[4], sa[5], sa[6], sa[7]);
    } else {
      tvhinfo(LS_CCCAM, "%s:%i: Provider ID #%d: 0x%04x:0x%06x",
              cccam->cccam_hostname, cccam->cccam_port, i + 1, caid, ep->id);
    }
  }

  pcard->running = 1;
  if (allocated)
    LIST_INSERT_HEAD(&cccam->cccam_cards, pcard, cs_card);
  return pcard;
}

/**
 * Handle reply to card data request
 */
static int
cccam_decode_card_data_reply(cccam_t *cccam, uint8_t *msg)
{
  /*int emm_allowed; */

  //cs_card_data_t *pcard;
  int i;
  unsigned int nprov;
  uint8_t **pid, **psa, *msg2; //, *ua

  /* nr of providers */
  nprov = msg[24];

  pid = nprov ? alloca(nprov * sizeof(uint8_t *)) : NULL;
  psa = nprov ? alloca(nprov * sizeof(uint8_t *)) : NULL;

  msg2 = msg + 25;
  for (i = 0; i < nprov; i++) {
    pid[i] = msg2;
    psa[i] = msg2 + 3;
    msg2 += 7;
  }

  caclient_set_status((caclient_t *)cccam, CACLIENT_STATUS_CONNECTED);
  cccam_new_card(cccam, msg, NULL, nprov, pid, NULL); /* TODO: psa? 4bytes cccam / 8bytes cwc */
/* TODO: EMM
  cccam->cccam_forward_emm = 0;
  if (cccam->cccam_emm) {
    ua = pcard->cs_ra.ua;
    emm_allowed = ua[0] || ua[1] || ua[2] || ua[3] ||
                  ua[4] || ua[5] || ua[6] || ua[7];

    if (!emm_allowed) {
      tvhinfo(LS_CCCAM,
              "%s:%i: Will not forward EMMs (not allowed by server)",
              cccam->cccam_hostname, cccam->cccam_port);
    } else if (pcard->cs_ra.type != CARD_UNKNOWN) {
      tvhinfo(LS_CCCAM, "%s:%i: Will forward EMMs",
              cccam->cccam_hostname, cccam->cccam_port);
      cccam->cccam_forward_emm = 1;
    } else {
      tvhinfo(LS_CCCAM,
             "%s:%i: Will not forward EMMs (unsupported CA system)",
              cccam->cccam_hostname, cccam->cccam_port);
    }
  }
*/
  return 0;
}


static int
cccam_ecm_reset(th_descrambler_t *th)
{
  cccam_service_t *ct = (cccam_service_t *)th;
  cccam_t *cccam = ct->cs_cccam;
  ecm_pid_t *ep;
  ecm_section_t *es;

  pthread_mutex_lock(&cccam->cccam_mutex);
  descrambler_change_keystate(th, DS_READY, 1);
  LIST_FOREACH(ep, &ct->cs_pids, ep_link)
    LIST_FOREACH(es, &ep->ep_sections, es_link)
      es->es_keystate = ES_UNKNOWN;
  ct->ecm_state = ECM_RESET;
  pthread_mutex_unlock(&cccam->cccam_mutex);
  return 0;
}


static void
cccam_ecm_idle(th_descrambler_t *th)
{
  cccam_service_t *ct = (cccam_service_t *)th;
  cccam_t *cccam = ct->cs_cccam;
  ecm_pid_t *ep;
  ecm_section_t *es;

  pthread_mutex_lock(&cccam->cccam_mutex);
  LIST_FOREACH(ep, &ct->cs_pids, ep_link)
    LIST_FOREACH(es, &ep->ep_sections, es_link)
      es->es_keystate = ES_IDLE;
  ct->ecm_state = ECM_RESET;
  pthread_mutex_unlock(&cccam->cccam_mutex);
}


static void
handle_ecm_reply(cccam_service_t *ct, ecm_section_t *es, uint8_t *msg,
                 int len, int seq, uint8_t *dcw)
{
  mpegts_service_t *t = (mpegts_service_t *)ct->td_service;
  cccam_t *cccam = ct->cs_cccam;
  ecm_pid_t *ep;
  ecm_section_t *es2, es3;
  char chaninfo[128];
  int i;
  uint32_t off;
  int64_t delay = (getfastmonoclock() - es->es_time) / 1000LL; // in ms

  uint8_t msg_type = msg[1];

  es->es_pending = 0;

  snprintf(chaninfo, sizeof(chaninfo), " (PID %d)", es->es_channel);

  if (msg_type != MSG_ECM_REQUEST) {

    /* ERROR */
    if (es->es_nok < CCCAM_MAX_NOKS)
      es->es_nok++;

    if(es->es_keystate == ES_FORBIDDEN)
      return; // We already know it's bad

    if (es->es_nok >= CCCAM_MAX_NOKS) {
      tvhdebug(LS_CCCAM,
              "Too many NOKs[%i] for service \"%s\"%s from %s",
              es->es_section, t->s_dvb_svcname, chaninfo, ct->td_nicename);
      es->es_keystate = ES_FORBIDDEN;
      goto forbid;
    }

    if (descrambler_resolved((service_t *)t, (th_descrambler_t *)ct)) {
      tvhdebug(LS_CCCAM,
              "NOK[%i] from %s: Already has a key for service \"%s\"",
               es->es_section, ct->td_nicename, t->s_dvb_svcname);
      es->es_nok = CCCAM_MAX_NOKS; /* do not send more ECM requests */
      es->es_keystate = ES_IDLE;
      if (ct->td_keystate == DS_READY)
        descrambler_change_keystate((th_descrambler_t *)ct, DS_IDLE, 1);
    }

    tvhdebug(LS_CCCAM,
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
    if (i && es->es_nok < CCCAM_MAX_NOKS)
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
      tvherror(LS_CCCAM,
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
      tvhdebug(LS_CCCAM, "Saving prefered PID %d for %s",
                               t->s_dvb_prefcapid, ct->td_nicename);
      service_request_save((service_t*)t, 0);
    }

    //if(len < 35) {
      tvhdebug(LS_CCCAM,
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
        tvhdebug(LS_CCCAM,
                 "Obtained DES keys for service \"%s\" in %"PRId64" ms, from %s",
                 t->s_dvb_svcname, delay, ct->td_nicename);
      es->es_keystate = ES_RESOLVED;
      es->es_resolved = 1;
      off = 8;
    //}
    //TODO: AES
    #if 0
    else {
      tvhdebug(LS_CCCAM,
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
        tvhdebug(LS_CCCAM,
                 "Obtained AES keys for service \"%s\" in %"PRId64" ms, from %s",
                 t->s_dvb_svcname, delay, ct->td_nicename);
      es->es_keystate = ES_RESOLVED;
      es->es_resolved = 1;
      off = 16;
    }
#endif
    es3 = *es;
    pthread_mutex_unlock(&cccam->cccam_mutex);
    descrambler_keys((th_descrambler_t *)ct,
                     off == 16 ? DESCRAMBLER_AES128_ECB : DESCRAMBLER_CSA_CBC,
                     0, dcw, dcw + off);
    snprintf(chaninfo, sizeof(chaninfo), "%s:%i", cccam->cccam_hostname, cccam->cccam_port);
    descrambler_notify((th_descrambler_t *)ct,
                       es3.es_caid, es3.es_provid,
                       caid2name(es3.es_caid),
                       es3.es_channel, delay,
                       1, "", chaninfo, "cccam");
    pthread_mutex_lock(&cccam->cccam_mutex);
  }
}

/**
 * Handle running reply
 * cccam_mutex is held
 */
static int
cccam_running_reply(cccam_t *cccam, uint8_t *buf, int len)
{
  cccam_service_t *ct;
  ecm_pid_t *ep;
  ecm_section_t *es;

  tvhtrace(LS_CCCAM, "response msg type=%d, response:", buf[1]);
  tvhlog_hexdump(LS_CCCAM, buf, len);

  switch (buf[1]) {
    case MSG_NEW_CARD_SIDINFO:
    case MSG_NEW_CARD:
      tvhdebug(LS_CCCAM, "add card message received");
      cccam_decode_card_data_reply(cccam, buf);
      break;
    case MSG_CARD_REMOVED:
      tvhdebug(LS_CCCAM, "del card message received");
      break;
    case MSG_KEEPALIVE:
      tvhdebug(LS_CCCAM, "keepalive");
      break;
    case MSG_EMM_REQUEST:   /* emm ack */
      //cccam_send_msg(cccam, MSG_EMM_REQUEST, NULL, 0, 0, 0, 0);
      sem_post(&cccam->ecm_mutex);
      tvhtrace(LS_CCCAM, "EMM message ACK received");
      break;
    case MSG_ECM_NOK1:      /* retry */
    case MSG_ECM_NOK2:      /* decode failed */
    //case MSG_CMD_05:      /* ? */
    case MSG_ECM_REQUEST: { /* request reply */

      uint8_t dcw[16];
      uint8_t seq = buf[0];

      cccam_decrypt_cw(cccam->cccam_nodeid, cccam->card_id, &buf[4]);
      memcpy(dcw, buf + 4, 16);
      cccam_decrypt(&cccam->recvblock, buf + 4, len - 4);
      tvhtrace(LS_CCCAM, "HEADER:");
      tvhlog_hexdump(LS_CCCAM, buf, 4);

      seq = cccam->seq;
      sem_post(&cccam->ecm_mutex);

      LIST_FOREACH(ct, &cccam->cccam_services, cs_link)
        LIST_FOREACH(ep, &ct->cs_pids, ep_link)
          LIST_FOREACH(es, &ep->ep_sections, es_link)
            if(es->es_seq == seq) {
              if (es->es_resolved) {
                mpegts_service_t *t = (mpegts_service_t *)ct->td_service;
                tvhdebug(LS_CCCAM,
                         "Ignore %sECM (PID %d) for service \"%s\" from %s (seq %i)",
                         es->es_pending ? "duplicate " : "",
                         ep->ep_pid, t->s_dvb_svcname, ct->td_nicename, es->es_seq);
                return 0;
              }
              if (es->es_pending) {
                handle_ecm_reply(ct, es, buf, len, seq, dcw);
                return 0;
              }
            }
      tvhwarn(LS_CCCAM, "Got unexpected ECM reply (seqno: %d)", seq);
      break;

    }
    case MSG_SRV_DATA:
      tvhinfo(LS_CCCAM, "CCcam server version %s nodeid=%02x%02x%02x%02x%02x%02x%02x%02x",
          &buf[12], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
      break;
    case MSG_CLI_DATA:
      tvhinfo(LS_CCCAM, "CCcam server authentication completed");
      break;
    default:
      tvhwarn(LS_CCCAM, "Unknown message received");
      break;
  }
  return 0;
}

/**
 *
 */
static int
cccam_must_break(cccam_t *cccam)
{
  return !cccam->cccam_running || !cccam->cac_enabled || cccam->cccam_reconfigure;
}


static int
cccam_read_message(cccam_t *cccam, uint8_t *buf, const char *state, int timeout)
{
  int32_t ret;
  uint16_t msglen;

  pthread_mutex_unlock(&cccam->cccam_mutex);
  ret = tcp_read_timeout(cccam->cccam_fd, buf, 4, timeout);
  pthread_mutex_lock(&cccam->cccam_mutex);
  if (ret) {
    tvhdebug(LS_CCCAM, "recv error %d or timeout", ret);
    return -1;
  }
  cccam_decrypt(&cccam->recvblock, buf, 4);
  msglen = (buf[2] << 8) | buf[3];
  if (msglen > 0) {
    if (msglen > CCCAM_NETMSGSIZE - 2) {
      tvhdebug(LS_CCCAM, "received message too large");
      return -1;
    }
    pthread_mutex_unlock(&cccam->cccam_mutex);
    ret = tcp_read_timeout(cccam->cccam_fd, buf + 4, msglen, 5000);
    pthread_mutex_lock(&cccam->cccam_mutex);
    if (ret) {
      tvhdebug(LS_CCCAM, "timeout reading message");
      return -1;
    }
    cccam_decrypt(&cccam->recvblock, buf + 4, msglen);
  }
  return msglen + 4;
}


/**
 *
 */
static int
cccam_read(cccam_t *cccam, void *buf, size_t len, int timeout)
{
  int r;

  pthread_mutex_unlock(&cccam->cccam_mutex);
  r = tcp_read_timeout(cccam->cccam_fd, buf, len, timeout);
  pthread_mutex_lock(&cccam->cccam_mutex);

  if (r && tvheadend_is_running())
    tvhwarn(LS_CCCAM, "read error %d (%s)", r, strerror(r));

  if(cccam_must_break(cccam))
    return ECONNABORTED;

  return r;
}


/**
 *
 */
static void
cccam_invalidate_cards(cccam_t *cccam)
{
  cs_card_data_t *cd;

  LIST_FOREACH(cd, &cccam->cccam_cards, cs_card)
    cd->running = 0;
}

/**
 *
 */
static void
cccam_free_cards(cccam_t *cccam)
{
  cs_card_data_t *cd;

  while((cd = LIST_FIRST(&cccam->cccam_cards)) != NULL) {
    LIST_REMOVE(cd, cs_card);
    descrambler_close_emm(cd->cccam_mux, cd, cd->cs_ra.caid);
    emm_reass_done(&cd->cs_ra);
    free(cd);
  }
}

/**
 *
 */
static void
cccam_flush_services(cccam_t *cccam)
{
  cccam_service_t *ct;

  LIST_FOREACH(ct, &cccam->cccam_services, cs_link)
    descrambler_flush_table_data(ct->td_service);
}

/**
 *
 */
static uint8_t
cccam_send_msg(cccam_t *cccam,
      cccam_msg_type_t cmd, uint8_t *buf, size_t len, int enq, int seq, uint32_t card_id)
{
  cccam_message_t *cm = malloc(sizeof(cccam_message_t));
  uint8_t *netbuf = cm->cm_data;

  if ((len + 4) > CCCAM_NETMSGSIZE) {
    free(cm);
    return 0;
  }

  memset(netbuf, 0, len + 4);
  if (cmd == MSG_NO_HEADER) {
    memcpy(netbuf, buf, len);
  } else {
    netbuf[0] = seq;
    netbuf[1] = cmd & 0xff;
    netbuf[2] = len >> 8;
    netbuf[3] = len & 0xff;
    if (buf)
      memcpy(netbuf + 4, buf, len);
    len += 4;
  }

  tvhtrace(LS_CCCAM, "sending message len %zu enq %d", len, enq);
  tvhlog_hexdump(LS_CCCAM, buf, len);

  if(enq) {
    cm->cm_len = len;
    cm->seq = seq;
    cm->card_id = card_id;
    pthread_mutex_lock(&cccam->cccam_writer_mutex);
    TAILQ_INSERT_TAIL(&cccam->cccam_writeq, cm, cm_link);
    tvh_cond_signal(&cccam->cccam_writer_cond, 0);
    pthread_mutex_unlock(&cccam->cccam_writer_mutex);
  } else {
    cccam_encrypt(&cccam->sendblock, netbuf, len);
    if (tvh_write(cccam->cccam_fd, netbuf, len))
      tvhinfo(LS_CCCAM, "write error");
    free(cm);
  }
  return seq;
}

/**
 * Send keep alive
 */
static void
cccam_send_ka(cccam_t *cccam)
{
  uint8_t buf[4];

  buf[0] = 0;
  buf[1] = MSG_KEEPALIVE;
  buf[2] = 0;
  buf[3] = 0;

  tvhdebug(LS_CCCAM, "send keepalive");
  cccam_send_msg(cccam, MSG_NO_HEADER, buf, 4, 0, 0, 0);
}

/**
 *
 */
static void *
cccam_writer_thread(void *aux)
{
  cccam_t *cccam = aux;
  cccam_message_t *cm;

  int64_t mono;
  int r;

  pthread_mutex_lock(&cccam->cccam_writer_mutex);

  while(cccam->cccam_writer_running) {

    if((cm = TAILQ_FIRST(&cccam->cccam_writeq)) != NULL) {
      TAILQ_REMOVE(&cccam->cccam_writeq, cm, cm_link);

      cccam_encrypt(&cccam->sendblock, cm->cm_data, cm->cm_len);
      sem_wait(&cccam->ecm_mutex);
      cccam->seq = cm->seq;
      cccam->card_id = cm->card_id;

      pthread_mutex_unlock(&cccam->cccam_writer_mutex);
      //      int64_t ts = getfastmonoclock();
      r = tvh_write(cccam->cccam_fd, cm->cm_data, cm->cm_len);
      if (r)
        tvhinfo(LS_CCCAM, "write error");
      //      printf("Write took %lld usec\n", getfastmonoclock() - ts);
      free(cm);
      pthread_mutex_lock(&cccam->cccam_writer_mutex);
      continue;
    }

    /* If nothing is to be sent in keepalive interval seconds we
       need to send a keepalive
       when disabled default to 1 min but don't send ka messages */
    int delay = cccam->cccam_keepalive_interval ? cccam->cccam_keepalive_interval : 60;
    mono = mclk() + sec2mono(delay);
    do {
      r = tvh_cond_timedwait(&cccam->cccam_writer_cond, &cccam->cccam_writer_mutex, mono);
      if(r == ETIMEDOUT) {
        if (cccam->cccam_keepalive_interval)
          cccam_send_ka(cccam);
        break;
      }
    } while (ERRNO_AGAIN(r));
  }

  pthread_mutex_unlock(&cccam->cccam_writer_mutex);
  return NULL;
}

/**
 *
 */
static void
sha1_make_login_key(cccam_t *cccam, uint8_t *buf)
{
  SHA_CTX sha1;
  uint8_t hash[SHA_DIGEST_LENGTH];

  cccam_crypt_xor(buf);

  SHA1_Init(&sha1);
  SHA1_Update(&sha1, buf, 16);
  SHA1_Final(hash, &sha1);

  tvhdebug(LS_CCCAM, "sha1 hash:");
  tvhlog_hexdump(LS_CCCAM, hash, sizeof(hash));

  cccam_crypt_init(&cccam->recvblock, hash, 20);
  cccam_decrypt(&cccam->recvblock, buf, 16);

  cccam_crypt_init(&cccam->sendblock, buf, 16);
  cccam_decrypt(&cccam->sendblock, hash, 20);

  // send crypted hash to server
  cccam_send_msg(cccam, MSG_NO_HEADER, hash, 20, 0, 0, 0);
}

/**
 * Login command
 */
static int
cccam_send_login(cccam_t *cccam)
{
  uint8_t buf[CCCAM_NETMSGSIZE];
  size_t ul;
  char pwd[255];
  uint8_t data[20];

  if (cccam->cccam_username == NULL)
    return 1;

  ul = strlen(cccam->cccam_username) + 1;
  if (ul > 128)
    return 1;

  /* send username */
  memset(buf, 0, CCCAM_NETMSGSIZE);
  memcpy(buf, cccam->cccam_username, ul);
  cccam_send_msg(cccam, MSG_NO_HEADER, buf, 20, 0, 0, 0);

  /* send password 'xored' with CCcam */
  memset(buf, 0, CCCAM_NETMSGSIZE);
  memset(pwd, 0, sizeof(pwd));
  memcpy(buf, cccam_str, 5);
  strncpy(pwd, cccam->cccam_password, sizeof(pwd) - 1);
  cccam_encrypt(&cccam->sendblock, (uint8_t *) pwd, strlen(pwd));
  cccam_send_msg(cccam, MSG_NO_HEADER, buf, 6, 0, 0, 0);

  if (cccam_read(cccam, data, 20, 5000)) {
    tvherror(LS_CCCAM, "login failed, pwd ack not received");
    return -2;
  }

  cccam_decrypt(&cccam->recvblock, data, 20);
  tvhdebug(LS_CCCAM, "login ack, response:");
  tvhlog_hexdump(LS_CCCAM, data, 20);

  if (memcmp(data, "CCcam", 5)) {
    tvherror(LS_CCCAM, "login failed, usr/pwd invalid");
    return -2;
  } else {
    tvhinfo(LS_CCCAM, "login succeeded");
  }

  return 0;
}

/**
 *
 */
static void
cccam_send_cli_data(cccam_t *cccam)
{
  uint8_t buf[CCCAM_NETMSGSIZE];

  memset(buf, 0, CCCAM_NETMSGSIZE);
  memcpy(buf, cccam->cccam_username, 20);
  memcpy(buf + 20, cccam->cccam_nodeid, 8);
  buf[28] = 0; // TODO: wantemus = 1;
  memcpy(buf + 29, cccam_version_str[cccam->cccam_version], 32);
  memcpy(buf + 61, "tvh", 32); // build number (ascii)
  cccam_send_msg(cccam, MSG_CLI_DATA, buf, 20 + 8 + 1 + 64, 0, 0, 0);
}

/**
 *
 */
static void
cccam_session(cccam_t *cccam)
{
  int r;
  uint8_t buf[CCCAM_NETMSGSIZE];
  pthread_t writer_thread_id;

  /**
   * Get init seed
   */
  if((r = cccam_read(cccam, buf, 16, 5000))) {
    tvhinfo(LS_CCCAM, "%s:%i: init error (no init seed received)",
            cccam->cccam_hostname, cccam->cccam_port);
    return;
  }
  tvhdebug(LS_CCCAM, "init seed received:");
  tvhlog_hexdump(LS_CCCAM, buf, 16);

  /* check for oscam-cccam */
  uint16_t sum = 0x1234;
  uint16_t recv_sum = (buf[14] << 8) | buf[15];
  int32_t i;
  for(i = 0; i < 14; i++) {
    sum += buf[i];
  }
  if (sum == recv_sum)
    tvhinfo(LS_CCCAM, "oscam server detected");

  sha1_make_login_key(cccam, buf);

  /**
   * Login
   */
  if (cccam_send_login(cccam))
    return;

  cccam_send_cli_data(cccam);

  /**
   * Ok, connection good, reset retry delay to zero
   */
  cccam_flush_services(cccam);

  /**
   * We do all requests from now on in a separate thread
   */
  sem_init(&cccam->ecm_mutex, 1, 1);
  cccam->cccam_writer_running = 1;
  tvh_cond_init(&cccam->cccam_writer_cond);
  pthread_mutex_init(&cccam->cccam_writer_mutex, NULL);
  TAILQ_INIT(&cccam->cccam_writeq);
  tvhthread_create(&writer_thread_id, NULL, cccam_writer_thread, cccam, "cccam-writer");

  /**
   * Mainloop
   */
  while (!cccam_must_break(cccam)) {
    if ((r = cccam_read_message(cccam, buf, "Decoderloop", 30000)) < 0)
      break;
    if (r > 0)
      cccam_running_reply(cccam, buf, r);
  };

  tvhdebug(LS_CCCAM, "session thread exiting");

  /**
   * Collect the writer thread
   */
  cccam->cccam_writer_running = 0;
  tvh_cond_signal(&cccam->cccam_writer_cond, 0);
  pthread_join(writer_thread_id, NULL);

  shutdown(cccam->cccam_fd, SHUT_RDWR);
  tvhdebug(LS_CCCAM, "Write thread joined");
}

/**
 *
 */
static void *
cccam_thread(void *aux)
{
  char hostname[256];
  int port;
  cccam_t *cccam = aux;
  int fd, d, r;
  char errbuf[100];
  int attempts = 0;
  int64_t mono;

  pthread_mutex_lock(&cccam->cccam_mutex);

  while(cccam->cccam_running) {
    cccam_invalidate_cards(cccam);
    caclient_set_status((caclient_t *)cccam, CACLIENT_STATUS_READY);

    snprintf(hostname, sizeof(hostname), "%s", cccam->cccam_hostname);
    port = cccam->cccam_port;

    tvhinfo(LS_CCCAM, "Attemping to connect to %s:%d", hostname, port);

    pthread_mutex_unlock(&cccam->cccam_mutex);
    fd = tcp_connect(hostname, port, NULL, errbuf, sizeof(errbuf), 10);
    pthread_mutex_lock(&cccam->cccam_mutex);

    if(fd == -1) {
      attempts++;
      tvhinfo(LS_CCCAM,
              "Connection attempt to %s:%d failed: %s",
              hostname, port, errbuf);
    } else {

      if(cccam->cccam_running == 0) {
        close(fd);
        break;
      }

      tvhinfo(LS_CCCAM, "Connected to %s:%d", hostname, port);
      attempts = 0;

      cccam->cccam_fd = fd;
      cccam->cccam_reconfigure = 0;

      cccam_session(cccam);

      cccam->cccam_fd = -1;
      close(fd);
      tvhinfo(LS_CCCAM, "Disconnected from %s:%i",
              cccam->cccam_hostname, cccam->cccam_port);
    }

    if(cccam->cccam_running == 0) continue;
    if(attempts == 1 || cccam->cccam_reconfigure) {
      cccam->cccam_reconfigure = 0;
      continue; // Retry immediately
    }

    caclient_set_status((caclient_t *)cccam, CACLIENT_STATUS_DISCONNECTED);

    d = 3;

    tvhinfo(LS_CCCAM,
            "%s:%i: Automatic connection attempt in %d seconds",
            cccam->cccam_hostname, cccam->cccam_port, d-1);

    mono = mclk() + sec2mono(d);
    do {
      r = tvh_cond_timedwait(&cccam->cccam_cond, &cccam->cccam_mutex, mono);
      if (r == ETIMEDOUT)
        break;
    } while (ERRNO_AGAIN(r));
  }

  tvhinfo(LS_CCCAM, "%s:%i inactive",
          cccam->cccam_hostname, cccam->cccam_port);
  cccam_free_cards(cccam);
  pthread_mutex_unlock(&cccam->cccam_mutex);
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
cccam_emm_send(void *aux, const uint8_t *radata, int ralen, void *mux)
{
  cs_card_data_t *pcard = aux;
  //cccam_t *cccam = pcard->cccam;

  tvhtrace(LS_CCCAM, "sending EMM for %04x mux %p", pcard->cs_ra.caid, mux);
  tvhlog_hexdump(LS_CCCAM, radata, ralen);

  // FIXME: missing arguments
  //cccam_send_emm(cccam, radata, ralen, pcard->SID,
  //              pcard->cs_ra.caid, ->provider, pcard->id);
}

/**
 *
 */
static void
cccam_emm(void *opaque, int pid, const uint8_t *data, int len, int emm)
{
  cs_card_data_t *pcard = opaque;
  cccam_t *cccam;
  void *mux;

  if (data == NULL) {  /* end-of-data */
    pcard->cccam_mux = NULL;
    return;
  }
  if (pcard->cccam_mux == NULL)
    return;
  cccam = pcard->cccam;
  pthread_mutex_lock(&cccam->cccam_mutex);
  mux = pcard->cccam_mux;

  //orig: if (pcard->running && cccam->cccam_forward_emm && cccam->cccam_writer_running) {
  if (pcard->running && cccam->cccam_writer_running) {
    if (cccam->cccam_emmex) {
      if (cccam->cccam_emm_mux && cccam->cccam_emm_mux != mux) {
        if (cccam->cccam_emm_update_time + sec2mono(25) > mclk())
          goto end_of_job;
      }
      cccam->cccam_emm_update_time = mclk();
    }
    cccam->cccam_emm_mux = mux;
    emm_filter(&pcard->cs_ra, data, len, mux, cccam_emm_send, pcard);
  }
end_of_job:
  pthread_mutex_unlock(&cccam->cccam_mutex);
}


static int
cccam_send_ecm(cccam_t *cccam, const uint8_t *msg, size_t len, int sid,
                uint16_t st_caid, uint32_t st_provider, uint32_t card_id)
{
  unsigned char buf[CCCAM_NETMSGSIZE-4];

  int seq = atomic_add(&cccam->cccam_seq, 1);

  buf[0] = st_caid >> 8;
  buf[1] = st_caid & 0xff;
  buf[2] = st_provider >> 24;
  buf[3] = st_provider >> 16;
  buf[4] = st_provider >> 8;
  buf[5] = st_provider & 0xff;
  buf[6] = card_id >> 24;
  buf[7] = card_id >> 16;
  buf[8] = card_id >> 8;
  buf[9] = card_id & 0xff;
  buf[10] = sid >> 8;
  buf[11] = sid & 0xff;
  buf[12] = len;
  memcpy(buf + 13, msg, len);
  return cccam_send_msg(cccam, MSG_ECM_REQUEST, buf, 13 + len, 1, seq, card_id);
}

#if 0
static int
cccam_send_emm(cccam_t *cccam, const uint8_t *msg, size_t len, int sid,
                uint16_t st_caid, uint32_t st_provider, uint32_t card_id)
{
  unsigned char buf[CCCAM_NETMSGSIZE-4];

  int seq = atomic_add(&cccam->cccam_seq, 1);

  buf[0] = st_caid >> 8;
  buf[1] = st_caid & 0xff;
  buf[2] = 0;
  buf[3] = st_provider >> 24;
  buf[4] = st_provider >> 16;
  buf[5] = st_provider >> 8;
  buf[6] = st_provider & 0xff;
  buf[7] = card_id >> 24;
  buf[8] = card_id >> 16;
  buf[9] = card_id >> 8;
  buf[10] = card_id & 0xff;
  buf[11] = len & 0xff;
  memcpy(buf + 12, msg, len);
  return cccam_send_msg(cccam, MSG_EMM_REQUEST, buf, 12 + len, 1, seq, card_id);
}
#endif
/**
 *
 */
static void
cccam_table_input(void *opaque, int pid, const uint8_t *data, int len, int emm)
{
  cccam_service_t *ct = opaque;
  elementary_stream_t *st;
  mpegts_service_t *t = (mpegts_service_t*)ct->td_service;
  uint16_t sid = t->s_dvb_service_id;
  cccam_t *cccam = ct->cs_cccam;

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

  pthread_mutex_lock(&cccam->cccam_mutex);
  pthread_mutex_lock(&t->s_stream_mutex);

  if (ct->td_keystate == DS_IDLE)
    goto end;

  if (ct->ecm_state == ECM_RESET) {
    /* clean all */
    cccam_service_pid_free(ct);
    /* move to init state */
    ct->ecm_state = ECM_INIT;
    ct->cs_channel = -1;
    t->s_dvb_prefcapid = 0;
    tvhdebug(LS_CCCAM, "Reset after unexpected or no reply for service \"%s\"", t->s_dvb_svcname);
  }

  LIST_FOREACH(ep, &ct->cs_pids, ep_link)
    if(ep->ep_pid == pid) break;

  if(ep == NULL) {
    if (ct->ecm_state == ECM_INIT) {
      // Validate prefered ECM PID
      tvhdebug(LS_CCCAM, "ECM state INIT");

      if(t->s_dvb_prefcapid_lock != PREFCAPID_OFF) {
        st = service_stream_find((service_t*)t, t->s_dvb_prefcapid);
        if (st && st->es_type == SCT_CA)
          LIST_FOREACH(c, &st->es_caids, link)
            LIST_FOREACH(pcard, &cccam->cccam_cards, cs_card)
              if(pcard->running &&
                 pcard->cs_ra.caid == c->caid &&
                 verify_provider(pcard, c->providerid))
                goto prefcapid_ok;
        tvhdebug(LS_CCCAM, "Invalid prefered ECM (PID %d) found for service \"%s\"", t->s_dvb_prefcapid, t->s_dvb_svcname);
        t->s_dvb_prefcapid = 0;
      }

prefcapid_ok:
      if(t->s_dvb_prefcapid == pid || t->s_dvb_prefcapid == 0 ||
         t->s_dvb_prefcapid_lock == PREFCAPID_OFF) {
        ep = calloc(1, sizeof(ecm_pid_t));
        ep->ep_pid = pid;

        LIST_INSERT_HEAD(&ct->cs_pids, ep, ep_link);
        tvhdebug(LS_CCCAM, "Insert %s ECM (PID %d) for service \"%s\"",
                 t->s_dvb_prefcapid ? "preferred" : "new", pid, t->s_dvb_svcname);
      }
    }
    if(ep == NULL)
      goto end;
  }

  st = service_stream_find((service_t *)t, pid);
  if (st) {
    LIST_FOREACH(c, &st->es_caids, link)
      LIST_FOREACH(pcard, &cccam->cccam_cards, cs_card)
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

      if(cccam->cccam_fd == -1) {
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
        tvhdebug(LS_CCCAM, "Filtering ECM (PID %d)", channel);
        goto end;
      }

      //tvhtrace(LS_CCCAM, "send ecm: len=%d sid=%04x caid=%04x provid=%06x", len, sid, caid, provid);
      //tvhlog_hexdump(LS_CCCAM, data, len);

      es->es_seq = cccam_send_ecm(cccam, data, len, sid, caid, provid, pcard->id);

      tvhdebug(LS_CCCAM,
               "Sending ECM%s section=%d/%d, for service \"%s\" (seqno: %d)",
               chaninfo, section, ep->ep_last_section, t->s_dvb_svcname, es->es_seq);
      es->es_time = getfastmonoclock();
      break;

    default:
      /* TODO: EMM */
      //if (cccam->cccam_forward_emm)
      //  cccam_send_msg(cccam, data, len, sid, 1, 0, 0);
      break;
  }

end:
  pthread_mutex_unlock(&t->s_stream_mutex);
  pthread_mutex_unlock(&cccam->cccam_mutex);
}

/**
 * cccam_mutex is held
 */
static void
cccam_service_pid_free(cccam_service_t *ct)
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
 * cccam_mutex is held
 */
static void
cccam_service_destroy0(th_descrambler_t *td)
{
  cccam_service_t *ct = (cccam_service_t *)td;
  int i;

  for (i = 0; i < CCCAM_ES_PIDS; i++)
    if (ct->cs_epids[i])
      descrambler_close_pid(ct->cs_mux, ct,
                            DESCRAMBLER_ECM_PID(ct->cs_epids[i]));

  cccam_service_pid_free(ct);

  LIST_REMOVE(td, td_service_link);

  LIST_REMOVE(ct, cs_link);

  free(ct->td_nicename);
  free(ct);
}

/**
 *
 */
static void
cccam_service_destroy(th_descrambler_t *td)
{
  cccam_service_t *ct = (cccam_service_t *)td;
  cccam_t *cccam = ct->cs_cccam;

  pthread_mutex_lock(&cccam->cccam_mutex);
  cccam_service_destroy0(td);
  pthread_mutex_unlock(&cccam->cccam_mutex);
}

/**
 * Check if our CAID's matches, and if so, link
 *
 * global_lock is held. Not that we care about that, but either way, it is.
 */
static void
cccam_service_start(caclient_t *cac, service_t *t)
{
  cccam_t *cccam = (cccam_t *)cac;
  cccam_service_t *ct;
  th_descrambler_t *td;
  elementary_stream_t *st;
  caid_t *c;
  cs_card_data_t *pcard;
  char buf[512];
  int i, reuse = 0, prefpid, prefpid_lock, forcecaid;

  extern const idclass_t mpegts_service_class;
  if (!idnode_is_instance(&t->s_id, &mpegts_service_class))
    return;

  pthread_mutex_lock(&cccam->cccam_mutex);
  pthread_mutex_lock(&t->s_stream_mutex);

  LIST_FOREACH(ct, &cccam->cccam_services, cs_link) {
    if (ct->td_service == t && ct->cs_cccam == cccam)
      break;
  }
  prefpid      = ((mpegts_service_t *)t)->s_dvb_prefcapid;
  prefpid_lock = ((mpegts_service_t *)t)->s_dvb_prefcapid_lock;
  forcecaid    = ((mpegts_service_t *)t)->s_dvb_forcecaid;
  LIST_FOREACH(pcard, &cccam->cccam_cards, cs_card) {
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
    if (ct) cccam_service_destroy0((th_descrambler_t*)ct);
    goto end;
  }
  if (ct) {
    reuse = 1;
    for (i = 0; i < CCCAM_ES_PIDS; i++) {
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

  ct                   = calloc(1, sizeof(cccam_service_t));
  ct->cs_cccam         = cccam;
  ct->cs_channel       = -1;
  ct->cs_mux           = ((mpegts_service_t *)t)->s_dvb_mux;
  ct->ecm_state        = ECM_INIT;

  td                   = (th_descrambler_t *)ct;
  snprintf(buf, sizeof(buf), "cccam-%s-%i-%04X", cccam->cccam_hostname, cccam->cccam_port, pcard->cs_ra.caid);
  td->td_nicename      = strdup(buf);
  td->td_service       = t;
  td->td_stop          = cccam_service_destroy;
  td->td_ecm_reset     = cccam_ecm_reset;
  td->td_ecm_idle      = cccam_ecm_idle;
  LIST_INSERT_HEAD(&t->s_descramblers, td, td_service_link);

  LIST_INSERT_HEAD(&cccam->cccam_services, ct, cs_link);

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
    if (i == CCCAM_ES_PIDS) break;
  }

  for (i = 0; i < CCCAM_ES_PIDS; i++)
    if (ct->cs_epids[i])
      descrambler_open_pid(ct->cs_mux, ct,
                           DESCRAMBLER_ECM_PID(ct->cs_epids[i]),
                           cccam_table_input, t);

  if (reuse & 2) {
    ct->cs_channel = -1;
    ct->ecm_state = ECM_INIT;
  }

  if (reuse != 1)
    tvhdebug(LS_CCCAM, "%s %susing CCCAM %s:%d",
             service_nicename(t), reuse ? "re" : "", cccam->cccam_hostname, cccam->cccam_port);
end:
  pthread_mutex_unlock(&t->s_stream_mutex);
  pthread_mutex_unlock(&cccam->cccam_mutex);
}

/**
 *
 */
static void
cccam_free(caclient_t *cac)
{
  cccam_t *cccam = (cccam_t *)cac;
  cccam_service_t *ct;

  while((ct = LIST_FIRST(&cccam->cccam_services)) != NULL)
    cccam_service_destroy((th_descrambler_t *)ct);

  cccam_free_cards(cccam);
  free((void *)cccam->cccam_password);
  free((void *)cccam->cccam_username);
  free((void *)cccam->cccam_hostname);
}

/**
 *
 */
static void
cccam_caid_update(caclient_t *cac, mpegts_mux_t *mux, uint16_t caid, uint16_t pid, int valid)
{
  cccam_t *cccam = (cccam_t *)cac;;
  cs_card_data_t *pcard;

  tvhtrace(LS_CCCAM,
           "caid update event - client %s mux %p caid %04x (%i) pid %04x (%i) valid %i",
           cac->cac_name, mux, caid, caid, pid, pid, valid);
  pthread_mutex_lock(&cccam->cccam_mutex);
  if (valid < 0 || cccam->cccam_running) {
    LIST_FOREACH(pcard, &cccam->cccam_cards, cs_card) {
      if (valid < 0 || pcard->cs_ra.caid == caid) {
        if (pcard->cccam_mux && pcard->cccam_mux != mux) continue;
        if (valid > 0) {
          pcard->cccam       = cccam;
          pcard->cccam_mux   = mux;
          descrambler_open_emm(mux, pcard, caid, cccam_emm);
        } else {
          pcard->cccam_mux   = NULL;
          descrambler_close_emm(mux, pcard, caid);
        }
      }
    }
  }
  pthread_mutex_unlock(&cccam->cccam_mutex);
}

/**
 *
 */
static void
cccam_conf_changed(caclient_t *cac)
{
  cccam_t *cccam = (cccam_t *)cac;
  pthread_t tid;

  if (cac->cac_enabled) {
    if (cccam->cccam_hostname == NULL || cccam->cccam_hostname[0] == '\0') {
      caclient_set_status(cac, CACLIENT_STATUS_NONE);
      return;
    }
    if (!cccam->cccam_running) {
      cccam->cccam_running = 1;
      tvhthread_create(&cccam->cccam_tid, NULL, cccam_thread, cccam, "cccam");
      return;
    }
    pthread_mutex_lock(&cccam->cccam_mutex);
    cccam->cccam_reconfigure = 1;
    if(cccam->cccam_fd >= 0)
      shutdown(cccam->cccam_fd, SHUT_RDWR);
    tvh_cond_signal(&cccam->cccam_cond, 0);
    pthread_mutex_unlock(&cccam->cccam_mutex);
  } else {
    if (!cccam->cccam_running)
      return;
    pthread_mutex_lock(&cccam->cccam_mutex);
    cccam->cccam_running = 0;
    tvh_cond_signal(&cccam->cccam_cond, 0);
    tid = cccam->cccam_tid;
    if (cccam->cccam_fd >= 0)
      shutdown(cccam->cccam_fd, SHUT_RDWR);
    pthread_mutex_unlock(&cccam->cccam_mutex);
    pthread_kill(tid, SIGHUP);
    pthread_join(tid, NULL);
    caclient_set_status(cac, CACLIENT_STATUS_NONE);
  }
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
static int
caclient_cccam_nodeid_set(void *o, const void *v)
{
  cccam_t *cccam = o;
  const char *s = v ?: "";
  uint8_t key[8];
  int i, u, l;
  uint64_t node_id;

  for(i = 0; i < ARRAY_SIZE(key); i++) {
    while(*s != 0 && !isxdigit(*s)) s++;
    u = *s ? nibble(*s++) : 0;
    while(*s != 0 && !isxdigit(*s)) s++;
    l = *s ? nibble(*s++) : 0;
    key[i] = (u << 4) | l;
  }

  node_id = be64toh(*((uint64_t*) key));
  if (!node_id)
    uuid_random(key, 8);

  if ((i = memcmp(cccam->cccam_nodeid, key, ARRAY_SIZE(key))) != 0)
    memcpy(cccam->cccam_nodeid, key, ARRAY_SIZE(key));
  return i;
}

static const void *
caclient_cccam_nodeid_get(void *o)
{
  cccam_t *cccam = o;
  snprintf(prop_sbuf, PROP_SBUF_LEN,
           "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
           cccam->cccam_nodeid[0x0],
           cccam->cccam_nodeid[0x1],
           cccam->cccam_nodeid[0x2],
           cccam->cccam_nodeid[0x3],
           cccam->cccam_nodeid[0x4],
           cccam->cccam_nodeid[0x5],
           cccam->cccam_nodeid[0x6],
           cccam->cccam_nodeid[0x7]);
  return &prop_sbuf_ptr;
}


static htsmsg_t *
caclient_cccam_class_cccam_version_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("2.0.11"), CCCAM_VERSION_2_0_11 },
    { N_("2.1.1"), CCCAM_VERSION_2_1_1 },
    { N_("2.1.2"), CCCAM_VERSION_2_1_2 },
    { N_("2.1.3"), CCCAM_VERSION_2_1_3 },
    { N_("2.1.4"), CCCAM_VERSION_2_1_4 },
    { N_("2.2.0"), CCCAM_VERSION_2_2_0 },
    { N_("2.2.1"), CCCAM_VERSION_2_2_1 },
    { N_("2.3.0"), CCCAM_VERSION_2_3_0 },
  };
  return strtab2htsmsg(tab, 1, lang);
}


const idclass_t caclient_cccam_class =
{
  .ic_super      = &caclient_class,
  .ic_class      = "caclient_cccam",
  .ic_caption    = N_("CCcam"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "username",
      .name     = N_("Username"),
      .desc     = N_("Login username."),
      .off      = offsetof(cccam_t, cccam_username),
      .opts     = PO_TRIM,
    },
    {
      .type     = PT_STR,
      .id       = "password",
      .name     = N_("Password"),
      .desc     = N_("Login password."),
      .off      = offsetof(cccam_t, cccam_password),
      .opts     = PO_PASSWORD
    },
    {
      .type     = PT_STR,
      .id       = "hostname",
      .name     = N_("Hostname/IP"),
      .desc     = N_("Hostname (or IP) of the server."),
      .off      = offsetof(cccam_t, cccam_hostname),
      .def.s    = "localhost",
      .opts     = PO_TRIM,
    },
    {
      .type     = PT_INT,
      .id       = "port",
      .name     = N_("Port"),
      .desc     = N_("Port to connect to."),
      .off      = offsetof(cccam_t, cccam_port),
    },
    {
      .type     = PT_STR,
      .id       = "nodeid",
      .name     = N_("Node ID"),
      .desc     = N_("Client node ID. Leave field empty to generate a random ID."),
      .set      = caclient_cccam_nodeid_set,
      .get      = caclient_cccam_nodeid_get,
    },
    {
      .type     = PT_INT,
      .id       = "version",
      .name     = N_("Version"),
      .desc     = N_("Protocol version."),
      .off      = offsetof(cccam_t, cccam_version),
      .list     = caclient_cccam_class_cccam_version_list,
      .def.i    = CCCAM_VERSION_2_3_0,
    },
#if 0
    {
      .type     = PT_BOOL,
      .id       = "emm",
      .name     = N_("Update card (EMM)"),
      .desc     = N_("Enable/disable offering of Entitlement Management Message updates."),
      .off      = offsetof(cccam_t, cccam_emm),
      .def.i    = 1
    },
    {
      .type     = PT_BOOL,
      .id       = "emmex",
      .name     = N_("Updates from one mux (EMM)"),
      .desc     = N_("Update Entitlement Management Messages from one mux only."),
      .off      = offsetof(cccam_t, cccam_emmex),
      .def.i    = 1
    },
#endif
    {
      .type     = PT_INT,
      .id       = "keepalive_interval",
      .name     = N_("Keepalive interval (0=disable)"),
      .desc     = N_("Keepalive interval in seconds"),
      .off      = offsetof(cccam_t, cccam_keepalive_interval),
      .def.i    = CCCAM_KEEPALIVE_INTERVAL,
    },
    { }
  }
};

/*
 *
 */
caclient_t *cccam_create(void)
{
  cccam_t *cccam = calloc(1, sizeof(*cccam));

  pthread_mutex_init(&cccam->cccam_mutex, NULL);
  tvh_cond_init(&cccam->cccam_cond);
  cccam->cac_free         = cccam_free;
  cccam->cac_start        = cccam_service_start;
  cccam->cac_conf_changed = cccam_conf_changed;
  cccam->cac_caid_update  = cccam_caid_update;
  cccam->cccam_keepalive_interval = CCCAM_KEEPALIVE_INTERVAL;
  cccam->cccam_version = CCCAM_VERSION_2_3_0;
  return (caclient_t *)cccam;
}

