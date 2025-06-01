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

#include <ctype.h>

#include "tvheadend.h"
#include "settings.h"
#include "dvr.h"
#include "htsp_server.h"
#include "streaming.h"
#include "intlconv.h"
#include "dbus.h"
#include "imagecache.h"
#include "access.h"
#include "notify.h"
#include "compat.h"
#include "string_list.h"
#include "epggrab.h" //Needed to get the epggrab_conf.epgdb_processparentallabels flag.

struct dvr_entry_list dvrentries;
static int dvr_in_init;

#if ENABLE_DBUS_1
static mtimer_t dvr_dbus_timer;
#endif
/// Periodically pre-fetch artwork for scheduled recordings.
static mtimer_t dvr_fanart_timer;
static string_list_t *dvr_fanart_to_prefetch;

static void dvr_entry_deferred_destroy(dvr_entry_t *de);
static void dvr_entry_set_timer(dvr_entry_t *de);
static void dvr_timer_rerecord(void *aux);
static void dvr_timer_expire(void *aux);
static void dvr_timer_disarm(void *aux);
static void dvr_timer_remove_files(void *aux);
static void dvr_entry_start_recording(dvr_entry_t *de, int clone);
static void dvr_timer_start_recording(void *aux);
static void dvr_timer_stop_recording(void *aux);
static int dvr_entry_rerecord(dvr_entry_t *de);
static time_t dvr_entry_get_segment_stop_extra(dvr_entry_t *de);
static void dvr_entry_watched_timer_arm(dvr_entry_t* de);
static void dvr_entry_watched_timer_disarm(dvr_entry_t* de);

static dvr_entry_t *_dvr_duplicate_event(dvr_entry_t *de);

static const void *dvr_entry_class_rating_icon_url_get(void *o);

/*
 *
 */

static struct strtab schedstatetab[] = {
  { "SCHEDULED",  DVR_SCHEDULED },
  { "RECORDING",  DVR_RECORDING },
  { "COMPLETED",  DVR_COMPLETED },
  { "NOSTATE",    DVR_NOSTATE   },
  { "MISSEDTM",   DVR_MISSED_TIME }
};

const char *
dvr_entry_sched_state2str(dvr_entry_sched_state_t s)
{
  return val2str(s, schedstatetab) ?: "INVALID";
}

static struct strtab rsstatetab[] = {
  { "PENDING",    DVR_RS_PENDING },
  { "WAIT",       DVR_RS_WAIT_PROGRAM_START },
  { "RUNNING",    DVR_RS_RUNNING },
  { "COMMERCIAL", DVR_RS_COMMERCIAL },
  { "ERROR",      DVR_RS_ERROR },
  { "EPGWAIT",    DVR_RS_EPG_WAIT },
  { "FINISHED",   DVR_RS_FINISHED }
};

const char *
dvr_entry_rs_state2str(dvr_rs_state_t s)
{
  return val2str(s, rsstatetab) ?: "INVALID";
}

void
dvr_entry_trace_(const char *file, int line, dvr_entry_t *de, const char *fmt, ...)
{
  char buf[512], ubuf[UUID_HEX_SIZE];
  va_list args;
  va_start(args, fmt);
  snprintf(buf, sizeof(buf), "entry %s - %s",
                             idnode_uuid_as_str(&de->de_id, ubuf),
                             fmt);
  tvhlogv(file, line, LOG_TRACE, LS_DVR, buf, &args);
  va_end(args);
}

void
dvr_entry_trace_time2_(const char *file, int line,
                       dvr_entry_t *de,
                       const char *t1name, time_t t1,
                       const char *t2name, time_t t2,
                       const char *fmt, ...)
{
  char buf[512], ubuf[UUID_HEX_SIZE], t1buf[32], t2buf[32];
  va_list args;
  va_start(args, fmt);
  snprintf(buf, sizeof(buf), "entry %s%s%s%s%s%s%s%s%s - %s",
                             idnode_uuid_as_str(&de->de_id, ubuf),
                             t1name ? " " : "",
                             t1name ? t1name : "",
                             t1name ? " " : "",
                             t1name ? gmtime2local(t1, t1buf, sizeof(t1buf)) : "",
                             t2name ? " " : "",
                             t2name ? t2name : "",
                             t2name ? " " : "",
                             t2name ? gmtime2local(t2, t2buf, sizeof(t2buf)) : "",
                             fmt);
  tvhlogv(file, line, LOG_TRACE, LS_DVR, buf, &args);
  va_end(args);
}

int dvr_entry_is_upcoming(dvr_entry_t *entry)
{
  dvr_entry_sched_state_t state = entry->de_sched_state;
  return state == DVR_RECORDING || state == DVR_SCHEDULED || state == DVR_NOSTATE;
}

int dvr_entry_is_upcoming_nodup(dvr_entry_t *entry)
{
  dvr_entry_sched_state_t state = entry->de_sched_state;
  if (_dvr_duplicate_event(entry))
    return 0;
  return state == DVR_RECORDING || state == DVR_SCHEDULED || state == DVR_NOSTATE;
}

int dvr_entry_is_finished(dvr_entry_t *entry, int flags)
{
  if (dvr_entry_is_upcoming(entry))
    return 0;
  if (!flags || (flags & DVR_FINISHED_ALL))
    return 1;

  int removed = entry->de_file_removed ||                       /* Removed by tvheadend */
                (entry->de_sched_state != DVR_MISSED_TIME &&
                 dvr_get_filesize(entry, 0) < 0);		/* Removed externally? */
  int success = entry->de_sched_state == DVR_COMPLETED;

  if (success) {
    if (entry->de_last_error == SM_CODE_OK)
      success = entry->de_data_errors < DVR_MAX_DATA_ERRORS;
    else
      success = dvr_entry_is_completed_ok(entry);
  }

  if ((flags & DVR_FINISHED_REMOVED_SUCCESS) && removed && success)
    return 1;
  if ((flags & DVR_FINISHED_REMOVED_FAILED) && removed && !success)
    return 1;
  if ((flags & DVR_FINISHED_SUCCESS) && success && !removed)
    return 1;
  if ((flags & DVR_FINISHED_FAILED) && !success && !removed)
    return 1;
  return 0;
}

/*
 *
 */
