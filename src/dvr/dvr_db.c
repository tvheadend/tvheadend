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
#include "notify.h"
#include "htsp_server.h"
#include "streaming.h"

static int de_tally;

int dvr_iov_max;

struct dvr_config_list dvrconfigs;
struct dvr_entry_list dvrentries;

static void dvr_timer_expire(void *aux);
static void dvr_timer_start_recording(void *aux);

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
static void
dvrdb_changed(void)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "reload", 1);
  notify_by_msg("dvrdb", m);
}

/**
 *
 */
static void
dvrconfig_changed(void)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "reload", 1);
  notify_by_msg("dvrconfig", m);
}


/**
 *
 */
void
dvr_entry_notify(dvr_entry_t *de)
{
  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_u32(m, "updateEntry", 1);
  htsmsg_add_u32(m, "id", de->de_id);
  htsmsg_add_str(m, "status", dvr_entry_status(de));
  htsmsg_add_str(m, "schedstate", dvr_entry_schedstatus(de));
  notify_by_msg("dvrdb", m);
}


/**
 *
 */
void
dvr_make_title(char *output, size_t outlen, dvr_entry_t *de)
{
  struct tm tm;
  char buf[40];
  int i;
  dvr_config_t *cfg = dvr_config_find_by_name_default(de->de_config_name);

  if(cfg->dvr_flags & DVR_CHANNEL_IN_TITLE)
    snprintf(output, outlen, "%s-", de->de_channel->ch_name);
  else
    output[0] = 0;
  
  snprintf(output + strlen(output), outlen - strlen(output),
	   "%s", lang_str_get(de->de_title, NULL));

  localtime_r(&de->de_start, &tm);
  
  if(cfg->dvr_flags & DVR_DATE_IN_TITLE) {
    strftime(buf, sizeof(buf), "%F", &tm);
    snprintf(output + strlen(output), outlen - strlen(output), ".%s", buf);
  }

  if(cfg->dvr_flags & DVR_TIME_IN_TITLE) {
    strftime(buf, sizeof(buf), "%H-%M", &tm);
    snprintf(output + strlen(output), outlen - strlen(output), ".%s", buf);
  }

  if(cfg->dvr_flags & DVR_EPISODE_IN_TITLE) {
    if(de->de_bcast && de->de_bcast->episode)  
      epg_episode_number_format(de->de_bcast->episode,
                                output + strlen(output),
                                outlen - strlen(output),
                                ".", "S%02d", NULL, "E%02d", NULL);
  }

  if(cfg->dvr_flags & DVR_CLEAN_TITLE) {
        for (i=0;i<strlen(output);i++) {
                if (
                        output[i]<32 ||
                        output[i]>122 ||
                        output[i]==34 ||
                        output[i]==39 ||
                        output[i]==92 ||
                        output[i]==58
                        ) output[i]='_';
        }
  }
}

/**
 *
 */
static void
dvr_entry_link(dvr_entry_t *de)
{
  time_t now, preamble;
  dvr_config_t *cfg = dvr_config_find_by_name_default(de->de_config_name);

  de->de_refcnt = 1;

  LIST_INSERT_HEAD(&dvrentries, de, de_global_link);

  time(&now);

  preamble = de->de_start - (60 * de->de_start_extra) - 30;

  if(now >= de->de_stop || de->de_dont_reschedule) {
    if(de->de_filename == NULL)
      de->de_sched_state = DVR_MISSED_TIME;
    else
      _dvr_entry_completed(de);
    gtimer_arm_abs(&de->de_timer, dvr_timer_expire, de, 
	       de->de_stop + cfg->dvr_retention_days * 86400);

  } else {
    de->de_sched_state = DVR_SCHEDULED;

    gtimer_arm_abs(&de->de_timer, dvr_timer_start_recording, de, preamble);
  }
  htsp_dvr_entry_add(de);
}

/**
 * Create the event
 */
