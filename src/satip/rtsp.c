/*
 *  Tvheadend - SAT-IP server - RTSP part
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

#include "tvheadend.h"
#include "htsbuf.h"
#include "config.h"
#include "profile.h"
#include "satip/server.h"

#include <ctype.h>

#define RTSP_TIMEOUT 30
#define RTP_BUFSIZE  (256*1024)
#define RTCP_BUFSIZE (16*1024)

#define STATE_DESCRIBE 0
#define STATE_SETUP    1
#define STATE_PLAY     2

typedef struct slave_subscription {
  LIST_ENTRY(slave_subscription) link;
  mpegts_service_t *service;
  th_subscription_t *ths;
  profile_chain_t prch;
} slave_subscription_t;

typedef struct session {
  TAILQ_ENTRY(session) link;
  int delsys;
  int stream;
  int frontend;
  int findex;
  int src;
  int state;
  int shutdown_on_close;
  uint32_t nsession;
  char session[9];
  dvb_mux_conf_t dmc;
  mpegts_apids_t pids;
  gtimer_t timer;
  dvb_mux_t *mux;
  int mux_created;
  profile_chain_t prch;
  th_subscription_t *subs;
  int rtp_peer_port;
  udp_connection_t *udp_rtp;
  udp_connection_t *udp_rtcp;
  http_connection_t *old_hc;
  LIST_HEAD(, slave_subscription) slaves;
} session_t;

static uint32_t session_number;
static uint16_t stream_id;
static char *rtsp_ip = NULL;
static int rtsp_port = -1;
static int rtsp_descramble = 1;
static int rtsp_rewrite_pmt = 0;
static int rtsp_muxcnf = MUXCNF_AUTO;
static void *rtsp_server = NULL;
static TAILQ_HEAD(,session) rtsp_sessions;
static pthread_mutex_t rtsp_lock;

static void rtsp_close_session(session_t *rs);
static void rtsp_free_session(session_t *rs);


/*
 *
 */
static int
rtsp_delsys(int fe, int *findex)
{
  int res, i;

  if (fe < 1)
    return DVB_SYS_NONE;
  pthread_mutex_lock(&global_lock);
  i = satip_server_conf.satip_dvbs;
  if (fe <= i) {
    res = DVB_SYS_DVBS;
    goto result;
  }
  fe -= i;
  i = satip_server_conf.satip_dvbs2;
  if (fe <= i) {
    res = DVB_SYS_DVBS;
    goto result;
  }
  fe -= i;
  i = satip_server_conf.satip_dvbt;
  if (fe <= i) {
    res = DVB_SYS_DVBT;
    goto result;
  }
  fe -= i;
  i = satip_server_conf.satip_dvbt2;
  if (fe <= i) {
    res = DVB_SYS_DVBT;
    goto result;
  }
  fe -= i;
  i = satip_server_conf.satip_dvbc;
  if (fe <= i) {
    res = DVB_SYS_DVBC_ANNEX_A;
    goto result;
  }
  fe -= i;
  i = satip_server_conf.satip_dvbc2;
  if (fe <= i) {
    res = DVB_SYS_DVBC_ANNEX_A;
    goto result;
  }
  fe -= i;
  i = satip_server_conf.satip_atsc;
  if (fe <= i) {
    res = DVB_SYS_ATSC;
    goto result;
  }
  fe -= i;
  i = satip_server_conf.satip_dvbcb;
  if (fe <= i) {
    res = DVB_SYS_DVBC_ANNEX_B;
    goto result;
  }
  pthread_mutex_unlock(&global_lock);
  return DVB_SYS_NONE;
result:
  pthread_mutex_unlock(&global_lock);
  *findex = i;
  return res;
}

/*
 *
 */
static struct session *
rtsp_new_session(int delsys, uint32_t nsession, int session)
{
  struct session *rs = calloc(1, sizeof(*rs));

  if (rs == NULL)
    return NULL;

  rs->nsession = nsession ?: session_number;
  snprintf(rs->session, sizeof(rs->session), "%08X", session_number);
  if (!nsession) {
    session_number += 9876;
    if (session_number == 0)
      session_number += 9876;
  }
  mpegts_pid_init(&rs->pids);
  TAILQ_INSERT_TAIL(&rtsp_sessions, rs, link);
  return rs;
}

/*
 *
 */
static struct session *
rtsp_find_session(http_connection_t *hc, int stream)
{
  struct session *rs, *first = NULL;

  TAILQ_FOREACH(rs, &rtsp_sessions, link) {
    if (hc->hc_session && !strcmp(rs->session, hc->hc_session)) {
      if (stream == rs->stream)
        return rs;
      if (first == NULL)
        first = rs;
    }
    if (rs->old_hc == hc && stream == rs->stream)
      return rs;
  }
  return first;
}

/*
 *
 */
static void
rtsp_session_timer_cb(void *aux)
{
  session_t *rs = aux;

  tvhwarn("satips", "-/%s/%i: session closed (timeout)", rs->session, rs->stream);
  pthread_mutex_unlock(&global_lock);
  pthread_mutex_lock(&rtsp_lock);
  rtsp_close_session(rs);
  rtsp_free_session(rs);
  pthread_mutex_unlock(&rtsp_lock);
  pthread_mutex_lock(&global_lock);
}

static inline void
rtsp_rearm_session_timer(session_t *rs)
{
  if (!rs->shutdown_on_close) {
    pthread_mutex_lock(&global_lock);
    gtimer_arm(&rs->timer, rtsp_session_timer_cb, rs, RTSP_TIMEOUT);
    pthread_mutex_unlock(&global_lock);
  }
}

/*
 *
 */
static char *
rtsp_check_urlbase(char *u)
{
  char *p, *s;

  if (*u == '/' || strncmp(u, "stream=", 7) == 0)
    return u;
  /* expect string: rtsp://<myip>[:<myport>]/ */
  if (u[0] == '\0' || strncmp(u, "rtsp://", 7))
    return NULL;
  u += 7;
  p = strchr(u, '/');
  if (p)
    *p = '\0';
  if ((s = strchr(u, ':')) != NULL) {
    *s = '\0';
    if (atoi(s + 1) != rtsp_port)
      return NULL;
  } else {
#if 0 /* VLC is broken */
    if (rtsp_port != 554)
      return NULL;
#endif
  }
  if (strcmp(u, rtsp_ip))
    return NULL;
  return p ? p + 1 : u + strlen(u);
}

/*
 *
 */
static int
rtsp_parse_args(http_connection_t *hc, char *u)
{
  char *s;
  int stream = 0;

  if (strncmp(u, "stream=", 7) == 0) {
    u += 7;
    for (s = u; isdigit(*s); s++);
    if (*s == '\0')
      return atoi(u);
    if (*s != '/' || *(s+1) != '\0')
      if (*s != '?')
        return -1;
    *s = '\0';
    stream = atoi(u);
    u = s + 1;
  } else {
    if (*u != '?')
      return -1;
    u++;
  }
  http_parse_args(&hc->hc_req_args, u);
  return stream;
}

