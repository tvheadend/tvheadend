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

#include "tvheadend.h"
#include "channels.h"
#include "subscriptions.h"
#include "tcp.h"
#include "packet.h"
#include "access.h"
#include "htsp.h"
#include "streaming.h"
#include "psi.h"
#include "htsmsg_binary.h"

#include <sys/statvfs.h>
#include "settings.h"
#include <sys/time.h>

static void *htsp_server;

#define HTSP_PROTO_VERSION 5

#define HTSP_PRIV_MASK (ACCESS_STREAMING)

extern const char *htsversion;
extern char *dvr_storage;

LIST_HEAD(htsp_connection_list, htsp_connection);
LIST_HEAD(htsp_subscription_list, htsp_subscription);

TAILQ_HEAD(htsp_msg_queue, htsp_msg);
TAILQ_HEAD(htsp_msg_q_queue, htsp_msg_q);

static struct htsp_connection_list htsp_async_connections;

static void htsp_streaming_input(void *opaque, streaming_message_t *sm);


/**
 *
 */
typedef struct htsp_msg {
  TAILQ_ENTRY(htsp_msg) hm_link;

  htsmsg_t *hm_msg;
  int hm_payloadsize;         /* For maintaining stats about streaming
				 buffer depth */

  pktbuf_t *hm_pb;      /* For keeping reference to packet payload.
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
  int hmq_strict_prio;      /* Serve this queue 'til it's empty */
  int hmq_length;
  int hmq_payload;          /* Bytes of streaming payload that's enqueued */
} htsp_msg_q_t;


/**
 *
 */
typedef struct htsp_connection {
  int htsp_fd;
  struct sockaddr_in *htsp_peer;

  int htsp_version;

  char *htsp_logname;
  char *htsp_peername;
  char *htsp_username;
  char *htsp_clientname;

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
  htsp_msg_q_t htsp_hmq_qstatus;

  /**
   *
   */
  struct htsp_subscription_list htsp_subscriptions;

  uint32_t htsp_granted_access;

  uint8_t htsp_challenge[32];

} htsp_connection_t;


/**
 *
 */
typedef struct htsp_subscription {
  htsp_connection_t *hs_htsp;

  LIST_ENTRY(htsp_subscription) hs_link;

  int hs_sid;  /* Subscription ID (set by client) */

  th_subscription_t *hs_s; // Temporary

  streaming_target_t hs_input;

  htsp_msg_q_t hs_q;

  time_t hs_last_report; /* Last queue status report sent */

  int hs_dropstats[PKT_NTYPES];

} htsp_subscription_t;


/**
 *
 */
static void
htsp_update_logname(htsp_connection_t *htsp)
{
  char buf[100];

  snprintf(buf, sizeof(buf), "%s%s%s%s%s%s",
	   htsp->htsp_peername,
	   htsp->htsp_username || htsp->htsp_clientname ? " [ " : "",
	   htsp->htsp_username ?: "",
	   htsp->htsp_username && htsp->htsp_clientname ? " | " : "",
	   htsp->htsp_clientname ?: "",
	   htsp->htsp_username || htsp->htsp_clientname ? " ]" : "");

  tvh_str_update(&htsp->htsp_logname, buf);
}


/**
 *
 */
static void
htsp_msg_destroy(htsp_msg_t *hm)
{
  htsmsg_destroy(hm->hm_msg);
  if(hm->hm_pb != NULL)
    pktbuf_ref_dec(hm->hm_pb);
  free(hm);
}


/**
 *
 */
static void
htsp_init_queue(htsp_msg_q_t *hmq, int strict_prio)
{
  TAILQ_INIT(&hmq->hmq_q);
  hmq->hmq_length = 0;
  hmq->hmq_strict_prio = strict_prio;
}


/**
 *
 */
static void
htsp_flush_queue(htsp_connection_t *htsp, htsp_msg_q_t *hmq)
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
htsp_subscription_destroy(htsp_connection_t *htsp, htsp_subscription_t *hs)
{
  LIST_REMOVE(hs, hs_link);
  subscription_unsubscribe(hs->hs_s);
  htsp_flush_queue(htsp, &hs->hs_q);
  free(hs);
}


/**
 *
 */
static void
htsp_send(htsp_connection_t *htsp, htsmsg_t *m, pktbuf_t *pb,
	  htsp_msg_q_t *hmq, int payloadsize)
{
  htsp_msg_t *hm = malloc(sizeof(htsp_msg_t));

  hm->hm_msg = m;
  hm->hm_pb = pb;
  if(pb != NULL)
    pktbuf_ref_inc(pb);
  hm->hm_payloadsize = payloadsize;
  
  pthread_mutex_lock(&htsp->htsp_out_mutex);

  TAILQ_INSERT_TAIL(&hmq->hmq_q, hm, hm_link);

  if(hmq->hmq_length == 0) {
    /* Activate queue */

    if(hmq->hmq_strict_prio) {
      TAILQ_INSERT_HEAD(&htsp->htsp_active_output_queues, hmq, hmq_link);
    } else {
      TAILQ_INSERT_TAIL(&htsp->htsp_active_output_queues, hmq, hmq_link);
    }
  }

  hmq->hmq_length++;
  hmq->hmq_payload += payloadsize;
  pthread_cond_signal(&htsp->htsp_out_cond);
  pthread_mutex_unlock(&htsp->htsp_out_mutex);
}

/**
 *
 */
static void
htsp_send_message(htsp_connection_t *htsp, htsmsg_t *m, htsp_msg_q_t *hmq)
{
  htsp_send(htsp, m, NULL, hmq ?: &htsp->htsp_hmq_ctrl, 0);
}


/**
 *
 */
