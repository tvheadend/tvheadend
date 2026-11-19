/*
 *  API - service related calls
 *
 *  Copyright (C) 2013 Adam Sutton
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

#ifndef __TVH_API_SERVICE_H__
#define __TVH_API_SERVICE_H__

#include "tvheadend.h"
#include "subscriptions.h"
#include "access.h"
#include "api.h"
#include "tcp.h"
#include "input.h"
#include "epggrab.h"  //Needed to get the next EPG grab times
#include "dvr/dvr.h"  //Needed to get the next schedule dvr time
#include "channels.h"
#include "imagecache.h"
#include "service.h"
#if ENABLE_TIMESHIFT
#include "timeshift.h"
#include "timeshift/private.h"
#include "timeshift/timeshift_svcbuf.h"
#include "timeshift/timeshift_svcts.h"
#include "profile.h"
#endif

static int
api_status_inputs
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int c = 0;
  htsmsg_t *l, *e;
  tvh_input_t *ti;
  tvh_input_stream_t *st;
  tvh_input_stream_list_t stl = { 0 };
  
  tvh_mutex_lock(&global_lock);
  TVH_INPUT_FOREACH(ti)
    ti->ti_get_streams(ti, &stl);
  tvh_mutex_unlock(&global_lock);

  l = htsmsg_create_list();
  while ((st = LIST_FIRST(&stl))) {
    e = tvh_input_stream_create_msg(st);
    htsmsg_add_msg(l, NULL, e);
    tvh_input_stream_destroy(st);
    LIST_REMOVE(st, link);
    free(st);
    c++;
  }
    
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  htsmsg_add_u32(*resp, "totalCount", c);

  return 0;
}

static int
api_status_subscriptions
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int c;
  htsmsg_t *l, *e;
  th_subscription_t *ths;

  l = htsmsg_create_list();
  c = 0;
  tvh_mutex_lock(&global_lock);
  LIST_FOREACH(ths, &subscriptions, ths_global_link) {
    e = subscription_create_msg(ths, perm->aa_lang_ui);
    htsmsg_add_msg(l, NULL, e);
    c++;
  }
  tvh_mutex_unlock(&global_lock);

  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  htsmsg_add_u32(*resp, "totalCount", c);

  return 0;
}

#if ENABLE_TIMESHIFT
/*
 * The recording a subscription makes, if any.  A recording that started
 * live reads the cache through no replay gate, so only its DVR entry says
 * what it is.  A cache-only warmup writes no file, and is not one.
 * (global_lock held)
 */
static dvr_entry_t *
api_status_cache_recording ( const th_subscription_t *s )
{
  dvr_entry_t *de;

  LIST_FOREACH(de, &dvrentries, de_global_link)
    if (de->de_s == s)
      return de->de_cache_only ? NULL : de;
  return NULL;
}

/* A hold on an idle channel, and how long it has left (global_lock held) */
static void
api_status_cache_keepalive ( htsmsg_t *e, const th_subscription_t *s )
{
  htsmsg_add_bool(e, "keepalive", 1);
  if (s->ths_remove_timer.mti_expire && s->ths_remove_timer.mti_callback)
    htsmsg_add_s64(e, "keepalive_expires",
                   MAX(0, mono2sec(s->ths_remove_timer.mti_expire - mclk())));
}

/*
 * The channel a cache is of, as the guide shows it: its name, its number
 * and its icon, the imagecache path to it.
 */
static void
api_status_cache_channel ( htsmsg_t *e, channel_t *ch, const char *blank )
{
  char num[32], icon[512];
  const char *s;

  htsmsg_add_str(e, "channel", channel_get_name(ch, blank));
  if (channel_get_number(ch) > 0 &&
      channel_get_number_as_str(ch, num, sizeof(num)))
    htsmsg_add_str(e, "channel_number", num);
  s = channel_get_icon(ch);
  if (!strempty(s) && (s = imagecache_get_propstr(s, icon, sizeof(icon))) != NULL)
    htsmsg_add_str(e, "channel_icon", s);
}

/*
 * A recording's programme, padding included, as a [start, stop] pair into
 * `recorded`, and into `ranges` while there is room: past that many
 * recordings of one channel at once, the rest go uncounted in the bytes.
 * The tuner's warm-up before that is not part of it: the recorder writes
 * nothing before the padded start.
 */
static void
api_status_cache_recorded ( htsmsg_t *recorded, dvr_entry_t *de,
                            time_t *ranges, int room, int *nranges )
{
  htsmsg_t *range = htsmsg_create_list();
  time_t start = dvr_entry_get_start_time(de, 0);
  time_t stop = dvr_entry_get_stop_time(de);

  htsmsg_add_s64(range, NULL, start);
  htsmsg_add_s64(range, NULL, stop);
  htsmsg_add_msg(recorded, NULL, range);
  if (*nranges < room) {
    ranges[2 * *nranges] = start;
    ranges[2 * *nranges + 1] = stop;
    (*nranges)++;
  }
}