/*
 *
 */
static void
rtsp_slave_add
  (session_t *rs, mpegts_service_t *master, mpegts_service_t *slave)
{
  char buf[128];
  slave_subscription_t *sub = calloc(1, sizeof(*sub));

  pthread_mutex_lock(&master->s_stream_mutex);
  if (master->s_slaves_pids == NULL)
    master->s_slaves_pids = mpegts_pid_alloc();
  pthread_mutex_unlock(&master->s_stream_mutex);
  master->s_link(master, slave);
  sub->service = slave;
  profile_chain_init(&sub->prch, NULL, NULL);
  sub->prch.prch_st = &sub->prch.prch_sq.sq_st;
  sub->prch.prch_id = slave;
  snprintf(buf, sizeof(buf), "SAT>IP Slave/%s", slave->s_nicename);
  sub->ths = subscription_create_from_service(&sub->prch, NULL,
                                              SUBSCRIPTION_NONE,
                                              buf, 0, NULL, NULL,
                                              buf, NULL);
  if (sub->ths == NULL) {
    tvherror("satips", "%i/%s/%i: unable to subscribe service %s\n",
             rs->frontend, rs->session, rs->stream, slave->s_nicename);
    profile_chain_close(&sub->prch);
    free(sub);
    master->s_unlink(master, slave);
  } else {
    LIST_INSERT_HEAD(&rs->slaves, sub, link);
    tvhdebug("satips", "%i/%s/%i: slave service %s subscribed",
             rs->frontend, rs->session, rs->stream, slave->s_nicename);
  }
}

/*
 *
 */
static void
rtsp_slave_remove
  (session_t *rs, mpegts_service_t *master, mpegts_service_t *slave)
{
  slave_subscription_t *sub;

  if (master == NULL)
    return;
  LIST_FOREACH(sub, &rs->slaves, link)
    if (sub->service == slave)
      break;
  if (sub == NULL)
    return;
  tvhdebug("satips", "%i/%s/%i: slave service %s unsubscribed",
          rs->frontend, rs->session, rs->stream, slave->s_nicename);
  master->s_unlink(master, slave);
  if (sub->ths)
    subscription_unsubscribe(sub->ths, UNSUBSCRIBE_FINAL);
  if (sub->prch.prch_id)
    profile_chain_close(&sub->prch);
  LIST_REMOVE(sub, link);
  free(sub);
}

/*
 *
 */
static void
rtsp_clean(session_t *rs)
{
  slave_subscription_t *sub;

  if (rs->state == STATE_PLAY) {
    satip_rtp_close((void *)(intptr_t)rs->stream);
    rs->state = STATE_DESCRIBE;
  }
  if (rs->subs) {
    while ((sub = LIST_FIRST(&rs->slaves)) != NULL)
      rtsp_slave_remove(rs, (mpegts_service_t *)rs->subs->ths_raw_service,
                        sub->service);
    subscription_unsubscribe(rs->subs, UNSUBSCRIBE_FINAL);
    rs->subs = NULL;
  }
  if (rs->prch.prch_id)
    profile_chain_close(&rs->prch);
  if (rs->mux && rs->mux_created &&
      (rtsp_muxcnf != MUXCNF_KEEP || LIST_EMPTY(&rs->mux->mm_services)))
    rs->mux->mm_delete((mpegts_mux_t *)rs->mux, 1);
  rs->mux = NULL;
  rs->mux_created = 0;
}

/*
 *
 */
static int
rtsp_validate_service(mpegts_service_t *s, mpegts_apids_t *pids)
{
  int av = 0, enc = 0;
  elementary_stream_t *st;

  pthread_mutex_lock(&s->s_stream_mutex);
  if (s->s_pmt_pid <= 0 || s->s_pcr_pid <= 0) {
    pthread_mutex_unlock(&s->s_stream_mutex);
    return 0;
  }
  TAILQ_FOREACH(st, &s->s_components, es_link) {
    if (st->es_type == SCT_CA)
      enc = 1;
    if (st->es_pid > 0)
      if (pids == NULL || mpegts_pid_wexists(pids, st->es_pid, MPS_WEIGHT_RAW))
        if ((SCT_ISVIDEO(st->es_type) || SCT_ISAUDIO(st->es_type)))
          av = 1;
  }
  pthread_mutex_unlock(&s->s_stream_mutex);
  return enc && av;
}

/*
 *
 */
static void
rtsp_manage_descramble(session_t *rs)
{
  idnode_set_t *found;
  mpegts_service_t *s, *snext;
  mpegts_service_t *master = (mpegts_service_t *)rs->subs->ths_raw_service;
  slave_subscription_t *sub;
  mpegts_apids_t pmt_pids;
  size_t si;
  int i, used = 0;

  if (rtsp_descramble <= 0)
    return;

  found = idnode_set_create(1);

  if (rs->mux == NULL || rs->subs == NULL)
    goto end;

  if (rs->pids.all) {
    LIST_FOREACH(s, &rs->mux->mm_services, s_dvb_mux_link)
      if (rtsp_validate_service(s, NULL))
        idnode_set_add(found, &s->s_id, NULL, NULL);
  } else {
    for (i = 0; i < rs->pids.count; i++) {
      s = mpegts_service_find_by_pid((mpegts_mux_t *)rs->mux, rs->pids.pids[i].pid);
      if (s != NULL && rtsp_validate_service(s, &rs->pids))
        if (!idnode_set_exists(found, &s->s_id))
          idnode_set_add(found, &s->s_id, NULL, NULL);
    }
  }

  /* Remove already used or no-longer required services */
  for (s = LIST_FIRST(&master->s_slaves); s; s = snext) {
    snext = LIST_NEXT(s, s_slaves_link);
    if (idnode_set_remove(found, &s->s_id))
      used++;
    else if (!idnode_set_exists(found, &s->s_id))
      rtsp_slave_remove(rs, master, s);
  }

  /* Add new ones */
  for (si = 0; used < rtsp_descramble && si < found->is_count; si++, used++) {
    s = (mpegts_service_t *)found->is_array[si];
    rtsp_slave_add(rs, master, s);
    idnode_set_remove(found, &s->s_id);
  }
  if (si < found->is_count)
    tvhwarn("satips", "%i/%s/%i: limit for descrambled services reached (wanted %zd allowed %d)",
            rs->frontend, rs->session, rs->stream,
            (used - si) + found->is_count, rtsp_descramble);
  
end:
  idnode_set_free(found);

  if (rtsp_rewrite_pmt) {
    /* handle PMT rewrite */
    mpegts_pid_init(&pmt_pids);
    LIST_FOREACH(sub, &rs->slaves, link) {
      if ((s = sub->service) == NULL) continue;
      if (s->s_pmt_pid <= 0 || s->s_pmt_pid >= 8191) continue;
      mpegts_pid_add(&pmt_pids, s->s_pmt_pid, MPS_WEIGHT_PMT);
    }
    satip_rtp_update_pmt_pids((void *)(intptr_t)rs->stream, &pmt_pids);
    mpegts_pid_done(&pmt_pids);
  }
}

