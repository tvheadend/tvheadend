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
#include "epggrab.h"
#include "misc/m3u.h"

#include <fcntl.h>
#include <sys/stat.h>

typedef struct auto_private {
  iptv_network_t *in_network;
  download_t      in_download;
  mtimer_t        in_auto_timer;
} auto_private_t;

/*
 *
 */
static int _epgcfg_from_str(const char *str, char *manual, size_t manual_len)
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
    { NULL }
  };
  int r = str ? str2val(str, cfgs) : -1;
  if (manual) *manual = '\0';
  if (r < 0) {
    const char *s = epggrab_ota_check_module_id(str);
    if (s) {
      r = MM_EPG_MANUAL;
      if (manual)
        strlcpy(manual, s, manual_len);
    }
  }
  return r;
}

/*
 *
 */
static void
iptv_auto_network_process_m3u_item(iptv_network_t *in,
                                   const char *last_url,
                                   const http_arg_list_t *remove_args,
                                   const http_arg_list_t *ignore_args,
                                   int ignore_path,
                                   int64_t chnum, htsmsg_t *item,
                                   int *total, int *count)
{
  htsmsg_t *conf;
  htsmsg_field_t *f;
  mpegts_mux_t *mm;
  iptv_mux_t *im;
  url_t u, u2;
  int change, epgcfg, muxprio, smuxprio;
  http_arg_list_t args;
  http_arg_t *ra1, *ra2, *ra2_next;
  size_t l;
  int64_t chnum2, vlcprog;
  const char *url, *url2, *name, *logo, *epgid, *tags;
  char *s;
  char custom[512], name2[128], buf[32], *moduleid, *n;

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
  if (!epgid) epgid = htsmsg_get_str(item, "tvg-chno");
  if (!epgid) epgid = htsmsg_get_str(item, "channel-number");
  chnum2 = epgid ? channel_get_number_from_str(epgid) : 0;

  muxprio = htsmsg_get_s32_or_default(item, "tvh-prio", -1);
  smuxprio = htsmsg_get_s32_or_default(item, "tvh-sprio", -1);

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
  moduleid = alloca(64);
  epgcfg = _epgcfg_from_str(htsmsg_get_str(item, "tvh-epg"), moduleid, 64);
  tags = htsmsg_get_str(item, "tvh-tags");
  if (!tags) tags = htsmsg_get_str(item, "group-title");
  if (tags) {
    tags = n = tvh_strdupa(tags);
    while (*n) {
      if (*n == '|')
        *n = '\n';
      n++;
    }
  }

  urlinit(&u);
  urlinit(&u2);
  url2 = url;
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
    goto end;
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
    u.query = http_arg_get_query(&args);
    http_arg_flush(&args);
    if (!urlrecompose(&u))
      url = url2 = u.raw;
  }

  /* remove requested arguments to ignore */
  if (!http_args_empty(ignore_args) || ignore_path > 0) {
    urlcopy(&u2, &u);
    if (!http_args_empty(ignore_args)) {
      http_arg_init(&args);
      http_parse_args(&args, u2.query);
      TAILQ_FOREACH(ra1, ignore_args, link)
        for (ra2 = TAILQ_FIRST(&args); ra2; ra2 = ra2_next) {
          ra2_next = TAILQ_NEXT(ra2, link);
          if (strcmp(ra1->key, ra2->key) == 0)
            http_arg_remove(&args, ra2);
        }
      free(u2.query);
      u2.query = http_arg_get_query(&args);
      http_arg_flush(&args);
    }
    if (ignore_path > 0 && u2.path) {
      for (; ignore_path > 0; ignore_path--) {
        s = strrchr(u2.path, '/');
        if (s)
          *s = '\0';
      }
    }
    if (!urlrecompose(&u2))
      url2 = u2.raw;
  }

