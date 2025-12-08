/*
 *  Tvheadend - RTSP routines
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

#include <signal.h>
#include "tvheadend.h"
#include "htsbuf.h"
#include "tcp.h"
#include "http.h"

/*
 * Utils
 */
int
rtsp_send_ext( http_client_t *hc, http_cmd_t cmd,
           const char *path, const char *query,
           http_arg_list_t *hdr,
           const char *body, size_t size )
{
  http_arg_list_t h;
  size_t blen = 7 + strlen(hc->hc_host) +
                (hc->hc_port != 554 ? 7 : 0) +
                (path ? strlen(path) : 1) + 1;
  char *buf = alloca(blen);
  char buf2[64];
  char buf_body[size + 3];

  if (hc->hc_rtsp_session) {
    if (hdr == NULL) {
      hdr = &h;
      http_arg_init(&h);
    }
    http_arg_set(hdr, "Session", hc->hc_rtsp_session);
  }

  if (size > 0) {
    if (hdr == NULL) {
      hdr = &h;
      http_arg_init(&h);
    }
    strlcpy(buf_body, body, sizeof(buf_body));
    strlcat(buf_body, "\r\n", 2);
    snprintf(buf2, sizeof(buf2), "%"PRIu64, (uint64_t)(size + 2));
    http_arg_set(hdr, "Content-Length", buf2);
  }

  http_client_basic_auth(hc, hdr, hc->hc_rtsp_user, hc->hc_rtsp_pass);
  if (hc->hc_port != 554)
    snprintf(buf2, sizeof(buf2), ":%d", hc->hc_port);
  else
    buf2[0] = '\0';
  snprintf(buf, blen, "rtsp://%s%s%s", hc->hc_host, buf2, path ? path : "/");
  if(size > 0)
    return http_client_send(hc, cmd, buf, query, hdr, buf_body, size + 2);
  else
    return http_client_send(hc, cmd, buf, query, hdr, NULL, 0);
}

void
rtsp_clear_session( http_client_t *hc )
{
  free(hc->hc_rtsp_session);
  free(hc->hc_rtp_dest);
  hc->hc_rtp_port       = 0;
  hc->hc_rtcp_port      = 0;
  hc->hc_rtsp_session   = NULL;
  hc->hc_rtp_dest       = NULL;
  hc->hc_rtp_multicast  = 0;
  hc->hc_rtsp_stream_id = -1;
  hc->hc_rtp_timeout    = 60;
}

/*
 * Options
 */

int
rtsp_options_decode( http_client_t *hc )
{
  char *argv[32], *p;
  int i, n, what = 0;

  p = http_arg_get(&hc->hc_args, "Public");
  if (p == NULL)
    return -EIO;
  n = http_tokenize(p, argv, 32, ',');
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
  return (hc->hc_code != 200 && what != 0x0f) ? -EIO : HTTP_CON_OK;
}

int
rtsp_setup_decode( http_client_t *hc, int satip )
{
  char *argv[32], *argv2[2], *p;
  int i, n, j;

  rtsp_clear_session(hc);
  if (hc->hc_code != 200)
    return -EIO;
  p = http_arg_get(&hc->hc_args, "Session");
  if (p == NULL)
    return -EIO;
  n = http_tokenize(p, argv, 32, ';');
  if (n < 1)
    return -EIO;
  hc->hc_rtsp_session = strdup(argv[0]);
  for (i = 1; i < n; i++) {
    if (strncasecmp(argv[i], "timeout=", 8) == 0) {
      hc->hc_rtp_timeout = atoi(argv[i] + 8);
      if (hc->hc_rtp_timeout < 20 || hc->hc_rtp_timeout > 3600) {
        tvhwarn(LS_RTSP, "timeout value out of range 20-3600 (%i)", hc->hc_rtp_timeout);
        return -EIO;
      }
    }
  }
  if (satip) {
    p = http_arg_get(&hc->hc_args, "com.ses.streamID");
    if (p == NULL)
      return -EIO;
    /* zero is valid stream id per specification */
    while (*p && ((*p == '0' && *(p + 1) == '0') || *p < ' '))
      p++;
    if (p[0] == '0' && p[1] == '\0') {
      hc->hc_rtsp_stream_id = 0;
    } else {
      hc->hc_rtsp_stream_id = atoll(p);
      if (hc->hc_rtsp_stream_id <= 0)
        return -EIO;
    }
  }
  p = http_arg_get(&hc->hc_args, "Transport");
  if (p == NULL)
    return -EIO;
  n = http_tokenize(p, argv, 32, ';');
  if (n < 2)
    return -EIO;
  hc->hc_rtp_tcp = -1;
  hc->hc_rtcp_tcp = -1;
  hc->hc_rtp_port = -1;
  hc->hc_rtcp_port = -1;
  if (!strcasecmp(argv[0], "RTP/AVP/TCP")) {
    for (i = 1; i < n; i++) {
      if (strncmp(argv[i], "interleaved=", 12) == 0) {
        j = http_tokenize(argv[i] + 12, argv2, 2, '-');
        if (j > 0) {
          hc->hc_rtp_tcp = atoi(argv2[0]);
          if (hc->hc_rtp_tcp < 0)
            return -EIO;
          if (j > 1) {
            hc->hc_rtcp_tcp = atoi(argv2[1]);
            if (hc->hc_rtcp_tcp < 0)
              return -EIO;
          }
        } else {
          return -EIO;
        }
      }
    }
  } else if (!strcasecmp(argv[0], "RTP/AVP") ||
             !strcasecmp(argv[0], "RTP/AVP/UDP") ||
             !strcasecmp(argv[0], "RTP/AVPF/UDP")) {
    if (n < 3)
      return -EIO;
    hc->hc_rtp_multicast = strcasecmp(argv[1], "multicast") == 0;
    if (strcasecmp(argv[1], "unicast") && !hc->hc_rtp_multicast)
      return -EIO;
    for (i = 2; i < n; i++) {
      if (strncmp(argv[i], "destination=", 12) == 0)
        hc->hc_rtp_dest = strdup(argv[i] + 12);
      else if (strncmp(argv[i], "client_port=", 12) == 0) {
        j = http_tokenize(argv[i] + 12, argv2, 2, '-');
        if (j > 0) {
          hc->hc_rtp_port = atoi(argv2[0]);
          if (hc->hc_rtp_port <= 0)
            return -EIO;
          if (j > 1) {
            hc->hc_rtcp_port = atoi(argv2[1]);
            if (hc->hc_rtcp_port <= 0)
              return -EIO;
          }
        } else {
          return -EIO;
        }
      }
      else if (strncmp(argv[i], "server_port=", 12) == 0) {
        j = http_tokenize(argv[i] + 12, argv2, 2, '-');
        if (j > 1) {
          hc->hc_rtcp_server_port = atoi(argv2[1]);
          if (hc->hc_rtcp_server_port <= 0)
            return -EIO;
        } else {
          return -EIO;
        }
      }
    }
  } else {
    return -EIO;
  }
  return HTTP_CON_OK;
}