static dvr_entry_t *_dvr_entry_create (
  const char *config_name, epg_broadcast_t *e,
  channel_t *ch, time_t start, time_t stop, 
  time_t start_extra, time_t stop_extra,
	const char *title, const char *description, const char *lang,
  epg_genre_t *content_type,
	const char *creator, dvr_autorec_entry_t *dae,
	dvr_prio_t pri)
{
  dvr_entry_t *de;
  char tbuf[64];
  struct tm tm;
  time_t t;
  dvr_config_t *cfg = dvr_config_find_by_name_default(config_name);

  LIST_FOREACH(de, &ch->ch_dvrs, de_channel_link)
    if(de->de_start == start && de->de_sched_state != DVR_COMPLETED)
      return NULL;

  de = calloc(1, sizeof(dvr_entry_t));
  de->de_id = ++de_tally;

  ch = de->de_channel = ch;
  LIST_INSERT_HEAD(&de->de_channel->ch_dvrs, de, de_channel_link);

  de->de_mc = cfg->dvr_mc;

  de->de_start   = start;
  de->de_stop    = stop;
  de->de_pri     = pri;
  if (start_extra)
    de->de_start_extra = start_extra;
  else if (ch->ch_dvr_extra_time_pre)
    de->de_start_extra = ch->ch_dvr_extra_time_pre;
  else
    de->de_start_extra = cfg->dvr_extra_time_pre;
  if (stop_extra)
    de->de_stop_extra = stop_extra;
  else if (ch->ch_dvr_extra_time_post)
    de->de_stop_extra  = ch->ch_dvr_extra_time_post;
  else
    de->de_stop_extra  = cfg->dvr_extra_time_post;
  de->de_config_name = strdup(cfg->dvr_config_name);
  de->de_creator = strdup(creator);

  de->de_desc  = NULL;
  // TODO: this really needs updating
  if (e) {
    if (e->episode && e->episode->title)
      de->de_title = lang_str_copy(e->episode->title);
    if (e->description)
      de->de_desc  = lang_str_copy(e->description);
    else if (e->episode && e->episode->description)
      de->de_desc = lang_str_copy(e->episode->description);
    else if (e->summary)
      de->de_desc = lang_str_copy(e->summary);
    else if (e->episode && e->episode->summary)
      de->de_desc = lang_str_copy(e->episode->summary);
  } else if (title) {
    de->de_title = lang_str_create();
    lang_str_add(de->de_title, title, lang, 0);
    if (description) {
      de->de_desc = lang_str_create();
      lang_str_add(de->de_desc, description, lang, 0);
    }
  }
  if (content_type) de->de_content_type = *content_type;
  de->de_bcast   = e;
  if (e) e->getref((epg_object_t*)e);

  dvr_entry_link(de);

  t = de->de_start - de->de_start_extra * 60;
  localtime_r(&t, &tm);
  if (strftime(tbuf, sizeof(tbuf), "%F %T", &tm) <= 0)
    *tbuf = 0;

  if(dae != NULL) {
    de->de_autorec = dae;
    LIST_INSERT_HEAD(&dae->dae_spawns, de, de_autorec_link);
  }

  tvhlog(LOG_INFO, "dvr", "\"%s\" on \"%s\" starting at %s, "
	 "scheduled for recording by \"%s\"",
	 lang_str_get(de->de_title, NULL), de->de_channel->ch_name, tbuf, creator);
	 
  dvrdb_changed();
  dvr_entry_save(de);
  return de;
}


/**
 *
 */
dvr_entry_t *
dvr_entry_create(const char *config_name,
                 channel_t *ch, time_t start, time_t stop, 
                 time_t start_extra, time_t stop_extra,
		             const char *title, const char *description, const char *lang,
                 epg_genre_t *content_type,
		             const char *creator, dvr_autorec_entry_t *dae,
                 dvr_prio_t pri)
{
  return _dvr_entry_create(config_name, NULL,
                           ch, start, stop, start_extra, stop_extra,
                           title, description, lang, content_type,
                           creator, dae, pri);
}

