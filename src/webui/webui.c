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

#include <sys/stat.h>
#include <fcntl.h>

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

enum {
  PLAYLIST_M3U,
  PLAYLIST_E2,
  PLAYLIST_SATIP_M3U\
};

enum {
  URLAUTH_NONE,
  URLAUTH_TICKET,
  URLAUTH_CODE
};

static int webui_xspf;

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
 *
 */
static const char *
page_playlist_authpath(int urlauth)
{
  switch (urlauth) {
  case URLAUTH_NONE:    return "";
  case URLAUTH_TICKET:  return "/ticket";
  case URLAUTH_CODE:    return "/auth";
  default: assert(0);   return "";
  };
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
  if (!tvheadend_webroot) return HTTP_STATUS_NOT_FOUND;
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
      *hc->hc_access->aa_username != '\0') {
    http_redirect(hc, "/", &hc->hc_req_args, 0);
    return 0;
  } else {
    return http_noaccess_code(hc);
  }
}

static int
page_logout(http_connection_t *hc, const char *remain, void *opaque)
{
  const char *username, *busername, *lang, *title, *text, *logout;
  char url[512];

  username = hc->hc_access ? hc->hc_access->aa_username : NULL;
  busername = hc->hc_username ? hc->hc_username : NULL;

  tvhtrace(LS_HTTP, "logout: username '%s', busername '%s'\n", username, busername);

  if (http_arg_get(&hc->hc_req_args, "_logout"))
    return HTTP_STATUS_UNAUTHORIZED;

  if (!http_arg_get(&hc->hc_args, "Authorization"))
    return HTTP_STATUS_UNAUTHORIZED;

  lang = hc->hc_access ? hc->hc_access->aa_lang_ui : NULL;
  title = tvh_gettext_lang(lang, N_("Logout"));
  htsbuf_qprintf(&hc->hc_reply,
                 "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
                 "<HTML><HEAD>\r\n"
                 "<TITLE>%s</TITLE>\r\n"
                 "</HEAD><BODY>\r\n"
                 "<H1>%s</H1>\r\n"
                 "<P>",
                 title, title);

  text = tvh_gettext_lang(lang, N_("Authenticated user"));
  htsbuf_qprintf(&hc->hc_reply, "<P>%s: %s</P>\r\n", text, username ?: "---");

  text = tvh_gettext_lang(lang, N_("\
Please, follow %s link and cancel the next authorization to correctly clear \
the cached browser credentals (login and password cache). Then click to \
the 'Default login' (anonymous access) or 'New login' link in the error page \
to reauthenticate."));
  logout = tvh_gettext_lang(lang, N_("logout"));

  snprintf(url, sizeof(url), "<A HREF=\"%s/logout?_logout=1\">%s</A>",
                             tvheadend_webroot ? tvheadend_webroot : "", logout);

  htsbuf_qprintf(&hc->hc_reply, text, url);

  snprintf(url, sizeof(url), "<A HREF=\"%s/logout?_logout=1\" "
                             "STYLE=\"border: 1px solid; border-radius: 4px; padding: .6em\">%s</A>",
                             tvheadend_webroot ? tvheadend_webroot : "", logout);

  text = tvh_gettext_lang(lang, N_("return"));

  htsbuf_qprintf(&hc->hc_reply, "</P>\r\n"
                                "<P STYLE=\"text-align: center; margin: 2em\">%s</P>\r\n"
                                "<P STYLE=\"text-align: center; margin: 2em\"><A HREF=\"%s/\" STYLE=\"border: 1px solid; border-radius: 4px; padding: .6em\">%s</A></P>\r\n"
                                "</BODY></HTML>\r\n",
                                url, tvheadend_webroot ? tvheadend_webroot : "", text);
  http_output_html(hc);
  return 0;
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
  int maxage = 10;              /* Default age */

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
    else if(!strcmp(postfix, "jpg") || !strcmp(postfix, "png")) {
      nogzip = 1;
      /* Increase amount of time in seconds that a client can cache
       * images since they rarely change. This avoids clients
       * requesting category icons frequently.
       */
      maxage = 60 * 60;
    }
  }

  fb_file *fp = fb_open(path, 0, (nogzip || gzip) ? 0 : 1);
  if (!fp) {
    tvherror(LS_WEBUI, "failed to open %s", path);
    return HTTP_STATUS_INTERNAL;
  }
  size = fb_size(fp);
  if (!gzip && fb_gzipped(fp))
    gzip = "gzip";

  http_send_begin(hc);
  http_send_header(hc, 200, content, size, gzip, NULL, maxage, 0, NULL, NULL);
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
  http_send_end(hc);
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
  char buf[128];
  const char *username;

  htsmsg_add_str(m, "type", "HTTP");
  if (hc->hc_proxy_ip) {
    tcp_get_str_from_ip(hc->hc_proxy_ip, buf, sizeof(buf));
    htsmsg_add_str(m, "proxy", buf);
  }
  username = http_username(hc);
  if (username)
    htsmsg_add_str(m, "user", username);
}

