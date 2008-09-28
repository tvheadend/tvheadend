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
#include "tcp.h"
#include "packet.h"
#include "access.h"
#include "htsp.h"
#include "streaming.h"

#include <libhts/htsmsg_binary.h>

extern const char *htsversion;

LIST_HEAD(htsp_connection_list, htsp_connection);

TAILQ_HEAD(htsp_msg_queue, htsp_msg);
TAILQ_HEAD(htsp_msg_q_queue, htsp_msg_q);

static struct htsp_connection_list htsp_async_connections;

/**
 *
 */
typedef struct htsp_msg {
  TAILQ_ENTRY(htsp_msg) hm_link;

  htsmsg_t *hm_msg;

  th_pktref_t *hm_pktref;     /* For keeping reference to packet.
				 hm_msg can contain messages that points
				 to packet payload so to avoid copy we
				 keep a reference here */
} htsp_msg_t;


/**
 *
 */
typedef struct htsp_msg_q {
  struct htsp_msg_queue hmq_q;

  TAILQ_ENTRY(htsp_msg_q) hmq_link;
  int hmq_length;
} htsp_msg_q_t;


/**
 *
 */
typedef struct htsp_connection {
  int htsp_fd;
  struct sockaddr_in *htsp_peer;
  char *htsp_name;

  /**
   * Async mode
   */
  int htsp_async_mode;
  LIST_ENTRY(htsp_connection) htsp_async_link;

  /**
   * Writer thread
   */
  pthread_t htsp_writer_thread;

  int htsp_writer_run;

  struct htsp_msg_q_queue htsp_active_output_queues;

  pthread_mutex_t htsp_out_mutex;
  pthread_cond_t htsp_out_cond;

  htsp_msg_q_t htsp_hmq_ctrl;
  htsp_msg_q_t htsp_hmq_epg;

  /**
   *
   */
  struct th_subscription_list htsp_subscriptions;

} htsp_connection_t;

/**
 *
 */
typedef struct htsp_stream {
  streaming_target_t hs_st;
  int hs_channelid; /* We can not deref channel since we don't (and can't)
		       hold global_lock during delivery */
		       
  htsp_connection_t *hs_htsp;

  htsp_msg_q_t hs_q;

} htsp_stream_t;



/**
 *
 */
static void htsp_subscription_callback(struct th_subscription *s,
				       subscription_event_t event,
				       void *opaque);




/**
 *
 */
static void
htsp_msg_destroy(htsp_msg_t *hm)
{
  htsmsg_destroy(hm->hm_msg);
  if(hm->hm_pktref != NULL) {
    pkt_ref_dec(hm->hm_pktref->pr_pkt);
    free(hm->hm_pktref);
  }
  free(hm);
}


/**
 *
 */
static void
htsp_init_queue(htsp_msg_q_t *hmq)
{
  TAILQ_INIT(&hmq->hmq_q);
  hmq->hmq_length = 0;
}


/**
 *
 */
static void
htsp_destroy_queue(htsp_connection_t *htsp, htsp_msg_q_t *hmq)
{
  htsp_msg_t *hm;

  if(hmq->hmq_length)
    TAILQ_REMOVE(&htsp->htsp_active_output_queues, hmq, hmq_link);

  while((hm = TAILQ_FIRST(&hmq->hmq_q)) != NULL) {
    TAILQ_REMOVE(&hmq->hmq_q, hm, hm_link);
    htsp_msg_destroy(hm);
  }
}


/**
 *
 */
static void
htsp_send(htsp_connection_t *htsp, htsmsg_t *m, th_pktref_t *pkt,
	  htsp_msg_q_t *hmq)
{
  htsp_msg_t *hm = malloc(sizeof(htsp_msg_t));

  hm->hm_msg = m;
  hm->hm_pktref = pkt;
  
  pthread_mutex_lock(&htsp->htsp_out_mutex);

  TAILQ_INSERT_TAIL(&hmq->hmq_q, hm, hm_link);

  if(hmq->hmq_length == 0) {
    /* Activate queue */
    TAILQ_INSERT_TAIL(&htsp->htsp_active_output_queues, hmq, hmq_link);
  }

  hmq->hmq_length++;
  pthread_cond_signal(&htsp->htsp_out_cond);
  pthread_mutex_unlock(&htsp->htsp_out_mutex);
}

/**
 *
 */
