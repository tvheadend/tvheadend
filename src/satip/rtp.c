/*
 *  Tvheadend - SAT-IP server - RTP part
 *
 *  Copyright (C) 2015 Jaroslav Kysela
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

#include <signal.h>
#include <ctype.h>
#include "tvheadend.h"
#include "config.h"
#include "input.h"
#include "streaming.h"
#include "satip/server.h"
#include <netinet/ip.h>
#if ENABLE_ANDROID
#include <sys/socket.h>
#endif

#define RTP_PACKETS 128
#define RTP_PAYLOAD (7*188+12)
#define RTCP_PAYLOAD (1420)

typedef struct satip_rtp_table {
  TAILQ_ENTRY(satip_rtp_table) link;
  mpegts_psi_table_t tbl;
  int pid;
  int remove_mark;
} satip_rtp_table_t;

typedef struct satip_rtp_session {
  TAILQ_ENTRY(satip_rtp_session) link;
  pthread_t tid;
  void *id;
  struct sockaddr_storage peer;
  struct sockaddr_storage peer2;
  int port;
  th_subscription_t *subs;
  streaming_queue_t *sq;
  int fd_rtp;
  int fd_rtcp;
  int frontend;
  int source;
  dvb_mux_conf_t dmc;
  mpegts_apids_t pids;
  TAILQ_HEAD(, satip_rtp_table) pmt_tables;
  udp_multisend_t um;
  struct iovec *um_iovec;
  int um_packet;
  uint16_t seq;
  signal_status_t sig;
  int sig_lock;
  pthread_mutex_t lock;
  uint8_t *table_data;
  int table_data_len;
} satip_rtp_session_t;

static pthread_mutex_t satip_rtp_lock;
static pthread_t satip_rtcp_tid;
static int satip_rtcp_run;
static TAILQ_HEAD(, satip_rtp_session) satip_rtp_sessions;

static void
satip_rtp_pmt_cb(mpegts_psi_table_t *mt, const uint8_t *buf, int len)
{
  satip_rtp_session_t *rtp;
  uint8_t out[1024], *ob;
  // uint16_t sid, pid;
  int l, ol;

  memcpy(out, buf, ol = 3);
  buf += ol;
  len -= ol;

  // sid = (buf[0] << 8) | buf[1];
  l = (buf[7] & 0x0f) << 8 | buf[8];

  if (l > len - 9)
    return;

  rtp = (satip_rtp_session_t *)mt->mt_opaque;

  memcpy(out + ol, buf, 9);

  ol  += 9;     /* skip common descriptors */
  buf += 9 + l;
  len -= 9 + l;

  /* no common descriptors */
  out[7+3] &= 0xf0;
  out[8+3] = 0;

  while (len >= 5) {
    //pid = (buf[1] & 0x1f) << 8 | buf[2];
    l   = (buf[3] & 0xf) << 8 | buf[4];

    if (l > len - 5)
      return;

    if (sizeof(out) < ol + l + 5 + 4 /* crc */) {
      tvherror("pass", "PMT entry too long (%i)", l);
      return;
    }

    memcpy(out + ol, buf, 5 + l);
    ol += 5 + l;

    buf += 5 + l;
    len -= 5 + l;
  }

  /* update section length */
  out[1] = (out[1] & 0xf0) | ((ol + 4 - 3) >> 8);
  out[2] = (ol + 4 - 3) & 0xff;

  ol = dvb_table_append_crc32(out, ol, sizeof(out));

  if (ol > 0 && (l = dvb_table_remux(mt, out, ol, &ob)) > 0) {
    rtp->table_data = ob;
    rtp->table_data_len = l;
  }
}

static void
satip_rtp_header(satip_rtp_session_t *rtp)
{
  struct iovec *v = rtp->um_iovec + rtp->um_packet;
  uint8_t *data = v->iov_base;
  uint32_t tstamp = dispatch_clock + rtp->seq;

  rtp->seq++;

  v->iov_len = 12;
  data[0] = 0x80;
  data[1] = 33;
  data[2] = (rtp->seq >> 8) & 0xff;
  data[3] = rtp->seq & 0xff;
  data[4] = (tstamp >> 24) & 0xff;
  data[5] = (tstamp >> 16) & 0xff;
  data[6] = (tstamp >> 8) & 0xff;
  data[7] = tstamp & 0xff;
  memset(data + 8, 0xa5, 4);
}

