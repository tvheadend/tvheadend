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

#include <ctype.h>
#include <openssl/sha.h>
#include "tvheadend.h"
#include "tcp.h"
#include "cclient.h"

/**
 *
 */
#define CCCAM_KEEPALIVE_INTERVAL  0
#define CCCAM_NETMSGSIZE          1024


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
  CCCAM_EXTENDED_NONE = 0,
  CCCAM_EXTENDED_EXT = 1,
} cccam_extended_t;

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
  "2.1.4",  "2.2.0", "2.2.1", "2.3.0",
};

static const char *cccam_build_str[CCCAM_VERSION_COUNT] = {
  "2892",   "2971",  "3094",  "3165",
  "3191",   "3290",  "3316",  "3367",
};

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
  cclient_t;

  /* From configuration */
  uint8_t cccam_nodeid[8];
  int cccam_extended_conf;
  int cccam_extended;
  int cccam_version;

  uint8_t cccam_oscam;
  uint8_t cccam_sendsleep;
  uint8_t cccam_cansid;

  uint8_t cccam_busy;

  struct cccam_crypt_block sendblock;
  struct cccam_crypt_block recvblock;

} cccam_t;


static const uint8_t cccam_str[] = "CCcam";

static void cccam_send_oscam_extended(cccam_t *cccam);

/**
 *
 */
static inline const char *cccam_get_version_str(cccam_t *cccam)
{
  int ver = MINMAX(cccam->cccam_version, 0, ARRAY_SIZE(cccam_version_str) - 1);
  return cccam_version_str[ver];
}

/**
 *
 */
static inline const char *cccam_get_build_str(cccam_t *cccam)
{
  int ver = MINMAX(cccam->cccam_version, 0, ARRAY_SIZE(cccam_version_str) - 1);
  return cccam_build_str[ver];
}

/**
 *
 */
static inline int cccam_set_busy(cccam_t *cccam)
{
  if (cccam->cccam_extended)
    return 0;
  if (cccam->cccam_busy)
    return 1;
  cccam->cccam_busy = 1;
  return 0;
}

/**
 *
 */
static inline void cccam_unset_busy(cccam_t *cccam)
{
  cccam->cccam_busy = 0;
}

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
    if (i <= 5)
      buf[i] ^= cccam_str[i];
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

/**
 *
 */
static int
cccam_oscam_check(cccam_t *cccam, uint8_t *buf)
{
  if (!cccam->cccam_oscam) {
    uint16_t sum = 0x1234;
    uint16_t recv_sum = (buf[14] << 8) | buf[15];
    int32_t i;
    for (i = 0; i < 14; i++)
      sum += buf[i];
    tvhtrace(cccam->cc_subsys, "%s: oscam check sum %04X recv sum %04X",
             cccam->cc_name, sum, recv_sum);
    cccam->cccam_oscam = sum == recv_sum;
    if (cccam->cccam_oscam)
      tvhinfo(cccam->cc_subsys, "%s: oscam server detected", cccam->cc_name);
  }
  return cccam->cccam_oscam;
}

/**
 *
 */
static int
cccam_oscam_nodeid_check(cccam_t *cccam, uint8_t *buf)
{
  if (!cccam->cccam_oscam) {
    uint16_t sum = 0x1234;
    uint16_t recv_sum = (buf[6] << 8) | buf[7];
    int32_t i;
    for (i = 0; i < 6; i++)
      sum += buf[i];
    tvhtrace(cccam->cc_subsys, "%s: oscam nodeid check sum %04X recv sum %04X",
             cccam->cc_name, sum, recv_sum);
    cccam->cccam_oscam = sum == recv_sum;
    if (cccam->cccam_oscam)
      tvhinfo(cccam->cc_subsys, "%s: oscam server detected", cccam->cc_name);
  }
  return cccam->cccam_oscam;
}

/**
 *
 */
static inline uint8_t *
cccam_set_ua(uint8_t *dst, uint8_t *src)
{
  /* FIXME */
  return memcpy(dst, src, 8);
}

/**
 *
 */
static inline uint8_t *
cccam_set_sa(uint8_t *dst, uint8_t *src)
{
  return memcpy(dst, src, 4);
}

/**
 * Handle reply to card data request
 */