static void
htsp_send_message(htsp_connection_t *htsp, htsmsg_t *m, htsp_msg_q_t *hmq)
{
  htsp_send(htsp, m, NULL, hmq ?: &htsp->htsp_hmq_ctrl);
}


/**
 *
 */
static htsmsg_t *
htsp_build_channel(channel_t *ch, const char *method)
{
  htsmsg_t *out = htsmsg_create();

  htsmsg_add_u32(out, "channelId", ch->ch_id);

  htsmsg_add_str(out, "channelName", ch->ch_name);
  if(ch->ch_icon != NULL)
    htsmsg_add_str(out, "channelIcon", ch->ch_icon);

  htsmsg_add_u32(out, "eventId",
		 ch->ch_epg_current != NULL ? ch->ch_epg_current->e_id : 0);

  htsmsg_add_str(out, "method", method);
  return out;
}


/**
 *
 */
static htsmsg_t *
htsp_build_tag(channel_tag_t *ct, const char *method)
{
  channel_tag_mapping_t *ctm;
  htsmsg_t *out = htsmsg_create();
  htsmsg_t *members = htsmsg_create_array();

  htsmsg_add_str(out, "tagId", ct->ct_identifier);

  htsmsg_add_str(out, "tagName", ct->ct_name);
  htsmsg_add_str(out, "tagIcon", ct->ct_icon);
  htsmsg_add_u32(out, "tagTitledIcon", ct->ct_titled_icon);

  LIST_FOREACH(ctm, &ct->ct_ctms, ctm_tag_link)
    htsmsg_add_u32(members, NULL, ctm->ctm_channel->ch_id);

  htsmsg_add_msg(out, "members", members);
  htsmsg_add_str(out, "method", method);
  return out;
}


/** 
 * Simple function to respond with an error
 */
static htsmsg_t *
htsp_error(const char *err)
{
  htsmsg_t *r = htsmsg_create();
  htsmsg_add_str(r, "_error", err);
  return r;
}


/**
 *
 */
static void
htsp_reply(htsp_connection_t *htsp, htsmsg_t *in, htsmsg_t *out)
{
  uint32_t seq;

  if(!htsmsg_get_u32(in, "seq", &seq))
    htsmsg_add_u32(out, "seq", seq);

  htsp_send_message(htsp, out, NULL);
}


/**
 * Switch the HTSP connection into async mode
 */
static htsmsg_t *
htsp_method_async(htsp_connection_t *htsp, htsmsg_t *in)
{
  channel_t *ch;
  channel_tag_t *ct;

  /* First, just OK the async request */
  htsp_reply(htsp, in, htsmsg_create()); 

  if(htsp->htsp_async_mode)
    return NULL; /* already in async mode */

  htsp->htsp_async_mode = 1;

  /* Send all channels */
  RB_FOREACH(ch, &channel_name_tree, ch_name_link)
    htsp_send_message(htsp, htsp_build_channel(ch, "channelAdd"), NULL);
    
  /* Send all enabled and external tags */
  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    if(ct->ct_enabled && !ct->ct_internal)
      htsp_send_message(htsp, htsp_build_tag(ct, "tagAdd"), NULL);
  
  /* Insert in list so it will get all updates */
  LIST_INSERT_HEAD(&htsp_async_connections, htsp, htsp_async_link);

  return NULL;
}


/**
 * Get information about the given event
 */
static htsmsg_t *
htsp_method_getEvent(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out;
  uint32_t eventid;
  event_t *e, *n;

  if(htsmsg_get_u32(in, "eventId", &eventid))
    return htsp_error("Missing argument 'eventId'");

  if((e = epg_event_find_by_id(eventid)) == NULL)
    return htsp_error("Event does not exist");

  out = htsmsg_create();

  htsmsg_add_u32(out, "start", e->e_start);
  htsmsg_add_u32(out, "stop", e->e_stop);
  htsmsg_add_str(out, "title", e->e_title);
  if(e->e_desc != NULL)
    htsmsg_add_str(out, "description", e->e_desc);

  n = RB_NEXT(e, e_channel_link);
  if(n != NULL)
    htsmsg_add_u32(out, "nextEventId", n->e_id);

  return out;
}


/**
 * Request subscription for a channel
 */