static int
satip_rtp_send(satip_rtp_session_t *rtp)
{
  struct iovec *v = rtp->um_iovec, *v2;
  int packets, copy, len, r;
  if (v->iov_len == RTP_PAYLOAD) {
    packets = rtp->um_packet;
    v2 = v + packets;
    copy = 1;
    if (v2->iov_len == RTP_PAYLOAD) {
      packets++;
      copy = 0;
    }
    r = udp_multisend_send(&rtp->um, rtp->fd_rtp, packets);
    if (r < 0)
      return r;
    if (copy)
      memcpy(v->iov_base, v2->iov_base, len = v2->iov_len);
    else
      len = 0;
    rtp->um_packet = 0;
    udp_multisend_clean(&rtp->um);
    v->iov_len = len;
  }
  if (v->iov_len == 0)
    satip_rtp_header(rtp);
  return 0;
}

static inline int
satip_rtp_append_data(satip_rtp_session_t *rtp, struct iovec **_v, uint8_t *data)
{
  struct iovec *v = *_v;
  int r;
  assert(v->iov_len + 188 <= RTP_PAYLOAD);
  memcpy(v->iov_base + v->iov_len, data, 188);
  v->iov_len += 188;
  if (v->iov_len == RTP_PAYLOAD) {
    if ((rtp->um_packet + 1) == RTP_PACKETS) {
      r = satip_rtp_send(rtp);
      if (r < 0)
        return r;
    } else {
      rtp->um_packet++;
      satip_rtp_header(rtp);
    }
    *_v = rtp->um_iovec + rtp->um_packet;
  } else {
    assert(v->iov_len < RTP_PAYLOAD);
  }
  return 0;
}

static int
satip_rtp_loop(satip_rtp_session_t *rtp, uint8_t *data, int len)
{
  int i, j, pid, last_pid = -1, r;
  mpegts_apid_t *pids = rtp->pids.pids;
  struct iovec *v = rtp->um_iovec + rtp->um_packet;
  satip_rtp_table_t *tbl;

  assert((len % 188) == 0);
  if (len > 0)
    rtp->sig_lock = 1;
  for ( ; len >= 188 ; data += 188, len -= 188) {
    pid = ((data[1] & 0x1f) << 8) | data[2];
    if (pid != last_pid && !rtp->pids.all) {
      for (i = 0; i < rtp->pids.count; i++) {
        j = pids[i].pid;
        if (pid < j) break;
        if (j == pid) goto found;
      }
      continue;
found:
      TAILQ_FOREACH(tbl, &rtp->pmt_tables, link)
        if (tbl->pid == pid) {
          dvb_table_parse(&tbl->tbl, "-", data, 188, 1, 0, satip_rtp_pmt_cb);
          if (rtp->table_data_len) {
            for (i = 0; i < rtp->table_data_len; i += 188) {
              r = satip_rtp_append_data(rtp, &v, data);
              if (r < 0)
                return r;
            }
            free(rtp->table_data);
            rtp->table_data = NULL;
          }
          break;
        }
      if (tbl)
        continue;
      last_pid = pid;
    }
    r = satip_rtp_append_data(rtp, &v, data);
    if (r < 0)
      return r;
  }
  return 0;
}

static void
satip_rtp_signal_status(satip_rtp_session_t *rtp, signal_status_t *sig)
{
  pthread_mutex_lock(&rtp->lock);
  rtp->sig = *sig;
  pthread_mutex_unlock(&rtp->lock);
}

