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
#include <sys/sendfile.h>

#include "tvheadend.h"
#include "access.h"
#include "http.h"
#include "webui.h"
#include "dvr/dvr.h"
#include "filebundle.h"
#include "psi.h"
#include "plumbing/tsfix.h"
#include "plumbing/globalheaders.h"
#include "epg.h"
#include "muxer.h"
#include "dvb/dvb.h"
#include "dvb/dvb_support.h"
#include "imagecache.h"

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
    http_redirect(hc, "simple.html");
  } else {
    http_redirect(hc, "extjs.html");
  }
  return 0;
}

static int
page_root2(http_connection_t *hc, const char *remain, void *opaque)
{
  if (!tvheadend_webroot) return 1;
  char *tmp = malloc(strlen(tvheadend_webroot) + 2);
  sprintf(tmp, "%s/", tvheadend_webroot);
  http_redirect(hc, tmp);
  free(tmp);
  return 0;
}

/**
 * Static download of a file from the filesystem
 */
int
page_static_file(http_connection_t *hc, const char *remain, void *opaque)
{
  int ret = 0;
  const char *base = opaque;
  char path[500];
  ssize_t size;
  const char *content = NULL, *postfix;
  char buf[4096];
  const char *gzip;

  if(remain == NULL)
    return 404;

  if(strstr(remain, ".."))
    return HTTP_STATUS_BAD_REQUEST;

  snprintf(path, sizeof(path), "%s/%s", base, remain);

  postfix = strrchr(remain, '.');
  if(postfix != NULL) {
    postfix++;
    if(!strcmp(postfix, "js"))
      content = "text/javascript; charset=UTF-8";
    else if(!strcmp(postfix, "css"))
      content = "text/css; charset=UTF-8";
  }

  // TODO: handle compression
  fb_file *fp = fb_open(path, 0, 1);
  if (!fp) {
    tvhlog(LOG_ERR, "webui", "failed to open %s", path);
    return 500;
  }
  size = fb_size(fp);
  gzip = fb_gzipped(fp) ? "gzip" : NULL;

  http_send_header(hc, 200, content, size, gzip, NULL, 10, 0, NULL);
  while (!fb_eof(fp)) {
    ssize_t c = fb_read(fp, buf, sizeof(buf));
    if (c < 0) {
      ret = 500;
      break;
    }
    if (tvh_write(hc->hc_fd, buf, c)) {
      ret = 500;
      break;
    }
  }
  fb_close(fp);

  return ret;
}

/**
 * HTTP stream loop
 */
static void
http_stream_run(http_connection_t *hc, streaming_queue_t *sq,
		const char *name, muxer_container_type_t mc)
{
  streaming_message_t *sm;
  int run = 1;
  int started = 0;
  muxer_t *mux = NULL;
  int timeouts = 0;
  struct timespec ts;
  struct timeval  tp;
  int err = 0;
  socklen_t errlen = sizeof(err);

  mux = muxer_create(mc);
  if(muxer_open_stream(mux, hc->hc_fd))
    run = 0;

  /* reduce timeout on write() for streaming */
  tp.tv_sec  = 5;
  tp.tv_usec = 0;
  setsockopt(hc->hc_fd, SOL_SOCKET, SO_SNDTIMEO, &tp, sizeof(tp));

  while(run) {
    pthread_mutex_lock(&sq->sq_mutex);
    sm = TAILQ_FIRST(&sq->sq_queue);
    if(sm == NULL) {      
      gettimeofday(&tp, NULL);
      ts.tv_sec  = tp.tv_sec + 1;
      ts.tv_nsec = tp.tv_usec * 1000;

      if(pthread_cond_timedwait(&sq->sq_cond, &sq->sq_mutex, &ts) == ETIMEDOUT) {
          timeouts++;

          //Check socket status
          getsockopt(hc->hc_fd, SOL_SOCKET, SO_ERROR, (char *)&err, &errlen);  
          if(err) {
      tvhlog(LOG_DEBUG, "webui",  "Stop streaming %s, client hung up", hc->hc_url_orig);
      run = 0;
          }else if(timeouts >= 20) {
      tvhlog(LOG_WARNING, "webui",  "Stop streaming %s, timeout waiting for packets", hc->hc_url_orig);
      run = 0;
          }
      }
      pthread_mutex_unlock(&sq->sq_mutex);
      continue;
    }

    timeouts = 0; //Reset timeout counter
    TAILQ_REMOVE(&sq->sq_queue, sm, sm_link);
    pthread_mutex_unlock(&sq->sq_mutex);

    switch(sm->sm_type) {
    case SMT_MPEGTS:
    case SMT_PACKET:
      if(started) {
        muxer_write_pkt(mux, sm->sm_type, sm->sm_data);
        sm->sm_data = NULL;
      }
      break;

    case SMT_START:
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
      if(getsockopt(hc->hc_fd, SOL_SOCKET, SO_ERROR, &err, &errlen)) {
        tvhlog(LOG_DEBUG, "webui",  "Stop streaming %s, client hung up",
               hc->hc_url_orig);
        run = 0;
      }
      break;

    case SMT_SKIP:
    case SMT_SPEED:
    case SMT_SIGNAL_STATUS:
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
      tvhlog(LOG_WARNING, "webui",  "Stop streaming %s, muxer reported errors", hc->hc_url_orig);
      run = 0;
    }
  }

  if(started)
    muxer_close(mux);

  muxer_destroy(mux);
}


