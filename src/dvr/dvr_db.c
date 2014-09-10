/*
 *  Digital Video Recorder
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
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include "settings.h"

#include "tvheadend.h"
#include "dvr.h"
#include "htsp_server.h"
#include "streaming.h"
#include "intlconv.h"
#include "dbus.h"
#include "imagecache.h"
#include "access.h"

int dvr_iov_max;

struct dvr_config_list dvrconfigs;
struct dvr_entry_list dvrentries;

#if ENABLE_DBUS_1
static gtimer_t dvr_dbus_timer;
#endif

static void dvr_entry_destroy(dvr_entry_t *de, int delconf);
static void dvr_timer_expire(void *aux);
static void dvr_timer_start_recording(void *aux);
static void dvr_timer_stop_recording(void *aux);
static int dvr_entry_class_disp_title_set(void *o, const void *v);

/*
 * Start / stop time calculators
 */
static int
dvr_entry_get_start_time( dvr_entry_t *de )
{
  time_t extra = de->de_start_extra;

  if (extra == 0) {
    if (de->de_channel)
      extra = de->de_channel->ch_dvr_extra_time_pre;
    else
      extra = de->de_config->dvr_extra_time_pre;
  }
  /* Note 30 seconds might not be enough (rotors) */
  return de->de_start - (60 * extra) - 30;
}

static int
dvr_entry_get_stop_time( dvr_entry_t *de )
{
  time_t extra = de->de_stop_extra;

  if (extra == 0) {
    if (de->de_channel)
      extra = de->de_channel->ch_dvr_extra_time_post;
    else
      extra = de->de_config->dvr_extra_time_pre;
  }
  return de->de_stop + (60 * extra);
}

int
dvr_entry_get_mc( dvr_entry_t *de )
{
  if (de->de_mc >= 0)
    return de->de_mc;
  return de->de_config->dvr_mc;
}

/*
 * DBUS next dvr start notifications
 */
#if ENABLE_DBUS_1
static void
dvr_dbus_timer_cb( void *aux )
{
  dvr_entry_t *de;
  time_t result, start, max = 0;
  static time_t last_result = 0;

  lock_assert(&global_lock);

  /* find the maximum value */
  LIST_FOREACH(de, &dvrentries, de_global_link) {
    if (de->de_sched_state != DVR_SCHEDULED)
      continue;
    start = dvr_entry_get_start_time(de);
    if (dispatch_clock < start && start > max)
      max = start;
  }
  /* lower the maximum value */
  result = max;
  LIST_FOREACH(de, &dvrentries, de_global_link) {
    if (de->de_sched_state != DVR_SCHEDULED)
      continue;
    start = dvr_entry_get_start_time(de);
    if (dispatch_clock < start && start < result)
      result = start;
  }
  /* different? send it.... */
  if (result && result != last_result) {
    dbus_emit_signal_s64("/dvr", "next", result);
    last_result = result;
  }
}
#endif

/*
 * Completed
 */
static void
_dvr_entry_completed(dvr_entry_t *de)
{
  de->de_sched_state = DVR_COMPLETED;
#if ENABLE_INOTIFY
  dvr_inotify_add(de);
#endif
}

/**
 * Return printable status for a dvr entry
 */
const char *
dvr_entry_status(dvr_entry_t *de)
{
  switch(de->de_sched_state) {
  case DVR_SCHEDULED:
    return "Scheduled for recording";
    
  case DVR_RECORDING:

    switch(de->de_rec_state) {
    case DVR_RS_PENDING:
      return "Waiting for stream";
    case DVR_RS_WAIT_PROGRAM_START:
      return "Waiting for program start";
    case DVR_RS_RUNNING:
      return "Running";
    case DVR_RS_COMMERCIAL:
      return "Commercial break";
    case DVR_RS_ERROR:
      return streaming_code2txt(de->de_last_error);
    default:
      return "Invalid";
    }

  case DVR_COMPLETED:
    if(dvr_get_filesize(de) == -1)
      return "File Missing";
    if(de->de_last_error)
      return streaming_code2txt(de->de_last_error);
    else
      return "Completed OK";

  case DVR_MISSED_TIME:
    return "Time missed";

  default:
    return "Invalid";
  }
}


/**
 *
 */
const char *
dvr_entry_schedstatus(dvr_entry_t *de)
{
  switch(de->de_sched_state) {
  case DVR_SCHEDULED:
    return "scheduled";
  case DVR_RECORDING:
    if(de->de_last_error)
      return "recordingError";
    else
      return "recording";
  case DVR_COMPLETED:
    if(de->de_last_error || dvr_get_filesize(de) == -1)
      return "completedError";
    else
      return "completed";
  case DVR_MISSED_TIME:
    return "completedError";
  default:
    return "unknown";
  }
}

/**
 *
 */
static int
dvr_charset_update(dvr_config_t *cfg, const char *charset)
{
  const char *s, *id;
  int change = strcmp(cfg->dvr_charset ?: "", charset ?: "");

  free(cfg->dvr_charset);
  free(cfg->dvr_charset_id);
  s = charset ? charset : intlconv_filesystem_charset();
  id = intlconv_charset_id(s, 1, 1);
  cfg->dvr_charset = s ? strdup(s) : NULL;
  cfg->dvr_charset_id = id ? strdup(id) : NULL;
  return change;
}

/**
 *
 */
void
dvr_make_title(char *output, size_t outlen, dvr_entry_t *de)
{
  struct tm tm;
  char buf[40];
  dvr_config_t *cfg = de->de_config;

  if(cfg->dvr_flags & DVR_CHANNEL_IN_TITLE)
    snprintf(output, outlen, "%s-", DVR_CH_NAME(de));
  else
    output[0] = 0;
  
  snprintf(output + strlen(output), outlen - strlen(output),
	   "%s", lang_str_get(de->de_title, NULL));

  if(cfg->dvr_flags & DVR_EPISODE_BEFORE_DATE) {
    if(cfg->dvr_flags & DVR_EPISODE_IN_TITLE) {
      if(de->de_bcast && de->de_bcast->episode)
        epg_episode_number_format(de->de_bcast->episode,
                                  output + strlen(output),
                                  outlen - strlen(output),
                                  ".", "S%02d", NULL, "E%02d", NULL);
    }
  }

  if(cfg->dvr_flags & DVR_SUBTITLE_IN_TITLE) {
    if(de->de_bcast && de->de_bcast->episode && de->de_bcast->episode->subtitle)
      snprintf(output + strlen(output), outlen - strlen(output),
           ".%s", lang_str_get(de->de_bcast->episode->subtitle, NULL));
  }

  localtime_r(&de->de_start, &tm);
  
  if(cfg->dvr_flags & DVR_DATE_IN_TITLE) {
    strftime(buf, sizeof(buf), "%F", &tm);
    snprintf(output + strlen(output), outlen - strlen(output), ".%s", buf);
  }

  if(cfg->dvr_flags & DVR_TIME_IN_TITLE) {
    strftime(buf, sizeof(buf), "%H-%M", &tm);
    snprintf(output + strlen(output), outlen - strlen(output), ".%s", buf);
  }

  if(!(cfg->dvr_flags & DVR_EPISODE_BEFORE_DATE)) {
    if(cfg->dvr_flags & DVR_EPISODE_IN_TITLE) {
      if(de->de_bcast && de->de_bcast->episode)
        epg_episode_number_format(de->de_bcast->episode,
                                  output + strlen(output),
                                  outlen - strlen(output),
                                  ".", "S%02d", NULL, "E%02d", NULL);
    }
  }
}

static void
dvr_entry_set_timer(dvr_entry_t *de)
{
  time_t now, start, stop;
  dvr_config_t *cfg = de->de_config;

  time(&now);

  start = dvr_entry_get_start_time(de);
  stop  = dvr_entry_get_stop_time(de);

  if(now >= stop || de->de_dont_reschedule) {

    if(de->de_filename == NULL)
      de->de_sched_state = DVR_MISSED_TIME;
    else
      _dvr_entry_completed(de);
    gtimer_arm_abs(&de->de_timer, dvr_timer_expire, de, 
                   de->de_stop + cfg->dvr_retention_days * 86400);

  } else if (de->de_sched_state == DVR_RECORDING)  {

    gtimer_arm_abs(&de->de_timer, dvr_timer_stop_recording, de, stop);

  } else if (de->de_channel) {

    de->de_sched_state = DVR_SCHEDULED;

    tvhtrace("dvr", "entry timer scheduled for %"PRItime_t, start);
    gtimer_arm_abs(&de->de_timer, dvr_timer_start_recording, de, start);
#if ENABLE_DBUS_1
    gtimer_arm(&dvr_dbus_timer, dvr_dbus_timer_cb, NULL, 5);
#endif

  } else {

    de->de_sched_state = DVR_NOSTATE;

  }
}


/**
 * Find dvr entry using 'fuzzy' search
 */
static int
dvr_entry_fuzzy_match(dvr_entry_t *de, epg_broadcast_t *e)
{
  time_t t1, t2;
  const char *title1, *title2;

  /* Matching ID */
  if (de->de_dvb_eid && de->de_dvb_eid == e->dvb_eid)
    return 1;

  /* No title */
  if (!(title1 = epg_broadcast_get_title(e, NULL)))
    return 0;
  if (!(title2 = lang_str_get(de->de_title, NULL)))
    return 0;

  /* Wrong length (+/-20%) */
  t1 = de->de_stop - de->de_start;
  t2  = e->stop - e->start;
  if ( abs(t2 - t1) > (t1 / 5) )
    return 0;

  /* Outside of window (should it be configurable)? */
  if ( abs(e->start - de->de_start) > 86400 )
    return 0;
  
  /* Title match (or contains?) */
  return strcmp(title1, title2) == 0;
}

