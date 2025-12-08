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
#include "config.h"
#include "iptv_private.h"
#include "iptv_rtcp.h"
#include "http.h"
#if ENABLE_ANDROID
#include <sys/socket.h>
#endif

typedef struct {
  http_client_t *hc;
  char *path;
  char *query;
  mtimer_t alive_timer;
  int play;
  time_t start_position;
  time_t range_start;
  time_t range_end;
  time_t position;
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
  if(rp->hc->hc_rtsp_keep_alive_cmd == RTSP_CMD_GET_PARAMETER)
    rtsp_get_parameter(rp->hc, "position");
  else if(rp->hc->hc_rtsp_keep_alive_cmd == RTSP_CMD_OPTIONS)
    rtsp_send(rp->hc, RTSP_CMD_OPTIONS, rp->path, rp->query, NULL);
  else if(rp->hc->hc_rtsp_keep_alive_cmd == RTSP_CMD_DESCRIBE) {
    rtsp_send(rp->hc, RTSP_CMD_DESCRIBE, rp->path, rp->query, NULL);
    rtsp_get_parameter(rp->hc, "position");
  } else
    return;
  mtimer_arm_rel(&rp->alive_timer, iptv_rtsp_alive_cb, im,
                 sec2mono(MAX(1, (rp->hc->hc_rtp_timeout / 2) - 1)));
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
  char *p;

  if (im == NULL) {
    /* teardown (or teardown timeout) */
    if (hc->hc_cmd == RTSP_CMD_TEARDOWN) {
      tvh_mutex_lock(&global_lock);
      mtimer_arm_rel(&hc->hc_close_timer, iptv_rtsp_close_cb, hc, 0);
      tvh_mutex_unlock(&global_lock);
    }
    return 0;
  }

  if (hc->hc_cmd == RTSP_CMD_GET_PARAMETER && hc->hc_code != HTTP_STATUS_OK) {
    tvhtrace(LS_IPTV, "GET_PARAMETER command returned invalid error code %d for '%s', "
        "fall back to OPTIONS in keep alive loop.", hc->hc_code, im->mm_iptv_url_raw);
    hc->hc_rtsp_keep_alive_cmd = RTSP_CMD_OPTIONS;
    return 0;
  }

  if (hc->hc_cmd != RTSP_CMD_DESCRIBE && hc->hc_code != HTTP_STATUS_OK) {
    tvherror(LS_IPTV, "invalid error code %d for '%s'", hc->hc_code, im->mm_iptv_url_raw);
    return 0;
  }

  if (hc->hc_cmd == RTSP_CMD_DESCRIBE && hc->hc_code != HTTP_STATUS_OK &&
      !(hc->hc_code == HTTP_STATUS_MOVED ||
      hc->hc_code == HTTP_STATUS_FOUND ||
      hc->hc_code == HTTP_STATUS_SEE_OTHER ||
      hc->hc_code == HTTP_STATUS_NOT_MODIFIED)) {
    tvherror(LS_IPTV, "DESCRIBE request returned an invalid error code (%d) for '%s', "
             "fall back to GET_PARAMETER in keep alive loop.", hc->hc_code, im->mm_iptv_url_raw);
    hc->hc_rtsp_keep_alive_cmd = RTSP_CMD_GET_PARAMETER;
    return 0;
  }

  rp = im->im_data;
  if(rp == NULL)
    return 0;

  switch (hc->hc_cmd) {
  case RTSP_CMD_DESCRIBE:
    if (rp->play) {
      // Already active, most probably a keep-alive response
      break;
    }
    if (hc->hc_code == HTTP_STATUS_MOVED ||
        hc->hc_code == HTTP_STATUS_FOUND ||
        hc->hc_code == HTTP_STATUS_SEE_OTHER ||
        hc->hc_code == HTTP_STATUS_NOT_MODIFIED) {
      // Redirect from RTSP server
      if (!hc->hc_handle_location) {
        tvherror(LS_IPTV, "received code %d from RTSP server but redirects disabled '%s'",
            hc->hc_code, im->mm_iptv_url_raw);
        return -1;
      }
      p = http_arg_get(&hc->hc_args, "Location");
      if (p == NULL) {
        tvherror(LS_IPTV, "received code %d from RTSP server but no new location given for '%s'",
            hc->hc_code, im->mm_iptv_url_raw);
        return -1;
      }
      tvhinfo(LS_IPTV, "received new location from RTSP server (code: %d) '%s' was '%s'",
          hc->hc_code, p, im->mm_iptv_url_raw);
    }
    break;
  case RTSP_CMD_SETUP:
    r = rtsp_setup_decode(hc, 0);
    if (r >= 0) {
      if(hc->hc_rtp_timeout > 20)
        hc->hc_rtp_timeout = 20;
      rtsp_play(hc, rp->path, rp->query);
      rp->play = 1;
    }
    break;
  case RTSP_CMD_PLAY:
    // Now let's set peer port for RTCP
    // Use the HTTP host for sending RTCP reports, NOT the hc_rtp_dest (which is where the stream is sent)
    if (im->mm_iptv_ret_url) {
      if (rtcp_connect(&im->im_rtcp_info, im->mm_iptv_ret_url, NULL, 0,
          im->mm_iptv_interface, im->mm_nicename) == 0) {
        im->im_use_retransmission = 1;
      }
    } else if (rtcp_connect(&im->im_rtcp_info, NULL, hc->hc_host,
        hc->hc_rtcp_server_port, im->mm_iptv_interface, im->mm_nicename) == 0) {
      im->im_use_retransmission = 1;
    }
    if (rtsp_play_decode(hc) == 0) {
      if (rp->start_position == 0)
        rp->position = rp->start_position = hc->hc_rtsp_stream_start;
      else {
        rp->position = hc->hc_rtsp_stream_start;
        tvhdebug(LS_IPTV, "rtsp: position update: %" PRItime_t,
            hc->hc_rtsp_stream_start);
      }
    } else
      rp->position = rp->start_position = time(NULL);
    hc->hc_cmd = HTTP_CMD_NONE;
    tvh_mutex_lock(&global_lock);
    if (im->mm_active)
      iptv_input_mux_started((iptv_input_t *)im->mm_active->mmi_input, im, 1);
    mtimer_arm_rel(&rp->alive_timer, iptv_rtsp_alive_cb, im,
                   sec2mono(MAX(1, (hc->hc_rtp_timeout / 2) - 1)));
    tvh_mutex_unlock(&global_lock);
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
  rtsp_priv_t *rp;
  iptv_mux_t *im = hc->hc_aux;
  int r;

  if (im == NULL)
    return 0;

  if (hc->hc_code != HTTP_STATUS_OK)
    return 0;

  rp = (rtsp_priv_t*) im->im_data;

  switch (hc->hc_cmd) {
  case RTSP_CMD_DESCRIBE:
    if (rp == NULL)
      break;
    if (rtsp_describe_decode(hc, buf, len) >= 0) {
      if(rp->range_start == 0)
        rp->range_start = hc->hc_rtsp_range_start;
      rp->range_end = hc->hc_rtsp_range_end;
      tvhdebug(LS_IPTV, "rtsp: buffer update, start: %" PRItime_t ", end: %" PRItime_t,
          rp->range_start, rp->range_end);
    }
    if(rp->play) {
      // Already active, most probably a keep-alive response
      break;
    }
    r = rtsp_setup(hc, rp->path, rp->query, NULL,
                   ntohs(IP_PORT(im->mm_iptv_connection->ip)),
                   ntohs(IP_PORT(im->im_rtcp_info.connection->ip)));
    if (r < 0) {
      udp_close(im->im_rtcp_info.connection);
      udp_close(im->mm_iptv_connection);
      http_client_close(hc);
      return -1;
    }
    udp_multirecv_init(&im->im_um, IPTV_PKTS, IPTV_PKT_PAYLOAD);
    udp_multirecv_init(&im->im_rtcp_info.um, IPTV_PKTS, IPTV_PKT_PAYLOAD);
    sbuf_alloc_(&im->im_temp_buffer, IPTV_BUF_SIZE);
    break;
  case RTSP_CMD_GET_PARAMETER:
    if (rp == NULL)
      break;
    // Generic position update
    if (strncmp(buf, "position", 8) == 0) {
      rp->position = strtoumax(buf + 10, NULL, 10);
      tvhdebug(LS_IPTV, "rtsp: position update: %" PRItime_t, rp->position);
    }
    break;
  default:
    if (len > 0) {
      tvherror(LS_IPTV, "unknown data %zd received for '%s':\n%s", len,
          im->mm_iptv_url_raw, (char* )buf);
    }
  }
  return 0;
}

/*
 * Setup RTSP(S) connection
 */
static int
iptv_rtsp_start
  ( iptv_input_t *mi, iptv_mux_t *im, const char *raw, const url_t *u )
{
  rtsp_priv_t *rp;
  http_client_t *hc;
  udp_connection_t *rtp, *rtcp;
  int r;
  int rtp_port;
  int max_rtp_port;

  if (!(hc = http_client_connect(im, RTSP_VERSION_1_0, u->scheme,
                                 u->host, u->port, NULL)))
    return SM_CODE_TUNING_FAILED;

  if (u->user)
    hc->hc_rtsp_user = strdup(u->user);
  if (u->pass)
    hc->hc_rtsp_pass = strdup(u->pass);
  
  rtp_port = config.rtsp_udp_min_port;
  max_rtp_port = config.rtsp_udp_max_port;

  if (!max_rtp_port) {
    rtp_port = 0;
  }

  if (rtp_port > max_rtp_port)
  {
    tvherror(LS_IPTV, "UDP minimum port is set higher than the UDP maximum port");
    http_client_close(hc);
    return SM_CODE_TUNING_FAILED;
  }

  while (udp_bind_double(&rtp, &rtcp,
                          LS_IPTV, "rtp", "rtcp",
                          NULL, rtp_port, NULL,
                          128 * 1024, 16384, 4 * 1024, 4 * 1024, 1) < 0)
  {
    if (!rtp_port) {
      tvherror(LS_IPTV, "could not bind a random UDP port for RTP");
      http_client_close(hc);
      return SM_CODE_TUNING_FAILED;
    } else if (max_rtp_port && rtp_port >= max_rtp_port - 2) {
      tvherror(LS_IPTV, "all the UDP ports allocated are used, please reduce the number of simultaneous IPTV streams or increase the number of ports in the global settings");
      http_client_close(hc);
      return SM_CODE_TUNING_FAILED;
    }
    rtp_port += 2;
  }

  hc->hc_hdr_received        = iptv_rtsp_header;
  hc->hc_data_received       = iptv_rtsp_data;
  hc->hc_handle_location     = 1;                      /* allow redirects */
  hc->hc_rtsp_keep_alive_cmd = RTSP_CMD_DESCRIBE;      /* start keep alive loop with DESCRIBE */
  http_client_register(hc);                            /* register to the HTTP thread */
  r = rtsp_describe(hc, u->path, u->query);
  if (r < 0) {
    tvherror(LS_IPTV, "rtsp: DESCRIBE failed");
    return SM_CODE_TUNING_FAILED;
  }
  rp = calloc(1, sizeof(*rp));
  rp->hc = hc;
  rp->path = strdup(u->path ?: "");
  rp->query = strdup(u->query ?: "");

  rtcp_init(&im->im_rtcp_info);
  im->im_data = rp;
  im->mm_iptv_fd = rtp->fd;
  im->mm_iptv_connection = rtp;
  im->im_rtcp_info.connection_fd = rtcp->fd;
  im->im_rtcp_info.connection = rtcp;
  im->mm_iptv_rtp_seq = -1;
  return 0;
}

/*
 * Stop connection
 */
static void
iptv_rtsp_stop
  ( iptv_input_t *mi, iptv_mux_t *im )
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
  tvh_mutex_unlock(&iptv_lock);
  mtimer_disarm(&rp->alive_timer);
  udp_multirecv_free(&im->im_um);
  udp_multirecv_free(&im->im_rtcp_info.um);
  if (!play)
    http_client_close(rp->hc);
  free(rp->path);
  free(rp->query);
  rtcp_destroy(&im->im_rtcp_info);
  free(rp);
  tvh_mutex_lock(&iptv_lock);
}

static void
iptv_rtp_header_callback ( iptv_mux_t *im, uint8_t *rtp, int len )
{
  rtcp_t *rtcp_info = &im->im_rtcp_info;
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
iptv_rtsp_read ( iptv_input_t *mi, iptv_mux_t *im )
{
  ssize_t r;
  if (im->mm_iptv_send_reports) {
    uint8_t buf[1500];
    /* RTCP - ignore all incoming packets for now */
    do {
      r = recv(im->im_rtcp_info.connection_fd, buf, sizeof(buf), MSG_DONTWAIT);
    } while (r > 0);
    r = iptv_rtp_read(im, iptv_rtp_header_callback);
  } else
    r = iptv_rtp_read(im, NULL);
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
