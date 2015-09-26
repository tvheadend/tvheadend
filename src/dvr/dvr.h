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

#ifndef DVR_H
#define DVR_H

#include <regex.h>
#include "epg.h"
#include "channels.h"
#include "subscriptions.h"
#include "muxer.h"
#include "profile.h"
#include "lang_str.h"

typedef struct dvr_config {
  idnode_t dvr_id;

  LIST_ENTRY(dvr_config) config_link;
  LIST_ENTRY(dvr_config) profile_link;

  int dvr_enabled;
  int dvr_valid;
  char *dvr_config_name;
  char *dvr_comment;
  profile_t *dvr_profile;
  char *dvr_storage;
  uint32_t dvr_retention_days;
  char *dvr_charset;
  char *dvr_charset_id;
  char *dvr_postproc;
  uint32_t dvr_extra_time_pre;
  uint32_t dvr_extra_time_post;
  uint32_t dvr_update_window;

  muxer_config_t dvr_muxcnf;

  char *dvr_pathname;
  int dvr_pathname_changed;

  int dvr_dir_per_day;
  int dvr_channel_dir;
  int dvr_channel_in_title;
  int dvr_omit_title;
  int dvr_date_in_title;
  int dvr_time_in_title;
  int dvr_whitespace_in_title;
  int dvr_title_dir;
  int dvr_episode_in_title;
  int dvr_clean_title;
  int dvr_tag_files;
  int dvr_skip_commercials;
  int dvr_subtitle_in_title;
  int dvr_windows_compatible_filenames;

  struct dvr_entry_list dvr_entries;
  struct dvr_autorec_entry_list dvr_autorec_entries;
  struct dvr_timerec_entry_list dvr_timerec_entries;

  idnode_list_head_t dvr_accesses;

} dvr_config_t;

extern struct dvr_config_list dvrconfigs;

extern struct dvr_entry_list dvrentries;

typedef enum {
  DVR_PRIO_IMPORTANT   = 0,
  DVR_PRIO_HIGH        = 1,
  DVR_PRIO_NORMAL      = 2,
  DVR_PRIO_LOW         = 3,
  DVR_PRIO_UNIMPORTANT = 4,
  DVR_PRIO_NOTSET      = 5,
} dvr_prio_t;


LIST_HEAD(dvr_rec_stream_list, dvr_rec_stream);


typedef enum {
  DVR_SCHEDULED,         /* Scheduled for recording (in the future) */
  DVR_RECORDING,
  DVR_COMPLETED,         /* If recording failed, de->de_error is set to
			    a string */
  DVR_NOSTATE,
  DVR_MISSED_TIME,
} dvr_entry_sched_state_t;


typedef enum {
  DVR_RS_PENDING,
  DVR_RS_WAIT_PROGRAM_START,
  DVR_RS_RUNNING,
  DVR_RS_COMMERCIAL,
  DVR_RS_ERROR,
} dvr_rs_state_t;
  