static htsmsg_t *
htsp_build_channel(channel_t *ch, const char *method)
{
  channel_tag_mapping_t *ctm;
  channel_tag_t *ct;
  service_t *t;

  htsmsg_t *out = htsmsg_create_map();
  htsmsg_t *tags = htsmsg_create_list();
  htsmsg_t *services = htsmsg_create_list();

  htsmsg_add_u32(out, "channelId", ch->ch_id);
  htsmsg_add_u32(out, "channelNumber", ch->ch_number);

  htsmsg_add_str(out, "channelName", ch->ch_name);
  if(ch->ch_icon != NULL)
    htsmsg_add_str(out, "channelIcon", ch->ch_icon);

  htsmsg_add_u32(out, "eventId",
		 ch->ch_epg_current != NULL ? ch->ch_epg_current->e_id : 0);
  htsmsg_add_u32(out, "nextEventId",
		 ch->ch_epg_next ? ch->ch_epg_next->e_id : 0);

  LIST_FOREACH(ctm, &ch->ch_ctms, ctm_channel_link) {
    ct = ctm->ctm_tag;
    if(ct->ct_enabled && !ct->ct_internal)
      htsmsg_add_u32(tags, NULL, ct->ct_identifier);
  }

  LIST_FOREACH(t, &ch->ch_services, s_ch_link) {
    htsmsg_t *svcmsg = htsmsg_create_map();
    uint16_t caid;
    htsmsg_add_str(svcmsg, "name", service_nicename(t));
    htsmsg_add_str(svcmsg, "type", service_servicetype_txt(t));
    if((caid = service_get_encryption(t)) != 0) {
      htsmsg_add_u32(svcmsg, "caid", caid);
      htsmsg_add_str(svcmsg, "caname", psi_caid2name(caid));
    }
    htsmsg_add_msg(services, NULL, svcmsg);
  }

  htsmsg_add_msg(out, "services", services);
  htsmsg_add_msg(out, "tags", tags);
  htsmsg_add_str(out, "method", method);
  return out;
}


/**
 *
 */
static htsmsg_t *
htsp_build_tag(channel_tag_t *ct, const char *method, int include_channels)
{
  channel_tag_mapping_t *ctm;
  htsmsg_t *out = htsmsg_create_map();
  htsmsg_t *members = include_channels ? htsmsg_create_list() : NULL;
 
  htsmsg_add_u32(out, "tagId", ct->ct_identifier);

  htsmsg_add_str(out, "tagName", ct->ct_name);
  htsmsg_add_str(out, "tagIcon", ct->ct_icon);
  htsmsg_add_u32(out, "tagTitledIcon", ct->ct_titled_icon);

  if(members != NULL) {
    LIST_FOREACH(ctm, &ct->ct_ctms, ctm_tag_link)
      htsmsg_add_u32(members, NULL, ctm->ctm_channel->ch_id);
    htsmsg_add_msg(out, "members", members);
  }

  htsmsg_add_str(out, "method", method);
  return out;
}


/**
 *
 */
static htsmsg_t *
htsp_build_dvrentry(dvr_entry_t *de, const char *method)
{
  htsmsg_t *out = htsmsg_create_map();
  const char *s = NULL, *error = NULL;

  htsmsg_add_u32(out, "id", de->de_id);
  htsmsg_add_u32(out, "channel", de->de_channel->ch_id);

  htsmsg_add_s32(out, "start", de->de_start);
  htsmsg_add_s32(out, "stop", de->de_stop);

  if( de->de_title != NULL )
    htsmsg_add_str(out, "title", de->de_title);
  if( de->de_desc != NULL )
    htsmsg_add_str(out, "description", de->de_desc);

  switch(de->de_sched_state) {
  case DVR_SCHEDULED:
    s = "scheduled";
    break;
  case DVR_RECORDING:
    s = "recording";
    break;
  case DVR_COMPLETED:
    s = "completed";
    if(de->de_last_error)
      error = streaming_code2txt(de->de_last_error);
    break;
  case DVR_MISSED_TIME:
    s = "missed";
    break;
  case DVR_NOSTATE:
    s = "invalid";
    break;
  }

  htsmsg_add_str(out, "state", s);
  if(error)
    htsmsg_add_str(out, "error", error);
  htsmsg_add_str(out, "method", method);
  return out;
}


/** 
 * Simple function to respond with an error
 */
static htsmsg_t *
htsp_error(const char *err)
{
  htsmsg_t *r = htsmsg_create_map();
  htsmsg_add_str(r, "error", err);
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
  dvr_entry_t *de;
  htsmsg_t *m;

  /* First, just OK the async request */
  htsp_reply(htsp, in, htsmsg_create_map()); 

  if(htsp->htsp_async_mode)
    return NULL; /* already in async mode */

  htsp->htsp_async_mode = 1;

  /* Send all enabled and external tags */
  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    if(ct->ct_enabled && !ct->ct_internal)
      htsp_send_message(htsp, htsp_build_tag(ct, "tagAdd", 0), NULL);
  
  /* Send all channels */
  RB_FOREACH(ch, &channel_name_tree, ch_name_link)
    htsp_send_message(htsp, htsp_build_channel(ch, "channelAdd"), NULL);
    
  /* Send all enabled and external tags (now with channel mappings) */
  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    if(ct->ct_enabled && !ct->ct_internal)
      htsp_send_message(htsp, htsp_build_tag(ct, "tagUpdate", 1), NULL);

  /* Send all DVR entries */
  LIST_FOREACH(de, &dvrentries, de_global_link)
    htsp_send_message(htsp, htsp_build_dvrentry(de, "dvrEntryAdd"), NULL);

  /* Notify that initial sync has been completed */
  m = htsmsg_create_map();
  htsmsg_add_str(m, "method", "initialSyncCompleted");
  htsp_send_message(htsp, m, NULL);

  /* Insert in list so it will get all updates */
  LIST_INSERT_HEAD(&htsp_async_connections, htsp, htsp_async_link);

  return NULL;
}

