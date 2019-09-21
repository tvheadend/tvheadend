/*
 *  tvheadend, CWC interface
 *  Copyright (C) 2007 Andreas Öman
 *            (C) 2017 Jaroslav Kysela
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

#include <ctype.h>
#include <openssl/des.h>

#include "tvheadend.h"
#include "cclient.h"

/**
 *
 */
#define TVHEADEND_PROTOCOL_ID 0x6502

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
typedef struct cwc {
  cclient_t;

  DES_key_schedule cwc_k1, cwc_k2;

  /* From configuration */

  uint8_t cwc_confedkey[14];
  char *cwc_password_salted;   /* salted version */
} cwc_t;


/**
 *
 */
static char * crypt_md5(const char *pw, const char *salt);

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
  uint8_t checksum = 0, *ptr, noPadBytes, padBytes[7];
  DES_cblock ivec;
  int i;

  noPadBytes = (8 - ((len - 1) % 8)) % 8;
  if (len + noPadBytes + 1 >= CWS_NETMSGSIZE-8) return -1;
  uuid_random(padBytes, noPadBytes);
  for (i = 0; i < noPadBytes; i++) buffer[len++] = padBytes[i];
  for (i = 2; i < len; i++) checksum ^= buffer[i];
  buffer[len++] = checksum;
  uuid_random((uint8_t *)ivec, 8);
  memcpy(buffer+len, ivec, 8);
  for (i = 2; i < len; i += 8) {
    ptr = buffer + i;
    DES_ncbc_encrypt(ptr, ptr, 8, &cwc->cwc_k1, &ivec, 1);
    DES_ecb_encrypt((DES_cblock *)ptr, (DES_cblock *)ptr, &cwc->cwc_k2, 0);
    DES_ecb_encrypt((DES_cblock *)ptr, (DES_cblock *)ptr, &cwc->cwc_k1, 1);
    memcpy(ivec, ptr, 8);
  }
  return len + 8;
}

/**
 *
 */
static int 
des_decrypt(uint8_t *buffer, int len, cwc_t *cwc)
{
  DES_cblock ivec, nextIvec;
  uint8_t *ptr, checksum = 0;;
  int i;

  if ((len-2) % 8 || (len-2) < 16) return -1;
  len -= 8;
  memcpy(nextIvec, buffer+len, 8);
  for (i = 2; i < len; i += 8) {
    ptr = buffer + i;
    memcpy(ivec, nextIvec, 8);
    memcpy(nextIvec, ptr, 8);
    DES_ecb_encrypt((DES_cblock *)ptr, (DES_cblock *)ptr, &cwc->cwc_k1, 0);
    DES_ecb_encrypt((DES_cblock *)ptr, (DES_cblock *)ptr, &cwc->cwc_k2, 1);
    DES_ncbc_encrypt(ptr, ptr, 8, &cwc->cwc_k1, &ivec, 0);
  } 
  tvhlog_hexdump(cwc->cc_subsys, buffer, len);
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
 * the ID)
 */
static int
cwc_send_msg(void *cc, const uint8_t *msg, size_t len,
             int sid, int enq, uint16_t st_caid, uint32_t st_provider,
             uint16_t *_seq)
{
  cwc_t *cwc = cc;
  cc_message_t *cm;
  uint8_t *buf;
  int seq, ret;

  if (len < 3)
    return -1;

  /* note: the last 16 bytes is pad/checksum for des_encrypt() */
  cm = malloc(sizeof(cc_message_t) + 12 + len + 16);

  if (cm == NULL)
    return -1;

  seq = atomic_add(&cwc->cc_seq, 1);

  buf = cm->cm_data;
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

  if ((ret = des_encrypt(buf, len, cwc)) <= 0) {
    free(cm);
    return -1;
  }
  len = (size_t)ret;

  buf[0] = (len - 2) >> 8;
  buf[1] = (len - 2) & 0xff;

  cm->cm_len = len;
  cc_write_message(cc, cm, enq);

  if (_seq)
    *_seq = seq;

  return 0;
}

/**
 * Card data command
 */
