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
#include "memoryinfo.h"

static tvh_mutex_t comet_mutex = TVH_THREAD_MUTEX_INITIALIZER;
static tvh_cond_t comet_cond;
static int comet_waiting;

#define MAILBOX_UNUSED_TIMEOUT      20
#define MAILBOX_EMPTY_REPLY_TIMEOUT 10

//#define mbdebug(fmt...) printf(fmt);
#define mbdebug(fmt...)


static LIST_HEAD(, comet_mailbox) mailboxes;

int mailbox_tally;
int comet_running;

static memoryinfo_t comet_memoryinfo = {
  .my_name = "Comet",
};

typedef struct comet_mailbox {
  char *cmb_boxid; /* SHA-1 hash */
  char *cmb_lang;  /* UI language */
  int cmb_refcount;
  int cmb_restricted; /* !admin */
  htsmsg_t *cmb_messages; /* A vector */
  int64_t cmb_last_used;
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

  memoryinfo_free(&comet_memoryinfo,
                  sizeof(*cmb) +
                    (strlen(cmb->cmb_boxid) + 1) +
                    (cmb->cmb_lang ? strlen(cmb->cmb_lang) + 1 : 0));

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

  tvh_mutex_lock(&comet_mutex);

  for(cmb = LIST_FIRST(&mailboxes); cmb != NULL; cmb = next) {
    next = LIST_NEXT(cmb, cmb_link);

    if(cmb->cmb_refcount == 1 &&
       cmb->cmb_last_used && cmb->cmb_last_used + sec2mono(60) < mclk())
      cmb_destroy(cmb);
  }
  tvh_mutex_unlock(&comet_mutex);
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
  cmb->cmb_refcount = 1;
  cmb->cmb_last_used = mclk();
  mailbox_tally++;

  LIST_INSERT_HEAD(&mailboxes, cmb, cmb_link);

  memoryinfo_alloc(&comet_memoryinfo, sizeof(*cmb) +
                                      (strlen(id) + 1) +
                                      (lang ? strlen(lang) + 1 : 0));
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
  const char *username = "";
  int64_t bfree, bused, btotal;
  int dvr = !http_access_verify(hc, ACCESS_RECORDER);
  int admin = !http_access_verify(hc, ACCESS_ADMIN);
  const char *s;

  htsmsg_add_str(m, "notificationClass", "accessUpdate");

  if (hc->hc_access) {
    username = hc->hc_access->aa_username ?: "";

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
  }
  htsmsg_add_str(m, "theme", access_get_theme(hc->hc_access));
  htsmsg_add_u32(m, "quicktips", config.ui_quicktips);
  htsmsg_add_u32(m, "chname_num", config.chname_num);
  htsmsg_add_u32(m, "chname_src", config.chname_src);
  htsmsg_add_str(m, "date_mask", config.date_mask);
  htsmsg_add_u32(m, "label_formatting", config.label_formatting);
  if (!access_noacl)
    htsmsg_add_str(m, "username", username);
  if (hc->hc_peer_ipstr)
    htsmsg_add_str(m, "address", hc->hc_peer_ipstr);
  htsmsg_add_u32(m, "dvr",      dvr);
  htsmsg_add_u32(m, "admin",    admin);

  htsmsg_add_s64(m, "time",     time(NULL));

  if (config.cookie_expires)
    htsmsg_add_u32(m, "cookie_expires", config.cookie_expires);

  if (config.ticket_expires)
    htsmsg_add_u32(m, "ticket_expires", config.ticket_expires);

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

  tcp_get_str_from_ip(hc->hc_self, buf, 50);

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
 *
 */
static htsmsg_t *
comet_message(comet_mailbox_t *cmb, int include_boxid, int ignore_null)
{
  htsmsg_t *m;

  if (ignore_null && cmb->cmb_messages == NULL)
    return NULL;
  m = htsmsg_create_map();
  if (include_boxid)
    htsmsg_add_str(m, "boxid", cmb->cmb_boxid);
  htsmsg_add_msg(m, "messages", cmb->cmb_messages ?: htsmsg_create_list());
  cmb->cmb_messages = NULL;
  cmb->cmb_last_used = mclk();
  return m;
}

/**
 *
 */
static comet_mailbox_t *
comet_find_mailbox(http_connection_t *hc, const char *cometid, const char *lang, int create)
{
  comet_mailbox_t *cmb = NULL;

  if (!atomic_get(&comet_running))
    return NULL;

  if (cometid != NULL)
    LIST_FOREACH(cmb, &mailboxes, cmb_link)
      if(!strcmp(cmb->cmb_boxid, cometid))
	break;

  if (!create)
    return cmb;
    
  if (cmb == NULL) {
    cmb = comet_mailbox_create(lang);
    comet_access_update(hc, cmb);
    comet_serverIpPort(hc, cmb);
  }
  cmb->cmb_last_used = 0; /* Make sure we're not flushed out */
  if (http_access_verify(hc, ACCESS_ADMIN)) {
    if (!cmb->cmb_restricted) {
      cmb->cmb_restricted = 1;
      tvh_mutex_unlock(&comet_mutex);
      comet_mailbox_add_logmsg(tvh_gettext_lang(lang, N_("Restricted log mode (no administrator)")), 0, 0);
      tvh_mutex_lock(&comet_mutex);
    }
  }

  return cmb;
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
  const char *lang = hc->hc_access->aa_lang_ui;
  int im = immediate ? atoi(immediate) : 0, e;
  int64_t mono;
  htsmsg_t *m;

  if(!im)
    tvh_safe_usleep(100000); /* Always sleep 0.1 sec to avoid comet storms */

  tvh_mutex_lock(&comet_mutex);
  cmb = comet_find_mailbox(hc, cometid, lang, 1);
  if (cmb == NULL) {
    tvh_mutex_unlock(&comet_mutex);
    return HTTP_STATUS_BAD_REQUEST;
  }

  if(!im && cmb->cmb_messages == NULL) {
    mono = mclk() + sec2mono(10);
    atomic_add(&comet_waiting, 1);
    do {
      e = tvh_cond_timedwait(&comet_cond, &comet_mutex, mono);
      if (e == ETIMEDOUT)
        break;
    } while (ERRNO_AGAIN(e));
    atomic_dec(&comet_waiting, 1);
    if (!atomic_get(&comet_running)) {
      tvh_mutex_unlock(&comet_mutex);
      return HTTP_STATUS_BAD_REQUEST;
    }
  }

  m = comet_message(cmb, 1, 0);
  tvh_mutex_unlock(&comet_mutex);

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
  const char *lang = hc->hc_access->aa_lang_ui;
  const char *s;

  if(cometid == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  tvh_mutex_lock(&comet_mutex);
  cmb = comet_find_mailbox(hc, cometid, lang, 0);
  if (cmb) {
    char buf[64];
    cmb->cmb_debug = !cmb->cmb_debug;

    if(cmb->cmb_messages == NULL)
      cmb->cmb_messages = htsmsg_create_list();

    if(cmb->cmb_restricted || http_access_verify(hc, ACCESS_ADMIN))
      s = N_("Only admin can watch the realtime log.");
    else if(cmb->cmb_debug)
      s = N_("Loglevel debug: enabled");
    else
      s = N_("Loglevel debug: disabled");
    snprintf(buf, sizeof(buf), "%s", tvh_gettext_lang(lang, s));

    htsmsg_t *m = htsmsg_create_map();
    htsmsg_add_str(m, "notificationClass", "logmessage");
    htsmsg_add_str(m, "logtxt", buf);
    htsmsg_add_msg(cmb->cmb_messages, NULL, m);

    tvh_cond_signal(&comet_cond, 1);
  }
  tvh_mutex_unlock(&comet_mutex);

  http_output_content(hc, "text/plain; charset=UTF-8");
  return 0;
}

/**
 *
 */
static void
comet_mailbox_ws_msg(http_connection_t *hc, comet_mailbox_t *cmb, int first, htsmsg_t *msg)
{
  htsmsg_t *m = NULL;
  tvh_mutex_lock(&comet_mutex);
  if (!atomic_get(&comet_running)) {
    tvh_mutex_unlock(&comet_mutex);
    return;
  }
  m = comet_message(cmb, first, 1);
  cmb->cmb_last_used = 0;
  tvh_mutex_unlock(&comet_mutex);
  if (m) {
    http_websocket_send_json(hc, m);
    htsmsg_destroy(m);
  }
}

/**
 * Websocket handler
 */
static int
comet_mailbox_ws(http_connection_t *hc, const char *remain, void *opaque)
{
  int res, first = 1;
  htsmsg_t *msg;
  const char *cometid = http_arg_get(&hc->hc_req_args, "boxid");
  const char *lang = hc->hc_access->aa_lang_ui;
  comet_mailbox_t *cmb;

  res = http_send_header_websocket(hc, "tvheadend-comet");

  tvh_mutex_lock(&comet_mutex);
  cmb = comet_find_mailbox(hc, cometid, lang, 1);
  if (cmb)
    cmb->cmb_refcount++;
  atomic_add(&comet_waiting, 1);
  tvh_mutex_unlock(&comet_mutex);
  if (!cmb)
    return -1;

  while (res == 0) {
    if (!atomic_get(&comet_running)) {
      res = -1;
      continue;
    }
    res = http_websocket_read(hc, &msg, 1000);
    if (res >= 0) {
      comet_mailbox_ws_msg(hc, cmb, first, msg);
      htsmsg_destroy(msg);
      http_alive(hc);
      first = 0;
    }
  }

  tvh_mutex_lock(&comet_mutex);
  assert(cmb->cmb_refcount > 0);
  cmb->cmb_refcount--;
  cmb->cmb_last_used = mclk();
  atomic_dec(&comet_waiting, 1);
  tvh_mutex_unlock(&comet_mutex);

  return res;
}

/**
 *
 */
void
comet_init(void)
{
  http_path_t *hp;

  memoryinfo_register(&comet_memoryinfo);
  tvh_mutex_lock(&comet_mutex);
  tvh_cond_init(&comet_cond, 1);
  atomic_set(&comet_running, 1);
  atomic_set(&comet_waiting, 0);
  tvh_mutex_unlock(&comet_mutex);
  hp = http_path_add("/comet/ws", NULL, comet_mailbox_ws, ACCESS_WEB_INTERFACE);
  hp->hp_flags = HTTP_PATH_WEBSOCKET;
  http_path_add("/comet/poll",  NULL, comet_mailbox_poll, ACCESS_WEB_INTERFACE);
  http_path_add("/comet/debug", NULL, comet_mailbox_dbg,  ACCESS_WEB_INTERFACE);
}

void
comet_done(void)
{
  comet_mailbox_t *cmb;

  tvh_mutex_lock(&comet_mutex);
  atomic_set(&comet_running, 0);
  tvh_cond_signal(&comet_cond, 1);
  tvh_mutex_unlock(&comet_mutex);

  while (atomic_get(&comet_waiting))
    tvh_usleep(10000);

  tvh_cond_destroy(&comet_cond);

  while ((cmb = LIST_FIRST(&mailboxes)) != NULL)
    cmb_destroy(cmb);

  tvh_mutex_lock(&global_lock);
  memoryinfo_unregister(&comet_memoryinfo);
  tvh_mutex_unlock(&global_lock);
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
comet_mailbox_rewrite_title(htsmsg_t *m, const char *lang)
{
  idnode_t *in;
  const char *s = htsmsg_get_str(m, "uuid");
  char buf[384];
  if (s) {
    idnode_lock();
    in = idnode_find(s, NULL, NULL);
    if (in)
      htsmsg_set_str(m, "text", idnode_get_title(in, lang, buf, sizeof(buf)));
    idnode_unlock();
  }
}

static void
comet_mailbox_rewrite_msg(int rewrite, htsmsg_t *m, const char *lang)
{
  switch (rewrite) {
  case NOTIFY_REWRITE_TITLE:
    comet_mailbox_rewrite_title(m, lang);
    break;
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

  if (!atomic_get(&comet_running))
    return;

  tvh_mutex_lock(&comet_mutex);

  if (atomic_get(&comet_running)) {
    LIST_FOREACH(cmb, &mailboxes, cmb_link) {

      if(cmb->cmb_restricted)
        continue;

      if(isdebug && !cmb->cmb_debug)
        continue;
        
      if(cmb->cmb_messages == NULL)
        cmb->cmb_messages = htsmsg_create_list();
      e = htsmsg_copy(m);
      if (cmb->cmb_lang && rewrite)
        comet_mailbox_rewrite_msg(rewrite, e, cmb->cmb_lang);
      htsmsg_add_msg(cmb->cmb_messages, NULL, e);
    }
    tvh_cond_signal(&comet_cond, 1);
  }

  tvh_mutex_unlock(&comet_mutex);
}

/**
 *
 */
void
comet_mailbox_add_logmsg(const char *txt, int isdebug, int rewrite)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "notificationClass", "logmessage");
  htsmsg_add_str(m, "logtxt", txt);
  comet_mailbox_add_message(m, isdebug, 0);
  htsmsg_destroy(m);
}
