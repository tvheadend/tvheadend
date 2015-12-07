/*
 *  Digital Video Recorder
 *  Copyright (C) 2008 Andreas Öman
 *  Copyright (C) 2014,2015 Jaroslav Kysela
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
#include "notify.h"

struct dvr_entry_list dvrentries;
static int dvr_in_init;

#if ENABLE_DBUS_1
static gtimer_t dvr_dbus_timer;
#endif

static void dvr_entry_set_timer(dvr_entry_t *de);
static void dvr_timer_rerecord(void *aux);
static void dvr_timer_expire(void *aux);
static void dvr_timer_remove_files(void *aux);
static void dvr_entry_start_recording(dvr_entry_t *de, int clone);
static void dvr_timer_start_recording(void *aux);
static void dvr_timer_stop_recording(void *aux);
static int dvr_entry_rerecord(dvr_entry_t *de);

/*
 *
 */
int
dvr_entry_verify(dvr_entry_t *de, access_t *a, int readonly)
{
  if (access_verify2(a, ACCESS_FAILED_RECORDER) &&
      (de->de_sched_state == DVR_COMPLETED &&
       de->de_last_error != SM_CODE_OK))
    return -1;

  if (readonly && !access_verify2(a, ACCESS_ALL_RECORDER))
    return 0;

  if (!access_verify2(a, ACCESS_ALL_RW_RECORDER))
    return 0;

  if (strcmp(de->de_owner ?: "", a->aa_username ?: ""))
    return -1;
  return 0;
}

/*
 *
 */
int
dvr_entry_set_state(dvr_entry_t *de, dvr_entry_sched_state_t state,
                    dvr_rs_state_t rec_state, int error_code)
{
  char id[16];
  if (de->de_sched_state != state ||
      de->de_rec_state != rec_state ||
      de->de_last_error != error_code) {
    if (de->de_bcast) {
      snprintf(id, sizeof(id), "%u", de->de_bcast->id);
      notify_delayed(id, "epg", "dvr_update");
    }
    de->de_sched_state = state;
    de->de_rec_state = rec_state;
    de->de_last_error = error_code;
    idnode_notify_changed(&de->de_id);
    htsp_dvr_entry_update(de);
    return 1;
  }
  return 0;
}

/*
 *
 */
static int
dvr_entry_assign_broadcast(dvr_entry_t *de, epg_broadcast_t *bcast)
{
  char id[16];
  if (bcast != de->de_bcast) {
    if (de->de_bcast) {
      snprintf(id, sizeof(id), "%u", de->de_bcast->id);
      de->de_bcast->putref((epg_object_t*)de->de_bcast);
      notify_delayed(id, "epg", "dvr_delete");
      de->de_bcast = NULL;
      de->de_dvb_eid = 0;
    }
    if (bcast) {
      bcast->getref((epg_object_t*)bcast);
      de->de_bcast = bcast;
      snprintf(id, sizeof(id), "%u", bcast->id);
      notify_delayed(id, "epg", "dvr_update");
    }
    return 1;
  }
  return 0;
}

/*
 *
 */
static void
dvr_entry_dont_rerecord(dvr_entry_t *de, int dont_rerecord)
{
  dont_rerecord = dont_rerecord ? 1 : 0;
  if (de->de_dont_rerecord ? 1 : 0 != dont_rerecord) {
    de->de_dont_rerecord = dont_rerecord;
    dvr_entry_save(de);
    idnode_notify_changed(&de->de_id);
    htsp_dvr_entry_update(de);
  }
}

/*
 *
 */
static int
dvr_entry_change_parent_child(dvr_entry_t *parent, dvr_entry_t *child, void *origin, int save)
{
  dvr_entry_t *p;

  if (parent == NULL && child == NULL)
    return 0;
  if (parent && parent->de_child == child) {
    assert(child == NULL || child->de_parent == parent);
    return 0;
  }
  if (child && child->de_parent == parent) {
    assert(parent == NULL || parent->de_child == child);
    return 0;
  }
  if (child == NULL) {
    if (parent->de_child) {
      p = parent->de_child->de_parent;
      parent->de_child->de_parent = NULL;
      if (save && p && p != origin) dvr_entry_save(p);
      parent->de_child = NULL;
      if (save && origin != parent) dvr_entry_save(parent);
      return 1;
    }
    return 0;
  }
  if (parent == NULL) {
    if (child->de_parent) {
      p = child->de_parent->de_child;
      child->de_parent->de_child = NULL;
      if (save && p && p != origin) dvr_entry_save(p);
      child->de_parent = NULL;
      if (save && origin != child) dvr_entry_save(child);
      return 1;
    }
    return 0;
  }
  if (parent->de_child) {
    p = parent->de_child->de_parent;
    parent->de_child->de_parent = NULL;
    if (save && p) dvr_entry_save(p);
  }
  if (child->de_parent) {
    p = child->de_parent->de_child;
    child->de_parent->de_child = NULL;
    if (save && p) dvr_entry_save(p);
  }
  parent->de_child = child;
  child->de_parent = parent;
  if (save && origin != parent) dvr_entry_save(parent);
  if (save && origin != child) dvr_entry_save(child);
  return 1;
}

/*
 * Start / stop time calculators
 */
static inline int extra_valid(time_t extra)
{
  return extra != 0 && extra != (time_t)-1;
}

static uint32_t
dvr_entry_warm_time( dvr_entry_t *de )
{
  return MIN(de->de_config->dvr_warm_time, 240);
}

time_t
dvr_entry_get_start_time( dvr_entry_t *de, int warm )
{
  return de->de_start - (60 * dvr_entry_get_extra_time_pre(de)) -
         (warm ? dvr_entry_warm_time(de) : 0);
}

time_t
dvr_entry_get_stop_time( dvr_entry_t *de )
{
  return de->de_stop + (60 * dvr_entry_get_extra_time_post(de));
}

time_t
dvr_entry_get_extra_time_pre( dvr_entry_t *de )
{
  time_t extra = de->de_start_extra;

  if (de->de_timerec)
    return 0;
  if (!extra_valid(extra)) {
    if (de->de_channel)
      extra = de->de_channel->ch_dvr_extra_time_pre;
    if (!extra_valid(extra))
      extra = de->de_config->dvr_extra_time_pre;
  }
  return extra;
}

time_t
dvr_entry_get_extra_time_post( dvr_entry_t *de )
{
  time_t extra = de->de_stop_extra;

  if (de->de_timerec)
    return 0;
  if (!extra_valid(extra)) {
    if (de->de_channel)
      extra = de->de_channel->ch_dvr_extra_time_post;
    if (!extra_valid(extra))
      extra = de->de_config->dvr_extra_time_post;
  }
  return extra;
}

const char *
dvr_entry_get_retention_string ( dvr_entry_t *de )
{
  char buf[24];
  uint32_t retention = dvr_entry_get_retention_days(de);

  if (retention < DVR_RET_ONREMOVE)
    snprintf(buf, sizeof(buf), "%i days", retention);
  else if (retention == DVR_RET_ONREMOVE)
    return strdup("On file removal");
  else
    return strdup("Forever");

  return strdup(buf);
}

const char *
dvr_entry_get_removal_string ( dvr_entry_t *de )
{
  char buf[24];
  uint32_t removal = dvr_entry_get_removal_days(de);

  if (removal < DVR_RET_SPACE)
    snprintf(buf, sizeof(buf), "%i days", removal);
  else if (removal == DVR_RET_SPACE)
    return strdup("Until space needed");
  else
    return strdup("Forever");

  return strdup(buf);
}

uint32_t
dvr_entry_get_retention_days( dvr_entry_t *de )
{
  if (de->de_retention > 0) {
    if (de->de_retention > DVR_RET_FOREVER)
      return DVR_RET_FOREVER;

    /* As we need the db entry when deleting the file on disk */
    if (dvr_entry_get_removal_days(de) != DVR_RET_FOREVER &&
        dvr_entry_get_removal_days(de) > de->de_retention)
      return DVR_RET_ONREMOVE;

    return de->de_retention;
  }
  return dvr_retention_cleanup(de->de_config->dvr_retention_days);
}

uint32_t
dvr_entry_get_removal_days ( dvr_entry_t *de )
{
  if (de->de_removal > 0) {
    if (de->de_removal > DVR_RET_FOREVER)
      return DVR_RET_FOREVER;

    return de->de_removal;
  }
  return dvr_retention_cleanup(de->de_config->dvr_removal_days);
}

uint32_t
dvr_entry_get_rerecord_errors( dvr_entry_t *de )
{
  return de->de_config->dvr_rerecord_errors;
}