/**
 * add a Dvrentry
 */
static htsmsg_t * 
htsp_method_addDvrEntry(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out;
  uint32_t eventid;
  event_t *e;
  dvr_entry_t *de;
  dvr_entry_sched_state_t dvr_status;
  const char *dvr_config_name;

  if((dvr_config_name = htsmsg_get_str(in, "configName")) == NULL)
    dvr_config_name = "";

  if(htsmsg_get_u32(in, "eventId", &eventid))
    eventid = -1;

  if ((e = epg_event_find_by_id(eventid)) == NULL)
  {
    uint32_t iChannelId, iStartTime, iStopTime, iPriority;
    channel_t *channel;
    const char *strTitle = NULL, *strDescription = NULL, *strCreator = NULL;

    // no event found with this event id.
    // check if there is at least a start time, stop time, channel id and title set
    if (htsmsg_get_u32(in, "channelId", &iChannelId) ||
        htsmsg_get_u32(in, "start", &iStartTime) ||
        htsmsg_get_u32(in, "stop", &iStopTime) ||
        (strTitle = htsmsg_get_str(in, "title")) == NULL)
    {
      // not enough info available to create a new entry
      return htsp_error("Invalid arguments");
    }

    // invalid channel
    if ((channel = channel_find_by_identifier(iChannelId)) == NULL)
      return htsp_error("Channel does not exist");

    // get the optional attributes
    if (htsmsg_get_u32(in, "priority", &iPriority))
      iPriority = DVR_PRIO_NORMAL;

    if ((strDescription = htsmsg_get_str(in, "description")) == NULL)
      strDescription = "";

    if ((strCreator = htsmsg_get_str(in, "creator")) == NULL || strcmp(strCreator, "") == 0)
      strCreator = htsp->htsp_username ? htsp->htsp_username : "anonymous";

    // create the dvr entry
    de = dvr_entry_create(dvr_config_name, channel, iStartTime, iStopTime, strTitle, strDescription, strCreator, NULL, NULL, 0, iPriority);
  }
  else
  {
    //create the dvr entry
    de = dvr_entry_create_by_event(dvr_config_name,e,
                                   htsp->htsp_username ?
                                   htsp->htsp_username : "anonymous",
                                   NULL, DVR_PRIO_NORMAL);
  }

  dvr_status = de != NULL ? de->de_sched_state : DVR_NOSTATE;
  
  //create response
  out = htsmsg_create_map();
  
  switch(dvr_status) {
  case DVR_SCHEDULED:
  case DVR_RECORDING:
  case DVR_MISSED_TIME:
  case DVR_COMPLETED:
    htsmsg_add_u32(out, "id", de->de_id);
    htsmsg_add_u32(out, "success", 1);
    break;  
  case DVR_NOSTATE:
    htsmsg_add_str(out, "error", "Could not add dvrEntry");
    htsmsg_add_u32(out, "success", 0);
    break;
  }
  return out;
}

/**
 * update a Dvrentry
 */
static htsmsg_t *
htsp_method_updateDvrEntry(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out;
  uint32_t dvrEntryId;
  dvr_entry_t *de;
  uint32_t start;
  uint32_t stop;
  const char *title = NULL;
    
  if(htsmsg_get_u32(in, "id", &dvrEntryId))
    return htsp_error("Missing argument 'id'");
  
  if( (de = dvr_entry_find_by_id(dvrEntryId)) == NULL) 
    return htsp_error("id not found");

  if(htsmsg_get_u32(in, "start", &start))
    start = de->de_start;
  
  if(htsmsg_get_u32(in, "stop", &stop))
    stop = de->de_stop;

  title = htsmsg_get_str(in, "title");
  if (title == NULL)
    title = de->de_title;

  de = dvr_entry_update(de, title, start, stop);

  //create response
  out = htsmsg_create_map();
  htsmsg_add_u32(out, "success", 1);
  
  return out;
}

/**
 * cancel a Dvrentry
 */
static htsmsg_t *
htsp_method_cancelDvrEntry(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out;
  uint32_t dvrEntryId;
  dvr_entry_t *de;

  if(htsmsg_get_u32(in, "id", &dvrEntryId))
    return htsp_error("Missing argument 'id'");

  if( (de = dvr_entry_find_by_id(dvrEntryId)) == NULL)
    return htsp_error("id not found");

  dvr_entry_cancel(de);

  //create response
  out = htsmsg_create_map();
  htsmsg_add_u32(out, "success", 1);

  return out;
}

/**
 * delete a Dvrentry
 */
static htsmsg_t *
htsp_method_deleteDvrEntry(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out;
  uint32_t dvrEntryId;
  dvr_entry_t *de;

  if(htsmsg_get_u32(in, "id", &dvrEntryId))
    return htsp_error("Missing argument 'id'");

  if( (de = dvr_entry_find_by_id(dvrEntryId)) == NULL)
    return htsp_error("id not found");

  dvr_entry_cancel_delete(de);

  //create response
  out = htsmsg_create_map();
  htsmsg_add_u32(out, "success", 1);

  return out;
}

/**
 *
 * do an epg query
 */
