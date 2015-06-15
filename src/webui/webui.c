/*
 *  tvheadend, WEBUI / HTML user interface
 *  Copyright (C) 2008 Andreas Öman
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
#include "access.h"
#include "http.h"
#include "webui.h"
#include "dvr/dvr.h"
#include "filebundle.h"
#include "streaming.h"
#include "profile.h"
#include "epg.h"
#include "muxer.h"
#include "imagecache.h"
#include "tcp.h"
#include "config.h"
#include "atomic.h"
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
 * Root page, we direct the client to different pages depending
 * on if it is a full blown browser or just some mobile app
 */
static int
page_root(http_connection_t *hc, const char *remain, void *opaque)
{
  if(is_client_simple(hc)) {
    http_redirect(hc, "simple.html", &hc->hc_req_args);
  } else {
    http_redirect(hc, "extjs.html", &hc->hc_req_args);
  }
  return 0;
}

static int
page_root2(http_connection_t *hc, const char *remain, void *opaque)
{
  if (!tvheadend_webroot) return 1;
  char *tmp = malloc(strlen(tvheadend_webroot) + 2);
  sprintf(tmp, "%s/", tvheadend_webroot);
  http_redirect(hc, tmp, &hc->hc_req_args);
  free(tmp);
  return 0;
}

