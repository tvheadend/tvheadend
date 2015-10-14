/*
 *  IPTV - automatic network based on playlists
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
#include "http.h"
#include "iptv_private.h"
#include "channels.h"

#include <fcntl.h>
#include <sys/stat.h>

/*
 *
 */
static char *get_m3u_str(char *data, char **res)
{
  char *p = data, first = *data;
  
  if (first == '"' || first == '\'') {
    data++;
    p = data;
    while (*data && *data != first)
      data++;
  } else {
    p = data;
    while (*data && *data != ',' && *data > ' ')
      data++;
  }
  *res = data;
  if (*data) {
    *data = '\0';
    (*res)++;
  }
  return p;
}

/*
 *
 */
static void
iptv_auto_network_process_m3u_item(iptv_network_t *in,
                                   const char *last_url,
                                   const http_arg_list_t *remove_args,
                                   const char *url, const char *name,
                                   const char *logo, const char *epgid,
                                   int64_t chnum, int *total, int *count)
{
  htsmsg_t *conf;
  mpegts_mux_t *mm;
  iptv_mux_t *im;
  url_t u;
  int change;
  http_arg_list_t args;
  http_arg_t *ra1, *ra2, *ra2_next;
  htsbuf_queue_t q;
  size_t l = 0;
  char url2[512], name2[128], *n;

  if (url == NULL ||
      (strncmp(url, "file://", 7) &&
       strncmp(url, "http://", 7) &&
       strncmp(url, "https://", 8)))
    return;

  if (chnum) {
    if (chnum % CHANNEL_SPLIT)
      chnum += *total;
    else
      chnum += (int64_t)*total * CHANNEL_SPLIT;
  }

  urlinit(&u);
  if (urlparse(url, &u))
    return;
  if (u.host == NULL || u.host[0] == '\0')
    goto end;

  /* remove requested arguments */
  if (!http_args_empty(remove_args)) {
    http_arg_init(&args);
    http_parse_args(&args, u.query);
    TAILQ_FOREACH(ra1, remove_args, link)
      for (ra2 = TAILQ_FIRST(&args); ra2; ra2 = ra2_next) {
        ra2_next = TAILQ_NEXT(ra2, link);
        if (strcmp(ra1->key, ra2->key) == 0)
          http_arg_remove(&args, ra2);
      }
    free(u.query);
    u.query = NULL;
    if (!http_args_empty(&args)) {
      htsbuf_queue_init(&q, 0);
      TAILQ_FOREACH(ra1, &args, link) {
        if (!htsbuf_empty(&q))
          htsbuf_append(&q, "&", 1);
        htsbuf_append_and_escape_url(&q, ra1->key);
        htsbuf_append(&q, "=", 1);
        htsbuf_append_and_escape_url(&q, ra1->val);
      }
      free(u.query);
      u.query = htsbuf_to_string(&q);
      htsbuf_queue_flush(&q);
    }
    http_arg_flush(&args);
    tvh_strlcatf(url2, sizeof(url2), l, "%s://", u.scheme);
    if (u.user && u.user[0] && u.pass && u.pass[0])
      tvh_strlcatf(url2, sizeof(url2), l, "%s:%s@", u.user, u.pass);
    tvh_strlcatf(url2, sizeof(url2), l, "%s", u.host);
    if (u.port > 0)
      tvh_strlcatf(url2, sizeof(url2), l, ":%d", u.port);
    tvh_strlcatf(url2, sizeof(url2), l, "%s", u.path);
    if (u.query)
      tvh_strlcatf(url2, sizeof(url2), l, "?%s", u.query);
    url = url2;
  }

  if (last_url) {
    snprintf(n = name2, sizeof(name2), "%s - %s", last_url, name);
  } else {
    n = (char *)name;
  }

  LIST_FOREACH(mm, &in->mn_muxes, mm_network_link) {
    im = (iptv_mux_t *)mm;
    if (strcmp(im->mm_iptv_url ?: "", url) == 0) {
      im->im_delete_flag = 0;
      if (strcmp(im->mm_iptv_svcname ?: "", name ?: "")) {
        free(im->mm_iptv_svcname);
        im->mm_iptv_svcname = name ? strdup(name) : NULL;
        change = 1;
      }
      if (im->mm_iptv_chnum != chnum) {
        im->mm_iptv_chnum = chnum;
        change = 1;
      }
      if ((im->mm_iptv_muxname == NULL || im->mm_iptv_muxname[0] == '\0') && n && *n) {
        free(im->mm_iptv_muxname);
        im->mm_iptv_muxname = strdup(n);
        change = 1;
      }
      if (strcmp(im->mm_iptv_icon ?: "", logo ?: "")) {
        free(im->mm_iptv_icon);
        im->mm_iptv_icon = logo ? strdup(logo) : NULL;
        change = 1;
      }
      if (strcmp(im->mm_iptv_epgid ?: "", epgid ?: "")) {
        free(im->mm_iptv_epgid);
        im->mm_iptv_epgid = epgid ? strdup(epgid) : NULL;
        change = 1;
      }
      if (change)
        idnode_notify_changed(&im->mm_id);
      (*total)++;
      goto end;
    }
  }


  conf = htsmsg_create_map();
  htsmsg_add_str(conf, "iptv_url", url);
  if (name)
    htsmsg_add_str(conf, "iptv_sname", name);
  im = iptv_mux_create0(in, NULL, conf);
  htsmsg_destroy(conf);

  if (im) {
    im->mm_config_save((mpegts_mux_t *)im);
    (*total)++;
    (*count)++;
  }

end:
  urlreset(&u);
}