static htsmsg_t *
htsp_method_epgQuery(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out, *eventIds;
  const char *query;
  int c, i;
  uint32_t channelid, tagid, epg_content_dvbcode = 0;
  channel_t *ch = NULL;
  channel_tag_t *ct = NULL;
  epg_query_result_t eqr;
  
  //only mandatory parameter is the query
  if( (query = htsmsg_get_str(in, "query")) == NULL )
    return htsp_error("Missing argument 'query'");
  
  if( !(htsmsg_get_u32(in, "channelId", &channelid)) )
    ch = channel_find_by_identifier(channelid);

  if( !(htsmsg_get_u32(in, "tagId", &tagid)) )
    ct = channel_tag_find_by_identifier(tagid);

  htsmsg_get_u32(in, "contentType", &epg_content_dvbcode);

  //do the query
  epg_query0(&eqr, ch, ct, epg_content_dvbcode, query);
  c = eqr.eqr_entries;

  // create reply
  out = htsmsg_create_map();
  if( c ) {
    eventIds = htsmsg_create_list();
    for(i = 0; i < c; ++i) {
        htsmsg_add_u32(eventIds, NULL, eqr.eqr_array[i]->e_id);
    }
    htsmsg_add_msg(out, "eventIds", eventIds);
  }
  
  epg_query_free(&eqr);
  
  return out;
}

/**
 *
 */
static htsmsg_t *
htsp_build_event(event_t *e)
{
  htsmsg_t *out;
  event_t *n;

  out = htsmsg_create_map();

  htsmsg_add_u32(out, "channelId", e->e_channel->ch_id);
  htsmsg_add_u32(out, "start", e->e_start);
  htsmsg_add_u32(out, "stop", e->e_stop);
  if(e->e_title != NULL)
    htsmsg_add_str(out, "title", e->e_title);
  if(e->e_desc != NULL)
    htsmsg_add_str(out, "description", e->e_desc);
  if(e->e_ext_desc != NULL)
    htsmsg_add_str(out, "ext_desc", e->e_ext_desc);
  if(e->e_ext_item != NULL)
    htsmsg_add_str(out, "ext_item", e->e_ext_item);
  if(e->e_ext_text != NULL)
    htsmsg_add_str(out, "ext_text", e->e_ext_text);

  if(e->e_content_type)
    htsmsg_add_u32(out, "contentType", e->e_content_type);

  n = RB_NEXT(e, e_channel_link);
  if(n != NULL)
    htsmsg_add_u32(out, "nextEventId", n->e_id);

  return out;
}

/**
 * Get information about the given event + 
 * n following events
 */
static htsmsg_t *
htsp_method_getEvents(htsp_connection_t *htsp, htsmsg_t *in)
{
  uint32_t eventid, numFollowing;
  htsmsg_t *out, *events;
  event_t *e;

  if(htsmsg_get_u32(in, "eventId", &eventid))
    return htsp_error("Missing argument 'eventId'");

  if(htsmsg_get_u32(in, "numFollowing", &numFollowing))
    return htsp_error("Missing argument 'numFollowing'");

  if((e = epg_event_find_by_id(eventid)) == NULL) {
    return htsp_error("Event does not exist");
  }

  out = htsmsg_create_map();
  events = htsmsg_create_list();
  
  htsmsg_add_msg(events, NULL, htsp_build_event(e));
  while( numFollowing-- > 0 ) {
    e = RB_NEXT(e, e_channel_link);
    if( e == NULL ) 
      break;
    htsmsg_add_msg(events, NULL, htsp_build_event(e));
  }
  
  htsmsg_add_msg(out, "events", events);
  return out;
}


/**
 * Get information about the given event
 */
static htsmsg_t *
htsp_method_getEvent(htsp_connection_t *htsp, htsmsg_t *in)
{
  uint32_t eventid;
  event_t *e;
  htsmsg_t *out;
  
  if(htsmsg_get_u32(in, "eventId", &eventid))
    return htsp_error("Missing argument 'eventId'");

  if((e = epg_event_find_by_id(eventid)) == NULL)
    return htsp_error("Event does not exist");

  out = htsp_build_event(e);  
  return out;
}

/**
 * Get total and free disk space on configured path
 */
static htsmsg_t *
htsp_method_getDiskSpace(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out;
  struct statvfs diskdata;
  dvr_config_t *cfg = dvr_config_find_by_name_default("");

  if(statvfs(cfg->dvr_storage,&diskdata) == -1)
    return htsp_error("Unable to stat path");
  
  out = htsmsg_create_map();
  htsmsg_add_s64(out, "freediskspace",
		 diskdata.f_bsize * (int64_t)diskdata.f_bavail);
  htsmsg_add_s64(out, "totaldiskspace",
		 diskdata.f_bsize * (int64_t)diskdata.f_blocks);
  return out;
}


/**
 * Get system time and diff to GMT
 */
static htsmsg_t *
htsp_method_getSysTime(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out;
  struct timeval tv;

  if(gettimeofday(&tv, NULL) == -1)
    return htsp_error("Unable to get system time"); 

  out = htsmsg_create_map();
  htsmsg_add_s32(out, "time", tv.tv_sec);
  htsmsg_add_s32(out, "timezone", timezone / 60);
  return out;
}


/**
 * Request subscription for a channel
 */
static htsmsg_t *
htsp_method_subscribe(htsp_connection_t *htsp, htsmsg_t *in)
{
  uint32_t chid, sid, weight;
  channel_t *ch;
  htsp_subscription_t *hs;

  if(htsmsg_get_u32(in, "channelId", &chid))
    return htsp_error("Missing argument 'channeId'");

  if(htsmsg_get_u32(in, "subscriptionId", &sid))
    return htsp_error("Missing argument 'subscriptionId'");

  if((ch = channel_find_by_identifier(chid)) == NULL)
    return htsp_error("Requested channel does not exist");

  weight = htsmsg_get_u32_or_default(in, "weight", 150);

  /*
   * We send the reply now to avoid the user getting the 'subscriptionStart'
   * async message before the reply to 'subscribe'.
   */
  htsp_reply(htsp, in, htsmsg_create_map());

  /* Initialize the HTSP subscription structure */

  hs = calloc(1, sizeof(htsp_subscription_t));

  hs->hs_htsp = htsp;
  htsp_init_queue(&hs->hs_q, 0);

  hs->hs_sid = sid;
  LIST_INSERT_HEAD(&htsp->htsp_subscriptions, hs, hs_link);
  streaming_target_init(&hs->hs_input, htsp_streaming_input, hs, 0);

  hs->hs_s = subscription_create_from_channel(ch, weight,
					      htsp->htsp_logname,
					      &hs->hs_input, 0);
  return NULL;
}