/*
 *
 */
static int
rtsp_start
  (http_connection_t *hc, session_t *rs, char *addrbuf,
   int newmux, int setup, int oldstate)
{
  mpegts_network_t *mn, *mn2;
  dvb_network_t *ln;
  dvb_mux_t *mux;
  mpegts_service_t *svc;
  char buf[384];
  int res = HTTP_STATUS_SERVICE, qsize = 3000000, created = 0;

  pthread_mutex_lock(&global_lock);
  if (newmux) {
    mux = NULL;
    mn2 = NULL;
    LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
      if (!idnode_is_instance(&mn->mn_id, &dvb_network_class))
        continue;
      ln = (dvb_network_t *)mn;
      if (ln->ln_type == rs->dmc.dmc_fe_type &&
          mn->mn_satip_source == rs->src) {
        if (!mn2) mn2 = mn;
        mux = dvb_network_find_mux((dvb_network_t *)mn, &rs->dmc,
                                   MPEGTS_ONID_NONE, MPEGTS_TSID_NONE);
        if (mux) break;
      }
    }
    if (mux == NULL && mn2 &&
        (rtsp_muxcnf == MUXCNF_AUTO || rtsp_muxcnf == MUXCNF_KEEP)) {
      dvb_mux_conf_str(&rs->dmc, buf, sizeof(buf));
      tvhwarn("satips", "%i/%s/%i: create mux %s",
              rs->frontend, rs->session, rs->stream, buf);
      mux = (dvb_mux_t *)
        mn2->mn_create_mux(mn2, (void *)(intptr_t)rs->nsession,
                           MPEGTS_ONID_NONE, MPEGTS_TSID_NONE,
                           &rs->dmc, 1);
      if (mux)
        created = 1;
    }
    if (mux == NULL) {
      dvb_mux_conf_str(&rs->dmc, buf, sizeof(buf));
      tvhwarn("satips", "%i/%s/%i: unable to create mux %s%s",
              rs->frontend, rs->session, rs->stream, buf,
              rtsp_muxcnf == MUXCNF_REJECT ? " (configuration)" : "");
      goto endclean;
    }
    if (rs->mux == mux && rs->subs)
      goto pids;
    rtsp_clean(rs);
    rs->mux = mux;
    rs->mux_created = created;
    if (profile_chain_raw_open(&rs->prch, (mpegts_mux_t *)rs->mux, qsize, 0))
      goto endclean;
    rs->subs = subscription_create_from_mux(&rs->prch, NULL,
                                   satip_server_conf.satip_weight,
                                   "SAT>IP",
                                   rs->prch.prch_flags |
                                   SUBSCRIPTION_STREAMING,
                                   addrbuf, hc->hc_username,
                                   http_arg_get(&hc->hc_args, "User-Agent"),
                                   NULL);
    if (!rs->subs)
      goto endclean;
    if (!rs->pids.all && rs->pids.count == 0)
      mpegts_pid_add(&rs->pids, 0, MPS_WEIGHT_RAW);
    /* retrigger play when new setup arrived */
    if (oldstate) {
      setup = 0;
      rs->state = STATE_SETUP;
    }
  } else {
pids:
    if (!rs->subs)
      goto endclean;
    if (!rs->pids.all && rs->pids.count == 0)
      mpegts_pid_add(&rs->pids, 0, MPS_WEIGHT_RAW);
    svc = (mpegts_service_t *)rs->subs->ths_raw_service;
    svc->s_update_pids(svc, &rs->pids);
    satip_rtp_update_pids((void *)(intptr_t)rs->stream, &rs->pids);
  }
  if (!setup && rs->state != STATE_PLAY) {
    if (rs->mux == NULL)
      goto endclean;
    satip_rtp_queue((void *)(intptr_t)rs->stream,
                    rs->subs, &rs->prch.prch_sq,
                    hc->hc_peer, rs->rtp_peer_port,
                    rs->udp_rtp->fd, rs->udp_rtcp->fd,
                    rs->frontend, rs->findex, &rs->mux->lm_tuning,
                    &rs->pids);
    if (!rs->pids.all && rs->pids.count == 0)
      mpegts_pid_add(&rs->pids, 0, MPS_WEIGHT_RAW);
    svc = (mpegts_service_t *)rs->subs->ths_raw_service;
    svc->s_update_pids(svc, &rs->pids);
    rs->state = STATE_PLAY;
  }
  rtsp_manage_descramble(rs);
  pthread_mutex_unlock(&global_lock);
  return 0;

endclean:
  rtsp_clean(rs);
  pthread_mutex_unlock(&global_lock);
  return res;
}

/*
 *
 */
static inline int
msys_to_tvh(http_connection_t *hc)
{
  static struct strtab tab[] = {
    { "dvbs",  DVB_SYS_DVBS },
    { "dvbs2", DVB_SYS_DVBS2 },
    { "dvbt",  DVB_SYS_DVBT },
    { "dvbt2", DVB_SYS_DVBT2 },
    { "dvbc",  DVB_SYS_DVBC_ANNEX_A },
    { "dvbc2", DVB_SYS_DVBC_ANNEX_C },
    { "atsc",  DVB_SYS_ATSC },
    { "dvbcb", DVB_SYS_DVBC_ANNEX_B }
  };
  const char *s = http_arg_get_remove(&hc->hc_req_args, "msys");
  return s[0] ? str2val(s, tab) : DVB_SYS_NONE;
}

static inline int
pol_to_tvh(http_connection_t *hc)
{
  static struct strtab tab[] = {
    { "h",  DVB_POLARISATION_HORIZONTAL },
    { "v",  DVB_POLARISATION_VERTICAL },
    { "l",  DVB_POLARISATION_CIRCULAR_LEFT },
    { "r",  DVB_POLARISATION_CIRCULAR_RIGHT },
  };
  const char *s = http_arg_get_remove(&hc->hc_req_args, "pol");
  return s[0] ? str2val(s, tab) : -1;
}

static int
fec_to_tvh(http_connection_t *hc)
{
  switch (atoi(http_arg_get_remove(&hc->hc_req_args, "fec"))) {
  case   0: return DVB_FEC_AUTO;
  case  12: return DVB_FEC_1_2;
  case  13: return DVB_FEC_1_3;
  case  15: return DVB_FEC_1_5;
  case  23: return DVB_FEC_2_3;
  case  25: return DVB_FEC_2_5;
  case  29: return DVB_FEC_2_9;
  case  34: return DVB_FEC_3_4;
  case  35: return DVB_FEC_3_5;
  case  45: return DVB_FEC_4_5;
  case 415: return DVB_FEC_4_15;
  case  56: return DVB_FEC_5_6;
  case  59: return DVB_FEC_5_9;
  case  67: return DVB_FEC_6_7;
  case  78: return DVB_FEC_7_8;
  case  79: return DVB_FEC_7_9;
  case 715: return DVB_FEC_7_15;
  case  89: return DVB_FEC_8_9;
  case 815: return DVB_FEC_8_15;
  case 910: return DVB_FEC_9_10;
  case 920: return DVB_FEC_9_20;
  default:  return DVB_FEC_NONE;
  }
}