int
dvr_entry_get_epg_running( dvr_entry_t *de )
{
  if (de->de_dvb_eid == 0)
    return 0;
  if (de->de_channel->ch_epg_running < 0)
    return de->de_config->dvr_running;
  return de->de_channel->ch_epg_running > 0;
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
    start = dvr_entry_get_start_time(de, 1);
    if (dispatch_clock < start && start > max)
      max = start;
  }
  /* lower the maximum value */
  result = max;
  LIST_FOREACH(de, &dvrentries, de_global_link) {
    if (de->de_sched_state != DVR_SCHEDULED)
      continue;
    start = dvr_entry_get_start_time(de, 1);
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
 *
 */
static void
dvr_entry_retention_arm(dvr_entry_t *de, gti_callback_t *cb, time_t when)
{
  uint32_t rerecord = dvr_entry_get_rerecord_errors(de);
  if (rerecord && (when - dispatch_clock) > 3600) {
    when = dispatch_clock + 3600;
    cb = dvr_timer_rerecord;
  }
  gtimer_arm_abs(&de->de_timer, cb, de, when);
}

/*
 *
 */
static void
dvr_entry_retention_timer(dvr_entry_t *de)
{
  time_t stop = de->de_stop;
  uint32_t removal = dvr_entry_get_removal_days(de);
  uint32_t retention = dvr_entry_get_retention_days(de);

  stop = de->de_stop + removal * (time_t)86400;
  if ((removal > 0 || retention == 0) && removal < DVR_RET_SPACE) {
    if (stop > dispatch_clock) {
      dvr_entry_retention_arm(de, dvr_timer_remove_files, stop);
      return;
    }
    if (dvr_get_filename(de))
      dvr_entry_delete(de, 1);    // delete actual file
    if (retention == DVR_RET_ONREMOVE) {
      dvr_entry_destroy(de, 1);   // also remove database entry
      return;
    }
  }

  if (retention  < DVR_RET_ONREMOVE) {
    stop = de->de_stop + retention * (time_t)86400;
    dvr_entry_retention_arm(de, dvr_timer_expire, stop);
  }
}

/*
 * No state
 */
static void
dvr_entry_nostate(dvr_entry_t *de, int error_code)
{
  dvr_entry_set_state(de, DVR_NOSTATE, DVR_RS_FINISHED, error_code);
  dvr_entry_retention_timer(de);
}

/*
 * Missed time
 */
static void
dvr_entry_missed_time(dvr_entry_t *de, int error_code)
{
  dvr_autorec_entry_t *dae = de->de_autorec;

  dvr_entry_set_state(de, DVR_MISSED_TIME, DVR_RS_FINISHED, error_code);
  dvr_entry_retention_timer(de);

  // Trigger autorec update in case of max schedules limit
  if (dae && dvr_autorec_get_max_sched_count(dae) > 0)
    dvr_autorec_changed(dae, 0);
}

/*
 * Completed
 */
static void
dvr_entry_completed(dvr_entry_t *de, int error_code)
{
  dvr_entry_set_state(de, DVR_COMPLETED, DVR_RS_FINISHED, error_code);
#if ENABLE_INOTIFY
  dvr_inotify_add(de);
#endif
  dvr_entry_retention_timer(de);
  if (de->de_autorec)
    dvr_autorec_completed(de, error_code);
}

/**
 * Return printable status for a dvr entry
 */
const char *
dvr_entry_status(dvr_entry_t *de)
{
  switch(de->de_sched_state) {
  case DVR_SCHEDULED:
    return N_("Scheduled for recording");
    
  case DVR_RECORDING:

    switch(de->de_rec_state) {
    case DVR_RS_PENDING:
      return N_("Waiting for stream");
    case DVR_RS_WAIT_PROGRAM_START:
      return N_("Waiting for program start");
    case DVR_RS_RUNNING:
      return N_("Running");
    case DVR_RS_COMMERCIAL:
      return N_("Commercial break");
    case DVR_RS_ERROR:
      return streaming_code2txt(de->de_last_error);
    case DVR_RS_EPG_WAIT:
      return N_("Waiting for EPG running flag");
    case DVR_RS_FINISHED:
      return N_("Finished");
    default:
      return N_("Invalid");
    }

  case DVR_COMPLETED:
    switch(de->de_last_error) {
      case SM_CODE_INVALID_TARGET:
        return N_("File not created");
      case SM_CODE_USER_ACCESS:
        return N_("User access error");
      case SM_CODE_USER_LIMIT:
        return N_("User limit reached");
      default:
        break;
    }
    if(dvr_get_filesize(de) == -1)
      return N_("File missing");
    if(de->de_last_error)
      return streaming_code2txt(de->de_last_error);
    else
      return N_("Completed OK");

  case DVR_MISSED_TIME:
    if (de->de_last_error == SM_CODE_SVC_NOT_ENABLED)
      return streaming_code2txt(de->de_last_error);
    return N_("Time missed");

  default:
    return N_("Invalid");
  }
}


/**
 *
 */
const char *
dvr_entry_schedstatus(dvr_entry_t *de)
{
  const char *s;
  uint32_t rerecord;

  switch(de->de_sched_state) {
  case DVR_SCHEDULED:
    s = "scheduled";
    break;
  case DVR_RECORDING:
    s = de->de_last_error ? "recordingError" : "recording";
    break;
  case DVR_COMPLETED:
    s = "completed";
    if(de->de_last_error || dvr_get_filesize(de) == -1)
      s = "completedError";
    rerecord = de->de_dont_rerecord ? 0 : dvr_entry_get_rerecord_errors(de);
    if(rerecord && (de->de_errors || de->de_data_errors > rerecord))
      s = "completedRerecord";
    break;
  case DVR_MISSED_TIME:
    s = "completedError";
    if (de->de_last_error == SM_CODE_SVC_NOT_ENABLED)
      s = "completedWarning";
    rerecord = de->de_dont_rerecord ? 0 : dvr_entry_get_rerecord_errors(de);
    if(rerecord)
      s = "completedRerecord";
    break;
  default:
    s = "unknown";
  }
  return s;
}

/**
 *
 */
uint32_t
dvr_usage_count(access_t *aa)
{
  dvr_entry_t *de;
  uint32_t used = 0;

  LIST_FOREACH(de, &dvrentries, de_global_link) {
    if (de->de_sched_state != DVR_RECORDING)
      continue;
    if (de->de_owner && de->de_owner[0]) {
      if (!strcmp(de->de_owner, aa->aa_username ?: ""))
        used++;
    } else if (!strcmp(de->de_creator ?: "", aa->aa_representative ?: "")) {
      used++;
    }
  }

  return used;
}

void
dvr_entry_set_timer(dvr_entry_t *de)
{
  time_t now, start, stop;

  if (dvr_in_init)
    return;

  time(&now);

  start = dvr_entry_get_start_time(de, 1);
  stop  = dvr_entry_get_stop_time(de);

  if (now >= stop || de->de_dont_reschedule) {

    /* EPG thinks that the program is running */
    if(de->de_running_start > de->de_running_stop) {
      stop = dispatch_clock + 10;
      goto recording;
    }

    if(htsmsg_is_empty(de->de_files))
      dvr_entry_missed_time(de, de->de_last_error);
    else
      dvr_entry_completed(de, de->de_last_error);

    if(dvr_entry_rerecord(de))
      return;

  } else if (de->de_sched_state == DVR_RECORDING)  {

recording:
    gtimer_arm_abs(&de->de_timer, dvr_timer_stop_recording, de, stop);

  } else if (de->de_channel && de->de_channel->ch_enabled) {

    dvr_entry_set_state(de, DVR_SCHEDULED, DVR_RS_PENDING, de->de_last_error);

    tvhtrace("dvr", "entry timer scheduled for %"PRItime_t, start);
    gtimer_arm_abs(&de->de_timer, dvr_timer_start_recording, de, start);
#if ENABLE_DBUS_1
    gtimer_arm(&dvr_dbus_timer, dvr_dbus_timer_cb, NULL, 5);
#endif

  } else {

    dvr_entry_nostate(de, de->de_last_error);

  }
}


/**
 * Get episode name
 */
static char *
dvr_entry_get_episode(epg_broadcast_t *bcast, char *buf, int len)
{
  if (!bcast || !bcast->episode)
    return NULL;
  if (epg_episode_number_format(bcast->episode,
                                buf, len, NULL,
                                _("Season %d"), ".", _("Episode %d"), "/%d"))
    return buf;
  return NULL;
}

/**
 * Find dvr entry using 'fuzzy' search
 */
static int
dvr_entry_fuzzy_match(dvr_entry_t *de, epg_broadcast_t *e, uint16_t eid, int64_t time_window)
{
  time_t t1, t2;
  const char *title1, *title2;
  char buf[64];

  /* Wrong length (+/-20%) */
  t1 = de->de_stop - de->de_start;
  t2 = e->stop - e->start;
  if (abs(t2 - t1) > (t1 / 5))
    return 0;

  /* Matching ID */
  if (de->de_dvb_eid && eid && de->de_dvb_eid == eid)
    return 1;

  /* No title */
  if (!(title1 = epg_broadcast_get_title(e, NULL)))
    return 0;
  if (!(title2 = lang_str_get(de->de_title, NULL)))
    return 0;

  /* Outside of window */
  if ((int64_t)llabs(e->start - de->de_start) > time_window)
    return 0;
  
  /* Title match (or contains?) */
  if (strcmp(title1, title2))
    return 0;

  /* episode check */
  if (dvr_entry_get_episode(e, buf, sizeof(buf)) && de->de_episode)
    if (strcmp(buf, de->de_episode))
      return 0;

  return 1;
}

/**
 * Create the event from config
 */
dvr_entry_t *
dvr_entry_create(const char *uuid, htsmsg_t *conf, int clone)
{
  dvr_entry_t *de, *de2;
  int64_t start, stop;
  htsmsg_t *m;
  char ubuf[UUID_HEX_SIZE];

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

  de->de_enabled = 1;
  de->de_config = dvr_config_find_by_name_default(NULL);
  if (de->de_config)
    LIST_INSERT_HEAD(&de->de_config->dvr_entries, de, de_config_link);

  idnode_load(&de->de_id, conf);

  /* filenames */
  m = htsmsg_get_list(conf, "files");
  if (m)
    de->de_files = htsmsg_copy(m);

  de->de_refcnt = 1;

  LIST_INSERT_HEAD(&dvrentries, de, de_global_link);

  if (de->de_channel && !de->de_dont_reschedule && !clone) {
    LIST_FOREACH(de2, &de->de_channel->ch_dvrs, de_channel_link)
      if(de2 != de &&
         de2->de_config == de->de_config &&
         de2->de_start == de->de_start &&
         de2->de_sched_state != DVR_COMPLETED &&
         de2->de_sched_state != DVR_MISSED_TIME &&
         strcmp(de2->de_owner ?: "", de->de_owner ?: "") == 0) {
        tvhlog(LOG_INFO, "dvr", "delete entry %s \"%s\" on \"%s\" start time %"PRId64", "
           "scheduled for recording by \"%s\" (duplicate with %s)",
          idnode_uuid_as_str(&de->de_id, ubuf),
          lang_str_get(de->de_title, NULL), DVR_CH_NAME(de),
          (int64_t)de2->de_start, de->de_creator ?: "",
          idnode_uuid_as_str(&de2->de_id, ubuf));
        dvr_entry_destroy(de, 0);
        return NULL;
      }
  }

  if (!clone)
    dvr_entry_set_timer(de);
  htsp_dvr_entry_add(de);

  return de;
}

/**
 * Create the event
 */
dvr_entry_t *
dvr_entry_create_(int enabled, const char *config_uuid, epg_broadcast_t *e,
                  channel_t *ch, time_t start, time_t stop,
                  time_t start_extra, time_t stop_extra,
                  const char *title, const char* subtitle, const char *description,
                  const char *lang, epg_genre_t *content_type,
                  const char *owner,
                  const char *creator, dvr_autorec_entry_t *dae,
                  dvr_timerec_entry_t *dte,
                  dvr_prio_t pri, int retention, int removal,
                  const char *comment)
{
  dvr_entry_t *de;
  char tbuf[64], *s;
  struct tm tm;
  time_t t;
  lang_str_t *l;
  htsmsg_t *conf;
  char ubuf[UUID_HEX_SIZE];

  conf = htsmsg_create_map();
  if (enabled >= 0)
    htsmsg_add_u32(conf, "enabled", !!enabled);
  htsmsg_add_s64(conf, "start", start);
  htsmsg_add_s64(conf, "stop", stop);
  htsmsg_add_str(conf, "channel", idnode_uuid_as_str(&ch->ch_id, ubuf));
  htsmsg_add_u32(conf, "pri", pri);
  htsmsg_add_u32(conf, "retention", retention);
  htsmsg_add_u32(conf, "removal", removal);
  htsmsg_add_str(conf, "config_name", config_uuid ?: "");
  htsmsg_add_s64(conf, "start_extra", start_extra);
  htsmsg_add_s64(conf, "stop_extra", stop_extra);
  htsmsg_add_str(conf, "owner", owner ?: "");
  htsmsg_add_str(conf, "creator", creator ?: "");
  htsmsg_add_str(conf, "comment", comment ?: "");
  if (e) {
    htsmsg_add_u32(conf, "dvb_eid", e->dvb_eid);
    if (e->episode && e->episode->title)
      lang_str_serialize(e->episode->title, conf, "title");
    if (e->episode && e->episode->subtitle)
      lang_str_serialize(e->episode->subtitle, conf, "subtitle");
    if (e->description)
      lang_str_serialize(e->description, conf, "description");
    else if (e->episode && e->episode->description)
      lang_str_serialize(e->episode->description, conf, "description");
    else if (e->summary)
      lang_str_serialize(e->summary, conf, "description");
    else if (e->episode && e->episode->summary)
      lang_str_serialize(e->episode->summary, conf, "description");
    if (e->episode && (s = dvr_entry_get_episode(e, tbuf, sizeof(tbuf))))
      htsmsg_add_str(conf, "episode", s);
  } else if (title) {
    l = lang_str_create();
    lang_str_add(l, title, lang, 0);
    lang_str_serialize(l, conf, "title");
    lang_str_destroy(l);
    if (description) {
      l = lang_str_create();
      lang_str_add(l, description, lang, 0);
      lang_str_serialize(l, conf, "description");
      lang_str_destroy(l);
    }
    if (subtitle) {
      l = lang_str_create();
      lang_str_add(l, subtitle, lang, 0);
      lang_str_serialize(l, conf, "subtitle");
      lang_str_destroy(l);
    }
  }
  if (content_type)
    htsmsg_add_u32(conf, "content_type", content_type->code / 16);
  if (e)
    htsmsg_add_u32(conf, "broadcast", e->id);
  if (dae)
  {
    htsmsg_add_str(conf, "autorec", idnode_uuid_as_str(&dae->dae_id, ubuf));
    htsmsg_add_str(conf, "directory", dae->dae_directory ?: "");
  }
  if (dte)
  {
    htsmsg_add_str(conf, "timerec", idnode_uuid_as_str(&dte->dte_id, ubuf));
    htsmsg_add_str(conf, "directory", dte->dte_directory ?: "");
  }

  de = dvr_entry_create(NULL, conf, 0);

  htsmsg_destroy(conf);

  if (de == NULL)
    return NULL;

  t = dvr_entry_get_start_time(de, 1);
  localtime_r(&t, &tm);
  if (strftime(tbuf, sizeof(tbuf), "%F %T", &tm) <= 0)
    *tbuf = 0;

  tvhlog(LOG_INFO, "dvr", "entry %s \"%s\" on \"%s\" starting at %s, "
	 "scheduled for recording by \"%s\"",
         idnode_uuid_as_str(&de->de_id, ubuf),
	 lang_str_get(de->de_title, NULL), DVR_CH_NAME(de), tbuf, creator ?: "");

  dvr_entry_save(de);
  return de;
}

/**
 *
 */
dvr_entry_t *
dvr_entry_create_htsp(int enabled, const char *config_uuid,
                      channel_t *ch, time_t start, time_t stop,
                      time_t start_extra, time_t stop_extra,
                      const char *title, const char* subtitle,
                      const char *description, const char *lang,
                      epg_genre_t *content_type,
                      const char *owner,
                      const char *creator, dvr_autorec_entry_t *dae,
                      dvr_prio_t pri, int retention, int removal,
                      const char *comment)
{
  char ubuf[UUID_HEX_SIZE];
  dvr_config_t *cfg = dvr_config_find_by_uuid(config_uuid);
  if (!cfg)
    cfg = dvr_config_find_by_name(config_uuid);
  return dvr_entry_create_(enabled,
                           cfg ? idnode_uuid_as_str(&cfg->dvr_id, ubuf) : NULL,
                           NULL,
                           ch, start, stop, start_extra, stop_extra,
                           title, subtitle, description, lang, content_type,
                           owner, creator, dae, NULL, pri, retention, removal,
                           comment);
}

/**
 *
 */
dvr_entry_t *
dvr_entry_create_by_event(int enabled, const char *config_uuid,
                          epg_broadcast_t *e,
                          time_t start_extra, time_t stop_extra,
                          const char *owner,
                          const char *creator, dvr_autorec_entry_t *dae,
                          dvr_prio_t pri, int retention, int removal,
                          const char *comment)
{
  if(!e->channel || !e->episode || !e->episode->title)
    return NULL;

  return dvr_entry_create_(enabled, config_uuid, e,
                           e->channel, e->start, e->stop,
                           start_extra, stop_extra,
                           NULL, NULL, NULL, NULL,
                           LIST_FIRST(&e->episode->genre),
                           owner, creator, dae, NULL, pri,
                           retention, removal, comment);
}

/**
 * Clone existing DVR entry and stop the previous
 */
dvr_entry_t *
dvr_entry_clone(dvr_entry_t *de)
{
  htsmsg_t *conf = htsmsg_create_map();
  dvr_entry_t *n;

  lock_assert(&global_lock);

  idnode_save(&de->de_id, conf);
  n = dvr_entry_create(NULL, conf, 1);
  htsmsg_destroy(conf);

  if (n) {
    if(de->de_sched_state == DVR_RECORDING) {
      de->de_dont_reschedule = 1;
      dvr_stop_recording(de, SM_CODE_SOURCE_RECONFIGURED, 1, 1);
      dvr_rec_migrate(de, n);
      n->de_start = dispatch_clock;
      dvr_entry_start_recording(n, 1);
    } else {
      dvr_entry_set_timer(n);
    }
    dvr_entry_save(n);
  }

  return n == NULL ? de : n;
}

/**
 *
 */
static int
dvr_entry_rerecord(dvr_entry_t *de)
{
  uint32_t rerecord;
  epg_broadcast_t *e, *ev;
  dvr_entry_t *de2;
  char cfg_uuid[UUID_HEX_SIZE];
  char buf[512];
  int64_t fsize1, fsize2;
  time_t pre;

  if (dvr_in_init || de->de_dont_rerecord)
    return 0;
  rerecord = dvr_entry_get_rerecord_errors(de);
  if (rerecord == 0)
    return 0;
  if ((de2 = de->de_parent) != NULL) {
    if (de->de_sched_state == DVR_COMPLETED &&
        de->de_errors == 0 &&
        de->de_data_errors < de->de_parent->de_data_errors) {
      fsize1 = dvr_get_filesize(de);
      fsize2 = dvr_get_filesize(de2);
      if (fsize1 / 5 < fsize2 / 6) {
        goto not_so_good;
      } else {
        dvr_entry_cancel_delete(de2, 1);
      }
    } else if (de->de_sched_state == DVR_COMPLETED) {
      if(dvr_get_filesize(de) == -1) {
delete_me:
        dvr_entry_cancel_delete(de, 0);
        dvr_entry_rerecord(de2);
        return 1;
      }
not_so_good:
      de->de_retention = DVR_RET_ONREMOVE;
      de->de_removal = DVR_RET_1DAY;
      dvr_entry_change_parent_child(de->de_parent, NULL, NULL, 1);
      dvr_entry_completed(de, SM_CODE_WEAK_STREAM);
      return 0;
    } else if (de->de_sched_state == DVR_MISSED_TIME) {
      goto delete_me;
    }
  }
  if (de->de_child)
    return 0;
  if (de->de_enabled == 0)
    return 0;
  if (de->de_channel == NULL)
    return 0;
  if (de->de_sched_state == DVR_SCHEDULED ||
      de->de_sched_state == DVR_RECORDING)
    return 0;
  if (de->de_sched_state == DVR_COMPLETED &&
      de->de_last_error == SM_CODE_WEAK_STREAM)
    return 0;
  /* rerecord if - DVR_NOSTATE, DVR_MISSED_TIME */
  if (de->de_sched_state == DVR_COMPLETED &&
      rerecord > de->de_data_errors && de->de_errors <= 0)
    return 0;

  e = NULL;
  pre = (60 * dvr_entry_get_extra_time_pre(de)) -
        dvr_entry_warm_time(de);
  RB_FOREACH(ev, &de->de_channel->ch_epg_schedule, sched_link) {
    if (de->de_bcast == ev) continue;
    if (ev->start - pre < dispatch_clock) continue;
    if (dvr_entry_fuzzy_match(de, ev, 0, INT64_MAX))
      if (!e || e->start > ev->start)
        e = ev;
  }

  if (e == NULL)
    return 0;

  tvhtrace("dvr", "  rerecord event %s on %s @ %"PRItime_t
                   " to %"PRItime_t,
                   epg_broadcast_get_title(e, NULL),
                   channel_get_name(e->channel),
                   e->start, e->stop);

  idnode_uuid_as_str(&de->de_config->dvr_id, cfg_uuid);
  snprintf(buf, sizeof(buf), _("Re-record%s%s"),
           de->de_comment ? ": " : "", de->de_comment ?: "");

  de2 = dvr_entry_create_by_event(1, cfg_uuid, e,
                                  de->de_start_extra, de->de_stop_extra,
                                  de->de_owner, de->de_creator, NULL,
                                  de->de_pri, de->de_retention, de->de_removal,
                                  buf);
  if (de2)
    dvr_entry_change_parent_child(de, de2, NULL, 1);

  return 0;
}

/**
 *
 */
static dvr_entry_t *_dvr_duplicate_event(dvr_entry_t* de)
{
  dvr_entry_t *de2;
  struct tm de_start;
  int record;

  if (!de->de_autorec)
    return NULL;

  record = de->de_autorec->dae_record;
  localtime_r(&de->de_start, &de_start);

  switch (record) {
    case DVR_AUTOREC_RECORD_ALL:
      return NULL;
    case DVR_AUTOREC_RECORD_DIFFERENT_EPISODE_NUMBER:
      if (strempty(de->de_episode))
        return NULL;
      break;
    case DVR_AUTOREC_RECORD_DIFFERENT_SUBTITLE:
      if (lang_str_empty(de->de_subtitle))
        return NULL;
      break;
    case DVR_AUTOREC_RECORD_DIFFERENT_DESCRIPTION:
      if (lang_str_empty(de->de_desc))
        return NULL;
      break;
    case DVR_AUTOREC_RECORD_ONCE_PER_WEEK:
      de_start.tm_mday -= (de_start.tm_wday + 6) % 7; // week = mon-sun
      mktime(&de_start); // adjusts de_start
      break;
  }

  // title not defined, can't be deduped
  if (lang_str_empty(de->de_title))
    return NULL;

  LIST_FOREACH(de2, &dvrentries, de_global_link) {
    if (de == de2)
      continue;

    // only earlier recordings qualify as master
    if (de2->de_start > de->de_start)
      continue;

    // only successful earlier recordings qualify as master
    if (de2->de_sched_state == DVR_MISSED_TIME ||
        (de2->de_sched_state == DVR_COMPLETED &&
         de2->de_last_error != SM_CODE_OK))
      continue;

    // if titles are not defined or do not match, don't dedup
    if (lang_str_compare(de->de_title, de2->de_title))
      continue;

    switch (record) {
      case DVR_AUTOREC_RECORD_DIFFERENT_EPISODE_NUMBER:
        if (!strempty(de2->de_episode) && !strcmp(de->de_episode, de2->de_episode))
          return de2;
        break;
      case DVR_AUTOREC_RECORD_DIFFERENT_SUBTITLE:
        if (!lang_str_compare(de->de_subtitle, de2->de_subtitle))
          return de2;
        break;
      case DVR_AUTOREC_RECORD_DIFFERENT_DESCRIPTION:
        if (!lang_str_compare(de->de_desc, de2->de_desc))
          return de2;
        break;
      case DVR_AUTOREC_RECORD_ONCE_PER_WEEK: {
        struct tm de2_start;
        localtime_r(&de2->de_start, &de2_start);
        de2_start.tm_mday -= (de2_start.tm_wday + 6) % 7; // week = mon-sun
        mktime(&de2_start); // adjusts de2_start
        if (de_start.tm_year == de2_start.tm_year && de_start.tm_yday == de2_start.tm_yday)
          return de2;
        break;
      }
      case DVR_AUTOREC_RECORD_ONCE_PER_DAY: {
        struct tm de2_start;
        localtime_r(&de2->de_start, &de2_start);
        if (de_start.tm_year == de2_start.tm_year && de_start.tm_yday == de2_start.tm_yday)
          return de2;
        break;
      }
    }
  }
  return NULL;
}

/**
 *
 */
void
dvr_entry_create_by_autorec(int enabled, epg_broadcast_t *e, dvr_autorec_entry_t *dae)
{
  char buf[512];
  char ubuf[UUID_HEX_SIZE];
  dvr_entry_t *de;
  uint32_t count = 0, max_count;

  /* Identical duplicate detection
     NOTE: Semantic duplicate detection is deferred to the start time of recording and then done using _dvr_duplicate_event by dvr_timer_start_recording. */
  LIST_FOREACH(de, &dvrentries, de_global_link) {
    if (de->de_bcast == e || (de->de_bcast && de->de_bcast->episode == e->episode))
      return;
  }

  /* Handle max schedules limit for autorrecord */
  if ((max_count = dvr_autorec_get_max_sched_count(dae)) > 0){
    count = 0;
    LIST_FOREACH(de, &dae->dae_spawns, de_autorec_link)
      if ((de->de_sched_state == DVR_SCHEDULED) ||
          (de->de_sched_state == DVR_RECORDING)) count++;

    if (count >= max_count) {
      tvhlog(LOG_DEBUG, "dvr", "Autorecord \"%s\": Not scheduling \"%s\" because of autorecord max schedules limit reached",
             dae->dae_name, lang_str_get(e->episode->title, NULL));
      return;
    }
  }

  snprintf(buf, sizeof(buf), _("Auto recording%s%s"),
           dae->dae_comment ? ": " : "",
           dae->dae_comment ?: "");

  dvr_entry_create_by_event(enabled, idnode_uuid_as_str(&dae->dae_config->dvr_id, ubuf),
                            e, dae->dae_start_extra, dae->dae_stop_extra,
                            dae->dae_owner, dae->dae_creator, dae, dae->dae_pri,
                            dae->dae_retention, dae->dae_removal, buf);
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

  if (de->de_timerec) {
    de->de_timerec->dte_spawn = NULL;
    de->de_timerec = NULL;
  }

  if(de->de_config != NULL)
    LIST_REMOVE(de, de_config_link);

  htsmsg_destroy(de->de_files);
  free(de->de_directory);
  free(de->de_owner);
  free(de->de_creator);
  free(de->de_comment);
  if (de->de_title) lang_str_destroy(de->de_title);
  if (de->de_subtitle)  lang_str_destroy(de->de_subtitle);
  if (de->de_desc)  lang_str_destroy(de->de_desc);
  dvr_entry_assign_broadcast(de, NULL);
  free(de->de_channel_name);
  free(de->de_episode);

  free(de);
}

/**
 *
 */
void
dvr_entry_destroy(dvr_entry_t *de, int delconf)
{
  char ubuf[UUID_HEX_SIZE];

  if (delconf)
    hts_settings_remove("dvr/log/%s", idnode_uuid_as_str(&de->de_id, ubuf));

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

  if (de->de_parent)
    dvr_entry_change_parent_child(de->de_parent, NULL, de, delconf);
  if (de->de_child)
    dvr_entry_change_parent_child(de, NULL, de, delconf);

  dvr_entry_dec_ref(de);
}

/**
 *
 */
void
dvr_entry_destroy_by_config(dvr_config_t *cfg, int delconf)
{
  dvr_entry_t *de;
  dvr_config_t *def = NULL;

  while ((de = LIST_FIRST(&cfg->dvr_entries)) != NULL) {
    LIST_REMOVE(de, de_config_link);
    if (def == NULL && delconf)
      def = dvr_config_find_by_name_default(NULL);
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
void
dvr_entry_save(dvr_entry_t *de)
{
  htsmsg_t *m = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];

  lock_assert(&global_lock);

  idnode_save(&de->de_id, m);
  if (de->de_files)
    htsmsg_add_msg(m, "files", htsmsg_copy(de->de_files));
  hts_settings_save(m, "dvr/log/%s", idnode_uuid_as_str(&de->de_id, ubuf));
  htsmsg_destroy(m);
}


/**
 *
 */
static void
dvr_timer_rerecord(void *aux)
{
  dvr_entry_t *de = aux;
  if (dvr_entry_rerecord(de))
    return;
  dvr_entry_retention_timer(de);
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


/**
 *
 */
static void
dvr_timer_remove_files(void *aux)
{
  dvr_entry_t *de = aux;
  dvr_entry_retention_timer(de);
}

/**
 *
 */

#define DVR_UPDATED_ENABLED      (1<<0)
#define DVR_UPDATED_CHANNEL      (1<<1)
#define DVR_UPDATED_STOP         (1<<2)
#define DVR_UPDATED_STOP_EXTRA   (1<<3)
#define DVR_UPDATED_START        (1<<4)
#define DVR_UPDATED_START_EXTRA  (1<<5)
#define DVR_UPDATED_TITLE        (1<<6)
#define DVR_UPDATED_SUBTITLE     (1<<7)
#define DVR_UPDATED_SUMMARY      (1<<8)
#define DVR_UPDATED_DESCRIPTION  (1<<9)
#define DVR_UPDATED_PRIO         (1<<10)
#define DVR_UPDATED_GENRE        (1<<11)
#define DVR_UPDATED_RETENTION    (1<<12)
#define DVR_UPDATED_REMOVAL      (1<<13)
#define DVR_UPDATED_EID          (1<<14)
#define DVR_UPDATED_BROADCAST    (1<<15)
#define DVR_UPDATED_EPISODE      (1<<16)

static char *dvr_updated_str(char *buf, size_t buflen, int flags)
{
  static const char *x = "ecoOsStumdpgrviBE";
  const char *p = x;
  char *w = buf, *end = buf + buflen;

  for (p = x; *p && flags && w != end; p++, flags >>= 1)
    if (flags & 1)
      *w++ = *p;
  if (w == end)
    *(w - 1) = '\0';
  else
    *w ='\0';
  return buf;
}

/**
 *
 */
static dvr_entry_t *_dvr_entry_update
  ( dvr_entry_t *de, int enabled, epg_broadcast_t *e, channel_t *ch,
    const char *title, const char *subtitle, const char *desc,
    const char *lang, time_t start, time_t stop,
    time_t start_extra, time_t stop_extra,
    dvr_prio_t pri, int retention, int removal )
{
  char buf[40];
  int save = 0, updated = 0;

  if (enabled >= 0) {
    enabled = !!enabled;
    if (de->de_enabled != enabled) {
      de->de_enabled = enabled;
      save |= DVR_UPDATED_ENABLED;
    }
  }

  if (!dvr_entry_is_editable(de)) {
    if (stop > 0) {
      if (stop < dispatch_clock)
        stop = dispatch_clock;
      if (stop < de->de_start)
        stop = de->de_start;
      if (stop != de->de_stop) {
        de->de_stop = stop;
        save |= DVR_UPDATED_STOP;
      }
    }
    if (stop_extra && (stop_extra != de->de_stop_extra)) {
      de->de_stop_extra = stop_extra;
      save |= DVR_UPDATED_STOP_EXTRA;
    }
    goto dosave;
  }

  /* Channel */
  if (ch && (ch != de->de_channel)) {
    de->de_channel = ch;
    save |= DVR_UPDATED_CHANNEL;
  }

  /* Start/Stop */
  if (e) {
    start = e->start;
    stop  = e->stop;
  }
  if (start && (start != de->de_start)) {
    de->de_start = start;
    save |= DVR_UPDATED_START;
  }
  if (stop && (stop != de->de_stop)) {
    de->de_stop = stop;
    save |= DVR_UPDATED_STOP;
  }
  if (start_extra && (start_extra != de->de_start_extra)) {
    de->de_start_extra = start_extra;
    save |= DVR_UPDATED_START_EXTRA;
  }
  if (stop_extra && (stop_extra != de->de_stop_extra)) {
    de->de_stop_extra = stop_extra;
    save |= DVR_UPDATED_STOP_EXTRA;
  }
  if (pri != DVR_PRIO_NOTSET && (pri != de->de_pri)) {
    de->de_pri = pri;
    save |= DVR_UPDATED_PRIO;
  }
  if (retention && (retention != de->de_retention)) {
    de->de_retention = retention;
    save |= DVR_UPDATED_RETENTION;
  }
  if (removal && (removal != de->de_removal)) {
    de->de_removal = removal;
    save |= DVR_UPDATED_REMOVAL;
  }
  if (save) {
    updated = 1;
    dvr_entry_set_timer(de);
  }

  /* Title */ 
  if (e && e->episode && e->episode->title) {
    save |= lang_str_set2(&de->de_title, e->episode->title) ? DVR_UPDATED_TITLE : 0;
  } else if (title) {
    save |= lang_str_set(&de->de_title, title, lang) ? DVR_UPDATED_TITLE : 0;
  }

  /* Subtitle*/
  if (e && e->episode && e->episode->subtitle) {
    save |= lang_str_set2(&de->de_subtitle, e->episode->subtitle) ? DVR_UPDATED_SUBTITLE : 0;
  } else if (subtitle) {
    save |= lang_str_set(&de->de_title, subtitle, lang) ? DVR_UPDATED_SUBTITLE : 0;
  }

  /* EID */
  if (e && e->dvb_eid != de->de_dvb_eid) {
    de->de_dvb_eid = e->dvb_eid;
    save |= DVR_UPDATED_EID;
  }

  /* Description */
  if (e && e->description) {
    save |= lang_str_set2(&de->de_desc, e->description) ? DVR_UPDATED_DESCRIPTION : 0;
  } else if (e && e->episode && e->episode->description) {
    save |= lang_str_set2(&de->de_desc, e->episode->description) ? DVR_UPDATED_DESCRIPTION : 0;
  } else if (e && e->summary) {
    save |= lang_str_set2(&de->de_desc, e->summary) ? DVR_UPDATED_DESCRIPTION : 0;
  } else if (e && e->episode && e->episode->summary) {
    save |= lang_str_set2(&de->de_desc, e->episode->summary) ? DVR_UPDATED_DESCRIPTION : 0;
  } else if (desc) {
    save |= lang_str_set(&de->de_desc, desc, lang) ? DVR_UPDATED_DESCRIPTION : 0;
  }

  /* Genre */
  if (e && e->episode) {
    epg_genre_t *g = LIST_FIRST(&e->episode->genre);
    if (g && (g->code / 16) != de->de_content_type) {
      de->de_content_type = g->code / 16;
      save |= DVR_UPDATED_GENRE;
    }
  }

  /* Broadcast */
  if (e && (de->de_bcast != e)) {
    dvr_entry_assign_broadcast(de, e);
    save |= DVR_UPDATED_BROADCAST;
  }

  /* Episode */
  if (dvr_entry_get_episode(de->de_bcast, buf, sizeof(buf))) {
    if (strcmp(de->de_episode ?: "", buf)) {
      free(de->de_episode);
      de->de_episode = strdup(buf);
      save |= DVR_UPDATED_EPISODE;
    }
  }

  /* Save changes */
dosave:
  if (save) {
    idnode_changed(&de->de_id);
    htsp_dvr_entry_update(de);
    if (tvhlog_limit(&de->de_update_limit, 60)) {
      tvhlog(LOG_INFO, "dvr", "\"%s\" on \"%s\": Updated%s (%s)",
             lang_str_get(de->de_title, NULL), DVR_CH_NAME(de),
             updated ? " Timer" : "",
             dvr_updated_str(buf, sizeof(buf), save));
    } else {
      tvhtrace("dvr", "\"%s\" on \"%s\": Updated%s (%s)",
               lang_str_get(de->de_title, NULL), DVR_CH_NAME(de),
               updated ? " Timer" : "",
               dvr_updated_str(buf, sizeof(buf), save));
    }
  }

  return de;
}

/**
 *
 */
dvr_entry_t * 
dvr_entry_update
  ( dvr_entry_t *de, int enabled, channel_t *ch,
    const char *title, const char *subtitle,
    const char *desc, const char *lang,
    time_t start, time_t stop,
    time_t start_extra, time_t stop_extra,
    dvr_prio_t pri, int retention, int removal )
{
  return _dvr_entry_update(de, enabled, NULL, ch, title, subtitle, desc, lang,
                           start, stop, start_extra, stop_extra,
                           pri, retention, removal);
}

/**
 * Used to notify the DVR that an event has been replaced in the EPG
 */
void 
dvr_event_replaced(epg_broadcast_t *e, epg_broadcast_t *new_e)
{
  dvr_entry_t *de;
  channel_t *ch = e->channel;
  epg_broadcast_t *e2;
  char ubuf[UUID_HEX_SIZE];

  assert(e != NULL);
  assert(new_e != NULL);

  /* Ignore */
  if (ch == NULL || e == new_e) return;

  /* Existing entry */
  LIST_FOREACH(de, &ch->ch_dvrs, de_channel_link) {

    if (de->de_bcast != e)
      continue;

    tvhtrace("dvr",
             "dvr entry %s event replaced %s on %s @ %"PRItime_t
             " to %"PRItime_t,
             idnode_uuid_as_str(&de->de_id, ubuf),
             epg_broadcast_get_title(e, NULL),
             channel_get_name(ch),
             e->start, e->stop);

    /* Ignore - already in progress */
    if (de->de_sched_state != DVR_SCHEDULED)
      return;

    /* If this was created by autorec - just remove it, it'll get recreated */
    if (de->de_autorec) {

      dvr_entry_assign_broadcast(de, NULL);
      dvr_entry_destroy(de, 1);

    /* Find match */
    } else {

      RB_FOREACH(e2, &ch->ch_epg_schedule, sched_link) {
        if (dvr_entry_fuzzy_match(de, e2, e2->dvb_eid,
                                  de->de_config->dvr_update_window)) {
          tvhtrace("dvr",
                   "  replacement event %s on %s @ %"PRItime_t
                   " to %"PRItime_t,
                   epg_broadcast_get_title(e2, NULL),
                   channel_get_name(ch),
                   e2->start, e2->stop);
          _dvr_entry_update(de, -1, e2, NULL, NULL, NULL, NULL, NULL,
                            0, 0, 0, 0, DVR_PRIO_NOTSET, 0, 0);
          return;
        }
      }
      dvr_entry_assign_broadcast(de, NULL);

    }
  }
}

/**
 * Used to notify the DVR that an event has been removed
 */
void
dvr_event_removed(epg_broadcast_t *e)
{
  dvr_entry_t *de;

  if (e->channel == NULL)
    return;
  LIST_FOREACH(de, &e->channel->ch_dvrs, de_channel_link) {
    if (de->de_bcast != e)
      continue;
    dvr_entry_assign_broadcast(de, NULL);
    dvr_entry_save(de);
  }
}

/**
 * Event was updated in epg
 */
void dvr_event_updated(epg_broadcast_t *e)
{
  dvr_entry_t *de;
  int found = 0;
  char ubuf[UUID_HEX_SIZE];

  if (e->channel == NULL)
    return;
  LIST_FOREACH(de, &e->channel->ch_dvrs, de_channel_link) {
    if (de->de_bcast != e)
      continue;
    _dvr_entry_update(de, -1, e, NULL, NULL, NULL, NULL,
                      NULL, 0, 0, 0, 0, DVR_PRIO_NOTSET, 0, 0);
    found++;
  }
  if (found == 0) {
    LIST_FOREACH(de, &e->channel->ch_dvrs, de_channel_link) {
      if (de->de_sched_state != DVR_SCHEDULED) continue;
      if (de->de_bcast) continue;
      if (dvr_entry_fuzzy_match(de, e, e->dvb_eid,
                                de->de_config->dvr_update_window)) {
        tvhtrace("dvr",
                 "dvr entry %s link to event %s on %s @ %"PRItime_t
                 " to %"PRItime_t,
                 idnode_uuid_as_str(&de->de_id, ubuf),
                 epg_broadcast_get_title(e, NULL),
                 channel_get_name(e->channel),
                 e->start, e->stop);
        _dvr_entry_update(de, -1, e, NULL, NULL, NULL, NULL,
                          NULL, 0, 0, 0, 0, DVR_PRIO_NOTSET, 0, 0);
        break;
      }
    }
  }
}

/**
 * Event running status is updated
 */
void dvr_event_running(epg_broadcast_t *e, epg_source_t esrc, epg_running_t running)
{
  dvr_entry_t *de;
  const char *srcname;
  char ubuf[UUID_HEX_SIZE];

  if (esrc != EPG_SOURCE_EIT || e->dvb_eid == 0 || e->channel == NULL)
    return;
  LIST_FOREACH(de, &e->channel->ch_dvrs, de_channel_link) {
    if (!dvr_entry_get_epg_running(de)) {
      atomic_exchange_time_t(&de->de_running_start, 0);
      atomic_exchange_time_t(&de->de_running_stop, 0);
      atomic_exchange_time_t(&de->de_running_pause, 0);
      continue;
    }
    if (running == EPG_RUNNING_NOW && de->de_dvb_eid == e->dvb_eid) {
      if (de->de_running_pause) {
        tvhdebug("dvr", "dvr entry %s event %s on %s - EPG unpause",
                 idnode_uuid_as_str(&de->de_id, ubuf),
                 epg_broadcast_get_title(e, NULL),
                 channel_get_name(e->channel));
        atomic_exchange_time_t(&de->de_running_pause, 0);
      }
      if (!de->de_running_start) {
        tvhdebug("dvr", "dvr entry %s event %s on %s - EPG marking start",
                 idnode_uuid_as_str(&de->de_id, ubuf),
                 epg_broadcast_get_title(e, NULL),
                 channel_get_name(e->channel));
        atomic_exchange_time_t(&de->de_running_start, dispatch_clock);
      }
      if (dvr_entry_get_start_time(de, 1) > dispatch_clock) {
        atomic_exchange_time_t(&de->de_start, dispatch_clock);
        dvr_entry_set_timer(de);
        tvhdebug("dvr", "dvr entry %s event %s on %s - EPG start",
                 idnode_uuid_as_str(&de->de_id, ubuf),
                 epg_broadcast_get_title(e, NULL),
                 channel_get_name(e->channel));
      }
    } else if ((running == EPG_RUNNING_STOP && de->de_dvb_eid == e->dvb_eid) ||
                running == EPG_RUNNING_NOW) {
      /*
       * make checking more robust
       * sometimes, the running bits are parsed randomly for a few moments
       * so don't expect that the broacasting has only 5 seconds
       */
      if (de->de_running_start + 5 > dispatch_clock)
        continue;

      srcname = de->de_dvb_eid == e->dvb_eid ? "event" : "other running event";
      if (!de->de_running_stop ||
          de->de_running_start > de->de_running_stop) {
        tvhdebug("dvr", "dvr entry %s %s %s on %s - EPG marking stop",
                 idnode_uuid_as_str(&de->de_id, ubuf), srcname,
                 epg_broadcast_get_title(e, NULL),
                 channel_get_name(de->de_channel));
      }
      atomic_exchange_time_t(&de->de_running_stop, dispatch_clock);
      atomic_exchange_time_t(&de->de_running_pause, 0);
      if (de->de_sched_state == DVR_RECORDING && de->de_running_start) {
        de->de_dont_reschedule = 1;
        dvr_stop_recording(de, SM_CODE_OK, 0, 0);
        tvhdebug("dvr", "dvr entry %s %s %s on %s - EPG stop",
                 idnode_uuid_as_str(&de->de_id, ubuf), srcname,
                 epg_broadcast_get_title(e, NULL),
                 channel_get_name(de->de_channel));
      }
    } else if (running == EPG_RUNNING_PAUSE && de->de_dvb_eid == e->dvb_eid) {
      if (!de->de_running_pause) {
        tvhdebug("dvr", "dvr entry %s event %s on %s - EPG pause",
                 idnode_uuid_as_str(&de->de_id, ubuf),
                 epg_broadcast_get_title(e, NULL),
                 channel_get_name(e->channel));
        atomic_exchange_time_t(&de->de_running_pause, dispatch_clock);
      }
    }
  }
}

/**
 *
 */
void
dvr_stop_recording(dvr_entry_t *de, int stopcode, int saveconf, int clone)
{
  dvr_rs_state_t rec_state = de->de_rec_state;
  dvr_autorec_entry_t *dae = de->de_autorec;

  if (!clone)
    dvr_rec_unsubscribe(de);

  if (stopcode != SM_CODE_INVALID_TARGET &&
      (rec_state == DVR_RS_PENDING ||
       rec_state == DVR_RS_WAIT_PROGRAM_START ||
       htsmsg_is_empty(de->de_files)))
    dvr_entry_missed_time(de, stopcode);
  else
    dvr_entry_completed(de, stopcode);

  tvhlog(LOG_INFO, "dvr", "\"%s\" on \"%s\": "
	 "End of program: %s",
	 lang_str_get(de->de_title, NULL), DVR_CH_NAME(de),
	 dvr_entry_status(de));

  if (dvr_entry_rerecord(de))
    return;

  if (saveconf)
    dvr_entry_save(de);

  dvr_entry_retention_timer(de);

  // Trigger autorecord update in case of schedules limit
  if (dae && dvr_autorec_get_max_sched_count(dae) > 0)
    dvr_autorec_changed(de->de_autorec, 0);
}


/**
 *
 */
static void
dvr_timer_stop_recording(void *aux)
{
  dvr_entry_t *de = aux;
  /* EPG thinks that the program is running */
  if (de->de_running_start > de->de_running_stop) {
    gtimer_arm(&de->de_timer, dvr_timer_stop_recording, de, 10);
    return;
  }
  dvr_stop_recording(aux, SM_CODE_OK, 1, 0);
}



/**
 *
 */
static void
dvr_entry_start_recording(dvr_entry_t *de, int clone)
{
  int r;

  if (!de->de_enabled) {
    dvr_entry_missed_time(de, SM_CODE_SVC_NOT_ENABLED);
    return;
  }

  dvr_entry_set_state(de, DVR_RECORDING, DVR_RS_PENDING, SM_CODE_OK);

  tvhlog(LOG_INFO, "dvr", "\"%s\" on \"%s\" recorder starting",
	 lang_str_get(de->de_title, NULL), DVR_CH_NAME(de));

  if (!clone && (r = dvr_rec_subscribe(de)) < 0) {
    dvr_entry_completed(de, r == -EPERM ? SM_CODE_USER_ACCESS :
                           (r == -EOVERFLOW ? SM_CODE_USER_LIMIT :
                            SM_CODE_BAD_SOURCE));
    return;
  }

  gtimer_arm_abs(&de->de_timer, dvr_timer_stop_recording, de,
                 dvr_entry_get_stop_time(de));
}


/**
 *
 */
static void
dvr_timer_start_recording(void *aux)
{
  dvr_entry_t *de = aux;

  if (de->de_channel == NULL || !de->de_channel->ch_enabled) {
    dvr_entry_set_state(de, DVR_NOSTATE, DVR_RS_PENDING, de->de_last_error);
    return;
  }

  // if duplicate, then delete it now, don't record!
  if (_dvr_duplicate_event(de)) {
    dvr_entry_cancel_delete(de, 1);
    return;
  }

  dvr_entry_start_recording(de, 0);
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
 * Unconditionally remove an entry
 */
static void
dvr_entry_purge(dvr_entry_t *de, int delconf)
{
  if(de->de_sched_state == DVR_RECORDING)
    dvr_stop_recording(de, SM_CODE_SOURCE_DELETED, delconf, 0);
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

  htsp_dvr_entry_update(de);
}

static void
dvr_entry_class_delete(idnode_t *self)
{
  dvr_entry_cancel_delete((dvr_entry_t *)self, 0);
}

static int
dvr_entry_class_perm(idnode_t *self, access_t *a, htsmsg_t *msg_to_write)
{
  dvr_entry_t *de = (dvr_entry_t *)self;

  if (access_verify2(a, ACCESS_OR|ACCESS_ADMIN|ACCESS_RECORDER))
    return -1;
  if (!access_verify2(a, ACCESS_ADMIN))
    return 0;
  if (dvr_entry_verify(de, a, msg_to_write == NULL ? 1 : 0))
    return -1;
  return 0;
}

static const char *
dvr_entry_class_get_title (idnode_t *self, const char *lang)
{
  dvr_entry_t *de = (dvr_entry_t *)self;
  const char *s;
  s = lang_str_get(de->de_title, lang);
  if (s == NULL || s[0] == '\0') {
    s = lang ? lang_str_get(de->de_title, NULL) : NULL;
    if (s == NULL || s[0] == '\0') {
      s = lang ? lang_str_get(de->de_desc, lang) : NULL;
      if (s == NULL || s[0] == '\0')
        s = lang_str_get(de->de_desc, NULL);
    }
  }
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
dvr_entry_class_config_name_opts(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de && !dvr_entry_is_editable(de))
    return PO_RDONLY | PO_ADVANCED;
  return PO_ADVANCED;
}

static uint32_t
dvr_entry_class_owner_opts(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de && de->de_id.in_access &&
      !access_verify2(de->de_id.in_access, ACCESS_ADMIN))
    return PO_ADVANCED;
  return PO_RDONLY | PO_ADVANCED;
}

static uint32_t
dvr_entry_class_start_extra_opts(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de && !dvr_entry_is_editable(de))
    return PO_RDONLY | PO_DURATION | PO_ADVANCED;
  return PO_DURATION | PO_ADVANCED;
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
  if (v != de->de_stop) {
    de->de_stop = v;
    return 1;
  }
  return 0;
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
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_config)
    idnode_uuid_as_str(&de->de_config->dvr_id, prop_sbuf);
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
}

htsmsg_t *
dvr_entry_class_config_name_list(void *o, const char *lang)
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

static char *
dvr_entry_class_config_name_rend(void *o, const char *lang)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_config)
    return strdup(de->de_config->dvr_config_name);
  return NULL;
}