static inline void *
http_stream_preop ( http_connection_t *hc )
{
  return tcp_connection_launch(hc->hc_fd, 1, http_stream_status, hc->hc_access);
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
    tvh_mutex_lock(&sq->sq_mutex);
    sq->sq_maxsize = 100000;
    tvh_mutex_unlock(&sq->sq_mutex);
  }

  while(!hc->hc_shutdown && run && tvheadend_is_running()) {
    tvh_mutex_lock(&sq->sq_mutex);
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
      tvh_mutex_unlock(&sq->sq_mutex);
      continue;
    }

    streaming_queue_remove(sq, sm);
    tvh_mutex_unlock(&sq->sq_mutex);

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
      if((mux->m_caps & MC_CAP_ANOTHER_SERVICE) != 0) /* give a chance to use another svc */
        break;
      if(sm->sm_code != SM_CODE_SOURCE_RECONFIGURED) {
        tvhwarn(LS_WEBUI,  "Stop streaming %s, %s", hc->hc_url_orig, 
                streaming_code2txt(sm->sm_code));
        run = 0;
      }
      break;

    case SMT_SERVICE_STATUS:
    case SMT_SIGNAL_STATUS:
    case SMT_DESCRAMBLE_INFO:
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
                      const char *url_remain, const char *type,
                      const char *profile,
                      const char *svcname, const char *chnum,
                      const char *logo, const char *epgid,
                      int urlauth, access_t *access)
{
  const char *delim = "?", *ticket = NULL;
  if (urlauth == URLAUTH_TICKET)
    ticket = access_ticket_create(url_remain, access);
  htsbuf_append_str(hq, "#EXTINF:-1");
  if (type)
    htsbuf_qprintf(hq, " type=\"%s\"", type);
  if (!strempty(logo)) {
    int id = imagecache_get_id(logo);
    if (id) {
      htsbuf_qprintf(hq, " logo=\"%s/imagecache/%d", hostpath, id);
      switch (urlauth) {
      case URLAUTH_NONE:
        break;
      case URLAUTH_TICKET:
        htsbuf_qprintf(hq, "?ticket=%s", ticket);
        break;
      case URLAUTH_CODE:
        if (!strempty(access->aa_auth))
          htsbuf_qprintf(hq, "?auth=%s", access->aa_auth);
        break;
      }
      htsbuf_append_str(hq, "\"");
    } else {
      htsbuf_qprintf(hq, " logo=\"%s\"", logo);
    }
  }
  if (epgid)
    htsbuf_qprintf(hq, " tvg-id=\"%s\"", epgid);
  if (chnum)
    htsbuf_qprintf(hq, " tvg-chno=\"%s\"", chnum);
  htsbuf_qprintf(hq, ",%s\n%s%s", svcname, hostpath, url_remain);
  switch (urlauth) {
  case URLAUTH_NONE:
    break;
  case URLAUTH_TICKET:
    htsbuf_qprintf(hq, "%sticket=%s", delim, ticket);
    delim = "&";
    break;
  case URLAUTH_CODE:
    if (!strempty(access->aa_auth)) {
      htsbuf_qprintf(hq, "%sauth=%s", delim, access->aa_auth);
      delim = "&";
    }
    break;
  }
  htsbuf_qprintf(hq, "%sprofile=%s\n", delim, profile);
}

/*
 *
 */
static void
http_e2_playlist_add(htsbuf_queue_t *hq, const char *hostpath,
                     const char *url_remain, const char *profile,
                     const char *svcname, int urlauth, access_t *access)
{
  htsbuf_append_str(hq, "#SERVICE 1:0:0:0:0:0:0:0:0:0:");
  htsbuf_append_and_escape_url(hq, hostpath);
  htsbuf_append_and_escape_url(hq, url_remain);
  htsbuf_qprintf(hq, "?profile=%s", profile);
  if (urlauth == URLAUTH_CODE && !strempty(access->aa_auth))
    htsbuf_qprintf(hq, "&auth=%s", access->aa_auth);
  htsbuf_qprintf(hq, ":%s\n#DESCRIPTION %s\n", svcname, svcname);
}

/*
 *
 */
static void
http_satip_m3u_playlist_add(htsbuf_queue_t *hq, const char *hostpath,
                            channel_t *ch, const char *blank,
                            int urlauth, access_t *access)
{
  char buf[64];
  const char *name, *logo;
  idnode_list_mapping_t *ilm;
  service_t *s = NULL;
  int id, src;

  LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
    /* cannot handle channels with more services for SAT>IP */
    if (s)
      return;
    s = (service_t *)ilm->ilm_in1;
  }
  src = (s && s->s_satip_source) ? s->s_satip_source(s) : -1;
  if (src < 1)
    return;
  name = channel_get_name(ch, blank);
  logo = channel_get_icon(ch);
  id = imagecache_get_id(logo);
  snprintf(buf, sizeof(buf), "/stream/channelid/%d", channel_get_id(ch));
  htsbuf_append_str(hq, "#EXTINF:-1");
  if (id) {
    htsbuf_qprintf(hq, " logo=%s/imagecache/%d", hostpath, id);
    if (urlauth == URLAUTH_CODE && !strempty(access->aa_auth))
      htsbuf_qprintf(hq, "?auth=%s", access->aa_auth);
  } else if (!strempty(logo)) {
    htsbuf_qprintf(hq, " logo=%s", logo);
  }
  htsbuf_qprintf(hq, ",%s\n%s%s?profile=pass", name, hostpath, buf);
  if (urlauth == URLAUTH_CODE && !strempty(access->aa_auth))
    htsbuf_qprintf(hq, "&auth=%s", access->aa_auth);
  htsbuf_append_str(hq, "\n");
}

/**
 * Output a playlist containing a single channel
 */
static int
http_channel_playlist(http_connection_t *hc, int pltype, int urlauth, channel_t *channel)
{
  htsbuf_queue_t *hq;
  char buf[255], hostpath[512], chnum[32];
  char *profile;
  const char *name, *blank;
  const char *lang = hc->hc_access->aa_lang_ui;
  char ubuf[UUID_HEX_SIZE];

  if (http_access_verify_channel(hc, ACCESS_STREAMING, channel))
    return http_noaccess_code(hc);

  profile = profile_validate_name(http_arg_get(&hc->hc_req_args, "profile"));
  http_get_hostpath(hc, hostpath, sizeof(hostpath));

  hq = &hc->hc_reply;

  snprintf(buf, sizeof(buf), "/stream/channelid/%d", channel_get_id(channel));

  blank = tvh_gettext_lang(lang, channel_blank_name);
  name = channel_get_name(channel, blank);

  if (pltype == PLAYLIST_M3U) {

    htsbuf_append_str(hq, "#EXTM3U\n");
    http_m3u_playlist_add(hq, hostpath, buf, NULL, profile, name,
                          channel_get_number_as_str(channel, chnum, sizeof(chnum)),
                          channel_get_icon(channel),
                          channel_get_uuid(channel, ubuf),
                          urlauth, hc->hc_access);

  } else if (pltype == PLAYLIST_E2) {

    htsbuf_qprintf(hq, "#NAME %s\n", name);
    http_e2_playlist_add(hq, hostpath, buf, profile, name, urlauth, hc->hc_access);

  } else if (pltype == PLAYLIST_SATIP_M3U) {

    http_satip_m3u_playlist_add(hq, hostpath, channel, blank, urlauth, hc->hc_access);

  }

  free(profile);
  return 0;
}


/**
 * Output a playlist containing all channels with a specific tag
 */