static int
bw_to_tvh(http_connection_t *hc)
{
  int bw = atof(http_arg_get_remove(&hc->hc_req_args, "bw")) * 1000;
  switch (bw) {
  case 0: return DVB_BANDWIDTH_AUTO;
  case DVB_BANDWIDTH_1_712_MHZ:
  case DVB_BANDWIDTH_5_MHZ:
  case DVB_BANDWIDTH_6_MHZ:
  case DVB_BANDWIDTH_7_MHZ:
  case DVB_BANDWIDTH_8_MHZ:
  case DVB_BANDWIDTH_10_MHZ:
    return bw;
  default:
    return DVB_BANDWIDTH_NONE;
  }
}

static int
rolloff_to_tvh(http_connection_t *hc)
{
  int ro = atof(http_arg_get_remove(&hc->hc_req_args, "ro")) * 1000;
  switch (ro) {
  case 0:
    return DVB_ROLLOFF_35;
  case DVB_ROLLOFF_20:
  case DVB_ROLLOFF_25:
  case DVB_ROLLOFF_35:
    return ro;
  default:
    return DVB_ROLLOFF_NONE;
  }
}

static int
pilot_to_tvh(http_connection_t *hc)
{
  const char *s = http_arg_get_remove(&hc->hc_req_args, "plts");
  if (strcmp(s, "on") == 0)
    return DVB_PILOT_ON;
  if (strcmp(s, "off") == 0)
    return DVB_PILOT_OFF;
  if (s[0] == '\0' || strcmp(s, "auto") == 0)
    return DVB_PILOT_AUTO;
  return DVB_ROLLOFF_NONE;
}

static int
tmode_to_tvh(http_connection_t *hc)
{
  static struct strtab tab[] = {
    { "auto", DVB_TRANSMISSION_MODE_AUTO },
    { "1k",   DVB_TRANSMISSION_MODE_1K },
    { "2k",   DVB_TRANSMISSION_MODE_2K },
    { "4k",   DVB_TRANSMISSION_MODE_4K },
    { "8k",   DVB_TRANSMISSION_MODE_8K },
    { "16k",  DVB_TRANSMISSION_MODE_16K },
    { "32k",  DVB_TRANSMISSION_MODE_32K },
  };
  const char *s = http_arg_get_remove(&hc->hc_req_args, "tmode");
  if (s[0]) {
    int v = str2val(s, tab);
    return v >= 0 ? v : DVB_TRANSMISSION_MODE_NONE;
  }
  return DVB_TRANSMISSION_MODE_AUTO;
}

static int
mtype_to_tvh(http_connection_t *hc)
{
  static struct strtab tab[] = {
    { "auto",   DVB_MOD_AUTO },
    { "qpsk",   DVB_MOD_QPSK },
    { "8psk",   DVB_MOD_PSK_8 },
    { "16qam",  DVB_MOD_QAM_16 },
    { "32qam",  DVB_MOD_QAM_32 },
    { "64qam",  DVB_MOD_QAM_64 },
    { "128qam", DVB_MOD_QAM_128 },
    { "256qam", DVB_MOD_QAM_256 },
    { "8vsb",   DVB_MOD_VSB_8 },
  };
  const char *s = http_arg_get_remove(&hc->hc_req_args, "mtype");
  if (s[0]) {
    int v = str2val(s, tab);
    return v >= 0 ? v : DVB_MOD_NONE;
  }
  return DVB_MOD_AUTO;
}

static int
gi_to_tvh(http_connection_t *hc)
{
  switch (atoi(http_arg_get_remove(&hc->hc_req_args, "gi"))) {
  case 0:     return DVB_GUARD_INTERVAL_AUTO;
  case 14:    return DVB_GUARD_INTERVAL_1_4;
  case 18:    return DVB_GUARD_INTERVAL_1_8;
  case 116:   return DVB_GUARD_INTERVAL_1_16;
  case 132:   return DVB_GUARD_INTERVAL_1_32;
  case 1128:  return DVB_GUARD_INTERVAL_1_128;
  case 19128: return DVB_GUARD_INTERVAL_19_128;
  case 19256: return DVB_GUARD_INTERVAL_19_256;
  default:    return DVB_GUARD_INTERVAL_NONE;
  }
}

static int
parse_pids(char *p, mpegts_apids_t *pids)
{
  char *x, *saveptr;
  int i = 0, pid;

  mpegts_pid_reset(pids);
  if (p == NULL || *p == '\0')
    return 0;
  x = strtok_r(p, ",", &saveptr);
  while (1) {
    if (x == NULL)
      break;
    if (strcmp(x, "all") == 0) {
      pids->all = 1;
    } else {
      pids->all = 0;
      pid = atoi(x);
      if (pid < 0 || pid > 8191)
        return -1;
      mpegts_pid_add(pids, pid, MPS_WEIGHT_RAW);
    }
    x = strtok_r(NULL, ",", &saveptr);
    i++;
  }
  if (i == 0)
    return -1;
  return 0;
}

static int
parse_transport(http_connection_t *hc)
{
  const char *s = http_arg_get(&hc->hc_args, "Transport");
  const char *u;
  int a, b;
  if (!s || strncmp(s, "RTP/AVP;unicast;client_port=", 28))
    return -1;
  for (s += 28, u = s; isdigit(*u); u++);
  if (*u != '-')
    return -1;
  a = atoi(s);
  for (s = ++u; isdigit(*s); s++);
  if (*s != '\0' && *s != ';')
    return -1;
  b = atoi(u);
  if (a + 1 != b)
    return -1;
  return a;
}

/*
 *
 */