typedef struct dvr_entry {

  idnode_t de_id;

  int de_refcnt;   /* Modification is protected under global_lock */


  /**
   * Upon dvr_entry_remove() this fields will be invalidated (and pointers
   * NULLed)
   */

  LIST_ENTRY(dvr_entry) de_global_link;
  
  channel_t *de_channel;
  LIST_ENTRY(dvr_entry) de_channel_link;

  char *de_channel_name;

  gtimer_t de_timer;

  /**
   * These meta fields will stay valid as long as reference count > 0
   */

  dvr_config_t *de_config;
  LIST_ENTRY(dvr_entry) de_config_link;

  int de_enabled;

  time_t de_start;
  time_t de_stop;

  time_t de_start_extra;
  time_t de_stop_extra;

  char *de_owner;
  char *de_creator;
  char *de_comment;
  htsmsg_t *de_files; /* List of all used files */
  char *de_directory; /* Can be set for autorec entries, will override any 
                         directory setting from the configuration */
  lang_str_t *de_title;      /* Title in UTF-8 (from EPG) */
  lang_str_t *de_subtitle;   /* Subtitle in UTF-8 (from EPG) */
  lang_str_t *de_desc;       /* Description in UTF-8 (from EPG) */
  uint32_t de_content_type;  /* Content type (from EPG) (only code) */

  uint16_t de_dvb_eid;

  int de_pri;
  int de_dont_reschedule;
  int de_mc;
  int de_retention;

  /**
   * EPG information / links
   */
  epg_broadcast_t *de_bcast;
  char *de_episode;

  /**
   * Major State
   */
  dvr_entry_sched_state_t de_sched_state;

  /**
   * Recording state (onyl valid if de_sched_state == DVR_RECORDING)
   */
  dvr_rs_state_t de_rec_state;

  /**
   * Number of errors (only to be modified by the recording thread)
   */
  uint32_t de_errors;

  /**
   * Number of data errors (only to be modified by the recording thread)
   */
  uint32_t de_data_errors;

  /**
   * Last error, see SM_CODE_ defines
   */
  uint32_t de_last_error;
  

  /**
   * Autorec linkage
   */
  LIST_ENTRY(dvr_entry) de_autorec_link;
  struct dvr_autorec_entry *de_autorec;

  /**
   * Timerec linkage
   */
  struct dvr_timerec_entry *de_timerec;

  /**
   * Fields for recording
   */
  pthread_t de_thread;

  th_subscription_t *de_s;

  /**
   * Stream worker chain
   */
  profile_chain_t *de_chain;

  /**
   * Inotify
   */
#if ENABLE_INOTIFY
  LIST_ENTRY(dvr_entry) de_inotify_link;
#endif

  /**
   * Entry change notification timer
   */
  time_t de_last_notify;

} dvr_entry_t;

#define DVR_CH_NAME(e) ((e)->de_channel == NULL ? (e)->de_channel_name : channel_get_name((e)->de_channel))

typedef enum {
  DVR_AUTOREC_RECORD_ALL = 0,
  DVR_AUTOREC_RECORD_DIFFERENT_EPISODE_NUMBER = 1,
  DVR_AUTOREC_RECORD_DIFFERENT_SUBTITLE = 2,
  DVR_AUTOREC_RECORD_DIFFERENT_DESCRIPTION = 3,
  DVR_AUTOREC_RECORD_ONCE_PER_WEEK = 4,
  DVR_AUTOREC_RECORD_ONCE_PER_DAY = 5
} dvr_autorec_dedup_t;


/**
 * Autorec entry
 */
typedef struct dvr_autorec_entry {
  idnode_t dae_id;

  TAILQ_ENTRY(dvr_autorec_entry) dae_link;

  char *dae_name;
  char *dae_directory;
  dvr_config_t *dae_config;
  LIST_ENTRY(dvr_autorec_entry) dae_config_link;

  int dae_enabled;
  char *dae_owner;
  char *dae_creator;
  char *dae_comment;

  char *dae_title;
  regex_t dae_title_preg;
  int dae_fulltext;
  
  uint32_t dae_content_type;

  int dae_start;        /* Minutes from midnight */
  int dae_start_window; /* Minutes (duration) */

  uint32_t dae_weekdays;

  channel_t *dae_channel;
  LIST_ENTRY(dvr_autorec_entry) dae_channel_link;

  channel_tag_t *dae_channel_tag;
  LIST_ENTRY(dvr_autorec_entry) dae_channel_tag_link;

  int dae_pri;

  struct dvr_entry_list dae_spawns;

  epg_brand_t *dae_brand;
  epg_season_t *dae_season;
  epg_serieslink_t *dae_serieslink;
  epg_episode_num_t dae_epnum;

  int dae_minduration;
  int dae_maxduration;
  int dae_retention;

  time_t dae_start_extra;
  time_t dae_stop_extra;
  
  int dae_record;
  
} dvr_autorec_entry_t;

TAILQ_HEAD(dvr_autorec_entry_queue, dvr_autorec_entry);

extern struct dvr_autorec_entry_queue autorec_entries;

/**
 * Timerec entry
 */
typedef struct dvr_timerec_entry {
  idnode_t dte_id;

  TAILQ_ENTRY(dvr_timerec_entry) dte_link;

  char *dte_name;
  char *dte_directory;
  dvr_config_t *dte_config;
  LIST_ENTRY(dvr_timerec_entry) dte_config_link;

  int dte_enabled;
  char *dte_owner;
  char *dte_creator;
  char *dte_comment;

  char *dte_title;

  int dte_start;  /* Minutes from midnight */
  int dte_stop;   /* Minutes from midnight */

  uint32_t dte_weekdays;

  channel_t *dte_channel;
  LIST_ENTRY(dvr_timerec_entry) dte_channel_link;

  int dte_pri;

  dvr_entry_t *dte_spawn;

  int dte_retention;
} dvr_timerec_entry_t;

