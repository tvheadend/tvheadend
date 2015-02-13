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
#include "input.h"
#include "htsbuf.h"
#include "htsmsg_xml.h"
#include "upnp.h"
#include "http.h"
#include "settings.h"
#include "config.h"
#include "satip/server.h"

#include <ctype.h>
#include <arpa/inet.h>
#include <openssl/sha.h>

#if defined(PLATFORM_FREEBSD) || ENABLE_ANDROID
#include <sys/types.h>
#include <sys/socket.h>
#endif

#define RTSP_TIMEOUT 30
#define RTSP_PIDS    128

typedef struct session {
  TAILQ_ENTRY(session) link;
  int delsys;
  int stream;
  int frontend;
  char session[9];
  dvb_mux_conf_t dmc;
  int16_t pids[RTSP_PIDS];
  gtimer_t timer;
} session_t;

static uint32_t session_number;
static char *rtsp_ip = NULL;
static int rtsp_port = -1;
static void *rtsp_server = NULL;
static TAILQ_HEAD(,session) rtsp_sessions;
static pthread_mutex_t rtsp_lock;

static void rtsp_close_session(session_t *rs);
static void rtsp_free_session(session_t *rs);


/*
 *
 */
static int
rtsp_delsys(int fe)
{
  int i;

  if (fe < 1)
    return DVB_SYS_NONE;
  pthread_mutex_lock(&global_lock);
  i = config_get_int("satip_dvbt", 0);
  if (fe <= i)
    return DVB_SYS_DVBT;
  fe -= i;
  i = config_get_int("satip_dvbs", 0);
  if (fe <= i)
    return DVB_SYS_DVBS;
  fe -= i;
  i = config_get_int("satip_dvbc", 0);
  if (fe <= i)
    return DVB_SYS_DVBC_ANNEX_A;
  pthread_mutex_unlock(&global_lock);
  return DVB_SYS_NONE;
}

/*
 *
 */
static struct session *
rtsp_new_session(int delsys)
{
  struct session *rs = calloc(1, sizeof(*rs));
  if (rs == NULL)
    return NULL;
  rs->delsys = delsys;
  snprintf(rs->session, sizeof(rs->session), "%08X", session_number);
  session_number += 9876;
  return rs;
}

/*
 *
 */
static struct session *
rtsp_find_session(http_connection_t *hc)
{
  struct session *rs;

  if (hc->hc_session == NULL)
    return NULL;
  TAILQ_FOREACH(rs, &rtsp_sessions, link)
    if (!strcmp(rs->session, hc->hc_session))
      return rs;
  return NULL;
}

/*
 *
 */
static void
rtsp_session_timer_cb(void *aux)
{
  session_t *rs = aux;
  
  rtsp_close_session(rs);
  rtsp_free_session(rs);
  tvhwarn("satips", "session %s closed (timeout)", rs->session);
}

static inline void
rtsp_rearm_session_timer(session_t *rs)
{
  gtimer_arm(&rs->timer, rtsp_session_timer_cb, rs, RTSP_TIMEOUT);
}

/*
 *
 */
static char *
rtsp_check_urlbase(char *u)
{
  char *p;

  /* expect string: rtsp://<myip>[:<myport>]/ */
  if (u[0] == '\0' || strncmp(u, "rtsp://", 7))
    return NULL;
  u += 7;
  p = strchr(u, '/');
  if (p == NULL)
    return NULL;
  *p = '\0';
  if ((p = strchr(u, ':')) != NULL) {
    *p = '\0';
    if (atoi(p + 1) != rtsp_port)
      return NULL;
  } else {
    if (rtsp_port != 554)
      return NULL;
  }
  if (strcmp(u, rtsp_ip))
    return NULL;
  return p;
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
    for (s = 0; isdigit(*s); s++);
    if (*s != '?')
      return -1;
    *s = '\0';
    stream = atoi(u);
    u = s + 1;
  } else {
    if (*u != '?')
      return -1;
  }
  http_parse_get_args(hc, u);
  return stream;
}

/*
 *
 */
static int
rtsp_addpids(session_t *rs, int16_t *pids)
{
  int pid, i, j;

  while ((pid = *pids++) >= 0) {
    for (i = 0; i < RTSP_PIDS; i++) {
      if (rs->pids[i] > pid) {
        if (rs->pids[RTSP_PIDS-1] >= 0)
          return -1;
        for (j = RTSP_PIDS-1; j != i; j--)
          rs->pids[j] = rs->pids[j-1];
        rs->pids[i] = pid;
        break;
      } else if (rs->pids[i] == pid)
        break;
    }
  }
  return 0;
}