/**
 * Create the event from config
 */
dvr_entry_t *
dvr_entry_create(const char *uuid, htsmsg_t *conf)
{
  dvr_entry_t *de, *de2;
  int64_t start, stop;
  const char *s;

  if (conf) {
    if (htsmsg_get_s64(conf, "start", &start))
      return NULL;
    if (htsmsg_get_s64(conf, "stop", &stop))
      return NULL;
    if ((htsmsg_get_str(conf, "channel")) == NULL &&
        (htsmsg_get_str(conf, "channelname")) == NULL)
      return NULL;
  }

  de = calloc(1, sizeof(*de));

  if (idnode_insert(&de->de_id, uuid, &dvr_entry_class, IDNODE_SHORT_UUID)) {
    if (uuid)
      tvhwarn("dvr", "invalid entry uuid '%s'", uuid);
    free(de);
    return NULL;
  }

  de->de_mc = -1;
  idnode_load(&de->de_id, conf);

  /* special case, becaous PO_NOSAVE, load ignores it */
  if (de->de_title == NULL &&
      (s = htsmsg_get_str(conf, "disp_title")) != NULL)
    dvr_entry_class_disp_title_set(de, s);

  de->de_refcnt = 1;

  LIST_INSERT_HEAD(&dvrentries, de, de_global_link);

  if (de->de_channel) {
    LIST_FOREACH(de2, &de->de_channel->ch_dvrs, de_channel_link)
      if(de2 != de &&
         de2->de_start == de->de_start &&
         de2->de_sched_state != DVR_COMPLETED) {
        dvr_entry_destroy(de, 0);
        return NULL;
      }
  }

  dvr_entry_set_timer(de);
  htsp_dvr_entry_add(de);

  return de;
}

/**
 * Create the event
 */
static dvr_entry_t *
_dvr_entry_create(const char *config_uuid, epg_broadcast_t *e,
                  channel_t *ch, time_t start, time_t stop,
                  time_t start_extra, time_t stop_extra,
                  const char *title, const char *description,
                  const char *lang, epg_genre_t *content_type,
                  const char *creator, dvr_autorec_entry_t *dae,
                  dvr_prio_t pri)
{
  dvr_entry_t *de;
  char tbuf[64];
  struct tm tm;
  time_t t;
  lang_str_t *l;
  htsmsg_t *conf;

  conf = htsmsg_create_map();
  htsmsg_add_s64(conf, "start", start);
  htsmsg_add_s64(conf, "stop", stop);
  htsmsg_add_str(conf, "channel", idnode_uuid_as_str(&ch->ch_id));
  htsmsg_add_u32(conf, "pri", pri);
  htsmsg_add_str(conf, "config_name", config_uuid ?: "");
  htsmsg_add_s64(conf, "start_extra", start_extra);
  htsmsg_add_s64(conf, "stop_extra", stop_extra);
  htsmsg_add_str(conf, "creator", creator ?: "");
  if (e) {
    htsmsg_add_u32(conf, "dvb_eid", e->dvb_eid);
    if (e->episode && e->episode->title)
      lang_str_serialize(e->episode->title, conf, "title");
    if (e->description)
      lang_str_serialize(e->description, conf, "description");
    else if (e->episode && e->episode->description)
      lang_str_serialize(e->episode->description, conf, "description");
    else if (e->summary)
      lang_str_serialize(e->summary, conf, "description");
    else if (e->episode && e->episode->summary)
      lang_str_serialize(e->episode->summary, conf, "description");
  } else if (title) {
    l = lang_str_create();
    lang_str_add(l, title, lang, 0);
    lang_str_serialize(l, conf, "title");
    if (description) {
      l = lang_str_create();
      lang_str_add(l, description, lang, 0);
      lang_str_serialize(l, conf, "description");
    }
  }
  if (content_type)
    htsmsg_add_u32(conf, "content_type", content_type->code / 16);
  if (e)
    htsmsg_add_u32(conf, "broadcast", e->id);
  if (dae)
    htsmsg_add_str(conf, "autorec", idnode_uuid_as_str(&dae->dae_id));

  de = dvr_entry_create(NULL, conf);

  htsmsg_destroy(conf);

  if (de == NULL)
    return NULL;

  t = dvr_entry_get_start_time(de);
  localtime_r(&t, &tm);
  if (strftime(tbuf, sizeof(tbuf), "%F %T", &tm) <= 0)
    *tbuf = 0;

  tvhlog(LOG_INFO, "dvr", "entry %s \"%s\" on \"%s\" starting at %s, "
	 "scheduled for recording by \"%s\"",
         idnode_uuid_as_str(&de->de_id),
	 lang_str_get(de->de_title, NULL), DVR_CH_NAME(de), tbuf, creator);

  dvr_entry_save(de);
  return de;
}

/**
 *
 */
dvr_entry_t *
dvr_entry_create_htsp(const char *config_uuid,
                      channel_t *ch, time_t start, time_t stop,
                      time_t start_extra, time_t stop_extra,
                      const char *title,
                      const char *description, const char *lang,
                      epg_genre_t *content_type,
                      const char *creator, dvr_autorec_entry_t *dae,
                      dvr_prio_t pri)
{
  dvr_config_t *cfg = dvr_config_find_by_uuid(config_uuid);
  if (!cfg)
    cfg = dvr_config_find_by_name(config_uuid);
  return _dvr_entry_create(cfg ? idnode_uuid_as_str(&cfg->dvr_id) : NULL,
                           NULL,
                           ch, start, stop, start_extra, stop_extra,
                           title, description, lang, content_type,
                           creator, dae, pri);
}

/**
 *
 */
dvr_entry_t *
dvr_entry_create_by_event(const char *config_uuid,
                          epg_broadcast_t *e,
                          time_t start_extra, time_t stop_extra,
                          const char *creator, 
                          dvr_autorec_entry_t *dae, dvr_prio_t pri)
{
  if(!e->channel || !e->episode || !e->episode->title)
    return NULL;

  return _dvr_entry_create(config_uuid, e,
                           e->channel, e->start, e->stop,
                           start_extra, stop_extra,
                           NULL, NULL, NULL,
                           LIST_FIRST(&e->episode->genre),
                           creator, dae, pri);
}

/**
 *
 */
static int _dvr_duplicate_event ( epg_broadcast_t *e )
{
  dvr_entry_t *de;
  epg_episode_num_t empty_epnum;
  int has_epnum = 1;

  /* skip episode duplicate check below if no episode number */
  memset(&empty_epnum, 0, sizeof(empty_epnum));
  if (epg_episode_number_cmp(&empty_epnum, &e->episode->epnum) == 0)
    has_epnum = 0;

  LIST_FOREACH(de, &dvrentries, de_global_link) {
    if (de->de_bcast) {
      if (de->de_bcast->episode == e->episode) return 1;

      if (has_epnum) {
        int ep_dup_det = (de->de_config->dvr_flags & DVR_EPISODE_DUPLICATE_DETECTION);

        if (ep_dup_det) {
          const char* de_title = lang_str_get(de->de_bcast->episode->title, NULL);
          const char* e_title = lang_str_get(e->episode->title, NULL);

          /* duplicate if title and episode match */
          if (de_title && e_title && strcmp(de_title, e_title) == 0
              && epg_episode_number_cmp(&de->de_bcast->episode->epnum, &e->episode->epnum) == 0) {
            return 1;
          }
        }
      }
    }
  }
  return 0;
}

/**
 *
 */
void
dvr_entry_create_by_autorec(epg_broadcast_t *e, dvr_autorec_entry_t *dae)
{
  char buf[200];

  /* Dup detection */
  if (_dvr_duplicate_event(e)) return;

  if(dae->dae_creator) {
    snprintf(buf, sizeof(buf), "Auto recording by: %s", dae->dae_creator);
  } else {
    snprintf(buf, sizeof(buf), "Auto recording");
  }
  dvr_entry_create_by_event(dae->dae_config_name, e, 0, 0, buf, dae, dae->dae_pri);
}

/**
 *
 */
void
dvr_entry_dec_ref(dvr_entry_t *de)
{
  lock_assert(&global_lock);

  if(de->de_refcnt > 1) {
    de->de_refcnt--;
    return;
  }

  idnode_unlink(&de->de_id);

  if(de->de_autorec != NULL)
    LIST_REMOVE(de, de_autorec_link);

  if(de->de_config != NULL)
    LIST_REMOVE(de, de_config_link);

  free(de->de_filename);
  free(de->de_creator);
  if (de->de_title) lang_str_destroy(de->de_title);
  if (de->de_desc)  lang_str_destroy(de->de_desc);
  if (de->de_bcast) de->de_bcast->putref((epg_object_t*)de->de_bcast);

  free(de);
}

/**
 *
 */
static void
dvr_entry_destroy(dvr_entry_t *de, int delconf)
{
  if (delconf)
    hts_settings_remove("dvr/log/%s", idnode_uuid_as_str(&de->de_id));

  htsp_dvr_entry_delete(de);
  
#if ENABLE_INOTIFY
  dvr_inotify_del(de);
#endif

  gtimer_disarm(&de->de_timer);
#if ENABLE_DBUS_1
  gtimer_arm(&dvr_dbus_timer, dvr_dbus_timer_cb, NULL, 2);
#endif

  if (de->de_channel)
    LIST_REMOVE(de, de_channel_link);
  LIST_REMOVE(de, de_global_link);
  de->de_channel = NULL;
  free(de->de_channel_name);
  de->de_channel_name = NULL;

  dvr_entry_dec_ref(de);
}

/**
 *
 */
