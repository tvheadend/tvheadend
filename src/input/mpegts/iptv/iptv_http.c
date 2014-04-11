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

#if ENABLE_CURL

/*
 * Connected
 */
static void
iptv_http_conn ( void *p )
{
  pthread_mutex_lock(&global_lock);
  iptv_input_mux_started(p);
  pthread_mutex_unlock(&global_lock);
}

/*
 * Receive data
 */
static size_t
iptv_http_data
  ( void *p, void *buf, size_t len )
{
  iptv_mux_t *im = p;

  pthread_mutex_lock(&iptv_lock);

  sbuf_append(&im->mm_iptv_buffer, buf, len);

  if (len > 0)
    iptv_input_recv_packets(im, len, 0);

  pthread_mutex_unlock(&iptv_lock);

  return len;
}

/*
 * Setup HTTP(S) connection
 */
static int
iptv_http_start
  ( iptv_mux_t *im, const url_t *u )
{
  http_client_t *hc;
  if (!(hc = http_connect(u, iptv_http_conn, iptv_http_data, NULL, im)))
    return SM_CODE_TUNING_FAILED;
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
  http_close(im->im_data);
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

#else /* ENABLE_CURL */

void
iptv_http_init ( void )
{
}

#endif /* ENABLE_CURL */
