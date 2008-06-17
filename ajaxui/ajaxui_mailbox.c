/*
 *  tvheadend, AJAX / HTML Mailboxes
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

#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/md5.h>

#include "tvhead.h"
#include "dispatch.h"
#include "http.h"
#include "ajaxui.h"
#include "transports.h"
#include "epg_xmltv.h"
#include "dvb_support.h"
#include "ajaxui_mailbox.h"
#include "cwc.h"

#define MAILBOX_UNUSED_TIMEOUT      15
#define MAILBOX_EMPTY_REPLY_TIMEOUT 10


//#define mbdebug(fmt...) printf(fmt);
#define mbdebug(fmt...)


static LIST_HEAD(, ajaxui_mailbox) mailboxes;

int mailbox_tally;

TAILQ_HEAD(ajaxui_letter_queue, ajaxui_letter);

typedef struct ajaxui_letter {
  TAILQ_ENTRY(ajaxui_letter) al_link;
  char *al_payload_a;
  char *al_payload_b;
} ajaxui_letter_t;


typedef struct ajaxui_mailbox {
  LIST_ENTRY(ajaxui_mailbox) amb_link;

  char *amb_boxid; /* an md5hash */

  dtimer_t amb_timer;

  http_reply_t *amb_hr; /* Pending request */

  struct ajaxui_letter_queue amb_letters;

} ajaxui_mailbox_t;


/**
 *
 */
static void
al_destroy(ajaxui_mailbox_t *amb, ajaxui_letter_t *al)
{
  TAILQ_REMOVE(&amb->amb_letters, al, al_link);
  free(al->al_payload_a);
  free(al->al_payload_b);
  free(al);
}


/**
 *
 */
static void
amb_destroy(ajaxui_mailbox_t *amb)
{
  ajaxui_letter_t *al;

  mbdebug("mailbox[%s]: destroyed\n", amb->amb_boxid);

  while((al = TAILQ_FIRST(&amb->amb_letters)) != NULL)
    al_destroy(amb, al);

  LIST_REMOVE(amb, amb_link);

  dtimer_disarm(&amb->amb_timer);

  free(amb->amb_boxid);
  free(amb);
}



/**
 *
 */
static void
ajax_mailbox_unused(void *opaque, int64_t now)
{
  ajaxui_mailbox_t *amb = opaque;
  assert(amb->amb_hr == NULL);
  amb_destroy(amb);
}

/**
 *
 */
static void
ajax_mailbox_connection_lost(http_reply_t *hr, void *opaque)
{
  ajaxui_mailbox_t *amb = opaque;
  assert(hr == amb->amb_hr);
  amb_destroy(amb);
}



/**
 *
 */
static ajaxui_mailbox_t *
ajax_mailbox_create(const char *id)
{
  ajaxui_mailbox_t *amb = calloc(1, sizeof(ajaxui_mailbox_t));

  amb->amb_boxid = strdup(id);

  mailbox_tally++;

  LIST_INSERT_HEAD(&mailboxes, amb, amb_link);

  TAILQ_INIT(&amb->amb_letters);

  dtimer_arm(&amb->amb_timer, ajax_mailbox_unused, amb,
	     MAILBOX_UNUSED_TIMEOUT);
  return amb;
}

/**
 *
 */
void
ajax_mailbox_start(tcp_queue_t *tq)
{
  struct timeval tv;
  uint8_t sum[16];
  char id[33];
  int i;
  struct AVMD5 *ctx;

  ctx = alloca(av_md5_size);

  gettimeofday(&tv, NULL);

  av_md5_init(ctx);
  av_md5_update(ctx, (void *)&tv, sizeof(tv));
  av_md5_update(ctx, (void *)&mailbox_tally, sizeof(uint32_t));
  av_md5_final(ctx, sum);

  for(i = 0; i < 16; i++) {
    id[i * 2 + 0] = "0123456789abcdef"[sum[i] >> 4];
    id[i * 2 + 1] = "0123456789abcdef"[sum[i] & 15];
  }
  id[32] = 0;

  mbdebug("Generated mailbox %s\n", id);

  ajax_mailbox_create(id);
  ajax_js(tq, "mailboxquery('%s')", id);
}


/**
 *
 */