static int
http_tag_playlist(http_connection_t *hc, int pltype, int urlauth, channel_tag_t *tag)
{
  htsbuf_queue_t *hq;
  char buf[255], hostpath[512], chnum[32], ubuf[UUID_HEX_SIZE];
  char *profile;
  const char *name, *blank, *sort, *lang;
  channel_t *ch;
  channel_t **chlist;
  int idx, count = 0;

  if (access_verify2(hc->hc_access, ACCESS_STREAMING))
    return http_noaccess_code(hc);

  lang = hc->hc_access->aa_lang_ui;
  hq = &hc->hc_reply;
  profile = profile_validate_name(http_arg_get(&hc->hc_req_args, "profile"));
  http_get_hostpath(hc, hostpath, sizeof(hostpath));
  sort = http_arg_get(&hc->hc_req_args, "sort");
  chlist = channel_get_sorted_list_for_tag(sort, tag, &count);

  if (pltype == PLAYLIST_M3U)
    htsbuf_append_str(hq, "#EXTM3U\n");
  else if (pltype == PLAYLIST_E2)
    htsbuf_qprintf(hq, "#NAME %s\n", tag->ct_name);
  blank = tvh_gettext_lang(lang, channel_blank_name);
  for (idx = 0; idx < count; idx++) {
    ch = chlist[idx];
    if (http_access_verify_channel(hc, ACCESS_STREAMING, ch))
      continue;
    snprintf(buf, sizeof(buf), "/stream/channelid/%d", channel_get_id(ch));
    name = channel_get_name(ch, blank);
    if (pltype == PLAYLIST_M3U) {
      http_m3u_playlist_add(hq, hostpath, buf, NULL, profile, name,
                            channel_get_number_as_str(ch, chnum, sizeof(chnum)),
                            channel_get_icon(ch),
                            channel_get_uuid(ch, ubuf),
                            urlauth, hc->hc_access);
    } else if (pltype == PLAYLIST_E2) {
      htsbuf_qprintf(hq, "#NAME %s\n", name);
      http_e2_playlist_add(hq, hostpath, buf, profile, name, urlauth, hc->hc_access);
    } else if (pltype == PLAYLIST_SATIP_M3U) {
      http_satip_m3u_playlist_add(hq, hostpath, ch, blank, urlauth, hc->hc_access);
    }
  }

  free(chlist);
  free(profile);
  return 0;
}


/**
 * Output a playlist pointing to tag-specific playlists
 */
static int
http_tag_list_playlist(http_connection_t *hc, int pltype, int urlauth)
{
  htsbuf_queue_t *hq;
  char buf[255], hostpath[512];
  channel_tag_t *ct;
  channel_tag_t **ctlist;
  channel_t *ch;
  channel_t **chlist;
  int labelidx = 0;
  int idx, count = 0;
  int chidx, chcount = 0;
  char *profile;
  const char *blank, *sort, *lang;
  idnode_list_mapping_t *ilm;

  if (access_verify2(hc->hc_access, ACCESS_STREAMING))
    return http_noaccess_code(hc);

  lang = hc->hc_access->aa_lang_ui;
  hq = &hc->hc_reply;
  profile = profile_validate_name(http_arg_get(&hc->hc_req_args, "profile"));
  http_get_hostpath(hc, hostpath, sizeof(hostpath));
  sort = http_arg_get(&hc->hc_req_args, "sort");
  ctlist = channel_tag_get_sorted_list(sort, &count);

  if (pltype == PLAYLIST_E2 || pltype == PLAYLIST_SATIP_M3U) {
    chlist = channel_get_sorted_list(sort, 0, &chcount);
  } else {
    chlist = NULL;
  }

  blank = tvh_gettext_lang(lang, channel_blank_name);
  htsbuf_append_str(hq, pltype == PLAYLIST_E2 ? "#NAME Tvheadend Channels\n" : "#EXTM3U\n");
  for (idx = 0; idx < count; idx++) {
    ct = ctlist[idx];
    if (pltype == PLAYLIST_M3U) {
      snprintf(buf, sizeof(buf), "/playlist/tagid/%d", idnode_get_short_uuid(&ct->ct_id));
      http_m3u_playlist_add(hq, hostpath, buf, "playlist",
                            profile, ct->ct_name, NULL,
                            channel_tag_get_icon(ct),
                            NULL, urlauth, hc->hc_access);
    } else if (pltype == PLAYLIST_E2) {
      htsbuf_qprintf(hq, "#SERVICE 1:64:%d:0:0:0:0:0:0:0::%s\n", labelidx++, ct->ct_name);
      htsbuf_qprintf(hq, "#DESCRIPTION %s\n", ct->ct_name);
      for (chidx = 0; chidx < chcount; chidx++) {
        ch = chlist[chidx];
        LIST_FOREACH(ilm, &ct->ct_ctms, ilm_in1_link)
          if (ch == (channel_t *)ilm->ilm_in2) {
            snprintf(buf, sizeof(buf), "/stream/channelid/%d", channel_get_id(ch));
            http_e2_playlist_add(hq, hostpath, buf, profile, channel_get_name(ch, blank), urlauth, hc->hc_access);
            break;
          }
      }
    } else if (pltype == PLAYLIST_SATIP_M3U) {
      for (chidx = 0; chidx < chcount; chidx++) {
        ch = chlist[chidx];
        LIST_FOREACH(ilm, &ct->ct_ctms, ilm_in1_link)
          if (ch == (channel_t *)ilm->ilm_in2)
            http_satip_m3u_playlist_add(hq, hostpath, ch, blank, urlauth, hc->hc_access);
      }
    }
  }

  free(ctlist);
  free(chlist);

  free(profile);
  return 0;
}


/**
 * Output a flat playlist with all channels
 */
