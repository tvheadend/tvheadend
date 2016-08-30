/*
 *  tvheadend, WEBUI / HTML user interface
 *  Copyright (C) 2008 Andreas Öman
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

#define _GNU_SOURCE /* for splice() */
#include <fcntl.h>

#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>

#include <sys/stat.h>

#include "tvheadend.h"
#include "config.h"
#include "http.h"
#include "tcp.h"
#include "webui.h"
#include "dvr/dvr.h"
#include "filebundle.h"
#include "streaming.h"
#include "imagecache.h"
#include "lang_codes.h"
#include "intlconv.h"
#if ENABLE_MPEGTS
#include "input.h"
#endif
#if ENABLE_SATIP_SERVER
#include "satip/server.h"
#endif

#if defined(PLATFORM_LINUX)
#include <sys/sendfile.h>
#elif defined(PLATFORM_FREEBSD)
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#endif

#if ENABLE_ANDROID
#include <sys/socket.h>
#endif

#define MIME_M3U "audio/x-mpegurl"
#define MIME_E2 "application/x-e2-bouquet"

enum {
  PLAYLIST_M3U,
  PLAYLIST_E2,
  PLAYLIST_SATIP_M3U\
};

static int webui_xspf;

/*
 *
 */
static int
http_channel_playlist_cmp(const void *a, const void *b)
{
  int r;
  channel_t *c1 = *(channel_t **)a, *c2 = *(channel_t **)b;
  int64_t n1 = channel_get_number(c1), n2 = channel_get_number(c2);
  if (n1 > n2)
    r = 1;
  else if (n1 < n2)
    r = -1;
  else
    r = strcasecmp(channel_get_name(c1), channel_get_name(c2));
  return r;
}

/*
 *
 */
static int
http_channel_tag_playlist_cmp(const void *a, const void *b)
{
  channel_tag_t *ct1 = *(channel_tag_t **)a, *ct2 = *(channel_tag_t **)b;
  int r = ct1->ct_index - ct2->ct_index;
  if (r == 0)
    r = strcasecmp(ct1->ct_name, ct2->ct_name);
  return r;
}

/**
 *
 */
static int
is_client_simple(http_connection_t *hc)
{
  char *c;

  if((c = http_arg_get(&hc->hc_args, "UA-OS")) != NULL) {
    if(strstr(c, "Windows CE") || strstr(c, "Pocket PC"))
      return 1;
  }

  if((c = http_arg_get(&hc->hc_args, "x-wap-profile")) != NULL) {
    return 1;
  }
  return 0;
}

/**
 * Root page, we direct the client to different pages depending
 * on if it is a full blown browser or just some mobile app
 */
static int
page_root(http_connection_t *hc, const char *remain, void *opaque)
{
  if(is_client_simple(hc)) {
    http_redirect(hc, "simple.html", &hc->hc_req_args, 0);
  } else {
    http_redirect(hc, "extjs.html", &hc->hc_req_args, 0);
  }
  return 0;
}

static int
page_root2(http_connection_t *hc, const char *remain, void *opaque)
{
  if (!tvheadend_webroot) return 1;
  http_redirect(hc, "/", &hc->hc_req_args, 0);
  return 0;
}

static int
page_no_webroot(http_connection_t *hc, const char *remain, void *opaque)
{
  size_t l;
  char *s;

  /* not found checks */
  if (!tvheadend_webroot)
    return HTTP_STATUS_NOT_FOUND;
  l = strlen(tvheadend_webroot);
  if (strncmp(hc->hc_url, tvheadend_webroot, l) == 0 &&
      hc->hc_url[l] == '/')
    return HTTP_STATUS_NOT_FOUND;

  /* redirect if url is not in the specified webroot */
  if (!remain)
    remain = "";
  s = alloca(2 + strlen(remain));
  s[0] = '/';
  strcpy(s + 1, remain);
  http_redirect(hc, s, &hc->hc_req_args, 0);
  return 0;
}

static int
page_login(http_connection_t *hc, const char *remain, void *opaque)
{
  if (hc->hc_access != NULL &&
      hc->hc_access->aa_username != NULL &&
      hc->hc_access->aa_username != '\0') {
    http_redirect(hc, "/", &hc->hc_req_args, 0);
    return 0;
  } else {
    return HTTP_STATUS_UNAUTHORIZED;
  }
}

static int
page_logout(http_connection_t *hc, const char *remain, void *opaque)
{
  if (hc->hc_access == NULL ||
      hc->hc_access->aa_username == NULL ||
      hc->hc_access->aa_username == '\0') {
redirect:
    http_redirect(hc, "/", &hc->hc_req_args, 0);
    return 0;
  } else {
    const char *s = http_arg_get(&hc->hc_args, "Cookie");
    if (s) {
      while (*s && *s != ';')
        s++;
      if (*s) s++;
      while (*s && *s <= ' ') s++;
      if (!strncmp(s, "logout=1", 8)) {
        hc->hc_logout_cookie = 2;
        goto redirect;
      }
      hc->hc_logout_cookie = 1;
    }
    return HTTP_STATUS_UNAUTHORIZED;
  }
}

/**
 * Static download of a file from the filesystem
 */