static void
ajax_mailbox_reply(ajaxui_mailbox_t *amb, http_reply_t *hr)
{
  ajaxui_letter_t *al;

  /* Modify the hidden element (as described in ajax_mailbox_start()), 
     if this div no longer exist, the rest of the javascript will bail
     out and we will not be reloaded */

  mbdebug("mailbox[%s]: sending reply\n", amb->amb_boxid);

  while((al = TAILQ_FIRST(&amb->amb_letters)) != NULL) {
    tcp_qprintf(&hr->hr_tq, "try {\r\n");
    tcp_qprintf(&hr->hr_tq, "%s%s", al->al_payload_a, al->al_payload_b ?: "");
    mbdebug("\t%s%s", al->al_payload_a, al->al_payload_b ?: "");

    tcp_qprintf(&hr->hr_tq, "}\r\n"
		"catch(err) {}\r\n");
    al_destroy(amb, al);
  }

  tcp_qprintf(&hr->hr_tq, "mailboxquery('%s');\r\n", amb->amb_boxid);

  http_output(hr->hr_connection, hr, "text/javascript", NULL, 0);
  amb->amb_hr = NULL;

  /* Arm a timer in case the browser won't call back */
  dtimer_arm(&amb->amb_timer, ajax_mailbox_unused, amb,
	     MAILBOX_UNUSED_TIMEOUT);
}

/**
 *
 */
static void
ajax_mailbox_empty_reply(void *opaque, int64_t now)
{
  ajaxui_mailbox_t *amb = opaque;
  http_reply_t *hr = amb->amb_hr;

  ajax_mailbox_reply(amb, hr);
  http_continue(hr->hr_connection);
}


/**
 * Poll callback
 *
 * Prepare the mailbox for reply
 */
static int
ajax_mailbox_poll(http_connection_t *hc, http_reply_t *hr, 
		  const char *remain, void *opaque)
{
  ajaxui_mailbox_t *amb;

  if(remain == NULL)
    return HTTP_STATUS_NOT_FOUND;

  mbdebug("mailbox[%s]: Incomming request ... ", remain);

  LIST_FOREACH(amb, &mailboxes, amb_link)
    if(!strcmp(amb->amb_boxid, remain))
      break;

  if(amb == NULL) {
    amb = ajax_mailbox_create(remain);
    mbdebug("creating mailbox ... ");
  }

  if(amb->amb_hr != NULL) {
    mbdebug("mailbox already processing\n");
    return 409;
  }
  if(TAILQ_FIRST(&amb->amb_letters) != NULL) {
    /* Pending letters, direct reply */
    mbdebug("direct reply\n");
    ajax_mailbox_reply(amb, hr);
    return 0;
  }

  mbdebug("nothing in queue, waiting\n");

  amb->amb_hr = hr;

  hr->hr_opaque = amb;
  hr->hr_destroy = ajax_mailbox_connection_lost;

  dtimer_arm(&amb->amb_timer, ajax_mailbox_empty_reply, amb,
	     MAILBOX_EMPTY_REPLY_TIMEOUT);
  return 0;
}


/**
 *
 */
void
ajax_mailbox_init(void)
{
  http_path_add("/ajax/mailbox",  NULL, ajax_mailbox_poll,
		ACCESS_WEB_INTERFACE);
}


/**
 * Delayed delivery of mailbox replies
 */
static void
ajax_mailbox_deliver(void *opaque, int64_t now)
{
  ajaxui_mailbox_t *amb = opaque;
  http_connection_t *hc;

  hc = amb->amb_hr->hr_connection;
  ajax_mailbox_reply(amb, amb->amb_hr);
  http_continue(hc);
}

/**
 *
 */
static void
ajax_mailbox_add_to_subscription(const char *subscription, 
				 const char *content_a, const char *content_b)
{
  ajaxui_mailbox_t *amb;
  ajaxui_letter_t *al;

  LIST_FOREACH(amb, &mailboxes, amb_link) {

    /* Avoid inserting the same message twice */

    TAILQ_FOREACH(al, &amb->amb_letters, al_link) {
      if(!strcmp(al->al_payload_a, content_a))
	break;
    }

    if(al == NULL) {

      al = malloc(sizeof(ajaxui_letter_t));
      al->al_payload_a = strdup(content_a);
    } else {
      /* Already exist, just move to tail */

      TAILQ_REMOVE(&amb->amb_letters, al, al_link);
      free(al->al_payload_b);
    }

    al->al_payload_b = content_b ? strdup(content_b) : NULL;

    TAILQ_INSERT_TAIL(&amb->amb_letters, al, al_link);

    if(amb->amb_hr != NULL)
      dtimer_arm_hires(&amb->amb_timer, ajax_mailbox_deliver, amb, 
		       getclock_hires() + 100000);
  }
}



/**
 *
 */
