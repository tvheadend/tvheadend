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
satip_rtsp_connection_t *
satip_rtsp_connection( satip_device_t *sd )
{
  satip_rtsp_connection_t *conn;
  char errbuf[256];

  conn = calloc(1, sizeof(satip_rtsp_connection_t));
  htsbuf_queue_init(&conn->wq2, 0);
  conn->port = 554;
  conn->timeout = 60;
  conn->fd = tcp_connect(sd->sd_info.addr, conn->port,
                         errbuf, sizeof(errbuf), 2);
  if (conn->fd < 0) {
    tvhlog(LOG_ERR, "satip", "RTSP - unable to connect - %s", errbuf);
    free(conn);
    return NULL;
  }
  conn->device = sd;
  conn->ping_time = dispatch_clock;
  return conn;
}

void
satip_rtsp_connection_close( satip_rtsp_connection_t *conn )
{
  
  htsbuf_queue_flush(&conn->wq2);
  free(conn->session);
  free(conn->header);
  free(conn->data);
  free(conn->wbuf);
  if (conn->fd > 0)
    close(conn->fd);
  free(conn);
}

int
satip_rtsp_send_partial( satip_rtsp_connection_t *conn )
{
  ssize_t r;

  conn->sending = 1;
  while (1) {
    r = send(conn->fd, conn->wbuf + conn->wpos, conn->wsize - conn->wpos, MSG_DONTWAIT);
    if (r < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
        continue;
      return -errno;
    }
    conn->wpos += r;
    if (conn->wpos >= conn->wsize) {
      conn->sending = 0;
      return 1;
    }
    break;
  }
  return 0;
}

int
satip_rtsp_send( satip_rtsp_connection_t *conn, htsbuf_queue_t *q,
                 satip_rtsp_cmd_t cmd )
{
  int r;

  conn->ping_time = dispatch_clock;
  conn->cmd = cmd;
  free(conn->wbuf);
  htsbuf_qprintf(q, "CSeq: %i\r\n\r\n", ++conn->cseq);
  conn->wbuf    = htsbuf_to_string(q);
  conn->wsize   = strlen(conn->wbuf);
  conn->wpos    = 0;
#if ENABLE_TRACE
  tvhtrace("satip", "%s - sending RTSP cmd", conn->device->sd_info.addr);
  tvhlog_hexdump("satip", conn->wbuf, conn->wsize);
#endif
  while (!(r = satip_rtsp_send_partial(conn))) ;
  return r;
}

static int
satip_rtsp_send2( satip_rtsp_connection_t *conn, htsbuf_queue_t *q,
                  satip_rtsp_cmd_t cmd )
{
  conn->wq2_loaded = 1;
  conn->wq2_cmd = cmd;
  htsbuf_appendq(&conn->wq2, q);
  return 1;
}

