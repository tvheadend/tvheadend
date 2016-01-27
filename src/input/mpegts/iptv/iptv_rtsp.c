/*
 *  IPTV - RTSP/RTSPS handler
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
#include "iptv_private.h"
#include "iptv_rtcp.h"
#include "http.h"
#if ENABLE_ANDROID
#include <sys/socket.h>
#endif

typedef struct {
  http_client_t *hc;
  udp_multirecv_t um;
  char *path;
  char *query;
  gtimer_t alive_timer;
  int play;
  iptv_rtcp_info_t * rtcp_info;
} rtsp_priv_t;

/*
 *
 */
static void
iptv_rtsp_close_cb ( void *aux )
{
  http_client_close((http_client_t *)aux);
}

/*
 * Alive timeout
 */
static void
iptv_rtsp_alive_cb ( void *aux )
{
  iptv_mux_t *im = aux;
  rtsp_priv_t *rp = im->im_data;

  rtsp_send(rp->hc, RTSP_CMD_OPTIONS, rp->path, rp->query, NULL);
  gtimer_arm(&rp->alive_timer, iptv_rtsp_alive_cb, im,
             MAX(1, (rp->hc->hc_rtp_timeout / 2) - 1));
}

/*
 * Connected
 */
static int
iptv_rtsp_header ( http_client_t *hc )
{
  iptv_mux_t *im = hc->hc_aux;
  rtsp_priv_t *rp;
  int r;

  if (im == NULL) {
    /* teardown (or teardown timeout) */
    if (hc->hc_cmd == RTSP_CMD_TEARDOWN) {
      pthread_mutex_lock(&global_lock);
      gtimer_arm(&hc->hc_close_timer, iptv_rtsp_close_cb, hc, 0);
      pthread_mutex_unlock(&global_lock);
    }
    return 0;
  }

  if (hc->hc_code != HTTP_STATUS_OK) {
    tvherror("iptv", "invalid error code %d for '%s'", hc->hc_code, im->mm_iptv_url_raw);
    return 0;
  }

  rp = im->im_data;

  switch (hc->hc_cmd) {
  case RTSP_CMD_SETUP:
    r = rtsp_setup_decode(hc, 0);
    if (r >= 0) {
      rtsp_play(hc, rp->path, rp->query);
      rp->play = 1;
    }
    break;
  case RTSP_CMD_PLAY:
    // Now let's set peer port for RTCP
    // Use the HTTP host for sending RTCP reports, NOT the hc_rtp_dest (which is where the stream is sent)
    if (udp_connect(rp->rtcp_info->connection, "rtcp", hc->hc_host, hc->hc_rtcp_server_port)) {
        tvhlog(LOG_WARNING, "rtsp", "Can't connect to remote, RTCP receiver reports won't be sent");
    }
    hc->hc_cmd = HTTP_CMD_NONE;
    pthread_mutex_lock(&global_lock);
    iptv_input_mux_started(hc->hc_aux);
    gtimer_arm(&rp->alive_timer, iptv_rtsp_alive_cb, im,
               MAX(1, (hc->hc_rtp_timeout / 2) - 1));
    pthread_mutex_unlock(&global_lock);
    break;
  default:
    break;
  }

  return 0;
}

/*
 * Receive data
 */
static int
iptv_rtsp_data
  ( http_client_t *hc, void *buf, size_t len )
{
  iptv_mux_t *im = hc->hc_aux;

  if (im == NULL)
    return 0;

  if (len > 0)
    tvherror("iptv", "unknown data %zd received for '%s'", len, im->mm_iptv_url_raw);

  return 0;
}

/*
 * Setup RTSP(S) connection
 */