static void
ajax_mailbox_update_div(const char *subscription, const char *prefix,
			const char *postfix, const char *content)
{
  char buf_a[500];
  char buf_b[500];

  content = ajaxui_escape_apostrophe(content);

  snprintf(buf_a, sizeof(buf_a), "$('%s_%s').innerHTML=", prefix, postfix);
  snprintf(buf_b, sizeof(buf_b), "'%s';\n\r", content);

  ajax_mailbox_add_to_subscription(subscription, buf_a, buf_b);
}



static void
ajax_mailbox_reload_div(const char *subscription, const char *prefix,
			const char *postfix, const char *url)
{
  char buf[1000];

  snprintf(buf, sizeof(buf), "new Ajax.Updater('%s_%s', '%s', "
	   "{method: 'get', evalScripts: true});\r\n",
	   prefix, postfix, url);

  ajax_mailbox_add_to_subscription(subscription, buf, NULL);
}



void
ajax_mailbox_tdmi_state_change(th_dvb_mux_instance_t *tdmi)
{
  ajax_mailbox_update_div(tdmi->tdmi_adapter->tda_identifier,
			  "state", tdmi->tdmi_identifier,
			  dvb_mux_state(tdmi));
}

void
ajax_mailbox_tdmi_qual_change(th_dvb_mux_instance_t *tdmi)
{
  char buf[10];
  snprintf(buf, sizeof(buf), "%d%%",
	   dvb_quality_to_percent(tdmi->tdmi_quality));
  ajax_mailbox_update_div(tdmi->tdmi_adapter->tda_identifier,
			  "qual", tdmi->tdmi_identifier,
			  buf);
}


void
ajax_mailbox_tdmi_name_change(th_dvb_mux_instance_t *tdmi)
{
  ajax_mailbox_update_div(tdmi->tdmi_adapter->tda_identifier,
			  "name", tdmi->tdmi_identifier,
			  tdmi->tdmi_network ?: "<noname>");
}


void
ajax_mailbox_tdmi_status_change(th_dvb_mux_instance_t *tdmi)
{
  ajax_mailbox_update_div(tdmi->tdmi_adapter->tda_identifier,
			  "status", tdmi->tdmi_identifier,
			  tdmi->tdmi_last_status);
}

void
ajax_mailbox_tdmi_services_change(th_dvb_mux_instance_t *tdmi)
{
  th_transport_t *t;
  char buf[20];
  int n, m;

  n = m = 0;
  LIST_FOREACH(t, &tdmi->tdmi_transports, tht_mux_link) {
    n++;
    if(transport_is_available(t))
      m++;
  }
  snprintf(buf, sizeof(buf), "%d / %d", m, n);
   
  ajax_mailbox_update_div(tdmi->tdmi_adapter->tda_identifier,
			  "nsvc", tdmi->tdmi_identifier,
			  buf);
}


void
ajax_mailbox_tda_change(th_dvb_adapter_t *tda)
{
  char buf[500];

  snprintf(buf, sizeof(buf), "/ajax/dvbadaptermuxlist/%s",
	   tda->tda_identifier);

  ajax_mailbox_reload_div(tda->tda_identifier,
			  "dvbmuxlist", tda->tda_identifier,
			  buf);
}


void
ajax_mailbox_xmltv_grabber_status_change(xmltv_grabber_t *xg)
{
  char buf[500];

  ajax_mailbox_update_div("xmltvgrabbers",
			  "status", xg->xg_identifier,
			  xmltv_grabber_status(xg));


  if(xg->xg_status == XMLTV_GRAB_OK) {
    snprintf(buf, sizeof(buf), "/ajax/xmltvgrabberlist/%s", xg->xg_identifier);
    ajax_mailbox_reload_div("xmltvgrabbers",
			    "details", xg->xg_identifier,
			    buf);
  } else {
    ajax_mailbox_update_div(xg->xg_identifier,
			    "details", xg->xg_identifier,
			    xmltv_grabber_status_long(xg));
  }
}



void
ajax_mailbox_transport_status_change(struct th_transport *t)
{
  ajax_mailbox_update_div("xmltvgrabbers",
			  "status", t->tht_identifier,
			  transport_status_to_text(t->tht_last_status));
}




void
ajax_mailbox_cwc_status_change(struct cwc *cwc)
{
  char id[20];
  snprintf(id, sizeof(id), "cwc_%d", cwc->cwc_id);

  ajax_mailbox_update_div("cwc", "status", id, cwc_status_to_text(cwc));
}