static const void *
dvr_entry_class_filename_get(void *o)
{
  static const char *ret;
  dvr_entry_t *de = (dvr_entry_t *)o;
  const char *s = dvr_get_filename(de);
  ret = s ?: "";
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
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_channel)
    idnode_uuid_as_str(&de->de_channel->ch_id, prop_sbuf);
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
}

static char *
dvr_entry_class_channel_rend(void *o, const char *lang)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_channel)
    return strdup(channel_get_name(de->de_channel));
  return NULL;
}

static int
dvr_entry_class_channel_name_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  channel_t   *ch;
  char ubuf[UUID_HEX_SIZE];
  if (!dvr_entry_is_editable(de))
    return 0;
  if (!strcmp(de->de_channel_name ?: "", v ?: ""))
    return 0;
  ch = v ? channel_find_by_name(v) : NULL;
  if (ch) {
    return dvr_entry_class_channel_set(o, idnode_uuid_as_str(&ch->ch_id, ubuf));
  } else {
    free(de->de_channel_name);
    de->de_channel_name = v ? strdup(v) : NULL;
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
dvr_entry_class_pri_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Not set"),        DVR_PRIO_NOTSET },
    { N_("Important"),      DVR_PRIO_IMPORTANT },
    { N_("High"),           DVR_PRIO_HIGH, },
    { N_("Normal"),         DVR_PRIO_NORMAL },
    { N_("Low"),            DVR_PRIO_LOW },
    { N_("Unimportant"),    DVR_PRIO_UNIMPORTANT },
  };
  return strtab2htsmsg(tab, 1, lang);
}

