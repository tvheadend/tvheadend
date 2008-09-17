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



typedef enum {
  DVR_SCHEDULED,         /* Scheduled for recording (in the future) */
  DVR_RECORDING,
  DVR_COMPLETED          /* If recording failed, de->de_error is set to
			    a string */

} dvr_entry_sched_state_t;


typedef struct dvr_entry {
  LIST_ENTRY(dvr_entry) de_global_link;
  int de_id;
  
  channel_t *de_channel;
  LIST_ENTRY(dvr_entry) de_channel_link;

  time_t de_start;
  time_t de_stop;

  char *de_creator;
  char *de_filename;   /* Initially null if no filename has been
			  generated yet */
  char *de_title;      /* Title in UTF-8 (from EPG) */
  char *de_desc;       /* Description in UTF-8 (from EPG) */

  char *de_error;

  dvr_entry_sched_state_t de_sched_state;
  
  gtimer_t de_timer;

  /**
   * Fields for recording
   */

  th_subscription_t *de_s;

} dvr_entry_t;

/**
 * Prototypes
 */
void dvr_entry_create_by_event(event_t *e, const char *creator);

void dvr_init(void);

void dvr_rec_start(dvr_entry_t *de);

dvr_entry_t *dvr_entry_find_by_id(int id);

void dvr_entry_cancel(dvr_entry_t *de);


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

#endif /* DVR_H  */