/**
 *
 */
dvr_entry_t *
dvr_entry_create_by_event(const char *config_name,
                          epg_broadcast_t *e,
                          time_t start_extra, time_t stop_extra,
                          const char *creator, 
                          dvr_autorec_entry_t *dae, dvr_prio_t pri)
{
  if(!e->channel || !e->episode || !e->episode->title)
    return NULL;

  return _dvr_entry_create(config_name, e,
                           e->channel, e->start, e->stop,
                           start_extra, stop_extra,
                           NULL, NULL, NULL,
                           LIST_FIRST(&e->episode->genre),
                           creator, dae, pri);
}

static int _dvr_duplicate_event ( epg_broadcast_t *e )
{
  dvr_entry_t *de;
  LIST_FOREACH(de, &dvrentries, de_global_link) {
    if (de->de_bcast && (de->de_bcast->episode == e->episode)) return 1;
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

  if(de->de_autorec != NULL)
    LIST_REMOVE(de, de_autorec_link);

  free(de->de_config_name);
  free(de->de_creator);
  if (de->de_title) lang_str_destroy(de->de_title);
  if (de->de_desc)  lang_str_destroy(de->de_desc);
  if(de->de_bcast) de->de_bcast->putref((epg_object_t*)de->de_bcast);

  free(de);
}




/**
 *
 */
static void
dvr_entry_remove(dvr_entry_t *de)
{
  hts_settings_remove("dvr/log/%d", de->de_id);

  htsp_dvr_entry_delete(de);
  
#if ENABLE_INOTIFY
  dvr_inotify_del(de);
#endif

  gtimer_disarm(&de->de_timer);

  LIST_REMOVE(de, de_channel_link);
  LIST_REMOVE(de, de_global_link);
  de->de_channel = NULL;

  dvrdb_changed();

  dvr_entry_dec_ref(de);
}


/**
 *
 */
static void
dvr_db_load_one(htsmsg_t *c, int id)
{
  dvr_entry_t *de;
  const char *s, *creator;
  channel_t *ch;
  uint32_t start, stop, bcid;
  int d;
  dvr_config_t *cfg;
  lang_str_t *title, *ls;

  if(htsmsg_get_u32(c, "start", &start))
    return;
  if(htsmsg_get_u32(c, "stop", &stop))
    return;

  if((s = htsmsg_get_str(c, "channel")) == NULL)
    return;
  if((ch = channel_find_by_name(s, 0, 0)) == NULL)
    return;

  s = htsmsg_get_str(c, "config_name");
  cfg = dvr_config_find_by_name_default(s);

  if(!(title = lang_str_deserialize(c, "title")))
    return;

  if((creator = htsmsg_get_str(c, "creator")) == NULL)
    return;

  de = calloc(1, sizeof(dvr_entry_t));
  de->de_id = id;

  de_tally = MAX(id, de_tally);

  de->de_channel = ch;
  LIST_INSERT_HEAD(&de->de_channel->ch_dvrs, de, de_channel_link);

  de->de_start   = start;
  de->de_stop    = stop;
  de->de_config_name = strdup(cfg->dvr_config_name);
  de->de_creator = strdup(creator);
  de->de_title   = title;
  de->de_pri     = dvr_pri2val(htsmsg_get_str(c, "pri"));
  
  if(htsmsg_get_s32(c, "start_extra", &d))
    if (ch->ch_dvr_extra_time_pre)
      de->de_start_extra = ch->ch_dvr_extra_time_pre;
    else
      de->de_start_extra = cfg->dvr_extra_time_pre;
  else
    de->de_start_extra = d;

  if(htsmsg_get_s32(c, "stop_extra", &d))
    if (ch->ch_dvr_extra_time_post)
      de->de_stop_extra = ch->ch_dvr_extra_time_post;
    else
      de->de_stop_extra = cfg->dvr_extra_time_post;
  else
    de->de_stop_extra = d;


  if ((ls = lang_str_deserialize(c, "description")))
    de->de_desc = ls;
  tvh_str_set(&de->de_filename, htsmsg_get_str(c, "filename"));

  htsmsg_get_u32(c, "errorcode", &de->de_last_error);
  htsmsg_get_u32(c, "errors", &de->de_errors);

  htsmsg_get_u32(c, "noresched", &de->de_dont_reschedule);

  s = htsmsg_get_str(c, "autorec");
  if(s != NULL) {
    dvr_autorec_entry_t *dae = autorec_entry_find(s, 0);

    if(dae != NULL) {
      de->de_autorec = dae;
      LIST_INSERT_HEAD(&dae->dae_spawns, de, de_autorec_link);
    }
  }


  de->de_content_type.code = htsmsg_get_u32_or_default(c, "contenttype", 0);

  if (!htsmsg_get_u32(c, "broadcast", &bcid)) {
    de->de_bcast = epg_broadcast_find_by_id(bcid, ch);
    if (de->de_bcast) {
      de->de_bcast->getref((epg_object_t*)de->de_bcast);
    }
  }

  de->de_mc = htsmsg_get_u32_or_default(c, "container", MC_MATROSKA);

  dvr_entry_link(de);
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
      dvr_db_load_one(c, atoi(f->hmf_name));
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

  htsmsg_add_str(m, "channel", de->de_channel->ch_name);
  htsmsg_add_u32(m, "start", de->de_start);
  htsmsg_add_u32(m, "stop", de->de_stop);
 
  htsmsg_add_s32(m, "start_extra", de->de_start_extra);
  htsmsg_add_s32(m, "stop_extra", de->de_stop_extra);
  
  htsmsg_add_str(m, "config_name", de->de_config_name);

  htsmsg_add_str(m, "creator", de->de_creator);

  if(de->de_filename != NULL)
    htsmsg_add_str(m, "filename", de->de_filename);

  lang_str_serialize(de->de_title, m, "title");

  if(de->de_desc != NULL)
    lang_str_serialize(de->de_desc, m, "description");

  htsmsg_add_str(m, "pri", dvr_val2pri(de->de_pri));

  if(de->de_last_error)
    htsmsg_add_u32(m, "errorcode", de->de_last_error);

  if(de->de_errors)
    htsmsg_add_u32(m, "errors", de->de_errors);

  htsmsg_add_u32(m, "noresched", de->de_dont_reschedule);

  if(de->de_autorec != NULL)
    htsmsg_add_str(m, "autorec", de->de_autorec->dae_id);

  if(de->de_content_type.code)
    htsmsg_add_u32(m, "contenttype", de->de_content_type.code);

  if(de->de_bcast)
    htsmsg_add_u32(m, "broadcast", de->de_bcast->id);

  htsmsg_add_u32(m, "container", de->de_mc);

  hts_settings_save(m, "dvr/log/%d", de->de_id);
  htsmsg_destroy(m);
}


/**
 *
 */
static void
dvr_timer_expire(void *aux)
{
  dvr_entry_t *de = aux;
  dvr_entry_remove(de);
 
}

static dvr_entry_t *_dvr_entry_update
  ( dvr_entry_t *de, epg_broadcast_t *e,
    const char *title, const char *desc, const char *lang, 
    time_t start, time_t stop, time_t start_extra, time_t stop_extra )
{
  int save = 0;

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

  /* Title */ 
  if (e && e->episode && e->episode->title) {
    if (de->de_title) lang_str_destroy(de->de_title);
    de->de_title = lang_str_copy(e->episode->title);
  } else if (title) {
    if (!de->de_title) de->de_title = lang_str_create();
    save = lang_str_add(de->de_title, title, lang, 1);
  }

  // TODO: description

  /* Genre */
  if (e && e->episode) {
    epg_genre_t *g = LIST_FIRST(&e->episode->genre);
    if (g && (g->code != de->de_content_type.code)) {
      de->de_content_type.code = g->code;
      save = 1;
    }
  }

  /* Broadcast */
  if (e && (de->de_bcast != e)) {
    de->de_bcast->putref(de->de_bcast);
    de->de_bcast = e;
    e->getref(e);
    save = 1;
  }

  /* Save changes */
  if (save) {
    dvr_entry_save(de);
    htsp_dvr_entry_update(de);
    dvr_entry_notify(de);
    tvhlog(LOG_INFO, "dvr", "\"%s\" on \"%s\": Updated Timer",
           lang_str_get(de->de_title, NULL), de->de_channel->ch_name);
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
 *
 * TODO: I think this will record the title slot event if its now a 
 *       completely different episode etc...
 */
void 
dvr_event_replaced(epg_broadcast_t *e, epg_broadcast_t *new_e)
{
  dvr_entry_t *de, *ude;
  if ( e == new_e ) return;

  de = dvr_entry_find_by_event(e);
  if (de != NULL) {
    ude = dvr_entry_find_by_event_fuzzy(new_e);
    if (ude == NULL && de->de_sched_state == DVR_SCHEDULED)
      dvr_entry_cancel(de);
    else if(new_e->episode && new_e->episode->title)
      _dvr_entry_update(de, new_e, NULL, NULL, NULL, 0, 0, 0, 0);
  }
}

void dvr_event_updated ( epg_broadcast_t *e )
{
  dvr_entry_t *de;
  de = dvr_entry_find_by_event(e);
  if (de) _dvr_entry_update(de, e, NULL, NULL, NULL, 0, 0, 0, 0);
}

/**
 *
 */
static void
dvr_stop_recording(dvr_entry_t *de, int stopcode)
{
  dvr_config_t *cfg = dvr_config_find_by_name_default(de->de_config_name);

  if (de->de_rec_state == DVR_RS_PENDING || de->de_rec_state == DVR_RS_WAIT_PROGRAM_START)
    de->de_sched_state = DVR_MISSED_TIME;
  else
    _dvr_entry_completed(de);

  dvr_rec_unsubscribe(de, stopcode);

  tvhlog(LOG_INFO, "dvr", "\"%s\" on \"%s\": "
	 "End of program: %s",
	 lang_str_get(de->de_title, NULL), de->de_channel->ch_name,
	 dvr_entry_status(de));

  dvr_entry_save(de);
  htsp_dvr_entry_update(de);
  dvr_entry_notify(de);

  gtimer_arm_abs(&de->de_timer, dvr_timer_expire, de, 
		 de->de_stop + cfg->dvr_retention_days * 86400);
}


/**
 *
 */
static void
dvr_timer_stop_recording(void *aux)
{
  dvr_stop_recording(aux, 0);
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
	 lang_str_get(de->de_title, NULL), de->de_channel->ch_name);

  dvr_entry_notify(de);
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
    if(de->de_id == id)
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

  LIST_FOREACH(de, &e->channel->ch_dvrs, de_channel_link)
    if(de->de_bcast == e) return de;
  return NULL;
}

/**
 * Find dvr entry using 'fuzzy' search
 */
dvr_entry_t *
dvr_entry_find_by_event_fuzzy(epg_broadcast_t *e)
{
  dvr_entry_t *de;
  
  if (!e->episode || !e->episode->title)
    return NULL;

  LIST_FOREACH(de, &e->channel->ch_dvrs, de_channel_link)
    if ((abs(de->de_start - e->start) < 600) && (abs(de->de_stop - e->stop) < 600)) {
        return de;
    }
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
    dvr_entry_remove(de);
    return NULL;

  case DVR_RECORDING:
    de->de_dont_reschedule = 1;
    dvr_stop_recording(de, SM_CODE_ABORTED);
    return de;

  case DVR_COMPLETED:
    dvr_entry_remove(de);
    return NULL;

  case DVR_MISSED_TIME:
    dvr_entry_remove(de);
    return NULL;

  default:
    abort();
  }
}


/**
 * Unconditionally remove an entry
 */
static void
dvr_entry_purge(dvr_entry_t *de)
{
  if(de->de_sched_state == DVR_RECORDING)
    dvr_stop_recording(de, SM_CODE_SOURCE_DELETED);

  dvr_entry_remove(de);
}

/**
 *
 */
void
dvr_destroy_by_channel(channel_t *ch)
{
  dvr_entry_t *de;

  while((de = LIST_FIRST(&ch->ch_dvrs)) != NULL)
    dvr_entry_purge(de);
}

/**
 *
 */
void
dvr_init(void)
{
  htsmsg_t *m, *l;
  htsmsg_field_t *f;
  const char *s;
  char buf[500];
  const char *homedir;
  struct stat st;
  uint32_t u32;
  dvr_config_t *cfg;

  dvr_iov_max = sysconf(_SC_IOV_MAX);

  /* Default settings */

  LIST_INIT(&dvrconfigs);
  cfg = dvr_config_create("");

  /* Override settings with config */

  l = hts_settings_load("dvr");
  if(l != NULL) {
    HTSMSG_FOREACH(f, l) {
      m = htsmsg_get_map_by_field(f);
      if(m == NULL)
        continue;

      s = htsmsg_get_str(m, "config_name");
      cfg = dvr_config_find_by_name(s);
      if(cfg == NULL)
        cfg = dvr_config_create(s);

      cfg->dvr_mc = htsmsg_get_u32_or_default(m, "container", MC_MATROSKA);

      htsmsg_get_s32(m, "pre-extra-time", &cfg->dvr_extra_time_pre);
      htsmsg_get_s32(m, "post-extra-time", &cfg->dvr_extra_time_post);
      htsmsg_get_u32(m, "retention-days", &cfg->dvr_retention_days);
      tvh_str_set(&cfg->dvr_storage, htsmsg_get_str(m, "storage"));

      if(!htsmsg_get_u32(m, "day-dir", &u32) && u32)
        cfg->dvr_flags |= DVR_DIR_PER_DAY;

      if(!htsmsg_get_u32(m, "channel-dir", &u32) && u32)
        cfg->dvr_flags |= DVR_DIR_PER_CHANNEL;

      if(!htsmsg_get_u32(m, "channel-in-title", &u32) && u32)
        cfg->dvr_flags |= DVR_CHANNEL_IN_TITLE;

      if(!htsmsg_get_u32(m, "date-in-title", &u32) && u32)
        cfg->dvr_flags |= DVR_DATE_IN_TITLE;

      if(!htsmsg_get_u32(m, "time-in-title", &u32) && u32)
        cfg->dvr_flags |= DVR_TIME_IN_TITLE;
   
      if(!htsmsg_get_u32(m, "whitespace-in-title", &u32) && u32)
        cfg->dvr_flags |= DVR_WHITESPACE_IN_TITLE;

      if(!htsmsg_get_u32(m, "title-dir", &u32) && u32)
        cfg->dvr_flags |= DVR_DIR_PER_TITLE;

      if(!htsmsg_get_u32(m, "episode-in-title", &u32) && u32)
        cfg->dvr_flags |= DVR_EPISODE_IN_TITLE;

      if(!htsmsg_get_u32(m, "clean-title", &u32) && u32)
        cfg->dvr_flags |= DVR_CLEAN_TITLE;

      if(!htsmsg_get_u32(m, "tag-files", &u32) && !u32)
        cfg->dvr_flags &= ~DVR_TAG_FILES;

      if(!htsmsg_get_u32(m, "skip-commercials", &u32) && !u32)
        cfg->dvr_flags &= ~DVR_SKIP_COMMERCIALS;

      tvh_str_set(&cfg->dvr_postproc, htsmsg_get_str(m, "postproc"));
    }

    htsmsg_destroy(l);
  }

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

  dvr_inotify_init();
  dvr_autorec_init();
  dvr_db_load();
  dvr_autorec_update();
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
    tvhlog(LOG_WARNING, "dvr", "Configuration '%s' not found", name);
    cfg = dvr_config_find_by_name("");
  }

  if (cfg == NULL) {
    cfg = dvr_config_create("");
  }

  return cfg;
}