int
page_static_file(http_connection_t *hc, const char *_remain, void *opaque)
{
  int ret = 0;
  const char *base = opaque;
  char *remain, *postfix;
  char path[500];
  ssize_t size;
  const char *content = NULL;
  char buf[4096];
  const char *gzip = NULL;
  int nogzip = 0;

  if(_remain == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if(strstr(_remain, ".."))
    return HTTP_STATUS_BAD_REQUEST;

  snprintf(path, sizeof(path), "%s/%s", base, _remain);

  remain = tvh_strdupa(_remain);
  postfix = strrchr(remain, '.');
  if(postfix != NULL && !strcmp(postfix + 1, "gz")) {
    gzip = "gzip";
    *postfix = '\0';
    postfix = strrchr(remain, '.');
  }
  if(postfix != NULL) {
    postfix++;
    if(!strcmp(postfix, "js"))
      content = "text/javascript; charset=UTF-8";
    else if(!strcmp(postfix, "css"))
      content = "text/css; charset=UTF-8";
    else if(!strcmp(postfix, "git"))
      nogzip = 1;
    else if(!strcmp(postfix, "jpg"))
      nogzip = 1;
    else if(!strcmp(postfix, "png"))
      nogzip = 1;
  }

  fb_file *fp = fb_open(path, 0, (nogzip || gzip) ? 0 : 1);
  if (!fp) {
    tvherror(LS_WEBUI, "failed to open %s", path);
    return HTTP_STATUS_INTERNAL;
  }
  size = fb_size(fp);
  if (!gzip && fb_gzipped(fp))
    gzip = "gzip";

  pthread_mutex_lock(&hc->hc_fd_lock);
  http_send_header(hc, 200, content, size, gzip, NULL, 10, 0, NULL, NULL);
  while (!fb_eof(fp)) {
    ssize_t c = fb_read(fp, buf, sizeof(buf));
    if (c < 0) {
      ret = HTTP_STATUS_INTERNAL;
      break;
    }
    if (tvh_write(hc->hc_fd, buf, c)) {
      ret = HTTP_STATUS_INTERNAL;
      break;
    }
  }
  pthread_mutex_unlock(&hc->hc_fd_lock);
  fb_close(fp);

  return ret;
}

/**
 * HTTP subscription handling
 */
static void
http_stream_status ( void *opaque, htsmsg_t *m )
{
  http_connection_t *hc = opaque;
  htsmsg_add_str(m, "type", "HTTP");
  if (hc->hc_username)
    htsmsg_add_str(m, "user", hc->hc_username);
}

static inline void *
http_stream_preop ( http_connection_t *hc )
{
  return tcp_connection_launch(hc->hc_fd, http_stream_status, hc->hc_access);
}

static inline void
http_stream_postop ( void *tcp_id )
{
  tcp_connection_land(tcp_id);
}

/**
 * HTTP stream loop
 */
static void
http_stream_run(http_connection_t *hc, profile_chain_t *prch,
		const char *name, th_subscription_t *s)
{
  streaming_message_t *sm;
  int run = 1, started = 0;
  streaming_queue_t *sq = &prch->prch_sq;
  muxer_t *mux = prch->prch_muxer;
  int ptimeout, grace = 20, r;
  struct timeval tp;
  streaming_start_t *ss_copy;
  int64_t lastpkt, mono;

  if(muxer_open_stream(mux, hc->hc_fd))
    run = 0;

  /* reduce timeout on write() for streaming */
  tp.tv_sec  = 5;
  tp.tv_usec = 0;
  setsockopt(hc->hc_fd, SOL_SOCKET, SO_SNDTIMEO, &tp, sizeof(tp));
  if (config.dscp >= 0)
    socket_set_dscp(hc->hc_fd, config.dscp, NULL, 0);

  lastpkt = mclk();
  ptimeout = prch->prch_pro ? prch->prch_pro->pro_timeout : 5;

  if (hc->hc_no_output) {
    pthread_mutex_lock(&sq->sq_mutex);
    sq->sq_maxsize = 100000;
    pthread_mutex_unlock(&sq->sq_mutex);
  }

  while(!hc->hc_shutdown && run && tvheadend_is_running()) {
    pthread_mutex_lock(&sq->sq_mutex);
    sm = TAILQ_FIRST(&sq->sq_queue);
    if(sm == NULL) {
      mono = mclk() + sec2mono(1);
      do {
        r = tvh_cond_timedwait(&sq->sq_cond, &sq->sq_mutex, mono);
        if (r == ETIMEDOUT) {
          /* Check socket status */
          if (tcp_socket_dead(hc->hc_fd)) {
            tvhdebug(LS_WEBUI,  "Stop streaming %s, client hung up", hc->hc_url_orig);
            run = 0;
          } else if((!started && mclk() - lastpkt > sec2mono(grace)) ||
                     (started && ptimeout > 0 && mclk() - lastpkt > sec2mono(ptimeout))) {
            tvhwarn(LS_WEBUI,  "Stop streaming %s, timeout waiting for packets", hc->hc_url_orig);
            run = 0;
          }
          break;
        }
      } while (ERRNO_AGAIN(r));
      pthread_mutex_unlock(&sq->sq_mutex);
      continue;
    }

    streaming_queue_remove(sq, sm);
    pthread_mutex_unlock(&sq->sq_mutex);

    switch(sm->sm_type) {
    case SMT_MPEGTS:
    case SMT_PACKET:
      if(started) {
        pktbuf_t *pb;
        int len;
        if (sm->sm_type == SMT_PACKET)
          pb = ((th_pkt_t*)sm->sm_data)->pkt_payload;
        else
          pb = sm->sm_data;
        subscription_add_bytes_out(s, len = pktbuf_len(pb));
        if (len > 0)
          lastpkt = mclk();
        muxer_write_pkt(mux, sm->sm_type, sm->sm_data);
        sm->sm_data = NULL;
      }
      break;

    case SMT_GRACE:
      grace = sm->sm_code < 5 ? 5 : grace;
      break;

    case SMT_START:
      grace = 10;
      if(!started) {
        tvhdebug(LS_WEBUI, "%s streaming %s",
                 hc->hc_no_output ? "Probe" : "Start", hc->hc_url_orig);
        http_output_content(hc, muxer_mime(mux, sm->sm_data));

        if (hc->hc_no_output) {
          streaming_msg_free(sm);
          mono = mclk() + sec2mono(2);
          while (mclk() < mono) {
            if (tcp_socket_dead(hc->hc_fd))
              break;
            tvh_safe_usleep(50000);
          }
          return;
        }

        ss_copy = streaming_start_copy((streaming_start_t *)sm->sm_data);
        if(muxer_init(mux, ss_copy, name) < 0)
          run = 0;
        streaming_start_unref(ss_copy);

        started = 1;
      } else if(muxer_reconfigure(mux, sm->sm_data) < 0) {
        tvhwarn(LS_WEBUI,  "Unable to reconfigure stream %s", hc->hc_url_orig);
      }
      break;

    case SMT_STOP:
      if(sm->sm_code != SM_CODE_SOURCE_RECONFIGURED) {
        tvhwarn(LS_WEBUI,  "Stop streaming %s, %s", hc->hc_url_orig, 
                streaming_code2txt(sm->sm_code));
        run = 0;
      }
      break;

    case SMT_SERVICE_STATUS:
      if(tcp_socket_dead(hc->hc_fd)) {
        tvhdebug(LS_WEBUI,  "Stop streaming %s, client hung up",
                 hc->hc_url_orig);
        run = 0;
      } else if((!started && mclk() - lastpkt > sec2mono(grace)) ||
                 (started && ptimeout > 0 && mclk() - lastpkt > sec2mono(ptimeout))) {
        tvhwarn(LS_WEBUI,  "Stop streaming %s, timeout waiting for packets", hc->hc_url_orig);
        run = 0;
      }
      break;

    case SMT_NOSTART_WARN:
    case SMT_SKIP:
    case SMT_SPEED:
    case SMT_SIGNAL_STATUS:
    case SMT_DESCRAMBLE_INFO:
    case SMT_TIMESHIFT_STATUS:
      break;

    case SMT_NOSTART:
      tvhwarn(LS_WEBUI,  "Couldn't start streaming %s, %s",
              hc->hc_url_orig, streaming_code2txt(sm->sm_code));
      run = 0;
      break;

    case SMT_EXIT:
      tvhwarn(LS_WEBUI,  "Stop streaming %s, %s", hc->hc_url_orig,
              streaming_code2txt(sm->sm_code));
      run = 0;
      break;
    }

    streaming_msg_free(sm);

    if(mux->m_errors) {
      if (!mux->m_eos)
        tvhwarn(LS_WEBUI,  "Stop streaming %s, muxer reported errors", hc->hc_url_orig);
      run = 0;
    }
  }

  if(started)
    muxer_close(mux);
}

/*
 *
 */
static void
http_m3u_playlist_add(htsbuf_queue_t *hq, const char *hostpath,
                      const char *url_remain, const char *profile,
                      const char *svcname, const char *logo,
                      const char *epgid, access_t *access)
{
  htsbuf_append_str(hq, "#EXTINF:-1");
  if (logo) {
    if (strncmp(logo, "imagecache/", 11) == 0)
      htsbuf_qprintf(hq, " logo=\"%s/%s\"", hostpath, logo);
    else
      htsbuf_qprintf(hq, " logo=\"%s\"", logo);
  }
  if (epgid)
    htsbuf_qprintf(hq, " tvg-id=\"%s\"", epgid);
  htsbuf_qprintf(hq, ",%s\n%s%s?ticket=%s", svcname, hostpath, url_remain,
                     access_ticket_create(url_remain, access));
  htsbuf_qprintf(hq, "&profile=%s\n", profile);
}

/*
 *
 */
static void
http_e2_playlist_add(htsbuf_queue_t *hq, const char *hostpath,
                     const char *url_remain, const char *profile,
                     const char *svcname)
{
  htsbuf_append_str(hq, "#SERVICE 1:0:0:0:0:0:0:0:0:0:");
  htsbuf_append_and_escape_url(hq, hostpath);
  htsbuf_append_and_escape_url(hq, url_remain);
  htsbuf_qprintf(hq, "&profile=%s:%s\n", profile, svcname);
  htsbuf_qprintf(hq, "#DESCRIPTION %s\n", svcname);
}

/*
 *
 */
static void
http_satip_m3u_playlist_add(htsbuf_queue_t *hq, const char *hostpath,
                            channel_t *ch)
{
  char buf[64];
  const char *name, *logo;
  idnode_list_mapping_t *ilm;
  service_t *s = NULL;
  int src;

  LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
    /* cannot handle channels with more services for SAT>IP */
    if (s)
      return;
    s = (service_t *)ilm->ilm_in1;
  }
  src = (s && s->s_satip_source) ? s->s_satip_source(s) : -1;
  if (src < 1)
    return;
  name = channel_get_name(ch);
  logo = channel_get_icon(ch);
  snprintf(buf, sizeof(buf), "/stream/channelid/%d", channel_get_id(ch));
  htsbuf_append_str(hq, "#EXTINF:-1");
  if (logo) {
    if (strncmp(logo, "imagecache/", 11) == 0)
      htsbuf_qprintf(hq, " logo=%s/%s", hostpath, logo);
    else
      htsbuf_qprintf(hq, " logo=%s", logo);
  }
  htsbuf_qprintf(hq, ",%s\n%s%s?profile=pass\n", name, hostpath, buf);
}

