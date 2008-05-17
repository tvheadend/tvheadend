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
#include <rpc/des_crypt.h>

#include "tvhead.h"
#include "tcp.h"
#include "psi.h"
#include "tsdemux.h"
#include "ffdecsa/FFdecsa.h"
#include "dispatch.h"
#include "transports.h"
#include "cwc.h"
#include "notify.h"

static int cwc_tally;

#define CWC_KEEPALIVE_INTERVAL 600

#define CWS_NETMSGSIZE 240
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

struct cwc_queue cwcs;

extern char *cwc_krypt(const char *key, const char *salt);

static LIST_HEAD(, cwc_transport) cwc_pending_requests;

typedef struct cwc_transport {
  th_descrambler_t ct_head;

  th_transport_t *ct_transport;

  struct cwc *ct_cwc;

  LIST_ENTRY(cwc_transport) ct_cwc_link; /* Always linked */

  LIST_ENTRY(cwc_transport) ct_link; /* linked if we are waiting
					on a reply from server */
  int ct_pending;

  enum {
    CT_UNKNOWN,
    CT_RESOLVED,
    CT_FORBIDDEN
  } ct_keystate;

  uint16_t ct_seq;

  uint8_t ct_ecm[256];
  int ct_ecmsize;

  void *ct_keys;

#define CT_CLUSTER_SIZE 32

  uint8_t ct_tsbcluster[188 * CT_CLUSTER_SIZE];
  int ct_fill;

} cwc_transport_t;


/**
 *
 */