int
satip_rtsp_receive( satip_rtsp_connection_t *conn )
{
  char buf[1024], *saveptr, *argv[3], *d, *p, *p1;
  htsbuf_queue_t header, data;
  int cseq_seen;
  ssize_t r;

  r = recv(conn->fd, buf, sizeof(buf), MSG_DONTWAIT);
  if (r == 0)
    return -ESTRPIPE;
  if (r < 0) {
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
      return 0;
    return -errno;
  }
#if ENABLE_TRACE
  if (r > 0) {
    tvhtrace("satip", "%s - received RTSP answer", conn->device->sd_info.addr);
    tvhlog_hexdump("satip", buf, r);
  }
#endif
  if (r + conn->rsize >= sizeof(conn->rbuf))
    return -EINVAL;
  memcpy(conn->rbuf + conn->rsize, buf, r);
  conn->rsize += r;
  conn->rbuf[conn->rsize] = '\0';
  if (conn->rsize > 3 &&
      (d = strstr(conn->rbuf, "\r\n\r\n")) != NULL) {
    *d = '\0';
    htsbuf_queue_init(&header, 0);
    htsbuf_queue_init(&data, 0);
    p = strtok_r(conn->rbuf, "\r\n", &saveptr);
    if (p == NULL)
      goto fail;
    tvhtrace("satip", "%s - RTSP answer '%s'", conn->device->sd_info.addr, p);
    if (http_tokenize(p, argv, 3, -1) != 3)
      goto fail;
    if (strcmp(argv[0], "RTSP/1.0"))
      goto fail;
    if ((conn->code = atoi(argv[1])) <= 0)
      goto fail;
    cseq_seen = 0;
    while ((p = strtok_r(NULL, "\r\n", &saveptr)) != NULL) {
      p1 = strdup(p);
      if (http_tokenize(p, argv, 2, ':') != 2)
        goto fail;
      if (strcmp(argv[0], "CSeq") == 0) {
        cseq_seen = conn->cseq == atoi(argv[1]);
      } else {
        htsbuf_append(&header, p1, strlen(p1));
        htsbuf_append(&header, "\n", 1);
      }
      free(p1);
    }
    if (!cseq_seen)
      goto fail;
    free(conn->header);
    free(conn->data);
    conn->header = htsbuf_to_string(&header);
    conn->data   = htsbuf_to_string(&data);
#if ENABLE_TRACE
    tvhtrace("satip", "%s - received RTSP header", conn->device->sd_info.addr);
    tvhlog_hexdump("satip", conn->header, strlen(conn->header));
    if (strlen(conn->data)) {
      tvhtrace("satip", "%s - received RTSP data", conn->device->sd_info.addr);
      tvhlog_hexdump("satip", conn->data, strlen(conn->data));
    }
#endif
    htsbuf_queue_flush(&header);
    htsbuf_queue_flush(&data);
    conn->rsize = 0;
    /* second write */
    if (conn->wq2_loaded && conn->code == 200) {
      r =  satip_rtsp_send(conn, &conn->wq2, conn->wq2_cmd);
      htsbuf_queue_flush(&conn->wq2);
      conn->wq2_loaded = 0;
      return r;
    }
    return 1;
fail:
    htsbuf_queue_flush(&header);
    htsbuf_queue_flush(&data);
    conn->rsize = 0;
    return -EINVAL;
  }
  /* unfinished */
  return 0;
}

/*
 *
 */

int
satip_rtsp_options_decode( satip_rtsp_connection_t *conn )
{
  char *argv[32], *s, *saveptr;
  int i, n, what = 0;
  
  s = strtok_r(conn->header, "\n", &saveptr);
  while (s) {
    n = http_tokenize(s, argv, 32, ',');
    if (strcmp(argv[0], "Public:") == 0)
      for (i = 1; i < n; i++) {
        if (strcmp(argv[i], "DESCRIBE") == 0)
          what |= 1;
        else if (strcmp(argv[i], "SETUP") == 0)
          what |= 2;
        else if (strcmp(argv[i], "PLAY") == 0)
          what |= 4;
        else if (strcmp(argv[i], "TEARDOWN") == 0)
          what |= 8;
      }
    s = strtok_r(NULL, "\n", &saveptr);
  }
  return (conn->code != 200 && what != 0x0f) ? -1 : 1;
}

void
satip_rtsp_options( satip_rtsp_connection_t *conn )
{
  htsbuf_queue_t q;
  htsbuf_queue_init(&q, 0);
  htsbuf_qprintf(&q,
           "OPTIONS rtsp://%s/ RTSP/1.0\r\n",
            conn->device->sd_info.addr);
  satip_rtsp_send(conn, &q, SATIP_RTSP_CMD_OPTIONS);
  htsbuf_queue_flush(&q);
}

