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
#include "cwc.h"
#include "notify.h"
#include "atomic.h"
#include "dtable.h"
#include "subscriptions.h"
#include "service.h"
#include "input.h"
#include "input/mpegts/tsdemux.h"
#include "tvhcsa.h"

/**
 *
 */
#define TVHEADEND_PROTOCOL_ID 0x6502
#define CWC_KEEPALIVE_INTERVAL 30

#define CWS_NETMSGSIZE 362
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
static struct cwc_queue cwcs;
static pthread_cond_t cwc_config_changed;
static pthread_mutex_t cwc_mutex;
static char *crypt_md5(const char *pw, const char *salt);

/**
 *
 */
typedef struct ecm_section {
  int es_section;
  int es_channel;

  uint16_t es_seq;
  char es_nok;
  char es_pending;
  int64_t es_time;  // time request was sent
  size_t es_ecmsize;
  uint8_t es_ecm[4070];

} ecm_section_t;


/**
 *
 */
typedef struct ecm_pid {
  LIST_ENTRY(ecm_pid) ep_link;
  
  uint16_t ep_pid;

  int ep_last_section;
  struct ecm_section *ep_sections[256];
} ecm_pid_t;


/**
 *
 */
typedef struct cwc_service {
  th_descrambler_t;

  struct cwc *cs_cwc;

  LIST_ENTRY(cwc_service) cs_link;

  int cs_channel;
  elementary_stream_t *cs_estream;
  mpegts_mux_t *cs_mux;

  /**
   * ECM Status
   */
  enum {
    ECM_INIT,
    ECM_VALID,
    ECM_RESET
  } ecm_state;

  tvhcsa_t cs_csa;
  
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

  /* Card caid */
  uint16_t cwc_caid;
  
  /* Card type */
  card_type_t cwc_card_type;
  
  /* Card providers */
  cwc_provider_t *cwc_providers;
  
  /* Number of Card providers */
  int cwc_num_providers;
  
  uint8_t cwc_ua[8];
  
  struct cwc *cwc;
  mpegts_mux_t *cwc_mux;

} cs_card_data_t;

/**
 *
 */
typedef struct cwc {
  int cwc_fd;
  int cwc_connected;

  int cwc_retry_delay;

  pthread_t cwc_tid;
  
  pthread_cond_t cwc_cond;

  pthread_mutex_t cwc_writer_mutex; 
  pthread_cond_t cwc_writer_cond; 
  int cwc_writer_running;
  struct cwc_message_queue cwc_writeq;

  TAILQ_ENTRY(cwc) cwc_link; /* Linkage protected via cwc_mutex */

  struct cwc_service_list cwc_services;


  struct cwc_cards_list cwc_cards;
  int cwc_seq;

  DES_key_schedule cwc_k1, cwc_k2;

  uint8_t cwc_buf[256];
  int cwc_bufptr;

  /* Emm forwarding */
  int cwc_forward_emm;

  /* Emm duplicate cache */
  struct {
#define EMM_CACHE_SIZE (1<<5)
#define EMM_CACHE_MASK (EMM_CACHE_SIZE-1)
    uint32_t cache[EMM_CACHE_SIZE];
    uint32_t w;
    uint32_t n;
  } cwc_emm_cache;

  /* Viaccess EMM assemble state */
  struct {
    int shared_toggle;
    int shared_len;
    uint8_t * shared_emm;
    void *ca_update_id;
  } cwc_viaccess_emm;
#define cwc_cryptoworks_emm cwc_viaccess_emm

  /* one update id */
  int64_t cwc_update_time;
  void *cwc_update_id;
  
  /* From configuration */

  uint8_t cwc_confedkey[14];
  char *cwc_username;
  char *cwc_password;
  char *cwc_password_salted;   /* salted version */
  char *cwc_comment;
  char *cwc_hostname;
  int cwc_port;
  char *cwc_id;
  int cwc_emm;
  int cwc_emmex;

  const char *cwc_errtxt;

  int cwc_enabled;
  int cwc_running;
  int cwc_reconfigure;
} cwc_t;


/**
 *
 */

