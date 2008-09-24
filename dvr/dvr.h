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

#include <libavformat/avformat.h>
#include "epg.h"
#include "channels.h"
#include "subscriptions.h"

extern char *dvr_storage;
extern char *dvr_format;
extern char *dvr_file_postfix;
extern uint32_t dvr_retention_days;
extern int dvr_flags;

#define DVR_DIR_PER_DAY      0x1
#define DVR_DIR_PER_CHANNEL  0x2
#define DVR_CHANNEL_IN_TITLE 0x4
#define DVR_DATE_IN_TITLE    0x8
#define DVR_TIME_IN_TITLE    0x10

LIST_HEAD(dvr_rec_stream_list, dvr_rec_stream);


typedef enum {
  DVR_SCHEDULED,         /* Scheduled for recording (in the future) */
  DVR_RECORDING,
  DVR_COMPLETED          /* If recording failed, de->de_error is set to
			    a string */

} dvr_entry_sched_state_t;


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

  time_t de_start;
  time_t de_stop;

  char *de_creator;
  char *de_filename;   /* Initially null if no filename has been
			  generated yet */
  char *de_title;      /* Title in UTF-8 (from EPG) */
  char *de_ititle;     /* Internal title optionally with channelname
			  date and time pre/post/fixed */
  char *de_desc;       /* Description in UTF-8 (from EPG) */

  char *de_error;

  dvr_entry_sched_state_t de_sched_state;

  uint32_t de_dont_reschedule;
  
  /**
   * Fields for recording
   */
  th_subscription_t *de_s;

  
  /**
   * Initialized upon SUBSCRIPTION_TRANSPORT_RUN
   */
  int64_t de_ts_offset;     /* Offset to compensate for PTS/DTS not beeing
			       0 at start of recording */

  int64_t de_ts_com_start;  /* Starttime for last/current commercial break */
  int64_t de_ts_com_offset; /* Offset to subtract to PTS/DTS to skip over
			       all commercial breaks so far */

  struct dvr_rec_stream_list de_streams;
  streaming_target_t de_st;
  AVFormatContext *de_fctx;

  enum {
    DE_RS_WAIT_AUDIO_LOCK = 0,
    DE_RS_WAIT_VIDEO_LOCK,
    DE_RS_RUNNING,
    DE_RS_COMMERCIAL,
  } de_rec_state;

  int de_header_written;

} dvr_entry_t;

/**
 * Prototypes
 */
void dvr_entry_create_by_event(event_t *e, const char *creator);

void dvr_init(void);

void dvr_autorec_init(void);

void dvr_destroy_by_channel(channel_t *ch);

void dvr_rec_subscribe(dvr_entry_t *de);

void dvr_rec_unsubscribe(dvr_entry_t *de);

dvr_entry_t *dvr_entry_find_by_id(int id);

void dvr_entry_cancel(dvr_entry_t *de);

void dvr_entry_dec_ref(dvr_entry_t *de);

void dvr_storage_set(const char *storage);

void dvr_retention_set(int days);

void dvr_flags_set(int flags);

/**
 * Query interface
 */
typedef struct dvr_query_result {
  dvr_entry_t **dqr_array;
  int dqr_entries;
  int dqr_alloced;
} dvr_query_result_t;

void dvr_query(dvr_query_result_t *dqr);
void dvr_query_free(dvr_query_result_t *dqr);
void dvr_query_sort(dvr_query_result_t *dqr);

/**
 *
 */
void dvr_autorec_add(const char *title, const char *channel,
		     const char *tag, const char *contentgrp,
		     const char *creator, const char *comment);


#endif /* DVR_H  */