static int
iptv_rtsp_start
  ( iptv_mux_t *im, const char *raw, const url_t *u )
{
  rtsp_priv_t *rp;
  http_client_t *hc;
  udp_connection_t *rtp, *rtcp;
  int r;

  if (!(hc = http_client_connect(im, RTSP_VERSION_1_0, u->scheme,
                                 u->host, u->port, NULL)))
    return SM_CODE_TUNING_FAILED;

  if (u->user)
    hc->hc_rtsp_user = strdup(u->user);
  if (u->pass)
    hc->hc_rtsp_pass = strdup(u->pass);

  if (udp_bind_double(&rtp, &rtcp,
                      "IPTV", "rtp", "rtcp",
                      NULL, 0, NULL,
                      128*1024, 16384, 4*1024, 4*1024) < 0) {
    http_client_close(hc);
    return SM_CODE_TUNING_FAILED;
  }

  hc->hc_hdr_received    = iptv_rtsp_header;
  hc->hc_data_received   = iptv_rtsp_data;
  hc->hc_handle_location = 1;        /* allow redirects */
  http_client_register(hc);          /* register to the HTTP thread */
  r = rtsp_setup(hc, u->path, u->query, NULL,
                 ntohs(IP_PORT(rtp->ip)),
                 ntohs(IP_PORT(rtcp->ip)));
  if (r < 0) {
    udp_close(rtcp);
    udp_close(rtp);
    http_client_close(hc);
    return SM_CODE_TUNING_FAILED;
  }

  rp = calloc(1, sizeof(*rp));
  rp->rtcp_info = calloc(1, sizeof(iptv_rtcp_info_t));
  rtcp_init(rp->rtcp_info);
  rp->rtcp_info->connection = rtcp;
  rp->hc = hc;
  udp_multirecv_init(&rp->um, IPTV_PKTS, IPTV_PKT_PAYLOAD);
  rp->path = strdup(u->path ?: "");
  rp->query = strdup(u->query ?: "");

  im->im_data = rp;
  im->mm_iptv_fd = rtp->fd;
  im->mm_iptv_connection = rtp;
  im->mm_iptv_fd2 = rtcp->fd;
  im->mm_iptv_connection2 = rtcp;

  return 0;
}

/*
 * Stop connection
 */
static void
iptv_rtsp_stop
  ( iptv_mux_t *im )
{
  rtsp_priv_t *rp = im->im_data;
  int play;

  lock_assert(&global_lock);

  if (rp == NULL)
    return;

  play = rp->play;
  im->im_data = NULL;
  rp->hc->hc_aux = NULL;
  if (play)
    rtsp_teardown(rp->hc, rp->path, "");
  pthread_mutex_unlock(&iptv_lock);
  gtimer_disarm(&rp->alive_timer);
  udp_multirecv_free(&rp->um);
  if (!play)
    http_client_close(rp->hc);
  free(rp->path);
  free(rp->query);
  rtcp_destroy(rp->rtcp_info);
  free(rp->rtcp_info);
  free(rp);
  pthread_mutex_lock(&iptv_lock);
}

static void
iptv_rtp_header_callback ( iptv_mux_t *im, uint8_t *rtp, int len )
{
  rtsp_priv_t *rp = im->im_data;
  iptv_rtcp_info_t *rtcp_info = rp->rtcp_info;
  ssize_t hlen;

  /* Basic headers checks */
  /* Version 2 */
  if ((rtp[0] & 0xC0) != 0x80)
    return;

  /* Header length (4bytes per CSRC) */
  hlen = ((rtp[0] & 0xf) * 4) + 12;
  if (rtp[0] & 0x10) {
    if (len < hlen+4)
      return;
    hlen += ((rtp[hlen+2] << 8) | rtp[hlen+3]) * 4;
    hlen += 4;
  }
  if (len < hlen || ((len - hlen) % 188) != 0)
    return;

  rtcp_receiver_update(rtcp_info, rtp);
}

/*
 * Read data
 */
static ssize_t
iptv_rtsp_read ( iptv_mux_t *im )
{
  rtsp_priv_t *rp = im->im_data;
  udp_multirecv_t *um = &rp->um;
  ssize_t r;
  uint8_t buf[1500];

  /* RTCP - ignore all incoming packets for now */
  do {
    r = recv(im->mm_iptv_fd2, buf, sizeof(buf), MSG_DONTWAIT);
  } while (r > 0);

  r = iptv_rtp_read(im, um, iptv_rtp_header_callback);
  if (r < 0 && ERRNO_AGAIN(errno))
    r = 0;
  return r;
}

/*
 * Initialise RTSP handler
 */

void
iptv_rtsp_init ( void )
{
  static iptv_handler_t ih[] = {
    {
      .scheme = "rtsp",
      .buffer_limit = UINT32_MAX, /* unlimited */
      .start  = iptv_rtsp_start,
      .stop   = iptv_rtsp_stop,
      .read   = iptv_rtsp_read,
      .pause  = iptv_input_pause_handler
    },
    {
      .scheme  = "rtsps",
      .buffer_limit = UINT32_MAX, /* unlimited */
      .start  = iptv_rtsp_start,
      .stop   = iptv_rtsp_stop,
      .read   = iptv_rtsp_read,
      .pause  = iptv_input_pause_handler
    }
  };
  iptv_handler_register(ih, 2);
}