int
dvr_entry_verify(dvr_entry_t *de, access_t *a, int readonly)
{
  if (access_verify2(a, ACCESS_FAILED_RECORDER) &&
      dvr_entry_is_finished(de, DVR_FINISHED_FAILED))
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
void
dvr_entry_changed(dvr_entry_t *de)
{
  idnode_changed(&de->de_id);
  htsp_dvr_entry_update(de);
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
    dvr_entry_trace(de, "set state - state %s rec_state %s error '%s'",
                    dvr_entry_sched_state2str(state),
                    dvr_entry_rs_state2str(rec_state),
                    streaming_code2txt(error_code));
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
int
dvr_entry_assign_broadcast(dvr_entry_t *de, epg_broadcast_t *bcast)
{
  char id[16];
  if (bcast != de->de_bcast) {
    if (de->de_bcast) {
      snprintf(id, sizeof(id), "%u", de->de_bcast->id);
      dvr_entry_trace(de, "unassign broadcast %s", id);
      LIST_REMOVE(de, de_bcast_link);
      de->de_bcast->ops->putref((epg_object_t*)de->de_bcast);
      notify_delayed(id, "epg", "dvr_delete");
      de->de_bcast = NULL;
      de->de_dvb_eid = 0;
    }
    if (bcast) {
      bcast->ops->getref((epg_object_t*)bcast);
      de->de_bcast = bcast;
      LIST_INSERT_HEAD(&bcast->dvr_entries, de, de_bcast_link);
      snprintf(id, sizeof(id), "%u", bcast->id);
      dvr_entry_trace(de, "assign broadcast %s", id);
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
    dvr_entry_trace(de, "don't rerecord change %d", dont_rerecord);
    de->de_dont_rerecord = dont_rerecord;
    dvr_entry_changed(de);
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
      if (save && p && p != origin) idnode_changed(&p->de_id);
      parent->de_child = NULL;
      if (save && origin != parent) idnode_changed(&parent->de_id);
      return 1;
    }
    return 0;
  }
  if (parent == NULL) {
    if (child->de_parent) {
      p = child->de_parent->de_child;
      child->de_parent->de_child = NULL;
      if (save && p && p != origin) idnode_changed(&p->de_id);
      child->de_parent = NULL;
      if (save && origin != child) idnode_changed(&child->de_id);
      return 1;
    }
    return 0;
  }
  if (parent->de_child) {
    p = parent->de_child->de_parent;
    parent->de_child->de_parent = NULL;
    if (save && p) idnode_changed(&p->de_id);
  }
  if (child->de_parent) {
    p = child->de_parent->de_child;
    child->de_parent->de_child = NULL;
    if (save && p) idnode_changed(&p->de_id);
  }
  parent->de_child = child;
  child->de_parent = parent;
  if (save && origin != parent) idnode_changed(&parent->de_id);
  if (save && origin != child) idnode_changed(&child->de_id);
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
  return MIN(de->de_config->dvr_warm_time, 600);
}

time_t
dvr_entry_get_start_time( dvr_entry_t *de, int warm )
{
  int64_t b = (dvr_entry_get_extra_time_pre(de)) +
              (warm ? dvr_entry_warm_time(de) : 0);
  if (de->de_start < b)
    return 0;
  return de->de_start - b;
}

time_t
dvr_entry_get_stop_time( dvr_entry_t *de )
{
  return time_t_out_of_range((int64_t)de->de_stop + dvr_entry_get_segment_stop_extra(de) + dvr_entry_get_extra_time_post(de));
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
  return MINMAX(extra, 0, 24*60) * 60;
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
  return MINMAX(extra, 0, 24*60) * 60;
}

char *
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

char *
dvr_entry_get_removal_string ( dvr_entry_t *de )
{
  char buf[24];
  uint32_t removal = dvr_entry_get_removal_days(de);

  if (removal < DVR_REM_SPACE)
    snprintf(buf, sizeof(buf), "%i days", removal);
  else if (removal == DVR_REM_SPACE)
    return strdup("Until space needed");
  else
    return strdup("Forever");

  return strdup(buf);
}

uint32_t
dvr_entry_get_retention_days( dvr_entry_t *de )
{
  if (de->de_retention > 0) {
    if (de->de_retention > DVR_RET_REM_FOREVER)
      return DVR_RET_REM_FOREVER;

    return de->de_retention;
  }
  return dvr_retention_cleanup(de->de_config->dvr_retention_days);
}

uint32_t
dvr_entry_get_removal_days ( dvr_entry_t *de )
{
  if (de->de_removal > 0) {
    if (de->de_removal > DVR_RET_REM_FOREVER)
      return DVR_RET_REM_FOREVER;

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
  channel_t *ch;

  if (de->de_dvb_eid == 0)
    return 0;
  if ((ch = de->de_channel) == NULL)
    return 0;
  if (ch->ch_epg_running < 0)
    return de->de_config->dvr_running;
  return ch->ch_epg_running > 0;
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
    if (gclk() < start && start > max)
      max = start;
  }
  /* lower the maximum value */
  result = max;
  LIST_FOREACH(de, &dvrentries, de_global_link) {
    if (de->de_sched_state != DVR_SCHEDULED)
      continue;
    start = dvr_entry_get_start_time(de, 1);
    if (gclk() < start && start < result)
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
  if (rerecord && (when - gclk()) > 3600) {
    when = gclk() + 3600;
    cb = dvr_timer_rerecord;
    dvr_entry_trace_time1(de, "when", when, "retention arm - rerecord");
  } else {
    dvr_entry_trace_time1(de, "when", when, "retention arm", "when");
  }
  gtimer_arm_absn(&de->de_timer, cb, de, when);
}

/*
 * Returns 1 if the database entry should be deleted on file removal
 * NOTE: retention can be postponed when retention < removal
 */
static int
dvr_entry_delete_retention_expired(dvr_entry_t *de)
{
  uint32_t retention = dvr_entry_get_retention_days(de);
  time_t stop;

  if (retention == DVR_RET_ONREMOVE)
    return 1;
  if (retention < DVR_RET_ONREMOVE) {
    stop = time_t_out_of_range((int64_t)de->de_stop + retention * (int64_t)86400);
    if (stop <= gclk())
      return 1;
  }
  return 0;
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
  int save;

  if ((removal > 0 || retention == 0) && removal < DVR_REM_SPACE && dvr_get_filesize(de, 0) > 0) {
    stop = time_t_out_of_range((int64_t)de->de_stop + removal * (int64_t)86400);
    if (stop > gclk()) {
      dvr_entry_retention_arm(de, dvr_timer_remove_files, stop);
      return;
    }
    save = 0;
    if (dvr_get_filename(de))
      save = dvr_entry_delete(de);                // delete actual file
    if (dvr_entry_delete_retention_expired(de)) { // in case retention was postponed (retention < removal)
      dvr_entry_deferred_destroy(de);             // also remove database entry
      return;
    }
    if (save)
      dvr_entry_changed(de);
  }

  if (retention < DVR_RET_ONREMOVE &&
      (dvr_get_filesize(de, 0) < 0 || removal == DVR_RET_REM_FOREVER)) { // we need the database entry for later file removal
    stop = time_t_out_of_range((int64_t)de->de_stop + retention * (int64_t)86400);
    dvr_entry_retention_arm(de, dvr_timer_expire, stop);
  }
  else {
    dvr_entry_retention_arm(de, dvr_timer_disarm,
        dvr_entry_get_rerecord_errors(de) ? INT_MAX : 0); // extend disarm to keep the rerecord logic running
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
static int
dvr_entry_completed(dvr_entry_t *de, int error_code)
{
  int change;
  change = dvr_entry_set_state(de, DVR_COMPLETED, DVR_RS_FINISHED, error_code);
#if ENABLE_INOTIFY
  dvr_inotify_add(de);
#endif
  dvr_entry_retention_timer(de);
  if (de->de_autorec)
    dvr_autorec_completed(de->de_autorec, error_code);
  return change;
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
      case SM_CODE_USER_LIMIT:
      case SM_CODE_NO_SPACE:
      case SM_CODE_PREVIOUSLY_RECORDED:
        return streaming_code2txt(de->de_last_error);
      default:
        break;
    }
    if (dvr_get_filesize(de, 0) < 0 && !de->de_file_removed)
      return N_("File missing");
    if(de->de_last_error != SM_CODE_FORCE_OK &&
       de->de_data_errors >= DVR_MAX_DATA_ERRORS) /* user configurable threshold? */
      return N_("Too many data errors");
    if(de->de_last_error)
      return streaming_code2txt(de->de_last_error);
    else
      return N_("Completed OK");

  case DVR_MISSED_TIME:
    if (de->de_last_error == SM_CODE_SVC_NOT_ENABLED || de->de_last_error == SM_CODE_NO_SPACE)
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
    if(!dvr_entry_is_completed_ok(de) ||
       (dvr_get_filesize(de, 0) < 0 && !de->de_file_removed))
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

int
dvr_entry_allow_fanart_lookup(const dvr_entry_t *de)
{
  char ubuf[UUID_HEX_SIZE];

  /* User doesn't want us to fetch artwork? */
  if (!de || !de->de_config || !de->de_config->dvr_fetch_artwork)
    return 0;

  /* Entry already have artwork? So nothing to do */
  if (de->de_image && *de->de_image &&
      de->de_fanart_image && *de->de_fanart_image)
    return 0;

  /* Allow any artwork even if we can't identify episode? */
  if (de->de_config->dvr_fetch_artwork_allow_unknown)
    return 1;

  /* Only want 'good' episodes since ones with bad data will get
   * bad artwork.  Good episodes have a season/episode (assume
   * episode) or year (assume movie).
   */
  if (!de->de_epnum.s_num && !de->de_epnum.e_num &&
      !de->de_copyright_year) {
    tvhdebug(LS_DVR, "Ignoring fanart for entry without good data for %s \"%s\"",
             lang_str_get(de->de_title, NULL),
             idnode_uuid_as_str(&de->de_id, ubuf));
    return 0;
  }

  return 1;
}

/// Add the entry details to a list of fanart to prefetch.
/// We then periodically check the list to update artwork.
/// We don't do the check too frequently since most providers
/// have limits on how frequently artwork can be fetched.
/// It doesn't matter if the entry gets deleted before we
/// perform the check.
static void
dvr_entry_fanart_add_to_prefetch(const dvr_entry_t *de)
{
  char ubuf[UUID_HEX_SIZE];

  if (!dvr_entry_allow_fanart_lookup(de))
    return;

  string_list_insert(dvr_fanart_to_prefetch,
                     idnode_uuid_as_str(&de->de_id, ubuf));
}

static void
dvr_entry_fanart_prefetch_cb(void *aux)
{
  char *id;
  dvr_entry_t *de;

  lock_assert(&global_lock);

  /* Only do one entry, even if list has many since we don't
   * want to overload fanart providers.
   *
   * However we may have items on the list that don't need
   * fanart lookup, so skip over any of this.
   */
  int done_one = 0;
  while (!done_one) {
    id = string_list_remove_first(dvr_fanart_to_prefetch);
    /* No entries left on list? */
    if (!id)
      break;

    de = dvr_entry_find_by_uuid(id);
    if (dvr_entry_allow_fanart_lookup(de)) {
      tvhinfo(LS_DVR, "Prefetching artwork for %s \"%s\"", id, lang_str_get(de->de_title, NULL));
      dvr_spawn_fetch_artwork(de);
      done_one = 1;
    }

    free(id);
  } /* !done_one */

  // Re-arm timer with a slight random factor to avoid queries at same
  // time every hour.
  mtimer_arm_rel(&dvr_fanart_timer, dvr_entry_fanart_prefetch_cb, NULL, sec2mono(3600 + random() % 900));
}

void
dvr_entry_set_timer(dvr_entry_t *de)
{
  time_t now = gclk(), start, stop;

  if (dvr_in_init)
    return;

  start = dvr_entry_get_start_time(de, 1);
  stop  = dvr_entry_get_stop_time(de);

  dvr_entry_trace_time2(de, "start", start, "stop", stop, "set timer");

  if (now >= stop || de->de_dont_reschedule) {

    /* EPG thinks that the program is running */
    if(de->de_running_start > de->de_running_stop && !de->de_dont_reschedule) {
      stop = now + 10;
      if (de->de_sched_state == DVR_RECORDING) {
        dvr_entry_trace_time1(de, "stop", stop, "set timer - running+");
        goto recording;
      }
    }

    if (de->de_sched_state == DVR_RECORDING) {
      dvr_stop_recording(de, de->de_last_error, 1, 0);
      return;
    }

    /* Files are missing and job was completed */
    if(htsmsg_is_empty(de->de_files) && !de->de_dont_reschedule)
      dvr_entry_missed_time(de, de->de_last_error);
    else
      dvr_entry_completed(de, de->de_last_error);

    if(dvr_entry_rerecord(de))
      return;

  } else if (de->de_sched_state == DVR_RECORDING)  {
    if (!de->de_enabled) {
      dvr_stop_recording(de, SM_CODE_ABORTED, 1, 0);
      return;
    }

recording:
    dvr_entry_trace_time1(de, "stop", stop, "set timer - arm");
    gtimer_arm_absn(&de->de_timer, dvr_timer_stop_recording, de, stop);

  } else if (de->de_channel && de->de_channel->ch_enabled) {

    dvr_entry_set_state(de, DVR_SCHEDULED, DVR_RS_PENDING, de->de_last_error);

    dvr_entry_trace_time1(de, "start", start, "set timer - schedule");
    gtimer_arm_absn(&de->de_timer, dvr_timer_start_recording, de, start);
    dvr_entry_fanart_add_to_prefetch(de);
#if ENABLE_DBUS_1
    mtimer_arm_rel(&dvr_dbus_timer, dvr_dbus_timer_cb, NULL, sec2mono(5));
#endif

  } else {

    dvr_entry_trace(de, "set timer - nostate");
    dvr_entry_nostate(de, de->de_last_error);

  }
}


/**
 * Get episode name
 */
static char *
dvr_entry_get_episode(epg_broadcast_t *bcast, char *buf, int len)
{
  if (!bcast)
    return NULL;
  if (epg_broadcast_epnumber_format(bcast,
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
  epg_episode_num_t epnum;

  /* Wrong length (+/-20%) */
  t1 = de->de_stop - de->de_start;
  t2 = e->stop - e->start;
  if (labs((long)(t2 - t1)) > (t1 / 5))
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
  if (strcasecmp(title1, title2))
    return 0;

  /* episode check */
  epg_broadcast_get_epnum(e, &epnum);
  if (epg_episode_number_cmpfull(&epnum, &de->de_epnum))
    return 0;

  return 1;
}

/*
 * Expect season and episode from an input string of
 * "Season A/B.Episode Y/Z"
 * or "Season A.Episode Y/Z"
 * or "Season A.Episode Y"
 * or some combination.
 */
static int extract_season_episode(epg_episode_num_t *epnum, const char *text)
{
  uint32_t s = 0, sc = 0, e = 0, ec = 0;
  const char *ch = text;

  memset(epnum, 0, sizeof(*epnum));

  /* Extract season and season count */
  if (strncasecmp(ch, "Season", 6))
    goto _episode;

  ch += 6;
  for (; *ch == ' '; ch++);
  for (; isdigit(*ch); ch++)
    s = (s * 10) + (*ch - '0');
  if (*ch == '/') {
    for (ch++; isdigit(*ch); ch++)
      sc = (sc * 10) + (*ch - '0');
  }

  /* Sanity check */
  if (*ch != '.')
    return 0;
  ch++;

  /* Extract episode and episode count */
_episode:
  if (strncasecmp(ch, "Episode", 7))
    return 0;
  ch += 7;
  for (; *ch == ' '; ch++);
  for (; isdigit(*ch); ch++)
    e = (e * 10) + (*ch - '0');
  if (*ch == '/') {
    for (ch++; isdigit(*ch); ch++)
      ec = (ec * 10) + (*ch - '0');
  }

  /* end-of-string check */
  if (*ch)
    return 0;

  if (e == 0)
    return 0;

  epnum->s_num = s;
  epnum->s_cnt = sc;
  epnum->e_num = e;
  epnum->e_cnt = ec;
  return 1;
}

/**
 * Create the event from config
 */
dvr_entry_t *
dvr_entry_create(const char *uuid, htsmsg_t *conf, int clone)
{
  dvr_entry_t *de, *de2;
  int64_t start, stop, create, watched, now;
  htsmsg_t *m;
  char ubuf[UUID_HEX_SIZE], ubuf2[UUID_HEX_SIZE];
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
      tvhwarn(LS_DVR, "invalid entry uuid '%s'", uuid);
    free(de);
    return NULL;
  }

  de->de_enabled = 1;
  de->de_config = dvr_config_find_by_name_default(NULL);
  if (de->de_config)
    LIST_INSERT_HEAD(&de->de_config->dvr_entries, de, de_config_link);
  de->de_pri = DVR_PRIO_DEFAULT;

  idnode_load(&de->de_id, conf);

  /* Time the node was created. */
  if (!htsmsg_get_s64(conf, "create", &create)) {
      de->de_create = create;
  } else {
      /* Old dvr entry without a create time, so fake one.  For
       * entries that are older than a day, assume these are old
       * (tvh4.2) recordings, so use the start time as the create
       * time. This will allow user to order records by create time
       * and get something reasonable. For all new records however, we
       * use now since they will be written to disk.
       */
      now = gclk();
      if (now > start && start < now - 86400)
          create = start;
      else
          create = now;
      de->de_create = create;
  }
  if (!htsmsg_get_s64(conf, "watched", &watched)) {
    de->de_watched = watched;
  } else {
    de->de_watched = 0;
  }

  /* Extract episode info */
  s = htsmsg_get_str(conf, "episode");
  if (s) {
    if (!extract_season_episode(&de->de_epnum, s))
      de->de_epnum.text = strdup(s);
  } else {
    m = htsmsg_get_map(conf, "episode");
    if (m)
      epg_episode_epnum_deserialize(m, &de->de_epnum);
  }

  /* filenames */
  m = htsmsg_get_list(conf, "files");
  if (m)
    de->de_files = htsmsg_copy(m);

  de->de_refcnt = 1;

  LIST_INSERT_HEAD(&dvrentries, de, de_global_link);

  /* We do early duplicate checking. Otherwise we have the scenario
   * where we have a dvr entry already on disk and an autorec creates
   * an entry that matches the same programme details in the future (a
   * repeat), but we still create a dvr_entry for that schedule and
   * then immediately cancel it when the start time arrives. So,
   * instead, we cancel that duplicate here and now.
   *
   * Note: this check has to be done _after_ insert in to de_global_link
   * otherwise the destroy will abort.
   */
  if (_dvr_duplicate_event(de)) {
      tvhtrace(LS_DVR, "Entry was duplicate for %s \"%s\" on \"%s\" start time %"PRId64", "
          "scheduled for recording by \"%s\"",
          idnode_uuid_as_str(&de->de_id, ubuf),
          lang_str_get(de->de_title, NULL), DVR_CH_NAME(de),
          (int64_t)de->de_start, de->de_creator ?: "");
      dvr_entry_destroy(de, 1);
      return NULL;
  }

  if (de->de_channel && !de->de_dont_reschedule && !clone) {
    LIST_FOREACH(de2, &de->de_channel->ch_dvrs, de_channel_link)
      if(de2 != de &&
         de2->de_config == de->de_config &&
         de2->de_start == de->de_start &&
         de2->de_sched_state != DVR_COMPLETED &&
         de2->de_sched_state != DVR_MISSED_TIME &&
         strcmp(de2->de_owner ?: "", de->de_owner ?: "") == 0) {
        tvhinfo(LS_DVR, "delete entry %s \"%s\" on \"%s\" start time %"PRId64", "
           "scheduled for recording by \"%s\" (duplicate with %s)",
          idnode_uuid_as_str(&de->de_id, ubuf),
          lang_str_get(de->de_title, NULL), DVR_CH_NAME(de),
          (int64_t)de2->de_start, de->de_creator ?: "",
          idnode_uuid_as_str(&de2->de_id, ubuf2));
        dvr_entry_destroy(de, 1);
        return NULL;
      }
  }

  if (!clone)
    dvr_entry_set_timer(de);

  /* Entry is marked for deletion, so set timer. */
  if (de->de_watched)
    dvr_entry_watched_timer_arm(de);

  htsp_dvr_entry_add(de);
  dvr_vfs_refresh_entry(de);

  return de;
}

/**
 * Create the event
 */
dvr_entry_t *
dvr_entry_create_from_htsmsg(htsmsg_t *conf, epg_broadcast_t *e)
{
  dvr_entry_t *de;
  char tbuf[64], *s;
  struct tm tm;
  time_t t;
  char ubuf[UUID_HEX_SIZE];
  epg_genre_t *genre;
  int summary_used = 0;

  if (e) {
    htsmsg_add_u32(conf, "broadcast", e->id);
    htsmsg_add_u32(conf, "dvb_eid", e->dvb_eid);
    htsmsg_set_uuid(conf, "channel", &e->channel->ch_id.in_uuid);
    if (!htsmsg_field_find(conf, "start"))
      htsmsg_add_s64(conf, "start", e->start);
    if (!htsmsg_field_find(conf, "stop"))
      htsmsg_add_s64(conf, "stop", e->stop);
    if (e->title)
      lang_str_serialize(e->title, conf, "title");
    if (e->subtitle)
      lang_str_serialize(e->subtitle, conf, "subtitle");
    if (e->description)
      lang_str_serialize(e->description, conf, "description");
    else if (e->summary) {
      lang_str_serialize(e->summary, conf, "description");
      summary_used = 1;
    }
    if (!summary_used && e->summary)
      lang_str_serialize(e->summary, conf, "summary");
    if ((s = dvr_entry_get_episode(e, tbuf, sizeof(tbuf))) != NULL)
      htsmsg_add_str(conf, "episode", s);
    if (e->copyright_year)
      htsmsg_add_u32(conf, "copyright_year", e->copyright_year);
    if (e->episodelink)
      htsmsg_add_str(conf, "uri", e->episodelink->uri);
    if (e->image)
      htsmsg_add_str(conf, "image", e->image);
    genre = LIST_FIRST(&e->genre);
    if (genre)
      htsmsg_add_u32(conf, "content_type", genre->code / 16);
    if(e->age_rating)
      htsmsg_add_u32(conf, "age_rating", e->age_rating);

    //Only process these fields if rating labels are enabled.
    if(epggrab_conf.epgdb_processparentallabels){
      if(e->rating_label){
        htsmsg_set_uuid(conf, "rating_label_uuid", &e->rating_label->rl_id.in_uuid);

        if(e->rating_label->rl_icon){
          htsmsg_add_str(conf, "rating_icon_saved", imagecache_get_propstr(e->rating_label->rl_icon, tbuf, sizeof(tbuf)));
        }

        if(e->rating_label->rl_country){
          htsmsg_add_str(conf, "rating_country_saved", e->rating_label->rl_country);
        }

        if(e->rating_label->rl_authority){
          htsmsg_add_str(conf, "rating_authority_saved", e->rating_label->rl_authority);
        }

      }
    }//END rating labels enabled.

  }

  de = dvr_entry_create(NULL, conf, 0);

  if (de == NULL)
    return NULL;

  t = dvr_entry_get_start_time(de, 1);
  localtime_r(&t, &tm);
  if (strftime(tbuf, sizeof(tbuf), "%F %T", &tm) <= 0)
    *tbuf = 0;

  tvhinfo(LS_DVR, "entry %s \"%s\" on \"%s\" starting at %s, "
         "with broadcast id \"%s\", "
	 "scheduled for recording by \"%s\"",
         idnode_uuid_as_str(&de->de_id, ubuf),
         lang_str_get(de->de_title, NULL), DVR_CH_NAME(de), tbuf,
         de->de_uri ? de->de_uri : "<noid>",
         de->de_creator ?: "");

  idnode_changed(&de->de_id);
  return de;
}

/**
 * Return the event for the last segment.
 */
static epg_broadcast_t *dvr_entry_get_segment_last( dvr_entry_t *de )
{
  time_t start, stop, maximum_stop_time;
  int max_progs_to_check;
  epg_broadcast_t *e, *next, *ret;
  epg_set_t *eplink1, *eplink2;

  if (!de)
    return NULL;

  /* If we have no broadcast data then can not search for matches */
  e = de->de_bcast;
  if (!e)
    return NULL;

  eplink1 = e->episodelink;

  /* If not a segmented programme then no segment extra time */
  if (eplink1 == NULL ||
      strncmp(eplink1->uri, "crid://", 7) ||
      !strstr(eplink1->uri, "#"))
    return 0;

  /* This URI is a CRID (from EIT) which contains an IMI so the
   * programme is segmented such as part1, <5 minute news>, part2. So
   * we need to check if the next few programmes have the same
   * crid+imi in which case we extend our stop time to include that.
   *
   * We record the "news" programme too since often these segmented
   * programmes have incorrect start/stop times.  This is also
   * consistent with xmltv that normally just merges these programmes
   * together.
   *
   * The Freeview NZ documents say we only need to check for segments
   * on the same channel and where each segment is within three hours
   * of the end of the previous segment. We'll use that as best
   * practice.
   *
   * We also put a couple of safety checks on the number of programmes
   * we check in case broadcast data is bad.
   */
  enum
  {
    THREE_HOURS   = 3 * 60 * 60,
    MAX_STOP_TIME = 9 * 60 * 60
  };

  ret = NULL;
  start = e->start;
  stop  = e->stop;
  maximum_stop_time = start + MAX_STOP_TIME;
  max_progs_to_check = 10;
  for (next = epg_broadcast_get_next(e);
       --max_progs_to_check && stop < maximum_stop_time &&
         next && next->start < stop + THREE_HOURS;
       next = epg_broadcast_get_next(next)) {
    eplink2 = next->episodelink;
    if (eplink2 && eplink1 == eplink2) {
      /* Identical CRID+IMI. So that means that programme is a
       * segment part of this programme. So extend our stop time
       * to include this programme.
       */
      ret = next;
    }
  }

  return ret;
}

/**
 * Determine stop time for the broadcast taking in
 * to account segmented programmes via EIT.
 */
static time_t dvr_entry_get_segment_stop_extra( dvr_entry_t *de )
{
  time_t segment_stop_extra;
  epg_broadcast_t *e;

  if (!de)
    return 0;

  /* Return any cached value we have previous calculated. */
  if (de->de_segment_stop_extra)
    return de->de_segment_stop_extra;

  /* Do not update stop extra without the last segment */
  e = dvr_entry_get_segment_last(de);
  if (!e)
    return 0;

  segment_stop_extra = e->stop - de->de_bcast->stop;
  if (segment_stop_extra != de->de_segment_stop_extra) {
    tvhinfo(LS_DVR, "Increasing stop for \"%s\" on \"%s\" \"%s\" by %"PRId64
                    " seconds at start %"PRId64" and original stop %"PRId64,
                    lang_str_get(e->title, NULL), DVR_CH_NAME(de), e->episodelink->uri,
                    (int64_t)segment_stop_extra, (int64_t)de->de_bcast->start,
                    (int64_t)de->de_bcast->stop);
    de->de_segment_stop_extra = segment_stop_extra;
  }

  return segment_stop_extra;
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
      dvr_stop_recording(de, SM_CODE_SOURCE_RECONFIGURED, 1, 1);
      dvr_rec_migrate(de, n);
      n->de_start = gclk();
      dvr_entry_start_recording(n, 1);
    } else {
      dvr_entry_set_timer(n);
    }
    idnode_changed(&n->de_id);
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
  char buf[512];
  int64_t fsize1, fsize2;
  time_t pre;
  uint32_t warm;
  htsmsg_t *conf;

  if (dvr_in_init || de->de_dont_rerecord)
    return 0;
  rerecord = dvr_entry_get_rerecord_errors(de);
  if (rerecord == 0)
    return 0;
  if ((de2 = de->de_parent) != NULL) {
    if (de->de_sched_state == DVR_COMPLETED &&
        de->de_errors == 0 &&
        de->de_data_errors < de2->de_data_errors) {
      fsize1 = dvr_get_filesize(de, DVR_FILESIZE_TOTAL);
      fsize2 = dvr_get_filesize(de2, DVR_FILESIZE_TOTAL);
      if (fsize1 / 5 < fsize2 / 6) {
        goto not_so_good;
      } else {
        dvr_entry_cancel_delete(de2, 1);
      }
    } else if (de->de_sched_state == DVR_COMPLETED) {
      if (dvr_get_filesize(de, 0) < 0) {
delete_me:
        dvr_entry_cancel_delete(de, 0);
        dvr_entry_rerecord(de2);
        return 1;
      }
not_so_good:
      de->de_retention = DVR_RET_ONREMOVE;
      de->de_removal = DVR_RET_REM_1DAY;
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
  pre = dvr_entry_get_extra_time_pre(de);
  warm = dvr_entry_warm_time(de);
  RB_FOREACH(ev, &de->de_channel->ch_epg_schedule, sched_link) {
    if (de->de_bcast == ev) continue;
    if (ev->start - pre - warm < gclk()) continue;
    if (dvr_entry_fuzzy_match(de, ev, 0, INT64_MAX))
      if (!e || e->start > ev->start)
        e = ev;
  }

  if (e == NULL)
    return 0;

  dvr_entry_trace_time2(de, "start", e->start, "stop", e->stop,
                        "rerecord event %s on %s",
                        epg_broadcast_get_title(e, NULL),
                        channel_get_name(e->channel, channel_blank_name));

  snprintf(buf, sizeof(buf), _("Re-record%s%s"),
           de->de_comment ? ": " : "", de->de_comment ?: "");
  conf = htsmsg_create_map();
  htsmsg_add_uuid(conf, "config_name", &de->de_config->dvr_id.in_uuid);
  htsmsg_add_s64(conf, "start_extra", de->de_start_extra);
  htsmsg_add_s64(conf, "stop_extra", de->de_stop_extra);
  htsmsg_add_u32(conf, "pri", de->de_pri);
  htsmsg_add_u32(conf, "retention", de->de_retention);
  htsmsg_add_u32(conf, "removal", de->de_removal);
  htsmsg_add_str2(conf, "owner", de->de_owner);
  htsmsg_add_str2(conf, "creator", de->de_creator);
  htsmsg_add_str(conf, "comment", buf);
  htsmsg_add_str2(conf, "directory", de->de_directory);
  de2 = dvr_entry_create_from_htsmsg(conf, e);
  htsmsg_destroy(conf);

  if (de2) {
    dvr_entry_change_parent_child(de, de2, NULL, 1);
  } else {
    /* we have already queued similar recordings, mark as resolved */
    de->de_dont_rerecord = 1;
    idnode_changed(&de->de_id);
  }

  return 0;
}

/**
 *
 */
typedef int (*_dvr_duplicate_fcn_t)(dvr_entry_t *de, dvr_entry_t *de2, void **aux);

static int _dvr_duplicate_epnum(dvr_entry_t *de, dvr_entry_t *de2, void **aux)
{
  return ! epg_episode_number_cmp(&(de->de_epnum), &(de2->de_epnum)) ;
}

static int _dvr_duplicate_title(dvr_entry_t *de, dvr_entry_t *de2, void **aux)
{
  return !lang_str_compare(de->de_title, de2->de_title);
}

static int _dvr_duplicate_subtitle(dvr_entry_t *de, dvr_entry_t *de2, void **aux)
{
  return !lang_str_compare(de->de_subtitle, de2->de_subtitle);
}

static int _dvr_duplicate_desc(dvr_entry_t *de, dvr_entry_t *de2, void **aux)
{
  return !lang_str_compare(de->de_desc, de2->de_desc);
}

static int _dvr_duplicate_per_month(dvr_entry_t *de, dvr_entry_t *de2, void **aux)
{
  struct tm *de1_start = *aux, de2_start;
  if (de1_start == NULL) {
    de1_start = calloc(1, sizeof(*de1_start));
    localtime_r(&de->de_start, de1_start);
    *aux = de1_start;
  }
  localtime_r(&de2->de_start, &de2_start);
  return de1_start->tm_year == de2_start.tm_year &&
         de1_start->tm_mon == de2_start.tm_mon;
}

static int _dvr_duplicate_per_week(dvr_entry_t *de, dvr_entry_t *de2, void **aux)
{
  struct tm *de1_start = *aux, de2_start;
  if (de1_start == NULL) {
    de1_start = calloc(1, sizeof(*de1_start));
    localtime_r(&de->de_start, de1_start);
    de1_start->tm_mday -= (de1_start->tm_wday + 6) % 7; // week = mon-sun
    mktime(de1_start); // adjusts de_start
    *aux = de1_start;
  }
  localtime_r(&de2->de_start, &de2_start);
  de2_start.tm_mday -= (de2_start.tm_wday + 6) % 7; // week = mon-sun
  mktime(&de2_start); // adjusts de2_start
  return de1_start->tm_year == de2_start.tm_year &&
         de1_start->tm_yday == de2_start.tm_yday;
}

static int _dvr_duplicate_per_day(dvr_entry_t *de, dvr_entry_t *de2, void **aux)
{
  struct tm *de1_start = *aux, de2_start;
  if (de1_start == NULL) {
    de1_start = calloc(1, sizeof(*de1_start));
    localtime_r(&de->de_start, de1_start);
    *aux = de1_start;
  }
  localtime_r(&de2->de_start, &de2_start);
  return de1_start->tm_year == de2_start.tm_year &&
         de1_start->tm_yday == de2_start.tm_yday;
}

/// Does the de have a crid programid?
static int is_uri_crid(const dvr_entry_t *de)
{
    return de && de->de_uri && !strncmp(de->de_uri, "crid://", 7);
}

/// Is the uri in the de one that is suitable for de-dup. See Issue
/// #4652 for details.
/// @return NULL if no id suitable, else the id that can be used for dedup.
static const char *_dvr_duplicate_get_dedup_program_id(const dvr_entry_t *de)
{
  if (!de) return 0;

  const char *ep_uri = de->de_uri;
  if (!ep_uri) return NULL;

  /* Only want to dedup on crid (unique broadcast id) or dd-progid
   * (unique xmltvid).
   */

  /* It's a crid? If so, we can use the id as the unique indicator */
  if (is_uri_crid(de))
    return ep_uri + 7;          /* +7 to skip past crid:// */

  if (!strncmp(ep_uri, "ddprogid://", 11)) {
    /* Unfortunately, we prepend the grabber to the id, which can be "xmltv"
     * or "/usr/bin/tv_grab_combiner"! Since the ddprogid can't contain /
     * we can just return the last component.
     */
    const char *last = strrchr(ep_uri, '/');
    /* "Can't" happen since by definition ddprogid:// must contain a slash */
    if (!last) return ep_uri;
    /* Return the bit after the last slash. */
    return last+1;              /* +1 to skip '/' */
  }
  /* Must be an internal tvh:// id or some other id we consider not unique */
  return NULL;
}

/// @return 1 if dup.
static int _dvr_duplicate_unique_match(dvr_entry_t *de1, dvr_entry_t *de2, void **aux)
{
  enum { NOT_DUP = 0, DUP = 1 };
  /* We should always have entries so if something is wrong it is a dup */
  if (!de1 || !de2) return DUP;

  const char *progid1  = _dvr_duplicate_get_dedup_program_id(de1);
  const char *progid2  = _dvr_duplicate_get_dedup_program_id(de2);
  if (progid1 && progid2 && !strcmp(progid1, progid2)) return DUP;

  const lang_str_t *title1 = de1->de_title;
  const lang_str_t *title2 = de2->de_title;

  if (!title1 || !title2) return NOT_DUP;

  /* Titles not equal? Can't be a dup then */
  if (lang_str_compare(de1->de_title, de2->de_title)) return NOT_DUP;

  /* We want to dup match only if both season
   * AND episode are present since OTA often have just "Ep 1" without
   * giving the season.
   */
  if (de1->de_epnum.s_num && de1->de_epnum.e_num &&
      de2->de_epnum.s_num && de2->de_epnum.e_num)
    return de1->de_epnum.s_num == de2->de_epnum.s_num && de1->de_epnum.e_num == de2->de_epnum.e_num ? DUP : NOT_DUP;

  /* If we don't have Season & Episode, then we will try matching just
   * season and just episode (if available). This is because if OTA
   * has just "Ep 1" then we don't know the season (so we would have
   * season 0 internally even though it could be S1 or S2), so we
   * can't know if it is a DUP with another showing of "Ep 1" (could
   * be different season), but we _do_ know that if they differ then
   * it's definitely a different episode.
   *
   * So:
   * - Ep1 == Ep1 ==> don't know if it's a dup or not
   * - Ep1 != Ep2 ==> definitely not a dup.
   * - S1 == S1 ==> don't know if dup or not
   * - S1 != S2 ==> definitely not a dup.
   * This assumes consistent metadata (so always have Season & Episode
   * info, or always only have same partial info of just season or
   * just episode).
   */
  if (de1->de_epnum.s_num != de2->de_epnum.s_num)
    return NOT_DUP;
  if (de1->de_epnum.e_num != de2->de_epnum.e_num)
    return NOT_DUP;

  /* Now, near the end, we can check for unequal programme ids. We do
   * this check relatively late since we want dup checking above to
   * match semantically the same episodes to allow previous dvr log
   * entries (before functionality of id was introduced) to be matched
   * against new entries.
   *
   * But only do this check for OTA if they both have the same
   * authority. This is because some broadcasters put the same episode
   * on two regional channels with identical details but different
   * crid authority. So if authority is different then we ignore
   * the crid completely, but if authority is the same then the
   * crid can be checked.
   */

  const int is_uri_crid1 = is_uri_crid(de1);
  const int is_uri_crid2 = is_uri_crid(de2);
  const int is_both_crid = is_uri_crid1 && is_uri_crid2;
  int is_same_authority = 1;
  if (is_both_crid) {
    /* If both entries have a crid (OTA) then check if they are from
     * the same authority. This is the bit before the final slash.
     *
     * So:
     * crid://a.com/44
     * 012345678901234
     * So reverse slash would find the char at id+12 so +1 for len
     * to include the slash itself.
     */
    if (progid1 && progid2) {
      const char *slash = strchr(progid1, '/');
      const ssize_t num_char = slash ? slash + 1 - progid1 : 0;
      is_same_authority = !strncmp(progid1, progid2, num_char);
    } else {
      is_same_authority = 0;
    }
  }

  const int do_progid_check =
    (!is_uri_crid1 && !is_uri_crid2) ||    /* e.g., xmltv */
    (is_both_crid && is_same_authority) || /* e.g, OTA on same broadcaster */
    (is_uri_crid1 && !is_uri_crid2) ||     /* e.g., OTA vs xmltv channel */
    (!is_uri_crid1 && is_uri_crid2);

  /* And we can finally here do 'different programme id'
   * check. Programme id identical check is done first, but non-identical
   * check is done late since some broadcasters put different programme id
   * for different region broadcasts so we want to match on episode details
   * first.
   */
  if (do_progid_check && progid1 && progid2 && strcmp(progid1, progid2))
    return NOT_DUP;

  /* Only one side has an id? We do nothing for this case since programmes in old dvr/log
   * don't have a programid persisted.
   *
   * if (!progid1 || !progid2) return NOT_DUP;
   */

  /* Identical subtitle AND identical description so maybe identical episode.
   * Some daytime OTA programmes have identical everything, but thankfully have
   * different crid so would be NOT_DUP at first check. But if everything is
   * identical then user should resort to using ONCE_PER_DAY rules, etc.
   */
  if (!lang_str_compare(de1->de_subtitle, de2->de_subtitle) &&
      !lang_str_compare(de1->de_desc, de2->de_desc))
    return DUP;

  /* If all tests have finished then we assume not a dup */
  return NOT_DUP;
}

/**
 *
 */
static dvr_entry_t *_dvr_duplicate_event(dvr_entry_t *de)
{
  static _dvr_duplicate_fcn_t fcns[] = {
    [DVR_AUTOREC_RECORD_UNIQUE]                    = _dvr_duplicate_unique_match,
    [DVR_AUTOREC_RECORD_DIFFERENT_EPISODE_NUMBER]  = _dvr_duplicate_epnum,
    [DVR_AUTOREC_LRECORD_DIFFERENT_EPISODE_NUMBER] = _dvr_duplicate_epnum,
    [DVR_AUTOREC_LRECORD_DIFFERENT_TITLE]          = _dvr_duplicate_title,
    [DVR_AUTOREC_RECORD_DIFFERENT_SUBTITLE]        = _dvr_duplicate_subtitle,
    [DVR_AUTOREC_LRECORD_DIFFERENT_SUBTITLE]       = _dvr_duplicate_subtitle,
    [DVR_AUTOREC_RECORD_DIFFERENT_DESCRIPTION]     = _dvr_duplicate_desc,
    [DVR_AUTOREC_LRECORD_DIFFERENT_DESCRIPTION]    = _dvr_duplicate_desc,
    [DVR_AUTOREC_RECORD_ONCE_PER_MONTH]            = _dvr_duplicate_per_month,
    [DVR_AUTOREC_LRECORD_ONCE_PER_MONTH]           = _dvr_duplicate_per_month,
    [DVR_AUTOREC_RECORD_ONCE_PER_WEEK]             = _dvr_duplicate_per_week,
    [DVR_AUTOREC_LRECORD_ONCE_PER_WEEK]            = _dvr_duplicate_per_week,
    [DVR_AUTOREC_RECORD_ONCE_PER_DAY]              = _dvr_duplicate_per_day,
    [DVR_AUTOREC_LRECORD_ONCE_PER_DAY]             = _dvr_duplicate_per_day,
  };
  dvr_entry_t *de2;
 _dvr_duplicate_fcn_t match;
  int record;
  void *aux;

  if (!de->de_autorec)
    return NULL;

  // title not defined, can't be deduped
  if (lang_str_empty(de->de_title))
    return NULL;

  if (de->de_autorec->dae_record == DVR_AUTOREC_RECORD_DVR_PROFILE)
    record = de->de_config->dvr_autorec_dedup;
  else
    record = de->de_autorec->dae_record;

  switch (record) {
    case DVR_AUTOREC_RECORD_ALL:
      return NULL;
    case DVR_AUTOREC_RECORD_UNIQUE:
      break;
    case DVR_AUTOREC_RECORD_DIFFERENT_EPISODE_NUMBER:
    case DVR_AUTOREC_LRECORD_DIFFERENT_EPISODE_NUMBER:
      if (de->de_epnum.e_num == 0 && de->de_epnum.text == NULL)
        return NULL;
      break;
    case DVR_AUTOREC_RECORD_DIFFERENT_SUBTITLE:
    case DVR_AUTOREC_LRECORD_DIFFERENT_SUBTITLE:
      if (lang_str_empty(de->de_subtitle))
        return NULL;
      break;
    case DVR_AUTOREC_RECORD_DIFFERENT_DESCRIPTION:
    case DVR_AUTOREC_LRECORD_DIFFERENT_DESCRIPTION:
      if (lang_str_empty(de->de_desc))
        return NULL;
      break;
    case DVR_AUTOREC_RECORD_ONCE_PER_DAY:
    case DVR_AUTOREC_LRECORD_ONCE_PER_DAY:
      break;
    case DVR_AUTOREC_RECORD_ONCE_PER_WEEK:
    case DVR_AUTOREC_LRECORD_ONCE_PER_WEEK:
      break;
    case DVR_AUTOREC_RECORD_ONCE_PER_MONTH:
    case DVR_AUTOREC_LRECORD_ONCE_PER_MONTH:
      break;
    case DVR_AUTOREC_LRECORD_DIFFERENT_TITLE:
      break;
   default:
      abort();
  }

  match = fcns[record];
  aux   = NULL;

  assert(match);

  if (record < DVR_AUTOREC_LRECORD_DIFFERENT_EPISODE_NUMBER || record == DVR_AUTOREC_RECORD_UNIQUE) {
    LIST_FOREACH(de2, &dvrentries, de_global_link) {
      if (de == de2)
        continue;

      // check for valid states
      if (de2->de_sched_state == DVR_NOSTATE ||
          de2->de_sched_state == DVR_MISSED_TIME)
        continue;

      // only earlier recordings qualify as master
      if (de2->de_start > de->de_start && de2->de_last_error != SM_CODE_PREVIOUSLY_RECORDED)
        continue;

      // only enabled upcoming recordings
      if (de2->de_sched_state == DVR_SCHEDULED && !de2->de_enabled)
        continue;

      // only successful earlier recordings qualify as master
      if (dvr_entry_is_finished(de2, DVR_FINISHED_FAILED | DVR_FINISHED_REMOVED_FAILED))
        continue;

      // some channels add "New:" to the title of the first showing, so title match with repeats will fail.
      // if we are going on to check CRIDs, ignore any title mismatch
      // otherwise if titles are not defined or do not match, don't dedup
      if (record != DVR_AUTOREC_RECORD_UNIQUE && lang_str_compare(de->de_title, de2->de_title))
        continue;

      if (match(de, de2, &aux)) {
        free(aux);
        return de2;
      }
    }
  } else {
    LIST_FOREACH(de2, &de->de_autorec->dae_spawns, de_autorec_link) {
      if (de == de2)
        continue;

      // check for valid states
      if (de2->de_sched_state == DVR_NOSTATE ||
          de2->de_sched_state == DVR_MISSED_TIME)
        continue;

      // only earlier recordings qualify as master
      if (de2->de_start > de->de_start && de2->de_last_error != SM_CODE_PREVIOUSLY_RECORDED)
        continue;

      // only enabled upcoming recordings
      if (de2->de_sched_state == DVR_SCHEDULED && !de2->de_enabled)
        continue;

      // only successful earlier recordings qualify as master
      if (dvr_entry_is_finished(de2, DVR_FINISHED_FAILED | DVR_FINISHED_REMOVED_FAILED))
        continue;

      // if titles are not defined or do not match, don't dedup
      if (record != DVR_AUTOREC_LRECORD_DIFFERENT_TITLE &&
          lang_str_compare(de->de_title, de2->de_title))
        continue;

      if (match(de, de2, &aux)) {
        free(aux);
        return de2;
      }
    }
  }
  free(aux);
  return NULL;
}

/*** Return non-zero if the broadcast new_bcast is better for recording than the one for old_de. */
static int
dvr_is_better_recording_timeslot(const epg_broadcast_t *new_bcast, const dvr_entry_t *old_de)
{
  /* If programme is recording (or completed) then it is the "best",
   * even if a better schedule is found after recording starts.
   */
  if (old_de && old_de->de_sched_state != DVR_SCHEDULED)
    return 0;

  if (!old_de || !old_de->de_bcast) return 1;            /* Old broadcast should always exist */
  int old_services = 0;
  int new_services = 0;
  int64_t old_chnumber, new_chnumber;
  const idnode_list_mapping_t *ilm;
  const epg_broadcast_t *old_bcast = old_de->de_bcast;
  const channel_t *old_channel = old_bcast->channel;
  const channel_t *new_channel = new_bcast->channel;

  /* Sanity check. */
  if (!new_channel) return 0;
  if (!old_channel) return 1;

  /* Always prefer a recording that has the correct service profile
   * (UHD, FHD, HD, SD).  Someone mentioned (#1846) that some channels can
   * show a recording earlier in the week in SD then later in the week
   * in HD so this would prefer the later HD recording if the user so
   * desired.
   */
  if (old_de->de_config && old_de->de_config->dvr_profile &&
      old_de->de_config->dvr_profile->pro_svfilter != PROFILE_SVF_NONE) {
    const int svf = old_de->de_config->dvr_profile->pro_svfilter;
    int old_has_svf = channel_has_correct_service_filter(old_channel, svf);
    int new_has_svf = channel_has_correct_service_filter(new_channel, svf);

    if (old_has_svf && !new_has_svf)
      return 0;
    if (!old_has_svf && new_has_svf)
      return 1;
    /* Also try "downgrading", where user asks for FHD, which we don't
     * have, but we could give them HD.
     */
    if (svf == PROFILE_SVF_FHD && !old_has_svf) {
      old_has_svf = channel_has_correct_service_filter(old_channel, PROFILE_SVF_HD);
      new_has_svf = channel_has_correct_service_filter(new_channel, PROFILE_SVF_HD);

      if (old_has_svf && !new_has_svf)
        return 0;
      if (!old_has_svf && new_has_svf)
        return 1;
    }

    /* Also try "downgrading", where user asks for UHD, which we don't
     * have, but we could give them FHD, then HD.
     */
    if (svf == PROFILE_SVF_UHD && !old_has_svf) {
      old_has_svf = channel_has_correct_service_filter(old_channel, PROFILE_SVF_FHD);
      new_has_svf = channel_has_correct_service_filter(new_channel, PROFILE_SVF_FHD);

      if (!old_has_svf && new_has_svf)
        return 1;

      old_has_svf = channel_has_correct_service_filter(old_channel, PROFILE_SVF_HD);
      new_has_svf = channel_has_correct_service_filter(new_channel, PROFILE_SVF_HD);

      if (old_has_svf && !new_has_svf)
        return 0;
      if (!old_has_svf && new_has_svf)
        return 1;
    }
  }

  /* Ealier start time is better; prefers non-timeshift channel X to X+1.
   * This gives us time to reschedule to X+1 if the recording on X fails.
   *
   * However, when creating an autorec, it can match a programme that has
   * already started. If so, it's better to prefer the later recording so
   * you get a full recording.
   *
   * So, if it's 09:10 and there is an hour long programme that
   * started at 09:00 but is repeated at 10:00, then let's record at
   * 10:00 instead and get the full hour, instead of at 09:10 and only
   * get 50 minutes.
   */
  if (new_bcast->start != old_bcast->start) {
    if (new_bcast->start > old_bcast->start && gclk() >  old_bcast->start) {
      return 1;
    } else if (new_bcast->start < old_bcast->start && gclk() > new_bcast->start) {
      return 0;
    }

    if (new_bcast->start < old_bcast->start)
      return 1;

    /* Later broadcast is always worse. */
    if (new_bcast->start > old_bcast->start)
      return 0;
  }

  /* If here, we have the same time. */

  /* Count the number of services each has. So, if a channel has multiple
   * services we assume it's a better choice since we have more fallbacks
   * if a tune fails.
   */
  LIST_FOREACH(ilm, &old_channel->ch_services, ilm_in2_link) {
    ++old_services;
  }
  LIST_FOREACH(ilm, &new_channel->ch_services, ilm_in2_link) {
    ++new_services;
  }
  if (new_services > old_services)
    return 1;
  if (old_services > new_services)
    return 0;

  /* Assume lower channel number is a better channel since they
   * typically paid more to be higher up the EPG.
   */
  old_chnumber = channel_get_number(old_channel);
  new_chnumber = channel_get_number(new_channel);
  /* Prefer channels with a number to ones without a number */
  if (!new_chnumber && old_chnumber)
    return 0;
  if (new_chnumber && !old_chnumber)
    return 1;
  if (new_chnumber < old_chnumber)
    return 1;
  if (new_chnumber > old_chnumber)
    return 0;

  /* All things being equal, prefer the existing recording */
  return 0;
}

/**
 *
 */
void
dvr_entry_create_by_autorec(int enabled, epg_broadcast_t *e, dvr_autorec_entry_t *dae)
{
  char buf[512];
  char t1buf[32], t2buf[32];
  const char *s;
  dvr_entry_t *de, *replace = NULL;
  uint32_t count = 0, max_count;
  htsmsg_t *conf;

  /* Identical duplicate detection
     NOTE: Semantic duplicate detection is deferred to the start time of recording and then done using _dvr_duplicate_event by dvr_timer_start_recording. */
  LIST_FOREACH(de, &dvrentries, de_global_link) {
    if (de->de_bcast == e || epg_episode_match(de->de_bcast, e))
      if (strcmp(dae->dae_owner ?: "", de->de_owner ?: "") == 0) {
        /* See if our new broadcast is better than our existing schedule,
         * but only if user want this overhead.
         */
        if (!dae->dae_config || !dae->dae_config->dvr_profile)
          return;

        if (!dae->dae_config->dvr_complex_scheduling)
          return;

        /* Our autorec can never be better than a manually scheduled programme
         * since user might schedule to avoid conflicts.
         */
        if (!de->de_autorec)
          return;

        /* Same broadcast, so new one can't be any better. */
        if (de->de_bcast == e)
          return;

        /* Existing entry wasn't enabled? If so assume user does not
         * want a new autorec scheduled that is enabled.
         */
        if (!de->de_enabled)
          return;

        /* If our new broadcast is "better" than the existing
         * scheduled one, then the existing one can be
         * replaced. Otherwise, we can return here and use the
         * existing one as being the best recording to make.
         */
        if (!dvr_is_better_recording_timeslot(e, de))
          return;

        /* New broadcast is better than existing one that is
         * scheduled. However, we still need to search to end of list
         * since the new broadcast may already be scheduled somewhere
         * else in the dvrentries.
         */
        replace = de;
      }
  }

  /* Have an entry that is worse than our new broadcast so remove it now
   * so that our max schedules check will be correct.
   */
  if (replace) {
    tvhinfo(LS_DVR, "Autorecord \"%s\" Replacing existing dvr recording entry of \"%s\" on %s @ start %s with recording on %s @ start %s",
            dae->dae_name, lang_str_get(e->title, NULL),
            DVR_CH_NAME(replace),
            gmtime2local(replace->de_bcast->start, t1buf, sizeof t1buf),
            e->channel ? channel_get_name(e->channel, channel_blank_name) : channel_blank_name,
            gmtime2local(e->start, t2buf, sizeof t2buf));
    dvr_entry_delete(replace);
    dvr_entry_destroy(replace, 1);
  }

  /* Handle max schedules limit for autorrecord */
  if ((max_count = dvr_autorec_get_max_sched_count(dae)) > 0){
    count = 0;
    LIST_FOREACH(de, &dae->dae_spawns, de_autorec_link)
      if ((de->de_sched_state == DVR_SCHEDULED) ||
          (de->de_sched_state == DVR_RECORDING)) count++;

    /* We drop this to a debug since on a reschedule numerous emitted */
    if (count >= max_count) {
      tvhdebug(LS_DVR, "Autorecord \"%s\": Not scheduling \"%s\" because of autorecord max schedules limit reached",
               dae->dae_name, lang_str_get(e->title, NULL));
      return;
    }
  }

  /* Prefer the recording comment or the name of the rule to an empty string */
  s = dae->dae_comment && *dae->dae_comment ? dae->dae_comment : dae->dae_name;
  snprintf(buf, sizeof(buf), _("Auto recording%s%s"), s ? ": " : "", s ?: "");

  conf = htsmsg_create_map();
  htsmsg_add_uuid(conf, "config_name", &dae->dae_config->dvr_id.in_uuid);
  htsmsg_add_s64(conf, "start_extra", dae->dae_start_extra);
  htsmsg_add_s64(conf, "stop_extra", dae->dae_stop_extra);
  htsmsg_add_u32(conf, "pri", dae->dae_pri);
  htsmsg_add_u32(conf, "retention", dae->dae_retention);
  htsmsg_add_u32(conf, "removal", dae->dae_removal);
  htsmsg_add_str2(conf, "owner", dae->dae_owner);
  htsmsg_add_str2(conf, "creator", dae->dae_creator);
  htsmsg_add_str(conf, "comment", buf);
  htsmsg_add_uuid(conf, "autorec", &dae->dae_id.in_uuid);
  htsmsg_add_str2(conf, "directory", dae->dae_directory);
  htsmsg_add_str2(conf, "cat1", dae->dae_cat1);
  htsmsg_add_str2(conf, "cat2", dae->dae_cat2);
  htsmsg_add_str2(conf, "cat3", dae->dae_cat3);
  dvr_entry_create_from_htsmsg(conf, e);
  htsmsg_destroy(conf);
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

  idnode_save_check(&de->de_id, 1);

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
  if (de->de_subtitle) lang_str_destroy(de->de_subtitle);
  if (de->de_summary) lang_str_destroy(de->de_summary);
  if (de->de_desc) lang_str_destroy(de->de_desc);
  dvr_entry_assign_broadcast(de, NULL);
  free(de->de_channel_name);
  free(de->de_epnum.text);
  free(de->de_image);
  free(de->de_fanart_image);
  free(de->de_uri);

  free(de);
}

/**
 *
 */
void
dvr_entry_destroy(dvr_entry_t *de, int delconf)
{
  char ubuf[UUID_HEX_SIZE];

  idnode_save_check(&de->de_id, delconf);

  if (delconf)
    hts_settings_remove("dvr/log/%s", idnode_uuid_as_str(&de->de_id, ubuf));

  htsp_dvr_entry_delete(de);

#if ENABLE_INOTIFY
  dvr_inotify_del(de);
#endif

  gtimer_disarm(&de->de_timer);
  mtimer_disarm(&de->de_deferred_timer);
  gtimer_disarm(&de->de_watched_timer);
#if ENABLE_DBUS_1
  mtimer_arm_rel(&dvr_dbus_timer, dvr_dbus_timer_cb, NULL, sec2mono(2));
#endif

  if (de->de_channel)
    LIST_REMOVE(de, de_channel_link);
  LIST_REMOVE(de, de_global_link);
  de->de_channel = NULL;

  if (de->de_parent)
    dvr_entry_change_parent_child(de->de_parent, NULL, de, delconf);
  if (de->de_child)
    dvr_entry_change_parent_child(de, NULL, de, delconf);

  /* Trigger a reschedule in case this entry affects an autorec.  For
   * example, deleting a recording could cause an autorec with a "max
   * count" to be able to schedule a new recording.  We have to do
   * this even if de was not an autorec since autorecs can interact
   * with manually scheduled programmes.
   *
   * We avoid rescheduling for cases where the entry has only just been
   * created and then immediately destroyed, giving a few seconds
   * lee-way in case of slow hardware.
   */
  if (!de->de_create || gclk() - de->de_create > 10)
      dvr_autorec_async_reschedule();
  dvr_entry_dec_ref(de);
}

/**
 *
 */
static void _deferred_destroy_cb(void *aux)
{
  dvr_entry_destroy(aux, 1);
}

static void
dvr_entry_deferred_destroy(dvr_entry_t *de)
{
  mtimer_arm_rel(&de->de_deferred_timer, _deferred_destroy_cb, de, 0);
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
      idnode_changed(&de->de_id);
  }
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
  tvhinfo(LS_DVR, "Watched timer expiring \"%s\"",
          lang_str_get(de->de_title, NULL));
  /* Force the watched timer to be reset. This ensures it
   * cannot be re-triggered on a restart of tvheadend.
   */
  de->de_watched = 0;
  dvr_entry_changed(de);
  /* Allow re-record if recording errors */
  dvr_entry_cancel_remove(de, 1);
}

static void
dvr_entry_watched_timer_cb(void *aux)
{
  tvhtrace(LS_DVR, "Entry watched cb");
  dvr_timer_expire(aux);
}

static void
dvr_entry_watched_timer_arm(dvr_entry_t* de)
{
  if (!de || !de->de_watched ||
      dvr_entry_get_removal_days(de) == DVR_RET_REM_FOREVER ||
      !de->de_config || !de->de_config->dvr_removal_after_playback)
    return;

  char t1buf[32];
  const time_t now = gclk();
  char ubuf[UUID_HEX_SIZE];
  time_t when = de->de_watched + de->de_config->dvr_removal_after_playback;

  /* We could just call the cb ourselves, but we'll set a timer if
   * the event is in the past so that we are always async.
   * This avoids any potential problems with caller trying to
   * reference our de after it is destroyed.
   */
  if (when < now)
    when = now;
  tvhinfo(LS_DVR, "Arming watched timer to delete \"%s\" %s @ %s (now+%"PRId64")",
          lang_str_get(de->de_title, NULL),
          idnode_uuid_as_str(&de->de_id, ubuf),
          gmtime2local(when, t1buf, sizeof t1buf),
          (int64_t)when-now);
  gtimer_arm_absn(&de->de_watched_timer, dvr_entry_watched_timer_cb, de, when);
}

static void
dvr_entry_watched_timer_disarm(dvr_entry_t* de)
{
  if (de) {
    dvr_entry_trace(de, "watched timer - disarm");
    de->de_watched = 0;
    gtimer_disarm(&de->de_watched_timer);
  }
}

/// Check if user wants played entries to be automatically deleted
/// If so, arm the timers.
static void
dvr_entry_watched_set_watched(dvr_entry_t* de)
{
  if (!de)
    return;

  /* User wants entry deleted after it is played, so mark earliest
   * "deleted" timestamp and arm.
   */
  de->de_watched = gclk();
  if (de->de_config && de->de_config->dvr_removal_after_playback)
    dvr_entry_watched_timer_arm(de);
}

/**
 *
 */
static void
dvr_timer_disarm(void *aux)
{
  dvr_entry_t *de = aux;
  dvr_entry_trace(de, "retention timer - disarm");
  gtimer_disarm(&de->de_timer);
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
#define DVR_UPDATED_CONFIG       (1<<17)
#define DVR_UPDATED_PLAYPOS      (1<<18)
#define DVR_UPDATED_PLAYCOUNT    (1<<19)
#define DVR_UPDATED_AGE_RATING   (1<<20)
#define DVR_UPDATED_COMMENT      (1<<21)

static char *dvr_updated_str(char *buf, size_t buflen, int flags)
{
  static const char *x = "ecoOsStumdpgrviBECPaAM";
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

int
dvr_entry_set_playcount(dvr_entry_t *de, uint32_t playcount)
{
  if (de->de_playcount == playcount)
    return 0;
  de->de_playcount = playcount;
  if (de->de_playcount)
    dvr_entry_watched_set_watched(de);
  else                          /* Not watched, so disarm timer */
    dvr_entry_watched_timer_disarm(de);
  return 1;
}

int
dvr_entry_incr_playcount(dvr_entry_t *de)
{
  return dvr_entry_set_playcount(de, de->de_playcount + 1);
}

/**
 *
 */
static dvr_entry_t *_dvr_entry_update
  ( dvr_entry_t *de, int enabled, const char *dvr_config_uuid,
    epg_broadcast_t *e, channel_t *ch,
    const char *title, const char *subtitle,
    const char *summary, const char *desc,
    const char *lang, time_t start, time_t stop,
    time_t start_extra, time_t stop_extra,
    dvr_prio_t pri, int retention, int removal,
    int playcount, int playposition, int age_rating,
    ratinglabel_t *rating_label, const char *comment)
{
  char buf[40];
  int save = 0, updated = 0;
  epg_episode_num_t epnum;

  if (enabled >= 0) {
    enabled = !!enabled;
    if (de->de_enabled != enabled) {
      de->de_enabled = enabled;
      save |= DVR_UPDATED_ENABLED;
    }
  }

  if (!dvr_entry_is_editable(de)) {
    if (stop > 0) {
      if (stop < gclk())
        stop = gclk();
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
    if (save & (DVR_UPDATED_STOP|DVR_UPDATED_STOP_EXTRA|DVR_UPDATED_ENABLED)) {
      updated = 1;
      dvr_entry_set_timer(de);
    }
    if (e && e->dvb_eid != de->de_dvb_eid) {
      de->de_dvb_eid = e->dvb_eid;
      save |= DVR_UPDATED_EID;
    }
    if (de->de_sched_state == DVR_RECORDING || de->de_sched_state == DVR_COMPLETED) {
      if (playcount >= 0 && playcount != de->de_playcount) {
        dvr_entry_set_playcount(de, playcount);
        save |= DVR_UPDATED_PLAYCOUNT;
      }
      if (playposition >= 0 && playposition != de->de_playposition) {
        de->de_playposition = playposition;
        save |= DVR_UPDATED_PLAYPOS;
      }
      if (retention && (retention != de->de_retention)) {
        de->de_retention = retention;
        save |= DVR_UPDATED_RETENTION;
      }
      if (removal && (removal != de->de_removal)) {
        de->de_removal = removal;
        save |= DVR_UPDATED_REMOVAL;
      }
      if (title) {
        save |= lang_str_set(&de->de_title, title, lang) ? DVR_UPDATED_TITLE : 0;
      }
      if (subtitle) {
        save |= lang_str_set(&de->de_subtitle, subtitle, lang) ? DVR_UPDATED_SUBTITLE : 0;
      }
      if (summary) {
        save |= lang_str_set(&de->de_summary, summary, lang) ? DVR_UPDATED_SUMMARY : 0;
      }
    }
    goto dosave;
  }

  /* Configuration */
  if (dvr_config_uuid) {
    de->de_config = dvr_config_find_by_name_default(dvr_config_uuid);
    save |= DVR_UPDATED_CONFIG;
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
  /* Manual Age Rating */
  if (age_rating != de->de_age_rating) {
    de->de_age_rating = age_rating;
    save |= DVR_UPDATED_AGE_RATING;
  }

  /* Comment */
  if (comment && strcmp(de->de_comment ?: "", comment)) {
    free(de->de_comment);
    de->de_comment = strdup(comment);
    save |= DVR_UPDATED_COMMENT;
  }

  /* Title */
  if (e && e->title) {
    save |= lang_str_set2(&de->de_title, e->title) ? DVR_UPDATED_TITLE : 0;
  } else if (title) {
    save |= lang_str_set(&de->de_title, title, lang) ? DVR_UPDATED_TITLE : 0;
  }

  /* Subtitle */
  if (e && e->subtitle) {
    save |= lang_str_set2(&de->de_subtitle, e->subtitle) ? DVR_UPDATED_SUBTITLE : 0;
  } else if (subtitle) {
    save |= lang_str_set(&de->de_subtitle, subtitle, lang) ? DVR_UPDATED_SUBTITLE : 0;
  }

  /* Summary */
  if (e && e->summary) {
    save |= lang_str_set2(&de->de_summary, e->summary) ? DVR_UPDATED_SUMMARY : 0;
  } else if (summary) {
    save |= lang_str_set(&de->de_summary, summary, lang) ? DVR_UPDATED_SUMMARY : 0;
  }

  /* EID */
  if (e && e->dvb_eid != de->de_dvb_eid) {
    de->de_dvb_eid = e->dvb_eid;
    save |= DVR_UPDATED_EID;
  }

  /* Age Rating from EPG*/
  if (e && e->age_rating != de->de_age_rating) {
    de->de_age_rating = e->age_rating;
    save |= DVR_UPDATED_AGE_RATING;
  }

  /* Description */
  if (e && e->description) {
    save |= lang_str_set2(&de->de_desc, e->description) ? DVR_UPDATED_DESCRIPTION : 0;
  } else if (e && e->summary) {
    save |= lang_str_set2(&de->de_desc, e->summary) ? DVR_UPDATED_DESCRIPTION : 0;
  } else if (desc) {
    save |= lang_str_set(&de->de_desc, desc, lang) ? DVR_UPDATED_DESCRIPTION : 0;
  }

  /* Genre */
  if (e) {
    epg_genre_t *g = LIST_FIRST(&e->genre);
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
  if (de->de_bcast) {
    epg_broadcast_get_epnum(de->de_bcast, &epnum);
  } else {
    memset(&epnum, 0, sizeof(epnum));
  }
  if (epg_episode_number_cmpfull(&de->de_epnum, &epnum)) {
    de->de_epnum = epnum;
    if (epnum.text)
      de->de_epnum.text = strdup(epnum.text);
    save |= DVR_UPDATED_EPISODE;
  }

  /* Save changes */
dosave:
  if (save) {
    dvr_entry_changed(de);
    if (tvhlog_limit(&de->de_update_limit, 60)) {
      tvhinfo(LS_DVR, "\"%s\" on \"%s\": Updated%s (%s)",
              lang_str_get(de->de_title, NULL), DVR_CH_NAME(de),
              updated ? " Timer" : "",
              dvr_updated_str(buf, sizeof(buf), save));
    } else {
      tvhtrace(LS_DVR, "\"%s\" on \"%s\": Updated%s (%s)",
               lang_str_get(de->de_title, NULL), DVR_CH_NAME(de),
               updated ? " Timer" : "",
               dvr_updated_str(buf, sizeof(buf), save));
    }
    /* The updates could mean the autorec rule no longer matches the event,
     * so we have to destroy the de and the conf and let the autorec rule
     * re-create when it next matches.
     *
     * If de is not scheduled (i.e., recording) then do nothing with the
     * rule since don't want to cancel items that are recording.
     * Also if no event then this autorec can't match it so we should delete
     * it.
     */
    if (dvr_autorec_entry_can_be_purged(de)) {
        dvr_entry_assign_broadcast(de, NULL);
        dvr_entry_destroy(de, 1);
        de = NULL;
    }
  }

  return de;
}

/**
 *
 */
dvr_entry_t *
dvr_entry_update
  ( dvr_entry_t *de, int enabled,
    const char *dvr_config_uuid, channel_t *ch,
    const char *title, const char *subtitle,
    const char *summary, const char *desc, const char *lang,
    time_t start, time_t stop,
    time_t start_extra, time_t stop_extra,
    dvr_prio_t pri, int retention, int removal, int playcount, int playposition,
    int age_rating, ratinglabel_t *rating_label, const char *comment)
{
  return _dvr_entry_update(de, enabled, dvr_config_uuid,
                           NULL, ch, title, subtitle, summary, desc, lang,
                           start, stop, start_extra, stop_extra,
                           pri, retention, removal, playcount, playposition,
                           age_rating, rating_label, comment);
}

/**
 * Used to notify the DVR that an event has been replaced in the EPG
 */
void
dvr_event_replaced(epg_broadcast_t *e, epg_broadcast_t *new_e)
{
  dvr_entry_t *de, *de_next;
  channel_t *ch = e->channel;
  epg_broadcast_t *e2;
  char t1buf[32], t2buf[32];

  assert(e != NULL);
  assert(new_e != NULL);

  /* Ignore */
  if (ch == NULL || e == new_e) return;

  /* Existing entry */
  for (de = LIST_FIRST(&ch->ch_dvrs); de; de = de_next) {
    de_next = LIST_NEXT(de, de_channel_link);

    if (de->de_bcast != e)
      continue;

    dvr_entry_trace_time2(de, "start", e->start, "stop", e->stop,
                          "event replaced %s on %s",
                          epg_broadcast_get_title(e, NULL),
                          channel_get_name(ch, channel_blank_name));

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
          tvhtrace(LS_DVR, "  replacement event %s on %s @ start %s stop %s",
                          epg_broadcast_get_title(e2, NULL),
                          channel_get_name(ch, channel_blank_name),
                          gmtime2local(e2->start, t1buf, sizeof(t1buf)),
                          gmtime2local(e2->stop, t2buf, sizeof(t2buf)));
          _dvr_entry_update(de, -1, NULL, e2, NULL, NULL, NULL, NULL, NULL,
                            NULL, 0, 0, 0, 0, DVR_PRIO_NOTSET, 0, 0, -1, -1, 0, NULL, NULL);
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
    idnode_changed(&de->de_id);
  }
}

/**
 * Event was updated in epg
 */
void dvr_event_updated(epg_broadcast_t *e)
{
  dvr_entry_t *de;

  if (e->channel == NULL)
    return;
  LIST_FOREACH(de, &e->dvr_entries, de_bcast_link) {
    assert(de->de_bcast == e);
    if (de->de_sched_state != DVR_SCHEDULED) continue;
    _dvr_entry_update(de, -1, NULL, e, NULL, NULL, NULL, NULL, NULL,
                      NULL, 0, 0, 0, 0, DVR_PRIO_NOTSET, 0, 0, -1, -1, 0, NULL, NULL);
  }
  LIST_FOREACH(de, &e->channel->ch_dvrs, de_channel_link) {
    if (de->de_sched_state != DVR_SCHEDULED) continue;
    if (de->de_bcast) continue;
    if (dvr_entry_fuzzy_match(de, e, e->dvb_eid,
                              de->de_config->dvr_update_window)) {
      dvr_entry_trace_time2(de, "start", e->start, "stop", e->stop,
                            "link to event %s on %s",
                            epg_broadcast_get_title(e, NULL),
                            channel_get_name(e->channel, channel_blank_name));
      _dvr_entry_update(de, -1, NULL, e, NULL, NULL, NULL, NULL, NULL,
                        NULL, 0, 0, 0, 0, DVR_PRIO_NOTSET, 0, 0, -1, -1, 0, NULL, NULL);
    }
  }
}

/**
 * Event running status is updated
 */
void dvr_event_running(epg_broadcast_t *e, epg_running_t running)
{
  dvr_entry_t *de;
  epg_broadcast_t *e2;
  const char *srcname;
  char ubuf[UUID_HEX_SIZE];

  if (e->dvb_eid == 0 || e->channel == NULL)
    return;
  tvhtrace(LS_DVR, "dvr event running check for %s on %s running %d",
           epg_broadcast_get_title(e, NULL),
           channel_get_name(e->channel, channel_blank_name),
           running);
  LIST_FOREACH(de, &e->channel->ch_dvrs, de_channel_link) {
    if (running == EPG_RUNNING_NOW && de->de_dvb_eid == e->dvb_eid) {
      if (de->de_running_pause) {
        tvhdebug(LS_DVR, "dvr entry %s event %s on %s - EPG unpause",
                 idnode_uuid_as_str(&de->de_id, ubuf),
                 epg_broadcast_get_title(e, NULL),
                 channel_get_name(e->channel, channel_blank_name));
        atomic_set_time_t(&de->de_running_pause, 0);
        atomic_add(&de->de_running_change, 1);
      }
      if (!de->de_running_start) {
        tvhdebug(LS_DVR, "dvr entry %s event %s on %s - EPG marking start",
                 idnode_uuid_as_str(&de->de_id, ubuf),
                 epg_broadcast_get_title(e, NULL),
                 channel_get_name(e->channel, channel_blank_name));
        atomic_set_time_t(&de->de_running_start, gclk());
        atomic_add(&de->de_running_change, 1);
      }
      if (dvr_entry_get_start_time(de, 1) > gclk()) {
        tvhdebug(LS_DVR, "dvr entry %s event %s on %s - EPG start",
                 idnode_uuid_as_str(&de->de_id, ubuf),
                 epg_broadcast_get_title(e, NULL),
                 channel_get_name(e->channel, channel_blank_name));
        atomic_set_time_t(&de->de_start, gclk());
        atomic_add(&de->de_running_change, 1);
        dvr_entry_set_timer(de);
      }
    } else if ((running == EPG_RUNNING_STOP && de->de_dvb_eid == e->dvb_eid) ||
                running == EPG_RUNNING_NOW) {
      /* Don't stop recording if we are a segmented programme since
       * (by definition) the first segment will be marked as stop long
       * before the second segment is marked as started. Otherwise if we
       * processed the stop then we will stop the dvr_thread from
       * recording tv packets. But if the event is behind the last segment,
       * stop it.
       */
      if (de->de_segment_stop_extra) {
        e2 = dvr_entry_get_segment_last(de);
        for ( ; e2; e2 = epg_broadcast_get_prev(e2))
          if (e == e2)
            break;
        if (e2)
          continue;
      }
      /*
       * make checking more robust
       * sometimes, the running bits are parsed randomly for a few moments
       * so don't expect that the broacasting has only 5 seconds
       */
      if (de->de_running_start + 5 > gclk())
        continue;

      srcname = de->de_dvb_eid == e->dvb_eid ? "event" : "other running event";
      if (!de->de_running_stop ||
          de->de_running_start > de->de_running_stop) {
        atomic_add(&de->de_running_change, 1);
        tvhdebug(LS_DVR, "dvr entry %s %s %s on %s - EPG marking stop",
                 idnode_uuid_as_str(&de->de_id, ubuf), srcname,
                 epg_broadcast_get_title(e, NULL),
                 channel_get_name(de->de_channel, channel_blank_name));
      }
      atomic_set_time_t(&de->de_running_stop, gclk());
      atomic_set_time_t(&de->de_running_pause, 0);
      if (de->de_sched_state == DVR_RECORDING && de->de_running_start) {
        if (dvr_entry_get_epg_running(de)) {
          tvhdebug(LS_DVR, "dvr entry %s %s %s on %s - EPG stop",
                   idnode_uuid_as_str(&de->de_id, ubuf), srcname,
                   epg_broadcast_get_title(e, NULL),
                   channel_get_name(de->de_channel, channel_blank_name));
          dvr_stop_recording(de, SM_CODE_OK, 0, 0);
        }
      }
    } else if (running == EPG_RUNNING_PAUSE && de->de_dvb_eid == e->dvb_eid) {
      if (!de->de_running_pause) {
        tvhdebug(LS_DVR, "dvr entry %s event %s on %s - EPG pause",
                 idnode_uuid_as_str(&de->de_id, ubuf),
                 epg_broadcast_get_title(e, NULL),
                 channel_get_name(e->channel, channel_blank_name));
        atomic_set_time_t(&de->de_running_pause, gclk());
        atomic_add(&de->de_running_change, 1);
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

  de->de_dont_reschedule = 1;

  if (stopcode != SM_CODE_INVALID_TARGET &&
      (rec_state == DVR_RS_PENDING ||
       rec_state == DVR_RS_WAIT_PROGRAM_START ||
       htsmsg_is_empty(de->de_files)))
    dvr_entry_missed_time(de, stopcode);
  else
    dvr_entry_completed(de, stopcode);

  tvhinfo(LS_DVR, "\"%s\" on \"%s\": "
	  "End of program: %s",
	  lang_str_get(de->de_title, NULL), DVR_CH_NAME(de),
	  dvr_entry_status(de));

  if (dvr_entry_rerecord(de))
    return;

  if (saveconf)
    idnode_changed(&de->de_id);

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
  if(de->de_sched_state != DVR_RECORDING)
    return;
  dvr_entry_trace_time2(de,
                        "rstart", de->de_running_start,
                        "rstop", de->de_running_stop,
                        "stop recording timer called");
  /* EPG thinks that the program is running */
  if (de->de_segment_stop_extra) {
    const time_t stop = dvr_entry_get_stop_time(de);
    dvr_entry_trace_time2(de, "running start", de->de_running_start, "running stop", de->de_running_stop, "forcing stop");
    dvr_entry_trace_time1(de, "stop", stop, "forcing stop - segment extra %"PRId64, (int64_t)de->de_segment_stop_extra);
    /* no-op. We fall through to the final dvr_stop_recording below.
    * This path is here purely to get a log.
    */
  } else if (de->de_running_start > de->de_running_stop) {
    gtimer_arm_rel(&de->de_timer, dvr_timer_stop_recording, de, 10);
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
  time_t stop;
  int r;

  if (!de->de_enabled) {
    dvr_entry_missed_time(de, SM_CODE_SVC_NOT_ENABLED);
    return;
  }

  dvr_entry_set_state(de, DVR_RECORDING, DVR_RS_PENDING, SM_CODE_OK);

  tvhinfo(LS_DVR, "\"%s\" on \"%s\" recorder starting",
	  lang_str_get(de->de_title, NULL), DVR_CH_NAME(de));

  /*
   * The running flag is updated only on the event change. When the DVR
   * entry is added too late, the running timers might not be updated.
   */
  if (de->de_bcast && de->de_running_start == 0 && de->de_running_stop == 0) {
    epg_broadcast_t *ebc = epg_broadcast_get_prev(de->de_bcast);
    if (de->de_bcast->running != EPG_RUNNING_NOTSET ||
        (ebc && ebc->running != EPG_RUNNING_NOTSET))
      de->de_running_stop = gclk();
  }

  if (!clone && (r = dvr_rec_subscribe(de)) < 0) {
    dvr_entry_completed(de, r == -EPERM ? SM_CODE_USER_ACCESS :
                           (r == -EOVERFLOW ? SM_CODE_USER_LIMIT :
                            SM_CODE_BAD_SOURCE));
    return;
  }

  /* Reset cached value for segment stopping so it can be recalculated
   * immediately at start of recording. This handles case where a
   * programme is split in to three-or-more segments and we want to
   * ensure we correctly pick up all segments at the start of a
   * recording. Otherwise if a broadcaster has an EIT window of say
   * 24h and we get the first two segments when we first calculate the
   * stop time then we would not recalculate the stop time when
   * further segments enter the EIT window.
   */
  de->de_segment_stop_extra = 0;
  stop = dvr_entry_get_stop_time(de);

  dvr_entry_trace_time2(de, "original stop", de->de_stop, "stop", stop, "stop timer set");

  gtimer_arm_absn(&de->de_timer, dvr_timer_stop_recording, de, stop);
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
 * Find the earliest scheduled dvr entry
 */
time_t
dvr_entry_find_earliest(void)
{
  time_t start;
  time_t earliest = 0;
  dvr_entry_t *de;

  LIST_FOREACH(de, &dvrentries, de_global_link)
  {
    if(dvr_entry_is_upcoming(de) && de->de_enabled)
    {
      start = dvr_entry_get_start_time(de, 1);
      if(earliest == 0)
      {
        earliest = start;
      }
      else if(start < earliest)
      {
        earliest = start;
      }
    }
  }//END FOREACH

  return earliest;

}

/**
 * Unconditionally remove an entry
 */
static void
dvr_entry_purge(dvr_entry_t *de, int delconf)
{
  int dont_reschedule;

  if(de->de_sched_state == DVR_RECORDING) {
    dont_reschedule = de->de_dont_reschedule;
    dvr_stop_recording(de, SM_CODE_SOURCE_DELETED, delconf, 0);
    if (!delconf)
      de->de_dont_reschedule = dont_reschedule;
  }
}


/* **************************************************************************
 * DVR Entry Class definition
 * **************************************************************************/

static void
dvr_entry_class_changed(idnode_t *self)
{
  dvr_entry_t *de = (dvr_entry_t *)self;
  if (de->de_in_unsubscribe)
    return;
  if (dvr_entry_is_valid(de))
    dvr_entry_set_timer(de);
  htsp_dvr_entry_update(de);
}

static htsmsg_t *
dvr_entry_class_save(idnode_t *self, char *filename, size_t fsize)
{
  dvr_entry_t *de = (dvr_entry_t *)self;
  htsmsg_t *m = htsmsg_create_map(), *e, *l, *c, *info;
  htsmsg_field_t *f;
  char ubuf[UUID_HEX_SIZE];
  const char *filename2;
  int64_t s64;

  idnode_save(&de->de_id, m);
  if ((e = epg_episode_epnum_serialize(&de->de_epnum)) != NULL)
    htsmsg_add_msg(m, "episode", e);
  if (de->de_files) {
    l = htsmsg_create_list();
    HTSMSG_FOREACH(f, de->de_files)
      if ((e = htsmsg_field_get_map(f)) != NULL) {
        filename2 = htsmsg_get_str(e, "filename");
        info = htsmsg_get_list(e, "info");
        if (filename2) {
          c = htsmsg_create_map();
          htsmsg_add_str(c, "filename", filename2);
          if (info)
            htsmsg_add_msg(c, "info", htsmsg_copy(info));
          if (!htsmsg_get_s64(e, "start", &s64))
            htsmsg_add_s64(c, "start", s64);
          if (!htsmsg_get_s64(e, "stop", &s64))
            htsmsg_add_s64(c, "stop", s64);
          if (!htsmsg_get_s64(e, "size", &s64))
            htsmsg_add_s64(c, "size", s64);
          htsmsg_add_msg(l, NULL, c);
        }
      }
    htsmsg_add_msg(m, "files", l);
  }
  if (filename)
    snprintf(filename, fsize, "dvr/log/%s", idnode_uuid_as_str(&de->de_id, ubuf));
  return m;
}

static void
dvr_entry_class_delete(idnode_t *self)
{
  dvr_entry_t *de = (dvr_entry_t *)self;
  dvr_entry_cancel_delete(de, 0);
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

static void
dvr_entry_class_get_title
  (idnode_t *self, const char *lang, char *dst, size_t dstsize)
{
  dvr_entry_t *de = (dvr_entry_t *)self;
  const char *s;
  s = lang_str_get(de->de_title, lang);
  if (tvh_str_default(s, NULL) == NULL) {
    s = lang ? lang_str_get(de->de_title, NULL) : NULL;
    if (tvh_str_default(s, NULL) == NULL) {
      s = lang ? lang_str_get(de->de_desc, lang) : NULL;
      if (tvh_str_default(s, NULL) == NULL)
        s = lang_str_get(de->de_desc, NULL);
    }
  }
  snprintf(dst, dstsize, "%s", s);
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
dvr_entry_class_create_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  return dvr_entry_class_time_set(de, &de->de_create, *(time_t *)v);
}

static int
dvr_entry_class_start_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  return dvr_entry_class_time_set(de, &de->de_start, *(time_t *)v);
}

static uint32_t
dvr_entry_class_start_opts(void *o, uint32_t opts)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de && !dvr_entry_is_editable(de))
    return PO_RDONLY;
  return 0;
}

static uint32_t
dvr_entry_class_config_name_opts(void *o, uint32_t opts)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de && !dvr_entry_is_editable(de))
    return PO_RDONLY | PO_ADVANCED;
  return PO_ADVANCED;
}

static uint32_t
dvr_entry_class_owner_opts(void *o, uint32_t opts)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de && de->de_id.in_access &&
      !access_verify2(de->de_id.in_access, ACCESS_ADMIN))
    return PO_ADVANCED;
  return PO_RDONLY | PO_ADVANCED;
}

static uint32_t
dvr_entry_class_start_extra_opts(void *o, uint32_t opts)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de && !dvr_entry_is_editable(de))
    return PO_RDONLY | PO_DURATION | PO_ADVANCED;
  return PO_DURATION | PO_ADVANCED | PO_DOC_NLIST;
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
    if (v < gclk())
      v = gclk();
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
  dvr_entry_t *de = (dvr_entry_t *)o;
  prop_ptr = dvr_get_filename(de) ?: "";
  return &prop_ptr;
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
    if (de->de_id.in_access &&
        !channel_access(ch, de->de_id.in_access, 1))
      return 0;
    if (de->de_channel)
      LIST_REMOVE(de, de_channel_link);
    free(de->de_channel_name);
    de->de_channel_name = strdup(channel_get_name(ch, ""));
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
    return strdup(channel_get_name(de->de_channel, tvh_gettext_lang(lang, channel_blank_name)));
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
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_channel)
    prop_ptr = channel_get_name(de->de_channel, channel_blank_name);
  else
    prop_ptr = de->de_channel_name;
  return &prop_ptr;
}

static int
dvr_entry_class_rating_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  ratinglabel_t   *rl = NULL;

  //If RL processing is not enabled, return a null and exit.
  if(!epggrab_conf.epgdb_processparentallabels){
    de->de_rating_label = NULL;
    return 0;
  }

  if (!dvr_entry_is_editable(de))
    return 0;

  //If the entry is in the past, don't link to the RL object.
  if (de->de_stop < gclk()){
    de->de_rating_label = NULL;
    return 0;
  }

  rl = v ? ratinglabel_find_from_uuid(v) : NULL;

  //If the rating label is found.
  if(rl){
    //Set the rating label pointer in the DVR entry object
    de->de_rating_label = rl;

    //Save the label and icon values
    if(de->de_rating_label_saved){
        free(de->de_rating_label_saved);
    }

    if(rl->rl_display_label){
        de->de_rating_label_saved = strdup(rl->rl_display_label);
    }

    if(de->de_rating_icon_saved){
        free(de->de_rating_icon_saved);
    }

    if(rl->rl_icon){
        de->de_rating_icon_saved = strdup(rl->rl_icon);
    }

    if(de->de_rating_authority_saved){
        free(de->de_rating_authority_saved);
    }

    if(rl->rl_authority){
        de->de_rating_authority_saved = strdup(rl->rl_authority);
    }

    if(de->de_rating_country_saved){
        free(de->de_rating_country_saved);
    }

    if(rl->rl_country){
        de->de_rating_country_saved = strdup(rl->rl_country);
    }

    return 1;
  }//END we got an RL object.

  return 0;
}

//Return the UUID string for this rating label of this entry.
//If RL is not enabled, this function must return an empty string,
//returning NULL will cause a crash.
static const void *
dvr_entry_class_rating_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_rating_label)
    idnode_uuid_as_str(&de->de_rating_label->rl_id, prop_sbuf);
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
}

static int
dvr_entry_class_pri_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  int pri = *(int *)v;
  if (pri == DVR_PRIO_NOTSET || pri < 0 || pri > DVR_PRIO_DEFAULT)
    pri = DVR_PRIO_DEFAULT;
  return dvr_entry_class_int_set(de, &de->de_pri, pri);
}

htsmsg_t *
dvr_entry_class_pri_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Default"),        DVR_PRIO_DEFAULT },
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
    { N_("DVR configuration"),  DVR_RET_REM_DVRCONFIG },
    { N_("1 day"),              DVR_RET_REM_1DAY },
    { N_("3 days"),             DVR_RET_REM_3DAY },
    { N_("5 days"),             DVR_RET_REM_5DAY },
    { N_("1 week"),             DVR_RET_REM_1WEEK },
    { N_("2 weeks"),            DVR_RET_REM_2WEEK },
    { N_("3 weeks"),            DVR_RET_REM_3WEEK },
    { N_("1 month"),            DVR_RET_REM_1MONTH },
    { N_("2 months"),           DVR_RET_REM_2MONTH },
    { N_("3 months"),           DVR_RET_REM_3MONTH },
    { N_("6 months"),           DVR_RET_REM_6MONTH },
    { N_("1 year"),             DVR_RET_REM_1YEAR },
    { N_("2 years"),            DVR_RET_REM_2YEARS },
    { N_("3 years"),            DVR_RET_REM_3YEARS },
    { N_("On file removal"),    DVR_RET_ONREMOVE },
    { N_("Forever"),            DVR_RET_REM_FOREVER },
  };
  return strtab2htsmsg_u32(tab, 1, lang);
}

htsmsg_t *
dvr_entry_class_removal_list ( void *o, const char *lang )
{
  static const struct strtab_u32 tab[] = {
    { N_("DVR configuration"),  DVR_RET_REM_DVRCONFIG },
    { N_("1 day"),              DVR_RET_REM_1DAY },
    { N_("3 days"),             DVR_RET_REM_3DAY },
    { N_("5 days"),             DVR_RET_REM_5DAY },
    { N_("1 week"),             DVR_RET_REM_1WEEK },
    { N_("2 weeks"),            DVR_RET_REM_2WEEK },
    { N_("3 weeks"),            DVR_RET_REM_3WEEK },
    { N_("1 month"),            DVR_RET_REM_1MONTH },
    { N_("2 months"),           DVR_RET_REM_2MONTH },
    { N_("3 months"),           DVR_RET_REM_3MONTH },
    { N_("6 months"),           DVR_RET_REM_6MONTH },
    { N_("1 year"),             DVR_RET_REM_1YEAR },
    { N_("2 years"),            DVR_RET_REM_2YEARS },
    { N_("3 years"),            DVR_RET_REM_3YEARS },
    { N_("Maintained space"),   DVR_REM_SPACE },
    { N_("Forever"),            DVR_RET_REM_FOREVER },
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
  dvr_entry_t *de = (dvr_entry_t *)o;
  dvr_autorec_entry_t *dae = de->de_autorec;
  if (dae) {
    const int comment = !strempty(dae->dae_comment);
    snprintf(prop_sbuf, PROP_SBUF_LEN, "%s%s%s%s",
             dae->dae_name ?: "",
             comment ? " (" : "",
             comment ? dae->dae_comment : "",
             comment ? ")" : "");
  } else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
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
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_timerec)
    idnode_uuid_as_str(&de->de_timerec->dte_id, prop_sbuf);
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
}

static const void *
dvr_entry_class_timerec_caption_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  dvr_timerec_entry_t *dte = de->de_timerec;
  if (dte) {
    snprintf(prop_sbuf, PROP_SBUF_LEN, "%s%s%s%s",
             dte->dte_name ?: "",
             dte->dte_comment ? " (" : "",
             dte->dte_comment,
             dte->dte_comment ? ")" : "");
  } else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
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
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_parent)
    idnode_uuid_as_str(&de->de_parent->de_id, prop_sbuf);
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
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
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_child)
    idnode_uuid_as_str(&de->de_child->de_id, prop_sbuf);
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
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
  v = tvh_str_default(v, "UnknownTitle");
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
  if (de->de_title)
    prop_ptr = lang_str_get(de->de_title, idnode_lang(o));
  else
    prop_ptr = NULL;
  if (prop_ptr == NULL)
    prop_ptr = "";
  return &prop_ptr;
}