/*
 *
 */
static int
rtsp_delpids(session_t *rs, int16_t *pids)
{
  int pid, i, j;
  
  while ((pid = *pids++) >= 0) {
    for (i = 0; i < RTSP_PIDS; i++) {
      if (rs->pids[i] > pid)
        break;
      else if (rs->pids[i] == pid) {
        for (j = i; rs->pids[j] >= 0 && j + 1 < RTSP_PIDS; j++)
          rs->pids[j] = rs->pids[j+1];
        rs->pids[RTSP_PIDS-1] = -1;
        break;
      }
    }
  }
  return 0;
}

/*
 *
 */
static int
rtsp_start(session_t *rs)
{
  return 0;
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

  if ((u = rtsp_check_urlbase(u)) == NULL)
    goto error;
  if (*u)
    goto error;

  pthread_mutex_lock(&rtsp_lock);
  rs = rtsp_find_session(hc);
  if (rs)
    rtsp_rearm_session_timer(rs);
  pthread_mutex_unlock(&rtsp_lock);
  http_arg_init(&args);
  http_arg_set(&args, "Public", "OPTIONS,DESCRIBE,SETUP,PLAY,TEARDOWN");
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
  if (s[0] == '\0')
    return DVB_PILOT_AUTO;
  return DVB_ROLLOFF_NONE;
}

