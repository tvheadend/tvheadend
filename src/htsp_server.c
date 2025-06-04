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

#include <fcntl.h>
#include <sys/stat.h>

#include "tvheadend.h"
#include "atomic.h"
#include "config.h"
#include "api.h"
#include "channels.h"
#include "subscriptions.h"
#include "tcp.h"
#include "packet.h"
#include "access.h"
#include "htsp_server.h"
#include "streaming.h"
#include "htsmsg_binary.h"
#include "epg.h"
#include "imagecache.h"
#include "descrambler/caid.h"
#include "notify.h"
#include "htsmsg_json.h"
#include "string_list.h"
#include "lang_codes.h"
#if ENABLE_TIMESHIFT
#include "timeshift.h"
#endif

#include "settings.h"
#include "epggrab.h"  //Needed to be able to test for epggrab_conf.epgdb_processparentallabels

/* **************************************************************************
 * Datatypes and variables
 * *************************************************************************/

static void *htsp_server, *htsp_server_2;

#define HTSP_PROTO_VERSION 43

#define HTSP_ASYNC_OFF  0x00
#define HTSP_ASYNC_ON   0x01
#define HTSP_ASYNC_EPG  0x02

#define HTSP_ASYNC_EPG_INTERVAL 30

#define HTSP_PRIV_MASK (ACCESS_HTSP_STREAMING)

extern char *dvr_storage;

LIST_HEAD(htsp_connection_list, htsp_connection);
LIST_HEAD(htsp_subscription_list, htsp_subscription);
LIST_HEAD(htsp_file_list, htsp_file);

TAILQ_HEAD(htsp_msg_queue, htsp_msg);
TAILQ_HEAD(htsp_msg_q_queue, htsp_msg_q);

static struct htsp_connection_list htsp_async_connections;
static struct htsp_connection_list htsp_connections;

static void htsp_streaming_input(void *opaque, streaming_message_t *sm);
static htsmsg_t *htsp_streaming_input_info(void *opaque, htsmsg_t *list);
const char * _htsp_get_subscription_status(int smcode);
static void htsp_epg_send_waiting(struct htsp_connection *, int64_t mintime);

static streaming_ops_t htsp_streaming_input_ops = {
  .st_cb   = htsp_streaming_input,
  .st_info = htsp_streaming_input_info
};

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
  int hmq_dead;
} htsp_msg_q_t;

/**
 *
 */
typedef struct htsp_connection {
  LIST_ENTRY(htsp_connection) htsp_link;

  int htsp_fd;
  struct sockaddr_storage *htsp_peer;

  uint32_t htsp_version;

  char *htsp_logname;
  char *htsp_peername;
  char *htsp_username;
  char *htsp_clientname;
  char *htsp_language; // for async updates

  int64_t  htsp_epg_window;      // only send async epg updates within this window (seconds)
  int64_t  htsp_epg_lastupdate;  // last update time for async epg events
  mtimer_t htsp_epg_timer;       // timer for async epg updates

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

  tvh_mutex_t htsp_out_mutex;
  tvh_cond_t htsp_out_cond;

  htsp_msg_q_t htsp_hmq_ctrl;
  htsp_msg_q_t htsp_hmq_epg;
  htsp_msg_q_t htsp_hmq_qstatus;

  struct htsp_subscription_list htsp_subscriptions;
  struct htsp_subscription_list htsp_dead_subscriptions;
  struct htsp_file_list htsp_files;
  int htsp_file_id;

  access_t *htsp_granted_access;

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
  int                hs_s_bytes_out;
  mtimer_t           hs_s_bytes_out_timer;

  streaming_target_t hs_input;
  profile_chain_t    hs_prch;

  htsp_msg_q_t hs_q;

  int64_t hs_last_report; /* Last queue status report sent */

  int hs_dropstats[PKT_NTYPES];

  int hs_wait_for_video;

  int hs_90khz;

  int hs_queue_depth;

#define NUM_FILTERED_STREAMS (64*8)

  uint64_t hs_filtered_streams[8]; // one bit per stream

  int hs_first;

  uint32_t hs_data_errors;

} htsp_subscription_t;


/**
 *
 */
typedef struct htsp_file {
  LIST_ENTRY(htsp_file) hf_link;
  int hf_id;          // ID sent to client
  int hf_fd;          // Our file descriptor
  char *hf_path;      // For logging
  uint32_t  hf_de_id; // Associated dvr entry
  th_subscription_t *hf_subscription;
} htsp_file_t;

#define HTSP_DEFAULT_QUEUE_DEPTH 500000

/* **************************************************************************
 * Support routines
 * *************************************************************************/

static void
htsp_trace(htsp_connection_t *htsp, int subsystem,
           const char *prefix, htsmsg_t *m)
{
  htsbuf_queue_t q;
  char *s;
  htsbuf_queue_init(&q, 0);
  htsmsg_json_serialize(m, &q, 0);
  s = htsbuf_to_string(&q);
  htsbuf_queue_flush(&q);
  tvhtrace(subsystem, "%s - %s '%s'", htsp->htsp_logname, prefix, s);
  free(s);
}

static void
htsp_disable_stream(htsp_subscription_t *hs, unsigned int id)
{
  if(id < NUM_FILTERED_STREAMS)
    hs->hs_filtered_streams[id / 64] |= 1 << (id & 63);
}


static void
htsp_enable_stream(htsp_subscription_t *hs, unsigned int id)
{
  if(id < NUM_FILTERED_STREAMS)
    hs->hs_filtered_streams[id / 64] &= ~(1 << (id & 63));
}


static inline int
htsp_is_stream_enabled(htsp_subscription_t *hs, unsigned int id)
{
  if(id < NUM_FILTERED_STREAMS)
    return !(hs->hs_filtered_streams[id / 64] & (1 << (id & 63)));
  return 1;
}

static inline int
htsp_anonymize(htsp_connection_t *htsp)
{
  return (htsp->htsp_granted_access->aa_rights & ACCESS_HTSP_ANONYMIZE) != 0;
}

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
htsp_flush_queue(htsp_connection_t *htsp, htsp_msg_q_t *hmq, int dead)
{
  htsp_msg_t *hm;

  tvh_mutex_lock(&htsp->htsp_out_mutex);

  if(hmq->hmq_length)
    TAILQ_REMOVE(&htsp->htsp_active_output_queues, hmq, hmq_link);

  while((hm = TAILQ_FIRST(&hmq->hmq_q)) != NULL) {
    TAILQ_REMOVE(&hmq->hmq_q, hm, hm_link);
    htsp_msg_destroy(hm);
  }

  // reset
  hmq->hmq_length = 0;
  hmq->hmq_payload = 0;
  hmq->hmq_dead = dead;
  tvh_mutex_unlock(&htsp->htsp_out_mutex);
}

/*
 *
 */
static const char *
htsp_image(htsp_connection_t *htsp, const char *image,
           char *buf, size_t buflen, int version)
{
  const char *ret = image;
  const int id = imagecache_get_id(image);

  /* Handle older clients */
  if (id) {
    if (htsp->htsp_version < version) {
      struct sockaddr_storage addr;
      socklen_t addrlen;
      char abuf[50];
      addrlen = sizeof(addr);
      getsockname(htsp->htsp_fd, (struct sockaddr*)&addr, &addrlen);
      tcp_get_str_from_ip(&addr, abuf, sizeof(abuf));
      snprintf(buf, buflen, "http://%s%s%s:%d%s/imagecache/%d",
                      (addr.ss_family == AF_INET6)?"[":"",
                      abuf,
                      (addr.ss_family == AF_INET6)?"]":"",
                      tvheadend_webui_port,
                      tvheadend_webroot ?: "",
                      id);
      ret = buf;
    } else if (htsp->htsp_version < 15) {
      /* older clients expects '/imagecache/' */
      snprintf(buf, buflen, "/imagecache/%d", id);
      ret = buf;
    } else {
      snprintf(buf, buflen, "imagecache/%d", id);
      ret = buf;
    }
  }
  return ret;
}

/**
 *
 */
static void
htsp_subscription_destroy(htsp_connection_t *htsp, htsp_subscription_t *hs)
{
  th_subscription_t *ts = hs->hs_s;

  hs->hs_s = NULL;
  mtimer_disarm(&hs->hs_s_bytes_out_timer);

  LIST_REMOVE(hs, hs_link);
  LIST_INSERT_HEAD(&htsp->htsp_dead_subscriptions, hs, hs_link);

  subscription_unsubscribe(ts, UNSUBSCRIBE_FINAL);

  if(hs->hs_prch.prch_st != NULL)
    profile_chain_close(&hs->hs_prch);

  htsp_flush_queue(htsp, &hs->hs_q, 1);
}

/**
 *
 */
static void
htsp_subscription_free(htsp_connection_t *htsp, htsp_subscription_t *hs)
{
  LIST_REMOVE(hs, hs_link);
  htsp_flush_queue(htsp, &hs->hs_q, 1);
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

  tvh_mutex_lock(&htsp->htsp_out_mutex);

  assert(!hmq->hmq_dead);

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
  tvh_cond_signal(&htsp->htsp_out_cond, 0);
  tvh_mutex_unlock(&htsp->htsp_out_mutex);
}

/**
 *
 */
static void
htsp_send_subscription(htsp_connection_t *htsp, htsmsg_t *m, pktbuf_t *pb,
	               htsp_subscription_t *hs, int payloadsize)
{
  if (tvhtrace_enabled()) {
    char buf[64];
    size_t l = 0;
    tvh_strlcatf(buf, sizeof(buf), l, "subscription %i", hs->hs_sid);
    if (payloadsize)
      tvh_strlcatf(buf, sizeof(buf), l, " (payload %d)", payloadsize);
    htsp_trace(htsp, LS_HTSP_SUB, buf, m);
  }

  htsp_send(htsp, m, pb, &hs->hs_q, payloadsize);
}

/**
 *
 */
static void
htsp_send_message(htsp_connection_t *htsp, htsmsg_t *m, htsp_msg_q_t *hmq)
{
  if (tvhtrace_enabled()) {
    const char *qname = "answer";
    if (hmq == &htsp->htsp_hmq_qstatus)
      qname = "status";
    htsp_trace(htsp, LS_HTSP_ANS, qname, m);
  }

  htsp_send(htsp, m, NULL, hmq ?: &htsp->htsp_hmq_ctrl, 0);
}

/**
 * Simple function to respond with an error
 */
static htsmsg_t *
htsp_error(htsp_connection_t *htsp, const char *errstr)
{
  htsmsg_t *r = htsmsg_create_map();
  htsmsg_add_str(r, "error", tvh_gettext_lang(htsp->htsp_language, errstr));
  return r;
}

/**
 * Simple function to respond with an success
 */
static htsmsg_t *
htsp_success(void)
{
  htsmsg_t *r = htsmsg_create_map();
  htsmsg_add_u32(r, "success", 1);
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
 * Check if user can access the channel
 */
static inline int
htsp_user_access_channel(htsp_connection_t *htsp, channel_t *ch)
{
  if (!ch || !ch->ch_enabled || LIST_FIRST(&ch->ch_services) == NULL) /* Don't pass unplayable channels to clients */
    return 0;
  if (!htsp)
    return 1;
  return channel_access(ch, htsp->htsp_granted_access, 0);
}

static const char *
htsp_dvr_config_name( htsp_connection_t *htsp, const char *config_name )
{
  dvr_config_t *cfg = NULL, *cfg2;
  access_t *perm = htsp->htsp_granted_access;
  htsmsg_field_t *f;
  const char *uuid;
  static char ubuf[UUID_HEX_SIZE];

  lock_assert(&global_lock);

  config_name = config_name ?: "";

  if (perm->aa_dvrcfgs == NULL)
    return config_name; /* no change */

  HTSMSG_FOREACH(f, perm->aa_dvrcfgs) {
    uuid = htsmsg_field_get_str(f) ?: "";
    if (strcmp(uuid, config_name) == 0)
      return config_name;
    cfg2 = dvr_config_find_by_uuid(uuid);
    if (cfg2 && strcmp(cfg2->dvr_config_name, config_name) == 0)
      return uuid;
    if (!cfg)
      cfg = cfg2;
  }

  if (!cfg && perm->aa_username)
    tvhinfo(LS_HTSP, "User '%s' has no valid dvr config in ACL, using default...", perm->aa_username);

  return cfg ? idnode_uuid_as_str(&cfg->dvr_id, ubuf) : NULL;
}

/**
 * Converts htsp input to internal API
 * @param htsp, current htsp connection
 * @param in, the htsp message input from client
 * @param autorec, true for autorecs, false for timerecs
 * @param add, true for new instances, false for update calls
 * @return the htsmsg_t config to be added or updated with idnode
 */
static htsmsg_t *
htsp_serierec_convert(htsp_connection_t *htsp, htsmsg_t *in, channel_t *ch, int autorec, int add)
{
  htsmsg_t *conf,*days;
  uint32_t u32;
  int64_t s64;
  int32_t approx_time, start, start_window, s32;
  int retval;
  const char *str;
  char ubuf[UUID_HEX_SIZE];

  conf = htsmsg_create_map();

  if (autorec) { // autorec specific
    if (!(retval = htsmsg_get_u32(in, "minduration", &u32)) || add)
      htsmsg_add_u32(conf, "minduration", !retval ? u32 : 0);  // 0 = any
    if (!(retval = htsmsg_get_u32(in, "maxduration", &u32)) || add)
      htsmsg_add_u32(conf, "maxduration", !retval ? u32 : 0);  // 0 = any
    if (!(retval = htsmsg_get_u32(in, "fulltext", &u32)) || add)
      htsmsg_add_u32(conf, "fulltext", !retval ? u32 : 0);     // 0 = off
    if (!(retval = htsmsg_get_u32(in, "dupDetect", &u32)) || add)
      htsmsg_add_u32(conf, "record", !retval ? u32 : DVR_AUTOREC_RECORD_ALL);
    if (!(retval = htsmsg_get_u32(in, "maxCount", &u32)) || add)
      htsmsg_add_u32(conf, "maxcount", !retval ? u32 : 0);     // 0 = unlimited
    if (!(retval = htsmsg_get_u32(in, "broadcastType", &u32)) || add)
      htsmsg_add_u32(conf, "btype", !retval ? u32 : 0); // 0 = all
    if (!(retval = htsmsg_get_s64(in, "startExtra", &s64)) || add)
      htsmsg_add_s64(conf, "start_extra", !retval ? (s64 < 0 ? 0 : s64)  : 0); // 0 = dvr config
    if (!(retval = htsmsg_get_s64(in, "stopExtra", &s64)) || add)
      htsmsg_add_s64(conf, "stop_extra", !retval ? (s64 < 0 ? 0 : s64) : 0);   // 0 = dvr config
    if ((str = htsmsg_get_str(in, "serieslinkUri")) || add)
      htsmsg_add_str(conf, "serieslink", str ?: "");

    if (add) { // for add, stay compatible with older "approxTime
      if(htsmsg_get_s32(in, "approxTime", &approx_time))
        approx_time = -1;
      if(htsmsg_get_s32(in, "start", &start))
        start = -1;
      if(htsmsg_get_s32(in, "startWindow", &start_window))
        start_window = -1;
      if (start < 0 || start_window < 0)
        start = start_window = -1;
      if (start < 0 && approx_time >= 0) {
        start = approx_time - 15;
        if (start < 0)
          start += 24 * 60;
        start_window = start + 30;
        if (start_window >= 24 * 60)
          start_window -= 24 * 60;
      }
      htsmsg_add_s32(conf, "start", start >= 0 ? start : -1); // -1 = any time
      htsmsg_add_s32(conf, "start_window", start_window >= 0 ? start_window : -1); // -1 = any duration
    }
    else { // for update, we don't care about "approxTime"
      if(!htsmsg_get_s32(in, "start", &s32))
        htsmsg_add_s32(conf, "start", s32 >= 0 ? s32 : -1);        // -1 = any time
      if(!htsmsg_get_s32(in, "startWindow", &s32))
        htsmsg_add_s32(conf, "start_window", s32 >= 0 ? s32 : -1); // -1 = any duration
    }
  }
  else { //timerec specific
    if (!(retval = htsmsg_get_u32(in, "start", &u32)) || add)
      htsmsg_add_u32(conf, "start", !retval ? u32 : 0);
    if (!(retval = htsmsg_get_u32(in, "stop", &u32)) || add)
      htsmsg_add_u32(conf, "stop", !retval ? u32 : 0);
  }

  if (!(retval = htsmsg_get_u32(in, "enabled", &u32)) || add)
    htsmsg_add_u32(conf, "enabled", !retval ? (u32 > 0 ? 1 : 0) : 1); // default on
  if (!(retval = htsmsg_get_u32(in, "retention", &u32)) || add)
    htsmsg_add_u32(conf, "retention", !retval ? u32 : DVR_RET_REM_DVRCONFIG);
  if (!(retval = htsmsg_get_u32(in, "removal", &u32)) || add)
    htsmsg_add_u32(conf, "removal", !retval ? u32 : DVR_RET_REM_DVRCONFIG);
  if(!(retval = htsmsg_get_u32(in, "priority", &u32)) || add)
    htsmsg_add_u32(conf, "pri", !retval ? u32 : DVR_PRIO_DEFAULT);
  if ((str = htsmsg_get_str(in, "name")) || add)
    htsmsg_add_str(conf, "name", str ?: "");
  if ((str = htsmsg_get_str(in, "comment")) || add)
    htsmsg_add_str(conf, "comment", str ?: "");
  if ((str = htsmsg_get_str(in, "directory")) || add)
    htsmsg_add_str(conf, "directory", str ?: "");
  if((str = htsmsg_get_str(in, "title")) || add)
    htsmsg_add_str(conf, "title", str ?: "");

  /* Only on creation */
  if (add) {
    str = htsp_dvr_config_name(htsp, htsmsg_get_str(in, "configName"));
    htsmsg_add_str(conf, "config_name", str ?: "");
    htsmsg_add_str2(conf, "owner",   htsp->htsp_granted_access->aa_username);
    htsmsg_add_str2(conf, "creator", htsp->htsp_granted_access->aa_representative);
  } else {
    str = htsmsg_get_str(in, "configName");
    if (str) {
      str = htsp_dvr_config_name(htsp, str);
      htsmsg_add_str(conf, "config_name", str ?: "");
    }
  }

  /* Weekdays only if present */
  if(!(retval = htsmsg_get_u32(in, "daysOfWeek", &u32))) {
    days = htsmsg_create_list();
    int i;
    for (i = 0; i < 7; i++)
      if (u32 & (1 << i))
        htsmsg_add_u32(days, NULL, i + 1);
    htsmsg_add_msg(conf, "weekdays", days);  // not set = all days
  }

  /* Allow channel to be cleared on update -> any channel */
  if (ch || !add) {
    htsmsg_add_str(conf, "channel", ch ? idnode_uuid_as_str(&ch->ch_id, ubuf) : "");
  }

  return conf;
}

/*
 *
 */
static void htsp_serialize_epnum
  (htsmsg_t *out, epg_episode_num_t *epnum, const char *textname)
{
  if (epnum->s_num) {
    htsmsg_add_u32(out, "seasonNumber", epnum->s_num);
    if (epnum->s_cnt)
      htsmsg_add_u32(out, "seasonCount", epnum->s_cnt);
  }
  if (epnum->e_num) {
    htsmsg_add_u32(out, "episodeNumber", epnum->e_num);
    if (epnum->e_cnt)
      htsmsg_add_u32(out, "episodeCount", epnum->e_cnt);
  }
  if (epnum->p_num) {
    htsmsg_add_u32(out, "partNumber", epnum->p_num);
    if (epnum->p_cnt)
      htsmsg_add_u32(out, "partCount", epnum->p_cnt);
  }
  if (epnum->text)
    htsmsg_add_str(out, textname ?: "episodeOnscreen", epnum->text);
}

/* **************************************************************************
 * File helpers
 * *************************************************************************/

static uint32_t
htsp_channel_tag_get_identifier(channel_tag_t *ct)
{
  static int prev = 0;
  if (ct->ct_htsp_id == 0)
    ct->ct_htsp_id = ++prev;
  return ct->ct_htsp_id;
}

static channel_tag_t *
htsp_channel_tag_find_by_id(htsp_connection_t *htsp, uint32_t id)
{
  channel_tag_t *ct;

  TAILQ_FOREACH(ct, &channel_tags, ct_link) {
    if (!channel_tag_access(ct, htsp->htsp_granted_access, 0))
      continue;
    if (id == ct->ct_htsp_id)
      return ct;
  }
  return NULL;
}

/**
 *
 */
static htsmsg_t *
htsp_file_open(htsp_connection_t *htsp, const char *path, int fd, dvr_entry_t *de)
{
  struct stat st;

  if (fd <= 0) {
    tvh_mutex_unlock(&global_lock);
    fd = tvh_open(path, O_RDONLY, 0);
    tvhdebug(LS_HTSP, "Opening file %s -- %s", path, fd < 0 ? strerror(errno) : "OK");
    tvh_mutex_lock(&global_lock);
    if(fd == -1)
      return htsp_error(htsp, N_("Unable to open file"));
  }

  htsp_file_t *hf = calloc(1, sizeof(htsp_file_t));
  hf->hf_fd = fd;
  hf->hf_id = ++htsp->htsp_file_id;
  hf->hf_path = strdup(path);
  hf->hf_de_id = de ? idnode_get_short_uuid(&de->de_id) : 0;

  if (de) {
    const char *charset = de->de_config ? de->de_config->dvr_charset_id : NULL;

    hf->hf_subscription =
      subscription_create_from_file("HTSP", charset, path,
                                    htsp->htsp_peername,
                                    htsp->htsp_granted_access->aa_representative,
                                    htsp->htsp_clientname);
  }

  LIST_INSERT_HEAD(&htsp->htsp_files, hf, hf_link);

  htsmsg_t *rep = htsmsg_create_map();
  htsmsg_add_u32(rep, "id", hf->hf_id);

  if(!fstat(hf->hf_fd, &st)) {
    htsmsg_add_s64(rep, "size", st.st_size);
    htsmsg_add_s64(rep, "mtime", st.st_mtime);
  }

  return rep;
}

/**
 *
 */
static htsp_file_t *
htsp_file_find(const htsp_connection_t *htsp, htsmsg_t *in)
{
  htsp_file_t *hf;

  int id = htsmsg_get_u32_or_default(in, "id", 0);

  LIST_FOREACH(hf, &htsp->htsp_files, hf_link) {
    if(hf->hf_id == id)
      return hf;
  }
  return NULL;
}

/**
 *
 */
static void
htsp_file_destroy(htsp_file_t *hf)
{
  tvhdebug(LS_HTSP, "Closed opened file %s", hf->hf_path);
  LIST_REMOVE(hf, hf_link);
  if (hf->hf_subscription) {
    tvh_mutex_lock(&global_lock);
    subscription_unsubscribe(hf->hf_subscription, UNSUBSCRIBE_FINAL);
    tvh_mutex_unlock(&global_lock);
  }
  free(hf->hf_path);
  close(hf->hf_fd);
  free(hf);
}

static void
htsp_file_update_stats(htsp_file_t *hf, size_t len)
{
  th_subscription_t *ts = hf->hf_subscription;
  if (ts) {
    subscription_add_bytes_in(ts, len);
    subscription_add_bytes_out(ts, len);
  }
}

/* **************************************************************************
 * Output message generators
 * *************************************************************************/

/**
 *
 */
static htsmsg_t *
htsp_build_channel(channel_t *ch, const char *method, htsp_connection_t *htsp)
{
  idnode_list_mapping_t *ilm;
  channel_tag_t *ct;
  service_t *t;
  epg_broadcast_t *now, *next = NULL;
  int64_t chnum = channel_get_number(ch);
  const char *icon;
  char buf[512];

  htsmsg_t *out = htsmsg_create_map();
  htsmsg_t *tags = htsmsg_create_list();
  htsmsg_t *services = htsmsg_create_list();

  htsmsg_add_u32(out, "channelId", channel_get_id(ch));
  htsmsg_add_str(out, "channelIdStr", idnode_uuid_as_str(&ch->ch_id, buf));
  
  htsmsg_add_u32(out, "channelNumber", channel_get_major(chnum));
  if (channel_get_minor(chnum))
    htsmsg_add_u32(out, "channelNumberMinor", channel_get_minor(chnum));

  htsmsg_add_str(out, "channelName", channel_get_name(ch, channel_blank_name));
  if ((icon = channel_get_icon(ch)))
    htsmsg_add_str(out, "channelIcon", htsp_image(htsp, icon, buf, sizeof(buf), 8));

  now  = ch->ch_epg_now;
  next = ch->ch_epg_next;
  htsmsg_add_u32(out, "eventId", now ? now->id : 0);
  htsmsg_add_u32(out, "nextEventId", next ? next->id : 0);

  LIST_FOREACH(ilm, &ch->ch_ctms, ilm_in2_link) {
    ct = (channel_tag_t *)ilm->ilm_in1;
    if(channel_tag_access(ct, htsp->htsp_granted_access, 0))
      htsmsg_add_u32(tags, NULL, htsp_channel_tag_get_identifier(ct));
  }

  LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
    t = (service_t *)ilm->ilm_in1;
    htsmsg_t *svcmsg = htsmsg_create_map();
    htsmsg_add_str(svcmsg, "name", service_nicename(t));

    /* Service type string, i.e. UHD, HD, Radio,... */
    htsmsg_add_str(svcmsg, "type", service_servicetype_txt(t));

    /* Service content, other = 0x00, tv = 0x01, radio = 0x02 */
    htsmsg_add_u32(svcmsg, "content", service_is_tv(t) ? 0x01 : (service_is_radio(t) ? 0x02 : 0x00));

    if (service_is_encrypted(t)) {
      htsmsg_add_u32(svcmsg, "caid", 65535);
      htsmsg_add_str(svcmsg, "caname", tvh_gettext_lang(htsp->htsp_language, N_("Encrypted service")));
    }

    /* HbbTv */
    if (t->s_hbbtv)
      htsmsg_add_msg(svcmsg, "hbbtv", htsmsg_copy(t->s_hbbtv));

    /* Provider */
    htsmsg_add_str2(svcmsg, "providername", t->s_provider_name(t));

    htsmsg_add_msg(services, NULL, svcmsg);
  }

  htsmsg_add_msg(out, "services", services);
  htsmsg_add_msg(out, "tags", tags);
  if (method)
    htsmsg_add_str(out, "method", method);
  return out;
}