/**
 * create a new named dvr config; the caller is responsible
 * to avoid duplicates
 */
dvr_config_t *
dvr_config_create(const char *name)
{
  dvr_config_t *cfg;

  if (name == NULL)
    name = "";

  tvhlog(LOG_INFO, "dvr", "Creating new configuration '%s'", name);

  cfg = calloc(1, sizeof(dvr_config_t));
  cfg->dvr_config_name = strdup(name);
  cfg->dvr_retention_days = 31;
  cfg->dvr_mc = MC_MATROSKA;
  cfg->dvr_flags = DVR_TAG_FILES | DVR_SKIP_COMMERCIALS;

  /* series link support */
  cfg->dvr_sl_brand_lock   = 1; // use brand linking
  cfg->dvr_sl_season_lock  = 0; // ignore season (except if no brand)
  cfg->dvr_sl_channel_lock = 1; // channel locked
  cfg->dvr_sl_time_lock    = 0; // time slot (approx) locked
  cfg->dvr_sl_more_recent  = 1; // Only record more reason episodes
  cfg->dvr_sl_quality_lock = 1; // Don't attempt to ajust quality

  /* dup detect */
  cfg->dvr_dup_detect_episode = 1; // detect dup episodes

  LIST_INSERT_HEAD(&dvrconfigs, cfg, config_link);

  return LIST_FIRST(&dvrconfigs);
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
  if (cfg != NULL) {
    tvhlog(LOG_INFO, "dvr", "Deleting configuration '%s'", 
        cfg->dvr_config_name);
    hts_settings_remove("dvr/config%s", cfg->dvr_config_name);
    LIST_REMOVE(cfg, config_link);
    dvrconfig_changed();
  }
}