static int
page_login(http_connection_t *hc, const char *remain, void *opaque)
{
  if (hc->hc_access != NULL &&
      hc->hc_access->aa_username != NULL &&
      hc->hc_access->aa_username != '\0') {
    http_redirect(hc, "/", &hc->hc_req_args);
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
    http_redirect(hc, "/", &hc->hc_req_args);
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
  }

  fb_file *fp = fb_open(path, 0, (nogzip || gzip) ? 0 : 1);
  if (!fp) {
    tvhlog(LOG_ERR, "webui", "failed to open %s", path);
    return HTTP_STATUS_INTERNAL;
  }
  size = fb_size(fp);
  if (!gzip && fb_gzipped(fp))
    gzip = "gzip";

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
  int run = 1;
  int started = 0;
  streaming_queue_t *sq = &prch->prch_sq;
  muxer_t *mux = prch->prch_muxer;
  time_t lastpkt;
  int ptimeout, grace = 20;
  struct timespec ts;
  struct timeval  tp;
  int err = 0;
  socklen_t errlen = sizeof(err);

  if(muxer_open_stream(mux, hc->hc_fd))
    run = 0;

  /* reduce timeout on write() for streaming */
  tp.tv_sec  = 5;
  tp.tv_usec = 0;
  setsockopt(hc->hc_fd, SOL_SOCKET, SO_SNDTIMEO, &tp, sizeof(tp));

  lastpkt = dispatch_clock;
  ptimeout = prch->prch_pro ? prch->prch_pro->pro_timeout : 5;

  while(!hc->hc_shutdown && run && tvheadend_running) {
    pthread_mutex_lock(&sq->sq_mutex);
    sm = TAILQ_FIRST(&sq->sq_queue);
    if(sm == NULL) {
      gettimeofday(&tp, NULL);
      ts.tv_sec  = tp.tv_sec + 1;
      ts.tv_nsec = tp.tv_usec * 1000;

      if(pthread_cond_timedwait(&sq->sq_cond, &sq->sq_mutex, &ts) == ETIMEDOUT) {

        /* Check socket status */
        if (getsockopt(hc->hc_fd, SOL_SOCKET, SO_ERROR, (char *)&err, &errlen) || err) {
          tvhlog(LOG_DEBUG, "webui",  "Stop streaming %s, client hung up", hc->hc_url_orig);
          run = 0;
        } else if((!started && dispatch_clock - lastpkt > grace) ||
                   (started && ptimeout > 0 && dispatch_clock - lastpkt > ptimeout)) {
          tvhlog(LOG_WARNING, "webui",  "Stop streaming %s, timeout waiting for packets", hc->hc_url_orig);
          run = 0;
        }
      }
      pthread_mutex_unlock(&sq->sq_mutex);
      continue;
    }

    TAILQ_REMOVE(&sq->sq_queue, sm, sm_link);
    pthread_mutex_unlock(&sq->sq_mutex);

    switch(sm->sm_type) {
    case SMT_MPEGTS:
    case SMT_PACKET:
      lastpkt = dispatch_clock;
      if(started) {
        pktbuf_t *pb;;
        if (sm->sm_type == SMT_PACKET)
          pb = ((th_pkt_t*)sm->sm_data)->pkt_payload;
        else
          pb = sm->sm_data;
        atomic_add(&s->ths_bytes_out, pktbuf_len(pb));
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
        tvhlog(LOG_DEBUG, "webui",  "Start streaming %s", hc->hc_url_orig);
        http_output_content(hc, muxer_mime(mux, sm->sm_data));

        if(muxer_init(mux, sm->sm_data, name) < 0)
          run = 0;

        started = 1;
      } else if(muxer_reconfigure(mux, sm->sm_data) < 0) {
        tvhlog(LOG_WARNING, "webui",  "Unable to reconfigure stream %s", hc->hc_url_orig);
      }
      break;

    case SMT_STOP:
      if(sm->sm_code != SM_CODE_SOURCE_RECONFIGURED) {
        tvhlog(LOG_WARNING, "webui",  "Stop streaming %s, %s", hc->hc_url_orig, 
               streaming_code2txt(sm->sm_code));
        run = 0;
      }
      break;

    case SMT_SERVICE_STATUS:
      if(getsockopt(hc->hc_fd, SOL_SOCKET, SO_ERROR, &err, &errlen) || err) {
        tvhlog(LOG_DEBUG, "webui",  "Stop streaming %s, client hung up",
               hc->hc_url_orig);
        run = 0;
      } else if((!started && dispatch_clock - lastpkt > grace) ||
                 (started && ptimeout > 0 && dispatch_clock - lastpkt > ptimeout)) {
        tvhlog(LOG_WARNING, "webui",  "Stop streaming %s, timeout waiting for packets", hc->hc_url_orig);
        run = 0;
      }
      break;

    case SMT_SKIP:
    case SMT_SPEED:
    case SMT_SIGNAL_STATUS:
    case SMT_TIMESHIFT_STATUS:
      break;

    case SMT_NOSTART:
      tvhlog(LOG_WARNING, "webui",  "Couldn't start streaming %s, %s",
             hc->hc_url_orig, streaming_code2txt(sm->sm_code));
      run = 0;
      break;

    case SMT_EXIT:
      tvhlog(LOG_WARNING, "webui",  "Stop streaming %s, %s", hc->hc_url_orig,
             streaming_code2txt(sm->sm_code));
      run = 0;
      break;
    }

    streaming_msg_free(sm);

    if(mux->m_errors) {
      if (!mux->m_eos)
        tvhlog(LOG_WARNING, "webui",  "Stop streaming %s, muxer reported errors", hc->hc_url_orig);
      run = 0;
    }
  }

  if(started)
    muxer_close(mux);
}


static char *
http_get_hostpath(http_connection_t *hc)
{  
  char buf[255];
  const char *host = http_arg_get(&hc->hc_args, "Host") ?: http_arg_get(&hc->hc_args, "X-Forwarded-Host");
  const char *proto = http_arg_get(&hc->hc_args, "X-Forwarded-Proto");

  snprintf(buf, sizeof(buf), "%s://%s%s", proto ?: "http", host, tvheadend_webroot ?: "");

  return strdup(buf);
}

/**
 * Output a playlist containing a single channel
 */
static int
http_channel_playlist(http_connection_t *hc, channel_t *channel)
{
  htsbuf_queue_t *hq;
  char buf[255];
  char *profile, *hostpath;

  if (http_access_verify_channel(hc, ACCESS_STREAMING, channel, 1))
    return HTTP_STATUS_UNAUTHORIZED;

  profile = profile_validate_name(http_arg_get(&hc->hc_req_args, "profile"));
  hostpath = http_get_hostpath(hc);

  hq = &hc->hc_reply;

  snprintf(buf, sizeof(buf), "/stream/channelid/%d", channel_get_id(channel));

  htsbuf_qprintf(hq, "#EXTM3U\n");
  htsbuf_qprintf(hq, "#EXTINF:-1,%s\n", channel_get_name(channel));
  htsbuf_qprintf(hq, "%s%s?ticket=%s", hostpath, buf,
     access_ticket_create(buf, hc->hc_access));

  htsbuf_qprintf(hq, "&profile=%s\n", profile);

  http_output_content(hc, "audio/x-mpegurl");

  free(hostpath);
  free(profile);
  return 0;
}