static int
cccam_decode_card_data_reply(cccam_t *cccam, uint8_t *msg)
{
  cc_card_data_t *pcard;
  int i;
  unsigned int nprov;
  uint8_t **pid, **psa, *saa, *msg2, ua[8];

  /* nr of providers */
  nprov = msg[24];

  pid = nprov ? alloca(nprov * sizeof(uint8_t *)) : NULL;
  psa = nprov ? alloca(nprov * sizeof(uint8_t *)) : NULL;
  saa = nprov ? alloca(nprov * 8) : NULL;

  if (pid == NULL || psa == NULL || saa == NULL)
    return -ENOMEM;

  msg2 = msg + 25;
  memset(saa, 0, nprov * 8);
  for (i = 0; i < nprov; i++) {
    pid[i] = msg2;
    psa[i] = cccam_set_sa(saa + i * 8, msg2 + 3);
    msg2 += 7;
  }

  caclient_set_status((caclient_t *)cccam, CACLIENT_STATUS_CONNECTED);
  cccam_set_ua(ua, msg + 16);
  pcard = cc_new_card((cclient_t *)cccam, (msg[12] << 8) | msg[13],
                      (msg[4] << 24) | (msg[5] << 16) | (msg[6] << 8) | msg[7],
                      ua, nprov, pid, psa);
  if (pcard) {
    pcard->cccam.cs_remote_id = (msg[8] << 24) | (msg[9] << 16) | (msg[10] << 8) | msg[11];
    pcard->cccam.cs_hop = msg[14];
    pcard->cccam.cs_reshare = msg[15];
  }

  return 0;
}

/**
 *
 */
static void
cccam_handle_keys(cccam_t *cccam, cc_service_t *ct, cc_ecm_section_t *es,
                  uint8_t *buf, int len, int seq)
{
  uint8_t *dcw_even, *dcw_odd, _dcw[16];

  if (!cccam->cccam_extended) {
    cccam_decrypt_cw(cccam->cccam_nodeid, es->es_card_id, buf + 4);
    memcpy(_dcw, buf + 4, 16);
    cccam_decrypt(&cccam->recvblock, buf + 4, len - 4);
  } else {
    memcpy(_dcw, buf + 4, 16);
  }

  dcw_even = buf[1] == MSG_ECM_REQUEST ? _dcw : NULL;
  dcw_odd  = buf[1] == MSG_ECM_REQUEST ? _dcw + 8 : NULL;

  cc_ecm_reply(ct, es, DESCRAMBLER_CSA_CBC, dcw_even, dcw_odd, seq);
}

/**
 *
 */
static void
cccam_handle_partner(cccam_t *cccam, uint8_t *msg)
{
  char *saveptr;
  char *p;
  p = strtok_r((char *)msg, "[", &saveptr);
  while (p) {
    if ((p = strtok_r(NULL, ",]", &saveptr)) == NULL)
      break;
    if (strncmp(p, "EXT", 3) == 0)
      cccam->cccam_extended = 1;
    else if (strncmp(p, "SID", 3) == 0)
      cccam->cccam_cansid = 1;
    else if (strncmp(p, "SLP", 3) == 0)
      cccam->cccam_sendsleep = 1;
  }
  tvhinfo(cccam->cc_subsys, "%s: server supports extended capabilities%s%s%s",
          cccam->cc_name,
          cccam->cccam_extended ? " EXT" : "",
          cccam->cccam_cansid ? " SID" : "",
          cccam->cccam_sendsleep ? " SLP" : "");
}

/**
 * Handle running reply
 * cc_mutex is held
 */