static void
dvr_entry_destroy_by_config(dvr_config_t *cfg, int delconf)
{
  dvr_entry_t *de;
  dvr_config_t *def = NULL;

  while ((de = LIST_FIRST(&cfg->dvr_entries)) != NULL) {
    LIST_REMOVE(de, de_config_link);
    if (!def)
      def = dvr_config_find_by_name_default("");
    de->de_config = def;
    if (def)
      LIST_INSERT_HEAD(&def->dvr_entries, de, de_config_link);
    if (delconf)
      dvr_entry_save(de);
  }
}

/**
 *
 */
static void
dvr_db_load(void)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;

  if((l = hts_settings_load("dvr/log")) != NULL) {
    HTSMSG_FOREACH(f, l) {
      if((c = htsmsg_get_map_by_field(f)) == NULL)
	continue;
      (void)dvr_entry_create(f->hmf_name, c);
    }
    htsmsg_destroy(l);
  }
}

/**
 *
 */
void
dvr_entry_save(dvr_entry_t *de)
{
  htsmsg_t *m = htsmsg_create_map();

  lock_assert(&global_lock);

  idnode_save(&de->de_id, m);
  hts_settings_save(m, "dvr/log/%s", idnode_uuid_as_str(&de->de_id));
  htsmsg_destroy(m);
}


/**
 *
 */
static void
dvr_timer_expire(void *aux)
{
  dvr_entry_t *de = aux;
  dvr_entry_destroy(de, 1);
 
}

static dvr_entry_t *_dvr_entry_update
  ( dvr_entry_t *de, epg_broadcast_t *e,
    const char *title, const char *desc, const char *lang, 
    time_t start, time_t stop, time_t start_extra, time_t stop_extra )
{
  int save = 0;

  if (!dvr_entry_is_editable(de))
    return de;

  /* Start/Stop */
  if (e) {
    start = e->start;
    stop  = e->stop;
  }
  if (start && (start != de->de_start)) {
    de->de_start = start;
    save = 1;
  }
  if (stop && (stop != de->de_stop)) {
    de->de_stop = stop;
    save = 1;
  }
  if (start_extra && (start_extra != de->de_start_extra)) {
    de->de_start_extra = start_extra;
    save = 1;
  }
  if (stop_extra && (stop_extra != de->de_stop_extra)) {
    de->de_stop_extra = stop_extra;
    save = 1;
  }
  if (save)
    dvr_entry_set_timer(de);

  /* Title */ 
  if (e && e->episode && e->episode->title) {
    if (de->de_title) lang_str_destroy(de->de_title);
    de->de_title = lang_str_copy(e->episode->title);
  } else if (title) {
    if (!de->de_title) de->de_title = lang_str_create();
    save = lang_str_add(de->de_title, title, lang, 1);
  }
  
  /* EID */
  if (e && e->dvb_eid != de->de_dvb_eid) {
    de->de_dvb_eid = e->dvb_eid;
    save = 1;
  }

  // TODO: description

  /* Genre */
  if (e && e->episode) {
    epg_genre_t *g = LIST_FIRST(&e->episode->genre);
    if (g && (g->code / 16) != de->de_content_type) {
      de->de_content_type = g->code / 16;
      save = 1;
    }
  }

  /* Broadcast */
  if (e && (de->de_bcast != e)) {
    if (de->de_bcast)
      de->de_bcast->putref(de->de_bcast);
    de->de_bcast = e;
    e->getref(e);
    save = 1;
  }

  /* Save changes */
  if (save) {
    idnode_changed(&de->de_id);
    htsp_dvr_entry_update(de);
    tvhlog(LOG_INFO, "dvr", "\"%s\" on \"%s\": Updated Timer",
           lang_str_get(de->de_title, NULL), DVR_CH_NAME(de));
  }

  return de;
}

/**
 *
 */
dvr_entry_t * 
dvr_entry_update
  (dvr_entry_t *de,
   const char* de_title, const char *de_desc, const char *lang,
   time_t de_start, time_t de_stop,
   time_t de_start_extra, time_t de_stop_extra) 
{
  return _dvr_entry_update(de, NULL, de_title, de_desc, lang, 
                           de_start, de_stop, de_start_extra, de_stop_extra);
}

/**
 * Used to notify the DVR that an event has been replaced in the EPG
 */
void 
dvr_event_replaced(epg_broadcast_t *e, epg_broadcast_t *new_e)
{
  dvr_entry_t *de;
  assert(e != NULL);
  assert(new_e != NULL);

  /* Ignore */
  if ( e == new_e ) return;

  /* Existing entry */
  if ((de = dvr_entry_find_by_event(e))) {
    tvhtrace("dvr",
             "dvr entry %s event replaced %s on %s @ %"PRItime_t
             " to %"PRItime_t,
             idnode_uuid_as_str(&de->de_id),
             epg_broadcast_get_title(e, NULL),
             channel_get_name(e->channel),
             e->start, e->stop);

    /* Ignore - already in progress */
    if (de->de_sched_state != DVR_SCHEDULED)
      return;

    /* Unlink the broadcast */
    e->putref(e);
    de->de_bcast = NULL;

    /* If this was craeted by autorec - just remove it, it'll get recreated */
    if (de->de_autorec) {
      dvr_entry_destroy(de, 1);

    /* Find match */
    } else {
      RB_FOREACH(e, &e->channel->ch_epg_schedule, sched_link) {
        if (dvr_entry_fuzzy_match(de, e)) {
          tvhtrace("dvr",
                   "  replacement event %s on %s @ %"PRItime_t
                   " to %"PRItime_t,
                   epg_broadcast_get_title(e, NULL),
                   channel_get_name(e->channel),
                   e->start, e->stop);
          e->getref(e);
          de->de_bcast = e;
          _dvr_entry_update(de, e, NULL, NULL, NULL, 0, 0, 0, 0);
          break;
        }
      }
    }
  }
}

void dvr_event_updated ( epg_broadcast_t *e )
{
  dvr_entry_t *de;
  de = dvr_entry_find_by_event(e);
  if (de)
    _dvr_entry_update(de, e, NULL, NULL, NULL, 0, 0, 0, 0);
  else {
    LIST_FOREACH(de, &dvrentries, de_global_link) {
      if (de->de_sched_state != DVR_SCHEDULED) continue;
      if (de->de_bcast) continue;
      if (de->de_channel != e->channel) continue;
      if (dvr_entry_fuzzy_match(de, e)) {
        tvhtrace("dvr",
                 "dvr entry %s link to event %s on %s @ %"PRItime_t
                 " to %"PRItime_t,
                 idnode_uuid_as_str(&de->de_id),
                 epg_broadcast_get_title(e, NULL),
                 channel_get_name(e->channel),
                 e->start, e->stop);
        e->getref(e);
        de->de_bcast = e;
        _dvr_entry_update(de, e, NULL, NULL, NULL, 0, 0, 0, 0);
        break;
      }
    }
  }
}

/**
 *
 */
static void
dvr_stop_recording(dvr_entry_t *de, int stopcode, int saveconf)
{
  dvr_config_t *cfg = de->de_config;

  if (de->de_rec_state == DVR_RS_PENDING ||
      de->de_rec_state == DVR_RS_WAIT_PROGRAM_START ||
      de->de_filename == NULL)
    de->de_sched_state = DVR_MISSED_TIME;
  else
    _dvr_entry_completed(de);

  dvr_rec_unsubscribe(de, stopcode);

  tvhlog(LOG_INFO, "dvr", "\"%s\" on \"%s\": "
	 "End of program: %s",
	 lang_str_get(de->de_title, NULL), DVR_CH_NAME(de),
	 dvr_entry_status(de));

  if (saveconf)
    dvr_entry_save(de);
  idnode_notify_simple(&de->de_id);
  htsp_dvr_entry_update(de);

  gtimer_arm_abs(&de->de_timer, dvr_timer_expire, de, 
		 de->de_stop + cfg->dvr_retention_days * 86400);
}


/**
 *
 */
static void
dvr_timer_stop_recording(void *aux)
{
  dvr_stop_recording(aux, 0, 1);
}



/**
 *
 */
static void
dvr_timer_start_recording(void *aux)
{
  dvr_entry_t *de = aux;

  de->de_sched_state = DVR_RECORDING;
  de->de_rec_state = DVR_RS_PENDING;

  tvhlog(LOG_INFO, "dvr", "\"%s\" on \"%s\" recorder starting",
	 lang_str_get(de->de_title, NULL), DVR_CH_NAME(de));

  idnode_changed(&de->de_id);
  htsp_dvr_entry_update(de);
  dvr_rec_subscribe(de);

  gtimer_arm_abs(&de->de_timer, dvr_timer_stop_recording, de, 
		 de->de_stop + (60 * de->de_stop_extra));
}


/**
 *
 */
dvr_entry_t *
dvr_entry_find_by_id(int id)
{
  dvr_entry_t *de;
  LIST_FOREACH(de, &dvrentries, de_global_link)
    if(idnode_get_short_uuid(&de->de_id) == id)
      break;
  return de;  
}


/**
 *
 */
dvr_entry_t *
dvr_entry_find_by_event(epg_broadcast_t *e)
{
  dvr_entry_t *de;

  if(!e->channel) return NULL;

  LIST_FOREACH(de, &e->channel->ch_dvrs, de_channel_link)
    if(de->de_bcast == e) return de;
  return NULL;
}

/*
 * Find DVR entry based on an episode
 */
dvr_entry_t *
dvr_entry_find_by_episode(epg_broadcast_t *e)
{
  if (e->episode) {
    dvr_entry_t *de;
    epg_broadcast_t *ebc;
    LIST_FOREACH(ebc, &e->episode->broadcasts, ep_link) {
      de = dvr_entry_find_by_event(ebc);
      if (de) return de;
    }
    return NULL;
  } else {
    return dvr_entry_find_by_event(e);
  }
}

