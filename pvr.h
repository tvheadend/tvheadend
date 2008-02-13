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

#include <ffmpeg/avformat.h>

extern char *pvrpath;
extern struct pvr_rec_list pvrr_global_list;


/*
 * PVR Internal recording status
 */
typedef enum {
  PVR_REC_STOP,
  PVR_REC_WAIT_SUBSCRIPTION,
  PVR_REC_WAIT_FOR_START,
  PVR_REC_WAIT_AUDIO_LOCK,
  PVR_REC_WAIT_VIDEO_LOCK,
  PVR_REC_RUNNING,
  PVR_REC_COMMERCIAL,

} pvrr_rec_status_t;


/*
 * PVR recording session
 */
typedef struct pvr_rec {

  LIST_ENTRY(pvr_rec) pvrr_global_link;

  int pvrr_id;               /* on-disk database id (filename) */
  th_channel_t *pvrr_channel;

  time_t pvrr_start;
  time_t pvrr_stop;

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

  pvrr_rec_status_t pvrr_rec_status; /* internal recording status */

  struct th_pkt_queue pvrr_pktq;
  int pvrr_pktq_len;
  pthread_mutex_t pvrr_pktq_mutex;
  pthread_cond_t pvrr_pktq_cond;

  int pvrr_ref;

  th_subscription_t *pvrr_s;

  pthread_t pvrr_ptid;
  dtimer_t pvrr_timer;

  th_muxer_t pvrr_muxer;

  int pvrr_header_written;

  int64_t pvrr_dts_offset;

} pvr_rec_t;




typedef enum {
  RECOP_ONCE,
  RECOP_DAILY,
  RECOP_WEEKLY,
  RECOP_ABORT,
  RECOP_CLEAR,
} recop_t;

void pvr_init(void);

void pvr_event_record_op(th_channel_t *ch, event_t *e, recop_t op);

char pvr_prog_status(event_t *e);

pvr_rec_t *pvr_get_log_entry(int e);

pvr_rec_t *pvr_get_tag_entry(int e);

void pvr_inform_status_change(pvr_rec_t *pvrr);

void pvr_channel_record_op(th_channel_t *ch, int duration);

int pvr_op2int(const char *op);

void pvr_do_op(pvr_rec_t *pvrr, recop_t op);

void pvr_clear_all_completed(void);

#endif /* PVR_H */