/**
 * Output a playlist containing a single channel
 */
static int
http_channel_playlist(http_connection_t *hc, channel_t *channel)
{
  htsbuf_queue_t *hq;
  char buf[255];
  const char *host;

  hq = &hc->hc_reply;
  host = http_arg_get(&hc->hc_args, "Host");

  snprintf(buf, sizeof(buf), "/stream/channelid/%d", channel->ch_id);

  htsbuf_qprintf(hq, "#EXTM3U\n");
  htsbuf_qprintf(hq, "#EXTINF:-1,%s\n", channel->ch_name);
  htsbuf_qprintf(hq, "http://%s%s?ticket=%s\n", host, buf, 
     access_ticket_create(buf));

  http_output_content(hc, "audio/x-mpegurl");

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
  channel_tag_mapping_t *ctm;
  const char *host;

  hq = &hc->hc_reply;
  host = http_arg_get(&hc->hc_args, "Host");

  htsbuf_qprintf(hq, "#EXTM3U\n");
  LIST_FOREACH(ctm, &tag->ct_ctms, ctm_tag_link) {
    snprintf(buf, sizeof(buf), "/stream/channelid/%d", ctm->ctm_channel->ch_id);
    htsbuf_qprintf(hq, "#EXTINF:-1,%s\n", ctm->ctm_channel->ch_name);
    htsbuf_qprintf(hq, "http://%s%s?ticket=%s\n", host, buf, 
       access_ticket_create(buf));
  }

  http_output_content(hc, "audio/x-mpegurl");

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
  const char *host;

  hq = &hc->hc_reply;
  host = http_arg_get(&hc->hc_args, "Host");

  htsbuf_qprintf(hq, "#EXTM3U\n");
  TAILQ_FOREACH(ct, &channel_tags, ct_link) {
    if(!ct->ct_enabled || ct->ct_internal)
      continue;

    snprintf(buf, sizeof(buf), "/playlist/tagid/%d", ct->ct_identifier);
    htsbuf_qprintf(hq, "#EXTINF:-1,%s\n", ct->ct_name);
    htsbuf_qprintf(hq, "http://%s%s?ticket=%s\n", host, buf, 
       access_ticket_create(buf));
  }

  http_output_content(hc, "audio/x-mpegurl");

  return 0;
}


/**
 * Output a flat playlist with all channels
 */