/**
 * Output a playlist containing a single channel
 */
static int
http_channel_playlist(http_connection_t *hc, int pltype, channel_t *channel)
{
  htsbuf_queue_t *hq;
  char buf[255];
  char *profile, *hostpath;
  const char *name;
  char ubuf[UUID_HEX_SIZE];

  if (http_access_verify_channel(hc, ACCESS_STREAMING, channel))
    return HTTP_STATUS_UNAUTHORIZED;

  profile = profile_validate_name(http_arg_get(&hc->hc_req_args, "profile"));
  hostpath = http_get_hostpath(hc);

  hq = &hc->hc_reply;

  snprintf(buf, sizeof(buf), "/stream/channelid/%d", channel_get_id(channel));

  name = channel_get_name(channel);

  if (pltype == PLAYLIST_M3U) {

    htsbuf_append_str(hq, "#EXTM3U\n");
    http_m3u_playlist_add(hq, hostpath, buf, profile, name,
                          channel_get_icon(channel),
                          channel_get_uuid(channel, ubuf),
                          hc->hc_access);

  } else if (pltype == PLAYLIST_E2) {

    htsbuf_qprintf(hq, "#NAME %s\n", name);
    http_e2_playlist_add(hq, hostpath, buf, profile, name);

  } else if (pltype == PLAYLIST_SATIP_M3U) {

    http_satip_m3u_playlist_add(hq, hostpath, channel);

  }

  free(hostpath);
  free(profile);
  return 0;
}


/**
 * Output a playlist containing all channels with a specific tag
 */
static int
http_tag_playlist(http_connection_t *hc, int pltype, channel_tag_t *tag)
{
  htsbuf_queue_t *hq;
  char buf[255], ubuf[UUID_HEX_SIZE];
  idnode_list_mapping_t *ilm;
  char *profile, *hostpath;
  const char *name;
  channel_t *ch;
  channel_t **chlist;
  int idx = 0, count = 0;

  if(hc->hc_access == NULL ||
     access_verify2(hc->hc_access, ACCESS_STREAMING))
    return HTTP_STATUS_UNAUTHORIZED;

  hq = &hc->hc_reply;

  profile = profile_validate_name(http_arg_get(&hc->hc_req_args, "profile"));
  hostpath = http_get_hostpath(hc);

  LIST_FOREACH(ilm, &tag->ct_ctms, ilm_in1_link) {
    ch = (channel_t *)ilm->ilm_in2;
    if (ch->ch_enabled)
      count++;
  }

  chlist = malloc(count * sizeof(channel_t *));

  LIST_FOREACH(ilm, &tag->ct_ctms, ilm_in1_link) {
    ch = (channel_t *)ilm->ilm_in2;
    if (ch->ch_enabled)
      chlist[idx++] = ch;
  }

  assert(idx == count);

  qsort(chlist, count, sizeof(channel_t *), http_channel_playlist_cmp);

  if (pltype == PLAYLIST_M3U)
    htsbuf_append_str(hq, "#EXTM3U\n");
  else if (pltype == PLAYLIST_E2)
    htsbuf_qprintf(hq, "#NAME %s\n", tag->ct_name);
  for (idx = 0; idx < count; idx++) {
    ch = chlist[idx];
    if (http_access_verify_channel(hc, ACCESS_STREAMING, ch))
      continue;
    snprintf(buf, sizeof(buf), "/stream/channelid/%d", channel_get_id(ch));
    name = channel_get_name(ch);
    if (pltype == PLAYLIST_M3U) {
      http_m3u_playlist_add(hq, hostpath, buf, profile, name,
                            channel_get_icon(ch),
                            channel_get_uuid(ch, ubuf),
                            hc->hc_access);
    } else if (pltype == PLAYLIST_E2) {
      htsbuf_qprintf(hq, "#NAME %s\n", name);
      http_e2_playlist_add(hq, hostpath, buf, profile, name);
    } else if (pltype == PLAYLIST_SATIP_M3U) {
      http_satip_m3u_playlist_add(hq, hostpath, ch);
    }
  }

  free(chlist);

  free(hostpath);
  free(profile);
  return 0;
}


/**
 * Output a playlist pointing to tag-specific playlists
 */
static int
http_tag_list_playlist(http_connection_t *hc, int pltype)
{
  htsbuf_queue_t *hq;
  char buf[255];
  channel_tag_t *ct;
  channel_tag_t **ctlist;
  channel_t *ch;
  channel_t **chlist;
  int labelidx = 0;
  int idx = 0, count = 0;
  int chidx = 0, chcount = 0;
  char *profile, *hostpath;
  idnode_list_mapping_t *ilm;

  if(hc->hc_access == NULL ||
     access_verify2(hc->hc_access, ACCESS_STREAMING))
    return HTTP_STATUS_UNAUTHORIZED;

  hq = &hc->hc_reply;

  profile = profile_validate_name(http_arg_get(&hc->hc_req_args, "profile"));
  hostpath = http_get_hostpath(hc);

  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    if(ct->ct_enabled && !ct->ct_internal)
      count++;

  ctlist = malloc(count * sizeof(channel_tag_t *));

  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    if(ct->ct_enabled && !ct->ct_internal)
      ctlist[idx++] = ct;

  assert(idx == count);

  qsort(ctlist, count, sizeof(channel_tag_t *), http_channel_tag_playlist_cmp);

  if (pltype == PLAYLIST_E2 || pltype == PLAYLIST_SATIP_M3U) {
    CHANNEL_FOREACH(ch)
      if (ch->ch_enabled)
        chcount++;

    chlist = malloc(chcount * sizeof(channel_t *));

    CHANNEL_FOREACH(ch)
      if (ch->ch_enabled)
        chlist[chidx++] = ch;

    assert(chidx == chcount);

    qsort(chlist, chcount, sizeof(channel_t *), http_channel_playlist_cmp);
  } else {
    chlist = NULL;
  }

  htsbuf_append_str(hq, pltype == PLAYLIST_E2 ? "#NAME Tvheadend Channels\n" : "#EXTM3U\n");
  for (idx = 0; idx < count; idx++) {
    ct = ctlist[idx];

    if (pltype == PLAYLIST_M3U) {
      snprintf(buf, sizeof(buf), "/playlist/tagid/%d", idnode_get_short_uuid(&ct->ct_id));
      http_m3u_playlist_add(hq, hostpath, buf, profile, ct->ct_name,
                            channel_tag_get_icon(ct), NULL, hc->hc_access);
    } else if (pltype == PLAYLIST_E2) {
      htsbuf_qprintf(hq, "#SERVICE 1:64:%d:0:0:0:0:0:0:0::%s\n", labelidx++, ct->ct_name);
      htsbuf_qprintf(hq, "#DESCRIPTION %s\n", ct->ct_name);
      for (chidx = 0; chidx < chcount; chidx++) {
        ch = chlist[chidx];
        LIST_FOREACH(ilm, &ct->ct_ctms, ilm_in1_link)
          if (ch == (channel_t *)ilm->ilm_in2) {
            snprintf(buf, sizeof(buf), "/stream/channelid/%d", channel_get_id(ch));
            http_e2_playlist_add(hq, hostpath, buf, profile, channel_get_name(ch));
            break;
          }
      }
    } else if (pltype == PLAYLIST_SATIP_M3U) {
      for (chidx = 0; chidx < chcount; chidx++) {
        ch = chlist[chidx];
        LIST_FOREACH(ilm, &ct->ct_ctms, ilm_in1_link)
          if (ch == (channel_t *)ilm->ilm_in2)
            http_satip_m3u_playlist_add(hq, hostpath, ch);
      }
    }
  }

  free(ctlist);
  free(chlist);

  free(hostpath);
  free(profile);
  return 0;
}


