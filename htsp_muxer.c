/*
 *  tvheadend, HTSP streaming
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
#include "htsp.h"
#include "htsp_muxer.h"
#include "mux.h"

static void
htsp_packet_input(void *opaque, th_muxstream_t *tms, th_pkt_t *pkt)
{
  htsp_t *htsp = opaque;
  htsmsg_t *m = htsmsg_create();
  th_muxer_t *tm = tms->tms_muxer;
  //  th_stream_t *st = tms->tms_stream;
  th_subscription_t *s = tm->tm_subscription;

  /*
   * Build a message for this frame
   */

  htsmsg_add_str(m, "method", "muxpkt");
  htsmsg_add_u32(m, "channelId", s->ths_channel->ch_id);

  htsmsg_add_u64(m, "stream", tms->tms_index);
  htsmsg_add_u64(m, "dts", pkt->pkt_dts);
  htsmsg_add_u64(m, "pts", pkt->pkt_pts);
  htsmsg_add_u32(m, "duration", pkt->pkt_duration);
  htsmsg_add_u32(m, "com", pkt->pkt_commercial);

  /**
   * Since we will serialize directly we use 'binptr' which is a binary
   * object that just points to data, thus avoiding a copy.
   */
  htsmsg_add_binptr(m, "payload", pkt->pkt_payload, pkt->pkt_payloadlen);

  htsp_send_msg(htsp, m, 1);
}

/**
 * Called when a subscription gets/loses access to a transport
 */
static void
htsp_subscription_callback(struct th_subscription *s,
			   subscription_event_t event, void *opaque)
{
  htsp_t *htsp = opaque;
  th_muxer_t *tm;
  th_muxstream_t *tms;
  int index = 0;
  htsmsg_t *m, *sub;
  th_stream_t *st;

  switch(event) {
  case TRANSPORT_AVAILABLE:
    tm = muxer_init(s, htsp_packet_input, htsp);

    m = htsmsg_create();

    htsmsg_add_str(m, "method", "subscription_start");
    htsmsg_add_u32(m, "channelId", s->ths_channel->ch_id);

    LIST_FOREACH(tms, &tm->tm_streams, tms_muxer_link0) {
      tms->tms_index = index++;

      st = tms->tms_stream;

      sub = htsmsg_create();
      htsmsg_add_u32(sub, "index", tms->tms_index);
      htsmsg_add_str(sub, "type", htstvstreamtype2txt(st->st_type));
      htsmsg_add_str(sub, "language", st->st_lang);

      htsmsg_add_msg(m, "stream", sub);
    }
    htsmsg_print(m);
    htsp_send_msg(htsp, m, 0);

    muxer_play(tm, AV_NOPTS_VALUE);
    break;

  case TRANSPORT_UNAVAILABLE:
    if(htsp->htsp_zombie == 0) {
      m = htsmsg_create();
      htsmsg_add_str(m, "method", "subscription_stop");
      htsmsg_add_u32(m, "channelId", s->ths_channel->ch_id);

      htsmsg_add_str(m, "reason", "unknown");
      htsp_send_msg(htsp, m, 0);
    }

    muxer_deinit(s->ths_muxer, s);
    break;
  }
}


/**
 *
 */
void
htsp_muxer_subscribe(htsp_t *htsp, channel_t *ch, int weight)
{
  th_subscription_t *s;

  s = subscription_create(ch, weight, "HTSP", htsp_subscription_callback, htsp,
			  0);
  LIST_INSERT_HEAD(&htsp->htsp_subscriptions, s, ths_subscriber_link);
}


/**
 *
 */
static void
htsp_subscription_destroy(th_subscription_t *s)
{
  LIST_REMOVE(s, ths_subscriber_link);
  subscription_unsubscribe(s);
}


/**
 *
 */
void
htsp_muxer_unsubscribe(htsp_t *htsp, uint32_t id)
{
  th_subscription_t *s;

  LIST_FOREACH(s, &htsp->htsp_subscriptions, ths_subscriber_link) {
    if(s->ths_channel->ch_id == id)
      break;
  }

  if(s != NULL)
    htsp_subscription_destroy(s);
}


/**
 *
 */
void
htsp_muxer_cleanup(htsp_t *htsp)
{
  th_subscription_t *s;

  while((s = LIST_FIRST(&htsp->htsp_subscriptions)) != NULL)
    htsp_subscription_destroy(s);
}


