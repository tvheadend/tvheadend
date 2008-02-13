/*
 *  tvheadend, RPC 
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
#include <libhts/htsmsg.h>

#include "tvhead.h"
#include "channels.h"
#include "subscriptions.h"
#include "dispatch.h"
#include "epg.h"
#include "pvr.h"
#include "rpc.h"


/** 
 * Build a channel information message
 */
static htsmsg_t *
rpc_build_channel_msg(th_channel_t *ch)
{
  htsmsg_t *m;

  m = htsmsg_create();

  htsmsg_add_str(m, "name", ch->ch_name);
  htsmsg_add_u32(m, "tag", ch->ch_tag);
  if(ch->ch_icon)
    htsmsg_add_str(m, "icon", refstr_get(ch->ch_icon));
  return m;
}







/** 
 * Simple function to respond 'ok' 
 */
htsmsg_t *
rpc_ok(rpc_session_t *ses)
{
  htsmsg_t *r = htsmsg_create();
  htsmsg_add_u32(r, "seq", ses->rs_seq);
  return r;
}



/** 
 * Simple function to respond with an error
 */
htsmsg_t *
rpc_error(rpc_session_t *ses, const char *err)
{
  htsmsg_t *r = htsmsg_create();
  htsmsg_add_u32(r, "seq", ses->rs_seq);
  htsmsg_add_str(r, "_error", err);
  return r;
}



/**
 * Login peer, set auth level and do various other stuff
 */
static htsmsg_t *
rpc_login(rpc_session_t *ses, htsmsg_t *in, void *opaque)
{
  const char *user = htsmsg_get_str(in, "username");
  const char *pass = htsmsg_get_str(in, "password");

  if(user == NULL || pass == NULL) {
    ses->rs_authlevel = 1;
    ses->rs_maxweight = 100;
  } else {
    /* FIXME: user/password database */
    ses->rs_authlevel = 1;
  }

  if(htsmsg_get_u32(in, "async", &ses->rs_is_async) < 0)
    ses->rs_is_async = 0;

  return rpc_ok(ses);
}







/*
 * channelsList method
 */
static htsmsg_t *
rpc_channels_list(rpc_session_t *ses, htsmsg_t *in, void *opaque)
{
  htsmsg_t *out;
  th_channel_t *ch;

  out = htsmsg_create();
  htsmsg_add_u32(out, "seq", ses->rs_seq);

  LIST_FOREACH(ch, &channels, ch_global_link)
    htsmsg_add_msg(out, "channel", rpc_build_channel_msg(ch));

  return out;
}

/*
 * eventInfo method
 */
static htsmsg_t *
rpc_event_info(rpc_session_t *ses, htsmsg_t *in, void *opaque)
{
  htsmsg_t *out;
  th_channel_t *ch;
  uint32_t u32;
  const char *s, *errtxt = NULL;
  event_t *e = NULL, *x;
  uint32_t tag, prev, next;
  pvr_rec_t *pvrr;

  out = htsmsg_create();
  htsmsg_add_u32(out, "seq", ses->rs_seq);

  epg_lock();

  if(htsmsg_get_u32(in, "tag", &u32) >= 0) {
    e = epg_event_find_by_tag(u32);
  } else if((s = htsmsg_get_str(in, "channel")) != NULL) {
    if((ch = channel_find(s, 0, NULL)) == NULL) {
      errtxt = "Channel not found";
    } else {
      if(htsmsg_get_u32(in, "time", &u32) < 0) {
	e = epg_event_get_current(ch);
      } else {
	e = epg_event_find_by_time(ch, u32);
      }
    }
  } else {
    errtxt = "Invalid arguments";
  }
  
  if(e == NULL) {
    errtxt = "Event not found";
  }

  if(errtxt != NULL) {
    htsmsg_add_str(out, "_error", errtxt);
  } else {
    tag = e->e_tag;
    x = TAILQ_PREV(e, event_queue, e_link);
    prev = x != NULL ? x->e_tag : 0;
    
    x = TAILQ_NEXT(e, e_link);
    next = x != NULL ? x->e_tag : 0;

    htsmsg_add_u32(out, "start", e->e_start);
    htsmsg_add_u32(out, "stop",  e->e_start + e->e_duration);

    if(e->e_title != NULL)
      htsmsg_add_str(out, "title", e->e_title);

    if(e->e_desc != NULL)
      htsmsg_add_str(out, "desc", e->e_desc);
	
    htsmsg_add_u32(out, "tag", tag);
    if(prev)
      htsmsg_add_u32(out, "prev", prev);
    if(next)
      htsmsg_add_u32(out, "next", next);

    if((pvrr = pvr_get_by_entry(e)) != NULL)
      htsmsg_add_u32(out, "pvrstatus", pvrr->pvrr_status);
  }

  epg_unlock();

  return out;
}

#if 0


/*
 * record method
 */