/**
 *
 */
static htsmsg_t *
htsp_build_tag(htsp_connection_t *htsp, channel_tag_t *ct, const char *method, int include_channels)
{
  char buf[512];
  const char *icon;
  idnode_list_mapping_t *ilm;
  htsmsg_t *out = htsmsg_create_map();
  htsmsg_t *members = include_channels ? htsmsg_create_list() : NULL;

  htsmsg_add_u32(out, "tagId", htsp_channel_tag_get_identifier(ct));
  htsmsg_add_str(out, "tagIdStr", idnode_uuid_as_str(&ct->ct_id, buf));

  htsmsg_add_u32(out, "tagIndex", ct->ct_index);

  htsmsg_add_str(out, "tagName", ct->ct_name);
  icon = channel_tag_get_icon(ct);
  if (!strempty(icon))
    htsmsg_add_str(out, "tagIcon", htsp_image(htsp, icon, buf, sizeof(buf), 34));
  htsmsg_add_u32(out, "tagTitledIcon", ct->ct_titled_icon);

  if(members != NULL) {
    LIST_FOREACH(ilm, &ct->ct_ctms, ilm_in1_link)
      htsmsg_add_u32(members, NULL, channel_get_id((channel_t *)ilm->ilm_in2));
    htsmsg_add_msg(out, "members", members);
  }

  htsmsg_add_str(out, "method", method);
  return out;
}

/**
 *
 */
static htsmsg_t *
htsp_build_dvrentry(htsp_connection_t *htsp, dvr_entry_t *de, const char *method, const char *lang, int statsonly)
{
  htsmsg_t *out = htsmsg_create_map(), *l, *m, *e, *info;
  htsmsg_field_t *f;
  const char *s = NULL, *error = NULL, *subscriptionError = NULL;
  const char *p, *last;
  int64_t fsize = -1, start, stop, size;
  uint32_t u32;
  char buf[512];
  char ubuf[UUID_HEX_SIZE];
  const char *str;

  htsmsg_add_u32(out, "id", idnode_get_short_uuid(&de->de_id));
  htsmsg_add_str(out, "idStr", idnode_uuid_as_str(&de->de_id, ubuf));

  if (!statsonly) {
    htsmsg_add_u32(out, "enabled", de->de_enabled >= 1 ? 1 : 0);
    if (de->de_channel)
      htsmsg_add_u32(out, "channel", channel_get_id(de->de_channel));
    if (de->de_channel_name) /* stays valid after channel deletion */
      htsmsg_add_str(out, "channelName", de->de_channel_name);

    if (de->de_bcast)
      htsmsg_add_u32(out, "eventId",  de->de_bcast->id);

    if (de->de_autorec)
      htsmsg_add_str(out, "autorecId", idnode_uuid_as_str(&de->de_autorec->dae_id, ubuf));

    if (de->de_timerec)
      htsmsg_add_str(out, "timerecId", idnode_uuid_as_str(&de->de_timerec->dte_id, ubuf));

    htsmsg_add_s64(out, "start",       de->de_start);
    htsmsg_add_s64(out, "stop",        de->de_stop);
    htsmsg_add_s64(out, "startExtra",  dvr_entry_get_extra_time_pre(de)/60);
    htsmsg_add_s64(out, "stopExtra",   dvr_entry_get_extra_time_post(de)/60);

    if (htsp->htsp_version > 24)
      htsmsg_add_u32(out, "retention",   dvr_entry_get_retention_days(de));
    else
      htsmsg_add_u32(out, "retention",   dvr_entry_get_retention_days(de) == DVR_RET_ONREMOVE ?
          dvr_entry_get_removal_days(de) : dvr_entry_get_retention_days(de));

    htsmsg_add_u32(out, "removal",     dvr_entry_get_removal_days(de));
    u32 = de->de_pri;
    if (htsp->htsp_version < 30 && u32 > DVR_PRIO_UNIMPORTANT)
      u32 = de->de_config->dvr_pri;
    else if (u32 == DVR_PRIO_NOTSET)
      u32 = DVR_PRIO_NORMAL;
    htsmsg_add_u32(out, "priority",    u32);
    htsmsg_add_u32(out, "contentType", de->de_content_type);
    htsmsg_add_u32(out, "ageRating", de->de_age_rating);

    //Only add the rating label fields if rating labels are enabled.
    if(epggrab_conf.epgdb_processparentallabels){
      //If this is still scheduled (in the future) then send the current values,
      //if not, send the 'saved' values.

      if(de->de_sched_state == DVR_SCHEDULED){
        if(de->de_rating_label){
          if(de->de_rating_label->rl_display_label){
            htsmsg_add_str(out, "ratingLabel", de->de_rating_label->rl_display_label);
          }
          //If the rating icon is not null.
          if(de->de_rating_label->rl_icon){
            str = de->de_rating_label->rl_icon;
            if (!strempty(str)) {
              str = imagecache_get_propstr(str, buf, sizeof(buf));
              if (str)
                htsmsg_add_str(out, "ratingIcon", str);
            }//END got an imagecache location
          }//END icon not null

          //The authority and country are added for Kodi's parentalRatingSource field.
          //Kodi looks for the authority first and if that is not present, then it uses the country.
          //There could be no label, but there could be an age.  The authority &
          //country could still be useful.
          if(de->de_rating_label->rl_authority){
            htsmsg_add_str(out, "ratingAuthority", de->de_rating_label->rl_authority);
          }//END authority saved not null

          if(de->de_rating_label->rl_country){
            htsmsg_add_str(out, "ratingCountry", de->de_rating_label->rl_country);
          }//END country saved not null
        }//END rating label not null
      }//END this is a scheduled recording.
      else
      {
        if(de->de_rating_label_saved){
          if(de->de_rating_label_saved){
            htsmsg_add_str(out, "ratingLabel", de->de_rating_label_saved);
          }
          if(de->de_rating_icon_saved){
            str = de->de_rating_icon_saved;
            if (!strempty(str)) {
              str = imagecache_get_propstr(str, buf, sizeof(buf));
              if (str)
                htsmsg_add_str(out, "ratingIcon", str);
            }//END got an imagecache location
          }//END icon not null

          if(de->de_rating_authority_saved){
            htsmsg_add_str(out, "ratingAuthority", de->de_rating_authority_saved);
          }//END authority saved not null

          if(de->de_rating_country_saved){
            htsmsg_add_str(out, "ratingCountry", de->de_rating_country_saved);
          }//END country saved not null
        }//END got saved rating label
      }//END this is not a scheduled recording.
    }//END processing rating labels is enabled

    if (de->de_sched_state == DVR_RECORDING || de->de_sched_state == DVR_COMPLETED) {
      htsmsg_add_u32(out, "playcount",    de->de_playcount);
      htsmsg_add_u32(out, "playposition", de->de_playposition);
    }

    if(de->de_title && (s = lang_str_get(de->de_title, lang)))
      htsmsg_add_str(out, "title", s);
    if (htsp->htsp_version < 32) {
      if ((s = lang_str_get(de->de_desc, lang))) {
        htsmsg_add_str(out, "description", s);
        if ((s = lang_str_get(de->de_summary, lang)))
          htsmsg_add_str(out, "summary", s);
        if ((s = lang_str_get(de->de_subtitle, lang)))
          htsmsg_add_str(out, "subtitle", s);
      } else if ((s = lang_str_get(de->de_summary, lang))) {
        htsmsg_add_str(out, "description", s);
        if ((s = lang_str_get(de->de_subtitle, lang)))
          htsmsg_add_str(out, "subtitle", s);
      } else if ((s = lang_str_get(de->de_subtitle, lang))) {
        htsmsg_add_str(out, "description", s);
      }
    } else {
      if (de->de_subtitle && (s = lang_str_get(de->de_subtitle, lang)))
        htsmsg_add_str(out, "subtitle", s);
      if (de->de_summary && (s = lang_str_get(de->de_summary, lang)))
        htsmsg_add_str(out, "summary", s);
      if (de->de_desc && (s = lang_str_get(de->de_desc, lang)))
        htsmsg_add_str(out, "description", s);
    }
    htsp_serialize_epnum(out, &de->de_epnum, "episode");
    htsmsg_add_str2(out, "owner", de->de_owner);
    htsmsg_add_str2(out, "creator", de->de_creator);
    htsmsg_add_str2(out, "comment", de->de_comment);
    /* We use the accessor since it will also try to get
     * an image from current EPG if recording does not have
     * an associated image.
     */
    const char *image = dvr_entry_get_image(de);
    if(!strempty(image))
      htsmsg_add_str(out, "image", htsp_image(htsp, image, buf, sizeof(buf), 34));
    /* htsmsg camelcase to be compatible with other names */
    image = de->de_fanart_image;
    if(!strempty(image))
      htsmsg_add_str(out, "fanartImage", htsp_image(htsp, image, buf, sizeof(buf), 34));
    if (de->de_copyright_year)
      htsmsg_add_u32(out, "copyrightYear", de->de_copyright_year);

    last = NULL;
    if (!htsmsg_is_empty(de->de_files) && de->de_config) {
      l = htsmsg_create_list();
      HTSMSG_FOREACH(f, de->de_files) {
        m = htsmsg_field_get_map(f);
        if (m == NULL) continue;
        s = last = htsmsg_get_str(m, "filename");
        if (s && (p = tvh_strbegins(s, de->de_config->dvr_storage)) != NULL) {
          e = htsmsg_copy(m);
          htsmsg_set_str(e, "filename", p);
          info = htsmsg_get_list(m, "info");
          if (info)
            htsmsg_set_msg(e, "info", htsmsg_copy(info));
          if (!htsmsg_get_s64(m, "start", &start))
            htsmsg_set_s64(e, "start", start);
          if (!htsmsg_get_s64(m, "stop", &stop))
            htsmsg_set_s64(e, "stop", stop);
          if (!htsmsg_get_s64(m, "size", &size))
            htsmsg_set_s64(e, "size", size);

          htsmsg_add_msg(l, NULL, e);
        }
      }
      htsmsg_add_msg(out, "files", l);
    }

    if(last && de->de_config)
      if ((p = tvh_strbegins(last, de->de_config->dvr_storage)))
        htsmsg_add_str(out, "path", p);

    if (de->de_config)
      htsmsg_add_str(out, "configId", idnode_uuid_as_str(&de->de_config->dvr_id, ubuf));
  }

  switch(de->de_sched_state) {
  case DVR_SCHEDULED:
    s = "scheduled";
    break;
  case DVR_RECORDING:
    s = "recording";
    fsize = dvr_get_filesize(de, DVR_FILESIZE_UPDATE);
    if (de->de_rec_state == DVR_RS_ERROR ||
       (de->de_rec_state == DVR_RS_PENDING && de->de_last_error != SM_CODE_OK))
    {
      error = streaming_code2txt(de->de_last_error);
      subscriptionError = _htsp_get_subscription_status(de->de_last_error);
    }
    break;
  case DVR_COMPLETED:
    s = "completed";
    fsize = dvr_get_filesize(de, DVR_FILESIZE_UPDATE);
    if (fsize < 0)
      error = "File missing";
    else if(!dvr_entry_is_completed_ok(de))
      error = streaming_code2txt(de->de_last_error);
    break;
  case DVR_MISSED_TIME:
    s = "missed";
    break;
  case DVR_NOSTATE:
    s = "invalid";
    break;
  }

  if (dvr_entry_is_upcoming(de))
    htsmsg_add_u32(out, "duplicate", dvr_entry_is_upcoming_nodup(de) ? 0 : 1);

  htsmsg_add_str(out, "state", s);
  if(error)
    htsmsg_add_str(out, "error", error);
  if (subscriptionError)
    htsmsg_add_str(out, "subscriptionError", subscriptionError);
  if (de->de_errors)
    htsmsg_add_u32(out, "streamErrors", de->de_errors);
  if (de->de_data_errors)
    htsmsg_add_u32(out, "dataErrors", de->de_data_errors);
  if (fsize >= 0)
    htsmsg_add_s64(out, "dataSize", fsize);
  htsmsg_add_str(out, "method", method);
  return out;
}

/**
 *
 */