static htsmsg_t *
htsp_method_subscribe(htsp_connection_t *htsp, htsmsg_t *in)
{
  th_subscription_t *s;
  uint32_t chid;
  channel_t *ch;

  if(htsmsg_get_u32(in, "channelId", &chid))
    return htsp_error("Missing argument 'channeId'");

  if((ch = channel_find_by_identifier(chid)) == NULL)
    return htsp_error("Requested channel does not exist");

  LIST_FOREACH(s, &htsp->htsp_subscriptions, ths_subscriber_link)
    if(s->ths_channel == ch)
      return htsmsg_create(); /* We just say ok if already subscribed */

  s = subscription_create_from_channel(ch, 500, "htsp",
				       htsp_subscription_callback, htsp);
  LIST_INSERT_HEAD(&htsp->htsp_subscriptions, s, ths_subscriber_link);
  return htsmsg_create();
}


/**
 * Request unsubscription for a channel
 */
static htsmsg_t *
htsp_method_unsubscribe(htsp_connection_t *htsp, htsmsg_t *in)
{
  th_subscription_t *s;
  uint32_t chid;
  channel_t *ch;

  if(htsmsg_get_u32(in, "channelId", &chid))
    return htsp_error("Missing argument 'channeId'");

  if((ch = channel_find_by_identifier(chid)) == NULL)
    return htsp_error("Requested channel does not exist");

  LIST_FOREACH(s, &htsp->htsp_subscriptions, ths_subscriber_link)
    if(s->ths_channel == ch)
      break;

  if(s == NULL)
    return htsmsg_create(); /* We just say ok if already unsubscribed */

  LIST_REMOVE(s, ths_subscriber_link);
  subscription_unsubscribe(s);
  return htsmsg_create();
}



/**
 * HTSP methods
 */
struct {
  const char *name;
  htsmsg_t *(*fn)(htsp_connection_t *htsp, htsmsg_t *in);
  int privmask;
} htsp_methods[] = {
  { "async", htsp_method_async, ACCESS_STREAMING},
  { "getEvent", htsp_method_getEvent, ACCESS_STREAMING},
  { "subscribe", htsp_method_subscribe, ACCESS_STREAMING},
  { "unsubscribe", htsp_method_unsubscribe, ACCESS_STREAMING},

};

#define NUM_METHODS (sizeof(htsp_methods) / sizeof(htsp_methods[0]))



/**
 * timeout is in ms, 0 means infinite timeout
 */
static int
htsp_read_message(htsp_connection_t *htsp, htsmsg_t **mp, int timeout)
{
  int v;
  size_t len;
  uint8_t data[4];
  void *buf;

  v = timeout ? tcp_read_timeout(htsp->htsp_fd, data, 4, timeout) : 
    tcp_read(htsp->htsp_fd, data, 4);

  if(v != 0)
    return v;

  len = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
  if(len > 1024 * 1024)
    return EMSGSIZE;
  if((buf = malloc(len)) == NULL)
    return ENOMEM;

  v = timeout ? tcp_read_timeout(htsp->htsp_fd, buf, len, timeout) : 
    tcp_read(htsp->htsp_fd, buf, len);
  
  if(v != 0) {
    free(buf);
    return v;
  }

  /* buf will be tied to the message.
   * NB: If the message can not be deserialized buf will be free'd by the
   * function.
   */
  *mp = htsmsg_binary_deserialize(buf, len, buf);
  if(*mp == NULL)
    return EBADMSG;

  return 0;
}

/**
 *
 */
static int
htsp_read_loop(htsp_connection_t *htsp)
{
  htsmsg_t *m, *reply;
  int r, i;
  const char *method, *username, *password;

  while(1) {
    if((r = htsp_read_message(htsp, &m, 0)) != 0)
      return r;

    username = htsmsg_get_str(m, "username");
    password = htsmsg_get_str(m, "password");

    pthread_mutex_lock(&global_lock);

    if((method = htsmsg_get_str(m, "method")) != NULL) {
      for(i = 0; i < NUM_METHODS; i++) {
	if(!strcmp(method, htsp_methods[i].name)) {

	  if(access_verify(username, password, 
			   (struct sockaddr *)htsp->htsp_peer,
			   htsp_methods[i].privmask)) {

	    reply = htsmsg_create();

	    htsmsg_add_u32(reply, "_noaccess", 1);
	  } else {
	    reply = htsp_methods[i].fn(htsp, m);
	  }
	  break;
	}
      }

      if(i == NUM_METHODS) {
	reply = htsp_error("Method not found");
      }

    } else {
      reply = htsp_error("No 'method' argument");
    }

    pthread_mutex_unlock(&global_lock);

    if(reply != NULL) /* Methods can do all the replying inline */
      htsp_reply(htsp, m, reply);

    htsmsg_destroy(m);
  }
}