static int
dvr_entry_class_disp_subtitle_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  const char *lang = idnode_lang(o);
  const char *s = "";
  v = tvh_str_default(v, "UnknownSubtitle");
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
  if (de->de_subtitle)
    prop_ptr = lang_str_get(de->de_subtitle, idnode_lang(o));
  else
    prop_ptr = NULL;
  if (prop_ptr == NULL)
    prop_ptr = "";
  return &prop_ptr;
}

static int
dvr_entry_class_disp_summary_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  const char *lang = idnode_lang(o);
  const char *s = "";
  v = tvh_str_default(v, "UnknownSummary");
  if (de->de_summary)
    s = lang_str_get(de->de_summary, lang);
  if (strcmp(s, v)) {
    lang_str_set(&de->de_summary, v, lang);
    return 1;
  }
  return 0;
}

static const void *
dvr_entry_class_disp_summary_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_summary)
    prop_ptr = lang_str_get(de->de_summary, idnode_lang(o));
  else
    prop_ptr = NULL;
  if (prop_ptr == NULL)
    prop_ptr = "";
  return &prop_ptr;
}

static const void *
dvr_entry_class_disp_description_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  if (de->de_desc)
    prop_ptr = lang_str_get(de->de_desc, idnode_lang(o));
  else
    prop_ptr = NULL;
  if (prop_ptr == NULL)
    prop_ptr = "";
  return &prop_ptr;
}