static htsmsg_t *
htsp_build_autorecentry(htsp_connection_t *htsp, dvr_autorec_entry_t *dae, const char *method)
{
  htsmsg_t *out = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];
  int tad;

  htsmsg_add_str(out, "id",          idnode_uuid_as_str(&dae->dae_id, ubuf));
  htsmsg_add_u32(out, "enabled",     dae->dae_enabled >= 1 ? 1 : 0);
  htsmsg_add_u32(out, "maxDuration", dae->dae_maxduration);
  htsmsg_add_u32(out, "minDuration", dae->dae_minduration);

  if (htsp->htsp_version > 24)
    htsmsg_add_u32(out, "retention",   dvr_autorec_get_retention_days(dae));
  else
    htsmsg_add_u32(out, "retention",   dvr_autorec_get_retention_days(dae) == DVR_RET_ONREMOVE ?
        dvr_autorec_get_removal_days(dae) : dvr_autorec_get_retention_days(dae));

  htsmsg_add_u32(out, "removal",     dvr_autorec_get_removal_days(dae));
  htsmsg_add_u32(out, "daysOfWeek",  dae->dae_weekdays);
  if (dae->dae_start >= 0 && dae->dae_start_window >= 0) {
    if (dae->dae_start > dae->dae_start_window)
      tad = 24 * 60 - dae->dae_start + dae->dae_start_window;
    else
      tad = dae->dae_start_window - dae->dae_start;
  } else {
    tad = -1;
  }
  htsmsg_add_s32(out, "approxTime",
                 dae->dae_start >= 0 && tad >= 0 ?
                   ((dae->dae_start + tad / 2) % (24 * 60)) : -1);
  htsmsg_add_s32(out, "start",       dae->dae_start >= 0 ? dae->dae_start : -1);
  htsmsg_add_s32(out, "startWindow", dae->dae_start_window >= 0 ? dae->dae_start_window : -1);
  htsmsg_add_u32(out, "priority",    dae->dae_pri);
  htsmsg_add_s64(out, "startExtra",  dvr_autorec_get_extra_time_pre(dae));
  htsmsg_add_s64(out, "stopExtra",   dvr_autorec_get_extra_time_post(dae));
  htsmsg_add_u32(out, "dupDetect",   dae->dae_record);
  htsmsg_add_u32(out, "maxCount",    dae->dae_max_count);
  htsmsg_add_u32(out, "broadcastType", dae->dae_btype);
  htsmsg_add_str2(out, "comment",    dae->dae_comment);

  if(dae->dae_title) {
    htsmsg_add_str(out, "title",     dae->dae_title);
    htsmsg_add_u32(out, "fulltext",  dae->dae_fulltext >= 1 ? 1 : 0);
  }
  htsmsg_add_str2(out, "name",       dae->dae_name);
  if(dae->dae_directory)
    htsmsg_add_str(out, "directory", dae->dae_directory);
  htsmsg_add_str2(out, "owner",      dae->dae_owner);
  htsmsg_add_str2(out, "creator",    dae->dae_creator);
  if(dae->dae_channel)
    htsmsg_add_u32(out, "channel",   channel_get_id(dae->dae_channel));
  if (dae->dae_serieslink_uri)
    htsmsg_add_str(out, "serieslinkUri", dae->dae_serieslink_uri);
  if (dae->dae_config)
     htsmsg_add_str(out, "configId", idnode_uuid_as_str(&dae->dae_config->dvr_id, ubuf));

  htsmsg_add_str(out, "method", method);

  return out;
}

/**
 *
 */
static htsmsg_t *
htsp_build_timerecentry(htsp_connection_t *htsp, dvr_timerec_entry_t *dte, const char *method)
{
  htsmsg_t *out = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];

  htsmsg_add_str(out, "id",          idnode_uuid_as_str(&dte->dte_id, ubuf));
  htsmsg_add_u32(out, "enabled",     dte->dte_enabled >= 1 ? 1 : 0);
  htsmsg_add_u32(out, "daysOfWeek",  dte->dte_weekdays);

  if (htsp->htsp_version > 24)
    htsmsg_add_u32(out, "retention",   dvr_timerec_get_retention_days(dte));
  else
    htsmsg_add_u32(out, "retention",   dvr_timerec_get_retention_days(dte) == DVR_RET_ONREMOVE ?
        dvr_timerec_get_removal_days(dte) : dvr_timerec_get_retention_days(dte));

  htsmsg_add_u32(out, "removal",     dvr_timerec_get_removal_days(dte));
  htsmsg_add_u32(out, "priority",    dte->dte_pri);
  htsmsg_add_s32(out, "start",       dte->dte_start);
  htsmsg_add_s32(out, "stop",        dte->dte_stop);
  htsmsg_add_str2(out, "comment",    dte->dte_comment);

  if(dte->dte_title)
    htsmsg_add_str(out, "title",     dte->dte_title);
  if(dte->dte_name)
    htsmsg_add_str(out, "name",      dte->dte_name);
  if(dte->dte_directory)
    htsmsg_add_str(out, "directory", dte->dte_directory);
  htsmsg_add_str2(out, "owner",      dte->dte_owner);
  htsmsg_add_str2(out, "creator",    dte->dte_creator);
  if(dte->dte_channel)
    htsmsg_add_u32(out, "channel",   channel_get_id(dte->dte_channel));
  if (dte->dte_config)
    htsmsg_add_str(out, "configId", idnode_uuid_as_str(&dte->dte_config->dvr_id, ubuf));

  htsmsg_add_str(out, "method", method);

  return out;
}

/**
 *
 */
static htsmsg_t *
htsp_build_event
  (epg_broadcast_t *e, const char *method, const char *lang, time_t update,
   htsp_connection_t *htsp )
{
  htsmsg_t *out;
  epg_broadcast_t *n;
  dvr_entry_t *de;
  epg_genre_t *g;
  epg_episode_num_t epnum;
  const char *str;
  char buf[512];
  const int of = htsp->htsp_granted_access->aa_htsp_output_format;

  /* Ignore? */
  if (update && e->updated <= update) return NULL;

  out = htsmsg_create_map();

  if (method)
    htsmsg_add_str(out, "method", method);

  htsmsg_add_u32(out, "eventId", e->id);
  if (e->channel)
    htsmsg_add_u32(out, "channelId", channel_get_id(e->channel));
  htsmsg_add_s64(out, "start", e->start);
  htsmsg_add_s64(out, "stop", e->stop);
  if ((str = epg_broadcast_get_title(e, lang)))
    htsmsg_add_str(out, "title", str);
  /* For basic format, we want to skip the large text fields
   * and go straight to doing the low-overhead fields.
   */
  if (of != ACCESS_HTSP_OUTPUT_FORMAT_BASIC) {
    if (htsp->htsp_version < 32) {
      if ((str = epg_broadcast_get_description(e, lang))) {
        htsmsg_add_str(out, "description", str);
        if ((str = epg_broadcast_get_summary(e, lang)))
          htsmsg_add_str(out, "summary", str);
        if ((str = epg_broadcast_get_subtitle(e, lang)))
          htsmsg_add_str(out, "subtitle", str);
      } else if ((str = epg_broadcast_get_summary(e, lang))) {
        htsmsg_add_str(out, "description", str);
        if ((str = epg_broadcast_get_subtitle(e, lang)))
          htsmsg_add_str(out, "subtitle", str);
      } else if ((str = epg_broadcast_get_subtitle(e, lang))) {
        htsmsg_add_str(out, "description", str);
      }
    } else {
      if ((str = epg_broadcast_get_subtitle(e, lang)))
        htsmsg_add_str(out, "subtitle", str);
      if ((str = epg_broadcast_get_summary(e, lang)))
        htsmsg_add_str(out, "summary", str);
      if ((str = epg_broadcast_get_description(e, lang)))
        htsmsg_add_str(out, "description", str);
    }

    if (e->credits)
      htsmsg_add_msg(out, "credits", htsmsg_copy(e->credits));
    if (e->category)
      string_list_serialize(e->category, out, "category");
    if (e->keyword)
      string_list_serialize(e->keyword, out, "keyword");
  }

  if (e->serieslink)
    htsmsg_add_str(out, "serieslinkUri", e->serieslink->uri);

  /* tvh:// uris are internal */
  if (e->episodelink && strncasecmp(e->episodelink->uri, "tvh://", 6))
    htsmsg_add_str(out, "episodeUri", e->episodelink->uri);

  if((g = LIST_FIRST(&e->genre))) {
    uint32_t code = g->code;
    if (htsp->htsp_version < 6) code = (code >> 4) & 0xF;
    htsmsg_add_u32(out, "contentType", code);
  }
  if (e->age_rating){
    htsmsg_add_u32(out, "ageRating", e->age_rating);
  }

  //If processing parental labels is enabled
  if(epggrab_conf.epgdb_processparentallabels)
  {
    //If this event had a label pointer that is not null
    if (e->rating_label)
      {
        //If there is a 'display label'
        //Do not fall-back to the 'label' because the 'display label'
        //may be intentionally null.
        if(e->rating_label->rl_display_label){
          htsmsg_add_str(out, "ratingLabel", e->rating_label->rl_display_label);
        }
        //If the rating icon is not null.
        if(e->rating_label->rl_icon){
          str = e->rating_label->rl_icon;
          if (!strempty(str)) {
            str = imagecache_get_propstr(str, buf, sizeof(buf));
            if (str)
              htsmsg_add_str(out, "ratingIcon", str);
          }//END got an imagecache location
        }//END icon not null

        if(e->rating_label->rl_authority){
          htsmsg_add_str(out, "ratingAuthority", e->rating_label->rl_authority);
        }//END authority saved not null

        if(e->rating_label->rl_country){
          htsmsg_add_str(out, "ratingCountry", e->rating_label->rl_country);
        }//END country saved not null

      }//END rating label not null
  }//END parental labels enabled.

  if (e->star_rating)
    htsmsg_add_u32(out, "starRating", e->star_rating);
  if (e->copyright_year)
    htsmsg_add_u32(out, "copyrightYear", e->copyright_year);
  if (e->first_aired)
    htsmsg_add_s64(out, "firstAired", e->first_aired);
  if (e->is_new)
    htsmsg_add_u32(out, "isNew", e->is_new);
  epg_broadcast_get_epnum(e, &epnum);
  htsp_serialize_epnum(out, &epnum, NULL);
  if (!strempty(e->image))
    htsmsg_add_str(out, "image", htsp_image(htsp, e->image, buf, sizeof(buf), 34));

  if (e->channel) {
    LIST_FOREACH(de, &e->channel->ch_dvrs, de_channel_link) {
      if (de->de_bcast != e)
        continue;
      if (dvr_entry_verify(de, htsp->htsp_granted_access, 1))
        continue;
      htsmsg_add_u32(out, "dvrId", idnode_get_short_uuid(&de->de_id));
      break;
    }
  }

  if ((n = epg_broadcast_get_next(e)))
    htsmsg_add_u32(out, "nextEventId", n->id);

  return out;
}

/* **************************************************************************
 * Message handlers
 * *************************************************************************/

/**
 * Hello, for protocol version negotiation
 */
static htsmsg_t *
htsp_method_hello(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *r;
  uint32_t v;
  const char *name, *lang;

  if(htsmsg_get_u32(in, "htspversion", &v))
    return htsp_error(htsp, N_("Invalid arguments"));

  if((name = htsmsg_get_str(in, "clientname")) == NULL)
    return htsp_error(htsp, N_("Invalid arguments"));

  r = htsmsg_create_map();

  tvh_str_update(&htsp->htsp_clientname, htsmsg_get_str(in, "clientname"));

  tvhinfo(LS_HTSP, "%s: Welcomed client software: %s (HTSPv%d)",
	  htsp->htsp_logname, name, v);

  htsmsg_add_u32(r, "htspversion", HTSP_PROTO_VERSION);
  htsmsg_add_str(r, "servername", config_get_server_name());
  htsmsg_add_str(r, "serverversion", tvheadend_version);
  htsmsg_add_bin(r, "challenge", htsp->htsp_challenge, 32);
  if (tvheadend_webroot)
    htsmsg_add_str(r, "webroot", tvheadend_webroot);
  lang = config_get_language();
  if (lang)
    htsmsg_add_str(r, "language", lang);

  /* Capabilities */
  htsmsg_add_msg(r, "servercapability", tvheadend_capabilities_list(1));
  htsmsg_add_u32(r, "api_version", TVH_API_VERSION);

  /* Set version to lowest num */
  htsp->htsp_version = MIN(HTSP_PROTO_VERSION, v);

  htsp_update_logname(htsp);
  return r;
}

/**
 * Try to authenticate
 */
static htsmsg_t *
htsp_method_authenticate(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *r = htsmsg_create_map();

  if(!(htsp->htsp_granted_access->aa_rights & HTSP_PRIV_MASK))
    htsmsg_add_u32(r, "noaccess", 1);
  else if (htsp->htsp_version > 25) {
    htsmsg_add_u32(r, "admin",          htsp->htsp_granted_access->aa_rights & ACCESS_ADMIN ? 1 : 0);
    htsmsg_add_u32(r, "streaming",      htsp->htsp_granted_access->aa_rights & ACCESS_HTSP_STREAMING ? 1 : 0);
    htsmsg_add_u32(r, "dvr",            htsp->htsp_granted_access->aa_rights & ACCESS_HTSP_RECORDER ? 1 : 0);
    htsmsg_add_u32(r, "faileddvr",      htsp->htsp_granted_access->aa_rights & ACCESS_FAILED_RECORDER ? 1 : 0);
    htsmsg_add_u32(r, "anonymous",      htsp->htsp_granted_access->aa_rights & ACCESS_HTSP_ANONYMIZE ? 1 : 0);
    htsmsg_add_u32(r, "limitall",       htsp->htsp_granted_access->aa_conn_limit);
    htsmsg_add_u32(r, "limitdvr",       htsp->htsp_granted_access->aa_conn_limit_dvr);
    htsmsg_add_u32(r, "limitstreaming", htsp->htsp_granted_access->aa_conn_limit_streaming);
    htsmsg_add_u32(r, "uilevel",        htsp->htsp_granted_access->aa_uilevel == UILEVEL_DEFAULT ?
        config.uilevel : htsp->htsp_granted_access->aa_uilevel);
    htsmsg_add_str(r, "uilanguage",     htsp->htsp_granted_access->aa_lang_ui ?
        htsp->htsp_granted_access->aa_lang_ui : (config.language_ui ? config.language_ui : ""));
  }

  return r;
}

/**
 * Try to authenticate
 */
static htsmsg_t *
htsp_method_api(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *resp = NULL, *ret = htsmsg_create_map();
  htsmsg_t *args, *args2 = NULL;
  const char *remain;
  int r;

  tvh_mutex_unlock(&global_lock);

  args   = htsmsg_get_map(in, "args");
  remain = htsmsg_get_str(in, "path");

  if (args == NULL)
    args = args2 = htsmsg_create_map();

  /* Call */
  r = api_exec(htsp->htsp_granted_access, remain, args, &resp);

  /* Convert error */
  if (r) {
    switch (r) {
      case EPERM:
      case EACCES:
        htsmsg_add_u32(ret, "noaccess", 1);
        break;
      case ENOENT:
      case ENOSYS:
        break;
      default:
        htsmsg_destroy(args2);
        htsmsg_destroy(ret);
        return htsp_error(htsp, N_("Bad request"));
    }
  } else if (resp) {
    /* Output response */
    htsmsg_add_msg(ret, "response", resp);
  }

  htsmsg_destroy(args2);

  tvh_mutex_lock(&global_lock);
  return ret;
}

/**
 * Get total and free disk space on configured path
 */
static htsmsg_t *
htsp_method_getDiskSpace(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out;
  int64_t bfree, bused, btotal;

  if (dvr_get_disk_space(&bfree, &bused, &btotal))
    return htsp_error(htsp, N_("Unable to stat path"));

  out = htsmsg_create_map();
  htsmsg_add_s64(out, "freediskspace", bfree);
  htsmsg_add_s64(out, "useddiskspace", bused);
  htsmsg_add_s64(out, "totaldiskspace", btotal);
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
  struct timezone tz;
  int tz_offset;
  struct tm serverLocalTime;

  if(gettimeofday(&tv, &tz) == -1)
    return htsp_error(htsp, N_("Unable to get system time"));

  if (!localtime_r(&tv.tv_sec, &serverLocalTime))
    return htsp_error(htsp, N_("Unable to get system local time"));
#if defined(HAS_GMTOFF)
  tz_offset = - serverLocalTime.tm_gmtoff / (60);
#else
  // NB: This will be a day out when GMT offsets >= 13hrs or <11 hrs apply
  struct tm serverGmTime;
  if (!gmtime_r(&tv.tv_sec, &serverGmTime))
    return htsp_error(htsp, N_("Unable to get system UTC time"));
  tz_offset = (serverGmTime.tm_hour - serverLocalTime.tm_hour) * 60;
  tz_offset += serverGmTime.tm_min - serverLocalTime.tm_min;
  if (tz_offset > 11 * 60)
    tz_offset -= 24 * 60;
  if (tz_offset <= -13 * 60)
    tz_offset += 24 * 60;
#endif

  out = htsmsg_create_map();
  htsmsg_add_s32(out, "time", tv.tv_sec);
  htsmsg_add_s32(out, "timezone", tz_offset/60);
  htsmsg_add_s32(out, "gmtoffset", -tz_offset);
  return out;
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
  dvr_autorec_entry_t *dae;
  dvr_timerec_entry_t *dte;
  htsmsg_t *m;
  uint32_t epg = 0;
  int64_t lastUpdate = -1;
  int64_t epgMaxTime = 0;
  const char *lang;

  /* Get optional flags, allow updating them if already in async mode */
  if (htsmsg_get_u32(in, "epg", &epg))
    epg = (htsp->htsp_async_mode & HTSP_ASYNC_EPG) ? 1 : 0;
  if (!htsmsg_get_s64(in, "lastUpdate", &lastUpdate))   // 0 = never
    htsp->htsp_epg_lastupdate = lastUpdate;
  else if (htsp->htsp_async_mode & HTSP_ASYNC_EPG)
    lastUpdate = htsp->htsp_epg_lastupdate;
  if (!htsmsg_get_s64(in, "epgMaxTime", &epgMaxTime)) { // 0 = unlimited window
    if (htsp->htsp_async_mode & HTSP_ASYNC_EPG) {
      /* Only allow to change the window in the correct range */
      if (htsp->htsp_epg_window && epgMaxTime > htsp->htsp_epg_lastupdate)
        htsp->htsp_epg_window = epgMaxTime-gclk();
    } else if (epgMaxTime > gclk()) {
      htsp->htsp_epg_window = epgMaxTime-gclk();
    } else {
      htsp->htsp_epg_window = 0;
    }
    if (htsp->htsp_epg_window)
      htsp->htsp_epg_window = MAX(htsp->htsp_epg_window, 600);
  }
  if ((lang = htsmsg_get_str(in, "language")) != NULL) {
    if (lang[0]) {
      htsp->htsp_language = strdup(lang);
    } else {
      free(htsp->htsp_language);
      htsp->htsp_language = NULL;
    }
  }

  /* First, just OK the async request */
  htsp_reply(htsp, in, htsmsg_create_map());

  /* Set epg */
  if(epg)
    htsp->htsp_async_mode |= HTSP_ASYNC_EPG;
  else
    htsp->htsp_async_mode &= ~HTSP_ASYNC_EPG;

  if(htsp->htsp_async_mode & HTSP_ASYNC_ON) {
    /* Sync epg on demand */
    if (epg)
      htsp_epg_send_waiting(htsp, lastUpdate);
    return NULL;
  }

  htsp->htsp_async_mode |= HTSP_ASYNC_ON;

  /* Send all enabled and external tags */
  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    if(channel_tag_access(ct, htsp->htsp_granted_access, 0))
      htsp_send_message(htsp, htsp_build_tag(htsp, ct, "tagAdd", 0), NULL);

  /* Send all channels */
  CHANNEL_FOREACH(ch)
    if (htsp_user_access_channel(htsp,ch))
      htsp_send_message(htsp, htsp_build_channel(ch, "channelAdd", htsp), NULL);

  /* Send all enabled and external tags (now with channel mappings) */
  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    if(channel_tag_access(ct, htsp->htsp_granted_access, 0))
      htsp_send_message(htsp, htsp_build_tag(htsp, ct, "tagUpdate", 1), NULL);

  /* Send all autorecs */
  TAILQ_FOREACH(dae, &autorec_entries, dae_link)
    if (!dvr_autorec_entry_verify(dae, htsp->htsp_granted_access, 1))
      htsp_send_message(htsp, htsp_build_autorecentry(htsp, dae, "autorecEntryAdd"), NULL);

  /* Send all timerecs */
  TAILQ_FOREACH(dte, &timerec_entries, dte_link)
    if (!dvr_timerec_entry_verify(dte, htsp->htsp_granted_access, 1))
      htsp_send_message(htsp, htsp_build_timerecentry(htsp, dte, "timerecEntryAdd"), NULL);

  /* Send all DVR entries */
  LIST_FOREACH(de, &dvrentries, de_global_link)
    if (!dvr_entry_verify(de, htsp->htsp_granted_access, 1))
      htsp_send_message(htsp, htsp_build_dvrentry(htsp, de, "dvrEntryAdd", htsp->htsp_language, 0), NULL);

  /* Send EPG updates */
  if (epg)
    htsp_epg_send_waiting(htsp, -1);

  /* Notify that initial sync has been completed */
  m = htsmsg_create_map();
  htsmsg_add_str(m, "method", "initialSyncCompleted");
  htsp_send_message(htsp, m, NULL);

  /* Insert in list so it will get all updates */
  LIST_INSERT_HEAD(&htsp_async_connections, htsp, htsp_async_link);

  return NULL;
}