int
satip_rtsp_setup_decode( satip_rtsp_connection_t *conn )
{
  char *argv[32], *s, *saveptr;
  int i, n;

  if (conn->code >= 400)
    return -1;
  if (conn->code != 200)
    return 0;
  conn->client_port = 0;
  s = strtok_r(conn->header, "\n", &saveptr);
  while (s) {
    n = http_tokenize(s, argv, 32, ';');
    if (strcmp(argv[0], "Session:") == 0) {
      conn->session = strdup(argv[1]);
      for (i = 2; i < n; i++) {
        if (strncmp(argv[i], "timeout=", 8) == 0) {
          conn->timeout = atoi(argv[i] + 8);
          if (conn->timeout <= 20 || conn->timeout > 3600)
            return -1;
        }
      }
    } else if (strcmp(argv[0], "com.ses.streamID:") == 0) {
      conn->stream_id = atoll(argv[1]);
      /* zero is valid stream id per specification */
      if (argv[1][0] == '0' && argv[1][0] == '\0')
        conn->stream_id = 0;
      else if (conn->stream_id <= 0)
        return -1;
    } else if (strcmp(argv[0], "Transport:") == 0) {
      if (strcmp(argv[1], "RTP/AVP"))
        return -1;
      if (strcmp(argv[2], "unicast"))
        return -1;
      for (i = 2; i < n; i++) {
        if (strncmp(argv[i], "client_port=", 12) == 0)
          conn->client_port = atoi(argv[i] + 12);
      }
    }
    s = strtok_r(NULL, "\n", &saveptr);
  }
  return 1;
}

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
  tvhtrace("satip", "%s - cannot translate %d", prefix, src);
  return defval;
}