/*
 * Who is in a cache, from the subscriptions rather than the cache's readers:
 * a client watching live has no reader until it seeks back, and it is in
 * the cache all the same.  Each says how far behind live it is, as in the
 * subscriptions' own cache fields, and whether a viewer is paused; a
 * recording also says the wall-clock span of its programme (padding
 * included), the part of the cache it is made of, added to `ranges`.  A hold
 * on an idle channel says how long it has left.  One that plays the channel
 * live past the cache -- an HTTP stream, which does not timeshift -- is
 * listed apart, in `live`, by who it is.
 */
static void
api_status_cache_client ( htsmsg_t *list, th_subscription_t *s )
{
  char buf[256];

  /* who it is, to tell viewers apart: the client (or user) and where it
     connects from */
  snprintf(buf, sizeof(buf), "%s%s%s%s",
           s->ths_client ?: s->ths_username ?: "",
           s->ths_hostname ? " (" : "",
           s->ths_hostname ?: "",
           s->ths_hostname ? ")" : "");
  htsmsg_add_str(list, NULL, buf);
}

static void
api_status_cache_members
  ( htsmsg_t *e, service_t *t, time_t *ranges, int room, int *nranges )
{
  htsmsg_t *viewers = htsmsg_create_list();
  htsmsg_t *paused = htsmsg_create_list();
  htsmsg_t *clients = htsmsg_create_list();
  htsmsg_t *recordings = htsmsg_create_list();
  htsmsg_t *recorded = htsmsg_create_list();
  htsmsg_t *live = htsmsg_create_list();
  th_subscription_t *s;
  dvr_entry_t *de;
  int64_t behind;
  int others = 0;

  *nranges = 0;
  LIST_FOREACH(s, &t->s_subscriptions, ths_service_link) {
    if (s->ths_cache_keepalive) {
      api_status_cache_keepalive(e, s);
      continue;
    }
    others++;
    de = api_status_cache_recording(s);
    if (de)
      api_status_cache_recorded(recorded, de, ranges, room, nranges);
    if (s->ths_gate || de) {
      /* a gate replays the cache; without one, the recording is live */
      htsmsg_add_s64(recordings, NULL,
                     s->ths_gate ? svcbuf_gate_behind(s->ths_gate) : 0);
    } else if (s->ths_prch && s->ths_prch->prch_svcts &&
               svcts_behind(s->ths_prch->prch_svcts, &behind)) {
      htsmsg_add_s64(viewers, NULL, behind);
      htsmsg_add_bool(paused, NULL, svcts_paused(s->ths_prch->prch_svcts));
      api_status_cache_client(clients, s);
    } else if (s->ths_weight != SUBSCRIPTION_PRIO_CACHE_WARMUP) {
      api_status_cache_client(live, s);
    }
  }
  htsmsg_add_msg(e, "viewers", viewers);
  htsmsg_add_msg(e, "paused", paused);
  htsmsg_add_msg(e, "clients", clients);
  htsmsg_add_msg(e, "recordings", recordings);
  htsmsg_add_msg(e, "recorded", recorded);
  htsmsg_add_msg(e, "live", live);
  htsmsg_add_s64(e, "subscriptions", others);
}

/* How much of a cache its recordings are made of, and where; added up in
 * `rec` for the budgets */
static void
api_status_cache_recorded_bytes
  ( htsmsg_t *e, svcbuf_t *sb, const time_t *ranges, int nranges,
    uint64_t *rec )
{
  uint64_t bytes[4];
  htsmsg_t *rb = htsmsg_create_map();

  svcbuf_bytes_within(sb, ranges, nranges, bytes);
  htsmsg_add_s64(rb, "ram", bytes[SVCBUF_LOC_RAM]);
  htsmsg_add_s64(rb, "staging", bytes[SVCBUF_LOC_STAGING]);
  htsmsg_add_s64(rb, "disk", bytes[SVCBUF_LOC_DISK]);
  htsmsg_add_msg(e, "recorded_bytes", rb);
  rec[SVCBUF_LOC_RAM]     += bytes[SVCBUF_LOC_RAM];
  rec[SVCBUF_LOC_STAGING] += bytes[SVCBUF_LOC_STAGING];
  rec[SVCBUF_LOC_DISK]    += bytes[SVCBUF_LOC_DISK];
}

