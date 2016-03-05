/*
 *  IPTV - HTTP/HTTPS handler
 *
 *  Copyright (C) 2013 Adam Sutton
 *  Copyright (C) 2014,2015 Jaroslav Kysela
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
#include "openssl/aes.h"

#if AES_BLOCK_SIZE != 16
#error "Wrong openssl!"
#endif

typedef struct http_priv {
  iptv_mux_t    *im;
  http_client_t *hc;
  uint8_t        shutdown;
  uint8_t        started;
  sbuf_t         m3u_sbuf;
  sbuf_t         key_sbuf;
  int            m3u_header;
  uint64_t       off;
  char          *host_url;
  int64_t        hls_seq;
  char          *hls_url;
  uint8_t        hls_url2;
  uint8_t        hls_encrypted;
  char          *hls_url_after_key;
  char          *hls_key_url;
  htsmsg_t      *hls_m3u;
  htsmsg_t      *hls_key;
  uint8_t       *hls_si;
  int64_t        hls_last_si;
  struct {
    char          tmp[AES_BLOCK_SIZE];
    int           tmp_len;
    AES_KEY       key;
    unsigned char iv[AES_BLOCK_SIZE];
  } hls_aes128;
} http_priv_t;

/***/

static int iptv_http_complete_key ( http_client_t *hc );

/*
 *
 */
static int
iptv_http_safe_global_lock( http_priv_t *hp )
{
  iptv_mux_t *im = hp->im;
  int r;

  while (1) {
    if (im->mm_active == NULL || hp->shutdown)
      return 0;
    r = pthread_mutex_trylock(&global_lock);
    if (r == 0)
      break;
    if (r != EBUSY)
      continue;
    sched_yield();
    if (im->mm_active == NULL || hp->shutdown)
      return 0;
    r = pthread_mutex_trylock(&global_lock);
    if (r == 0)
      break;
    if (r == EBUSY)
      tvh_safe_usleep(10000);
  }
  return 1;
}

/*
 *
 */
static char *
iptv_http_get_url( http_priv_t *hp, htsmsg_t *m )
{
  htsmsg_t *items, *item, *inf, *sel = NULL, *key;
  htsmsg_field_t *f;
  int64_t seq, bandwidth, sel_bandwidth = 0;
  int width, height, sel_width = 0, sel_height = 0;
  const char *s;

  items = htsmsg_get_list(m, "items");

  /*
   * sequence sync
   */
  if (!hp->hls_url2) {
    seq = htsmsg_get_s64_or_default(m, "media-sequence", -1);
    if (seq >= 0) {
      hp->hls_url2 = 1;
      hp->hls_m3u = m;
      if (hp->hls_url == NULL && hp->hc->hc_url != NULL)
        hp->hls_url = strdup(hp->hc->hc_url);
    }
    if (seq >= 0 && hp->hls_seq) {
      for (; seq < hp->hls_seq; seq++) {
        if (items) {
          f = HTSMSG_FIRST(items);
          if (f)
            htsmsg_field_destroy(items, f);
        }
      }
    } else {
      if (seq >= 0)
        hp->hls_seq = seq;
    }
    htsmsg_delete_field(m, "media-sequence");
  }

  /*
   * extract the URL
   */
  if (items == NULL)
    return NULL;
  HTSMSG_FOREACH(f, items) {
    if ((item = htsmsg_field_get_map(f)) == NULL) continue;
    inf = htsmsg_get_map(item, "stream-inf");
    if (inf) {
      bandwidth = htsmsg_get_s64_or_default(inf, "BANDWIDTH", 0);
      s = htsmsg_get_str(inf, "RESOLUTION");
      width = height = 0;
      if (s)
        sscanf(s, "%dx%d", &width, &height);
      if (htsmsg_get_str(item, "m3u-url"))
        if ((width == 0 && sel_bandwidth < bandwidth) ||
            (bandwidth > 200000 && sel_width < width && sel_height < height) ||
            (sel == NULL && bandwidth > 1000)) {
          sel = item;
          sel_bandwidth = bandwidth;
          sel_width = width;
          sel_height = height;
        }
    } else {
      s = htsmsg_get_str(item, "m3u-url");
      if (s && s[0]) {
        key = htsmsg_get_map(item, "x-key");
        if (key && strcmp(htsmsg_get_str(key, "METHOD") ?: "NONE", "NONE") != 0)
          key = htsmsg_copy(key);
        else
          key = NULL;
        s = strdup(s);
        htsmsg_field_destroy(items, f);
        if (hp->hls_url) {
          hp->hls_url2 = 1;
          hp->hls_m3u = m;
          htsmsg_destroy(hp->hls_key);
          hp->hls_key = key;
          hp->hls_seq++;
        }
        return (char *)s;
      }
    }
  }
  if (sel && hp->hls_url == NULL) {
    inf = htsmsg_get_map(sel, "stream-inf");
    bandwidth = htsmsg_get_s64_or_default(inf, "BANDWIDTH", 0);
    tvhdebug("iptv", "HLS - selected stream %s, %"PRId64"kb/s, %s",
             htsmsg_get_str(inf, "RESOLUTION"),
             bandwidth / 1024,
             htsmsg_get_str(inf, "CODECS"));
    s = htsmsg_get_str(sel, "m3u-url");
    if (s && s[0]) {
      free(hp->hls_url);
      hp->hls_url = strdup(s);
      return strdup(s);
    }
  }
  return NULL;
}

