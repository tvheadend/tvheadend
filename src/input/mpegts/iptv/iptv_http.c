/*
 *  IPTV - HTTP/HTTPS handler
 *
 *  Copyright (C) 2013 Adam Sutton
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

/*
 * Connected
 */
static int
iptv_http_header ( http_client_t *hc )
{
  if (hc->hc_aux == NULL)
    return 0;

  /* multiple headers for redirections */
  if (hc->hc_code == HTTP_STATUS_OK) {
    pthread_mutex_lock(&global_lock);
    iptv_input_mux_started(hc->hc_aux);
    pthread_mutex_unlock(&global_lock);
  }
  return 0;
}

/*
 * Receive data
 */
static int
iptv_http_data
  ( http_client_t *hc, void *buf, size_t len )
{
  iptv_mux_t *im = hc->hc_aux;

  if (im == NULL)
    return 0;

  pthread_mutex_lock(&iptv_lock);

  tsdebug_write((mpegts_mux_t *)im, buf, len);
  sbuf_append(&im->mm_iptv_buffer, buf, len);

  if (len > 0)
    iptv_input_recv_packets(im, len);

  pthread_mutex_unlock(&iptv_lock);

  return 0;
}

/*
 * Setup HTTP(S) connection
 */
static int
iptv_http_start
  ( iptv_mux_t *im, const char *raw, const url_t *u )
{
  http_client_t *hc;
  int r;

  if (!(hc = http_client_connect(im, HTTP_VERSION_1_1, u->scheme,
                                 u->host, u->port, NULL)))
    return SM_CODE_TUNING_FAILED;
  hc->hc_hdr_received    = iptv_http_header;
  hc->hc_data_received   = iptv_http_data;
  hc->hc_handle_location = 1;        /* allow redirects */
  hc->hc_io_size         = 128*1024; /* increase buffering */
  http_client_register(hc);          /* register to the HTTP thread */
  r = http_client_simple(hc, u);
  if (r < 0) {
    http_client_close(hc);
    return SM_CODE_TUNING_FAILED;
  }
  im->im_data = hc;

  return 0;
}

/*
 * Stop connection
 */
static void
iptv_http_stop
  ( iptv_mux_t *im )
{
  http_client_t *hc = im->im_data;

  hc->hc_aux = NULL;
  pthread_mutex_unlock(&iptv_lock);
  http_client_close(hc);
  pthread_mutex_lock(&iptv_lock);
}


/*
 * Initialise HTTP handler
 */

void
iptv_http_init ( void )
{
  static iptv_handler_t ih[] = {
    {
      .scheme = "http",
      .start  = iptv_http_start,
      .stop   = iptv_http_stop,
    },
    {
      .scheme  = "https",
      .start  = iptv_http_start,
      .stop   = iptv_http_stop,
    }
  };
  iptv_handler_register(ih, 2);
}
