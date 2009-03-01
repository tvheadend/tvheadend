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
#include "http.h"
#include "webui/webui.h"
#include "access.h"

static pthread_mutex_t comet_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t comet_cond = PTHREAD_COND_INITIALIZER;

#define MAILBOX_UNUSED_TIMEOUT      20
#define MAILBOX_EMPTY_REPLY_TIMEOUT 10

//#define mbdebug(fmt...) printf(fmt);
#define mbdebug(fmt...)


static LIST_HEAD(, comet_mailbox) mailboxes;

int mailbox_tally;

typedef struct comet_mailbox {
  char *cmb_boxid; /* an md5hash */
  htsmsg_t *cmb_messages; /* A vector */
  time_t cmb_last_used;
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

  free(cmb->cmb_boxid);
  free(cmb);
}


/**
 *
 */
void
comet_flush(void)
{
  comet_mailbox_t *cmb, *next;

  pthread_mutex_lock(&comet_mutex);

  for(cmb = LIST_FIRST(&mailboxes); cmb != NULL; cmb = next) {
    next = LIST_NEXT(cmb, cmb_link);

    if(cmb->cmb_last_used && cmb->cmb_last_used + 60 < dispatch_clock)
      cmb_destroy(cmb);
  }
  pthread_mutex_unlock(&comet_mutex);
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
  time(&cmb->cmb_last_used);
  mailbox_tally++;

  LIST_INSERT_HEAD(&mailboxes, cmb, cmb_link);
  return cmb;
}

/**
 *
 */
static void
comet_access_update(http_connection_t *hc, comet_mailbox_t *cmb)
{
  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_str(m, "notificationClass", "accessUpdate");

  htsmsg_add_u32(m, "dvr",   !http_access_verify(hc, ACCESS_RECORDER));
  htsmsg_add_u32(m, "admin", !http_access_verify(hc, ACCESS_ADMIN));

  if(cmb->cmb_messages == NULL)
    cmb->cmb_messages = htsmsg_create_list();
  htsmsg_add_msg(cmb->cmb_messages, NULL, m);
}

/**
 * Poll callback
 */
static int
comet_mailbox_poll(http_connection_t *hc, const char *remain, void *opaque)
{
  comet_mailbox_t *cmb = NULL; 
  const char *cometid = http_arg_get(&hc->hc_req_args, "boxid");
  time_t reqtime;
  struct timespec ts;
  htsmsg_t *m;

  usleep(100000); /* Always sleep 0.1 sec to avoid comet storms */

  pthread_mutex_lock(&comet_mutex);

  if(cometid != NULL)
    LIST_FOREACH(cmb, &mailboxes, cmb_link)
      if(!strcmp(cmb->cmb_boxid, cometid))
	break;
    
  if(cmb == NULL) {
    cmb = comet_mailbox_create();
    comet_access_update(hc, cmb);
  }
  time(&reqtime);

  ts.tv_sec = reqtime + 10;
  ts.tv_nsec = 0;

  cmb->cmb_last_used = 0; /* Make sure we're not flushed out */

  if(cmb->cmb_messages == NULL)
    pthread_cond_timedwait(&comet_cond, &comet_mutex, &ts);

  m = htsmsg_create_map();
  htsmsg_add_str(m, "boxid", cmb->cmb_boxid);
  htsmsg_add_msg(m, "messages", cmb->cmb_messages ?: htsmsg_create_list());
  cmb->cmb_messages = NULL;
  
  cmb->cmb_last_used = dispatch_clock;

  pthread_mutex_unlock(&comet_mutex);

  htsmsg_json_serialize(m, &hc->hc_reply, 0);
  htsmsg_destroy(m);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}


/**
 *
 */
void
comet_init(void)
{
  http_path_add("/comet",  NULL, comet_mailbox_poll, ACCESS_WEB_INTERFACE);
}


/**
 *
 */
void
comet_mailbox_add_message(htsmsg_t *m)
{
  comet_mailbox_t *cmb;

  pthread_mutex_lock(&comet_mutex);

  LIST_FOREACH(cmb, &mailboxes, cmb_link) {

    if(cmb->cmb_messages == NULL)
      cmb->cmb_messages = htsmsg_create_list();
    htsmsg_add_msg(cmb->cmb_messages, NULL, htsmsg_copy(m));
  }

  pthread_cond_broadcast(&comet_cond);
  pthread_mutex_unlock(&comet_mutex);
}