/*
 *
 */
static int
iptv_auto_network_process_m3u(iptv_network_t *in, char *data,
                              const char *last_url,
                              http_arg_list_t *remove_args,
                              int64_t chnum)
{
  char *url, *name = NULL, *logo = NULL, *epgid = NULL;
  int total = 0, count = 0;

  while (*data && *data != '\n') data++;
  if (*data) data++;
  while (*data) {
    if (strncmp(data, "#EXTINF:", 8) == 0) {
      name = NULL;
      logo = NULL;
      epgid = NULL;
      data += 8;
      while (1) {
        while (*data && *data <= ' ') data++;
        if (*data == ',') break;
        if (strncmp(data, "tvg-logo=", 9) == 0)
          logo = get_m3u_str(data + 9, &data);
        else if (strncmp(data, "tvg-id=", 7) == 0)
          epgid = get_m3u_str(data + 7, &data);
        else if (strncmp(data, "logo=", 5) == 0)
          logo = get_m3u_str(data + 5, &data);
        else {
          data++;
          while (*data && *data != ',' && *data > ' ') data++;
        }
      }
      if (*data == ',') {
        data++;
        while (*data && *data <= ' ') data++;
        if (*data)
          name = data;
      }
      while (*data && *data != '\n') data++;
      if (*data) { *data = '\0'; data++; }
      continue;
    }
    while (*data && *data <= ' ') data++;
    url = data;
    while (*data && *data != '\n') data++;
    if (*data) { *data = '\0'; data++; }
    if (*url)
      iptv_auto_network_process_m3u_item(in, last_url, remove_args,
                                         url, name, logo, epgid,
                                         chnum, &total, &count);
  }

  if (total == 0)
    return -1;
  tvhinfo("iptv", "m3u parse: %d new mux(es) in network '%s' (total %d)",
          count, in->mn_network_name, total);
  return 0;
}

/*
 *
 */
static int
iptv_auto_network_process(iptv_network_t *in, const char *last_url, char *data, size_t len)
{
  mpegts_mux_t *mm;
  int r = -1, count, n, i;
  http_arg_list_t remove_args;
  char *argv[10];

  /* note that we know that data are terminated with '\0' */

  if (data == NULL || len == 0)
    return -1;

  http_arg_init(&remove_args);
  if (in->in_remove_args && in->in_remove_args) {
    n = http_tokenize(in->in_remove_args, argv, ARRAY_SIZE(argv), -1);
    for (i = 0; i < n; i++)
      http_arg_set(&remove_args, argv[i], "1");
  }

  LIST_FOREACH(mm, &in->mn_muxes, mm_network_link)
    ((iptv_mux_t *)mm)->im_delete_flag = 1;

  while (*data && *data <= ' ') data++;

  if (!strncmp(data, "#EXTM3U", 7))
    r = iptv_auto_network_process_m3u(in, data, last_url, &remove_args, in->in_channel_number);

  http_arg_flush(&remove_args);

  if (r == 0) {
    count = 0;
    LIST_FOREACH(mm, &in->mn_muxes, mm_network_link)
      if (((iptv_mux_t *)mm)->im_delete_flag) {
        mm->mm_delete(mm, 1);
        count++;
      }
    tvhinfo("iptv", "removed %d mux(es) from network '%s'", count, in->mn_network_name);
  } else {
    LIST_FOREACH(mm, &in->mn_muxes, mm_network_link)
      ((iptv_mux_t *)mm)->im_delete_flag = 0;
    tvherror("iptv", "unknown playlist format for network '%s'", in->mn_network_name);
  }

  return -1;
}

/*
 *
 */