/*
 *
 */
static void
iptv_http_data_aes128 ( http_priv_t *hp, sbuf_t *sb, int off )
{
  unsigned char *in  = sb->sb_data + off;
  unsigned char *out = in;
  size_t size        = sb->sb_ptr - off;

  AES_cbc_encrypt(in, out, size, &hp->hls_aes128.key, hp->hls_aes128.iv, AES_DECRYPT);
}

/*
 * Connected
 */
static int
iptv_http_header ( http_client_t *hc )
{
  http_priv_t *hp = hc->hc_aux;
  iptv_mux_t *im;
  char *argv[3], *s;
  int n;

  if (hp == NULL)
    return 0;
  im = hp->im;
  if (im == NULL)
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
      if (hp->m3u_header > 10) {
        hp->m3u_header = 0;
        return 0;
      }
      hp->m3u_header++;
      sbuf_reset(&hp->m3u_sbuf, 8192);
      sbuf_reset(&hp->key_sbuf, 32);
      hp->hls_encrypted = 0;
      return 0;
    }
  }

  hp->m3u_header = 0;
  hp->off = 0;
  if (iptv_http_safe_global_lock(hp)) {
    if (!hp->started) {
      iptv_input_mux_started(hp->im);
    } else {
      iptv_input_recv_flush(hp->im);
    }
    pthread_mutex_unlock(&global_lock);
    hp->started = 1;
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
  http_priv_t *hp = hc->hc_aux;
  iptv_mux_t *im;
  int pause = 0, off, rem;
  uint8_t tsbuf[188];

  if (hp == NULL || hp->im == NULL || hc->hc_code != HTTP_STATUS_OK)
    return 0;

  im = hp->im;

  if (hp->m3u_header) {
    sbuf_append(&hp->m3u_sbuf, buf, len);
    return 0;
  }
  if (hc->hc_data_complete == iptv_http_complete_key) {
    sbuf_append(&hp->key_sbuf, buf, len);
    return 0;
  }

  pthread_mutex_lock(&iptv_lock);

  if (hp->hls_encrypted) {
    off = im->mm_iptv_buffer.sb_ptr;
    if (hp->hls_aes128.tmp_len + len >= AES_BLOCK_SIZE) {
      if (hp->hls_aes128.tmp_len) {
        sbuf_append(&im->mm_iptv_buffer, hp->hls_aes128.tmp, hp->hls_aes128.tmp_len);
        rem = AES_BLOCK_SIZE - hp->hls_aes128.tmp_len;
        hp->hls_aes128.tmp_len = 0;
        sbuf_append(&im->mm_iptv_buffer, buf, rem);
        len -= rem;
        buf += rem;
      }
      rem = len % AES_BLOCK_SIZE;
      sbuf_append(&im->mm_iptv_buffer, buf, len - rem);
      buf += len - rem;
      len = rem;
    }
    memcpy(hp->hls_aes128.tmp + hp->hls_aes128.tmp_len, buf, len);
    hp->hls_aes128.tmp_len += len;
    if (off == im->mm_iptv_buffer.sb_ptr)
      return 0;
    buf = im->mm_iptv_buffer.sb_data + im->mm_iptv_buffer.sb_ptr;
    len = im->mm_iptv_buffer.sb_ptr - off;
    assert((len % 16) == 0);
    iptv_http_data_aes128(hp, &im->mm_iptv_buffer, off);
  } else {
    sbuf_append(&im->mm_iptv_buffer, buf, len);
  }
  tsdebug_write((mpegts_mux_t *)im, buf, len);
  hp->off += len;

  if (hp->hls_url && hp->off == 0 && len >= 2*188) {
    free(hp->hls_si);
    hp->hls_si = malloc(2*188);
    memcpy(hp->hls_si, buf, 2*188);
  }

  if (hp->hls_last_si + sec2mono(1) <= mclk() && hp->hls_si) {
    /* do rounding to start of the last MPEG-TS packet */
    rem = 188 - (hp->off % 188);
    if (im->mm_iptv_buffer.sb_ptr >= rem) {
      im->mm_iptv_buffer.sb_ptr -= rem;
      memcpy(tsbuf, im->mm_iptv_buffer.sb_data + im->mm_iptv_buffer.sb_ptr, rem);
      sbuf_append(&im->mm_iptv_buffer, hp->hls_si, 2*188);
      hp->hls_last_si = mclk();
      sbuf_append(&im->mm_iptv_buffer, tsbuf, rem);
      hp->off += rem;
    }
  }

  if (len > 0)
    if (iptv_input_recv_packets(im, len) == 1)
      pause = hc->hc_pause = 1;

  pthread_mutex_unlock(&iptv_lock);

  if (pause && iptv_http_safe_global_lock(hp)) {
    if (im->mm_active && !hp->shutdown)
      mtimer_arm_rel(&im->im_pause_timer, iptv_input_unpause, im, sec2mono(1));
    pthread_mutex_unlock(&global_lock);
  }
  return 0;
}