static int
http_channel_list_playlist(http_connection_t *hc)
{
  htsbuf_queue_t *hq;
  char buf[255];
  channel_t *ch;
  const char *host;

  hq = &hc->hc_reply;
  host = http_arg_get(&hc->hc_args, "Host");

  htsbuf_qprintf(hq, "#EXTM3U\n");
  RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
    snprintf(buf, sizeof(buf), "/stream/channelid/%d", ch->ch_id);

    htsbuf_qprintf(hq, "#EXTINF:-1,%s\n", ch->ch_name);
    htsbuf_qprintf(hq, "http://%s%s?ticket=%s\n", host, buf, 
       access_ticket_create(buf));
  }

  http_output_content(hc, "audio/x-mpegurl");

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
  const char *host;
  off_t fsize;
  time_t durration;
  struct tm tm;
  int bandwidth;

  hq = &hc->hc_reply;
  host = http_arg_get(&hc->hc_args, "Host");

  htsbuf_qprintf(hq, "#EXTM3U\n");
  LIST_FOREACH(de, &dvrentries, de_global_link) {
    fsize = dvr_get_filesize(de);
    if(!fsize)
      continue;

    durration  = de->de_stop - de->de_start;
    durration += (de->de_stop_extra + de->de_start_extra)*60;
    bandwidth = ((8*fsize) / (durration*1024.0));
    strftime(buf, sizeof(buf), "%FT%T%z", localtime_r(&(de->de_start), &tm));

    htsbuf_qprintf(hq, "#EXTINF:%"PRItime_t",%s\n", durration, lang_str_get(de->de_title, NULL));
    
    htsbuf_qprintf(hq, "#EXT-X-TARGETDURATION:%"PRItime_t"\n", durration);
    htsbuf_qprintf(hq, "#EXT-X-STREAM-INF:PROGRAM-ID=%d,BANDWIDTH=%d\n", de->de_id, bandwidth);
    htsbuf_qprintf(hq, "#EXT-X-PROGRAM-DATE-TIME:%s\n", buf);

    snprintf(buf, sizeof(buf), "/dvrfile/%d", de->de_id);
    htsbuf_qprintf(hq, "http://%s%s?ticket=%s\n", host, buf, 
       access_ticket_create(buf));
  }

  http_output_content(hc, "audio/x-mpegurl");

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
  const char *ticket_id = NULL;
  time_t durration = 0;
  off_t fsize = 0;
  int bandwidth = 0;
  struct tm tm;  
  const char *host = http_arg_get(&hc->hc_args, "Host");

  durration  = de->de_stop - de->de_start;
  durration += (de->de_stop_extra + de->de_start_extra)*60;
    
  fsize = dvr_get_filesize(de);

  if(fsize) {
    bandwidth = ((8*fsize) / (durration*1024.0));
    strftime(buf, sizeof(buf), "%FT%T%z", localtime_r(&(de->de_start), &tm));

    htsbuf_qprintf(hq, "#EXTM3U\n");
    htsbuf_qprintf(hq, "#EXTINF:%"PRItime_t",%s\n", durration, lang_str_get(de->de_title, NULL));
    
    htsbuf_qprintf(hq, "#EXT-X-TARGETDURATION:%"PRItime_t"\n", durration);
    htsbuf_qprintf(hq, "#EXT-X-STREAM-INF:PROGRAM-ID=%d,BANDWIDTH=%d\n", de->de_id, bandwidth);
    htsbuf_qprintf(hq, "#EXT-X-PROGRAM-DATE-TIME:%s\n", buf);

    snprintf(buf, sizeof(buf), "/dvrfile/%d", de->de_id);
    ticket_id = access_ticket_create(buf);
    htsbuf_qprintf(hq, "http://%s%s?ticket=%s\n", host, buf, ticket_id);

    http_output_content(hc, "application/x-mpegURL");
  } else {
    http_error(hc, HTTP_STATUS_NOT_FOUND);
    return HTTP_STATUS_NOT_FOUND;
  }

  return 0;
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
    http_redirect(hc, "/playlist/channels");
    return HTTP_STATUS_FOUND;
  }

  nc = http_tokenize((char *)remain, components, 2, '/');
  if(!nc) {
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    return HTTP_STATUS_BAD_REQUEST;
  }

  if(nc == 2)
    http_deescape(components[1]);

  pthread_mutex_lock(&global_lock);

  if(nc == 2 && !strcmp(components[0], "channelid"))
    ch = channel_find_by_identifier(atoi(components[1]));
  else if(nc == 2 && !strcmp(components[0], "channel"))
    ch = channel_find_by_name(components[1], 0, 0);
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
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    r = HTTP_STATUS_BAD_REQUEST;
  }

  pthread_mutex_unlock(&global_lock);

  return r;
}