TAILQ_HEAD(dvr_timerec_entry_queue, dvr_timerec_entry);

extern struct dvr_timerec_entry_queue timerec_entries;

/**
 *
 */

extern const idclass_t dvr_config_class;
extern const idclass_t dvr_entry_class;
extern const idclass_t dvr_autorec_entry_class;
extern const idclass_t dvr_timerec_entry_class;

/**
 * Prototypes
 */

static inline int dvr_config_is_valid(dvr_config_t *cfg)
  { return cfg->dvr_valid; }

static inline int dvr_config_is_default(dvr_config_t *cfg)
  { return cfg->dvr_config_name == NULL || cfg->dvr_config_name[0] == '\0'; }

dvr_config_t *dvr_config_find_by_name(const char *name);

dvr_config_t *dvr_config_find_by_name_default(const char *name);

dvr_config_t *dvr_config_find_by_list(htsmsg_t *list, const char *name);

dvr_config_t *dvr_config_create(const char *name, const char *uuid, htsmsg_t *conf);

static inline dvr_config_t *dvr_config_find_by_uuid(const char *uuid)
  { return (dvr_config_t*)idnode_find(uuid, &dvr_config_class, NULL); }

void dvr_config_delete(const char *name);

void dvr_config_save(dvr_config_t *cfg);

void dvr_config_destroy_by_profile(profile_t *pro, int delconf);

/*
 *
 */

uint32_t dvr_usage_count(access_t *aa);

static inline int dvr_entry_is_editable(dvr_entry_t *de)
  { return de->de_sched_state == DVR_SCHEDULED; }

static inline int dvr_entry_is_valid(dvr_entry_t *de)
  { return de->de_refcnt > 0; }

int dvr_entry_get_mc(dvr_entry_t *de);

int dvr_entry_get_retention( dvr_entry_t *de );

int dvr_entry_get_start_time( dvr_entry_t *de );

int dvr_entry_get_stop_time( dvr_entry_t *de );

int dvr_entry_get_extra_time_post( dvr_entry_t *de );

int dvr_entry_get_extra_time_pre( dvr_entry_t *de );

void dvr_entry_init(void);

void dvr_entry_done(void);

void dvr_entry_save(dvr_entry_t *de);

void dvr_entry_destroy_by_config(dvr_config_t *cfg, int delconf);

int dvr_entry_set_state(dvr_entry_t *de, dvr_entry_sched_state_t state,
                        dvr_rs_state_t rec_state, int error_code);

const char *dvr_entry_status(dvr_entry_t *de);

const char *dvr_entry_schedstatus(dvr_entry_t *de);

void dvr_entry_create_by_autorec(int enabled, epg_broadcast_t *e, dvr_autorec_entry_t *dae);

void dvr_entry_created(dvr_entry_t *de);

dvr_entry_t *
dvr_entry_create ( const char *uuid, htsmsg_t *conf );


dvr_entry_t *
dvr_entry_create_by_event( int enabled,
                           const char *dvr_config_uuid,
                           epg_broadcast_t *e,
                           time_t start_extra, time_t stop_extra,
                           const char *owner, const char *creator,
                           dvr_autorec_entry_t *dae,
                           dvr_prio_t pri, int retention,
                           const char *comment );

dvr_entry_t *
dvr_entry_create_htsp( int enabled, const char *dvr_config_uuid,
                       channel_t *ch, time_t start, time_t stop,
                       time_t start_extra, time_t stop_extra,
                       const char *title, const char *subtitle,
                       const char *description,
                       const char *lang, epg_genre_t *content_type,
                       const char *owner, const char *creator,
                       dvr_autorec_entry_t *dae,
                       dvr_prio_t pri, int retention,
                       const char *comment );

dvr_entry_t *
dvr_entry_update( dvr_entry_t *de, int enabled, channel_t *ch,
                  const char *title, const char *subtitle,
                  const char *desc, const char *lang,
                  time_t start, time_t stop,
                  time_t start_extra, time_t stop_extra,
                  dvr_prio_t pri, int retention );