/*
 * Complete data
 */
static void
iptv_http_reconnect ( http_client_t *hc, const char *url )
{
  url_t u;
  int r;

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
}

static int
iptv_http_complete
  ( http_client_t *hc )
{
  http_priv_t *hp = hc->hc_aux;
  const char *s;
  char *url;
  htsmsg_t *m, *m2;
  int r;

  if (hp == NULL || hp->im == NULL)
    return 0;

  if (hp->m3u_header) {
    hp->m3u_header = 0;

    sbuf_append(&hp->m3u_sbuf, "", 1);

    if (hc->hc_url) {
      free(hp->host_url);
      hp->host_url = strdup(hc->hc_url);
    }
    m = parse_m3u((char *)hp->m3u_sbuf.sb_data, NULL, hp->host_url);
    sbuf_free(&hp->m3u_sbuf);
url:
    url = iptv_http_get_url(hp, m);
    if (hp->hls_m3u == m)
      m = NULL;
    if (hp->hls_key) {
      s = htsmsg_get_str(hp->hls_key, "METHOD");
      if (s == NULL || strcmp(s, "AES-128")) {
        tvherror("iptv", "unknown crypto method '%s'", s);
        goto end;
      }
      memset(&hp->hls_aes128.iv, 0, sizeof(hp->hls_aes128.iv));
      s = htsmsg_get_str(hp->hls_key, "IV");
      if (s != NULL) {
        if (s[0] != '0' || (s[1] != 'x' && s[1] != 'X') ||
            strlen(s) != (AES_BLOCK_SIZE * 2) + 2) {
          tvherror("iptv", "unknown IV type or length (%s)", s);
          goto end;
        }
        hex2bin(hp->hls_aes128.iv, sizeof(hp->hls_aes128.iv), s + 2);
      }
      s = htsmsg_get_str(hp->hls_key, "URI");
      if (s == NULL) {
        tvherror("iptv", "no URI in KEY attribute");
        goto end;
      }
      hp->hls_encrypted = 1;
      if (strcmp(hp->hls_key_url ?: "", s)) {
        free(hp->hls_key_url);
        hp->hls_key_url = strdup(s);
        free(hp->hls_url_after_key);
        hp->hls_url_after_key = url;
          url = strdup(s);
        hc->hc_data_complete = iptv_http_complete_key;
        sbuf_reset(&hp->key_sbuf, 32);
      }
    }
    tvhtrace("iptv", "m3u url: '%s'", url);
    if (url == NULL) {
      tvherror("iptv", "m3u contents parsing failed");
      goto fin;
    }
new_m3u:
    iptv_http_reconnect(hc, url);
end:
    free(url);
fin:
    htsmsg_destroy(m);
  } else {
    if (hp->hls_url && hp->hls_m3u) {
      m = hp->hls_m3u;
      hp->hls_m3u = NULL;
      m2 = htsmsg_get_list(m, "items");
      if (m2 && htsmsg_is_empty(m2)) {
        r = htsmsg_get_bool_or_default(m, "x-endlist", 0);
        hp->hls_url2 = 0;
        if (r == 0) {
          url = strdup(hp->hls_url);
          goto new_m3u;
        }
        htsmsg_destroy(m);
      } else {
        goto url;
      }
    }
  }
  return 0;
}