static int
tmode_to_tvh(http_connection_t *hc)
{
  static struct strtab tab[] = {
    { "1k",  DVB_TRANSMISSION_MODE_1K },
    { "2k",  DVB_TRANSMISSION_MODE_2K },
    { "4k",  DVB_TRANSMISSION_MODE_4K },
    { "8k",  DVB_TRANSMISSION_MODE_8K },
    { "16k", DVB_TRANSMISSION_MODE_16K },
    { "32k", DVB_TRANSMISSION_MODE_32K },
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
    { "qpsk",   DVB_MOD_QPSK },
    { "8psk",   DVB_MOD_PSK_8 },
    { "16qam",  DVB_MOD_QAM_16 },
    { "32qam",  DVB_MOD_QAM_32 },
    { "64qam",  DVB_MOD_QAM_64 },
    { "128qam", DVB_MOD_QAM_128 },
    { "256qam", DVB_MOD_QAM_256 },
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
parse_pids(char *p, int16_t *pids)
{
  char *x, *saveptr;
  int i = 0;

  if (p == '\0') {
    pids[0] = -1;
    return 0;
  }
  x = strtok_r(p, ",", &saveptr);
  while (1) {
    if (x == NULL)
      break;
    if (i >= RTSP_PIDS)
      return -1;
    pids[i] = atoi(x);
    if (pids[i] < 0 || pids[i] > 8191)
      return -1;
    x = strtok_r(NULL, ",", &saveptr);
  }
  if (i == 0)
    return -1;
  pids[i] = -1;
  return 0;
}

/*
 *
 */
static int
rtsp_process_play(http_connection_t *hc, int setup)
{
  session_t *rs;
  int errcode = HTTP_STATUS_BAD_REQUEST;
  int stream, delsys = DVB_SYS_NONE, msys, fe, src, freq, pol, sr;
  int fec, ro, plts, bw, tmode, mtype, gi, plp, t2id, sm, c2tft, ds, specinv;
  char *u, *s;
  char *pids, *addpids, *delpids;
  int16_t _pids[RTSP_PIDS+1], _addpids[RTSP_PIDS+1], _delpids[RTSP_PIDS+1];
  dvb_mux_conf_t *dmc;

  u = tvh_strdupa(hc->hc_url);
  if ((u = rtsp_check_urlbase(u)) == NULL ||
      (stream = rtsp_parse_args(hc, u)) < 0)
    goto error2;

  fe = atoi(http_arg_get_remove(&hc->hc_req_args, "fe"));
  addpids = http_arg_get_remove(&hc->hc_req_args, "addpids");
  if (parse_pids(addpids, _addpids)) goto error2;
  delpids = http_arg_get_remove(&hc->hc_req_args, "delpids");
  if (parse_pids(delpids, _delpids)) goto error2;
  msys = msys_to_tvh(hc);
  if (msys < 0)
    goto error2;

  if (addpids || delpids) {
    if (setup)
      goto error2;
    if (!stream)
      goto error2;
  }

  pthread_mutex_lock(&rtsp_lock);

  rs = rtsp_find_session(hc);

  if (fe > 0) {
    delsys = rtsp_delsys(fe);
    if (delsys == DVB_SYS_NONE)
      goto error;
  }

  if (setup) {
    if (msys == DVB_SYS_NONE)
      goto error;
    if (!rs)
      rs = rtsp_new_session(msys);
    else
      rtsp_close_session(rs);
  } else {
    if (!rs || stream != rs->stream) {
      if (rs)
        errcode = HTTP_STATUS_NOT_FOUND;
      goto error;
    }
  }

  if (!setup && rs->frontend == fe && TAILQ_EMPTY(&hc->hc_req_args))
    goto play;

  dmc = &rs->dmc;
  dvb_mux_conf_init(dmc, msys);
  rs->frontend = fe;

  pids = http_arg_get_remove(&hc->hc_req_args, "pids");
  if (parse_pids(pids, _pids)) goto error;
  freq = atof(http_arg_get_remove(&hc->hc_req_args, "freq")) * 1000;
  if (freq < 1000) goto error;
  mtype = mtype_to_tvh(hc);
  if (mtype == DVB_MOD_NONE) goto error;

  if (msys == DVB_SYS_DVBS || msys == DVB_SYS_DVBS2) {

    src = atoi(http_arg_get_remove(&hc->hc_req_args, "src"));
    if (src < 1) goto error;
    pol = pol_to_tvh(hc);
    if (pol < 0) goto error;
    sr = atof(http_arg_get_remove(&hc->hc_req_args, "sr")) * 1000;
    if (sr < 1000) goto error;
    fec = fec_to_tvh(hc);
    if (fec == DVB_FEC_NONE) goto error;
    ro = rolloff_to_tvh(hc);
    if (ro == DVB_ROLLOFF_NONE ||
        (ro != DVB_ROLLOFF_35 && msys == DVB_SYS_DVBS)) goto error;
    plts = pilot_to_tvh(hc);
    if (plts == DVB_PILOT_NONE) goto error;

    if (!TAILQ_EMPTY(&hc->hc_req_args))
      goto error;

    dmc->dmc_fe_rolloff = ro;
    dmc->dmc_fe_pilot = plts;
    dmc->u.dmc_fe_qpsk.polarisation = pol;
    dmc->u.dmc_fe_qpsk.symbol_rate = sr;
    dmc->u.dmc_fe_qpsk.fec_inner = fec;

  } else if (msys == DVB_SYS_DVBT || msys == DVB_SYS_DVBT2) {

    bw = bw_to_tvh(hc);
    if (bw == DVB_BANDWIDTH_NONE) goto error;
    tmode = tmode_to_tvh(hc);
    if (tmode == DVB_TRANSMISSION_MODE_NONE) goto error;
    gi = gi_to_tvh(hc);
    if (gi == DVB_GUARD_INTERVAL_NONE) goto error;
    fec = fec_to_tvh(hc);
    if (fec == DVB_FEC_NONE) goto error;
    plp = atoi(http_arg_get_remove(&hc->hc_req_args, "plp"));
    if (plp < 0 || plp > 255) goto error;
    s = http_arg_get_remove(&hc->hc_req_args, "t2id");
    t2id = s[0] ? atoi(s) : DVB_NO_STREAM_ID_FILTER;
    if (t2id < 0 || t2id > 65535) goto error;
    sm = atoi(http_arg_get_remove(&hc->hc_req_args, "sm"));
    if (sm < 0 || sm > 1) goto error;

    if (!TAILQ_EMPTY(&hc->hc_req_args))
      goto error;

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

    c2tft = atoi(http_arg_get_remove(&hc->hc_req_args, "c2tft"));
    if (c2tft < 0 || c2tft > 2) goto error;
    bw = bw_to_tvh(hc);
    if (bw == DVB_BANDWIDTH_NONE) goto error;
    sr = atof(http_arg_get_remove(&hc->hc_req_args, "sr")) * 1000;
    if (sr < 1000) goto error;
    ds = atoi(http_arg_get_remove(&hc->hc_req_args, "ds"));
    if (ds < 0 || ds > 255) goto error;
    plp = atoi(http_arg_get_remove(&hc->hc_req_args, "plp"));
    if (plp < 0 || plp > 255) goto error;
    specinv = atoi(http_arg_get_remove(&hc->hc_req_args, "specinv"));
    if (specinv < 0 || specinv > 1) goto error;

    if (!TAILQ_EMPTY(&hc->hc_req_args))
      goto error;

    dmc->u.dmc_fe_qpsk.symbol_rate = sr;
    dmc->u.dmc_fe_qpsk.fec_inner = DVB_FEC_NONE;
    dmc->dmc_fe_inversion = specinv;
    dmc->dmc_fe_stream_id = plp;
    dmc->dmc_fe_pls_code = ds; /* check */

  } else {

    goto error;

  }

  dmc->dmc_fe_freq = freq;
  dmc->dmc_fe_modulation = mtype;

  if (setup) {
    if (pids)
      rtsp_addpids(rs, _pids);
    goto end;
  }

play:
  if (delpids)
    rtsp_delpids(rs, _delpids);
  if (addpids)
    rtsp_addpids(rs, _addpids);
  if (rtsp_start(rs) < 0) {
    errcode = HTTP_STATUS_SERVICE;;
    goto error;
  }

end:
  pthread_mutex_unlock(&rtsp_lock);
  return 0;

error:
  pthread_mutex_unlock(&rtsp_lock);
error2:
  http_error(hc, errcode);
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
  int stream;

  if ((u = rtsp_check_urlbase(u)) == NULL ||
      (stream = rtsp_parse_args(hc, u)) < 0) {
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    return 0;
  }

  pthread_mutex_lock(&rtsp_lock);
  rs = rtsp_find_session(hc);
  if (!rs || stream != rs->stream) {
    pthread_mutex_unlock(&rtsp_lock);
    http_error(hc, !rs ? HTTP_STATUS_BAD_SESSION : HTTP_STATUS_NOT_FOUND);
  } else {
    rtsp_close_session(rs);
    pthread_mutex_unlock(&rtsp_lock);
    rtsp_free_session(rs);
    http_send_header(hc, HTTP_STATUS_OK, NULL, 0, NULL, NULL, 0, NULL, NULL, NULL);
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
rtsp_serve(int fd, void **opaque, struct sockaddr_storage *peer,
           struct sockaddr_storage *self)
{
  http_connection_t hc;

  memset(&hc, 0, sizeof(http_connection_t));
  *opaque = &hc;
  /* Note: global_lock held on entry */
  pthread_mutex_unlock(&global_lock);

  hc.hc_fd      = fd;
  hc.hc_peer    = peer;
  hc.hc_self    = self;
  hc.hc_process = rtsp_process_request;
  hc.hc_cseq    = 1;

  http_serve_requests(&hc);

  close(fd);

  /* Note: leave global_lock held for parent */
  pthread_mutex_lock(&global_lock);
  *opaque = NULL;
}

/*
 *
 */
static void
rtsp_close_session(session_t *rs)
{
  gtimer_disarm(&rs->timer);
}

/*
 *
 */
static void
rtsp_free_session(session_t *rs)
{
  gtimer_disarm(&rs->timer);
  TAILQ_REMOVE(&rtsp_sessions, rs, link);
  free(rs);
}

/*
 *
 */
static void
rtsp_close_sessions(void)
{
  session_t *rs;
  while ((rs = TAILQ_FIRST(&rtsp_sessions)) != NULL) {
    rtsp_close_session(rs);
    rtsp_free_session(rs);
  }
}

/*
 *
 */
void satip_server_rtsp_init(const char *bindaddr, int port)
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
  }
  if (rtsp_port != port && rtsp_server) {
    pthread_mutex_lock(&rtsp_lock);
    rtsp_close_sessions();
    pthread_mutex_unlock(&rtsp_lock);
    tcp_server_delete(rtsp_server);
    reg = 1;
  }
  free(rtsp_ip);
  rtsp_ip = strdup(bindaddr);
  rtsp_port = port;
  rtsp_server = tcp_server_create(bindaddr, port, &ops, NULL);
  if (reg)
    tcp_server_register(rtsp_server);
}

void satip_server_rtsp_register(void)
{
  tcp_server_register(rtsp_server);
}

void satip_server_rtsp_done(void)
{
  pthread_mutex_lock(&global_lock);
  rtsp_close_sessions();
  if (rtsp_server)
    tcp_server_delete(rtsp_server);
  rtsp_server = NULL;
  rtsp_port = -1;
  free(rtsp_ip);
  rtsp_ip = NULL;
  pthread_mutex_unlock(&global_lock);
}