/**
 * Output a playlist containing all channels with a specific tag
 */
static int
http_tag_playlist(http_connection_t *hc, channel_tag_t *tag)
{
  htsbuf_queue_t *hq;
  char buf[255];
  idnode_list_mapping_t *ilm;
  char *profile, *hostpath;
  channel_t *ch;

  if(hc->hc_access == NULL ||
     access_verify2(hc->hc_access, ACCESS_STREAMING))
    return HTTP_STATUS_UNAUTHORIZED;

  hq = &hc->hc_reply;

  profile = profile_validate_name(http_arg_get(&hc->hc_req_args, "profile"));
  hostpath = http_get_hostpath(hc);

  htsbuf_qprintf(hq, "#EXTM3U\n");
  LIST_FOREACH(ilm, &tag->ct_ctms, ilm_in1_link) {
    ch = (channel_t *)ilm->ilm_in2;
    if (http_access_verify_channel(hc, ACCESS_STREAMING, ch, 0))
      continue;
    snprintf(buf, sizeof(buf), "/stream/channelid/%d", channel_get_id(ch));
    htsbuf_qprintf(hq, "#EXTINF:-1,%s\n", channel_get_name(ch));
    htsbuf_qprintf(hq, "%s%s?ticket=%s", hostpath, buf,
       access_ticket_create(buf, hc->hc_access));
    htsbuf_qprintf(hq, "&profile=%s\n", profile);
  }

  http_output_content(hc, "audio/x-mpegurl");

  free(hostpath);
  free(profile);
  return 0;
}


/**
 * Output a playlist pointing to tag-specific playlists
 */
static int
http_tag_list_playlist(http_connection_t *hc)
{
  htsbuf_queue_t *hq;
  char buf[255];
  channel_tag_t *ct;
  char *profile, *hostpath;

  if(hc->hc_access == NULL ||
     access_verify2(hc->hc_access, ACCESS_STREAMING))
    return HTTP_STATUS_UNAUTHORIZED;

  hq = &hc->hc_reply;

  profile = profile_validate_name(http_arg_get(&hc->hc_req_args, "profile"));
  hostpath = http_get_hostpath(hc);

  htsbuf_qprintf(hq, "#EXTM3U\n");
  TAILQ_FOREACH(ct, &channel_tags, ct_link) {
    if(!ct->ct_enabled || ct->ct_internal)
      continue;

    snprintf(buf, sizeof(buf), "/playlist/tagid/%d", idnode_get_short_uuid(&ct->ct_id));
    htsbuf_qprintf(hq, "#EXTINF:-1,%s\n", ct->ct_name);
    htsbuf_qprintf(hq, "%s%s?ticket=%s", hostpath, buf,
       access_ticket_create(buf, hc->hc_access));
    htsbuf_qprintf(hq, "&profile=%s\n", profile);
  }

  http_output_content(hc, "audio/x-mpegurl");

  free(hostpath);
  free(profile);
  return 0;
}


/**
 * Output a flat playlist with all channels
 */
static int
http_channel_list_playlist_cmp(const void *a, const void *b)
{
  channel_t *c1 = *(channel_t **)a, *c2 = *(channel_t **)b;
  int r = channel_get_number(c1) - channel_get_number(c2);
  if (r == 0)
    r = strcasecmp(channel_get_name(c1), channel_get_name(c2));
  return r;
}