/**
 * Request unsubscription for a channel
 */
static htsmsg_t *
htsp_method_unsubscribe(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsp_subscription_t *s;
  uint32_t sid;

  if(htsmsg_get_u32(in, "subscriptionId", &sid))
    return htsp_error("Missing argument 'subscriptionId'");

  LIST_FOREACH(s, &htsp->htsp_subscriptions, hs_link)
    if(s->hs_sid == sid)
      break;
  
  /*
   * We send the reply here or the user will get the 'subscriptionStop'
   * async message before the reply to 'unsubscribe'.
   */
  htsp_reply(htsp, in, htsmsg_create_map());

  if(s == NULL)
    return NULL; /* Subscription did not exist, but we don't really care */

  htsp_subscription_destroy(htsp, s);
  return NULL;
}


/**
 * Change weight for a subscription
 */
static htsmsg_t *
htsp_method_change_weight(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsp_subscription_t *hs;
  uint32_t sid, weight;

  if(htsmsg_get_u32(in, "subscriptionId", &sid))
    return htsp_error("Missing argument 'subscriptionId'");

 weight = htsmsg_get_u32_or_default(in, "weight", 150);

  LIST_FOREACH(hs, &htsp->htsp_subscriptions, hs_link)
    if(hs->hs_sid == sid)
      break;
  
  if(hs == NULL)
    return htsp_error("Requested subscription does not exist");

  htsp_reply(htsp, in, htsmsg_create_map());

  subscription_change_weight(hs->hs_s, weight);
  return NULL;
}


/**
 * Try to authenticate
 */
static htsmsg_t *
htsp_method_authenticate(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *r = htsmsg_create_map();

  if(!(htsp->htsp_granted_access & HTSP_PRIV_MASK))
    htsmsg_add_u32(r, "noaccess", 1);
  
  return r;
}


/**
 * Update challenge
 */
static int
htsp_generate_challenge(htsp_connection_t *htsp)
{
  int fd, n;

  if((fd = tvh_open("/dev/urandom", O_RDONLY, 0)) < 0)
    return -1;
  
  n = read(fd, &htsp->htsp_challenge, 32);
  close(fd);
  return n != 32;
}



/**
 * Hello, for protocol version negotiation
 */
static htsmsg_t *
htsp_method_hello(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *r = htsmsg_create_map();
  uint32_t v;
  const char *name;

  if(htsmsg_get_u32(in, "htspversion", &v))
    return htsp_error("Missing argument 'htspversion'");

  if((name = htsmsg_get_str(in, "clientname")) == NULL)
    return htsp_error("Missing argument 'clientname'");

  tvh_str_update(&htsp->htsp_clientname, htsmsg_get_str(in, "clientname"));

  tvhlog(LOG_INFO, "htsp", "%s: Welcomed client software: %s",
	 htsp->htsp_logname, name);

  htsmsg_add_u32(r, "htspversion", HTSP_PROTO_VERSION);
  htsmsg_add_str(r, "servername", "HTS Tvheadend");
  htsmsg_add_str(r, "serverversion", htsversion);
  htsmsg_add_bin(r, "challenge", htsp->htsp_challenge, 32);

  htsp_update_logname(htsp);
  return r;
}



/**
 * HTSP methods
 */
struct {
  const char *name;
  htsmsg_t *(*fn)(htsp_connection_t *htsp, htsmsg_t *in);
  int privmask;
} htsp_methods[] = {
  { "hello", htsp_method_hello, 0},
  { "authenticate", htsp_method_authenticate, 0},
  { "enableAsyncMetadata", htsp_method_async, ACCESS_STREAMING},
  { "getEvent", htsp_method_getEvent, ACCESS_STREAMING},
  { "getEvents", htsp_method_getEvents, ACCESS_STREAMING},
  { "getDiskSpace", htsp_method_getDiskSpace, ACCESS_STREAMING},
  { "getSysTime", htsp_method_getSysTime, ACCESS_STREAMING},
  { "subscribe", htsp_method_subscribe, ACCESS_STREAMING},
  { "unsubscribe", htsp_method_unsubscribe, ACCESS_STREAMING},
  { "subscriptionChangeWeight", htsp_method_change_weight, ACCESS_STREAMING},
  { "addDvrEntry", htsp_method_addDvrEntry, ACCESS_RECORDER},
  { "updateDvrEntry", htsp_method_updateDvrEntry, ACCESS_RECORDER},
  { "cancelDvrEntry", htsp_method_cancelDvrEntry, ACCESS_RECORDER},
  { "deleteDvrEntry", htsp_method_deleteDvrEntry, ACCESS_RECORDER},
  { "epgQuery", htsp_method_epgQuery, ACCESS_STREAMING},

};

#define NUM_METHODS (sizeof(htsp_methods) / sizeof(htsp_methods[0]))


/**
 * Raise privs by field in message
 */