void dvr_destroy_by_channel(channel_t *ch, int delconf);

void dvr_stop_recording(dvr_entry_t *de, int stopcode, int saveconf);

int dvr_rec_subscribe(dvr_entry_t *de);

void dvr_rec_unsubscribe(dvr_entry_t *de);

void dvr_event_replaced(epg_broadcast_t *e, epg_broadcast_t *new_e);

void dvr_event_updated(epg_broadcast_t *e);

dvr_entry_t *dvr_entry_find_by_id(int id);

static inline dvr_entry_t *dvr_entry_find_by_uuid(const char *uuid)
  { return (dvr_entry_t*)idnode_find(uuid, &dvr_entry_class, NULL); }

dvr_entry_t *dvr_entry_find_by_event(epg_broadcast_t *e);

dvr_entry_t *dvr_entry_find_by_event_fuzzy(epg_broadcast_t *e);

dvr_entry_t *dvr_entry_find_by_episode(epg_broadcast_t *e);

const char *dvr_get_filename(dvr_entry_t *de);

int64_t dvr_get_filesize(dvr_entry_t *de);

dvr_entry_t *dvr_entry_stop(dvr_entry_t *de);

dvr_entry_t *dvr_entry_cancel(dvr_entry_t *de);

void dvr_entry_dec_ref(dvr_entry_t *de);

void dvr_entry_delete(dvr_entry_t *de);

void dvr_entry_cancel_delete(dvr_entry_t *de);

htsmsg_t *dvr_entry_class_mc_list (void *o, const char *lang);
htsmsg_t *dvr_entry_class_pri_list(void *o, const char *lang);
htsmsg_t *dvr_entry_class_config_name_list(void *o, const char *lang);
htsmsg_t *dvr_entry_class_duration_list(void *o, const char *not_set, int max, int step, const char *lang);

int dvr_entry_verify(dvr_entry_t *de, access_t *a, int readonly);

void dvr_disk_space_init(void);
void dvr_disk_space_done(void);
int dvr_get_disk_space(int64_t *bfree, int64_t *btotal);

/**
 *
 */

dvr_autorec_entry_t *
dvr_autorec_create(const char *uuid, htsmsg_t *conf);

dvr_entry_t *
dvr_entry_create_(int enabled, const char *config_uuid, epg_broadcast_t *e,
                  channel_t *ch, time_t start, time_t stop,
                  time_t start_extra, time_t stop_extra,
                  const char *title, const char* subtitle, const char *description,
                  const char *lang, epg_genre_t *content_type,
                  const char *owner, const char *creator,
                  dvr_autorec_entry_t *dae, dvr_timerec_entry_t *tae,
                  dvr_prio_t pri, int retention, const char *comment);

dvr_autorec_entry_t *
dvr_autorec_create_htsp(const char *dvr_config_name, const char *title, int fulltext,
                            channel_t *ch, uint32_t enabled, int32_t start,
                            int32_t start_window, uint32_t days, time_t start_extra,
                            time_t stop_extra, dvr_prio_t pri, int retention,
                            int min_duration, int max_duration, dvr_autorec_dedup_t dup_detect,
                            const char *owner, const char *creator,
                            const char *comment, const char *name, const char *directory);

dvr_autorec_entry_t *
dvr_autorec_add_series_link(const char *dvr_config_name,
                            epg_broadcast_t *event,
                            const char *owner, const char *creator,
                            const char *comment);

void dvr_autorec_save(dvr_autorec_entry_t *dae);

void dvr_autorec_changed(dvr_autorec_entry_t *dae, int purge);

static inline dvr_autorec_entry_t *
dvr_autorec_find_by_uuid(const char *uuid)
  { return (dvr_autorec_entry_t*)idnode_find(uuid, &dvr_autorec_entry_class, NULL); }


htsmsg_t * dvr_autorec_entry_class_time_list(void *o, const char *null);
htsmsg_t * dvr_autorec_entry_class_weekdays_get(uint32_t weekdays);
htsmsg_t * dvr_autorec_entry_class_weekdays_list (void *o, const char *list);
char * dvr_autorec_entry_class_weekdays_rend(uint32_t weekdays, const char *lang);