/**
 * Subscribes to a service and starts the streaming loop
 */
static int
http_stream_service(http_connection_t *hc, service_t *service)
{
  streaming_queue_t sq;
  th_subscription_t *s;
  streaming_target_t *gh;
  streaming_target_t *tsfix;
  streaming_target_t *st;
  dvr_config_t *cfg;
  muxer_container_type_t mc;
  int flags;
  const char *str;
  size_t qsize;
  const char *name;

  mc = muxer_container_txt2type(http_arg_get(&hc->hc_req_args, "mux"));
  if(mc == MC_UNKNOWN) {
    cfg = dvr_config_find_by_name_default("");
    mc = cfg->dvr_mc;
  }

  if ((str = http_arg_get(&hc->hc_req_args, "qsize")))
    qsize = atoll(str);
  else
    qsize = 1500000;

  if(mc == MC_PASS || mc == MC_RAW) {
    streaming_queue_init2(&sq, SMT_PACKET, qsize);
    gh = NULL;
    tsfix = NULL;
    st = &sq.sq_st;
    flags = SUBSCRIPTION_RAW_MPEGTS;
  } else {
    streaming_queue_init2(&sq, 0, qsize);
    gh = globalheaders_create(&sq.sq_st);
    tsfix = tsfix_create(gh);
    st = tsfix;
    flags = 0;
  }

  s = subscription_create_from_service(service, "HTTP", st, flags,
				       inet_ntoa(hc->hc_peer->sin_addr),
				       hc->hc_username,
				       http_arg_get(&hc->hc_args, "User-Agent"));
  if(s) {
    name = strdupa(service->s_ch ?
                   service->s_ch->ch_name : service->s_nicename);
    pthread_mutex_unlock(&global_lock);
    http_stream_run(hc, &sq, name, mc);
    pthread_mutex_lock(&global_lock);
    subscription_unsubscribe(s);
  }

  if(gh)
    globalheaders_destroy(gh);

  if(tsfix)
    tsfix_destroy(tsfix);

  streaming_queue_deinit(&sq);

  return 0;
}


/**
 * Subscribes to a service and starts the streaming loop
 */
#if ENABLE_LINUXDVB
static int
http_stream_tdmi(http_connection_t *hc, th_dvb_mux_instance_t *tdmi)
{
  th_subscription_t *s;
  streaming_queue_t sq;
  const char *name;
  streaming_queue_init(&sq, SMT_PACKET);

  s = dvb_subscription_create_from_tdmi(tdmi, "HTTP", &sq.sq_st,
					inet_ntoa(hc->hc_peer->sin_addr),
					hc->hc_username,
					http_arg_get(&hc->hc_args, "User-Agent"));
  name = strdupa(tdmi->tdmi_identifier);
  pthread_mutex_unlock(&global_lock);
  http_stream_run(hc, &sq, name, MC_RAW);
  pthread_mutex_lock(&global_lock);
  subscription_unsubscribe(s);

  streaming_queue_deinit(&sq);

  return 0;
}
#endif


/**
 * Subscribes to a channel and starts the streaming loop
 */