static void
cwc_send_data_req(cwc_t *cwc)
{
  uint8_t buf[3];

  buf[0] = MSG_CARD_DATA_REQ;
  buf[1] = 0;
  buf[2] = 0;

  cwc_send_msg(cwc, buf, 3, 0, 0, 0, 1, NULL);
}


/**
 * Send keep alive
 */
static void
cwc_send_ka(void *cc)
{
  cwc_t *cwc = cc;
  uint8_t buf[3];

  buf[0] = MSG_KEEPALIVE;
  buf[1] = 0;
  buf[2] = 0;

  cwc_send_msg(cwc, buf, 3, 0, 1, 0, 0, NULL);
}

/**
 * Handle reply to card data request
 */
static int
cwc_decode_card_data_reply(cwc_t *cwc, uint8_t *msg, int len)
{
  int plen, i;
  unsigned int nprov;
  uint8_t **pid, **psa, *msg2;

  msg += 12;
  len -= 12;

  if(len < 3) {
    tvhinfo(cwc->cc_subsys, "%s: Invalid card data reply", cwc->cc_name);
    return -1;
  }

  plen = (msg[1] & 0xf) << 8 | msg[2];

  if(plen < 14) {
    tvhinfo(cwc->cc_subsys, "%s: Invalid card data reply (message)", cwc->cc_name);
    return -1;
  }

  nprov = msg[14];

  msg2  = msg + 15;
  plen -= 12;

  if(plen < nprov * 11) {
    tvhinfo(cwc->cc_subsys, "%s: Invalid card data reply (provider list)",
            cwc->cc_name);
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
  cc_new_card((cclient_t *)cwc, (msg[4] << 8) | msg[5],
               0, msg + 6, nprov, pid, psa);

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

  if (cwc->cc_username == NULL ||
      cwc->cwc_password_salted == NULL)
    return 1;

  ul = strlen(cwc->cc_username) + 1;
  pl = strlen(cwc->cwc_password_salted) + 1;

  if (ul + pl > 255)
    return 1;

  buf[0] = MSG_CLIENT_2_SERVER_LOGIN;
  buf[1] = 0;
  buf[2] = ul + pl;
  memcpy(buf + 3,      cwc->cc_username, ul);
  memcpy(buf + 3 + ul, cwc->cwc_password_salted, pl);

  cwc_send_msg(cwc, buf, ul + pl + 3, TVHEADEND_PROTOCOL_ID, 0, 0, 0, NULL);

  return 0;
}

/**
 *
 */
static void
handle_ecm_reply(cc_service_t *ct, cc_ecm_section_t *es,
                 uint8_t *msg, int len, int seq)
{
  cwc_t *cwc = ct->cs_client;
  uint32_t off;
  int type;

  if (len < 19) {
    cc_ecm_reply(ct, es, DESCRAMBLER_NONE, NULL, NULL, seq);
  } else {
    type = DESCRAMBLER_CSA_CBC;
    if (len <= 22) {
      off = 8;
    } else if (len <= 40) {
      off = 16;
      type = DESCRAMBLER_AES128_ECB;
    } else {
      tvherror(cwc->cc_subsys, "%s: wrong ECM reply length %d",
               cwc->cc_name, len);
      return;
    }
    cc_ecm_reply(ct, es, type, msg + 3, msg + 3 + off, seq);
  }
}

/**
 * Handle running reply
 * cwc_mutex is held
 */
static int
cwc_running_reply(cwc_t *cwc, uint8_t msgtype, uint8_t *msg, int len)
{
  cc_service_t *ct;
  cc_ecm_section_t *es;
  uint16_t seq;
  int plen;
  short caid;
  uint8_t *u8;

  switch(msgtype) {
    case 0x80:
    case 0x81:
      if (len < 12) {
        tvherror(cwc->cc_subsys, "%s: wrong 0x%02X length %d",
                 cwc->cc_name, msgtype, len);
        return -1;
      }
      seq = (msg[2] << 8) | msg[3];
      len -= 12;
      msg += 12;
      es = cc_find_pending_section((cclient_t *)cwc, seq, &ct);
      if (es)
        handle_ecm_reply(ct, es, msg, len, seq);
      break;

    case 0xD3:
      if (len < 16) {
        tvherror(cwc->cc_subsys, "%s: wrong 0xD3 length %d", cwc->cc_name, len);
        return -1;
      }

      caid = (msg[6] << 8) | msg[7];

      if (caid){
        if(len < 3) {
          tvhinfo(cwc->cc_subsys, "%s: Invalid card data reply", cwc->cc_name);
          return -1;
        }

        plen = (msg[1] & 0xf) << 8 | msg[2];

        if(plen < 14) {
          tvhinfo(cwc->cc_subsys, "%s: Invalid card data reply (message)", cwc->cc_name);
          return -1;
        }

        caclient_set_status((caclient_t *)cwc, CACLIENT_STATUS_CONNECTED);
        u8 = &msg[8];
        cc_new_card((cclient_t *)cwc, caid, 0, NULL, 1, &u8, NULL);
      }
  }
  return 0;
}

/**
 *
 */
static int
cwc_read_message0
  (cwc_t *cwc, const char *state, sbuf_t *rbuf, int *rsize, int timeout)
{
  int msglen;

  if (rbuf->sb_ptr < 2)
    return 0;

  msglen = (rbuf->sb_data[0] << 8) | rbuf->sb_data[1];
  if(rbuf->sb_ptr < 2 + msglen)
    return 0;

  *rsize = msglen + 2;
  if((msglen = des_decrypt(rbuf->sb_data, msglen + 2, cwc)) < 15) {
    tvhinfo(cwc->cc_subsys, "%s: %s: Decrypt failed",
            cwc->cc_name, state);
    return -1;
  }

  tvhtrace(LS_CWC, "%s: decrypted message", cwc->cc_name);
  tvhlog_hexdump(cwc->cc_subsys, rbuf->sb_data, msglen + 2);

  return msglen;
}

/**
 *
 */
static int
cwc_read_message
  (cwc_t *cwc, const char *state, uint8_t *buf, int len, int timeout)
{
  int msglen, r;

  if ((r = cc_read((cclient_t *)cwc, buf, 2, timeout))) {
    if (tvheadend_is_running())
      tvhinfo(cwc->cc_subsys, "%s: %s: Read error (header): %s",
              cwc->cc_name, state, strerror(r));
    return -1;
  }

  msglen = (buf[0] << 8) | buf[1];
  if (msglen > len) {
    if (tvheadend_is_running())
      tvhinfo(cwc->cc_subsys, "%s: %s: Invalid message size: %d",
              cwc->cc_name, state, msglen);
    return -1;
  }

  /* We expect the rest of the message to arrive fairly quick,
     so just wait 1 second here */

  if ((r = cc_read((cclient_t *)cwc, buf + 2, msglen, 1000))) {
    if (tvheadend_is_running())
      tvhinfo(cwc->cc_subsys, "%s: %s: Read error: %s",
              cwc->cc_name, state, strerror(r));
    return -1;
  }

  if ((msglen = des_decrypt(buf, msglen + 2, cwc)) < 15) {
    tvhinfo(cwc->cc_subsys, "%s: %s: Decrypt failed",
            cwc->cc_name, state);
    return -1;
  }

  tvhtrace(LS_CWC, "%s: decrypted message", cwc->cc_name);
  tvhlog_hexdump(cwc->cc_subsys, buf, msglen + 2);

  return msglen;
}

/**
 *
 */
static int
cwc_init_session(void *cc)
{
  cwc_t *cwc = cc;
  uint8_t buf[CWS_NETMSGSIZE];
  int r;

  /**
   * Get login key
   */
  if ((r = cc_read((cclient_t *)cwc, buf, 14, 5000))) {
    tvhinfo(cwc->cc_subsys, "%s: No login key received: %s",
            cwc->cc_name, strerror(r));
    return -1;
  }

  des_make_login_key(cwc, buf);

  /**
   * Login
   */
  if (cwc_send_login(cwc))
    return -1;
  
  if (cwc_read_message(cwc, "Wait login response", buf, sizeof(buf), 5000) < 0)
    return -1;

  if (buf[12] != MSG_CLIENT_2_SERVER_LOGIN_ACK) {
    tvhinfo(cwc->cc_subsys, "%s: Login failed", cwc->cc_name);
    return -1;
  }

  des_make_session_key(cwc);

  /**
   * Request card data
   */
  cwc_send_data_req(cwc);
  if ((r = cwc_read_message(cwc, "Request card data", buf, sizeof(buf), 5000)) < 0)
    return -1;

  if (buf[12] != MSG_CARD_DATA) {
    tvhinfo(cwc->cc_subsys, "%s: Card data request failed", cwc->cc_name);
    return -1;
  }

  if (cwc_decode_card_data_reply(cwc, buf, r) < 0)
    return -1;
  return 0;
}

/**
 *
 */
static int
cwc_read(void *cc, sbuf_t *rbuf)
{
  cwc_t *cwc = cc;
  const int ka_interval = cwc->cc_keepalive_interval * 2 * 1000;
  int r, rsize;

  while (1) {
    r = cwc_read_message0(cwc, "DecoderLoop", rbuf, &rsize, ka_interval);
    if (r == 0)
      break;
    if (r < 0)
      return -1;
    if (r > 12) {
      if (cwc_running_reply(cwc, rbuf->sb_data[12], rbuf->sb_data, r) < 0)
        return -1;
      sbuf_cut(rbuf, rsize);
    } else if (r > 0) {
      return -1;
    }
  }
  return 0;
}

/**
 *
 */
static int
cwc_send_ecm(void *cc, cc_service_t *ct, cc_ecm_section_t *es,
             cc_card_data_t *pcard, const uint8_t *data, int len)
{
  mpegts_service_t *t = (mpegts_service_t *)ct->td_service;
  uint16_t sid = service_id16(t);
  uint16_t seq;
  int r;

  r = cwc_send_msg(cc, data, len, sid, 1, es->es_caid, es->es_provid, &seq);
  if (r == 0)
    es->es_seq = seq;
  return r;
}

/**
 *
 */
static void
cwc_send_emm(void *cc, cc_service_t *ct,
             cc_card_data_t *pcard, uint32_t provid,
             const uint8_t *data, int len)
{
  mpegts_service_t *t;
  uint16_t sid = 0;

  if (ct) {
    t = (mpegts_service_t *)ct->td_service;
    sid = service_id16(t);
  }

  cwc_send_msg(cc, data, len, sid, 1, pcard->cs_ra.caid, provid, NULL);
}

/**
 *
 */
static void
cwc_free(caclient_t *cac)
{
  cwc_t *cwc = (cwc_t *)cac;
  char *salted = cwc->cwc_password_salted;
  cc_free(cac);
  free((void *)salted);
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

  free(cwc->cwc_password_salted);
  cwc->cwc_password_salted =
    cwc->cc_password ? crypt_md5(cwc->cc_password, "$1$abcdefgh$") : NULL;

  cc_conf_changed(cac);
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
  .ic_super      = &caclient_cc_class,
  .ic_class      = "caclient_cwc",
  .ic_caption    = N_("Code Word Client (newcamd)"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "deskey",
      .name     = N_("DES key"),
      .desc     = N_("DES Key."),
      .set      = caclient_cwc_class_deskey_set,
      .get      = caclient_cwc_class_deskey_get,
      .opts     = PO_PASSWORD,
      .def.s    = "00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d",
      .group    = 2,
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

  cwc->cc_subsys = LS_CWC;
  cwc->cc_id     = "newcamd";

  tvh_mutex_init(&cwc->cc_mutex, NULL);
  tvh_cond_init(&cwc->cc_cond, 1);
  cwc->cac_free         = cwc_free;
  cwc->cac_start        = cc_service_start;
  cwc->cac_conf_changed = cwc_conf_changed;
  cwc->cac_caid_update  = cc_caid_update;
  cwc->cc_keepalive_interval = CC_KEEPALIVE_INTERVAL;
  cwc->cc_init_session  = cwc_init_session;
  cwc->cc_read          = cwc_read;
  cwc->cc_send_ecm      = cwc_send_ecm;
  cwc->cc_send_emm      = cwc_send_emm;
  cwc->cc_keepalive     = cwc_send_ka;
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