/**
 * Output a flat playlist with all channels
 */
static int
http_channel_list_playlist(http_connection_t *hc, int pltype)
{
  htsbuf_queue_t *hq;
  char buf[255], ubuf[UUID_HEX_SIZE];
  channel_t *ch;
  channel_t **chlist;
  int idx = 0, count = 0;
  char *profile, *hostpath;
  const char *name;

  if(hc->hc_access == NULL ||
     access_verify2(hc->hc_access, ACCESS_STREAMING))
    return HTTP_STATUS_UNAUTHORIZED;

  hq = &hc->hc_reply;

  profile = profile_validate_name(http_arg_get(&hc->hc_req_args, "profile"));
  hostpath = http_get_hostpath(hc);

  CHANNEL_FOREACH(ch)
    if (ch->ch_enabled)
      count++;

  chlist = malloc(count * sizeof(channel_t *));

  CHANNEL_FOREACH(ch)
    if (ch->ch_enabled)
      chlist[idx++] = ch;

  assert(idx == count);

  qsort(chlist, count, sizeof(channel_t *), http_channel_playlist_cmp);

  htsbuf_append_str(hq, pltype == PLAYLIST_E2 ? "#NAME Tvheadend Channels\n" : "#EXTM3U\n");
  for (idx = 0; idx < count; idx++) {
    ch = chlist[idx];

    if (http_access_verify_channel(hc, ACCESS_STREAMING, ch))
      continue;

    name = channel_get_name(ch);
    snprintf(buf, sizeof(buf), "/stream/channelid/%d", channel_get_id(ch));

    if (pltype == PLAYLIST_M3U) {
      http_m3u_playlist_add(hq, hostpath, buf, profile, name,
                            channel_get_icon(ch),
                            channel_get_uuid(ch, ubuf),
                            hc->hc_access);
    } else if (pltype == PLAYLIST_E2) {
      http_e2_playlist_add(hq, hostpath, buf, profile, name);
    } else if (pltype == PLAYLIST_SATIP_M3U) {
      http_satip_m3u_playlist_add(hq, hostpath, ch);
    }
  }

  free(chlist);

  free(hostpath);
  free(profile);
  return 0;
}



/**
 * Output a playlist of all recordings.
 */
static int
http_dvr_list_playlist(http_connection_t *hc, int pltype)
{
  htsbuf_queue_t *hq;
  char buf[255], ubuf[UUID_HEX_SIZE];
  dvr_entry_t *de;
  const char *uuid;
  char *hostpath;
  off_t fsize;
  time_t durration;
  struct tm tm;
  int bandwidth;

  if (pltype != PLAYLIST_M3U)
    return HTTP_STATUS_BAD_REQUEST;

  hq = &hc->hc_reply;
  hostpath = http_get_hostpath(hc);

  htsbuf_append_str(hq, "#EXTM3U\n");
  LIST_FOREACH(de, &dvrentries, de_global_link) {
    fsize = dvr_get_filesize(de, 0);
    if(!fsize)
      continue;

    if (de->de_channel &&
        http_access_verify_channel(hc, ACCESS_RECORDER, de->de_channel))
      continue;

    durration  = dvr_entry_get_stop_time(de) - dvr_entry_get_start_time(de, 0);
    bandwidth = ((8*fsize) / (durration*1024.0));
    strftime(buf, sizeof(buf), "%FT%T%z", localtime_r(&(de->de_start), &tm));

    htsbuf_qprintf(hq, "#EXTINF:%"PRItime_t",%s\n", durration, lang_str_get(de->de_title, NULL));
    
    htsbuf_qprintf(hq, "#EXT-X-TARGETDURATION:%"PRItime_t"\n", durration);
    uuid = idnode_uuid_as_str(&de->de_id, ubuf);
    htsbuf_qprintf(hq, "#EXT-X-STREAM-INF:PROGRAM-ID=%s,BANDWIDTH=%d\n", uuid, bandwidth);
    htsbuf_qprintf(hq, "#EXT-X-PROGRAM-DATE-TIME:%s\n", buf);

    snprintf(buf, sizeof(buf), "/dvrfile/%s", uuid);
    htsbuf_qprintf(hq, "%s%s?ticket=%s\n", hostpath, buf,
       access_ticket_create(buf, hc->hc_access));
  }

  free(hostpath);
  return 0;
}

/**
 * Output a playlist with a http stream for a dvr entry (.m3u format)
 */
static int
http_dvr_playlist(http_connection_t *hc, int pltype, dvr_entry_t *de)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  char buf[255], ubuf[UUID_HEX_SIZE];
  const char *ticket_id = NULL, *uuid;
  time_t durration = 0;
  off_t fsize = 0;
  int bandwidth = 0, ret = 0;
  struct tm tm;  
  char *hostpath;

  if(pltype != PLAYLIST_M3U)
    return HTTP_STATUS_BAD_REQUEST;

  if(http_access_verify(hc, ACCESS_RECORDER))
    return HTTP_STATUS_UNAUTHORIZED;

  if(dvr_entry_verify(de, hc->hc_access, 1))
    return HTTP_STATUS_NOT_FOUND;

  hostpath  = http_get_hostpath(hc);
  durration  = dvr_entry_get_stop_time(de) - dvr_entry_get_start_time(de, 0);
  fsize = dvr_get_filesize(de, 0);

  if(fsize) {
    bandwidth = ((8*fsize) / (durration*1024.0));
    strftime(buf, sizeof(buf), "%FT%T%z", localtime_r(&(de->de_start), &tm));

    htsbuf_append_str(hq, "#EXTM3U\n");
    htsbuf_qprintf(hq, "#EXTINF:%"PRItime_t",%s\n", durration, lang_str_get(de->de_title, NULL));
    
    htsbuf_qprintf(hq, "#EXT-X-TARGETDURATION:%"PRItime_t"\n", durration);
    uuid = idnode_uuid_as_str(&de->de_id, ubuf);
    htsbuf_qprintf(hq, "#EXT-X-STREAM-INF:PROGRAM-ID=%s,BANDWIDTH=%d\n", uuid, bandwidth);
    htsbuf_qprintf(hq, "#EXT-X-PROGRAM-DATE-TIME:%s\n", buf);

    snprintf(buf, sizeof(buf), "/dvrfile/%s", uuid);
    ticket_id = access_ticket_create(buf, hc->hc_access);
    htsbuf_qprintf(hq, "%s%s?ticket=%s\n", hostpath, buf, ticket_id);
  } else {
    ret = HTTP_STATUS_NOT_FOUND;
  }

  free(hostpath);
  return ret;
}


/**
 * Handle requests for playlists.
 */
