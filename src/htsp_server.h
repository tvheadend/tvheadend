/*
 *  tvheadend, HTSP interface
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

#ifndef HTSP_H_
#define HTSP_H_

#include "epg.h"
#include "dvr/dvr.h"

typedef enum {
  HTSP_DVR_PLAYCOUNT_RESET = 0,
  HTSP_DVR_PLAYCOUNT_SET   = 1,
  HTSP_DVR_PLAYCOUNT_KEEP  = INT32_MAX-1,
  HTSP_DVR_PLAYCOUNT_INCR  = INT32_MAX
} dvr_playcount_t;

void htsp_init(const char *bindaddr);
void htsp_register(void);
void htsp_done(void);

void htsp_channel_update_nownext(channel_t *ch);

void htsp_channel_add(channel_t *ch);
void htsp_channel_update(channel_t *ch);
void htsp_channel_delete(channel_t *ch);

void htsp_tag_add(channel_tag_t *ct);
void htsp_tag_update(channel_tag_t *ct);
void htsp_tag_delete(channel_tag_t *ct);

void htsp_dvr_entry_add(dvr_entry_t *de);
void htsp_dvr_entry_update(dvr_entry_t *de);
void htsp_dvr_entry_update_stats(dvr_entry_t *de);
void htsp_dvr_entry_delete(dvr_entry_t *de);

void htsp_autorec_entry_add(dvr_autorec_entry_t *dae);
void htsp_autorec_entry_update(dvr_autorec_entry_t *dae);
void htsp_autorec_entry_delete(dvr_autorec_entry_t *dae);

void htsp_timerec_entry_add(dvr_timerec_entry_t *dte);
void htsp_timerec_entry_update(dvr_timerec_entry_t *dte);
void htsp_timerec_entry_delete(dvr_timerec_entry_t *dte);

void htsp_event_add(epg_broadcast_t *ebc);
void htsp_event_update(epg_broadcast_t *ebc);
void htsp_event_delete(epg_broadcast_t *ebc);

#endif /* HTSP_H_ */