htsmsg_t *
dvr_entry_class_retention_list ( void *o, const char *lang )
{
  static const struct strtab_u32 tab[] = {
    { N_("DVR configuration"),  DVR_RET_DVRCONFIG },
    { N_("1 day"),              DVR_RET_1DAY },
    { N_("3 days"),             DVR_RET_3DAY },
    { N_("5 days"),             DVR_RET_5DAY },
    { N_("1 week"),             DVR_RET_1WEEK },
    { N_("2 weeks"),            DVR_RET_2WEEK },
    { N_("3 weeks"),            DVR_RET_3WEEK },
    { N_("1 month"),            DVR_RET_1MONTH },
    { N_("2 months"),           DVR_RET_2MONTH },
    { N_("3 months"),           DVR_RET_3MONTH },
    { N_("6 months"),           DVR_RET_6MONTH },
    { N_("1 year"),             DVR_RET_1YEAR },
    { N_("2 years"),            DVR_RET_2YEARS },
    { N_("3 years"),            DVR_RET_3YEARS },
    { N_("On file removal"),    DVR_RET_ONREMOVE },
    { N_("Forever"),            DVR_RET_FOREVER },
  };
  return strtab2htsmsg_u32(tab, 1, lang);
}

htsmsg_t *
dvr_entry_class_removal_list ( void *o, const char *lang )
{
  static const struct strtab_u32 tab[] = {
    { N_("DVR configuration"),  DVR_RET_DVRCONFIG },
    { N_("1 day"),              DVR_RET_1DAY },
    { N_("3 days"),             DVR_RET_3DAY },
    { N_("5 days"),             DVR_RET_5DAY },
    { N_("1 week"),             DVR_RET_1WEEK },
    { N_("2 weeks"),            DVR_RET_2WEEK },
    { N_("3 weeks"),            DVR_RET_3WEEK },
    { N_("1 month"),            DVR_RET_1MONTH },
    { N_("2 months"),           DVR_RET_2MONTH },
    { N_("3 months"),           DVR_RET_3MONTH },
    { N_("6 months"),           DVR_RET_6MONTH },
    { N_("1 year"),             DVR_RET_1YEAR },
    { N_("2 years"),            DVR_RET_2YEARS },
    { N_("3 years"),            DVR_RET_3YEARS },
    { N_("Maintained space"),   DVR_RET_SPACE },
    { N_("Forever"),            DVR_RET_FOREVER },
  };
  return strtab2htsmsg_u32(tab, 1, lang);
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
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_autorec)
    idnode_uuid_as_str(&de->de_autorec->dae_id, prop_sbuf);
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
}