static int
page_http_playlist(http_connection_t *hc, const char *remain, void *opaque)
{
  char *components[2], *cmd, *s;
  int nc, r, pltype = PLAYLIST_M3U;
  channel_t *ch = NULL;
  dvr_entry_t *de = NULL;
  channel_tag_t *tag = NULL;

  if (remain && !strcmp(remain, "e2")) {
    pltype = PLAYLIST_E2;
    remain = NULL;
  }

  if (remain && !strcmp(remain, "satip")) {
    pltype = PLAYLIST_SATIP_M3U;
    remain = NULL;
  }

  if (remain && !strncmp(remain, "e2/", 3)) {
    pltype = PLAYLIST_E2;
    remain += 3;
  }

  if (remain && !strncmp(remain, "satip/", 6)) {
    pltype = PLAYLIST_SATIP_M3U;
    remain += 6;
  }

  if(!remain || *remain == '\0') {
    http_redirect(hc, pltype == PLAYLIST_E2 ? "/playlist/e2/channels" :
                      (pltype == PLAYLIST_SATIP_M3U ?
                        "/playlist/satip/channels" :
                        "/playlist/channels"),
                      &hc->hc_req_args, 0);
    return HTTP_STATUS_FOUND;
  }

  nc = http_tokenize((char *)remain, components, 2, '/');
  if(!nc)
    return HTTP_STATUS_BAD_REQUEST;

  cmd = tvh_strdupa(components[0]);

  if(nc == 2)
    http_deescape(components[1]);

  pthread_mutex_lock(&global_lock);

  if(nc == 2 && !strcmp(components[0], "channelid"))
    ch = channel_find_by_id(atoi(components[1]));
  else if(nc == 2 && !strcmp(components[0], "channelnumber"))
    ch = channel_find_by_number(components[1]);
  else if(nc == 2 && !strcmp(components[0], "channelname"))
    ch = channel_find_by_name(components[1]);
  else if(nc == 2 && !strcmp(components[0], "channel"))
   ch = channel_find(components[1]);
  else if(nc == 2 && !strcmp(components[0], "dvrid"))
    de = dvr_entry_find_by_id(atoi(components[1]));
  else if(nc == 2 && !strcmp(components[0], "tagid"))
    tag = channel_tag_find_by_identifier(atoi(components[1]));
  else if(nc == 2 && !strcmp(components[0], "tagname"))
    tag = channel_tag_find_by_name(components[1], 0);
  else if(nc == 2 && !strcmp(components[0], "tag")) {
    if (uuid_hexvalid(components[1]))
      tag = channel_tag_find_by_uuid(components[1]);
    else
      tag = channel_tag_find_by_name(components[1], 0);
  }

  if(ch)
    r = http_channel_playlist(hc, pltype, ch);
  else if(tag)
    r = http_tag_playlist(hc, pltype, tag);
  else if(de) {
    if (pltype == PLAYLIST_SATIP_M3U)
      r = HTTP_STATUS_BAD_REQUEST;
    else
      r = http_dvr_playlist(hc, pltype, de);
  } else {
    cmd = s = tvh_strdupa(components[0]);
    while (*s && *s != '.') s++;
    if (*s == '.') {
      *s = '\0';
      s++;
    }
    if (s[0] != '\0' && strcmp(s, "m3u") && strcmp(s, "m3u8"))
      r = HTTP_STATUS_BAD_REQUEST;
    else if(!strcmp(cmd, "tags"))
      r = http_tag_list_playlist(hc, pltype);
    else if(!strcmp(cmd, "channels"))
      r = http_channel_list_playlist(hc, pltype);
    else if(pltype != PLAYLIST_SATIP_M3U &&
            !strcmp(cmd, "recordings"))
      r = http_dvr_list_playlist(hc, pltype);
    else {
      r = HTTP_STATUS_BAD_REQUEST;
    }
  }

  pthread_mutex_unlock(&global_lock);

  if (r == 0)
    http_output_content(hc, pltype == PLAYLIST_E2 ? MIME_E2 : MIME_M3U);

  return r;
}


/**
 * Subscribes to a service and starts the streaming loop
 */
static int
http_stream_service(http_connection_t *hc, service_t *service, int weight)
{
  th_subscription_t *s;
  profile_t *pro;
  profile_chain_t prch;
  const char *str;
  size_t qsize;
  const char *name;
  void *tcp_id;
  int res = HTTP_STATUS_SERVICE;
  int flags, eflags = 0;

  if(http_access_verify(hc, ACCESS_ADVANCED_STREAMING))
    return HTTP_STATUS_UNAUTHORIZED;

  if ((str = http_arg_get(&hc->hc_req_args, "descramble")))
    if (strcmp(str, "0") == 0)
      eflags |= SUBSCRIPTION_NODESCR;

  if ((str = http_arg_get(&hc->hc_req_args, "emm")))
    if (strcmp(str, "1") == 0)
      eflags |= SUBSCRIPTION_EMM;

  flags = SUBSCRIPTION_MPEGTS | eflags;
  if ((eflags & SUBSCRIPTION_NODESCR) == 0)
    flags |= SUBSCRIPTION_PACKET;
  if(!(pro = profile_find_by_list(hc->hc_access->aa_profiles,
                                  http_arg_get(&hc->hc_req_args, "profile"),
                                  "service", flags)))
    return HTTP_STATUS_NOT_ALLOWED;

  if((tcp_id = http_stream_preop(hc)) == NULL)
    return HTTP_STATUS_NOT_ALLOWED;

  if ((str = http_arg_get(&hc->hc_req_args, "qsize")))
    qsize = atoll(str);
  else
    qsize = 1500000;

  profile_chain_init(&prch, pro, service);
  if (!profile_chain_open(&prch, NULL, 0, qsize)) {

    s = subscription_create_from_service(&prch, NULL, weight, "HTTP",
                                         prch.prch_flags | SUBSCRIPTION_STREAMING |
                                           eflags,
                                         hc->hc_peer_ipstr,
				         hc->hc_username,
				         http_arg_get(&hc->hc_args, "User-Agent"),
				         NULL);
    if(s) {
      name = tvh_strdupa(service->s_nicename);
      pthread_mutex_unlock(&global_lock);
      http_stream_run(hc, &prch, name, s);
      pthread_mutex_lock(&global_lock);
      subscription_unsubscribe(s, UNSUBSCRIBE_FINAL);
      res = 0;
    }
  }

  profile_chain_close(&prch);
  http_stream_postop(tcp_id);
  return res;
}

/**
 * Subscribe to a mux for grabbing a raw dump
 *
 * TODO: can't currently force this to be on a particular input
 */
#if ENABLE_MPEGTS
static int
http_stream_mux(http_connection_t *hc, mpegts_mux_t *mm, int weight)
{
  th_subscription_t *s;
  profile_chain_t prch;
  size_t qsize;
  const char *name, *str;
  void *tcp_id;
  char *p, *saveptr = NULL;
  mpegts_apids_t pids;
  mpegts_service_t *ms;
  int res = HTTP_STATUS_SERVICE, i;

  if(http_access_verify(hc, ACCESS_ADVANCED_STREAMING))
    return HTTP_STATUS_UNAUTHORIZED;

  if((tcp_id = http_stream_preop(hc)) == NULL)
    return HTTP_STATUS_NOT_ALLOWED;

  if ((str = http_arg_get(&hc->hc_req_args, "qsize")))
    qsize = atoll(str);
  else
    qsize = 10000000;

  mpegts_pid_init(&pids);
  if ((str = http_arg_get(&hc->hc_req_args, "pids"))) {
    p = tvh_strdupa(str);
    p = strtok_r(p, ",", &saveptr);
    while (p) {
      if (strcmp(p, "all") == 0) {
        pids.all = 1;
      } else {
        i = atoi(p);
        if (i < 0 || i > 8192)
          return HTTP_STATUS_BAD_REQUEST;
        if (i == 8192)
          pids.all = 1;
        else
          mpegts_pid_add(&pids, i, MPS_WEIGHT_RAW);
      }
      p = strtok_r(NULL, ",", &saveptr);
    }
    if (!pids.all && pids.count <= 0)
      return HTTP_STATUS_BAD_REQUEST;
  } else {
    pids.all = 1;
  }

  if (!profile_chain_raw_open(&prch, mm, qsize, 1)) {

    s = subscription_create_from_mux(&prch, NULL, weight ?: 10, "HTTP",
                                     prch.prch_flags |
                                     SUBSCRIPTION_STREAMING,
                                     hc->hc_peer_ipstr, hc->hc_username,
                                     http_arg_get(&hc->hc_args, "User-Agent"),
                                     NULL);
    if (s) {
      name = tvh_strdupa(s->ths_title);
      ms = (mpegts_service_t *)s->ths_service;
      if (ms->s_update_pids(ms, &pids) == 0) {
        pthread_mutex_unlock(&global_lock);
        http_stream_run(hc, &prch, name, s);
        pthread_mutex_lock(&global_lock);
      }
      subscription_unsubscribe(s, UNSUBSCRIBE_FINAL);
      res = 0;
    }
  }

  profile_chain_close(&prch);
  http_stream_postop(tcp_id);

  return res;
}
#endif