/**
 * Get information about the given event
 */
static htsmsg_t *
htsp_method_getChannel(htsp_connection_t *htsp, htsmsg_t *in)
{
  uint32_t channelId;
  channel_t *ch = NULL;

  if (htsmsg_get_u32(in, "channelId", &channelId))
    return htsp_error(htsp, N_("Invalid arguments"));
  if (!(ch = channel_find_by_id(channelId)))
    return htsp_error(htsp, N_("Channel does not exist"));

  return htsp_build_channel(ch, NULL, htsp);
}

/**
 * Get information about the given event
 */
static htsmsg_t *
htsp_method_getEvent(htsp_connection_t *htsp, htsmsg_t *in)
{
  uint32_t eventId;
  epg_broadcast_t *e;
  const char *lang;

  if(htsmsg_get_u32(in, "eventId", &eventId))
    return htsp_error(htsp, N_("Invalid arguments"));
  lang = htsmsg_get_str(in, "language") ?: htsp->htsp_language;

  if((e = epg_broadcast_find_by_id(eventId)) == NULL)
    return htsp_error(htsp, N_("Event does not exist"));

  return htsp_build_event(e, NULL, lang, 0, htsp);
}

/**
 * Get information about the given event +
 * n following events
 */
static htsmsg_t *
htsp_method_getEvents(htsp_connection_t *htsp, htsmsg_t *in)
{
  uint32_t u32, numFollowing;
  int64_t maxTime = 0;
  htsmsg_t *out, *events;
  epg_broadcast_t *e = NULL;
  channel_t *ch = NULL;
  const char *lang;

  /* Optional fields */
  if (!htsmsg_get_u32(in, "channelId", &u32))
    if (!(ch = channel_find_by_id(u32)))
      return htsp_error(htsp, N_("Channel does not exist"));
  if (!htsmsg_get_u32(in, "eventId", &u32))
    if (!(e = epg_broadcast_find_by_id(u32)))
      return htsp_error(htsp, N_("Event does not exist"));

  /* Check access */

  numFollowing = htsmsg_get_u32_or_default(in, "numFollowing", 0);
  maxTime      = htsmsg_get_s64_or_default(in, "maxTime", 0);
  lang         = htsmsg_get_str(in, "language") ?: htsp->htsp_language;

  /* Use event as starting point */
  if (e || ch) {

    if (!e) e = ch->ch_epg_now ?: ch->ch_epg_next;

    if (e && !htsp_user_access_channel(htsp, e->channel))
      return htsp_error(htsp, N_("User does not have access"));

    /* Output */
    events = htsmsg_create_list();
    while (e) {
      if (maxTime && e->start > maxTime) break;
      htsmsg_add_msg(events, NULL, htsp_build_event(e, NULL, lang, 0, htsp));
      if (numFollowing == 1) break;
      if (numFollowing) numFollowing--;
      e = epg_broadcast_get_next(e);
    }

  /* All channels */
  } else {

    events = htsmsg_create_list();
    CHANNEL_FOREACH(ch) {
      int num = numFollowing;
      if (!htsp_user_access_channel(htsp, ch))
        continue;
      RB_FOREACH(e, &ch->ch_epg_schedule, sched_link) {
        if (maxTime && e->start > maxTime) break;
        htsmsg_add_msg(events, NULL, htsp_build_event(e, NULL, lang, 0, htsp));
        if (num == 1) break;
        if (num) num--;
      }
    }

  }

  /* Send */
  out = htsmsg_create_map();
  htsmsg_add_msg(out, "events", events);
  return out;
}

/**
 *
 * do an epg query
 */
static htsmsg_t *
htsp_method_epgQuery(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out, *array;
  const char *query;
  int i;
  uint32_t u32, full;
  channel_t *ch = NULL;
  channel_tag_t *ct = NULL;
  epg_query_t eq;
  const char *lang;
  int min_duration;
  int max_duration;
  char ubuf[UUID_HEX_SIZE];

  /* Required */
  if( (query = htsmsg_get_str(in, "query")) == NULL )
    return htsp_error(htsp, N_("Invalid arguments"));

  memset(&eq, 0, sizeof(eq));

  if(htsmsg_get_bool_or_default(in, "fulltext", 0))
    eq.fulltext = 1;
  eq.stitle = strdup(query);

  /* Optional */
  if(!(htsmsg_get_u32(in, "channelId", &u32))) {
    if (!(ch = channel_find_by_id(u32)))
      return htsp_error(htsp, N_("Channel does not exist"));
    else
      eq.channel = strdup(idnode_uuid_as_str(&ch->ch_id, ubuf));
  }
  if(!(htsmsg_get_u32(in, "tagId", &u32))) {
    if (!(ct = htsp_channel_tag_find_by_id(htsp, u32)))
      return htsp_error(htsp, N_("Channel tag does not exist"));
    else
      eq.channel_tag = strdup(idnode_uuid_as_str(&ct->ct_id, ubuf));
  }
  if (!htsmsg_get_u32(in, "contentType", &u32)) {
    if(htsp->htsp_version < 6) u32 <<= 4;
    eq.genre_count = 1;
    eq.genre = eq.genre_static;
    eq.genre[0] = u32;
  }
  lang = htsmsg_get_str(in, "language") ?: htsp->htsp_language;
  eq.lang = lang ? strdup(lang) : NULL;
  full = htsmsg_get_u32_or_default(in, "full", 0);

  min_duration = htsmsg_get_u32_or_default(in, "minduration", 0);
  max_duration = htsmsg_get_u32_or_default(in, "maxduration", INT_MAX);
  eq.duration.comp = EC_RG;
  eq.duration.val1 = min_duration;
  eq.duration.val2 = max_duration;
  tvhtrace(LS_HTSP, "min_duration %d and max_duration %d", min_duration, max_duration);

  /* Check access */
  if (ch && !htsp_user_access_channel(htsp, ch))
    return htsp_error(htsp, N_("User does not have access"));

  /* Query */
  epg_query(&eq, htsp->htsp_granted_access);

  /* Create Reply */
  out = htsmsg_create_map();
  if( eq.entries ) {
    array = htsmsg_create_list();
    for(i = 0; i < eq.entries; ++i) {
      if (full)
        htsmsg_add_msg(array, NULL,
                       htsp_build_event(eq.result[i], NULL, lang, 0, htsp));
      else
        htsmsg_add_u32(array, NULL, eq.result[i]->id);
    }
    htsmsg_add_msg(out, full ? "events" : "eventIds", array);
  }

  epg_query_free(&eq);

  return out;
}

static htsmsg_t *
htsp_method_getEpgObject(htsp_connection_t *htsp, htsmsg_t *in)
{
  uint32_t id, u32;
  epg_object_type_t type;
  epg_object_t *eo;
  htsmsg_t *out;

  // TODO: should really block access based on channel, but actually
  //       that's really hard to do here!

  /* Required fields */
  if (htsmsg_get_u32(in, "id", &id))
    return htsp_error(htsp, N_("Invalid arguments"));

  /* Optional fields */
  if (!htsmsg_get_u32(in, "type", &u32) && (u32 <= EPG_TYPEMAX))
    type = u32;
  else
    type = EPG_UNDEF;

  /* Get object */
  if (!(eo = epg_object_find_by_id(id, type)))
    return htsp_error(htsp, N_("Invalid EPG object request"));

  /* Serialize */
  if (!(out = epg_object_serialize(eo)))
    return htsp_error(htsp, N_("Internal error"));

  return out;
}

/**
 *
 */
static htsmsg_t *
htsp_method_getDvrConfigs(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out, *l, *c;
  htsmsg_field_t *f;
  dvr_config_t *cfg;
  const char *uuid, *s;
  char ubuf[UUID_HEX_SIZE];

  l = htsmsg_create_list();

  LIST_FOREACH(cfg, &dvrconfigs, config_link)
    if (cfg->dvr_enabled) {
      uuid = idnode_uuid_as_str(&cfg->dvr_id, ubuf);
      if (htsp->htsp_granted_access->aa_dvrcfgs) {
        HTSMSG_FOREACH(f, htsp->htsp_granted_access->aa_dvrcfgs) {
          if (!(s = htsmsg_field_get_str(f)))
            continue;
          if (strcmp(s, uuid) == 0)
            break;
        }
        if (f == NULL)
          continue;
      }
      c = htsmsg_create_map();
      htsmsg_add_str(c, "uuid", uuid);
      htsmsg_add_str(c, "name", cfg->dvr_config_name ?: "");
      htsmsg_add_str(c, "comment", cfg->dvr_comment ?: "");
      htsmsg_add_msg(l, NULL, c);
    }

  out = htsmsg_create_map();

  htsmsg_add_msg(out, "dvrconfigs", l);

  return out;
}

/**
 * add a Dvrentry
 */