/**
 *
 */
static void
dvr_save(dvr_config_t *cfg)
{
  htsmsg_t *m = htsmsg_create_map();
  if (cfg->dvr_config_name != NULL && strlen(cfg->dvr_config_name) != 0)
    htsmsg_add_str(m, "config_name", cfg->dvr_config_name);
  htsmsg_add_str(m, "storage", cfg->dvr_storage);
  htsmsg_add_u32(m, "container", cfg->dvr_mc);
  htsmsg_add_u32(m, "retention-days", cfg->dvr_retention_days);
  htsmsg_add_u32(m, "pre-extra-time", cfg->dvr_extra_time_pre);
  htsmsg_add_u32(m, "post-extra-time", cfg->dvr_extra_time_post);
  htsmsg_add_u32(m, "day-dir",          !!(cfg->dvr_flags & DVR_DIR_PER_DAY));
  htsmsg_add_u32(m, "channel-dir",      !!(cfg->dvr_flags & DVR_DIR_PER_CHANNEL));
  htsmsg_add_u32(m, "channel-in-title", !!(cfg->dvr_flags & DVR_CHANNEL_IN_TITLE));
  htsmsg_add_u32(m, "date-in-title",    !!(cfg->dvr_flags & DVR_DATE_IN_TITLE));
  htsmsg_add_u32(m, "time-in-title",    !!(cfg->dvr_flags & DVR_TIME_IN_TITLE));
  htsmsg_add_u32(m, "whitespace-in-title", !!(cfg->dvr_flags & DVR_WHITESPACE_IN_TITLE));
  htsmsg_add_u32(m, "title-dir", !!(cfg->dvr_flags & DVR_DIR_PER_TITLE));
  htsmsg_add_u32(m, "episode-in-title", !!(cfg->dvr_flags & DVR_EPISODE_IN_TITLE));
  htsmsg_add_u32(m, "clean-title", !!(cfg->dvr_flags & DVR_CLEAN_TITLE));
  htsmsg_add_u32(m, "tag-files", !!(cfg->dvr_flags & DVR_TAG_FILES));
  htsmsg_add_u32(m, "skip-commercials", !!(cfg->dvr_flags & DVR_SKIP_COMMERCIALS));
  if(cfg->dvr_postproc != NULL)
    htsmsg_add_str(m, "postproc", cfg->dvr_postproc);

  hts_settings_save(m, "dvr/config%s", cfg->dvr_config_name);
  htsmsg_destroy(m);

  dvrconfig_changed();
}