static int
cccam_running_reply(cccam_t *cccam, uint8_t *buf, int len)
{
  cc_service_t *ct;
  cc_ecm_section_t *es;
  uint32_t cardid;
  uint8_t seq;

  if (len < 4)
    return -1;

  tvhtrace(cccam->cc_subsys, "%s: response msg type=%d, response",
           cccam->cc_name, buf[1]);
  tvhlog_hexdump(cccam->cc_subsys, buf, len);

  switch (buf[1]) {
    case MSG_NEW_CARD_SIDINFO:
    case MSG_NEW_CARD:
      tvhtrace(cccam->cc_subsys, "%s: add card message received", cccam->cc_name);
      cccam_decode_card_data_reply(cccam, buf);
      break;
    case MSG_CARD_REMOVED:
      if (len >= 8) {
        cardid = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
        tvhtrace(cccam->cc_subsys, "%s: del card %08X message received", cccam->cc_name, cardid);
        cc_remove_card_by_id((cclient_t *)cccam, cardid);
      }
      break;
    case MSG_KEEPALIVE:
      tvhtrace(cccam->cc_subsys, "%s: keepalive", cccam->cc_name);
      break;
    case MSG_EMM_REQUEST:   /* emm ack */
      tvhtrace(cccam->cc_subsys, "%s: EMM message ACK received", cccam->cc_name);
      cccam_unset_busy(cccam);
      break;
    case MSG_SLEEPSEND:
      tvhtrace(cccam->cc_subsys, "%s: Sleep send received", cccam->cc_name);
      if (len >= 5) goto req;
      break;
    case MSG_ECM_NOK1:      /* retry */
    case MSG_ECM_NOK2:      /* decode failed */
      if (len >= 2 && len < 8) goto req;
      if (len > 5) {
        /* partner detection */
        if (len >= 12 && strncmp((char *)buf + 4, "PARTNER:", 8) == 0) {
          cccam_handle_partner(cccam, buf + 4);
        } else {
          goto req;
        }
      }
      break;
    //case MSG_CMD_05:      /* ? */
    case MSG_ECM_REQUEST: { /* request reply */
req:
      seq = cccam->cccam_extended ? buf[0] : 1;
      es = cc_find_pending_section((cclient_t *)cccam, seq, &ct);
      if (es)
        cccam_handle_keys(cccam, ct, es, buf, len, seq);
      cccam_unset_busy(cccam);
      return 0;
    }
    case MSG_SRV_DATA:
      if (len == 0x4c) {
        tvhinfo(cccam->cc_subsys,
                "%s: CCcam server version %s nodeid=%02x%02x%02x%02x%02x%02x%02x%02x",
                cccam->cc_name, buf + 12,
                buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
        if (cccam_oscam_nodeid_check(cccam, buf + 4))
          cccam_send_oscam_extended(cccam);
      } else {
        tvhtrace(cccam->cc_subsys, "%s: SRV_DATA: unknown length %d",
                 cccam->cc_name, len);
      }
      break;
    case MSG_CLI_DATA:
      tvhinfo(cccam->cc_subsys, "%s: CCcam server authentication completed",
              cccam->cc_name);
      break;
    default:
      tvhwarn(cccam->cc_subsys, "%s: Unknown message received",
              cccam->cc_name);
      break;
  }
  return 0;
}

/**
 *
 */
static int
cccam_read_message0(cccam_t *cccam, const char *state, sbuf_t *rbuf, int timeout)
{
  uint16_t msglen;
  uint8_t hdr[4];
  struct cccam_crypt_block block;

  if (rbuf->sb_ptr < 4)
    return 0;
  block = cccam->recvblock;
  memcpy(hdr, rbuf->sb_data, 4);
  cccam_decrypt(&cccam->recvblock, hdr, 4);
  msglen = (hdr[2] << 8) | hdr[3];
  if (rbuf->sb_ptr >= msglen + 4) {
    memcpy(rbuf->sb_data, hdr, 4);
    cccam_decrypt(&cccam->recvblock, rbuf->sb_data + 4, msglen);
    return msglen + 4;
  } else {
    cccam->recvblock = block;
    return 0;
  }
}

/**
 *
 */
static int
cccam_send_msg(cccam_t *cccam, cccam_msg_type_t cmd,
               uint8_t *buf, size_t len, int enq,
               uint8_t seq, uint32_t card_id)
{
  cc_message_t *cm;
  uint8_t *netbuf;

  if (len + 4 > CCCAM_NETMSGSIZE)
    return -1;

  cm = malloc(sizeof(cc_message_t) + len + 4);
  if (cm == NULL)
    return -1;

  netbuf = cm->cm_data;
  if (cmd == MSG_NO_HEADER) {
    memcpy(netbuf, buf, len);
  } else {
    netbuf[0] = cccam->cccam_extended ? seq : 0;
    netbuf[1] = cmd;
    netbuf[2] = len >> 8;
    netbuf[3] = len;
    if (buf)
      memcpy(netbuf + 4, buf, len);
    len += 4;
  }

  cccam_encrypt(&cccam->sendblock, cm->cm_data, len);
  cm->cm_len = len;
  cc_write_message((cclient_t *)cccam, cm, enq);

  return 0;
}

/**
 * Send keep alive
 */
static void
cccam_send_ka(void *cc)
{
  cccam_t *cccam = cc;
  uint8_t buf[4];

  buf[0] = 0;
  buf[1] = MSG_KEEPALIVE;
  buf[2] = 0;
  buf[3] = 0;

  tvhdebug(cccam->cc_subsys, "%s: send keepalive", cccam->cc_name);
  cccam_send_msg(cccam, MSG_NO_HEADER, buf, 4, 1, 0, 0);
}

/**
 * Send keep alive
 */
static void
cccam_send_oscam_extended(cccam_t *cccam)
{
  char buf[256];
  tvhdebug(cccam->cc_subsys, "%s: send oscam extended", cccam->cc_name);
  snprintf(buf, sizeof(buf), "PARTNER: OSCam v%s, build r%s (%s) [EXT,SID,SLP]",
           cccam_get_version_str(cccam), cccam_get_build_str(cccam),
           "unknown");
  cccam_send_msg(cccam, MSG_ECM_NOK1, (uint8_t *)buf, strlen(buf) + 1, 1, 0, 0);
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

  tvhdebug(cccam->cc_subsys, "%s: sha1 hash", cccam->cc_name);
  tvhlog_hexdump(cccam->cc_subsys, hash, sizeof(hash));

  cccam_crypt_init(&cccam->recvblock, hash, sizeof(hash));
  cccam_decrypt(&cccam->recvblock, buf, 16);

  cccam_crypt_init(&cccam->sendblock, buf, 16);
  cccam_decrypt(&cccam->sendblock, hash, sizeof(hash));

  // send crypted hash to server
  cccam_send_msg(cccam, MSG_NO_HEADER, hash, sizeof(hash), 0, 0, 0);
}

/**
 * Login command
 */
static int
cccam_send_login(cccam_t *cccam)
{
  uint8_t buf[20], data[20], *pwd;
  size_t l;

  if (cccam->cc_username == NULL)
    return 1;

  l = MIN(strlen(cccam->cc_username), 20);

  /* send username */
  memset(buf + l, 0, 20 - l);
  memcpy(buf, cccam->cc_username, l);
  cccam_send_msg(cccam, MSG_NO_HEADER, buf, 20, 0, 0, 0);

  /* send password 'xored' with CCcam */
  memcpy(buf, cccam_str, 5);
  buf[5] = 0;
  if (cccam->cc_password && cccam->cc_password[0]) {
    l = strlen(cccam->cc_password);
    pwd = alloca(l + 1);
    strcpy((char *)pwd, cccam->cc_password);
    cccam_encrypt(&cccam->sendblock, pwd, l);
  }
  cccam_send_msg(cccam, MSG_NO_HEADER, buf, 6, 0, 0, 0);

  tvhdebug(cccam->cc_subsys, "%s: login response", cccam->cc_name);
  if (cc_read((cclient_t *)cccam, data, 20, 5000)) {
    tvherror(cccam->cc_subsys, "%s: login failed, pwd ack not received", cccam->cc_name);
    return -2;
  }

  cccam_decrypt(&cccam->recvblock, data, 20);
  if (memcmp(data, "CCcam", 5)) {
    tvherror(cccam->cc_subsys, "%s: login failed, usr/pwd invalid", cccam->cc_name);
    return -2;
  } else {
    tvhinfo(cccam->cc_subsys, "%s: login succeeded", cccam->cc_name);
  }

  return 0;
}

/**
 *
 */
static void
cccam_send_cli_data(cccam_t *cccam)
{
  const int32_t size = 20 + 8 + 6 + 26 + 4 + 28 + 1;
  uint8_t buf[size];

  memset(buf, 0, sizeof(buf));
  strncpy((char *)buf, cccam->cc_username ?: "", 20);
  memcpy(buf + 20, cccam->cccam_nodeid, 8);
  buf[28] = 0; // TODO: wantemus = 1;
  strncpy((char *)buf + 29, cccam_get_version_str(cccam), 31);
  memcpy(buf + 61, "tvh", 3); // build number (ascii)
  cccam_send_msg(cccam, MSG_CLI_DATA, buf, size, 0, 0, 0);
}

/**
 *
 */
static void
cccam_oscam_update_idnode(cccam_t *cccam)
{
  uint8_t p[8];
  uint16_t sum = 0x1234;
  int32_t i;

  memcpy(p, cccam->cccam_nodeid, 4);
  p[4] = 'T'; /* identify ourselves as TVH */
  p[5] = 0xaa;
  for (i = 0; i < 5; i++)
    p[5] ^= p[i];
  for (i = 0; i < 6; i++)
    sum += p[i];
  p[6] = sum >> 8;
  p[7] = sum;
  memcpy(cccam->cccam_nodeid, p, 8);
}

/**
 *
 */
static int
cccam_init_session(void *cc)
{
  cccam_t *cccam = cc;
  uint8_t buf[256];
  int r;

  cccam->cccam_oscam = 0;
  cccam->cccam_extended = 0;
  cccam->cccam_sendsleep = 0;
  cccam->cccam_cansid = 0;

  /**
   * Get init seed
   */
  tvhtrace(cccam->cc_subsys, "%s: init seed", cccam->cc_name);
  if((r = cc_read(cc, buf, 16, 5000))) {
    tvhinfo(cccam->cc_subsys, "%s: init error (no init seed received)", cccam->cc_name);
    return -1;
  }

  /* check for oscam-cccam */
  cccam_oscam_check(cccam, buf);

  sha1_make_login_key(cccam, buf);

  /**
   * Login
   */
  if (cccam_send_login(cccam))
    return -1;

  cccam_send_cli_data(cccam);

  return 0;
}

/**
 *
 */
static int
cccam_send_ecm(void *cc, cc_service_t *ct, cc_ecm_section_t *es,
               cc_card_data_t *pcard, const uint8_t *msg, int len)
{
  mpegts_service_t *t = (mpegts_service_t *)ct->td_service;
  cccam_t *cccam = cc;
  uint8_t *buf;
  uint16_t caid, sid;
  uint32_t provid, card_id;
  int seq;

  if (len > 255) {
    tvherror(cccam->cc_subsys, "%s: ECM too big (%d bytes)", cccam->cc_name, len);
    return -1;
  }

  if (cccam_set_busy(cccam)) {
    tvhinfo(cccam->cc_subsys, "%s: Ignore ECM request %02X (server is busy)",
            cccam->cc_name, msg[0]);
    return -1;
  }

  seq = atomic_add(&cccam->cc_seq, 1);
  caid = es->es_caid;
  provid = es->es_provid;
  card_id = pcard->cs_id;
  es->es_card_id = card_id;
  sid = service_id16(t);
  es->es_seq = seq & 0xff;

  buf = alloca(len + 13);
  buf[ 0] = caid >> 8;
  buf[ 1] = caid & 0xff;
  buf[ 2] = provid >> 24;
  buf[ 3] = provid >> 16;
  buf[ 4] = provid >> 8;
  buf[ 5] = provid & 0xff;
  buf[ 6] = card_id >> 24;
  buf[ 7] = card_id >> 16;
  buf[ 8] = card_id >> 8;
  buf[ 9] = card_id & 0xff;
  buf[10] = sid >> 8;
  buf[11] = sid & 0xff;
  buf[12] = len;
  memcpy(buf + 13, msg, len);

  return cccam_send_msg(cccam, MSG_ECM_REQUEST, buf, 13 + len, 1, seq, card_id);
}

/**
 *
 */
static void
cccam_send_emm(void *cc, cc_service_t *ct, cc_card_data_t *pcard,
               uint32_t provid, const uint8_t *msg, int len)
{
  cccam_t *cccam = cc;
  uint8_t *buf;
  uint16_t caid;
  uint32_t card_id;
  int seq;

  if (len > 255) {
    tvherror(cccam->cc_subsys, "%s: EMM too big (%d bytes)",
             cccam->cc_name, len);
    return;
  }

  if (cccam_set_busy(cccam)) {
    tvhinfo(cccam->cc_subsys, "%s: Ignore EMM request %02X (server is busy)",
            cccam->cc_name, msg[0]);
    return;
  }

  seq = atomic_add(&cccam->cc_seq, 1);
  caid = pcard->cs_ra.caid;
  card_id = pcard->cs_id;

  buf = alloca(len + 12);
  buf[ 0] = caid >> 8;
  buf[ 1] = caid & 0xff;
  buf[ 2] = 0;
  buf[ 3] = provid >> 24;
  buf[ 4] = provid >> 16;
  buf[ 5] = provid >> 8;
  buf[ 6] = provid & 0xff;
  buf[ 7] = card_id >> 24;
  buf[ 8] = card_id >> 16;
  buf[ 9] = card_id >> 8;
  buf[10] = card_id & 0xff;
  buf[11] = len;
  memcpy(buf + 12, msg, len);

  cccam_set_busy(cccam);

  cccam_send_msg(cccam, MSG_EMM_REQUEST, buf, 12 + len, 1, seq, card_id);
}

/**
 *
 */
static int
cccam_read(void *cc, sbuf_t *rbuf)
{
  cccam_t *cccam = cc;
  const int ka_interval = cccam->cc_keepalive_interval * 2 * 1000;
  int r;

  while (1) {
    r = cccam_read_message0(cccam, "Decoderloop", rbuf, ka_interval);
    if (r == 0)
      break;
    if (r < 0)
      return -1;
    if (r > 0) {
      int ret = cccam_running_reply(cccam, rbuf->sb_data, r);
      if (ret < 0)
        return -1;
      sbuf_cut(rbuf, r);
    }
  }
  return 0;
}

/**
 *
 */
static void
cccam_no_services(void *cc)
{
  cccam_unset_busy((cccam_t *)cc);
}

/**
 *
 */
static void
cccam_conf_changed(caclient_t *cac)
{
  cccam_t *cccam = (cccam_t *)cac;

  /**
   * Update nodeid for the oscam extended mode
   */
  if (cccam->cccam_extended_conf == CCCAM_EXTENDED_EXT)
    cccam_oscam_update_idnode(cccam);
  cc_conf_changed(cac);
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
caclient_cccam_class_cccam_extended_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("None"), CCCAM_EXTENDED_NONE },
    { N_("Oscam-EXT"), CCCAM_EXTENDED_EXT },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
caclient_cccam_class_cccam_version_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("2.0.11"), CCCAM_VERSION_2_0_11 },
    { N_("2.1.1"),  CCCAM_VERSION_2_1_1 },
    { N_("2.1.2"),  CCCAM_VERSION_2_1_2 },
    { N_("2.1.3"),  CCCAM_VERSION_2_1_3 },
    { N_("2.1.4"),  CCCAM_VERSION_2_1_4 },
    { N_("2.2.0"),  CCCAM_VERSION_2_2_0 },
    { N_("2.2.1"),  CCCAM_VERSION_2_2_1 },
    { N_("2.3.0"),  CCCAM_VERSION_2_3_0 },
  };
  return strtab2htsmsg(tab, 1, lang);
}