static void *
satip_rtp_thread(void *aux)
{
  satip_rtp_session_t *rtp = aux;
  streaming_queue_t *sq = rtp->sq;
  streaming_message_t *sm;
  th_subscription_t *subs = rtp->subs;
  pktbuf_t *pb;
  char peername[50];
  int alive = 1, fatal = 0, r;

  tcp_get_str_from_ip((struct sockaddr *)&rtp->peer, peername, sizeof(peername));
  tvhdebug("satips", "RTP streaming to %s:%d open", peername, rtp->port);

  pthread_mutex_lock(&sq->sq_mutex);
  while (rtp->sq && !fatal) {
    sm = TAILQ_FIRST(&sq->sq_queue);
    if (sm == NULL) {
      r = satip_rtp_send(rtp);
      if (r) {
        fatal = 1;
        continue;
      }
      pthread_cond_wait(&sq->sq_cond, &sq->sq_mutex);
      continue;
    }
    streaming_queue_remove(sq, sm);
    pthread_mutex_unlock(&sq->sq_mutex);

    switch (sm->sm_type) {
    case SMT_MPEGTS:
      pb = sm->sm_data;
      subscription_add_bytes_out(subs, pktbuf_len(pb));
      pthread_mutex_lock(&rtp->lock);
      r = satip_rtp_loop(rtp, pktbuf_ptr(pb), pktbuf_len(pb));
      pthread_mutex_unlock(&rtp->lock);
      if (r) fatal = 1;
      break;
    case SMT_SIGNAL_STATUS:
      satip_rtp_signal_status(rtp, sm->sm_data);
      break;
    case SMT_NOSTART:
    case SMT_EXIT:
      alive = 0;
      break;

    case SMT_START:
    case SMT_STOP:
    case SMT_NOSTART_WARN:
    case SMT_PACKET:
    case SMT_GRACE:
    case SMT_SKIP:
    case SMT_SPEED:
    case SMT_SERVICE_STATUS:
    case SMT_TIMESHIFT_STATUS:
    case SMT_DESCRAMBLE_INFO:
      break;
    }

    streaming_msg_free(sm);
    pthread_mutex_lock(&sq->sq_mutex);
  }
  pthread_mutex_unlock(&sq->sq_mutex);

  tvhdebug("satips", "RTP streaming to %s:%d closed (%s request)",
           peername, rtp->port, alive ? "remote" : "streaming");

  return NULL;
}

/*
 *
 */
static satip_rtp_session_t *
satip_rtp_find(void *id)
{
  satip_rtp_session_t *rtp;

  TAILQ_FOREACH(rtp, &satip_rtp_sessions, link)
    if (rtp->id == id)
      break;
  return rtp;
}

/*
 *
 */
void satip_rtp_queue(void *id, th_subscription_t *subs,
                     streaming_queue_t *sq,
                     struct sockaddr_storage *peer, int port,
                     int fd_rtp, int fd_rtcp,
                     int frontend, int source, dvb_mux_conf_t *dmc,
                     mpegts_apids_t *pids)
{
  satip_rtp_session_t *rtp = calloc(1, sizeof(*rtp));

  if (rtp == NULL)
    return;

  rtp->id = id;
  rtp->peer = *peer;
  rtp->peer2 = *peer;
  IP_PORT_SET(rtp->peer2, htons(port + 1));
  rtp->port = port;
  rtp->fd_rtp = fd_rtp;
  rtp->fd_rtcp = fd_rtcp;
  rtp->subs = subs;
  rtp->sq = sq;
  mpegts_pid_init(&rtp->pids);
  mpegts_pid_copy(&rtp->pids, pids);
  TAILQ_INIT(&rtp->pmt_tables);
  udp_multisend_init(&rtp->um, RTP_PACKETS, RTP_PAYLOAD, &rtp->um_iovec);
  satip_rtp_header(rtp);
  rtp->frontend = frontend;
  rtp->dmc = *dmc;
  rtp->source = source;
  pthread_mutex_init(&rtp->lock, NULL);

  if (config.dscp >= 0) {
    socket_set_dscp(rtp->fd_rtp, config.dscp, NULL, 0);
    socket_set_dscp(rtp->fd_rtcp, config.dscp, NULL, 0);
  } else {
    socket_set_dscp(rtp->fd_rtp, IPTOS_DSCP_EF, NULL, 0);
    socket_set_dscp(rtp->fd_rtcp, IPTOS_DSCP_EF, NULL, 0);
  }

  pthread_mutex_lock(&satip_rtp_lock);
  TAILQ_INSERT_TAIL(&satip_rtp_sessions, rtp, link);
  tvhthread_create(&rtp->tid, NULL, satip_rtp_thread, rtp, "satip-rtp");
  pthread_mutex_unlock(&satip_rtp_lock);
}