static htsmsg_t *
htsp_method_addDvrEntry(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *conf, *out;
  dvr_config_t *dvr_conf;
  uint32_t eventid;
  epg_broadcast_t *e = NULL;
  dvr_entry_t *de;
  dvr_entry_sched_state_t dvr_status;
  const char *s, *lang;
  int64_t start, stop;
  uint32_t u32;
  channel_t *ch = NULL;

  if(!htsmsg_get_u32(in, "channelId", &u32))
    ch = channel_find_by_id(u32);
  if(!htsmsg_get_u32(in, "eventId", &eventid)) {
    e = epg_broadcast_find_by_id(eventid);
    ch = e ? e->channel : ch;
  }

  /* Check access */
  if (!htsp_user_access_channel(htsp, ch))
    return htsp_error(htsp, N_("User does not have access"));
  if (!ch)
    return htsp_error(htsp, N_("Channel does not exist"));

  /* Options */
  conf = htsmsg_create_map();
  htsmsg_copy_field(conf, "enabled", in, NULL);
  s = htsmsg_get_str(in, "configName");
  if (s) {
    dvr_conf = dvr_config_find_by_uuid(s);
    if (dvr_conf == NULL)
      dvr_conf = dvr_config_find_by_name(s);
    if (dvr_conf)
      htsmsg_add_uuid(conf, "config_name", &dvr_conf->dvr_id.in_uuid);
  }
  htsmsg_copy_field(conf, "start_extra", in, "startExtra");
  htsmsg_copy_field(conf, "stop_extra", in, "stopExtra");
  htsmsg_copy_field(conf, "pri", in, "priority");
  htsmsg_copy_field(conf, "retention", in, NULL);
  htsmsg_copy_field(conf, "removal", in, NULL);
  htsmsg_copy_field(conf, "comment", in, NULL);

  if (!(lang  = htsmsg_get_str(in, "language")))
    lang = htsp->htsp_language;

  /* Auth */
  s = htsp->htsp_granted_access->aa_username;
  htsmsg_add_str2(conf, "owner", s);
  s = htsp->htsp_granted_access->aa_representative;
  htsmsg_add_str2(conf, "creator", s);

  /* Manual timer */
  if (!e) {

    /* Required attributes */
    if (htsmsg_get_s64(in, "start", &start) ||
        htsmsg_get_s64(in, "stop", &stop) ||
        !(s = htsmsg_get_str(in, "title")) ||
        *s == '\0') {
      htsmsg_destroy(conf);
      return htsp_error(htsp, N_("Invalid arguments"));
    }

    htsmsg_add_uuid(conf, "channel", &ch->ch_id.in_uuid);
    htsmsg_add_s64(conf, "start", start);
    htsmsg_add_s64(conf, "stop", stop);
    lang_str_serialize_one(conf, "title", s, lang);

    /* Optional attributes */
    s = htsmsg_get_str(in, "subtitle");
    if (s)
      lang_str_serialize_one(conf, "subtitle", s, lang);
    s = htsmsg_get_str(in, "summary");
    if (s)
      lang_str_serialize_one(conf, "summary", s, lang);
    s = htsmsg_get_str(in, "description");
    if (s)
      lang_str_serialize_one(conf, "description", s, lang);
    u32 = htsmsg_get_u32_or_default(in, "ageRating", 0);
    if(u32)
      htsmsg_add_u32(conf, "age_rating", u32);
  }

  /* Create the dvr entry */
  de = dvr_entry_create_from_htsmsg(conf, e);

  htsmsg_destroy(conf);

  dvr_status = de != NULL ? de->de_sched_state : DVR_NOSTATE;

  /* Create response */
  out = htsmsg_create_map();

  switch(dvr_status) {
  case DVR_SCHEDULED:
  case DVR_RECORDING:
  case DVR_MISSED_TIME:
  case DVR_COMPLETED:
    htsmsg_add_u32(out, "id", idnode_get_short_uuid(&de->de_id));
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
 * Find DVR entry
 */
static dvr_entry_t *
htsp_findDvrEntry(htsp_connection_t *htsp, htsmsg_t *in, htsmsg_t **out, int readonly)
{
  uint32_t dvrEntryId;
  dvr_entry_t *de;

  if(htsmsg_get_u32(in, "id", &dvrEntryId)) {
    *out = htsp_error(htsp, N_("Invalid arguments"));
    return NULL;
  }

  if((de = dvr_entry_find_by_id(dvrEntryId)) == NULL) {
    *out = htsp_error(htsp, N_("DVR entry not found"));
    return NULL;
  }

  if(dvr_entry_verify(de, htsp->htsp_granted_access, readonly)) {
    *out = htsp_error(htsp, N_("User does not have access"));
    return NULL;
  }

  return de;
}


/**
 * update a Dvrentry
 */
static htsmsg_t *
htsp_method_updateDvrEntry(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out = NULL;
  uint32_t u32;
  dvr_entry_t *de;
  time_t start, stop, start_extra, stop_extra, priority;
  const char *dvr_config_name, *title, *subtitle, *summary, *desc, *lang, *comment;
  channel_t *channel = NULL;
  int enabled, retention, removal, playcount = -1, playposition = -1;
  int age_rating;
  ratinglabel_t *rating_label;

  de = htsp_findDvrEntry(htsp, in, &out, 0);
  if (de == NULL)
    return out;

  if(!htsmsg_get_u32(in, "channelId", &u32))
    channel = channel_find_by_id(u32);
  if (!channel)
    channel = de->de_channel;

  /* Check access new channel */
  if (channel && !htsp_user_access_channel(htsp, channel))
    return htsp_error(htsp, N_("User does not have access to channel"));

  enabled     = htsmsg_get_s64_or_default(in, "enabled",    -1);
  dvr_config_name = htsp_dvr_config_name(htsp, htsmsg_get_str(in, "configName"));
  start       = htsmsg_get_s64_or_default(in, "start",      0);
  stop        = htsmsg_get_s64_or_default(in, "stop",       0);
  start_extra = htsmsg_get_s64_or_default(in, "startExtra", 0);
  stop_extra  = htsmsg_get_s64_or_default(in, "stopExtra",  0);
  retention   = htsmsg_get_u32_or_default(in, "retention",  DVR_RET_REM_DVRCONFIG);
  removal     = htsmsg_get_u32_or_default(in, "removal",    DVR_RET_REM_DVRCONFIG);
  priority    = htsmsg_get_u32_or_default(in, "priority",   DVR_PRIO_NOTSET);
  age_rating  = htsmsg_get_u32_or_default(in, "ageRating",  0);
  rating_label = NULL;  //Rating labels not supported for manually created DVR entries
  title       = htsmsg_get_str(in, "title");
  subtitle    = htsmsg_get_str(in, "subtitle");
  summary     = htsmsg_get_str(in, "summary");
  desc        = htsmsg_get_str(in, "description");
  lang        = htsmsg_get_str(in, "language") ?: htsp->htsp_language;
  comment     = htsmsg_get_str(in, "comment");

  if(!htsmsg_get_u32(in, "playcount", &u32)) {
    if (u32 > INT_MAX)
      u32 = HTSP_DVR_PLAYCOUNT_INCR;
    switch (u32) {
      case HTSP_DVR_PLAYCOUNT_INCR:
        playcount = de->de_playcount + 1;
        break;
      case HTSP_DVR_PLAYCOUNT_SET:
        if (!de->de_playcount)
          playcount = 1;
        break;
      case HTSP_DVR_PLAYCOUNT_RESET:
        playcount = 0;
        break;
      case HTSP_DVR_PLAYCOUNT_KEEP:
        break;
      default:
        playcount = u32;
        break;
    }
  }
  if(!htsmsg_get_u32(in, "playposition", &u32))
    playposition = u32 > INT_MAX ? INT_MAX : u32;

  de = dvr_entry_update(de, enabled, dvr_config_name, channel, title, subtitle,
                        summary, desc, lang, start, stop, start_extra, stop_extra,
                        priority, retention, removal, playcount, playposition,
                        age_rating, rating_label, comment);

  return htsp_success();
}

/**
 * stop a Dvrentry
 */
static htsmsg_t *
htsp_method_stopDvrEntry(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out = NULL;
  dvr_entry_t *de;

  de = htsp_findDvrEntry(htsp, in, &out, 0);
  if (de == NULL)
    return out;

  dvr_entry_stop(de);

  return htsp_success();
}

/**
 * cancel a Dvrentry
 */
static htsmsg_t *
htsp_method_cancelDvrEntry(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out = NULL;
  dvr_entry_t *de;

  de = htsp_findDvrEntry(htsp, in, &out, 0);
  if (de == NULL)
    return out;

  dvr_entry_cancel(de, 0);

  return htsp_success();
}

/**
 * delete a Dvrentry
 */
static htsmsg_t *
htsp_method_deleteDvrEntry(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out;
  dvr_entry_t *de;

  de = htsp_findDvrEntry(htsp, in, &out, 0);
  if (de == NULL)
    return out;

  dvr_entry_cancel_remove(de, 0);

  return htsp_success();
}

/**
 * add a Dvr autorec entry
 */
static htsmsg_t *
htsp_method_addAutorecEntry(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out;
  dvr_autorec_entry_t *dae;
  const char *str;
  uint32_t u32;
  int64_t s64;
  channel_t *ch = NULL;
  char ubuf[UUID_HEX_SIZE];

  /* Options */
  if(!(str = htsmsg_get_str(in, "title")))
    return htsp_error(htsp, N_("Invalid arguments"));

  if (htsp->htsp_version > 24) {
    if (!htsmsg_get_s64(in, "channelId", &s64)) { // not sending or -1 = any channel
      if (s64 >= 0)
        ch = channel_find_by_id((uint32_t)s64);
    }
  }
  else {
    if (!htsmsg_get_u32(in, "channelId", &u32))   // not sending = any channel
      ch = channel_find_by_id(u32);
  }

  /* Check access channel */
  if (ch && !htsp_user_access_channel(htsp, ch))
    return htsp_error(htsp, N_("User does not have access"));

  /* Create autorec config from htsp and add */
  dae = dvr_autorec_create_htsp(htsp_serierec_convert(htsp, in, ch, 1, 1));

  /* create response */
  out = htsmsg_create_map();

  if (dae) {
    htsmsg_add_str(out, "id", idnode_uuid_as_str(&dae->dae_id, ubuf));
    htsmsg_add_u32(out, "success", 1);
  }
  else {
    htsmsg_add_str(out, "error", "Could not add autorec entry");
    htsmsg_add_u32(out, "success", 0);
  }
  return out;
}


/**
 * update a Dvr autorec entry
 */
static htsmsg_t *
htsp_method_updateAutorecEntry(htsp_connection_t *htsp, htsmsg_t *in)
{
  const char *daeId;
  dvr_autorec_entry_t *dae;
  int64_t s64;
  channel_t *ch = NULL;

  if (!(daeId = htsmsg_get_str(in, "id")))
    return htsp_error(htsp, N_("Invalid arguments"));

  if((dae = dvr_autorec_find_by_uuid(daeId)) == NULL)
    return htsp_error(htsp, N_("Automatic schedule entry not found"));

  if(dvr_autorec_entry_verify(dae, htsp->htsp_granted_access, 0))
    return htsp_error(htsp, N_("User does not have access"));

  /* Do we have a channel? No = keep old one */
  if (!htsmsg_get_s64(in, "channelId", &s64)) //s64 -> -1 = any channel
  {
    if (s64 >= 0)
      ch = channel_find_by_id((uint32_t)s64);

    /* Check access new channel */
    if (ch && !htsp_user_access_channel(htsp, ch))
      return htsp_error(htsp, N_("User does not have access"));
  }

  /* Update autorec config from htsp and save */
  dvr_autorec_update_htsp(dae, htsp_serierec_convert(htsp, in, ch, 1, 0));

  return htsp_success();
}


/**
 * delete a Dvr autorec entry
 */
static htsmsg_t *
htsp_method_deleteAutorecEntry(htsp_connection_t *htsp, htsmsg_t *in)
{
  const char *daeId;
  dvr_autorec_entry_t *dae;

  if (!(daeId = htsmsg_get_str(in, "id")))
    return htsp_error(htsp, N_("Invalid arguments"));

  if((dae = dvr_autorec_find_by_uuid(daeId)) == NULL)
    return htsp_error(htsp, N_("Automatic schedule entry not found"));

  if(dvr_autorec_entry_verify(dae, htsp->htsp_granted_access, 0))
    return htsp_error(htsp, N_("User does not have access"));

  autorec_destroy_by_id(daeId, 1);

  return htsp_success();
}

/**
 * add a Dvr timerec entry
 */
static htsmsg_t *
htsp_method_addTimerecEntry(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out;
  dvr_timerec_entry_t *dte;
  const char *str;
  channel_t *ch = NULL;
  uint32_t u32;
  int64_t s64;
  char ubuf[UUID_HEX_SIZE];

  /* Options */
  if(!(str = htsmsg_get_str(in, "title")))
    return htsp_error(htsp, N_("Invalid arguments"));

  if (htsp->htsp_version > 24) {
    if (!htsmsg_get_s64(in, "channelId", &s64)) { // not sending or -1 = any channel
      if (s64 >= 0)
        ch = channel_find_by_id((uint32_t)s64);
    }
  }
  else {
    if (!htsmsg_get_u32(in, "channelId", &u32))   // not sending = any channel
      ch = channel_find_by_id(u32);
  }

  /* Check access channel */
  if (ch && !htsp_user_access_channel(htsp, ch))
    return htsp_error(htsp, N_("User does not have access"));

  /* Create timerec config from htsp and add */
  dte = dvr_timerec_create_htsp(htsp_serierec_convert(htsp, in, ch, 0, 1));

  /* create response */
  out = htsmsg_create_map();

  if (dte) {
    htsmsg_add_str(out, "id", idnode_uuid_as_str(&dte->dte_id, ubuf));
    htsmsg_add_u32(out, "success", 1);
  }
  else {
    htsmsg_add_str(out, "error", "Could not add timerec entry");
    htsmsg_add_u32(out, "success", 0);
  }
  return out;
}

/**
 * update a Dvr timerec entry
 */
static htsmsg_t *
htsp_method_updateTimerecEntry(htsp_connection_t *htsp, htsmsg_t *in)
{
  const char *dteId;
  dvr_timerec_entry_t *dte;
  int64_t s64;
  channel_t *ch = NULL;

  if (!(dteId = htsmsg_get_str(in, "id")))
    return htsp_error(htsp, N_("Invalid arguments"));

  if((dte = dvr_timerec_find_by_uuid(dteId)) == NULL)
    return htsp_error(htsp, N_("Automatic time scheduler entry not found"));

  if(dvr_timerec_entry_verify(dte, htsp->htsp_granted_access, 0))
    return htsp_error(htsp, N_("User does not have access"));

  /* Do we have a channel? No = keep old one */
  if (!htsmsg_get_s64(in, "channelId", &s64)) //s64 -> -1 = any channel
  {
    if (s64 >= 0)
      ch = channel_find_by_id((uint32_t)s64);

    /* Check access new channel */
    if (ch && !htsp_user_access_channel(htsp, ch))
      return htsp_error(htsp, N_("User does not have access"));
  }

  /* Update timerec config from htsp and save */
  dvr_timerec_update_htsp(dte, htsp_serierec_convert(htsp, in, ch, 0, 0));

  return htsp_success();
}

/**
 * delete a Dvr timerec entry
 */
static htsmsg_t *
htsp_method_deleteTimerecEntry(htsp_connection_t *htsp, htsmsg_t *in)
{
  const char *dteId;
  dvr_timerec_entry_t *dte;

  if (!(dteId = htsmsg_get_str(in, "id")))
    return htsp_error(htsp, N_("Invalid arguments"));

  if((dte = dvr_timerec_find_by_uuid(dteId)) == NULL)
    return htsp_error(htsp, N_("Automatic time scheduler entry not found"));

  if(dvr_timerec_entry_verify(dte, htsp->htsp_granted_access, 0))
    return htsp_error(htsp, N_("User does not have access"));

  timerec_destroy_by_id(dteId, 1);

  return htsp_success();
}

/**
 * Return cutpoint data for a recording (if present).
 *
 * Request message fields:
 * id                 u32    required   DVR entry id
 *
 * Result message fields:
 * cutpoints          msg[]  optional   List of cutpoint entries, if a file is
 *                                      found and has some valid data.
 *
 * Cutpoint fields:
 * start              u32    required   Cut start time in ms.
 * end                u32    required   Cut end time in ms.
 * type               u32    required   Action type:
 *                                      0=Cut, 1=Mute, 2=Scene,
 *                                      3=Commercial break.
 **/
static htsmsg_t *
htsp_method_getDvrCutpoints(htsp_connection_t *htsp, htsmsg_t *in)
{
  uint32_t dvrEntryId;
  dvr_entry_t *de;
  if (htsmsg_get_u32(in, "id", &dvrEntryId))
    return htsp_error(htsp, N_("Invalid arguments"));

  if((de = dvr_entry_find_by_id(dvrEntryId)) == NULL)
    return htsp_error(htsp, N_("DVR schedule not found"));

  if(dvr_entry_verify(de, htsp->htsp_granted_access, 1))
    return htsp_error(htsp, N_("User does not have access"));

  htsmsg_t *msg = htsmsg_create_map();

  dvr_cutpoint_list_t *list = dvr_get_cutpoint_list(de);

  if (list != NULL) {
    htsmsg_t *cutpoint_list = htsmsg_create_list();
    dvr_cutpoint_t *cp;
    TAILQ_FOREACH(cp, list, dc_link) {
      htsmsg_t *cutpoint = htsmsg_create_map();
      htsmsg_add_u32(cutpoint, "start", cp->dc_start_ms);
      htsmsg_add_u32(cutpoint, "end", cp->dc_end_ms);
      htsmsg_add_u32(cutpoint, "type", cp->dc_type);

      htsmsg_add_msg(cutpoint_list, NULL, cutpoint);
    }
    htsmsg_add_msg(msg, "cutpoints", cutpoint_list);
  }

  // Cleanup...
  dvr_cutpoint_list_destroy(list);

  return msg;
}

/**
 * Request a ticket for a http url pointing to a channel or dvr
 */
static htsmsg_t *
htsp_method_getTicket(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out;
  uint32_t id;
  char path[255];
  const char *ticket = NULL;
  channel_t *ch;
  dvr_entry_t *de;

  if(!htsmsg_get_u32(in, "channelId", &id)) {
    if (!(ch = channel_find_by_id(id)))
      return htsp_error(htsp, N_("Channel not found"));
    if (!htsp_user_access_channel(htsp, ch))
      return htsp_error(htsp, N_("User does not have access"));

    snprintf(path, sizeof(path), "/stream/channelid/%d", id);
    ticket = access_ticket_create(path, htsp->htsp_granted_access);
  } else if(!htsmsg_get_u32(in, "dvrId", &id)) {
    if (!(de = dvr_entry_find_by_id(id)))
      return htsp_error(htsp, N_("DVR schedule does not exist"));
    if (!htsp_user_access_channel(htsp, de->de_channel))
      return htsp_error(htsp, N_("User does not have access"));

    snprintf(path, sizeof(path), "/dvrfile/%d", id);
    ticket = access_ticket_create(path, htsp->htsp_granted_access);
  } else {
    return htsp_error(htsp, N_("Invalid arguments"));
  }

  out = htsmsg_create_map();

  htsmsg_add_str(out, "path", path);
  htsmsg_add_str(out, "ticket", ticket);

  return out;
}

/*
 *
 */
static void _bytes_out_cb(void *aux)
{
  htsp_subscription_t *hs = aux;
  if (hs->hs_s) {
    subscription_add_bytes_out(hs->hs_s, atomic_exchange(&hs->hs_s_bytes_out, 0));
    mtimer_arm_rel(&hs->hs_s_bytes_out_timer, _bytes_out_cb, hs, ms2mono(200));
  }
}

/**
 * Request subscription for a channel
 */
static htsmsg_t *
htsp_method_subscribe(htsp_connection_t *htsp, htsmsg_t *in)
{
  uint32_t chid, sid, weight, req90khz, timeshiftPeriod = 0;
  const char *str, *profile_id;
  channel_t *ch;
  htsp_subscription_t *hs;
  profile_t *pro;

  if(htsmsg_get_u32(in, "subscriptionId", &sid))
    return htsp_error(htsp, N_("Invalid arguments"));

  if(!htsmsg_get_u32(in, "channelId", &chid)) {
    if((ch = channel_find_by_id(chid)) == NULL)
      return htsp_error(htsp, N_("Channel does not exist"));
  } else if((str = htsmsg_get_str(in, "channelName")) != NULL) {
    if((ch = channel_find_by_name(str)) == NULL)
      return htsp_error(htsp, N_("Channel does not exist"));
  } else {
    return htsp_error(htsp, N_("Invalid arguments"));
  }
  if (!htsp_user_access_channel(htsp, ch))
    return htsp_error(htsp, N_("User does not have access"));

  weight = htsmsg_get_u32_or_default(in, "weight", 0);
  req90khz = htsmsg_get_u32_or_default(in, "90khz", 0);

  profile_id = htsmsg_get_str(in, "profile");

#if ENABLE_TIMESHIFT
  if(ch->ch_remote_timeshift) {
    timeshiftPeriod = ~0;
  } else {
    if (timeshift_conf.enabled) {
      timeshiftPeriod = htsmsg_get_u32_or_default(in, "timeshiftPeriod", 0);
      if (!timeshift_conf.unlimited_period)
        timeshiftPeriod = MIN(timeshiftPeriod, timeshift_conf.max_period * 60);
    }
  }
#endif

  /* Initialize the HTSP subscription structure */

  hs = calloc(1, sizeof(htsp_subscription_t));

  hs->hs_htsp = htsp;
  hs->hs_90khz = req90khz;
  hs->hs_queue_depth = htsmsg_get_u32_or_default(in, "queueDepth",
						 HTSP_DEFAULT_QUEUE_DEPTH);
  htsp_init_queue(&hs->hs_q, 0);

  hs->hs_sid = sid;
  streaming_target_init(&hs->hs_input, &htsp_streaming_input_ops, hs, 0);

#if ENABLE_TIMESHIFT
  if (ch->ch_remote_timeshift) {
    tvhdebug(LS_HTSP, "using remote timeshift (RTSP)");
  } else {
    if (timeshiftPeriod != 0) {
      if (timeshiftPeriod == ~0)
        tvhdebug(LS_HTSP, "using timeshift buffer (unlimited)");
      else
        tvhdebug(LS_HTSP, "using timeshift buffer (%u mins)",
            timeshiftPeriod / 60);
    }
  }
#endif

  pro = profile_find_by_list(htsp->htsp_granted_access->aa_profiles, profile_id,
                             "htsp", SUBSCRIPTION_PACKET | SUBSCRIPTION_HTSP);
  profile_chain_init(&hs->hs_prch, pro, ch, 1);
  if (profile_chain_work(&hs->hs_prch, &hs->hs_input, timeshiftPeriod, ch->ch_remote_timeshift ?
      PROFILE_WORK_REMOTE_TS : PROFILE_WORK_NONE)) {
    tvherror(LS_HTSP, "unable to create profile chain '%s'", profile_get_name(pro));
    profile_chain_close(&hs->hs_prch);
    free(hs);
    return htsp_error(htsp, N_("Stream setup error"));
  }

  /*
   * We send the reply now to avoid the user getting the 'subscriptionStart'
   * async message before the reply to 'subscribe'.
   *
   * Send some optional boolean flags back to the subscriber so it can infer
   * if we support those
   *
   */
  htsmsg_t *rep = htsmsg_create_map();
  if(req90khz)
    htsmsg_add_u32(rep, "90khz", 1);
  if(hs->hs_prch.prch_sharer->prsh_tsfix)
    htsmsg_add_u32(rep, "normts", 1);
  if(hs->hs_s)
    htsmsg_add_u32(rep, "weight", hs->hs_s->ths_weight >= 0 ? hs->hs_s->ths_weight : 0);

#if ENABLE_TIMESHIFT
  if (ch->ch_remote_timeshift) {
    htsmsg_add_u32(rep, "timeshiftPeriod", timeshiftPeriod);
  } else {
    if (timeshiftPeriod)
      htsmsg_add_u32(rep, "timeshiftPeriod", timeshiftPeriod);
  }
#endif

  htsp_reply(htsp, in, rep);

  /*
   * subscribe now...
   */
  LIST_INSERT_HEAD(&htsp->htsp_subscriptions, hs, hs_link);

  tvhdebug(LS_HTSP, "%s - subscribe to %s using profile %s",
           htsp->htsp_logname, channel_get_name(ch, channel_blank_name),
           profile_get_name(pro));
  hs->hs_s = subscription_create_from_channel(&hs->hs_prch, NULL, weight,
					      htsp->htsp_logname,
					      SUBSCRIPTION_PACKET |
					      SUBSCRIPTION_STREAMING,
					      htsp->htsp_peername,
					      htsp->htsp_granted_access->aa_representative,
					      htsp->htsp_clientname,
					      NULL);
  if (hs->hs_s)
    mtimer_arm_rel(&hs->hs_s_bytes_out_timer, _bytes_out_cb, hs, ms2mono(200));
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
    return htsp_error(htsp, N_("Invalid arguments"));

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
    return htsp_error(htsp, N_("Invalid arguments"));

  weight = htsmsg_get_u32_or_default(in, "weight", 0);

  LIST_FOREACH(hs, &htsp->htsp_subscriptions, hs_link)
    if(hs->hs_sid == sid)
      break;

  if(hs == NULL)
    return htsp_error(htsp, N_("Subscription does not exist"));

  htsp_reply(htsp, in, htsmsg_create_map());

  subscription_change_weight(hs->hs_s, weight);
  return NULL;
}

/**
 * Skip stream
 */
static htsmsg_t *
htsp_method_skip(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsp_subscription_t *hs;
  uint32_t sid, abs;
  int64_t s64;
  streaming_skip_t skip;

  if(htsmsg_get_u32(in, "subscriptionId", &sid))
    return htsp_error(htsp, N_("Invalid arguments"));

  LIST_FOREACH(hs, &htsp->htsp_subscriptions, hs_link)
    if(hs->hs_sid == sid)
      break;

  if(hs == NULL)
    return htsp_error(htsp, N_("Subscription does not exist"));

  abs = htsmsg_get_u32_or_default(in, "absolute", 0);

  memset(&skip, 0, sizeof(skip));
  if(!htsmsg_get_s64(in, "time", &s64)) {
    skip.type = abs ? SMT_SKIP_ABS_TIME : SMT_SKIP_REL_TIME;
    skip.time = hs->hs_90khz ? s64 : ts_rescale_inv(s64, 1000000);
    tvhtrace(LS_HTSP_SUB, "skip: %s %"PRId64" (%s)", abs ? "abs" : "rel",
             skip.time, hs->hs_90khz ? "90kHz" : "1MHz");
  } else if (!htsmsg_get_s64(in, "size", &s64)) {
    skip.type = abs ? SMT_SKIP_ABS_SIZE : SMT_SKIP_REL_SIZE;
    skip.size = s64;
    tvhtrace(LS_HTSP_SUB, "skip: %s by size %"PRId64, abs ? "abs" : "rel", s64);
  } else {
    return htsp_error(htsp, N_("Invalid arguments"));
  }

  subscription_set_skip(hs->hs_s, &skip);

  htsp_reply(htsp, in, htsmsg_create_map());
  return NULL;
}

/*
 * Set stream speed
 */
static htsmsg_t *
htsp_method_speed(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsp_subscription_t *hs;
  uint32_t sid;
  int32_t speed;

  if(htsmsg_get_u32(in, "subscriptionId", &sid))
    return htsp_error(htsp, N_("Invalid arguments"));
  if(htsmsg_get_s32(in, "speed", &speed))
    return htsp_error(htsp, N_("Invalid arguments"));

  LIST_FOREACH(hs, &htsp->htsp_subscriptions, hs_link)
    if(hs->hs_sid == sid)
      break;

  if(hs == NULL)
    return htsp_error(htsp, N_("Subscription does not exist"));

  tvhtrace(LS_HTSP_SUB, "speed: %d", speed);
  subscription_set_speed(hs->hs_s, speed);

  htsp_reply(htsp, in, htsmsg_create_map());
  return NULL;
}

/*
 * Revert to live
 */
static htsmsg_t *
htsp_method_live(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsp_subscription_t *hs;
  uint32_t sid;
  streaming_skip_t skip;

  if(htsmsg_get_u32(in, "subscriptionId", &sid))
    return htsp_error(htsp, N_("Invalid arguments"));

  LIST_FOREACH(hs, &htsp->htsp_subscriptions, hs_link)
    if(hs->hs_sid == sid)
      break;

  if(hs == NULL)
    return htsp_error(htsp, N_("Subscription does not exist"));

  memset(&skip, 0, sizeof(skip));
  skip.type = SMT_SKIP_LIVE;
  tvhtrace(LS_HTSP_SUB, "live");
  subscription_set_skip(hs->hs_s, &skip);

  htsp_reply(htsp, in, htsmsg_create_map());
  return NULL;
}

/**
 * Change filters for a subscription
 */
static htsmsg_t *
htsp_method_filter_stream(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsp_subscription_t *hs;
  uint32_t sid;
  htsmsg_t *l;
  if(htsmsg_get_u32(in, "subscriptionId", &sid))
    return htsp_error(htsp, N_("Invalid arguments"));

  LIST_FOREACH(hs, &htsp->htsp_subscriptions, hs_link)
    if(hs->hs_sid == sid)
      break;

  if(hs == NULL)
    return htsp_error(htsp, N_("Subscription does not exist"));

  if((l = htsmsg_get_list(in, "enable")) != NULL) {
    htsmsg_field_t *f;
    HTSMSG_FOREACH(f, l) {
      if(f->hmf_type == HMF_S64)
        htsp_enable_stream(hs, f->hmf_s64);
    }
  }

  if((l = htsmsg_get_list(in, "disable")) != NULL) {
    htsmsg_field_t *f;
    HTSMSG_FOREACH(f, l) {
      if(f->hmf_type == HMF_S64)
        htsp_disable_stream(hs, f->hmf_s64);
    }
  }
  return htsmsg_create_map();
}


/**
 * Open file
 */
static htsmsg_t *
htsp_method_file_open(htsp_connection_t *htsp, htsmsg_t *in)
{
  const char *str, *s2;
  const char *filename = NULL;
  char buf[PATH_MAX];

  if((str = htsmsg_get_str(in, "file")) == NULL)
    return htsp_error(htsp, N_("Invalid arguments"));

  // optional leading slash
  if (*str == '/')
    str++;

  if((s2 = tvh_strbegins(str, "dvr/")) != NULL ||
     (s2 = tvh_strbegins(str, "dvrfile/")) != NULL) {
    dvr_entry_t *de = dvr_entry_find_by_id(atoi(s2));
    if(de == NULL)
      return htsp_error(htsp, N_("DVR schedule does not exist"));

    if (dvr_entry_verify(de, htsp->htsp_granted_access, 1))
      return htsp_error(htsp, N_("User does not have access"));

    filename = dvr_get_filename(de);

    if (filename == NULL)
      return htsp_error(htsp, N_("DVR schedule does not have a file yet"));

    return htsp_file_open(htsp, filename, 0, de);

  } else if ((s2 = tvh_strbegins(str, "imagecache/")) != NULL) {
    int r, fd = -1;
    tvh_mutex_unlock(&global_lock);
    r = imagecache_filename(atoi(s2), buf, sizeof(buf));
    tvh_mutex_lock(&global_lock);
    if (r == 0)
      fd = tvh_open(buf, O_RDONLY, 0);
    if (fd < 0)
      return htsp_error(htsp, N_("Failed to open image"));
    return htsp_file_open(htsp, str, fd, NULL);

  } else {
    return htsp_error(htsp, N_("Unknown file"));
  }
}

/**
 *
 */
static htsmsg_t *
htsp_method_file_read(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsp_file_t *hf = htsp_file_find(htsp, in);
  htsmsg_t *rep = NULL;
  const char *e = NULL;
  int64_t off;
  int64_t size;
  int fd;

  if(hf == NULL)
    return htsp_error(htsp, N_("Invalid file"));

  if(htsmsg_get_s64(in, "size", &size))
    return htsp_error(htsp, N_("Invalid parameters"));

  fd = hf->hf_fd;

  tvh_mutex_unlock(&global_lock);

  /* Seek (optional) */
  if (!htsmsg_get_s64(in, "offset", &off))
    if(lseek(fd, off, SEEK_SET) != off) {
      e = "Seek error";
      goto error;
    }

  /* Read */
  void *m = malloc(size);
  if(m == NULL) {
    e = N_("Not enough memory");
    goto error;
  }

  int r = read(fd, m, size);
  if(r < 0) {
    free(m);
    e = N_("Read error");
    goto error;
  }

  htsp_file_update_stats(hf, r);

  rep = htsmsg_create_map();
  htsmsg_add_bin_alloc(rep, "data", m, r);

error:
  tvh_mutex_lock(&global_lock);
  return e ? htsp_error(htsp, e) : rep;
}

/**
 *
 */
static htsmsg_t *
htsp_method_file_close(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsp_file_t *hf = htsp_file_find(htsp, in);
  uint32_t u32;
  dvr_entry_t *de;

  if(hf == NULL)
    return htsp_error(htsp, N_("Invalid file"));

  if (hf->hf_de_id > 0 && (de = dvr_entry_find_by_id(hf->hf_de_id)))
  {
    int save = 0;
    /* Only allow incrementing playcount on file close, the rest can be done with "updateDvrEntry" */
    if (htsp->htsp_version < 27 || htsmsg_get_u32_or_default(in, "playcount", HTSP_DVR_PLAYCOUNT_INCR) == HTSP_DVR_PLAYCOUNT_INCR) {
      dvr_entry_incr_playcount(de);
      save = 1;
    }
    if(htsp->htsp_version >= 27 && !htsmsg_get_u32(in, "playposition", &u32)) {
      de->de_playposition = u32;
      save = 1;
    }
    if (save)
      dvr_entry_changed(de);
  }

  tvh_mutex_unlock(&global_lock);
  htsp_file_destroy(hf);
  tvh_mutex_lock(&global_lock);
  return htsmsg_create_map();
}

/**
 *
 */
static htsmsg_t *
htsp_method_file_stat(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsp_file_t *hf = htsp_file_find(htsp, in);
  htsmsg_t *rep;
  struct stat st;
  int fd;

  if(hf == NULL)
    return htsp_error(htsp, N_("Invalid file"));

  fd = hf->hf_fd;

  tvh_mutex_unlock(&global_lock);
  rep = htsmsg_create_map();
  if(!fstat(fd, &st)) {
    htsmsg_add_s64(rep, "size", st.st_size);
    htsmsg_add_s64(rep, "mtime", st.st_mtime);
  }
  tvh_mutex_lock(&global_lock);

  return rep;
}

/**
 *
 */
static htsmsg_t *
htsp_method_file_seek(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsp_file_t *hf = htsp_file_find(htsp, in);
  htsmsg_t *rep;
  const char *str;
  int64_t off;
  int fd, whence;

  if(hf == NULL)
    return htsp_error(htsp, N_("Invalid file"));

  if (htsmsg_get_s64(in, "offset", &off))
    return htsp_error(htsp, N_("Invalid parameters"));

  if ((str = htsmsg_get_str(in, "whence"))) {
    if (!strcmp(str, "SEEK_SET"))
      whence = SEEK_SET;
    else if (!strcmp(str, "SEEK_CUR"))
      whence = SEEK_CUR;
    else if (!strcmp(str, "SEEK_END"))
      whence = SEEK_END;
    else
      return htsp_error(htsp, N_("Invalid parameters"));
  } else {
    whence = SEEK_SET;
  }

  fd = hf->hf_fd;
  tvh_mutex_unlock(&global_lock);

  if ((off = lseek(fd, off, whence)) < 0) {
    tvh_mutex_lock(&global_lock);
    return htsp_error(htsp, N_("Seek error"));
  }

  rep = htsmsg_create_map();
  htsmsg_add_s64(rep, "offset", off);

  tvh_mutex_lock(&global_lock);
  return rep;
}

/**
 *
 */
static htsmsg_t *
htsp_method_getProfiles(htsp_connection_t *htsp, htsmsg_t *in)
{
  htsmsg_t *out, *l;

  l = htsmsg_create_list();
  profile_get_htsp_list(l, htsp->htsp_granted_access->aa_profiles);

  out = htsmsg_create_map();

  htsmsg_add_msg(out, "profiles", l);

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
  { "hello",                    htsp_method_hello,              ACCESS_ANONYMOUS},
  { "authenticate",             htsp_method_authenticate,       ACCESS_ANONYMOUS},
  { "api",                      htsp_method_api,                ACCESS_ANONYMOUS},
  { "getDiskSpace",             htsp_method_getDiskSpace,       ACCESS_HTSP_STREAMING},
  { "getSysTime",               htsp_method_getSysTime,         ACCESS_HTSP_STREAMING},
  { "enableAsyncMetadata",      htsp_method_async,              ACCESS_HTSP_STREAMING},
  { "getChannel",               htsp_method_getChannel,         ACCESS_HTSP_STREAMING},
  { "getEvent",                 htsp_method_getEvent,           ACCESS_HTSP_STREAMING},
  { "getEvents",                htsp_method_getEvents,          ACCESS_HTSP_STREAMING},
  { "epgQuery",                 htsp_method_epgQuery,           ACCESS_HTSP_STREAMING},
  { "getEpgObject",             htsp_method_getEpgObject,       ACCESS_HTSP_STREAMING},
  { "getDvrConfigs",            htsp_method_getDvrConfigs,      ACCESS_HTSP_RECORDER},
  { "addDvrEntry",              htsp_method_addDvrEntry,        ACCESS_HTSP_RECORDER},
  { "updateDvrEntry",           htsp_method_updateDvrEntry,     ACCESS_HTSP_RECORDER},
  { "stopDvrEntry",             htsp_method_stopDvrEntry,       ACCESS_HTSP_RECORDER},
  { "cancelDvrEntry",           htsp_method_cancelDvrEntry,     ACCESS_HTSP_RECORDER},
  { "deleteDvrEntry",           htsp_method_deleteDvrEntry,     ACCESS_HTSP_RECORDER},
  { "addAutorecEntry",          htsp_method_addAutorecEntry,    ACCESS_HTSP_RECORDER},
  { "updateAutorecEntry",       htsp_method_updateAutorecEntry, ACCESS_HTSP_RECORDER},
  { "deleteAutorecEntry",       htsp_method_deleteAutorecEntry, ACCESS_HTSP_RECORDER},
  { "addTimerecEntry",          htsp_method_addTimerecEntry,    ACCESS_HTSP_RECORDER},
  { "updateTimerecEntry",       htsp_method_updateTimerecEntry, ACCESS_HTSP_RECORDER},
  { "deleteTimerecEntry",       htsp_method_deleteTimerecEntry, ACCESS_HTSP_RECORDER},
  { "getDvrCutpoints",          htsp_method_getDvrCutpoints,    ACCESS_HTSP_RECORDER},
  { "getTicket",                htsp_method_getTicket,          ACCESS_HTSP_STREAMING},
  { "subscribe",                htsp_method_subscribe,          ACCESS_HTSP_STREAMING},
  { "unsubscribe",              htsp_method_unsubscribe,        ACCESS_HTSP_STREAMING},
  { "subscriptionChangeWeight", htsp_method_change_weight,      ACCESS_HTSP_STREAMING},
  { "subscriptionSeek",         htsp_method_skip,               ACCESS_HTSP_STREAMING},
  { "subscriptionSkip",         htsp_method_skip,               ACCESS_HTSP_STREAMING},
  { "subscriptionSpeed",        htsp_method_speed,              ACCESS_HTSP_STREAMING},
  { "subscriptionLive",         htsp_method_live,               ACCESS_HTSP_STREAMING},
  { "subscriptionFilterStream", htsp_method_filter_stream,      ACCESS_HTSP_STREAMING},
  { "getProfiles",              htsp_method_getProfiles,        ACCESS_HTSP_STREAMING},
  { "fileOpen",                 htsp_method_file_open,          ACCESS_HTSP_RECORDER},
  { "fileRead",                 htsp_method_file_read,          ACCESS_HTSP_RECORDER},
  { "fileClose",                htsp_method_file_close,         ACCESS_HTSP_RECORDER},
  { "fileStat",                 htsp_method_file_stat,          ACCESS_HTSP_RECORDER},
  { "fileSeek",                 htsp_method_file_seek,          ACCESS_HTSP_RECORDER},
};

#define NUM_METHODS (sizeof(htsp_methods) / sizeof(htsp_methods[0]))

/* **************************************************************************
 * Message processing
 * *************************************************************************/

/**
 *
 */
struct htsp_verify_struct {
  const uint8_t *digest;
  const uint8_t *challenge;
};

static int
htsp_verify_callback(void *aux, const char *passwd)
{
  struct htsp_verify_struct *v = aux;
  uint8_t d[20];

  if (v->digest == NULL || v->challenge == NULL) return 0;
  sha1_calc(d, (uint8_t *)passwd, strlen(passwd), v->challenge, 32);
  return memcmp(d, v->digest, 20) == 0;
}

/**
 * Raise privs by field in message
 */
static int
htsp_authenticate(htsp_connection_t *htsp, htsmsg_t *m)
{
  struct htsp_verify_struct vs;
  const char *username;
  const void *digest;
  size_t digestlen;
  access_t *rights;
  int privgain = 0;

  if((username = htsmsg_get_str(m, "username")) == NULL)
    return 0;

  if(!htsmsg_get_bin(m, "digest", &digest, &digestlen)) {

    vs.digest = digest;
    vs.challenge = htsp->htsp_challenge;
    rights = access_get(htsp->htsp_peer, username,
                        htsp_verify_callback, &vs);

    if (rights->aa_rights == 0) {
      tvhinfo(LS_HTSP, "%s: Unauthorized access", htsp->htsp_logname);
      access_destroy(rights);
      return 0;
    }

    rights->aa_rights |= ACCESS_HTSP_INTERFACE;
    privgain = (rights->aa_rights |
                htsp->htsp_granted_access->aa_rights) !=
                  htsp->htsp_granted_access->aa_rights;

    tvhinfo(LS_HTSP, "%s: Identified as user '%s'",
	    htsp->htsp_logname, username);
    tvh_str_update(&htsp->htsp_username, username);
    htsp_update_logname(htsp);
    if(privgain)
      tvhinfo(LS_HTSP, "%s: Privileges updated", htsp->htsp_logname);

    access_destroy(htsp->htsp_granted_access);
    htsp->htsp_granted_access = rights;

    if (htsp->htsp_language == NULL && rights->aa_lang)
      htsp->htsp_language = strdup(rights->aa_lang);

  } else {

    tvhinfo(LS_HTSP, "%s: Identified as user '%s' (unverified)",
	    htsp->htsp_logname, username);
    tvh_str_update(&htsp->htsp_username, username);
    htsp_update_logname(htsp);

  }

  notify_reload("connections");

  return privgain;
}

/**
 * timeout is in ms, 0 means infinite timeout
 */
static int
htsp_read_message(htsp_connection_t *htsp, htsmsg_t **mp, int timeout)
{
  int v;
  uint32_t len;
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

  /* buf will be tied to the message (on success) */
  /* bellow fcn calls free(buf) (on failure) */
  *mp = htsmsg_binary_deserialize0(buf, len, buf);
  if(*mp == NULL)
    return EBADMSG;

  return 0;
}

/*
 * Status callback
 */
static void
htsp_server_status ( void *opaque, htsmsg_t *m )
{
  htsp_connection_t *htsp = opaque;
  access_t *aa;
  char buf[128];
  htsmsg_add_str(m, "type", "HTSP");
  if (htsp->htsp_username) {
    aa = htsp->htsp_granted_access;
    if (!strcmp(htsp->htsp_username, aa->aa_username ?: ""))
      snprintf(buf, sizeof(buf), "%s", htsp->htsp_username);
    else
      snprintf(buf, sizeof(buf), "[%s]", htsp->htsp_username);
    htsmsg_add_str(m, "user", buf);
  }
}

/**
 *
 */
static int
htsp_read_loop(htsp_connection_t *htsp)
{
  htsmsg_t *m = NULL, *reply;
  int run = 1, r = 0, i, streaming = 0;
  const char *method;
  void *tcp_id = NULL;;

  if(htsp_generate_challenge(htsp)) {
    tvherror(LS_HTSP, "%s: Unable to generate challenge",
	     htsp->htsp_logname);
    return 1;
  }

  tvh_mutex_lock(&global_lock);

  htsp->htsp_granted_access = access_get_by_addr(htsp->htsp_peer);
  htsp->htsp_granted_access->aa_rights |= ACCESS_HTSP_INTERFACE;

  tcp_id = tcp_connection_launch(htsp->htsp_fd, streaming, htsp_server_status,
                                 htsp->htsp_granted_access);

  tvh_mutex_unlock(&global_lock);

  if (tcp_id == NULL)
    return 0;

  tvhinfo(LS_HTSP, "Got connection from %s", htsp->htsp_logname);

  /* Session main loop */

  while(run && tvheadend_is_running()) {
readmsg:
    reply = NULL;

    if((r = htsp_read_message(htsp, &m, 0)) != 0)
      break;

    tvh_mutex_lock(&global_lock);
    if (htsp_authenticate(htsp, m)) {
      tcp_connection_land(tcp_id);
      tcp_id = tcp_connection_launch(htsp->htsp_fd, streaming, htsp_server_status,
                                     htsp->htsp_granted_access);
      if (tcp_id == NULL) {
        reply = htsmsg_create_map();
        htsmsg_add_u32(reply, "noaccess", 1);
        htsmsg_add_u32(reply, "connlimit", 1);
        run = 0;
        goto send_reply_with_unlock;
      }
    }

    if((method = htsmsg_get_str(m, "method")) != NULL) {
      tvhtrace(LS_HTSP, "%s - method %s", htsp->htsp_logname, method);
      if (tvhtrace_enabled())
        htsp_trace(htsp, LS_HTSP_REQ, "request", m);
      for(i = 0; i < NUM_METHODS; i++) {
        if(!strcmp(method, htsp_methods[i].name)) {

          if((htsp->htsp_granted_access->aa_rights &
              htsp_methods[i].privmask) !=
                htsp_methods[i].privmask) {

      	    tvh_mutex_unlock(&global_lock);
            /* Classic authentication failed delay */
            tvh_safe_usleep(250000);

            reply = htsmsg_create_map();
            htsmsg_add_u32(reply, "noaccess", 1);
            htsp_reply(htsp, m, reply);

            htsmsg_destroy(m);
            goto readmsg;

          } else {
            if (!strcmp(method, "subscribe") && !streaming) {
              tcp_connection_land(tcp_id);
              tcp_id = tcp_connection_launch(htsp->htsp_fd, 1, htsp_server_status,
                                             htsp->htsp_granted_access);
              if (tcp_id == NULL) {
                reply = htsmsg_create_map();
                htsmsg_add_u32(reply, "noaccess", 1);
                htsmsg_add_u32(reply, "connlimit", 1);
                goto send_reply_with_unlock;
              }
              streaming = 1;
            }
            reply = htsp_methods[i].fn(htsp, m);
          }
          break;
        }
      }

      if(i == NUM_METHODS) {
        reply = htsp_error(htsp, N_("Method not found"));
      }

    } else {
      reply = htsp_error(htsp, N_("Invalid arguments"));
    }

send_reply_with_unlock:
    tvh_mutex_unlock(&global_lock);

    if(reply != NULL) /* Methods can do all the replying inline */
      htsp_reply(htsp, m, reply);

    htsmsg_destroy(m);
  }

  tvh_mutex_lock(&global_lock);
  tcp_connection_land(tcp_id);
  tvh_mutex_unlock(&global_lock);
  return tvheadend_is_running() ? r : 0;
}

/**
 *
 */
static void *
htsp_write_scheduler(void *aux)
{
  htsp_connection_t *htsp = aux;
  htsp_msg_q_t *hmq;
  htsp_msg_t *hm;
  void *dptr;
  size_t dlen;
  int r;

  tvh_mutex_lock(&htsp->htsp_out_mutex);

  while(htsp->htsp_writer_run) {

    if((hmq = TAILQ_FIRST(&htsp->htsp_active_output_queues)) == NULL) {
      /* Nothing to be done, go to sleep */
      tvh_cond_wait(&htsp->htsp_out_cond, &htsp->htsp_out_mutex);
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

    tvh_mutex_unlock(&htsp->htsp_out_mutex);

    if (htsmsg_binary_serialize(hm->hm_msg, &dptr, &dlen, INT32_MAX) != 0) {
      tvhwarn(LS_HTSP, "%s: failed to serialize data", htsp->htsp_logname);
      htsp_msg_destroy(hm);
      tvh_mutex_lock(&htsp->htsp_out_mutex);
      continue;
    }

    htsp_msg_destroy(hm);

    r = tvh_write(htsp->htsp_fd, dptr, dlen);
    free(dptr);
    tvh_mutex_lock(&htsp->htsp_out_mutex);

    if (r) {
      tvhinfo(LS_HTSP, "%s: Write error -- %s",
              htsp->htsp_logname, strerror(errno));
      break;
    }
  }
  // Shutdown socket to make receive thread terminate entire HTSP connection

  shutdown(htsp->htsp_fd, SHUT_RDWR);
  tvh_mutex_unlock(&htsp->htsp_out_mutex);
  return NULL;
}

/**
 *
 */
static void
htsp_serve(int fd, void **opaque, struct sockaddr_storage *source,
	   struct sockaddr_storage *self)
{
  htsp_connection_t htsp;
  char buf[50];
  htsp_subscription_t *s;

  // Note: global_lock held on entry

  if (config.dscp >= 0)
    socket_set_dscp(fd, config.dscp, NULL, 0);

  tcp_get_str_from_ip(source, buf, 50);

  memset(&htsp, 0, sizeof(htsp_connection_t));
  *opaque = &htsp;

  TAILQ_INIT(&htsp.htsp_active_output_queues);

  htsp_init_queue(&htsp.htsp_hmq_ctrl, 0);
  htsp_init_queue(&htsp.htsp_hmq_qstatus, 1);
  htsp_init_queue(&htsp.htsp_hmq_epg, 0);

  htsp.htsp_peername = strdup(buf);
  htsp_update_logname(&htsp);

  htsp.htsp_fd = fd;
  htsp.htsp_peer = source;
  htsp.htsp_writer_run = 1;

  tvh_mutex_init(&htsp.htsp_out_mutex, NULL);

  LIST_INSERT_HEAD(&htsp_connections, &htsp, htsp_link);
  tvh_mutex_unlock(&global_lock);

  tvh_thread_create(&htsp.htsp_writer_thread, NULL,
                    htsp_write_scheduler, &htsp, "htsp-write");

  /**
   * Reader loop
   */

  htsp_read_loop(&htsp);

  tvhinfo(LS_HTSP, "%s: Disconnected", htsp.htsp_logname);

  /**
   * Ok, we're back, other end disconnected. Clean up stuff.
   */

  tvh_mutex_lock(&global_lock);

  /* no async notifications from now */
  if(htsp.htsp_async_mode)
    LIST_REMOVE(&htsp, htsp_async_link);

  mtimer_disarm(&htsp.htsp_epg_timer);

  /* deregister this client */
  LIST_REMOVE(&htsp, htsp_link);

  /* Beware! Closing subscriptions will invoke a lot of callbacks
     down in the streaming code. So we do this as early as possible
     to avoid any weird lockups */
  while((s = LIST_FIRST(&htsp.htsp_subscriptions)) != NULL)
    htsp_subscription_destroy(&htsp, s);

  tvh_mutex_unlock(&global_lock);

  tvh_mutex_lock(&htsp.htsp_out_mutex);
  htsp.htsp_writer_run = 0;
  tvh_cond_signal(&htsp.htsp_out_cond, 0);
  tvh_mutex_unlock(&htsp.htsp_out_mutex);

  pthread_join(htsp.htsp_writer_thread, NULL);

  while((s = LIST_FIRST(&htsp.htsp_dead_subscriptions)) != NULL)
    htsp_subscription_free(&htsp, s);

  htsp_msg_q_t *hmq;

  TAILQ_FOREACH(hmq, &htsp.htsp_active_output_queues, hmq_link) {
    htsp_msg_t *hm;
    while((hm = TAILQ_FIRST(&hmq->hmq_q)) != NULL) {
      TAILQ_REMOVE(&hmq->hmq_q, hm, hm_link);
      htsp_msg_destroy(hm);
    }
  }

  htsp_file_t *hf;
  while((hf = LIST_FIRST(&htsp.htsp_files)) != NULL)
    htsp_file_destroy(hf);

  close(fd);

  /* Free memory (leave lock in place, for parent method) */
  tvh_mutex_lock(&global_lock);
  free(htsp.htsp_logname);
  free(htsp.htsp_peername);
  free(htsp.htsp_username);
  free(htsp.htsp_clientname);
  free(htsp.htsp_language);
  access_destroy(htsp.htsp_granted_access);
  *opaque = NULL;
}

/*
 * Cancel callback
 */
static void
htsp_server_cancel ( void *opaque )
{
  htsp_connection_t *htsp = opaque;

  if (htsp)
    shutdown(htsp->htsp_fd, SHUT_RDWR);
}

/**
 *  Fire up HTSP server
 */
void
htsp_init(const char *bindaddr)
{
  extern int tvheadend_htsp_port_extra;
  static tcp_server_ops_t ops = {
    .start  = htsp_serve,
    .stop   = NULL,
    .cancel = htsp_server_cancel
  };
  if (tvheadend_htsp_port > 0)
    htsp_server = tcp_server_create(LS_HTSP, "HTSP", bindaddr, tvheadend_htsp_port, &ops, NULL);
  if (tvheadend_htsp_port_extra > 0)
    htsp_server_2 = tcp_server_create(LS_HTSP, "HTSP2", bindaddr, tvheadend_htsp_port_extra, &ops, NULL);
}

/*
 *
 */
void
htsp_register(void)
{
  if (htsp_server)
    tcp_server_register(htsp_server);
  if (htsp_server_2)
    tcp_server_register(htsp_server_2);
}

/**
 *  Fire down HTSP server
 */
void
htsp_done(void)
{
  tvh_mutex_lock(&global_lock);
  if (htsp_server_2)
    tcp_server_delete(htsp_server_2);
  if (htsp_server)
    tcp_server_delete(htsp_server);
  tvh_mutex_unlock(&global_lock);
}

/* **************************************************************************
 * Asynchronous updates
 * *************************************************************************/

/**
 *
 */
static void
htsp_async_send(htsmsg_t *m, int mode, void *aux)
{
  htsp_connection_t *htsp;

  lock_assert(&global_lock);
  LIST_FOREACH(htsp, &htsp_async_connections, htsp_async_link)
    if (htsp->htsp_async_mode & mode)
      htsp_send_message(htsp, htsmsg_copy(m), NULL);
  htsmsg_destroy(m);
}

/**
 *
 */
typedef htsmsg_t *(*http_async_send_cb_t)(htsp_connection_t *htsp, void *aux);

static void
htsp_async_send_cb(http_async_send_cb_t cb, int mode, void *aux)
{
  htsp_connection_t *htsp;
  htsmsg_t *m;

  lock_assert(&global_lock);
  LIST_FOREACH(htsp, &htsp_async_connections, htsp_async_link)
    if (htsp->htsp_async_mode & mode) {
      m = cb(htsp, aux);
      if (m != NULL)
        htsp_send_message(htsp, m, NULL);
    }
}

/**
 * Called from channel.c when a new channel is created
 */
static void
_htsp_channel_update(channel_t *ch, const char *method, htsmsg_t *msg)
{
  htsp_connection_t *htsp;
  LIST_FOREACH(htsp, &htsp_async_connections, htsp_async_link) {
    if (htsp->htsp_async_mode & HTSP_ASYNC_ON)
      if (htsp_user_access_channel(htsp,ch)) {
        htsmsg_t *m = msg ? htsmsg_copy(msg)
                        : htsp_build_channel(ch, method, htsp);
        htsp_send_message(htsp, m, NULL);
      }
  }
  htsmsg_destroy(msg);
}

/**
 * EPG subsystem calls this function when the current/next event
 * changes for a channel, e may be NULL if there is no current event.
 *
 * global_lock is held
 */
void
htsp_channel_update_nownext(channel_t *ch)
{
  epg_broadcast_t *now, *next;
  htsmsg_t *m;

  m = htsmsg_create_map();
  htsmsg_add_str(m, "method", "channelUpdate");
  htsmsg_add_u32(m, "channelId", channel_get_id(ch));

  now  = ch->ch_epg_now;
  next = ch->ch_epg_next;
  htsmsg_add_u32(m, "eventId",     now  ? now->id : 0);
  htsmsg_add_u32(m, "nextEventId", next ? next->id : 0);
  _htsp_channel_update(ch, NULL, m);
}

void
htsp_channel_add(channel_t *ch)
{
  _htsp_channel_update(ch, "channelAdd", NULL);
}

/**
 * Called from channel.c when a channel is updated
 */
void
htsp_channel_update(channel_t *ch)
{
  if (htsp_user_access_channel(NULL, ch))
    _htsp_channel_update(ch, "channelUpdate", NULL);
  else // in case the channel was ever sent to the client
    htsp_channel_delete(ch);
}

/**
 * Called from channel.c when a channel is deleted
 */
void
htsp_channel_delete(channel_t *ch)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "channelId", channel_get_id(ch));
  htsmsg_add_str(m, "method", "channelDelete");
  htsp_async_send(m, HTSP_ASYNC_ON, ch);
}

/**
 *
 */
static htsmsg_t *
htsp_tag_update_msg(htsp_connection_t *htsp, void *tag)
{
  if (!channel_tag_access(tag, htsp->htsp_granted_access, 0))
    return NULL;
  return htsp_build_tag(htsp, tag, "tagUpdate", 1);
}

/**
 * Called from channel.c when a tag is exported
 */
void
htsp_tag_add(channel_tag_t *ct)
{
  htsp_async_send_cb(htsp_tag_update_msg, HTSP_ASYNC_ON, ct);
}

/**
 * Called from channel.c when an exported tag is changed
 */
void
htsp_tag_update(channel_tag_t *ct)
{
  if (ct->ct_enabled && !ct->ct_internal)
    htsp_async_send_cb(htsp_tag_update_msg, HTSP_ASYNC_ON, ct);
  else // in case the tag was ever sent to the client
    htsp_tag_delete(ct);
}


/**
 * Called from channel.c when an exported tag is deleted
 */
void
htsp_tag_delete(channel_tag_t *ct)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "tagId", htsp_channel_tag_get_identifier(ct));
  htsmsg_add_str(m, "method", "tagDelete");
  htsp_async_send(m, HTSP_ASYNC_ON, ct);
}

