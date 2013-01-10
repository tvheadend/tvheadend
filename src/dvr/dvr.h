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
#include "lang_str.h"

typedef struct dvr_config {
  char *dvr_config_name;
  char *dvr_storage;
  uint32_t dvr_retention_days;
  int dvr_flags;
  char *dvr_postproc;
  int dvr_extra_time_pre;
  int dvr_extra_time_post;

  muxer_container_type_t dvr_mc;

  /* Series link support */
  int dvr_sl_brand_lock;
  int dvr_sl_season_lock;
  int dvr_sl_channel_lock;
  int dvr_sl_time_lock;
  int dvr_sl_more_recent;
  int dvr_sl_quality_lock;

  /* Duplicate detect */
  int dvr_dup_detect_episode;

  LIST_ENTRY(dvr_config) config_link;
} dvr_config_t;

extern struct dvr_config_list dvrconfigs;

extern struct dvr_entry_list dvrentries;

#define DVR_DIR_PER_DAY		0x1
#define DVR_DIR_PER_CHANNEL	0x2
#define DVR_CHANNEL_IN_TITLE	0x4
#define DVR_DATE_IN_TITLE	0x8
#define DVR_TIME_IN_TITLE	0x10
#define DVR_WHITESPACE_IN_TITLE	0x20
#define DVR_DIR_PER_TITLE	0x40
#define DVR_EPISODE_IN_TITLE	0x80
#define DVR_CLEAN_TITLE	        0x100
#define DVR_TAG_FILES           0x200
#define DVR_SKIP_COMMERCIALS    0x400

typedef enum {
  DVR_PRIO_IMPORTANT,
  DVR_PRIO_HIGH,
  DVR_PRIO_NORMAL,
  DVR_PRIO_LOW,
  DVR_PRIO_UNIMPORTANT,
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

  int de_refcnt;   /* Modification is protected under global_lock */


  /**
   * Upon dvr_entry_remove() this fields will be invalidated (and pointers
   * NULLed)
   */

  LIST_ENTRY(dvr_entry) de_global_link;
  int de_id;
  
  channel_t *de_channel;
  LIST_ENTRY(dvr_entry) de_channel_link;

  gtimer_t de_timer;

  /**
   * These meta fields will stay valid as long as reference count > 0
   */

  char *de_config_name;

  time_t de_start;
  time_t de_stop;

  time_t de_start_extra;
  time_t de_stop_extra;

  char *de_creator;
  char *de_filename;   /* Initially null if no filename has been
			  generated yet */
  lang_str_t *de_title;      /* Title in UTF-8 (from EPG) */
  lang_str_t *de_desc;       /* Description in UTF-8 (from EPG) */
  epg_genre_t de_content_type; /* Content type (from EPG) */

  dvr_prio_t de_pri;

  uint32_t de_dont_reschedule;

  muxer_container_type_t de_mc;

  /**
   * EPG information / links
   */
  epg_broadcast_t *de_bcast;

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
   * Last error, see SM_CODE_ defines
   */
  uint32_t de_last_error;
  

  /**
   * Autorec linkage
   */
  LIST_ENTRY(dvr_entry) de_autorec_link;
  struct dvr_autorec_entry *de_autorec;

  /**
   * Fields for recording
   */
  pthread_t de_thread;

  th_subscription_t *de_s;
  streaming_queue_t de_sq;
  streaming_target_t *de_tsfix;
  streaming_target_t *de_gh;
  
  /**
   * Initialized upon SUBSCRIPTION_TRANSPORT_RUN
   */

  struct muxer *de_mux;

  /**
   * Inotify
   */
#if ENABLE_INOTIFY
  LIST_ENTRY(dvr_entry) de_inotify_link;
#endif

} dvr_entry_t;


/**
 * Autorec entry
 */
typedef struct dvr_autorec_entry {
  TAILQ_ENTRY(dvr_autorec_entry) dae_link;
  char *dae_id;

  char *dae_config_name;

  int dae_enabled;
  char *dae_creator;
  char *dae_comment;

  char *dae_title;
  regex_t dae_title_preg;
  
  epg_genre_t dae_content_type;

  int dae_approx_time; /* Minutes from midnight */

  int dae_weekdays;

  channel_t *dae_channel;
  LIST_ENTRY(dvr_autorec_entry) dae_channel_link;

  channel_tag_t *dae_channel_tag;
  LIST_ENTRY(dvr_autorec_entry) dae_channel_tag_link;

  dvr_prio_t dae_pri;

  struct dvr_entry_list dae_spawns;

  epg_brand_t *dae_brand;
  epg_season_t *dae_season;
  epg_serieslink_t *dae_serieslink;
  epg_episode_num_t dae_epnum;

} dvr_autorec_entry_t;


/**
 * Prototypes
 */

void dvr_make_title(char *output, size_t outlen, dvr_entry_t *de);

dvr_config_t *dvr_config_find_by_name(const char *name);

dvr_config_t *dvr_config_find_by_name_default(const char *name);