/**
 * Subscribes to a channel and starts the streaming loop
 */
static int
http_stream_channel(http_connection_t *hc, channel_t *ch, int weight)
{
  th_subscription_t *s;
  profile_t *pro;
  profile_chain_t prch;
  char *str;
  size_t qsize;
  const char *name;
  void *tcp_id;
  int res = HTTP_STATUS_SERVICE;

  if (http_access_verify_channel(hc, ACCESS_STREAMING, ch))
    return HTTP_STATUS_UNAUTHORIZED;

  if(!(pro = profile_find_by_list(hc->hc_access->aa_profiles,
                                  http_arg_get(&hc->hc_req_args, "profile"),
                                  "channel",
                                  SUBSCRIPTION_PACKET | SUBSCRIPTION_MPEGTS)))
    return HTTP_STATUS_NOT_ALLOWED;

  if((tcp_id = http_stream_preop(hc)) == NULL)
    return HTTP_STATUS_NOT_ALLOWED;

  if ((str = http_arg_get(&hc->hc_req_args, "qsize")))
    qsize = atoll(str);
  else
    qsize = 1500000;

  profile_chain_init(&prch, pro, ch);
  if (!profile_chain_open(&prch, NULL, 0, qsize)) {

    s = subscription_create_from_channel(&prch,
                 NULL, weight, "HTTP",
                 prch.prch_flags | SUBSCRIPTION_STREAMING,
                 hc->hc_peer_ipstr, hc->hc_username,
                 http_arg_get(&hc->hc_args, "User-Agent"),
                 NULL);

    if(s) {
      name = tvh_strdupa(channel_get_name(ch));
      pthread_mutex_unlock(&global_lock);
      http_stream_run(hc, &prch, name, s);
      pthread_mutex_lock(&global_lock);
      subscription_unsubscribe(s, UNSUBSCRIBE_FINAL);
      res = 0;
    }
  }

  profile_chain_close(&prch);
  http_stream_postop(tcp_id);

  return res;
}


/**
 * Handle the http request. http://tvheadend/stream/channelid/<chid>
 *                          http://tvheadend/stream/channel/<uuid>
 *                          http://tvheadend/stream/channelnumber/<channelnumber>
 *                          http://tvheadend/stream/channelname/<channelname>
 *                          http://tvheadend/stream/service/<servicename>
 *                          http://tvheadend/stream/mux/<muxid>
 */