/**
 *
 */
dvr_entry_t *
dvr_entry_cancel(dvr_entry_t *de)
{
  switch(de->de_sched_state) {
  case DVR_SCHEDULED:
    dvr_entry_destroy(de, 1);
    return NULL;

  case DVR_RECORDING:
    de->de_dont_reschedule = 1;
    dvr_stop_recording(de, SM_CODE_ABORTED, 1);
    return de;

  case DVR_COMPLETED:
    dvr_entry_destroy(de, 1);
    return NULL;

  case DVR_MISSED_TIME:
    dvr_entry_destroy(de, 1);
    return NULL;

  default:
    abort();
  }
}


/**
 * Unconditionally remove an entry
 */
static void
dvr_entry_purge(dvr_entry_t *de, int delconf)
{
  if(de->de_sched_state == DVR_RECORDING)
    dvr_stop_recording(de, SM_CODE_SOURCE_DELETED, delconf);
}


/* **************************************************************************
 * DVR Entry Class definition
 * **************************************************************************/

static void
dvr_entry_class_save(idnode_t *self)
{
  dvr_entry_t *de = (dvr_entry_t *)self;
  dvr_entry_save(de);
  if (dvr_entry_is_valid(de))
    dvr_entry_set_timer(de);
}

static void
dvr_entry_class_delete(idnode_t *self)
{
  dvr_entry_cancel_delete((dvr_entry_t *)self);
}

static const char *
dvr_entry_class_get_title (idnode_t *self)
{
  dvr_entry_t *de = (dvr_entry_t *)self;
  const char *s;
  s = lang_str_get(de->de_title, NULL);
  if (s == NULL || s[0] == '\0')
    s = lang_str_get(de->de_desc, NULL);
  return s;
}

static int
dvr_entry_class_time_set(dvr_entry_t *de, time_t *v, time_t nv)
{
  if (!dvr_entry_is_editable(de))
    return 0;
  if (nv != *v) {
    *v = nv;
    return 1;
  }
  return 0;
}

static int
dvr_entry_class_int_set(dvr_entry_t *de, int *v, int nv)
{
  if (!dvr_entry_is_editable(de))
    return 0;
  if (nv != *v) {
    *v = nv;
    return 1;
  }
  return 0;
}

static int
dvr_entry_class_start_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  return dvr_entry_class_time_set(de, &de->de_start, *(time_t *)v);
}

static uint32_t
dvr_entry_class_start_opts(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de && !dvr_entry_is_editable(de))
    return PO_RDONLY;
  return 0;
}

static uint32_t
dvr_entry_class_start_extra_opts(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de && !dvr_entry_is_editable(de))
    return PO_RDONLY | PO_DURATION;
  return PO_DURATION;
}

static int
dvr_entry_class_start_extra_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  return dvr_entry_class_time_set(de, &de->de_start_extra, *(time_t *)v);
}

static int
dvr_entry_class_stop_set(void *o, const void *_v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  time_t v = *(time_t *)_v;

  if (!dvr_entry_is_editable(de)) {
    if (v < dispatch_clock)
      v = dispatch_clock;
  }
  if (v < de->de_start)
    v = de->de_start;
  return dvr_entry_class_time_set(de, &de->de_stop, v);
}

static int
dvr_entry_class_config_name_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  dvr_config_t *cfg;

  if (!dvr_entry_is_editable(de))
    return 0;
  cfg = v ? dvr_config_find_by_uuid(v) : NULL;
  if (!cfg)
    cfg = dvr_config_find_by_name_default(v);
  if (cfg == NULL) {
    if (de->de_config) {
      LIST_REMOVE(de, de_config_link);
      de->de_config = NULL;
      return 1;
    }
  } else if (de->de_config != cfg) {
    if (de->de_config)
      LIST_REMOVE(de, de_config_link);
    de->de_config = cfg;
    LIST_INSERT_HEAD(&cfg->dvr_entries, de, de_config_link);
    return 1;
  }
  return 0;
}

static const void *
dvr_entry_class_config_name_get(void *o)
{
  static const char *ret;
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_config)
    ret = idnode_uuid_as_str(&de->de_config->dvr_id);
  else
    ret = "";
  return &ret;
}

static int
dvr_entry_class_channel_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  channel_t   *ch;

  if (!dvr_entry_is_editable(de))
    return 0;
  ch = v ? channel_find_by_uuid(v) : NULL;
  if (!de->de_config) {
    de->de_config = dvr_config_find_by_name_default("");
    if (de->de_config)
      LIST_INSERT_HEAD(&de->de_config->dvr_entries, de, de_config_link);
  }
  if (ch == NULL) {
    if (de->de_channel) {
      LIST_REMOVE(de, de_channel_link);
      free(de->de_channel_name);
      de->de_channel_name = NULL;
      de->de_channel = NULL;
      return 1;
    }
  } else if (de->de_channel != ch) {
    if (de->de_channel)
      LIST_REMOVE(de, de_channel_link);
    free(de->de_channel_name);
    de->de_channel_name = strdup(channel_get_name(ch));
    de->de_channel = ch;
    LIST_INSERT_HEAD(&ch->ch_dvrs, de, de_channel_link);
    return 1;
  }
  return 0;
}

static const void *
dvr_entry_class_channel_get(void *o)
{
  static const char *ret;
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_channel)
    ret = idnode_uuid_as_str(&de->de_channel->ch_id);
  else
    ret = "";
  return &ret;
}

static htsmsg_t *
dvr_entry_class_channel_list(void *o)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "channel/list");
  htsmsg_add_str(m, "event", "channel");
  return m;
}

static int
dvr_entry_class_channel_name_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  channel_t   *ch;
  if (!dvr_entry_is_editable(de))
    return 0;
  if (!de->de_config) {
    de->de_config = dvr_config_find_by_name_default("");
    if (de->de_config)
      LIST_INSERT_HEAD(&de->de_config->dvr_entries, de, de_config_link);
  }
  if (!strcmp(de->de_channel_name ?: "", v ?: ""))
    return 0;
  ch = v ? channel_find_by_name(v) : NULL;
  if (ch) {
    return dvr_entry_class_channel_set(o, idnode_uuid_as_str(&ch->ch_id));
  } else {
    free(de->de_channel_name);
    de->de_channel_name = strdup(v);
    return 1;
  }
}

static const void *
dvr_entry_class_channel_name_get(void *o)
{
  static const char *ret;
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_channel)
    ret = channel_get_name(de->de_channel);
  else
    ret = de->de_channel_name;
  return &ret;
}

static int
dvr_entry_class_pri_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  return dvr_entry_class_int_set(de, &de->de_pri, *(int *)v);
}

htsmsg_t *
dvr_entry_class_pri_list ( void *o )
{
  static const struct strtab tab[] = {
    { "Not set",                  -1 },
    { "Important",                DVR_PRIO_IMPORTANT },
    { "High",                     DVR_PRIO_HIGH, },
    { "Normal",                   DVR_PRIO_NORMAL },
    { "Low",                      DVR_PRIO_LOW },
    { "Unimportant",              DVR_PRIO_UNIMPORTANT },
  };
  return strtab2htsmsg(tab);
}

static int
dvr_entry_class_mc_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  return dvr_entry_class_int_set(de, &de->de_mc, *(int *)v);
}

static htsmsg_t *
dvr_entry_class_mc_list ( void *o )
{
  static const struct strtab tab[] = {
    { "Not set",                       -1 },
    { "Matroska (mkv)",                MC_MATROSKA, },
    { "Same as source (pass through)", MC_PASS, },
#if ENABLE_LIBAV
    { "MPEG-TS",                       MC_MPEGTS },
    { "MPEG-PS (DVD)",                 MC_MPEGPS },
#endif
  };
  return strtab2htsmsg(tab);
}

htsmsg_t *
dvr_entry_class_config_name_list(void *o)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_t *p = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "idnode/load");
  htsmsg_add_str(m, "event", "dvrconfig");
  htsmsg_add_u32(p, "enum",  1);
  htsmsg_add_str(p, "class", dvr_config_class.ic_class);
  htsmsg_add_msg(m, "params", p);
  return m;
}


static int
dvr_entry_class_autorec_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  dvr_autorec_entry_t *dae;
  if (!dvr_entry_is_editable(de))
    return 0;
  dae = v ? dvr_autorec_find_by_uuid(v) : NULL;
  if (dae == NULL) {
    if (de->de_autorec) {
      LIST_REMOVE(de, de_autorec_link);
      de->de_autorec = NULL;
      return 1;
    }
  } else if (de->de_autorec != dae) {
    de->de_autorec = dae;
    LIST_INSERT_HEAD(&dae->dae_spawns, de, de_autorec_link);
    return 1;
  }
  return 0;
}

static const void *
dvr_entry_class_autorec_get(void *o)
{
  static const char *ret;
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_autorec)
    ret = idnode_uuid_as_str(&de->de_autorec->dae_id);
  else
    ret = "";
  return &ret;
}

static char *
dvr_entry_class_autorec_rend(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  const char *s = "";
  if (de->de_autorec) {
    if (de->de_autorec->dae_name != NULL &&
        de->de_autorec->dae_name[0] != '\0')
      s = de->de_autorec->dae_name;
    else if (de->de_autorec->dae_comment != NULL &&
        de->de_autorec->dae_comment[0] != '\0')
      s = de->de_autorec->dae_comment;
    else
      s = de->de_autorec->dae_title;
  }
  return strdup(s);
}