int
rtsp_play_decode( http_client_t *hc )
{
  char *argv[32], *p;
  int n;

  if (hc->hc_code != 200)
    return -EIO;
  p = http_arg_get(&hc->hc_args, "Range");
  if (p == NULL)
    return -EIO;
  n = http_tokenize(p, argv, 32, '=');
  if (n < 1 || strncmp(argv[0], "npt", 3))
    return -EIO;
  hc->hc_rtsp_stream_start = strtoumax(argv[1], NULL, 10);
  p = http_arg_get(&hc->hc_args, "Scale");
  if (p == NULL)
    return -EIO;
  hc->hc_rtsp_scale = strtof(p, NULL);
  return 0;
}

int
rtsp_setup( http_client_t *hc,
            const char *path, const char *query,
            const char *multicast_addr,
            int rtp_port, int rtcp_port )
{
  http_arg_list_t h;
  char transport[256];

  if (rtcp_port < 0) {
    snprintf(transport, sizeof(transport),
      "RTP/AVP/TCP;interleaved=%d-%d", rtp_port, rtp_port + 1);
  } else if (multicast_addr) {
    snprintf(transport, sizeof(transport),
      "RTP/AVP;multicast;destination=%s;ttl=1;client_port=%i-%i",
      multicast_addr, rtp_port, rtcp_port);
  } else if(hc->hc_rtp_avpf) {
    snprintf(transport, sizeof(transport),
      "RTP/AVPF/UDP;unicast;client_port=%i-%i", rtp_port, rtcp_port);
  } else {
    snprintf(transport, sizeof(transport),
      "RTP/AVP;unicast;client_port=%i-%i", rtp_port, rtcp_port);
  }

  http_arg_init(&h);
  http_arg_set(&h, "Transport", transport);
  return rtsp_send(hc, RTSP_CMD_SETUP, path, query, &h);
}

int rtsp_describe_decode(http_client_t *hc, const char *buf, size_t len) {
  const char *p;
  char transport[64];
  int n, t, transport_type;

  p = http_arg_get(&hc->hc_args, "Content-Type");
  if (p == NULL || strncmp(p, "application/sdp", 15)) {
    tvhwarn(LS_RTSP, "describe: unkwown response content");
    return -EIO;
  }
  for (n = 0; n < len; n++) {
    p = buf + n;
    if (strncmp(p, "a=range", 7) == 0) {
      // Parse RTSP range info
      if (strncmp(p + 8, "npt=", 4) == 0) {
        sscanf(p + 8, "npt=%" PRItime_t "-%" PRItime_t, &hc->hc_rtsp_range_start,
            &hc->hc_rtsp_range_end);
      }
    }
    if (strncmp(p, "m=video", 7) == 0) {
      // Parse and select RTP/AVPF stream if available for retransmission support
      if (sscanf(p, "m=video %d %s %d\n", &t, transport, &transport_type) == 3) {
        tvhtrace(LS_RTSP, "describe: found transport: %d %s %d", t, transport,
            transport_type);
        if (strncmp(transport, "RTP/AVPF", 8) == 0) {
          hc->hc_rtp_avpf = 1;
        }
      }
    }
  }
  return HTTP_CON_OK;
}

int
rtsp_get_parameter( http_client_t *hc, const char *parameter ) {
  http_arg_list_t hdr;
  http_arg_init(&hdr);
  http_arg_set(&hdr, "Content-Type", "text/parameters");
  return rtsp_send_ext(hc, RTSP_CMD_GET_PARAMETER, NULL, NULL, &hdr, parameter, strlen(parameter));
}

int
rtsp_set_speed( http_client_t *hc, double speed ) {
  char buf[64];
  http_arg_list_t h;
  http_arg_init(&h);
  snprintf(buf, sizeof(buf), "%.2f", speed);
  http_arg_set(&h, "Scale", buf);
  return rtsp_send(hc, RTSP_CMD_PLAY, NULL, NULL, &h);
}

int
rtsp_set_position( http_client_t *hc, time_t position ) {
  char buf[64];
  http_arg_list_t h;
  http_arg_init(&h);
  snprintf(buf, sizeof(buf), "npt=%" PRItime_t "-", position);
  http_arg_set(&h, "Range", buf);
  return rtsp_send(hc, RTSP_CMD_PLAY, NULL, NULL, &h);
}
