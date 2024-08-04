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
#include "streaming.h"
#include "satip/server.h"
#include "input/mpegts/iptv/iptv_private.h"

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
  char *peer_ipstr;
  int stream;
  int frontend;
  int findex;
  int src;
  int state;
  int playing;
  int weight;
  int used_weight;
  http_connection_t *shutdown_on_close;
  http_connection_t *tcp_data;
  int perm_lock;
  int no_data;
  uint32_t nsession;
  char session[9];
  dvb_mux_conf_t dmc;
  dvb_mux_conf_t dmc_tuned;
  mpegts_apids_t pids;
  mtimer_t timer;
  mpegts_mux_t *mux;
  int mux_created;
  profile_chain_t prch;
  th_subscription_t *subs;
  int rtp_peer_port;
  int rtp_udp_bound;
  udp_connection_t *udp_rtp;
  udp_connection_t *udp_rtcp;
  void *rtp_handle;
  http_connection_t *old_hc;
  LIST_HEAD(, slave_subscription) slaves;
} session_t;

static uint32_t session_number;
static uint16_t stream_id;
static char *rtsp_ip = NULL;
static char *rtsp_nat_ip = NULL;
static char *rtp_src_ip = NULL;
static int rtsp_port = -1;
static int rtsp_nat_port = -1;
static int rtsp_descramble = 1;
static int rtsp_rewrite_pmt = 0;
static int rtsp_muxcnf = MUXCNF_AUTO;
static void *rtsp_server = NULL;
static TAILQ_HEAD(,session) rtsp_sessions;
static tvh_mutex_t rtsp_lock;

static void rtsp_close_session(session_t *rs);
static void rtsp_free_session(session_t *rs);

/*
 *
 */
static inline int rtsp_is_nat_active(void)
{
  return rtsp_nat_ip[0] != '\0';
}

/*
 *
 */
int
satip_rtsp_delsys(int fe, int *findex, const char **ftype)
{
  const char *t = NULL;
  int res, i;

  if (fe < 1)
    return DVB_SYS_NONE;
  tvh_mutex_lock(&global_lock);
  i = satip_server_conf.satip_dvbs;
  if (fe <= i) {
    res = DVB_SYS_DVBS;
    t = "DVB-S";
    goto result;
  }
  fe -= i;
  i = satip_server_conf.satip_dvbs2;
  if (fe <= i) {
    res = DVB_SYS_DVBS;
    t = "DVB-S2";
    goto result;
  }
  fe -= i;
  i = satip_server_conf.satip_dvbt;
  if (fe <= i) {
    res = DVB_SYS_DVBT;
    t = "DVB-T";
    goto result;
  }
  fe -= i;
  i = satip_server_conf.satip_dvbt2;
  if (fe <= i) {
    res = DVB_SYS_DVBT;
    t = "DVB-T2";
    goto result;
  }
  fe -= i;
  i = satip_server_conf.satip_dvbc;
  if (fe <= i) {
    res = DVB_SYS_DVBC_ANNEX_A;
    t = "DVB-C";
    goto result;
  }
  fe -= i;
  i = satip_server_conf.satip_dvbc2;
  if (fe <= i) {
    res = DVB_SYS_DVBC_ANNEX_A;
    t = "DVB-C2";
    goto result;
  }
  fe -= i;
  i = satip_server_conf.satip_atsc_t;
  if (fe <= i) {
    res = DVB_SYS_ATSC;
    t = "ATSC-T";
    goto result;
  }
  fe -= i;
  i = satip_server_conf.satip_atsc_c;
  if (fe <= i) {
    res = DVB_SYS_DVBC_ANNEX_B;
    t = "ATSC-C";
    goto result;
  }
  fe -= i;
  i = satip_server_conf.satip_isdb_t;
  if (fe <= i) {
    res = DVB_SYS_ISDBT;
    t = "ISDB-T";
    goto result;
  }
  tvh_mutex_unlock(&global_lock);
  return DVB_SYS_NONE;
result:
  tvh_mutex_unlock(&global_lock);
  *findex = i;
  if (ftype)
    *ftype = t;
  return res;
}

/*
 *
 */
