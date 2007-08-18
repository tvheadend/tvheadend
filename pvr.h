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

extern char *pvrpath;
extern pthread_mutex_t pvr_mutex;
extern struct pvr_rec_list pvrr_work_list[PVRR_WORK_MAX];
extern struct pvr_rec_list pvrr_global_list;

typedef enum {
  RECOP_TOGGLE,
  RECOP_ONCE,
  RECOP_DAILY,
  RECOP_WEEKLY,
  RECOP_CANCEL,
} recop_t;

void pvr_init(void);

void pvr_event_record_op(th_channel_t *ch, event_t *e, recop_t op);

char pvr_prog_status(event_t *e);

pvr_rec_t *pvr_get_log_entry(int e);

pvr_rec_t *pvr_get_tag_entry(int e);

void pvr_inform_status_change(pvr_rec_t *pvrr);

void pvr_database_save(void);

#endif /* PVR_H */