static const void *
dvr_entry_class_disp_extratext_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  prop_ptr = NULL;
  if (de->de_subtitle)
    prop_ptr = lang_str_get(de->de_subtitle, idnode_lang(o));
  if (prop_ptr == NULL)
    prop_ptr = lang_str_get(de->de_summary, idnode_lang(o));
  if (prop_ptr == NULL)
    prop_ptr = lang_str_get(de->de_desc, idnode_lang(o));
  if (prop_ptr == NULL)
    prop_ptr = "";
  return &prop_ptr;
}

static int
dvr_entry_class_disp_extratext_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  const char *lang = idnode_lang(o);
  v = tvh_str_default(v, "?????");

  if (lang_str_get(de->de_subtitle, lang))
    return lang_str_set(&de->de_subtitle, v, lang);
  if (lang_str_get(de->de_summary, lang))
    return lang_str_set(&de->de_summary, v, lang);
  if (lang_str_get(de->de_desc, lang))
    return lang_str_set(&de->de_desc, v, lang);
  // If subtitle, summary or descripcion is not set, the extratext
  // field is stored in subtitle by default
  return lang_str_set(&de->de_subtitle, v, lang);
}

static int
dvr_entry_class_disp_episode_set(void *o, const void *v)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  epg_episode_num_t epnum;
  if (!extract_season_episode(&epnum, (const char *)v))
    epnum.text = (char *)v;
  if (epg_episode_number_cmpfull(&de->de_epnum, &epnum)) {
    free(de->de_epnum.text);
    de->de_epnum = epnum;
    if (epnum.text)
      de->de_epnum.text = strdup(epnum.text);
    return 1;
  }
  return 0;
}