/* The budgets the caches draw on, and what their recordings take of them */
static htsmsg_t *
api_status_cache_budget ( const uint64_t *rec )
{
  htsmsg_t *b = htsmsg_create_map();

  htsmsg_add_bool(b, "enabled", timeshift_conf.enabled && timeshift_conf.record_cache);
  htsmsg_add_bool(b, "ram_only", timeshift_conf.ram_only);
  htsmsg_add_bool(b, "unlimited", timeshift_conf.unlimited_size);
  htsmsg_add_s64(b, "size_used", timeshift_size_used());
  htsmsg_add_s64(b, "size_max", timeshift_conf.ram_only ?
                                  timeshift_conf.ram_size : timeshift_conf.max_size);
  htsmsg_add_s64(b, "ram_used", timeshift_ram_used());
  htsmsg_add_s64(b, "ram_max", timeshift_conf.ram_size);
  htsmsg_add_s64(b, "staging_used", svcbuf_staging_used());
  htsmsg_add_s64(b, "staging_max", svcbuf_staging_limit());
  /* of all that, what the caches' recordings are made of */
  htsmsg_add_s64(b, "rec_ram", rec[SVCBUF_LOC_RAM]);
  htsmsg_add_s64(b, "rec_staging", rec[SVCBUF_LOC_STAGING]);
  htsmsg_add_s64(b, "rec_disk", rec[SVCBUF_LOC_DISK]);
  htsmsg_add_s64(b, "keepalive", timeshift_conf.cache_keepalive);
  /* the time a cache grows to, so a young one can be drawn as filling it */
  htsmsg_add_s64(b, "period", timeshift_conf.unlimited_period ? 0 :
                              (int64_t)timeshift_conf.max_period * 60);
  return b;
}

/*
 * The shared channel caches: one entry per service holding one, keyed by
 * the service, plus the budgets they draw on.  `buckets` sets how finely
 * each cache's span is mapped (default 120).
 */
static int
api_status_caches
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int c = 0;
  int buckets = htsmsg_get_s32_or_default(args, "buckets", 120);
  htsmsg_t *l;
  htsmsg_t *e;
  service_t *t;
  svcbuf_t *sb;
  idnode_list_mapping_t *ilm;
  const char *blank = tvh_gettext_lang(perm->aa_lang_ui, channel_blank_name);
  int nranges;
  /* recordings' programmes on one cache, as start, stop pairs */
  time_t ranges[2 * 16];
  uint64_t rec[4] = { 0, 0, 0, 0 };
  char buf[256];

  (void)opaque;
  (void)op;
  l = htsmsg_create_list();
  tvh_mutex_lock(&global_lock);
  TAILQ_FOREACH(t, &service_all, s_all_link) {
    tvh_mutex_lock(&t->s_stream_mutex);
    sb = svcbuf_acquire(t);
    tvh_mutex_unlock(&t->s_stream_mutex);
    if (sb == NULL)
      continue;
    e = svcbuf_status(sb, buckets);

    htsmsg_add_uuid(e, "uuid", &t->s_id.in_uuid);
    htsmsg_add_str(e, "service", service_adapter_nicename(t, buf, sizeof(buf)));
    ilm = LIST_FIRST(&t->s_channels);
    if (ilm)
      api_status_cache_channel(e, (channel_t *)ilm->ilm_in2, blank);

    api_status_cache_members(e, t, ranges, (int)ARRAY_SIZE(ranges) / 2, &nranges);
    api_status_cache_recorded_bytes(e, sb, ranges, nranges, rec);
    svcbuf_release(sb);

    htsmsg_add_msg(l, NULL, e);
    c++;
  }
  tvh_mutex_unlock(&global_lock);

  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  htsmsg_add_u32(*resp, "totalCount", c);
  htsmsg_add_msg(*resp, "budget", api_status_cache_budget(rec));
  return 0;
}
#endif

static int
api_status_connections
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  tvh_mutex_lock(&global_lock);
  *resp = tcp_server_connections();
  tvh_mutex_unlock(&global_lock);
  return 0;
}

static int
api_connections_cancel
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_field_t *f;
  htsmsg_t *ids;
  uint32_t id;
  const char *s;

  if (!(f = htsmsg_field_find(args, "id")))
    return EINVAL;
  s = htsmsg_field_get_str(f);
  if (s && strcmp(s, "all") == 0) {
    tvh_mutex_lock(&global_lock);
    tcp_connection_cancel_all();
    tvh_mutex_unlock(&global_lock);
    return 0;
  }
  if (!(ids = htsmsg_field_get_list(f)))
    if (htsmsg_field_get_u32(f, &id))
      return EINVAL;

  if (ids) {
    HTSMSG_FOREACH(f, ids) {
      if (htsmsg_field_get_u32(f, &id)) continue;
      if (!id) continue;
      tvh_mutex_lock(&global_lock);
      tcp_connection_cancel(id);
      tvh_mutex_unlock(&global_lock);
    }
  } else {
    tvh_mutex_lock(&global_lock);
    tcp_connection_cancel(id);
    tvh_mutex_unlock(&global_lock);
  }
  return 0;
}

