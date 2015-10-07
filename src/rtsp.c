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

#include <pthread.h>
#include <signal.h>
#include "tvheadend.h"
#include "htsbuf.h"
#include "tcp.h"
#include "http.h"

/*
 * Utils
 */
int
rtsp_send( http_client_t *hc, http_cmd_t cmd,
           const char *path, const char *query,
           http_arg_list_t *hdr )
{
  http_arg_list_t h;
  size_t blen = 7 + strlen(hc->hc_host) +
                (hc->hc_port != 554 ? 7 : 0) +
                (path ? strlen(path) : 1) + 1;
  char *buf = alloca(blen);
  char buf2[7];

  if (hc->hc_rtsp_session) {
    if (hdr == NULL) {
      hdr = &h;
      http_arg_init(&h);
    }
    http_arg_set(hdr, "Session", hc->hc_rtsp_session);
  }
  http_client_basic_auth(hc, hdr, hc->hc_rtsp_user, hc->hc_rtsp_pass);
  if (hc->hc_port != 554)
    snprintf(buf2, sizeof(buf2), ":%d", hc->hc_port);
  else
    buf2[0] = '\0';
  snprintf(buf, blen, "rtsp://%s%s%s", hc->hc_host, buf2, path ? path : "/");
  return http_client_send(hc, cmd, buf, query, hdr, NULL, 0);
}

void
rtsp_clear_session( http_client_t *hc )
{
  free(hc->hc_rtsp_session);
  free(hc->hc_rtp_dest);
  hc->hc_rtp_port       = 0;
  hc->hc_rtpc_port      = 0;
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

#if 0
  { http_arg_t *ra;
  TAILQ_FOREACH(ra, &hc->hc_args, link)
    printf("  %s: %s\n", ra->key, ra->val); }
#endif
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
        tvhwarn("rtsp", "timeout value out of range 20-3600 (%i)", hc->hc_rtp_timeout);
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
  hc->hc_rtpc_port = -1;
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
  } else if (!strcasecmp(argv[0], "RTP/AVP")) {
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
            hc->hc_rtpc_port = atoi(argv2[1]);
            if (hc->hc_rtpc_port <= 0)
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
rtsp_setup( http_client_t *hc,
            const char *path, const char *query,
            const char *multicast_addr,
            int rtp_port, int rtpc_port )
{
  http_arg_list_t h;
  char transport[256];

  if (rtpc_port < 0) {
    snprintf(transport, sizeof(transport),
      "RTP/AVP/TCP;interleaved=%d-%d", rtp_port, rtp_port + 1);
  } else if (multicast_addr) {
    snprintf(transport, sizeof(transport),
      "RTP/AVP;multicast;destination=%s;ttl=1;client_port=%i-%i",
      multicast_addr, rtp_port, rtpc_port);
  } else {
    snprintf(transport, sizeof(transport),
      "RTP/AVP;unicast;client_port=%i-%i", rtp_port, rtpc_port);
  }

  http_arg_init(&h);
  http_arg_set(&h, "Transport", transport);
  return rtsp_send(hc, RTSP_CMD_SETUP, path, query, &h);
}

int
rtsp_describe_decode( http_client_t *hc )
{
  http_arg_t *ra;  

  /* TODO: Probably rewrite the data to the htsmsg tree ? */
  printf("describe: %i\n", hc->hc_code);
  TAILQ_FOREACH(ra, &hc->hc_args, link)
    printf("  %s: %s\n", ra->key, ra->val);
  printf("data:\n%s\n",    hc->hc_data);
  return HTTP_CON_OK;
}