static void
htsp_record(htsp_connection_t *hc, htsp_msg_t *m)
{
  htsp_msg_t *r;
  uint32_t u32;
  const char *s, *errtxt = NULL;
  event_t *e = NULL;
  recop_t op;

  r = htsp_create_msg();
  htsp_add_u32(r, "seq", hc->hc_seq);

  epg_lock();

  s = htsp_get_string(m, "command");
  if(s == NULL) {
    errtxt = "Missing 'command' field";
  } else {
    op = pvr_op2int(s);
    if(op == -1) {
      errtxt = "Invalid 'command' field";
    } else {

      if((u32 = htsp_get_u32(m, "tag")) != 0) {
	e = epg_event_find_by_tag(u32);
      
	if(e != NULL) {
	  pvr_event_record_op(e->e_ch, e, op);
	  errtxt = NULL;
	} else {
	  errtxt = "Event not found";
	}
      } else {
	errtxt = "Missing identifier";
      }
    }
  }

  epg_unlock();
  
  if(errtxt)
    htsp_add_string(r, "_error", errtxt);

  htsp_send_msg(hc, NULL, r);
  htsp_free_msg(r);
}
#endif

#if 0

/*
 * subscribe
 */
static void
htsp_subscribe(htsp_connection_t *hc, htsp_msg_t *m)
{
  htsp_msg_t *r;
  const char *str, *errtxt = NULL;
  th_channel_t *ch;
  int weight;

  r = htsp_create_msg();
  htsp_add_u32(r, "seq", hc->hc_seq);

  if((str = htsp_get_string(m, "channel")) != NULL) {
    if((ch = channel_find(str, 0)) != NULL) {

      weight = htsp_get_u32(m, "weight") ?: 100;
      if(weight > hc->hc_maxweight)
	weight = hc->hc_maxweight;

      htsp_stream_subscribe(hc, ch, weight);
    } else {
      errtxt = "Channel not found";
    }
  } else {
    errtxt = "Missing 'channel' field";
  }

  htsp_send_msg(hc, NULL, r);
  htsp_free_msg(r);
}

/*
 * unsubscribe
 */
static void
htsp_subscribe(htsp_connection_t *hc, htsp_msg_t *m)
{
  htsp_msg_t *r;
  const char *str, *errtxt = NULL;
  th_channel_t *ch;

  r = htsp_create_msg();
  htsp_add_u32(r, "seq", hc->hc_seq);

  if((str = htsp_get_string(m, "channel")) != NULL) {
    if((ch = channel_find(str, 0)) != NULL) {
      htsp_stream_unsubscribe(hc, ch);
    } else {
      errtxt = "Channel not found";
    }
  } else {
    errtxt = "Missing 'channel' field";
  }

  htsp_send_msg(hc, NULL, r);
  htsp_free_msg(r);
}
#endif

/*
 * List of all methods and their required auth level
 */

static rpc_cmd_t common_rpc[] = {
  { "login",        rpc_login,         0 },
  { "channelsList", rpc_channels_list, 1 },
  { "eventInfo",    rpc_event_info,    1 },
#if 0
  { "subscribe",    rpc_subscribe,     1 },
  { "unsubscribe",  rpc_unsubscribe,   1 },
#endif
  //  { "record",       rpc_record,        2 },
  { NULL,           NULL,              0 },
};


/*
 *
 */
static htsmsg_t *
rpc_dispatch_set(rpc_session_t *ses, htsmsg_t *in, rpc_cmd_t *cmds,
		 void *opaque, const char *method)
{
  for(; cmds->rc_name; cmds++) {
    if(strcmp(cmds->rc_name, method))
      continue;
    if(cmds->rc_authlevel < ses->rs_authlevel)
      return rpc_error(ses, "Insufficient privileges");
    return cmds->rc_func(ses, in, opaque);
  }
  return NULL;
}


/*
 *
 */
htsmsg_t *
rpc_dispatch(rpc_session_t *ses, htsmsg_t *in, rpc_cmd_t *cmds, void *opaque)
{
  const char *method;
  htsmsg_t *out;

  if(htsmsg_get_u32(in, "seq", &ses->rs_seq))
    ses->rs_seq = 0;

  if((method = htsmsg_get_str(in, "method")) == NULL)
    return rpc_error(ses, "Method not specified");

  out = rpc_dispatch_set(ses, in, common_rpc, opaque, method);
  if(out == NULL && cmds != NULL) 
    out = rpc_dispatch_set(ses, in, cmds, opaque, method);

  return out ?: rpc_error(ses, "Method not found");
}


/*
 *
 */
void
rpc_init(rpc_session_t *ses, const char *logname)
{
  memset(ses, 0, sizeof(rpc_session_t));
  ses->rs_logname = strdup(logname);
}

/*
 *
 */
void
rpc_deinit(rpc_session_t *ses)
{
  free((void *)ses->rs_logname);
}