skip_url:
  if (last_url) {
    if (name[0])
      snprintf(n = name2, sizeof(name2), "%s - %s", last_url, name);
    else
      n = (char *)last_url;
  } else {
    n = (char *)name;
  }

  LIST_FOREACH(mm, &in->mn_muxes, mm_network_link) {
    im = (iptv_mux_t *)mm;
    if (strcmp(im->mm_iptv_url_cmpid ?: (im->mm_iptv_url ?: ""), url2) == 0) {
      im->im_delete_flag = 0;
      change = 0;
      if (strcmp(im->mm_iptv_svcname ?: "", name)) {
        free(im->mm_iptv_svcname);
        im->mm_iptv_svcname = strdup(name);
        change = 1;
      }
      if (im->mm_iptv_chnum != chnum) {
        mpegts_network_bouquet_trigger((mpegts_network_t *)in, 0); /* propagate LCN change */
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
        im->mm_iptv_tags = tags ? strdup(tags) : NULL;
        change = 1;
      }
      if (epgcfg >= 0 && im->mm_epg != epgcfg) {
        im->mm_epg = epgcfg;
        change = 1;
      }
      if (moduleid[0] && strcmp(im->mm_epg_module_id ?: "", moduleid)) {
        free(im->mm_epg_module_id);
        im->mm_epg_module_id = strdup(moduleid);
        change = 1;
      }
      if (muxprio >= 0 && im->mm_iptv_priority != muxprio) {
        im->mm_iptv_priority = muxprio;
        change = 1;
      }
      if (smuxprio >= 0 && im->mm_iptv_streaming_priority != smuxprio) {
        im->mm_iptv_streaming_priority = smuxprio;
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
  htsmsg_add_str(conf, "iptv_url_cmpid", url2);
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
  if (moduleid[0])
    htsmsg_add_str(conf, "epg_module_id", moduleid);
  if (in->in_tsid_accept_zero_value)
    htsmsg_add_s32(conf, "tsid_zero", 1);
  if (!htsmsg_get_s64(item, "vlc-program", &vlcprog) &&
      vlcprog > 1 && vlcprog < 8191)
    htsmsg_add_s32(conf, "sid_filter", vlcprog);
  if (muxprio >= 0)
    htsmsg_add_s32(conf, "priority", muxprio);
  if (smuxprio >= 0)
    htsmsg_add_s32(conf, "spriority", smuxprio);

  im = iptv_mux_create0(in, NULL, conf);
  mpegts_mux_post_create((mpegts_mux_t *)im);
  htsmsg_destroy(conf);

  if (im) {
    idnode_changed(&im->mm_id);
    (*total)++;
    (*count)++;
  }

end:
  if (u.raw != u2.raw)
    urlreset(&u2);
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
                              http_arg_list_t *ignore_args,
                              int ignore_path,
                              int64_t chnum)
{
  int total = 0, count = 0;
  htsmsg_t *m, *items, *item;
  htsmsg_field_t *f;
  int ret = 0;

  m = parse_m3u(data, in->in_ctx_charset, host_url);
  items = htsmsg_get_list(m, "items");
  if (items) {
    HTSMSG_FOREACH(f, items) {
      if ((item = htsmsg_field_get_map(f)) == NULL) continue;
      iptv_auto_network_process_m3u_item(in, last_url,
                                         remove_args, ignore_args, ignore_path,
                                         chnum, item, &total, &count);
      
    }
  }
  htsmsg_destroy(m);
  if (total == 0)
    ret = -1;
  else
    tvhinfo(LS_IPTV, "m3u parse: %d new mux(es) in network '%s' (total %d)",
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
  http_arg_list_t remove_args, ignore_args;
  char *argv[32], *removes, *ignores;

  /* note that we know that data are terminated with '\0' */

  if (data == NULL || len == 0)
    return -1;

  http_arg_init(&remove_args);
  if (in->in_remove_args) {
    removes = tvh_strdupa(in->in_remove_args);
    n = http_tokenize(removes, argv, ARRAY_SIZE(argv), -1);
    for (i = 0; i < n; i++)
      http_arg_set(&remove_args, argv[i], NULL);
  }

  http_arg_init(&ignore_args);
  if (in->in_ignore_args) {
    ignores = tvh_strdupa(in->in_ignore_args);
    n = http_tokenize(ignores, argv, ARRAY_SIZE(argv), -1);
    for (i = 0; i < n; i++)
      http_arg_set(&ignore_args, argv[i], NULL);
  }

  LIST_FOREACH(mm, &in->mn_muxes, mm_network_link)
    ((iptv_mux_t *)mm)->im_delete_flag = 1;

  while (*data && *data <= ' ') data++;

  if (!strncmp(data, "#EXTM3U", 7))
    r = iptv_auto_network_process_m3u(in, data, last_url, host_url,
                                      &remove_args, &ignore_args,
                                      in->in_ignore_path,
                                      in->in_channel_number);

  http_arg_flush(&remove_args);
  http_arg_flush(&ignore_args);

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
      tvhinfo(LS_IPTV, "removed %d mux(es) from network '%s'", count, in->mn_network_name);
  } else {
    LIST_FOREACH(mm, &in->mn_muxes, mm_network_link)
      ((iptv_mux_t *)mm)->im_delete_flag = 0;
    tvherror(LS_IPTV, "unknown playlist format for network '%s'", in->mn_network_name);
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
  mtimer_disarm(&ap->in_auto_timer);
}

/*
 *
 */
static void
iptv_auto_network_trigger0(void *aux)
{
  auto_private_t *ap = aux;
  iptv_network_t *in = ap->in_network;

  if (in->mn_enabled)
    download_start(&ap->in_download, in->in_url, ap);
  mtimer_arm_rel(&ap->in_auto_timer, iptv_auto_network_trigger0, ap,
                 sec2mono(MAX(1, in->in_refetch_period) * 60));
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
  download_init(&ap->in_download, LS_IPTV);
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
  mtimer_disarm(&ap->in_auto_timer);
  download_done(&ap->in_download);
  free(ap);
}