void satip_rtp_update_pids(void *id, mpegts_apids_t *pids)
{
  satip_rtp_session_t *rtp;

  pthread_mutex_lock(&satip_rtp_lock);
  rtp = satip_rtp_find(id);
  if (rtp) {
    pthread_mutex_lock(&rtp->lock);
    mpegts_pid_copy(&rtp->pids, pids);
    pthread_mutex_unlock(&rtp->lock);
  }
  pthread_mutex_unlock(&satip_rtp_lock);
}

void satip_rtp_update_pmt_pids(void *id, mpegts_apids_t *pmt_pids)
{
  satip_rtp_session_t *rtp;
  satip_rtp_table_t *tbl, *tbl_next;
  int i, pid;

  pthread_mutex_lock(&satip_rtp_lock);
  rtp = satip_rtp_find(id);
  if (rtp) {
    pthread_mutex_lock(&rtp->lock);
    TAILQ_FOREACH(tbl, &rtp->pmt_tables, link)
      if (!mpegts_pid_rexists(pmt_pids, tbl->pid))
        tbl->remove_mark = 1;
    for (i = 0; i < pmt_pids->count; i++) {
      pid = pmt_pids->pids[i].pid;
      TAILQ_FOREACH(tbl, &rtp->pmt_tables, link)
        if (tbl->pid == pid)
          break;
      if (!tbl) {
        tbl = calloc(1, sizeof(*tbl));
        dvb_table_parse_init(&tbl->tbl, "satip-pmt", pid, rtp);
        tbl->pid = pid;
        TAILQ_INSERT_TAIL(&rtp->pmt_tables, tbl, link);
      }
    }
    for (tbl = TAILQ_FIRST(&rtp->pmt_tables); tbl; tbl = tbl_next){
      tbl_next = TAILQ_NEXT(tbl, link);
      if (tbl->remove_mark) {
        TAILQ_REMOVE(&rtp->pmt_tables, tbl, link);
        free(tbl);
      }
    }
    pthread_mutex_unlock(&rtp->lock);
  }
  pthread_mutex_unlock(&satip_rtp_lock);
}

void satip_rtp_close(void *id)
{
  satip_rtp_session_t *rtp;
  satip_rtp_table_t *tbl;
  streaming_queue_t *sq;

  pthread_mutex_lock(&satip_rtp_lock);
  rtp = satip_rtp_find(id);
  if (rtp) {
    TAILQ_REMOVE(&satip_rtp_sessions, rtp, link);
    sq = rtp->sq;
    pthread_mutex_lock(&sq->sq_mutex);
    rtp->sq = NULL;
    pthread_cond_signal(&sq->sq_cond);
    pthread_mutex_unlock(&sq->sq_mutex);
    pthread_mutex_unlock(&satip_rtp_lock);
    pthread_join(rtp->tid, NULL);
    udp_multisend_free(&rtp->um);
    mpegts_pid_done(&rtp->pids);
    while ((tbl = TAILQ_FIRST(&rtp->pmt_tables)) != NULL) {
      dvb_table_parse_done(&tbl->tbl);
      TAILQ_REMOVE(&rtp->pmt_tables, tbl, link);
      free(tbl);
    }
    pthread_mutex_destroy(&rtp->lock);
    free(rtp);
  } else {
    pthread_mutex_unlock(&satip_rtp_lock);
  }
}

/*
 *
 */
static const char *
satip_rtcp_pol(int pol)
{
  switch (pol) {
  case DVB_POLARISATION_HORIZONTAL:
    return "h";
  case DVB_POLARISATION_VERTICAL:
    return "v";
  case DVB_POLARISATION_CIRCULAR_LEFT:
    return "l";
  case DVB_POLARISATION_CIRCULAR_RIGHT:
    return "r";
  case DVB_POLARISATION_OFF:
    return "off";
  default:
    return "";
  }
}

/*
 *
 */
static const char *
satip_rtcp_fec(int fec)
{
  static char buf[16];
  char *p = buf;
  const char *s;

  if (fec == DVB_FEC_AUTO || fec == DVB_FEC_NONE)
    return "";
  s = dvb_fec2str(fec);
  if (s == NULL)
    return "";
  strncpy(buf, s, sizeof(buf));
  buf[sizeof(buf)-1] = '\0';
  p = strchr(buf, '/');
  while (p && *p) {
    *p = *(p+1);
    p++;
  }
  return buf;
}

/*
 *
 */
