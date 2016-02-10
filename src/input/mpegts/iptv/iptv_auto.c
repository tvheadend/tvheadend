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
#include "download.h"
#include "misc/m3u.h"

#include <fcntl.h>
#include <sys/stat.h>

typedef struct auto_private {
  iptv_network_t *in_network;
  download_t      in_download;
  gtimer_t        in_auto_timer;
} auto_private_t;

/*
 *
 */
static int _epgcfg_from_str(const char *str)
{
  static struct strtab cfgs[] = {
    { "0",                 MM_EPG_DISABLE },
    { "none",              MM_EPG_DISABLE },
    { "disable",           MM_EPG_DISABLE },
    { "off",               MM_EPG_DISABLE },
    { "1",                 MM_EPG_ENABLE },
    { "all",               MM_EPG_ENABLE },
    { "enable",            MM_EPG_ENABLE },
    { "on",                MM_EPG_ENABLE },
    { "force",             MM_EPG_FORCE },
    { "eit",               MM_EPG_ONLY_EIT },
    { "uk_freesat",        MM_EPG_ONLY_UK_FREESAT },
    { "uk_freeview",       MM_EPG_ONLY_UK_FREEVIEW },
    { "viasat_baltic",     MM_EPG_ONLY_VIASAT_BALTIC },
    { "opentv_sky_uk",     MM_EPG_ONLY_OPENTV_SKY_UK },
    { "opentv_sky_italia", MM_EPG_ONLY_OPENTV_SKY_ITALIA },
    { "opentv_sky_ausat",  MM_EPG_ONLY_OPENTV_SKY_AUSAT },
    { "bulsatcom_39e",     MM_EPG_ONLY_BULSATCOM_39E },
    { "psip",              MM_EPG_ONLY_PSIP },
    { NULL }
  };
  return str ? str2val(str, cfgs) : -1;
}

/*
 *
 */
static void
iptv_auto_network_process_m3u_item(iptv_network_t *in,
                                   const char *last_url,
                                   const http_arg_list_t *remove_args,
                                   int64_t chnum, htsmsg_t *item,
                                   int *total, int *count)
{
  htsmsg_t *conf;
  htsmsg_field_t *f;
  mpegts_mux_t *mm;
  iptv_mux_t *im;
  url_t u;
  int change, epgcfg;
  http_arg_list_t args;
  http_arg_t *ra1, *ra2, *ra2_next;
  htsbuf_queue_t q;
  size_t l;
  int64_t chnum2;
  const char *url, *name, *logo, *epgid, *tags;
  char url2[512], custom[512], name2[128], buf[32], *n;

  url = htsmsg_get_str(item, "m3u-url");

  if (url == NULL ||
      (strncmp(url, "file://", 7) &&
       strncmp(url, "pipe://", 7) &&
       strncmp(url, "http://", 7) &&
       strncmp(url, "https://", 8) &&
       strncmp(url, "rtsp://", 7) &&
       strncmp(url, "rtsps://", 8) &&
       strncmp(url, "udp://", 6) &&
       strncmp(url, "rtp://", 6)))
    return;

  epgid = htsmsg_get_str(item, "tvh-chnum");
  chnum2 = epgid ? prop_intsplit_from_str(epgid, CHANNEL_SPLIT) : 0;
  if (chnum2 > 0) {
    chnum += chnum2;
  } else if (chnum) {
    if (chnum % CHANNEL_SPLIT)
      chnum += *total;
    else
      chnum += (int64_t)*total * CHANNEL_SPLIT;
  }

  name = htsmsg_get_str(item, "m3u-name");
  if (name == NULL)
    name = "";

  logo = htsmsg_get_str(item, "tvg-logo");
  if (logo == NULL)
    logo = htsmsg_get_str(item, "logo");

  epgid = htsmsg_get_str(item, "tvg-id");
  epgcfg = _epgcfg_from_str(htsmsg_get_str(item, "tvh-epg"));
  tags  = htsmsg_get_str(item, "tvh-tags");
  if (tags) {
    tags = n = strdupa(tags);
    while (*n) {
      if (*n == '|')
        *n = '\n';
      n++;
    }
  }

  urlinit(&u);
  custom[0] = '\0';

  if (strncmp(url, "pipe://", 7) == 0)
    goto skip_url;

  if (strncmp(url, "http://", 7) == 0 ||
      strncmp(url, "https://", 8) == 0) {
    conf = htsmsg_get_list(item, "m3u-http-headers");
    if (conf) {
      l = 0;
      HTSMSG_FOREACH(f, conf)
        if ((n = (char *)htsmsg_field_get_str(f)) != NULL)
          tvh_strlcatf(custom, sizeof(custom), l, "%s\n", n);
    }
  }

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
    l = 0;
    tvh_strlcatf(url2, sizeof(url2), l, "%s://", u.scheme);
    if (u.user && u.user[0] && u.pass && u.pass[0])
      tvh_strlcatf(url2, sizeof(url2), l, "%s:%s@", u.user, u.pass);
    tvh_strlcatf(url2, sizeof(url2), l, "%s", u.host);
    if (u.port > 0)
      tvh_strlcatf(url2, sizeof(url2), l, ":%d", u.port);
    if (u.path)
      tvh_strlcatf(url2, sizeof(url2), l, "%s", u.path);
    if (u.query)
      tvh_strlcatf(url2, sizeof(url2), l, "?%s", u.query);
    url = url2;
  }