dvr_config_t *dvr_config_create(const char *name);

void dvr_config_delete(const char *name);

void dvr_entry_notify(dvr_entry_t *de);

void dvr_entry_save(dvr_entry_t *de);

const char *dvr_entry_status(dvr_entry_t *de);

const char *dvr_entry_schedstatus(dvr_entry_t *de);

void dvr_entry_create_by_autorec(epg_broadcast_t *e, dvr_autorec_entry_t *dae);

dvr_entry_t *dvr_entry_create_by_event
  (const char *dvr_config_name,
   epg_broadcast_t *e, 
   time_t start_extra, time_t stop_extra,
   const char *creator,
   dvr_autorec_entry_t *dae,
   dvr_prio_t pri);

dvr_entry_t *dvr_entry_create
  (const char *dvr_config_name,
   channel_t *ch, time_t start, time_t stop, 
   time_t start_extra, time_t stop_extra,
	 const char *title, const char *description, const char *lang,
   epg_genre_t *content_type,
	 const char *creator, dvr_autorec_entry_t *dae,
	 dvr_prio_t pri);

dvr_entry_t *dvr_entry_update
  (dvr_entry_t *de,
   const char* de_title, const char *de_desc, const char *lang, 
   time_t de_start, time_t de_stop,
   time_t de_start_extra, time_t de_stop_extra );

void dvr_init(void);

void dvr_autorec_init(void);

void dvr_autorec_update(void);

void dvr_destroy_by_channel(channel_t *ch);

void dvr_rec_subscribe(dvr_entry_t *de);

void dvr_rec_unsubscribe(dvr_entry_t *de, int stopcode);

void dvr_event_replaced(epg_broadcast_t *e, epg_broadcast_t *new_e);

void dvr_event_updated(epg_broadcast_t *e);

dvr_entry_t *dvr_entry_find_by_id(int id);

dvr_entry_t *dvr_entry_find_by_event(epg_broadcast_t *e);

dvr_entry_t *dvr_entry_find_by_event_fuzzy(epg_broadcast_t *e);

dvr_entry_t *dvr_entry_find_by_episode(epg_broadcast_t *e);

int64_t dvr_get_filesize(dvr_entry_t *de);

dvr_entry_t *dvr_entry_cancel(dvr_entry_t *de);

void dvr_entry_dec_ref(dvr_entry_t *de);

void dvr_storage_set(dvr_config_t *cfg, const char *storage);

void dvr_container_set(dvr_config_t *cfg, const char *container);

void dvr_postproc_set(dvr_config_t *cfg, const char *postproc);

void dvr_retention_set(dvr_config_t *cfg, int days);

void dvr_flags_set(dvr_config_t *cfg, int flags);

void dvr_extra_time_pre_set(dvr_config_t *cfg, int d);

void dvr_extra_time_post_set(dvr_config_t *cfg, int d);

void dvr_entry_delete(dvr_entry_t *de);

void dvr_entry_cancel_delete(dvr_entry_t *de);

/**
 * Query interface
 */
typedef struct dvr_query_result {
  dvr_entry_t **dqr_array;
  int dqr_entries;
  int dqr_alloced;
} dvr_query_result_t;

typedef int (dvr_entry_filter)(dvr_entry_t *entry);
typedef int (dvr_entry_comparator)(const void *a, const void *b);

void dvr_query(dvr_query_result_t *dqr);
void dvr_query_filter(dvr_query_result_t *dqr, dvr_entry_filter filter);
void dvr_query_free(dvr_query_result_t *dqr);

void dvr_query_sort_cmp(dvr_query_result_t *dqr, dvr_entry_comparator cmp);
void dvr_query_sort(dvr_query_result_t *dqr);

int dvr_sort_start_descending(const void *A, const void *B);
int dvr_sort_start_ascending(const void *A, const void *B);

/**
 *
 */
void dvr_autorec_add(const char *dvr_config_name,
                     const char *title, const char *channel,
		     const char *tag, epg_genre_t *content_type,
		     const char *creator, const char *comment);

void dvr_autorec_add_series_link(const char *dvr_config_name,
                                 epg_broadcast_t *event,
                        		     const char *creator, const char *comment);

void dvr_autorec_check_event(epg_broadcast_t *e);
void dvr_autorec_check_brand(epg_brand_t *b);
void dvr_autorec_check_season(epg_season_t *s);
void dvr_autorec_check_serieslink(epg_serieslink_t *s);


void autorec_destroy_by_channel(channel_t *ch);

dvr_autorec_entry_t *autorec_entry_find(const char *id, int create);

/**
 *
 */
dvr_prio_t dvr_pri2val(const char *s);

const char *dvr_val2pri(dvr_prio_t v);

/**
 * Inotify support
 */
void dvr_inotify_init ( void );
void dvr_inotify_add  ( dvr_entry_t *de );
void dvr_inotify_del  ( dvr_entry_t *de );

#endif /* DVR_H  */