/**
 *
 */
static void *
htsp_write_scheduler(void *aux)
{
  htsp_connection_t *htsp = aux;
  int r;
  htsp_msg_q_t *hmq;
  htsp_msg_t *hm;
  void *dptr;
  size_t dlen;

  pthread_mutex_lock(&htsp->htsp_out_mutex);

  while(1) {

    if((hmq = TAILQ_FIRST(&htsp->htsp_active_output_queues)) == NULL) {
      /* No active queues at all */
      if(!htsp->htsp_writer_run)
	break; /* Should not run anymore, bail out */
      
      /* Nothing to be done, go to sleep */
      pthread_cond_wait(&htsp->htsp_out_cond, &htsp->htsp_out_mutex);
      continue;
    }

    hm = TAILQ_FIRST(&hmq->hmq_q);
    TAILQ_REMOVE(&hmq->hmq_q, hm, hm_link);
    hmq->hmq_length--;

    TAILQ_REMOVE(&htsp->htsp_active_output_queues, hmq, hmq_link);
    if(hmq->hmq_length) {
      /* Still messages to be sent, put back at the end of active queues */
      TAILQ_INSERT_TAIL(&htsp->htsp_active_output_queues, hmq, hmq_link);
    }

    pthread_mutex_unlock(&htsp->htsp_out_mutex);

    r = htsmsg_binary_serialize(hm->hm_msg, &dptr, &dlen, INT32_MAX);

    htsp_msg_destroy(hm);
    
    write(htsp->htsp_fd, dptr, dlen);
    free(dptr);
    pthread_mutex_lock(&htsp->htsp_out_mutex);
  }

  pthread_mutex_unlock(&htsp->htsp_out_mutex);
  return NULL;
}


/**
 *
 */
static void
htsp_serve(int fd, void *opaque, struct sockaddr_in *source)
{
  htsp_connection_t htsp;
  char buf[30];
  th_subscription_t *s;

  snprintf(buf, sizeof(buf), "%s", inet_ntoa(source->sin_addr));

  memset(&htsp, 0, sizeof(htsp_connection_t));

  TAILQ_INIT(&htsp.htsp_active_output_queues);

  htsp_init_queue(&htsp.htsp_hmq_ctrl);
  htsp_init_queue(&htsp.htsp_hmq_epg);

  htsp.htsp_name = strdup(buf);

  htsp.htsp_fd = fd;
  htsp.htsp_peer = source;
  htsp.htsp_writer_run = 1;

  pthread_create(&htsp.htsp_writer_thread, NULL, htsp_write_scheduler, &htsp);

  /**
   * Reader loop
   */

  htsp_read_loop(&htsp);

  /**
   * Ok, we're back, other end disconnected. Clean up stuff.
   */

  pthread_mutex_lock(&global_lock);

  /* Beware! Closing subscriptions will invoke a lot of callbacks
     down in the streaming code. So we do this as early as possible
     to avoid any weird lockups */
  while((s = LIST_FIRST(&htsp.htsp_subscriptions)) != NULL) {
    LIST_REMOVE(s, ths_subscriber_link);
    subscription_unsubscribe(s);
  }

  if(htsp.htsp_async_mode)
    LIST_REMOVE(&htsp, htsp_async_link);

  pthread_mutex_unlock(&global_lock);

  free(htsp.htsp_name);

  pthread_mutex_lock(&htsp.htsp_out_mutex);
  htsp.htsp_writer_run = 0;
  pthread_cond_signal(&htsp.htsp_out_cond);
  pthread_mutex_unlock(&htsp.htsp_out_mutex);

  pthread_join(htsp.htsp_writer_thread, NULL);
  close(fd);
}
  
/**
 *  Fire up HTSP server
 */
void
htsp_init(void)
{
  tcp_server_create(9982, htsp_serve, NULL);
}

/**
 *
 */
static void
htsp_async_send(htsmsg_t *m)
{
  htsp_connection_t *htsp;

  LIST_FOREACH(htsp, &htsp_async_connections, htsp_async_link)
    htsp_send_message(htsp, htsmsg_copy(m), NULL);
  htsmsg_destroy(m);
}