static void
htsp_authenticate(htsp_connection_t *htsp, htsmsg_t *m)
{
  const char *username;
  const void *digest;
  size_t digestlen;
  uint32_t access;
  int privgain;
  int match;

  if((username = htsmsg_get_str(m, "username")) == NULL)
    return;

  if(strcmp(htsp->htsp_username ?: "", username)) {
    tvhlog(LOG_INFO, "htsp", "%s: Identified as user %s", 
	   htsp->htsp_logname, username);
    tvh_str_update(&htsp->htsp_username, username);
    htsp_update_logname(htsp);
  }


  if(htsmsg_get_bin(m, "digest", &digest, &digestlen))
    return;

  access = access_get_hashed(username, digest, htsp->htsp_challenge,
			     (struct sockaddr *)htsp->htsp_peer, &match);

  privgain = (access | htsp->htsp_granted_access) != htsp->htsp_granted_access;
    
  if(privgain)
    tvhlog(LOG_INFO, "htsp", "%s: Privileges raised", htsp->htsp_logname);

  htsp->htsp_granted_access |= access;
}


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
  const char *method;

  if(htsp_generate_challenge(htsp)) {
    tvhlog(LOG_ERR, "htsp", "%s: Unable to generate challenge",
	   htsp->htsp_logname);
    return 1;
  }

  pthread_mutex_lock(&global_lock);
  htsp->htsp_granted_access = 
    access_get_by_addr((struct sockaddr *)htsp->htsp_peer);
  pthread_mutex_unlock(&global_lock);

  tvhlog(LOG_INFO, "htsp", "Got connection from %s", htsp->htsp_logname);

  /* Session main loop */

  while(1) {
  readmsg:
    if((r = htsp_read_message(htsp, &m, 0)) != 0)
      return r;

    pthread_mutex_lock(&global_lock);
    htsp_authenticate(htsp, m);

    if((method = htsmsg_get_str(m, "method")) != NULL) {
      for(i = 0; i < NUM_METHODS; i++) {
	if(!strcmp(method, htsp_methods[i].name)) {

	  if((htsp->htsp_granted_access & htsp_methods[i].privmask) != 
	     htsp_methods[i].privmask) {

	    pthread_mutex_unlock(&global_lock);

	    /* Classic authentication failed delay */
	    usleep(250000);
	    
	    reply = htsmsg_create_map();
	    htsmsg_add_u32(reply, "noaccess", 1);
	    htsp_reply(htsp, m, reply);

	    htsmsg_destroy(m);
	    goto readmsg;

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
    hmq->hmq_payload -= hm->hm_payloadsize;

    TAILQ_REMOVE(&htsp->htsp_active_output_queues, hmq, hmq_link);
    if(hmq->hmq_length) {
      /* Still messages to be sent, put back in active queues */
      if(hmq->hmq_strict_prio) {
	TAILQ_INSERT_HEAD(&htsp->htsp_active_output_queues, hmq, hmq_link);
      } else {
	TAILQ_INSERT_TAIL(&htsp->htsp_active_output_queues, hmq, hmq_link);
      }
    }

    pthread_mutex_unlock(&htsp->htsp_out_mutex);

    r = htsmsg_binary_serialize(hm->hm_msg, &dptr, &dlen, INT32_MAX);

#if 0
    if(hm->hm_pktref) {
      usleep(hm->hm_payloadsize * 3);
    }
#endif
    htsp_msg_destroy(hm);
   
    /* ignore return value */ 
    r = write(htsp->htsp_fd, dptr, dlen);
    if(r != dlen)
      tvhlog(LOG_INFO, "htsp", "%s: Write error -- %s", 
	     htsp->htsp_logname, strerror(errno));
    free(dptr);
    pthread_mutex_lock(&htsp->htsp_out_mutex);
    if(r != dlen) 
      break;
  }

  pthread_mutex_unlock(&htsp->htsp_out_mutex);
  return NULL;
}


/**
 *
 */
static void
htsp_serve(int fd, void *opaque, struct sockaddr_in *source,
	   struct sockaddr_in *self)
{
  htsp_connection_t htsp;
  char buf[30];
  htsp_subscription_t *s;

  snprintf(buf, sizeof(buf), "%s", inet_ntoa(source->sin_addr));

  memset(&htsp, 0, sizeof(htsp_connection_t));

  TAILQ_INIT(&htsp.htsp_active_output_queues);

  htsp_init_queue(&htsp.htsp_hmq_ctrl, 0);
  htsp_init_queue(&htsp.htsp_hmq_qstatus, 1);
  htsp_init_queue(&htsp.htsp_hmq_epg, 0);

  htsp.htsp_peername = strdup(buf);
  htsp_update_logname(&htsp);

  htsp.htsp_fd = fd;
  htsp.htsp_peer = source;
  htsp.htsp_writer_run = 1;

  pthread_create(&htsp.htsp_writer_thread, NULL, htsp_write_scheduler, &htsp);

  /**
   * Reader loop
   */

  htsp_read_loop(&htsp);

  tvhlog(LOG_INFO, "htsp", "%s: Disconnected", htsp.htsp_logname);

  /**
   * Ok, we're back, other end disconnected. Clean up stuff.
   */

  pthread_mutex_lock(&global_lock);

  /* Beware! Closing subscriptions will invoke a lot of callbacks
     down in the streaming code. So we do this as early as possible
     to avoid any weird lockups */
  while((s = LIST_FIRST(&htsp.htsp_subscriptions)) != NULL) {
    htsp_subscription_destroy(&htsp, s);
  }

  if(htsp.htsp_async_mode)
    LIST_REMOVE(&htsp, htsp_async_link);

  pthread_mutex_unlock(&global_lock);

  free(htsp.htsp_logname);
  free(htsp.htsp_peername);
  free(htsp.htsp_username);
  free(htsp.htsp_clientname);

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
  htsp_server = tcp_server_create(9982, htsp_serve, NULL);
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
htsp_channgel_update_current(channel_t *ch)
{
  htsmsg_t *m;
  time_t now;

  time(&now);
  m = htsmsg_create_map();
  htsmsg_add_str(m, "method", "channelUpdate");
  htsmsg_add_u32(m, "channelId", ch->ch_id);

  htsmsg_add_u32(m, "eventId",
		 ch->ch_epg_current ? ch->ch_epg_current->e_id : 0);
  htsmsg_add_u32(m, "nextEventId",
		 ch->ch_epg_next ? ch->ch_epg_next->e_id : 0);
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
  htsmsg_t *m = htsmsg_create_map();
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
  htsp_async_send(htsp_build_tag(ct, "tagAdd", 1));
}


/**
 * Called from channel.c when an exported tag is changed
 */
void
htsp_tag_update(channel_tag_t *ct)
{
  htsp_async_send(htsp_build_tag(ct, "tagUpdate", 1));
}


/**
 * Called from channel.c when an exported tag is deleted
 */
void
htsp_tag_delete(channel_tag_t *ct)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "tagId", ct->ct_identifier);
  htsmsg_add_str(m, "method", "tagDelete");
  htsp_async_send(m);
}


/**
 * Called from dvr_db.c when a DVR entry is created
 */
void
htsp_dvr_entry_add(dvr_entry_t *de)
{
  htsp_async_send(htsp_build_dvrentry(de, "dvrEntryAdd"));
}


/**
 * Called from dvr_db.c when a DVR entry is updated
 */
void
htsp_dvr_entry_update(dvr_entry_t *de)
{
  htsp_async_send(htsp_build_dvrentry(de, "dvrEntryUpdate"));
}


/**
 * Called from dvr_db.c when a DVR entry is deleted
 */
void
htsp_dvr_entry_delete(dvr_entry_t *de)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", de->de_id);
  htsmsg_add_str(m, "method", "dvrEntryDelete");
  htsp_async_send(m);
}


const static char frametypearray[PKT_NTYPES] = {
  [PKT_I_FRAME] = 'I',
  [PKT_P_FRAME] = 'P',
  [PKT_B_FRAME] = 'B',
};

/**
 * Build a htsmsg from a th_pkt and enqueue it on our HTSP service
 */
static void
htsp_stream_deliver(htsp_subscription_t *hs, th_pkt_t *pkt)
{
  htsmsg_t *m = htsmsg_create_map(), *n;
  htsp_msg_t *hm;
  htsp_connection_t *htsp = hs->hs_htsp;
  int64_t ts;
  int qlen = hs->hs_q.hmq_payload;

  if((qlen > 500000 && pkt->pkt_frametype == PKT_B_FRAME) ||
     (qlen > 750000 && pkt->pkt_frametype == PKT_P_FRAME) || 
     (qlen > 1500000)) {

    hs->hs_dropstats[pkt->pkt_frametype]++;

    /* Queue size protection */
    pkt_ref_dec(pkt);
    return;
  }
 
  htsmsg_add_str(m, "method", "muxpkt");
  htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);
  htsmsg_add_u32(m, "frametype", frametypearray[pkt->pkt_frametype]);

  htsmsg_add_u32(m, "stream", pkt->pkt_componentindex);
  htsmsg_add_u32(m, "com", pkt->pkt_commercial);


  if(pkt->pkt_pts != PTS_UNSET) {
    int64_t pts = ts_rescale(pkt->pkt_pts, 1000000);
    htsmsg_add_s64(m, "pts", pts);
  }

  if(pkt->pkt_dts != PTS_UNSET) {
    int64_t dts = ts_rescale(pkt->pkt_dts, 1000000);
    htsmsg_add_s64(m, "dts", dts);
  }

  uint32_t dur = ts_rescale(pkt->pkt_duration, 1000000);
  htsmsg_add_u32(m, "duration", dur);
  
  pkt = pkt_merge_header(pkt);

  /**
   * Since we will serialize directly we use 'binptr' which is a binary
   * object that just points to data, thus avoiding a copy.
   */
  htsmsg_add_binptr(m, "payload", pktbuf_ptr(pkt->pkt_payload),
		    pktbuf_len(pkt->pkt_payload));
  htsp_send(htsp, m, pkt->pkt_payload, &hs->hs_q, pktbuf_len(pkt->pkt_payload));

  if(hs->hs_last_report != dispatch_clock) {
    signal_status_t status;

    /* Send a queue and signal status report every second */

    hs->hs_last_report = dispatch_clock;

    m = htsmsg_create_map();
    htsmsg_add_str(m, "method", "queueStatus");
    htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);
    htsmsg_add_u32(m, "packets", hs->hs_q.hmq_length);
    htsmsg_add_u32(m, "bytes", hs->hs_q.hmq_payload);

    /**
     * Figure out real time queue delay 
     */
    
    pthread_mutex_lock(&htsp->htsp_out_mutex);

    if(TAILQ_FIRST(&hs->hs_q.hmq_q) == NULL) {
      htsmsg_add_s64(m, "delay", 0);
    } else if((hm = TAILQ_FIRST(&hs->hs_q.hmq_q)) != NULL &&
	      (n = hm->hm_msg) != NULL && !htsmsg_get_s64(n, "dts", &ts) &&
	      pkt->pkt_dts != PTS_UNSET && ts != PTS_UNSET) {
      htsmsg_add_s64(m, "delay", pkt->pkt_dts - ts);
    }
    pthread_mutex_unlock(&htsp->htsp_out_mutex);

    htsmsg_add_u32(m, "Bdrops", hs->hs_dropstats[PKT_B_FRAME]);
    htsmsg_add_u32(m, "Pdrops", hs->hs_dropstats[PKT_P_FRAME]);
    htsmsg_add_u32(m, "Idrops", hs->hs_dropstats[PKT_I_FRAME]);

    /* We use a special queue for queue status message so they're not
       blocked by anything else */
    htsp_send_message(hs->hs_htsp, m, &hs->hs_htsp->htsp_hmq_qstatus);


    if(!service_get_signal_status(hs->hs_s->ths_service, &status)) {

      m = htsmsg_create_map();
      htsmsg_add_str(m, "method", "signalStatus");
      htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);

      htsmsg_add_str(m, "feStatus",   status.status_text);
      if(status.snr != -2)
	htsmsg_add_u32(m, "feSNR",    status.snr);
      if(status.signal != -2)
	htsmsg_add_u32(m, "feSignal", status.signal);
      if(status.ber != -2)
	htsmsg_add_u32(m, "feBER",    status.ber);
      if(status.unc != -2)
	htsmsg_add_u32(m, "feUNC",    status.unc);
      htsp_send_message(hs->hs_htsp, m, &hs->hs_htsp->htsp_hmq_qstatus);
    }

  }
  pkt_ref_dec(pkt);
}