static const void *
dvr_entry_class_disp_episode_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  const char *lang;
  char buf1[32], buf2[32];
  if (de->de_epnum.e_num || de->de_epnum.s_num) {
    lang = idnode_lang(o);
    snprintf(buf1, sizeof(buf1), "%s %%d", tvh_gettext_lang(lang, N_("Season")));
    snprintf(buf2, sizeof(buf2), "%s %%d", tvh_gettext_lang(lang, N_("Episode")));
    epg_episode_num_format(&de->de_epnum, prop_sbuf, PROP_SBUF_LEN, NULL,
                           buf1, ".", buf2, "/%d");
    return &prop_sbuf_ptr;
  } else if (de->de_epnum.text) {
    prop_ptr = de->de_epnum.text;
  } else {
    prop_ptr = "";
  }
  return &prop_ptr;
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
    size = dvr_get_filesize(de, DVR_FILESIZE_UPDATE);
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
  strlcpy(prop_sbuf, dvr_entry_status(de), PROP_SBUF_LEN);
  return &prop_sbuf_ptr;
}

static const void *
dvr_entry_class_sched_status_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  strlcpy(prop_sbuf, dvr_entry_schedstatus(de), PROP_SBUF_LEN);
  return &prop_sbuf_ptr;
}

static const void *
dvr_entry_class_channel_icon_url_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  channel_t *ch = de->de_channel;
  if (ch == NULL) {
    prop_ptr = "";
  } else {
    return channel_class_get_icon(ch);
  }
  return &prop_ptr;
}