/**
 *
 */
void
dvr_storage_set(dvr_config_t *cfg, const char *storage)
{
  if(cfg->dvr_storage != NULL && !strcmp(cfg->dvr_storage, storage))
    return;

  tvh_str_set(&cfg->dvr_storage, storage);
  dvr_save(cfg);
}

/**
 *
 */
void
dvr_container_set(dvr_config_t *cfg, const char *container)
{
  muxer_container_type_t mc;

  mc = muxer_container_txt2type(container);
  if(mc == MC_UNKNOWN)
    mc = MC_MATROSKA;

  if(cfg->dvr_mc == mc)
    return;

  cfg->dvr_mc = mc;

  dvr_save(cfg);
}


/**
 *
 */
void
dvr_postproc_set(dvr_config_t *cfg, const char *postproc)
{
  if(cfg->dvr_postproc != NULL && !strcmp(cfg->dvr_postproc, postproc))
    return;

  tvh_str_set(&cfg->dvr_postproc, !strcmp(postproc, "") ? NULL : postproc);
  dvr_save(cfg);
}


/**
 *
 */
void
dvr_retention_set(dvr_config_t *cfg, int days)
{
  dvr_entry_t *de;
  if(days < 1 || cfg->dvr_retention_days == days)
    return;

  cfg->dvr_retention_days = days;

  /* Also, rearm all timres */

  LIST_FOREACH(de, &dvrentries, de_global_link)
    if(de->de_sched_state == DVR_COMPLETED)
      gtimer_arm_abs(&de->de_timer, dvr_timer_expire, de, 
		     de->de_stop + cfg->dvr_retention_days * 86400);
  dvr_save(cfg);
}