static int
satip_status_build(satip_rtp_session_t *rtp, char *buf, int len)
{
  char pids[1400];
  const char *delsys, *msys, *pilot, *rolloff;
  const char *bw, *tmode, *gi, *plp, *t2id, *sm, *c2tft, *ds, *specinv;
  int r, level = 0, lock = 0, quality = 0;

  lock = rtp->sig_lock;
  switch (rtp->sig.signal_scale) {
  case SIGNAL_STATUS_SCALE_RELATIVE:
    level = MINMAX((rtp->sig.signal * 245) / 0xffff, 0, 240);
    break;
  case SIGNAL_STATUS_SCALE_DECIBEL:
    level = MINMAX((rtp->sig.signal + 90000) / 375, 0, 240);
    break;
  default:
    level = lock ? 10 : 0;
    break;
  }
  switch (rtp->sig.snr_scale) {
  case SIGNAL_STATUS_SCALE_RELATIVE:
    quality = MINMAX((rtp->sig.snr * 16) / 0xffff, 0, 15);
    break;
  case SIGNAL_STATUS_SCALE_DECIBEL:
    quality = MINMAX(rtp->sig.snr / 2000, 0, 15);
    break;
  default:
    quality = lock ? 1 : 0;
    break;
  }

  mpegts_pid_dump(&rtp->pids, pids, sizeof(pids), 0, 0);

  switch (rtp->dmc.dmc_fe_delsys) {
  case DVB_SYS_DVBS:
  case DVB_SYS_DVBS2:
    delsys = rtp->dmc.dmc_fe_delsys == DVB_SYS_DVBS ? "dvbs" : "dvbs2";
    switch (rtp->dmc.dmc_fe_modulation) {
    case DVB_MOD_QPSK:  msys = "qpsk"; break;
    case DVB_MOD_PSK_8: msys = "8psk"; break;
    default:            msys = ""; break;
    }
    switch (rtp->dmc.dmc_fe_pilot) {
    case DVB_PILOT_ON:  pilot = "on"; break;
    case DVB_PILOT_OFF: pilot = "off"; break;
    default:            pilot = ""; break;
    }
    switch (rtp->dmc.dmc_fe_rolloff) {
    case DVB_ROLLOFF_20: rolloff = "20"; break;
    case DVB_ROLLOFF_25: rolloff = "25"; break;
    case DVB_ROLLOFF_35: rolloff = "35"; break;
    default:             rolloff = ""; break;
    }
    /* ver=<major>.<minor>;src=<srcID>;tuner=<feID>,<level>,<lock>,<quality>,<frequency>,<polarisation>,\
     * <system>,<type>,<pilots>,<roll_off>,<symbol_rate>,<fec_inner>;pids=<pid0>,...,<pidn>
     */
    r = snprintf(buf, len,
      "ver=1.0;src=%d;tuner=%d,%d,%d,%d,%.f,%s,%s,%s,%s,%s,%.f,%s;pids=%s",
      rtp->source, rtp->frontend, level, lock, quality,
      (float)rtp->dmc.dmc_fe_freq / 1000.0,
      satip_rtcp_pol(rtp->dmc.u.dmc_fe_qpsk.polarisation),
      delsys, msys, pilot, rolloff,
      (float)rtp->dmc.u.dmc_fe_qpsk.symbol_rate / 1000.0,
      satip_rtcp_fec(rtp->dmc.u.dmc_fe_qpsk.fec_inner),
      pids);
    break;
  case DVB_SYS_DVBT:
  case DVB_SYS_DVBT2:
    delsys = rtp->dmc.dmc_fe_delsys == DVB_SYS_DVBT ? "dvbt" : "dvbt2";
    switch (rtp->dmc.u.dmc_fe_ofdm.bandwidth) {
    case DVB_BANDWIDTH_1_712_MHZ:  bw = "1.712"; break;
    case DVB_BANDWIDTH_5_MHZ:      bw = "5"; break;
    case DVB_BANDWIDTH_6_MHZ:      bw = "6"; break;
    case DVB_BANDWIDTH_7_MHZ:      bw = "7"; break;
    case DVB_BANDWIDTH_8_MHZ:      bw = "8"; break;
    case DVB_BANDWIDTH_10_MHZ:     bw = "10"; break;
    default:                       bw = ""; break;
    }
    switch (rtp->dmc.u.dmc_fe_ofdm.transmission_mode) {
    case DVB_TRANSMISSION_MODE_1K:  tmode = "1k"; break;
    case DVB_TRANSMISSION_MODE_2K:  tmode = "2k"; break;
    case DVB_TRANSMISSION_MODE_4K:  tmode = "4k"; break;
    case DVB_TRANSMISSION_MODE_8K:  tmode = "8k"; break;
    case DVB_TRANSMISSION_MODE_16K: tmode = "16k"; break;
    case DVB_TRANSMISSION_MODE_32K: tmode = "32k"; break;
    default:                        tmode = ""; break;
    }
    switch (rtp->dmc.dmc_fe_modulation) {
    case DVB_MOD_QAM_16:  msys = "qam16"; break;
    case DVB_MOD_QAM_32:  msys = "qam32"; break;
    case DVB_MOD_QAM_64:  msys = "qam64"; break;
    case DVB_MOD_QAM_128: msys = "qam128"; break;
    default:              msys = ""; break;
    }
    switch (rtp->dmc.u.dmc_fe_ofdm.guard_interval) {
    case DVB_GUARD_INTERVAL_1_4:    gi = "14"; break;
    case DVB_GUARD_INTERVAL_1_8:    gi = "18"; break;
    case DVB_GUARD_INTERVAL_1_16:   gi = "116"; break;
    case DVB_GUARD_INTERVAL_1_32:   gi = "132"; break;
    case DVB_GUARD_INTERVAL_1_128:  gi = "1128"; break;
    case DVB_GUARD_INTERVAL_19_128: gi = "19128"; break;
    case DVB_GUARD_INTERVAL_19_256: gi = "19256"; break;
    default:                        gi = ""; break;
    }
    plp = "";
    t2id = "";
    sm = "";
    /* ver=1.1;tuner=<feID>,<level>,<lock>,<quality>,<freq>,<bw>,<msys>,<tmode>,<mtype>,<gi>,\
     * <fec>,<plp>,<t2id>,<sm>;pids=<pid0>,...,<pidn>
     */
    r = snprintf(buf, len,
      "ver=1.1;tuner=%d,%d,%d,%d,%.f,%s,%s,%s,%s,%s,%s,%s,%s,%s;pids=%s",
      rtp->frontend, level, lock, quality,
      (float)rtp->dmc.dmc_fe_freq / 1000000.0,
      bw, delsys, tmode, msys, gi,
      satip_rtcp_fec(rtp->dmc.u.dmc_fe_ofdm.code_rate_HP),
      plp, t2id, sm, pids);
    break;
  case DVB_SYS_DVBC_ANNEX_A:
  case DVB_SYS_DVBC_ANNEX_C:
    delsys = rtp->dmc.dmc_fe_delsys == DVB_SYS_DVBC_ANNEX_A ? "dvbc" : "dvbc2";
    bw = "";
    switch (rtp->dmc.dmc_fe_modulation) {
    case DVB_MOD_QAM_16:  msys = "qam16"; break;
    case DVB_MOD_QAM_32:  msys = "qam32"; break;
    case DVB_MOD_QAM_64:  msys = "qam64"; break;
    case DVB_MOD_QAM_128: msys = "qam128"; break;
    default:              msys = ""; break;
    }
    c2tft = "";
    ds = "";
    plp = "";
    specinv = "";
    /* ver=1.2;tuner=<feID>,<level>,<lock>,<quality>,<freq>,<bw>,<msys>,<mtype>,<sr>,<c2tft>,<ds>,<plp>,
     * <specinv>;pids=<pid0>,...,<pidn>
     */
    r = snprintf(buf, len,
      "ver=1.1;tuner=%d,%d,%d,%d,%.f,%s,%s,%s,%.f,%s,%s,%s,%s;pids=%s",
      rtp->frontend, level, lock, quality,
      (float)rtp->dmc.dmc_fe_freq / 1000000.0,
      bw, delsys, msys,
      (float)rtp->dmc.u.dmc_fe_qam.symbol_rate / 1000.0,
      c2tft, ds, plp, specinv, pids);
    break;
  default:
    return 0;
  }

  return r >= len ? len - 1 : r;
}