static const void *
dvr_entry_class_autorec_caption_get(void *o)
{
  static const char *ret;
  dvr_entry_t *de = (dvr_entry_t *)o;
  dvr_autorec_entry_t *dae = de->de_autorec;
  if (dae) {
    ret = prop_sbuf;
    snprintf(prop_sbuf, PROP_SBUF_LEN, "%s%s%s%s",
             dae->dae_name ?: "",
             dae->dae_comment ? " (" : "",
             dae->dae_comment,
             dae->dae_comment ? ")" : "");
  } else
    ret = "";
  return &ret;
}

static int
dvr_entry_class_timerec_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  dvr_timerec_entry_t *dte;
  if (!dvr_entry_is_editable(de))
    return 0;
  dte = v ? dvr_timerec_find_by_uuid(v) : NULL;
  if (dte == NULL) {
    if (de->de_timerec) {
      de->de_timerec->dte_spawn = NULL;
      de->de_timerec = NULL;
      return 1;
    }
  } else if (de->de_timerec != dte) {
    de->de_timerec = dte;
    dte->dte_spawn = de;
    return 1;
  }
  return 0;
}

static const void *
dvr_entry_class_timerec_get(void *o)
{
  static const char *ret;
  char ubuf[UUID_HEX_SIZE];
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_timerec)
    ret = idnode_uuid_as_str(&de->de_timerec->dte_id, ubuf);
  else
    ret = "";
  return &ret;
}