/**
 * Called when a DVR entry is updated/added
 */
static void
_htsp_dvr_entry_update(dvr_entry_t *de, const char *method, htsmsg_t *msg)
{
  htsp_connection_t *htsp;
  LIST_FOREACH(htsp, &htsp_async_connections, htsp_async_link) {
    if (htsp->htsp_async_mode & HTSP_ASYNC_ON)
      if (!dvr_entry_verify(de, htsp->htsp_granted_access, 1)) {
        htsmsg_t *m = msg ? htsmsg_copy(msg)
                        : htsp_build_dvrentry(htsp, de, method, htsp->htsp_language, 0);
        htsp_send_message(htsp, m, NULL);
      }
  }
  htsmsg_destroy(msg);
}

/**
 * Called from dvr_db.c when a DVR entry is created
 */
void
htsp_dvr_entry_add(dvr_entry_t *de)
{
  _htsp_dvr_entry_update(de, "dvrEntryAdd", NULL);
}

/**
 * Called from dvr_db.c when a DVR entry is updated
 */
void
htsp_dvr_entry_update(dvr_entry_t *de)
{
  _htsp_dvr_entry_update(de, "dvrEntryUpdate", NULL);
}

/**
 * Called from dvr_rec.c when a DVR entry is recording
 */
void
htsp_dvr_entry_update_stats(dvr_entry_t *de)
{
  htsp_connection_t *htsp;
  LIST_FOREACH(htsp, &htsp_async_connections, htsp_async_link) {
    if (htsp->htsp_async_mode & HTSP_ASYNC_ON){
      if (!dvr_entry_verify(de, htsp->htsp_granted_access, 1)) {
        htsmsg_t *m = htsp_build_dvrentry(htsp, de, "dvrEntryUpdate", htsp->htsp_language, htsp->htsp_version <= 25 ? 0 : 1);
        htsp_send_message(htsp, m, NULL);
      }
    }
  }
}