skip_url:
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
        im->mm_iptv_svcname = strdup(name);
        change = 1;
      }
      if (im->mm_iptv_chnum != chnum) {
        iptv_bouquet_trigger(in, 0); /* propagate LCN change */
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
      if (strcmp(im->mm_iptv_hdr ?: "", custom)) {
        free(im->mm_iptv_hdr);
        im->mm_iptv_hdr = strdup(custom);
        change = 1;
      }
      if (strcmp(im->mm_iptv_tags ?: "", tags ?: "")) {
        free(im->mm_iptv_tags);
        im->mm_iptv_tags = strdup(tags);
        change = 1;
      }
      if (epgcfg >= 0 && im->mm_epg != epgcfg) {
        im->mm_epg = epgcfg;
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
  if (n)
    htsmsg_add_str(conf, "iptv_muxname", n);
  if (name)
    htsmsg_add_str(conf, "iptv_sname", name);
  if (chnum) {
    snprintf(buf, sizeof(buf), "%ld.%ld",
             (long)(chnum / CHANNEL_SPLIT), (long)(chnum % CHANNEL_SPLIT));
    htsmsg_add_str(conf, "channel_number", buf);
  }
  if (logo)
    htsmsg_add_str(conf, "iptv_icon", logo);
  if (epgid)
    htsmsg_add_str(conf, "iptv_epgid", epgid);
  if (tags)
    htsmsg_add_str(conf, "iptv_tags", tags);
  if (!in->in_scan_create)
    htsmsg_add_s32(conf, "scan_result", MM_SCAN_OK);
  if (custom[0])
    htsmsg_add_str(conf, "iptv_hdr", custom);
  if (epgcfg >= 0)
    htsmsg_add_s32(conf, "epg", epgcfg);
  im = iptv_mux_create0(in, NULL, conf);
  htsmsg_destroy(conf);

  if (im) {
    idnode_changed(&im->mm_id);
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
                              const char *host_url,
                              http_arg_list_t *remove_args,
                              int64_t chnum)
{
  int total = 0, count = 0;
  htsmsg_t *m, *items, *item;
  htsmsg_field_t *f;
  int ret = 0;

  m = parse_m3u(data, in->in_ctx_charset, host_url);
  items = htsmsg_get_list(m, "items");
  HTSMSG_FOREACH(f, items) {
    if ((item = htsmsg_field_get_map(f)) == NULL) continue;
    iptv_auto_network_process_m3u_item(in, last_url,
                                       remove_args, chnum,
                                       item,
                                       &total, &count);
    
  }
  htsmsg_destroy(m);
  if (total == 0)
    ret = -1;
  else
    tvhinfo("iptv", "m3u parse: %d new mux(es) in network '%s' (total %d)",
            count, in->mn_network_name, total);
  return ret;
}

/*
 *
 */
static int
iptv_auto_network_process(void *aux, const char *last_url,
                          const char *host_url, char *data, size_t len)
{
  auto_private_t *ap = aux;
  iptv_network_t *in = ap->in_network;
  mpegts_mux_t *mm, *mm2;
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
    r = iptv_auto_network_process_m3u(in, data, last_url, host_url,
                                      &remove_args, in->in_channel_number);

  http_arg_flush(&remove_args);

  if (r == 0) {
    count = 0;
    for (mm = LIST_FIRST(&in->mn_muxes); mm; mm = mm2) {
      mm2 = LIST_NEXT(mm, mm_network_link);
      if (((iptv_mux_t *)mm)->im_delete_flag) {
        mm->mm_delete(mm, 1);
        count++;
      }
    }
    if (count > 0)
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
static void
iptv_auto_network_stop( void *aux )
{
  auto_private_t *ap = aux;
  gtimer_disarm(&ap->in_auto_timer);
}

/*
 *
 */
static void
iptv_auto_network_trigger0(void *aux)
{
  auto_private_t *ap = aux;
  iptv_network_t *in = ap->in_network;

  download_start(&ap->in_download, in->in_url, ap);
  gtimer_arm(&ap->in_auto_timer, iptv_auto_network_trigger0, ap,
             MAX(1, in->in_refetch_period) * 60);
}

/*
 *
 */
void
iptv_auto_network_trigger( iptv_network_t *in )
{
  auto_private_t *ap = in->in_auto;
  if (ap) {
    ap->in_download.ssl_peer_verify = in->in_ssl_peer_verify;
    iptv_auto_network_trigger0(ap);
  }
}

/*
 *
 */
void
iptv_auto_network_init( iptv_network_t *in )
{
  auto_private_t *ap = calloc(1, sizeof(auto_private_t));
  ap->in_network = in;
  in->in_auto = ap;
  download_init(&ap->in_download, "iptv");
  ap->in_download.process = iptv_auto_network_process;
  ap->in_download.stop = iptv_auto_network_stop;
  iptv_auto_network_trigger(in);
}

/*
 *
 */
void
iptv_auto_network_done( iptv_network_t *in )
{
  auto_private_t *ap = in->in_auto;
  in->in_auto = NULL;
  gtimer_disarm(&ap->in_auto_timer);
  download_done(&ap->in_download);
  free(ap);
}