static void cwc_service_destroy(th_descrambler_t *td);
void cwc_emm_conax(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len);
void cwc_emm_irdeto(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len);
void cwc_emm_dre(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len);
void cwc_emm_seca(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len);
void cwc_emm_viaccess(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len);
void cwc_emm_nagra(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len);
void cwc_emm_nds(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len);
void cwc_emm_cryptoworks(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len);
void cwc_emm_bulcrypt(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len);


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
static void 
des_random_get(uint8_t *buffer, uint8_t len)
{
  uint8_t idx = 0;
  int randomNo = 0;
  
  for (idx = 0; idx < len; idx++) {
      if (!(idx % 3)) randomNo = rand();
      buffer[idx] = (randomNo >> ((idx % 3) << 3)) & 0xff;
    }
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
  des_random_get(padBytes, noPadBytes);
  for (i = 0; i < noPadBytes; i++) buffer[len++] = padBytes[i];
  for (i = 2; i < len; i++) checksum ^= buffer[i];
  buffer[len++] = checksum;
  des_random_get((uint8_t *)ivec, 8);
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
  int seq;
  
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
  
  if((len = des_encrypt(buf, len, cwc)) <= 0) {
    free(cm);
    return -1;
  }

  tvhtrace("cwc", "sending message sid %d len %"PRIsize_t" enq %d", sid, len, enq);
  tvhlog_hexdump("cwc", buf, len);

  buf[0] = (len - 2) >> 8;
  buf[1] = (len - 2) & 0xff;
  
  if(enq) {
    cm->cm_len = len;
    pthread_mutex_lock(&cwc->cwc_writer_mutex);
    TAILQ_INSERT_TAIL(&cwc->cwc_writeq, cm, cm_link);
    pthread_cond_signal(&cwc->cwc_writer_cond);
    pthread_mutex_unlock(&cwc->cwc_writer_mutex);
  } else {
    if (tvh_write(cwc->cwc_fd, buf, len))
      tvhlog(LOG_INFO, "cwc", "write error %s", strerror(errno));

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
static void 
cwc_comet_status_update(cwc_t *cwc)
{
  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_str(m, "id", cwc->cwc_id);
  htsmsg_add_u32(m, "connected", !!cwc->cwc_connected);
  notify_by_msg("cwcStatus", m);
}

/**
 * Handle reply to card data request
 */
static int
cwc_decode_card_data_reply(cwc_t *cwc, uint8_t *msg, int len)
{
  int plen, i;
  unsigned int nprov;
  const char *n;

  msg += 12;
  len -= 12;

  if(len < 3) {
    tvhlog(LOG_INFO, "cwc", "Invalid card data reply");
    return -1;
  }

  plen = (msg[1] & 0xf) << 8 | msg[2];

  if(plen < 14) {
    tvhlog(LOG_INFO, "cwc", "Invalid card data reply (message)");
    return -1;
  }

  nprov = msg[14];

  if(plen < nprov * 11) {
    tvhlog(LOG_INFO, "cwc", "Invalid card data reply (provider list)");
    return -1;
  }

  cwc->cwc_connected = 1;
  cwc_comet_status_update(cwc);
  struct cs_card_data *pcard;
  pcard = calloc(1, sizeof(struct cs_card_data));
  pcard->cwc_caid = (msg[4] << 8) | msg[5];
  
  n = descrambler_caid2name(pcard->cwc_caid & 0xff00) ?: "Unknown";
  
  memcpy(pcard->cwc_ua, &msg[6], 8);
  pcard->cwc_card_type = detect_card_type(pcard->cwc_caid);
  
  msg  += 15;
  plen -= 12;
  
  pcard->cwc_num_providers = nprov;
  pcard->cwc_providers = calloc(nprov, sizeof(pcard->cwc_providers[0]));
  
  tvhlog(LOG_INFO, "cwc", "%s:%i: Connected as user %s "
         "to a %s-card [0x%04x : %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x] "
         "with %d providers",
         cwc->cwc_hostname, cwc->cwc_port,
         cwc->cwc_username, n, pcard->cwc_caid,
         pcard->cwc_ua[0], pcard->cwc_ua[1], pcard->cwc_ua[2], pcard->cwc_ua[3], pcard->cwc_ua[4], pcard->cwc_ua[5], pcard->cwc_ua[6], pcard->cwc_ua[7],
         nprov);
  
  for(i = 0; i < nprov; i++) {
    pcard->cwc_providers[i].id = (msg[0] << 16) | (msg[1] << 8) | msg[2];
    pcard->cwc_providers[i].sa[0] = msg[3];
    pcard->cwc_providers[i].sa[1] = msg[4];
    pcard->cwc_providers[i].sa[2] = msg[5];
    pcard->cwc_providers[i].sa[3] = msg[6];
    pcard->cwc_providers[i].sa[4] = msg[7];
    pcard->cwc_providers[i].sa[5] = msg[8];
    pcard->cwc_providers[i].sa[6] = msg[9];
    pcard->cwc_providers[i].sa[7] = msg[10];
    
    tvhlog(LOG_INFO, "cwc", "%s:%i: Provider ID #%d: 0x%06x %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x",
           cwc->cwc_hostname, cwc->cwc_port, i + 1,
           pcard->cwc_providers[i].id,
           pcard->cwc_providers[i].sa[0],
           pcard->cwc_providers[i].sa[1],
           pcard->cwc_providers[i].sa[2],
           pcard->cwc_providers[i].sa[3],
           pcard->cwc_providers[i].sa[4],
           pcard->cwc_providers[i].sa[5],
           pcard->cwc_providers[i].sa[6],
           pcard->cwc_providers[i].sa[7]);
    
    msg += 11;
  }
  
  LIST_INSERT_HEAD(&cwc->cwc_cards, pcard, cs_card);
  
  cwc->cwc_forward_emm = 0;
  if (cwc->cwc_emm) {
    int emm_allowed = (pcard->cwc_ua[0] || pcard->cwc_ua[1] ||
                       pcard->cwc_ua[2] || pcard->cwc_ua[3] ||
                       pcard->cwc_ua[4] || pcard->cwc_ua[5] ||
                       pcard->cwc_ua[6] || pcard->cwc_ua[7]);
    
    if (!emm_allowed) {
      tvhlog(LOG_INFO, "cwc", 
	     "%s:%i: Will not forward EMMs (not allowed by server)",
	     cwc->cwc_hostname, cwc->cwc_port);
    } else if (pcard->cwc_card_type != CARD_UNKNOWN) {
      tvhlog(LOG_INFO, "cwc", "%s:%i: Will forward EMMs",
	     cwc->cwc_hostname, cwc->cwc_port);
      cwc->cwc_forward_emm = 1;
    } else {
      tvhlog(LOG_INFO, "cwc", 
	     "%s:%i: Will not forward EMMs (unsupported CA system)",
	     cwc->cwc_hostname, cwc->cwc_port);
    }
  }

  return 0;
}

/**
 * Login command
 */
static void
cwc_send_login(cwc_t *cwc)
{
  uint8_t buf[CWS_NETMSGSIZE];
  int ul = strlen(cwc->cwc_username) + 1;
  int pl = strlen(cwc->cwc_password_salted) + 1;

  buf[0] = MSG_CLIENT_2_SERVER_LOGIN;
  buf[1] = 0;
  buf[2] = ul + pl;
  memcpy(buf + 3,      cwc->cwc_username, ul);
  memcpy(buf + 3 + ul, cwc->cwc_password_salted, pl);
 
  cwc_send_msg(cwc, buf, ul + pl + 3, TVHEADEND_PROTOCOL_ID, 0, 0, 0);
}



static void
handle_ecm_reply(cwc_service_t *ct, ecm_section_t *es, uint8_t *msg,
		 int len, int seq)
{
  mpegts_service_t *t = (mpegts_service_t *)ct->td_service;
  th_descrambler_t *td;
  ecm_pid_t *ep, *epn;
  char chaninfo[32];
  int i;
  int64_t delay = (getmonoclock() - es->es_time) / 1000LL; // in ms
  es->es_pending = 0;

  snprintf(chaninfo, sizeof(chaninfo), " (PID %d)", es->es_channel);

  if(len < 19) {
    
    /* ERROR */

    if (es->es_nok < 3)
      es->es_nok++;

    if(ct->td_keystate == DS_FORBIDDEN)
      return; // We already know it's bad

    if (es->es_nok > 2) {
      tvhlog(LOG_DEBUG, "cwc",
             "Too many NOKs for service \"%s\"%s from %s",
             t->s_dvb_svcname, chaninfo, ct->td_nicename);
      goto forbid;
    }

    LIST_FOREACH(td, &t->s_descramblers, td_service_link)
      if (td != (th_descrambler_t *)ct && td->td_keystate == DS_RESOLVED) {
        tvhlog(LOG_DEBUG, "cwc",
	       "NOK from %s: Already has a key for service \"%s\", from %s",
	       ct->td_nicename,  t->s_dvb_svcname, td->td_nicename);
        es->es_nok = 3; /* do not send more ECM requests */
        ct->td_keystate = DS_IDLE;
      }

    tvhlog(LOG_DEBUG, "cwc", "Received NOK for service \"%s\"%s (seqno: %d "
	   "Req delay: %"PRId64" ms)", t->s_dvb_svcname, chaninfo, seq, delay);

forbid:
    LIST_FOREACH(ep, &ct->cs_pids, ep_link) {
      for(i = 0; i <= ep->ep_last_section; i++)
	if(ep->ep_sections[i] == NULL) {
          if(es->es_nok < 2) /* only first hit is allowed */
            return;
	} else {
          if(ep->ep_sections[i]->es_pending ||
	     ep->ep_sections[i]->es_nok == 0)
	    return;
        }
    }
    tvhlog(LOG_ERR, "cwc",
	   "Can not descramble service \"%s\", access denied (seqno: %d "
	   "Req delay: %"PRId64" ms) from %s",
	   t->s_dvb_svcname, seq, delay, ct->td_nicename);

    ct->td_keystate = DS_FORBIDDEN;
    ct->ecm_state = ECM_RESET;

    return;

  } else {

    es->es_nok = 0;
    ct->cs_channel = es->es_channel;
    ct->ecm_state = ECM_VALID;

    if(t->s_dvb_prefcapid == 0 || t->s_dvb_prefcapid != ct->cs_channel) {
      t->s_dvb_prefcapid = ct->cs_channel;
      tvhlog(LOG_DEBUG, "cwc", "Saving prefered PID %d for %s",
                               t->s_dvb_prefcapid, ct->td_nicename);
      service_request_save((service_t*)t, 0);
    }

    tvhlog(LOG_DEBUG, "cwc",
	   "Received ECM reply%s for service \"%s\" "
	   "even: %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x"
	   " odd: %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x (seqno: %d "
	   "Req delay: %"PRId64" ms)",
	   chaninfo,
	   t->s_dvb_svcname,
	   msg[3 + 0], msg[3 + 1], msg[3 + 2], msg[3 + 3], msg[3 + 4],
	   msg[3 + 5], msg[3 + 6], msg[3 + 7], msg[3 + 8], msg[3 + 9],
	   msg[3 + 10],msg[3 + 11],msg[3 + 12],msg[3 + 13],msg[3 + 14],
	   msg[3 + 15], seq, delay);

    if(ct->td_keystate != DS_RESOLVED)
      tvhlog(LOG_DEBUG, "cwc",
	     "Obtained key for service \"%s\" in %"PRId64" ms, from %s",
	     t->s_dvb_svcname, delay, ct->td_nicename);

    descrambler_keys((th_descrambler_t *)ct, msg + 3, msg + 3 + 8);

    ep = LIST_FIRST(&ct->cs_pids);
    while(ep != NULL) {
      if (ct->cs_channel == ep->ep_pid) {
        ep = LIST_NEXT(ep, ep_link);
      }
      else {
        epn = LIST_NEXT(ep, ep_link);
        for(i = 0; i < 256; i++)
          free(ep->ep_sections[i]);
        LIST_REMOVE(ep, ep_link);
        tvhlog(LOG_WARNING, "cwc", "Delete ECM (PID %d) for service \"%s\" from %s",
                                   ep->ep_pid, t->s_dvb_svcname, ct->td_nicename);
        free(ep);
        ep = epn;
      }
    }
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
  int plen,i;
  short caid;
  const char *n;
  unsigned int nprov;
  
  switch(msgtype) {
    case 0x80:
    case 0x81:
      len -= 12;
      msg += 12;
      LIST_FOREACH(ct, &cwc->cwc_services, cs_link) {
        LIST_FOREACH(ep, &ct->cs_pids, ep_link) {
          for(i = 0; i <= ep->ep_last_section; i++) {
            es = ep->ep_sections[i];
            if(es != NULL) {
              if(es->es_seq == seq && es->es_pending) {
                handle_ecm_reply(ct, es, msg, len, seq);
                return 0;
              }
            }
          }
        }
      }
      tvhlog(LOG_WARNING, "cwc", "Got unexpected ECM reply (seqno: %d)", seq);
      LIST_FOREACH(ct, &cwc->cwc_services, cs_link) {
        ct->ecm_state = ECM_RESET;
      }
      break;
      
    case 0xD3:
      caid = (msg[6] << 8) | msg[7];
      
      if (caid){
        if(len < 3) {
          tvhlog(LOG_INFO, "cwc", "Invalid card data reply");
          return -1;
        }
        
        plen = (msg[1] & 0xf) << 8 | msg[2];
        
        if(plen < 14) {
          tvhlog(LOG_INFO, "cwc", "Invalid card data reply (message)");
          return -1;
        }
        
        nprov = msg[14];
        
        if(plen < nprov * 11) {
          tvhlog(LOG_INFO, "cwc", "Invalid card data reply (provider list)");
          return -1;
	      }
        cwc->cwc_connected = 1;
        cwc_comet_status_update(cwc);
        struct cs_card_data *pcard;
        pcard = calloc(1, sizeof(struct cs_card_data));
        pcard->cwc_caid = caid;
        pcard->cwc_num_providers = 1;
        pcard->cwc_providers = calloc(1, sizeof(pcard->cwc_providers[0]));
        
        n = descrambler_caid2name(pcard->cwc_caid & 0xff00) ?: "Unknown";
        
        memcpy(pcard->cwc_ua, &msg[6], 8);
        
        pcard->cwc_card_type = detect_card_type(pcard->cwc_caid);
        
        pcard->cwc_providers[0].id = (msg[8] << 16) | (msg[9] << 8) | msg[10];
        
        tvhlog(LOG_INFO, "cwc", "%s:%i: Connected as user %s "
               "to a %s-card [0x%04x : %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x] "
               "with one Provider ID 0x%06x",
               cwc->cwc_hostname, cwc->cwc_port,
               cwc->cwc_username, n, pcard->cwc_caid,
               pcard->cwc_ua[0], pcard->cwc_ua[1], pcard->cwc_ua[2], pcard->cwc_ua[3], pcard->cwc_ua[4], pcard->cwc_ua[5], pcard->cwc_ua[6],
               pcard->cwc_ua[7], pcard->cwc_providers[0].id);

        LIST_INSERT_HEAD(&cwc->cwc_cards, pcard, cs_card);
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
  return !cwc->cwc_running || !cwc->cwc_enabled || cwc->cwc_reconfigure;
}

/**
 *
 */
static int
cwc_read(cwc_t *cwc, void *buf, size_t len, int timeout)
{
  int r;

  pthread_mutex_unlock(&cwc_mutex);
  r = tcp_read_timeout(cwc->cwc_fd, buf, len, timeout);
  pthread_mutex_lock(&cwc_mutex);

  if (r && tvheadend_running)
    tvhwarn("cwc", "read error %d (%s)", r, strerror(r));

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
    if (tvheadend_running)
      tvhlog(LOG_INFO, "cwc", "%s:%i: %s: Read error (header): %s",
	     cwc->cwc_hostname, cwc->cwc_port, state, strerror(r));
    return -1;
  }

  msglen = (buf[0] << 8) | buf[1];
  if(msglen >= CWS_NETMSGSIZE) {
    if (tvheadend_running)
      tvhlog(LOG_INFO, "cwc", "%s:%i: %s: Invalid message size: %d",
	     cwc->cwc_hostname, cwc->cwc_port, state, msglen);
    return -1;
  }

  /* We expect the rest of the message to arrive fairly quick,
     so just wait 1 second here */

  if((r = cwc_read(cwc, cwc->cwc_buf + 2, msglen, 1000))) {
    if (tvheadend_running)
      tvhlog(LOG_INFO, "cwc", "%s:%i: %s: Read error: %s",
	     cwc->cwc_hostname, cwc->cwc_port, state, strerror(r));
    return -1;
  }

  if((msglen = des_decrypt(cwc->cwc_buf, msglen + 2, cwc)) < 15) {
    tvhlog(LOG_INFO, "cwc", "%s:%i: %s: Decrypt failed",
	   cwc->cwc_hostname, cwc->cwc_port, state);
    return -1;
  }
  return msglen;
}

/**
 *
 */
static void *
cwc_writer_thread(void *aux)
{
  cwc_t *cwc = aux;
  cwc_message_t *cm;
  struct timespec ts;
  int r;

  pthread_mutex_lock(&cwc->cwc_writer_mutex);

  while(cwc->cwc_writer_running) {

    if((cm = TAILQ_FIRST(&cwc->cwc_writeq)) != NULL) {
      TAILQ_REMOVE(&cwc->cwc_writeq, cm, cm_link);
      pthread_mutex_unlock(&cwc->cwc_writer_mutex);
      //      int64_t ts = getmonoclock();
      if (tvh_write(cwc->cwc_fd, cm->cm_data, cm->cm_len))
        tvhlog(LOG_INFO, "cwc", "write error %s", strerror(errno));
      //      printf("Write took %lld usec\n", getmonoclock() - ts);
      free(cm);
      pthread_mutex_lock(&cwc->cwc_writer_mutex);
      continue;
    }


    /* If nothing is to be sent in CWC_KEEPALIVE_INTERVAL seconds we
       need to send a keepalive */
    ts.tv_sec  = time(NULL) + CWC_KEEPALIVE_INTERVAL;
    ts.tv_nsec = 0;
    r = pthread_cond_timedwait(&cwc->cwc_writer_cond, 
			       &cwc->cwc_writer_mutex, &ts);
    if(r == ETIMEDOUT)
      cwc_send_ka(cwc);
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
    tvhlog(LOG_INFO, "cwc", "%s:%i: No login key received: %s",
	   cwc->cwc_hostname, cwc->cwc_port, strerror(r));
    return;
  }

  des_make_login_key(cwc, cwc->cwc_buf);

  /**
   * Login
   */
  cwc_send_login(cwc);
  
  if(cwc_read_message(cwc, "Wait login response", 5000) < 0)
    return;

  if(cwc->cwc_buf[12] != MSG_CLIENT_2_SERVER_LOGIN_ACK) {
    tvhlog(LOG_INFO, "cwc", "%s:%i: Login failed",
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
    tvhlog(LOG_INFO, "cwc", "%s:%i: Card data request failed",
           cwc->cwc_hostname, cwc->cwc_port);
    return;
  }

  if(cwc_decode_card_data_reply(cwc, cwc->cwc_buf, r) < 0)
    return;

  /**
   * Ok, connection good, reset retry delay to zero
   */
  cwc->cwc_retry_delay = 0;

  /**
   * We do all requests from now on in a separate thread
   */
  cwc->cwc_writer_running = 1;
  pthread_cond_init(&cwc->cwc_writer_cond, NULL);
  pthread_mutex_init(&cwc->cwc_writer_mutex, NULL);
  TAILQ_INIT(&cwc->cwc_writeq);
  tvhthread_create(&writer_thread_id, NULL, cwc_writer_thread, cwc, 0);

  /**
   * Mainloop
   */
  while(!cwc_must_break(cwc)) {

    if((r = cwc_read_message(cwc, "Decoderloop", 
			     CWC_KEEPALIVE_INTERVAL * 2 * 1000)) < 0)
      break;
    cwc_running_reply(cwc, cwc->cwc_buf[12], cwc->cwc_buf, r);
  }
  tvhdebug("cwc", "session thread exiting");

  /**
   * Collect the writer thread
   */
  shutdown(cwc->cwc_fd, SHUT_RDWR);
  cwc->cwc_writer_running = 0;
  pthread_cond_signal(&cwc->cwc_writer_cond);
  pthread_join(writer_thread_id, NULL);
  tvhlog(LOG_DEBUG, "cwc", "Write thread joined");
}


/**
 *
 */
static void *
cwc_thread(void *aux)
{
  cwc_service_t *ct;
  cs_card_data_t *cd;
  cwc_t *cwc = aux;
  int fd, d;
  char errbuf[100];
  mpegts_service_t *t;
  char hostname[256];
  int port;
  struct timespec ts;
  int attempts = 0;

  pthread_mutex_lock(&cwc_mutex);

  while(cwc->cwc_running) {
    
    while(cwc->cwc_running && cwc->cwc_enabled == 0)
      pthread_cond_wait(&cwc->cwc_cond, &cwc_mutex);
    if (cwc->cwc_running == 0) continue;

    snprintf(hostname, sizeof(hostname), "%s", cwc->cwc_hostname);
    port = cwc->cwc_port;

    tvhlog(LOG_INFO, "cwc", "Attemping to connect to %s:%d", hostname, port);

    pthread_mutex_unlock(&cwc_mutex);

    fd = tcp_connect(hostname, port, NULL, errbuf, sizeof(errbuf), 10);

    pthread_mutex_lock(&cwc_mutex);

    if(fd == -1) {
      attempts++;
      tvhlog(LOG_INFO, "cwc", 
	     "Connection attempt to %s:%d failed: %s",
	     hostname, port, errbuf);
    } else {

      if(cwc->cwc_running == 0) {
	close(fd);
	break;
      }

      tvhlog(LOG_INFO, "cwc", "Connected to %s:%d", hostname, port);
      attempts = 0;

      cwc->cwc_fd = fd;
      cwc->cwc_reconfigure = 0;

      cwc_session(cwc);

      cwc->cwc_fd = -1;
      close(fd);
      cwc->cwc_connected = 0;
      cwc_comet_status_update(cwc);
      tvhlog(LOG_INFO, "cwc", "Disconnected from %s:%i", 
             cwc->cwc_hostname, cwc->cwc_port);
    }

    if(cwc->cwc_running == 0) continue;
    if(attempts == 1) continue; // Retry immediately
    d = 3;

    ts.tv_sec = time(NULL) + d;
    ts.tv_nsec = 0;

    tvhlog(LOG_INFO, "cwc", 
	   "%s:%i: Automatic connection attempt in in %d seconds",
	   cwc->cwc_hostname, cwc->cwc_port, d);

    pthread_cond_timedwait(&cwc_config_changed, &cwc_mutex, &ts);
  }

  tvhlog(LOG_INFO, "cwc", "%s:%i destroyed",
         cwc->cwc_hostname, cwc->cwc_port);

  while((ct = LIST_FIRST(&cwc->cwc_services)) != NULL) {
    t = (mpegts_service_t *)ct->td_service;
    pthread_mutex_lock(&t->s_stream_mutex);
    cwc_service_destroy((th_descrambler_t *)&ct);
    pthread_mutex_unlock(&t->s_stream_mutex);
  }

  while((cd = LIST_FIRST(&cwc->cwc_cards)) != NULL) {
    LIST_REMOVE(cd, cs_card);
    descrambler_close_emm(cd->cwc_mux, cd, cd->cwc_caid);
    free(cd->cwc_providers);
    free(cd);
  }
  free((void *)cwc->cwc_password);
  free((void *)cwc->cwc_password_salted);
  free((void *)cwc->cwc_username);
  free((void *)cwc->cwc_comment);
  free((void *)cwc->cwc_hostname);
  free((void *)cwc->cwc_id);
  free((void *)cwc->cwc_viaccess_emm.shared_emm);
  free(cwc);

  pthread_mutex_unlock(&cwc_mutex);
  return NULL;
}


/**
 *
 */
static int
verify_provider(struct cs_card_data *pcard, uint32_t providerid)
{
  int i;

  if(providerid == 0)
    return 1;
  
  for(i = 0; i < pcard->cwc_num_providers; i++)
    if(providerid == pcard->cwc_providers[i].id)
      return 1;
  return 0;
}

/**
 *
 */
static void
cwc_emm_cache_insert(cwc_t *cwc, uint32_t crc)
{
  /* evict the oldest entry */
  cwc->cwc_emm_cache.cache[cwc->cwc_emm_cache.w] = crc;
  cwc->cwc_emm_cache.w = (cwc->cwc_emm_cache.w+1)&EMM_CACHE_MASK;
  if (cwc->cwc_emm_cache.n < EMM_CACHE_SIZE)
    cwc->cwc_emm_cache.n++;
}

static int
cwc_emm_cache_lookup(cwc_t *cwc, uint32_t crc)
{
  int i;
  for (i=0; i<cwc->cwc_emm_cache.n; i++)
    if (cwc->cwc_emm_cache.cache[i] == crc)
      return 1;
  return 0;
}


/**
 *
 */
static void
cwc_emm(void *opaque, int pid, const uint8_t *data, int len)
{
  struct cs_card_data *pcard = opaque;
  cwc_t *cwc;
  void *ca_update_id;

  if (data == NULL) {  /* end-of-data */
    pcard->cwc_mux = NULL;
    return;
  }
  if (pcard->cwc_mux == NULL)
    return;
  pthread_mutex_lock(&cwc_mutex);
  cwc          = pcard->cwc;
  ca_update_id = pcard->cwc_mux;
  if (cwc->cwc_forward_emm && cwc->cwc_writer_running) {
    if (cwc->cwc_emmex) {
      if (cwc->cwc_update_id != ca_update_id) {
        int64_t delta = getmonoclock() - cwc->cwc_update_time;
        if (delta < 25000000UL)		/* 25 seconds */
          goto end_of_job;
      }
      cwc->cwc_update_time = getmonoclock();
    }
    cwc->cwc_update_id = ca_update_id;
    switch (pcard->cwc_card_type) {
      case CARD_CONAX:
        cwc_emm_conax(cwc, pcard, data, len);
        break;
      case CARD_IRDETO:
        cwc_emm_irdeto(cwc, pcard, data, len);
        break;
      case CARD_SECA:
        cwc_emm_seca(cwc, pcard, data, len);
        break;
      case CARD_VIACCESS:
        cwc_emm_viaccess(cwc, pcard, data, len);
        break;
      case CARD_DRE:
        cwc_emm_dre(cwc, pcard, data, len);
        break;
      case CARD_NAGRA:
        cwc_emm_nagra(cwc, pcard, data, len);
        break;
      case CARD_NDS:
        cwc_emm_nds(cwc, pcard, data, len);
        break;
      case CARD_CRYPTOWORKS:
        cwc_emm_cryptoworks(cwc, pcard, data, len);
        break;
      case CARD_BULCRYPT:
        cwc_emm_bulcrypt(cwc, pcard, data, len);
        break;
      case CARD_UNKNOWN:
        break;
    }
  }
end_of_job:
  pthread_mutex_unlock(&cwc_mutex);
}


/**
 * conax emm handler
 */
void
cwc_emm_conax(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len)
{
  if (data[0] == 0x82) {
    int i;
    for (i=0; i < pcard->cwc_num_providers; i++) {
      if (memcmp(&data[3], &pcard->cwc_providers[i].sa[1], 7) == 0) {
        cwc_send_msg(cwc, data, len, 0, 1, 0, 0);
        break;
      }
    }
  }
}


/**
 * irdeto emm handler
 * inspired by opensasc-ng, https://opensvn.csie.org/traccgi/opensascng/
 */
void
cwc_emm_irdeto(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len)
{
  int emm_mode = data[3] >> 3;
  int emm_len = data[3] & 0x07;
  int match = 0;
  
  if (emm_mode & 0x10){
    // try to match card
    match = (emm_mode == pcard->cwc_ua[4] &&
             (!emm_len || // zero length
              !memcmp(&data[4], &pcard->cwc_ua[5], emm_len))); // exact match
  } else {
    // try to match provider
    int i;
    for(i=0; i < pcard->cwc_num_providers; i++) {
      match = (emm_mode == pcard->cwc_providers[i].sa[4] &&
               (!emm_len || // zero length
                !memcmp(&data[4], &pcard->cwc_providers[i].sa[5], emm_len)));
      // exact match
      if (match) break;
    }
  }
  
  if (match)
    cwc_send_msg(cwc, data, len, 0, 1, 0, 0);
}


/**
 * seca emm handler
 * inspired by opensasc-ng, https://opensvn.csie.org/traccgi/opensascng/
 */
void
cwc_emm_seca(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len)
{
  int match = 0;

  if (data[0] == 0x82) {                    //unique emm
    if (memcmp(&data[3], &pcard->cwc_ua[2], 6) == 0) {
      match = 1;
    }
  } 
  else if (data[0] == 0x84) {               //shared emm
    /* XXX this part is untested but should do no harm */
    int i;
    for (i=0; i < pcard->cwc_num_providers; i++) {
      if (memcmp(&data[5], &pcard->cwc_providers[i].sa[5], 3) == 0) {
        match = 1;
        break;
      }
    }
  }
  else if (data[0] == 0x83) {               //global emm -> seca3
    match = 1;
  }

  if (match)
    cwc_send_msg(cwc, data, len, 0, 1, 0, 0);
}

/**
 * viaccess emm handler
 * inspired by opensasc-ng, https://opensvn.csie.org/traccgi/opensascng/
 */
static
const uint8_t * nano_start(const uint8_t * data)
{
  switch(data[0]) {
  case 0x88: return &data[8];
  case 0x8e: return &data[7];
  case 0x8c:
  case 0x8d: return &data[3];
  case 0x80:
  case 0x81: return &data[4];
  }
  return NULL;
}

static
const uint8_t * nano_checknano90fromnano(const uint8_t * data)
{
  if(data && data[0]==0x90 && data[1]==0x03) return data;
  return 0;
}

static
const uint8_t * nano_checknano90(const uint8_t * data)
{
  return nano_checknano90fromnano(nano_start(data));
}

static
int sort_nanos(uint8_t *dest, const uint8_t *src, int len)
{
  int w = 0, c = -1;

  while (1) {
    int j, n;
    n = 0x100;
    for (j = 0; j < len;) {
      int l = src[j + 1] + 2;
      if (src[j] == c) {
	if (w + l > len) {
	  return -1;
	}
	memcpy(dest + w, src + j, l);
	w += l;
      }
      else if (src[j] > c && src[j] < n) {
	n = src[j];
      }
      j += l;
    }
    if (n == 0x100) {
      break;
    }
    c = n;
  }
  return 0;
}

static int via_provider_id(const uint8_t * data)
{
  const uint8_t * tmp;
  tmp = nano_checknano90(data);
  if (!tmp) return 0;
  return (tmp[2] << 16) | (tmp[3] << 8) | (tmp[4]&0xf0);
}


void
cwc_emm_viaccess(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int mlen)
{
  /* Get SCT len */
  int len = 3 + ((data[1] & 0x0f) << 8) + data[2];

  switch (data[0])
    {
    case 0x8c:
    case 0x8d:
    {
      int match = 0;
      int i;
      uint32_t id;
      
      /* Match provider id */
      id = via_provider_id(data);
      if (!id) break;
      
      for(i = 0; i < pcard->cwc_num_providers; i++)
        if(pcard->cwc_providers[i].id == id) {
          match = 1;
          break;
        }
      if (!match) break;
      
      if (data[0] != cwc->cwc_viaccess_emm.shared_toggle) {
        cwc->cwc_viaccess_emm.shared_len = 0;
        free(cwc->cwc_viaccess_emm.shared_emm);
        cwc->cwc_viaccess_emm.shared_emm = (uint8_t*)malloc(len);
        if (cwc->cwc_viaccess_emm.shared_emm) {
          cwc->cwc_viaccess_emm.shared_len = len;
          memcpy(cwc->cwc_viaccess_emm.shared_emm, data, len);
          cwc->cwc_viaccess_emm.ca_update_id = cwc->cwc_update_id;
        }
        cwc->cwc_viaccess_emm.shared_toggle = data[0];
      }
    }
      break;
    case 0x8e:
      if (cwc->cwc_viaccess_emm.shared_emm &&
          cwc->cwc_viaccess_emm.ca_update_id == cwc->cwc_update_id) {
	int match = 0;
	int i;
	/* Match SA and provider in shared */
	for(i = 0; i < pcard->cwc_num_providers; i++) {
	  if(memcmp(&data[3],&pcard->cwc_providers[i].sa[4], 3)) continue;
	  if((data[6]&2)) continue;
	  if(via_provider_id(cwc->cwc_viaccess_emm.shared_emm) != pcard->cwc_providers[i].id) continue;
	  match = 1;
	  break;
	}
	if (!match) break;

	uint8_t * tmp = alloca(len + cwc->cwc_viaccess_emm.shared_len);
	const uint8_t * ass = nano_start(data);
	len -= (ass - data);
	if((data[6] & 2) == 0)  {
	  int addrlen = len - 8;
	  len=0;
	  tmp[len++] = 0x9e;
	  tmp[len++] = addrlen;
	  memcpy(&tmp[len], &ass[0], addrlen); len += addrlen;
	  tmp[len++] = 0xf0;
	  tmp[len++] = 0x08;
	  memcpy(&tmp[len],&ass[addrlen],8); len += 8;
	} else {
	  memcpy(tmp, ass, len);
	}
	ass = nano_start(cwc->cwc_viaccess_emm.shared_emm);
	int l = cwc->cwc_viaccess_emm.shared_len - (ass - cwc->cwc_viaccess_emm.shared_emm);
	memcpy(&tmp[len], ass, l); len += l;

	uint8_t *ass2 = (uint8_t*) alloca(len+7);
	if(ass2) {
	  uint32_t crc;

	  memcpy(ass2, data, 7);
	  if (sort_nanos(ass2 + 7, tmp, len)) {
	    return;
	  }

	  /* Set SCT len */
	  len += 4;
	  ass2[1] = (len>>8) | 0x70;
	  ass2[2] = len & 0xff;
	  len += 3;

	  crc = tvh_crc32(ass2, len, 0xffffffff);
	  if (!cwc_emm_cache_lookup(cwc, crc)) {
	    tvhlog(LOG_DEBUG, "cwc",
		   "Send EMM "
		   "%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x"
		   "...%02x.%02x.%02x.%02x",
		   ass2[0], ass2[1], ass2[2], ass2[3],
		   ass2[4], ass2[5], ass2[6], ass2[7],
		   ass2[len-4], ass2[len-3], ass2[len-2], ass2[len-1]);
	    cwc_send_msg(cwc, ass2, len, 0, 1, 0, 0);
	    cwc_emm_cache_insert(cwc, crc);
	  }
	}
      }
      break;
    }
}

/**
 * t->s_streaming_mutex is held
 */
static void
cwc_table_input(void *opaque, int pid, const uint8_t *data, int len)
{
  cwc_service_t *ct = opaque;
  elementary_stream_t *st = ct->cs_estream;
  mpegts_service_t *t = (mpegts_service_t*)ct->td_service;
  uint16_t sid = t->s_dvb_service_id;
  cwc_t *cwc = ct->cs_cwc;
  int channel;
  int section;
  ecm_pid_t *ep;
  ecm_section_t *es;
  char chaninfo[32];
  caid_t *c;

  if (data == NULL)
    return;

  if (ct->td_keystate == DS_IDLE)
    return;

  if(len > 4096)
    return;

  if((data[0] & 0xf0) != 0x80)
    return;

  LIST_FOREACH(ep, &ct->cs_pids, ep_link) {
    if(ep->ep_pid == pid)
      break;
  }

  pthread_mutex_lock(&t->s_stream_mutex);
  if(ep == NULL) {
    if (ct->ecm_state == ECM_RESET) {
      ct->ecm_state = ECM_INIT;
      ct->cs_channel = -1;
      t->s_dvb_prefcapid = 0;
      tvhlog(LOG_DEBUG, "cwc", "Reset after unexpected or no reply for service \"%s\"", t->s_dvb_svcname);
    }

    if (ct->ecm_state == ECM_INIT) {
      // Validate prefered ECM PID
      if(t->s_dvb_prefcapid != 0) {
        struct elementary_stream *prefca
          = service_stream_find((service_t*)t, t->s_dvb_prefcapid);
        if (!prefca || prefca->es_type != SCT_CA) {
          tvhlog(LOG_DEBUG, "cwc", "Invalid prefered ECM (PID %d) found for service \"%s\"", t->s_dvb_prefcapid, t->s_dvb_svcname);
          t->s_dvb_prefcapid = 0;
        }
      }

      if(t->s_dvb_prefcapid == pid) {
        ep = calloc(1, sizeof(ecm_pid_t));
        ep->ep_pid = t->s_dvb_prefcapid;
        LIST_INSERT_HEAD(&ct->cs_pids, ep, ep_link);
        tvhlog(LOG_DEBUG, "cwc", "Insert prefered ECM (PID %d) for service \"%s\"", t->s_dvb_prefcapid, t->s_dvb_svcname);
      }
      else if(t->s_dvb_prefcapid == 0) {
          ep = calloc(1, sizeof(ecm_pid_t));
          ep->ep_pid = pid;
          LIST_INSERT_HEAD(&ct->cs_pids, ep, ep_link);
          tvhlog(LOG_DEBUG, "cwc", "Insert new ECM (PID %d) for service \"%s\"", pid, t->s_dvb_svcname);
      }
    }
  }
  pthread_mutex_unlock(&t->s_stream_mutex);

  if(ep == NULL)
    return;

  struct cs_card_data *pcard = NULL;
  int i_break = 0;
  LIST_FOREACH(c, &st->es_caids, link) {
    LIST_FOREACH(pcard,&cwc->cwc_cards, cs_card) {
      if(pcard->cwc_caid == c->caid && verify_provider(pcard, c->providerid)){
        i_break = 1;
        break;
      }
    }
    if (i_break == 1) break;
  }

  if(c == NULL)
    return;

  switch(data[0]) {
    case 0x80:
    case 0x81:
      /* ECM */
      
      if(pcard->cwc_caid >> 8 == 6) {
        ep->ep_last_section = data[5];
        section = data[4];
        
      } else {
        ep->ep_last_section = 0;
        section = 0;
      }
      
      channel = pid;
      snprintf(chaninfo, sizeof(chaninfo), " (PID %d)", channel);
      
      if(ep->ep_sections[section] == NULL)
        ep->ep_sections[section] = calloc(1, sizeof(ecm_section_t));
      
      es = ep->ep_sections[section];
      
      if(es->es_ecmsize == len && !memcmp(es->es_ecm, data, len))
        break; /* key already sent */
      
      if(cwc->cwc_fd == -1) {
        // New key, but we are not connected (anymore), can not descramble
        ct->td_keystate = DS_UNKNOWN;
        break;
      }
      es->es_channel = channel;
      es->es_section = section;
      es->es_pending = 1;
      
      memcpy(es->es_ecm, data, len);
      es->es_ecmsize = len;
      
      
      if(ct->cs_channel >= 0 && channel != -1 &&
         ct->cs_channel != channel) {
        tvhlog(LOG_DEBUG, "cwc", "Filtering ECM (PID %d)", channel);
        return;
      }
      
      es->es_seq = cwc_send_msg(cwc, data, len, sid, 1, c->caid, c->providerid);
      
      tvhlog(LOG_DEBUG, "cwc",
             "Sending ECM%s section=%d/%d, for service \"%s\" (seqno: %d)",
             chaninfo, section, ep->ep_last_section, t->s_dvb_svcname, es->es_seq);
      es->es_time = getmonoclock();
      break;
      
    default:
      /* EMM */
      if (cwc->cwc_forward_emm)
        cwc_send_msg(cwc, data, len, sid, 1, 0, 0);
      break;
  }
}

/**
 * dre emm handler
 */
void
cwc_emm_dre(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len)
{
  int match = 0;

  if (data[0] == 0x87) {
    if (memcmp(&data[3], &pcard->cwc_ua[4], 4) == 0) {
      match = 1;
    }
  } 
  else if (data[0] == 0x86) {
    int i;
    for (i=0; i < pcard->cwc_num_providers; i++) {
      if (memcmp(&data[40], &pcard->cwc_providers[i].sa[4], 4) == 0) {
        /*      if (memcmp(&data[3], &cwc->cwc_providers[i].sa[4], 1) == 0) { */
        match = 1;
        break;
      }
    }
  }

  if (match)
    cwc_send_msg(cwc, data, len, 0, 1, 0, 0);
}

void
cwc_emm_nagra(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len)
{
  int match = 0;
  unsigned char hexserial[4];

  if (data[0] == 0x83) {      // unique|shared
    hexserial[0] = data[5];
    hexserial[1] = data[4];
    hexserial[2] = data[3];
    hexserial[3] = data[6];
    if (memcmp(hexserial, &pcard->cwc_ua[4], (data[7] == 0x10) ? 3 : 4) == 0)
      match = 1;
  } 
  else if (data[0] == 0x82) { // global
    match = 1;
  }

  if (match)
    cwc_send_msg(cwc, data, len, 0, 1, 0, 0);
}

void
cwc_emm_nds(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len)
{
  int match = 0;
  int i;
  int serial_count = ((data[3] >> 4) & 3) + 1;
  unsigned char emmtype = (data[3] & 0xC0) >> 6;

  if (emmtype == 1 || emmtype == 2) {  // unique|shared
    for (i = 0; i < serial_count; i++) {
      if (memcmp(&data[i * 4 + 4], &pcard->cwc_ua[4], 5 - emmtype) == 0) {
        match = 1;
        break;
      }
    }
  }
  else if (emmtype == 0) {  // global
    match = 1;
  }

  if (match)
    cwc_send_msg(cwc, data, len, 0, 1, 0, 0);
}

void
cwc_emm_cryptoworks(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len)
{
  int match = 0;

  switch (data[0]) {
  case 0x82: /* unique */
    match = len >= 10 && memcmp(data + 5, pcard->cwc_ua + 3, 5) == 0;
    break;
  case 0x84: /* emm-sh */
    if (len >= 9 && memcmp(data + 5, pcard->cwc_ua + 3, 4) == 0) {
      if (cwc->cwc_cryptoworks_emm.shared_emm) {
        free(cwc->cwc_cryptoworks_emm.shared_emm);
        cwc->cwc_cryptoworks_emm.shared_len = 0;
      }
      cwc->cwc_cryptoworks_emm.shared_emm = malloc(len);
      if (cwc->cwc_cryptoworks_emm.shared_emm) {
        cwc->cwc_cryptoworks_emm.shared_len = len;
        memcpy(cwc->cwc_cryptoworks_emm.shared_emm, data, len);
        cwc->cwc_cryptoworks_emm.ca_update_id = cwc->cwc_update_id;
      }
    }
    break;
  case 0x86: /* emm-sb */ 
    if (cwc->cwc_cryptoworks_emm.shared_emm &&
        cwc->cwc_cryptoworks_emm.ca_update_id == cwc->cwc_update_id) {
      /* python: EMM_SH[0:12] + EMM_SB[5:-1] + EMM_SH[12:-1] */
      uint32_t elen = len - 5 + cwc->cwc_cryptoworks_emm.shared_len - 12;
      uint8_t *tmp = malloc(elen);
      uint8_t *composed = tmp ? malloc(elen + 12) : NULL;
      if (composed) {
        memcpy(tmp, data + 5, len - 5);
        memcpy(tmp + len - 5, cwc->cwc_cryptoworks_emm.shared_emm + 12,
                                cwc->cwc_cryptoworks_emm.shared_len - 12);
        memcpy(composed, cwc->cwc_cryptoworks_emm.shared_emm, 12);
        sort_nanos(composed + 12, tmp, elen);
        composed[1] = ((elen + 9) >> 8) | 0x70;
        composed[2] = (elen + 9) & 0xff;
        cwc_send_msg(cwc, composed, elen + 12, 0, 1, 0, 0);
        free(composed);
        free(tmp);
      } else if (tmp)
        free(tmp);

      free(cwc->cwc_cryptoworks_emm.shared_emm);
      cwc->cwc_cryptoworks_emm.shared_emm = NULL;
      cwc->cwc_cryptoworks_emm.shared_len = 0;
    }
    break;
  case 0x88: /* global */
  case 0x89: /* global */
    match = 1;
    break;
  default:
    break;
  }

  if (match)
    cwc_send_msg(cwc, data, len, 0, 1, 0, 0);
}

void
cwc_emm_bulcrypt(cwc_t *cwc, struct cs_card_data *pcard, const uint8_t *data, int len)
{
  int match = 0;

  switch (data[0]) {
  case 0x82: /* unique - bulcrypt (1 card) */
  case 0x8a: /* unique - polaris  (1 card) */
  case 0x85: /* unique - bulcrypt (4 cards) */
  case 0x8b: /* unique - polaris  (4 cards) */
    match = len >= 10 && memcmp(data + 3, pcard->cwc_ua + 2, 3) == 0;
    break;
  case 0x84: /* shared - (1024 cards) */
    match = len >= 10 && memcmp(data + 3, pcard->cwc_ua + 2, 2) == 0;
    break;
  }

  if (match)
    cwc_send_msg(cwc, data, len, 0, 1, 0, 0);
}

/**
 * cwc_mutex is held
 * s_stream_mutex is held
 */
static void 
cwc_service_destroy(th_descrambler_t *td)
{
  cwc_service_t *ct = (cwc_service_t *)td;
  ecm_pid_t *ep;
  int i;

  descrambler_close_pid(ct->cs_mux, ct, ct->cs_estream->es_pid);

  while((ep = LIST_FIRST(&ct->cs_pids)) != NULL) {
    for(i = 0; i < 256; i++)
      free(ep->ep_sections[i]);
    LIST_REMOVE(ep, ep_link);
    free(ep);
  }

  LIST_REMOVE(td, td_service_link);

  LIST_REMOVE(ct, cs_link);

  tvhcsa_destroy(&ct->cs_csa);
  free(ct->td_nicename);
  free(ct);
}

/**
 * Check if our CAID's matches, and if so, link
 *
 * global_lock is held. Not that we care about that, but either way, it is.
 */
void
cwc_service_start(service_t *t)
{
  cwc_t *cwc;
  cwc_service_t *ct;
  th_descrambler_t *td;
  elementary_stream_t *st;
  caid_t *c;
  struct cs_card_data *pcard;
  char buf[512];

  extern const idclass_t mpegts_service_class;
  if (!idnode_is_instance(&t->s_id, &mpegts_service_class))
    return;

  pthread_mutex_lock(&cwc_mutex);
  TAILQ_FOREACH(cwc, &cwcs, cwc_link) {
    LIST_FOREACH(ct, &cwc->cwc_services, cs_link) {
      if (ct->td_service == t && ct->cs_cwc == cwc)
        break;
    }
    LIST_FOREACH(pcard, &cwc->cwc_cards, cs_card) {
      if (pcard->cwc_caid == 0) continue;
      TAILQ_FOREACH(st, &t->s_components, es_link) {
        LIST_FOREACH(c, &st->es_caids, link) {
          if (c->caid == pcard->cwc_caid)
            break;
        }
        if (c) break;
      }
      if (st) break;
    }
    if (!pcard) {
      if (ct) cwc_service_destroy((th_descrambler_t*)ct);
      continue;
    }

    if (ct) continue;

    ct                   = calloc(1, sizeof(cwc_service_t));
    ct->cs_cwc           = cwc;
    ct->cs_channel       = -1;
    ct->cs_estream       = st;
    ct->cs_mux           = ((mpegts_service_t *)t)->s_dvb_mux;
    ct->ecm_state        = ECM_INIT;

    td                   = (th_descrambler_t *)ct;
    tvhcsa_init(td->td_csa = &ct->cs_csa);
    snprintf(buf, sizeof(buf), "cwc-%s-%i", cwc->cwc_hostname, cwc->cwc_port);
    td->td_nicename      = strdup(buf);
    td->td_service       = t;
    td->td_stop          = cwc_service_destroy;
    LIST_INSERT_HEAD(&t->s_descramblers, td, td_service_link);

    LIST_INSERT_HEAD(&cwc->cwc_services, ct, cs_link);

    descrambler_open_pid(ct->cs_mux, ct,
                         DESCRAMBLER_ECM_PID(ct->cs_estream->es_pid),
                         cwc_table_input, t);

    tvhlog(LOG_DEBUG, "cwc", "%s using CWC %s:%d",
	   service_nicename(t), cwc->cwc_hostname, cwc->cwc_port);

  }
  pthread_mutex_unlock(&cwc_mutex);
}


/**
 *
 */
static void
cwc_destroy(cwc_t *cwc)
{
  TAILQ_REMOVE(&cwcs, cwc, cwc_link);  
  cwc->cwc_running = 0;
  pthread_cond_signal(&cwc->cwc_cond);
}

/**
 *
 */
void
cwc_caid_update(mpegts_mux_t *mux, uint16_t caid, uint16_t pid, int valid)
{
  cwc_t *cwc;
  struct cs_card_data *pcard;

  tvhtrace("cwc",
           "caid update event - mux %p caid %04x (%i) pid %04x (%i) valid %i",
           mux, caid, caid, pid, pid, valid);
  pthread_mutex_lock(&cwc_mutex);
  TAILQ_FOREACH(cwc, &cwcs, cwc_link) {
    if (cwc->cwc_running) {
      LIST_FOREACH(pcard, &cwc->cwc_cards, cs_card) {
        if (pcard->cwc_caid == caid) {
          if (pcard->cwc_mux != mux) continue;
          if (valid) {
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
  }
  pthread_mutex_unlock(&cwc_mutex);
}


/**
 *
 */
static cwc_t *
cwc_entry_find(const char *id, int create)
{
  char buf[20];
  cwc_t *cwc;
  static int tally;

  if(id != NULL) {
    TAILQ_FOREACH(cwc, &cwcs, cwc_link) {
      if(!strcmp(cwc->cwc_id, id))
        return cwc;
    }
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

  cwc = calloc(1, sizeof(cwc_t));
  pthread_cond_init(&cwc->cwc_cond, NULL);
  cwc->cwc_id = strdup(id); 
  cwc->cwc_running = 1;
  TAILQ_INSERT_TAIL(&cwcs, cwc, cwc_link);  

  tvhthread_create(&cwc->cwc_tid, NULL, cwc_thread, cwc, 0);

  return cwc;
}


/**
 *
 */
static htsmsg_t *
cwc_record_build(cwc_t *cwc)
{
  htsmsg_t *e = htsmsg_create_map();
  char buf[100];

  htsmsg_add_str(e, "id", cwc->cwc_id);
  htsmsg_add_u32(e, "enabled",  !!cwc->cwc_enabled);
  htsmsg_add_u32(e, "connected", !!cwc->cwc_connected);

  htsmsg_add_str(e, "hostname", cwc->cwc_hostname ?: "");
  htsmsg_add_u32(e, "port", cwc->cwc_port);

  htsmsg_add_str(e, "username", cwc->cwc_username ?: "");
  htsmsg_add_str(e, "password", cwc->cwc_password ?: "");
  snprintf(buf, sizeof(buf),
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
  
  htsmsg_add_str(e, "deskey", buf);
  htsmsg_add_u32(e, "emm", cwc->cwc_emm);
  htsmsg_add_u32(e, "emmex", cwc->cwc_emmex);
  htsmsg_add_str(e, "comment", cwc->cwc_comment ?: "");

  return e;
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
static htsmsg_t *
cwc_entry_update(void *opaque, const char *id, htsmsg_t *values, int maycreate)
{
  cwc_t *cwc;
  const char *s;
  uint32_t u32;
  uint8_t key[14];
  int u, l, i;

  if((cwc = cwc_entry_find(id, maycreate)) == NULL)
    return NULL;

  if((s = htsmsg_get_str(values, "username")) != NULL) {
    free(cwc->cwc_username);
    cwc->cwc_username = strdup(s);
  }

  if((s = htsmsg_get_str(values, "password")) != NULL) {
    free(cwc->cwc_password);
    free(cwc->cwc_password_salted);
    cwc->cwc_password = strdup(s);
    cwc->cwc_password_salted = crypt_md5(s, "$1$abcdefgh$");
  }

  if((s = htsmsg_get_str(values, "comment")) != NULL) {
    free(cwc->cwc_comment);
    cwc->cwc_comment = strdup(s);
  }

  if((s = htsmsg_get_str(values, "hostname")) != NULL) {
    free(cwc->cwc_hostname);
    cwc->cwc_hostname = strdup(s);
  }

  if(!htsmsg_get_u32(values, "enabled", &u32))
    cwc->cwc_enabled = u32;

  if(!htsmsg_get_u32(values, "port", &u32))
    cwc->cwc_port = u32;

  if((s = htsmsg_get_str(values, "deskey")) != NULL) {
    for(i = 0; i < 14; i++) {
      while(*s != 0 && !isxdigit(*s)) s++;
      if(*s == 0)
	break;
      u = nibble(*s++);
      while(*s != 0 && !isxdigit(*s)) s++;
      if(*s == 0)
	break;
      l = nibble(*s++);
      key[i] = (u << 4) | l;
    }
    memcpy(cwc->cwc_confedkey, key, 14);
  }

  if(!htsmsg_get_u32(values, "emm", &u32))
    cwc->cwc_emm = u32;

  if(!htsmsg_get_u32(values, "emmex", &u32))
    cwc->cwc_emmex = u32;

  cwc->cwc_reconfigure = 1;

  if(cwc->cwc_fd != -1)
    shutdown(cwc->cwc_fd, SHUT_RDWR);

  pthread_cond_signal(&cwc->cwc_cond);

  pthread_cond_broadcast(&cwc_config_changed);

  return cwc_record_build(cwc);
}
  


/**
 *
 */
static int
cwc_entry_delete(void *opaque, const char *id)
{
  cwc_t *cwc;

  pthread_cond_broadcast(&cwc_config_changed);

  if((cwc = cwc_entry_find(id, 0)) == NULL)
    return -1;
  cwc_destroy(cwc);
  return 0;
}


/**
 *
 */
static htsmsg_t *
cwc_entry_get_all(void *opaque)
{
  htsmsg_t *r = htsmsg_create_list();
  cwc_t *cwc;

  TAILQ_FOREACH(cwc, &cwcs, cwc_link)
    htsmsg_add_msg(r, NULL, cwc_record_build(cwc));

  return r;
}

/**
 *
 */
static htsmsg_t *
cwc_entry_get(void *opaque, const char *id)
{
  cwc_t *cwc;


  if((cwc = cwc_entry_find(id, 0)) == NULL)
    return NULL;
  return cwc_record_build(cwc);
}

/**
 *
 */
/**
 *
 */
static htsmsg_t *
cwc_entry_create(void *opaque)
{
  pthread_cond_broadcast(&cwc_config_changed);
  return cwc_record_build(cwc_entry_find(NULL, 1));
}




/**
 *
 */
static const dtable_class_t cwc_dtc = {
  .dtc_record_get     = cwc_entry_get,
  .dtc_record_get_all = cwc_entry_get_all,
  .dtc_record_create  = cwc_entry_create,
  .dtc_record_update  = cwc_entry_update,
  .dtc_record_delete  = cwc_entry_delete,
  .dtc_read_access = ACCESS_ADMIN,
  .dtc_write_access = ACCESS_ADMIN,
  .dtc_mutex = &cwc_mutex,
};



/**
 *
 */
void
cwc_init(void)
{
  dtable_t *dt;

  TAILQ_INIT(&cwcs);
  pthread_mutex_init(&cwc_mutex, NULL);
  pthread_cond_init(&cwc_config_changed, NULL);

  dt = dtable_create(&cwc_dtc, "cwc", NULL);
  dtable_load(dt);
}


/**
 *
 */
void
cwc_done(void)
{
  cwc_t *cwc;
  pthread_t tid;

  dtable_delete("cwc");
  pthread_mutex_lock(&cwc_mutex);
  while ((cwc = TAILQ_FIRST(&cwcs)) != NULL) {
    tid = cwc->cwc_tid;
    cwc_destroy(cwc);
    pthread_mutex_unlock(&cwc_mutex);
    pthread_kill(tid, SIGTERM);
    pthread_join(tid, NULL);
    pthread_mutex_lock(&cwc_mutex);
  }
  pthread_mutex_unlock(&cwc_mutex);
}


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