static int
http_stream(http_connection_t *hc, const char *remain, void *opaque)
{
  char *components[2];
  channel_t *ch = NULL;
  service_t *service = NULL;
#if ENABLE_MPEGTS
  mpegts_mux_t *mm = NULL;
#endif
  const char *str;
  int weight = 0;

  hc->hc_keep_alive = 0;

  if(remain == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if(http_tokenize((char *)remain, components, 2, '/') != 2)
    return HTTP_STATUS_BAD_REQUEST;

  http_deescape(components[1]);

  if ((str = http_arg_get(&hc->hc_req_args, "weight")))
    weight = atoi(str);

  scopedgloballock();

  if(!strcmp(components[0], "channelid")) {
    ch = channel_find_by_id(atoi(components[1]));
  } else if(!strcmp(components[0], "channelnumber")) {
    ch = channel_find_by_number(components[1]);
  } else if(!strcmp(components[0], "channelname")) {
    ch = channel_find_by_name(components[1]);
  } else if(!strcmp(components[0], "channel")) {
    ch = channel_find(components[1]);
  } else if(!strcmp(components[0], "service")) {
    service = service_find_by_identifier(components[1]);
#if ENABLE_MPEGTS
  } else if(!strcmp(components[0], "mux")) {
    // TODO: do we want to be able to force starting a particular instance
    mm      = mpegts_mux_find(components[1]);
#endif
  }

  if(ch != NULL) {
    return http_stream_channel(hc, ch, weight);
  } else if(service != NULL) {
    return http_stream_service(hc, service, weight);
#if ENABLE_MPEGTS
  } else if(mm != NULL) {
    return http_stream_mux(hc, mm, weight);
#endif
  } else {
    return HTTP_STATUS_BAD_REQUEST;
  }
}

/**
 * Generate a xspf playlist
 * http://en.wikipedia.org/wiki/XML_Shareable_Playlist_Format
 */
static int
page_xspf(http_connection_t *hc, const char *remain, void *opaque)
{
  size_t maxlen;
  char *buf, *hostpath = http_get_hostpath(hc);
  const char *title, *profile, *ticket, *image;
  size_t len;

  if ((title = http_arg_get(&hc->hc_req_args, "title")) == NULL)
    title = "TVHeadend Stream";
  profile = http_arg_get(&hc->hc_req_args, "profile");
  ticket  = http_arg_get(&hc->hc_req_args, "ticket");
  image   = http_arg_get(&hc->hc_req_args, "image");

  maxlen = strlen(remain) + strlen(title) + 512;
  buf = alloca(maxlen);

  pthread_mutex_lock(&global_lock);
  if (ticket == NULL) {
    snprintf(buf, maxlen, "/%s", remain);
    ticket = access_ticket_create(buf, hc->hc_access);
  }
  snprintf(buf, maxlen, "\
<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n\
<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\">\r\n\
  <trackList>\r\n\
     <track>\r\n\
       <title>%s</title>\r\n\
       <location>%s/%s%s%s%s%s</location>\r\n%s%s%s\
     </track>\r\n\
  </trackList>\r\n\
</playlist>\r\n", title, hostpath, remain,
    profile ? "?profile=" : "", profile ?: "",
    profile ? "&ticket=" : "?ticket=", ticket,
    image ? "       <image>" : "", image ?: "", image ? "</image>\r\n" : "");
  pthread_mutex_unlock(&global_lock);

  len = strlen(buf);
  pthread_mutex_lock(&hc->hc_fd_lock);
  http_send_header(hc, 200, "application/xspf+xml", len, 0, NULL, 10, 0, NULL, NULL);
  tvh_write(hc->hc_fd, buf, len);
  pthread_mutex_unlock(&hc->hc_fd_lock);

  free(hostpath);
  return 0;
}

/**
 * Generate an M3U playlist
 * http://en.wikipedia.org/wiki/M3U
 */
static int
page_m3u(http_connection_t *hc, const char *remain, void *opaque)
{
  size_t maxlen;
  char *buf, *hostpath = http_get_hostpath(hc);
  const char *title, *profile, *ticket;
  size_t len;

  if ((title = http_arg_get(&hc->hc_req_args, "title")) == NULL)
    title = "TVHeadend Stream";
  profile = http_arg_get(&hc->hc_req_args, "profile");
  ticket = http_arg_get(&hc->hc_req_args, "ticket");

  maxlen = strlen(remain) + strlen(title) + 256;
  buf = alloca(maxlen);

  pthread_mutex_lock(&global_lock);
  if (ticket == NULL) {
    snprintf(buf, maxlen, "/%s", remain);
    ticket = access_ticket_create(buf, hc->hc_access);
  }
  snprintf(buf, maxlen, "\
#EXTM3U\r\n\
#EXTINF:-1,%s\r\n\
%s/%s%s%s%s%s\r\n", title, hostpath, remain,
    profile ? "?profile=" : "", profile ?: "",
    profile ? "&ticket=" : "?ticket=", ticket);
  pthread_mutex_unlock(&global_lock);

  len = strlen(buf);
  pthread_mutex_lock(&hc->hc_fd_lock);
  http_send_header(hc, 200, MIME_M3U, len, 0, NULL, 10, 0, NULL, NULL);
  tvh_write(hc->hc_fd, buf, len);
  pthread_mutex_unlock(&hc->hc_fd_lock);

  free(hostpath);
  return 0;
}

static char *
page_play_path_modify(http_connection_t *hc, const char *path, int *cut)
{
  /*
   * For curl, wget and TVHeadend do not send the playlist, stream directly
   */
  const char *agent = http_arg_get(&hc->hc_args, "User-Agent");
  int direct = 0;

  if (agent == NULL)
    direct = 1; /* direct streaming for no user-agent providers */
  else if (strncasecmp(agent, "MPlayer ", 8) == 0)
    direct = 1;
  else if (strncasecmp(agent, "curl/", 5) == 0)
    direct = 1;
  else if (strncasecmp(agent, "wget/", 5) == 0)
    direct = 1;
  else if (strncasecmp(agent, "TVHeadend/", 10) == 0)
    direct = 1;
  else if (strncasecmp(agent, "Lavf/", 5) == 0) /* ffmpeg, libav */
    direct = 1;
  else if (strstr(agent, "shoutcastsource")) /* some media players */
    direct = 1;

  if (direct)
    return strdup(path + 5); /* note: skip the /play */
  return NULL;
}

static int
page_play(http_connection_t *hc, const char *remain, void *opaque)
{
  char *playlist;

  if(remain == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if(hc->hc_access == NULL ||
     access_verify2(hc->hc_access, ACCESS_OR |
                                   ACCESS_STREAMING |
                                   ACCESS_ADVANCED_STREAMING |
                                   ACCESS_RECORDER))
    return HTTP_STATUS_UNAUTHORIZED;

  playlist = http_arg_get(&hc->hc_req_args, "playlist");
  if (playlist) {
    if (strcmp(playlist, "xspf") == 0)
      return page_xspf(hc, remain, opaque);
    if (strcmp(playlist, "m3u") == 0)
      return page_m3u(hc, remain, opaque);
  }
  if (webui_xspf)
    return page_xspf(hc, remain, opaque);
  return page_m3u(hc, remain, opaque);
}

/**
 * Download a recorded file
 */
static int
page_dvrfile(http_connection_t *hc, const char *remain, void *opaque)
{
  int fd, ret;
  struct stat st;
  const char *content = NULL, *range, *filename;
  dvr_entry_t *de;
  char *fname, *charset;
  char *basename;
  char *str, *str0;
  char range_buf[255];
  char *disposition = NULL;
  off_t content_len, chunk;
  intmax_t file_start, file_end;
  void *tcp_id;
  th_subscription_t *sub;
  htsbuf_queue_t q;
#if defined(PLATFORM_LINUX)
  ssize_t r;
#elif defined(PLATFORM_FREEBSD) || defined(PLATFORM_DARWIN)
  off_t r;
#endif
  
  if(remain == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if(hc->hc_access == NULL ||
     (access_verify2(hc->hc_access, ACCESS_OR |
                                    ACCESS_STREAMING |
                                    ACCESS_ADVANCED_STREAMING |
                                    ACCESS_RECORDER)))
    return HTTP_STATUS_UNAUTHORIZED;

  pthread_mutex_lock(&global_lock);

  de = dvr_entry_find_by_uuid(remain);
  if (de == NULL)
    de = dvr_entry_find_by_id(atoi(remain));
  if(de == NULL || (filename = dvr_get_filename(de)) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_NOT_FOUND;
  }
  if(dvr_entry_verify(de, hc->hc_access, 1)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  fname = tvh_strdupa(filename);
  content = muxer_container_filename2mime(fname, 1);
  charset = de->de_config ? de->de_config->dvr_charset_id : NULL;

  pthread_mutex_unlock(&global_lock);

  basename = strrchr(fname, '/');
  if (basename) {
    basename++; /* Skip '/' */
    str0 = intlconv_utf8safestr(intlconv_charset_id("ASCII", 1, 1),
                                basename, strlen(basename) * 3);
    if (str0 == NULL)
      return HTTP_STATUS_INTERNAL;
    htsbuf_queue_init(&q, 0);
    htsbuf_append_and_escape_url(&q, basename);
    str = htsbuf_to_string(&q);
    r = 50 + strlen(str0) + strlen(str);
    disposition = alloca(r);
    snprintf(disposition, r,
             "attachment; filename=\"%s\"; filename*=UTF-8''%s",
             str0, str);
    htsbuf_queue_flush(&q);
    free(str);
    free(str0);
  }

  fd = tvh_open(fname, O_RDONLY, 0);
  if(fd < 0)
    return HTTP_STATUS_NOT_FOUND;

  if(fstat(fd, &st) < 0) {
    close(fd);
    return HTTP_STATUS_NOT_FOUND;
  }

  file_start = 0;
  file_end = st.st_size-1;
  
  range = http_arg_get(&hc->hc_args, "Range");
  if(range != NULL)
    sscanf(range, "bytes=%jd-%jd", &file_start, &file_end);

  //Sanity checks
  if(file_start < 0 || file_start >= st.st_size) {
    close(fd);
    return HTTP_STATUS_OK;
  }
  if(file_end < 0 || file_end >= st.st_size) {
    close(fd);
    return HTTP_STATUS_OK;
  }

  if(file_start > file_end) {
    close(fd);
    return HTTP_STATUS_OK;
  }

  content_len = file_end - file_start+1;
  
  sprintf(range_buf, "bytes %jd-%jd/%jd",
    file_start, file_end, (intmax_t)st.st_size);

  if(file_start > 0)
    if (lseek(fd, file_start, SEEK_SET) != file_start) {
      close(fd);
      return HTTP_STATUS_INTERNAL;
    }

  pthread_mutex_lock(&global_lock);
  tcp_id = http_stream_preop(hc);
  sub = NULL;
  if (tcp_id && !hc->hc_no_output && content_len > 64*1024) {
    sub = subscription_create(NULL, 1, "HTTP",
                              SUBSCRIPTION_NONE, NULL,
                              hc->hc_peer_ipstr, hc->hc_username,
                              http_arg_get(&hc->hc_args, "User-Agent"));
    if (sub == NULL) {
      http_stream_postop(tcp_id);
      tcp_id = NULL;
    } else {
      str = intlconv_to_utf8safestr(charset, fname, strlen(fname) * 3);
      if (str == NULL)
        str = intlconv_to_utf8safestr(intlconv_charset_id("ASCII", 1, 1),
                                      fname, strlen(fname) * 3);
      if (str == NULL)
        str = strdup("error");
      basename = malloc(strlen(str) + 7 + 1);
      strcpy(basename, "file://");
      strcat(basename, str);
      sub->ths_dvrfile = basename;
      free(str);
    }
  }
  pthread_mutex_unlock(&global_lock);
  if (tcp_id == NULL) {
    close(fd);
    return HTTP_STATUS_NOT_ALLOWED;
  }

  pthread_mutex_lock(&hc->hc_fd_lock);
  http_send_header(hc, range ? HTTP_STATUS_PARTIAL_CONTENT : HTTP_STATUS_OK,
       content, content_len, NULL, NULL, 10, 
       range ? range_buf : NULL, disposition, NULL);

  ret = 0;
  if(!hc->hc_no_output) {
    while(content_len > 0) {
      chunk = MIN(1024 * (sub ? 128 : 1024 * 1024), content_len);
#if defined(PLATFORM_LINUX)
      r = sendfile(hc->hc_fd, fd, NULL, chunk);
#elif defined(PLATFORM_FREEBSD)
      sendfile(fd, hc->hc_fd, 0, chunk, NULL, &r, 0);
#elif defined(PLATFORM_DARWIN)
      r = chunk;
      sendfile(fd, hc->hc_fd, 0, NULL, &r, 0);
#endif
      if(r < 0) {
        ret = -1;
        break;
      }
      content_len -= r;
      if (sub) {
        subscription_add_bytes_in(sub, r);
        subscription_add_bytes_out(sub, r);
      }
    }
  }
  pthread_mutex_unlock(&hc->hc_fd_lock);
  close(fd);

  pthread_mutex_lock(&global_lock);
  if (sub)
    subscription_unsubscribe(sub, UNSUBSCRIBE_FINAL);
  http_stream_postop(tcp_id);
  pthread_mutex_unlock(&global_lock);
  return ret;
}

/**
 * Fetch image cache image
 */
/**
 * Static download of a file from the filesystem
 */
static int
page_imagecache(http_connection_t *hc, const char *remain, void *opaque)
{
  uint32_t id;
  int fd;
  char buf[8192];
  struct stat st;
  ssize_t c;

  if(remain == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if(hc->hc_access == NULL ||
     (access_verify2(hc->hc_access, ACCESS_OR |
                                    ACCESS_WEB_INTERFACE |
                                    ACCESS_STREAMING |
                                    ACCESS_ADVANCED_STREAMING |
                                    ACCESS_HTSP_STREAMING |
                                    ACCESS_HTSP_RECORDER |
                                    ACCESS_RECORDER)))
    return HTTP_STATUS_UNAUTHORIZED;

  if(sscanf(remain, "%d", &id) != 1)
    return HTTP_STATUS_BAD_REQUEST;

  /* Fetch details */
  pthread_mutex_lock(&global_lock);
  fd = imagecache_open(id);
  pthread_mutex_unlock(&global_lock);

  /* Check result */
  if (fd < 0)
    return HTTP_STATUS_NOT_FOUND;
  if (fstat(fd, &st)) {
    close(fd);
    return HTTP_STATUS_NOT_FOUND;
  }

  pthread_mutex_lock(&hc->hc_fd_lock);
  http_send_header(hc, 200, NULL, st.st_size, 0, NULL, 10, 0, NULL, NULL);

  while (!hc->hc_no_output) {
    c = read(fd, buf, sizeof(buf));
    if (c <= 0)
      break;
    if (tvh_write(hc->hc_fd, buf, c))
      break;
  }
  pthread_mutex_unlock(&hc->hc_fd_lock);
  close(fd);

  return 0;
}

/**
 *
 */
static void
webui_static_content(const char *http_path, const char *source)
{
  http_path_add(http_path, (void *)source, page_static_file,
    ACCESS_WEB_INTERFACE);
}


/**
 *
 */
static int
favicon(http_connection_t *hc, const char *remain, void *opaque)
{
  return page_static_file(hc, "logo.png", (void *)"src/webui/static/img");
}

/**
 *
 */
static int http_file_test(const char *path)
{
  fb_file *fb = fb_open(path, 0, 0);
  if (fb) {
    fb_close(fb);
    return 0;
  }
  return -1;
}

/**
 *
 */
static int
http_redir(http_connection_t *hc, const char *remain, void *opaque)
{
  const char *lang, *theme;
  char *components[3];
  char buf[256];
  int nc;

  if (!remain)
    return HTTP_STATUS_BAD_REQUEST;
  nc = http_tokenize((char *)remain, components, 3, '/');
  if(!nc)
    return HTTP_STATUS_BAD_REQUEST;

  if (nc == 1) {
    if (!strcmp(components[0], "locale.js")) {
      lang = tvh_gettext_get_lang(hc->hc_access->aa_lang_ui);
      if (lang) {
        snprintf(buf, sizeof(buf), "src/webui/static/intl/tvh.%s.js.gz", lang);
        if (!http_file_test(buf)) {
          snprintf(buf, sizeof(buf), "/static/intl/tvh.%s.js.gz", lang);
          http_redirect(hc, buf, NULL, 0);
          return 0;
        }
      }
      snprintf(buf, sizeof(buf), "tvh_locale={};tvh_locale_lang='';");
      pthread_mutex_lock(&hc->hc_fd_lock);
      http_send_header(hc, 200, "text/javascript; charset=UTF-8", strlen(buf), 0, NULL, 10, 0, NULL, NULL);
      tvh_write(hc->hc_fd, buf, strlen(buf));
      pthread_mutex_unlock(&hc->hc_fd_lock);
      return 0;
    }
    if (!strcmp(components[0], "theme.css")) {
      theme = access_get_theme(hc->hc_access);
      if (theme) {
        snprintf(buf, sizeof(buf), "src/webui/static/tvh.%s.css.gz", theme);
        if (!http_file_test(buf)) {
          snprintf(buf, sizeof(buf), "/static/tvh.%s.css.gz", theme);
          http_css_import(hc, buf);
          return 0;
        }
      }
      return HTTP_STATUS_BAD_REQUEST;
    }
    if (!strcmp(components[0], "theme.debug.css")) {
      theme = access_get_theme(hc->hc_access);
      if (theme) {
        snprintf(buf, sizeof(buf), "src/webui/static/extjs/resources/css/xtheme-%s.css", theme);
        if (!http_file_test(buf)) {
          snprintf(buf, sizeof(buf), "/static/extjs/resources/css/xtheme-%s.css", theme);
          http_css_import(hc, buf);
          return 0;
        }
      }
      return HTTP_STATUS_BAD_REQUEST;
    }
    if (!strcmp(components[0], "theme.app.debug.css")) {
      theme = access_get_theme(hc->hc_access);
      if (theme) {
        snprintf(buf, sizeof(buf), "src/webui/static/app/ext-%s.css", theme);
        if (!http_file_test(buf)) {
          snprintf(buf, sizeof(buf), "/static/app/ext-%s.css", theme);
          http_css_import(hc, buf);
          return 0;
        }
      }
      return HTTP_STATUS_BAD_REQUEST;
    }
  }

  return HTTP_STATUS_BAD_REQUEST;
}

int page_statedump(http_connection_t *hc, const char *remain, void *opaque);

/**
 * WEB user interface
 */
void
webui_init(int xspf)
{
  const char *s;

  webui_xspf = xspf;

  if (tvheadend_webui_debug)
    tvhinfo(LS_WEBUI, "Running web interface in debug mode");

  s = tvheadend_webroot;
  tvheadend_webroot = NULL;
  http_path_add("", NULL, page_no_webroot, ACCESS_WEB_INTERFACE);
  tvheadend_webroot = s;

  http_path_add("", NULL, page_root2, ACCESS_WEB_INTERFACE);
  http_path_add("/", NULL, page_root, ACCESS_WEB_INTERFACE);
  http_path_add("/login", NULL, page_login, ACCESS_WEB_INTERFACE);
  http_path_add("/logout", NULL, page_logout, ACCESS_WEB_INTERFACE);

#if CONFIG_SATIP_SERVER
  http_path_add("/satip_server", NULL, satip_server_http_page, ACCESS_ANONYMOUS);
#endif

  http_path_add_modify("/play", NULL, page_play, ACCESS_ANONYMOUS, page_play_path_modify);
  http_path_add("/dvrfile", NULL, page_dvrfile, ACCESS_ANONYMOUS);
  http_path_add("/favicon.ico", NULL, favicon, ACCESS_WEB_INTERFACE);
  http_path_add("/playlist", NULL, page_http_playlist, ACCESS_ANONYMOUS);
  http_path_add("/xmltv", NULL, page_xmltv, ACCESS_ANONYMOUS);
  http_path_add("/markdown", NULL, page_markdown, ACCESS_ANONYMOUS);

  http_path_add("/state", NULL, page_statedump, ACCESS_ADMIN);

  http_path_add("/stream",  NULL, http_stream,  ACCESS_ANONYMOUS);

  http_path_add("/imagecache", NULL, page_imagecache, ACCESS_ANONYMOUS);

  http_path_add("/redir",  NULL, http_redir, ACCESS_ANONYMOUS);

  webui_static_content("/static",        "src/webui/static");

  simpleui_start();
  extjs_start();
  comet_init();
  webui_api_init();
}

void
webui_done(void)
{
  comet_done();
}