/**
 * EPG subsystem calls this function when the current event
 * changes for a channel, e may be NULL if there is no current event.
 *
 * global_lock is held
 */
void
htsp_event_update(channel_t *ch, event_t *e)
{
  htsmsg_t *m;
  time_t now;

  time(&now);
  m = htsmsg_create();
  htsmsg_add_str(m, "method", "channelUpdate");
  htsmsg_add_u32(m, "channelId", ch->ch_id);
  
  if(e == NULL)
    e = epg_event_find_by_time(ch, now);
  
  htsmsg_add_u32(m, "eventId", e ? e->e_id : 0);
  htsp_async_send(m);
}

/**
 * Called from channel.c when a new channel is created
 */
void
htsp_channel_add(channel_t *ch)
{
  htsp_async_send(htsp_build_channel(ch, "channelAdd"));
}


/**
 * Called from channel.c when a channel is updated
 */
void
htsp_channel_update(channel_t *ch)
{
  htsp_async_send(htsp_build_channel(ch, "channelUpdate"));
}


/**
 * Called from channel.c when a channel is deleted
 */
void
htsp_channel_delete(channel_t *ch)
{
  htsmsg_t *m = htsmsg_create();
  htsmsg_add_u32(m, "channelId", ch->ch_id);
  htsmsg_add_str(m, "method", "channelDelete");
  htsp_async_send(m);
}


/**
 * Called from channel.c when a tag is exported
 */
void
htsp_tag_add(channel_tag_t *ct)
{
  htsp_async_send(htsp_build_tag(ct, "tagAdd"));
}


/**
 * Called from channel.c when an exported tag is changed
 */
void
htsp_tag_update(channel_tag_t *ct)
{
  htsp_async_send(htsp_build_tag(ct, "tagUpdate"));
}


/**
 * Called from channel.c when an exported tag is deleted
 */
void
htsp_tag_delete(channel_tag_t *ct)
{
  htsmsg_t *m = htsmsg_create();
  htsmsg_add_str(m, "tagId", ct->ct_identifier);
  htsmsg_add_str(m, "method", "tagDelete");
  htsp_async_send(m);
}


/**
 *
 */
static void
htsp_send_subscription_status(htsp_connection_t *htsp, channel_t *ch,
			      const char *txt)
{
  htsmsg_t *m;

  if(ch == NULL)
    return;

  m = htsmsg_create();
  htsmsg_add_str(m, "method", "subscriptionStatus");
  htsmsg_add_u32(m, "channelId", ch->ch_id);

  if(txt != NULL)
    htsmsg_add_str(m, "status", txt);

  htsp_send_message(htsp, m, NULL);
}


/**
 * Build a htsmsg from a th_pkt and enqueue it on our HTSP transport
 */
static void
htsp_stream_deliver(void *opaque, struct th_pktref *pr)
{
  htsp_stream_t *hs = opaque;
  th_pkt_t *pkt = pr->pr_pkt;
  htsmsg_t *m = htsmsg_create();

  htsmsg_add_str(m, "method", "muxpkt");
  htsmsg_add_u32(m, "channelId", hs->hs_channelid);

  htsmsg_add_u64(m, "stream", pkt->pkt_componentindex);
  htsmsg_add_u64(m, "dts", pkt->pkt_dts);
  htsmsg_add_u64(m, "pts", pkt->pkt_pts);
  htsmsg_add_u32(m, "duration", pkt->pkt_duration);
  htsmsg_add_u32(m, "com", pkt->pkt_commercial);
  
  /**
   * Since we will serialize directly we use 'binptr' which is a binary
   * object that just points to data, thus avoiding a copy.
   */
  htsmsg_add_binptr(m, "payload", pkt->pkt_payload, pkt->pkt_payloadlen);

  htsp_send(hs->hs_htsp, m, pr, &hs->hs_q);
}


/**
 * Send a 'subscriptionStart' message to client informing about
 * delivery start and all components.
 *
 * Setup a streaming target that will deliver packets to the HTSP transport.
 */