/*
 *
 */
int satip_rtp_status(void *id, char *buf, int len)
{
  satip_rtp_session_t *rtp;
  int r = 0;

  buf[0] = '\0';
  pthread_mutex_lock(&satip_rtp_lock);
  rtp = satip_rtp_find(id);
  if (rtp) {
    pthread_mutex_lock(&rtp->lock);
    r = satip_status_build(rtp, buf, len);
    pthread_mutex_unlock(&rtp->lock);
  }
  pthread_mutex_unlock(&satip_rtp_lock);
  return r;
}

/*
 *
 */
static int
satip_rtcp_build(satip_rtp_session_t *rtp, uint8_t *msg)
{
  char buf[1500];
  int len, len2;

  pthread_mutex_lock(&rtp->lock);
  len = satip_status_build(rtp, buf, sizeof(buf));
  pthread_mutex_unlock(&rtp->lock);

  len = len2 = MIN(len, RTCP_PAYLOAD - 16);
  if (len == 0)
    len++;
  while ((len % 4) != 0)
    buf[len++] = 0;
  memcpy(msg + 16, buf, len);

  len += 16;
  msg[0] = 0x80;
  msg[1] = 204;
  msg[2] = (((len - 1) / 4) >> 8) & 0xff;
  msg[3] = ((len - 1) / 4) & 0xff;
  msg[4] = 0;
  msg[5] = 0;
  msg[6] = 0;
  msg[7] = 0;
  msg[8] = 'S';
  msg[9] = 'E';
  msg[10] = 'S';
  msg[11] = '1';
  msg[12] = 0;
  msg[13] = 0;
  msg[14] = (len2 >> 8) & 0xff;
  msg[15] = len2 & 0xff;

  return len;
}

