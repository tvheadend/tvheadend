/*
 *  tvheadend, COMET
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


#include <libhts/htsmsg.h>
#include <libhts/htsmsg_json.h>

#include "tvhead.h"
#include "dispatch.h"
#include "http.h"
#include "webui/webui.h"
#include "access.h"

#define MAILBOX_UNUSED_TIMEOUT      20
#define MAILBOX_EMPTY_REPLY_TIMEOUT 10

//#define mbdebug(fmt...) printf(fmt);
#define mbdebug(fmt...)


static LIST_HEAD(, comet_mailbox) mailboxes;

int mailbox_tally;

typedef struct comet_mailbox {

  char *cmb_boxid; /* an md5hash */
  dtimer_t cmb_timer;
  http_reply_t *cmb_hr; /* Pending request */

  htsmsg_t *cmb_messages; /* A vector */

  LIST_ENTRY(comet_mailbox) cmb_link;

} comet_mailbox_t;


/**
 *
 */
static void
cmb_destroy(comet_mailbox_t *cmb)
{
  mbdebug("mailbox[%s]: destroyed\n", cmb->cmb_boxid);

  if(cmb->cmb_messages != NULL)
    htsmsg_destroy(cmb->cmb_messages);

  LIST_REMOVE(cmb, cmb_link);

  dtimer_disarm(&cmb->cmb_timer);

  free(cmb->cmb_boxid);
  free(cmb);
}



/**
 *
 */
static void
comet_mailbox_unused(void *opaque, int64_t now)
{
  comet_mailbox_t *cmb = opaque;
  assert(cmb->cmb_hr == NULL);
  cmb_destroy(cmb);
}

/**
 *
 */
static void
comet_mailbox_connection_lost(http_reply_t *hr, void *opaque)
{
  comet_mailbox_t *cmb = opaque;
  assert(hr == cmb->cmb_hr);
  cmb_destroy(cmb);
}



/**
 *
 */
static comet_mailbox_t *
comet_mailbox_create(void)
{
  comet_mailbox_t *cmb = calloc(1, sizeof(comet_mailbox_t));

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

  cmb->cmb_boxid = strdup(id);

  mailbox_tally++;

  LIST_INSERT_HEAD(&mailboxes, cmb, cmb_link);

  dtimer_arm(&cmb->cmb_timer, comet_mailbox_unused, cmb,
	     MAILBOX_UNUSED_TIMEOUT);
  return cmb;
}

/**
 *
 */
static void
comet_mailbox_reply(comet_mailbox_t *cmb, http_reply_t *hr)
{
  htsmsg_t *m = htsmsg_create();

  htsmsg_add_str(m, "boxid", cmb->cmb_boxid);

  htsmsg_add_msg(m, "messages", cmb->cmb_messages ?: htsmsg_create_array());
  cmb->cmb_messages = NULL;

  htsmsg_json_serialize(m, &hr->hr_q, 0);

  htsmsg_destroy(m);

  http_output(hr->hr_connection, hr, "text/x-json; charset=UTF-8", NULL, 0);
  cmb->cmb_hr = NULL;

  /* Arm a timer in case the browser won't call back */
  dtimer_arm(&cmb->cmb_timer, comet_mailbox_unused, cmb,
	     MAILBOX_UNUSED_TIMEOUT);
}

/**
 *
 */
static void
comet_mailbox_empty_reply(void *opaque, int64_t now)
{
  comet_mailbox_t *cmb = opaque;
  http_reply_t *hr = cmb->cmb_hr;

  comet_mailbox_reply(cmb, hr);
  http_continue(hr->hr_connection);
}


/**
 * Poll callback
 *
 * Prepare the mailbox for reply
 */
static int
comet_mailbox_poll(http_connection_t *hc, http_reply_t *hr, 
		   const char *remain, void *opaque)
{
  comet_mailbox_t *cmb = NULL; 
  const char *cometid = http_arg_get(&hc->hc_req_args, "boxid");

  if(cometid != NULL)
    LIST_FOREACH(cmb, &mailboxes, cmb_link)
      if(!strcmp(cmb->cmb_boxid, cometid))
	break;
    
  if(cmb == NULL)
    cmb = comet_mailbox_create();

  if(cmb->cmb_hr != NULL) {
    mbdebug("mailbox already processing\n");
    return 409;
  }

  if(cmb->cmb_messages != NULL) {
    /* Pending letters, direct reply */
    mbdebug("direct reply\n");
    comet_mailbox_reply(cmb, hr);
    return 0;
  }

  mbdebug("nothing in queue, waiting\n");

  cmb->cmb_hr = hr;

  hr->hr_opaque = cmb;
  hr->hr_destroy = comet_mailbox_connection_lost;

  dtimer_arm(&cmb->cmb_timer, comet_mailbox_empty_reply, cmb,
	     MAILBOX_EMPTY_REPLY_TIMEOUT);
  return 0;
}


/**
 *
 */
#if 0
static dtimer_t comet_clock;

static void
comet_ticktack(void *opaque, int64_t now)
{
  char buf[64];
  htsmsg_t *x;

  snprintf(buf, sizeof(buf), "The clock is now %lluµs past 1970",
	   now);

  x = htsmsg_create();
  htsmsg_add_str(x, "type", "logmessage");
  htsmsg_add_str(x, "logtxt", buf);
  
  comet_mailbox_add_message(x);

  htsmsg_destroy(x);
  dtimer_arm(&comet_clock, comet_ticktack, NULL, 1);
}
#endif

/**
 *
 */
void
comet_init(void)
{
  http_path_add("/comet",  NULL, comet_mailbox_poll, ACCESS_WEB_INTERFACE);

  //  dtimer_arm(&comet_clock, comet_ticktack, NULL, 1);
}


/**
 * Delayed delivery of mailbox replies
 */
static void
comet_mailbox_deliver(void *opaque, int64_t now)
{
  comet_mailbox_t *cmb = opaque;
  http_connection_t *hc;

  hc = cmb->cmb_hr->hr_connection;
  comet_mailbox_reply(cmb, cmb->cmb_hr);
  http_continue(hc);
}

/**
 *
 */
void
comet_mailbox_add_message(htsmsg_t *m)
{
  comet_mailbox_t *cmb;

  LIST_FOREACH(cmb, &mailboxes, cmb_link) {

    if(cmb->cmb_messages == NULL)
      cmb->cmb_messages = htsmsg_create_array();
    htsmsg_add_msg(cmb->cmb_messages, NULL, htsmsg_copy(m));

    if(cmb->cmb_hr != NULL)
      dtimer_arm_hires(&cmb->cmb_timer, comet_mailbox_deliver, cmb, 
		       getclock_hires() + 100000);
  }
}