const idclass_t caclient_cccam_class =
{
  .ic_super      = &caclient_cc_class,
  .ic_class      = "caclient_cccam",
  .ic_caption    = N_("CCcam"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "nodeid",
      .name     = N_("Node ID"),
      .desc     = N_("Client node ID. Leave field empty to generate a random ID."),
      .set      = caclient_cccam_nodeid_set,
      .get      = caclient_cccam_nodeid_get,
      .group    = 2,
    },
    {
      .type     = PT_INT,
      .id       = "extended",
      .name     = N_("Extended mode"),
      .desc     = N_("Extended mode settings."),
      .off      = offsetof(cccam_t, cccam_extended_conf),
      .list     = caclient_cccam_class_cccam_extended_list,
      .def.i    = CCCAM_EXTENDED_EXT,
      .opts     = PO_DOC_NLIST,
      .group    = 2,
    },
    {
      .type     = PT_INT,
      .id       = "version",
      .name     = N_("Version"),
      .desc     = N_("Protocol version."),
      .off      = offsetof(cccam_t, cccam_version),
      .list     = caclient_cccam_class_cccam_version_list,
      .def.i    = CCCAM_VERSION_2_3_0,
      .opts     = PO_DOC_NLIST,
      .group    = 2,
    },
    {
      .type     = PT_INT,
      .id       = "keepalive_interval",
      .name     = N_("Keepalive interval (0=disable)"),
      .desc     = N_("Keepalive interval in seconds"),
      .off      = offsetof(cccam_t, cc_keepalive_interval),
      .def.i    = CCCAM_KEEPALIVE_INTERVAL,
      .group    = 4,
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

  cccam->cc_subsys = LS_CCCAM;
  cccam->cc_id     = "cccam";

  tvh_mutex_init(&cccam->cc_mutex, NULL);
  tvh_cond_init(&cccam->cc_cond, 1);
  cccam->cac_free         = cc_free;
  cccam->cac_start        = cc_service_start;
  cccam->cac_conf_changed = cccam_conf_changed;
  cccam->cac_caid_update  = cc_caid_update;
  cccam->cc_keepalive_interval = CCCAM_KEEPALIVE_INTERVAL;
  cccam->cccam_version    = CCCAM_VERSION_2_3_0;
  cccam->cc_init_session  = cccam_init_session;
  cccam->cc_read          = cccam_read;
  cccam->cc_send_ecm      = cccam_send_ecm;
  cccam->cc_send_emm      = cccam_send_emm;
  cccam->cc_keepalive     = cccam_send_ka;
  cccam->cc_no_services   = cccam_no_services;
  return (caclient_t *)cccam;
}