/**
 *
 */
void
dvr_flags_set(dvr_config_t *cfg, int flags)
{
  if(cfg->dvr_flags == flags)
    return;

  cfg->dvr_flags = flags;
  dvr_save(cfg);
}


/**
 *
 */
void
dvr_extra_time_pre_set(dvr_config_t *cfg, int d)
{
  if(cfg->dvr_extra_time_pre == d)
    return;

  cfg->dvr_extra_time_pre = d;
  dvr_save(cfg);
}


/**
 *
 */
void
dvr_extra_time_post_set(dvr_config_t *cfg, int d)
{
  if(cfg->dvr_extra_time_post == d)
    return;

  cfg->dvr_extra_time_post = d;
  dvr_save(cfg);
}


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

    dvr_config_t *cfg = dvr_config_find_by_name_default(de->de_config_name);
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
  dvr_entry_remove(de);
}

/**
 *
 */
void
dvr_entry_cancel_delete(dvr_entry_t *de)
{
  switch(de->de_sched_state) {
  case DVR_SCHEDULED:
    dvr_entry_remove(de);
    break;

  case DVR_RECORDING:
    de->de_dont_reschedule = 1;
    dvr_stop_recording(de, SM_CODE_ABORTED);
  case DVR_COMPLETED:
    dvr_entry_delete(de);
    break;

  case DVR_MISSED_TIME:
    dvr_entry_remove(de);
    break;

  default:
    abort();
  }
}