static int
http_channel_list_playlist(http_connection_t *hc)
{
  htsbuf_queue_t *hq;
  char buf[255];
  channel_t *ch;
  channel_t **chlist;
  int idx = 0, count = 0;
  char *profile, *hostpath;

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

  qsort(chlist, count, sizeof(channel_t *), http_channel_list_playlist_cmp);

  htsbuf_qprintf(hq, "#EXTM3U\n");
  for (idx = 0; idx < count; idx++) {
    ch = chlist[idx];

    if (http_access_verify_channel(hc, ACCESS_STREAMING, ch, 0))
      continue;

    snprintf(buf, sizeof(buf), "/stream/channelid/%d", channel_get_id(ch));

    htsbuf_qprintf(hq, "#EXTINF:-1,%s\n", channel_get_name(ch));
    htsbuf_qprintf(hq, "%s%s?ticket=%s", hostpath, buf,
       access_ticket_create(buf, hc->hc_access));
    htsbuf_qprintf(hq, "&profile=%s\n", profile);
  }

  free(chlist);

  http_output_content(hc, "audio/x-mpegurl");

  free(hostpath);
  free(profile);
  return 0;
}



/**
 * Output a playlist of all recordings.
 */
static int
http_dvr_list_playlist(http_connection_t *hc)
{
  htsbuf_queue_t *hq;
  char buf[255];
  dvr_entry_t *de;
  const char *uuid;
  char *hostpath = http_get_hostpath(hc);
  off_t fsize;
  time_t durration;
  struct tm tm;
  int bandwidth;

  hq = &hc->hc_reply;

  htsbuf_qprintf(hq, "#EXTM3U\n");
  LIST_FOREACH(de, &dvrentries, de_global_link) {
    fsize = dvr_get_filesize(de);
    if(!fsize)
      continue;

    if (de->de_channel &&
        http_access_verify_channel(hc, ACCESS_RECORDER, de->de_channel, 0))
      continue;


    durration  = dvr_entry_get_stop_time(de) - dvr_entry_get_start_time(de);
    bandwidth = ((8*fsize) / (durration*1024.0));
    strftime(buf, sizeof(buf), "%FT%T%z", localtime_r(&(de->de_start), &tm));

    htsbuf_qprintf(hq, "#EXTINF:%"PRItime_t",%s\n", durration, lang_str_get(de->de_title, NULL));
    
    htsbuf_qprintf(hq, "#EXT-X-TARGETDURATION:%"PRItime_t"\n", durration);
    uuid = idnode_uuid_as_str(&de->de_id);
    htsbuf_qprintf(hq, "#EXT-X-STREAM-INF:PROGRAM-ID=%s,BANDWIDTH=%d\n", uuid, bandwidth);
    htsbuf_qprintf(hq, "#EXT-X-PROGRAM-DATE-TIME:%s\n", buf);

    snprintf(buf, sizeof(buf), "/dvrfile/%s", uuid);
    htsbuf_qprintf(hq, "%s%s?ticket=%s\n", hostpath, buf,
       access_ticket_create(buf, hc->hc_access));
  }

  http_output_content(hc, "audio/x-mpegurl");

  free(hostpath);
  return 0;
}

/**
 * Output a playlist with a http stream for a dvr entry (.m3u format)
 */