static const void *
dvr_entry_class_timerec_caption_get(void *o)
{
  static const char *ret;
  dvr_entry_t *de = (dvr_entry_t *)o;
  dvr_timerec_entry_t *dte = de->de_timerec;
  if (dte) {
    ret = prop_sbuf;
    snprintf(prop_sbuf, PROP_SBUF_LEN, "%s%s%s%s",
             dte->dte_name ?: "",
             dte->dte_comment ? " (" : "",
             dte->dte_comment,
             dte->dte_comment ? ")" : "");
  } else
    ret = "";
  return &ret;
}

static int
dvr_entry_class_parent_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o, *de2;
  if (!dvr_entry_is_editable(de))
    return 0;
  de2 = v ? dvr_entry_find_by_uuid(v) : NULL;
  return dvr_entry_change_parent_child(de2, de, de, 1);
}

static const void *
dvr_entry_class_parent_get(void *o)
{
  static const char *ret;
  char ubuf[UUID_HEX_SIZE];
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_parent)
    ret = idnode_uuid_as_str(&de->de_parent->de_id, ubuf);
  else
    ret = "";
  return &ret;
}

static int
dvr_entry_class_child_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o, *de2;
  if (!dvr_entry_is_editable(de))
    return 0;
  de2 = v ? dvr_entry_find_by_uuid(v) : NULL;
  return dvr_entry_change_parent_child(de, de2, de, 1);
}

static const void *
dvr_entry_class_child_get(void *o)
{
  static const char *ret;
  char ubuf[UUID_HEX_SIZE];
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_child)
    ret = idnode_uuid_as_str(&de->de_child->de_id, ubuf);
  else
    ret = "";
  return &ret;
}

static int
dvr_entry_class_broadcast_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  uint32_t id = *(uint32_t *)v;
  epg_broadcast_t *bcast;
  if (!dvr_entry_is_editable(de))
    return 0;
  bcast = epg_broadcast_find_by_id(id);
  return dvr_entry_assign_broadcast(de, bcast);
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