static int
http_channel_list_playlist(http_connection_t *hc, int pltype, int urlauth)
{
  htsbuf_queue_t *hq;
  char buf[255], hostpath[512], chnum[32], ubuf[UUID_HEX_SIZE];
  channel_t *ch;
  channel_t **chlist;
  int idx = 0, count = 0;
  char *profile;
  const char *name, *blank, *sort, *lang;

  if (access_verify2(hc->hc_access, ACCESS_STREAMING))
    return http_noaccess_code(hc);

  hq = &hc->hc_reply;

  lang = hc->hc_access->aa_lang_ui;
  profile = profile_validate_name(http_arg_get(&hc->hc_req_args, "profile"));
  http_get_hostpath(hc, hostpath, sizeof(hostpath));
  sort = http_arg_get(&hc->hc_req_args, "sort");
  chlist = channel_get_sorted_list(sort, 0, &count);
  blank = tvh_gettext_lang(lang, channel_blank_name);

  htsbuf_append_str(hq, pltype == PLAYLIST_E2 ? "#NAME Tvheadend Channels\n" : "#EXTM3U\n");
  for (idx = 0; idx < count; idx++) {
    ch = chlist[idx];

    if (http_access_verify_channel(hc, ACCESS_STREAMING, ch))
      continue;

    name = channel_get_name(ch, blank);
    snprintf(buf, sizeof(buf), "/stream/channelid/%d", channel_get_id(ch));

    if (pltype == PLAYLIST_M3U) {
      http_m3u_playlist_add(hq, hostpath, buf, NULL, profile, name,
                            channel_get_number_as_str(ch, chnum, sizeof(chnum)),
                            channel_get_icon(ch),
                            channel_get_uuid(ch, ubuf),
                            urlauth, hc->hc_access);
    } else if (pltype == PLAYLIST_E2) {
      http_e2_playlist_add(hq, hostpath, buf, profile, name, urlauth, hc->hc_access);
    } else if (pltype == PLAYLIST_SATIP_M3U) {
      http_satip_m3u_playlist_add(hq, hostpath, ch, blank, urlauth, hc->hc_access);
    }
  }

  free(chlist);
  free(profile);
  return 0;
}



/**
 * Output a playlist of all recordings.
 */
static int
http_dvr_list_playlist(http_connection_t *hc, int pltype, int urlauth)
{
  htsbuf_queue_t *hq;
  char buf[255], hostpath[512], ubuf[UUID_HEX_SIZE];
  dvr_entry_t *de;
  const char *uuid;
  off_t fsize;
  time_t durration;
  struct tm tm;
  int bandwidth;

  if (pltype != PLAYLIST_M3U)
    return HTTP_STATUS_BAD_REQUEST;

  hq = &hc->hc_reply;
  http_get_hostpath(hc, hostpath, sizeof(hostpath));

  htsbuf_append_str(hq, "#EXTM3U\n");
  LIST_FOREACH(de, &dvrentries, de_global_link) {
    fsize = dvr_get_filesize(de, 0);
    if(!fsize)
      continue;

    if (de->de_channel &&
        http_access_verify_channel(hc, ACCESS_RECORDER, de->de_channel))
      continue;

    if (hc->hc_access == NULL)
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
    htsbuf_qprintf(hq, "%s%s", hostpath, buf);

    switch (urlauth) {
    case URLAUTH_TICKET:
      htsbuf_qprintf(hq, "?ticket=%s\n", access_ticket_create(buf, hc->hc_access));
      break;
    case URLAUTH_CODE:
      if (!strempty(hc->hc_access->aa_auth))
        htsbuf_qprintf(hq, "?auth=%s\n", hc->hc_access->aa_auth);
      break;
    }
    
    htsbuf_append_str(hq, "\n");
  }

  return 0;
}

/**
 * Output a playlist with a http stream for a dvr entry (.m3u format)
 */
static int
http_dvr_playlist(http_connection_t *hc, int pltype, int urlauth, dvr_entry_t *de)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  char buf[255], hostpath[512], ubuf[UUID_HEX_SIZE];
  const char *ticket_id = NULL, *uuid;
  time_t durration = 0;
  off_t fsize = 0;
  int bandwidth = 0, ret = 0;
  struct tm tm;  

  if(pltype != PLAYLIST_M3U)
    return HTTP_STATUS_BAD_REQUEST;

  if(http_access_verify(hc, ACCESS_RECORDER))
    return http_noaccess_code(hc);

  if(dvr_entry_verify(de, hc->hc_access, 1))
    return HTTP_STATUS_NOT_FOUND;

  http_get_hostpath(hc, hostpath, sizeof(hostpath));
  durration = dvr_entry_get_stop_time(de) - dvr_entry_get_start_time(de, 0);
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
    htsbuf_qprintf(hq, "%s%s", hostpath, buf);

    switch (urlauth) {
    case URLAUTH_TICKET:
      ticket_id = access_ticket_create(buf, hc->hc_access);
      htsbuf_qprintf(hq, "?ticket=%s", ticket_id);
      break;
    case URLAUTH_CODE:
      if (!strempty(hc->hc_access->aa_auth))
        htsbuf_qprintf(hq, "?auth=%s", hc->hc_access->aa_auth);
      break;
    }
    htsbuf_append_str(hq, "\n");
  } else {
    ret = HTTP_STATUS_NOT_FOUND;
  }

  return ret;
}


/**
 * Handle requests for playlists.
 */
static int
page_http_playlist_
  (http_connection_t *hc, const char *remain, void *opaque, int urlauth)
{
  char *components[2], *cmd, *s, buf[40];
  const char *cs;
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

  if (!remain || *remain == '\0') {
    switch (pltype) {
    case PLAYLIST_E2:        cs = "e2/channels"; break;
    case PLAYLIST_SATIP_M3U: cs = "satip/channels"; break;
    default:                 cs = "channels"; break;
    }
    snprintf(buf, sizeof(buf), "/playlist%s/%s", page_playlist_authpath(urlauth), cs);
    http_redirect(hc, buf, &hc->hc_req_args, 0);
    return HTTP_STATUS_FOUND;
  }

  nc = http_tokenize((char *)remain, components, 2, '/');
  if(!nc)
    return HTTP_STATUS_BAD_REQUEST;

  cmd = tvh_strdupa(components[0]);

  if(nc == 2)
    http_deescape(components[1]);

  tvh_mutex_lock(&global_lock);

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
    tag = channel_tag_find_by_id(atoi(components[1]));
  else if(nc == 2 && !strcmp(components[0], "tagname"))
    tag = channel_tag_find_by_name(components[1], 0);
  else if(nc == 2 && !strcmp(components[0], "tag")) {
    if (uuid_hexvalid(components[1]))
      tag = channel_tag_find_by_uuid(components[1]);
    else
      tag = channel_tag_find_by_name(components[1], 0);
  }

  if(ch)
    r = http_channel_playlist(hc, pltype, urlauth, ch);
  else if(tag)
    r = http_tag_playlist(hc, pltype, urlauth, tag);
  else if(de) {
    if (pltype == PLAYLIST_SATIP_M3U)
      r = HTTP_STATUS_BAD_REQUEST;
    else
      r = http_dvr_playlist(hc, pltype, urlauth, de);
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
      r = http_tag_list_playlist(hc, pltype, urlauth);
    else if(!strcmp(cmd, "channels"))
      r = http_channel_list_playlist(hc, pltype, urlauth);
    else if(pltype != PLAYLIST_SATIP_M3U &&
            !strcmp(cmd, "recordings"))
      r = http_dvr_list_playlist(hc, pltype, urlauth);
    else {
      r = HTTP_STATUS_BAD_REQUEST;
    }
  }

  tvh_mutex_unlock(&global_lock);

  if (r == 0)
    http_output_content(hc, pltype == PLAYLIST_E2 ? MIME_E2 : MIME_M3U);

  return r;
}