static int
dvr_entry_class_broadcast_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  uint32_t id = *(uint32_t *)v;
  epg_broadcast_t *bcast;
  if (!dvr_entry_is_editable(de))
    return 0;
  bcast = epg_broadcast_find_by_id(id, de->de_channel);
  if (bcast == NULL) {
    if (de->de_bcast) {
      de->de_bcast->putref((epg_object_t*)de->de_bcast);
      de->de_bcast = NULL;
      return 1;
    }
  } else if (de->de_bcast != bcast) {
    if (de->de_bcast)
      de->de_bcast->putref((epg_object_t*)de->de_bcast);
    de->de_bcast = bcast;
    de->de_bcast->getref((epg_object_t*)bcast);
    return 1;
  }
  return 0;
}

static const void *
dvr_entry_class_broadcast_get(void *o)
{
  static uint32_t id;
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_bcast)
    id = de->de_bcast->id;
  else
    id = 0;
  return &id;
}

static char *
dvr_entry_class_broadcast_rend(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  const char *s = "";
  if (de->de_bcast)
    s = lang_str_get(de->de_bcast->summary, NULL);
  return strdup(s);
}

static int
dvr_entry_class_disp_title_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  const char *s = "";
  if (v == NULL || *((char *)v) == '\0')
    v = "UnknownTitle";
  if (de->de_title)
    s = lang_str_get(de->de_title, NULL);
  if (strcmp(s, v ?: "")) {
    lang_str_destroy(de->de_title);
    de->de_title = lang_str_create();
    if (v)
      lang_str_add(de->de_title, v, NULL, 0);
    return 1;
  }
  return 0;
}

static const void *
dvr_entry_class_disp_title_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  static const char *s;
  s = "";
  if (de->de_title) {
    s = lang_str_get(de->de_title, NULL);
    if (s == NULL)
      s = "";
  }
  return &s;
}

static const void *
dvr_entry_class_disp_description_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  static const char *s;
  s = "";
  if (de->de_title) {
    s = lang_str_get(de->de_desc, NULL);
    if (s == NULL)
      s = "";
  }
  return &s;
}

static const void *
dvr_entry_class_episode_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  static const char *s;
  static char buf[100];
  s = "";
  if (de->de_bcast && de->de_bcast->episode)
    if (epg_episode_number_format(de->de_bcast->episode,
                                  buf, sizeof(buf), NULL,
                                  "Season %d", ".", "Episode %d", "/%d"))
      s = buf;
  return &s;
}

static const void *
dvr_entry_class_url_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  static const char *s;
  static char buf[100];
  s = "";
  if (de->de_sched_state == DVR_COMPLETED) {
    snprintf(buf, sizeof(buf), "dvrfile/%s", idnode_uuid_as_str(&de->de_id));
    s = buf;
  }
  return &s;
}

static const void *
dvr_entry_class_filesize_get(void *o)
{
  static int64_t size;
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_sched_state == DVR_COMPLETED)
    size = dvr_get_filesize(de);
  else
    size = 0;
  return &size;
}

static const void *
dvr_entry_class_start_real_get(void *o)
{
  static time_t tm;
  dvr_entry_t *de = (dvr_entry_t *)o;
  tm = dvr_entry_get_start_time(de);
  return &tm;
}

static const void *
dvr_entry_class_stop_real_get(void *o)
{
  static time_t tm;
  dvr_entry_t *de = (dvr_entry_t *)o;
  tm  = dvr_entry_get_stop_time(de);
  return &tm;
}

static const void *
dvr_entry_class_duration_get(void *o)
{
  static time_t tm;
  time_t start, stop;
  dvr_entry_t *de = (dvr_entry_t *)o;
  start = dvr_entry_get_start_time(de);
  stop  = dvr_entry_get_stop_time(de);
  if (stop > start)
    tm = stop - start;
  else
    tm = 0;
  return &tm;
}

static const void *
dvr_entry_class_status_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  static const char *s;
  static char buf[100];
  strncpy(buf, dvr_entry_status(de), sizeof(buf));
  buf[sizeof(buf)-1] = '\0';
  s = buf;
  return &s;
}

static const void *
dvr_entry_class_sched_status_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  static const char *s;
  static char buf[100];
  strncpy(buf, dvr_entry_schedstatus(de), sizeof(buf));
  buf[sizeof(buf)-1] = '\0';
  s = buf;
  return &s;
}

static const void *
dvr_entry_class_channel_icon_url_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  channel_t *ch = de->de_channel;
  static const char *s;
  static char buf[256];
  uint32_t id;
  if (ch == NULL) {
    s = "";
  } else if ((id = imagecache_get_id(ch->ch_icon)) != 0) {
    snprintf(buf, sizeof(buf), "imagecache/%d", id);
  } else {
    strncpy(buf, ch->ch_icon ?: "", sizeof(buf));
    buf[strlen(buf)-1] = '\0';
  }
  s = buf;
  return &s;
}

htsmsg_t *
dvr_entry_class_duration_list(void *o, const char *not_set, int max)
{
  int i;
  htsmsg_t *e, *l = htsmsg_create_list();
  char buf[32];
  e = htsmsg_create_map();
  htsmsg_add_u32(e, "key", 0);
  htsmsg_add_str(e, "val", not_set);
  htsmsg_add_msg(l, NULL, e);
  for (i = 1; i <= 120;  i++) {
    snprintf(buf, sizeof(buf), "%d min%s", i, i > 1 ? "s" : "");
    e = htsmsg_create_map();
    htsmsg_add_u32(e, "key", i);
    htsmsg_add_str(e, "val", buf);
    htsmsg_add_msg(l, NULL, e);
  }
  for (i = 120; i <= max;  i += 30) {
    if ((i % 60) == 0)
      snprintf(buf, sizeof(buf), "%d hrs", i / 60);
    else
      snprintf(buf, sizeof(buf), "%d hrs %d min%s", i / 60, i % 60, (i % 60) > 0 ? "s" : "");
    e = htsmsg_create_map();
    htsmsg_add_u32(e, "key", i);
    htsmsg_add_str(e, "val", buf);
    htsmsg_add_msg(l, NULL, e);
  }
  return l;
}

static htsmsg_t *
dvr_entry_class_extra_list(void *o)
{
  return dvr_entry_class_duration_list(o, "Not set (use channel or DVR config)", 4*60);
}
                                        
static htsmsg_t *
dvr_entry_class_content_type_list(void *o)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "epg/content_type/list");
  return m;
}

const idclass_t dvr_entry_class = {
  .ic_class     = "dvrentry",
  .ic_caption   = "DVR Entry",
  .ic_event     = "dvrentry",
  .ic_save      = dvr_entry_class_save,
  .ic_get_title = dvr_entry_class_get_title,
  .ic_delete    = dvr_entry_class_delete,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_TIME,
      .id       = "start",
      .name     = "Start Time",
      .set      = dvr_entry_class_start_set,
      .off      = offsetof(dvr_entry_t, de_start),
      .get_opts = dvr_entry_class_start_opts,
    },
    {
      .type     = PT_TIME,
      .id       = "start_extra",
      .name     = "Extra Start Time",
      .off      = offsetof(dvr_entry_t, de_start_extra),
      .set      = dvr_entry_class_start_extra_set,
      .list     = dvr_entry_class_extra_list,
      .get_opts = dvr_entry_class_start_extra_opts,
      .opts     = PO_DURATION,
    },
    {
      .type     = PT_TIME,
      .id       = "start_real",
      .name     = "Scheduled Start Time",
      .get      = dvr_entry_class_start_real_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_TIME,
      .id       = "stop",
      .name     = "Stop Time",
      .set      = dvr_entry_class_stop_set,
      .off      = offsetof(dvr_entry_t, de_stop),
    },
    {
      .type     = PT_TIME,
      .id       = "stop_extra",
      .name     = "Extra Stop Time",
      .off      = offsetof(dvr_entry_t, de_stop_extra),
      .list     = dvr_entry_class_extra_list,
      .opts     = PO_DURATION,
    },
    {
      .type     = PT_TIME,
      .id       = "stop_real",
      .name     = "Scheduled Stop Time",
      .get      = dvr_entry_class_stop_real_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_TIME,
      .id       = "duration",
      .name     = "Duration",
      .get      = dvr_entry_class_duration_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_DURATION,
    },
    {
      .type     = PT_STR,
      .id       = "channel",
      .name     = "Channel",
      .set      = dvr_entry_class_channel_set,
      .get      = dvr_entry_class_channel_get,
      .list     = dvr_entry_class_channel_list,
      .get_opts = dvr_entry_class_start_opts,
    },
    {
      .type     = PT_STR,
      .id       = "channel_icon",
      .name     = "Channel Icon",
      .get      = dvr_entry_class_channel_icon_url_get,
      .opts     = PO_HIDDEN | PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "channelname",
      .name     = "Channel Name",
      .get      = dvr_entry_class_channel_name_get,
      .set      = dvr_entry_class_channel_name_set,
      .off      = offsetof(dvr_entry_t, de_channel_name),
    },
    {
      .type     = PT_LANGSTR,
      .id       = "title",
      .name     = "Title",
      .off      = offsetof(dvr_entry_t, de_title),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "disp_title",
      .name     = "Title",
      .get      = dvr_entry_class_disp_title_get,
      .set      = dvr_entry_class_disp_title_set,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_LANGSTR,
      .id       = "description",
      .name     = "Description",
      .off      = offsetof(dvr_entry_t, de_desc),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "disp_description",
      .name     = "Description",
      .get      = dvr_entry_class_disp_description_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN,
    },
    {
      .type     = PT_INT,
      .id       = "pri",
      .name     = "Priority",
      .off      = offsetof(dvr_entry_t, de_pri),
      .def.i    = DVR_PRIO_NORMAL,
      .set      = dvr_entry_class_pri_set,
      .list     = dvr_entry_class_pri_list,
    },
    {
      .type     = PT_INT,
      .id       = "container",
      .name     = "Container",
      .off      = offsetof(dvr_entry_t, de_mc),
      .def.i    = MC_MATROSKA,
      .set      = dvr_entry_class_mc_set,
      .list     = dvr_entry_class_mc_list,
    },
    {
      .type     = PT_STR,
      .id       = "config_name",
      .name     = "DVR Configuration",
      .set      = dvr_entry_class_config_name_set,
      .get      = dvr_entry_class_config_name_get,
      .list     = dvr_entry_class_config_name_list,
      .get_opts = dvr_entry_class_start_opts,
    },
    {
      .type     = PT_STR,
      .id       = "creator",
      .name     = "Creator",
      .off      = offsetof(dvr_entry_t, de_creator),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "filename",
      .name     = "Filename",
      .off      = offsetof(dvr_entry_t, de_filename),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_U32,
      .id       = "errorcode",
      .name     = "Error Code",
      .off      = offsetof(dvr_entry_t, de_last_error),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_U32,
      .id       = "errors",
      .name     = "Errors",
      .off      = offsetof(dvr_entry_t, de_errors),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_U16,
      .id       = "dvb_eid",
      .name     = "DVB EPG ID",
      .off      = offsetof(dvr_entry_t, de_dvb_eid),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_BOOL,
      .id       = "noresched",
      .name     = "Do Not Reschedule",
      .off      = offsetof(dvr_entry_t, de_dvb_eid),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "autorec",
      .name     = "Auto Record",
      .set      = dvr_entry_class_autorec_set,
      .get      = dvr_entry_class_autorec_get,
      .rend     = dvr_entry_class_autorec_rend,
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_U32,
      .id       = "content_type",
      .name     = "Content Type",
      .list     = dvr_entry_class_content_type_list,
      .off      = offsetof(dvr_entry_t, de_content_type),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_U32,
      .id       = "broadcast",
      .name     = "Broadcast Type",
      .set      = dvr_entry_class_broadcast_set,
      .get      = dvr_entry_class_broadcast_get,
      .rend     = dvr_entry_class_broadcast_rend,
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "episode",
      .name     = "Episode",
      .get      = dvr_entry_class_episode_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN,
    },
    {
      .type     = PT_STR,
      .id       = "url",
      .name     = "URL",
      .get      = dvr_entry_class_url_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN,
    },
    {
      .type     = PT_S64,
      .id       = "filesize",
      .name     = "File Size",
      .get      = dvr_entry_class_filesize_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "status",
      .name     = "Status",
      .get      = dvr_entry_class_status_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN,
    },
    {
      .type     = PT_STR,
      .id       = "sched_status",
      .name     = "Schedule Status",
      .get      = dvr_entry_class_sched_status_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN,
    },
    {}
  }
};