/*
 *
 */
static void *
satip_rtcp_thread(void *aux)
{
  satip_rtp_session_t *rtp;
  struct timespec ts;
  uint8_t msg[RTCP_PAYLOAD+1];
  char addrbuf[50];
  int r, len, err;

  tvhtrace("satips", "starting rtcp thread");
  while (satip_rtcp_run) {
    ts.tv_sec  = 0;
    ts.tv_nsec = 150000000;
    do {
      r = nanosleep(&ts, &ts);
      if (!satip_rtcp_run)
        goto end;
    } while (r && ts.tv_nsec);
    pthread_mutex_lock(&satip_rtp_lock);
    TAILQ_FOREACH(rtp, &satip_rtp_sessions, link) {
      if (rtp->sq == NULL) continue;
      len = satip_rtcp_build(rtp, msg);
      if (len <= 0) continue;
      if (tvhtrace_enabled()) {
        msg[len] = '\0';
        tcp_get_str_from_ip((struct sockaddr*)&rtp->peer2, addrbuf, sizeof(addrbuf));
        tvhtrace("satips", "RTCP send to %s:%d : %s", addrbuf, IP_PORT(rtp->peer2), msg + 16);
      }
      r = sendto(rtp->fd_rtcp, msg, len, 0,
                 (struct sockaddr*)&rtp->peer2,
                 rtp->peer2.ss_family == AF_INET6 ?
                   sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));
      if (r < 0) {
        err = errno;
        tcp_get_str_from_ip((struct sockaddr*)&rtp->peer2, addrbuf, sizeof(addrbuf));
        tvhwarn("satips", "RTCP send to error %s:%d : %s",
                addrbuf, IP_PORT(rtp->peer2), strerror(err));
      }
    }
    pthread_mutex_unlock(&satip_rtp_lock);
  }
end:
  return NULL;
}

/*
 *
 */
void satip_rtp_init(int boot)
{
  TAILQ_INIT(&satip_rtp_sessions);
  pthread_mutex_init(&satip_rtp_lock, NULL);

  if (boot)
    satip_rtcp_run = 0;

  if (!boot && !satip_rtcp_run) {
    satip_rtcp_run = 1;
    tvhthread_create(&satip_rtcp_tid, NULL, satip_rtcp_thread, NULL, "satip-rtcp");
  }
}

/*
 *
 */
void satip_rtp_done(void)
{
  assert(TAILQ_EMPTY(&satip_rtp_sessions));
  if (satip_rtcp_run) {
    satip_rtcp_run = 0;
    pthread_kill(satip_rtcp_tid, SIGTERM);
    pthread_join(satip_rtcp_tid, NULL);
  }
}