/**
 * Called from dvr_db.c when a DVR entry is deleted
 */
void
htsp_dvr_entry_delete(dvr_entry_t *de)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", idnode_get_short_uuid(&de->de_id));
  htsmsg_add_str(m, "method", "dvrEntryDelete");
  htsp_async_send(m, HTSP_ASYNC_ON, de);
}

/**
 * Called when a autorec entry is updated/added
 */
static void
_htsp_autorec_entry_update(dvr_autorec_entry_t *dae, const char *method, htsmsg_t *msg)
{
  htsp_connection_t *htsp;
  LIST_FOREACH(htsp, &htsp_async_connections, htsp_async_link) {
    if (htsp->htsp_async_mode & HTSP_ASYNC_ON) {
      if (!dvr_autorec_entry_verify(dae, htsp->htsp_granted_access, 1)) {
        htsmsg_t *m = msg ? htsmsg_copy(msg)
                          : htsp_build_autorecentry(htsp, dae, method);
        htsp_send_message(htsp, m, NULL);
      }
    }
  }
  htsmsg_destroy(msg);
}

/**
 * Called from dvr_autorec.c when a autorec entry is added
 */
void
htsp_autorec_entry_add(dvr_autorec_entry_t *dae)
{
  _htsp_autorec_entry_update(dae, "autorecEntryAdd", NULL);
}

/**
 * Called from dvr_autorec.c when a autorec entry is updated
 */
void
htsp_autorec_entry_update(dvr_autorec_entry_t *dae)
{
  _htsp_autorec_entry_update(dae, "autorecEntryUpdate", NULL);
}

/**
 * Called from dvr_autorec.c when a autorec entry is deleted
 */
void
htsp_autorec_entry_delete(dvr_autorec_entry_t *dae)
{
  char ubuf[UUID_HEX_SIZE];

  if(dae == NULL)
    return;

  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "id", idnode_uuid_as_str(&dae->dae_id, ubuf));
  htsmsg_add_str(m, "method", "autorecEntryDelete");
  htsp_async_send(m, HTSP_ASYNC_ON, dae);
}

/**
 * Called when a timerec entry is updated/added
 */
static void
_htsp_timerec_entry_update(dvr_timerec_entry_t *dte, const char *method, htsmsg_t *msg)
{
  htsp_connection_t *htsp;
  LIST_FOREACH(htsp, &htsp_async_connections, htsp_async_link) {
    if (htsp->htsp_async_mode & HTSP_ASYNC_ON) {
      if (!dvr_timerec_entry_verify(dte, htsp->htsp_granted_access, 1)) {
        htsmsg_t *m = msg ? htsmsg_copy(msg)
                          : htsp_build_timerecentry(htsp, dte, method);
        htsp_send_message(htsp, m, NULL);
      }
    }
  }
  htsmsg_destroy(msg);
}

/**
 * Called from dvr_timerec.c when a timerec entry is added
 */
void
htsp_timerec_entry_add(dvr_timerec_entry_t *dte)
{
  _htsp_timerec_entry_update(dte, "timerecEntryAdd", NULL);
}

/**
 * Called from dvr_timerec.c when a timerec entry is updated
 */
void
htsp_timerec_entry_update(dvr_timerec_entry_t *dte)
{
  _htsp_timerec_entry_update(dte, "timerecEntryUpdate", NULL);
}

/**
 * Called from dvr_timerec.c when a timerec entry is deleted
 */
void
htsp_timerec_entry_delete(dvr_timerec_entry_t *dte)
{
  char ubuf[UUID_HEX_SIZE];

  if(dte == NULL)
    return;

  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "id", idnode_uuid_as_str(&dte->dte_id, ubuf));
  htsmsg_add_str(m, "method", "timerecEntryDelete");
  htsp_async_send(m, HTSP_ASYNC_ON, dte);
}

/**
 * Called every "HTSP_ASYNC_EPG_INTERVAL" seconds
 * Keep the async epg window up to date
 */
static void
htsp_epg_window_cb(void *aux)
{
  htsp_connection_t *htsp = aux;
  htsp_epg_send_waiting(htsp, htsp->htsp_epg_lastupdate);
}

/**
 * Send all waiting EPG events
 */
static void
htsp_epg_send_waiting(htsp_connection_t *htsp, int64_t mintime)
{
  epg_broadcast_t *ebc;
  channel_t *ch;
  int64_t maxtime;

  maxtime = gclk() + htsp->htsp_epg_window;
  htsp->htsp_epg_lastupdate = maxtime;

  /* Push new events */
  CHANNEL_FOREACH(ch) {
    if (!htsp_user_access_channel(htsp, ch)) continue;
    RB_FOREACH(ebc, &ch->ch_epg_schedule, sched_link) {
      if (ebc->start <= mintime) continue;
      if (htsp->htsp_epg_window && ebc->start > maxtime) break;
      htsmsg_t *e = htsp_build_event(ebc, "eventAdd", htsp->htsp_language, 0, htsp);
      if (e) htsp_send_message(htsp, e, NULL);
    }
  }

  /* Keep the epg window up to date */
  if (htsp->htsp_epg_window)
    mtimer_arm_rel(&htsp->htsp_epg_timer, htsp_epg_window_cb,
                   htsp, sec2mono(HTSP_ASYNC_EPG_INTERVAL));
}

/**
 * Called when a event entry is updated/added
 */
static void
_htsp_event_update(epg_broadcast_t *ebc, const char *method, htsmsg_t *msg)
{
  htsp_connection_t *htsp;
  LIST_FOREACH(htsp, &htsp_async_connections, htsp_async_link) {
    if (htsp->htsp_async_mode & HTSP_ASYNC_EPG) {
      /* Use last update instead of window time as we do not want to push an update
       * for an event we still have to send with "htsp_epg_window_cb" */
      if (!htsp->htsp_epg_window || ebc->start <= htsp->htsp_epg_lastupdate) {
        if (htsp_user_access_channel(htsp,ebc->channel)) {
          htsmsg_t *m = msg ? htsmsg_copy(msg)
                          : htsp_build_event(ebc, method, htsp->htsp_language, 0, htsp);
          htsp_send_message(htsp, m, NULL);
        }
      }
    }
  }
  htsmsg_destroy(msg);
}

/**
 * Event added
 */
void
htsp_event_add(epg_broadcast_t *ebc)
{
  _htsp_event_update(ebc, "eventAdd", NULL);
}

/**
 * Event updated
 */
void
htsp_event_update(epg_broadcast_t *ebc)
{
  _htsp_event_update(ebc, "eventUpdate", NULL);
}

/**
 * Event deleted
 */
void
htsp_event_delete(epg_broadcast_t *ebc)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "method", "eventDelete");
  htsmsg_add_u32(m, "eventId", ebc->id);
  htsp_async_send(m, HTSP_ASYNC_EPG, ebc);
}