static void
input_clear_stats(const char *uuid)
{
  tvh_input_instance_t *tii;
  tvh_input_t *ti;

  tvh_mutex_lock(&global_lock);
  if ((tii = tvh_input_instance_find_by_uuid(uuid)) != NULL)
    if (tii->tii_clear_stats)
      tii->tii_clear_stats(tii);
  if ((ti = tvh_input_find_by_uuid(uuid)) != NULL)
    if (ti->ti_clear_stats)
      ti->ti_clear_stats(ti);
  tvh_mutex_unlock(&global_lock);
}

static int
api_status_input_clear_stats
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_field_t *f;
  htsmsg_t *ids;
  const char *uuid;

  if (!(f = htsmsg_field_find(args, "uuid")))
    return EINVAL;
  if (!(ids = htsmsg_field_get_list(f))) {
    if ((uuid = htsmsg_field_get_str(f)) == NULL)
      return EINVAL;
    input_clear_stats(uuid);
  } else {
    HTSMSG_FOREACH(f, ids) {
      if ((uuid = htsmsg_field_get_str(f)) == NULL) continue;
      input_clear_stats(uuid);
    }
  }
  return 0;
}

static int
api_status_activity
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *cats;
  time_t temp_earliest = 0;
  time_t temp_dvr = 0;
  time_t temp_ota = 0;
  time_t temp_int = 0;
  time_t temp_mux = 0;
  th_subscription_t *ths;
  uint32_t subscriptionCount = 0;

  temp_dvr = dvr_entry_find_earliest();

  //Only evaluate the OTA grabber cron if there are active OTA modules.
  if(epggrab_count_type(EPGGRAB_OTA))
  {
    temp_ota = epggrab_get_next_ota();
  }
  
  //Only evaluate the internal grabber cron if there are active internal modules.
  if(epggrab_count_type(EPGGRAB_INT))
  {
    temp_int = epggrab_get_next_int();
  }
  
  temp_mux = mpegts_mux_sched_next();

  temp_earliest = temp_dvr;

  if(temp_ota && ((temp_ota < temp_earliest) || (temp_earliest == 0)))
  {
    temp_earliest = temp_ota;
  }

  if(temp_int && ((temp_int < temp_earliest) || (temp_earliest == 0)))
  {
    temp_earliest = temp_int;
  }

  if(temp_mux && ((temp_mux < temp_earliest) || (temp_earliest == 0)))
  {
    temp_earliest = temp_mux;
  }

  cats = htsmsg_create_map();
  htsmsg_add_s64(cats, "dvr", temp_dvr);
  htsmsg_add_s64(cats, "ota_grabber", temp_ota);
  htsmsg_add_s64(cats, "int_grabber", temp_int);
  htsmsg_add_s64(cats, "mux_scheduler", temp_mux);

  tvh_mutex_lock(&global_lock);
  LIST_FOREACH(ths, &subscriptions, ths_global_link) {
    subscriptionCount++;
  }
  tvh_mutex_unlock(&global_lock);

  *resp = htsmsg_create_map();
  htsmsg_add_s64(*resp, "current_time", gclk());
  htsmsg_add_s64(*resp, "next_activity", temp_earliest);
  htsmsg_add_msg(*resp, "activities", cats);
  htsmsg_add_u32(*resp, "subscription_count", subscriptionCount);
  htsmsg_add_u32(*resp, "connection_count", tcp_server_connections_count());

  return 0;

}

void api_status_init ( void )
{
  static api_hook_t ah[] = {
    { "status/connections",   ACCESS_ADMIN, api_status_connections, NULL },
    { "status/subscriptions", ACCESS_ADMIN, api_status_subscriptions, NULL },
#if ENABLE_TIMESHIFT
    { "status/caches",        ACCESS_ADMIN, api_status_caches, NULL },
#endif
    { "status/inputs",        ACCESS_ADMIN, api_status_inputs, NULL },
    { "status/inputclrstats", ACCESS_ADMIN, api_status_input_clear_stats, NULL },
    { "status/activity",      ACCESS_ADMIN, api_status_activity, NULL },
    { "connections/cancel",   ACCESS_ADMIN, api_connections_cancel, NULL },
    { NULL },
  };

  api_register_all(ah);
}


#endif /* __TVH_API_IDNODE_H__ */