static int
iptv_http_complete_key
  ( http_client_t *hc )
{
  http_priv_t *hp = hc->hc_aux;

  if (hp == NULL)
    return 0;
  tvhtrace("iptv", "received key len %d", hp->key_sbuf.sb_ptr);
  if (hp->key_sbuf.sb_ptr != AES_BLOCK_SIZE) {
    tvherror("iptv", "AES-128 key wrong length (%d)\n", hp->key_sbuf.sb_ptr);
    return 0;
  }
  AES_set_decrypt_key(hp->key_sbuf.sb_data, 128, &hp->hls_aes128.key);
  hc->hc_data_complete = iptv_http_complete;
  iptv_http_reconnect(hc, hp->hls_url_after_key);
  free(hp->hls_url_after_key);
  hp->hls_url_after_key = NULL;
  htsmsg_destroy(hp->hls_key);
  hp->hls_key = NULL;
  return 0;
}

/*
 * Custom headers
 */
static void
iptv_http_create_header
  ( http_client_t *hc, http_arg_list_t *h, const url_t *url, int keepalive )
{
  http_priv_t *hp = hc->hc_aux;

  if (hp == NULL || hp->im == NULL)
    return;
  http_client_basic_args(hc, h, url, keepalive);
  http_client_add_args(hc, h, hp->im->mm_iptv_hdr);
}

/*
 * Setup HTTP(S) connection
 */
static int
iptv_http_start
  ( iptv_mux_t *im, const char *raw, const url_t *u )
{
  http_priv_t *hp;
  http_client_t *hc;
  int r;

  hp = calloc(1, sizeof(*hp));
  hp->im = im;
  if (!(hc = http_client_connect(hp, HTTP_VERSION_1_1, u->scheme,
                                 u->host, u->port, NULL))) {
    free(hp);
    return SM_CODE_TUNING_FAILED;
  }
  hc->hc_hdr_create      = iptv_http_create_header;
  hc->hc_hdr_received    = iptv_http_header;
  hc->hc_data_received   = iptv_http_data;
  hc->hc_data_complete   = iptv_http_complete;
  hc->hc_handle_location = 1;        /* allow redirects */
  hc->hc_io_size         = 128*1024; /* increase buffering */
  hp->hc = hc;
  im->im_data = hp;
  sbuf_init(&hp->m3u_sbuf);
  sbuf_init(&hp->key_sbuf);
  sbuf_init_fixed(&im->mm_iptv_buffer, IPTV_BUF_SIZE);
  http_client_register(hc);          /* register to the HTTP thread */
  r = http_client_simple(hc, u);
  if (r < 0) {
    http_client_close(hc);
    im->im_data = NULL;
    free(hp);
    return SM_CODE_TUNING_FAILED;
  }

  return 0;
}

/*
 * Stop connection
 */
static void
iptv_http_stop
  ( iptv_mux_t *im )
{
  http_priv_t *hp = im->im_data;

  hp->hc->hc_aux = NULL;
  hp->shutdown = 1;
  pthread_mutex_unlock(&iptv_lock);
  http_client_close(hp->hc);
  pthread_mutex_lock(&iptv_lock);
  im->im_data = NULL;
  sbuf_free(&hp->m3u_sbuf);
  sbuf_free(&hp->key_sbuf);
  htsmsg_destroy(hp->hls_m3u);
  htsmsg_destroy(hp->hls_key);
  free(hp->hls_url);
  free(hp->hls_url_after_key);
  free(hp->hls_key_url);
  free(hp->hls_si);
  free(hp->host_url);
  free(hp);
}


/*
 * Pause/Unpause
 */
static void
iptv_http_pause
  ( iptv_mux_t *im, int pause )
{
  http_priv_t *hp = im->im_data;

  assert(pause == 0);
  http_client_unpause(hp->hc);
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
      .buffer_limit = 5000,
      .start  = iptv_http_start,
      .stop   = iptv_http_stop,
      .pause  = iptv_http_pause
    },
    {
      .scheme  = "https",
      .buffer_limit = 5000,
      .start  = iptv_http_start,
      .stop   = iptv_http_stop,
      .pause  = iptv_http_pause
    }
  };
  iptv_handler_register(ih, 2);
}