static int
http_dvr_playlist(http_connection_t *hc, dvr_entry_t *de)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  char buf[255];
  const char *ticket_id = NULL, *uuid;
  time_t durration = 0;
  off_t fsize = 0;
  int bandwidth = 0, ret = 0;
  struct tm tm;  
  char *hostpath;

  if(http_access_verify(hc, ACCESS_RECORDER))
    return HTTP_STATUS_UNAUTHORIZED;

  if(dvr_entry_verify(de, hc->hc_access, 1))
    return HTTP_STATUS_NOT_FOUND;

  hostpath  = http_get_hostpath(hc);
  durration  = dvr_entry_get_stop_time(de) - dvr_entry_get_start_time(de);
  fsize = dvr_get_filesize(de);

  if(fsize) {
    bandwidth = ((8*fsize) / (durration*1024.0));
    strftime(buf, sizeof(buf), "%FT%T%z", localtime_r(&(de->de_start), &tm));

    htsbuf_qprintf(hq, "#EXTM3U\n");
    htsbuf_qprintf(hq, "#EXTINF:%"PRItime_t",%s\n", durration, lang_str_get(de->de_title, NULL));
    
    htsbuf_qprintf(hq, "#EXT-X-TARGETDURATION:%"PRItime_t"\n", durration);
    uuid = idnode_uuid_as_str(&de->de_id);
    htsbuf_qprintf(hq, "#EXT-X-STREAM-INF:PROGRAM-ID=%s,BANDWIDTH=%d\n", uuid, bandwidth);
    htsbuf_qprintf(hq, "#EXT-X-PROGRAM-DATE-TIME:%s\n", buf);

    snprintf(buf, sizeof(buf), "/dvrfile/%s", uuid);
    ticket_id = access_ticket_create(buf, hc->hc_access);
    htsbuf_qprintf(hq, "%s%s?ticket=%s\n", hostpath, buf, ticket_id);

    http_output_content(hc, "application/x-mpegURL");
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
  char *components[2];
  int nc, r;
  channel_t *ch = NULL;
  dvr_entry_t *de = NULL;
  channel_tag_t *tag = NULL;

  if(!remain) {
    http_redirect(hc, "/playlist/channels", &hc->hc_req_args);
    return HTTP_STATUS_FOUND;
  }

  nc = http_tokenize((char *)remain, components, 2, '/');
  if(!nc)
    return HTTP_STATUS_BAD_REQUEST;

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
  else if(nc == 2 && !strcmp(components[0], "tag"))
    tag = channel_tag_find_by_name(components[1], 0);

  if(ch)
    r = http_channel_playlist(hc, ch);
  else if(tag)
    r = http_tag_playlist(hc, tag);
  else if(de)
    r = http_dvr_playlist(hc, de);
  else if(!strcmp(components[0], "tags"))
    r = http_tag_list_playlist(hc);
  else if(!strcmp(components[0], "channels"))
    r = http_channel_list_playlist(hc);
  else if(!strcmp(components[0], "channels.m3u"))
    r = http_channel_list_playlist(hc);
  else if(!strcmp(components[0], "recordings"))
    r = http_dvr_list_playlist(hc);
  else {
    r = HTTP_STATUS_BAD_REQUEST;
  }

  pthread_mutex_unlock(&global_lock);

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

  if(http_access_verify(hc, ACCESS_ADVANCED_STREAMING))
    return HTTP_STATUS_UNAUTHORIZED;

  if(!(pro = profile_find_by_list(hc->hc_access->aa_profiles,
                                  http_arg_get(&hc->hc_req_args, "profile"),
                                  "service",
                                  SUBSCRIPTION_PACKET | SUBSCRIPTION_MPEGTS)))
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
                                         prch.prch_flags | SUBSCRIPTION_STREAMING,
                                         hc->hc_peer_ipstr,
				         hc->hc_username,
				         http_arg_get(&hc->hc_args, "User-Agent"),
				         NULL);
    if(s) {
      name = tvh_strdupa(service->s_nicename);
      pthread_mutex_unlock(&global_lock);
      http_stream_run(hc, &prch, name, s);
      pthread_mutex_lock(&global_lock);
      subscription_unsubscribe(s, 0);
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
      subscription_unsubscribe(s, 0);
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

  if (http_access_verify_channel(hc, ACCESS_STREAMING, ch, 1))
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
      subscription_unsubscribe(s, 0);
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
  const char *title, *profile, *image;
  size_t len;

  if ((title = http_arg_get(&hc->hc_req_args, "title")) == NULL)
    title = "TVHeadend Stream";
  profile = http_arg_get(&hc->hc_req_args, "profile");
  image   = http_arg_get(&hc->hc_req_args, "image");

  maxlen = strlen(remain) + strlen(title) + 512;
  buf = alloca(maxlen);

  snprintf(buf, maxlen, "\
<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n\
<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\">\r\n\
  <trackList>\r\n\
     <track>\r\n\
       <title>%s</title>\r\n\
       <location>%s/%s%s%s</location>\r\n%s%s%s\
     </track>\r\n\
  </trackList>\r\n\
</playlist>\r\n", title, hostpath, remain, profile ? "?profile=" : "", profile ?: "",
  image ? "       <image>" : "", image ?: "", image ? "</image>\r\n" : "");

  len = strlen(buf);
  http_send_header(hc, 200, "application/xspf+xml", len, 0, NULL, 10, 0, NULL, NULL);
  tvh_write(hc->hc_fd, buf, len);

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
  const char *title, *profile;
  size_t len;

  if ((title = http_arg_get(&hc->hc_req_args, "title")) == NULL)
    title = "TVHeadend Stream";
  profile = http_arg_get(&hc->hc_req_args, "profile");

  maxlen = strlen(remain) + strlen(title) + 256;
  buf = alloca(maxlen);

  snprintf(buf, maxlen, "\
#EXTM3U\r\n\
#EXTINF:-1,%s\r\n\
%s/%s%s%s\r\n", title, hostpath, remain, profile ? "?profile=" : "", profile ?: "");

  len = strlen(buf);
  http_send_header(hc, 200, "audio/x-mpegurl", len, 0, NULL, 10, 0, NULL, NULL);
  tvh_write(hc->hc_fd, buf, len);

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
  char *fname;
  char *basename;
  char *str;
  char range_buf[255];
  char disposition[256];
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
    return HTTP_STATUS_NOT_FOUND;
  }

  fname = tvh_strdupa(filename);
  content = muxer_container_type2mime(de->de_mc, 1);

  pthread_mutex_unlock(&global_lock);

  basename = strrchr(fname, '/');
  if (basename) {
    basename++; /* Skip '/' */
    htsbuf_queue_init(&q, 0);
    htsbuf_append_and_escape_url(&q, basename);
    str = htsbuf_to_string(&q);
    snprintf(disposition, sizeof(disposition), "attachment; filename=\"%s\"", str);
    htsbuf_queue_flush(&q);
    free(str);
  } else {
    disposition[0] = 0;
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
  
  sprintf(range_buf, "bytes %jd-%jd/%zd",
    file_start, file_end, (size_t)st.st_size);

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
      basename = malloc(strlen(fname) + 7 + 1);
      strcpy(basename, "file://");
      strcat(basename, fname);
      sub->ths_dvrfile = basename;
    }
  }
  pthread_mutex_unlock(&global_lock);
  if (tcp_id == NULL) {
    close(fd);
    return HTTP_STATUS_NOT_ALLOWED;
  }

  http_send_header(hc, range ? HTTP_STATUS_PARTIAL_CONTENT : HTTP_STATUS_OK,
       content, content_len, NULL, NULL, 10, 
       range ? range_buf : NULL,
       disposition[0] ? disposition : NULL, NULL);

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
        sub->ths_bytes_in += r;
        sub->ths_bytes_out += r;
      }
    }
  }
  close(fd);

  pthread_mutex_lock(&global_lock);
  if (sub)
    subscription_unsubscribe(sub, 0);
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

  http_send_header(hc, 200, NULL, st.st_size, 0, NULL, 10, 0, NULL, NULL);

  while (1) {
    c = read(fd, buf, sizeof(buf));
    if (c <= 0)
      break;
    if (tvh_write(hc->hc_fd, buf, c))
      break;
  }
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
  http_redirect(hc, "static/htslogo.png", NULL);
  return 0;
}

int page_statedump(http_connection_t *hc, const char *remain, void *opaque);

/**
 * WEB user interface
 */
void
webui_init(int xspf)
{
  webui_xspf = xspf;

  if (tvheadend_webui_debug)
    tvhlog(LOG_INFO, "webui", "Running web interface in debug mode");

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

  http_path_add("/state", NULL, page_statedump, ACCESS_ADMIN);

  http_path_add("/stream",  NULL, http_stream,  ACCESS_STREAMING);

  http_path_add("/imagecache", NULL, page_imagecache, ACCESS_ANONYMOUS);

  webui_static_content("/static",        "src/webui/static");
  webui_static_content("/docs",          "docs/html");
  webui_static_content("/docresources",  "docs/docresources");

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