static int
rtsp_parse_cmd
  (http_connection_t *hc, int stream, int cmd,
   session_t **rrs, int *valid, int *oldstate)
{
  session_t *rs = NULL;
  int errcode = HTTP_STATUS_BAD_REQUEST, r, findex = 0, has_args;
  int delsys = DVB_SYS_NONE, msys, fe, src, freq, pol, sr;
  int fec, ro, plts, bw, tmode, mtype, gi, plp, t2id, sm, c2tft, ds, specinv;
  char *s;
  const char *caller;
  mpegts_apids_t pids, addpids, delpids;
  dvb_mux_conf_t *dmc;
  char buf[256];
  http_arg_t *arg;

  switch (cmd) {
  case -1: caller = "DESCRIBE"; break;
  case  0: caller = "PLAY"; break;
  case  1: caller = "SETUP"; break;
  default: caller = NULL; break;
  }

  *rrs = NULL;
  *oldstate = 0;
  mpegts_pid_init(&pids);
  mpegts_pid_init(&addpids);
  mpegts_pid_init(&delpids);

  has_args = !TAILQ_EMPTY(&hc->hc_req_args);

  fe = atoi(http_arg_get_remove(&hc->hc_req_args, "fe"));
  s = http_arg_get_remove(&hc->hc_req_args, "addpids");
  if (parse_pids(s, &addpids)) goto end;
  s = http_arg_get_remove(&hc->hc_req_args, "delpids");
  if (parse_pids(s, &delpids)) goto end;
  s = http_arg_get_remove(&hc->hc_req_args, "pids");
  if (parse_pids(s, &pids)) goto end;
  msys = msys_to_tvh(hc);
  freq = atof(http_arg_get_remove(&hc->hc_req_args, "freq")) * 1000;
  *valid = freq >= 10000;

  if (addpids.count > 0 || delpids.count > 0) {
    if (cmd)
      goto end;
    if (!stream)
      goto end;
  }

  rs = rtsp_find_session(hc, stream);

  if (fe > 0) {
    delsys = rtsp_delsys(fe, &findex);
    if (delsys == DVB_SYS_NONE)
      goto end;
  } else {
    delsys = msys;
  }

  if (cmd) {
    if (!rs) {
      rs = rtsp_new_session(msys, 0, -1);
      if (delsys == DVB_SYS_NONE) goto end;
      if (msys == DVB_SYS_NONE) goto end;
      if (!(*valid)) goto end;
    } else if (stream != rs->stream) {
      rs = rtsp_new_session(msys, rs->nsession, stream);
      if (delsys == DVB_SYS_NONE) goto end;
      if (msys == DVB_SYS_NONE) goto end;
      if (!(*valid)) goto end;
    } else {
      if (cmd < 0 && rs->state != STATE_DESCRIBE) {
        errcode = HTTP_STATUS_CONFLICT;
        goto end;
      }
      if (!has_args && rs->state == STATE_DESCRIBE && cmd > 0) {
        r = parse_transport(hc);
        if (r < 0) {
          errcode = HTTP_STATUS_BAD_TRANSFER;
          goto end;
        }
        rs->rtp_peer_port = r;
        *valid = 1;
        goto ok;
      }
      *oldstate = rs->state;
      rtsp_close_session(rs);
    }
    if (cmd > 0) {
      r = parse_transport(hc);
      if (r < 0) {
        errcode = HTTP_STATUS_BAD_TRANSFER;
        goto end;
      }
      if (rs->state == STATE_PLAY && rs->rtp_peer_port != r) {
        errcode = HTTP_STATUS_METHOD_INVALID;
        goto end;
      }
      rs->rtp_peer_port = r;
    }
    rs->frontend = fe > 0 ? fe : 1;
    dmc = &rs->dmc;
  } else {
    if (!rs || stream != rs->stream) {
      if (rs)
        errcode = HTTP_STATUS_NOT_FOUND;
      goto end;
    }
    *oldstate = rs->state;
    dmc = &rs->dmc;
    if (rs->mux == NULL) goto end;
    if (!fe) {
      fe = rs->frontend;
      findex = rs->findex;
    }
    if (rs->frontend != fe)
      goto end;
    if (*valid) {
      if (delsys == DVB_SYS_NONE) goto end;
      if (msys == DVB_SYS_NONE) goto end;
    } else {
      if (!TAILQ_EMPTY(&hc->hc_req_args)) goto end;
      goto play;
    }
  }

  dvb_mux_conf_init(dmc, msys);

  mtype = mtype_to_tvh(hc);
  if (mtype == DVB_MOD_NONE) goto end;

  src = 1;

  if (msys == DVB_SYS_DVBS || msys == DVB_SYS_DVBS2) {

    src = atoi(http_arg_get_remove(&hc->hc_req_args, "src"));
    if (src < 1) goto end;
    pol = pol_to_tvh(hc);
    if (pol < 0) goto end;
    sr = atof(http_arg_get_remove(&hc->hc_req_args, "sr")) * 1000;
    if (sr < 1000) goto end;
    fec = fec_to_tvh(hc);
    if (fec == DVB_FEC_NONE) goto end;
    ro = rolloff_to_tvh(hc);
    if (ro == DVB_ROLLOFF_NONE ||
        (ro != DVB_ROLLOFF_35 && msys == DVB_SYS_DVBS)) goto end;
    plts = pilot_to_tvh(hc);
    if (plts == DVB_PILOT_NONE) goto end;

    if (!TAILQ_EMPTY(&hc->hc_req_args)) goto eargs;

    dmc->dmc_fe_rolloff = ro;
    dmc->dmc_fe_pilot = plts;
    dmc->u.dmc_fe_qpsk.polarisation = pol;
    dmc->u.dmc_fe_qpsk.symbol_rate = sr;
    dmc->u.dmc_fe_qpsk.fec_inner = fec;

  } else if (msys == DVB_SYS_DVBT || msys == DVB_SYS_DVBT2) {

    freq *= 1000;
    if (freq < 0) goto end;
    bw = bw_to_tvh(hc);
    if (bw == DVB_BANDWIDTH_NONE) goto end;
    tmode = tmode_to_tvh(hc);
    if (tmode == DVB_TRANSMISSION_MODE_NONE) goto end;
    gi = gi_to_tvh(hc);
    if (gi == DVB_GUARD_INTERVAL_NONE) goto end;
    fec = fec_to_tvh(hc);
    if (fec == DVB_FEC_NONE) goto end;
    s = http_arg_get_remove(&hc->hc_req_args, "plp");
    if (s[0]) {
      plp = atoi(s);
      if (plp < 0 || plp > 255) goto end;
    } else {
      plp = DVB_NO_STREAM_ID_FILTER;
    }
    t2id = atoi(http_arg_get_remove(&hc->hc_req_args, "t2id"));
    if (t2id < 0 || t2id > 65535) goto end;
    sm = atoi(http_arg_get_remove(&hc->hc_req_args, "sm"));
    if (sm < 0 || sm > 1) goto end;

    if (!TAILQ_EMPTY(&hc->hc_req_args)) goto eargs;

    dmc->u.dmc_fe_ofdm.bandwidth = bw;
    dmc->u.dmc_fe_ofdm.code_rate_HP = fec;
    dmc->u.dmc_fe_ofdm.code_rate_LP = DVB_FEC_NONE;
    dmc->u.dmc_fe_ofdm.transmission_mode = tmode;
    dmc->u.dmc_fe_ofdm.guard_interval = gi;
    dmc->u.dmc_fe_ofdm.hierarchy_information = DVB_HIERARCHY_AUTO;
    dmc->dmc_fe_stream_id = plp;
    dmc->dmc_fe_pls_code = t2id;
    dmc->dmc_fe_pls_mode = sm; /* check */

  } else if (msys == DVB_SYS_DVBC_ANNEX_A || msys == DVB_SYS_DVBC_ANNEX_C) {

    freq *= 1000;
    if (freq < 0) goto end;
    c2tft = atoi(http_arg_get_remove(&hc->hc_req_args, "c2tft"));
    if (c2tft < 0 || c2tft > 2) goto end;
    bw = bw_to_tvh(hc);
    if (bw == DVB_BANDWIDTH_NONE) goto end;
    sr = atof(http_arg_get_remove(&hc->hc_req_args, "sr")) * 1000;
    if (sr < 1000) goto end;
    ds = atoi(http_arg_get_remove(&hc->hc_req_args, "ds"));
    if (ds < 0 || ds > 255) goto end;
    s = http_arg_get_remove(&hc->hc_req_args, "plp");
    if (s[0]) {
      plp = atoi(http_arg_get_remove(&hc->hc_req_args, "plp"));
      if (plp < 0 || plp > 255) goto end;
    } else {
      plp = DVB_NO_STREAM_ID_FILTER;
    }
    specinv = atoi(http_arg_get_remove(&hc->hc_req_args, "specinv"));
    if (specinv < 0 || specinv > 1) goto end;
    fec = fec_to_tvh(hc);
    if (fec == DVB_FEC_NONE) goto end;

    if (!TAILQ_EMPTY(&hc->hc_req_args)) goto eargs;

    dmc->u.dmc_fe_qam.symbol_rate = sr;
    dmc->u.dmc_fe_qam.fec_inner = fec;
    dmc->dmc_fe_inversion = specinv;
    dmc->dmc_fe_stream_id = plp;
    dmc->dmc_fe_pls_code = ds; /* check */

  } else if (msys == DVB_SYS_ATSC || msys == DVB_SYS_DVBC_ANNEX_B) {

    if (!TAILQ_EMPTY(&hc->hc_req_args)) goto eargs;

  } else {

    goto end;

  }

  dmc->dmc_fe_freq = freq;
  dmc->dmc_fe_modulation = mtype;
  rs->delsys = delsys;
  rs->frontend = fe;
  rs->findex = findex;
  rs->old_hc = hc;

  if (cmd) {
    stream_id++;
    if (stream_id == 0)
      stream_id++;
    rs->stream = stream_id % 0x7fff;
  }
  rs->src = src;

  if (cmd < 0)
    rs->shutdown_on_close = 1;

play:
  if (pids.count > 0)
    mpegts_pid_copy(&rs->pids, &pids);
  if (delpids.count > 0)
    mpegts_pid_del_group(&rs->pids, &delpids);
  if (addpids.count > 0)
    mpegts_pid_add_group(&rs->pids, &addpids);

  dvb_mux_conf_str(dmc, buf, sizeof(buf));
  r = strlen(buf);
  tvh_strlcatf(buf, sizeof(buf), r, " pids ");
  if (mpegts_pid_dump(&rs->pids, buf + r, sizeof(buf) - r, 0, 0) == 0)
    tvh_strlcatf(buf, sizeof(buf), r, "<none>");

  tvhdebug("satips", "%i/%s/%d: %s from %s:%d %s",
           rs->frontend, rs->session, rs->stream,
           caller, hc->hc_peer_ipstr, IP_PORT(*hc->hc_peer), buf);

ok:
  errcode = 0;
  *rrs = rs;

end:
  if (rs)
    rtsp_rearm_session_timer(rs);
  mpegts_pid_done(&addpids);
  mpegts_pid_done(&delpids);
  mpegts_pid_done(&pids);
  return errcode;

eargs:
  TAILQ_FOREACH(arg, &hc->hc_req_args, link)
    tvhwarn("satips", "%i/%s/%i: extra parameter '%s'='%s'",
      rs->frontend, rs->session, rs->stream,
      arg->key, arg->val);
  goto end;
}

