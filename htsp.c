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
#if 0
static void
htsp_flush_queue(htsp_msg_q_t *hmq)
{
  abort();
}
#endif

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

#if 0
/**
 *
 */
static void
htsp_send_message(htsp_connection_t *htsp, htsmsg_t *m, int queue)
{
  htsp_msg_q_t *hm = malloc(sizeof(htsp_msg_q_t));

  hm->hm_msg = m;
  hm->hm_pktref = NULL;
  

}
#endif

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
  htsmsg_add_str(out, "description", e->e_desc);

  n = RB_NEXT(e, e_channel_link);
  if(n != NULL)
    htsmsg_add_u32(out, "nextEventId", n->e_id);

  return out;
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

  htsp_read_loop(&htsp);

  pthread_mutex_lock(&global_lock);
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
