/*
 *  Private Video Recorder
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

#ifndef PVR_H
#define PVR_H

#include <libavformat/avformat.h>

extern char *pvrpath;
extern struct pvr_rec_list pvrr_global_list;



/*
 * PVR recording session
 */
typedef struct pvr_rec {

  LIST_ENTRY(pvr_rec) pvrr_global_link;

  int pvrr_id;               /* on-disk database id (filename) */

  channel_t *pvrr_channel;
  LIST_ENTRY(pvr_rec) pvrr_channel_link;

  time_t pvrr_start;
  time_t pvrr_stop;

  char *pvrr_creator;        /* Who schedule the recording */

  char *pvrr_filename;       /* May be null if we havent figured out a name
				yet, this happens upon record start.
				Notice that this is full path */
  char *pvrr_title;          /* Title in UTF-8 */
  char *pvrr_desc;           /* Description in UTF-8 */

  char *pvrr_printname;      /* Only ASCII chars, used for logging and such */
  const char *pvrr_fmt_lavfname;   /* File format lavf name */
  const char *pvrr_fmt_postfix;    /* File format file postfix */

  char pvrr_status;          /* defined in libhts/htstv.h */
  char pvrr_error;           /* dito - but status returned from recorder */

  struct th_pkt_queue pvrr_pktq;
  int pvrr_pktq_len;
  pthread_mutex_t pvrr_pktq_mutex;
  pthread_cond_t pvrr_pktq_cond;

  int pvrr_ref;

  th_subscription_t *pvrr_s;

  pthread_t pvrr_ptid;
  dtimer_t pvrr_timer;

  th_ffmuxer_t pvrr_tffm;

  int64_t pvrr_dts_offset;

} pvr_rec_t;


void pvr_init(void);

pvr_rec_t *pvr_get_log_entry(int e);

pvr_rec_t *pvr_get_tag_entry(int e);

void pvr_inform_status_change(pvr_rec_t *pvrr);

void pvr_clear_all_completed(void);

int pvr_clear(pvr_rec_t *pvrr);

int pvr_abort(pvr_rec_t *pvrr);

pvr_rec_t *pvr_get_by_entry(event_t *e);

pvr_rec_t *pvr_schedule_by_event(event_t *e, const char *creator);

pvr_rec_t *pvr_schedule_by_channel_and_time(channel_t *ch, int duration,
					    const char *creator);

void pvr_destroy_by_channel(channel_t *ch);

#endif /* PVR_H */