/**
 *
 */
void
dvr_destroy_by_channel(channel_t *ch, int delconf)
{
  dvr_entry_t *de;

  while((de = LIST_FIRST(&ch->ch_dvrs)) != NULL) {
    LIST_REMOVE(de, de_channel_link);
    de->de_channel = NULL;
    de->de_channel_name = strdup(channel_get_name(ch));
    dvr_entry_purge(de, delconf);
  }
}

/**
 * find a dvr config by name, return NULL if not found
 */
dvr_config_t *
dvr_config_find_by_name(const char *name)
{
  dvr_config_t *cfg;

  if (name == NULL)
    name = "";

  LIST_FOREACH(cfg, &dvrconfigs, config_link)
    if (!strcmp(name, cfg->dvr_config_name))
      return cfg;

  return NULL;
}

/**
 * find a dvr config by name, return the default config if not found
 */
dvr_config_t *
dvr_config_find_by_name_default(const char *name)
{
  dvr_config_t *cfg;

  cfg = dvr_config_find_by_name(name);

  if (cfg == NULL) {
    if (name && name[0])
      tvhlog(LOG_WARNING, "dvr", "Configuration '%s' not found", name);
    cfg = dvr_config_find_by_name("");
  } else if (!cfg->dvr_enabled) {
    tvhlog(LOG_WARNING, "dvr", "Configuration '%s' not enabled", name);
    cfg = dvr_config_find_by_name("");
  }

  if (cfg == NULL) {
    cfg = dvr_config_create("", NULL, NULL);
    dvr_config_save(cfg);
  }

  return cfg;
}

/*
 *
 */
static void
dvr_config_update_flags(dvr_config_t *cfg)
{
  int r = 0;
  if (cfg->dvr_dir_per_day)          r |= DVR_DIR_PER_DAY;
  if (cfg->dvr_channel_dir)          r |= DVR_DIR_PER_CHANNEL;
  if (cfg->dvr_channel_in_title)     r |= DVR_CHANNEL_IN_TITLE;
  if (cfg->dvr_date_in_title)        r |= DVR_DATE_IN_TITLE;
  if (cfg->dvr_time_in_title)        r |= DVR_TIME_IN_TITLE;
  if (cfg->dvr_whitespace_in_title)  r |= DVR_WHITESPACE_IN_TITLE;
  if (cfg->dvr_title_dir)            r |= DVR_DIR_PER_TITLE;
  if (cfg->dvr_episode_in_title)     r |= DVR_EPISODE_IN_TITLE;
  if (cfg->dvr_clean_title)          r |= DVR_CLEAN_TITLE;
  if (cfg->dvr_tag_files)            r |= DVR_TAG_FILES;
  if (cfg->dvr_skip_commercials)     r |= DVR_SKIP_COMMERCIALS;
  if (cfg->dvr_subtitle_in_title)    r |= DVR_SUBTITLE_IN_TITLE;
  if (cfg->dvr_episode_before_date)  r |= DVR_EPISODE_BEFORE_DATE;
  if (cfg->dvr_episode_duplicate)    r |= DVR_EPISODE_DUPLICATE_DETECTION;
  cfg->dvr_flags = r | DVR_FLAGS_VALID;
  r = 0;
  if (cfg->dvr_rewrite_pat)          r |= MC_REWRITE_PAT;
  if (cfg->dvr_rewrite_pmt)          r |= MC_REWRITE_PMT;
  cfg->dvr_muxcnf.m_flags = r;
}

/**
 * create a new named dvr config; the caller is responsible
 * to avoid duplicates
 */
dvr_config_t *
dvr_config_create(const char *name, const char *uuid, htsmsg_t *conf)
{
  dvr_config_t *cfg;

  if (name == NULL)
    name = "";

  cfg = calloc(1, sizeof(dvr_config_t));
  LIST_INIT(&cfg->dvr_entries);
  LIST_INIT(&cfg->dvr_accesses);

  if (idnode_insert(&cfg->dvr_id, uuid, &dvr_config_class, 0)) {
    if (uuid)
      tvherror("dvr", "invalid config uuid '%s'", uuid);
    free(cfg);
    return NULL;
  }

  cfg->dvr_enabled = 1;
  if (name)
    cfg->dvr_config_name = strdup(name);
  cfg->dvr_retention_days = 31;
  cfg->dvr_mc = MC_MATROSKA;
  cfg->dvr_flags = DVR_TAG_FILES | DVR_SKIP_COMMERCIALS;
  dvr_charset_update(cfg, intlconv_filesystem_charset());

  /* series link support */
  cfg->dvr_sl_brand_lock   = 1; // use brand linking
  cfg->dvr_sl_season_lock  = 0; // ignore season (except if no brand)
  cfg->dvr_sl_channel_lock = 1; // channel locked
  cfg->dvr_sl_time_lock    = 0; // time slot (approx) locked
  cfg->dvr_sl_more_recent  = 1; // Only record more reason episodes
  cfg->dvr_sl_quality_lock = 1; // Don't attempt to ajust quality

  /* Muxer config */
  cfg->dvr_muxcnf.m_cache  = MC_CACHE_DONTKEEP;
  cfg->dvr_muxcnf.m_flags |= MC_REWRITE_PAT;

  /* dup detect */
  cfg->dvr_dup_detect_episode = 1; // detect dup episodes

  /* Default recording file and directory permissions */

  cfg->dvr_muxcnf.m_file_permissions      = 0664;
  cfg->dvr_muxcnf.m_directory_permissions = 0775;

  if (conf) {
    idnode_load(&cfg->dvr_id, conf);
    if (dvr_config_is_default(cfg))
      cfg->dvr_enabled = 1;
    dvr_config_update_flags(cfg);
  }
  
  tvhlog(LOG_INFO, "dvr", "Creating new configuration '%s'", cfg->dvr_config_name);

  LIST_INSERT_HEAD(&dvrconfigs, cfg, config_link);

  return cfg;
}

/**
 * destroy a dvr config
 */
static void
dvr_config_destroy(dvr_config_t *cfg, int delconf)
{
  if (delconf) {
    tvhlog(LOG_INFO, "dvr", "Deleting configuration '%s'", 
           cfg->dvr_config_name);
    hts_settings_remove("dvr/config/%s", idnode_uuid_as_str(&cfg->dvr_id));
  }
  LIST_REMOVE(cfg, config_link);
  idnode_unlink(&cfg->dvr_id);

  dvr_entry_destroy_by_config(cfg, delconf);
  access_destroy_by_dvr_config(cfg, delconf);

  free(cfg->dvr_charset_id);
  free(cfg->dvr_charset);
  free(cfg->dvr_storage);
  free(cfg->dvr_config_name);
  free(cfg);
}

/**
 *
 */
