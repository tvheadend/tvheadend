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

#include "tvheadend.h"
#include "tcp.h"
#include "psi.h"
#include "tsdemux.h"
#include "ffdecsa/FFdecsa.h"
#include "cwc.h"
#include "notify.h"
#include "atomic.h"
#include "dtable.h"
#include "subscriptions.h"

#include <openssl/des.h>

/**
 *
 */

#define CWC_KEEPALIVE_INTERVAL 30

#define CWS_NETMSGSIZE 272
#define CWS_FIRSTCMDNO 0xe0

/**
 * cards for which emm updates are handled
 */
typedef enum {
  CARD_IRDETO,
  CARD_DRE,
  CARD_CONAX,
  CARD_SECA,
  CARD_VIACCESS,
  CARD_NAGRA,
  CARD_NDS,
  CARD_UNKNOWN
} card_type_t;

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
  th_descrambler_t cs_head;

  service_t *cs_service;

  struct cwc *cs_cwc;

  LIST_ENTRY(cwc_service) cs_link;

  int cs_okchannel;

  /**
   * Status of the key(s) in cs_keys
   */
  enum {
    CS_UNKNOWN,
    CS_RESOLVED,
    CS_FORBIDDEN
  } cs_keystate;

  void *cs_keys;


  uint8_t cs_cw[16];
  int cs_pending_cw_update;

  /**
   * CSA
   */
  int cs_cluster_size;
  uint8_t *cs_tsbcluster;
  int cs_fill;

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

/**
 *
 */
typedef struct cwc {
  int cwc_fd;
  int cwc_connected;

  int cwc_retry_delay;

  pthread_cond_t cwc_cond;

  pthread_mutex_t cwc_writer_mutex; 
  pthread_cond_t cwc_writer_cond; 
  int cwc_writer_running;
  struct cwc_message_queue cwc_writeq;

  TAILQ_ENTRY(cwc) cwc_link; /* Linkage protected via cwc_mutex */

  struct cwc_service_list cwc_services;

  uint16_t cwc_caid;

  int cwc_seq;

  DES_key_schedule cwc_k1, cwc_k2;

  uint8_t cwc_buf[256];
  int cwc_bufptr;

  /* Card Unique Address */
  uint8_t cwc_ua[8];

  /* Provider IDs */
  cwc_provider_t cwc_providers[256];
  int cwc_num_providers;

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
  } cwc_viaccess_emm;

  /* Card type */
  card_type_t cwc_card_type;
  
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

  const char *cwc_errtxt;

  int cwc_enabled;
  int cwc_running;
  int cwc_reconfigure;
} cwc_t;


/**
 *
 */