/**
 * Send a 'subscriptionStart' message to client informing about
 * delivery start and all components.
 */
static void
htsp_subscription_start(htsp_subscription_t *hs, const streaming_start_t *ss)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_t *streams = htsmsg_create_list();
  htsmsg_t *c;
  htsmsg_t *sourceinfo = htsmsg_create_map();
  int i;
  const source_info_t *si = &ss->ss_si;

  for(i = 0; i < ss->ss_num_components; i++) {
    const streaming_start_component_t *ssc = &ss->ss_components[i];

    c = htsmsg_create_map();
    htsmsg_add_u32(c, "index", ssc->ssc_index);
    htsmsg_add_str(c, "type", streaming_component_type2txt(ssc->ssc_type));
    if(ssc->ssc_lang[0])
      htsmsg_add_str(c, "language", ssc->ssc_lang);
    
    if(ssc->ssc_type == SCT_DVBSUB) {
      htsmsg_add_u32(c, "composition_id", ssc->ssc_composition_id);
      htsmsg_add_u32(c, "ancillary_id", ssc->ssc_ancillary_id);
    }

    if(ssc->ssc_type == SCT_MPEG2VIDEO || ssc->ssc_type == SCT_H264) {
      if(ssc->ssc_width)
	htsmsg_add_u32(c, "width", ssc->ssc_width);
      if(ssc->ssc_height)
	htsmsg_add_u32(c, "height", ssc->ssc_height);
      if (ssc->ssc_aspect_num)
        htsmsg_add_u32(c, "aspect_num", ssc->ssc_aspect_num);
      if (ssc->ssc_aspect_den)
        htsmsg_add_u32(c, "aspect_den", ssc->ssc_aspect_den);
    }

    if (SCT_ISAUDIO(ssc->ssc_type))
    {
      if (ssc->ssc_channels)
        htsmsg_add_u32(c, "channels", ssc->ssc_channels);
      if (ssc->ssc_sri)
        htsmsg_add_u32(c, "rate", ssc->ssc_sri);
    }

    htsmsg_add_msg(streams, NULL, c);
  }
  
  htsmsg_add_msg(m, "streams", streams);

  if(si->si_adapter ) htsmsg_add_str(sourceinfo, "adapter",  si->si_adapter );
  if(si->si_mux     ) htsmsg_add_str(sourceinfo, "mux"    ,  si->si_mux     );
  if(si->si_network ) htsmsg_add_str(sourceinfo, "network",  si->si_network );
  if(si->si_provider) htsmsg_add_str(sourceinfo, "provider", si->si_provider);
  if(si->si_service ) htsmsg_add_str(sourceinfo, "service",  si->si_service );
  
  htsmsg_add_msg(m, "sourceinfo", sourceinfo);
 
  htsmsg_add_str(m, "method", "subscriptionStart");
  htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);
  htsp_send(hs->hs_htsp, m, NULL, &hs->hs_q, 0);
}


