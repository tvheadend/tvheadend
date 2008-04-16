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

#include "tvhead.h"
#include "dispatch.h"
#include "http.h"
#include "ajaxui.h"

#define MAILBOX_UNUSED_TIMEOUT      15
#define MAILBOX_EMPTY_REPLY_TIMEOUT 10


static LIST_HEAD(, ajaxui_mailbox) mailboxes;

int mailbox_tally;

TAILQ_HEAD(ajaxui_letter_queue, ajaxui_letter);

typedef struct ajaxui_letter {
  TAILQ_ENTRY(ajaxui_letter) al_link;
  char *al_payload;
} ajaxui_letter_t;


typedef struct ajaxui_mailbox {
  LIST_ENTRY(ajaxui_mailbox) amb_link;

  uint32_t amb_boxid;
  char *amb_subscriptionid;

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
  free(al);
}


/**
 *
 */
static void
amb_destroy(ajaxui_mailbox_t *amb)
{
  ajaxui_letter_t *al;

  while((al = TAILQ_FIRST(&amb->amb_letters)) != NULL)
    al_destroy(amb, al);

  LIST_REMOVE(amb, amb_link);

  dtimer_disarm(&amb->amb_timer);

  free(amb->amb_subscriptionid);
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
int
ajax_mailbox_create(char *subscriptionid)
{
  ajaxui_mailbox_t *amb = calloc(1, sizeof(ajaxui_mailbox_t));

  mailbox_tally++;

  amb->amb_boxid = mailbox_tally;
  amb->amb_subscriptionid = strdup(subscriptionid);
  
  LIST_INSERT_HEAD(&mailboxes, amb, amb_link);

  TAILQ_INIT(&amb->amb_letters);

  dtimer_arm(&amb->amb_timer, ajax_mailbox_unused, amb,
	     MAILBOX_UNUSED_TIMEOUT);
  return amb->amb_boxid;
}


/**
 *
 */
static void
ajax_mailbox_reply(ajaxui_mailbox_t *amb, http_reply_t *hr)
{
  ajaxui_letter_t *al;

  while((al = TAILQ_FIRST(&amb->amb_letters)) != NULL) {
    tcp_qprintf(&hr->hr_tq, "%s", al->al_payload);
    al_destroy(amb, al);
  }

  tcp_qprintf(&hr->hr_tq, "new Ajax.Request('/ajax/mailbox/%d');\r\n",
	      amb->amb_boxid);

  http_output(hr->hr_connection, hr, "text/javascript", NULL, 0);
  amb->amb_hr = NULL;

  dtimer_arm(&amb->amb_timer, ajax_mailbox_unused, amb,
	     MAILBOX_UNUSED_TIMEOUT);
}


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
  uint32_t boxid;
  ajaxui_mailbox_t *amb;

  if(remain == NULL)
    return HTTP_STATUS_NOT_FOUND;

  boxid = atoi(remain);
  LIST_FOREACH(amb, &mailboxes, amb_link)
    if(amb->amb_boxid == boxid)
      break;
  if(amb == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if(amb->amb_hr != NULL)
    return 409;

  if(TAILQ_FIRST(&amb->amb_letters) != NULL) {
    /* Pending letters, direct reply */
    ajax_mailbox_reply(amb, hr);
    return 0;
  }

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
  http_path_add("/ajax/mailbox",  NULL, ajax_mailbox_poll);
}


/**
 *
 */
static void
ajax_mailbox_add_to_subscription(const char *subscription, const char *content)
{
  ajaxui_mailbox_t *amb;
  ajaxui_letter_t *al;
  http_connection_t *hc;

  LIST_FOREACH(amb, &mailboxes, amb_link) {
    if(strcmp(subscription, amb->amb_subscriptionid))
      continue;

    al = malloc(sizeof(ajaxui_letter_t));
    al->al_payload = strdup(content);
    TAILQ_INSERT_TAIL(&amb->amb_letters, al, al_link);

    if(amb->amb_hr != NULL) {
      hc = amb->amb_hr->hr_connection;
      ajax_mailbox_reply(amb, amb->amb_hr);
      http_continue(hc);
    }
  }
}



/**
 *
 */
static void
ajax_mailbox_update_div(const char *subscription, const char *prefix,
			const char *postfix, const char *content)
{
  char buf[1000];

  snprintf(buf, sizeof(buf),
	   "document.getElementById('%s_%s').innerHTML='%s';\r\n",
	   prefix, postfix, content);

  ajax_mailbox_add_to_subscription(subscription, buf);
}



void
ajax_mailbox_tdmi_state_change(th_dvb_mux_instance_t *tdmi)
{
  const char *txt;

  switch(tdmi->tdmi_state) {
  case TDMI_IDLE:      txt = "Idle";      break;
  case TDMI_IDLESCAN:  txt = "Scanning";  break;
  case TDMI_RUNNING:   txt = "Running";   break;
  default:             txt = "???";       break;
  }
  
  
  ajax_mailbox_update_div(tdmi->tdmi_adapter->tda_identifier,
			  "state", tdmi->tdmi_identifier,
			  txt);
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