static void cwc_service_destroy(th_descrambler_t *td);
static void cwc_detect_card_type(cwc_t *cwc);
void cwc_emm_conax(cwc_t *cwc, uint8_t *data, int len);
void cwc_emm_irdeto(cwc_t *cwc, uint8_t *data, int len);
void cwc_emm_dre(cwc_t *cwc, uint8_t *data, int len);
void cwc_emm_seca(cwc_t *cwc, uint8_t *data, int len);
void cwc_emm_viaccess(cwc_t *cwc, uint8_t *data, int len);
void cwc_emm_nagra(cwc_t *cwc, uint8_t *data, int len);
void cwc_emm_nds(cwc_t *cwc, uint8_t *data, int len);


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
cwc_send_msg(cwc_t *cwc, const uint8_t *msg, size_t len, int sid, int enq)
{
  cwc_message_t *cm = malloc(sizeof(cwc_message_t));
  uint8_t *buf = cm->cm_data;
  int seq, n;

  if(len + 12 > CWS_NETMSGSIZE)
    return -1;

  memset(buf, 0, 12);
  memcpy(buf + 12, msg, len);

  len += 12;

  seq = atomic_add(&cwc->cwc_seq, 1);

  buf[2] = seq >> 8;
  buf[3] = seq;
  buf[4] = sid >> 8;
  buf[5] = sid;

  if((len = des_encrypt(buf, len, cwc)) < 0) {
    free(buf);
    return -1;
  }

  buf[0] = (len - 2) >> 8;
  buf[1] =  len - 2;


  if(enq) {
    cm->cm_len = len;
    pthread_mutex_lock(&cwc->cwc_writer_mutex);
    TAILQ_INSERT_TAIL(&cwc->cwc_writeq, cm, cm_link);
    pthread_cond_signal(&cwc->cwc_writer_cond);
    pthread_mutex_unlock(&cwc->cwc_writer_mutex);
  } else {
    n = write(cwc->cwc_fd, buf, len);
    if(n != len)
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

  cwc_send_msg(cwc, buf, 3, 0, 0);
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

  cwc_send_msg(cwc, buf, 3, 0, 0);
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
  cwc->cwc_caid = (msg[4] << 8) | msg[5];
  n = psi_caid2name(cwc->cwc_caid & 0xff00) ?: "Unknown";

  memcpy(cwc->cwc_ua, &msg[6], 8);

  tvhlog(LOG_INFO, "cwc", "%s: Connected as user 0x%02x "
	 "to a %s-card [0x%04x : %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x] "
	 "with %d providers", 
	 cwc->cwc_hostname,
	 msg[3], n, cwc->cwc_caid, 
	 cwc->cwc_ua[0], cwc->cwc_ua[1], cwc->cwc_ua[2], cwc->cwc_ua[3], cwc->cwc_ua[4], cwc->cwc_ua[5], cwc->cwc_ua[6], cwc->cwc_ua[7],
	 nprov);

  cwc_detect_card_type(cwc);

  msg  += 15;
  plen -= 12;

  cwc->cwc_num_providers = nprov;

  for(i = 0; i < nprov; i++) {
    cwc->cwc_providers[i].id = (msg[0] << 16) | (msg[1] << 8) | msg[2];
    cwc->cwc_providers[i].sa[0] = msg[3];
    cwc->cwc_providers[i].sa[1] = msg[4];
    cwc->cwc_providers[i].sa[2] = msg[5];
    cwc->cwc_providers[i].sa[3] = msg[6];
    cwc->cwc_providers[i].sa[4] = msg[7];
    cwc->cwc_providers[i].sa[5] = msg[8];
    cwc->cwc_providers[i].sa[6] = msg[9];
    cwc->cwc_providers[i].sa[7] = msg[10];

    tvhlog(LOG_INFO, "cwc", "%s: Provider ID #%d: 0x%06x %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x",
	   cwc->cwc_hostname, i + 1,
	   cwc->cwc_providers[i].id,
	   cwc->cwc_providers[i].sa[0],
	   cwc->cwc_providers[i].sa[1],
	   cwc->cwc_providers[i].sa[2],
	   cwc->cwc_providers[i].sa[3],
	   cwc->cwc_providers[i].sa[4],
	   cwc->cwc_providers[i].sa[5],
	   cwc->cwc_providers[i].sa[6],
	   cwc->cwc_providers[i].sa[7]);

    msg += 11;
  }

  cwc->cwc_forward_emm = 0;
  if (cwc->cwc_emm) {
    int emm_allowed = (cwc->cwc_ua[0] || cwc->cwc_ua[1] ||
		       cwc->cwc_ua[2] || cwc->cwc_ua[3] ||
		       cwc->cwc_ua[4] || cwc->cwc_ua[5] ||
		       cwc->cwc_ua[6] || cwc->cwc_ua[7]);

    if (!emm_allowed) {
      tvhlog(LOG_INFO, "cwc", 
	     "%s: Will not forward EMMs (not allowed by server)",
	     cwc->cwc_hostname);
    } else if (cwc->cwc_card_type != CARD_UNKNOWN) {
      tvhlog(LOG_INFO, "cwc", "%s: Will forward EMMs",
	     cwc->cwc_hostname);
      cwc->cwc_forward_emm = 1;
    } else {
      tvhlog(LOG_INFO, "cwc", 
	     "%s: Will not forward EMMs (unsupported CA system)",
	     cwc->cwc_hostname);
    }
  }

  return 0;
}

/**
 * Detects the cam card type
 * If you want to add another card, have a look at
 * http://www.dvbservices.com/identifiers/ca_system_id?page=3
 * 
 * based on the equivalent in sasc-ng
 */
static void
cwc_detect_card_type(cwc_t *cwc)
{
  uint8_t c_sys = cwc->cwc_caid >> 8;
		
  switch(c_sys) {
  case 0x17:
  case 0x06: 
    cwc->cwc_card_type = CARD_IRDETO;
    tvhlog(LOG_INFO, "cwc", "%s: irdeto card",
	   cwc->cwc_hostname);
    break;
  case 0x05:
    cwc->cwc_card_type = CARD_VIACCESS;
    tvhlog(LOG_INFO, "cwc", "%s: viaccess card",
	   cwc->cwc_hostname);
    break;
  case 0x0b:
    cwc->cwc_card_type = CARD_CONAX;
    tvhlog(LOG_INFO, "cwc", "%s: conax card",
	   cwc->cwc_hostname);
    break;
  case 0x01:
    cwc->cwc_card_type = CARD_SECA;
    tvhlog(LOG_INFO, "cwc", "%s: seca card",
	   cwc->cwc_hostname);
  case 0x4a:
    cwc->cwc_card_type = CARD_DRE;
    tvhlog(LOG_INFO, "cwc", "%s: dre card",
	   cwc->cwc_hostname);
    break;
  case 0x18:
    cwc->cwc_card_type = CARD_NAGRA;
    tvhlog(LOG_INFO, "cwc", "%s: nagra card",
	   cwc->cwc_hostname);
    break;
  case 0x09:
    cwc->cwc_card_type = CARD_NDS;
    tvhlog(LOG_INFO, "cwc", "%s: nds card",
	   cwc->cwc_hostname);
    break;
  default:
    cwc->cwc_card_type = CARD_UNKNOWN;
    break;
  }
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
 
  cwc_send_msg(cwc, buf, ul + pl + 3, 0, 0);
}



static void
handle_ecm_reply(cwc_service_t *ct, ecm_section_t *es, uint8_t *msg,
		 int len, int seq)
{
  service_t *t = ct->cs_service;
  ecm_pid_t *ep;
  char chaninfo[32];
  int i;
  int64_t delay = (getmonoclock() - es->es_time) / 1000LL; // in ms

  if(es->es_channel != -1) {
    snprintf(chaninfo, sizeof(chaninfo), " (channel %d)", es->es_channel);
  } else {
    chaninfo[0] = 0;
  }

  es->es_pending = 0;

  if(len < 19) {
    
    /* ERROR */

    if(ct->cs_okchannel == es->es_channel)
      ct->cs_okchannel = -1;

    if(ct->cs_keystate == CS_FORBIDDEN)
      return; // We already know it's bad

    es->es_nok = 1;

    tvhlog(LOG_DEBUG, "cwc", "Received NOK for service \"%s\"%s (seqno: %d "
	   "Req delay: %lld ms)", t->s_svcname, chaninfo, seq, delay);

    LIST_FOREACH(ep, &ct->cs_pids, ep_link) {
      for(i = 0; i <= ep->ep_last_section; i++)
	if(ep->ep_sections[i] == NULL || 
	   ep->ep_sections[i]->es_pending ||
	   ep->ep_sections[i]->es_nok == 0)
	  return;
    }
    tvhlog(LOG_ERR, "cwc",
	   "Can not descramble service \"%s\", access denied (seqno: %d "
	   "Req delay: %lld ms)",
	   t->s_svcname, seq, delay);
    ct->cs_keystate = CS_FORBIDDEN;
    return;

  } else {

    ct->cs_okchannel = es->es_channel;
    es->es_nok = 0;

    tvhlog(LOG_DEBUG, "cwc",
	   "Received ECM reply%s for service \"%s\" "
	   "even: %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x"
	   " odd: %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x (seqno: %d "
	   "Req delay: %lld ms)",
	   chaninfo,
	   t->s_svcname,
	   msg[3 + 0], msg[3 + 1], msg[3 + 2], msg[3 + 3], msg[3 + 4],
	   msg[3 + 5], msg[3 + 6], msg[3 + 7], msg[3 + 8], msg[3 + 9],
	   msg[3 + 10],msg[3 + 11],msg[3 + 12],msg[3 + 13],msg[3 + 14],
	   msg[3 + 15], seq, delay);
    
    if(ct->cs_keystate != CS_RESOLVED)
      tvhlog(LOG_INFO, "cwc",
	     "Obtained key for for service \"%s\" in %lld ms, from %s",
	     t->s_svcname, delay, ct->cs_cwc->cwc_hostname);
    
    ct->cs_keystate = CS_RESOLVED;
    memcpy(ct->cs_cw, msg + 3, 16);
    ct->cs_pending_cw_update = 1;
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
  int i;

  len -= 12;
  msg += 12;

  switch(msgtype) {
  case 0x80:
  case 0x81:
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
    break;
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
    tvhlog(LOG_INFO, "cwc", "%s: %s: Read error (header): %s",
	   cwc->cwc_hostname, state, strerror(r));
    return -1;
  }

  msglen = (buf[0] << 8) | buf[1];
  if(msglen >= CWS_NETMSGSIZE) {
    tvhlog(LOG_INFO, "cwc", "%s: %s: Invalid message size: %d",
	   cwc->cwc_hostname, state, msglen);
    return -1;
  }

  /* We expect the rest of the message to arrive fairly quick,
     so just wait 1 second here */

  if((r = cwc_read(cwc, cwc->cwc_buf + 2, msglen, 1000))) {
    tvhlog(LOG_INFO, "cwc", "%s: %s: Read error: %s",
	   cwc->cwc_hostname, state, strerror(r));
    return -1;
  }

  if((msglen = des_decrypt(cwc->cwc_buf, msglen + 2, cwc)) < 15) {
    tvhlog(LOG_INFO, "cwc", "%s: %s: Decrypt failed",
	   state, cwc->cwc_hostname);
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
      r = write(cwc->cwc_fd, cm->cm_data, cm->cm_len);
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
    tvhlog(LOG_INFO, "cwc", "%s: No login key received: %s",
	   cwc->cwc_hostname, strerror(r));
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
    tvhlog(LOG_INFO, "cwc", "%s: Login failed", cwc->cwc_hostname);
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
    tvhlog(LOG_INFO, "cwc", "%s: Card data request failed", cwc->cwc_hostname);
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
  pthread_create(&writer_thread_id, NULL, cwc_writer_thread, cwc);

  /**
   * Mainloop
   */
  while(!cwc_must_break(cwc)) {

    if((r = cwc_read_message(cwc, "Decoderloop", 
			     CWC_KEEPALIVE_INTERVAL * 2 * 1000)) < 0)
      break;
    cwc_running_reply(cwc, cwc->cwc_buf[12], cwc->cwc_buf, r);
  }

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
  cwc_t *cwc = aux;
  int fd, d;
  char errbuf[100];
  service_t *t;
  char hostname[256];
  int port;
  struct timespec ts;
  int attempts = 0;

  pthread_mutex_lock(&cwc_mutex);

  while(cwc->cwc_running) {
    
    while(cwc->cwc_running && cwc->cwc_enabled == 0)
      pthread_cond_wait(&cwc->cwc_cond, &cwc_mutex);

    snprintf(hostname, sizeof(hostname), "%s", cwc->cwc_hostname);
    port = cwc->cwc_port;

    tvhlog(LOG_INFO, "cwc", "Attemping to connect to %s:%d", hostname, port);

    pthread_mutex_unlock(&cwc_mutex);

    fd = tcp_connect(hostname, port, errbuf, sizeof(errbuf), 10);

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
      cwc->cwc_caid = 0;
      cwc->cwc_connected = 0;
      cwc_comet_status_update(cwc);
      tvhlog(LOG_INFO, "cwc", "Disconnected from %s",  cwc->cwc_hostname);
    }

    if(subscriptions_active()) {
      if(attempts == 1)
	continue; // Retry immediately
      d = 3;
    } else {
      d = 60;
    }

    ts.tv_sec = time(NULL) + d;
    ts.tv_nsec = 0;

    tvhlog(LOG_INFO, "cwc", 
	   "%s: Automatic connection attempt in in %d seconds",
	   cwc->cwc_hostname, d);

    pthread_cond_timedwait(&cwc_config_changed, &cwc_mutex, &ts);
  }

  tvhlog(LOG_INFO, "cwc", "%s destroyed", cwc->cwc_hostname);

  while((ct = LIST_FIRST(&cwc->cwc_services)) != NULL) {
    t = ct->cs_service;
    pthread_mutex_lock(&t->s_stream_mutex);
    cwc_service_destroy(&ct->cs_head);
    pthread_mutex_unlock(&t->s_stream_mutex);
  }

  free((void *)cwc->cwc_password);
  free((void *)cwc->cwc_password_salted);
  free((void *)cwc->cwc_username);
  free((void *)cwc->cwc_hostname);
  free(cwc);

  pthread_mutex_unlock(&cwc_mutex);
  return NULL;
}


/**
 *
 */
static int
verify_provider(cwc_t *cwc, uint32_t providerid)
{
  int i;

  if(providerid == 0)
    return 1;
  
  for(i = 0; i < cwc->cwc_num_providers; i++) 
    if(providerid == cwc->cwc_providers[i].id)
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
void
cwc_emm(uint8_t *data, int len)
{
  cwc_t *cwc;

  pthread_mutex_lock(&cwc_mutex);

  TAILQ_FOREACH(cwc, &cwcs, cwc_link) {
    if(cwc->cwc_forward_emm && cwc->cwc_writer_running) {
      switch (cwc->cwc_card_type) {
      case CARD_CONAX:
	cwc_emm_conax(cwc, data, len);
	break;
      case CARD_IRDETO:
	cwc_emm_irdeto(cwc, data, len);
	break;
      case CARD_SECA:
	cwc_emm_seca(cwc, data, len);
	break;
      case CARD_VIACCESS:
	cwc_emm_viaccess(cwc, data, len);
	break;
      case CARD_DRE:
	cwc_emm_dre(cwc, data, len);
	break;
      case CARD_NAGRA:
	cwc_emm_nagra(cwc, data, len);
	break;
      case CARD_NDS:
	cwc_emm_nds(cwc, data, len);
	break;
      case CARD_UNKNOWN:
	break;
      }
    }
  }
  pthread_mutex_unlock(&cwc_mutex);
}


/**
 * conax emm handler
 */
void
cwc_emm_conax(cwc_t *cwc, uint8_t *data, int len)
{
  if (data[0] == 0x82) {
    int i;
    for (i=0; i < cwc->cwc_num_providers; i++) {
      if (memcmp(&data[3], &cwc->cwc_providers[i].sa[1], 7) == 0) {
        cwc_send_msg(cwc, data, len, 0, 1);
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
cwc_emm_irdeto(cwc_t *cwc, uint8_t *data, int len)
{
  int emm_mode = data[3] >> 3;
  int emm_len = data[3] & 0x07;
  int match = 0;
  
  if (emm_mode & 0x10){
    // try to match card
    match = (emm_mode == cwc->cwc_ua[4] && 
	     (!emm_len || // zero length
	      !memcmp(&data[4], &cwc->cwc_ua[5], emm_len))); // exact match
  } else {
    // try to match provider
    int i;
    for(i=0; i < cwc->cwc_num_providers; i++) {
      match = (emm_mode == cwc->cwc_providers[i].sa[4] && 
	       (!emm_len || // zero length
		!memcmp(&data[4], &cwc->cwc_providers[i].sa[5], emm_len)));
      // exact match
      if (match) break;
    }
  }
  
  if (match)
    cwc_send_msg(cwc, data, len, 0, 1);
}


/**
 * seca emm handler
 * inspired by opensasc-ng, https://opensvn.csie.org/traccgi/opensascng/
 */
void
cwc_emm_seca(cwc_t *cwc, uint8_t *data, int len)
{
  int match = 0;

  if (data[0] == 0x82) {
    if (memcmp(&data[3], &cwc->cwc_ua[2], 6) == 0) {
      match = 1;
    }
  } 
  else if (data[0] == 0x84) {
    /* XXX this part is untested but should do no harm */
    int i;
    for (i=0; i < cwc->cwc_num_providers; i++) {
      if (memcmp(&data[5], &cwc->cwc_providers[i].sa[5], 3) == 0) {
        match = 1;
        break;
      }
    }
  }

  if (match)
    cwc_send_msg(cwc, data, len, 0, 1);
}

/**
 * viaccess emm handler
 * inspired by opensasc-ng, https://opensvn.csie.org/traccgi/opensascng/
 */
static
uint8_t * nano_start(uint8_t * data)
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
uint8_t * nano_checknano90fromnano(uint8_t * data)
{
  if(data && data[0]==0x90 && data[1]==0x03) return data;
  return 0;
}

static
uint8_t * nano_checknano90(uint8_t * data)
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

static int via_provider_id(uint8_t * data)
{
  const uint8_t * tmp;
  tmp = nano_checknano90(data);
  if (!tmp) return 0;
  return (tmp[2] << 16) | (tmp[3] << 8) | (tmp[4]&0xf0);
}


void
cwc_emm_viaccess(cwc_t *cwc, uint8_t *data, int mlen)
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

	for(i = 0; i < cwc->cwc_num_providers; i++)
	  if(cwc->cwc_providers[i].id == id) {
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
	  }
	  cwc->cwc_viaccess_emm.shared_toggle = data[0];
	}
      }
      break;
    case 0x8e:
      if (cwc->cwc_viaccess_emm.shared_emm) {
	int match = 0;
	int i;
	/* Match SA and provider in shared */
	for(i = 0; i < cwc->cwc_num_providers; i++) {
	  if(memcmp(&data[3],&cwc->cwc_providers[i].sa[4], 3)) continue;
	  if((data[6]&2)) continue;
	  if(via_provider_id(cwc->cwc_viaccess_emm.shared_emm) != cwc->cwc_providers[i].id) continue;
	  match = 1;
	  break;
	}
	if (!match) break;

	uint8_t * tmp = alloca(len + cwc->cwc_viaccess_emm.shared_len);
	uint8_t * ass = nano_start(data);
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

	ass = (uint8_t*) alloca(len+7);
	if(ass) {
	  uint32_t crc;

	  memcpy(ass, data, 7);
	  if (sort_nanos(ass + 7, tmp, len)) {
	    return;
	  }

	  /* Set SCT len */
	  len += 4;
	  ass[1] = (len>>8) | 0x70;
	  ass[2] = len & 0xff;
	  len += 3;

	  crc = crc32(ass, len, 0xffffffff);
	  if (!cwc_emm_cache_lookup(cwc, crc)) {
	    tvhlog(LOG_DEBUG, "cwc",
		   "Send EMM "
		   "%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x"
		   "...%02x.%02x.%02x.%02x",
		   ass[0], ass[1], ass[2], ass[3],
		   ass[4], ass[5], ass[6], ass[7],
		   ass[len-4], ass[len-3], ass[len-2], ass[len-1]);
	    cwc_send_msg(cwc, ass, len, 0, 1);
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
cwc_table_input(struct th_descrambler *td, struct service *t,
		struct elementary_stream *st, const uint8_t *data, int len)
{
  cwc_service_t *ct = (cwc_service_t *)td;
  uint16_t sid = t->s_dvb_service_id;
  cwc_t *cwc = ct->cs_cwc;
  int channel;
  int section;
  ecm_pid_t *ep;
  ecm_section_t *es;
  char chaninfo[32];
  caid_t *c;

  if(len > 4096)
    return;

  if((data[0] & 0xf0) != 0x80)
    return;

  LIST_FOREACH(ep, &ct->cs_pids, ep_link) {
    if(ep->ep_pid == st->es_pid)
      break;
  }

  if(ep == NULL) {
    ep = calloc(1, sizeof(ecm_pid_t));
    ep->ep_pid = st->es_pid;
    LIST_INSERT_HEAD(&ct->cs_pids, ep, ep_link);
  }


  LIST_FOREACH(c, &st->es_caids, link) {
    if(cwc->cwc_caid == c->caid)
      break;
  }

  if(c == NULL)
    return;

  if(!verify_provider(cwc, c->providerid))
    return;

  switch(data[0]) {
  case 0x80:
  case 0x81:
    /* ECM */
    
    if(cwc->cwc_caid >> 8 == 6) {
      channel = data[6] << 8 | data[7];
      snprintf(chaninfo, sizeof(chaninfo), " (channel %d)", channel);
      ep->ep_last_section = data[5]; 
      section = data[4];
    } else {
      channel = -1;
      chaninfo[0] = 0;
      ep->ep_last_section = 0; 
      section = 0;
    }

    if(ep->ep_sections[section] == NULL)
      ep->ep_sections[section] = calloc(1, sizeof(ecm_section_t));

    es = ep->ep_sections[section];

    if(es->es_ecmsize == len && !memcmp(es->es_ecm, data, len))
      break; /* key already sent */

    if(cwc->cwc_fd == -1) {
      // New key, but we are not connected (anymore), can not descramble
      ct->cs_keystate = CS_UNKNOWN;
      break;
    }

    es->es_channel = channel;
    es->es_section = section;
    es->es_pending = 1;

    memcpy(es->es_ecm, data, len);
    es->es_ecmsize = len;

    if(ct->cs_okchannel != -1 && channel != -1 && 
       ct->cs_okchannel != channel) {
      tvhlog(LOG_DEBUG, "cwc", "Filtering ECM channel %d", channel);
      return;
    }

    es->es_seq = cwc_send_msg(cwc, data, len, sid, 1);

    tvhlog(LOG_DEBUG, "cwc", 
	   "Sending ECM%s section=%d/%d, for service %s (seqno: %d) PID %d", 
	   chaninfo, section, ep->ep_last_section, t->s_svcname, es->es_seq,
	   st->es_pid);
    es->es_time = getmonoclock();
    break;

  default:
    /* EMM */
    if (cwc->cwc_forward_emm)
      cwc_send_msg(cwc, data, len, sid, 1);
    break;
  }
}

/**
 * dre emm handler
 */
void
cwc_emm_dre(cwc_t *cwc, uint8_t *data, int len)
{
  int match = 0;

  if (data[0] == 0x87) {
    if (memcmp(&data[3], &cwc->cwc_ua[4], 4) == 0) {
      match = 1;
    }
  } 
  else if (data[0] == 0x86) {
    int i;
    for (i=0; i < cwc->cwc_num_providers; i++) {
      if (memcmp(&data[40], &cwc->cwc_providers[i].sa[4], 4) == 0) {
/*      if (memcmp(&data[3], &cwc->cwc_providers[i].sa[4], 1) == 0) { */
        match = 1;
        break;
      }
    }
  }

  if (match)
    cwc_send_msg(cwc, data, len, 0, 1);
}

void
cwc_emm_nagra(cwc_t *cwc, uint8_t *data, int len)
{
  int match = 0;
  unsigned char hexserial[4];

  if (data[0] == 0x83) {      // unique|shared
    hexserial[0] = data[5];
    hexserial[1] = data[4];
    hexserial[2] = data[3];
    hexserial[3] = data[6];
    if (memcmp(hexserial, &cwc->cwc_ua[4], (data[7] == 0x10) ? 3 : 4) == 0)
      match = 1;
  } 
  else if (data[0] == 0x82) { // global
    match = 1;
  }

  if (match)
    cwc_send_msg(cwc, data, len, 0, 1);
}

void
cwc_emm_nds(cwc_t *cwc, uint8_t *data, int len)
{
  int match = 0;
  int i;
  int serial_count = ((data[3] >> 4) & 3) + 1;
  unsigned char emmtype = (data[3] & 0xC0) >> 6;

  if (emmtype == 1 || emmtype == 2) {  // unique|shared
    for (i = 0; i < serial_count; i++) {
      if (memcmp(&data[i * 4 + 4], &cwc->cwc_ua[4], 5 - emmtype) == 0) {
        match = 1;
        break;
      }
    }
  }
  else if (emmtype == 0) {  // global
    match = 1;
  }

  if (match)
    cwc_send_msg(cwc, data, len, 0, 1);
}

/**
 *
 */
static void
update_keys(cwc_service_t *ct)
{
  int i;
  ct->cs_pending_cw_update = 0;
  for(i = 0; i < 8; i++)
    if(ct->cs_cw[i]) {
      set_even_control_word(ct->cs_keys, ct->cs_cw);
      break;
    }
  
  for(i = 0; i < 8; i++)
    if(ct->cs_cw[8 + i]) {
      set_odd_control_word(ct->cs_keys, ct->cs_cw + 8);
      break;
    }
}


/**
 *
 */
static int
cwc_descramble(th_descrambler_t *td, service_t *t, struct elementary_stream *st,
	       const uint8_t *tsb)
{
  cwc_service_t *ct = (cwc_service_t *)td;
  int r;
  unsigned char *vec[3];

  if(ct->cs_keystate == CS_FORBIDDEN)
    return 1;

  if(ct->cs_keystate != CS_RESOLVED)
    return -1;

  if(ct->cs_fill == 0 && ct->cs_pending_cw_update)
    update_keys(ct);

  memcpy(ct->cs_tsbcluster + ct->cs_fill * 188, tsb, 188);
  ct->cs_fill++;

  if(ct->cs_fill != ct->cs_cluster_size)
    return 0;

  while(1) {

    vec[0] = ct->cs_tsbcluster;
    vec[1] = ct->cs_tsbcluster + ct->cs_fill * 188;
    vec[2] = NULL;
    
    r = decrypt_packets(ct->cs_keys, vec);
    if(r > 0) {
      int i;
      const uint8_t *t0 = ct->cs_tsbcluster;

      for(i = 0; i < r; i++) {
	ts_recv_packet2(t, t0);
	t0 += 188;
      }

      r = ct->cs_fill - r;
      assert(r >= 0);

      if(r > 0)
	memmove(ct->cs_tsbcluster, t0, r * 188);
      ct->cs_fill = r;

      if(ct->cs_pending_cw_update && r > 0)
	continue;
    } else {
      ct->cs_fill = 0;
    }
    break;
  }
  if(ct->cs_pending_cw_update)
    update_keys(ct);

  return 0;
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

  while((ep = LIST_FIRST(&ct->cs_pids)) != NULL) {
    for(i = 0; i < 256; i++)
      free(ep->ep_sections[i]);
    LIST_REMOVE(ep, ep_link);
    free(ep);
  }

  LIST_REMOVE(td, td_service_link);

  LIST_REMOVE(ct, cs_link);

  free_key_struct(ct->cs_keys);
  free(ct->cs_tsbcluster);
  free(ct);
}

/**
 *
 */
static inline elementary_stream_t *
cwc_find_stream_by_caid(service_t *t, int caid)
{
  elementary_stream_t *st;
  caid_t *c;

  TAILQ_FOREACH(st, &t->s_components, es_link) {
    LIST_FOREACH(c, &st->es_caids, link) {
      if(c->caid == caid)
	return st;
    }
  }
  return NULL;
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

  pthread_mutex_lock(&cwc_mutex);
  TAILQ_FOREACH(cwc, &cwcs, cwc_link) {
    if(cwc->cwc_caid == 0)
      continue;

    if(cwc_find_stream_by_caid(t, cwc->cwc_caid) == NULL)
      continue;

    ct = calloc(1, sizeof(cwc_service_t));
    ct->cs_cluster_size = get_suggested_cluster_size();
    ct->cs_tsbcluster = malloc(ct->cs_cluster_size * 188);

    ct->cs_keys = get_key_struct();
    ct->cs_cwc = cwc;
    ct->cs_service = t;
    ct->cs_okchannel = -1;

    td = &ct->cs_head;
    td->td_stop       = cwc_service_destroy;
    td->td_table      = cwc_table_input;
    td->td_descramble = cwc_descramble;
    LIST_INSERT_HEAD(&t->s_descramblers, td, td_service_link);

    LIST_INSERT_HEAD(&cwc->cwc_services, ct, cs_link);

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
  pthread_mutex_lock(&cwc_mutex);
  TAILQ_REMOVE(&cwcs, cwc, cwc_link);  
  cwc->cwc_running = 0;
  pthread_cond_signal(&cwc->cwc_cond);
  pthread_mutex_unlock(&cwc_mutex);
}


/**
 *
 */
static cwc_t *
cwc_entry_find(const char *id, int create)
{
  pthread_attr_t attr;
  pthread_t ptid;
  char buf[20];
  cwc_t *cwc;
  static int tally;

  if(id != NULL) {
    TAILQ_FOREACH(cwc, &cwcs, cwc_link)
      if(!strcmp(cwc->cwc_id, id))
	return cwc;
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

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&ptid, &attr, cwc_thread, cwc);
  pthread_attr_destroy(&attr);

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