const char *
dvr_entry_get_image(const dvr_entry_t *de)
{
  if (de && de->de_bcast && de->de_bcast->image)
    return de->de_bcast->image;
  if (de && de->de_image)
    return de->de_image;
  return NULL;
}

const char *
dvr_entry_get_fanart_image(const dvr_entry_t *de)
{
  return de->de_fanart_image;
}

static const void *
dvr_entry_class_image_get0(dvr_entry_t *de, const char *image)
{
  prop_ptr = imagecache_get_propstr(image, prop_sbuf, PROP_SBUF_LEN);
  if (prop_ptr == NULL)
    prop_ptr = "";
  return &prop_ptr;
}

static const void *
dvr_entry_class_image_get(void *o)
{
  return dvr_entry_class_image_get0(o, dvr_entry_get_image(o));
}

static void
dvr_entry_class_image_notify(void *o, const char *lang)
{
  (void)imagecache_get_id(dvr_entry_get_image(o));
}

static const void *
dvr_entry_class_fanart_image_get(void *o)
{
  return dvr_entry_class_image_get0(o, dvr_entry_get_fanart_image(o));
}

static void
dvr_entry_class_fanart_image_notify(void *o, const char *lang)
{
  (void)imagecache_get_id(dvr_entry_get_fanart_image(o));
}