static const char frametypearray[PKT_NTYPES] = {
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
  htsmsg_t *m;
  htsp_msg_t *hm;
  htsp_connection_t *htsp = hs->hs_htsp;
  int64_t ts;
  int qlen = hs->hs_q.hmq_payload;
  int video = SCT_ISVIDEO(pkt->pkt_type);
  size_t payloadlen;

  if (pkt->pkt_err)
    hs->hs_data_errors += pkt->pkt_err;
  if(pkt->pkt_payload == NULL) {
    return;
  }

  if(!htsp_is_stream_enabled(hs, pkt->pkt_componentindex)) {
    pkt_ref_dec(pkt);
    return;
  }

  if(video &&
     ((qlen > hs->hs_queue_depth     && pkt->v.pkt_frametype == PKT_B_FRAME) ||
      (qlen > hs->hs_queue_depth * 2 && pkt->v.pkt_frametype == PKT_P_FRAME) ||
      (qlen > hs->hs_queue_depth * 3))) {

    hs->hs_dropstats[pkt->v.pkt_frametype]++;

    /* Queue size protection */
    pkt_ref_dec(pkt);
    return;
  }

  m = htsmsg_create_map();

  htsmsg_add_str(m, "method", "muxpkt");
  htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);
  if (video)
    htsmsg_add_u32(m, "frametype", frametypearray[pkt->v.pkt_frametype]);

  htsmsg_add_u32(m, "stream", pkt->pkt_componentindex);
  htsmsg_add_u32(m, "com", pkt->pkt_commercial);

  if(pkt->pkt_pts != PTS_UNSET) {
    int64_t pts = hs->hs_90khz ? pkt->pkt_pts : ts_rescale(pkt->pkt_pts, 1000000);
    htsmsg_add_s64(m, "pts", pts);
  }

  if(pkt->pkt_dts != PTS_UNSET) {
    int64_t dts = hs->hs_90khz ? pkt->pkt_dts : ts_rescale(pkt->pkt_dts, 1000000);
    htsmsg_add_s64(m, "dts", dts);
  }

  uint32_t dur = hs->hs_90khz ? pkt->pkt_duration : ts_rescale(pkt->pkt_duration, 1000000);
  htsmsg_add_u32(m, "duration", dur);

  /**
   * Since we will serialize directly we use 'binptr' which is a binary
   * object that just points to data, thus avoiding a copy.
   */
  payloadlen = pktbuf_len(pkt->pkt_payload);
  htsmsg_add_bin_ptr(m, "payload", pktbuf_ptr(pkt->pkt_payload), payloadlen);
  htsp_send_subscription(htsp, m, pkt->pkt_payload, hs, payloadlen);
  atomic_add(&hs->hs_s_bytes_out, payloadlen);

  if(mono2sec(hs->hs_last_report) != mono2sec(mclk())) {

    /* Send a queue and signal status report every second */

    hs->hs_last_report = mclk();

    m = htsmsg_create_map();
    htsmsg_add_str(m, "method", "queueStatus");
    htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);
    htsmsg_add_u32(m, "packets", hs->hs_q.hmq_length);
    htsmsg_add_u32(m, "bytes", hs->hs_q.hmq_payload);
    if (hs->hs_data_errors)
      htsmsg_add_u32(m, "errors", hs->hs_data_errors);

    /**
     * Figure out real time queue delay
     */

    tvh_mutex_lock(&htsp->htsp_out_mutex);

    int64_t min_dts = PTS_UNSET;
    int64_t max_dts = PTS_UNSET;
    TAILQ_FOREACH(hm, &hs->hs_q.hmq_q, hm_link) {
      if(!hm->hm_msg)
	continue;
      if(htsmsg_get_s64(hm->hm_msg, "dts", &ts))
	continue;
      if(ts == PTS_UNSET)
	continue;

      if(min_dts == PTS_UNSET)
	min_dts = ts;
      else
	min_dts = MIN(ts, min_dts);

      if(max_dts == PTS_UNSET)
	max_dts = ts;
      else
	max_dts = MAX(ts, max_dts);
    }

    htsmsg_add_s64(m, "delay", max_dts - min_dts);

    tvh_mutex_unlock(&htsp->htsp_out_mutex);

    htsmsg_add_u32(m, "Bdrops", hs->hs_dropstats[PKT_B_FRAME]);
    htsmsg_add_u32(m, "Pdrops", hs->hs_dropstats[PKT_P_FRAME]);
    htsmsg_add_u32(m, "Idrops", hs->hs_dropstats[PKT_I_FRAME]);

    /* We use a special queue for queue status message so they're not
       blocked by anything else */
    htsp_send_message(hs->hs_htsp, m, &hs->hs_htsp->htsp_hmq_qstatus);
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
  htsmsg_t *m,*streams, *c, *sourceinfo;
  const char *type;
  char ubuf[UUID_HEX_SIZE];
  int i;
  const source_info_t *si;

  tvhdebug(LS_HTSP, "%s - subscription start", hs->hs_htsp->htsp_logname);

  for(i = 0; i < ss->ss_num_components; i++) {
    const streaming_start_component_t *ssc = &ss->ss_components[i];
    if (ssc->ssc_disabled) continue;
    if (SCT_ISVIDEO(ssc->es_type)) {
      if (ssc->es_width == 0 || ssc->es_height == 0) {
        hs->hs_wait_for_video = 1;
        return;
      }
      break;
    }
  }
  hs->hs_wait_for_video = 0;

  m = htsmsg_create_map();
  streams = htsmsg_create_list();
  sourceinfo = htsmsg_create_map();
  for(i = 0; i < ss->ss_num_components; i++) {
    const streaming_start_component_t *ssc = &ss->ss_components[i];
    if(ssc->ssc_disabled) continue;

    c = htsmsg_create_map();
    htsmsg_add_u32(c, "index", ssc->es_index);
    if (ssc->es_type == SCT_AAC)
      type = "AAC"; /* override */
    else
      type = streaming_component_type2txt(ssc->es_type);
    htsmsg_add_str(c, "type", type);
    if(ssc->es_lang[0])
      htsmsg_add_str(c, "language", ssc->es_lang);

    if(ssc->es_type == SCT_DVBSUB) {
      htsmsg_add_u32(c, "composition_id", ssc->es_composition_id);
      htsmsg_add_u32(c, "ancillary_id", ssc->es_ancillary_id);
    }

    if(SCT_ISVIDEO(ssc->es_type)) {
      if(ssc->es_width)
        htsmsg_add_u32(c, "width", ssc->es_width);
      if(ssc->es_height)
        htsmsg_add_u32(c, "height", ssc->es_height);
      if(ssc->es_frame_duration)
        htsmsg_add_u32(c, "duration", hs->hs_90khz ? ssc->es_frame_duration :
                       ts_rescale(ssc->es_frame_duration, 1000000));
      if (ssc->es_aspect_num)
        htsmsg_add_u32(c, "aspect_num", ssc->es_aspect_num);
      if (ssc->es_aspect_den)
        htsmsg_add_u32(c, "aspect_den", ssc->es_aspect_den);
    }

    if (SCT_ISAUDIO(ssc->es_type))
    {
      htsmsg_add_u32(c, "audio_type", ssc->es_audio_type);
      if (ssc->es_audio_version)
        htsmsg_add_u32(c, "audio_version", ssc->es_audio_version);
      if (ssc->es_channels)
        htsmsg_add_u32(c, "channels", ssc->es_channels);
      if (ssc->es_sri)
        htsmsg_add_u32(c, "rate", ssc->es_sri);
      htsmsg_add_u32(c, "rds_uecp", ssc->es_rds_uecp == 0 ? 0 : 1);
    }

    if (ssc->ssc_gh)
      htsmsg_add_bin_ptr(m, "meta", pktbuf_ptr(ssc->ssc_gh),
		         pktbuf_len(ssc->ssc_gh));

    htsmsg_add_msg(streams, NULL, c);
  }

  htsmsg_add_msg(m, "streams", streams);

  si = &ss->ss_si;
  if(!uuid_empty(&si->si_adapter_uuid))
    htsmsg_add_str(sourceinfo, "adapter_uuid",
                   uuid_get_hex(&si->si_adapter_uuid, ubuf));
  if(!uuid_empty(&si->si_mux_uuid))
    htsmsg_add_str(sourceinfo, "mux_uuid",
                   uuid_get_hex(&si->si_mux_uuid, ubuf));
  if(!uuid_empty(&si->si_network_uuid))
    htsmsg_add_str(sourceinfo, "network_uuid",
                   uuid_get_hex(&si->si_network_uuid, ubuf));
  if (!htsp_anonymize(hs->hs_htsp)) {
    htsmsg_add_str2(sourceinfo, "adapter",      si->si_adapter     );
    htsmsg_add_str2(sourceinfo, "mux",          si->si_mux         );
    htsmsg_add_str2(sourceinfo, "network",      si->si_network     );
    htsmsg_add_str2(sourceinfo, "network_type", si->si_network_type);
    htsmsg_add_str2(sourceinfo, "provider",     si->si_provider    );
    htsmsg_add_str2(sourceinfo, "service",      si->si_service     );
    htsmsg_add_str2(sourceinfo, "satpos",       si->si_satpos      );
  }

  htsmsg_add_msg(m, "sourceinfo", sourceinfo);

  htsmsg_add_str(m, "method", "subscriptionStart");
  htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);
  htsp_send_subscription(hs->hs_htsp, m, NULL, hs, 0);
}

/**
 * Send a 'subscriptionStop' stop
 */
static void
htsp_subscription_stop(htsp_subscription_t *hs, const char *err, const char *subscriptionErr)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "method", "subscriptionStop");
  htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);
  tvhdebug(LS_HTSP, "%s - subscription stop", hs->hs_htsp->htsp_logname);

  if(err != NULL)
    htsmsg_add_str(m, "status", err);

  if(subscriptionErr != NULL)
    htsmsg_add_str(m, "subscriptionError", subscriptionErr);

  htsp_send_subscription(hs->hs_htsp, m, NULL, hs, 0);
}

/**
 * Send a 'subscriptionGrace' message
 */
static void
htsp_subscription_grace(htsp_subscription_t *hs, int grace)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "method", "subscriptionGrace");
  htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);
  htsmsg_add_u32(m, "graceTimeout", grace);
  tvhdebug(LS_HTSP, "%s - subscription grace %i seconds", hs->hs_htsp->htsp_logname, grace);

  htsp_send_subscription(hs->hs_htsp, m, NULL, hs, 0);
}

/**
 * Send a 'subscriptionStatus' message
 */
static void
htsp_subscription_status(htsp_subscription_t *hs, const char *err, const char *subscriptionErr)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "method", "subscriptionStatus");
  htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);

  if(err != NULL)
    htsmsg_add_str(m, "status", err);

  if(subscriptionErr != NULL)
    htsmsg_add_str(m, "subscriptionError", subscriptionErr);

  htsp_send_subscription(hs->hs_htsp, m, NULL, hs, 0);
}

/**
 * Convert the SM_CODE to an understandable string
 */
const char *
_htsp_get_subscription_status(int smcode)
{
  switch (smcode)
  {
  case SM_CODE_NOT_FREE:
  case SM_CODE_NO_FREE_ADAPTER:
  case SM_CODE_NO_ADAPTERS:
    return "noFreeAdapter";
  case SM_CODE_NO_ACCESS:
  case SM_CODE_NO_DESCRAMBLER:
    return "scrambled";
  case SM_CODE_NO_INPUT:
  case SM_CODE_BAD_SIGNAL:
    return "badSignal";
  case SM_CODE_TUNING_FAILED:
    return "tuningFailed";
  case SM_CODE_SUBSCRIPTION_OVERRIDDEN:
    return "subscriptionOverridden";
  case SM_CODE_MUX_NOT_ENABLED:
    return "muxNotEnabled";
  case SM_CODE_INVALID_TARGET:
    return "invalidTarget";
  case SM_CODE_USER_ACCESS:
    return "userAccess";
  case SM_CODE_USER_LIMIT:
    return "userLimit";
  case SM_CODE_WEAK_STREAM:
    return "weakStream";
  case SM_CODE_NO_SPACE:
    return "noDiskSpace";
  default:
    return streaming_code2txt(smcode);
  }
}

/**
 *
 */
static void
htsp_subscription_service_status(htsp_subscription_t *hs, int status)
{
  if(status & TSS_PACKETS) {
    htsp_subscription_status(hs, NULL, NULL);
  } else if(status & TSS_ERRORS) {
    htsp_subscription_status(hs, service_tss2text(status), NULL);
  }
}

/**
 *
 */
static void
htsp_subscription_signal_status(htsp_subscription_t *hs, signal_status_t *sig)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "method", "signalStatus");
  htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);
  if (!htsp_anonymize(hs->hs_htsp)) {
    htsmsg_add_str(m, "feStatus",   sig->status_text);
    if((sig->snr != -2) && (sig->snr_scale == SIGNAL_STATUS_SCALE_RELATIVE))
      htsmsg_add_u32(m, "feSNR",    sig->snr);
    if((sig->signal != -2) && (sig->signal_scale == SIGNAL_STATUS_SCALE_RELATIVE))
      htsmsg_add_u32(m, "feSignal", sig->signal);
    if(sig->ber != -2)
      htsmsg_add_u32(m, "feBER",    sig->ber);
    if(sig->unc != -2)
      htsmsg_add_u32(m, "feUNC",    sig->unc);
  } else {
    htsmsg_add_str(m, "feStatus", "");
  }
  htsp_send_message(hs->hs_htsp, m, &hs->hs_htsp->htsp_hmq_qstatus);
}

/**
 *
 */
static void
htsp_subscription_descramble_info(htsp_subscription_t *hs, descramble_info_t *di)
{
  /* don't bother old clients */
  if (hs->hs_htsp->htsp_version < 24)
    return;
  if (htsp_anonymize(hs->hs_htsp))
    return;

  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "method", "descrambleInfo");
  htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);
  htsmsg_add_u32(m, "pid", di->pid);
  htsmsg_add_u32(m, "caid", di->caid);
  htsmsg_add_u32(m, "provid", di->provid);
  htsmsg_add_u32(m, "ecmtime", di->ecmtime);
  htsmsg_add_u32(m, "hops", di->hops);
  if (di->cardsystem[0])
    htsmsg_add_str(m, "cardsystem", di->cardsystem);
  if (di->reader[0])
    htsmsg_add_str(m, "reader", di->reader);
  if (di->from[0])
    htsmsg_add_str(m, "from", di->from);
  if (di->protocol[0])
    htsmsg_add_str(m, "protocol", di->protocol);
  htsp_send_message(hs->hs_htsp, m, &hs->hs_htsp->htsp_hmq_qstatus);
}

/**
 *
 */
static void
htsp_subscription_speed(htsp_subscription_t *hs, int speed)
{
  htsmsg_t *m = htsmsg_create_map();
  tvhdebug(LS_HTSP, "%s - subscription speed", hs->hs_htsp->htsp_logname);
  htsmsg_add_str(m, "method", "subscriptionSpeed");
  htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);
  htsmsg_add_s32(m, "speed", speed);
  htsp_send_subscription(hs->hs_htsp, m, NULL, hs, 0);
}

/**
 *
 */
#if ENABLE_TIMESHIFT
static void
htsp_subscription_timeshift_status(htsp_subscription_t *hs, timeshift_status_t *status)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "method", "timeshiftStatus");
  htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);
  htsmsg_add_u32(m, "full", status->full);
  htsmsg_add_s64(m, "shift", hs->hs_90khz ? status->shift : ts_rescale(status->shift, 1000000));
  if (status->pts_start != PTS_UNSET)
    htsmsg_add_s64(m, "start", hs->hs_90khz ? status->pts_start : ts_rescale(status->pts_start, 1000000)) ;
  if (status->pts_end != PTS_UNSET)
    htsmsg_add_s64(m, "end", hs->hs_90khz ? status->pts_end : ts_rescale(status->pts_end, 1000000)) ;
  htsp_send_subscription(hs->hs_htsp, m, NULL, hs, 0);
}
#endif

/**
 *
 */
static void
htsp_subscription_skip(htsp_subscription_t *hs, streaming_skip_t *skip)
{
  htsmsg_t *m = htsmsg_create_map();
  tvhdebug(LS_HTSP, "%s - subscription skip", hs->hs_htsp->htsp_logname);
  htsmsg_add_str(m, "method", "subscriptionSkip");
  htsmsg_add_u32(m, "subscriptionId", hs->hs_sid);

  /* Flush pkt buffers */
#if ENABLE_TIMESHIFT
  if (skip->type != SMT_SKIP_ERROR && timeshift_conf.enabled) {
    htsp_flush_queue(hs->hs_htsp, &hs->hs_q, 0);
    htsp_subscription_timeshift_status(hs, &skip->timeshift);
  }
#endif

  if (skip->type == SMT_SKIP_ABS_TIME || skip->type == SMT_SKIP_ABS_SIZE)
    htsmsg_add_u32(m, "absolute", 1);
  if (skip->type == SMT_SKIP_ERROR)
    htsmsg_add_u32(m, "error", 1);
  else if (skip->type == SMT_SKIP_ABS_TIME || skip->type == SMT_SKIP_REL_TIME)
    htsmsg_add_s64(m, "time", hs->hs_90khz ? skip->time : ts_rescale(skip->time, 1000000));
  else if (skip->type == SMT_SKIP_ABS_SIZE || skip->type == SMT_SKIP_REL_SIZE)
    htsmsg_add_s64(m, "size", skip->size);
  htsp_send_subscription(hs->hs_htsp, m, NULL, hs, 0);
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
    if (hs->hs_wait_for_video)
      break;
    if (!hs->hs_first)
      tvhdebug(LS_HTSP, "%s - first packet", hs->hs_htsp->htsp_logname);
    hs->hs_first = 1;
    htsp_stream_deliver(hs, sm->sm_data);
    // reference is transfered
    sm->sm_data = NULL;
    break;

  case SMT_START:
    htsp_subscription_start(hs, sm->sm_data);
    break;

  case SMT_STOP:
    htsp_subscription_stop(hs, streaming_code2txt(sm->sm_code),
        sm->sm_code ? _htsp_get_subscription_status(sm->sm_code) : NULL);
    break;

  case SMT_GRACE:
    htsp_subscription_grace(hs, sm->sm_code);
    break;

  case SMT_SERVICE_STATUS:
    htsp_subscription_service_status(hs, sm->sm_code);
    break;

  case SMT_SIGNAL_STATUS:
    htsp_subscription_signal_status(hs, sm->sm_data);
    break;

  case SMT_DESCRAMBLE_INFO:
    htsp_subscription_descramble_info(hs, sm->sm_data);
    break;

  case SMT_NOSTART:
  case SMT_NOSTART_WARN:
    htsp_subscription_status(hs,  streaming_code2txt(sm->sm_code),
        sm->sm_code ? _htsp_get_subscription_status(sm->sm_code) : NULL);
    break;

  case SMT_MPEGTS:
    break;

  case SMT_EXIT:
    abort();

  case SMT_SKIP:
    htsp_subscription_skip(hs, sm->sm_data);
    break;

  case SMT_SPEED:
    htsp_subscription_speed(hs, sm->sm_code);
    break;

  case SMT_TIMESHIFT_STATUS:
#if ENABLE_TIMESHIFT
    htsp_subscription_timeshift_status(hs, sm->sm_data);
#endif
    break;
  }
  streaming_msg_free(sm);
}

static htsmsg_t *
htsp_streaming_input_info(void *opaque, htsmsg_t *list)
{
  char buf[512];
  htsp_subscription_t *hs = opaque;
  snprintf(buf, sizeof(buf), "htsp input: %s", hs->hs_htsp->htsp_logname);
  htsmsg_add_str(list, NULL, buf);
  return list;
}
