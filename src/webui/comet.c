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
#include <arpa/inet.h>
#include <sys/socket.h>
#include <openssl/sha.h>

#include "htsmsg.h"
#include "htsmsg_json.h"

#include "tvheadend.h"
#include "config.h"
#include "http.h"
#include "dvr/dvr.h"
#include "webui/webui.h"
#include "access.h"
#include "notify.h"
#include "tcp.h"

static pthread_mutex_t comet_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t comet_cond = PTHREAD_COND_INITIALIZER;

#define MAILBOX_UNUSED_TIMEOUT      20
#define MAILBOX_EMPTY_REPLY_TIMEOUT 10

//#define mbdebug(fmt...) printf(fmt);
#define mbdebug(fmt...)


static LIST_HEAD(, comet_mailbox) mailboxes;

int mailbox_tally;
int comet_running;

typedef struct comet_mailbox {
  char *cmb_boxid; /* SHA-1 hash */
  char *cmb_lang;  /* UI language */
  htsmsg_t *cmb_messages; /* A vector */
  time_t cmb_last_used;
  LIST_ENTRY(comet_mailbox) cmb_link;
  int cmb_debug;
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

  free(cmb->cmb_lang);
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
comet_mailbox_create(const char *lang)
{
  comet_mailbox_t *cmb = calloc(1, sizeof(comet_mailbox_t));

  struct timeval tv;
  uint8_t sum[20];
  char id[20 * 2 + 1];
  int i;
  SHA_CTX sha1;

  gettimeofday(&tv, NULL);

  SHA1_Init(&sha1);
  SHA1_Update(&sha1, (void *)&tv, sizeof(tv));
  SHA1_Update(&sha1, (void *)&mailbox_tally, sizeof(uint32_t));
  SHA1_Final(sum, &sha1);

  for(i = 0; i < sizeof(sum); i++) {
    id[i * 2 + 0] = "0123456789abcdef"[sum[i] >> 4];
    id[i * 2 + 1] = "0123456789abcdef"[sum[i] & 15];
  }
  id[40] = 0;

  cmb->cmb_boxid = strdup(id);
  cmb->cmb_lang = lang ? strdup(lang) : NULL;
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
  extern int access_noacl;

  htsmsg_t *m = htsmsg_create_map();
  const char *username;
  int64_t bfree, bused, btotal;
  int dvr = !http_access_verify(hc, ACCESS_RECORDER);
  int admin = !http_access_verify(hc, ACCESS_ADMIN);
  const char *s;

  username = hc->hc_access ? (hc->hc_access->aa_username ?: "") : "";

  htsmsg_add_str(m, "notificationClass", "accessUpdate");

  switch (hc->hc_access->aa_uilevel) {
  case UILEVEL_BASIC:    s = "basic";    break;
  case UILEVEL_ADVANCED: s = "advanced"; break;
  case UILEVEL_EXPERT:   s = "expert";   break;
  default:               s = NULL;       break;
  }
  if (s) {
    htsmsg_add_str(m, "uilevel", s);
    if (config.uilevel_nochange)
      htsmsg_add_u32(m, "uilevel_nochange", config.uilevel_nochange);
  }
  htsmsg_add_u32(m, "quicktips", config.ui_quicktips);
  if (!access_noacl)
    htsmsg_add_str(m, "username", username);
  if (hc->hc_peer_ipstr)
    htsmsg_add_str(m, "address", hc->hc_peer_ipstr);
  htsmsg_add_u32(m, "dvr",      dvr);
  htsmsg_add_u32(m, "admin",    admin);

  htsmsg_add_s64(m, "time",     time(NULL));

  if (config.cookie_expires)
    htsmsg_add_u32(m, "cookie_expires", config.cookie_expires);

  if (config.info_area && config.info_area[0])
    htsmsg_add_str(m, "info_area", config.info_area);

  if (dvr && !dvr_get_disk_space(&bfree, &bused, &btotal)) {
    htsmsg_add_s64(m, "freediskspace", bfree);
    htsmsg_add_s64(m, "useddiskspace", bused);
    htsmsg_add_s64(m, "totaldiskspace", btotal);
  }

  if (admin && config.wizard)
    htsmsg_add_str(m, "wizard", config.wizard);

  if(cmb->cmb_messages == NULL)
    cmb->cmb_messages = htsmsg_create_list();
  htsmsg_add_msg(cmb->cmb_messages, NULL, m);
}

/**
 *
 */
static void
comet_serverIpPort(http_connection_t *hc, comet_mailbox_t *cmb)
{
  char buf[50];
  uint32_t port;

  tcp_get_str_from_ip((struct sockaddr*)hc->hc_self, buf, 50);

  if(hc->hc_self->ss_family == AF_INET)
    port = ((struct sockaddr_in*)hc->hc_self)->sin_port;
  else if(hc->hc_self->ss_family == AF_INET6)
    port = ((struct sockaddr_in6*)hc->hc_self)->sin6_port;
  else
    port = 0;

  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_str(m, "notificationClass", "setServerIpPort");

  htsmsg_add_str(m, "ip", buf);
  htsmsg_add_u32(m, "port", ntohs(port));

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
  const char *immediate = http_arg_get(&hc->hc_req_args, "immediate");
  int im = immediate ? atoi(immediate) : 0;
  time_t reqtime;
  struct timespec ts;
  htsmsg_t *m;

  if(!im)
    usleep(100000); /* Always sleep 0.1 sec to avoid comet storms */

  pthread_mutex_lock(&comet_mutex);
  if (!comet_running) {
    pthread_mutex_unlock(&comet_mutex);
    return 400;
  }

  if(cometid != NULL)
    LIST_FOREACH(cmb, &mailboxes, cmb_link)
      if(!strcmp(cmb->cmb_boxid, cometid))
	break;
    
  if(cmb == NULL) {
    cmb = comet_mailbox_create(hc->hc_access->aa_lang_ui);
    comet_access_update(hc, cmb);
    comet_serverIpPort(hc, cmb);
  }
  time(&reqtime);

  ts.tv_sec = reqtime + 10;
  ts.tv_nsec = 0;

  cmb->cmb_last_used = 0; /* Make sure we're not flushed out */

  if(!im && cmb->cmb_messages == NULL) {
    pthread_cond_timedwait(&comet_cond, &comet_mutex, &ts);
    if (!comet_running) {
      pthread_mutex_unlock(&comet_mutex);
      return 400;
    }
  }

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
 * Poll callback
 */
static int
comet_mailbox_dbg(http_connection_t *hc, const char *remain, void *opaque)
{
  comet_mailbox_t *cmb = NULL; 
  const char *cometid = http_arg_get(&hc->hc_req_args, "boxid");

  if(cometid == NULL)
    return 400;

  pthread_mutex_lock(&comet_mutex);
  
  LIST_FOREACH(cmb, &mailboxes, cmb_link) {
    if(!strcmp(cmb->cmb_boxid, cometid)) {
      char buf[64];
      cmb->cmb_debug = !cmb->cmb_debug;
 
      if(cmb->cmb_messages == NULL)
	cmb->cmb_messages = htsmsg_create_list();
 
      htsmsg_t *m = htsmsg_create_map();
      htsmsg_add_str(m, "notificationClass", "logmessage");
      snprintf(buf, sizeof(buf), "Loglevel debug: %sabled", 
	       cmb->cmb_debug ? "en" : "dis");
      htsmsg_add_str(m, "logtxt", buf);
      htsmsg_add_msg(cmb->cmb_messages, NULL, m);

      pthread_cond_broadcast(&comet_cond);
    }
  }
  pthread_mutex_unlock(&comet_mutex);

  http_output_content(hc, "text/plain; charset=UTF-8");
  return 0;
}

/**
 *
 */
void
comet_init(void)
{
  pthread_mutex_lock(&comet_mutex);
  comet_running = 1;
  pthread_mutex_unlock(&comet_mutex);
  http_path_add("/comet/poll",  NULL, comet_mailbox_poll, ACCESS_WEB_INTERFACE);
  http_path_add("/comet/debug", NULL, comet_mailbox_dbg,  ACCESS_WEB_INTERFACE);
}

void
comet_done(void)
{
  comet_mailbox_t *cmb;

  pthread_mutex_lock(&comet_mutex);
  comet_running = 0;
  while ((cmb = LIST_FIRST(&mailboxes)) != NULL)
    cmb_destroy(cmb);
  pthread_mutex_unlock(&comet_mutex);
}

/**
 *
 */
static void
comet_mailbox_rewrite_str(htsmsg_t *m, const char *key, const char *lang)
{
  const char *s = htsmsg_get_str(m, key), *p;
  if (s) {
    p = tvh_gettext_lang(lang, s);
    if (p != s)
      htsmsg_set_str(m, key, p);
  }
}

static void
comet_mailbox_rewrite_msg(int rewrite, htsmsg_t *m, const char *lang)
{
  switch (rewrite) {
  case NOTIFY_REWRITE_SUBSCRIPTIONS:
    comet_mailbox_rewrite_str(m, "state", lang);
    break;
  }
}

/**
 *
 */
void
comet_mailbox_add_message(htsmsg_t *m, int isdebug, int rewrite)
{
  comet_mailbox_t *cmb;
  htsmsg_t *e;

  pthread_mutex_lock(&comet_mutex);

  if (comet_running) {
    LIST_FOREACH(cmb, &mailboxes, cmb_link) {

      if(isdebug && !cmb->cmb_debug)
        continue;

      if(cmb->cmb_messages == NULL)
        cmb->cmb_messages = htsmsg_create_list();
      e = htsmsg_copy(m);
      if (cmb->cmb_lang && rewrite)
        comet_mailbox_rewrite_msg(rewrite, e, cmb->cmb_lang);
      htsmsg_add_msg(cmb->cmb_messages, NULL, e);
    }
  }

  pthread_cond_broadcast(&comet_cond);
  pthread_mutex_unlock(&comet_mutex);
}