static int
page_http_playlist
  (http_connection_t *hc, const char *remain, void *opaque)
{
  return page_http_playlist_(hc, remain, opaque, URLAUTH_NONE);
}

static int
page_http_playlist_ticket
  (http_connection_t *hc, const char *remain, void *opaque)
{
  return page_http_playlist_(hc, remain, opaque, URLAUTH_TICKET);
}

static int
page_http_playlist_auth
  (http_connection_t *hc, const char *remain, void *opaque)
{
  if (hc->hc_access == NULL || strempty(hc->hc_access->aa_auth))
    return http_noaccess_code(hc);
  return page_http_playlist_(hc, remain, opaque, URLAUTH_CODE);
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
  muxer_hints_t *hints;
  const char *str;
  size_t qsize;
  const char *name;
  void *tcp_id;
  int res = HTTP_STATUS_SERVICE;
  int flags, eflags = 0;

  if(http_access_verify(hc, ACCESS_ADVANCED_STREAMING))
    return http_noaccess_code(hc);

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

  hints = muxer_hints_create(http_arg_get(&hc->hc_args, "User-Agent"));

  profile_chain_init(&prch, pro, service, 1);
  if (!profile_chain_open(&prch, NULL, hints, 0, qsize)) {

    s = subscription_create_from_service(&prch, NULL, weight, "HTTP",
                                         prch.prch_flags | SUBSCRIPTION_STREAMING |
                                           eflags,
                                         hc->hc_peer_ipstr,
				         http_username(hc),
				         http_arg_get(&hc->hc_args, "User-Agent"),
				         NULL);
    if(s) {
      name = tvh_strdupa(service->s_nicename);
      tvh_mutex_unlock(&global_lock);
      http_stream_run(hc, &prch, name, s);
      tvh_mutex_lock(&global_lock);
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
    return http_noaccess_code(hc);

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
                                     hc->hc_peer_ipstr, http_username(hc),
                                     http_arg_get(&hc->hc_args, "User-Agent"),
                                     NULL);
    if (s) {
      name = tvh_strdupa(s->ths_title);
      ms = (mpegts_service_t *)s->ths_service;
      if (ms->s_update_pids(ms, &pids) == 0) {
        tvh_mutex_unlock(&global_lock);
        http_stream_run(hc, &prch, name, s);
        tvh_mutex_lock(&global_lock);
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
  muxer_hints_t *hints;
  char *str;
  size_t qsize;
  const char *name;
  void *tcp_id;
  int res = HTTP_STATUS_SERVICE;

  if (http_access_verify_channel(hc, ACCESS_STREAMING, ch))
    return http_noaccess_code(hc);

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

  hints = muxer_hints_create(http_arg_get(&hc->hc_args, "User-Agent"));

  profile_chain_init(&prch, pro, ch, 1);
  if (!profile_chain_open(&prch, NULL, hints, 0, qsize)) {

    s = subscription_create_from_channel(&prch,
                 NULL, weight, "HTTP",
                 prch.prch_flags | SUBSCRIPTION_STREAMING,
                 hc->hc_peer_ipstr, http_username(hc),
                 http_arg_get(&hc->hc_args, "User-Agent"),
                 NULL);

    if(s) {
      name = tvh_strdupa(channel_get_name(ch, channel_blank_name));
      tvh_mutex_unlock(&global_lock);
      http_stream_run(hc, &prch, name, s);
      tvh_mutex_lock(&global_lock);
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
  int weight = 0, r;

  hc->hc_keep_alive = 0;

  if(remain == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if(http_tokenize((char *)remain, components, 2, '/') != 2)
    return HTTP_STATUS_BAD_REQUEST;

  http_deescape(components[1]);

  if ((str = http_arg_get(&hc->hc_req_args, "weight")))
    weight = atoi(str);

  tvh_mutex_lock(&global_lock);

  if(!strcmp(components[0], "channelid")) {
    ch = channel_find_by_id(atoi(components[1]));
  } else if(!strcmp(components[0], "channelnumber")) {
    ch = channel_find_by_number(components[1]);
  } else if(!strcmp(components[0], "channelname")) {
    ch = channel_find_by_name(components[1]);
  } else if(!strcmp(components[0], "channel")) {
    ch = channel_find(components[1]);
  } else if(!strcmp(components[0], "service")) {
    service = service_find_by_uuid(components[1]);
#if ENABLE_MPEGTS
  } else if(!strcmp(components[0], "mux")) {
    // TODO: do we want to be able to force starting a particular instance
    mm = mpegts_mux_find(components[1]);
#endif
  }

  if(ch != NULL) {
    r = http_stream_channel(hc, ch, weight);
  } else if(service != NULL) {
    r = http_stream_service(hc, service, weight);
#if ENABLE_MPEGTS
  } else if(mm != NULL) {
    r = http_stream_mux(hc, mm, weight);
#endif
  } else {
    r = HTTP_STATUS_BAD_REQUEST;
  }

  tvh_mutex_unlock(&global_lock);
  return r;
}

/**
 * Generate a xspf playlist
 * http://en.wikipedia.org/wiki/XML_Shareable_Playlist_Format
 */
static int
page_xspf(http_connection_t *hc, const char *remain, void *opaque, int urlauth)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  char buf[80], hostpath[512];
  const char *title, *profile, *ticket, *image, *delim = "?";

  if ((title = http_arg_get(&hc->hc_req_args, "title")) == NULL)
    title = "TVHeadend Stream";
  http_get_hostpath(hc, hostpath, sizeof(hostpath));
  htsbuf_qprintf(hq, "\
<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n\
<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\">\r\n\
  <trackList>\r\n\
     <track>\r\n\
       <title>%s</title>\r\n\
       <location>%s/%s", title, hostpath, remain);
  profile = http_arg_get(&hc->hc_req_args, "profile");
  if (profile) {
    htsbuf_qprintf(hq, "?profile=%s", profile);
    delim = "&";
  }
  switch (urlauth) {
  case URLAUTH_TICKET:
    ticket = http_arg_get(&hc->hc_req_args, "ticket");
    if (strempty(ticket)) {
      snprintf(buf, sizeof(buf), "/%s", remain);
      tvh_mutex_lock(&global_lock);
      ticket = access_ticket_create(buf, hc->hc_access);
      tvh_mutex_unlock(&global_lock);
    }
    htsbuf_qprintf(hq, "%sticket=%s", delim, ticket);
    break;
  case URLAUTH_CODE:
    ticket = http_arg_get(&hc->hc_req_args, "auth");
    if (strempty(ticket))
      if (hc->hc_access && !strempty(hc->hc_access->aa_auth))
        ticket = hc->hc_access->aa_auth;
    if (!strempty(ticket))
      htsbuf_qprintf(hq, "%sauth=%s", delim, ticket);
    break;
  default:
    break;
  }
  htsbuf_append_str(hq, "</location>\n");
  image = http_arg_get(&hc->hc_req_args, "image");
  if (image)
    htsbuf_qprintf(hq, "       <image>%s</image>\n", image);
  htsbuf_append_str(hq, "\
     </track>\r\n\
  </trackList>\r\n\
</playlist>\r\n");
  http_output_content(hc, MIME_XSPF_XML);
  return 0;
}

/**
 * Generate an M3U playlist
 * http://en.wikipedia.org/wiki/M3U
 */
static int
page_m3u(http_connection_t *hc, const char *remain, void *opaque, int urlauth)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  char buf[80], hostpath[512];
  const char *title, *profile, *ticket, *delim = "?";

  if ((title = http_arg_get(&hc->hc_req_args, "title")) == NULL)
    title = "TVHeadend Stream";
  http_get_hostpath(hc, hostpath, sizeof(hostpath));
  htsbuf_qprintf(hq, "\
#EXTM3U\r\n\
#EXTINF:-1,%s\r\n\
%s/%s", title, hostpath, remain);
  profile = http_arg_get(&hc->hc_req_args, "profile");
  if (profile) {
    htsbuf_qprintf(hq, "?profile=%s", profile);
    delim = "&";
  }
  switch (urlauth) {
  case URLAUTH_TICKET:
    ticket = http_arg_get(&hc->hc_req_args, "ticket");
    if (strempty(ticket)) {
      snprintf(buf, sizeof(buf), "/%s", remain);
      tvh_mutex_lock(&global_lock);
      ticket = access_ticket_create(buf, hc->hc_access);
      tvh_mutex_unlock(&global_lock);
    }
    htsbuf_qprintf(hq, "%sticket=%s", delim, ticket);
    break;
  case URLAUTH_CODE:
    ticket = http_arg_get(&hc->hc_req_args, "auth");
    if (strempty(ticket))
      if (hc->hc_access && !strempty(hc->hc_access->aa_auth))
        ticket = hc->hc_access->aa_auth;
    if (!strempty(ticket))
      htsbuf_qprintf(hq, "%sauth=%s", delim, ticket);
    break;
  default:
    break;
  }
  htsbuf_append_str(hq, "\n");
  http_output_content(hc, MIME_M3U);
  return 0;
}

static char *
page_play_path_modify_(http_connection_t *hc, const char *path, int *cut, int skip)
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
    return strdup(path + skip); /* note: skip the prefix */
  return NULL;
}

static char *
page_play_path_modify5(http_connection_t *hc, const char *path, int *cut)
{
  return page_play_path_modify_(hc, path, cut, 5);
}

static char *
page_play_path_modify10(http_connection_t *hc, const char *path, int *cut)
{
  return page_play_path_modify_(hc, path, cut, 10);
}

static char *
page_play_path_modify12(http_connection_t *hc, const char *path, int *cut)
{
  return page_play_path_modify_(hc, path, cut, 12);
}

static int
page_play_(http_connection_t *hc, const char *remain, void *opaque, int urlauth)
{
  char *playlist;

  if(remain == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if(access_verify2(hc->hc_access, ACCESS_OR |
                                   ACCESS_STREAMING |
                                   ACCESS_ADVANCED_STREAMING |
                                   ACCESS_RECORDER))
    return http_noaccess_code(hc);

  playlist = http_arg_get(&hc->hc_req_args, "playlist");
  if (playlist) {
    if (strcmp(playlist, "xspf") == 0)
      return page_xspf(hc, remain, opaque, urlauth);
    if (strcmp(playlist, "m3u") == 0)
      return page_m3u(hc, remain, opaque, urlauth);
  }
  if (webui_xspf)
    return page_xspf(hc, remain, opaque, urlauth);
  return page_m3u(hc, remain, opaque, urlauth);
}

static int
page_play(http_connection_t *hc, const char *remain, void *opaque)
{
  return page_play_(hc, remain, opaque, URLAUTH_NONE);
}

static int
page_play_ticket(http_connection_t *hc, const char *remain, void *opaque)
{
  return page_play_(hc, remain, opaque, URLAUTH_TICKET);
}

static int
page_play_auth(http_connection_t *hc, const char *remain, void *opaque)
{
  if (hc->hc_access == NULL || strempty(hc->hc_access->aa_auth))
    return http_noaccess_code(hc);
  return page_play_(hc, remain, opaque, URLAUTH_CODE);
}

/**
 *
 */
static const char *webui_dvbapi_get_service_type(mpegts_service_t *ms)
{
  switch(ms->s_dvb_servicetype) {
   case 0x01:
   case 0x11:
   default:
     return "TV";

   case 0x02:
   case 0x07:
   case 0x0a:
     return "Radio";

   case 0x03:
     return "Teletext";

   case 0x0c:
     return "Data";
  }
}

static int
page_srvid2(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  service_t *t;
  mpegts_service_t *ms;
  caid_t *ca;
  elementary_stream_t *st;
  int first;
  char buf1[16], buf2[16], buf3[16];

  tvh_mutex_lock(&global_lock);
  TAILQ_FOREACH(t, &service_all, s_all_link) {
    if (!idnode_is_instance(&t->s_id, &mpegts_service_class))
      continue;
    ms = (mpegts_service_t *)t;
    first = 1;
    TAILQ_FOREACH(st, &t->s_components.set_all, es_link) {
      LIST_FOREACH(ca, &st->es_caids, link)
        ca->filter = 0;
      LIST_FOREACH(ca, &st->es_caids, link) {
        if (ms->s_components.set_service_id == 0) continue;
        if (first) {
          snprintf(buf1, sizeof(buf1), "%04X:", ms->s_components.set_service_id);
          first = 0;
        } else {
          buf1[0] = ','; buf1[1] = '\0';
        }
        snprintf(buf2, sizeof(buf2), "%04X", ca->caid);
        if (ca->providerid > 0)
          snprintf(buf3, sizeof(buf3), "@%06X", ca->providerid);
        else
          buf3[0] = '\0';
        htsbuf_qprintf(hq, "%s%s%s", buf1, buf2, buf3);
      }
    }
    if (first) continue;
    htsbuf_qprintf(hq, "|%s|%s||%s\n",
                   strempty(ms->s_dvb_svcname) ? "???" : ms->s_dvb_svcname,
                   webui_dvbapi_get_service_type(ms),
                   ms->s_dvb_mux->mm_network->mn_provider_network_name ?: "");
  }
  tvh_mutex_unlock(&global_lock);
  http_output_content(hc, "text/plain");
  return 0;
}

/**
 * Send a file
 */
int
http_serve_file(http_connection_t *hc, const char *fname,
                int fconv, const char *content,
                int (*preop)(http_connection_t *hc, off_t file_start,
                             size_t content_len, void *opaque),
                int (*postop)(http_connection_t *hc, off_t file_start,
                              size_t content_len, off_t file_size, void *opaque),
                void (*stats)(http_connection_t *hc, size_t len, void *opaque),
                void *opaque)
{
  int fd, ret, close_ret;
  struct stat st;
  const char *range;
  char *basename;
  char *str, *str0;
  char range_buf[255];
  char *disposition = NULL;
  off_t content_len, total_len, chunk;
  intmax_t file_start, file_end;
  htsbuf_queue_t q;
#if defined(PLATFORM_LINUX)
  ssize_t r;
#elif defined(PLATFORM_FREEBSD) || defined(PLATFORM_DARWIN)
  off_t o, r;
#endif
  
  if (fconv) {
    basename = strrchr(fname, '/');
    if (basename) {
      basename++; /* Skip '/' */
      str0 = intlconv_utf8safestr(intlconv_charset_id("ASCII", 1, 1),
                                  basename, strlen(basename) * 3);
      if (str0 == NULL) {
        tvherror(LS_HTTP, "unable to convert filename '%s' to a safe form using charset '%s'",
                 basename, intlconv_charset_id("ASCII", 1, 1));
        return HTTP_STATUS_INTERNAL;
      }
      htsbuf_queue_init(&q, 0);
      htsbuf_append_and_escape_rfc8187(&q, basename);
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

  content_len = total_len = file_end - file_start+1;
  
  sprintf(range_buf, "bytes %jd-%jd/%jd",
          file_start, file_end, (intmax_t)st.st_size);

#if defined(PLATFORM_LINUX)
  if(file_start > 0) {
    off_t off;
    if ((off = lseek(fd, file_start, SEEK_SET)) != file_start) {
      tvherror(LS_HTTP, "unable to seek (offset %jd, returned %jd): %s",
               file_start, (intmax_t)off, strerror(errno));
      close(fd);
      return HTTP_STATUS_INTERNAL;
    }
  }
#elif defined(PLATFORM_FREEBSD) || defined(PLATFORM_DARWIN)
  o = file_start;
#endif

  if (preop) {
    ret = preop(hc, file_start, content_len, opaque);
    if (ret) {
      close(fd);
      return ret;
    }
  }

  http_send_begin(hc);
  http_send_header(hc, range ? HTTP_STATUS_PARTIAL_CONTENT : HTTP_STATUS_OK,
                   content, content_len, NULL, NULL, 10,
                   range ? range_buf : NULL, disposition, NULL);

  ret = 0;
  if(!hc->hc_no_output) {
    while(content_len > 0) {
      chunk = MIN(1024 * ((stats ? 128 : 1024) * 1024), content_len);
#if defined(PLATFORM_LINUX)
      r = sendfile(hc->hc_fd, fd, NULL, chunk);
      if (r < 0) {
        ret = -1;
        break;
      }
#elif defined(PLATFORM_FREEBSD)
      ret = sendfile(fd, hc->hc_fd, o, chunk, NULL, &r, 0);
      if (ret < 0)
        break;
      o += r;
#elif defined(PLATFORM_DARWIN)
      r = chunk;
      ret = sendfile(fd, hc->hc_fd, o, &r, NULL, 0);
      if (ret < 0)
        break;
      o += r;
#endif
      content_len -= r;
      if (stats)
        stats(hc, r, opaque);
    }
  }

  http_send_end(hc);
  close_ret = close(fd);
  if (close_ret != 0)
    ret = close_ret;

  /* We do postop _after_ the close since close will block until the
   * buffers have been received by the client.
   */
  if (ret == 0 && postop)
    ret = postop(hc, file_start, total_len, st.st_size, opaque);

  return ret;
}

/**
 * Download a recorded file
 */
typedef struct page_dvrfile_priv {
  const char *uuid;
  char *fname;
  const char *charset;
  const char *content;
  void *tcp_id;
  th_subscription_t *sub;
} page_dvrfile_priv_t;

static int
page_dvrfile_preop(http_connection_t *hc, off_t file_start,
                   size_t content_len, void *opaque)
{
  page_dvrfile_priv_t *priv = opaque;

  tvh_mutex_lock(&global_lock);
  priv->tcp_id = http_stream_preop(hc);
  priv->sub = NULL;
  if (priv->tcp_id && !hc->hc_no_output && content_len > 64*1024) {
    priv->sub = subscription_create_from_file("HTTP", priv->charset,
                                              priv->fname, hc->hc_peer_ipstr,
                                              http_username(hc),
                                              http_arg_get(&hc->hc_args, "User-Agent"));
    if (priv->sub == NULL) {
      http_stream_postop(priv->tcp_id);
      priv->tcp_id = NULL;
    }
  }
  tvh_mutex_unlock(&global_lock);
  if (priv->tcp_id == NULL)
    return HTTP_STATUS_NOT_ALLOWED;
  return 0;
}

static int
page_dvrfile_postop(http_connection_t *hc,
                    off_t file_start,
                    size_t content_len,
                    off_t file_size,
                    void *opaque)
{
  dvr_entry_t *de;
  const page_dvrfile_priv_t *priv = opaque;
  /* We are fully played when we send the last segment */
  const int is_fully_played = file_start + content_len >= file_size;
  tvhdebug(LS_HTTP, "page_dvrfile_postop: file start=%ld content len=%ld file size=%ld done=%d",
           (long)file_start, (long)content_len, (long)file_size, is_fully_played);

  if (!is_fully_played)
    return 0;

  tvh_mutex_lock(&global_lock);
  /* Play count + 1 when not doing HEAD */
  if (!hc->hc_no_output) {
    de = dvr_entry_find_by_uuid(priv->uuid);
    if (de == NULL)
      de = dvr_entry_find_by_id(atoi(priv->uuid));
    if (de && !dvr_entry_verify(de, hc->hc_access, 0)) {
      dvr_entry_incr_playcount(de);
      dvr_entry_changed(de);
    }
  }
  tvh_mutex_unlock(&global_lock);
  return 0;
}

static void
page_dvrfile_stats(http_connection_t *hc, size_t len, void *opaque)
{
  page_dvrfile_priv_t *priv = opaque;
  if (priv->sub) {
    subscription_add_bytes_in(priv->sub, len);
    subscription_add_bytes_out(priv->sub, len);
  }
}

static int
page_dvrfile(http_connection_t *hc, const char *remain, void *opaque)
{
  int ret;
  const char *filename;
  dvr_entry_t *de;
  page_dvrfile_priv_t priv;

  if(remain == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if(access_verify2(hc->hc_access, ACCESS_OR |
                                   ACCESS_STREAMING |
                                   ACCESS_ADVANCED_STREAMING |
                                   ACCESS_RECORDER))
    return http_noaccess_code(hc);

  tvh_mutex_lock(&global_lock);
  de = dvr_entry_find_by_uuid(remain);
  if (de == NULL)
    de = dvr_entry_find_by_id(atoi(remain));
  if(de == NULL || (filename = dvr_get_filename(de)) == NULL) {
    tvh_mutex_unlock(&global_lock);
    return HTTP_STATUS_NOT_FOUND;
  }
  if(dvr_entry_verify(de, hc->hc_access, 1)) {
    tvh_mutex_unlock(&global_lock);
    return http_noaccess_code(hc);
  }

  priv.uuid = remain;
  priv.fname = tvh_strdupa(filename);
  priv.content = muxer_container_filename2mime(priv.fname, 1);
  priv.charset = de->de_config ? de->de_config->dvr_charset_id : NULL;
  priv.tcp_id = NULL;
  priv.sub = NULL;

  tvh_mutex_unlock(&global_lock);

  ret = http_serve_file(hc, priv.fname, 1, priv.content,
                        page_dvrfile_preop, page_dvrfile_postop, page_dvrfile_stats, &priv);

  tvh_mutex_lock(&global_lock);
  if (priv.sub)
    subscription_unsubscribe(priv.sub, UNSUBSCRIBE_FINAL);
  http_stream_postop(priv.tcp_id);
  tvh_mutex_unlock(&global_lock);
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
  int r;
  char fname[PATH_MAX];

  if(remain == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if(access_verify2(hc->hc_access, ACCESS_OR |
                                   ACCESS_WEB_INTERFACE |
                                   ACCESS_STREAMING |
                                   ACCESS_ADVANCED_STREAMING |
                                   ACCESS_HTSP_STREAMING |
                                   ACCESS_HTSP_RECORDER |
                                   ACCESS_RECORDER))
    return http_noaccess_code(hc);

  if(sscanf(remain, "%d", &id) != 1)
    return HTTP_STATUS_BAD_REQUEST;

  /* Fetch details */
  r = imagecache_filename(id, fname, sizeof(fname));

  if (r)
    return HTTP_STATUS_NOT_FOUND;

  return http_serve_file(hc, fname, 0, NULL, NULL, NULL, NULL, NULL);
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
      http_send_begin(hc);
      http_send_header(hc, 200, "text/javascript; charset=UTF-8", strlen(buf), 0, NULL, 10, 0, NULL, NULL);
      tvh_write(hc->hc_fd, buf, strlen(buf));
      http_send_end(hc);
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
  http_path_t *hp;

  webui_xspf = xspf;

  if (tvheadend_webui_debug)
    tvhinfo(LS_WEBUI, "Running web interface in debug mode");

  s = tvheadend_webroot;
  tvheadend_webroot = NULL;
  http_path_add("", NULL, page_no_webroot, ACCESS_WEB_INTERFACE);
  tvheadend_webroot = s;

  http_path_add("", NULL, page_root2, ACCESS_WEB_INTERFACE);
  hp = http_path_add("/", NULL, page_root, ACCESS_WEB_INTERFACE);
  hp->hp_flags = HTTP_PATH_NO_VERIFICATION; /* redirect only */
  http_path_add("/login", NULL, page_login, ACCESS_WEB_INTERFACE);
  hp = http_path_add("/logout", NULL, page_logout, ACCESS_WEB_INTERFACE);
  hp->hp_flags = HTTP_PATH_NO_VERIFICATION;

#if CONFIG_SATIP_SERVER
  http_path_add("/satip_server", NULL, satip_server_http_page, ACCESS_ANONYMOUS);
#endif

  http_path_add_modify("/play", NULL, page_play, ACCESS_ANONYMOUS, page_play_path_modify5);
  http_path_add_modify("/play/ticket", NULL, page_play_ticket, ACCESS_ANONYMOUS, page_play_path_modify12);
  http_path_add_modify("/play/auth", NULL, page_play_auth, ACCESS_ANONYMOUS, page_play_path_modify10);
  http_path_add("/dvrfile", NULL, page_dvrfile, ACCESS_ANONYMOUS);
  http_path_add("/favicon.ico", NULL, favicon, ACCESS_WEB_INTERFACE);
  http_path_add("/playlist", NULL, page_http_playlist, ACCESS_ANONYMOUS);
  http_path_add("/playlist/ticket", NULL, page_http_playlist_ticket, ACCESS_ANONYMOUS);
  http_path_add("/playlist/auth", NULL, page_http_playlist_auth, ACCESS_ANONYMOUS);
  http_path_add("/xmltv", NULL, page_xmltv, ACCESS_ANONYMOUS);
  http_path_add("/special/srvid2", NULL, page_srvid2, ACCESS_ADMIN);
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