static struct session *
rtsp_new_session(const char *ipstr, int delsys, uint32_t nsession, int session)
{
  struct session *rs = NULL;
  int count_s = satip_server_conf.satip_max_sessions;
  int count_u = satip_server_conf.satip_max_user_connections;

  if (count_s > 0 || count_u > 0)
  TAILQ_FOREACH(rs, &rtsp_sessions, link) {
    if (--count_s == 0) {
      tvhnotice(LS_SATIPS, "Max number (%i) of active RTSP sessions reached.",
                satip_server_conf.satip_max_sessions);
      return NULL;
    }
    if (strcmp(rs->peer_ipstr, ipstr) == 0 && --count_u == 0) {
      tvhnotice(LS_SATIPS, "Max number (%i) of active RTSP sessions per user (IP: %s).",
                satip_server_conf.satip_max_user_connections, ipstr);
      return NULL;
    }
  }

  rs = calloc(1, sizeof(*rs));
  if (rs == NULL)
    return NULL;

  rs->peer_ipstr = strdup(ipstr);
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

  if (stream <= 0)
    return NULL;
  TAILQ_FOREACH(rs, &rtsp_sessions, link) {
    if (hc->hc_session &&
        strcmp(rs->session, hc->hc_session) == 0 &&
        strcmp(rs->peer_ipstr, hc->hc_peer_ipstr) == 0) {
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

  tvhwarn(LS_SATIPS, "-/%s/%i: session closed (timeout)", rs->session, rs->stream);
  tvh_mutex_unlock(&global_lock);
  tvh_mutex_lock(&rtsp_lock);
  rtsp_close_session(rs);
  rtsp_free_session(rs);
  tvh_mutex_unlock(&rtsp_lock);
  tvh_mutex_lock(&global_lock);
}

static inline void
rtsp_rearm_session_timer(session_t *rs)
{
  if (!rs->shutdown_on_close || (rs->rtp_peer_port == RTSP_TCP_DATA)) {
    tvh_mutex_lock(&global_lock);
    mtimer_arm_rel(&rs->timer, rtsp_session_timer_cb, rs, sec2mono(RTSP_TIMEOUT));
    tvh_mutex_unlock(&global_lock);
  }
}

/*
 *
 */
static char *
rtsp_check_urlbase(char *u)
{
  char *p, *s;
  int t;

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
    t = atoi(s + 1);
    if (t != rtsp_port) {
      if (rtsp_nat_port <= 0 || t != rtsp_nat_port)
        return NULL;
    }
  } else {
#if 0 /* VLC is broken */
    if (rtsp_port != 554)
      return NULL;
#endif
  }
  if (strcmp(u, rtsp_ip)) {
    if (rtsp_nat_ip == NULL)
      return NULL;
    if (rtsp_is_nat_active()) {
      if (rtsp_nat_ip[0] != '*' && strcmp(u, rtsp_nat_ip))
        return NULL;
    }
  }
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
static inline const char *
rtsp_conn_ip(http_connection_t *hc, char *buf, size_t buflen, int *port)
{
  const char *used_ip = rtsp_ip;
  int used_port = rtsp_port, local;

  if (!rtsp_is_nat_active())
    goto end;
  /* note: it initializes hc->hc_local_ip, too */
  local = http_check_local_ip(hc) > 0;
  if (local || satip_server_conf.satip_nat_name_force) {
    used_ip = rtsp_nat_ip;
    if (rtsp_nat_port > 0)
      used_port = rtsp_nat_port;
  }

  if (used_ip == NULL || used_ip[0] == '*' || used_ip[0] == '\0') {
    if (local) {
      tcp_get_str_from_ip(hc->hc_local_ip, buf, buflen);
      used_ip = buf;
    } else {
      used_ip = "127.0.0.1";
    }
  }

end:
  *port = used_port > 0 ? used_port : 554;
  return used_ip;
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

  tvh_mutex_lock(&master->s_stream_mutex);
  if (master->s_slaves_pids == NULL)
    master->s_slaves_pids = mpegts_pid_alloc();
  tvh_mutex_unlock(&master->s_stream_mutex);
  master->s_link(master, slave);
  sub->service = slave;
  profile_chain_init(&sub->prch, NULL, slave, 0);
  snprintf(buf, sizeof(buf), "SAT>IP Slave/%s", slave->s_nicename);
  sub->ths = subscription_create_from_service(&sub->prch, NULL, rs->used_weight,
                                              buf, SUBSCRIPTION_NONE, NULL, NULL,
                                              buf, NULL);
  if (sub->ths == NULL) {
    tvherror(LS_SATIPS, "%i/%s/%i: unable to subscribe service %s\n",
             rs->frontend, rs->session, rs->stream, slave->s_nicename);
    profile_chain_close(&sub->prch);
    free(sub);
    master->s_unlink(master, slave);
  } else {
    LIST_INSERT_HEAD(&rs->slaves, sub, link);
    tvhdebug(LS_SATIPS, "%i/%s/%i: slave service %s subscribed",
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
  tvhdebug(LS_SATIPS, "%i/%s/%i: slave service %s unsubscribed",
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
rtsp_clean(session_t *rs, int clean_mux)
{
  slave_subscription_t *sub;

  if (rs->rtp_handle) {
    satip_rtp_close(rs->rtp_handle);
    rs->rtp_handle = NULL;
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
  if (clean_mux) {
    if (rs->mux && rs->mux_created &&
        (rtsp_muxcnf != MUXCNF_KEEP || LIST_EMPTY(&rs->mux->mm_services)))
      rs->mux->mm_delete(rs->mux, 1);
    rs->mux = NULL;
    rs->mux_created = 0;
  }
}

/*
 *
 */
static void
rtsp_no_data(void *opaque)
{
  session_t *rs = opaque;
  rs->no_data = 1;
}

/*
 *
 */
static int
rtsp_validate_service(mpegts_service_t *s, mpegts_apids_t *pids)
{
  int av = 0, enc = 0;
  elementary_stream_t *st;

  tvh_mutex_lock(&s->s_stream_mutex);
  if (s->s_components.set_pmt_pid <= 0 || s->s_components.set_pcr_pid <= 0) {
    tvh_mutex_unlock(&s->s_stream_mutex);
    return 0;
  }
  TAILQ_FOREACH(st, &s->s_components.set_all, es_link) {
    if (st->es_type == SCT_CA)
      enc = 1;
    if (st->es_pid > 0)
      if (pids == NULL || mpegts_pid_wexists(pids, st->es_pid, MPS_WEIGHT_RAW))
        if ((SCT_ISVIDEO(st->es_type) || SCT_ISAUDIO(st->es_type)))
          av = 1;
  }
  tvh_mutex_unlock(&s->s_stream_mutex);
  if (enc == 0 || av == 0)
    return 0;
  return pids == NULL ||
         mpegts_pid_wexists(pids, s->s_components.set_pmt_pid, MPS_WEIGHT_RAW);
}

/*
 *
 */
static void
rtsp_manage_descramble(session_t *rs)
{
  idnode_set_t *found;
  mpegts_service_t *s, *master;
  slave_subscription_t *sub;
  mpegts_apids_t pmt_pids;
  size_t si;
  int i, used = 0;

  if (rtsp_descramble <= 0)
    return;

  found = idnode_set_create(1);

  if (rs->mux == NULL || rs->subs == NULL)
    goto end;

  master = (mpegts_service_t *)rs->subs->ths_raw_service;

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
  for (si = 0; si < master->s_slaves.is_count; si++) {
    s = (mpegts_service_t *)master->s_slaves.is_array[si];
    if (idnode_set_remove(found, &s->s_id)) {
      used++;
    } else if (!idnode_set_exists(found, &s->s_id)) {
      rtsp_slave_remove(rs, master, s);
      if(si)
          si--;
    }
  }

  /* Add new ones */
  for (si = 0; used < rtsp_descramble && si < found->is_count; si++, used++) {
    s = (mpegts_service_t *)found->is_array[si];
    rtsp_slave_add(rs, master, s);
    idnode_set_remove(found, &s->s_id);
  }
  if (si < found->is_count)
    tvhwarn(LS_SATIPS, "%i/%s/%i: limit for descrambled services reached (wanted %zd allowed %d)",
            rs->frontend, rs->session, rs->stream,
            (used - si) + found->is_count, rtsp_descramble);
  
end:
  idnode_set_free(found);

  if (rtsp_rewrite_pmt) {
    /* handle PMT rewrite */
    mpegts_pid_init(&pmt_pids);
    LIST_FOREACH(sub, &rs->slaves, link) {
      if ((s = sub->service) == NULL) continue;
      if (s->s_components.set_pmt_pid <= 0 ||
          s->s_components.set_pmt_pid >= 8191) continue;
      mpegts_pid_add(&pmt_pids, s->s_components.set_pmt_pid, MPS_WEIGHT_PMT);
    }
    satip_rtp_update_pmt_pids(rs->rtp_handle, &pmt_pids);
    mpegts_pid_done(&pmt_pids);
  }
}

/*
 *
 */
static int
rtsp_start
  (http_connection_t *hc, session_t *rs, char *addrbuf,
   int newmux, int cmd)
{
  mpegts_network_t *mn, *mn2;
  dvb_network_t *ln;
  mpegts_mux_t *mux;
  mpegts_service_t *svc;
  dvb_mux_conf_t dmc;
  slave_subscription_t *sub;
  char buf[384];
  int res = HTTP_STATUS_NOT_ALLOWED, qsize = 3000000, created = 0, weight;

  tvh_mutex_lock(&global_lock);
  weight = satip_server_conf.satip_weight;
  if (satip_server_conf.satip_allow_remote_weight && rs->weight)
    weight = rs->weight;
  dmc = rs->dmc_tuned;
  if (newmux) {
    mux = NULL;
    mn2 = NULL;
    LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
      if (idnode_is_instance(&mn->mn_id, &dvb_network_class)) {
        ln = (dvb_network_t *)mn;
        if (ln->ln_type == rs->dmc.dmc_fe_type &&
            mn->mn_satip_source == rs->src) {
          if (!mn2) mn2 = mn;
          mux = (mpegts_mux_t *)
                dvb_network_find_mux((dvb_network_t *)mn, &rs->dmc,
                                     MPEGTS_ONID_NONE, MPEGTS_TSID_NONE, 1, rtsp_muxcnf == MUXCNF_REJECT_EXACT_MATCH );
          if (mux) {
            dmc = ((dvb_mux_t *)mux)->lm_tuning;
            rs->perm_lock = 0;
            break;
          }
        }
      }
#if ENABLE_IPTV
      if (idnode_is_instance(&mn->mn_id, &iptv_network_class)) {
        LIST_FOREACH(mux, &mn->mn_muxes, mm_network_link) {
          if (rs->dmc.dmc_fe_type == DVB_TYPE_T &&
              deltaU32(rs->dmc.dmc_fe_freq, ((iptv_mux_t *)mux)->mm_iptv_satip_dvbt_freq) < 2000)
            break;
          if (rs->dmc.dmc_fe_type == DVB_TYPE_C &&
              deltaU32(rs->dmc.dmc_fe_freq, ((iptv_mux_t *)mux)->mm_iptv_satip_dvbc_freq) < 2000)
            break;
          if (rs->dmc.dmc_fe_type == DVB_TYPE_S &&
              deltaU32(rs->dmc.dmc_fe_freq, ((iptv_mux_t *)mux)->mm_iptv_satip_dvbs_freq) < 2000)
            break;
          }
        if (mux) {
          dmc = rs->dmc;
          rs->perm_lock = 1;
          break;
        }
      }
#endif
      if (idnode_is_instance(&mn->mn_id, &dvb_network_class)) {
        LIST_FOREACH(mux, &mn->mn_muxes, mm_network_link) {
          if (rs->dmc.dmc_fe_type == DVB_TYPE_T &&
              deltaU32(rs->dmc.dmc_fe_freq, ((dvb_mux_t *)mux)->mm_dvb_satip_dvbt_freq) < 2000)
            break;
          if (rs->dmc.dmc_fe_type == DVB_TYPE_C &&
              deltaU32(rs->dmc.dmc_fe_freq, ((dvb_mux_t *)mux)->mm_dvb_satip_dvbc_freq) < 2000)
            break;
          if (rs->dmc.dmc_fe_type == DVB_TYPE_S &&
              deltaU32(rs->dmc.dmc_fe_freq, ((dvb_mux_t *)mux)->mm_dvb_satip_dvbs_freq) < 2000)
            break;
          }
        if (mux) {
          dmc = rs->dmc;
          rs->perm_lock = 1;
          break;
        }
      }
    }
    if (mux == NULL && mn2 &&
        (rtsp_muxcnf == MUXCNF_AUTO || rtsp_muxcnf == MUXCNF_KEEP)) {
      dvb_mux_conf_str(&rs->dmc, buf, sizeof(buf));
      tvhwarn(LS_SATIPS, "%i/%s/%i: create mux %s",
              rs->frontend, rs->session, rs->stream, buf);
      mux =
        mn2->mn_create_mux(mn2, (void *)(intptr_t)rs->nsession,
                           MPEGTS_ONID_NONE, MPEGTS_TSID_NONE,
                           &rs->dmc, 1);
      if (mux) {
        created = 1;
        dmc = ((dvb_mux_t *)mux)->lm_tuning;
        rs->perm_lock = 1;
      }
    }
    if (mux == NULL) {
      dvb_mux_conf_str(&rs->dmc, buf, sizeof(buf));
      tvhwarn(LS_SATIPS, "%i/%s/%i: unable to create mux %s%s",
              rs->frontend, rs->session, rs->stream, buf,
              (rtsp_muxcnf == MUXCNF_REJECT || rtsp_muxcnf == MUXCNF_REJECT_EXACT_MATCH ) ? " (configuration)" : "");
      goto endclean;
    }
    if (rs->mux == mux && rs->subs) {
      if (rs->no_data) {
        dvb_mux_conf_str(&rs->dmc, buf, sizeof(buf));
        tvhwarn(LS_SATIPS, "%i/%s/%i: subscription fails because mux %s can't tune",
                rs->frontend, rs->session, rs->stream, buf);
        goto endclean;
      }
      goto pids;
    }
    rtsp_clean(rs, 1);
    rs->dmc_tuned = dmc;
    rs->mux = mux;
    rs->mux_created = created;
    if (profile_chain_raw_open(&rs->prch, (mpegts_mux_t *)rs->mux, qsize, 0))
      goto endclean;
    rs->used_weight = weight;
    rs->subs = subscription_create_from_mux(&rs->prch, NULL,
                                   weight,
                                   "SAT>IP",
                                   rs->prch.prch_flags |
                                   SUBSCRIPTION_STREAMING,
                                   addrbuf, http_username(hc),
                                   http_arg_get(&hc->hc_args, "User-Agent"),
                                   NULL);
    if (!rs->subs)
      goto endclean;
    if (!rs->pids.all && rs->pids.count == 0)
      mpegts_pid_add(&rs->pids, 0, MPS_WEIGHT_RAW);
  } else {
pids:
    if (!rs->subs)
      goto endclean;
    if (!rs->pids.all && rs->pids.count == 0)
      mpegts_pid_add(&rs->pids, 0, MPS_WEIGHT_RAW);
    svc = (mpegts_service_t *)rs->subs->ths_raw_service;
    svc->s_update_pids(svc, &rs->pids);
    satip_rtp_update_pids(rs->rtp_handle, &rs->pids);
    if (rs->used_weight != weight && weight > 0) {
      subscription_set_weight(rs->subs, rs->used_weight = weight);
      LIST_FOREACH(sub, &rs->slaves, link)
        subscription_set_weight(sub->ths, weight);
    }
  }
  if (cmd != RTSP_CMD_DESCRIBE && rs->rtp_handle == NULL) {
    if (rs->mux == NULL)
      goto endclean;
    rs->no_data = 0;
    rs->rtp_handle =
      satip_rtp_queue(rs->subs, &rs->prch.prch_sq,
                      hc, hc->hc_peer, rs->rtp_peer_port,
                      rs->udp_rtp ? rs->udp_rtp->fd : hc->hc_fd,
                      rs->udp_rtcp ? rs->udp_rtcp->fd : -1,
                      rs->findex, rs->src, &rs->dmc_tuned,
                      &rs->pids,
                      cmd == RTSP_CMD_PLAY || rs->playing,
                      rs->perm_lock, rtsp_no_data, rs);
    if (rs->rtp_handle == NULL) {
      res = HTTP_STATUS_INTERNAL;
      goto endclean;
    }
    rs->tcp_data = rs->udp_rtp ? NULL : hc;
    if (!rs->pids.all && rs->pids.count == 0)
      mpegts_pid_add(&rs->pids, 0, MPS_WEIGHT_RAW);
    svc = (mpegts_service_t *)rs->subs->ths_raw_service;
    svc->s_update_pids(svc, &rs->pids);
    rs->playing = cmd == RTSP_CMD_PLAY || rs->playing;
    rs->state = STATE_PLAY;
  } else if (cmd == RTSP_CMD_PLAY) {
    rs->playing = 1;
    if (rs->mux == NULL)
      goto endclean;
    satip_rtp_allow_data(rs->rtp_handle);
  }
  rtsp_manage_descramble(rs);
  tvh_mutex_unlock(&global_lock);
  return 0;

endclean:
  rtsp_clean(rs, 0);
  tvh_mutex_unlock(&global_lock);
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
    { "isdbt",  DVB_SYS_ISDBT },
    { "dvbcb", DVB_SYS_DVBC_ANNEX_B }
  };
  const char *s = http_arg_get_remove(&hc->hc_req_args, "msys");
  return s && s[0] ? str2val(s, tab) : DVB_SYS_NONE;
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
  return s && s[0] ? str2val(s, tab) : -1;
}

static int
fec_to_tvh(http_connection_t *hc)
{
  switch (atoi(http_arg_get_remove(&hc->hc_req_args, "fec") ?: "0")) {
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
  int bw = atof(http_arg_get_remove(&hc->hc_req_args, "bw") ?: "0") * 1000;
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
  int ro = atof(http_arg_get_remove(&hc->hc_req_args, "ro") ?: "0") * 1000;
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
  if (s && strcmp(s, "on") == 0)
    return DVB_PILOT_ON;
  if (s && strcmp(s, "off") == 0)
    return DVB_PILOT_OFF;
  if (s == NULL || s[0] == '\0' || strcmp(s, "auto") == 0)
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
  if (s && s[0]) {
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
  if (s && s[0]) {
    int v = str2val(s, tab);
    return v >= 0 ? v : DVB_MOD_NONE;
  }
  return DVB_MOD_AUTO;
}

static int
gi_to_tvh(http_connection_t *hc)
{
  switch (atoi(http_arg_get_remove(&hc->hc_req_args, "gi") ?: "0")) {
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
    if (strcmp(x, "all") == 0 || strcmp(x, "8192") == 0) {
      if (satip_server_conf.satip_restrict_pids_all) {
        pids->all = 0;
        for (pid = 1; pid <= 2; pid++) /* CAT, TSDT */
           mpegts_pid_add(pids, pid, MPS_WEIGHT_RAW);
        for (pid = 0x10; pid < 0x20; pid++) /* NIT ... SIT */
           mpegts_pid_add(pids, pid, MPS_WEIGHT_RAW);
        mpegts_pid_add(pids, 0x1ffb, MPS_WEIGHT_RAW); /* ATSC */
      } else {
        pids->all = 1;
      }
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
parse_transport(http_connection_t *hc, char *destip, size_t destip_len)
{
  const char *s = http_arg_get(&hc->hc_args, "Transport");
  const char *u;
  char *x;
  int a, b;
  if (!s)
    return -1;
  destip[0] = '\0';
  if (strncmp(s, "RTP/AVP;unicast;", 16) == 0) {
    s += 16;
    a = -1;
    while (*s) {
      if (strncmp(s, "destination=", 12) == 0) {
        s += 12;
        strlcpy(destip, s, destip_len);
        for (x = destip; *x && *x != ';'; x++);
        *x = '\0';
        for (; *s && *s != ';'; s++);
        if (*s != '\0' && *s != ';') return -1;
        if (*s == ';') s++;
        continue;
      }
      if (strncmp(s, "client_port=", 12) == 0) {
        for (s += 12, u = s; isdigit(*u); u++);
        if (*u != '-') return -1;
        a = atoi(s);
        for (s = ++u; isdigit(*s); s++);
        if (*s != '\0' && *s != ';') return -1;
        b = atoi(u);
        if (a + 1 != b) return -1;
        if (*s == ';') s++;
        continue;
      }
      return -1;
    }
    return a;
  } else if ((strncmp(s, "RTP/AVP/TCP;interleaved=0-1", 27) == 0) &&
             !satip_server_conf.satip_notcp_mode) {
    return RTSP_TCP_DATA;
  }
  return -1;
}

/*
 *
 */
static int
rtsp_parse_cmd
  (http_connection_t *hc, int stream, int cmd,
   session_t **rrs, int *valid)
{
  session_t *rs = NULL;
  int errcode = HTTP_STATUS_BAD_REQUEST, r, findex = 1, has_args, weight = 0;
  int delsys = DVB_SYS_NONE, msys, fe, src, freq, pol, sr;
  int fec, ro, plts, bw, tmode, mtype, gi, plp, t2id, sm, c2tft, ds, specinv;
  int alloc_stream_id = 0;
  char *s;
  const char *caller;
  mpegts_apids_t pids, addpids, delpids;
  dvb_mux_conf_t *dmc;
  char buf[256];
  http_arg_t *arg;

  switch (cmd) {
  case RTSP_CMD_DESCRIBE: caller = "DESCRIBE"; break;
  case RTSP_CMD_PLAY:     caller = "PLAY";     break;
  case RTSP_CMD_SETUP:    caller = "SETUP";    break;
  default:                caller = NULL;       break;
  }

  *rrs = NULL;
  mpegts_pid_init(&pids);
  mpegts_pid_init(&addpids);
  mpegts_pid_init(&delpids);

  has_args = !TAILQ_EMPTY(&hc->hc_req_args);

  fe = atoi(http_arg_get_remove(&hc->hc_req_args, "fe") ?: 0);
  fe = satip_server_conf.satip_drop_fe ? 0 : fe;
  s = http_arg_get_remove(&hc->hc_req_args, "addpids");
  if (parse_pids(s, &addpids)) goto end;
  s = http_arg_get_remove(&hc->hc_req_args, "delpids");
  if (parse_pids(s, &delpids)) goto end;
  s = http_arg_get_remove(&hc->hc_req_args, "pids");
  if (parse_pids(s, &pids)) goto end;
  msys = msys_to_tvh(hc);
  freq = atof(http_arg_get_remove(&hc->hc_req_args, "freq")) * 1000;
  *valid = freq >= 10000;
  weight = atoi(http_arg_get_remove(&hc->hc_req_args, "tvhweight"));

  if (addpids.count > 0 || delpids.count > 0) {
    if (cmd == RTSP_CMD_SETUP)
      goto end;
    if (!stream)
      goto end;
  }

  rs = rtsp_find_session(hc, stream);

  if (fe > 0) {
    delsys = satip_rtsp_delsys(fe, &findex, NULL);
    if (delsys == DVB_SYS_NONE) {
      tvherror(LS_SATIPS, "fe=%d does not exist", fe);
      goto end;
    }
  } else {
    delsys = msys;
  }

  if (cmd == RTSP_CMD_SETUP) {
    if (!rs) {
      rs = rtsp_new_session(hc->hc_peer_ipstr, msys, 0, -1);
      if (rs == NULL) goto end;
      if (delsys == DVB_SYS_NONE) goto end;
      if (msys == DVB_SYS_NONE) goto end;
      if (!(*valid)) goto end;
      alloc_stream_id = 1;
    } else if (stream != rs->stream) {
      rs = rtsp_new_session(hc->hc_peer_ipstr, msys, rs->nsession, stream);
      if (rs == NULL) goto end;
      if (delsys == DVB_SYS_NONE) goto end;
      if (msys == DVB_SYS_NONE) goto end;
      if (!(*valid)) goto end;
      alloc_stream_id = 1;
    } else {
      if (!has_args && rs->state == STATE_DESCRIBE) {
        r = parse_transport(hc, buf, sizeof(buf));
        if (r < 0 || (buf[0] && strcmp(buf, hc->hc_peer_ipstr))) {
          errcode = HTTP_STATUS_BAD_TRANSFER;
          goto end;
        }
        rs->rtp_peer_port = r;
        *valid = 1;
        goto ok;
      }
    }
    r = parse_transport(hc, buf, sizeof(buf));
    if (r < 0 || (buf[0] && strcmp(buf, hc->hc_peer_ipstr))) {
      errcode = HTTP_STATUS_BAD_TRANSFER;
      goto end;
    }
    if (rs->state == STATE_PLAY && rs->rtp_peer_port != r) {
      errcode = HTTP_STATUS_METHOD_INVALID;
      goto end;
    }
    rs->rtp_peer_port = r;
    rs->frontend = fe > 0 ? fe : 1;
  } else {
    if (!rs && !stream && cmd == RTSP_CMD_DESCRIBE)
      rs = rtsp_new_session(hc->hc_peer_ipstr, msys, 0, -1);
    if (!rs || stream != rs->stream) {
      if (rs)
        errcode = HTTP_STATUS_NOT_FOUND;
      goto end;
    }
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

  dvb_mux_conf_init(NULL, dmc = &rs->dmc, msys);

  mtype = mtype_to_tvh(hc);
  if (mtype == DVB_MOD_NONE) goto end;

  src = http_arg_get(&hc->hc_req_args, "src") ?
    atoi(http_arg_get_remove(&hc->hc_req_args, "src")) : 1;
  if (src < 1) goto end;

  if (msys == DVB_SYS_DVBS || msys == DVB_SYS_DVBS2) {

    pol = pol_to_tvh(hc);
    if (pol < 0) goto end;
    sr = atof(http_arg_get_remove(&hc->hc_req_args, "sr") ?: "0") * 1000;
    if (sr < 1000) goto end;
    fec = fec_to_tvh(hc);
    if (fec == DVB_FEC_NONE) goto end;
    ro = rolloff_to_tvh(hc);
    if (ro == DVB_ROLLOFF_NONE ||
        (ro != DVB_ROLLOFF_35 && msys == DVB_SYS_DVBS)) goto end;
    plts = pilot_to_tvh(hc);
    if (plts == DVB_PILOT_NONE) goto end;

    TAILQ_FOREACH(arg, &hc->hc_req_args, link)
      tvhwarn(LS_SATIPS, "%i/%s/%i: extra parameter '%s'='%s'",
        rs->frontend, rs->session, rs->stream,
        arg->key, arg->val);

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
    if (s && s[0]) {
      plp = atoi(s);
      if (plp < 0 || plp > 255) goto end;
    } else {
      plp = DVB_NO_STREAM_ID_FILTER;
    }
    t2id = atoi(http_arg_get_remove(&hc->hc_req_args, "t2id") ?: "0");
    if (t2id < 0 || t2id > 65535) goto end;
    sm = atoi(http_arg_get_remove(&hc->hc_req_args, "sm") ?: "0");
    if (sm < 0 || sm > 1) goto end;

    TAILQ_FOREACH(arg, &hc->hc_req_args, link)
      tvhwarn(LS_SATIPS, "%i/%s/%i: extra parameter '%s'='%s'",
        rs->frontend, rs->session, rs->stream,
        arg->key, arg->val);

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
    c2tft = atoi(http_arg_get_remove(&hc->hc_req_args, "c2tft") ?: "0");
    if (c2tft < 0 || c2tft > 2) goto end;
    bw = bw_to_tvh(hc);
    if (bw == DVB_BANDWIDTH_NONE) goto end;
    sr = atof(http_arg_get_remove(&hc->hc_req_args, "sr") ?: "0") * 1000;
    if (sr < 1000) goto end;
    ds = atoi(http_arg_get_remove(&hc->hc_req_args, "ds") ?: "0");
    if (ds < 0 || ds > 255) goto end;
    s = http_arg_get_remove(&hc->hc_req_args, "plp");
    if (s && s[0]) {
      plp = atoi(http_arg_get_remove(&hc->hc_req_args, "plp"));
      if (plp < 0 || plp > 255) goto end;
    } else {
      plp = DVB_NO_STREAM_ID_FILTER;
    }
    specinv = atoi(http_arg_get_remove(&hc->hc_req_args, "specinv") ?: "0");
    if (specinv < 0 || specinv > 1) goto end;
    fec = fec_to_tvh(hc);
    if (fec == DVB_FEC_NONE) goto end;

    TAILQ_FOREACH(arg, &hc->hc_req_args, link)
      tvhwarn(LS_SATIPS, "%i/%s/%i: extra parameter '%s'='%s'",
        rs->frontend, rs->session, rs->stream,
        arg->key, arg->val);

    dmc->u.dmc_fe_qam.symbol_rate = sr;
    dmc->u.dmc_fe_qam.fec_inner = fec;
    dmc->dmc_fe_inversion = specinv;
    dmc->dmc_fe_stream_id = plp;
    dmc->dmc_fe_pls_code = ds; /* check */

  } else if (msys == DVB_SYS_ISDBT || msys == DVB_SYS_ATSC || msys == DVB_SYS_DVBC_ANNEX_B) {

    freq *= 1000;
    if (freq < 0) goto end;
    TAILQ_FOREACH(arg, &hc->hc_req_args, link)
      tvhwarn(LS_SATIPS, "%i/%s/%i: extra parameter '%s'='%s'",
        rs->frontend, rs->session, rs->stream,
        arg->key, arg->val);

  } else {

    goto end;

  }

  dmc->dmc_fe_freq = freq;
  dmc->dmc_fe_modulation = mtype;
  dmc->dmc_fe_delsys = msys;

  rs->frontend = fe;
  rs->findex = findex;
  rs->src = src;
  if (weight > 0)
    rs->weight = weight;
  else if (cmd == RTSP_CMD_SETUP)
    rs->weight = 0;
  rs->old_hc = hc;

  if (alloc_stream_id) {
    stream_id++;
    if (stream_id == 0)
      stream_id++;
    rs->stream = stream_id % 0x7fff;
  } else {
    /* don't subscribe to the new mux, if it was already done */
    if (memcmp(dmc, &rs->dmc_tuned, sizeof(*dmc)) == 0 && !rs->no_data)
      *valid = 0;
  }

  if (cmd == RTSP_CMD_DESCRIBE)
    rs->shutdown_on_close = hc;

play:
  if (pids.count > 0 || pids.all)
    mpegts_pid_copy(&rs->pids, &pids);
  if (delpids.count > 0)
    mpegts_pid_del_group(&rs->pids, &delpids);
  if (addpids.count > 0)
    mpegts_pid_add_group(&rs->pids, &addpids);

  dvb_mux_conf_str(&rs->dmc, buf, sizeof(buf));
  r = strlen(buf);
  tvh_strlcatf(buf, sizeof(buf), r, " pids ");
  if (mpegts_pid_dump(&rs->pids, buf + r, sizeof(buf) - r, 0, 0) == 0)
    tvh_strlcatf(buf, sizeof(buf), r, "<none>");

  tvhdebug(LS_SATIPS, "%i/%s/%d: %s from %s:%d %s",
           rs->frontend, rs->session, rs->stream,
           caller, hc->hc_peer_ipstr, ntohs(IP_PORT(*hc->hc_peer)), buf);

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
    tvh_mutex_lock(&rtsp_lock);
    TAILQ_FOREACH(rs, &rtsp_sessions, link)
      if (!strcmp(rs->session, hc->hc_session)) {
        rtsp_rearm_session_timer(rs);
        found = 1;
      }
    tvh_mutex_unlock(&rtsp_lock);
    if (!found) {
      http_error(hc, HTTP_STATUS_BAD_SESSION);
      return 0;
    }
  }
  http_arg_init(&args);
  http_arg_set(&args, "Public", "OPTIONS,DESCRIBE,SETUP,PLAY,TEARDOWN");
  if (hc->hc_session)
    http_arg_set(&args, "Session", hc->hc_session);
  http_send_begin(hc);
  http_send_header(hc, HTTP_STATUS_OK, NULL, 0, NULL, NULL, 0, NULL, NULL, &args);
  http_send_end(hc);
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
  unsigned long mono = mclk();
  int dvbt, dvbc;

  htsbuf_append_str(q, "v=0\r\n");
  htsbuf_qprintf(q, "o=- %lu %lu IN %s %s\r\n",
                 rs ? (unsigned long)rs->nsession : mono - 123,
                 mono,
                 strchr(rtsp_ip, ':') ? "IP6" : "IP4",
                 rtsp_ip);

  tvh_mutex_lock(&global_lock);
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
  tvh_mutex_unlock(&global_lock);

  htsbuf_append_str(q, "t=0 0\r\n");
}

static void
rtsp_describe_session(session_t *rs, htsbuf_queue_t *q)
{
  char buf[4096];

  if (rs->stream > 0)
    htsbuf_qprintf(q, "a=control:stream=%d\r\n", rs->stream);
  htsbuf_qprintf(q, "a=tool:%s\r\n", config_get_http_server_name());
  htsbuf_append_str(q, "m=video 0 RTP/AVP 33\r\n");
  if (strchr(rtsp_ip, ':'))
    htsbuf_append_str(q, "c=IN IP6 ::0\r\n");
  else
    htsbuf_append_str(q, "c=IN IP4 0.0.0.0\r\n");
  if (rs->state == STATE_PLAY || rs->state == STATE_SETUP) {
    satip_rtp_status(rs->rtp_handle, buf, sizeof(buf));
    htsbuf_qprintf(q, "a=fmtp:33 %s\r\n", buf);
    htsbuf_qprintf(q, "a=%s\r\n", rs->state == STATE_SETUP ? "inactive" : "sendonly");
  } else {
    htsbuf_append_str(q, "a=fmtp:33\r\n");
    htsbuf_append_str(q, "a=inactive\r\n");
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
  char buf[96], buf1[46];
  const char *used_ip = NULL;
  int r = HTTP_STATUS_BAD_REQUEST;
  int stream, first = 1, valid, used_port;

  htsbuf_queue_init(&q, 0);

  arg = http_arg_get(&hc->hc_args, "Accept");
  if (arg == NULL || strcmp(arg, "application/sdp"))
    goto error;

  if ((u = rtsp_check_urlbase(u)) == NULL)
    goto error;

  stream = rtsp_parse_args(hc, u);

  tvh_mutex_lock(&rtsp_lock);

  if (TAILQ_FIRST(&hc->hc_req_args)) {
    if (stream < 0) {
      tvh_mutex_unlock(&rtsp_lock);
      goto error;
    }
    r = rtsp_parse_cmd(hc, stream, RTSP_CMD_DESCRIBE, &rs, &valid);
    if (r) {
      tvh_mutex_unlock(&rtsp_lock);
      goto error;
    }
  }

  TAILQ_FOREACH(rs, &rtsp_sessions, link) {
    if (hc->hc_session) {
      if (strcmp(hc->hc_session, rs->session))
        continue;
      rtsp_rearm_session_timer(rs);
    }
    if (stream > 0 && rs->stream != stream)
      continue;
    if (satip_server_conf.satip_anonymize &&
        strcmp(hc->hc_peer_ipstr, rs->peer_ipstr))
      continue;
    if (first) {
      rtsp_describe_header(hc->hc_session ? rs : NULL, &q);
      first = 0;
    }
    rtsp_describe_session(rs, &q);
  }
  tvh_mutex_unlock(&rtsp_lock);

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
  used_ip = rtsp_conn_ip(hc, buf1, sizeof(buf1), &used_port);
  if ((stream > 0) && (used_port != 554))
    snprintf(buf, sizeof(buf), "rtsp://%s:%d/stream=%i", used_ip, used_port, stream);
  else if ((stream > 0) && (used_port == 554))
    snprintf(buf, sizeof(buf), "rtsp://%s/stream=%i", used_ip, stream);
  else if (used_port != 554)
    snprintf(buf, sizeof(buf), "rtsp://%s:%d", used_ip, used_port);
  else
    snprintf(buf, sizeof(buf), "rtsp://%s", used_ip);
  http_arg_set(&args, "Content-Base", buf);
  http_send_begin(hc);
  http_send_header(hc, HTTP_STATUS_OK, "application/sdp", q.hq_size,
                   NULL, NULL, 0, NULL, NULL, &args);
  tcp_write_queue(hc->hc_fd, &q);
  http_send_end(hc);
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
rtsp_process_play(http_connection_t *hc, int cmd)
{
  session_t *rs;
  int errcode = HTTP_STATUS_BAD_REQUEST, valid = 0, i, stream, used_port;
  char buf[256], buf1[46], *u = tvh_strdupa(hc->hc_url);
  const char *used_ip = NULL;
  http_arg_list_t args;

  http_arg_init(&args);

  if ((u = rtsp_check_urlbase(u)) == NULL)
    goto error2;

  if ((stream = rtsp_parse_args(hc, u)) < 0)
    goto error2;

  tvh_mutex_lock(&rtsp_lock);

  errcode = rtsp_parse_cmd(hc, stream, cmd, &rs, &valid);

  if (errcode) goto error;

  if (cmd == RTSP_CMD_SETUP && rs->rtp_peer_port != RTSP_TCP_DATA &&
      !rs->rtp_udp_bound) {
    if (udp_bind_double(&rs->udp_rtp, &rs->udp_rtcp,
                        LS_SATIPS, "rtsp", "rtcp",
                        (rtp_src_ip != NULL && rtp_src_ip[0] != '\0') ? rtp_src_ip : rtsp_ip, 0, NULL,
                        4*1024, 4*1024,
                        RTP_BUFSIZE, RTCP_BUFSIZE)) {
      errcode = HTTP_STATUS_INTERNAL;
      goto error;
    }
    if (udp_connect(rs->udp_rtp,  "RTP",  hc->hc_peer_ipstr, rs->rtp_peer_port) ||
        udp_connect(rs->udp_rtcp, "RTCP", hc->hc_peer_ipstr, rs->rtp_peer_port + 1)) {
      udp_close(rs->udp_rtp);
      rs->udp_rtp = NULL;
      udp_close(rs->udp_rtcp);
      rs->udp_rtcp = NULL;
      errcode = HTTP_STATUS_INTERNAL;
      goto error;
    }
    rs->rtp_udp_bound = 1;
  }

  if (cmd == RTSP_CMD_SETUP && rs->rtp_peer_port == RTSP_TCP_DATA)
    rs->shutdown_on_close = hc;

  if ((errcode = rtsp_start(hc, rs, hc->hc_peer_ipstr, valid, cmd)) != 0)
    goto error;

  if (cmd == RTSP_CMD_SETUP) {
    snprintf(buf, sizeof(buf), "%s;timeout=%d", rs->session, RTSP_TIMEOUT);
    http_arg_set(&args, "Session", buf);
    i = rs->rtp_peer_port;
    if (i == RTSP_TCP_DATA) {
      snprintf(buf, sizeof(buf), "RTP/AVP/TCP;interleaved=0-1");
    } else {
      snprintf(buf, sizeof(buf), "RTP/AVP;unicast;client_port=%d-%d", i, i+1);
    }
    http_arg_set(&args, "Transport", buf);
    snprintf(buf, sizeof(buf), "%d", rs->stream);
    http_arg_set(&args, "com.ses.streamID", buf);
  } else {
    used_ip = rtsp_conn_ip(hc, buf1, sizeof(buf1), &used_port);
    if (used_port != 554)
      snprintf(buf, sizeof(buf), "url=rtsp://%s:%d/stream=%d", used_ip, used_port, rs->stream);
    else
      snprintf(buf, sizeof(buf), "url=rtsp://%s/stream=%d", used_ip, rs->stream);
    http_arg_set(&args, "RTP-Info", buf);
  }

  tvh_mutex_unlock(&rtsp_lock);

  http_send_begin(hc);
  http_send_header(hc, HTTP_STATUS_OK, NULL, 0, NULL, NULL, 0, NULL, NULL, &args);
  http_send_end(hc);

  goto end;

error:
  tvh_mutex_unlock(&rtsp_lock);
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

  tvhdebug(LS_SATIPS, "-/%s/%i: teardown from %s:%d",
           hc->hc_session, stream, hc->hc_peer_ipstr, ntohs(IP_PORT(*hc->hc_peer)));

  tvh_mutex_lock(&rtsp_lock);
  rs = rtsp_find_session(hc, stream);
  if (!rs || stream != rs->stream) {
    tvh_mutex_unlock(&rtsp_lock);
    http_error(hc, !rs ? HTTP_STATUS_BAD_SESSION : HTTP_STATUS_NOT_FOUND);
  } else {
    strlcpy(session, rs->session, sizeof(session));
    rtsp_close_session(rs);
    rtsp_free_session(rs);
    tvh_mutex_unlock(&rtsp_lock);
    http_arg_init(&args);
    http_arg_set(&args, "Session", session);
    http_send_begin(hc);
    http_send_header(hc, HTTP_STATUS_OK, NULL, 0, NULL, NULL, 0, NULL, NULL, NULL);
    http_send_end(hc);
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
    return rtsp_process_play(hc, hc->hc_cmd);
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

  tvh_mutex_lock(&rtsp_lock);
  for (rs = TAILQ_FIRST(&rtsp_sessions); rs; rs = rs_next) {
    rs_next = TAILQ_NEXT(rs, link);
    if (rs->shutdown_on_close == hc) {
      rtsp_close_session(rs);
      rtsp_free_session(rs);
    } else if (rs->tcp_data == hc) {
      satip_rtp_close(rs->rtp_handle);
      rs->rtp_handle = NULL;
      rs->tcp_data = NULL;
      rs->state = STATE_SETUP;
    }
  }
  tvh_mutex_unlock(&rtsp_lock);
}

/*
 *
 */
static void
rtsp_stream_status ( void *opaque, htsmsg_t *m )
{
  http_connection_t *hc = opaque;
  char buf[128];
  const char *username;
  struct session *rs = NULL;
  htsmsg_t *c, *tcp = NULL, *udp = NULL;
  int udpport, s32;

  htsmsg_add_str(m, "type", "SAT>IP");
  if (hc->hc_proxy_ip) {
    tcp_get_str_from_ip(hc->hc_proxy_ip, buf, sizeof(buf));
    htsmsg_add_str(m, "proxy", buf);
  }
  username = http_username(hc);
  if (username)
    htsmsg_add_str(m, "user", username);

  TAILQ_FOREACH(rs, &rtsp_sessions, link) {
    if (hc->hc_session &&
        strcmp(rs->session, hc->hc_session) == 0 &&
        strcmp(rs->peer_ipstr, hc->hc_peer_ipstr) == 0 &&
        (udpport = rs->rtp_peer_port) > 0) {
      if (udpport == RTSP_TCP_DATA) {
        if (rs->tcp_data == hc) {
          s32 = htsmsg_get_s32_or_default(m, "peer_port", -1);
          if (!tcp) tcp = htsmsg_create_list();
          htsmsg_add_s32(tcp, NULL, s32);
        }
      } else {
        if (!udp) udp = htsmsg_create_list();
        htsmsg_add_s32(udp, NULL, udpport);
        htsmsg_add_s32(udp, NULL, udpport+1);
      }
    }
  }
  if (tcp || udp) {
    c = htsmsg_create_map();
    if (tcp) htsmsg_add_msg(c, "tcp", tcp);
    if (udp) htsmsg_add_msg(c, "udp", udp);
    htsmsg_add_msg(m, "peer_extra_ports", c);
  }
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
  tcp_get_str_from_ip(peer, buf + strlen(buf), sizeof(buf) - strlen(buf));
  aa.aa_representative = buf;

  tcp = tcp_connection_launch(fd, 1, rtsp_stream_status, &aa);

  /* Note: global_lock held on entry */
  tvh_mutex_unlock(&global_lock);

  hc.hc_subsys  = LS_SATIPS;
  hc.hc_fd      = fd;
  hc.hc_peer    = peer;
  hc.hc_self    = self;
  hc.hc_process = rtsp_process_request;
  hc.hc_cseq    = 1;

  http_serve_requests(&hc);

  rtsp_flush_requests(&hc);

  close(fd);

  /* Note: leave global_lock held for parent */
  tvh_mutex_lock(&global_lock);
  *opaque = NULL;

  tcp_connection_land(tcp);
}

/*
 *
 */
static void
rtsp_close_session(session_t *rs)
{
  satip_rtp_close(rs->rtp_handle);
  rs->rtp_handle = NULL;
  rs->playing = 0;
  rs->state = STATE_DESCRIBE;
  udp_close(rs->udp_rtp);
  rs->udp_rtp = NULL;
  udp_close(rs->udp_rtcp);
  rs->udp_rtcp = NULL;
  rs->rtp_udp_bound = 0;
  rs->shutdown_on_close = NULL;
  rs->tcp_data = NULL;
  tvh_mutex_lock(&global_lock);
  mpegts_pid_reset(&rs->pids);
  rtsp_clean(rs, 1);
  mtimer_disarm(&rs->timer);
  tvh_mutex_unlock(&global_lock);
}

/*
 *
 */
static void
rtsp_free_session(session_t *rs)
{
  TAILQ_REMOVE(&rtsp_sessions, rs, link);
  tvh_mutex_lock(&global_lock);
  mtimer_disarm(&rs->timer);
  tvh_mutex_unlock(&global_lock);
  mpegts_pid_done(&rs->pids);
  free(rs->peer_ipstr);
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
    tvh_mutex_lock(&rtsp_lock);
    rs = TAILQ_FIRST(&rtsp_sessions);
    if (rs) {
      rtsp_close_session(rs);
      rtsp_free_session(rs);
    }
    tvh_mutex_unlock(&rtsp_lock);
  } while (rs != NULL);
}

/*
 *
 */
void satip_server_rtsp_init
  (const char *bindaddr, int port, int descramble, int rewrite_pmt, int muxcnf,
   const char *rtp_src, const char *nat_ip, int nat_port)
{
  static tcp_server_ops_t ops = {
    .start  = rtsp_serve,
    .stop   = NULL,
    .cancel = http_cancel
  };
  int reg = 0;
  uint8_t rnd[4];
  char *s;
  if (!rtsp_server) {
    uuid_random(rnd, sizeof(rnd));
    session_number = *(uint32_t *)rnd;
    TAILQ_INIT(&rtsp_sessions);
    tvh_mutex_init(&rtsp_lock, NULL);
    satip_rtp_init(1);
  }
  if (rtsp_port != port && rtsp_server) {
    rtsp_close_sessions();
    tcp_server_delete(rtsp_server);
    rtsp_server = NULL;
    reg = 1;
  }
  s = rtsp_ip;
  if(bindaddr != NULL)
    rtsp_ip = strdup(bindaddr);
  free(s);
  rtsp_port = port;
  rtsp_descramble = descramble;
  rtsp_rewrite_pmt = rewrite_pmt;
  rtsp_muxcnf = muxcnf;
  s = rtp_src_ip;
  rtp_src_ip = rtp_src ? strdup(rtp_src) : NULL;
  free(s);
  s = rtsp_nat_ip;
  rtsp_nat_ip = nat_ip ? strdup(nat_ip) : NULL;
  free(s);
  rtsp_nat_port = nat_port;
  if (!rtsp_server)
    rtsp_server = tcp_server_create(LS_SATIPS, "SAT>IP RTSP", bindaddr, port, &ops, NULL);
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
  tvh_mutex_lock(&global_lock);
  if (rtsp_server)
    tcp_server_delete(rtsp_server);
  tvh_mutex_unlock(&global_lock);
  if (rtsp_server)
    rtsp_close_sessions();
  tvh_mutex_lock(&global_lock);
  rtsp_server = NULL;
  rtsp_port = -1;
  rtsp_nat_port = -1;
  free(rtsp_ip);
  free(rtp_src_ip);
  free(rtsp_nat_ip);
  rtsp_ip = rtp_src_ip = rtsp_nat_ip = NULL;
  satip_rtp_done();
  tvh_mutex_unlock(&global_lock);
}