static int
dvr_entry_class_disp_title_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  const char *lang = idnode_lang(&de->de_id);
  const char *s = "";
  if (v == NULL || *((char *)v) == '\0')
    v = "UnknownTitle";
  if (de->de_title)
    s = lang_str_get(de->de_title, lang);
  if (strcmp(s, v)) {
    lang_str_set(&de->de_title, v, lang);
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
    s = lang_str_get(de->de_title, idnode_lang(o));
    if (s == NULL)
      s = "";
  }
  return &s;
}

static int
dvr_entry_class_disp_subtitle_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  const char *lang = idnode_lang(o);
  const char *s = "";
  if (v == NULL || *((char *)v) == '\0')
    v = "UnknownSubtitle";
  if (de->de_subtitle)
    s = lang_str_get(de->de_subtitle, lang);
  if (strcmp(s, v)) {
    lang_str_set(&de->de_subtitle, v, lang);
    return 1;
  }
  return 0;
}

static const void *
dvr_entry_class_disp_subtitle_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  static const char *s;
  s = "";
  if (de->de_subtitle) {
    s = lang_str_get(de->de_subtitle, idnode_lang(o));
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
  if (de->de_desc) {
    s = lang_str_get(de->de_desc, idnode_lang(o));
    if (s == NULL)
      s = "";
  }
  return &s;
}

static const void *
dvr_entry_class_url_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  char ubuf[UUID_HEX_SIZE];
  prop_sbuf[0] = '\0';
  if (de->de_sched_state == DVR_COMPLETED ||
      de->de_sched_state == DVR_RECORDING)
    snprintf(prop_sbuf, PROP_SBUF_LEN, "dvrfile/%s", idnode_uuid_as_str(&de->de_id, ubuf));
  return &prop_sbuf_ptr;
}

static const void *
dvr_entry_class_filesize_get(void *o)
{
  static int64_t size;
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_sched_state == DVR_COMPLETED ||
      de->de_sched_state == DVR_RECORDING) {
    size = dvr_get_filesize(de);
    if (size < 0)
      size = 0;
  } else
    size = 0;
  return &size;
}

static const void *
dvr_entry_class_start_real_get(void *o)
{
  static time_t tm;
  dvr_entry_t *de = (dvr_entry_t *)o;
  tm = dvr_entry_get_start_time(de, 1);
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
  start = dvr_entry_get_start_time(de, 0);
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
  if (ch == NULL) {
    s = "";
  } else if ((s = channel_get_icon(ch)) == NULL) {
    s = "";
  }
  return &s;
}

static const void *
dvr_entry_class_duplicate_get(void *o)
{
  static time_t null = 0;
  dvr_entry_t *de = (dvr_entry_t *)o;
  de = _dvr_duplicate_event(de);
  return de ? &de->de_start : &null;
}

htsmsg_t *
dvr_entry_class_duration_list(void *o, const char *not_set, int max, int step, const char *lang)
{
  int i;
  htsmsg_t *e, *l = htsmsg_create_list();
  const char *hrs  = tvh_gettext_lang(lang, N_("hrs"));
  const char *min  = tvh_gettext_lang(lang, N_("min"));
  const char *mins = tvh_gettext_lang(lang, N_("mins"));
  char buf[32];
  e = htsmsg_create_map();
  htsmsg_add_u32(e, "key", 0);
  htsmsg_add_str(e, "val", not_set);
  htsmsg_add_msg(l, NULL, e);
  for (i = 1; i <= 120;  i++) {
    snprintf(buf, sizeof(buf), "%d %s", i, i > 1 ? mins : min);
    e = htsmsg_create_map();
    htsmsg_add_u32(e, "key", i * step);
    htsmsg_add_str(e, "val", buf);
    htsmsg_add_msg(l, NULL, e);
  }
  for (i = 120; i <= max; i += 30) {
    if ((i % 60) == 0)
      snprintf(buf, sizeof(buf), "%d %s", i / 60, hrs);
    else
      snprintf(buf, sizeof(buf), "%d %s %d %s", i / 60, hrs, i % 60, (i % 60) > 0 ? mins : min);
    e = htsmsg_create_map();
    htsmsg_add_u32(e, "key", i * step);
    htsmsg_add_str(e, "val", buf);
    htsmsg_add_msg(l, NULL, e);
  }
  return l;
}

static htsmsg_t *
dvr_entry_class_extra_list(void *o, const char *lang)
{
  const char *msg = N_("Not set (use channel or DVR configuration)");
  return dvr_entry_class_duration_list(o, tvh_gettext_lang(lang, msg), 4*60, 1, lang);
}
                                        
static htsmsg_t *
dvr_entry_class_content_type_list(void *o, const char *lang)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "epg/content_type/list");
  return m;
}

const idclass_t dvr_entry_class = {
  .ic_class     = "dvrentry",
  .ic_caption   = N_("DVR entry"),
  .ic_event     = "dvrentry",
  .ic_save      = dvr_entry_class_save,
  .ic_get_title = dvr_entry_class_get_title,
  .ic_delete    = dvr_entry_class_delete,
  .ic_perm      = dvr_entry_class_perm,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .off      = offsetof(dvr_entry_t, de_enabled),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_TIME,
      .id       = "start",
      .name     = N_("Start time"),
      .set      = dvr_entry_class_start_set,
      .off      = offsetof(dvr_entry_t, de_start),
      .get_opts = dvr_entry_class_start_opts,
    },
    {
      .type     = PT_TIME,
      .id       = "start_extra",
      .name     = N_("Pre-recording padding"),
      .off      = offsetof(dvr_entry_t, de_start_extra),
      .set      = dvr_entry_class_start_extra_set,
      .list     = dvr_entry_class_extra_list,
      .get_opts = dvr_entry_class_start_extra_opts,
    },
    {
      .type     = PT_TIME,
      .id       = "start_real",
      .name     = N_("Scheduled start time"),
      .get      = dvr_entry_class_start_real_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_TIME,
      .id       = "stop",
      .name     = N_("Stop time"),
      .set      = dvr_entry_class_stop_set,
      .off      = offsetof(dvr_entry_t, de_stop),
    },
    {
      .type     = PT_TIME,
      .id       = "stop_extra",
      .name     = N_("Post-recording padding"),
      .off      = offsetof(dvr_entry_t, de_stop_extra),
      .list     = dvr_entry_class_extra_list,
      .opts     = PO_SORTKEY | PO_ADVANCED,
    },
    {
      .type     = PT_TIME,
      .id       = "stop_real",
      .name     = N_("Scheduled stop time"),
      .get      = dvr_entry_class_stop_real_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_TIME,
      .id       = "duration",
      .name     = N_("Duration"),
      .get      = dvr_entry_class_duration_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_DURATION,
    },
    {
      .type     = PT_STR,
      .id       = "channel",
      .name     = N_("Channel"),
      .set      = dvr_entry_class_channel_set,
      .get      = dvr_entry_class_channel_get,
      .rend     = dvr_entry_class_channel_rend,
      .list     = channel_class_get_list,
      .get_opts = dvr_entry_class_start_opts,
    },
    {
      .type     = PT_STR,
      .id       = "channel_icon",
      .name     = N_("Channel icon"),
      .get      = dvr_entry_class_channel_icon_url_get,
      .opts     = PO_HIDDEN | PO_RDONLY | PO_NOSAVE | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "channelname",
      .name     = N_("Channel name"),
      .get      = dvr_entry_class_channel_name_get,
      .set      = dvr_entry_class_channel_name_set,
      .off      = offsetof(dvr_entry_t, de_channel_name),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_LANGSTR,
      .id       = "title",
      .name     = N_("Title"),
      .off      = offsetof(dvr_entry_t, de_title),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "disp_title",
      .name     = N_("Title"),
      .get      = dvr_entry_class_disp_title_get,
      .set      = dvr_entry_class_disp_title_set,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_LANGSTR,
      .id       = "subtitle",
      .name     = N_("Subtitle"),
      .off      = offsetof(dvr_entry_t, de_subtitle),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "disp_subtitle",
      .name     = N_("Subtitle"),
      .get      = dvr_entry_class_disp_subtitle_get,
      .set      = dvr_entry_class_disp_subtitle_set,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_LANGSTR,
      .id       = "description",
      .name     = N_("Description"),
      .off      = offsetof(dvr_entry_t, de_desc),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "disp_description",
      .name     = N_("Description"),
      .get      = dvr_entry_class_disp_description_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN,
    },
    {
      .type     = PT_INT,
      .id       = "pri",
      .name     = N_("Priority"),
      .off      = offsetof(dvr_entry_t, de_pri),
      .def.i    = DVR_PRIO_NORMAL,
      .set      = dvr_entry_class_pri_set,
      .list     = dvr_entry_class_pri_list,
      .opts     = PO_SORTKEY,
    },
    {
      .type     = PT_U32,
      .id       = "retention",
      .name     = N_("DVR log retention"),
      .off      = offsetof(dvr_entry_t, de_retention),
      .def.i    = DVR_RET_DVRCONFIG,
      .list     = dvr_entry_class_retention_list,
      .opts     = PO_HIDDEN | PO_EXPERT
    },
    {
      .type     = PT_U32,
      .id       = "removal",
      .name     = N_("DVR file retention period"),
      .off      = offsetof(dvr_entry_t, de_removal),
      .def.i    = DVR_RET_DVRCONFIG,
      .list     = dvr_entry_class_removal_list,
      .opts     = PO_HIDDEN | PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .id       = "config_name",
      .name     = N_("DVR configuration"),
      .set      = dvr_entry_class_config_name_set,
      .get      = dvr_entry_class_config_name_get,
      .list     = dvr_entry_class_config_name_list,
      .rend     = dvr_entry_class_config_name_rend,
      .get_opts = dvr_entry_class_config_name_opts,
    },
    {
      .type     = PT_STR,
      .id       = "owner",
      .name     = N_("Owner"),
      .off      = offsetof(dvr_entry_t, de_owner),
      .list     = user_get_userlist,
      .get_opts = dvr_entry_class_owner_opts,
    },
    {
      .type     = PT_STR,
      .id       = "creator",
      .name     = N_("Creator"),
      .off      = offsetof(dvr_entry_t, de_creator),
      .get_opts = dvr_entry_class_owner_opts,
    },
    {
      .type     = PT_STR,
      .id       = "filename",
      .name     = N_("Filename"),
      .get      = dvr_entry_class_filename_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "directory",
      .name     = N_("Directory"),
      .off      = offsetof(dvr_entry_t, de_directory),
      .opts     = PO_RDONLY | PO_NOUI,
    },
    {
      .type     = PT_U32,
      .id       = "errorcode",
      .name     = N_("Error code"),
      .off      = offsetof(dvr_entry_t, de_last_error),
      .opts     = PO_RDONLY | PO_NOUI,
    },
    {
      .type     = PT_U32,
      .id       = "errors",
      .name     = N_("Errors"),
      .off      = offsetof(dvr_entry_t, de_errors),
      .opts     = PO_RDONLY | PO_ADVANCED,
    },
    {
      .type     = PT_U32,
      .id       = "data_errors",
      .name     = N_("Data errors"),
      .off      = offsetof(dvr_entry_t, de_data_errors),
      .opts     = PO_RDONLY | PO_ADVANCED,
    },
    {
      .type     = PT_U16,
      .id       = "dvb_eid",
      .name     = N_("DVB EPG ID"),
      .off      = offsetof(dvr_entry_t, de_dvb_eid),
      .opts     = PO_RDONLY | PO_EXPERT,
    },
    {
      .type     = PT_BOOL,
      .id       = "noresched",
      .name     = N_("Don't reschedule"),
      .off      = offsetof(dvr_entry_t, de_dont_reschedule),
      .opts     = PO_HIDDEN | PO_NOUI,
    },
    {
      .type     = PT_BOOL,
      .id       = "norerecord",
      .name     = N_("Don't re-record"),
      .off      = offsetof(dvr_entry_t, de_dont_rerecord),
      .opts     = PO_HIDDEN | PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "autorec",
      .name     = N_("Auto record"),
      .set      = dvr_entry_class_autorec_set,
      .get      = dvr_entry_class_autorec_get,
      .opts     = PO_RDONLY | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "autorec_caption",
      .name     = N_("Auto record caption"),
      .get      = dvr_entry_class_autorec_caption_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "timerec",
      .name     = N_("Auto time record"),
      .set      = dvr_entry_class_timerec_set,
      .get      = dvr_entry_class_timerec_get,
      .opts     = PO_RDONLY | PO_EXPERT,
    },
    {
      .type     = PT_STR,
      .id       = "timerec_caption",
      .name     = N_("Time record caption"),
      .get      = dvr_entry_class_timerec_caption_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "parent",
      .name     = N_("Parent entry"),
      .set      = dvr_entry_class_parent_set,
      .get      = dvr_entry_class_parent_get,
      .opts     = PO_RDONLY | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "child",
      .name     = N_("Slave entry"),
      .set      = dvr_entry_class_child_set,
      .get      = dvr_entry_class_child_get,
      .opts     = PO_RDONLY | PO_NOUI,
    },
    {
      .type     = PT_U32,
      .id       = "content_type",
      .name     = N_("Content type"),
      .list     = dvr_entry_class_content_type_list,
      .off      = offsetof(dvr_entry_t, de_content_type),
      .opts     = PO_RDONLY | PO_SORTKEY,
    },
    {
      .type     = PT_U32,
      .id       = "broadcast",
      .name     = N_("Broadcast"),
      .set      = dvr_entry_class_broadcast_set,
      .get      = dvr_entry_class_broadcast_get,
      .opts     = PO_RDONLY | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "episode",
      .name     = N_("Episode"),
      .off      = offsetof(dvr_entry_t, de_episode),
      .opts     = PO_RDONLY | PO_HIDDEN,
    },
    {
      .type     = PT_STR,
      .id       = "url",
      .name     = N_("URL"),
      .get      = dvr_entry_class_url_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_NOUI,
    },
    {
      .type     = PT_S64,
      .id       = "filesize",
      .name     = N_("File size"),
      .get      = dvr_entry_class_filesize_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "status",
      .name     = N_("Status"),
      .get      = dvr_entry_class_status_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_LOCALE,
    },
    {
      .type     = PT_STR,
      .id       = "sched_status",
      .name     = N_("Schedule status"),
      .get      = dvr_entry_class_sched_status_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN | PO_NOUI,
    },
    {
      .type     = PT_TIME,
      .id       = "duplicate",
      .name     = N_("Rerun of"),
      .get      = dvr_entry_class_duplicate_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .off      = offsetof(dvr_entry_t, de_comment),
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
    free(de->de_channel_name);
    de->de_channel_name = strdup(channel_get_name(ch));
    dvr_entry_purge(de, delconf);
  }
}