static int
iptv_auto_network_file(iptv_network_t *in, const char *filename)
{
  int fd, res;
  struct stat st;
  char *data, *last_url;
  size_t r;
  off_t off;

  fd = tvh_open(filename, O_RDONLY, 0);
  if (fd < 0) {
    tvherror("iptv", "unable to open file '%s' (network '%s'): %s",
             filename, in->mn_network_name, strerror(errno));
    return -1;
  }
  if (fstat(fd, &st) || st.st_size == 0) {
    tvherror("iptv", "unable to stat file '%s' (network '%s'): %s",
             filename, in->mn_network_name, strerror(errno));
    close(fd);
    return -1;
  }
  data = malloc(st.st_size+1);
  off = 0;
  do {
    r = read(fd, data + off, st.st_size - off);
    if (r < 0) {
      if (ERRNO_AGAIN(errno))
        continue;
      break;
    }
    off += r;
  } while (off != st.st_size);
  close(fd);

  if (off == st.st_size) {
    data[off] = '\0';
    last_url = strrchr(filename, '/');
    if (last_url)
      last_url++;
    res = iptv_auto_network_process(in, last_url, data, off);
  } else {
    res = -1;
  }
  free(data);
  return res;
}

/*
 *
 */
static void
iptv_auto_network_fetch_done(void *aux)
{
  http_client_t *hc = aux;
  iptv_network_t *in = hc->hc_aux;
  if (in->in_http_client) {
    in->in_http_client = NULL;
    http_client_close((http_client_t *)aux);
  }
}

/*
 *
 */
static int
iptv_auto_network_fetch_complete(http_client_t *hc)
{
  iptv_network_t *in = hc->hc_aux;
  char *last_url = NULL;
  url_t u;

  switch (hc->hc_code) {
  case HTTP_STATUS_MOVED:
  case HTTP_STATUS_FOUND:
  case HTTP_STATUS_SEE_OTHER:
  case HTTP_STATUS_NOT_MODIFIED:
    return 0;
  }

  urlinit(&u);
  if (!urlparse(in->in_url, &u)) {
    last_url = strrchr(u.path, '/');
    if (last_url)
      last_url++;
  }

  pthread_mutex_lock(&global_lock);

  if (in->in_http_client == NULL)
    goto out;

  if (hc->hc_code == HTTP_STATUS_OK && hc->hc_result == 0 && hc->hc_data_size > 0)
    iptv_auto_network_process(in, last_url, hc->hc_data, hc->hc_data_size);
  else
    tvherror("iptv", "unable to fetch data from url for network '%s' [%d-%d/%zd]",
             in->mn_network_name, hc->hc_code, hc->hc_result, hc->hc_data_size);

  /* note: http_client_close must be called outside http_client callbacks */
  gtimer_arm(&in->in_fetch_timer, iptv_auto_network_fetch_done, hc, 0);

out:
  pthread_mutex_unlock(&global_lock);

  urlreset(&u);
  return 0;
}

/*
 *
 */
static void
iptv_auto_network_fetch(void *aux)
{
  iptv_network_t *in = aux;
  http_client_t *hc;
  url_t u;

  urlinit(&u);

  if (in->in_url == NULL)
    goto done;

  if (strncmp(in->in_url, "file://", 7) == 0) {
    iptv_auto_network_file(in, in->in_url + 7);
    goto done;
  }

  gtimer_disarm(&in->in_auto_timer);
  if (in->in_http_client) {
    http_client_close(in->in_http_client);
    in->in_http_client = NULL;
  }

  if (urlparse(in->in_url, &u) < 0) {
    tvherror("iptv", "wrong url for network '%s'", in->mn_network_name);
    goto done;
  }
  hc = http_client_connect(in, HTTP_VERSION_1_1, u.scheme, u.host, u.port, NULL);
  if (hc == NULL) {
    tvherror("iptv", "unable to open http client for network '%s'", in->mn_network_name);
    goto done;
  }
  hc->hc_handle_location = 1;
  hc->hc_data_limit = 1024*1024;
  hc->hc_data_complete = iptv_auto_network_fetch_complete;
  http_client_register(hc);
  http_client_ssl_peer_verify(hc, in->in_ssl_peer_verify);
  if (http_client_simple(hc, &u) < 0) {
    http_client_close(hc);
    tvherror("iptv", "unable to send http command for network '%s'", in->mn_network_name);
    goto done;
  }

  in->in_http_client = hc;

  gtimer_arm(&in->in_auto_timer, iptv_auto_network_fetch, in,
             MAX(1, in->in_refetch_period) * 60);
done:
  urlreset(&u);
}

/*
 *
 */
void
iptv_auto_network_trigger( iptv_network_t *in )
{
  gtimer_arm(&in->in_auto_timer, iptv_auto_network_fetch, in, 0);
}

/*
 *
 */
void
iptv_auto_network_init( iptv_network_t *in )
{
  iptv_auto_network_trigger(in);
}

/*
 *
 */
void
iptv_auto_network_done( iptv_network_t *in )
{
  if (in->in_http_client) {
    http_client_close(in->in_http_client);
    in->in_http_client = NULL;
  }
  gtimer_disarm(&in->in_auto_timer);
  gtimer_disarm(&in->in_fetch_timer);
}