/*
 *
 */
static int
rtsp_process_options(http_connection_t *hc)
{
  http_arg_list_t args;
  char *u = tvh_strdupa(hc->hc_url);
  session_t *rs;
  int found;

  if (strcmp(u, "*") != 0 && (u = rtsp_check_urlbase(u)) == NULL)
    goto error;

  if (hc->hc_session) {
    found = 0;
    pthread_mutex_lock(&rtsp_lock);
    TAILQ_FOREACH(rs, &rtsp_sessions, link)
      if (!strcmp(rs->session, hc->hc_session)) {
        rtsp_rearm_session_timer(rs);
        found = 1;
      }
    pthread_mutex_unlock(&rtsp_lock);
    if (!found) {
      http_error(hc, HTTP_STATUS_BAD_SESSION);
      return 0;
    }
  }
  http_arg_init(&args);
  http_arg_set(&args, "Public", "OPTIONS,DESCRIBE,SETUP,PLAY,TEARDOWN");
  if (hc->hc_session)
    http_arg_set(&args, "Session", hc->hc_session);
  http_send_header(hc, HTTP_STATUS_OK, NULL, 0, NULL, NULL, 0, NULL, NULL, &args);
  http_arg_flush(&args);
  return 0;

error:
  http_error(hc, HTTP_STATUS_BAD_REQUEST);
  return 0;
}

/*
 *
 */
static void
rtsp_describe_header(session_t *rs, htsbuf_queue_t *q)
{
  unsigned long mono = getmonoclock();
  int dvbt, dvbc;

  htsbuf_qprintf(q, "v=0\r\n");
  htsbuf_qprintf(q, "o=- %lu %lu IN %s %s\r\n",
                 rs ? (unsigned long)rs->nsession : mono - 123,
                 mono,
                 strchr(rtsp_ip, ':') ? "IP6" : "IP4",
                 rtsp_ip);

  pthread_mutex_lock(&global_lock);
  htsbuf_qprintf(q, "s=SatIPServer:1 %d",
                 satip_server_conf.satip_dvbs +
                 satip_server_conf.satip_dvbs2);
  dvbt = satip_server_conf.satip_dvbt +
         satip_server_conf.satip_dvbt2;
  dvbc = satip_server_conf.satip_dvbc +
         satip_server_conf.satip_dvbc2;
  if (dvbc)
    htsbuf_qprintf(q, " %d %d\r\n", dvbt, dvbc);
  else if (dvbt)
    htsbuf_qprintf(q, " %d\r\n", dvbt);
  else
    htsbuf_append(q, "\r\n", 1);
  pthread_mutex_unlock(&global_lock);

  htsbuf_qprintf(q, "t=0 0\r\n");
}

