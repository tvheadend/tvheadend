/*
 *  tvheadend, HTSP interface
 *  Copyright (C) 2007 Andreas Öman
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

#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "tvhead.h"
#include "channels.h"
#include "subscriptions.h"
#include "dispatch.h"
#include "rpc.h"
#include "htsp.h"
#include "htsp_muxer.h"
#include "tcp.h"
#include "epg.h"

#include <libhts/htsmsg_binary.h>

static LIST_HEAD(, htsp) htsp_sessions;

/*
 *
 */
int
htsp_send_msg(htsp_t *htsp, htsmsg_t *m, int media)
{
  tcp_session_t *tcp = &htsp->htsp_tcp_session;
  tcp_queue_t *tq;
  void *data;
  size_t datalen;
  int max, r = -1;

  tq = media ? &tcp->tcp_q_low : &tcp->tcp_q_hi;

  max = tq->tq_maxdepth - tq->tq_depth; /* max size we are able to enqueue */
  
  if(htsmsg_binary_serialize(m, &data, &datalen, max) == 0)
    r = tcp_send_msg(tcp, tq, data, datalen);

  htsmsg_destroy(m);
  return r;
}


/**
 * build a channel message
 */
static htsmsg_t *
htsp_build_channel_msg(channel_t *ch, const char *method)
{
  htsmsg_t *msg = htsmsg_create();
  event_t *e;

  htsmsg_add_str(msg, "method", method);
  htsmsg_add_str(msg, "channelGroupName", ch->ch_group->tcg_name);
  htsmsg_add_str(msg, "channelName", ch->ch_name);
  htsmsg_add_u32(msg, "channelTag", ch->ch_tag);
  if(ch->ch_icon != NULL)
    htsmsg_add_str(msg, "channelIcon", ch->ch_icon);
  
  if((e = epg_event_get_current(ch)) != NULL)
    htsmsg_add_u32(msg, "currentEvent", e->e_tag);

  return msg;
}




/**
 * channels_list
 */
static void
htsp_send_all_channels(htsp_t *htsp)
{
  htsmsg_t *msg;
  channel_group_t *tcg;
  channel_t *ch;

  TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {
    if(tcg->tcg_hidden)
      continue;

    msg = htsmsg_create();
    htsmsg_add_str(msg, "method", "channelGroupAdd");
    htsmsg_add_str(msg, "channelGroupName", tcg->tcg_name);
    htsp_send_msg(htsp, msg, 0);

    TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link) {
      if(LIST_FIRST(&ch->ch_transports) == NULL)
	continue;

      msg = htsp_build_channel_msg(ch, "channelAdd");
      htsp_send_msg(htsp, msg, 0);
    }
  }
}

/**
 * 
 */
void
htsp_async_channel_update(channel_t *ch)
{
  htsp_t *htsp;
  htsmsg_t *msg;

  LIST_FOREACH(htsp, &htsp_sessions, htsp_global_link) {
    if(!htsp->htsp_rpc.rs_is_async)
      continue;

    msg = htsp_build_channel_msg(ch, "channelUpdate");
    htsp_send_msg(htsp, msg, 0);
  }
}


/*
 *
 */
static void
htsp_input(htsp_t *htsp, const void *buf, int len)
{
  htsmsg_t *in, *out;
  int i, was_async;
  const uint8_t *v = buf;

  printf("Got %d bytes\n", len);
  for(i =0 ; i < len; i++)
    printf("%02x.", v[i]);
  printf("\n");

  if((in = htsmsg_binary_deserialize(buf, len, NULL)) == NULL) {
    printf("deserialize failed\n");
    return;
  }

  was_async = htsp->htsp_rpc.rs_is_async;

  printf("INPUT:\n");
  htsmsg_print(in);

  out = rpc_dispatch(&htsp->htsp_rpc, in, NULL, htsp,
		     (struct sockaddr *)&htsp->htsp_tcp_session.tcp_peer_addr);

  htsmsg_destroy(in);

  printf("OUTPUT:\n");
  htsmsg_print(out);

  htsp_send_msg(htsp, out, 0);

  if(!was_async && htsp->htsp_rpc.rs_is_async) {
    printf("Session went into async mode\n");
    /* Session went into async state */
    htsp_send_all_channels(htsp);
  }
}


/*
 * data available on socket
 */
static void
htsp_data_input(htsp_t *htsp)
{
  int r, l;
  tcp_session_t *tcp = &htsp->htsp_tcp_session;

  if(htsp->htsp_bufptr < 4) {
    r = read(tcp->tcp_fd, htsp->htsp_buf + htsp->htsp_bufptr,
	     4 - htsp->htsp_bufptr);
    if(r < 1) {
      tcp_disconnect(tcp, r == 0 ? ECONNRESET : errno);
      return;
    }

    htsp->htsp_bufptr += r;
    if(htsp->htsp_bufptr < 4)
      return;

    htsp->htsp_msglen = (htsp->htsp_buf[0] << 24) + (htsp->htsp_buf[1] << 16) +
      (htsp->htsp_buf[2] << 8) + htsp->htsp_buf[3] + 4;

    if(htsp->htsp_msglen < 12 || htsp->htsp_msglen > 16 * 1024 * 1024) {
      tcp_disconnect(tcp, EBADMSG);
      return;
    }

    if(htsp->htsp_bufsize < htsp->htsp_msglen) {
      htsp->htsp_bufsize = htsp->htsp_msglen;
      free(htsp->htsp_buf);
      htsp->htsp_buf = malloc(htsp->htsp_bufsize);
    }
  }

  l = htsp->htsp_msglen - htsp->htsp_bufptr;
  
  r = read(tcp->tcp_fd, htsp->htsp_buf + htsp->htsp_bufptr, l);
  if(r < 1) {
    tcp_disconnect(tcp, r == 0 ? ECONNRESET : errno);
    return;
  }
  
  htsp->htsp_bufptr += r;
  
  if(htsp->htsp_bufptr == htsp->htsp_msglen) {
    htsp_input(htsp, htsp->htsp_buf + 4, htsp->htsp_msglen - 4);
    htsp->htsp_bufptr = 0;
    htsp->htsp_msglen = 0;
  }
}



/*
 * 
 */
static void
htsp_disconnect(htsp_t *htsp)
{
  LIST_REMOVE(htsp, htsp_global_link);

  free(htsp->htsp_buf);
  rpc_deinit(&htsp->htsp_rpc);
}


/*
 * 
 */
static void
htsp_connect(htsp_t *htsp)
{
  rpc_init(&htsp->htsp_rpc, "htsp");
 
  htsp->htsp_bufsize = 1000;
  htsp->htsp_buf = malloc(htsp->htsp_bufsize);

  LIST_INSERT_HEAD(&htsp_sessions, htsp, htsp_global_link);
}

/*
 *
 */
static void
htsp_tcp_callback(tcpevent_t event, void *tcpsession)
{
  htsp_t *htsp = tcpsession;

  switch(event) {
  case TCP_CONNECT:
    htsp_connect(htsp);
    break;

  case TCP_DISCONNECT:
    htsp_disconnect(htsp);
    break;

  case TCP_INPUT:
    htsp_data_input(htsp);
    break;
  }
}


/*
 *  Fire up HTSP server
 */

void
htsp_start(int port)
{
  tcp_create_server(port, sizeof(htsp_t), "htsp", htsp_tcp_callback);
}