/**
 *
 */
const char *
dvr_get_filename(dvr_entry_t *de)
{
  htsmsg_field_t *f;
  htsmsg_t *m;
  const char *s;
  if ((f = htsmsg_field_last(de->de_files)) != NULL &&
      (m = htsmsg_field_get_map(f)) != NULL &&
      (s = htsmsg_get_str(m, "filename")) != NULL)
    return s;
  else
    return NULL;
}

/**
 *
 */
int64_t
dvr_get_filesize(dvr_entry_t *de)
{
  const char *filename;
  struct stat st;

  filename = dvr_get_filename(de);

  if(filename == NULL)
    return -1;

  if(stat(filename, &st) != 0)
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
  { "unimportant", DVR_PRIO_UNIMPORTANT }
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
dvr_entry_delete(dvr_entry_t *de, int no_missed_time_resched)
{
  dvr_config_t *cfg = de->de_config;
  htsmsg_t *m;
  htsmsg_field_t *f;
  time_t t;
  struct tm tm;
  const char *filename;
  char tbuf[64], ubuf[UUID_HEX_SIZE], *rdir, *postcmd;
  int r;

  t = dvr_entry_get_start_time(de, 1);
  localtime_r(&t, &tm);
  if (strftime(tbuf, sizeof(tbuf), "%F %T", &tm) <= 0)
    *tbuf = 0;

  tvhlog(LOG_INFO, "dvr", "delete entry %s \"%s\" on \"%s\" start time %s, "
	 "scheduled for recording by \"%s\", retention \"%s\" removal \"%s\"",
         idnode_uuid_as_str(&de->de_id, ubuf),
	 lang_str_get(de->de_title, NULL), DVR_CH_NAME(de), tbuf,
	 de->de_creator ?: "",
	 dvr_entry_get_retention_string(de),
	 dvr_entry_get_removal_string(de));

  if(!htsmsg_is_empty(de->de_files)) {
#if ENABLE_INOTIFY
    dvr_inotify_del(de);
#endif
    rdir = NULL;
    if(cfg->dvr_title_dir || cfg->dvr_channel_dir || cfg->dvr_dir_per_day || de->de_directory)
      rdir = cfg->dvr_storage;

    HTSMSG_FOREACH(f, de->de_files) {
      m = htsmsg_field_get_map(f);
      if (m == NULL) continue;
      filename = htsmsg_get_str(m, "filename");
      if (filename == NULL) continue;
      r = deferred_unlink(filename, rdir);
      if(r && r != -ENOENT)
        tvhlog(LOG_WARNING, "dvr", "Unable to remove file '%s' from disk -- %s",
  	       filename, strerror(-errno));

      postcmd = de->de_config->dvr_postremove;
      if (postcmd && postcmd[0])
        dvr_spawn_postcmd(de, postcmd, filename);
      htsmsg_delete_field(m, "filename");
    }
  }
  if (no_missed_time_resched)
    dvr_entry_set_state(de, DVR_MISSED_TIME, DVR_RS_PENDING, de->de_last_error);
  else
    dvr_entry_missed_time(de, de->de_last_error);
}

/**
 *
 */
void
dvr_entry_set_rerecord(dvr_entry_t *de, int cmd)
{
  if (cmd < 0) { /* toggle */
    if (de->de_parent) return;
    cmd = de->de_dont_rerecord ? 1 : 0;
  }
  if (cmd == 0 && !de->de_dont_rerecord) {
    de->de_dont_rerecord = 1;
    if (de->de_child)
      dvr_entry_cancel_delete(de->de_child, 0);
  } else {
    de->de_dont_rerecord = 0;
    dvr_entry_rerecord(de);
  }
}

/**
 *
 */
dvr_entry_t *
dvr_entry_stop(dvr_entry_t *de)
{
  if(de->de_sched_state == DVR_RECORDING) {
    de->de_dont_reschedule = 1;
    dvr_stop_recording(de, SM_CODE_OK, 1, 0);
    return de;
  }

  return de;
}

/**
 *
 */
dvr_entry_t *
dvr_entry_cancel(dvr_entry_t *de, int rerecord)
{
  dvr_entry_t *parent = de->de_parent;

  switch(de->de_sched_state) {
  case DVR_RECORDING:
    de->de_dont_reschedule = 1;
    dvr_stop_recording(de, SM_CODE_ABORTED, 1, 0);
    break;

  case DVR_SCHEDULED:
  case DVR_COMPLETED:
  case DVR_MISSED_TIME:
  case DVR_NOSTATE:
    dvr_entry_destroy(de, 1);
    de = NULL;
    break;

  default:
    abort();
  }

  if (parent) {
    if (!rerecord)
      dvr_entry_dont_rerecord(parent, 1);
    else
      dvr_entry_rerecord(parent);
  }

  return de;
}

/**
 *
 */
void
dvr_entry_cancel_delete(dvr_entry_t *de, int rerecord)
{
  dvr_entry_t *parent = de->de_parent;
  dvr_autorec_entry_t *dae = de->de_autorec;

  switch(de->de_sched_state) {
  case DVR_RECORDING:
    de->de_dont_reschedule = 1;
    dvr_stop_recording(de, SM_CODE_ABORTED, 1, 0);
  case DVR_COMPLETED:
    dvr_entry_delete(de, 1);
    dvr_entry_destroy(de, 1);
    break;

  case DVR_SCHEDULED:
  case DVR_MISSED_TIME:
  case DVR_NOSTATE:
    dvr_entry_destroy(de, 1);
    break;

  default:
    abort();
  }

  if (parent) {
    if (!rerecord)
      dvr_entry_dont_rerecord(parent, 1);
    else
      dvr_entry_rerecord(parent);
  }

  // Trigger autorec update in case of max sched count limit
  if (dae && dae->dae_max_sched_count > 0)
    dvr_autorec_changed(dae, 0);

}

/**
 *
 */
void
dvr_entry_init(void)
{
  htsmsg_t *l, *c, *rere;
  htsmsg_field_t *f;
  const char *parent, *child;
  dvr_entry_t *de1, *de2;

  dvr_in_init = 1;
  rere = htsmsg_create_map();
  /* load config, but remove parent/child fields */
  if((l = hts_settings_load("dvr/log")) != NULL) {
    HTSMSG_FOREACH(f, l) {
      if((c = htsmsg_get_map_by_field(f)) == NULL)
	continue;
      parent = htsmsg_get_str(c, "parent");
      child = htsmsg_get_str(c, "child");
      if (parent && parent[0])
        htsmsg_set_str(rere, parent, f->hmf_name);
      if (child && child[0])
        htsmsg_set_str(rere, f->hmf_name, child);
      htsmsg_delete_field(c, "parent");
      htsmsg_delete_field(c, "child");
      (void)dvr_entry_create(f->hmf_name, c, 0);
    }
    htsmsg_destroy(l);
  }
  dvr_in_init = 0;
  /* process parent/child mapping */
  HTSMSG_FOREACH(f, rere) {
    if((child = htsmsg_field_get_str(f)) == NULL)
      continue;
    parent = f->hmf_name;
    de1 = dvr_entry_find_by_uuid(parent);
    de2 = dvr_entry_find_by_uuid(child);
    dvr_entry_change_parent_child(de1, de2, NULL, 0);
  }
  htsmsg_destroy(rere);
  /* update the entry timers, call rerecord */
  for (de1 = LIST_FIRST(&dvrentries); de1; de1 = de2) {
    de2 = LIST_NEXT(de1, de_global_link);
    dvr_entry_set_timer(de1);
  }
}

void
dvr_entry_done(void)
{
  dvr_entry_t *de;
  lock_assert(&global_lock);
  while ((de = LIST_FIRST(&dvrentries)) != NULL)
      dvr_entry_destroy(de, 0);
}