static const void *
dvr_entry_class_rating_icon_url_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  ratinglabel_t *rl = de->de_rating_label;
  if ((rl == NULL) || (de->de_sched_state > DVR_SCHEDULED)) {
    //See if there is a saved icon and if so return the imagecache path for that icon.
    prop_ptr = "";
    if(de->de_rating_icon_saved){
        prop_ptr = de->de_rating_icon_saved;
        prop_ptr = imagecache_get_propstr(prop_ptr, prop_sbuf, PROP_SBUF_LEN);
    }
  } else {
    //Get the icon from the live RL object.
    return ratinglabel_class_get_icon (rl);
  }
  return &prop_ptr;
}

static const void *
dvr_entry_class_rating_label_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  ratinglabel_t *rl = de->de_rating_label;
  if (rl == NULL) {
    prop_ptr = "";
    if(de->de_rating_label_saved){
        prop_ptr = de->de_rating_label_saved;
    }
  } else {
    if(de->de_sched_state == DVR_SCHEDULED){
      prop_ptr = rl->rl_display_label;
    }
    else
    {
      prop_ptr = de->de_rating_label_saved;
    }

  }
  return &prop_ptr;
}

static const void *
dvr_entry_class_rating_authority_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  ratinglabel_t *rl = de->de_rating_label;
  if (rl == NULL) {
    prop_ptr = "";
    if(de->de_rating_authority_saved){
        prop_ptr = de->de_rating_authority_saved;
    }
  } else {
    if(de->de_sched_state == DVR_SCHEDULED){
      prop_ptr = rl->rl_authority;
    }
    else
    {
      prop_ptr = de->de_rating_authority_saved;
    }

  }
  return &prop_ptr;
}

static const void *
dvr_entry_class_rating_country_get(void *o)
{
  dvr_entry_t *de = (dvr_entry_t *)o;
  ratinglabel_t *rl = de->de_rating_label;
  if (rl == NULL) {
    prop_ptr = "";
    if(de->de_rating_country_saved){
        prop_ptr = de->de_rating_country_saved;
    }
  } else {
    if(de->de_sched_state == DVR_SCHEDULED){
      prop_ptr = rl->rl_country;
    }
    else
    {
      prop_ptr = de->de_rating_country_saved;
    }

  }
  return &prop_ptr;
}


static const void *
dvr_entry_class_duplicate_get(void *o)
{
  static time_t null = 0;
  dvr_entry_t *de = (dvr_entry_t *)o;
  de = _dvr_duplicate_event(de);
  return de ? &de->de_start : &null;
}

static const void *
dvr_entry_class_first_aired_get(void *o)
{
  static time_t null = 0;
  const dvr_entry_t *de = (const dvr_entry_t *)o;
  return de && de->de_bcast ? &de->de_bcast->first_aired : &null;
}

static const void *
dvr_entry_class_category_get(void *o)
{
  const dvr_entry_t *de = (dvr_entry_t *)o;
  htsmsg_t *l;
  if (de->de_bcast && de->de_bcast->category) {
    l = string_list_to_htsmsg(de->de_bcast->category);
  } else {
    l = htsmsg_create_list();
  }
  return l;
}

static const void *
dvr_entry_class_credits_get(void *o)
{
  const dvr_entry_t *de = (dvr_entry_t *)o;
  htsmsg_t *l;
  if (de->de_bcast && de->de_bcast->credits) {
    l = htsmsg_copy(de->de_bcast->credits);
  } else {
    l = htsmsg_create_map();
  }
  return l;
}

static const void *
dvr_entry_class_keyword_get(void *o)
{
  const dvr_entry_t *de = (dvr_entry_t *)o;
  htsmsg_t *l;
  if (de->de_bcast && de->de_bcast->keyword) {
    l = string_list_to_htsmsg(de->de_bcast->keyword);
  } else {
    l = htsmsg_create_list();
  }
  return l;
}