static void
htsp_subscription_start(htsp_connection_t *htsp, th_subscription_t *s,
			channel_t *ch, streaming_pad_t *sp)
{
  streaming_component_t *sc;
  htsp_stream_t *hs;
  htsmsg_t *m, *streams, *c;

  assert(s->ths_st == NULL);

  hs = calloc(1, sizeof(htsp_stream_t));
  hs->hs_htsp = htsp;
  hs->hs_channelid = ch->ch_id;
  htsp_init_queue(&hs->hs_q);

  m = htsmsg_create();
  htsmsg_add_u32(m, "channelId", ch->ch_id);
  htsmsg_add_str(m, "method", "subscriptionStart");

  streaming_target_init(&hs->hs_st, htsp_stream_deliver, hs);

  /**
   * Lock streming pad delivery so we can hook us up.
   */
  pthread_mutex_lock(sp->sp_mutex);

  /* Setup each stream */ 
  streams = htsmsg_create_array();
  LIST_FOREACH(sc, &sp->sp_components, sc_link) {
    c = htsmsg_create();
    htsmsg_add_u32(c, "index", sc->sc_index);
    htsmsg_add_str(c, "type", streaming_component_type2txt(sc->sc_type));
    htsmsg_add_str(c, "language", sc->sc_lang);
    htsmsg_add_msg(streams, NULL, c);
  }

  htsmsg_add_msg(m, "streams", streams);

  htsp_send(hs->hs_htsp, m, NULL, &hs->hs_q);

  /* Link to the pad */
  streaming_target_connect(sp, &hs->hs_st);

  s->ths_st = &hs->hs_st;

  /* Once we unlock here we will start getting the callback */
  pthread_mutex_unlock(sp->sp_mutex);
}


/**
 * Send a 'subscriptionStart' stop
 *
 * Tear down all streaming related stuff.
 */
static void
htsp_subscription_stop(htsp_connection_t *htsp, th_subscription_t *s,
		       const char *reason)
{
  htsp_stream_t *hs;
  htsmsg_t *m;

  assert(s->ths_st != NULL);

  hs = (htsp_stream_t *)s->ths_st;

  /* Unlink from pad */
  streaming_target_disconnect(&hs->hs_st);

  /* Send a stop message back */
  m = htsmsg_create();
  htsmsg_add_u32(m, "channelId", hs->hs_channelid);
  htsmsg_add_str(m, "method", "subscriptionStop");

  if(reason)
    htsmsg_add_str(m, "reason", reason);

  /* Send on normal control queue cause we are about do destroy
     the per-subscription queue */
  htsp_send_message(hs->hs_htsp, m, NULL);

  htsp_destroy_queue(htsp, &hs->hs_q);

  free(hs);

  s->ths_st = NULL;
}




/**
 *
 */
static void
htsp_subscription_callback(struct th_subscription *s,
			   subscription_event_t event, void *opaque)
{
  htsp_connection_t *htsp = opaque;
  channel_t *ch = s->ths_channel; /* Beware: may be NULL if a channel
				     is destroyed while we're subscribing
				     to it. It's not dangerous, just keep
				     it in mind. */
  th_transport_t *t;

  switch(event) {
  case SUBSCRIPTION_EVENT_INVALID:
    abort();

  case SUBSCRIPTION_TRANSPORT_RUN:

    assert(ch != NULL); /* ch must be valid here */
    htsp_send_subscription_status(htsp, ch, NULL);

    t = s->ths_transport;
    htsp_subscription_start(htsp, s, ch, &t->tht_streaming_pad);
    return;

  case SUBSCRIPTION_NO_INPUT:
    htsp_send_subscription_status(htsp, ch, "No input detected");
    break;

  case SUBSCRIPTION_NO_DESCRAMBLER:
    htsp_send_subscription_status(htsp, ch, "No descrambler available");
    break;

  case SUBSCRIPTION_NO_ACCESS:
    htsp_send_subscription_status(htsp, ch, "Access denied");
    break;

  case SUBSCRIPTION_RAW_INPUT:
    htsp_send_subscription_status(htsp, ch,
				  "Unable to reassemble packets from input");
    break;

  case SUBSCRIPTION_VALID_PACKETS:
    htsp_send_subscription_status(htsp, ch, NULL);
    break;

  case SUBSCRIPTION_TRANSPORT_NOT_AVAILABLE:
    htsp_send_subscription_status(htsp, ch,
				  "No transport available, retrying...");
    break;
    
  case SUBSCRIPTION_TRANSPORT_LOST:
    htsp_subscription_stop(htsp, s, "Transport destroyed");
    break;
    
  case SUBSCRIPTION_DESTROYED:
    htsp_subscription_stop(htsp, s, NULL);
    return;
  }
}