static void
rtsp_describe_session(session_t *rs, htsbuf_queue_t *q)
{
  char buf[4096];

  htsbuf_qprintf(q, "a=control:stream=%d\r\n", rs->stream);
  htsbuf_qprintf(q, "a=tool:tvheadend\r\n");
  htsbuf_qprintf(q, "m=video 0 RTP/AVP 33\r\n");
  if (strchr(rtsp_ip, ':'))
    htsbuf_qprintf(q, "c=IN IP6 ::0\r\n");
  else
    htsbuf_qprintf(q, "c=IN IP4 0.0.0.0\r\n");
  if (rs->state == STATE_PLAY) {
    satip_rtp_status((void *)(intptr_t)rs->stream, buf, sizeof(buf));
    htsbuf_qprintf(q, "a=fmtp:33 %s\r\n", buf);
    htsbuf_qprintf(q, "a=sendonly\r\n");
  } else {
    htsbuf_qprintf(q, "a=fmtp:33\r\n");
    htsbuf_qprintf(q, "a=inactive\r\n");
  }
}

/*
 *
 */
static int
rtsp_process_describe(http_connection_t *hc)
{
  http_arg_list_t args;
  const char *arg;
  char *u = tvh_strdupa(hc->hc_url);
  session_t *rs;
  htsbuf_queue_t q;
  char buf[96];
  int r = HTTP_STATUS_BAD_REQUEST;
  int stream, first = 1, valid, oldstate;

  htsbuf_queue_init(&q, 0);

  arg = http_arg_get(&hc->hc_args, "Accept");
  if (arg == NULL || strcmp(arg, "application/sdp"))
    goto error;

  if ((u = rtsp_check_urlbase(u)) == NULL)
    goto error;

  stream = rtsp_parse_args(hc, u);

  pthread_mutex_lock(&rtsp_lock);

  if (TAILQ_FIRST(&hc->hc_req_args)) {
    if (stream < 0) {
      pthread_mutex_unlock(&rtsp_lock);
      goto error;
    }
    r = rtsp_parse_cmd(hc, stream, -1, &rs, &valid, &oldstate);
    if (r) {
      pthread_mutex_unlock(&rtsp_lock);
      goto error;
    }
  }

  TAILQ_FOREACH(rs, &rtsp_sessions, link) {
    if (hc->hc_session) {
      if (strcmp(hc->hc_session, rs->session))
        continue;
      rtsp_rearm_session_timer(rs);
      if (stream > 0 && rs->stream != stream)
        continue;
    }
    if (first) {
      rtsp_describe_header(hc->hc_session ? rs : NULL, &q);
      first = 0;
    }
    rtsp_describe_session(rs, &q);
  }
  pthread_mutex_unlock(&rtsp_lock);

  if (first) {
    if (hc->hc_session) {
      http_error(hc, HTTP_STATUS_BAD_SESSION);
      return 0;
    }
    rtsp_describe_header(NULL, &q);
  }

  http_arg_init(&args);
  if (hc->hc_session)
    http_arg_set(&args, "Session", hc->hc_session);
  if (stream > 0)
    snprintf(buf, sizeof(buf), "rtsp://%s/stream=%i", rtsp_ip, stream);
  else
    snprintf(buf, sizeof(buf), "rtsp://%s", rtsp_ip);
  http_arg_set(&args, "Content-Base", buf);
  http_send_header(hc, HTTP_STATUS_OK, "application/sdp", q.hq_size,
                   NULL, NULL, 0, NULL, NULL, &args);
  tcp_write_queue(hc->hc_fd, &q);
  http_arg_flush(&args);
  htsbuf_queue_flush(&q);
  return 0;

error:
  htsbuf_queue_flush(&q);
  http_error(hc, HTTP_STATUS_BAD_REQUEST);
  return 0;
}

/*
 *
 */
static int
rtsp_process_play(http_connection_t *hc, int setup)
{
  session_t *rs;
  int errcode = HTTP_STATUS_BAD_REQUEST, valid = 0, oldstate = 0, i, stream;;
  char buf[256], *u = tvh_strdupa(hc->hc_url);
  http_arg_list_t args;

  http_arg_init(&args);

  if ((u = rtsp_check_urlbase(u)) == NULL)
    goto error2;

  if ((stream = rtsp_parse_args(hc, u)) < 0)
    goto error2;

  pthread_mutex_lock(&rtsp_lock);

  errcode = rtsp_parse_cmd(hc, stream, setup, &rs, &valid, &oldstate);

  if (errcode) goto error;

  if (setup) {
    if (udp_bind_double(&rs->udp_rtp, &rs->udp_rtcp,
                        "satips", "rtsp", "rtcp",
                        rtsp_ip, 0, NULL,
                        4*1024, 4*1024,
                        RTP_BUFSIZE, RTCP_BUFSIZE)) {
      errcode = HTTP_STATUS_INTERNAL;
      goto error;
    }
    if (udp_connect(rs->udp_rtp,  "RTP",  hc->hc_peer_ipstr, rs->rtp_peer_port) ||
        udp_connect(rs->udp_rtcp, "RTCP", hc->hc_peer_ipstr, rs->rtp_peer_port + 1)) {
      errcode = HTTP_STATUS_INTERNAL;
      goto error;
    }
  }

  if ((errcode = rtsp_start(hc, rs, hc->hc_peer_ipstr, valid, setup, oldstate)) < 0)
    goto error;

  if (setup) {
    snprintf(buf, sizeof(buf), "%s;timeout=%d", rs->session, RTSP_TIMEOUT);
    http_arg_set(&args, "Session", buf);
    i = rs->rtp_peer_port;
    snprintf(buf, sizeof(buf), "RTP/AVP;unicast;client_port=%d-%d", i, i+1);
    http_arg_set(&args, "Transport", buf);
    snprintf(buf, sizeof(buf), "%d", rs->stream);
    http_arg_set(&args, "com.ses.streamID", buf);
  } else {
    if (rtsp_port != 554)
      snprintf(buf, sizeof(buf), "url=rtsp://%s:%d/stream=%d", rtsp_ip, rtsp_port, rs->stream);
    else
      snprintf(buf, sizeof(buf), "url=rtsp://%s/stream=%d", rtsp_ip, rs->stream);
    http_arg_set(&args, "RTP-Info", buf);
  }

  pthread_mutex_unlock(&rtsp_lock);

  http_send_header(hc, HTTP_STATUS_OK, NULL, 0, NULL, NULL, 0, NULL, NULL, &args);

  goto end;

error:
  pthread_mutex_unlock(&rtsp_lock);
error2:
  http_error(hc, errcode);

end:
  http_arg_flush(&args);
  return 0;
}

/*
 *
 */