/**
 * Send a 'subscriptionStart' stop
 */
static void
htsp_subscription_stop(htsp_subscription_t *hs, const char *err)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "method", "subscriptionStop");
  htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);

  if(err != NULL)
    htsmsg_add_str(m, "status", err);

  htsp_send(hs->hs_htsp, m, NULL, &hs->hs_q, 0);
}


/**
 * Send a 'subscriptionStatus' message
 */
static void
htsp_subscription_status(htsp_subscription_t *hs, const char *err)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "method", "subscriptionStatus");
  htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);

  if(err != NULL)
    htsmsg_add_str(m, "status", err);

  htsp_send(hs->hs_htsp, m, NULL, &hs->hs_q, 0);
}


/**
 *
 */
static void
htsp_subscription_service_status(htsp_subscription_t *hs, int status)
{
  if(status & TSS_PACKETS) {
    htsp_subscription_status(hs, NULL);
  } else if(status & (TSS_GRACEPERIOD | TSS_ERRORS)) {
    htsp_subscription_status(hs, service_tss2text(status));
  }
}

/**
 *
 */
static void
htsp_streaming_input(void *opaque, streaming_message_t *sm)
{
  htsp_subscription_t *hs = opaque;

  switch(sm->sm_type) {
  case SMT_PACKET:
    htsp_stream_deliver(hs, sm->sm_data); // reference is transfered
    sm->sm_data = NULL;
    break;

  case SMT_START:
    htsp_subscription_start(hs, sm->sm_data);
    break;

  case SMT_STOP:
    htsp_subscription_stop(hs, streaming_code2txt(sm->sm_code));
    break;

  case SMT_SERVICE_STATUS:
    htsp_subscription_service_status(hs, sm->sm_code);
    break;

  case SMT_NOSTART:
    htsp_subscription_status(hs,  streaming_code2txt(sm->sm_code));
    break;

  case SMT_MPEGTS:
    break;

  case SMT_EXIT:
    abort();
  }
  streaming_msg_free(sm);
}