void
dvr_config_delete(const char *name)
{
  dvr_config_t *cfg;

  if (name == NULL || strlen(name) == 0) {
    tvhlog(LOG_WARNING,"dvr","Attempt to delete default config ignored");
    return;
  }

  cfg = dvr_config_find_by_name(name);
  if (cfg != NULL)
    dvr_config_destroy(cfg, 1);
}

/*
 *
 */
void
dvr_config_save(dvr_config_t *cfg)
{
  htsmsg_t *m = htsmsg_create_map();

  lock_assert(&global_lock);

  idnode_save(&cfg->dvr_id, m);
  hts_settings_save(m, "dvr/config/%s", idnode_uuid_as_str(&cfg->dvr_id));
  htsmsg_destroy(m);
}

/* **************************************************************************
 * DVR Config Class definition
 * **************************************************************************/

static void
dvr_config_class_save(idnode_t *self)
{
  dvr_config_t *cfg = (dvr_config_t *)self;
  if (dvr_config_is_default(cfg))
    cfg->dvr_enabled = 1;
  dvr_config_update_flags(cfg);
  dvr_config_save(cfg);
}

static void
dvr_config_class_delete(idnode_t *self)
{
  dvr_config_t *cfg = (dvr_config_t *)self;
  if (!dvr_config_is_default(cfg))
      dvr_config_destroy(cfg, 1);
}

static int
dvr_config_class_perm(idnode_t *self, access_t *a, htsmsg_t *msg_to_write)
{
  dvr_config_t *cfg = (dvr_config_t *)self;
  htsmsg_field_t *f;
  const char *uuid, *my_uuid;

  if (access_verify2(a, ACCESS_RECORDER))
    return -1;
  if (!access_verify2(a, ACCESS_ADMIN))
    return 0;
  if (a->aa_dvrcfgs) {
    my_uuid = idnode_uuid_as_str(&cfg->dvr_id);
    HTSMSG_FOREACH(f, a->aa_dvrcfgs) {
      uuid = htsmsg_field_get_str(f) ?: "";
      if (!strcmp(uuid, my_uuid))
        goto fine;
    }
    return -1;
  }
fine:
  if (strcmp(cfg->dvr_config_name ?: "", a->aa_username ?: ""))
    return -1;
  return 0;
}

static int
dvr_config_class_enabled_set(void *o, const void *v)
{
  dvr_config_t *cfg = (dvr_config_t *)o;
  if (dvr_config_is_default(cfg) && dvr_config_is_valid(cfg))
    return 0;
  if (cfg->dvr_enabled != *(int *)v) {
    cfg->dvr_enabled = *(int *)v;
    return 1;
  }
  return 0;
}

static uint32_t
dvr_config_class_enabled_opts(void *o)
{
  dvr_config_t *cfg = (dvr_config_t *)o;
  if (cfg && dvr_config_is_default(cfg) && dvr_config_is_valid(cfg))
    return PO_RDONLY;
  return 0;
}

static int
dvr_config_class_name_set(void *o, const void *v)
{
  dvr_config_t *cfg = (dvr_config_t *)o;
  if (dvr_config_is_default(cfg) && dvr_config_is_valid(cfg))
    return 0;
  if (strcmp(cfg->dvr_config_name ?: "", v ?: "")) {
    if (dvr_config_is_valid(cfg) && (v == NULL || *(char *)v == '\0'))
      return 0;
    free(cfg->dvr_config_name);
    cfg->dvr_config_name = strdup(v);
    return 1;
  }
  return 0;
}

static const char *
dvr_config_class_get_title (idnode_t *self)
{
  dvr_config_t *cfg = (dvr_config_t *)self;
  if (cfg->dvr_config_name && cfg->dvr_config_name[0] != '\0')
    return cfg->dvr_config_name;
  return "(Default Profile)";
}

static int
dvr_config_class_charset_set(void *o, const void *v)
{
  dvr_config_t *cfg = (dvr_config_t *)o;
  return dvr_charset_update(cfg, v);
}

static htsmsg_t *
dvr_config_class_charset_list(void *o)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "intlconv/charsets");
  return m;
}

static htsmsg_t *
dvr_config_class_cache_list(void *o)
{
  static struct strtab tab[] = {
    { "Unknown",            MC_CACHE_UNKNOWN },
    { "System",             MC_CACHE_SYSTEM },
    { "Do not keep",        MC_CACHE_DONTKEEP },
    { "Sync",               MC_CACHE_SYNC },
    { "Sync + Do not keep", MC_CACHE_SYNCDONTKEEP }
  };
  return strtab2htsmsg(tab);
}

const idclass_t dvr_config_class = {
  .ic_class      = "dvrconfig",
  .ic_caption    = "DVR Configuration Profile",
  .ic_event      = "dvrconfig",
  .ic_save       = dvr_config_class_save,
  .ic_get_title  = dvr_config_class_get_title,
  .ic_delete     = dvr_config_class_delete,
  .ic_perm       = dvr_config_class_perm,
  .ic_groups     = (const property_group_t[]) {
      {
         .name   = "DVR Behaviour",
         .number = 1,
      },
      {
         .name   = "Recording File Options",
         .number = 2,
      },
      {
         .name   = "Subdirectory Options",
         .number = 3,
      },
      {
         .name   = "Filename Options",
         .number = 4,
         .column = 1,
      },
      {
         .name   = "",
         .number = 5,
         .parent = 4,
         .column = 2,
      },
      {}
  },
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = "Enabled",
      .set      = dvr_config_class_enabled_set,
      .off      = offsetof(dvr_config_t, dvr_enabled),
      .def.i    = 1,
      .group    = 1,
      .get_opts = dvr_config_class_enabled_opts,
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = "Config Name",
      .set      = dvr_config_class_name_set,
      .off      = offsetof(dvr_config_t, dvr_config_name),
      .def.s    = "! New config",
      .group    = 1,
      .get_opts = dvr_config_class_enabled_opts,
    },
    {
      .type     = PT_INT,
      .id       = "container",
      .name     = "Container",
      .off      = offsetof(dvr_config_t, dvr_mc),
      .def.i    = MC_MATROSKA,
      .list     = dvr_entry_class_mc_list,
      .group    = 1,
    },
    {
      .type     = PT_INT,
      .id       = "cache",
      .name     = "Cache Scheme",
      .off      = offsetof(dvr_config_t, dvr_muxcnf.m_cache),
      .def.i    = MC_CACHE_DONTKEEP,
      .list     = dvr_config_class_cache_list,
      .group    = 1,
    },
    {
      .type     = PT_U32,
      .id       = "retention-days",
      .name     = "DVR Log Retention Time (days)",
      .off      = offsetof(dvr_config_t, dvr_retention_days),
      .def.u32  = 31,
      .group    = 1,
    },
    {
      .type     = PT_U32,
      .id       = "pre-extra-time",
      .name     = "Extra Time Before Recordings (minutes)",
      .off      = offsetof(dvr_config_t, dvr_extra_time_pre),
      .group    = 1,
    },
    {
      .type     = PT_U32,
      .id       = "post-extra-time",
      .name     = "Extra Time After Recordings (minutes)",
      .off      = offsetof(dvr_config_t, dvr_extra_time_post),
      .group    = 1,
    },
    {
      .type     = PT_BOOL,
      .id       = "episode-duplicate-detection",
      .name     = "Episode Duplicate Detect",
      .off      = offsetof(dvr_config_t, dvr_episode_duplicate),
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "postproc",
      .name     = "Post-Processor Command",
      .off      = offsetof(dvr_config_t, dvr_postproc),
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "storage",
      .name     = "Recording System Path",
      .off      = offsetof(dvr_config_t, dvr_storage),
      .group    = 2,
    },
    {
      .type     = PT_PERM,
      .id       = "file-permissions",
      .name     = "File Permissions (octal, e.g. 0664)",
      .off      = offsetof(dvr_config_t, dvr_muxcnf.m_file_permissions),
      .def.u32  = 0664,
      .group    = 2,
    },
    {
      .type     = PT_STR,
      .id       = "charset",
      .name     = "Filename Charset",
      .off      = offsetof(dvr_config_t, dvr_charset),
      .set      = dvr_config_class_charset_set,
      .list     = dvr_config_class_charset_list,
      .def.s    = "UTF-8",
      .group    = 2,
    },
    {
      .type     = PT_BOOL,
      .id       = "rewrite-pat",
      .name     = "Rewrite PAT",
      .off      = offsetof(dvr_config_t, dvr_rewrite_pat),
      .def.i    = 1,
      .group    = 2,
    },
    {
      .type     = PT_BOOL,
      .id       = "rewrite-pmt",
      .name     = "Rewrite PMT",
      .off      = offsetof(dvr_config_t, dvr_rewrite_pmt),
      .group    = 2,
    },
    {
      .type     = PT_BOOL,
      .id       = "tag-files",
      .name     = "Tag Files With Metadata",
      .off      = offsetof(dvr_config_t, dvr_tag_files),
      .def.i    = 1,
      .group    = 2,
    },
    {
      .type     = PT_BOOL,
      .id       = "skip-commercials",
      .name     = "Skip Commercials",
      .off      = offsetof(dvr_config_t, dvr_skip_commercials),
      .def.i    = 1,
      .group    = 2,
    },
    {
      .type     = PT_PERM,
      .id       = "directory-permissions",
      .name     = "Directory Permissions (octal, e.g. 0775)",
      .off      = offsetof(dvr_config_t, dvr_muxcnf.m_directory_permissions),
      .def.u32  = 0775,
      .group    = 3,
    },
    {
      .type     = PT_BOOL,
      .id       = "day-dir",
      .name     = "Make Subdirectories Per Day",
      .off      = offsetof(dvr_config_t, dvr_dir_per_day),
      .group    = 3,
    },
    {
      .type     = PT_BOOL,
      .id       = "channel-dir",
      .name     = "Make Subdirectories Per Channel",
      .off      = offsetof(dvr_config_t, dvr_channel_dir),
      .group    = 3,
    },
    {
      .type     = PT_BOOL,
      .id       = "title-dir",
      .name     = "Make Subdirectories Per Title",
      .off      = offsetof(dvr_config_t, dvr_title_dir),
      .group    = 3,
    },
    {
      .type     = PT_BOOL,
      .id       = "channel-in-title",
      .name     = "Include Channel Name In Filename",
      .off      = offsetof(dvr_config_t, dvr_channel_in_title),
      .group    = 4,
    },
    {
      .type     = PT_BOOL,
      .id       = "date-in-title",
      .name     = "Include Date In Filename",
      .off      = offsetof(dvr_config_t, dvr_date_in_title),
      .group    = 4,
    },
    {
      .type     = PT_BOOL,
      .id       = "time-in-title",
      .name     = "Include Time In Filename",
      .off      = offsetof(dvr_config_t, dvr_time_in_title),
      .group    = 4,
    },
    {
      .type     = PT_BOOL,
      .id       = "episode-in-title",
      .name     = "Include Episode In Filename",
      .off      = offsetof(dvr_config_t, dvr_episode_in_title),
      .group    = 4,
    },
    {
      .type     = PT_BOOL,
      .id       = "subtitle-in-title",
      .name     = "Include Subtitle In Filename",
      .off      = offsetof(dvr_config_t, dvr_subtitle_in_title),
      .group    = 5,
    },
    {
      .type     = PT_BOOL,
      .id       = "episode-before-date",
      .name     = "Put Episode In Filename Before Date And Time",
      .off      = offsetof(dvr_config_t, dvr_episode_before_date),
      .group    = 5,
    },
    {
      .type     = PT_BOOL,
      .id       = "clean-title",
      .name     = "Remove All Unsafe Characters From Filename",
      .off      = offsetof(dvr_config_t, dvr_clean_title),
      .group    = 5,
    },
    {
      .type     = PT_BOOL,
      .id       = "whitespace-in-title",
      .name     = "Replace Whitespace In Title with '-'",
      .off      = offsetof(dvr_config_t, dvr_whitespace_in_title),
      .group    = 5,
    },
    {}
  },
};

