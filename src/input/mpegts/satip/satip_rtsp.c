/*
 *  Tvheadend - SAT>IP DVB RTSP client
 *
 *  Copyright (C) 2014 Jaroslav Kysela
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
#include <signal.h>
#include "tvheadend.h"
#include "htsbuf.h"
#include "tcp.h"
#include "http.h"
#include "satip_private.h"

/*
 *
 */

typedef struct tvh2satip {
  int         t; ///< TVH internal value
  const char *s; ///< SATIP API value
} tvh2satip_t;

#define TABLE_EOD -1

static const char *
satip_rtsp_setup_find(const char *prefix, tvh2satip_t *tbl,
                      int src, const char *defval)
{
  while (tbl->t >= 0) {
    if (tbl->t == src)
     return tbl->s;
    tbl++;
  }
  tvhtrace(LS_SATIP, "%s - cannot translate %d", prefix, src);
  return defval;
}

#define ADD(s, d, def) \
  strcat(buf, "&" #d "="), strcat(buf, satip_rtsp_setup_find(#d, d, dmc->s, def))

static void
satip_rtsp_add_val(const char *name, char *buf, uint32_t val)
{
  sprintf(buf + strlen(buf), "&%s=%i", name, val / 1000);
  if (val % 1000) {
    char *sec = buf + strlen(buf);
    sprintf(sec, ".%03u", val % 1000);
    if (sec[3] == '0') {
      sec[3] = '\0';
      if (sec[2] == '0')
        sec[2] = '\0';
    }
  }
}

int
satip_rtsp_setup( http_client_t *hc, int src, int fe,
                  int udp_port, const dvb_mux_conf_t *dmc, int flags,
                  int weight )
{
  static tvh2satip_t msys[] = {
    { .t = DVB_SYS_DVBT,                      "dvbt"  },
    { .t = DVB_SYS_DVBT2,                     "dvbt2" },
    { .t = DVB_SYS_DVBS,                      "dvbs"  },
    { .t = DVB_SYS_DVBS2,                     "dvbs2" },
    { .t = DVB_SYS_DVBC_ANNEX_A,              "dvbc"  },
    { .t = DVB_SYS_DVBC_ANNEX_C,              "dvbc"  },
    { .t = DVB_SYS_ATSC,                      "atsc"  },
    { .t = DVB_SYS_DVBC_ANNEX_B,              "dvbcb" },
    { .t = TABLE_EOD }
  };
  static tvh2satip_t pol[] = {
    { .t = DVB_POLARISATION_HORIZONTAL,       "h"     },
    { .t = DVB_POLARISATION_VERTICAL,         "v"     },
    { .t = DVB_POLARISATION_CIRCULAR_LEFT,    "l"     },
    { .t = DVB_POLARISATION_CIRCULAR_RIGHT,   "r"     },
    { .t = TABLE_EOD }
  };
  static tvh2satip_t ro[] = {
    { .t = DVB_ROLLOFF_AUTO,                  "0.35"  },
    { .t = DVB_ROLLOFF_20,                    "0.20"  },
    { .t = DVB_ROLLOFF_25,                    "0.25"  },
    { .t = DVB_ROLLOFF_35,                    "0.35"  },
    { .t = TABLE_EOD }
  };
  static tvh2satip_t mtype[] = {
    { .t = DVB_MOD_AUTO,                      "auto"  },
    { .t = DVB_MOD_QAM_16,                    "16qam" },
    { .t = DVB_MOD_QAM_32,                    "32qam" },
    { .t = DVB_MOD_QAM_64,                    "64qam" },
    { .t = DVB_MOD_QAM_128,                   "128qam"},
    { .t = DVB_MOD_QAM_256,                   "256qam"},
    { .t = DVB_MOD_QPSK,                      "qpsk"  },
    { .t = DVB_MOD_PSK_8,                     "8psk"  },
    { .t = TABLE_EOD }
  };
  static tvh2satip_t plts[] = {
    { .t = DVB_PILOT_AUTO,                    "auto"  },
    { .t = DVB_PILOT_ON,                      "on"    },
    { .t = DVB_PILOT_OFF,                     "off"   },
    { .t = TABLE_EOD }
  };
  static tvh2satip_t fec[] = {
    { .t = DVB_FEC_AUTO,                      "auto"  },
    { .t = DVB_FEC_1_2,                       "12"    },
    { .t = DVB_FEC_2_3,                       "23"    },
    { .t = DVB_FEC_3_4,                       "34"    },
    { .t = DVB_FEC_3_5,                       "35"    },
    { .t = DVB_FEC_4_5,                       "45"    },
    { .t = DVB_FEC_5_6,                       "56"    },
    { .t = DVB_FEC_7_8,                       "78"    },
    { .t = DVB_FEC_8_9,                       "89"    },
    { .t = DVB_FEC_9_10,                      "910"   },
    { .t = TABLE_EOD }
  };
  static tvh2satip_t tmode[] = {
    { .t = DVB_TRANSMISSION_MODE_AUTO,        "auto"  },
    { .t = DVB_TRANSMISSION_MODE_1K,          "1k"    },
    { .t = DVB_TRANSMISSION_MODE_2K,          "2k"    },
    { .t = DVB_TRANSMISSION_MODE_4K,          "4k"    },
    { .t = DVB_TRANSMISSION_MODE_8K,          "8k"    },
    { .t = DVB_TRANSMISSION_MODE_16K,         "16k"   },
    { .t = DVB_TRANSMISSION_MODE_32K,         "32k"   },
    { .t = TABLE_EOD }
  };
  static tvh2satip_t gi[] = {
    { .t = DVB_GUARD_INTERVAL_AUTO,           "auto"  },
    { .t = DVB_GUARD_INTERVAL_1_4,            "14"    },
    { .t = DVB_GUARD_INTERVAL_1_8,            "18"    },
    { .t = DVB_GUARD_INTERVAL_1_16,           "116"   },
    { .t = DVB_GUARD_INTERVAL_1_32,           "132"   },
    { .t = DVB_GUARD_INTERVAL_1_128,          "1128"  },
    { .t = DVB_GUARD_INTERVAL_19_128,         "19128" },
    { .t = DVB_GUARD_INTERVAL_19_256,         "19256" },
    { .t = TABLE_EOD }
  };

  char buf[512];
  char *stream = NULL;
  char _stream[32];

  if (src > 0)
    sprintf(buf, "src=%i&", src);
  else
    buf[0] = '\0';
  sprintf(buf + strlen(buf), "fe=%i", fe);
  if (dmc->dmc_fe_delsys == DVB_SYS_DVBS ||
      dmc->dmc_fe_delsys == DVB_SYS_DVBS2) {
    satip_rtsp_add_val("freq", buf, dmc->dmc_fe_freq);
    satip_rtsp_add_val("sr", buf, dmc->u.dmc_fe_qpsk.symbol_rate);
    ADD(dmc_fe_delsys,              msys,  "dvbs");
    if (dmc->dmc_fe_modulation != DVB_MOD_NONE &&
        dmc->dmc_fe_modulation != DVB_MOD_AUTO &&
        dmc->dmc_fe_modulation != DVB_MOD_QAM_AUTO)
      ADD(dmc_fe_modulation,        mtype, "qpsk");
    ADD(u.dmc_fe_qpsk.polarisation, pol,   "h"   );
    if (dmc->u.dmc_fe_qpsk.fec_inner != DVB_FEC_NONE &&
        dmc->u.dmc_fe_qpsk.fec_inner != DVB_FEC_AUTO)
      ADD(u.dmc_fe_qpsk.fec_inner,  fec,   "auto");
    if (dmc->dmc_fe_rolloff != DVB_ROLLOFF_NONE &&
        dmc->dmc_fe_rolloff != DVB_ROLLOFF_AUTO)
      ADD(dmc_fe_rolloff,           ro,    "0.35");
    if (dmc->dmc_fe_pilot != DVB_PILOT_NONE &&
        dmc->dmc_fe_pilot != DVB_PILOT_AUTO) {
      ADD(dmc_fe_pilot,             plts,  "auto");
    } else if ((flags & SATIP_SETUP_PILOT_ON) != 0 &&
               dmc->dmc_fe_delsys == DVB_SYS_DVBS2) {
      strcat(buf, "&plts=on");
    }
  } else if (dmc->dmc_fe_delsys == DVB_SYS_DVBC_ANNEX_A ||
             dmc->dmc_fe_delsys == DVB_SYS_DVBC_ANNEX_B ||
             dmc->dmc_fe_delsys == DVB_SYS_DVBC_ANNEX_C) {
    satip_rtsp_add_val("freq", buf, dmc->dmc_fe_freq / 1000);
    satip_rtsp_add_val("sr", buf, dmc->u.dmc_fe_qam.symbol_rate);
    ADD(dmc_fe_delsys,              msys,  "dvbc");
    if (dmc->dmc_fe_modulation != DVB_MOD_AUTO &&
        dmc->dmc_fe_modulation != DVB_MOD_NONE &&
        dmc->dmc_fe_modulation != DVB_MOD_QAM_AUTO)
      ADD(dmc_fe_modulation,          mtype, "64qam");
    /* missing plp */
    if (dmc->u.dmc_fe_qam.fec_inner != DVB_FEC_NONE &&
        dmc->u.dmc_fe_qam.fec_inner != DVB_FEC_AUTO)
      /* note: OctopusNet device does not handle 'fec=auto' */
      ADD(u.dmc_fe_qam.fec_inner,   fec,   "auto");
  } else if (dmc->dmc_fe_delsys == DVB_SYS_DVBT ||
             dmc->dmc_fe_delsys == DVB_SYS_DVBT2) {
    satip_rtsp_add_val("freq", buf, dmc->dmc_fe_freq / 1000);
    if (dmc->u.dmc_fe_ofdm.bandwidth != DVB_BANDWIDTH_AUTO &&
        dmc->u.dmc_fe_ofdm.bandwidth != DVB_BANDWIDTH_NONE)
      satip_rtsp_add_val("bw", buf, dmc->u.dmc_fe_ofdm.bandwidth);
    ADD(dmc_fe_delsys, msys, "dvbt");
    if (dmc->dmc_fe_modulation != DVB_MOD_AUTO &&
        dmc->dmc_fe_modulation != DVB_MOD_NONE &&
        dmc->dmc_fe_modulation != DVB_MOD_QAM_AUTO)
      ADD(dmc_fe_modulation, mtype, "64qam");
    if (dmc->u.dmc_fe_ofdm.transmission_mode != DVB_TRANSMISSION_MODE_AUTO &&
        dmc->u.dmc_fe_ofdm.transmission_mode != DVB_TRANSMISSION_MODE_NONE)
      ADD(u.dmc_fe_ofdm.transmission_mode, tmode, "8k");
    if (dmc->u.dmc_fe_ofdm.guard_interval != DVB_GUARD_INTERVAL_AUTO &&
        dmc->u.dmc_fe_ofdm.guard_interval != DVB_GUARD_INTERVAL_NONE)
      ADD(u.dmc_fe_ofdm.guard_interval, gi, "18");
    if (dmc->dmc_fe_delsys == DVB_SYS_DVBT2)
      if (dmc->dmc_fe_stream_id != DVB_NO_STREAM_ID_FILTER)
        satip_rtsp_add_val("pls", buf, (dmc->dmc_fe_stream_id & 0xff) * 1000);
  } else if (dmc->dmc_fe_delsys == DVB_SYS_ATSC ||
             dmc->dmc_fe_delsys == DVB_SYS_DVBC_ANNEX_B) {
    satip_rtsp_add_val("freq", buf, dmc->dmc_fe_freq / 1000);
    if (dmc->dmc_fe_modulation != DVB_MOD_AUTO &&
        dmc->dmc_fe_modulation != DVB_MOD_NONE &&
        dmc->dmc_fe_modulation != DVB_MOD_QAM_AUTO)
      ADD(dmc_fe_modulation, mtype,
          dmc->dmc_fe_delsys == DVB_SYS_ATSC ? "8vsb" : "64qam");
  }
  if (weight > 0)
    satip_rtsp_add_val("tvhweight", buf, (uint32_t)weight * 1000);
  if (flags & SATIP_SETUP_PIDS0) {
    strcat(buf, "&pids=0");
    if (flags & SATIP_SETUP_PIDS21)
      strcat(buf, ",21");
  } else if (flags & SATIP_SETUP_PIDS21)
    strcat(buf, "&pids=21");
  tvhtrace(LS_SATIP, "setup params - %s", buf);
  if (hc->hc_rtsp_stream_id >= 0)
    snprintf(stream = _stream, sizeof(_stream), "/stream=%li",
             hc->hc_rtsp_stream_id);
  if (flags & SATIP_SETUP_PLAY)
    return rtsp_play(hc, stream, buf);
  if (flags & SATIP_SETUP_TCP)
    return rtsp_setup(hc, stream, buf, NULL, 0, -1);
  return rtsp_setup(hc, stream, buf, NULL, udp_port, udp_port + 1);
}

static const char *
satip_rtsp_pids_strip( const char *s, int maxlen )
{
  char *ptr;

  if (s == NULL)
    return NULL;
  while (*s == ',')
    s++;
  while (strlen(s) > maxlen) {
    ptr = strrchr(s, ',');
    if (ptr == NULL)
      abort();
    *ptr = '\0';
  }
  if (*s == '\0')
    return NULL;
  return s;
}

int
satip_rtsp_play( http_client_t *hc, const char *pids,
                 const char *addpids, const char *delpids,
                 int max_pids_len, int weight )
{
  htsbuf_queue_t q;
  char *stream = NULL;
  char _stream[32];
  char *query;
  int r, split = 0;

  pids    = satip_rtsp_pids_strip(pids   , max_pids_len);
  addpids = satip_rtsp_pids_strip(addpids, max_pids_len);
  delpids = satip_rtsp_pids_strip(delpids, max_pids_len);

  if (pids == NULL && addpids == NULL && delpids == NULL && weight <= 0)
    return -EINVAL;

  //printf("pids = '%s' addpids = '%s' delpids = '%s'\n", pids, addpids, delpids);

  htsbuf_queue_init(&q, 0);
  /* pids setup and add/del requests cannot be mixed per specification */
  if (pids) {
    htsbuf_qprintf(&q, "pids=%s", pids);
  } else {
    if (delpids)
      htsbuf_qprintf(&q, "delpids=%s", delpids);
    if (addpids) {
      if (delpids) {
        /* try to maintain the maximum request size - simple split */
        if (strlen(addpids) + strlen(delpids) >= max_pids_len)
          split = 1;
        else
          htsbuf_append(&q, "&", 1);
      }
      if (!split)
        htsbuf_qprintf(&q, "addpids=%s", addpids);
    }
  }
  if (weight)
    htsbuf_qprintf(&q, "%stvhweight=%d", htsbuf_empty(&q) ? "" : "&", weight);
  if (hc->hc_rtsp_stream_id >= 0)
    snprintf(stream = _stream, sizeof(_stream), "/stream=%li",
             hc->hc_rtsp_stream_id);
  query = htsbuf_to_string(&q);
  r = rtsp_play(hc, stream, query);
  free(query);
  if (r >= 0 && split) {
    htsbuf_queue_init(&q, 0);
    htsbuf_qprintf(&q, "addpids=%s", addpids);
    query = htsbuf_to_string(&q);
    r = rtsp_play(hc, stream, query);
    free(query);
  }
  return r;
}