static int
http_stream_channel(http_connection_t *hc, channel_t *ch)
{
  streaming_queue_t sq;
  th_subscription_t *s;
  streaming_target_t *gh;
  streaming_target_t *tsfix;
  streaming_target_t *st;
  dvr_config_t *cfg;
  int priority = 100;
  int flags;
  muxer_container_type_t mc;
  char *str;
  size_t qsize;
  const char *name;

  mc = muxer_container_txt2type(http_arg_get(&hc->hc_req_args, "mux"));
  if(mc == MC_UNKNOWN) {
    cfg = dvr_config_find_by_name_default("");
    mc = cfg->dvr_mc;
  }

  if ((str = http_arg_get(&hc->hc_req_args, "qsize")))
    qsize = atoll(str);
  else
    qsize = 1500000;

  if(mc == MC_PASS || mc == MC_RAW) {
    streaming_queue_init2(&sq, SMT_PACKET, qsize);
    gh = NULL;
    tsfix = NULL;
    st = &sq.sq_st;
    flags = SUBSCRIPTION_RAW_MPEGTS;
  } else {
    streaming_queue_init2(&sq, 0, qsize);
    gh = globalheaders_create(&sq.sq_st);
    tsfix = tsfix_create(gh);
    st = tsfix;
    flags = 0;
  }

  s = subscription_create_from_channel(ch, priority, "HTTP", st, flags,
               inet_ntoa(hc->hc_peer->sin_addr),
               hc->hc_username,
               http_arg_get(&hc->hc_args, "User-Agent"));

  if(s) {
    name = strdupa(ch->ch_name);
    pthread_mutex_unlock(&global_lock);
    http_stream_run(hc, &sq, name, mc);
    pthread_mutex_lock(&global_lock);
    subscription_unsubscribe(s);
  }

  if(gh)
    globalheaders_destroy(gh);
  if(tsfix)
    tsfix_destroy(tsfix);

  streaming_queue_deinit(&sq);

  return 0;
}


/**
 * Handle the http request. http://tvheadend/stream/channelid/<chid>
 *                          http://tvheadend/stream/channel/<chname>
 *                          http://tvheadend/stream/service/<servicename>
 *                          http://tvheadend/stream/mux/<muxid>
 */
static int
http_stream(http_connection_t *hc, const char *remain, void *opaque)
{
  char *components[2];
  channel_t *ch = NULL;
  service_t *service = NULL;
#if ENABLE_LINUXDVB
  th_dvb_mux_instance_t *tdmi = NULL;
#endif

  hc->hc_keep_alive = 0;

  if(remain == NULL) {
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    return HTTP_STATUS_BAD_REQUEST;
  }

  if(http_tokenize((char *)remain, components, 2, '/') != 2) {
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    return HTTP_STATUS_BAD_REQUEST;
  }

  http_deescape(components[1]);

  scopedgloballock();

  if(!strcmp(components[0], "channelid")) {
    ch = channel_find_by_identifier(atoi(components[1]));
  } else if(!strcmp(components[0], "channel")) {
    ch = channel_find_by_name(components[1], 0, 0);
  } else if(!strcmp(components[0], "service")) {
    service = service_find_by_identifier(components[1]);
#if ENABLE_LINUXDVB
  } else if(!strcmp(components[0], "mux")) {
    tdmi = dvb_mux_find_by_identifier(components[1]);
#endif
  }

  if(ch != NULL) {
    return http_stream_channel(hc, ch);
  } else if(service != NULL) {
    return http_stream_service(hc, service);
#if ENABLE_LINUXDVB
  } else if(tdmi != NULL) {
    return http_stream_tdmi(hc, tdmi);
#endif
  } else {
    http_error(hc, HTTP_STATUS_BAD_REQUEST);
    return HTTP_STATUS_BAD_REQUEST;
  }
}

/**
 * Download a recorded file
 */