void dvr_autorec_check_event(epg_broadcast_t *e);
void dvr_autorec_check_brand(epg_brand_t *b);
void dvr_autorec_check_season(epg_season_t *s);
void dvr_autorec_check_serieslink(epg_serieslink_t *s);

void autorec_destroy_by_config(dvr_config_t *cfg, int delconf);

void autorec_destroy_by_channel(channel_t *ch, int delconf);

void autorec_destroy_by_channel_tag(channel_tag_t *ct, int delconf);

void autorec_destroy_by_id(const char *id, int delconf);

void dvr_autorec_init(void);

void dvr_autorec_done(void);

void dvr_autorec_update(void);

static inline int
  dvr_autorec_entry_verify(dvr_autorec_entry_t *dae, access_t *a, int readonly)
{
  if (readonly && !access_verify2(a, ACCESS_ALL_RECORDER))
    return 0;
  if (!access_verify2(a, ACCESS_ALL_RW_RECORDER))
    return 0;
  if (strcmp(dae->dae_owner ?: "", a->aa_username ?: ""))
    return -1;
  return 0;
}

int dvr_autorec_get_retention( dvr_autorec_entry_t *dae );

int dvr_autorec_get_extra_time_post( dvr_autorec_entry_t *dae );

int dvr_autorec_get_extra_time_pre( dvr_autorec_entry_t *dae );

/**
 *
 */

dvr_timerec_entry_t *
dvr_timerec_create(const char *uuid, htsmsg_t *conf);

dvr_timerec_entry_t*
dvr_timerec_create_htsp(const char *dvr_config_name, const char *title,
                            channel_t *ch, uint32_t enabled, uint32_t start, uint32_t stop,
                            uint32_t weekdays, dvr_prio_t pri, int retention,
                            const char *owner, const char *creator, const char *comment, 
                            const char *name, const char *directory);

static inline dvr_timerec_entry_t *
dvr_timerec_find_by_uuid(const char *uuid)
  { return (dvr_timerec_entry_t*)idnode_find(uuid, &dvr_timerec_entry_class, NULL); }


void dvr_timerec_save(dvr_timerec_entry_t *dae);

void dvr_timerec_check(dvr_timerec_entry_t *dae);

void timerec_destroy_by_config(dvr_config_t *cfg, int delconf);

void timerec_destroy_by_channel(channel_t *ch, int delconf);

void timerec_destroy_by_id(const char *id, int delconf);

void dvr_timerec_init(void);

void dvr_timerec_done(void);

void dvr_timerec_update(void);

static inline int dvr_timerec_entry_verify
  (dvr_timerec_entry_t *dte, access_t *a, int readonly)
{
  if (readonly && !access_verify2(a, ACCESS_ALL_RECORDER))
    return 0;
  if (!access_verify2(a, ACCESS_ALL_RW_RECORDER))
    return 0;
  if (strcmp(dte->dte_owner ?: "", a->aa_username ?: ""))
    return -1;
  return 0;
}

int dvr_timerec_get_retention( dvr_timerec_entry_t *dte );

/**
 *
 */
dvr_prio_t dvr_pri2val(const char *s);

const char *dvr_val2pri(dvr_prio_t v);

/**
 * Inotify support
 */
void dvr_inotify_init ( void );
void dvr_inotify_done ( void );
void dvr_inotify_add  ( dvr_entry_t *de );
void dvr_inotify_del  ( dvr_entry_t *de );

/**
 * Cutpoints support
 **/

typedef struct dvr_cutpoint {
  TAILQ_ENTRY(dvr_cutpoint) dc_link;
  uint64_t dc_start_ms;
  uint64_t dc_end_ms;
  enum {
    DVR_CP_CUT,
    DVR_CP_MUTE,
    DVR_CP_SCENE,
    DVR_CP_COMM
  } dc_type;
} dvr_cutpoint_t;

typedef TAILQ_HEAD(,dvr_cutpoint) dvr_cutpoint_list_t;

dvr_cutpoint_list_t *dvr_get_cutpoint_list (dvr_entry_t *de);
void dvr_cutpoint_list_destroy (dvr_cutpoint_list_t *list);

/**
 *
 */

void dvr_init(void);
void dvr_config_init(void);

void dvr_done(void);


#endif /* DVR_H  */