static const void *
dvr_entry_class_genre_get(void *o)
{
  const dvr_entry_t *de = (dvr_entry_t *)o;
  htsmsg_t *l = htsmsg_create_list();
  if (de && de->de_bcast) {
    epg_genre_t *eg;
    LIST_FOREACH(eg, &de->de_bcast->genre, link) {
      htsmsg_add_u32(l, NULL, eg->code);
    }
  }
  return l;
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

CLASS_DOC(dvrentry)
PROP_DOC(dvr_status)
PROP_DOC(dvr_start_extra)
PROP_DOC(dvr_stop_extra)

const idclass_t dvr_entry_class = {
  .ic_class     = "dvrentry",
  .ic_caption   = N_("Digital Video Recorder"),
  .ic_event     = "dvrentry",
  .ic_doc       = tvh_doc_dvrentry_class,
  .ic_changed   = dvr_entry_class_changed,
  .ic_save      = dvr_entry_class_save,
  .ic_get_title = dvr_entry_class_get_title,
  .ic_delete    = dvr_entry_class_delete,
  .ic_perm      = dvr_entry_class_perm,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/disable the entry."),
      .off      = offsetof(dvr_entry_t, de_enabled),
    },
    {
      .type     = PT_TIME,
      .id       = "create",
      .name     = N_("Time the entry was created"),
      .desc     = N_("The create time of the entry describing the recording."),
      .set      = dvr_entry_class_create_set,
      .off      = offsetof(dvr_entry_t, de_create),
      .opts     = PO_HIDDEN | PO_RDONLY | PO_NOUI,
    },
    {
      .type     = PT_TIME,
      .id       = "watched",
      .name     = N_("Time the entry was last watched"),
      .desc     = N_("Time the entry was last watched."),
      .off      = offsetof(dvr_entry_t, de_watched),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_TIME,
      .id       = "start",
      .name     = N_("Start time"),
      .desc     = N_("The start time of the recording."),
      .set      = dvr_entry_class_start_set,
      .off      = offsetof(dvr_entry_t, de_start),
      .get_opts = dvr_entry_class_start_opts,
    },
    {
      .type     = PT_TIME,
      .id       = "start_extra",
      .name     = N_("Pre-recording padding"),
      .desc     = N_("Start recording earlier than the "
                     "EPG/timer-defined start time by x minutes."),
      .doc      = prop_doc_dvr_start_extra,
      .off      = offsetof(dvr_entry_t, de_start_extra),
      .set      = dvr_entry_class_start_extra_set,
      .list     = dvr_entry_class_extra_list,
      .get_opts = dvr_entry_class_start_extra_opts,
    },
    {
      .type     = PT_TIME,
      .id       = "start_real",
      .name     = N_("Scheduled start time"),
      .desc     = N_("The scheduled start time, including any padding."),
      .get      = dvr_entry_class_start_real_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_TIME,
      .id       = "stop",
      .name     = N_("Stop time"),
      .desc     = N_("The time the entry stops/stopped being recorded."),
      .set      = dvr_entry_class_stop_set,
      .off      = offsetof(dvr_entry_t, de_stop),
    },
    {
      .type     = PT_TIME,
      .id       = "stop_extra",
      .name     = N_("Post-recording padding"),
      .desc     = N_("Continue recording for x minutes after scheduled "
                     "stop time."),
      .doc      = prop_doc_dvr_stop_extra,
      .off      = offsetof(dvr_entry_t, de_stop_extra),
      .list     = dvr_entry_class_extra_list,
      .opts     = PO_SORTKEY | PO_ADVANCED | PO_DOC_NLIST,
    },
    {
      .type     = PT_TIME,
      .id       = "stop_real",
      .name     = N_("Scheduled stop time"),
      .desc     = N_("The scheduled stop time, including any padding."),
      .get      = dvr_entry_class_stop_real_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_TIME,
      .id       = "duration",
      .name     = N_("Scheduled Duration"),
      .desc     = N_("The total scheduled duration."),
      .get      = dvr_entry_class_duration_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_DURATION,
    },
    {
      .type     = PT_STR,
      .id       = "channel",
      .name     = N_("Channel"),
      .desc     = N_("The channel name the entry will record from."),
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
      .desc     = N_("Channel icon URL."),
      .get      = dvr_entry_class_channel_icon_url_get,
      .opts     = PO_HIDDEN | PO_RDONLY | PO_NOSAVE | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "channelname",
      .name     = N_("Channel name"),
      .desc     = N_("Name of channel the entry recorded from."),
      .get      = dvr_entry_class_channel_name_get,
      .set      = dvr_entry_class_channel_name_set,
      .off      = offsetof(dvr_entry_t, de_channel_name),
      .opts     = PO_HIDDEN | PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "image", /* Name chosen to be compatible with api_epg */
      .name     = N_("Episode image"),
      .desc     = N_("Episode image."),
      .get      = dvr_entry_class_image_get,
      .notify   = dvr_entry_class_image_notify,
      .off      = offsetof(dvr_entry_t, de_image),
      .opts     = PO_HIDDEN,
    },
    {
      .type     = PT_STR,
      .id       = "fanart_image",
      .name     = N_("Fanart image"),
      .desc     = N_("Fanart image."),
      .get      = dvr_entry_class_fanart_image_get,
      .notify   = dvr_entry_class_fanart_image_notify,
      .off      = offsetof(dvr_entry_t, de_fanart_image),
      .opts     = PO_HIDDEN,
    },
    {
      .type     = PT_LANGSTR,
      .id       = "title",
      .name     = N_("Title"),
      .desc     = N_("Title of the program."),
      .off      = offsetof(dvr_entry_t, de_title),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "disp_title",
      .name     = N_("Title"),
      .desc     = N_("Title of the program (display only)."),
      .get      = dvr_entry_class_disp_title_get,
      .set      = dvr_entry_class_disp_title_set,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_LANGSTR,
      .id       = "subtitle",
      .name     = N_("Subtitle"),
      .desc     = N_("Subtitle of the program (if any)."),
      .off      = offsetof(dvr_entry_t, de_subtitle),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "disp_subtitle",
      .name     = N_("Subtitle"),
      .desc     = N_("Subtitle of the program (if any) (display only)."),
      .get      = dvr_entry_class_disp_subtitle_get,
      .set      = dvr_entry_class_disp_subtitle_set,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_LANGSTR,
      .id       = "summary",
      .name     = N_("Summary"),
      .desc     = N_("Summary of the program (if any)."),
      .off      = offsetof(dvr_entry_t, de_summary),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "disp_summary",
      .name     = N_("Summary"),
      .desc     = N_("Summary of the program (if any) (display only)."),
      .get      = dvr_entry_class_disp_summary_get,
      .set      = dvr_entry_class_disp_summary_set,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_LANGSTR,
      .id       = "description",
      .name     = N_("Description"),
      .desc     = N_("Program synopsis."),
      .off      = offsetof(dvr_entry_t, de_desc),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "disp_description",
      .name     = N_("Description"),
      .desc     = N_("Program synopsis (display only)."),
      .get      = dvr_entry_class_disp_description_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN,
    },
    {
      .type     = PT_STR,
      .id       = "disp_extratext",
      .name     = N_("Extra text"),
      .desc     = N_("Subtitle, summary or description of the program (if any)."),
      .get      = dvr_entry_class_disp_extratext_get,
      .set      = dvr_entry_class_disp_extratext_set,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_INT,
      .id       = "pri",
      .name     = N_("Priority"),
      .desc     = N_("Priority of the recording. Higher priority entries "
                     "will take precedence and cancel lower-priority events. "
                     "The 'Not Set' value inherits the settings from "
                     "the assigned DVR configuration."),
      .off      = offsetof(dvr_entry_t, de_pri),
      .def.i    = DVR_PRIO_DEFAULT,
      .set      = dvr_entry_class_pri_set,
      .list     = dvr_entry_class_pri_list,
      .opts     = PO_SORTKEY | PO_DOC_NLIST | PO_ADVANCED,
    },
    {
      .type     = PT_U32,
      .id       = "retention",
      .name     = N_("DVR log retention"),
      .desc     = N_("Number of days to retain entry information."),
      .off      = offsetof(dvr_entry_t, de_retention),
      .def.i    = DVR_RET_REM_DVRCONFIG,
      .list     = dvr_entry_class_retention_list,
      .opts     = PO_HIDDEN | PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_U32,
      .id       = "removal",
      .name     = N_("DVR file retention period"),
      .desc     = N_("Number of days to keep the file."),
      .off      = offsetof(dvr_entry_t, de_removal),
      .def.i    = DVR_RET_REM_DVRCONFIG,
      .list     = dvr_entry_class_removal_list,
      .opts     = PO_HIDDEN | PO_ADVANCED | PO_DOC_NLIST,
    },
    {
      .type     = PT_U32,
      .id       = "playposition",
      .name     = N_("Last played position"),
      .desc     = N_("Last played position when the recording isn't fully watched yet."),
      .off      = offsetof(dvr_entry_t, de_playposition),
      .def.i    = 0,
      .opts     = PO_HIDDEN | PO_NOUI | PO_DOC_NLIST,
    },
    {
      .type     = PT_U32,
      .id       = "playcount",
      .name     = N_("Recording play count"),
      .desc     = N_("Number of times this recording was played."),
      .off      = offsetof(dvr_entry_t, de_playcount),
      .def.i    = 0,
      .opts     = PO_HIDDEN | PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .id       = "config_name",
      .name     = N_("DVR configuration"),
      .desc     = N_("The DVR profile to be used/used by the recording."),
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
      .desc     = N_("Owner of the entry."),
      .off      = offsetof(dvr_entry_t, de_owner),
      .list     = user_get_userlist,
      .get_opts = dvr_entry_class_owner_opts,
      .opts     = PO_SORTKEY,
    },
    {
      .type     = PT_STR,
      .id       = "creator",
      .name     = N_("Creator"),
      .desc     = N_("The user who created the recording, or the "
                     "auto-recording source and IP address if scheduled "
                     "by a matching rule."),
      .off      = offsetof(dvr_entry_t, de_creator),
      .get_opts = dvr_entry_class_owner_opts,
      .opts     = PO_SORTKEY,
    },
    {
      .type     = PT_STR,
      .id       = "filename",
      .name     = N_("Filename"),
      .desc     = N_("Filename used by the entry."),
      .get      = dvr_entry_class_filename_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "directory",
      .name     = N_("Directory"),
      .desc     = N_("Directory used by the entry."),
      .off      = offsetof(dvr_entry_t, de_directory),
      .opts     = PO_RDONLY | PO_NOUI,
    },
    {
      .type     = PT_U32,
      .id       = "errorcode",
      .name     = N_("Error code"),
      .desc     = N_("Error code of entry."),
      .off      = offsetof(dvr_entry_t, de_last_error),
      .opts     = PO_RDONLY | PO_NOUI,
    },
    {
      .type     = PT_U32,
      .id       = "errors",
      .name     = N_("Errors"),
      .desc     = N_("Number of errors that occurred during recording."),
      .off      = offsetof(dvr_entry_t, de_errors),
      .opts     = PO_RDONLY | PO_ADVANCED,
    },
    {
      .type     = PT_U32,
      .id       = "data_errors",
      .name     = N_("Data errors"),
      .desc     = N_("Number of errors that occurred during recording "
                     "(Transport errors)."),
      .off      = offsetof(dvr_entry_t, de_data_errors),
      .opts     = PO_RDONLY | PO_ADVANCED,
    },
    {
      .type     = PT_U16,
      .id       = "dvb_eid",
      .name     = N_("DVB EPG ID"),
      .desc     = N_("The EPG ID used by the entry."),
      .off      = offsetof(dvr_entry_t, de_dvb_eid),
      .opts     = PO_RDONLY | PO_EXPERT,
    },
    {
      .type     = PT_BOOL,
      .id       = "noresched",
      .name     = N_("Don't reschedule"),
      .desc     = N_("Don't re-schedule if recording fails."),
      .off      = offsetof(dvr_entry_t, de_dont_reschedule),
      .opts     = PO_HIDDEN | PO_NOUI,
    },
    {
      .type     = PT_BOOL,
      .id       = "norerecord",
      .name     = N_("Don't re-record"),
      .desc     = N_("Don't re-record if recording fails."),
      .off      = offsetof(dvr_entry_t, de_dont_rerecord),
      .opts     = PO_HIDDEN | PO_ADVANCED,
    },
    {
      .type     = PT_U32,
      .id       = "fileremoved",
      .name     = N_("File removed"),
      .desc     = N_("The recorded file was removed intentionally"),
      .off      = offsetof(dvr_entry_t, de_file_removed),
      .opts     = PO_HIDDEN | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "uri",
      .name     = N_("Program unique ID (from grabber)"),
      .desc     = N_("Program unique ID (from grabber), such as MV101010101.0000"),
      .off      = offsetof(dvr_entry_t, de_uri),
      .opts     = PO_RDONLY | PO_EXPERT
    },
    {
      .type     = PT_STR,
      .id       = "autorec",
      .name     = N_("Auto record"),
      .desc     = N_("Automatically record."),
      .set      = dvr_entry_class_autorec_set,
      .get      = dvr_entry_class_autorec_get,
      .opts     = PO_RDONLY | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "autorec_caption",
      .name     = N_("Auto record caption"),
      .desc     = N_("Automatic recording caption."),
      .get      = dvr_entry_class_autorec_caption_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "timerec",
      .name     = N_("Auto time record"),
      .desc     = N_("Timer-based automatic recording."),
      .set      = dvr_entry_class_timerec_set,
      .get      = dvr_entry_class_timerec_get,
      .opts     = PO_RDONLY | PO_EXPERT,
    },
    {
      .type     = PT_STR,
      .id       = "timerec_caption",
      .name     = N_("Time record caption"),
      .desc     = N_("Timer-based automatic record caption."),
      .get      = dvr_entry_class_timerec_caption_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "parent",
      .name     = N_("Parent entry"),
      .desc     = N_("Parent entry."),
      .set      = dvr_entry_class_parent_set,
      .get      = dvr_entry_class_parent_get,
      .opts     = PO_RDONLY | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "child",
      .name     = N_("Slave entry"),
      .desc     = N_("Slave entry."),
      .set      = dvr_entry_class_child_set,
      .get      = dvr_entry_class_child_get,
      .opts     = PO_RDONLY | PO_NOUI,
    },
    {
      .type     = PT_U32,
      .id       = "content_type",
      .name     = N_("Content type"),
      .desc     = N_("Content type."),
      .list     = dvr_entry_class_content_type_list,
      .off      = offsetof(dvr_entry_t, de_content_type),
      .opts     = PO_RDONLY | PO_SORTKEY,
    },
    {
      .type     = PT_U16,
      .id       = "copyright_year",
      .name     = N_("Copyright year"),
      .desc     = N_("The copyright year of the program."),
      .off      = offsetof(dvr_entry_t, de_copyright_year),
      .opts     = PO_RDONLY | PO_EXPERT,
    },
    {
      .type     = PT_U32,
      .id       = "broadcast",
      .name     = N_("Broadcast"),
      .desc     = N_("Broadcast."),
      .set      = dvr_entry_class_broadcast_set,
      .get      = dvr_entry_class_broadcast_get,
      /* Has to be available to UI for "show duplicate" from dvr upcoming */
      .opts     = PO_RDONLY | PO_HIDDEN,
    },
    {
      .type     = PT_STR,
      .id       = "episode_disp",
      .name     = N_("Episode"),
      .desc     = N_("Episode number/ID."),
      .set      = dvr_entry_class_disp_episode_set,
      .get      = dvr_entry_class_disp_episode_get,
      .opts     = PO_HIDDEN | PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "url",
      .name     = N_("URL"),
      .desc     = N_("URL."),
      .get      = dvr_entry_class_url_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_NOUI,
    },
    {
      .type     = PT_S64,
      .id       = "filesize",
      .name     = N_("File size"),
      .desc     = N_("Recording file size."),
      .get      = dvr_entry_class_filesize_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "status",
      .name     = N_("Status"),
      .doc      = prop_doc_dvr_status,
      .desc     = N_("The recording/entry status."),
      .get      = dvr_entry_class_status_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_LOCALE,
    },
    {
      .type     = PT_STR,
      .id       = "sched_status",
      .name     = N_("Schedule status"),
      .desc     = N_("Schedule status."),
      .get      = dvr_entry_class_sched_status_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN | PO_NOUI,
    },
    {
      .type     = PT_TIME,
      .id       = "duplicate",
      .name     = N_("Rerun of"),
      .desc     = N_("Name (or date) of program the entry is a rerun of."),
      .get      = dvr_entry_class_duplicate_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_ADVANCED,
    },
    {
      .type     = PT_TIME,
      .id       = "first_aired",
      .name     = N_("First aired"),
      .desc     = N_("Time when the program was first aired"),
      .get      = dvr_entry_class_first_aired_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-form text field, enter whatever you like here."),
      .off      = offsetof(dvr_entry_t, de_comment),
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "category",
      .name     = N_("Category"),
      .desc     = N_("Extra categories, typically from xmltv"),
      .get      = dvr_entry_class_category_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_NOUI
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "credits",
      .name     = N_("Credits"),
      .desc     = N_("Credits such as cast members"),
      .get      = dvr_entry_class_credits_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_NOUI
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "keyword",
      .name     = N_("Keyword"),
      .desc     = N_("Extra keywords, typically from xmltv"),
      .get      = dvr_entry_class_keyword_get,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_NOUI
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "genre",
      .name     = N_("Genre"),
      .desc     = N_("Genre of program"),
      .get      = dvr_entry_class_genre_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_U16,
      .id       = "age_rating",
      .name     = N_("Age Rating"),
      .desc     = N_("The age rating of the program."),
      .off      = offsetof(dvr_entry_t, de_age_rating),
      .opts     = PO_RDONLY | PO_EXPERT,
    },
    {
      .type     = PT_STR,
      .id       = "rating_label_saved",
      .name     = N_("Saved Rating Label"),
      .desc     = N_("Saved parental rating for once recording is complete."),
      .off      = offsetof(dvr_entry_t, de_rating_label_saved),
      .opts     = PO_RDONLY | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "rating_icon_saved",
      .name     = N_("Saved Rating Icon Path"),
      .desc     = N_("Saved parental rating icon for once recording is complete."),
      .off      = offsetof(dvr_entry_t, de_rating_icon_saved),
      .opts     = PO_RDONLY | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "rating_authority_saved",
      .name     = N_("Saved Rating Authority"),
      .desc     = N_("Saved parental rating authority for once recording is complete."),
      .off      = offsetof(dvr_entry_t, de_rating_authority_saved),
      .opts     = PO_RDONLY | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "rating_country_saved",
      .name     = N_("Saved Rating Country"),
      .desc     = N_("Saved parental rating country for once recording is complete."),
      .off      = offsetof(dvr_entry_t, de_rating_country_saved),
      .opts     = PO_RDONLY | PO_NOUI,
    },
    //This needs to go after the 'saved' properties because loading the RL object
    //can refresh the 'saved' objects for scheduled entries.
    {
      .type     = PT_STR,
      .id       = "rating_label_uuid",
      .name     = N_("Rating Label UUID"),
      .desc     = N_("Parental rating label UUID."),
      .set      = dvr_entry_class_rating_set,
      .get      = dvr_entry_class_rating_get,
      .opts     = PO_RDONLY | PO_NOUI,
    },
    //This needs to go after the RL object is loaded because the
    //getter needs the object in order to get the imagecache icon path.
    {
      .type     = PT_STR,
      .id       = "rating_icon",
      .name     = N_("Rating Icon"),
      .desc     = N_("Rating Icon URL."),
      .get      = dvr_entry_class_rating_icon_url_get,
      .opts     = PO_HIDDEN | PO_RDONLY | PO_NOSAVE | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "rating_authority",
      .name     = N_("Rating Authority"),
      .desc     = N_("Rating Authority."),
      .get      = dvr_entry_class_rating_authority_get,
      .opts     = PO_HIDDEN | PO_RDONLY | PO_NOSAVE | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "rating_country",
      .name     = N_("Rating Country"),
      .desc     = N_("Rating Country."),
      .get      = dvr_entry_class_rating_country_get,
      .opts     = PO_HIDDEN | PO_RDONLY | PO_NOSAVE | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "rating_label",
      .name     = N_("Rating Label"),
      .desc     = N_("Rating Label."),
      .get      = dvr_entry_class_rating_label_get,
      .opts     = PO_HIDDEN | PO_RDONLY | PO_NOSAVE,
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
    de->de_channel_name = strdup(channel_get_name(ch, ""));
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
dvr_get_filesize(dvr_entry_t *de, int flags)
{
  htsmsg_field_t *f;
  htsmsg_t *m;
  const char *filename;
  int first = 1;
  int64_t res = 0, size;

  if (de->de_files == NULL)
    return -1;

  HTSMSG_FOREACH(f, de->de_files)
    if ((m = htsmsg_field_get_map(f)) != NULL) {
      filename = htsmsg_get_str(m, "filename");
      if (flags & DVR_FILESIZE_UPDATE)
        size = dvr_vfs_update_filename(filename, m);
      else
        size = htsmsg_get_s64_or_default(m, "size", -1);
      if (filename && size >= 0) {
        res = (flags & DVR_FILESIZE_TOTAL) ? (res + size) : size;
        first = 0;
      }
    }

  return first ? -1 : res;
}

/**
 *
 */
int
dvr_entry_delete(dvr_entry_t *de)
{
  dvr_config_t *cfg = de->de_config;
  htsmsg_t *m;
  htsmsg_field_t *f;
  time_t t;
  struct tm tm;
  const char *filename;
  char *str1, *str2;
  char tbuf[64], ubuf[UUID_HEX_SIZE], *rdir, *cmd;
  int r, ret = 0;

  t = dvr_entry_get_start_time(de, 1);
  localtime_r(&t, &tm);
  if (strftime(tbuf, sizeof(tbuf), "%F %T", &tm) <= 0)
    *tbuf = 0;

  str1 = dvr_entry_get_retention_string(de);
  str2 = dvr_entry_get_removal_string(de);
  tvhinfo(LS_DVR, "delete entry %s \"%s\" on \"%s\" start time %s, "
	  "scheduled for recording by \"%s\", retention \"%s\" removal \"%s\"",
          idnode_uuid_as_str(&de->de_id, ubuf),
	  lang_str_get(de->de_title, NULL), DVR_CH_NAME(de), tbuf,
	  de->de_creator ?: "", str1, str2);
  free(str2);
  free(str1);

  if(!htsmsg_is_empty(de->de_files)) {
#if ENABLE_INOTIFY
    dvr_inotify_del(de);
#endif
    rdir = NULL;
    if(cfg->dvr_title_dir || cfg->dvr_channel_dir || cfg->dvr_dir_per_day || de->de_directory)
      rdir = cfg->dvr_storage;

    dvr_vfs_remove_entry(de);
    HTSMSG_FOREACH(f, de->de_files) {
      m = htsmsg_field_get_map(f);
      if (m == NULL) continue;
      filename = htsmsg_get_str(m, "filename");
      if (filename == NULL) continue;
      r = deferred_unlink(filename, rdir);
      if(r && r != -ENOENT)
        tvhwarn(LS_DVR, "Unable to remove file '%s' from disk -- %s",
  	        filename, strerror(errno));

      cmd = de->de_config->dvr_postremove;
      if (cmd && cmd[0])
        dvr_spawn_cmd(de, cmd, filename, 0);
      htsmsg_delete_field(m, "filename");
      ret = 1;
    }
    de->de_file_removed = 1;
  }

  return ret;
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
void
dvr_entry_move(dvr_entry_t *de, int to_failed)
{
  if(de->de_sched_state == DVR_COMPLETED)
    if (dvr_entry_completed(de, to_failed ? SM_CODE_USER_REQUEST :
                                            SM_CODE_FORCE_OK))
      idnode_changed(&de->de_id);
}

/**
 *
 */
dvr_entry_t *
dvr_entry_stop(dvr_entry_t *de)
{
  if(de->de_sched_state == DVR_RECORDING) {
    dvr_stop_recording(de, SM_CODE_OK, 1, 0);
    return de;
  }

  return de;
}

/**
 * Cancels an upcoming or active recording
 */
dvr_entry_t *
dvr_entry_cancel(dvr_entry_t *de, int rerecord)
{
  dvr_entry_t *parent = de->de_parent;

  switch(de->de_sched_state) {
  case DVR_RECORDING:
    dvr_stop_recording(de, SM_CODE_ABORTED, 1, 0);
    break;
  /* Cancel is not valid for finished recordings */
  case DVR_COMPLETED:
  case DVR_MISSED_TIME:
    return de;
  case DVR_SCHEDULED:
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

/*
 * Toggle/set/unset previously recorded state
 */
void
dvr_entry_set_prevrec(dvr_entry_t *de, int cmd)
{
  if (de->de_sched_state == DVR_RECORDING)
    return;

  if (cmd < 0) /* toggle */
    cmd = de->de_dont_reschedule ? 0 : 1;

  if (cmd == !!de->de_dont_reschedule)
    return;

  if (cmd) {
    de->de_dont_reschedule = 1;
    de->de_dont_rerecord = 1;
    de->de_file_removed = 1;
    dvr_entry_completed(de, SM_CODE_PREVIOUSLY_RECORDED);
  } else {
    de->de_dont_reschedule = 0;
    de->de_dont_rerecord = 0;
    de->de_file_removed = 0;
    dvr_entry_set_timer(de);
  }

  dvr_entry_retention_timer(de);

  dvr_entry_changed(de);

  tvhinfo(LS_DVR, "\"%s\" on \"%s\": "
                  "%sset as previously recorded",
                  lang_str_get(de->de_title, NULL), DVR_CH_NAME(de),
                  cmd ? "" : "un");
}

/**
 * Called by 'dvr_entry_cancel_remove' and 'dvr_entry_cancel_delete'
 * delete = 0 -> remove finished and active recordings (visible as removed)
 * delete = 1 -> delete finished and active recordings (not visible anymore)
 */
static void
dvr_entry_cancel_delete_remove(dvr_entry_t *de, int rerecord, int _delete)
{
  dvr_entry_t *parent = de->de_parent;
  dvr_autorec_entry_t *dae = de->de_autorec;
  int save;

  switch(de->de_sched_state) {
  case DVR_RECORDING:
    dvr_stop_recording(de, SM_CODE_ABORTED, 1, 0);
  case DVR_MISSED_TIME:
  case DVR_COMPLETED:
    save = dvr_entry_delete(de);                           /* Remove files */
    if (_delete || dvr_entry_delete_retention_expired(de)) /* In case retention was postponed (retention < removal) */
      dvr_entry_destroy(de, 1);                            /* Delete database */
    else if (save) {
      dvr_entry_changed(de);
      dvr_entry_retention_timer(de);                       /* As retention timer depends on file removal */
    }
    break;
  case DVR_SCHEDULED:
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
 * Upcoming recording  -> cancel
 * Active recording    -> cancel + remove
 * Finished recordings -> remove
 * The latter 2 will be visible as "removed recording"
 */
void
dvr_entry_cancel_remove(dvr_entry_t *de, int rerecord)
{
  dvr_entry_cancel_delete_remove(de, rerecord, 0);
}

/**
 * Upcoming recording  -> cancel
 * Active recording    -> abort + delete
 * Finished recordings -> delete
 * The latter 2 will NOT be visible as "removed recording"
 */
void
dvr_entry_cancel_delete(dvr_entry_t *de, int rerecord)
{
  dvr_entry_cancel_delete_remove(de, rerecord, 1);
}

/**
 *
 */
int
dvr_entry_file_moved(const char *src, const char *dst)
{
  dvr_entry_t *de;
  htsmsg_t *m;
  htsmsg_field_t *f;
  const char *filename;
  int r = -1, chg;

  if (!src || !dst || src[0] == '\0' || dst[0] == '\0' || access(dst, R_OK))
    return r;
  tvh_mutex_lock(&global_lock);
  LIST_FOREACH(de, &dvrentries, de_global_link) {
    if (htsmsg_is_empty(de->de_files)) continue;
    chg = 0;
    HTSMSG_FOREACH(f, de->de_files)
      if ((m = htsmsg_field_get_map(f)) != NULL) {
        filename = htsmsg_get_str(m, "filename");
        if (filename && strcmp(filename, src) == 0) {
          htsmsg_set_str(m, "filename", dst);
          dvr_vfs_refresh_entry(de);
          r = 0;
          chg++;
        }
      }
    if (chg)
      idnode_changed(&de->de_id);
  }
  tvh_mutex_unlock(&global_lock);
  return r;
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
  dvr_fanart_to_prefetch = string_list_create();
  idclass_register(&dvr_entry_class);
  rere = htsmsg_create_map();
  /* load config, but remove parent/child fields */
  if((l = hts_settings_load("dvr/log")) != NULL) {
    HTSMSG_FOREACH(f, l) {
      if((c = htsmsg_get_map_by_field(f)) == NULL)
	continue;
      parent = htsmsg_get_str(c, "parent");
      child = htsmsg_get_str(c, "child");
      if (parent && parent[0])
        htsmsg_set_str(rere, parent, htsmsg_field_name(f));
      if (child && child[0])
        htsmsg_set_str(rere, htsmsg_field_name(f), child);
      htsmsg_delete_field(c, "parent");
      htsmsg_delete_field(c, "child");
      (void)dvr_entry_create(htsmsg_field_name(f), c, 0);
    }
    htsmsg_destroy(l);
  }
  dvr_in_init = 0;
  /* process parent/child mapping */
  HTSMSG_FOREACH(f, rere) {
    if((child = htsmsg_field_get_str(f)) == NULL)
      continue;
    parent = htsmsg_field_name(f);
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

  // After a while we get one new entry and prefetch artwork for an
  // upcoming dvr entry.
  mtimer_arm_rel(&dvr_fanart_timer, dvr_entry_fanart_prefetch_cb, NULL, sec2mono(3600));
}

void
dvr_entry_done(void)
{
  dvr_entry_t *de;
  lock_assert(&global_lock);
  while ((de = LIST_FIRST(&dvrentries)) != NULL) {
    if (de->de_sched_state == DVR_RECORDING)
      dvr_rec_unsubscribe(de);
    dvr_entry_destroy(de, 0);
  }
  string_list_destroy(dvr_fanart_to_prefetch);
}