static int
page_dvrfile(http_connection_t *hc, const char *remain, void *opaque)
{
  int fd, i;
  struct stat st;
  const char *content = NULL, *postfix, *range;
  dvr_entry_t *de;
  char *fname;
  char range_buf[255];
  char disposition[256];
  off_t content_len, file_start, file_end, chunk;
  ssize_t r;
  
  if(remain == NULL)
    return 404;

  pthread_mutex_lock(&global_lock);

  de = dvr_entry_find_by_id(atoi(remain));
  if(de == NULL || de->de_filename == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  fname = strdup(de->de_filename);
  content = muxer_container_type2mime(de->de_mc, 1);
  postfix = muxer_container_suffix(de->de_mc, 1);

  pthread_mutex_unlock(&global_lock);

  fd = tvh_open(fname, O_RDONLY, 0);
  free(fname);
  if(fd < 0)
    return 404;

  if(fstat(fd, &st) < 0) {
    close(fd);
    return 404;
  }

  file_start = 0;
  file_end = st.st_size-1;
  
  range = http_arg_get(&hc->hc_args, "Range");
  if(range != NULL)
    sscanf(range, "bytes=%"PRId64"-%"PRId64"", &file_start, &file_end);

  //Sanity checks
  if(file_start < 0 || file_start >= st.st_size) {
    close(fd);
    return 200;
  }
  if(file_end < 0 || file_end >= st.st_size) {
    close(fd);
    return 200;
  }

  if(file_start > file_end) {
    close(fd);
    return 200;
  }

  content_len = file_end - file_start+1;
  
  sprintf(range_buf, "bytes %"PRId64"-%"PRId64"/%"PRId64"",
    file_start, file_end, st.st_size);

  if(file_start > 0)
    lseek(fd, file_start, SEEK_SET);

  if(de->de_title != NULL) {
    snprintf(disposition, sizeof(disposition),
       "attachment; filename=%s.%s", lang_str_get(de->de_title, NULL), postfix);
    i = 20;
    while(disposition[i]) {
      if(disposition[i] == ' ')
  disposition[i] = '_';
      i++;
    }
    
  } else {
    disposition[0] = 0;
  }

  http_send_header(hc, range ? HTTP_STATUS_PARTIAL_CONTENT : HTTP_STATUS_OK,
       content, content_len, NULL, NULL, 10, 
       range ? range_buf : NULL,
       disposition[0] ? disposition : NULL);

  if(!hc->hc_no_output) {
    while(content_len > 0) {
      chunk = MIN(1024 * 1024 * 1024, content_len);
      r = sendfile(hc->hc_fd, fd, NULL, chunk);
      if(r == -1) {
  close(fd);
  return -1;
      }
      content_len -= r;
    }
  }
  close(fd);
  return 0;
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
    return 404;

  if(sscanf(remain, "%d", &id) != 1)
    return HTTP_STATUS_BAD_REQUEST;

  if ((fd = imagecache_open(id)) < 0)
    return 404;
  if (fstat(fd, &st)) {
    close(fd);
    return 404;
  }

  http_send_header(hc, 200, NULL, st.st_size, 0, NULL, 10, 0, NULL);

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
  http_path_add(http_path, strdup(source), page_static_file,
    ACCESS_WEB_INTERFACE);
}


/**
 *
 */
static int
favicon(http_connection_t *hc, const char *remain, void *opaque)
{
  http_redirect(hc, "static/htslogo.png");
  return 0;
}

int page_statedump(http_connection_t *hc, const char *remain, void *opaque);

/**
 * WEB user interface
 */
void
webui_init(void)
{
  if (tvheadend_webui_debug)
    tvhlog(LOG_INFO, "webui", "Running web interface in debug mode");

  http_path_add("", NULL, page_root2, ACCESS_WEB_INTERFACE);
  http_path_add("/", NULL, page_root, ACCESS_WEB_INTERFACE);

  http_path_add("/dvrfile", NULL, page_dvrfile, ACCESS_WEB_INTERFACE);
  http_path_add("/favicon.ico", NULL, favicon, ACCESS_WEB_INTERFACE);
  http_path_add("/playlist", NULL, page_http_playlist, ACCESS_WEB_INTERFACE);

  http_path_add("/state", NULL, page_statedump, ACCESS_ADMIN);

  http_path_add("/stream",  NULL, http_stream,  ACCESS_STREAMING);

  http_path_add("/imagecache", NULL, page_imagecache, ACCESS_WEB_INTERFACE);

  webui_static_content("/static",        "src/webui/static");
  webui_static_content("/docs",          "docs/html");
  webui_static_content("/docresources",  "docs/docresources");

  simpleui_start();
  extjs_start();
  comet_init();

}