/**
 *
 */
static void
dvr_query_add_entry(dvr_query_result_t *dqr, dvr_entry_t *de)
{
  if(dqr->dqr_entries == dqr->dqr_alloced) {
    /* Need to alloc more space */

    dqr->dqr_alloced = MAX(100, dqr->dqr_alloced * 2);
    dqr->dqr_array = realloc(dqr->dqr_array, 
			     dqr->dqr_alloced * sizeof(dvr_entry_t *));
  }
  dqr->dqr_array[dqr->dqr_entries++] = de;
}

void
dvr_query_filter(dvr_query_result_t *dqr, dvr_entry_filter filter)
{
  dvr_entry_t *de;

  memset(dqr, 0, sizeof(dvr_query_result_t));

  LIST_FOREACH(de, &dvrentries, de_global_link)
    if (filter(de))
      dvr_query_add_entry(dqr, de);
}

static int all_filter(dvr_entry_t *entry)
{
  return 1;
}

/**
 *
 */
void
dvr_query(dvr_query_result_t *dqr)
{
  return dvr_query_filter(dqr, all_filter);
}

/**
 *
 */
void
dvr_query_free(dvr_query_result_t *dqr)
{
  free(dqr->dqr_array);
}

/**
 * Sorting functions
 */
int
dvr_sort_start_descending(const void *A, const void *B)
{
  dvr_entry_t *a = *(dvr_entry_t **)A;
  dvr_entry_t *b = *(dvr_entry_t **)B;
  return b->de_start - a->de_start;
}

int
dvr_sort_start_ascending(const void *A, const void *B)
{
  return -dvr_sort_start_descending(A, B);
}


/**
 *
 */
void
dvr_query_sort_cmp(dvr_query_result_t *dqr, dvr_entry_comparator sf)
{
  if(dqr->dqr_array == NULL)
    return;

  qsort(dqr->dqr_array, dqr->dqr_entries, sizeof(dvr_entry_t *), sf);
}  

void
dvr_query_sort(dvr_query_result_t *dqr)
{
  dvr_query_sort_cmp(dqr, dvr_sort_start_descending);
}

/**
 *
 */
int64_t
dvr_get_filesize(dvr_entry_t *de)
{
  struct stat st;

  if(de->de_filename == NULL)
    return -1;

  if(stat(de->de_filename, &st) != 0)
    return -1;

  return st.st_size;
}



/**
 *
 */
static struct strtab priotab[] = {
  { "important",   DVR_PRIO_IMPORTANT },
  { "high",        DVR_PRIO_HIGH },
  { "normal",      DVR_PRIO_NORMAL },
  { "low",         DVR_PRIO_LOW },
  { "unimportant", DVR_PRIO_UNIMPORTANT },
};

dvr_prio_t
dvr_pri2val(const char *s)
{
  return str2val_def(s, priotab, DVR_PRIO_NORMAL);
}

const char *
dvr_val2pri(dvr_prio_t v)
{
  return val2str(v, priotab) ?: "invalid";
}


/**
 *
 */
void
dvr_entry_delete(dvr_entry_t *de)
{
  if(de->de_filename != NULL) {
#if ENABLE_INOTIFY
    dvr_inotify_del(de);
#endif
    if(unlink(de->de_filename) && errno != ENOENT)
      tvhlog(LOG_WARNING, "dvr", "Unable to remove file '%s' from disk -- %s",
	     de->de_filename, strerror(errno));

    /* Also delete directories, if they were created for the recording and if they are empty */

    dvr_config_t *cfg = de->de_config;
    char path[500];

    snprintf(path, sizeof(path), "%s", cfg->dvr_storage);

    if(cfg->dvr_flags & DVR_DIR_PER_TITLE || cfg->dvr_flags & DVR_DIR_PER_CHANNEL || cfg->dvr_flags & DVR_DIR_PER_DAY) {
      char *p;
      int l;

      l = strlen(de->de_filename);
      p = alloca(l + 1);
      memcpy(p, de->de_filename, l);
      p[l--] = 0;

      for(; l >= 0; l--) {
        if(p[l] == '/') {
          p[l] = 0;
          if(strncmp(p, cfg->dvr_storage, strlen(p)) == 0)
            break;
          if(rmdir(p) == -1)
            break;
        }
      }
    }

  }
  dvr_entry_destroy(de, 1);
}

/**
 *
 */
void
dvr_entry_cancel_delete(dvr_entry_t *de)
{
  switch(de->de_sched_state) {
  case DVR_SCHEDULED:
    dvr_entry_destroy(de, 1);
    break;

  case DVR_RECORDING:
    de->de_dont_reschedule = 1;
    dvr_stop_recording(de, SM_CODE_ABORTED, 1);
  case DVR_COMPLETED:
    dvr_entry_delete(de);
    break;

  case DVR_MISSED_TIME:
    dvr_entry_destroy(de, 1);
    break;

  default:
    abort();
  }
}

/**
 *
 */
void
dvr_config_init(void)
{
  htsmsg_t *m, *l;
  htsmsg_field_t *f;
  char buf[500];
  const char *homedir;
  struct stat st;
  dvr_config_t *cfg;

  dvr_iov_max = sysconf(_SC_IOV_MAX);

  /* Default settings */

  LIST_INIT(&dvrconfigs);

  if ((l = hts_settings_load("dvr/config")) != NULL) {
    HTSMSG_FOREACH(f, l) {
      if ((m = htsmsg_get_map_by_field(f)) == NULL) continue;
      (void)dvr_config_create(NULL, f->hmf_name, m);
    }
    htsmsg_destroy(l);
  }

  /* Create the default entry */

  cfg = dvr_config_find_by_name_default("");
  assert(cfg);

  LIST_FOREACH(cfg, &dvrconfigs, config_link) {
    if(cfg->dvr_storage == NULL || !strlen(cfg->dvr_storage)) {
      /* Try to figure out a good place to put them videos */

      homedir = getenv("HOME");

      if(homedir != NULL) {
        snprintf(buf, sizeof(buf), "%s/Videos", homedir);
        if(stat(buf, &st) == 0 && S_ISDIR(st.st_mode))
          cfg->dvr_storage = strdup(buf);
        
        else if(stat(homedir, &st) == 0 && S_ISDIR(st.st_mode))
          cfg->dvr_storage = strdup(homedir);
        else
          cfg->dvr_storage = strdup(getcwd(buf, sizeof(buf)));
      }

      tvhlog(LOG_WARNING, "dvr",
             "Output directory for video recording is not yet configured "
             "for DVR configuration \"%s\". "
             "Defaulting to to \"%s\". "
             "This can be changed from the web user interface.",
             cfg->dvr_config_name, cfg->dvr_storage);
    }
  }
}

void
dvr_init(void)
{
#if ENABLE_INOTIFY
  dvr_inotify_init();
#endif
  dvr_autorec_init();
  dvr_db_load();
  dvr_autorec_update();
}

/**
 *
 */
void
dvr_done(void)
{
  dvr_config_t *cfg;
  dvr_entry_t *de;

#if ENABLE_INOTIFY
  dvr_inotify_done();
#endif
  pthread_mutex_lock(&global_lock);
  while ((de = LIST_FIRST(&dvrentries)) != NULL)
    dvr_entry_destroy(de, 0);
  while ((cfg = LIST_FIRST(&dvrconfigs)) != NULL)
    dvr_config_destroy(cfg, 0);
  pthread_mutex_unlock(&global_lock);
  dvr_autorec_done();
}