static void
cwc_set_state(cwc_t *cwc, int newstate, const char *errtxt)
{
  cwc->cwc_state = newstate;

  if(newstate == CWC_STATE_IDLE)
    cwc->cwc_errtxt = errtxt;

  notify_cwc_status_change(cwc);
}



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
static uint8_t *
des_key_spread(uint8_t *normal)
{
  static uint8_t spread[16];

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
  return spread;
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
des_encrypt(uint8_t *buffer, int len, uint8_t *deskey)
{
  uint8_t checksum = 0;
  uint8_t noPadBytes;
  uint8_t padBytes[7];
  char ivec[8];
  uint16_t i;

  if (!deskey) return len;
  noPadBytes = (8 - ((len - 1) % 8)) % 8;
  if (len + noPadBytes + 1 >= CWS_NETMSGSIZE-8) return -1;
  des_random_get(padBytes, noPadBytes);
  for (i = 0; i < noPadBytes; i++) buffer[len++] = padBytes[i];
  for (i = 2; i < len; i++) checksum ^= buffer[i];
  buffer[len++] = checksum;
  des_random_get((uint8_t *)ivec, 8);
  memcpy(buffer+len, ivec, 8);
  for (i = 2; i < len; i += 8) {
    cbc_crypt((char *)deskey  , (char *) buffer+i, 8, DES_ENCRYPT, ivec);
    ecb_crypt((char *)deskey+8, (char *) buffer+i, 8, DES_DECRYPT);
    ecb_crypt((char *)deskey  , (char *) buffer+i, 8, DES_ENCRYPT);
    memcpy(ivec, buffer+i, 8);
  }
  len += 8;
  return len;
}

/**
 *
 */
static int 
des_decrypt(uint8_t *buffer, int len, uint8_t *deskey)
{
  char ivec[8];
  char nextIvec[8];
  int i;
  uint8_t checksum = 0;

  if (!deskey) return len;
  if ((len-2) % 8 || (len-2) < 16) return -1;
  len -= 8;
  memcpy(nextIvec, buffer+len, 8);
  for (i = 2; i < len; i += 8)
    {
      memcpy(ivec, nextIvec, 8);
      memcpy(nextIvec, buffer+i, 8);
      ecb_crypt((char *)deskey  , (char *) buffer+i, 8, DES_DECRYPT);
      ecb_crypt((char *)deskey+8, (char *) buffer+i, 8, DES_ENCRYPT);
      cbc_crypt((char *)deskey  , (char *) buffer+i, 8, DES_DECRYPT, ivec);
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
  uint8_t des14[14];
  int i;

  for (i = 0; i < 14; i++) 
    des14[i] = cwc->cwc_confedkey[i] ^ k[i];
  memcpy(cwc->cwc_key, des_key_spread(des14), 16);
}

/**
 *
 */
static void
des_make_session_key(cwc_t *cwc)
{
  uint8_t des14[14], *k2 = (uint8_t *)cwc->cwc_password;
  int i, l = strlen(cwc->cwc_password);

  memcpy(des14, cwc->cwc_confedkey, 14);

  for (i = 0; i < l; i++)
    des14[i % 14] ^= k2[i];

  memcpy(cwc->cwc_key, des_key_spread(des14), 16);
}


/**
 *
 */
static void
cwc_timeout(void *aux, int64_t now)
{
  cwc_t *cwc = aux;
  tcp_disconnect(&cwc->cwc_tcp_session, ETIMEDOUT);
}


/**
 *
 */
static int
cwc_send_msg(cwc_t *cwc, const uint8_t *msg, size_t len, int sid)
{
  tcp_session_t *ses = &cwc->cwc_tcp_session;

  uint8_t *buf = malloc(CWS_NETMSGSIZE);

  memset(buf, 0, 12);
  memcpy(buf + 12, msg, len);

  len += 12;

  cwc->cwc_seq++;

  buf[2] = cwc->cwc_seq >> 8;
  buf[3] = cwc->cwc_seq;
  buf[4] = sid >> 8;
  buf[5] = sid;

  if((len = des_encrypt(buf, len, cwc->cwc_key)) < 0) {
    free(buf);
    return -1;
  }

  buf[0] = (len - 2) >> 8;
  buf[1] =  len - 2;

  tcp_send_msg(ses, &ses->tcp_q_hi, buf, len);

  /* Expect a response within 4 seconds */
  dtimer_arm(&cwc->cwc_idle_timer, cwc_timeout, cwc, 4);
  return cwc->cwc_seq;
}



/**
 * Card data command
 */
static void
cwc_send_data_req(cwc_t *cwc)
{
  uint8_t buf[CWS_NETMSGSIZE];

  cwc_set_state(cwc, CWC_STATE_WAIT_CARD_DATA, NULL);

  buf[0] = MSG_CARD_DATA_REQ;
  buf[1] = 0;
  buf[2] = 0;

  cwc_send_msg(cwc, buf, 3, 0);
}


/**
 * Send keep alive
 */
static void
cwc_send_ka(void *aux, int64_t now)
{
  cwc_t *cwc = aux;
  uint8_t buf[CWS_NETMSGSIZE];

  buf[0] = MSG_KEEPALIVE;
  buf[1] = 0;
  buf[2] = 0;

  cwc_send_msg(cwc, buf, 3, 0);
  dtimer_arm(&cwc->cwc_send_ka_timer, cwc_send_ka, cwc,
	     CWC_KEEPALIVE_INTERVAL);
}


/**
 * Handle reply to card data request
 */
static int
cwc_dispatch_card_data_reply(cwc_t *cwc, uint8_t msgtype, 
			     uint8_t *msg, int len)
{
  int plen;
  const char *n;

  if(msgtype != MSG_CARD_DATA)
    return EBADMSG;

  msg += 12;
  len -= 12;
  plen = (msg[1] & 0xf) << 8 | msg[2];

  if(plen < 14)
    return EBADMSG;
  
  cwc->cwc_caid = (msg[4] << 8) | msg[5];
  n = psi_caid2name(cwc->cwc_caid) ?: "Unknown";

  syslog(LOG_INFO, "cwc: %s: Connected as user 0x%02x "
	 "to a %s-card [0x%04x : %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x] "
	 "with %d providers", 
	 cwc->cwc_tcp_session.tcp_hostname,
	 msg[3], n, cwc->cwc_caid, 
	 msg[6], msg[7], msg[8], msg[9], msg[10], msg[11], msg[12], msg[13],
	 msg[14]);

  cwc_set_state(cwc, CWC_STATE_RUNNING, NULL);

  /* Send KA every CWC_KEEPALIVE_INTERVAL seconds */

  dtimer_arm(&cwc->cwc_send_ka_timer, cwc_send_ka, cwc,
	     CWC_KEEPALIVE_INTERVAL);

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
  int pl = strlen(cwc->cwc_password) + 1;

  cwc_set_state(cwc, CWC_STATE_WAIT_LOGIN_ACK, NULL);

  buf[0] = MSG_CLIENT_2_SERVER_LOGIN;
  buf[1] = 0;
  buf[2] = ul + pl;
  memcpy(buf + 3,      cwc->cwc_username, ul);
  memcpy(buf + 3 + ul, cwc->cwc_password, pl);
 
  cwc_send_msg(cwc, buf, ul + pl + 3, 0);
}


/**
 * Handle reply to login
 */
static int
cwc_dispatch_login_reply(cwc_t *cwc, uint8_t msgtype, uint8_t *msg, int len)
{
  switch(msgtype) {
  case MSG_CLIENT_2_SERVER_LOGIN_ACK:
    des_make_session_key(cwc);
    cwc_send_data_req(cwc);
    return 0;
  
  case MSG_CLIENT_2_SERVER_LOGIN_NAK:
    return EACCES;
    
  default:
    return EBADMSG;
  }
}


/**
 * Handle reply to login
 */
static int
cwc_dispatch_running_reply(cwc_t *cwc, uint8_t msgtype, uint8_t *msg, int len)
{
  cwc_transport_t *ct;
  uint16_t seq = (msg[2] << 8) | msg[3];
  th_transport_t *t;

  len -= 12;
  msg += 12;

  switch(msgtype) {
  case 0x80:
  case 0x81:
    LIST_FOREACH(ct, &cwc_pending_requests, ct_link) {
      if(ct->ct_seq == seq)
	break;
    }

    if(ct == NULL)
      return 0;

    t = ct->ct_transport;

    LIST_REMOVE(ct, ct_link);
    ct->ct_pending = 0;

    if(len < 19) {

      if(ct->ct_keystate != CT_FORBIDDEN) {
	syslog(LOG_ERR, 
	       "Can not descramble \"%s\" for service \"%s\", access denied",
	       t->tht_identifier, t->tht_svcname);
	ct->ct_keystate = CT_FORBIDDEN;
      }

      return 0;
    }

    if(ct->ct_keystate != CT_RESOLVED)
      syslog(LOG_INFO,
	     "Obtained key for \"%s\" for service \"%s\"",
	     t->tht_identifier, t->tht_svcname);

    ct->ct_keystate = CT_RESOLVED;
    set_control_words(ct->ct_keys, msg + 3, msg + 3 + 8);
    break;
  }
  return 0;
}


/**
 *
 */
static void
cwc_data_input(cwc_t *cwc)
{
  int msglen, r;
  tcp_session_t *ses = &cwc->cwc_tcp_session;
  int fd = ses->tcp_fd;

  if(cwc->cwc_state == CWC_STATE_WAIT_LOGIN_KEY) {

    r = read(fd, cwc->cwc_buf + cwc->cwc_bufptr, 14 - cwc->cwc_bufptr);
    if(r < 1) {
      tcp_disconnect(ses, r == 0 ? ECONNRESET : errno);
      return;
    }

    cwc->cwc_bufptr += r;
    if(cwc->cwc_bufptr < 14)
      return;

    des_make_login_key(cwc, cwc->cwc_buf);
    cwc_send_login(cwc);
    cwc->cwc_bufptr = 0;

  } else {

    if(cwc->cwc_bufptr < 2) {
      msglen = 2;
    } else {
      msglen = ((cwc->cwc_buf[0] << 8) | cwc->cwc_buf[1]) + 2;
      if(msglen >= CWS_NETMSGSIZE) {
	tcp_disconnect(ses, EMSGSIZE);
	return;
      }
    }

    r = read(fd, cwc->cwc_buf + cwc->cwc_bufptr, msglen - cwc->cwc_bufptr);
    if(r < 1) {
      tcp_disconnect(ses, r == 0 ? ECONNRESET : errno);
      return;
    }

    cwc->cwc_bufptr += r;

    if(msglen > 2 && cwc->cwc_bufptr == msglen) {
      if((msglen = des_decrypt(cwc->cwc_buf, msglen, cwc->cwc_key)) < 15) {
	tcp_disconnect(ses, EILSEQ);
	return;
      }
      cwc->cwc_bufptr = 0;

      switch(cwc->cwc_state) {
      case CWC_STATE_WAIT_LOGIN_ACK:
	r = cwc_dispatch_login_reply(cwc, cwc->cwc_buf[12],
				     cwc->cwc_buf, msglen);
	break;

      case CWC_STATE_WAIT_CARD_DATA:
	r = cwc_dispatch_card_data_reply(cwc, cwc->cwc_buf[12],
					 cwc->cwc_buf, msglen);
	break;

      case CWC_STATE_RUNNING:
	r = cwc_dispatch_running_reply(cwc, cwc->cwc_buf[12],
					 cwc->cwc_buf, msglen);
	break;

      default:
	r = EBADMSG;
	break;
      }

      if(r != 0) {
	tcp_disconnect(ses, r);
	return;
      }
    }
  }
}



/**
 *
 */
static void
cwc_tcp_callback(tcpevent_t event, void *tcpsession)
{
  cwc_t *cwc = tcpsession;

  dtimer_disarm(&cwc->cwc_idle_timer);

  switch(event) {
  case TCP_CONNECT:
    cwc->cwc_seq = 0;
    cwc->cwc_bufptr = 0;
    cwc_set_state(cwc, CWC_STATE_WAIT_LOGIN_KEY, NULL);
    /* We want somthing within 20 seconds */
    dtimer_arm(&cwc->cwc_idle_timer, cwc_timeout, cwc, 20); 
    break;

  case TCP_DISCONNECT:
    cwc_set_state(cwc, CWC_STATE_IDLE, "Disconnected");
    dtimer_disarm(&cwc->cwc_send_ka_timer);
    break;

  case TCP_INPUT:
    cwc_data_input(cwc);
    break;
  }
}


/**
 *
 */
static void
cwc_table_input(struct th_descrambler *td, struct th_transport *t,
		struct th_stream *st, uint8_t *data, int len)
{
  cwc_transport_t *ct = (cwc_transport_t *)td;
  uint16_t sid = t->tht_dvb_service_id;
  cwc_t *cwc = ct->ct_cwc;

  if(cwc->cwc_caid != st->st_caid)
    return;

  if((data[0] & 0xf0) != 0x80)
    return;

  switch(data[0]) {
  case 0x80:
  case 0x81:
    /* ECM */

    if(ct->ct_pending)
      return;

    if(ct->ct_ecmsize == len && !memcmp(ct->ct_ecm, data, len))
      return; /* key already sent */

    if(cwc->cwc_state != CWC_STATE_RUNNING) {
      /* New key, but we are not connected (anymore), can not descramble */
      ct->ct_keystate = CT_UNKNOWN;
      break;
    }

    memcpy(ct->ct_ecm, data, len);
    ct->ct_ecmsize = len;
    ct->ct_seq = cwc_send_msg(cwc, data, len, sid);
    LIST_INSERT_HEAD(&cwc_pending_requests, ct, ct_link);
    ct->ct_pending = 1;
    break;

  default:
    /* EMM */
    cwc_send_msg(cwc, data, len, sid);
    break;
  }
}


/**
 *
 */
static int
cwc_descramble(th_descrambler_t *td, th_transport_t *t, struct th_stream *st,
	       uint8_t *tsb)
{
  cwc_transport_t *ct = (cwc_transport_t *)td;
  int r, i;
  unsigned char *vec[3];
  uint8_t *t0;

  if(ct->ct_keystate == CT_FORBIDDEN)
    return 1;

  if(ct->ct_keystate != CT_RESOLVED)
    return -1;

  memcpy(ct->ct_tsbcluster + ct->ct_fill * 188, tsb, 188);
  ct->ct_fill++;

  if(ct->ct_fill != CT_CLUSTER_SIZE)
    return 0;

  ct->ct_fill = 0;

  vec[0] = ct->ct_tsbcluster;
  vec[1] = ct->ct_tsbcluster + CT_CLUSTER_SIZE * 188;
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
 *
 */
static void
cwc_transport_destroy(cwc_transport_t *ct)
{
  if(ct->ct_pending)
    LIST_REMOVE(ct, ct_link);

  LIST_REMOVE(ct, ct_cwc_link);

  free_key_struct(ct->ct_keys);
  free(ct);
}

/**
 *
 */
static void 
cwc_transport_stop(th_descrambler_t *td)
{
  LIST_REMOVE(td, td_transport_link);

  cwc_transport_destroy((cwc_transport_t *)td);
}


/**
 * Check if our CAID's matches, and if so, link
 */
void
cwc_transport_start(th_transport_t *t)
{
  cwc_t *cwc;
  cwc_transport_t *ct;
  th_descrambler_t *td;
  th_stream_t *st;

  TAILQ_FOREACH(cwc, &cwcs, cwc_link) {

    LIST_FOREACH(st, &t->tht_streams, st_link)
      if(st->st_caid == cwc->cwc_caid)
	break;

    if(st == NULL)
      continue;

    ct = calloc(1, sizeof(cwc_transport_t));
    ct->ct_keys = get_key_struct();
    ct->ct_cwc = cwc;
    ct->ct_transport = t;
    LIST_INSERT_HEAD(&cwc->cwc_transports, ct, ct_cwc_link);
    td = &ct->ct_head;

    td->td_stop       = cwc_transport_stop;
    td->td_table      = cwc_table_input;
    td->td_descramble = cwc_descramble;
    LIST_INSERT_HEAD(&t->tht_descramblers, td, td_transport_link);
  }
}


/**
 *
 */
static void
cwc_save(void)
{
  cwc_t *cwc;

  FILE *fp;
  char buf[400];

  snprintf(buf, sizeof(buf), "%s/cwc.cfg",  settings_dir);
  if((fp = settings_open_for_write(buf)) == NULL)
    return;

  TAILQ_FOREACH(cwc, &cwcs, cwc_link) {

    fprintf(fp, "cwc {\n");
    fprintf(fp, "\thostname = %s\n", cwc->cwc_tcp_session.tcp_hostname);
    fprintf(fp, "\tport = %d\n",     cwc->cwc_tcp_session.tcp_port);

    fprintf(fp, "\tusername = %s\n", cwc->cwc_username);
    fprintf(fp, "\tpassword = %s\n", cwc->cwc_password);
    fprintf(fp, "\tdeskey = "
	    "%02x:%02x:%02x:%02x:%02x:%02x:%02x:"
	    "%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
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
    fprintf(fp, "\tenabled = %d\n", cwc->cwc_tcp_session.tcp_enabled);


    fprintf(fp, "}\n");
  }
  fclose(fp);

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
const char *
cwc_add(const char *hostname, const char *porttxt, const char *username, 
	const char *password, const char *deskey, const char *enabled,
	int save, int salt)
{
  const char *k;
  cwc_t *cwc;
  uint8_t key[14];

  int i, u, l, port;

  if(hostname == NULL || strlen(hostname) < 1)
    return "Invalid hostname";

  if(porttxt == NULL)
    return "Invalid port";
  
  port = atoi(porttxt);
  if(port < 1 || port > 65535)
    return "Invalid port";

  if(username == NULL || strlen(username) < 1)
    return "Invalid username";
  if(password == NULL || strlen(password) < 1)
    return "Invalid password";

  if(deskey == NULL)
    return "Invalid DES-key";

  k = deskey;
  for(i = 0; i < 14; i++) {
    while(*k != 0 && !isxdigit(*k)) k++;
    if(*k == 0)
      break;
    u = nibble(*k++);
    while(*k != 0 && !isxdigit(*k)) k++;
    if(*k == 0)
      break;
    l = nibble(*k++);
    key[i] = (u << 4) | l;
  }
  
  if(i != 14)
    return "DES-key does not contain 14 valid hexadecimal bytes";
    
  if(enabled == NULL)
    return "Enabled not set";

  cwc = tcp_create_client(hostname, port, sizeof(cwc_t), 
			  "cwc", cwc_tcp_callback, atoi(enabled));
  cwc->cwc_username = strdup(username);

  if(salt) {
    cwc->cwc_password = strdup(cwc_krypt(password, "$1$abcdefgh$"));
  } else {
    cwc->cwc_password = strdup(password);
  }

  cwc->cwc_id = ++cwc_tally;

  memcpy(cwc->cwc_confedkey, key, 14);
  TAILQ_INSERT_TAIL(&cwcs, cwc, cwc_link);  

  if(save)
    cwc_save();
  return NULL;
}

/**
 *
 */
cwc_t *
cwc_find(int id)
{
  cwc_t *cwc;
  TAILQ_FOREACH(cwc, &cwcs, cwc_link)
    if(cwc->cwc_id == id)
      break;
  return cwc;
}


/**
 *
 */
void
cwc_delete(cwc_t *cwc)
{
  cwc_transport_t *ct;

  while((ct = LIST_FIRST(&cwc->cwc_transports)) != NULL)
    cwc_transport_destroy(ct);

  TAILQ_REMOVE(&cwcs, cwc, cwc_link);  
  free((void *)cwc->cwc_password);
  free((void *)cwc->cwc_username);

  tcp_destroy_client(&cwc->cwc_tcp_session);
  cwc_save();
}

/**
 *
 */
void
cwc_set_enable(cwc_t *cwc, int enabled)
{
  tcp_enable_disable(&cwc->cwc_tcp_session, enabled);
  cwc_save();
}


/**
 *
 */
void
cwc_init(void)
{
  struct config_head cl;
  config_entry_t *ce;
  char buf[400];

  TAILQ_INIT(&cwcs);
  
  snprintf(buf, sizeof(buf), "%s/cwc.cfg",  settings_dir);
  
  TAILQ_INIT(&cl);
  config_read_file0(buf, &cl);

  TAILQ_FOREACH(ce, &cl, ce_link) {
    if(ce->ce_type != CFG_SUB || strcasecmp("cwc", ce->ce_key))
      continue;
    
    cwc_add(config_get_str_sub(&ce->ce_sub, "hostname", NULL),
	    config_get_str_sub(&ce->ce_sub, "port",     NULL),
	    config_get_str_sub(&ce->ce_sub, "username", NULL),
	    config_get_str_sub(&ce->ce_sub, "password", NULL),
	    config_get_str_sub(&ce->ce_sub, "deskey",   NULL),
	    config_get_str_sub(&ce->ce_sub, "enabled",  NULL),
	    0, 0);
  }
}

/**
 *
 */
static struct strtab cwcstatustab[] = {
  { "Waiting for login key",                 CWC_STATE_WAIT_LOGIN_KEY },
  { "Waiting for login ACK",                 CWC_STATE_WAIT_LOGIN_ACK },
  { "Waiting for card data",                 CWC_STATE_WAIT_CARD_DATA },
  { "Running",                               CWC_STATE_RUNNING },
};

const char *
cwc_status_to_text(struct cwc *cwc)
{
  static char buf[100];
  char buf2[30];
  const char *n;

  if(cwc->cwc_state == CWC_STATE_IDLE)
    return cwc->cwc_errtxt ?: "Not connected";

  if(cwc->cwc_state == CWC_STATE_RUNNING) {

    n = psi_caid2name(cwc->cwc_caid);
    if(n == NULL) {
      snprintf(buf2, sizeof(buf2), "0x%x", cwc->cwc_caid);
      n = buf2;
    }

    snprintf(buf, sizeof(buf), "Connected to a %s card", n);
    return buf;
  }

  return val2str(cwc->cwc_state, cwcstatustab) ?: "Invalid status";
}