#define ADD(s, d, def) \
  strcat(buf, "&" #d "="), strcat(buf, satip_rtsp_setup_find(#d, d, dmc->s, def))

static void
satip_rtsp_add_val(const char *name, char *buf, uint32_t val)
{
  char sec[4];

  sprintf(buf + strlen(buf), "&%s=%i", name, val / 1000);
  if (val % 1000) {
    sprintf(sec, ".%03i", val % 1000);
    if (sec[3] == '0') {
      sec[3] = '\0';
      if (sec[2] == '0')
        sec[2] = '\0';
    }
  }
}

int
satip_rtsp_setup( satip_rtsp_connection_t *conn, int src, int fe,
                  int udp_port, const dvb_mux_conf_t *dmc,
                  int connection_close )
{
  static tvh2satip_t msys[] = {
    { .t = DVB_SYS_DVBT,                      "dvbt"  },
    { .t = DVB_SYS_DVBT2,                     "dvbt2" },
    { .t = DVB_SYS_DVBS,                      "dvbs"  },
    { .t = DVB_SYS_DVBS2,                     "dvbs2" },
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
  htsbuf_queue_t q;
  int r;

  htsbuf_queue_init(&q, 0);
  if (src > 0)
    sprintf(buf, "src=%i&", src);
  else
    buf[0] = '\0';
  sprintf(buf + strlen(buf), "fe=%i", fe);
  satip_rtsp_add_val("freq", buf, dmc->dmc_fe_freq);
  if (dmc->dmc_fe_delsys == DVB_SYS_DVBS ||
      dmc->dmc_fe_delsys == DVB_SYS_DVBS2) {
    satip_rtsp_add_val("sr",   buf, dmc->u.dmc_fe_qpsk.symbol_rate);
    ADD(dmc_fe_delsys,              msys,  "dvbs");
    ADD(dmc_fe_modulation,          mtype, "qpsk");
    ADD(u.dmc_fe_qpsk.polarisation, pol,   "h"   );
    ADD(u.dmc_fe_qpsk.fec_inner,    fec,   "auto");
    ADD(dmc_fe_rolloff,             ro,    "0.35");
    if (dmc->dmc_fe_pilot != DVB_PILOT_AUTO)
      ADD(dmc_fe_pilot,             plts,  "auto");
  } else {
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
  }
  tvhtrace("satip", "setup params - %s", buf);
  if (conn->stream_id > 0)
    htsbuf_qprintf(&q, "SETUP rtsp://%s/stream=%li?",
                   conn->device->sd_info.addr, conn->stream_id);
  else
    htsbuf_qprintf(&q, "SETUP rtsp://%s/?", conn->device->sd_info.addr);
  htsbuf_qprintf(&q,
      "%s RTSP/1.0\r\nTransport: RTP/AVP;unicast;client_port=%i-%i\r\n",
      buf, udp_port, udp_port+1);
  if (conn->session)
    htsbuf_qprintf(&q, "Session: %s\r\n", conn->session);
  if (connection_close)
    htsbuf_qprintf(&q, "Connection: close\r\n");
  r = satip_rtsp_send(conn, &q, SATIP_RTSP_CMD_SETUP);
  htsbuf_queue_flush(&q);
  return r;
}

static const char *
satip_rtsp_pids_strip( satip_rtsp_connection_t *conn, const char *s )
{
  int maxlen = conn->device->sd_pids_len;
  char *ptr;

  if (s == NULL)
    return NULL;
  while (*s == ',')
    s++;
  while (strlen(s) > maxlen) {
    ptr = rindex(s, ',');
    if (ptr == NULL)
      abort();
    *ptr = '\0';
  }
  if (*s == '\0')
    return NULL;
  return s;
}

int
satip_rtsp_play( satip_rtsp_connection_t *conn, const char *pids,
                 const char *addpids, const char *delpids )
{
  htsbuf_queue_t q;
  int r, split = 0;

  pids    = satip_rtsp_pids_strip(conn, pids);
  addpids = satip_rtsp_pids_strip(conn, addpids);
  delpids = satip_rtsp_pids_strip(conn, delpids);

  if (pids == NULL && addpids == NULL && delpids == NULL)
    return 1;

  // printf("pids = '%s' addpids = '%s' delpids = '%s'\n", pids, addpids, delpids);

  htsbuf_queue_init(&q, 0);
  htsbuf_qprintf(&q, "PLAY rtsp://%s/stream=%li?",
                 conn->device->sd_info.addr, conn->stream_id);
  /* pids setup and add/del requests cannot be mixed per specification */
  if (pids) {
    htsbuf_qprintf(&q, "pids=%s", pids);
  } else {
    if (delpids)
      htsbuf_qprintf(&q, "delpids=%s", delpids);
    if (addpids) {
      if (delpids) {
        /* try to maintain the maximum request size - simple split */
        if (strlen(addpids) + strlen(delpids) <= conn->device->sd_pids_len)
          split = 1;
        else
          htsbuf_append(&q, "&", 1);
      }
      if (!split)
        htsbuf_qprintf(&q, "addpids=%s", addpids);
    }
  }
  htsbuf_qprintf(&q, " RTSP/1.0\r\nSession: %s\r\n", conn->session);
  r = satip_rtsp_send(conn, &q, SATIP_RTSP_CMD_PLAY);
  htsbuf_queue_flush(&q);
  if (r || !split)
    return r;

  htsbuf_queue_init(&q, 0);
  htsbuf_qprintf(&q, "PLAY rtsp://%s/stream=%li?",
                 conn->device->sd_info.addr, conn->stream_id);
  htsbuf_qprintf(&q, "addpids=%s", addpids);
  htsbuf_qprintf(&q, " RTSP/1.0\r\nSession: %s\r\n", conn->session);
  r = satip_rtsp_send2(conn, &q, SATIP_RTSP_CMD_PLAY);
  htsbuf_queue_flush(&q);
  return r;
}

int
satip_rtsp_teardown( satip_rtsp_connection_t *conn )
{
  int r;
  htsbuf_queue_t q;
  htsbuf_queue_init(&q, 0);
  htsbuf_qprintf(&q,
           "TEARDOWN rtsp://%s/stream=%li RTSP/1.0\r\nSession: %s\r\n",
            conn->device->sd_info.addr, conn->stream_id, conn->session);
  r = satip_rtsp_send(conn, &q, SATIP_RTSP_CMD_TEARDOWN);
  htsbuf_queue_flush(&q);
  return r;
}

#if 0
static int
satip_rtsp_describe_decode
  ( satip_connection_t *conn )
{
  if (header == NULL)
    return 1;
  printf("describe: %i\n", conn->code);
  printf("header:\n%s\n",  conn->header);
  printf("data:\n%s\n",    conn->data);
  return 0;
}

static void
satip_rtsp_describe( satip_connection_t *conn )
{
  htsbuf_queue_t q;
  htsbuf_queue_init(&q, 0);
  htsbuf_qprintf(&q,
           "DESCRIBE rtsp://%s/ RTSP/1.0\r\n", sd->sd_info.addr);
  satip_rtsp_write(conn, &q);
  htsbuf_queue_flush(&q);
}
#endif
