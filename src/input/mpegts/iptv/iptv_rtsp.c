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
#include "http.h"
#include "udp.h"

typedef struct {
  http_client_t *hc;
  udp_multirecv_t um;
  char *url;
  gtimer_t alive_timer;
} rtsp_priv_t;

/*
 * Alive timeout
 */
static void
iptv_rtsp_alive_cb ( void *aux )
{
  iptv_mux_t *im = aux;
  rtsp_priv_t *rp = im->im_data;

  rtsp_options(rp->hc);
  gtimer_arm(&rp->alive_timer, iptv_rtsp_alive_cb, im, rp->hc->hc_rtp_timeout / 2);
}

/*
 * Connected
 */
static int
iptv_rtsp_header ( http_client_t *hc )
{
  iptv_mux_t *im = hc->hc_aux;
  rtsp_priv_t *rp = im->im_data;
  int r;

  if (im == NULL)
    return 0;

  if (hc->hc_code != HTTP_STATUS_OK) {
    tvherror("iptv", "invalid error code %d for '%s'", hc->hc_code, im->mm_iptv_url);
    return 0;
  }

  switch (hc->hc_cmd) {
  case RTSP_CMD_SETUP:
    r = rtsp_setup_decode(hc, 0);
    if (r >= 0)
      rtsp_play(hc, rp->url, "");
    break;
  case RTSP_CMD_PLAY:
    hc->hc_cmd = HTTP_CMD_NONE;
    pthread_mutex_lock(&global_lock);
    iptv_input_mux_started(hc->hc_aux);
    gtimer_arm(&rp->alive_timer, iptv_rtsp_alive_cb, im, hc->hc_rtp_timeout / 2);
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
    tvherror("iptv", "unknown data %zd received for '%s'", len, im->mm_iptv_url);

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
  udp_connection_t *rtp, *rtpc;
  int r;

  if (!(hc = http_client_connect(im, RTSP_VERSION_1_0, u->scheme,
                                 u->host, u->port, NULL)))
    return SM_CODE_TUNING_FAILED;

  if (udp_bind_double(&rtp, &rtpc,
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
                 ntohs(IP_PORT(rtpc->ip)));
  if (r < 0) {
    udp_close(rtpc);
    udp_close(rtp);
    http_client_close(hc);
    return SM_CODE_TUNING_FAILED;
  }

  rp = calloc(1, sizeof(*rp));
  rp->hc = hc;
  udp_multirecv_init(&rp->um, IPTV_PKTS, IPTV_PKT_PAYLOAD);
  rp->url = strdup(u->raw);

  im->im_data = rp;
  im->mm_iptv_fd = rtp->fd;
  im->mm_iptv_connection = rtp;
  im->mm_iptv_fd2 = rtpc->fd;
  im->mm_iptv_connection2 = rtpc;

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

  lock_assert(&global_lock);

  if (rp == NULL)
    return;
  im->im_data = NULL;
  rp->hc->hc_aux = NULL;
  pthread_mutex_unlock(&iptv_lock);
  gtimer_disarm(&rp->alive_timer);
  udp_multirecv_free(&rp->um);
  http_client_close(rp->hc);
  free(rp->url);
  free(rp);
  pthread_mutex_lock(&iptv_lock);
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

  /* RTPC - ignore all incoming packets for now */
  do {
    r = recv(im->mm_iptv_fd2, buf, sizeof(buf), MSG_DONTWAIT);
  } while (r > 0);

  r = iptv_rtp_read(im, um);
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
      .start  = iptv_rtsp_start,
      .stop   = iptv_rtsp_stop,
      .read   = iptv_rtsp_read,
    },
    {
      .scheme  = "rtsps",
      .start  = iptv_rtsp_start,
      .stop   = iptv_rtsp_stop,
      .read   = iptv_rtsp_read,
    }
  };
  iptv_handler_register(ih, 2);
}
