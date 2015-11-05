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
#include "misc/m3u.h"

/*
 * Connected
 */
static int
iptv_http_header ( http_client_t *hc )
{
  iptv_mux_t *im = hc->hc_aux;
  char *argv[3], *s;
  int n;

  if (hc->hc_aux == NULL)
    return 0;

  /* multiple headers for redirections */
  if (hc->hc_code != HTTP_STATUS_OK)
    return 0;

  s = http_arg_get(&hc->hc_args, "Content-Type");
  if (s) {
    n = http_tokenize(s, argv, ARRAY_SIZE(argv), ';');
    if (n > 0 &&
        (strcasecmp(s, "audio/mpegurl") == 0 ||
         strcasecmp(s, "audio/x-mpegurl") == 0 ||
         strcasecmp(s, "application/x-mpegurl") == 0 ||
         strcasecmp(s, "application/apple.vnd.mpegurl") == 0 ||
         strcasecmp(s, "application/vnd.apple.mpegurl") == 0)) {
      if (im->im_m3u_header > 10) {
        im->im_m3u_header = 0;
        return 0;
      }
      im->im_m3u_header++;
      return 0;
    }
  }

  im->im_m3u_header = 0;
  pthread_mutex_lock(&global_lock);
  iptv_input_mux_started(hc->hc_aux);
  pthread_mutex_unlock(&global_lock);
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

  if (im == NULL || hc->hc_code != HTTP_STATUS_OK)
    return 0;

  if (im->im_m3u_header) {
    sbuf_append(&im->mm_iptv_buffer, buf, len);
    return 0;
  }

  pthread_mutex_lock(&iptv_lock);

  sbuf_append(&im->mm_iptv_buffer, buf, len);
  tsdebug_write((mpegts_mux_t *)im, buf, len);

  if (len > 0)
    iptv_input_recv_packets(im, len);

  pthread_mutex_unlock(&iptv_lock);

  return 0;
}

/*
 * Complete data
 */
static int
iptv_http_complete
  ( http_client_t *hc )
{
  iptv_mux_t *im = hc->hc_aux;
  const char *url, *host_url;
  char *p;
  htsmsg_t *m, *items, *item;
  htsmsg_field_t *f;
  url_t u;
  size_t l;
  int r;

  if (im->im_m3u_header) {
    im->im_m3u_header = 0;

    sbuf_append(&im->mm_iptv_buffer, "", 1);

    if ((p = http_arg_get(&hc->hc_args, "Host")) != NULL) {
      l = strlen(p) + 16;
      host_url = alloca(l);
      snprintf((char *)host_url, l, "%s://%s", hc->hc_ssl ? "https" : "http", p);
    } else if (im->mm_iptv_url_raw) {
      host_url = strdupa(im->mm_iptv_url_raw);
      if ((p = strchr(host_url, '/')) != NULL) {
        p++;
        if (*p == '/')
          p++;
        if ((p = strchr(p, '/')) != NULL)
          *p = '\0';
      }
      urlinit(&u);
      r = urlparse(host_url, &u);
      urlreset(&u);
      if (r)
        goto end;
    } else {
      host_url = NULL;
    }

    m = parse_m3u((char *)im->mm_iptv_buffer.sb_data, NULL, host_url);
    items = htsmsg_get_list(m, "items");
    url = NULL;
    HTSMSG_FOREACH(f, items) {
      if ((item = htsmsg_field_get_map(f)) == NULL) continue;
      url = htsmsg_get_str(items, "m3u-url");
      if (url && url[0]) break;
    }
    tvhtrace("iptv", "m3u url: '%s'", url);
    if (url == NULL) {
      tvherror("iptv", "m3u contents parsing failed");
      goto fin;
    }
    urlinit(&u);
    if (!urlparse(url, &u)) {
      hc->hc_keepalive = 0;
      r = http_client_simple_reconnect(hc, &u, HTTP_VERSION_1_1);
      if (r < 0)
        tvherror("iptv", "cannot reopen http client: %d'", r);
    } else {
      tvherror("iptv", "m3u url invalid '%s'", url);
    }
    urlreset(&u);
fin:
    htsmsg_destroy(m);
end:
    sbuf_reset(&im->mm_iptv_buffer, IPTV_BUF_SIZE);
  }
  return 0;
}

/*
 * Custom headers
 */
static void
iptv_http_create_header
  ( http_client_t *hc, http_arg_list_t *h, const url_t *url, int keepalive )
{
  iptv_mux_t *im = hc->hc_aux;

  http_client_basic_args(hc, h, url, keepalive);
  http_client_add_args(hc, h, im->mm_iptv_hdr);
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
  hc->hc_hdr_create      = iptv_http_create_header;
  hc->hc_hdr_received    = iptv_http_header;
  hc->hc_data_received   = iptv_http_data;
  hc->hc_data_complete   = iptv_http_complete;
  hc->hc_handle_location = 1;        /* allow redirects */
  hc->hc_io_size         = 128*1024; /* increase buffering */
  http_client_register(hc);          /* register to the HTTP thread */
  r = http_client_simple(hc, u);
  if (r < 0) {
    http_client_close(hc);
    return SM_CODE_TUNING_FAILED;
  }
  im->im_data = hc;
  sbuf_init_fixed(&im->mm_iptv_buffer, IPTV_BUF_SIZE);

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