static int
rtsp_process_teardown(http_connection_t *hc)
{
  char *u = tvh_strdupa(hc->hc_url);
  struct session *rs = NULL;
  http_arg_list_t args;
  char session[16];
  int stream;

  if ((u = rtsp_check_urlbase(u)) == NULL ||
      (stream = rtsp_parse_args(hc, u)) < 0) {
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    return 0;
  }

  tvhdebug("satips", "-/%s/%i: teardown from %s:%d",
           hc->hc_session, stream, hc->hc_peer_ipstr, IP_PORT(*hc->hc_peer));

  pthread_mutex_lock(&rtsp_lock);
  rs = rtsp_find_session(hc, stream);
  if (!rs || stream != rs->stream) {
    pthread_mutex_unlock(&rtsp_lock);
    http_error(hc, !rs ? HTTP_STATUS_BAD_SESSION : HTTP_STATUS_NOT_FOUND);
  } else {
    strncpy(session, rs->session, sizeof(session));
    session[sizeof(session)-1] = '\0';
    rtsp_close_session(rs);
    rtsp_free_session(rs);
    pthread_mutex_unlock(&rtsp_lock);
    http_arg_init(&args);
    http_arg_set(&args, "Session", session);
    http_send_header(hc, HTTP_STATUS_OK, NULL, 0, NULL, NULL, 0, NULL, NULL, NULL);
    http_arg_flush(&args);
  }
  return 0;
}

/**
 * Process a RTSP request
 */
static int
rtsp_process_request(http_connection_t *hc, htsbuf_queue_t *spill)
{
  switch (hc->hc_cmd) {
  case RTSP_CMD_OPTIONS:
    return rtsp_process_options(hc);
  case RTSP_CMD_DESCRIBE:
    return rtsp_process_describe(hc);
  case RTSP_CMD_SETUP:
  case RTSP_CMD_PLAY:
    return rtsp_process_play(hc, hc->hc_cmd == RTSP_CMD_SETUP);
  case RTSP_CMD_TEARDOWN:
    return rtsp_process_teardown(hc);
  default:
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    return 0;
  }
}

/*
 *
 */
static void
rtsp_flush_requests(http_connection_t *hc)
{
  struct session *rs, *rs_next;

  pthread_mutex_lock(&rtsp_lock);
  for (rs = TAILQ_FIRST(&rtsp_sessions); rs; rs = rs_next) {
    rs_next = TAILQ_NEXT(rs, link);
    if (rs->shutdown_on_close) {
      rtsp_close_session(rs);
      rtsp_free_session(rs);
    }
  }
  pthread_mutex_unlock(&rtsp_lock);
}

/*
 *
 */
static void
rtsp_stream_status ( void *opaque, htsmsg_t *m )
{
  http_connection_t *hc = opaque;
  htsmsg_add_str(m, "type", "SAT>IP");
  if (hc->hc_username)
    htsmsg_add_str(m, "user", hc->hc_username);
}

/*
 *
 */
static void
rtsp_serve(int fd, void **opaque, struct sockaddr_storage *peer,
           struct sockaddr_storage *self)
{
  http_connection_t hc;
  access_t aa;
  char buf[128];
  void *tcp;

  memset(&hc, 0, sizeof(http_connection_t));
  *opaque = &hc;

  memset(&aa, 0, sizeof(aa));
  strcpy(buf, "SAT>IP Client ");
  tcp_get_str_from_ip((struct sockaddr *)peer, buf + strlen(buf), sizeof(buf) - strlen(buf));
  aa.aa_representative = buf;

  tcp = tcp_connection_launch(fd, rtsp_stream_status, &aa);

  /* Note: global_lock held on entry */
  pthread_mutex_unlock(&global_lock);

  hc.hc_fd      = fd;
  hc.hc_peer    = peer;
  hc.hc_self    = self;
  hc.hc_process = rtsp_process_request;
  hc.hc_cseq    = 1;

  http_serve_requests(&hc);

  close(fd);

  rtsp_flush_requests(&hc);

  /* Note: leave global_lock held for parent */
  pthread_mutex_lock(&global_lock);
  *opaque = NULL;

  tcp_connection_land(tcp);
}

/*
 *
 */
static void
rtsp_close_session(session_t *rs)
{
  satip_rtp_close((void *)(intptr_t)rs->stream);
  rs->state = STATE_DESCRIBE;
  udp_close(rs->udp_rtp);
  rs->udp_rtp = NULL;
  udp_close(rs->udp_rtcp);
  rs->udp_rtcp = NULL;
  pthread_mutex_lock(&global_lock);
  mpegts_pid_reset(&rs->pids);
  rtsp_clean(rs);
  gtimer_disarm(&rs->timer);
  pthread_mutex_unlock(&global_lock);
}

/*
 *
 */
static void
rtsp_free_session(session_t *rs)
{
  TAILQ_REMOVE(&rtsp_sessions, rs, link);
  pthread_mutex_lock(&global_lock);
  gtimer_disarm(&rs->timer);
  pthread_mutex_unlock(&global_lock);
  mpegts_pid_done(&rs->pids);
  free(rs);
}

/*
 *
 */
static void
rtsp_close_sessions(void)
{
  session_t *rs;
  do {
    pthread_mutex_lock(&rtsp_lock);
    rs = TAILQ_FIRST(&rtsp_sessions);
    if (rs) {
      rtsp_close_session(rs);
      rtsp_free_session(rs);
    }
    pthread_mutex_unlock(&rtsp_lock);
  } while (rs != NULL);
}

/*
 *
 */
void satip_server_rtsp_init
  (const char *bindaddr, int port, int descramble, int rewrite_pmt, int muxcnf)
{
  static tcp_server_ops_t ops = {
    .start  = rtsp_serve,
    .stop   = NULL,
    .cancel = http_cancel
  };
  int reg = 0;
  uint8_t rnd[4];
  if (!rtsp_server) {
    uuid_random(rnd, sizeof(rnd));
    session_number = *(uint32_t *)rnd;
    TAILQ_INIT(&rtsp_sessions);
    pthread_mutex_init(&rtsp_lock, NULL);
    satip_rtp_init(1);
  }
  if (rtsp_port != port && rtsp_server) {
    rtsp_close_sessions();
    tcp_server_delete(rtsp_server);
    rtsp_server = NULL;
    reg = 1;
  }
  free(rtsp_ip);
  rtsp_ip = strdup(bindaddr);
  rtsp_port = port;
  rtsp_descramble = descramble;
  rtsp_rewrite_pmt = rewrite_pmt;
  rtsp_muxcnf = muxcnf;
  if (!rtsp_server)
    rtsp_server = tcp_server_create(bindaddr, port, &ops, NULL);
  if (reg)
    tcp_server_register(rtsp_server);
}

void satip_server_rtsp_register(void)
{
  tcp_server_register(rtsp_server);
  satip_rtp_init(0);
}

void satip_server_rtsp_done(void)
{
  pthread_mutex_lock(&global_lock);
  if (rtsp_server)
    tcp_server_delete(rtsp_server);
  pthread_mutex_unlock(&global_lock);
  rtsp_close_sessions();
  pthread_mutex_lock(&global_lock);
  rtsp_server = NULL;
  rtsp_port = -1;
  free(rtsp_ip);
  rtsp_ip = NULL;
  satip_rtp_done();
  pthread_mutex_unlock(&global_lock);
}
