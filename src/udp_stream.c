/*
 *  TVHeadend - UDP stream common routines
 *
 *  Copyright (C) 2019 Stephane Duperron
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

#define _GNU_SOURCE
#include "tvheadend.h"
#include "config.h"
#include "udp_stream.h"

static tvh_mutex_t udp_streams_mutex = TVH_THREAD_MUTEX_INITIALIZER;
static struct udp_stream_list udp_streams;

udp_stream_t *
create_udp_stream ( udp_connection_t *uc, const char *hint )
{
  udp_stream_t *ustream;
  char port_buf[6];  
  size_t unlen;

  ustream = calloc(1,sizeof(udp_stream_t));
  if (!ustream) {
    return NULL;
  }

  ustream->us_uc = uc;
  ustream->us_hint = strdup(hint);
  snprintf(port_buf, 6, "%d", uc->port);  
  unlen = strlen(uc->host) + strlen(port_buf) + 8;
  ustream->us_udp_url = malloc(unlen);
  snprintf(ustream->us_udp_url, unlen, "udp://%s:%s", uc->host, port_buf);  

  tvh_mutex_lock(&udp_streams_mutex);
  LIST_INSERT_HEAD(&udp_streams, ustream, us_link);
  tvh_mutex_unlock(&udp_streams_mutex);
  return ustream;
}

void
delete_udp_stream (udp_stream_t * ustream)
{
  tvh_mutex_lock(&udp_streams_mutex);
  LIST_REMOVE(ustream, us_link);
  tvh_mutex_unlock(&udp_streams_mutex);
  free(ustream->us_hint);
  free(ustream->us_udp_url);
  free(ustream);  
}

udp_stream_t *
find_udp_stream_by_hint ( const char *hint )
{
  udp_stream_t *us;
  int found = 0;

  tvh_mutex_lock(&udp_streams_mutex);
  LIST_FOREACH(us, &udp_streams, us_link)
  if(strcmp(us->us_hint, hint) == 0) {
    found = 1;
    break;
  }
  tvh_mutex_unlock(&udp_streams_mutex);

  if (found) {
    return us;
  } else {
    return NULL;
  }
}

udp_stream_t *
find_udp_stream_by_url ( const char *url )
{
  udp_stream_t *us;
  int found = 0;

  tvh_mutex_lock(&udp_streams_mutex);
  LIST_FOREACH(us, &udp_streams, us_link)
  if(strcmp(us->us_udp_url, url) == 0) {
    found = 1;
    break;
  }
  tvh_mutex_unlock(&udp_streams_mutex);

  if (found) {
    return us;
  } else {
    return NULL;
  }
}

static void *
udp_stream_thread ( void *aux )    
{
  udp_stream_t *us = (udp_stream_t *)aux;
  profile_chain_t *prch = &(us->us_prch);
  streaming_message_t *sm;
  int run = 1, started = 0;
  streaming_queue_t *sq = &prch->prch_sq;
  muxer_t *mux = prch->prch_muxer;
  int ptimeout, grace = 20, r;
  streaming_start_t *ss_copy;
  int64_t lastpkt, mono;
  int fd = us->us_uc->fd;

  if(muxer_open_stream(mux, fd))
    run = 0;
	mux->m_config.m_output_chunk = us->us_uc->txsize;
 
  if (config.dscp >= 0)
    socket_set_dscp(fd, config.dscp, NULL, 0);

  lastpkt = mclk();
  ptimeout = prch->prch_pro ? prch->prch_pro->pro_timeout : 5;

  while(atomic_get(&us->us_running) && run && tvheadend_is_running()) {
    tvh_mutex_lock(&sq->sq_mutex);
    sm = TAILQ_FIRST(&sq->sq_queue);
    if(sm == NULL) {
      mono = mclk() + sec2mono(1);
      do {
        r = tvh_cond_timedwait(&sq->sq_cond, &sq->sq_mutex, mono);
        if (r == ETIMEDOUT) {
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
        subscription_add_bytes_out(us->us_subscript, len = pktbuf_len(pb));
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
        tvhdebug(LS_UDP, "Start streaming %s", us->us_udp_url);
        ss_copy = streaming_start_copy((streaming_start_t *)sm->sm_data);
        if(muxer_init(mux, ss_copy, us->us_content_name) < 0)
          run = 0;
        streaming_start_unref(ss_copy);

        started = 1;
      } else if(muxer_reconfigure(mux, sm->sm_data) < 0) {
        tvhwarn(LS_UDP,  "Unable to reconfigure stream %s", us->us_udp_url);
      }
      break;

    case SMT_STOP:
      if((mux->m_caps & MC_CAP_ANOTHER_SERVICE) != 0) /* give a chance to use another svc */
        break;
      if(sm->sm_code != SM_CODE_SOURCE_RECONFIGURED) {
        tvhwarn(LS_UDP,  "Stop streaming %s, %s", us->us_udp_url, 
                streaming_code2txt(sm->sm_code));
        run = 0;
      }
      break;

    case SMT_SERVICE_STATUS:
    case SMT_SIGNAL_STATUS:
    case SMT_DESCRAMBLE_INFO:
      if((!started && mclk() - lastpkt > sec2mono(grace)) ||
                 (started && ptimeout > 0 && mclk() - lastpkt > sec2mono(ptimeout))) {
        tvhwarn(LS_UDP,  "Stop streaming %s, timeout waiting for packets", us->us_udp_url);
        run = 0;
      }
      break;

    case SMT_NOSTART_WARN:
    case SMT_SKIP:
    case SMT_SPEED:
    case SMT_TIMESHIFT_STATUS:
      break;

    case SMT_NOSTART:
      tvhwarn(LS_UDP,  "Couldn't start streaming %s, %s",
              us->us_udp_url, streaming_code2txt(sm->sm_code));
      run = 0;
      break;

    case SMT_EXIT:
      tvhwarn(LS_UDP,  "Stop streaming %s, %s", us->us_udp_url,
              streaming_code2txt(sm->sm_code));
      run = 0;
      break;
    }

    streaming_msg_free(sm);

    if(mux->m_errors) {
      if (!mux->m_eos)
        tvhwarn(LS_UDP,  "Stop streaming %s, muxer reported errors", us->us_udp_url);
      run = 0;
    }
  }

  if(started)
    muxer_close(mux);
  atomic_set(&us->us_running, 0);
  tvh_mutex_lock(us->us_global_lock);
  subscription_unsubscribe(us->us_subscript, UNSUBSCRIBE_FINAL);
  profile_chain_close(&us->us_prch);
  tvh_mutex_unlock(us->us_global_lock);
  udp_close(us->us_uc);
  free(us->us_content_name);
  delete_udp_stream(us); 
  return NULL;  
}

int
udp_stream_run (udp_stream_t *us)
{
  if (atomic_get(&us->us_running)) {
    tvhwarn(LS_UDP,  "UDP stream %s is already running", us->us_udp_url);
    return -1;
  }
  atomic_set(&us->us_running, 1);
  return tvh_thread_create(&us->us_tid, NULL, udp_stream_thread, (void *)us, us->us_udp_url);
}

int
udp_stream_shutdown (udp_stream_t *us)
{
  if (!atomic_get(&us->us_running)) {
    tvhwarn(LS_UDP,  "UDP stream %s is not currently running", us->us_udp_url);
    return -1;
  }
  atomic_set(&us->us_running, 0);
  return pthread_join(us->us_tid, NULL);
}